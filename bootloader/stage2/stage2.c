
void stage2_main(int a, int b) {
  int *p = (int *)0x9000;

  *p = a + b;
  return;
}