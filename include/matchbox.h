#ifndef MATCHBOX_H
#define MATCHBOX_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Matchbox Matchbox;

Matchbox* matchbox_create(void);
void matchbox_destroy(Matchbox* mb);
bool matchbox_run(Matchbox* mb, const char* source);

#ifdef __cplusplus
}
#endif

#endif
