#include "mapserv.h"

int main()
{
    mapObj* map = msLoadMap("test.map", NULL, NULL);
    char* str = msWriteMapToString(map);
    printf("%s\n", str);
    msFree(str);
    msFreeMap(map);
}
