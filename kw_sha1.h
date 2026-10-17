#ifndef KW_SHA1
#define KW_SHA1

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void sha1(const uint8_t *message, size_t ML, uint32_t result[5]);

typedef struct {
    uint32_t word[80];
} package;

void printS(uint32_t result[]);

void resultToChar(uint32_t result[], char* resultChar, size_t ML);

#endif 