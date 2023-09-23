#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

  FILE* file = fopen("readme.txt", "w+");

  if (file) {
    printf("open file success \n");

    const char* str = "hello world!";
    size_t len = strlen(str);

    uint32_t ret = fwrite(str, len, 1, file);

    printf("fwrite ret = %d \n", ret);

    fclose(file);
  }

  file = fopen("readme.txt", "r");

  if (file) {
    char buf[20];

    uint32_t ret = fseek(file, 6, SEEK_SET);

    printf("fseek ret = %d \n", ret);

    ret = fread(buf, 1, 20, file);

    printf("fread ret = %d \n", ret);

    printf("read from file : [%s] \n", buf);

    fclose(file);
  }

  int id = getpid();

  printf("getpid : %d\n", id);

  return 0;
}
