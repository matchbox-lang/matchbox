#include "cli/test_runner.h"
#include "cli/options.h"
#include "cli/path.h"
#include "common/errors.h"
#include "common/io.h"
#include "common/memory.h"
#include "compiler/analyzer.h"
#include "compiler/compiler.h"
#include "compiler/diagnostics.h"
#include "compiler/parser.h"
#include "compiler/scope.h"
#include "runtime/module_object.h"
#include "vm/vm.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestCase
{
    const char* path;
    char* name;
    size_t statement_index;
    int line;
    bool skipped;
} TestCase;

typedef struct TestRun
{
    TestOutput output;
    Vector paths;
    Vector cases;
    int passed;
    int skipped;
    int failed;
    int validation_failed;
} TestRun;

static void discover_path(TestRun* run, const char* path);

static bool should_print_result(TestOutput output, bool passed)
{
    if (output == TestOutputAll) {
        return true;
    }

    return (output == TestOutputPassed) == passed;
}

static void print_test_location(FILE* stream, const TestCase* test)
{
    const char* path = path_for_display(test->path);

    fprintf(stream, "%s", path);

    if (test->line > 0) {
        fprintf(stream, ":%d", test->line);
    }

    if (test->name) {
        fprintf(stream, " %s", test->name);
    }
}

static void print_test_result(const TestRun* run, const TestCase* test, bool passed)
{
    if (!should_print_result(run->output, passed)) {
        return;
    }

    const char* result = "FAIL";

    if (passed) {
        result = "PASS";
    }

    printf("%s ", result);
    print_test_location(stdout, test);
    printf("\n");
}

static void report_filtered_test_failure(const TestRun* run, const TestCase* test, bool passed)
{
    if (passed || run->output != TestOutputPassed) {
        return;
    }

    fputs("Error: Test failed at ", stderr);
    print_test_location(stderr, test);
    fputc('\n', stderr);
}

static void add_test_case(TestRun* run, const char* path, ASTNode* ast, size_t index)
{
    TestCase* test = allocate_zeroed_memory(1, sizeof(TestCase));

    test->path = path;
    test->statement_index = index;
    test->skipped = ast->function_definition.is_test_skipped;

    Token name = ast->function_definition.test_name;

    test->line = ast->function_definition.token.line;

    if (name.length > 0) {
        test->name = allocate_memory(name.length + 1);

        memcpy(test->name, name.chars, name.length);

        test->name[name.length] = '\0';
    }

    push_vector_item(&run->cases, test);
}

static void collect_test_case(TestRun* run, const char* path, ASTNode* ast, size_t index)
{
    bool test = ast->type == ASTFunctionDefinition && ast->function_definition.is_test;

    if (!test) {
        return;
    }

    add_test_case(run, path, ast, index);
}

static void collect_test_cases(TestRun* run, const char* path, Vector* nodes)
{
    for (size_t i = 0; i < nodes->count; i++) {
        ASTNode* ast = nodes->data[i];

        collect_test_case(run, path, ast, i);
    }
}

static char* read_test_source(const char* path)
{
    char* source = get_file_contents(path);

    if (!source) {
        const char* detail = strerror(errno);

        fatal_error("Could not read file %s: %s", path, detail);
    }

    return source;
}

static void discover_source_tests(TestRun* run, const char* path, ErrorContext* context)
{
    char* source = read_test_source(path);

    if (!source_has_test_keyword(source)) {
        return;
    }

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
    use_memory_group(context->previous_memory);
    collect_test_cases(run, path, &ast->compound.statements);
}

static bool discover_file_with_context(TestRun* run, const char* path, MemoryGroup* memory)
{
    ErrorContext context;

    begin_error_context(&context, memory);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);

        return false;
    }

    discover_source_tests(run, path, &context);
    end_error_context(&context);

    return true;
}

static void discover_file(TestRun* run, const char* path)
{
    if (path_has_extension(path, ".fail.mb")) {
        return;
    }

    MemoryGroup memory = {0};
    bool valid = discover_file_with_context(run, path, &memory);

    free_memory_group(&memory);

    if (!valid) {
        run->validation_failed++;

        report_error("Validation failed for %s", path);
    }
}

static void discover_directory_entry(TestRun* run, const char* path, const char* name)
{
    if (path_is_current_or_parent_directory(name)) {
        return;
    }

    char* child = join_path(path, name);

    discover_path(run, child);
    free_path(child);
}

static void discover_directory(TestRun* run, const char* path)
{
    DIR* directory = opendir(path);
    if (!directory) {
        run->validation_failed++;

        report_error("Could not read directory %s", path);

        return;
    }

    struct dirent* entry;

    errno = 0;

    while ((entry = readdir(directory))) {
        discover_directory_entry(run, path, entry->d_name);

        errno = 0;
    }

    int read_error = errno;

    closedir(directory);

    if (read_error != 0) {
        run->validation_failed++;

        report_error("Could not read directory %s", path);
    }
}

