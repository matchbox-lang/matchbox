#ifndef ANALYZER_FUNCTION_H
#define ANALYZER_FUNCTION_H

#include "compiler/analyzer/analyzer.h"

typedef struct FunctionPath
{
    ASTNode* function;
    struct FunctionPath* previous;
} FunctionPath;

void register_function_overload(Scope* scope, ASTNode* ast);
void resolve_function_signature(Analyzer* analyzer, ASTNode* ast);
void analyze_parameter(Analyzer* analyzer, ASTNode* ast);
void analyze_function_call(Analyzer* analyzer, ASTNode* ast);
void analyze_function(Analyzer* analyzer, ASTNode* ast);
void analyze_return(Analyzer* analyzer, ASTNode* ast);

#endif
