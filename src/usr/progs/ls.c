#include "../libs/meow_libc.h"

int _start(int argc, char **argv) {
  const char *target_dir = (argc > 1) ? argv[1] : ".";
  sys_list_dir(target_dir);
  sys_exit();
  return 0;
}