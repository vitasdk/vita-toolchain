// Compiled with -fPIC -ftls-model=initial-exec to force R_ARM_TLS_IE32
// (see sample_tls_ie32_def.c for the full build command)
extern __thread int ie_var;

int use_ie(void) {
    return ie_var;
}

int module_start(int argc, void *args) {
    return use_ie();
}
