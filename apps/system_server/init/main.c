#include <math.h>

void k_print(char* str);

int main(int argc, const char** argv) {
  char buf[2];
  buf[0] = '0';
  buf[1] = 0;

  double a = cos(0.3);

  k_print("hello Sys call \n");

  while (1) {
    buf[0] += 2;
  }

  return 0;
}
