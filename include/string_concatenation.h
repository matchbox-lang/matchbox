#ifndef STRING_CONCATENATION_H
#define STRING_CONCATENATION_H

#include "string_object.h"
#include "token.h"
#include "value.h"

StringObject* concatenateStringParts(Value* values, const TokenType* types, size_t count);

#endif
