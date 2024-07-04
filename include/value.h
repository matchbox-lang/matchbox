#ifndef VALUE_H
#define VALUE_H

#include <stdbool.h>
#include <stddef.h>

#define ARRAY_INIT_CAPACITY 4

#define BOOL_VALUE(value) ((Value){.boolVal = value})
#define FLOAT_VALUE(value) ((Value){.floatVal = value})
#define INT_VALUE(value) ((Value){.intVal = value})
#define POINTER_VALUE(ptr) ((Value){.pointer = ptr})

#define AS_BOOL(value) ((value).boolVal)
#define AS_FLOAT(value) ((value).floatVal)
#define AS_INT(value) ((value).intVal)
#define AS_POINTER(value) ((value).pointer)

typedef struct Object Object;

typedef union Value
{
    bool boolVal;
    float floatVal;
    int intVal;
    void* pointer;
} Value;

typedef struct ValueArray
{
    Value* values;
    size_t capacity;
    size_t count;
} ValueArray;

ValueArray* initValueArray(ValueArray* array);
void freeValueArray(ValueArray* array);
size_t countValueArray(ValueArray* array);
void resizeValueArray(ValueArray* array, size_t capacity);
void writeValueArray(ValueArray* array, Value value);

#endif
