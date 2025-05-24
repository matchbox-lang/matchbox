#include "floatobject.h"
#include "object.h"

FloatObject* createFloatObject(float value)
{
    FloatObject* floatObject = ALLOCATE_OBJECT(FloatObject, OBJ_FLOAT);
    floatObject->value = value;

    return floatObject;
}
