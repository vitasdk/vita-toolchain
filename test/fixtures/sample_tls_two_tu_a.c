// Source for sample_tls_two_tu.elf (native TLS test, rule 4a). Built with:
//   arm-vita-eabi-gcc -Wl,-q -Wl,-e,module_start -nostdlib -O0 -o sample_tls_two_tu.elf sample_tls_two_tu_a.c sample_tls_two_tu_b.c
int global_var = 0xdead;
__thread int tbss_counter;

int use_tls_a(int argc) {
    tbss_counter = argc;
    return tbss_counter;
}
