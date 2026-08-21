// Source for sample_pic.elf (Issue #274 repro). Built with:
//   arm-vita-eabi-gcc -Wl,-q -fPIC -O2 -o sample_pic.elf sample_pic.c
int shared_state = 42;
int __attribute__((noinline)) get_state(void) { return shared_state; }
int main(void) { return get_state(); }
