#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "functionobject.h"
#include "moduleobject.h"
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
void compile(Compiler* compiler, char* source);

#endif
