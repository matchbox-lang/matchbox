#ifndef COMPILER_H
#define COMPILER_H

#include "chunk.h"

void initCompiler(Chunk* chunk);
void compile(char* source);

#endif