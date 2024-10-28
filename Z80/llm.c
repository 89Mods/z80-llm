#include <stdint.h>
#include <math.h>
#include <stdio.h>

#include "defines.h"
#include "ata.h"
#include "ext4.h"
#include "crc16.h"
#include "tokenizer.h"
#include "llm.h"

uint16_t* curr_tok_sequence = (uint16_t*)(0xFFFF-512*2);
uint8_t* lut_buffer = (uint8_t*)0xC000;
char str_buff[96];
uint32_t gelu_first_block;
uint32_t last_lut_block;
uint16_t num_valid_toks;

typedef struct {
	uint16_t rows;
	uint16_t cols;
	uint32_t start_block;
#ifdef EMULATED_BUFFERS
	float* data;
#endif
} matrix_buff;

ext4_FIL f_tok_embeddings;
ext4_FIL f_pos_embeddings;

matrix_buff keys_matrix;
matrix_buff queries_matrix;
matrix_buff values_matrix;
matrix_buff values_transposed_matrix;
matrix_buff kq_matrix;
matrix_buff ff_matrix;

const uint16_t cv_blocks = 4096;
const uint16_t cv_row_blocks = 8;
uint32_t cv_start, cv_shortcut_start, cv_copy_start;

float* paged_buffer = (float*)0xC000;

uint32_t cv_hash(void);
void print_cv_sample(void);
float f_inv_sqrt(float x);
void putfloat(float x);
float f_gelu(float xf);

uint64_t xorshift_state = 0x13EE062180A669BCULL;
void xorshift64();

void mb_write_row(matrix_buff* bf, uint16_t row, float* vals) {
	uint16_t blocks_per_row = bf->cols / 128;
	if((bf->cols % 128) != 0) blocks_per_row++;
	uint32_t start_block = (uint32_t)row * (uint32_t)blocks_per_row + bf->start_block;
	ata_writeSectors(start_block, blocks_per_row);
	uint8_t* ptr = (uint8_t*)vals;
	for(uint16_t i = 0; i < blocks_per_row; i++) {
		ata_writeBuffer(ptr);
		ptr += 512;
		ata_busyWait();
	}
}

void mb_read_row(matrix_buff* bf, uint16_t row, float* vals) {
	uint16_t blocks_per_row = bf->cols / 128;
	if((bf->cols % 128) != 0) blocks_per_row++;
	uint32_t start_block = (uint32_t)row * (uint32_t)blocks_per_row + bf->start_block;
	ata_readSectors(start_block, blocks_per_row);
	uint8_t* ptr = (uint8_t*)vals;
	for(uint16_t i = 0; i < blocks_per_row; i++) {
		ata_readBuffer(ptr);
		ptr += 512;
		ata_busyWait();
	}
}

void mb_write_block(matrix_buff* bf, uint16_t row, uint16_t row_block, float* vals) {
	uint16_t blocks_per_row = bf->cols / 128;
	if((bf->cols % 128) != 0) blocks_per_row++;
	uint32_t start_block = (uint32_t)row * (uint32_t)blocks_per_row + bf->start_block;
	start_block += row_block;
	ata_writeSectors(start_block, 1);
	ata_writeBuffer((uint8_t*)vals);
	ata_busyWait();
}

void cv_write_arrs(uint16_t x, float* vals, uint16_t len, uint32_t offset) {
	if(len > 1024 || (len&127) != 0) {
		puts("UNSUPPORTED!\r\n");
		fatal();
	}
	uint32_t start_block = x * cv_row_blocks + offset;
	uint16_t num_sectors = len / 128;
	ata_writeSectors(start_block, num_sectors);
	uint8_t* ptr = (uint8_t*)vals;
	while(len != 0) {
		ata_writeBuffer(ptr);
		ptr += 512;
		len -= 128;
		ata_busyWait();
	}
}

void cv_write_arr(uint16_t x, float* vals, uint16_t len) { cv_write_arrs(x, vals, len, cv_start); }
void cv_shortcut_write_arr(uint16_t x, float* vals, uint16_t len) { cv_write_arrs(x, vals, len, cv_shortcut_start); }
void cv_copy_write_arr(uint16_t x, float* vals, uint16_t len) { cv_write_arrs(x, vals, len, cv_copy_start); }

