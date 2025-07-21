#ifndef NATIVE_H
#define NATIVE_H

#include "value.h"

Value __exit(Value* args);
Value __print(Value* args);
Value __clamp(Value* args);
Value __abs(Value* args);
Value __min(Value* args);
Value __max(Value* args);
Value __byteorder(Value* args);

#endif
