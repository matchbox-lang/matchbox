#ifndef BUILTIN_H
#define BUILTIN_H

#include "common/type.h"
#include "runtime/function_object.h"
#include "runtime/value.h"

#define BUILTINS_MAX 99
#define BUILTIN_PARAMS_MAX 4
#define BUILTIN_VARIADIC -1
#define BUILTIN_TYPED_VARIADIC -2

typedef struct Builtin
{
    const char* name;
    FunctionEntry entry;
    int param_count;
    TypeKind params[BUILTIN_PARAMS_MAX];
    TypeKind return_type_id;
    bool constant_evaluable;
} Builtin;

bool is_builtin_name(const char* name);
const Builtin* resolve_builtin(const char* name, const Type* argument_types, size_t argument_count);

#endif
