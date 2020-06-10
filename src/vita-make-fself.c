#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <zlib.h>

#include "vita-export.h"
#include "sce-elf.h"
#include "endian-utils.h"
#include "self.h"
#include "sha256.h"

void usage(const char **argv) {
	fprintf(stderr, "usage: %s [-s|-ss|-a 0x2XXXXXXXXXXXXXXX] [-c] input.velf output-eboot.bin\n", argv[0] ? argv[0] : "vita-make-fself");
	fprintf(stderr, "\t-s : Generate a safe eboot.bin. A safe eboot.bin does not have access\n\tto restricted APIs and important parts of the filesystem.\n");
	fprintf(stderr, "\t-ss: Generate a secret-safe eboot.bin. Do not use this option if you don't know what it does.\n");
	fprintf(stderr, "\t-a : Authid for more permissions (SceShell: 0x2800000000000001).\n");
	fprintf(stderr, "\t-c : Enable compression.\n");
	fprintf(stderr, "\t-m : Memory budget for the application. (Normal app: 0, System mode app: 0x100000 - 0x280100)\n");
	fprintf(stderr, "\t-pm : Physically contiguous memory budget for the application. (Note: The budget will be subtracted from standard memory budget)\n");
	fprintf(stderr, "\t-at : ATTRIBUTE word in Control Info section 6.\n");
	exit(1);
}

