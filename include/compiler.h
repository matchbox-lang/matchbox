#ifndef COMPILER_H
#define COMPILER_H

#include "chunk.h"
#include "functionobject.h"

FunctionObject* compile(char* source);

#endif