#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <zlib.h>
#include <getopt.h>

#include "export.h"
#include "sce-elf.h"
#include "endian-utils.h"
#include "self.h"
#include "sha256.h"

#define EXPECT(EXPR, FMT, ...) if(!(EXPR)){fprintf(stderr,FMT"\n",##__VA_ARGS__);ret=-1;goto clear;}

typedef struct{int dummy;char*help;void*val;}opt_ex;
enum{HELP=0,COMP,AUTH,VNDR,STYP,HDRV,APPV,SDKT,TYPE};/* for a sexier access to options[] */
struct option options[] = {
	/*name,hasArg,          defaultVal, description (for --help screen), value if found */
	{"help",    0, (int*)&(opt_ex){  0,"display this help and exit"},1},
	{"compress",0, (int*)&(opt_ex){  0,"compress the inner velf"},1},
	{"authid",  1, (int*)&(opt_ex){  1,"1:normal,2:safe,3:secret"}},
	{"vendorid",1, (int*)&(opt_ex){  0,"vendor id"}},
	{"selftype",1, (int*)&(opt_ex){  8,"1:lv0,2:lv1,3:lv2,4:app,5:SPU,6:secldr,7:appldr,8:NPDRM"}},
	{"ver_hdr", 1, (int*)&(opt_ex){  3,"version (header)"}},
	{"ver_app", 1, (int*)&(opt_ex){  0,"version (appinfo)"}},
	{"sdktype" ,1, (int*)&(opt_ex){192,"sdk type"}},
	{"type"    ,1, (int*)&(opt_ex){  1,"1:self,2:?,3:pkg"}},
	{}
};

