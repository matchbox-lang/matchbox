#include "cli/runner.h"
#include "cli/path.h"
#include "common/errors.h"
#include "common/io.h"
#include "compiler/analyzer/analyzer.h"
#include "compiler/parser/parser.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "config.h"
#include <errno.h>
#include <string.h>

static _Noreturn void read_file_error(const char* filename)
{
    const char* detail = strerror(errno);

    fatal_error("Could not read file %s: %s", filename, detail);
}

bool validate_source_path(const char* path)
{
    if (!path_exists(path)) {
        report_error("Could not find path %s", path);

        return false;
    }

    if (!path_is_directory(path) && !path_has_extension(path, PROGRAM_FILE_EXTENSION)) {
        report_error("Expected %s file but found %s", PROGRAM_FILE_EXTENSION, path);

        return false;
    }

    return true;
}

char* read_source(const char* path)
{
    char* source = read_file_contents(path);
    if (!source) {
        read_file_error(path);
    }

    return source;
}

ASTNode* analyze_source(const char* source)
{
    Scope* scope = create_scope(NULL);
    ASTNode* ast = create_compound_node(scope);
    Parser parser;
    Analyzer analyzer;

    init_parser(&parser, ast);
    init_analyzer(&analyzer, ast);

    if (!parse(&parser, source)) {
        unexpected_end_of_input_error(parser.current_token);
    }

    analyze(&analyzer, 0);

    return ast;
}

static bool run_with_context(RunnerAction action, const void* data, MemoryGroup* memory)
{
    ErrorContext context;

    begin_error_context(&context, memory);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);

        return false;
    }

    bool success = action(data, context.previous_memory);

    end_error_context(&context);

    return success;
}

bool run_isolated(RunnerAction action, const void* data)
{
    MemoryGroup memory = {0};
    bool success = run_with_context(action, data, &memory);

    free_memory_group(&memory);

    return success;
}
