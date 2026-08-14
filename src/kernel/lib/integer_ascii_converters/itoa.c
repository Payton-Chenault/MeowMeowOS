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

  // Only handle negative signs for decimal (base 10)
  if (n < 0 && base == 10) {
    is_negative = true;
    n = -n;
  }

  // Lookup table for digits (supports up to base 16)
  const char *digits = "0123456789ABCDEF";

  while (n != 0) {
    // Use unsigned cast to handle large numbers and wrap-around
    unsigned int un = (unsigned int)n;
    str[i++] = digits[un % base];
    n = un / base;
  }

  if (is_negative)
    str[i++] = '-';
  str[i] = '\0';

  // Reverse the string (your existing logic)
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