int main(int argc, const char **argv) {
	const char *input_path, *output_path;
	FILE *fin = NULL;
	FILE *fout = NULL;
	uint32_t mod_nid;

	argc--;
	argv++; // strip first argument
	if (argc < 2)
		usage(argv);

	int safe = 0;
	int compressed = 0;
	uint32_t mem_budget = 0;
	uint32_t phycont_mem_budget = 0;
	uint32_t attribute_cinfo = 0;
	uint64_t authid = 0;
	while (argc > 2) {
		if (strcmp(*argv, "-s") == 0) {
			safe = 2;
		} else if (strcmp(*argv, "-ss") == 0) {
			safe = 3;
		} else if (strcmp(*argv, "-c") == 0) {
			compressed = 1;
		} else if (strcmp(*argv, "-a") == 0) {
			argc--;
			argv++;
			
			if (argc > 2)
				authid = strtoull(*argv, NULL, 0);
		} else if (strcmp(*argv, "-m") == 0) {
			argc--;
			argv++;
			
			if (argc > 2)
				mem_budget = strtoul(*argv, NULL, 0);
		} else if (strcmp(*argv, "-pm") == 0) {
			argc--;
			argv++;
			
			if (argc > 2)
				phycont_mem_budget = strtoul(*argv, NULL, 0);
		} else if (strcmp(*argv, "-at") == 0) {
			argc--;
			argv++;
			
			if (argc > 2)
				attribute_cinfo = strtoul(*argv, NULL, 0);
		}
		argc--;
		argv++;
	}
	input_path = argv[0];
	output_path = argv[1];

	if (sha256_32_file(input_path, &mod_nid) != 0) {
		perror("Cannot generate module NID");
		goto error;
	}

	fin = fopen(input_path, "rb");
	if (!fin) {
		perror("Failed to open input file");
		goto error;
	}
	fseek(fin, 0, SEEK_END);
	size_t sz = ftell(fin);
	fseek(fin, 0, SEEK_SET);

	char *input = calloc(1, sz);
	if (!input) {
		perror("Failed to allocate buffer for input file");
		goto error;
	}
	if (fread(input, sz, 1, fin) != 1) {
		static const char s[] = "Failed to read input file";
		if (feof(fin))
			fprintf(stderr, "%s: unexpected end of file\n", s);
		else
			perror(s);
		goto error;
	}
	fclose(fin);
	fin = NULL;

	Elf32_Ehdr *ehdr = (Elf32_Ehdr*)input;

	// write module nid
	if (ehdr->e_type == ET_SCE_EXEC) {
		Elf32_Phdr *phdr = (Elf32_Phdr*)(input + ehdr->e_phoff);
		sce_module_info_raw *info = (sce_module_info_raw *)(input + phdr->p_offset + phdr->p_paddr);
		info->library_nid = htole32(mod_nid);
	} else if (ehdr->e_type == ET_SCE_RELEXEC) {
		int seg = ehdr->e_entry >> 30;
		int off = ehdr->e_entry & 0x3fffffff;
		Elf32_Phdr *phdr = (Elf32_Phdr*)(input + ehdr->e_phoff + seg * ehdr->e_phentsize);
		sce_module_info_raw *info = (sce_module_info_raw *)(input + phdr->p_offset + off);
		info->library_nid = htole32(mod_nid);
	}

	SCE_header hdr = { 0 };
	hdr.magic = 0x454353; // "SCE\0"
	hdr.version = 3;
	hdr.sdk_type = 0xC0;
	hdr.header_type = 1;
	hdr.metadata_offset = 0x600; // ???
	hdr.header_len = HEADER_LEN;
	hdr.elf_filesize = sz;
	// self_filesize
	hdr.self_offset = 4;
	hdr.appinfo_offset = 0x80;
	hdr.elf_offset = sizeof(SCE_header) + sizeof(SCE_appinfo);
	hdr.phdr_offset = hdr.elf_offset + sizeof(Elf32_Ehdr);
	hdr.phdr_offset = (hdr.phdr_offset + 0xf) & ~0xf; // align
	// hdr.shdr_offset = ;
	hdr.section_info_offset = hdr.phdr_offset + sizeof(Elf32_Phdr) * ehdr->e_phnum;
	hdr.sceversion_offset = hdr.section_info_offset + sizeof(segment_info) * ehdr->e_phnum;
	hdr.controlinfo_offset = hdr.sceversion_offset + sizeof(SCE_version);
	hdr.controlinfo_size = sizeof(SCE_controlinfo_5) + sizeof(SCE_controlinfo_6) + sizeof(SCE_controlinfo_7);
	hdr.self_filesize = 0;

	uint32_t offset_to_real_elf = HEADER_LEN;

	// SCE_header should be ok

	SCE_appinfo appinfo = { 0 };
	if (authid) {
		appinfo.authid = authid;
	} else {
		if (safe)
			appinfo.authid = 0x2F00000000000000ULL | safe;
		else
			appinfo.authid = 0x2F00000000000001ULL;
	}
	appinfo.vendor_id = 0;
	appinfo.self_type = 8;
	appinfo.version = 0x1000000000000;
	appinfo.padding = 0;

	SCE_version ver = { 0 };
	ver.unk1 = 1;
	ver.unk2 = 0;
	ver.unk3 = 16;
	ver.unk4 = 0;

	SCE_controlinfo_5 control_5 = { 0 };
	control_5.common.type = 5;
	control_5.common.size = sizeof(control_5);
	control_5.common.unk = 1;
	SCE_controlinfo_6 control_6 = { 0 };
	control_6.common.type = 6;
	control_6.common.size = sizeof(control_6);
	control_6.common.unk = 1;
	control_6.is_used = 1;
	if (mem_budget) {
		control_6.attr = attribute_cinfo;
		control_6.phycont_memsize = phycont_mem_budget;
		control_6.total_memsize = mem_budget;	
	}
	SCE_controlinfo_7 control_7 = { 0 };
	control_7.common.type = 7;
	control_7.common.size = sizeof(control_7);

	Elf32_Ehdr myhdr = { 0 };
	memcpy(myhdr.e_ident, "\177ELF\1\1\1", 8);
	myhdr.e_type = ehdr->e_type;
	myhdr.e_machine = 0x28;
	myhdr.e_version = 1;
	myhdr.e_entry = ehdr->e_entry;
	myhdr.e_phoff = 0x34;
	myhdr.e_flags = 0x05000000U;
	myhdr.e_ehsize = 0x34;
	myhdr.e_phentsize = 0x20;
	myhdr.e_phnum = ehdr->e_phnum;

	fout = fopen(output_path, "wb");
	if (!fout) {
		perror("Failed to open output file");
		goto error;
	}

	fseek(fout, hdr.appinfo_offset, SEEK_SET);
	if (fwrite(&appinfo, sizeof(appinfo), 1, fout) != 1) {
		perror("Failed to write appinfo");
		goto error;
	}

	fseek(fout, hdr.elf_offset, SEEK_SET);
	fwrite(&myhdr, sizeof(myhdr), 1, fout);

	// copy elf phdr in same format
	fseek(fout, hdr.phdr_offset, SEEK_SET);
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		Elf32_Phdr *phdr = (Elf32_Phdr*)(input + ehdr->e_phoff + ehdr->e_phentsize * i);
		// but fixup alignment, TODO: fix in toolchain
		if (phdr->p_align > 0x1000)
			phdr->p_align = 0x1000;
		if (fwrite(phdr, sizeof(*phdr), 1, fout) != 1) {
			perror("Failed to write phdr");
			goto error;
		}
	}

	// convert elf phdr info to segment info that sony loader expects
	// first round we assume no compression
	fseek(fout, hdr.section_info_offset, SEEK_SET);
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		Elf32_Phdr *phdr = (Elf32_Phdr*)(input + ehdr->e_phoff + ehdr->e_phentsize * i); // TODO: sanity checks
		segment_info sinfo = { 0 };
		sinfo.offset = offset_to_real_elf + phdr->p_offset;
		sinfo.length = phdr->p_filesz;
		sinfo.compression = 1;
		sinfo.encryption = 2;
		if (fwrite(&sinfo, sizeof(sinfo), 1, fout) != 1) {
			perror("Failed to write segment info");
			goto error;
		}
	}

	fseek(fout, hdr.sceversion_offset, SEEK_SET);
	if (fwrite(&ver, sizeof(ver), 1, fout) != 1) {
		perror("Failed to write SCE_version");
		goto error;
	}

	fseek(fout, hdr.controlinfo_offset, SEEK_SET);
	fwrite(&control_5, sizeof(control_5), 1, fout);
	fwrite(&control_6, sizeof(control_6), 1, fout);
	fwrite(&control_7, sizeof(control_7), 1, fout);

	if (!compressed) {
		fseek(fout, HEADER_LEN, SEEK_SET);
		if (fwrite(input, sz, 1, fout) != 1) {
			perror("Failed to write a copy of input ELF");
			goto error;
		}
	} else {
		for (int i = 0; i < ehdr->e_phnum; ++i) {
			Elf32_Phdr *phdr = (Elf32_Phdr*)(input + ehdr->e_phoff + ehdr->e_phentsize * i); // TODO: sanity checks
			segment_info sinfo = { 0 };
			unsigned char *buf = malloc(2 * phdr->p_filesz + 12);
			sinfo.length = 2 * phdr->p_filesz + 12;
			if (compress2(buf, (uLongf *)&sinfo.length, (unsigned char *)input + phdr->p_offset, phdr->p_filesz, Z_BEST_COMPRESSION) != Z_OK) {
				free(buf);
				perror("compress failed");
				goto error;
			}
			// padding
			uint64_t pad = ((sinfo.length + 3) & ~3) - sinfo.length;
			for (int i = 0; i < pad; i++) {
				buf[pad+sinfo.length] = 0;
			}
			sinfo.offset = ftell(fout);
			sinfo.compression = 2;
			sinfo.encryption = 2;
			fseek(fout, hdr.section_info_offset + i * sizeof(segment_info), SEEK_SET);
			if (fwrite(&sinfo, sizeof(sinfo), 1, fout) != 1) {
				perror("Failed to write segment info");
				free(buf);
				goto error;
			}
			fseek(fout, sinfo.offset, SEEK_SET);
			if (fwrite(buf, sinfo.length, 1, fout) != 1) {
				perror("Failed to write segment to fself");
				goto error;
			}
			free(buf);
		}
	}

	fseek(fout, 0, SEEK_END);
	hdr.self_filesize = ftell(fout);
	fseek(fout, 0, SEEK_SET);
	if (fwrite(&hdr, sizeof(hdr), 1, fout) != 1) {
		perror("Failed to write SCE header");
		goto error;
	}


	fclose(fout);

	return 0;
error:
	if (fin)
		fclose(fin);
	if (fout)
		fclose(fout);
	return 1;
}
