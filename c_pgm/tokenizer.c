#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define PTR_TYPE uint64_t

#include "tokenizer.h"

typedef struct {
    char* left;
    char* right;
} BytePair;

FILE *vocab = NULL;
FILE *dict = NULL;
uint16_t vocab_len;
uint32_t dict_len;

void bpe_init() {
    vocab = fopen("../libtorch_llm/data/vocab.bpe", "rb");
    vocab_len = 0;
    fseek(vocab, 0L, SEEK_END);
    uint32_t sz = ftell(vocab);
    rewind(vocab);
    char buffer;
    while(ftell(vocab) < sz) {
        fread(&buffer,1,1,vocab);
        if(buffer == '\n') vocab_len++;
    }
    vocab_len--; //Account for NL at end of file
    printf("vocab.bpe determined to have %u entries\r\n", vocab_len);
    dict = fopen("../libtorch_llm/data/dict.bin", "rb");
    fseek(dict, 0L, SEEK_END);
    dict_len = ftell(dict);
    rewind(dict);
}

char tok_buffer[TOK_BUFFER_LEN];
BytePair str_pairs[TOK_MAX_PAIRS];
char parts_buffer[TOK_BUFFER_LEN];
char* str_parts[TOK_MAX_PARTS*2];
char left_buff[LEFTRIGHT_BUFF];
char right_buff[LEFTRIGHT_BUFF];
char word_buff[WORD_BUFF_LEN];

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
        printf("Iteration %u\r\n", iter++);
        for(uint16_t k = 0; k < num_parts; k++) printf("(%s) ", str_parts[k]);
        printf("\r\n");
        for(uint16_t k = 0; k < num_pairs; k++) {
            printf("((%s) <-> (%s)) ", str_pairs[k].left, str_pairs[k].right);
        }
        printf("\r\n");
#endif
        BytePair* first = NULL;
        rewind(vocab);
        for(uint16_t i = 0; i < vocab_len; i++) {
            char* temp2 = left_buff;
            uint8_t check = 0;
            while(1) {
                fread(&temp,1,1,vocab);  
                if(temp == ' ') break;
                *temp2 = temp;
                temp2++;
                check++;
                if(check == LEFTRIGHT_BUFF) goto buffer_overflow_fail_0;
            }
            *temp2 = 0;
            temp2 = right_buff;
            check = 0;
            while(1) {
                fread(&temp,1,1,vocab);  
                if(temp == '\n') break;
                *temp2 = temp;
                temp2++;
                check++;
                if(check == LEFTRIGHT_BUFF) goto buffer_overflow_fail_0;
            }
            *temp2 = 0;
            for(uint16_t j = 0; j < num_pairs; j++) {
                BytePair curr = str_pairs[j];
                if(strcmp(left_buff, curr.left) == 0 && strcmp(right_buff, curr.right) == 0) {
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
    rewind(dict);
    uint32_t start;
    fread(&start,1,4,dict);
    for(uint16_t i = 0; i < num_parts; i++) {
        char* part = str_parts[i];
        fseek(dict, start, SEEK_SET);
        uint8_t flag = 0;
        while(1) {
            uint16_t number;
            fread(&number,1,2,dict);
            char* temp2 = left_buff;
            uint8_t check = 0;
            while(1) {
                if(ftell(dict) >= dict_len) {
                    flag = 1;
                    break;
                }
                fread(temp2,1,1,dict);
                if(*temp2 == 0) break;
                temp2++;
                check++;
                if(check == LEFTRIGHT_BUFF-1) {
                    while(*temp2 != 0) fread(temp2,1,1,dict);
                    break;
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
    printf("\033[1;31mERROR:\033[0m Overflowed buffer\r\n");
    return 0;
}

const char special_chars[] = ",.-+_ \"/()%&$!?\n\t':;0123456789\\";
uint16_t tokenize(const char* input, uint16_t* output) {
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
            printf("\033[1;31mERROR:\033[0m Overflowed word buffer\r\n");
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
    return count;
}

const char null_token[] = "<|blank|>";
const char error_token[] = "<|error|>";
void untokenize(uint16_t* input, uint16_t input_len, char* output) {
    uint8_t null_len = strlen(null_token);
    uint8_t error_len = strlen(error_token);
    for(uint16_t i = 0; i < input_len; i++) {
        uint16_t token = input[i];
        if(token == 0) {
            memcpy(output, null_token, null_len);
            output += null_len;
            continue;
        }
        rewind(dict);
        fseek(dict, token<<2, SEEK_SET);
        uint32_t pos;
        fread(&pos,1,4,dict);
        fseek(dict, pos, SEEK_SET);
        uint16_t number;
        fread(&number,1,2,dict);
        if(number != token-1) {
            memcpy(output, error_token, error_len);
            output += error_len;
            continue;
        }
        while(1) {
            fread(output,1,1,dict);
            if(*output == 0) break;
            if(*output == '~') *output = ' ';
            output++;
        }
    }
    *output = 0;
}
