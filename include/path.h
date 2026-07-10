#ifndef PATH_H
#define PATH_H

#include <stdbool.h>

char* joinPath(const char* directory, const char* name);
bool pathExists(const char* path);
bool pathHasExtension(const char* path, const char* extension);
bool pathIsCurrentOrParentDirectory(const char* path);
bool pathIsDirectory(const char* path);

#endif
