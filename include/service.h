#ifndef SVC_H
#define SVC_H

#define SERVICES_MAX 7

typedef struct Service
{
    char* name;
    int opcode;
    int paramCount;
    int params[4];
    int typeId;
} Service;

typedef enum ServiceOpcode
{
    SOP_EXIT,
    SOP_PRINT,
    SOP_CLAMP,
    SOP_ABS,
    SOP_MIN,
    SOP_MAX,
    SOP_BYTEORDER
} ServiceOpcode;

Service* getServiceByName(char* name);

#endif
