#ifndef NATIVE_H
#define NATIVE_H

#include "value.h"

Value __exit(int argCount, Value* args);
Value __print(int argCount, Value* args);
Value __clamp(int argCount, Value* args);
Value __abs(int argCount, Value* args);
Value __min(int argCount, Value* args);
Value __max(int argCount, Value* args);
Value __byteorder(int argCount, Value* args);

#endif
