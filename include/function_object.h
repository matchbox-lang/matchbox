#ifndef FUNCTION_OBJECT_H
#define FUNCTION_OBJECT_H

#include "code_object.h"

#define AS_FUNCTION_OBJECT(value) ((FunctionObject*)AS_OBJECT(value))

typedef enum FunctionType
{
    FUNCTION_BUILTIN,
    FUNCTION_DEFINED
} FunctionType;

typedef struct VM VM;
typedef struct FunctionObject FunctionObject;

typedef void (*FunctionEntry)(
    VM* vm,
    FunctionObject* function,
    Value* frame
);

typedef struct FunctionObject
{
    Object obj;
    FunctionType type;
    FunctionEntry entry;
    CodeObject code;
    int localCount;
    int maxStackCount;
    int paramCount;
    int returnCount;
} FunctionObject;

FunctionObject* createFunctionObject();
void freeFunctionObject(FunctionObject* function);
void enterBytecodeFunction(VM* vm, FunctionObject* function, Value* frame);

#endif
