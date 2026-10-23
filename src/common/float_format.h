#ifndef FLOAT_FORMAT_H
#define FLOAT_FORMAT_H

#include <stddef.h>

void formatShortestF32(char* buffer, size_t size, float value);
void formatShortestF64(char* buffer, size_t size, double value);

#endif
