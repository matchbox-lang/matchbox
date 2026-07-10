#include "options.h"
#include <stdbool.h>
#include <stdio.h>

void initOptions(Options* options)
{
    options->mode = PROGRAM_RUN;
    options->testOutput = TEST_OUTPUT_FAILED;
    options->disassemble = false;
    options->executablePath = NULL;
    options->filename = NULL;
    options->testPath = NULL;
}
