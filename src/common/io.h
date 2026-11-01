#ifndef COMMON_IO_H
#define COMMON_IO_H

#include <stdio.h>

char* get_file_contents(const char* filename);
size_t put_file_contents(const char* filename, const void* data);
int read_stream_line(char **lineptr, size_t *n, FILE *stream);

#endif
