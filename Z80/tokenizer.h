#ifndef TOKENIZER_H_
#define TOKENIZER_H_

#define TOK_BUFFER_LEN 256
#define TOK_MAX_PAIRS 32
#define TOK_MAX_PARTS 32
#define LEFTRIGHT_BUFF 96
#define WORD_BUFF_LEN 64

void bpe_init();
uint8_t gen_pairs(char** str_parts, uint8_t num_parts);
uint8_t bpe(const char* input, uint16_t* res_buff);
uint16_t tokenize(const char* input, uint16_t* output);
void untokenize(uint16_t input, char* output);

#endif
