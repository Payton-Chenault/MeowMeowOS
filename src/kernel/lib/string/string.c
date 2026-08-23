#include "string.h"

#include "../integer_ascii_converters/itoa.h"

#include <stdarg.h>
#include <stdbool.h>

size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len])
    len++;
  return len;
}

int strcmp(const char *str1, const char *str2) {
  while (*str1 && (*str1 == *str2)) {
    str1++;
    str2++;
  }
  return *(const unsigned char *)str1 - *(const unsigned char *)str2;
}

int strcasecmp(const char *s1, const char *s2) {
  unsigned char c1, c2;
  do {
    c1 = (unsigned char)*s1++;
    c2 = (unsigned char)*s2++;
    if (c1 >= 'A' && c1 <= 'Z') c1 += ('a' - 'A');
    if (c2 >= 'A' && c2 <= 'Z') c2 += ('a' - 'A');
  } while (c1 && c1 == c2);
  return c1 - c2;
}

char *strstr(const char *haystack, const char *needle) {
  if (!*needle)
    return (char *)haystack;

  for (const char *h = haystack; *h != '\0'; h++) {
    const char *h_ptr = h;
    const char *n_ptr = needle;

    while (*h_ptr != '\0' && *n_ptr != '\0' && *h_ptr == *n_ptr) {
      h_ptr++;
      n_ptr++;
    }

    if (*n_ptr == '\0') {
      return (char *)h;
    }
  }

  return NULL;
}

char *strtok(char *str, const char *delimiters) {
  static char *last_str = NULL;
  if (str != NULL) {
    last_str = str;
  }

  if (last_str == NULL || *last_str == '\0') {
    return NULL;
  }

  char *token_start = last_str;
  while (*token_start != '\0') {
    bool is_delim = false;
    for (int i = 0; delimiters[i] != '\0'; i++) {
      if (*token_start == delimiters[i]) {
        is_delim = true;
        break;
      }
    }
    if (!is_delim)
      break;
    token_start++;
  }

  if (*token_start == '\0') {
    last_str = NULL;
    return NULL;
  }

  char *token_end = token_start;
  while (*token_end != '\0') {
    bool is_delim = false;
    for (int i = 0; delimiters[i] != '\0'; i++) {
      if (*token_end == delimiters[i]) {
        is_delim = true;
        break;
      }
    }
    if (is_delim) {
      *token_end = '\0';
      last_str = token_end + 1;
      return token_start;
    }
    token_end++;
  }

  last_str = token_end;
  return token_start;
}

char *strcpy(char *dest, const char *src) {
  char *saved = dest;
  while (*src) {
    *dest++ = *src++;
  }
  *dest = '\0';
  return saved;
}

char *strncpy(char *dest, const char *src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dest[i] = src[i];
  }

  for (; i < n; i++) {
    dest[i] = '\0';
  }
  return dest;
}

char *strcat(char *dest, const char *src) {
  char *ptr = dest;

  while (*ptr != '\0') {
    ptr++;
  }

  while (*src != '\0') {
    *ptr++ = *src++;
  }

  *ptr = '\0';

  return dest;
}

char *strrchr(const char *s, int c) {
  const char *last = 0;

  do {
    if (*s == (char)c) {
      last = s;
    }
  } while (*s++);

  return (char *)last;
}

int snprintf(char *str, size_t size, const char *format, ...) {
  if (size == 0)
    return 0;

  va_list args;
  va_start(args, format);

  size_t dest_idx = 0;
  const char *p = format;

  while (*p != '\0' && dest_idx < size - 1) {
    if (*p != '%') {
      str[dest_idx++] = *p++;
      continue;
    }

    p++; // Skip '%'
    if (*p == '\0')
      break;

    if (*p == 's') {
      const char *s = va_arg(args, const char *);
      if (!s)
        s = "(null)";
      while (*s != '\0' && dest_idx < size - 1) {
        str[dest_idx++] = *s++;
      }
    } else if (*p == 'd' || *p == 'i') {
      int val = va_arg(args, int);
      char buf[32];
      itoa(val, buf, 10);
      int b = 0;
      while (buf[b] != '\0' && dest_idx < size - 1) {
        str[dest_idx++] = buf[b++];
      }
    } else if (*p == 'u') {
      unsigned int val = va_arg(args, unsigned int);
      char buf[32];
      itoa((int)val, buf, 10);
      int b = 0;
      while (buf[b] != '\0' && dest_idx < size - 1) {
        str[dest_idx++] = buf[b++];
      }
    } else if (*p == 'x' || *p == 'X') {
      unsigned int val = va_arg(args, unsigned int);
      char buf[32];
      itoa((int)val, buf, 16);
      int b = 0;
      while (buf[b] != '\0' && dest_idx < size - 1) {
        str[dest_idx++] = buf[b++];
      }
    } else if (*p == '%') {
      str[dest_idx++] = '%';
    }
    p++;
  }

  str[dest_idx] = '\0';
  va_end(args);
  return (int)dest_idx;
}

void *memset(void *ptr, int value, size_t num) {
  unsigned char *p = (unsigned char *)ptr;
  for (size_t i = 0; i < num; i++) {
    p[i] = (unsigned char)value;
  }
  return ptr;
}

void *memcpy(void *dest, const void *src, size_t num) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  for (size_t i = 0; i < num; i++) {
    d[i] = s[i];
  }
  return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;

  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  if (n == 0)
    return 0;

  while (n > 0 && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
    n--;
  }

  if (n == 0) {
    return 0;
  }

  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}