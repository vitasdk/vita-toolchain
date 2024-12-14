
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "vita-nid-db-yml.h"
#include "vita-nid-db.h"
#include "defs.h"
#include "utils/fs_list.h"


int is_number_ch(int ch){
	if((unsigned)(ch - '0') <= 9){
		return 1;
	}

	return 0;
}

int parse_fw_string(const char *s, uint32_t *v){

	if(strlen(s) > strlen("00.000.000")){
		printf("error %d\n", __LINE__);
		return -1;
	}

	if(s[0] == '.'){
		printf("error %d\n", __LINE__);
		return -1;
	}

	char fws[0x10];

	if(s[1] == '.'){
		snprintf(fws, sizeof(fws), "0%s", s);
	}else{
		snprintf(fws, sizeof(fws), "%s", s);
	}

	if(strlen(fws) == strlen("00.00")){
		snprintf(&(fws[5]), 8, "0.000");
	}else if(strlen(fws) == strlen("00.000")){
		snprintf(&(fws[6]), 8, ".000");
	}else if(strlen(fws) == strlen("00.000.000")){
	}

	if(fws[2] != '.'){return -1;}
	if(fws[6] != '.'){return -1;}

	// .
	fws[2] = fws[3];
	fws[3] = fws[4];
	fws[4] = fws[5];
	// .
	fws[5] = fws[7];
	fws[6] = fws[8];
	fws[7] = fws[9];
	fws[8] = 0;

	if(is_number_ch(fws[0]) == 0){return -1;}
	if(is_number_ch(fws[1]) == 0){return -1;}
	if(is_number_ch(fws[2]) == 0){return -1;}
	if(is_number_ch(fws[3]) == 0){return -1;}
	if(is_number_ch(fws[4]) == 0){return -1;}
	if(is_number_ch(fws[5]) == 0){return -1;}
	if(is_number_ch(fws[6]) == 0){return -1;}
	if(is_number_ch(fws[7]) == 0){return -1;}

	*v = strtol(fws, NULL, 16);

	return 0;
}


int database_version(const char *version, void *argp){
	return 0;
}

int database_firmware(const char *firmware, void *argp){

	uint32_t fw = 0;

	int res = parse_fw_string(firmware, &fw);
	if(res < 0){
		return -1;
	}

	DBContext *ctx = (DBContext *)argp;

	DBFirmware *result;

	db_search_or_new_firmware(ctx, fw, &result);
	ctx->pFirmware = result;

	return 0;
}

int module_name(const char *name, void *argp){

	DBContext *ctx = (DBContext *)argp;
	DBModule *module;

	db_search_or_new_module(ctx->pFirmware, name, &module);
	ctx->pModule = module;

	return 0;
}

int module_fingerprint(uint32_t fingerprint, void *argp){
	DBContext *ctx = (DBContext *)argp;
	ctx->pModule->fingerprint = fingerprint;
	return 0;
}

int library_name(const char *name, void *argp){
	DBContext *ctx = (DBContext *)argp;
	DBLibrary *result;

	db_new_library(ctx->pModule, name, &result);
	ctx->pLibrary = result;
	return 0;
}

int library_stubname(const char *name, void *argp){
	DBContext *ctx = (DBContext *)argp;
	free(ctx->pLibrary->stubname);
	ctx->pLibrary->stubname = strdup(name);
	return 0;
}

int library_version(uint32_t version, void *argp){
	DBContext *ctx = (DBContext *)argp;
	ctx->pLibrary->version = version;
	return 0;
}

int library_nid(uint32_t nid, void *argp){
	DBContext *ctx = (DBContext *)argp;
	ctx->pLibrary->nid = nid;
	return 0;
}

int library_privilege(const char *privilege, void *argp){
	DBContext *ctx = (DBContext *)argp;

	if(strcmp(privilege, "kernel") == 0){
		ctx->pLibrary->privilege = LIBRARY_PRIVILEGE_KERNEL;
	}else if(strcmp(privilege, "user") == 0){
		ctx->pLibrary->privilege = LIBRARY_PRIVILEGE_USER;
	}

	return 0;
}

int entry_function(const char *name, uint32_t nid, void *argp){

	DBContext *ctx = (DBContext *)argp;
	DBEntry *entry;

	db_new_entry_function(ctx->pLibrary, name, &entry);
	entry->nid = nid;

	return 0;
}

