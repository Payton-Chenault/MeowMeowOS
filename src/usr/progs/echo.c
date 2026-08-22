#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    sys_print(argv[i]);
    if (i < argc - 1) {
      sys_print(" ");
    }
  }
  sys_print("\n");

  return 0;
}