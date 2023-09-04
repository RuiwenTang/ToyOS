#include <math.h>
#include <stdio.h>

int main(int argc, const char** argv) {
  char buf[50];
  buf[0] = '\0';

  double a = cos(0.3);

  sprintf(buf, "a = %lf\n", a);

  printf("cos(0.3) = %lf\n", a);

  return 0;
}
