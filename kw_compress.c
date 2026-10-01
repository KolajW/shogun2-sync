#include "kw_compress.h"

void kw_compress(uint8_t *buff, size_t ML, char fileName[]) {
    z_stream z = {0};
    int ret;
    uint8_t new[CHUNK];
    z.next_in = buff;
    z.avail_in = ML;
    z.next_out = new;
    z.avail_out = CHUNK;

    z.zalloc = NULL;
    z.zfree = NULL;
    z.opaque = NULL;
    ret = deflateInit(&z,Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
        printf("Error 004 COMPRESSION INIT PROBLEM: %d\n", ret);
        return;
    }
    FILE *compressedSave;
    compressedSave = fopen(fileName, "wb");
    if (compressedSave == NULL) {
        printf("Error 002 FILE WRITING PROBLEM\n");
        return;
    }
    do
    {
        z.next_out = new;
        z.avail_out = CHUNK;

        ret = deflate(&z, Z_FINISH);
        if (ret < 0) {
            printf("Error 003 COMPRESSION PROBLEM: %d\n", ret);
            fclose(compressedSave);
            deflateEnd(&z);
            return;
        }

        size_t savedBytes = CHUNK - z.avail_out;
        fwrite(new,1,savedBytes,compressedSave);
    } while (z.avail_out == 0);
    
    fclose(compressedSave);
    deflateEnd(&z);
}
