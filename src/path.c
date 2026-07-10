#include "path.h"
#include "util.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char* allocatePath(size_t length)
{
    char* path = malloc(length + 1);

    if (!path) {
        outOfMemoryError();
    }

    return path;
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

char* joinPath(const char* directory, const char* name)
{
    const char* separator = pathNeedsSeparator(directory) ? "\\" : "";
    size_t length = strlen(directory) + strlen(separator) + strlen(name);
    char* path = allocatePath(length);

    snprintf(path, length + 1, "%s%s%s", directory, separator, name);

    return path;
}

bool pathExists(const char* path)
{
    struct stat info;

    return stat(path, &info) == 0;
}

bool pathHasExtension(const char* path, const char* extension)
{
    size_t pathLength = strlen(path);
    size_t extensionLength = strlen(extension);

    if (pathLength < extensionLength) {
        return false;
    }

    const char* pathExtension = path + pathLength - extensionLength;

    return strcmp(pathExtension, extension) == 0;
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
