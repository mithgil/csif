#include <inttypes.h>
#include <stdio.h>

int main() {
    int8_t  a = -10;
    uint8_t b = 200;
    int16_t c = -1000;
    uint16_t d = 50000;
    int32_t e = -100000;
    uint32_t f = 4000000000;
    int64_t g = -10000000000;
    uint64_t h = 10000000000000000000ULL;
    
    printf("int8_t:  %" PRId8 "\n", a);
    printf("uint8_t: %" PRIu8 "\n", b);
    printf("int16_t: %" PRId16 "\n", c);
    printf("uint16_t: %" PRIu16 "\n", d);
    printf("int32_t: %" PRId32 "\n", e);
    printf("uint32_t: %" PRIu32 "\n", f);
    printf("int64_t: %" PRId64 "\n", g);
    printf("uint64_t: %" PRIu64 "\n", h);
    
    // 十六進制輸出
    printf("hex: 0x%" PRIx64 "\n", h);
    printf("HEX: 0x%" PRIX64 "\n", h);
    
    return 0;
}