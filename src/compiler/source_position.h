#ifndef SOURCE_POSITION_H
#define SOURCE_POSITION_H

typedef struct SourcePosition
{
    const char* chars;
    int line;
    int column;
} SourcePosition;

#endif
