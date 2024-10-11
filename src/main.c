#include "file.h"
#include "vm.h"
#include <stdlib.h>
#include <stdio.h>

static void usage(char* prog)
{
    printf("Usage: %s <file>\n", prog);
    exit(1);
}

static void repl()
{
    char line[1024];

    while (1) {
        printf("> ");

        if (!fgets(line, 1024, stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static void runFile(const char* filename)
{
    char* source = getFileContents(filename);

    if (!source) {
        fprintf(stderr, "Error: Could not read file %s\n", filename);
        exit(1);
    }

    interpret(source);
    freeFileContents(source);
}

int main(int argc, char* argv[])
{
    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        usage(argv[0]);
    }

    return 0;
}
