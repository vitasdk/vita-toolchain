// Source for sample_tls_tbss.elf (native TLS test, rule 4b). Built with:
//   arm-vita-eabi-gcc -Wl,-q -Wl,-e,module_start -nostdlib -O0 -o sample_tls_tbss.elf sample_tls_tbss.c
__thread int tbss_only_var;

int module_start(int argc, void *args) {
    tbss_only_var = argc;
    return tbss_only_var;
}
