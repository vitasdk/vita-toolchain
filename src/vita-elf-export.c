#include <stdio.h>
#include <yaml.h>

#include "vita-export.h"
#include "yamlemitter.h"


vita_export_t *vita_exports_load(const char *filename, const char *elf, int verbose);
vita_export_t *vita_exports_loads(FILE *text, const char *elf, int verbose);
vita_export_t *vita_export_generate_default(const char *elf);
void vita_exports_free(vita_export_t *exp);

static void show_usage(void)
{
	fprintf(stderr, "Usage: vita-elf-export mod-type elf exports imports\n\n"
					"mod-type: valid values: 'u'/'user' for user mode, else 'k'/'kernel' for kernel mode\n\n"
					"elf: path to the elf produced by the toolchain to be used by vita-elf-create\n"
					"exports: path to the yaml file specifying the module information and exports\n"
					"json: path to write the import json generated by this tool\n");
}

char* hextostr(char *buf, int x){
		sprintf(buf,"0x%x",x);
		return buf;
}

char* booltostr(char *buf, int x){
		if(x){
			sprintf(buf,"true");
		}else{
			sprintf(buf,"false");
		}
		
		return buf;
}

int pack_export_symbols(yaml_emitter_t * emitter, yaml_event_t *event, vita_export_symbol **symbols, size_t symbol_n)
{
	char buffer[100];

	if( !yamlemitter_mapping_start(emitter, event))
		return 0;
		
	for (int i = 0; i < symbol_n; ++i)
	{		

		if( !yamlemitter_key_value(emitter, event, symbols[i]->name, strdup(hextostr(buffer,symbols[i]->nid))))
			return 0;
			
	}
	
	yaml_mapping_end_event_initialize(event);
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	
	return -1;
	
}

int main(int argc, char *argv[])
{
	if (argc != 5)
	{
		show_usage();
		return EXIT_FAILURE;
	}
	
	const char *type = argv[1];
	const char *elf_path = argv[2];
	const char *export_path = argv[3];
	const char *import_path = argv[4];
	char buffer[100];

	
	int is_kernel = 0;
	
	// check if kernel or user
	if (strcmp(type, "kernel") == 0 || strcmp(type, "k") == 0)
	{
		is_kernel = 1;
	}
	else if (strcmp(type, "user") == 0 || strcmp(type, "u") == 0)
	{
		is_kernel = 0;
	}
	else
	{
		fprintf(stderr, "error: invalid mod-type '%s'. see usage for more info\n", type);
		return EXIT_FAILURE;
	}
	
	// load our exports
	vita_export_t *exports = vita_exports_load(export_path, elf_path, 0);
	
	if (!exports)
		return EXIT_FAILURE;
	
	yaml_emitter_t emitter;
	yaml_event_t event;

	/* Create the Emitter object. */
	yaml_emitter_initialize(&emitter);
	
	FILE *fp = fopen(import_path, "w");

	if (!fp)
	{
		// TODO: handle this
		fprintf(stderr, "could not open '%s' for writing\n", import_path);
		return EXIT_FAILURE;
	}

	yaml_emitter_set_output_file(&emitter, fp);

	/* Create and emit the STREAM-START event. */
	if( !yamlemitter_stream_start(&emitter, &event))
		goto error;
		
	if( !yamlemitter_document_start(&emitter, &event))
		goto error;
		
	if( !yamlemitter_mapping_start(&emitter, &event))
		goto error;
		
	if( !yamlemitter_key(&emitter, &event,"modules"))
		goto error;
		
	if( !yamlemitter_mapping_start(&emitter, &event))
		goto error;
			
	if( !yamlemitter_key(&emitter, &event,exports->name))
		goto error;

	if( !yamlemitter_mapping_start(&emitter, &event))
		goto error;
	
	if( !yamlemitter_key_value(&emitter, &event,"nid",strdup(hextostr(buffer,exports->nid))))
		goto error;
		
	if( !yamlemitter_key(&emitter, &event,"libraries"))
		goto error;
					
	if( !yamlemitter_mapping_start(&emitter, &event))
		goto error;
	
	for (int i = 0; i < exports->module_n; ++i)
	{
		vita_library_export *lib = exports->modules[i];
		
		int kernel_lib = is_kernel;
		
		if (lib->syscall)
		{
			if (is_kernel)
			{
				kernel_lib = 0;
			}
			else
			{
				fprintf(stderr, "error: got syscall flag for user module. did you mean to pass as kernel module?");
				return EXIT_FAILURE;
			}
		}
		
		if( !yamlemitter_key(&emitter, &event, lib->name))
			goto error;
		
		if( !yamlemitter_mapping_start(&emitter, &event))
			goto error;
			
		if( !yamlemitter_key_value(&emitter, &event,"nid",strdup(hextostr(buffer,lib->nid))))
			goto error;
		
		if( !yamlemitter_key_value(&emitter, &event,"kernel",strdup(booltostr(buffer,kernel_lib))))
			goto error;
		
		
		if(lib->function_n){
			
			if( !yamlemitter_key(&emitter, &event, "functions"))
				goto error;
					
			if(!pack_export_symbols(&emitter, &event, lib->functions, lib->function_n))
				goto error;
		}
		
		if(lib->variable_n){
			
			if( !yamlemitter_key(&emitter, &event, "variables"))
				goto error;
				
			if(!pack_export_symbols(&emitter, &event, lib->variables, lib->variable_n))
				goto error;
		}
		
		yaml_mapping_end_event_initialize(&event);
		if (!yaml_emitter_emit(&emitter, &event))
			goto error;
			
	}
	
	if( !yamlemitter_mapping_end(&emitter, &event))
		goto error;
	
	if( !yamlemitter_mapping_end(&emitter, &event))
		goto error;

	if( !yamlemitter_mapping_end(&emitter, &event))
		goto error;
	
	if( !yamlemitter_mapping_end(&emitter, &event))
		goto error;

	if( !yamlemitter_document_end(&emitter, &event))
		goto error;
		
	if( !yamlemitter_stream_end(&emitter, &event))
		goto error;

	/* On error. */
error:
	fclose(fp);
	/* Destroy the Emitter object. */
	yaml_emitter_delete(&emitter);
	// TODO: free exports, free json
	return 0;
}
