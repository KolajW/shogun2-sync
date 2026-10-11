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
        printf("Error 004: Compression init failed: %d\n", ret);
        return;
    }
    FILE *compressedSave;
    compressedSave = fopen(fileName, "wb");
    if (compressedSave == NULL) {
        printf("Error 002: File write failed\n");
        return;
    }
    do
    {
        z.next_out = new;
        z.avail_out = CHUNK;

        ret = deflate(&z, Z_FINISH);
        if (ret < 0) {
            printf("Error 003: Compression failed: %d\n", ret);
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

bool kw_decompress(uint8_t *buff, size_t ML, char fileName[]) {
    z_stream z = {0};
    int ret;
    uint8_t out[CHUNK];
    z.next_in = buff;
    z.avail_in = ML;

    z.zalloc = NULL;
    z.zfree = NULL;
    z.opaque = NULL;

    ret = inflateInit(&z);
    if (ret != Z_OK) {
        printf("Error 011: Decompression init failed: %d\n", ret);
        return 1;
    }

    FILE *decompressedSave = fopen(fileName, "wb");
    if (decompressedSave == NULL) {
        printf("Error 002: File write failed\n");
        inflateEnd(&z);
        return 1;
    }

    do {
        z.next_out = out;
        z.avail_out = CHUNK;

        ret = inflate(&z, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
            printf("Error 012: Decompression failed: %d\n", ret);
            fclose(decompressedSave);
            inflateEnd(&z);
            return 1;
        }

        size_t writtenBytes = CHUNK - z.avail_out;
        if (writtenBytes > 0) {
            fwrite(out, 1, writtenBytes, decompressedSave);
        }
    } while (ret != Z_STREAM_END && z.avail_out == 0);

    fclose(decompressedSave);
    inflateEnd(&z);

    if (ret != Z_STREAM_END) {
        printf("Error 013: Decompression incomplete: %d\n", ret);
        return 1;
    }

    return 0;
}

bool kw_decompress_file(char inFileName[], char outFileName[]) {
    FILE *f = fopen(inFileName, "rb");
    if (f == NULL) {
        printf("Error 001: Save file not found\n");
        return 1;
    }
    fseek(f, 0L, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    uint8_t *buf = malloc(size);
    if (buf == NULL) {
        fclose(f);
        printf("Error 010: Memory allocation failed\n");
        return 1;
    }
    fread(buf, 1, size, f);
    fclose(f);

    bool res = kw_decompress(buf, size, outFileName);
    free(buf);
    return res;
}