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

#ifndef YAMLEMITTER_H
#define YAMLEMITTER_H

#include  <yaml.h>

int yamlemitter_stream_start(yaml_emitter_t * emitter, yaml_event_t *event);
int yamlemitter_document_start(yaml_emitter_t * emitter, yaml_event_t *event);
int yamlemitter_mapping_start(yaml_emitter_t * emitter, yaml_event_t *event);
int yamlemitter_key(yaml_emitter_t * emitter, yaml_event_t *event, const char* key);
int yamlemitter_key_value(yaml_emitter_t * emitter, yaml_event_t *event, const char* key, const char* value);
int yamlemitter_stream_end(yaml_emitter_t * emitter, yaml_event_t *event);
int yamlemitter_document_end(yaml_emitter_t * emitter, yaml_event_t *event);
int yamlemitter_mapping_end(yaml_emitter_t * emitter, yaml_event_t *event);


#endif // YAMLEMITTER_H