int entry_variable(const char *name, uint32_t nid, void *argp){

	DBContext *ctx = (DBContext *)argp;
	DBEntry *entry;

	db_new_entry_variable(ctx->pLibrary, name, &entry);
	entry->nid = nid;

	return 0;
}

const VitaNIDCallbacks my_VitaNIDCallbacks = {
	.size = sizeof(VitaNIDCallbacks),
	.database_version   = database_version,
	.database_firmware  = database_firmware,
	.module_name        = module_name,
	.module_fingerprint = module_fingerprint,
	.library_name       = library_name,
	.library_stubname   = library_stubname,
	.library_version    = library_version,
	.library_nid        = library_nid,
	.library_privilege  = library_privilege,
	.entry_function     = entry_function,
	.entry_variable     = entry_variable
};

int db_top_list_callback(FSListEntry *ent, void *argp){

	if(ent->isDir != 0){
		return 0;
	}

	return add_nid_db_by_path(ent->path_full, argp);
}

int library_callback(DBLibrary *library, void *argp){

	StubContext *ctx = (StubContext *)argp;
	NidStub *stub;

	if(library->stubname != NULL && ctx->ignored_stubname == 0){
		_vita_nid_db_search_or_create_stub_by_name(ctx, library->stubname, &stub);
	}else if(library->privilege == LIBRARY_PRIVILEGE_KERNEL){
		_vita_nid_db_search_or_create_stub_by_name(ctx, library->name, &stub);
	}else{
		_vita_nid_db_search_or_create_stub_by_name(ctx, library->module->name, &stub);
	}

	_vita_nid_db_stub_push_library(stub, library);

	return 0;
}

int module_callback(DBModule *module, void *argp){
	db_execute_library_vector(module, library_callback, argp);
	return 0;
}

int fw_callback(DBFirmware *fw, void *argp){
	((StubContext *)argp)->firmware = fw->firmware;
	db_execute_module_vector(fw, module_callback, argp);
	return 0;
}

int vita_nid_db_mkdir(const char *name){
#if defined(_WIN32) && !defined(__CYGWIN__)
	return mkdir(name);
#else
	return mkdir(name, 0777); // create directory if it doesn't exist
#endif
}

const char *find_item(int argc, char *argv[], const char *name){

	for(int i=0;i<argc;i++){
		if(strstr(argv[i], name) != NULL){
			return (char *)(strstr(argv[i], name) + strlen(name));
		}
	}

	return NULL;
}

void vita_nid_db_gen_asm(NidStub *stub, DBEntry *entry, int is_function){

	char path[0x400];

	// snprintf(path, sizeof(path), "%s", stub->name);
	// vita_nid_db_mkdir(path);
	// snprintf(path, sizeof(path), "%s/%s", stub->name, entry->library->name);
	// vita_nid_db_mkdir(path);
	// snprintf(path, sizeof(path), "%s/%s/%s.S", stub->name, entry->library->name, entry->name);
	snprintf(path, sizeof(path), "%s_%s_%s.S", stub->name, entry->library->name, entry->name);

	FILE *fp = fopen(path, "wb");

	fprintf(fp, ".arch armv7a\n\n");

	if(is_function != 0){
		fprintf(fp, ".section .vitalink.fstubs.%s,\"ax\",%%progbits\n\n", entry->library->name);
	}else{
		fprintf(fp, ".section .vitalink.vstubs.%s,\"\",%%progbits\n\n", entry->library->name);
	}

	fprintf(fp, "\t.align 4\n");
	fprintf(fp, "\t.global %s\n", entry->name);

	if(is_function != 0){
		fprintf(fp, ".type %s, %%function\n", entry->name);
	}else{
		fprintf(fp, ".type %s, %%object\n", entry->name);
	}

	int flag = 0;

	if(entry->library->privilege == LIBRARY_LOCATE_KERNEL){
		flag |= VITA_STUB_GEN_2_FLAG_IS_KERNEL;
	}

	fprintf(fp, "%s:\n", entry->name);
	fprintf(fp, ".if GEN_WEAK_EXPORTS\n");
	fprintf(fp, "\t.word 0x%04X%04X\n", entry->library->version & 0xFFFF, (flag | VITA_STUB_GEN_2_FLAG_WEAK) & 0xFFFF);
	fprintf(fp, ".else\n");
	fprintf(fp, "\t.word 0x%04X%04X\n", entry->library->version & 0xFFFF, flag & 0xFFFF);
	fprintf(fp, ".endif //GEN_WEAK_EXPORTS\n");
	fprintf(fp, "\t.word 0x%08X\n", entry->library->nid);
	fprintf(fp, "\t.word 0x%08X\n", entry->nid);
	fprintf(fp, "\t.align 4\n");

	fclose(fp);
}

