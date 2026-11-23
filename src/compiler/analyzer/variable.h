#ifndef ANALYZER_VARIABLE_H
#define ANALYZER_VARIABLE_H

#include "common/string_view.h"
#include "compiler/analyzer/analyzer.h"

ASTNode* find_symbol(Analyzer* analyzer, StringView id);
void validate_constant_binding(Analyzer* analyzer, ASTNode* symbol, Token token);
void validate_constant_reference(Analyzer* analyzer, ASTNode* symbol, Token token);
void validate_constant_mutation(ASTNode* symbol, ReferenceType type, Token token);
void analyze_variable(Analyzer* analyzer, ASTNode* ast);
void analyze_assignment(Analyzer* analyzer, ASTNode* ast);
void analyze_variable_definition(Analyzer* analyzer, ASTNode* ast);

#endif
