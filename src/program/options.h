#ifndef OPTIONS_H
#define OPTIONS_H

#include "program.h"
#include <stdbool.h>

typedef enum TestOutput
{
    TEST_OUTPUT_ALL,
    TEST_OUTPUT_FAILED,
    TEST_OUTPUT_PASSED
} TestOutput;

typedef struct Options
{
    ProgramMode mode;
    TestOutput testOutput;
    bool disassemble;
    const char* executablePath;
    const char* filename;
    const char* testPath;
} Options;

void initOptions(Options* options);

#endif
