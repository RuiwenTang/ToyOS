

void k_print(char* str);

void* k_mmap(int size);

int k_unmmap(void*, int size);

void main() {
  k_print("Hello sys call\n");

  char* buffer = k_mmap(0x1000);

  // int ret = k_unmmap(buffer, 0x1000);

  // char buf[2];
  // buf[0] = '0' + ret;
  // buf[1] = 0;

  // k_print(buf);

  while (1) {
    buffer[0] += 1;
  }

  return;
}