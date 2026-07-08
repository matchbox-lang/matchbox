#include "options.h"
#include <stdbool.h>
#include <stdio.h>

void initOptions(Options* options)
{
    options->disassemble = false;
    options->filename = NULL;
}
