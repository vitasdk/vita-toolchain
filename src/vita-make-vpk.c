#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <zip.h>

#define EXPECT(EXPR, FMT, ...) if(!(EXPR))return fprintf(stderr,FMT"\n",##__VA_ARGS__),-1;

#define EBOOT_FILE "eboot.bin"
#define PARAM_FILE "param.sfo"
#define EBOOT_PATH "eboot.bin"
#define PARAM_PATH "sce_sys/param.sfo"

/* recursively import files/directories into zip */
int file_add_req(zip_t *zip, char* outer_path, char* inner_path)
{
	struct stat s;
	EXPECT(stat(outer_path,&s) == 0 , "can't access %s",outer_path)
	if(S_ISDIR(s.st_mode)){
		DIR *dir = opendir(outer_path);
		struct dirent *entry;
		while((entry = readdir(dir))){
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;
			char outer[4096];
			char inner[4096];
			int len_o = snprintf(outer, sizeof(outer)-1, "%s/%s", outer_path, entry->d_name);
			int len_i = snprintf(inner, sizeof(inner)-1, "%s/%s", inner_path, entry->d_name);
			outer[len_i] = 0;
			inner[len_o] = 0;
			file_add_req(zip,outer,inner);
		}
		closedir(dir);
	}else if(S_ISREG(s.st_mode)){
		/*fprintf(stderr, "Packing \"%s\" as \"%s\"\n", outer_path, inner_path);*/
		zip_file_add(zip, inner_path, zip_source_file(zip, outer_path, 0, 0), 0);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if(argc<=1)
		return fprintf(stderr, "Usage:%s output.vpk [DIR/FILE[=vpk_path]]\n", argv[0]),-1;
	/* auto-complete missing argument with common mandatory files*/
	if(argc==2)
		argv[argc++] = EBOOT_FILE;
	if(argc==3)
		argv[argc++] = PARAM_FILE;

	int ret=0;
	zip_t *zip = zip_open(argv[1], ZIP_CREATE | ZIP_TRUNCATE, &ret);
	EXPECT(zip != NULL, "zip_open() failed (%i)", ret);

	for (int i = 2; i < argc; i++) {
		char*equal_pos=strchr(argv[i],'=');
		char*outer_path=argv[i];
		char*inner_path=argv[i];
		if(equal_pos){
			*equal_pos='\0';
			inner_path=equal_pos+1;
		}
		/* generate vpk_path for mandatory files */
		if(equal_pos==NULL && strcmp(outer_path,PARAM_FILE)==0)
			inner_path=PARAM_PATH;
		if(equal_pos==NULL && strcmp(outer_path,EBOOT_FILE)==0)
			inner_path=EBOOT_PATH;
		
		ret |= file_add_req(zip,outer_path,inner_path);
	}
	/* Don't write the zip file if something went wrong */
	if(ret<0){
		fprintf(stderr,"VPK generation canceled because of error(s) during packing\n");
		return zip_discard(zip),ret;
	}
	/* warn if a mandatory files is missing */
	if (zip_name_locate(zip, EBOOT_PATH, 0) < 0)
		fprintf(stderr,"Warning: %s vpk does not have any \""EBOOT_PATH"\"\n",argv[1]);
	if (zip_name_locate(zip, PARAM_PATH, 0) < 0)
		fprintf(stderr,"Warning: %s does not have any \""PARAM_PATH"\"\n",argv[1]);

	EXPECT(zip_close(zip) == 0, "zip_close() failed");
	return 0;
}
