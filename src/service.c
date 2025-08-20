#include "service.h"
#include "token.h"
#include <string.h>

Service services[SERVICES_MAX] = {
    {"exit", SOP_EXIT, 0, {}, T_NONE},
    {"print", SOP_PRINT, 1, {T_INT}, T_NONE},
    {"clamp", SOP_CLAMP, 3, {T_INT, T_INT, T_INT}, T_INT},
    {"abs", SOP_ABS, 1, {T_INT}, T_INT},
    {"min", SOP_MIN, 2, {T_INT, T_INT}, T_INT},
    {"max", SOP_MAX, 2, {T_INT, T_INT}, T_INT},
    {"byteorder", SOP_BYTEORDER, 0, {}, T_INT}
};

Service* getServiceByName(char* name)
{
    for (int i = 0; i < SERVICES_MAX; i++) {
        Service* service = &services[i];

        if (strcmp(name, service->name) == 0) {
            return service;
        }
    }

    return NULL;
}
