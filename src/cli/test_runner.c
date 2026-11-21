#include "cli/test_runner.h"
#include "cli/options.h"
#include "cli/path.h"
#include "cli/test_discovery.h"
#include "common/errors.h"
#include "common/io.h"
#include "common/memory.h"
#include "compiler/analyzer/analyzer.h"
#include "compiler/parser/parser.h"
#include "compiler/compiler.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/module_object.h"
#include "vm/vm.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>
#define DUP _dup
#define DUP2 _dup2
#define CLOSE _close
#define FILENO _fileno
#else
#include <unistd.h>
#define DUP dup
#define DUP2 dup2
#define CLOSE close
#define FILENO fileno
#endif

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

static bool should_print_result(TestOutput output, bool passed)
{
    if (output == TestOutputAll) {
        return true;
    }

    if (output == TestOutputSkipped) {
        return false;
    }

    return (output == TestOutputPassed) == passed;
}

static void print_test_location(FILE* stream, const TestCase* test)
{
    const char* path = strip_relative_path_prefix(test->path);

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
    bool filtered = run->output == TestOutputPassed || run->output == TestOutputSkipped;

    if (passed || !filtered) {
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
    char* source = read_file_contents(path);
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

static bool discover_test_file_with_context(TestRun* run, const char* path, MemoryGroup* memory)
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

static void discover_test_file(TestRun* run, const char* path)
{
    if (path_has_extension(path, ".fail" PROGRAM_FILE_EXTENSION)) {
        return;
    }

    MemoryGroup memory = {0};
    bool valid = discover_test_file_with_context(run, path, &memory);

    free_memory_group(&memory);

    if (!valid) {
        run->validation_failed++;

        const char* display_path = strip_relative_path_prefix(path);

        report_error("Validation failed for %s", display_path);
    }
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

static bool run_test_block_with_context(const TestCase* test, MemoryGroup* memory)
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

static bool run_isolated_test_block(const TestCase* test)
{
    MemoryGroup memory = {0};
    bool passed = run_test_block_with_context(test, &memory);

    free_memory_group(&memory);

    return passed;
}

static FILE* create_test_output_file(void)
{
#ifdef _WIN32
    char* path = _tempnam(NULL, "mbx");

    if (!path) {
        return NULL;
    }

    int flags = _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_TEMPORARY;
    int descriptor = _open(path, flags, _S_IREAD | _S_IWRITE);

    free(path);

    if (descriptor < 0) {
        return NULL;
    }

    FILE* output = _fdopen(descriptor, "w+b");

    if (!output) {
        _close(descriptor);
    }

    return output;
#else
    return tmpfile();
#endif
}

static FILE* begin_test_output_capture(int* saved_stdout)
{
    if (fflush(stdout) != 0) {
        report_error("Could not flush test output");

        return NULL;
    }

    FILE* output = create_test_output_file();

    if (!output) {
        report_error("Could not capture test output");

        return NULL;
    }

    int stdout_fd = FILENO(stdout);
    int output_fd = FILENO(output);

    *saved_stdout = DUP(stdout_fd);

    if (*saved_stdout < 0) {
        fclose(output);
        report_error("Could not save standard output");

        return NULL;
    }

    if (DUP2(output_fd, stdout_fd) < 0) {
        CLOSE(*saved_stdout);
        fclose(output);
        report_error("Could not redirect test output");

        return NULL;
    }

    return output;
}

static bool end_test_output_capture(int saved_stdout)
{
    bool flushed = fflush(stdout) == 0;
    int stdout_fd = FILENO(stdout);
    int restored = DUP2(saved_stdout, stdout_fd);

    CLOSE(saved_stdout);
    clearerr(stdout);

    if (restored < 0) {
        fatal_error("Could not restore standard output");
    }

    if (!flushed) {
        report_error("Could not flush captured test output");
    }

    return flushed;
}

static void print_captured_test_output(FILE* output)
{
    if (fseek(output, 0, SEEK_SET) != 0) {
        report_error("Could not rewind captured test output");

        return;
    }

    char buffer[4096];
    size_t count = fread(buffer, 1, sizeof(buffer), output);

    if (count > 0) {
        fputs("Captured output:\n", stderr);
    }

    while (count > 0) {
        fwrite(buffer, 1, count, stderr);

        count = fread(buffer, 1, sizeof(buffer), output);
    }

    if (ferror(output)) {
        report_error("Could not read captured test output");
    }
}

static bool run_test_with_captured_output(const TestCase* test)
{
    int saved_stdout;
    FILE* output = begin_test_output_capture(&saved_stdout);

    if (!output) {
        return false;
    }

    bool passed = run_isolated_test_block(test);
    bool flushed = end_test_output_capture(saved_stdout);

    passed = passed && flushed;

    if (!passed) {
        print_captured_test_output(output);
    }

    fclose(output);

    return passed;
}

static void skip_test_case(TestRun* run, const TestCase* test)
{
    run->skipped++;

    if (run->output != TestOutputAll && run->output != TestOutputSkipped) {
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

    bool passed = run_test_with_captured_output(test);

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

    if (!path_is_directory(path) && !path_has_extension(path, PROGRAM_FILE_EXTENSION)) {
        report_error("Expected %s file but found %s", PROGRAM_FILE_EXTENSION, path);

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

static bool discover_tests(TestRun* run, const char* path)
{
    run->validation_failed += discover_test_paths(&run->paths, path);

    for (size_t i = 0; i < run->paths.count; i++) {
        const char* filename = run->paths.data[i];

        discover_test_file(run, filename);
    }

    if (run->validation_failed > 0) {
        return false;
    }

    if (run->cases.count == 0) {
        report_error("No test blocks found in %s", path);

        return false;
    }

    return true;
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

static int report_test_run(const TestRun* run)
{
    printf("%d passed; %d failed; %d skipped.\n", run->passed, run->failed, run->skipped);

    if (run->failed > 0) {
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

    bool discovered = discover_tests(&run, path);

    if (!discovered) {
        free_test_run(&run);

        return EXIT_FAILURE;
    }

    run_test_cases(&run);

    int status = report_test_run(&run);

    free_test_run(&run);

    return status;
}
