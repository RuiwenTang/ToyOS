
void kernel_main() {
  int *p = (int *)0x300000;

  *p = 0xAABBCCDD;
}