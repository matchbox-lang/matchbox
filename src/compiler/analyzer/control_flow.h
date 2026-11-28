#ifndef ANALYZER_CONTROL_FLOW_H
#define ANALYZER_CONTROL_FLOW_H

#include "compiler/analyzer/analyzer.h"

typedef struct LoopPath
{
    ASTNode* ast;
    struct LoopPath* previous;
    Vector* entry_states;
} LoopPath;

void merge_branch_reference(Analyzer* analyzer, ASTNode* ast, ASTNode* result, bool first, bool has_expected_type,
    Token token);
Scope* enter_branch_scope(Analyzer* analyzer, ASTNode* branch, ASTNode* binding);
Type analyze_branch_contents(Analyzer* analyzer, ASTNode* branch, Type type, bool moves_result);
Type analyze_else_branch(Analyzer* analyzer, ASTNode* branch, Type type, bool moves_result);
void analyze_while(Analyzer* analyzer, ASTNode* ast);
void analyze_for(Analyzer* analyzer, ASTNode* ast);
void analyze_loop_control(Analyzer* analyzer, ASTNode* ast);
void validate_expression_branch(ASTNode* branch, Token token);
void analyze_conditional(Analyzer* analyzer, ASTNode* ast);

#endif
