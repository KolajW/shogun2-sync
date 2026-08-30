#include "kw_sha1.h"
#include <assert.h>
#include "zlib/zlib.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) _setmode(_fileno(file), _O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

void kw_compress(uint8_t *buff, size_t ML, char fileName[]);