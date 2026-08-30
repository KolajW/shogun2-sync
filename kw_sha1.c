#include "kw_sha1.h"

#define leftRotate(x, offset) (((x) << (offset)) | ((x) >> (32 - (offset))))

void sha1(const uint8_t *message, size_t ML, uint32_t result[5]){
    uint32_t H0 = 0x67452301;
    uint32_t H1 = 0xEFCDAB89;
    uint32_t H2 = 0x98BADCFE;
    uint32_t H3 = 0x10325476;
    uint32_t H4 = 0xC3D2E1F0;

    //Counting size in BITS
    uint64_t bits = (uint64_t)ML * 8;

    size_t diff = (ML + 1) % 64;
    if(diff<=56){
        diff = 56 - diff;
    }
    else{
        diff = 64 - diff + 56;
    }
    

    uint8_t *input = malloc(ML + 1 + diff + 8);

    memcpy(input, message, ML);
    
    input[ML] = 0x80;
    
    for (size_t i = 0; i < diff; i++)
    {
        input[ML + i + 1] = 0x0;
    }
    
    for (size_t i = 0; i < 8; i++)
    {
        input[ML + diff + 1 + i] = (bits >> (8 * (7-i))) & 0xFF;
    }
    
    size_t div = (ML + 1 + diff + 8)/64;
    //package *packages = malloc(div  * sizeof(package));
    package pg;
    //Break into 512 bits chunks
    for (size_t i = 0; i < div; i++)
    {
        for (size_t j = 0; j < 16; j++)
        {
            pg.word[j] = 0;
            for (size_t t = 0; t < 4; t++)
            {
                pg.word[j] = (pg.word[j] << 8) | input[j * 4 + i * 64 + t];
            }
        }

        //Extend to 80 Words
        for (size_t c = 16; c < 80; c++)
        {
           pg.word[c] = leftRotate((pg.word[c-3]^pg.word[c-8]^pg.word[c-14]^pg.word[c-16]),1);
        }

        //Hash Values
        uint32_t a = H0;
        uint32_t b = H1;
        uint32_t c = H2;
        uint32_t d = H3;
        uint32_t e = H4;

        

        //Main Loop
        for (size_t j = 0; j < 80; j++)
        {
            uint32_t f = 0;
            uint32_t k = 0;
            if (j >= 0 && j <= 19)
            {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }
            else if (j >= 20 && j <= 39){
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (j >= 40 && j <= 59){
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else if (j >= 60 && j <= 79){
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            uint32_t temp = leftRotate(a,5) + f + e + k + pg.word[j];
            e = d;
            d = c;
            c = leftRotate(b,30);
            b = a;
            a = temp;
        }

        H0 = H0 + a;
        H1 = H1 + b;
        H2 = H2 + c;
        H3 = H3 + d;
        H4 = H4 + e;
        
    }

    result[0] = H0;
    result[1] = H1;
    result[2] = H2;
    result[3] = H3;
    result[4] = H4;

    //free(packages);
    free(input);
}

void printS(uint32_t result[]){
    for (size_t i = 0; i < 5; i++)
    {
        printf("%08x ",result[i]);
    }
    printf("\n");
    
}

void resultToChar(uint32_t result[], char* resultChar, size_t ML){
    int offset = 0;
    for (size_t i = 0; i < 5; i++) {
        offset += snprintf(resultChar + offset, ML - offset, "%08x", result[i]);
    }
}

/*uint32_t leftRotate(uint32_t x, size_t offset){
    return ((x<<offset) | (x>>(32-offset)));
}*/