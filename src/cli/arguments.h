#ifndef CLI_ARGUMENTS_H
#define CLI_ARGUMENTS_H

#include <stdbool.h>

typedef struct Options Options;

bool parseCommandLine(Options* options, int argc, char* const argv[]);

#endif