void cv_read_arrs(uint16_t x, float* vals, uint16_t len, uint32_t offset) {
	if(len > 1024 || (len&127) != 0) {
		puts("UNSUPPORTED!\r\n");
		fatal();
	}
	uint32_t start_block = x * cv_row_blocks + offset;
	uint16_t num_sectors = len / 128;
	ata_readSectors(start_block, num_sectors);
	uint8_t* ptr = (uint8_t*)vals;
	while(len != 0) {
		ata_readBuffer(ptr);
		ptr += 512;
		len -= 128;
		ata_busyWait();
	}
}

void cv_read_arr(uint16_t x, float* vals, uint16_t len) { cv_read_arrs(x, vals, len, cv_start); }
void cv_shortcut_read_arr(uint16_t x, float* vals, uint16_t len) { cv_read_arrs(x, vals, len, cv_shortcut_start); }
void cv_copy_read_arr(uint16_t x, float* vals, uint16_t len) { cv_read_arrs(x, vals, len, cv_copy_start); }

void cv_write_arr64(uint16_t x, uint16_t y, float* vals) {
	float buffer[128];
	uint32_t start_block = x * cv_row_blocks + cv_start;
	start_block += y >> 1;
	ata_readSectors(start_block, 1);
	ata_readBuffer((uint8_t*)buffer);
	float* copy_targ = (y & 1) != 0 ? buffer + 64 : buffer;
	for(uint8_t i = 0; i < 64; i++) copy_targ[i] = vals[i];
	ata_writeSectors(start_block, 1);
	ata_writeBuffer((uint8_t*)buffer);
	ata_busyWait();
}

void cv_create_shortcut(void) {
	float* buff = paged_buffer;
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		cv_shortcut_write_arr(i, buff, 1024);
	}
}

void cv_create_copy(void) {
	float* buff = paged_buffer;
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		cv_copy_write_arr(i, buff, 1024);
	}
}

void cv_add_shortcut(void) {
	float* buff = paged_buffer;
	float* buff2 = paged_buffer + 1024;
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		cv_shortcut_read_arr(i, buff2, 1024);
		for(uint16_t i = 0; i < 1024; i++) buff[i] += buff2[i];
		cv_write_arr(i, buff, 1024);
	}
}

void fnf() {
	puts("\033[1;31mFATAL:\033[0m File not found!\r\n");
	fatal();
}

void fcnbo() {
	puts("\033[1;31mFATAL:\033[0m File could not be opened\r\n");
	fatal();
}

void ferr() {
	puts("\033[1;31mFATAL:\033[0m File error\r\n");
	fatal();
}