int make_target_stub_callback(NidStub *nid_stub, void *argp){
	fprintf(nid_stub->context->fp, " lib%s_stub.a", nid_stub->name);
	return 0;
}

int make_obj_function_callback(DBEntry *entry, void *argp){
	_VitaNIDLibStub *libstub = (_VitaNIDLibStub *)argp;
	vita_nid_db_gen_asm(libstub->nid_stub, entry, 1);
	fprintf(libstub->nid_stub->context->fp, " %s_%s_%s.o", libstub->nid_stub->name, entry->library->name, entry->name);
	return 0;
}

int make_obj_variable_callback(DBEntry *entry, void *argp){
	_VitaNIDLibStub *libstub = (_VitaNIDLibStub *)argp;
	vita_nid_db_gen_asm(libstub->nid_stub, entry, 0);
	fprintf(libstub->nid_stub->context->fp, " %s_%s_%s.o", libstub->nid_stub->name, entry->library->name, entry->name);
	return 0;
}

int make_obj_libstub_callback(_VitaNIDLibStub *libstub, void *argp){
	db_execute_function_vector(libstub->library, make_obj_function_callback, libstub);
	db_execute_variable_vector(libstub->library, make_obj_variable_callback, libstub);
	return 0;
}

int make_objs_callback(NidStub *nid_stub, void *argp){
	fprintf((FILE *)argp, "%s_OBJS =", nid_stub->name);
	libstub_execute_vector(nid_stub, make_obj_libstub_callback, NULL);
	fprintf((FILE *)argp, "\n");
	return 0;
}

int make_target_weak_stub_callback(NidStub *nid_stub, void *argp){
	fprintf(nid_stub->context->fp, " lib%s_stub_weak.a", nid_stub->name);
	return 0;
}

int make_obj_function_weak_callback(DBEntry *entry, void *argp){
	_VitaNIDLibStub *libstub = (_VitaNIDLibStub *)argp;
	fprintf(libstub->nid_stub->context->fp, " %s_%s_%s.wo", libstub->nid_stub->name, entry->library->name, entry->name);
	return 0;
}

int make_obj_variable_weak_callback(DBEntry *entry, void *argp){
	_VitaNIDLibStub *libstub = (_VitaNIDLibStub *)argp;
	fprintf(libstub->nid_stub->context->fp, " %s_%s_%s.wo", libstub->nid_stub->name, entry->library->name, entry->name);
	return 0;
}

int make_obj_libstub_weak_callback(_VitaNIDLibStub *libstub, void *argp){
	db_execute_function_vector(libstub->library, make_obj_function_weak_callback, libstub);
	db_execute_variable_vector(libstub->library, make_obj_variable_weak_callback, libstub);
	return 0;
}

int make_objs_weak_callback(NidStub *nid_stub, void *argp){

	fprintf(nid_stub->context->fp, "%s_weak_OBJS =", nid_stub->name);
	libstub_execute_vector(nid_stub, make_obj_libstub_weak_callback, NULL);
	fprintf(nid_stub->context->fp, "\n");

	return 0;
}

