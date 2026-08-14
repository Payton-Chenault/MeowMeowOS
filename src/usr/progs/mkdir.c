#include "../libs/meow_libc.h"

int _start(int argc, char **argv) {
  if (argc < 2) {
    sys_exit();
    return 1;
  }

  sys_mkdir(argv[1]);
  sys_exit();
  return 0;
}