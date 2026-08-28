#include "itoa.h"
#include <stdbool.h>

void itoa(int n, char *str, int base) {
  int i = 0;
  bool is_negative = false;
  if (n == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }

  if (n < 0 && base == 10) {
    is_negative = true;
    n = -n;
  }

  const char *digits = "0123456789ABCDEF";
  while (n != 0) {
    unsigned int un = (unsigned int)n;
    str[i++] = digits[un % base];
    n = un / base;
  }

  if (is_negative)
    str[i++] = '-';
  str[i] = '\0';

  int start = 0;
  int end = i - 1;
  while (start < end) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
    start++;
    end--;
  }
}

void utoa(uint32_t n, char *str, int base) {
  int i = 0;
  if (n == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }

  const char *digits = "0123456789ABCDEF";
  while (n != 0) {
    uint32_t rem = n % base;
    str[i++] = digits[rem];
    n /= base;
  }
  str[i] = '\0';

  int start = 0;
  int end = i - 1;
  while (start < end) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
    start++;
    end--;
  }
}