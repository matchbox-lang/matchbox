#ifndef COMPILER_H
#define COMPILER_H

#include "chunk.h"

void initCompiler();
void compile(char* source, Chunk* chunk);

#endif