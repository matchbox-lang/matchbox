#ifndef PATH_H
#define PATH_H

#include <stdbool.h>

char* join_path(const char* directory, const char* name);
void free_path(char* path);
const char* path_for_display(const char* path);
bool path_exists(const char* path);
bool path_has_extension(const char* path, const char* extension);
bool path_is_current_or_parent_directory(const char* path);
bool path_is_directory(const char* path);

#endif
