#include "service.h"
#include "token.h"
#include <string.h>

static Service services[] = {
    {"exit", 0, 0, {}, T_NONE},
    {"print", 1, 1, {T_INT}, T_NONE},
    {"clamp", 2, 3, {T_INT, T_INT, T_INT}, T_INT},
    {"abs", 3, 1, {T_INT}, T_INT},
    {"min", 4, 2, {T_INT, T_INT}, T_INT},
    {"max", 5, 2, {T_INT, T_INT}, T_INT},
    {"byteorder", 6, 0, {}, T_INT}
};

Service* getServiceByName(char* name)
{
    for (int i = 0; i < SYS_SIZE; i++) {
        Service* service = &services[i];

        if (strcmp(name, service->name) == 0) {
            return service;
        }
    }

    return NULL;
}
