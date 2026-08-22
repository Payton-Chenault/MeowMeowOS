#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    return 1;
  }

  sys_remove(argv[1]);
  return 0;
}