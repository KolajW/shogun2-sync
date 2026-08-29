#ifndef KW_SHA1
#define KW_SHA1

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void sha1();

uint32_t leftRotate(uint32_t x, size_t offset);

//Chunk rep
typedef struct {
    uint32_t word[80];
} package;

void printS(uint32_t result[]);

#endif 