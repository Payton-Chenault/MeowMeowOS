#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  unsigned int ticks = sys_uptime();
  char num_buf[32];
  itoa(ticks, num_buf, 10);

  sys_print(num_buf);
  sys_print("\n");

  return 0;
}