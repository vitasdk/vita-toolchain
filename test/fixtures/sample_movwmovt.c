// Source for sample_movwmovt.elf (Issue #225 repro). Built with:
//   arm-vita-eabi-gcc -Wl,-q -o sample_movwmovt.elf sample_movwmovt.c -lScePower_stub
int main() {
    __asm__ volatile (
        "movw r0, #:lower16:scePowerIsPowerOnline\n"
        "movt r0, #:upper16:scePowerIsPowerOnline\n"
        :
        :
        :
        "r0"
    );
    return 0;
}
