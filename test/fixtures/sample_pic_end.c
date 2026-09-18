/* Built with -nostdlib -O2 -fPIC -Wl,-q,-e,module_start. */
extern char _end[];
char *module_start(void) { return _end; }
