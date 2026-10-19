#ifndef OUTPUT_H
#define OUTPUT_H

#include "token.h"
#include "value.h"

bool isPrintableType(TokenType type);
size_t writeValue(char* bytes, size_t capacity, Value* frame, TokenType type);
void printValues(Value* values, const TokenType* types, size_t count);

#endif
