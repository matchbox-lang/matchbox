#ifndef COMPILER_H
#define COMPILER_H

#include "common/vector.h"
#include "compiler/analyzer.h"
#include "compiler/ast.h"
#include "compiler/parser.h"
#include "runtime/function_object.h"
#include "runtime/module_object.h"
#include <stddef.h>

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
    ModuleObject* module;
    FunctionObject* function;
    ASTNode* ast;
    size_t statement_index;
    size_t peephole_barrier;
    int register_count;
    int frame_base_count;
    int local_position_offset;
    bool repl;
} Compiler;

void init_compiler(Compiler* compiler, ModuleObject* module);
void free_compiler(Compiler* compiler);
bool compile(Compiler* compiler, const char* source);
CompileResult compile_repl(Compiler* compiler, const char* source);

#endif
