/*

Copyright (C) 2016, Francisco José García García

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "yaml-emitter.h"

int yamlemitter_stream_start(yaml_emitter_t * emitter, yaml_event_t *event)
{
	/* Create and emit the STREAM-START event. */
	yaml_stream_start_event_initialize(event, YAML_UTF8_ENCODING);
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	
	return 1;
}

int yamlemitter_document_start(yaml_emitter_t * emitter, yaml_event_t *event)
{
	if (!yaml_document_start_event_initialize(event,NULL,NULL,NULL,0))	
		return 0;
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	return 1;
}

int yamlemitter_mapping_start(yaml_emitter_t * emitter, yaml_event_t *event)
{
	yaml_mapping_start_event_initialize(event,NULL,(unsigned char*)"tag:yaml.org,2002:map",1,YAML_BLOCK_MAPPING_STYLE);
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	return 1;
}

int yamlemitter_key_value(yaml_emitter_t * emitter, yaml_event_t *event, const char* key, const char* value)
{
	yaml_scalar_event_initialize(event, NULL, NULL,(yaml_char_t*)key,-1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	
	if(value){
		yaml_scalar_event_initialize(event, NULL, NULL,(yaml_char_t*)value,-1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
		if (!yaml_emitter_emit(emitter, event))
			return 0;
	}
	
	return 1;
}

int yamlemitter_key(yaml_emitter_t * emitter, yaml_event_t *event, const char* key){
	return yamlemitter_key_value(emitter, event, key, NULL);
}

int yamlemitter_stream_end(yaml_emitter_t * emitter, yaml_event_t *event)
{
	/* Create and emit the STREAM-START event. */
	yaml_stream_end_event_initialize(event);
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	
	return 1;
}

int yamlemitter_document_end(yaml_emitter_t * emitter, yaml_event_t *event)
{
	if (!yaml_document_end_event_initialize(event,1))	
		return 0;
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	return 1;
}

int yamlemitter_mapping_end(yaml_emitter_t * emitter, yaml_event_t *event)
{
	yaml_mapping_end_event_initialize(event);
	if (!yaml_emitter_emit(emitter, event))
		return 0;
	return 1;
}