uint8_t llm_init(const char* initial_text, uint32_t first_free_sector) {
	/*for(int i = 0; i < 100; i++) {
		uint16_t aaa = (uint16_t)xorshift_state;
		float random_float = (float)(aaa) / 65536.0f;
		putfloat(random_float);
		xorshift64();
		putchar(' ');
	}*/
	set_page(1);
	xorshift64();
	xorshift64();
	xorshift64();
	xorshift64();
	xorshift64();
	ata_set_write_protect(first_free_sector);
	{
		puts("Loading GELU LUT...\r\n");
		last_lut_block = 0xFFFFFFFFUL;
		gelu_first_block = first_free_sector;
		uint64_t inode_num;
		uint8_t file_type;
		int e4ret = ext4_find_file("/LUTs/gelu.bin", &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		ext4_FIL fhandle;
		e4ret = ext4_open_read(inode_num, &fhandle);
		if(e4ret != EXT4_OKAY) fcnbo();
		set_page(3);
		uint16_t limit_blocks = (uint32_t)fhandle.limit / 512UL;
		uint16_t block_pos = 0;
		while(fhandle.position < fhandle.limit) {
			check_uart_in();
			e4ret = ext4_read(&fhandle, lut_buffer, 512, NULL);
			e4ret = ata_writeSectors(first_free_sector, 1);
			if(e4ret) {
				puts("ATA write fail\r\n");
				fatal();
			}
			first_free_sector++;
			ata_writeBuffer(lut_buffer);
			ata_busyWait();
			block_pos++;
			disable_text_buffer();
			sprintf(str_buff, "\r%u/%u", block_pos, limit_blocks);
			puts(str_buff);
			enable_text_buffer();
		}
		set_page(1);
		newl();
		putchar('\n');
	}
	{
		uint64_t inode_num;
		uint8_t file_type;
		int e4ret = ext4_find_file("/model/token_embeddings.bin", &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		e4ret = ext4_open_read(inode_num, &f_tok_embeddings);
		if(e4ret != EXT4_OKAY) fcnbo();
		e4ret = ext4_find_file("/model/positional_embeddings.bin", &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		e4ret = ext4_open_read(inode_num, &f_pos_embeddings);
		if(e4ret != EXT4_OKAY) fcnbo();
	}
	first_free_sector++;
	cv_start = first_free_sector;
	first_free_sector += cv_blocks*2+8;
	cv_shortcut_start = first_free_sector;
	first_free_sector += cv_blocks*2+8;
	cv_copy_start = first_free_sector;
	first_free_sector += cv_blocks*2+8;
	{
		keys_matrix.rows = 512;
		keys_matrix.cols = 64;
		keys_matrix.start_block = first_free_sector;
		first_free_sector += 515;
		
		queries_matrix.rows = 512;
		queries_matrix.cols = 64;
		queries_matrix.start_block = first_free_sector;
		first_free_sector += 515;
		
		values_matrix.rows = 512;
		values_matrix.cols = 64;
		values_matrix.start_block = first_free_sector;
		first_free_sector += 515;
		
		values_transposed_matrix.rows = 64;
		values_transposed_matrix.cols = 512;
		values_transposed_matrix.start_block = first_free_sector;
		first_free_sector += 300;
		
		kq_matrix.rows = 512;
		kq_matrix.cols = 512;
		kq_matrix.start_block = first_free_sector;
		first_free_sector += 2050;
		
		ff_matrix.rows = 512;
		ff_matrix.cols = 2048;
		ff_matrix.start_block = first_free_sector;
		first_free_sector += 8200;
	}
	
	puts("\r\nTokenizing...");
	num_valid_toks = tokenize(initial_text, curr_tok_sequence);
	newl();
	set_page(0);
	for(uint16_t i = 0; i < num_valid_toks; i++) {
		sprintf(str_buff, "%u, ", curr_tok_sequence[i]);
		puts(str_buff);
	}
	puts("\r\n\r\nInitial text: ");
	for(uint16_t i = 0; i < num_valid_toks; i++) {
		untokenize(curr_tok_sequence[i], str_buff);
		puts(str_buff);
	}
	puts("\r\n\r\n");
	for(uint16_t i = 0; i < num_valid_toks; i++) {
		curr_tok_sequence[511-i] = curr_tok_sequence[num_valid_toks-1-i];
	}
	for(uint16_t i = 0; i < 512-num_valid_toks; i++) curr_tok_sequence[i] = 0;
	set_page(1);
	
	return 0;
}

void batch_norm(ext4_FIL* bparams) {
	float* buff = paged_buffer;
	float* buff2 = paged_buffer + 1024;
	float* mean = buff2;
	float* var = buff2 + 512;
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		mean[i] = 0;
		for(int j = 0; j < 1024; j++) mean[i] += buff[j];
		mean[i] = mean[i] / 1024.0f;
	}
	check_uart_in();
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		var[i] = 0;
		for(int j = 0; j < 1024; j++) {
			buff[j] -= mean[i];
			var[i] += buff[j] * buff[j];
		}
		var[i] = var[i] / 1023.0f;
		var[i] = f_inv_sqrt(var[i]);
	}
	check_uart_in();
	
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		for(int j = 0; j < 1024; j++) {
			buff[j] -= mean[i];
			buff[j] = buff[j] * var[i];
		}
		cv_write_arr(i, buff, 1024);
	}
	
	ext4_read(bparams, (uint8_t*)buff2, 1024*4, NULL);
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		for(int j = 0; j < 1024; j++) buff[j] = buff[j] * buff2[j];
		cv_write_arr(i, buff, 1024);
	}
	check_uart_in();
	
	ext4_read(bparams, (uint8_t*)buff2, 1024*4, NULL);
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		for(int j = 0; j < 1024; j++) buff[j] += buff2[j];
		cv_write_arr(i, buff, 1024);
	}
}

