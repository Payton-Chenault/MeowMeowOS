#include "../libs/meow_libc.h"

int _start(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    sys_print(argv[i]);
    if (i < argc - 1) {
      sys_print(" ");
    }
  }
  sys_print("\n");

  sys_exit();
  return 0;
}