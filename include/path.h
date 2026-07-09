#ifndef PATH_H
#define PATH_H

#include <stdbool.h>

bool pathHasExtension(const char* path, const char* extension);
bool pathIsCurrentOrParentDirectory(const char* path);
bool pathIsDirectory(const char* path);
bool pathExists(const char* path);
char* joinPath(const char* directory, const char* name);

#endif