void matrix_multiply_with_embeddings(ext4_FIL* weights_file, matrix_buff* output, uint8_t print_progress) {
	float* buffer = paged_buffer;
	float* buffer2 = buffer + 1024;
	float* buffer3 = buffer2 + 1024;
	float sum;
	uint8_t ret;
	uint32_t read;
	for(uint16_t i = 0; i < output->rows; i++) {
		if(print_progress) {
			sprintf(str_buff, "\r%u/%u", i+1, output->rows);
			puts(str_buff);
		}
		cv_copy_read_arr(i, buffer, 1024);
		ret = ext4_seek(weights_file, 0ULL);
		if(ret != EXT4_OKAY) ferr();
		for(uint16_t j = 0; j < output->cols; j++) {
			check_uart_in();
			ret = ext4_read(weights_file, (uint8_t*)buffer2, 1024*4, &read);
			if(ret != EXT4_OKAY || read != 1024*4) ferr();
			sum = 0;
			for(uint16_t k = 0; k < 1024; k++) sum += buffer[k] * buffer2[k];
			buffer3[j] = sum;
		}
		mb_write_row(output, i, buffer3);
	}
	if(print_progress) newl();
}

void feed_forward(ext4_FIL* in_params, ext4_FIL* out_params) {
	cv_create_copy();
	float* buffer = paged_buffer;
	float* buffer2 = buffer + 2048;
	uint32_t read;
	uint8_t ret;
	puts("Feed forward in...\r\n");
	matrix_multiply_with_embeddings(in_params, &ff_matrix, 1);
	set_page(4);
	//Add bias
	ret = ext4_seek(in_params, 1024UL*2048UL*4UL);
	if(ret != EXT4_OKAY) ferr();
	ret = ext4_read(in_params, (uint8_t*)buffer2, 2048*4, &read);
	if(ret != EXT4_OKAY || read != 2048*4) ferr();
	for(uint16_t i = 0; i < ff_matrix.rows; i++) {
		mb_read_row(&ff_matrix, i, buffer);
		for(uint16_t j = 0; j < ff_matrix.cols; j++) {
			buffer[j] += buffer2[j];
			buffer[j] = f_gelu(buffer[j]);
		}
		mb_write_row(&ff_matrix, i, buffer);
	}
	puts("Feed forward out...\r\n");
	//Matmul
	for(uint16_t i = 0; i < 512; i++) {
		sprintf(str_buff, "\r%u/512", i+1);
		disable_text_buffer();
		puts(str_buff);
		enable_text_buffer();
		mb_read_row(&ff_matrix, i, buffer);
		ret = ext4_seek(out_params, 0ULL);
		if(ret != EXT4_OKAY) ferr();
		for(uint16_t j = 0; j < 1024; j++) {
			check_uart_in();
			ret = ext4_read(out_params, (uint8_t*)buffer2, 4UL*ff_matrix.cols, NULL);
			if(ret != EXT4_OKAY) ferr();
			float sum = 0;
			for(uint16_t k = 0; k < ff_matrix.cols; k++) sum += buffer[k] * buffer2[k];
			set_page(5);
			buffer2[j] = sum;
			set_page(4);
		}
		set_page(5);
		cv_write_arr(i, buffer2, 1024);
		set_page(4);
	}
	disable_text_buffer();
	newl();
	enable_text_buffer();
	//Add bias
	ret = ext4_seek(out_params, 2048UL*1024UL*4UL);
	if(ret != EXT4_OKAY) ferr();
	ret = ext4_read(out_params, (uint8_t*)buffer2, 1024*4, &read);
	if(ret != EXT4_OKAY || read != 1024*4) ferr();
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buffer, 1024);
		for(uint16_t j = 0; j < 1024; j++) buffer[j] += buffer2[j];
		cv_write_arr(i, buffer, 1024);
	}
	set_page(1);
}

//sqrt(context_size) = sqrt(512)
#define SDK 22.627416998f

