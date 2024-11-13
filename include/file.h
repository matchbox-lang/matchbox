#ifndef FILE_H
#define FILE_H

#include <stddef.h>

char* getFileContents(const char* filename);
size_t putFileContents(const char* filename, const void* data);

#endif
