#ifndef VALUE_H
#define VALUE_H

#include <stdbool.h>
#include <stddef.h>

#define BOOL_VALUE(value) ((Value){.boolValue = value})
#define FLOAT_VALUE(value) ((Value){.floatValue = value})
#define INT_VALUE(value) ((Value){.intValue = value})
#define POINTER_VALUE(ptr) ((Value){.pointer = ptr})

#define AS_BOOL(value) ((value).boolValue)
#define AS_FLOAT(value) ((value).floatValue)
#define AS_INT(value) ((value).intValue)
#define AS_POINTER(value) ((value).pointer)

typedef union Value
{
    bool boolValue;
    float floatValue;
    int intValue;
    void* pointer;
} Value;

typedef struct ValueArray
{
    Value* data;
    size_t capacity;
    size_t count;
} ValueArray;

void initValueArray(ValueArray* array);
void freeValueArray(ValueArray* array);
size_t countValueArray(ValueArray* array);
void reserveValueArray(ValueArray* array, size_t capacity);
void resizeValueArray(ValueArray* array, size_t size);
size_t pushValue(ValueArray* array, Value value);
Value getValueAt(ValueArray* array, size_t index);
void setValueAt(ValueArray* array, size_t index, Value item);

#endif
