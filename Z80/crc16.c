#include <stdint.h>
#include "crc16.h"

uint16_t gen_crc16(const uint8_t *data, uint16_t size) {
    uint16_t out = 0;
    uint8_t bits_read = 0;
    while(size > 0) {
        uint16_t bit_flag = out >> 15;
        out <<= 1;
        out |= (*data >> bits_read) & 1;
        bits_read++;
        if(bits_read > 7) {
            bits_read = 0;
            data++;
            size--;
        }
        if(bit_flag) out ^= CRC16;
    }
    
    for(uint8_t i = 0; i < 16; i++) {
        uint16_t bit_flag = out >> 15;
        out <<= 1;
        if(bit_flag) out ^= CRC16;
    }
    
    uint16_t crc = 0;
    uint16_t j = 0x0001;
    for(uint16_t i = 0x8000; i != 0; i >>= 1, j <<= 1) {
        if((i & out) != 0) crc |= j;
    }
    return crc;
}
