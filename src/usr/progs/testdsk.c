#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  const char *test_file = "dsk_test.txt";
  const char *test_data = "MeowMeowOS Disk Test Data!";
  char read_buf[64];

  // Create file (will contain a single space)
  int fd = sys_create(test_file);
  if (fd < 0) {
    return 1;
  }
  sys_close(fd);

  // Reopen and write test data from offset 0
  fd = sys_open(test_file);
  if (fd < 0) {
    return 1;
  }

  sys_write(fd, test_data, strlen(test_data));
  sys_close(fd);

  // Open and read back
  fd = sys_open(test_file);
  if (fd < 0) {
    return 1;
  }

  int read = sys_read(fd, read_buf, sizeof(read_buf) - 1);
  read_buf[read] = '\0';
  sys_close(fd);

  sys_print(read_buf);
  sys_print("\n");

  sys_remove(test_file);
  return 0;
}