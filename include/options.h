#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

typedef struct Options
{
    bool disassemble;
    const char* filename;
} Options;

void initOptions(Options* options, int argc, char* argv[]);

#endif
