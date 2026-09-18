/* Built with -nostdlib -O2 -fPIC -Wl,-q,-e,module_start and a variable stub
 * generated from sample_pic_imported.yml. */
extern int imported_value;
int *module_start(void) { return &imported_value; }
