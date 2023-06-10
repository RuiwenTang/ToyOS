

void k_print(char* str);

void* k_mmap(int size);

void main() {
  k_print("Hello sys call\n");

  char* buffer = k_mmap(512);

  while (1) {
    buffer[0] += 1;
  }

  return;
}