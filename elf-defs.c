#include <elf.h>
#include "elf-defs.h"

const char *elf_decode_e_type(int e_type)
{
	return "(unknown)";
}

const char *elf_decode_sh_type(int sh_type)
{
	return "(unknown)";
}

const char *elf_decode_p_type(int p_type)
{
	return "(unknown)";
}

const char *elf_decode_st_bind(int st_bind)
{
	switch (st_bind) {
#define STB(name) case STB_##name: return #name
		STB(LOCAL);
		STB(GLOBAL);
		STB(WEAK);
		STB(NUM);
#undef STB
	}
	if (st_bind >= STB_LOOS && st_bind <= STB_HIOS)
		return "(OS-specific)";
	if (st_bind >= STB_LOPROC && st_bind <= STB_HIPROC)
		return "(Processor-specific)";
	return "(unknown)";
}

const char *elf_decode_st_type(int st_type)
{
	switch (st_type) {
#define STT(name) case STT_##name: return #name
		STT(NOTYPE);
		STT(OBJECT);
		STT(FUNC);
		STT(SECTION);
		STT(FILE);
		STT(COMMON);
		STT(TLS);
		STT(NUM);
#undef STT
	}
	if (st_type >= STT_LOOS && st_type <= STT_HIOS)
		return "(OS-specific)";
	if (st_type >= STT_LOPROC && st_type <= STT_HIPROC)
		return "(Processor-specific)";
	return "(unknown)";
}

const char *elf_decode_r_type(int r_type)
{
	switch(r_type) {
#define R_ARM(type) case R_ARM_##type: return "R_ARM_" #type
		R_ARM(NONE);
		R_ARM(V4BX);
		R_ARM(ABS32);
		R_ARM(REL32);
		R_ARM(THM_CALL);
		R_ARM(CALL);
		R_ARM(JUMP24);
		R_ARM(TARGET1);
		R_ARM(TARGET2);
		R_ARM(PREL31);
		R_ARM(MOVW_ABS_NC);
		R_ARM(MOVT_ABS);
		R_ARM(THM_MOVW_ABS_NC);
		R_ARM(THM_MOVT_ABS);
#undef R_ARM
	}

	return "<<INVALID RELOCATION>>";
}


