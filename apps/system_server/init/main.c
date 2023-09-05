#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char** argv) {
  char buf[50];
  buf[0] = '\0';

  double a = cos(0.3);

  void* ptr = malloc(20);
  void* ptr1 = malloc(20);

  sprintf(buf, "a = %lf\n", a);

  printf("cos(0.3) = %lf\n", a);

  printf("ptr from malloc is %p \n", ptr);
  printf("ptr1 from malloc is %p \n", ptr1);

  free(ptr);

  ptr = malloc(20);
  printf("ptr again from malloc is %p \n", ptr);

  return 0;
}
