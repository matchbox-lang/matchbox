#include "cli/options.h"
#include <stdbool.h>
#include <stdio.h>

void init_options(Options* options)
{
    options->mode = ProgramRun;
    options->test_output = TestOutputFailed;
    options->disassemble = false;
    options->executable_path = NULL;
    options->filename = NULL;
    options->test_path = NULL;
}
