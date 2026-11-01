#ifndef FUNCTION_OBJECT_H
#define FUNCTION_OBJECT_H

#include "runtime/code_object.h"

#define AS_FUNCTION_OBJECT(value) ((FunctionObject*)AS_OBJECT(value))

typedef enum FunctionType
{
    FuncBuiltin,
    FuncDefined
} FunctionType;

typedef struct ArgumentFormat ArgumentFormat;
typedef struct VM VM;
typedef struct FunctionObject FunctionObject;

typedef void (*FunctionEntry)(
    VM* vm,
    const FunctionObject* function,
    Value* frame
);

typedef struct FunctionObject
{
    Object obj;
    FunctionType type;
    FunctionEntry entry;
    CodeObject code;
    int local_count;
    int max_stack_count;
    int param_count;
    int return_count;
    ArgumentFormat* argument_formats;
    size_t argument_count;
} FunctionObject;

FunctionObject* create_function_object();
void free_function_object(FunctionObject* function);
void enter_bytecode_function(VM* vm, const FunctionObject* function, Value* frame);

#endif
