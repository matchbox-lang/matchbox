#include "intobject.h"
#include "object.h"

IntObject* createIntObject(int value)
{
    IntObject* intObject = ALLOCATE_OBJECT(IntObject, OBJ_INT);
    intObject->value = value;

    return intObject;
}
