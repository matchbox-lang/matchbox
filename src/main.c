#include "file.h"
#include "config.h"
#include "vm.h"
#include <stdlib.h>
#include <stdio.h>

static void repl(CommandArgs* args)
{
    char line[1024];
    
    initVM();

    while (1) {
        printf("> ");

        if (!fgets(line, 1024, stdin)) {
            printf("\n");
            break;
        }

        interpret(line, args);
    }
}

static void file(CommandArgs* args)
{
    char* source = getFileContents(args->filename);

    if (!source) {
        fprintf(stderr, "Error: Could not read file %s\n", args->filename);
        usage();
    }
    
    initVM();
    interpret(source, args);
    free(source);
}

int main(int argc, char* argv[])
{
    CommandArgs args;
    initCommandArgs(&args, argc, argv);

    if (argc == 1) {
        repl(&args);
    } else {
        file(&args);
    }

    return 0;
}
