#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "defines.h"
#include "ext4.h"

#define PTR_TYPE uint16_t

#include "tokenizer.h"

typedef struct {
    char* left;
    char* right;
} BytePair;

ext4_FIL vocab;
ext4_FIL dict;
uint16_t vocab_len;
uint32_t dict_len;

char* tok_buffer = (char*)HIGHMEM_START;
#define TOK_BUFFER_END (HIGHMEM_START+TOK_BUFFER_LEN)
BytePair* str_pairs = (BytePair*)TOK_BUFFER_END;
#define STR_PAIRS_END (TOK_BUFFER_END+TOK_MAX_PAIRS*sizeof(BytePair))
char* parts_buffer = (char*)STR_PAIRS_END;
#define PARTS_BUFFER_END (STR_PAIRS_END+TOK_BUFFER_LEN)
char** str_parts = (char**)PARTS_BUFFER_END;
#define STR_PARTS_END (PARTS_BUFFER_END+TOK_MAX_PARTS*2*sizeof(char*))
char* left_buff = (char*)STR_PARTS_END;
#define LEFT_BUFF_END (STR_PARTS_END+LEFTRIGHT_BUFF)
char* right_buff = (char*)LEFT_BUFF_END;
#define RIGHT_BUFF_END (LEFT_BUFF_END+LEFTRIGHT_BUFF)
char* word_buff = (char*)RIGHT_BUFF_END;

void bpe_init() {
    uint8_t p = get_page();
    set_page(0);
    uint64_t inode;
    uint32_t read;
    uint8_t ftype;
    uint8_t ret = ext4_find_file("/data/vocab.bpe", &inode, &ftype);
    if(ret != EXT4_OKAY || ext4_open_read(inode, &vocab) != EXT4_OKAY) {
        puts("\033[1;31mFATAL:\033[0m Could not open /data/vocab.bpe\r\n");
        fatal();
    }
    while(1) {
        ext4_read(&vocab, (uint8_t*)left_buff, LEFTRIGHT_BUFF*2, &read);
        if(read == 0) break;
        for(uint16_t i = 0; i < (uint16_t)read; i++) {
            if(left_buff[i] == '\n') {
                vocab_len++;
            }
        }
    }
    vocab_len--; //Account for newline at end of file
    sprintf(left_buff, "vocab.bpe determined to have %u entries\r\n", vocab_len);
    puts(left_buff);
    
    ret = ext4_find_file("/data/dict.bin", &inode, &ftype);
    if(ret != EXT4_OKAY || ext4_open_read(inode, &dict) != EXT4_OKAY) {
        puts("\033[1;31mFATAL:\033[0m Could not open /data/dict.bin\r\n");
        fatal();
    }
    dict_len = (uint32_t)dict.limit;
    sprintf(left_buff, "dict_len = %lu\r\n", (uint32_t)dict.limit);
    puts(left_buff);
    set_page(p);
}

uint8_t gen_pairs(char** str_parts, uint8_t num_parts) {
    char* ptr = tok_buffer;
    uint8_t i;
    for(i = 0; i < num_parts-1; i++) {
        if(i == TOK_MAX_PAIRS) {
            //printf("ERROR: TOK_MAX_PAIRS exceeded\r\n");
            return 255;
        }
        int len = strlen(str_parts[i]);
        if((ptr-tok_buffer) + len >= TOK_BUFFER_LEN) {
            //printf("ERROR: tok_buffer out of space\r\n");
            return 255;
        }
        memcpy(ptr, str_parts[i], len);
        str_pairs[i].left = ptr;
        ptr += len;
        *ptr = 0;
        ptr++;
        len = strlen(str_parts[i+1]);
        if((ptr-tok_buffer) + len >= TOK_BUFFER_LEN) {
            //printf("ERROR: tok_buffer out of space\r\n");
            return 255;
        }
        memcpy(ptr, str_parts[i+1], len);
        str_pairs[i].right = ptr;
        ptr += len;
        *ptr = 0;
        ptr++;
    }
    return i;
}

