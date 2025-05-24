#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define PROGRAM_COMMAND "matchbox"
#define PROGRAM_VERSION "Matchbox 0.2.0"

typedef struct CommandArguments
{
    bool disassemble;
    const char* filename;
} CommandArguments;

void printUsage();
void initCommandArguments(CommandArguments* args, int count, char* argv[]);

#endif