void self_attention(uint8_t which) {
	cv_create_copy();
	uint64_t inode_num;
	uint8_t file_type;
	ext4_FIL weights_file;
	for(uint8_t i = 0; i < 16; i++) {
		{
			disable_text_buffer();
			uint8_t i2 = i + 1;
			puthex(i2);
			putchar(' ');
			enable_text_buffer();
		}
		float* buffer = paged_buffer;
		float* buffer2 = buffer + 128;
		float* buffer3 = buffer2 + 128;
		float* buffer4 = buffer3 + 512;
		{
			sprintf(str_buff, "/model/block%u/attention/%u/keys.bin", which, i);
			int e4ret = ext4_find_file(str_buff, &inode_num, &file_type);
			if(e4ret != EXT4_OKAY) fnf();
			e4ret = ext4_open_read(inode_num, &weights_file);
			if(e4ret != EXT4_OKAY) fcnbo();
			matrix_multiply_with_embeddings(&weights_file, &keys_matrix, 0);
		}
		disable_text_buffer();
		puts("k ");
		enable_text_buffer();
		{
			sprintf(str_buff, "/model/block%u/attention/%u/queries.bin", which, i);
			int e4ret = ext4_find_file(str_buff, &inode_num, &file_type);
			if(e4ret != EXT4_OKAY) fnf();
			e4ret = ext4_open_read(inode_num, &weights_file);
			if(e4ret != EXT4_OKAY) fcnbo();
			matrix_multiply_with_embeddings(&weights_file, &queries_matrix, 0);
		}
		disable_text_buffer();
		puts("q ");
		enable_text_buffer();
		{
			sprintf(str_buff, "/model/block%u/attention/%u/values.bin", which, i);
			int e4ret = ext4_find_file(str_buff, &inode_num, &file_type);
			if(e4ret != EXT4_OKAY) fnf();
			e4ret = ext4_open_read(inode_num, &weights_file);
			if(e4ret != EXT4_OKAY) fcnbo();
			matrix_multiply_with_embeddings(&weights_file, &values_matrix, 0);
		}
		disable_text_buffer();
		puts("v ");
		enable_text_buffer();
		
		//Keys x queries
		const float div_by = SDK;
		for(uint16_t j = 0; j < 512; j++) {
			check_uart_in();
			mb_read_row(&queries_matrix, j, buffer);
			for(uint16_t k = 0; k < j+1; k++) {
				mb_read_row(&keys_matrix, k, buffer2);
				float sum = 0;
				for(uint8_t l = 0; l < 64; l++) sum += buffer[l] * buffer2[l];
				buffer3[k] = sum / div_by;
			}
			for(uint16_t k = j+1; k < 512; k++) {
				buffer3[k] = 0;
			}
			mb_write_row(&kq_matrix, j, buffer3);
		}
		disable_text_buffer();
		puts("kq ");
		enable_text_buffer();
		
		//Softmax
		for(uint16_t j = 0; j < 512; j++) {
			check_uart_in();
			mb_read_row(&kq_matrix, j, buffer3);
			if(j == 0) buffer3[0] = 1;
			else {
				float sum = 0;
				for(int k = 0; k < j+1; k++) {
					buffer3[k] = exp(buffer3[k]);
					//Guard against infinities
					if(buffer3[k] > 10000000) buffer3[k] = 10000000;
				}
				for(int k = 0; k < j+1; k++) {
					sum += buffer3[k];
					//Largest value we are willing to accept
					if(sum > 10000000) break;
				}
				//if(sum == 0) printf("\r\n%d %d\r\n", which, j);
				for(int k = 0; k < j+1; k++) {
					buffer3[k] = buffer3[k] / sum;
				}
			}
			mb_write_row(&kq_matrix, j, buffer3);
		}
		disable_text_buffer();
		puts("sm ");
		enable_text_buffer();
		
		//The values matrix needs to be transposed
		{
			float block_buffer[128];
			for(uint16_t j = 0; j < 512/128; j++) {
				uint16_t buffer_pos = 0;
				for(uint16_t k = 0; k < 128; k++) {
					set_page((uint8_t)(buffer_pos>>12)+4);
					mb_read_row(&values_matrix, j*128+k, paged_buffer+(buffer_pos&0xFFF));
					buffer_pos += 64;
				}
				for(uint16_t k = 0; k < 64; k++) {
					for(uint16_t l = 0; l < 128; l++) {
						uint16_t val_addr = l * 64 + k;
						set_page((uint8_t)(val_addr>>12)+4);
						block_buffer[l] = paged_buffer[val_addr&0xFFF];
					}
					mb_write_block(&values_transposed_matrix, k, j, block_buffer);
				}
			}
			set_page(1);
		}
		disable_text_buffer();
		puts("tp ");
		enable_text_buffer();
		
		//Final matrix multiply of kq_matrix times transposed values
		for(uint16_t j = 0; j < 512; j++) {
			mb_read_row(&kq_matrix, j, buffer4);
			for(uint16_t k = 0; k < 64; k++) {
				check_uart_in();
				mb_read_row(&values_transposed_matrix, k, buffer3);
				float sum = 0;
				for(uint16_t l = 0; l < 512; l++) sum += buffer4[l] * buffer3[l];
				buffer2[k] = sum;
			}
			cv_write_arr64(j, i, buffer2);
		}
		if(i != 15) {
			disable_text_buffer();
			puts("\r                    \r");
			enable_text_buffer();
		}
	}
	disable_text_buffer();
	newl();
	enable_text_buffer();
}

