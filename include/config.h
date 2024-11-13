#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define PROGRAM_COMMAND "matchbox"
#define PROGRAM_VERSION "Matchbox 0.2.0"

typedef struct CommandArgs
{
    bool disassemble;
    const char* filename;
} CommandArgs;

void usage();
void initCommandArgs(CommandArgs* args, int count, char* argv[]);

#endif