uint8_t bpe(const char* input, uint16_t* res_buff) {
    char read_buffer[32];
    char temp;
    uint16_t num_parts = strlen(input);
    char* ptr = parts_buffer;
    for(uint8_t i = 0; i < num_parts; i++) {
        ptr[0] = input[i];
        ptr[1] = 0;
        if(i == TOK_MAX_PARTS) goto buffer_overflow_fail_0;
        str_parts[i] = ptr;
        ptr += 2;
    }
    uint8_t num_pairs = gen_pairs(str_parts, num_parts);
    if(num_pairs == 255) goto buffer_overflow_fail_0;
#ifdef PRINTD_TOK
    uint16_t iter = 0;
#endif
    while(1) {
#ifdef PRINTD_TOK
        sprintf(left_buff, "Iteration %u\r\n", iter++);
        puts(left_buff);
        for(uint16_t k = 0; k < num_parts; k++) {
            sprintf(left_buff, "(%s) ", str_parts[k]);
            puts(left_buff);
        }
        puts("\r\n");
        for(uint16_t k = 0; k < num_pairs; k++) {
            sprintf(left_buff, "((%s) <-> (%s)) ", str_pairs[k].left, str_pairs[k].right);
            puts(left_buff);
        }
        puts("\r\n");
#endif
        BytePair* first = NULL;
        ext4_seek(&vocab, 0);
        uint8_t pos = 0;
        for(uint16_t i = 0; i < vocab_len; i++) {
            char* temp2 = left_buff;
            uint8_t check = 0;
            uint8_t flag = 0;
            while(1) {
                if((pos & 31) == 0) ext4_read(&vocab, (uint8_t*)read_buffer, 32, NULL);
                temp = read_buffer[pos & 31];
                pos++;
                if(temp == ' ' && !flag) {
                    flag = 1;
                    *temp2 = 0;
                    temp2 = right_buff;
                    check = 0;
                    continue;
                }
                if(temp == '\n' && flag) break;
                *temp2 = temp;
                temp2++;
                check++;
                if(check == LEFTRIGHT_BUFF) goto buffer_overflow_fail_0;
            }
            *temp2 = 0;
            for(uint16_t j = 0; j < num_pairs; j++) {
                BytePair* curr = str_pairs + j;
                if(strcmp(left_buff, curr->left) == 0 && strcmp(right_buff, curr->right) == 0) {
                    first = str_pairs + j;
                    i = vocab_len + 1;
                    break;
                }
            }
        }
        if(first == NULL) break;
        
        uint16_t i = 0;
        char** str_parts_new = str_parts + (TOK_MAX_PARTS/2);
        uint16_t num_new_parts = 0;
        char* old_ptr = ptr;
        while(i < num_parts) {
            uint16_t found = 0xFFFF;
            for(uint16_t k = i; k < num_parts; k++) {
                if(strcmp(str_parts[k], first->left) == 0) {
                    found = k;
                    break;
                }
            }
            uint16_t end = found == 0xFFFF ? num_parts : found;
            for(uint16_t k = i; k < end; k++) {
                if(num_new_parts == TOK_MAX_PARTS) goto buffer_overflow_fail_0;
                str_parts_new[num_new_parts++] = ptr;
                uint16_t len = strlen(str_parts[k]);
                memcpy(ptr, str_parts[k], len);
                ptr += len;
                *ptr = 0;
                ptr++;
            }
            if(found == 0xFFFF) break;
            i = found;
            
            if(strcmp(str_parts[i], first->left) == 0 && i < num_parts-1 && strcmp(str_parts[i+1], first->right) == 0) {
                if(num_new_parts == TOK_MAX_PARTS) goto buffer_overflow_fail_0;
                str_parts_new[num_new_parts++] = ptr;
                uint16_t len = strlen(first->left);
                memcpy(ptr, first->left, len);
                ptr += len;
                len = strlen(first->right);
                memcpy(ptr, first->right, len);
                ptr += len;
                *ptr = 0;
                ptr++;
                i += 2;
            }else {
                if(num_new_parts == TOK_MAX_PARTS) goto buffer_overflow_fail_0;
                str_parts_new[num_new_parts++] = ptr;
                uint16_t len = strlen(str_parts[i]);
                memcpy(ptr, str_parts[i], len);
                ptr += len;
                i++;
                *ptr = 0;
                ptr++;
            }
        }
        
        char* dest_ptr = parts_buffer;
        uint16_t diff = (PTR_TYPE)old_ptr - (PTR_TYPE)dest_ptr;
        for(uint16_t i = 0; i < num_new_parts; i++) {
            str_parts[i] = str_parts_new[i] - diff;
        }
        while(1) {
            *dest_ptr = *old_ptr;
            dest_ptr++;
            old_ptr++;
            if(old_ptr == ptr) break;
        }
        ptr = dest_ptr;
        num_parts = num_new_parts;
        if(num_new_parts == 1) break;
        num_pairs = gen_pairs(str_parts, num_parts);
        if(num_pairs == 255) goto buffer_overflow_fail_0;
    }
    ext4_seek(&dict, 0);
    uint32_t start;
    ext4_read(&dict, (uint8_t*)&start, 4, NULL);
    for(uint16_t i = 0; i < num_parts; i++) {
        char* part = str_parts[i];
        ext4_seek(&dict, (uint64_t)start);
        uint8_t flag = 0;
        uint8_t pos = 0;
        while(1) {
            uint16_t number;
            if((pos & 31) == 0) ext4_read(&dict, (uint8_t*)&number, 2, NULL);
            else if((pos & 31) == 31) {
                ext4_read(&dict, (uint8_t*)read_buffer, 1, NULL);
                number = (uint16_t)read_buffer[31] & 0xFF;
                number |= (uint16_t)read_buffer[0] << 8;
                pos = 0;
            }else {
                pos &= 31;
                number = (uint16_t)read_buffer[pos] & 0xFF;
                number |= (uint16_t)read_buffer[pos+1] << 8;
                pos += 2;
            }
            char* temp2 = left_buff;
            uint8_t check = 0;
            while(1) {
                if((uint32_t)dict.position >= dict_len) {
                    flag = 1;
                    break;
                }
                pos &= 31;
                if(pos == 0) ext4_read(&dict, (uint8_t*)read_buffer, 32, NULL);
                *temp2 = read_buffer[pos];
                pos++;
                if(*temp2 == 0) break;
                temp2++;
                check++;
                if(check == LEFTRIGHT_BUFF*2-1) {
                    while(*temp2) {
                        if(pos == 0) ext4_read(&dict, (uint8_t*)read_buffer, 32, NULL);
                        *temp2 = read_buffer[pos];
                        pos++;
                    }
                }
            }
            if(strcmp(left_buff, part) == 0 && !flag) {
                *res_buff = number + 1;
                res_buff++;
                break;
            }
            if(flag) {
                *res_buff = 0;
                res_buff++;
                break;
            }
        }
    }
    
    return num_parts;
buffer_overflow_fail_0:
    puts("\033[1;31mERROR:\033[0m Overflowed buffer\r\n");
    return 0;
}

