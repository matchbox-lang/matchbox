#ifndef STRING_CONCATENATION_H
#define STRING_CONCATENATION_H

#include "string_object.h"
#include "token.h"
#include "value.h"

StringObject* concatenateStringValues(Value left, Value right, TokenType leftType, TokenType rightType);

#endif