//361905243834260, 14/24
uint8_t llm_step(uint16_t* next_tok) {
	set_page(1);
	check_uart_in();
	puts("Embedding...");
	{
		float* raw = paged_buffer;
		float* raw2 = paged_buffer + 1024;
		uint32_t read;
		for(uint16_t i = 0; i < 512; i++) {
			set_page(0);
			uint32_t tok_pos = (uint32_t)curr_tok_sequence[i]*4096UL;
			set_page(1);
			ext4_seek(&f_tok_embeddings, (uint64_t)tok_pos);
			uint8_t* rawp = (uint8_t*)raw;
			ext4_read(&f_tok_embeddings, rawp, 1024*4, &read);
			cv_write_arr(i, raw, 1024);
		}
		ext4_seek(&f_pos_embeddings, 0);
		for(uint16_t i = 0; i < 512; i++) {
			ext4_read(&f_pos_embeddings, (uint8_t*)raw, 1024*4, NULL);
			cv_read_arr(i, raw2, 1024);
			for(uint16_t j = 0; j < 1024; j++) raw[j] = raw[j] + raw2[j];
			cv_write_arr(i, raw, 1024);
		}
		newl();
	}
	print_cv_sample();
	uint64_t inode_num;
	uint8_t file_type;
	ext4_FIL bparams;
	ext4_FIL out_params;
	uint8_t i = 0;
	
	/*
	 * Debug: load CV from file
	 */
	/*{
		i = 9;
		uint32_t read;
		int e4ret = ext4_find_file("/layerdump9.bin", &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		e4ret = ext4_open_read(inode_num, &bparams);
		if(e4ret != EXT4_OKAY) fcnbo();
		float* wbuff = paged_buffer;
		for(int j = 0; j < 512; j++) {
			e4ret = ext4_read(&bparams, (uint8_t*)wbuff, 4096, &read);
			if(e4ret != EXT4_OKAY || read != 4096) fcnbo();
			cv_write_arr(j, wbuff, 1024);
		}
	}*/
	
	for(; i < 24; i++) {
		sprintf(str_buff, "\n%u/%u\r\n", i+1, 24);
		puts(str_buff);
		//Print stack pointer, just because
#asm
		push hl
		push bc
		ld h,0
		ld l,h
		add hl,sp
		ld b,0xC0
		ld c,0x00
		ld a,l
		ld (bc),a
		ld a,h
		inc c
		ld (bc),a
		pop bc
		pop hl
#endasm
		{
			uint8_t* ptr = (uint8_t*)0xC000;
			puthex(*(ptr+1));
			puthex(*ptr);
			newl();
		}
		cv_create_shortcut();
		sprintf(str_buff, "/model/block%u/norm1.bin", i);
		int e4ret = ext4_find_file(str_buff, &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		e4ret = ext4_open_read(inode_num, &bparams);
		if(e4ret != EXT4_OKAY) fcnbo();
		batch_norm(&bparams);
		print_cv_sample();
		self_attention(i);
		print_cv_sample();
		cv_add_shortcut();
		cv_create_shortcut();
		sprintf(str_buff, "/model/block%u/norm2.bin", i);
		e4ret = ext4_find_file(str_buff, &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		e4ret = ext4_open_read(inode_num, &bparams);
		if(e4ret != EXT4_OKAY) fcnbo();
		batch_norm(&bparams);
		sprintf(str_buff, "/model/block%u/dense/ff0.bin", i);
		e4ret = ext4_find_file(str_buff, &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		e4ret = ext4_open_read(inode_num, &bparams);
		if(e4ret != EXT4_OKAY) fcnbo();
		sprintf(str_buff, "/model/block%u/dense/ff1.bin", i);
		e4ret = ext4_find_file(str_buff, &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		e4ret = ext4_open_read(inode_num, &out_params);
		if(e4ret != EXT4_OKAY) fcnbo();
		feed_forward(bparams, out_params);
		cv_add_shortcut();
		print_cv_sample();
	}
	uint8_t e4ret = ext4_find_file("/model/norm.bin", &inode_num, &file_type);
	if(e4ret != EXT4_OKAY) fnf();
	e4ret = ext4_open_read(inode_num, &bparams);
	if(e4ret != EXT4_OKAY) fcnbo();
	batch_norm(&bparams);
	print_cv_sample();
	uint32_t hash = cv_hash();
	sprintf(str_buff, "CV CRC16x2: %lu\r\n", hash);
	puts(str_buff);
	check_uart_in();
	puts("Unembedding...\r\n");
	uint16_t token;
	/*{
		e4ret = ext4_find_file("/model/unembedding.bin", &inode_num, &file_type);
		if(e4ret != EXT4_OKAY) fnf();
		e4ret = ext4_open_read(inode_num, &bparams);
		if(e4ret != EXT4_OKAY) fcnbo();
		float* buff = paged_buffer;
		float* buff2 = paged_buffer + 1024;
		float sum;
		uint16_t topk_idx[10];
		float topk_vals[10] = {-10000, -10000, -10000, -10000, -10000, -10000, -10000, -10000, -10000, -10000};
		const float temperature = 0.621f;
		cv_read_arr(511, buff, 1024); //Only interested in the latest prediction
		const uint16_t end = 50257;
		for(uint16_t i = 0; i < end; i++) {
			check_uart_in();
			putchar('\r');
			uint16_t i2 = i+1;
			puthex(i2>>8);
			puthex(i2);
			putchar('/');
			puthex(end>>8);
			puthex(end);
			e4ret = ext4_read(bparams, (uint8_t*)buff2, 4*1024, NULL);
			if(e4ret != EXT4_OKAY) ferr();
			sum = 0;
			for(uint16_t j = 0; j < 1024; j++) sum += buff[j] * buff2[j];
			sum = sum / temperature;
			
			if(topk_vals[0] >= sum) continue;
			for(uint8_t j = 9; j != 255; j--) {
				if(sum > topk_vals[j]) {
					if(j == 0) {
						topk_vals[j] = sum;
						topk_idx[j] = i;
					}else {
						for(uint8_t k = 0; k < j; k++) {
							topk_vals[k] = topk_vals[k + 1];
							topk_idx[k] = topk_idx[k + 1];
						}
						topk_vals[j] = sum;
						topk_idx[j] = i;
					}
					break;
				}
			}
		}
		
		puts("\r\n\r\ntopk:\r\n");
		for(uint8_t i = 0; i < 11; i++) {
			if(i != 10) {
				sprintf(str_buff, "%u - ", topk_idx[i]);
				puts(str_buff);
				putfloat(topk_vals[i]);
			}
			newl();
		}

		sum = 0;
		for(uint16_t i = 0; i < 10; i++) {
			topk_vals[i] = exp(topk_vals[i]);
			sum += topk_vals[i];
		}
		for(uint16_t i = 0; i < 10; i++) {
			topk_vals[i] = topk_vals[i] / sum;
			putfloat(topk_vals[i]);
			newl();
		}
		check_uart_in();
		
		uint16_t aaa = (uint16_t)xorshift_state;
		float random_float = (float)(aaa) / 65536.0f;
		xorshift64();
		sum = 0;
		for(token = 0; token < 10; token++) {
			sum += topk_vals[token];
			if(sum >= random_float) break;
		}
		*next_tok = topk_idx[token];
		sprintf(str_buff, "\r\nChosen token: #%u\r\n\r\n", *next_tok);
		puts(str_buff);
		set_page(0);
		for(int i = 0; i < 511; i++) curr_tok_sequence[i] = curr_tok_sequence[i + 1];
		curr_tok_sequence[511] = token == 10 ? 0 : *next_tok;
		num_valid_toks = num_valid_toks == 512 ? 512 : num_valid_toks + 1;
		set_page(1);
	}*/
	return 0;
}

float f_inv_sqrt(float x) {
	if(x < 0) x = -x;
	float x2,y = 0;
	uint32_t i;

	if(x == 0.0f) return 0.0f;
	x2 = x * 0.5f;
	i = *(uint32_t *)&x;                    // evil floating point bit hack
	i = 0x5f3759dfUL - (i >> 1);         // what the fuck?
	y = *(float *)&i;
	y *= 1.5f - (x2 * y * y);
	y *= 1.5f - (x2 * y * y);
	y *= 1.5f - (x2 * y * y);
	return y;
}

float f_gelu(float xf) {
	int32_t xi = (int32_t)(xf * 65536.0f);
	if(xi >= 0x00100000L) return xf;
	if(xi <= -0x00100000L) return 0.0f;
	uint32_t faddr = xi * 4;
	if(xi < 0) {
		xi = -xi;
		faddr = xi * 4 + 0x00100000*4;
	}
	uint8_t p = get_page();
	set_page(3);
	uint32_t block = faddr / 512UL;
	uint16_t block_offset = (uint16_t)(faddr % 512UL);
	block += gelu_first_block;
	if(last_lut_block != block) {
		ata_readSectors(block, 1);
		uint8_t* ptr = lut_buffer;
		ata_readBuffer(ptr);
		ata_busyWait();
		last_lut_block = block;
	}
	int32_t* res_p = (int32_t*)(lut_buffer + block_offset);
	int32_t res = *res_p;
	set_page(p);
	return (float)res / 65536.0f;
}

void print_cv_sample(void) {
	disable_text_buffer();
	float* debug_buff = paged_buffer;
	puts("Context vec sample:\r\n\t");
	for(uint8_t i = 0; i < 2; i++) {
		cv_read_arr(i == 0 ? 0 : 511, debug_buff, 1024);
		for(int k2 = 0; k2 < 10; k2++) {
			putfloat(debug_buff[k2]);
			putchar(' ');
		}
		puts("[...] ");
		for(int k2 = 0; k2 < 10; k2++) {
			putfloat(debug_buff[1023-9+k2]);
			putchar(' ');
		}
		newl();
		if(i == 0) putchar('\t');
	}
	enable_text_buffer();
}

uint32_t cv_hash(void) {
	uint32_t hash = 0;
	float raw[1024];
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, raw, 1024);
		uint32_t crc = (uint32_t)gen_crc16((uint8_t*)raw, 1024*4);
		if((i&1)!=0) {
			hash ^= crc << 16;
		}else {
			hash ^= crc;
		}
	}
	return hash;
}

void putfloat(float x) {
	if(x < 0) {
		putchar('-');
		x = -x;
	}
	uint32_t fixed = (uint32_t)x;
	sprintf(str_buff, "%lu.", fixed);
	puts(str_buff);
	x -= floor(x);
	fixed = (uint32_t)(x * 16777216.0f);
	fixed &= 0xFFFFFFUL;
	for(uint8_t i = 0; i < 6; i++) {
		fixed *= 10;
		putchar('0' + (char)(fixed>>24));
		fixed &= 0xFFFFFFUL;
	}
}

void xorshift64() {
	xorshift_state ^= xorshift_state << 13;
	xorshift_state ^= xorshift_state >> 7;
	xorshift_state ^= xorshift_state << 17;
}