const char special_chars[] = ",.-+_ \"/()%&$!?\n\t':;0123456789\\";
uint16_t tokenize(const char* input, uint16_t* output) {
    uint8_t p = get_page();
    set_page(0);
    uint16_t count = 0;
    uint8_t word_buff_pos = 0;
    char end_of_word = 1;
    for(uint16_t i = 0;;i++) {
        char next = input[i];
        if(next == 126) next = '-';
        if(next < 0) continue;
        const char* special_check = special_chars;
        char found = 0;
        while(*special_check && next != 1) {
            if(*special_check == next) {
                found = next;
                break;
            }
            special_check++;
        }
        
        if(word_buff_pos >= WORD_BUFF_LEN-1) {
            puts("\033[1;31mERROR:\033[0m Overflowed word buffer\r\n");
            set_page(p);
            return 0;
        }
        if(found || next == 0) {
            if(word_buff_pos != 0) {
                if(end_of_word) {
                    for(uint16_t i = word_buff_pos; i > 0; i--) word_buff[i] = word_buff[i - 1];
                    word_buff[0] = 126;
                    word_buff[word_buff_pos+1] = 0;
                }else word_buff[word_buff_pos] = 0;
                uint8_t diff = bpe(word_buff, output);
                output += diff;
                count += diff;
            }
            end_of_word = found == ' ' || found == '\n' || found == '\t';
            if(!end_of_word && next) {
                word_buff[0] = found;
                word_buff[1] = 0;
                uint8_t diff = bpe(word_buff, output);
                output += diff;
                count += diff;
            }
            word_buff_pos = 0;
        }else {
            word_buff[word_buff_pos++] = next;
        }
        if(next == 0) break;
    }
    set_page(p);
    return count;
}

const char null_token[] = "<|blank|>";
const char error_token[] = "<|error|>";
void untokenize(uint16_t input, char* output) {
    uint8_t p = get_page();
    set_page(0);
    uint8_t null_len = strlen(null_token);
    uint8_t error_len = strlen(error_token);
    uint16_t token = input;
    if(token == 0) {
        memcpy(output, null_token, null_len);
        output += null_len;
        set_page(p);
        goto untokenize_done;
    }
    ext4_seek(&dict, (uint64_t)(((uint32_t)token)<<2UL));
    uint32_t pos;
    ext4_read(&dict, (uint8_t*)&pos, 4, NULL);
    ext4_seek(&dict, (uint64_t)pos);
    uint16_t number;
    ext4_read(&dict, (uint8_t*)&number, 2, NULL);
    if(number != token-1) {
        memcpy(output, error_token, error_len);
        output += error_len;
        goto untokenize_done;
    }
    while(1) {
        ext4_read(&dict, (uint8_t*)output, 1, NULL);
        if(*output == 0) break;
        if(*output == '~') *output = ' ';
        output++;
    }
untokenize_done:
    *output = 0;
    set_page(p);
}
