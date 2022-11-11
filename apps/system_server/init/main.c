
char buffer[512];

void k_print(char* str);

void main() {
  k_print("Hello sys call\n");

  while (1) {
    buffer[0] += 1;
  }

  return;
}