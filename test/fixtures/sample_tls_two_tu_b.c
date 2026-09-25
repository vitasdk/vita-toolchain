extern int global_var;
extern int use_tls_a(int argc);
__thread int *tls_ptr = &global_var;
__thread int tdata_flag = 7;

int module_start(int argc, void *args) {
    return use_tls_a(*tls_ptr + tdata_flag);
}
