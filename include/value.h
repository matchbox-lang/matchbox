#ifndef VALUE_H
#define VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOOL_VALUE(value) ((Value){.boolValue = value})
#define SIGNED_VALUE(value) ((Value){.signedValue = value})
#define UNSIGNED_VALUE(value) ((Value){.unsignedValue = value})
#define F32_VALUE(value) ((Value){.f32Value = value})
#define F64_VALUE(value) ((Value){.f64Value = value})
#define POINTER_VALUE(ptr) ((Value){.pointerValue = ptr})

#define I8_VALUE(value) SIGNED_VALUE((int8_t)(value))
#define I16_VALUE(value) SIGNED_VALUE((int16_t)(value))
#define I32_VALUE(value) SIGNED_VALUE((int32_t)(value))
#define U8_VALUE(value) UNSIGNED_VALUE((uint8_t)(value))
#define U16_VALUE(value) UNSIGNED_VALUE((uint16_t)(value))
#define U32_VALUE(value) UNSIGNED_VALUE((uint32_t)(value))

#define AS_BOOL(value) ((value).boolValue)
#define AS_SIGNED(value) ((value).signedValue)
#define AS_UNSIGNED(value) ((value).unsignedValue)
#define AS_F32(value) ((value).f32Value)
#define AS_F64(value) ((value).f64Value)
#define AS_POINTER(value) ((value).pointerValue)

#define AS_I8(value) ((int8_t)AS_SIGNED(value))
#define AS_I16(value) ((int16_t)AS_SIGNED(value))
#define AS_I32(value) ((int32_t)AS_SIGNED(value))
#define AS_U8(value) ((uint8_t)AS_UNSIGNED(value))
#define AS_U16(value) ((uint16_t)AS_UNSIGNED(value))
#define AS_U32(value) ((uint32_t)AS_UNSIGNED(value))

#if UINTPTR_MAX == UINT32_MAX
typedef union Value
{
    bool boolValue;
    int32_t signedValue;
    uint32_t unsignedValue;
    float f32Value;
    void* pointerValue;
} Value;
#elif UINTPTR_MAX == UINT64_MAX
typedef union Value
{
    bool boolValue;
    int64_t signedValue;
    uint64_t unsignedValue;
    float f32Value;
    double f64Value;
    void* pointerValue;
} Value;
#else
#error Unsupported pointer width
#endif

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
void* getValueAsPointer(ValueArray* array, size_t index);
void setValueAt(ValueArray* array, size_t index, Value item);

#endif
