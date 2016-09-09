#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "self.h"

void usage(char *argv[]) {
	fprintf(stderr, "Usage: %s [-s] input.velf output-eboot.bin\n", argv[0] ? argv[0] : "make_fself");
	fprintf(stderr, "\t-s: Generate a safe eboot.bin. A safe eboot.bin does not have access\n\tto restricted APIs and important parts of the filesystem.\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	const char *input_path, *output_path;
	FILE *fin = NULL;
	FILE *fout = NULL;

	if (argc != 3 && argc != 4)
		usage(argv);

	int safe = 0;
	if (argc == 4) {
		if (strcmp(argv[1], "-s") == 0)
			safe = 1;
		else
			usage(argv);

		input_path = argv[2];
		output_path = argv[3];
	} else {
		input_path = argv[1];
		output_path = argv[2];
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

	ELF_header *ehdr = (ELF_header*)input;

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
	hdr.phdr_offset = hdr.elf_offset + sizeof(ELF_header);
	// hdr.shdr_offset = ;
	hdr.section_info_offset = hdr.phdr_offset + sizeof(e_phdr) * ehdr->e_phnum;
	hdr.sceversion_offset = hdr.section_info_offset + sizeof(segment_info) * ehdr->e_phnum;
	hdr.controlinfo_offset = hdr.sceversion_offset + sizeof(SCE_version);
	hdr.controlinfo_size = sizeof(SCE_controlinfo_5) + sizeof(SCE_controlinfo_6) + sizeof(SCE_controlinfo_7);
	hdr.self_filesize = hdr.section_info_offset + sizeof(segment_info) * ehdr->e_phnum + sz;

	uint32_t offset_to_real_elf = HEADER_LEN;

	// SCE_header should be ok

	SCE_appinfo appinfo = { 0 };
	if (safe)
		appinfo.authid = 0x2F00000000000002ULL;
	else
		appinfo.authid = 0x2F00000000000001ULL;
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
	control_6.unk1 = 1;
	SCE_controlinfo_7 control_7 = { 0 };
	control_7.common.type = 7;
	control_7.common.size = sizeof(control_7);

	ELF_header myhdr = { 0 };
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
	if (fwrite(&hdr, sizeof(hdr), 1, fout) != 1) {
		perror("Failed to write SCE header");
		goto error;
	}
	if (fwrite(&appinfo, sizeof(appinfo), 1, fout) != 1) {
		perror("Failed to write appinfo");
		goto error;
	}
	fwrite(&myhdr, sizeof(myhdr), 1, fout);
	// copy elf phdr in same format
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		e_phdr *phdr = (e_phdr*)(input + ehdr->e_phoff + ehdr->e_phentsize * i);
		// but fixup alignment, TODO: fix in toolchain
		if (phdr->p_align > 0x1000)
			phdr->p_align = 0x1000;
		if (fwrite(phdr, sizeof(*phdr), 1, fout) != 1) {
			perror("Failed to write phdr");
			goto error;
		}
	}

	// convert elf phdr info to segment info that sony loader expects
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		e_phdr *phdr = (e_phdr*)(input + ehdr->e_phoff + ehdr->e_phentsize * i); // TODO: sanity checks
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

	if (fwrite(&ver, sizeof(ver), 1, fout) != 1) {
		perror("Failed to write SCE_version");
		goto error;
	}
	fwrite(&control_5, sizeof(control_5), 1, fout);
	fwrite(&control_6, sizeof(control_6), 1, fout);
	fwrite(&control_7, sizeof(control_7), 1, fout);

	fseek(fout, HEADER_LEN, SEEK_SET);

	if (fwrite(input, sz, 1, fout) != 1) {
		perror("Failed to write a copy of input ELF");
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
