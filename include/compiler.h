#ifndef COMPILER_H
#define COMPILER_H

#include "moduleobject.h"

void initCompiler(ModuleObject* module);
void freeCompiler();
void compile(char* source);

#endif
