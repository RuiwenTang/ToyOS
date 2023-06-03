#include <stdlib.h>
#include <string.h>

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {
  if (!nmemb) return;

  /**
   * Yes, this is bubble sort. I had a sneaking suspicion that the missing
   * implementation of qsort was the one thing holding the GCC port back from
   * working, and I didn't feel like implementing quicksort just to test that
   * theory. So, bubble sort for now it is.
   *
   * FIXME: Make qsort actually qsort ;)
   */

  int ncompar = nmemb - 1;
  uint8_t switchbuf[size];
  while (ncompar) {
    uint8_t *array = base;
    int switches = 0;
    for (int i = 0; i < ncompar; i++) {
      if (compar(array, array + size) > 0) {
        memcpy(switchbuf, array, size);
        memcpy(array, array + size, size);
        memcpy(array + size, switchbuf, size);
        switches += 1;
      }
      array += size;
    }
    if (!switches) break;
    ncompar--;
  }
}
