#ifndef BUFFER_H
#define BUFFER_H

#include <stdio.h>

char* getFileContents(const char* filename);
size_t putFileContents(const char* filename, const void* data);
int getStreamContents(char **lineptr, size_t *n, FILE *stream);

#endif
