#ifndef ANALYZER_EXPRESSION_H
#define ANALYZER_EXPRESSION_H

#include "compiler/analyzer/analyzer.h"

Type analyze_binary_operand_types(Token operator, Type left_type, Type right_type, Type* operand_type);
void analyze_binary(Analyzer* analyzer, ASTNode* ast);
void analyze_prefix(Analyzer* analyzer, ASTNode* ast);
void validate_prefix_operand_type(Token operator, Type type);

#endif
