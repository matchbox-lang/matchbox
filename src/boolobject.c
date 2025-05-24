#include "boolobject.h"
#include "object.h"

BoolObject* createBoolObject(bool value)
{
    BoolObject* boolObject = ALLOCATE_OBJECT(BoolObject, OBJ_BOOL);
    boolObject->value = value;

    return boolObject;
}
