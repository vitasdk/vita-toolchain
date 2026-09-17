#include <psp2/kernel/modulemgr.h>

int simple_greeting(int value)
{
    return value * 2;
}

int module_start(SceSize argc, const void *args)
{
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
    return SCE_KERNEL_STOP_SUCCESS;
}

int module_exit(void)
{
    return SCE_KERNEL_STOP_SUCCESS;
}