static void discover_path(TestRun* run, const char* path)
{
    if (path_is_directory(path)) {
        discover_directory(run, path);

        return;
    }

    if (!path_has_extension(path, ".mb")) {
        return;
    }

    size_t length = strlen(path);
    char* copy = allocate_memory(length + 1);

    memcpy(copy, path, length + 1);
    push_vector_item(&run->paths, copy);
}

static int compare_paths(const void* left, const void* right)
{
    const char* left_path = *(const char* const*)left;
    const char* right_path = *(const char* const*)right;

    return strcmp(left_path, right_path);
}

static bool run_test_block(const TestCase* test)
{
    char* source = read_test_source(test->path);
    ModuleObject* module = create_module_object();
    Compiler compiler;
    VM vm;

    init_compiler(&compiler, module);

    if (!compile_test(&compiler, source, test->statement_index)) {
        unexpected_end_of_input_error(compiler.parser.current_token);
    }

    init_vm(&vm, module);

    int status = interpret(&vm);

    return status == EXIT_SUCCESS;
}

static bool run_block_with_context(const TestCase* test, MemoryGroup* memory)
{
    ErrorContext context;
    
    begin_error_context(&context, memory);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);

        return false;
    }

    bool passed = run_test_block(test);

    end_error_context(&context);

    return passed;
}

static bool run_isolated_block(const TestCase* test)
{
    MemoryGroup memory = {0};
    bool passed = run_block_with_context(test, &memory);

    free_memory_group(&memory);

    return passed;
}

static void skip_test_case(TestRun* run, const TestCase* test)
{
    run->skipped++;

    if (run->output != TestOutputAll) {
        return;
    }

    printf("SKIP ");
    print_test_location(stdout, test);
    printf("\n");
}

static void run_test_case(TestRun* run, const TestCase* test)
{
    if (test->skipped) {
        skip_test_case(run, test);

        return;
    }

    fflush(stdout);

    bool passed = run_isolated_block(test);

    report_filtered_test_failure(run, test, passed);
    print_test_result(run, test, passed);

    if (passed) {
        run->passed++;

        return;
    }

    run->failed++;
}

static void free_test_run(TestRun* run)
{
    for (size_t i = 0; i < run->cases.count; i++) {
        TestCase* test = run->cases.data[i];

        free_memory(test->name);
        free_memory(test);
    }

    for (size_t i = 0; i < run->paths.count; i++) {
        char* path = run->paths.data[i];

        free_memory(path);
    }

    free_vector(&run->cases);
    free_vector(&run->paths);
}

static bool validate_test_path(const char* path)
{
    if (!path_exists(path)) {
        report_error("Could not find path %s", path);

        return false;
    }

    if (!path_is_directory(path) && !path_has_extension(path, ".mb")) {
        report_error("Expected .mb file but found %s", path);

        return false;
    }

    return true;
}

static const char* get_test_path(const Options* options)
{
    if (options->test_path) {
        return options->test_path;
    }

    return ".";
}

static void init_test_run(TestRun* run, TestOutput output)
{
    *run = (TestRun){.output = output};

    init_vector(&run->paths);
    init_vector(&run->cases);
}

static void discover_tests(TestRun* run, const char* path)
{
    discover_path(run, path);
    qsort(run->paths.data, run->paths.count, sizeof(void*), compare_paths);

    for (size_t i = 0; i < run->paths.count; i++) {
        const char* filename = run->paths.data[i];

        discover_file(run, filename);
    }
}

static void run_test_cases(TestRun* run)
{
    printf("Running %lu test blocks...\n", (unsigned long)run->cases.count);
    fflush(stdout);

    for (size_t i = 0; i < run->cases.count; i++) {
        TestCase* test = run->cases.data[i];

        run_test_case(run, test);
    }
}

static int report_test_run(const TestRun* run, const char* path)
{
    printf("%d passed; %d failed; %d skipped.\n", run->passed, run->failed, run->skipped);

    if (run->validation_failed > 0) {
        report_error("%d paths failed validation", run->validation_failed);
    }

    if (run->cases.count == 0) {
        report_error("No test found in %s", path);

        return EXIT_FAILURE;
    }

    if (run->failed > 0 || run->validation_failed > 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int run_tests(const Options* options)
{
    const char* path = get_test_path(options);

    if (!validate_test_path(path)) {
        return EXIT_FAILURE;
    }

    TestRun run;

    init_test_run(&run, options->test_output);
    discover_tests(&run, path);
    run_test_cases(&run);

    int status = report_test_run(&run, path);

    free_test_run(&run);

    return status;
}
