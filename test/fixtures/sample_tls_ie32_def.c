// Part of sample_tls_ie32.elf (native TLS test, rule 4d). Built with:
//   arm-vita-eabi-gcc -Wl,-q -Wl,-e,module_start -nostdlib -O0 -o sample_tls_ie32.elf sample_tls_ie32_def.c sample_tls_ie32_use.c
__thread int ie_var = 42;
