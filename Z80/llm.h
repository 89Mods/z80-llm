#ifndef LLM_H_
#define LLM_H_

uint8_t llm_init(const char* initial_text, uint32_t first_free_sector);
uint8_t llm_step(uint16_t* next_token);

#endif
