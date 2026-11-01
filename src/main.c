#include "cli/arguments.h"
#include "cli/options.h"
#include "cli/program.h"
#include <stdio.h>

int main(int argc, char* argv[])
{
    Options options;
    init_options(&options);

    if (!parse_command_line(&options, argc, argv)) {
        print_usage(stderr);

        return 1;
    }

    return run_program(&options);
}
