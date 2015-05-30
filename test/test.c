int sceKernelGetThreadId();
extern int SceKernelStackGuard;

volatile int y=17, z;

void _start() {
  volatile int x=27,a,b,c,d,e;
  x = sceKernelGetThreadId();
  y = x + 5;
  x -= SceKernelStackGuard + y;
  x += z;
}
