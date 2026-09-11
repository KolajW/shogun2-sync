#include "kw_compress.h"
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#endif


void test(char saveName[]);
void diff();
bool init();