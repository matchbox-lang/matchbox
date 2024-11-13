#ifndef SERVICE_H
#define SERVICE_H

typedef struct Service
{
    char* name;
    int opcode;
    int paramCount;
    int params[4];
    int typeId;
} Service;

typedef enum ServiceCode
{
    SYS_EXIT,
    SYS_PRINT,
    SYS_CLAMP,
    SYS_ABS,
    SYS_MIN,
    SYS_MAX,
    SYS_BYTEORDER,
    SYS_SIZE
} ServiceCode;

Service* getServiceByName(char* name);

#endif