int main(int argc, char **argv) {
	int ret = 0, optidx = 0;
	FILE* fin=NULL, *fout=NULL;
	char* inbuf = NULL;

	/* load arguments */
	while (getopt_long_only(argc, argv, "", options, &optidx) != -1) {
		if(options[optidx].has_arg)
			options[optidx].val=strtol(optarg,NULL,0);
	}

	/* print help if requested or bad arg count */
	if(*options[HELP].flag || (argc-optind) != 2){
		fprintf(stderr,"Usage: %s [OPTIONS] inbuf.velf output-eboot.bin\nOPTIONS:\n", argv[0]);
		for(struct option*opt = options; opt->name ; opt++){
			char line[32] = {};
			snprintf(line,sizeof(line)-1,"%s%s%.*i", opt->name, opt->has_arg?"=":" ",opt->has_arg, opt->has_arg?*opt->flag:0);
			fprintf(stderr,"  --%-16s%s\n",line, ((opt_ex*)opt->flag)->help);
		}
		return -1;
	}
	
	char*inpath=argv[argc-2];
	char*outpath=argv[argc-1];
	EXPECT(fin  = fopen(inpath, "rb"), "Failed to open \"%s\"",inpath);
	EXPECT(fout = fopen(outpath,"wb"), "Failed to open \"%s\"",outpath);
	
	fseek(fin, 0, SEEK_END);
	size_t sz = ftell(fin);
	fseek(fin, 0, SEEK_SET);

	EXPECT((inbuf = malloc(sz)), "Unable to allocate file size (%zu)", sz);
	EXPECT(fread(inbuf, sz, 1, fin) == 1, "Failed to read inbuf file");

	Elf32_Ehdr *ehdr = (Elf32_Ehdr*)inbuf;

	EXPECT((ehdr->e_type==ET_SCE_EXEC) || (ehdr->e_type==ET_SCE_RELEXEC), "%s is not a SCE(REL)EXEC .velf file",inpath)
	bool is_rel = (ehdr->e_type == ET_SCE_RELEXEC);
	// write module nid
	Elf32_Phdr          *phdr = (Elf32_Phdr*          )(inbuf + ehdr->e_phoff  + (is_rel?(ehdr->e_entry >> 30)*ehdr->e_phentsize:0));
	sce_module_info_raw *info = (sce_module_info_raw *)(inbuf + phdr->p_offset + (is_rel?(ehdr->e_entry & 0x3fffffff):phdr->p_paddr));
	EXPECT(sha256_32_file(inpath, &info->library_nid) == 0, "Unable to compute SHA256");
	info->library_nid = htole32(info->library_nid);

	SCE_header hdr = {
		.magic       = 0x454353, // "SCE\0"
		.version     = *options[HDRV].flag,
		.sdk_type    = *options[SDKT].flag,
		.header_type = *options[TYPE].flag,
		.metadata_offset = 0x600, // ???
		.header_len = 0x1000,
		.elf_filesize = sz,
		.self_offset = 4,
		.appinfo_offset = sizeof(SCE_header),
		.elf_offset = sizeof(SCE_header) + sizeof(SCE_appinfo),
		.phdr_offset = hdr.elf_offset + sizeof(Elf32_Ehdr),
		.phdr_offset = (hdr.phdr_offset + 0xf) & ~0xf, // align
		.section_info_offset = hdr.phdr_offset + sizeof(Elf32_Phdr) * ehdr->e_phnum,
		.sceversion_offset = hdr.section_info_offset + sizeof(segment_info) * ehdr->e_phnum,
		.controlinfo_offset = hdr.sceversion_offset + sizeof(SCE_version),
		.controlinfo_size = sizeof(SCE_controlinfo_5) + sizeof(SCE_controlinfo_6) + sizeof(SCE_controlinfo_7),
		.self_filesize = 0,/* Will be updated later */
	};
	SCE_appinfo appinfo = {
		.authid    = *options[AUTH].flag | (0x2FLLU<<56),
		.vendor_id = *options[VNDR].flag,
		.self_type = *options[STYP].flag,
		.version   = *options[APPV].flag | (1LLU<<48),
		.padding = 0,
	};
	SCE_version ver = {
		.unk1 = 1,
		.unk2 = 0,
		.unk3 = 16,
		.unk4 = 0,
	};
	SCE_controlinfo_5 control_5 = {{5, sizeof(control_5), 1}};
	SCE_controlinfo_6 control_6 = {{6, sizeof(control_6), 1}, 1};
	SCE_controlinfo_7 control_7 = {{7, sizeof(control_7)}};
	Elf32_Ehdr myhdr = {
		.e_ident = "\177ELF\1\1\1\0",
		.e_type = ehdr->e_type,
		.e_machine = 0x28,
		.e_version = 1,
		.e_entry = ehdr->e_entry,
		.e_phoff = 0x34,
		.e_flags = 0x05000000U,
		.e_ehsize = 0x34,
		.e_phentsize = 0x20,
		.e_phnum = ehdr->e_phnum,
	};
	
	fseek(fout, hdr.appinfo_offset, SEEK_SET);
	EXPECT(fwrite(&appinfo, sizeof(appinfo), 1, fout) == 1, "Failed to write appinfo");
	fseek(fout, hdr.elf_offset, SEEK_SET);
	EXPECT(fwrite(&myhdr, sizeof(myhdr), 1, fout) == 1, "Unable to write new ELF header");

	// copy elf phdr in same format
	fseek(fout, hdr.phdr_offset, SEEK_SET);
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		Elf32_Phdr *phdr = (Elf32_Phdr*)(inbuf + ehdr->e_phoff + ehdr->e_phentsize * i);
		// but fixup alignment, TODO: fix in toolchain
		if (phdr->p_align > 0x1000)
			phdr->p_align = 0x1000;
		EXPECT(fwrite(phdr, sizeof(*phdr), 1, fout) == 1, "Unable to write Phdr[%i]",i);
	}

	// convert elf phdr info to segment info that Sony loader expects
	// first round we assume no compression
	fseek(fout, hdr.section_info_offset, SEEK_SET);
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		Elf32_Phdr *phdr = (Elf32_Phdr*)(inbuf + ehdr->e_phoff + ehdr->e_phentsize * i); // TODO: sanity checks
		segment_info sinfo = {
			.offset = hdr.header_len + phdr->p_offset,
			.length = phdr->p_filesz,
			.compression = 1,
			.encryption = 2,
		};
		EXPECT(fwrite(&sinfo, sizeof(sinfo), 1, fout) == 1, "Unable to write Segment[%i]",i);
	}

	fseek(fout, hdr.sceversion_offset, SEEK_SET);
	EXPECT(fwrite(&ver, sizeof(ver), 1, fout) == 1, "Unable to write SCE version")

	fseek(fout, hdr.controlinfo_offset, SEEK_SET);
	fwrite(&control_5, sizeof(control_5), 1, fout);
	fwrite(&control_6, sizeof(control_6), 1, fout);
	fwrite(&control_7, sizeof(control_7), 1, fout);

	if (!*options[COMP].flag) {
		fseek(fout, hdr.header_len, SEEK_SET);
		EXPECT(fwrite(inbuf, sz, 1, fout) == 1, "Failed to write a copy of inbuf ELF");
	} else {
		for (int i = 0; i < ehdr->e_phnum; ++i) {
			Elf32_Phdr *phdr = (Elf32_Phdr*)(inbuf + ehdr->e_phoff + ehdr->e_phentsize * i); // TODO: sanity checks
			segment_info sinfo = {
				.length = 2 * phdr->p_filesz + 12,
				.compression = 2,
				.encryption = 2,
			};
			unsigned char *buf = malloc(sinfo.length);
			if (compress2(buf, (uLongf *)&sinfo.length, (unsigned char *)inbuf + phdr->p_offset, phdr->p_filesz, Z_BEST_COMPRESSION) != Z_OK) {
				free(buf);
				EXPECT(0, "compress failed");
			}
			// padding
			uint64_t pad = ((sinfo.length + 3) & ~3) - sinfo.length;
			for (int i = 0; i < pad; i++) {
				buf[pad+sinfo.length] = 0;
			}
			sinfo.offset = ftell(fout);
			fseek(fout, hdr.section_info_offset + i * sizeof(segment_info), SEEK_SET);
			EXPECT(fwrite(&sinfo, sizeof(sinfo), 1, fout) == 1, "Failed to write segment info");
			fseek(fout, sinfo.offset, SEEK_SET);
			EXPECT(fwrite(buf, sinfo.length, 1, fout) == 1, "Failed to write segment to fself");
			free(buf);
		}
	}

	fseek(fout, 0, SEEK_END);
	hdr.self_filesize = ftell(fout);
	fseek(fout, 0, SEEK_SET);
	EXPECT(fwrite(&hdr, sizeof(hdr), 1, fout) == 1, "Failed to write SCE header");

clear:
	if(fin)fclose(fin);
	if(fout)fclose(fout);
	if(inbuf)free(inbuf);

	return ret;
}
