#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "function_object.h"
#include "module_object.h"
#include "parser.h"
#include "vector.h"
#include <stddef.h>

typedef struct Compiler
{
    Parser parser;
    Vector functionReferences;
    ModuleObject* module;
    FunctionObject* function;
    AST* ast;
    size_t statementIndex;
    int stackCount;
} Compiler;

void initCompiler(Compiler* compiler, ModuleObject* module);
void freeCompiler(Compiler* compiler);
bool compile(Compiler* compiler, char* source);
bool compileRepl(Compiler* compiler, char* source);

#endif
