#ifndef VALUE_H
#define VALUE_H

#include "common/data_type.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOOL_VALUE(value) ((Value){.bool_value = value})
#define SIGNED_VALUE(value) ((Value){.signed_value = value})
#define UNSIGNED_VALUE(value) ((Value){.unsigned_value = value})
#define F32_VALUE(value) ((Value){.f32_value = value})
#define F64_VALUE(value) ((Value){.f64_value = value})
#define POINTER_VALUE(ptr) ((Value){.pointer_value = ptr})

#define I8_VALUE(value) SIGNED_VALUE((int8_t)(value))
#define I16_VALUE(value) SIGNED_VALUE((int16_t)(value))
#define I32_VALUE(value) SIGNED_VALUE((int32_t)(value))
#define U8_VALUE(value) UNSIGNED_VALUE((uint8_t)(value))
#define U16_VALUE(value) UNSIGNED_VALUE((uint16_t)(value))
#define U32_VALUE(value) UNSIGNED_VALUE((uint32_t)(value))

#define AS_BOOL(value) ((value).bool_value)
#define AS_SIGNED(value) ((value).signed_value)
#define AS_UNSIGNED(value) ((value).unsigned_value)
#define AS_F32(value) ((value).f32_value)
#define AS_F64(value) ((value).f64_value)
#define AS_POINTER(value) ((value).pointer_value)

#define AS_I8(value) ((int8_t)AS_SIGNED(value))
#define AS_I16(value) ((int16_t)AS_SIGNED(value))
#define AS_I32(value) ((int32_t)AS_SIGNED(value))
#define AS_U8(value) ((uint8_t)AS_UNSIGNED(value))
#define AS_U16(value) ((uint16_t)AS_UNSIGNED(value))
#define AS_U32(value) ((uint32_t)AS_UNSIGNED(value))

#if UINTPTR_MAX == UINT32_MAX
typedef union Value
{
    bool bool_value;
    int32_t signed_value;
    uint32_t unsigned_value;
    float f32_value;
    void* pointer_value;
} Value;
#elif UINTPTR_MAX == UINT64_MAX
typedef union Value
{
    bool bool_value;
    int64_t signed_value;
    uint64_t unsigned_value;
    float f32_value;
    double f64_value;
    void* pointer_value;
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

void init_value_array(ValueArray* array);
void free_value_array(ValueArray* array);
int32_t read_signed_value(Value value, DataType type);
uint32_t read_unsigned_value(Value value, DataType type);
int64_t read_i64(const Value* frame, size_t position);
uint64_t read_u64(const Value* frame, size_t position);
double read_f64(const Value* frame, size_t position);
void write_i64(Value* frame, int64_t value);
void write_f64(Value* frame, double value);
size_t count_value_array(const ValueArray* array);
void reserve_value_array(ValueArray* array, size_t capacity);
void resize_value_array(ValueArray* array, size_t size);
size_t push_value(ValueArray* array, Value value);
void* get_value_as_pointer(const ValueArray* array, size_t index);
void set_value_at(ValueArray* array, size_t index, Value item);

#endif
