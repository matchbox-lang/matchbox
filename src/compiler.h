#ifndef COMPILER_H
#define COMPILER_H

#include "compiler/ast.h"
#include "runtime/function_object.h"
#include "runtime/module_object.h"
#include "compiler/parser.h"
#include "compiler/analyzer.h"
#include "common/vector.h"
#include <stddef.h>

typedef struct Compiler
{
    Parser parser;
    Analyzer analyzer;
    Vector functionReferences;
    ModuleObject* module;
    FunctionObject* function;
    ASTNode* ast;
    size_t statementIndex;
    size_t peepholeBarrier;
    int registerCount;
    int frameBaseCount;
    int localPositionOffset;
    bool repl;
} Compiler;

void initCompiler(Compiler* compiler, ModuleObject* module);
void freeCompiler(Compiler* compiler);
bool compile(Compiler* compiler, char* source);
bool compileRepl(Compiler* compiler, char* source);

#endif
