#ifndef STRING_CONCATENATION_H
#define STRING_CONCATENATION_H

#include "compiler/token.h"
#include "runtime/string_object.h"
#include "runtime/value.h"

StringObject* concatenateStringParts(Value* values, const TokenType* types, size_t count);

#endif
