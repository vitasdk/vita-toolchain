#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define EXPECT(EXPR, FMT, ...) if(!(EXPR))return fprintf(stderr,FMT"\n",##__VA_ARGS__),-1;

#define PSF_MAGIC   0x46535000
#define PSF_VERSION 0x00000101
#define MAX_ENTRY (256)

#define SW(VAL) (VAL)
#define SH(VAL) (VAL)
#define ALIGN32(VAL) ((VAL + 3) & ~3)

typedef struct{
	uint32_t magic;
	uint32_t version;
	uint32_t keyofs;
	uint32_t valofs;
	uint32_t count;
}sfo_header_t;

typedef struct{
	uint16_t nameofs;
	uint8_t  alignment;
	uint8_t  type;
	uint32_t valsize;
	uint32_t totalsize;
	uint32_t dataofs;
}sfo_entry_t;

typedef struct{
	const char *name;
	uint32_t value;
	const char *data;
}entry_t;

entry_t *entry_find(entry_t*entries, const char *name)
{
	//fprintf(stderr,"%s : %s\n",__FUNCTION__, name);
	for(int i = 0; i < MAX_ENTRY; i++)
		if((!name && !entries[i].name)
		 ||( name &&  entries[i].name && !strcmp(entries[i].name, name)))
			return &entries[i];
	return NULL;
}

int main(int argc, char **argv)
{
	if(argc <= 1)
		return fprintf(stderr, "Usage: %s param.sfo [NAME=STR]... [NAME=+INT]...\n", argv[0]),-1;
	
	/* Initial entries */
	entry_t entries[MAX_ENTRY] = {
		{"APP_VER", 0, "00.00" },
		{"ATTRIBUTE", 0x8000, NULL },
		{"ATTRIBUTE2", 0, NULL },
		{"ATTRIBUTE_MINOR", 0x10, NULL },
		{"BOOT_FILE", 32, "" },
		{"CATEGORY", 0, "gd" },
		{"CONTENT_ID", 48, "" },
		{"EBOOT_APP_MEMSIZE", 0, NULL },
		{"EBOOT_ATTRIBUTE", 0, NULL },
		{"EBOOT_PHY_MEMSIZE", 0, NULL },
		{"LAREA_TYPE", 0, NULL },
		{"NP_COMMUNICATION_ID", 16, "" },
		{"PARENTAL_LEVEL", 0, NULL },
		{"PSP2_DISP_VER", 0, "00.000" },
		{"PSP2_SYSTEM_VER", 0, NULL },
		{"STITLE", 52, "Homebrew" },
		{"TITLE", 0x80, "Homebrew" },
		{"TITLE_ID", 0, "ABCD99999" },
		{"VERSION", 0, "00.00" },
	};

	entry_t *last_entry = entry_find(entries,NULL)-1;
	for(int i = 2 ; i < argc ; i++){
		char*val=strchr(argv[i],'=');
		if(!val){
			fprintf(stderr, "No given value for \"%s\"\n",argv[i]);
			continue;
		}
		*val++='\0';
		/* locate this entry, or locate a free (NULL) slot for it */
		(last_entry = entry_find(entries,argv[i])) || (last_entry = entry_find(entries,NULL));
		EXPECT(last_entry, "No more space for \"%s\"",argv[i])
		bool has_sign = (val[0]=='+'||val[0]=='-');
		*last_entry = (entry_t){
			name :argv[i],
			value:has_sign?strtoul(val,NULL,0):0,
			data :has_sign?NULL:val
		};
	}

	FILE *fp = fopen(argv[1], "wb");
	EXPECT(fp != NULL, "Unable to open \"%s\"",argv[1])
	/* compute each SFO sections offset */
	size_t entries_off = sizeof(sfo_header_t);
	size_t keys_off = entries_off+((last_entry-entries+1)*sizeof(sfo_entry_t));
	size_t data_off = keys_off;

	sfo_entry_t sfo_entries[MAX_ENTRY] = {};

	/* write keys */
	for(int i=0;i < MAX_ENTRY && entries[i].name; i++){
		sfo_entries[i].nameofs=SH(data_off-keys_off);
		fseek(fp,data_off ,SEEK_SET);
		data_off += fwrite(entries[i].name, sizeof(char), strlen(entries[i].name)+1, fp);
	}
	data_off=ALIGN32(data_off);

	/* write header + update sfo_entries */
	fseek(fp,0,SEEK_SET);
	fwrite(&(sfo_header_t){
		.magic=SW(PSF_MAGIC),
		.version=SW(PSF_VERSION),
		.count=SW(last_entry-entries+1),
		.keyofs=SW(keys_off),
		.valofs=SW(data_off),
		},1,sizeof(sfo_header_t),fp);

	/* write data + update sfo_entries */
	size_t data_len = 0;
	for(int i=0;i < MAX_ENTRY && entries[i].name; i++){
		entry_t*entry = &entries[i];
		sfo_entry_t*sfo_entry = &sfo_entries[i];
		sfo_entries[i].alignment=4;
		sfo_entries[i].type=entries[i].data ? 2 : 4;
		uint32_t valsize = entries[i].data ? strlen(entries[i].data)+1 : 0;
		uint32_t totsize = entries[i].value ? entries[i].value : ALIGN32(valsize);
		sfo_entries[i].valsize = SW(valsize);
		sfo_entries[i].totalsize = SW(totsize);
		sfo_entries[i].dataofs = data_len;
		
		fseek(fp,data_off+data_len ,SEEK_SET);
		if(entries[i].data){
			fwrite( entries[i].data, sizeof(char), valsize, fp);
			data_len += totsize;
		}else{
			fwrite(&entries[i].value, sizeof(uint32_t), 1, fp);
			data_len += 4;
		}
		fwrite("\0\0\0\0", sizeof(char), 4, fp);//padding (TODO: align instead)
	}
	/* write filled entries */
	fseek(fp,entries_off ,SEEK_SET);
	fwrite(sfo_entries,sizeof(*sfo_entries),last_entry-entries+1, fp);

	fclose(fp);
	return 0;
}