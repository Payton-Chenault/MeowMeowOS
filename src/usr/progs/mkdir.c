#include "../libs/meow_libc.h"

int _start(int argc, char **argv)
{
  char dbg[128];

  sys_print(dbg);

  if (argc >= 2 && argv[1] != NULL)
  {
    sys_print(dbg);
  }

  if (argc < 2)
  {
    sys_print("Usage: mkdir <dir>\n");
    sys_exit();
    return 1;
  }

  int rc = sys_mkdir(argv[1]);
  sys_print(dbg);

  if (rc != 0)
  {
    char msg[128];
    snprintf(msg, sizeof(msg), "mkdir failed: %s\n", argv[1]);
    sys_print(msg);
    sys_exit();
    return 1;
  }

  sys_exit();
  return 0;
}