int gen_makefile(const char *dstdir, StubContext *stub_ctx){

	vita_nid_db_mkdir(dstdir);
	chdir(dstdir);

	FILE *fp = fopen("makefile", "wb");
	stub_ctx->fp = fp;

	fprintf(fp,
		"ifdef VITASDK\n"
		"PREFIX = $(VITASDK)/bin/\n"
		"endif\n\n"
		"ARCH ?= $(PREFIX)arm-vita-eabi\n"
		"AS = $(ARCH)-as\n"
		"AR = $(ARCH)-ar\n"
		"RANLIB = $(ARCH)-ranlib\n\n"
	);

	fprintf(fp, "TARGETS =");
	stub_execute_vector(stub_ctx, make_target_stub_callback, fp);
	fprintf(fp, "\n");

	fprintf(fp, "TARGETS_WEAK =");
	stub_execute_vector(stub_ctx, make_target_weak_stub_callback, fp);
	fprintf(fp, "\n");

	stub_execute_vector(stub_ctx, make_objs_callback, fp);
	stub_execute_vector(stub_ctx, make_objs_weak_callback, fp);

	fprintf(fp, "SceKernel_OBJS =\n");
	fprintf(fp, "ALL_OBJS=\n");
	fprintf(fp, "\n");
	fprintf(fp, "all: $(TARGETS) $(TARGETS_WEAK)\n\n");
	fprintf(fp, "define LIBRARY_template\n");
	fprintf(fp, " $(1): $$($(1:lib%%_stub.a=%%)_OBJS)\n");
	fprintf(fp, " ALL_OBJS += $$($(1:lib%%_stub.a=%%)_OBJS)\n");
	fprintf(fp, "endef\n");
	fprintf(fp, "define LIBRARY_WEAK_template\n");
	fprintf(fp, " $(1): $$($(1:lib%%_stub_weak.a=%%)_weak_OBJS)\n");
	fprintf(fp, " ALL_OBJS += $$($(1:lib%%_stub_weak.a=%%)_weak_OBJS)\n");
	fprintf(fp, "endef\n\n");
	fprintf(fp, "$(foreach library,$(TARGETS),$(eval $(call LIBRARY_template,$(library))))\n");
	fprintf(fp, "$(foreach library,$(TARGETS_WEAK),$(eval $(call LIBRARY_WEAK_template,$(library))))\n\n");
	fprintf(fp, "install: $(TARGETS) $(TARGETS_WEAK)\n");
	fprintf(fp, "\tcp $(TARGETS) $(VITASDK)/arm-vita-eabi/lib\n");
	fprintf(fp, "\tcp $(TARGETS_WEAK) $(VITASDK)/arm-vita-eabi/lib\n\n");
	fprintf(fp, "clean:\n");
	fprintf(fp, "\trm -f $(TARGETS) $(TARGETS_WEAK) $(ALL_OBJS)\n\n");
	fprintf(fp, "$(TARGETS) $(TARGETS_WEAK):\n");
	fprintf(fp, "\t@echo \"$?\" > $@-objs\n");
	fprintf(fp, "\t$(AR) cru $@ @$@-objs\n");
	fprintf(fp, "\t$(RANLIB) $@\n");
	fprintf(fp, "\trm $^ $@-objs\n\n");
	fprintf(fp, "%%.o: %%.S\n");
	fprintf(fp, "\t$(AS) --defsym GEN_WEAK_EXPORTS=0 $< -o $@\n\n");
	fprintf(fp, "%%.wo: %%.S\n");
	fprintf(fp, "\t$(AS) --defsym GEN_WEAK_EXPORTS=1 $< -o $@\n");
	fprintf(fp, "\trm $^\n\n");

	fclose(fp);

	return 0;
}

int cmake_asm_function_callback(DBEntry *entry, void *argp){
	_VitaNIDLibStub *libstub = (_VitaNIDLibStub *)argp;
	vita_nid_db_gen_asm(libstub->nid_stub, entry, 1);
	fprintf(libstub->nid_stub->context->fp, "\t%s_%s_%s.S\n", libstub->nid_stub->name, entry->library->name, entry->name);
	return 0;
}

int cmake_asm_variable_callback(DBEntry *entry, void *argp){
	_VitaNIDLibStub *libstub = (_VitaNIDLibStub *)argp;
	vita_nid_db_gen_asm(libstub->nid_stub, entry, 0);
	fprintf(libstub->nid_stub->context->fp, "\t%s_%s_%s.S\n", libstub->nid_stub->name, entry->library->name, entry->name);
	return 0;
}

int cmake_asm_library_callback(_VitaNIDLibStub *libstub, void *argp){
	db_execute_function_vector(libstub->library, cmake_asm_function_callback, libstub);
	db_execute_variable_vector(libstub->library, cmake_asm_variable_callback, libstub);
	return 0;
}

int cmake_libstub_callback(NidStub *nid_stub, void *argp){
	fprintf(nid_stub->context->fp, "set(%s_ASM\n", nid_stub->name);
	libstub_execute_vector(nid_stub, cmake_asm_library_callback, NULL);
	fprintf(nid_stub->context->fp, ")\n\n");
	return 0;
}

