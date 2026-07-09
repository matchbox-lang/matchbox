#ifndef OPTIONS_H
#define OPTIONS_H

#include "program.h"
#include <stdbool.h>

typedef struct Options
{
    ProgramMode mode;
    bool disassemble;
    const char* executablePath;
    const char* filename;
    const char* testPath;
} Options;

void initOptions(Options* options);

#endif
