#ifndef OPTIONS_H
#define OPTIONS_H

#include "cli/program.h"
#include <stdbool.h>

typedef enum TestOutput
{
    TestOutputAll,
    TestOutputFailed,
    TestOutputPassed
} TestOutput;

typedef struct Options
{
    ProgramMode mode;
    TestOutput test_output;
    bool disassemble;
    const char* executable_path;
    const char* filename;
    const char* test_path;
} Options;

void init_options(Options* options);

#endif
