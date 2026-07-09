#include "path.h"
#include "util.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool pathHasExtension(const char* path, const char* extension)
{
    size_t pathLength = strlen(path);
    size_t extensionLength = strlen(extension);

    if (pathLength < extensionLength) {
        return false;
    }

    return strcmp(path + pathLength - extensionLength, extension) == 0;
}

bool pathIsCurrentOrParentDirectory(const char* path)
{
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

bool pathIsDirectory(const char* path)
{
    struct stat info;

    return stat(path, &info) == 0 && (info.st_mode & S_IFDIR);
}

bool pathExists(const char* path)
{
    struct stat info;

    return stat(path, &info) == 0;
}

static bool pathEndsWithSeparator(const char* path)
{
    size_t length = strlen(path);

    if (length == 0) {
        return false;
    }

    if (path[length - 1] == '\\') {
        return true;
    }

    return path[length - 1] == '/';
}

static bool pathNeedsSeparator(const char* path)
{
    if (path[0] == '\0') {
        return false;
    }

    return !pathEndsWithSeparator(path);
}

static char* allocatePath(size_t length)
{
    char* path = malloc(length + 1);

    if (!path) {
        outOfMemoryError();
    }

    return path;
}

char* joinPath(const char* directory, const char* name)
{
    size_t length = strlen(directory) + strlen(name);
    bool needsSeparator = pathNeedsSeparator(directory);
    char* path;

    if (needsSeparator) {
        length++;
    }

    path = allocatePath(length);
    strcpy(path, directory);

    if (needsSeparator) {
        strcat(path, "\\");
    }

    strcat(path, name);

    return path;
}
