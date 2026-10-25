#ifndef CONVERSION_H
#define CONVERSION_H

#include "compiler/ast.h"
#include "compiler/token.h"
#include <stdint.h>

uint64_t integerLiteralToValue(Token token);
uint64_t binaryLiteralToValue(Token token);
uint64_t hexadecimalLiteralToValue(Token token);
uint64_t octalLiteralToValue(Token token);
double floatLiteralToValue(Token token);

bool canImplicitlyWidenInteger(TokenType source, TokenType destination);
bool canImplicitlyConvertIntegerToFloat(TokenType source, TokenType destination);
bool canImplicitlyWidenFloat(TokenType source, TokenType destination);
bool canImplicitlyWidenType(TokenType source, TokenType destination);
bool canExplicitlyConvertType(TokenType source, TokenType destination);
TokenType getCommonNumericType(TokenType left, TokenType right);
void foldExplicitConversion(ASTNode* ast);

#endif
