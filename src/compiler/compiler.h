#ifndef COMPILER_H
#define COMPILER_H

#include "common/vector.h"
#include "compiler/analyzer/analyzer.h"
#include "compiler/parser/parser.h"
#include "compiler/ast.h"
#include "runtime/function_object.h"
#include "runtime/module_object.h"
#include <stddef.h>

typedef struct LoopJumps LoopJumps;

typedef enum CompileResult
{
    CompileError,
    CompileIncomplete,
    CompileSuccess
} CompileResult;

typedef struct Compiler
{
    Parser parser;
    Analyzer analyzer;
    Vector function_references;
    Vector omitted_statements;
    Vector repl_submission_boundaries;
    ModuleObject* module;
    FunctionObject* function;
    ASTNode* ast;
    LoopJumps* loop_jumps;
    size_t statement_index;
    size_t peephole_barrier;
    int register_count;
    int frame_base_count;
    int local_position_offset;
    bool repl_mode;
    bool test_mode;
    size_t test_index;
} Compiler;

void init_compiler(Compiler* compiler, ModuleObject* module);
void free_compiler(Compiler* compiler);
bool compile(Compiler* compiler, const char* source);
bool compile_test(Compiler* compiler, const char* source, size_t test_index);
CompileResult compile_repl(Compiler* compiler, const char* source);

#endif