int cmake_libstub_list_callback(NidStub *nid_stub, void *argp){
	fprintf(nid_stub->context->fp, "\t\"%s\"\n", nid_stub->name);
	return 0;
}

int gen_cmake(const char *dstdir, StubContext *stub_ctx){

	vita_nid_db_mkdir(dstdir);
	chdir(dstdir);

	FILE *fp = fopen("CMakeLists.txt", "wb");
	stub_ctx->fp = fp;

	fprintf(fp,
		"cmake_minimum_required(VERSION 3.10)\n"
		"if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)\n"
		"\tif(DEFINED ENV{VITASDK})\n"
		"\t\tset(CMAKE_TOOLCHAIN_FILE \"$ENV{VITASDK}/share/vita.toolchain.cmake\" CACHE PATH \"toolchain file\")\n"
		"\telse()\n"
		"\t\tmessage(FATAL_ERROR \"Please define VITASDK to point to your SDK path!\")\n"
		"\tendif()\n"
		"endif()\n"
		"project(vitalibs)\n"
		"enable_language(ASM)\n"
		"\n"
	);

	stub_execute_vector(stub_ctx, cmake_libstub_callback, fp);

	fprintf(fp, "set(USER_LIBRARIES\n");
	stub_execute_vector(stub_ctx, cmake_libstub_list_callback, fp);
	fprintf(fp, ")\n\n");

	fprintf(fp,
		"foreach(library ${USER_LIBRARIES})\n"
		"\tadd_library(${library}_stub STATIC ${${library}_ASM})\n"
		"\ttarget_compile_definitions(${library}_stub PRIVATE -DGEN_WEAK_EXPORTS=0)\n"
		"\tadd_library(${library}_stub_weak STATIC ${${library}_ASM})\n"
		"\ttarget_compile_definitions(${library}_stub_weak PRIVATE -DGEN_WEAK_EXPORTS=1)\n"
		"endforeach(library)\n"
	);

	fclose(fp);

	return 0;
}

extern "C" {
	int main(int argc, char *argv[]){

		const char *cmake = find_item(argc, argv, "-cmake=");
		const char *yml = find_item(argc, argv, "-yml=");
		const char *output = find_item(argc, argv, "-output=");
		const char *ignore_stubname = find_item(argc, argv, "-ignore-stubname=");

		if(yml == NULL || output == NULL){
			return EXIT_FAILURE;
		}

		g_VitaNIDCallbacks_register(&my_VitaNIDCallbacks);

		int res;
		struct stat stat_buf;

		res = stat(yml, &stat_buf);
		if(res != 0){
			return EXIT_FAILURE;
		}

		DBContext *context;
		db_new_context(&context);

		if(S_ISDIR(stat_buf.st_mode)){

			FSListEntry *nid_db_list = NULL;

			res = fs_list_init(&nid_db_list, yml, NULL, NULL);
			if(res >= 0){
				res = fs_list_execute(nid_db_list->child, db_top_list_callback, context);
			}
			fs_list_fini(nid_db_list);
			nid_db_list = NULL;

		}else if(S_ISREG(stat_buf.st_mode)){

			FILE *fp = fopen(yml, "r");
			if(fp == NULL){
				return EXIT_FAILURE;
			}

			res = add_nid_db_by_fp(fp, context);

			fclose(fp);
			fp = NULL;
		}

		StubContext stub_ctx;
		stub_ctx.Stub.next = (NidStub *)&(stub_ctx.Stub);
		stub_ctx.Stub.prev = (NidStub *)&(stub_ctx.Stub);
		stub_ctx.firmware = 0;
		stub_ctx.fp = NULL;
		stub_ctx.ignored_stubname = 0;

		if(ignore_stubname != NULL && strcmp(ignore_stubname, "true") == 0){
			stub_ctx.ignored_stubname = 1;
		}

		db_execute_fw_vector(context, fw_callback, &stub_ctx);

		if(cmake != NULL && strcmp(cmake, "true") == 0){
			gen_cmake(output, &stub_ctx);
		}else{
			gen_makefile(output, &stub_ctx);
		}

		// TODO: free stub_ctx

		db_free_context(context);

		return EXIT_SUCCESS;
	}
}
