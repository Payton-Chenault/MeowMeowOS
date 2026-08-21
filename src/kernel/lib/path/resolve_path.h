#ifndef RESOLVE_PATH_H
#define RESOLVE_PATH_H

#include <stdbool.h>
#include <stddef.h>

bool resolve_path(const char *cwd, const char *input, char *output,
                  size_t output_size);

#endif