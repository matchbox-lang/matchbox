#include "cli/arguments.h"
#include "cli/options.h"
#include "cli/program.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
    Options options;
    initOptions(&options);

    if (!parseCommandLine(&options, argc, argv)) {
        printUsage(stderr);

        return 1;
    }

    if (!runProgram(&options)) {
        return 1;
    }

    return 0;
}
