#include "endian.h"

bool isLittleEndian()
{
    int i = 1;
    char *c = (char*)&i;

    return (bool)*c;
}
