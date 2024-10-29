#ifndef CHUNK_H
#define CHUNK_H

#include "function.h"
#include "value.h"
#include <stddef.h>
#include <stdint.h>

typedef struct Chunk
{
    uint8_t* data;
    size_t capacity;
    size_t count;
    FunctionArray functions;
    ValueArray globals;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
size_t countChunk(Chunk* chunk);
void reserveChunk(Chunk* chunk, size_t capacity);
void pushByte(Chunk* chunk, uint8_t byte);
uint8_t getByteAt(Chunk* chunk, size_t index);
void setByteAt(Chunk* chunk, size_t index, uint8_t byte);
uint8_t* chunkBegin(Chunk* chunk);
uint8_t* chunkEnd(Chunk* chunk);

#endif
