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

const uint8_t digest_constant[0x14] = {
	0x62, 0x7C, 0xB1, 0x80, 0x8A, 0xB9, 0x38, 0xE3, 0x2C, 0x8C, 0x09, 0x17, 0x08, 0x72, 0x6A, 0x57, 0x9E, 0x25, 0x86, 0xE4
};

typedef enum SceSelfType {
	SCE_SELF_TYPE_KERNEL   = 7,
	SCE_SELF_TYPE_NPDRM    = 8,
	SCE_SELF_TYPE_KBL      = 9,
	SCE_SELF_TYPE_SECURITY = 0xB,
	SCE_SELF_TYPE_USER     = 0xD
} SceSelfType;

void usage(const char **argv) {
	fprintf(stderr, "usage: %s [-s|-ss|-a 0x2XXXXXXXXXXXXXXX] [-c] [-na] input.velf output-eboot.bin\n", argv[0] ? argv[0] : "vita-make-fself");
	fprintf(stderr, "\t-s : Generate a safe eboot.bin. A safe eboot.bin does not have access\n\tto restricted APIs and important parts of the filesystem.\n");
	fprintf(stderr, "\t-ss: Generate a secret-safe eboot.bin. Do not use this option if you don't know what it does.\n");
	fprintf(stderr, "\t-a : Authid for more permissions (SceShell: 0x2800000000000001).\n");
	fprintf(stderr, "\t-c : Enable compression.\n");
	fprintf(stderr, "\t-m : Memory budget for the application in kilobytes. (Normal app: 0, System mode app: 0x1000 - 0x12800)\n");
	fprintf(stderr, "\t-pm: Physically contiguous memory budget for the application in kilobytes. (Note: The budget will be subtracted from standard memory budget)\n");
	fprintf(stderr, "\t-at: ATTRIBUTE word in Control Info section 6.\n");
	fprintf(stderr, "\t-na: Disable ASLR.\n");
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
	int noaslr = 0;
	int self_type = SCE_SELF_TYPE_NPDRM;
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
		} else if (strcmp(*argv, "--self_type") == 0) {
			argc--;
			argv++;
			if (argc > 2)
				self_type = strtoul(*argv, NULL, 0);
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
		} else if (strcmp(*argv, "-na") == 0) {
			noaslr = 1;
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
		info->module_nid = htole32(mod_nid);
	} else if (ehdr->e_type == ET_SCE_RELEXEC) {
		int seg = ehdr->e_entry >> 30;
		int off = ehdr->e_entry & 0x3fffffff;
		Elf32_Phdr *phdr = (Elf32_Phdr*)(input + ehdr->e_phoff + seg * ehdr->e_phentsize);
		sce_module_info_raw *info = (sce_module_info_raw *)(input + phdr->p_offset + off);
		info->module_nid = htole32(mod_nid);
	}

	uint8_t elf_digest[0x20];

	sha256_vector(1, (uint8_t *[]){input}, (size_t[]){sz}, elf_digest);

	SCE_header hdr = { 0 };
	hdr.magic = 0x454353; // "SCE\0"
	hdr.version = 3;
	hdr.sdk_type = 0xC0;
	hdr.header_type = 1;
	hdr.metadata_offset = 0x600; // ext_header size
	hdr.header_len = HEADER_LEN;
	hdr.elf_filesize = sz;
	hdr.self_filesize = 0;
	hdr.self_offset = 4;
	hdr.appinfo_offset = 0x80;
	hdr.elf_offset = sizeof(SCE_header) + sizeof(SCE_appinfo);
	hdr.phdr_offset = hdr.elf_offset + sizeof(Elf32_Ehdr);
	hdr.phdr_offset = (hdr.phdr_offset + 0xf) & ~0xf; // align
	hdr.shdr_offset = 0;
	hdr.section_info_offset = hdr.phdr_offset + sizeof(Elf32_Phdr) * ehdr->e_phnum;
	hdr.sceversion_offset = hdr.section_info_offset + sizeof(segment_info) * ehdr->e_phnum;
	hdr.controlinfo_offset = hdr.sceversion_offset + sizeof(SCE_version);
	hdr.controlinfo_size = sizeof(SCE_controlinfo_4) + sizeof(SCE_controlinfo_5) + sizeof(SCE_controlinfo_6) + sizeof(SCE_controlinfo_7);

	uint32_t offset_to_real_elf = HEADER_LEN;

	if (self_type == SCE_SELF_TYPE_SECURITY) {
		hdr.sdk_type = 0x40;
		hdr.metadata_offset = 0x370; // ext_header size
		hdr.header_len = 0x800;
		hdr.controlinfo_size = sizeof(SCE_controlinfo_4) + sizeof(SCE_controlinfo_6) + sizeof(SCE_controlinfo_7);
		offset_to_real_elf = 0x800;
	}

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
	appinfo.self_type = self_type; // app/user/kernel/sm
	appinfo.version = 0x1000000000000;
	appinfo.padding = 0;

	SCE_version ver = { 0 };
	ver.unk1 = 1;
	ver.unk2 = 0;
	ver.unk3 = 16;
	ver.unk4 = 0;

	SCE_controlinfo_4 control_4 = { 0 };
	control_4.common.type = 4;
	control_4.common.size = sizeof(control_4);
	control_4.common.unk = 1;
	memcpy(control_4.constant, digest_constant, sizeof(control_4.constant));
	memcpy(control_4.elf_digest, elf_digest, sizeof(control_4.elf_digest));
	control_4.min_required_fw = 0LL; // on fself

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
	if (noaslr) {
		myhdr.e_flags = 0x05001000U;
	} else {
		myhdr.e_flags = 0x05000000U;
	}
	myhdr.e_ehsize = 0x34;
	myhdr.e_phentsize = 0x20;
	myhdr.e_phnum = ehdr->e_phnum;

	if (self_type == SCE_SELF_TYPE_SECURITY) {
		myhdr.e_machine = 0xF00D;
		myhdr.e_flags = 0x8060000U;
		myhdr.e_shentsize = 0x28;
		myhdr.e_shnum = 8;
		myhdr.e_shstrndx = 7;
	}

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
	fwrite(&control_4, sizeof(control_4), 1, fout);
	if (self_type != SCE_SELF_TYPE_SECURITY) {
		fwrite(&control_5, sizeof(control_5), 1, fout);
	}
	fwrite(&control_6, sizeof(control_6), 1, fout);
	fwrite(&control_7, sizeof(control_7), 1, fout);

	fseek(fout, offset_to_real_elf, SEEK_SET);

	if (!compressed) {
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

#define SEGMENT_ALIGNMENT (0x10)

			pad = (ftell(fout) & (SEGMENT_ALIGNMENT - 1));
			if (((i + 1) != ehdr->e_phnum) && (pad != 0)) {
				fseek(fout, (ftell(fout) + (SEGMENT_ALIGNMENT - 1)) & ~(SEGMENT_ALIGNMENT - 1), SEEK_SET);
			}
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
