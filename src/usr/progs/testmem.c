#include "../libs/meow_libc.h"

int _start(int argc, char **argv) {
  (void)argc;
  (void)argv;
  char *ptr = (char *)sys_alloc_page();

  if (!ptr) {
    sys_print("Memory allocation failed\n");
    sys_exit();
    return 1;
  }

  for (int i = 0; i < 256; i++) {
    ptr[i] = (char)(i % 256);
  }

  int valid = 1;
  for (int i = 0; i < 256; i++) {
    if (ptr[i] != (char)(i % 256)) {
      valid = 0;
      break;
    }
  }

  sys_print(valid ? "Memory test passed\n" : "Memory test failed\n");
  sys_free_page(ptr);
  sys_exit();
  return 0;
}