#ifndef FLOAT_FORMAT_H
#define FLOAT_FORMAT_H

#include <stddef.h>

void format_shortest_f32(char* buffer, size_t size, float value);
void format_shortest_f64(char* buffer, size_t size, double value);

#endif
