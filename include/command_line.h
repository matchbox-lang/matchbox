#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include <stdbool.h>

typedef struct Options Options;

bool parseCommandLine(Options* options, int argc, char* argv[]);

#endif
