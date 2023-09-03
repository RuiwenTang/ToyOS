#include <math.h>
#include <stdio.h>

void k_print(char* str);

int main(int argc, const char** argv) {
  char buf[50];
  buf[0] = '\0';

  double a = cos(0.3);

  sprintf(buf, "a = %lf\n", a);

  k_print(buf);

  while (1) {
  }

  return 0;
}
