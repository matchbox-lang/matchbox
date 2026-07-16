#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "function_object.h"
#include "module_object.h"
#include "parser.h"
#include "analyzer.h"
#include "vector.h"
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
    int registerCount;
    int frameBaseCount;
} Compiler;

void initCompiler(Compiler* compiler, ModuleObject* module);
void freeCompiler(Compiler* compiler);
bool compile(Compiler* compiler, char* source);
bool compileRepl(Compiler* compiler, char* source);

#endif
