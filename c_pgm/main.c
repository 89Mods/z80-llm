#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "crc16.h"
#include "tokenizer.h"

#define EMULATED_BUFFERS

void print_cv_sample();

float f_inv_sqrt(float x) {
	if(x < 0) x = -x;
	float x2,y = 0;
	uint32_t i;

	if(x == 0.0f) return 0.0f;
	x2 = x * 0.5f;
	i = *(uint32_t *)&x;
	i = 0x5f3759df - (i >> 1);
	y = *(float *)&i;
	y *= 1.5f - (x2 * y * y);
	y *= 1.5f - (x2 * y * y);
	y *= 1.5f - (x2 * y * y);
	return y;
}

/*
 * Emulate Z80 disk buffers
 */
float context_vec[512][1024];
void cv_write_arr(uint16_t x, float* vals, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) context_vec[x][i] = vals[i];
}

void cv_write_arr64(uint16_t x, uint16_t y, float* vals) {
	for(uint16_t i = 0; i < 64; i++) context_vec[x][i + y*64] = vals[i];
}

void cv_read_arr(uint16_t x, float* vals, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) vals[i] = context_vec[x][i];
}

float context_shortcut[512][1024];
void cv_create_shortcut() {
	float raw[1024];
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, raw, 1024);
		for(uint16_t j = 0; j < 1024; j++) context_shortcut[i][j] = raw[j];
	}
}

void cv_add_shortcut() {
	float buff[1024];
	float buff2[1024];
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		for(uint16_t j = 0; j < 1024; j++) buff2[j] = context_shortcut[i][j];
		for(uint16_t j = 0; j < 1024; j++) buff[j] += buff2[j];
		cv_write_arr(i, buff, 1024);
	}
}

float context_copy[512][1024]; //For when an operation needs to both update the context vector while needing its old values again
void cv_create_copy() {
	float raw[1024];
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, raw, 1024);
		for(uint16_t j = 0; j < 1024; j++) context_copy[i][j] = raw[j];
	}
}

void cv_read_copy_arr(uint16_t x, float* vals, uint16_t len) {
    for(uint16_t i = 0; i < len; i++) vals[i] = context_copy[x][i];
}

FILE* gelu_lut;

float f_gelu(float xf) {
	int32_t xi = (int32_t)(xf * 65536.0f);
	if(xi >= 0x00100000) return xf;
	if(xi <= -0x00100000) return 0.0f;
	uint32_t faddr = xi * 4;
	if(xi < 0) {
		xi = -xi;
		faddr = xi * 4 + 0x00100000*4;
	}
	fseek(gelu_lut, faddr, SEEK_SET);
	int32_t res;
	fread(&res, 1, 4, gelu_lut);
	return (float)res / 65536.0f;
}

typedef struct {
	uint16_t rows;
	uint16_t cols;
	uint32_t start_block;
#ifdef EMULATED_BUFFERS
	float* data;
#endif
} matrix_buff;

void mb_read_row(matrix_buff* bf, uint16_t row, float* vals) {
	for(uint16_t i = 0; i < bf->cols; i++) vals[i] = bf->data[(uint32_t)row * (uint32_t)bf->cols + i];
}

void mb_write_row(matrix_buff* bf, uint16_t row, float* vals) {
	for(uint16_t i = 0; i < bf->cols; i++) bf->data[(uint32_t)row * (uint32_t)bf->cols + i] = vals[i];
}

void mb_write_block(matrix_buff* bf, uint16_t row, uint16_t row_block, float* vals) {
	for(uint16_t i = 0; i < 512/4; i++) bf->data[(uint32_t)row * (uint32_t)bf->cols + i + row_block * (512/4)] = vals[i];
}

matrix_buff keys_matrix;
matrix_buff queries_matrix;
matrix_buff values_matrix;
matrix_buff values_transposed_matrix;
matrix_buff kq_matrix;
matrix_buff ff_matrix;

uint32_t cv_hash() {
    uint32_t hash = 0;
    float raw[1024];
    for(int i = 0; i < 512; i++) {
        cv_read_arr(i, raw, 1024);
        uint32_t crc = (uint32_t)gen_crc16((uint8_t*)raw, 1024*4);
        if(i % 2) {
            hash ^= crc << 16;
        }else {
            hash ^= crc;
        }
    }
    return hash;
}

void batch_norm(FILE* bparams) {
	float buff[1024];
	float buff2[1024];
	float* mean = buff2;
	float* var = buff2 + 512;
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		mean[i] = 0;
		for(int j = 0; j < 1024; j++) mean[i] += buff[j];
		mean[i] = mean[i] / 1024.0f;
	}
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		var[i] = 0;
		for(int j = 0; j < 1024; j++) {
			buff[j] -= mean[i];
			var[i] += buff[j] * buff[j];
		}
		var[i] = var[i] / 1023.0f;
		var[i] = f_inv_sqrt(var[i]);
		//if(i < 20) printf("%f %f\r\n", sqrt(f_tofloat(var[i])), f_tofloat(f_sqrt(var[i])));
	}
	
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		for(int j = 0; j < 1024; j++) {
			buff[j] -= mean[i];
			buff[j] = buff[j] * var[i];
		}
		cv_write_arr(i, buff, 1024);
	}
	
	rewind(bparams);
	fread(buff2, 1024, 4, bparams);
	
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		for(int j = 0; j < 1024; j++) buff[j] = buff[j] * buff2[j];
		cv_write_arr(i, buff, 1024);
	}
	
	fread(buff2, 1024, 4, bparams);
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buff, 1024);
		for(int j = 0; j < 1024; j++) buff[j] += buff2[j];
		cv_write_arr(i, buff, 1024);
		//if(i == 0) for(int j = 0; j < 20; j++) printf("%f\r\n", f_tofloat(buff[j]));
	}
}

void matrix_multiply_with_embeddings(FILE* weights_file, matrix_buff* output) {
	float buffer[1024];
	float buffer2[1024];
	float buffer3[2048];
	for(uint16_t i = 0; i < output->rows; i++) {
		cv_read_copy_arr(i, buffer, 1024);
		rewind(weights_file);
		for(uint16_t j = 0; j < output->cols; j++) {
			fread(buffer2, 4, 1024, weights_file);
			float sum = 0;
			for(uint16_t k = 0; k < 1024; k++) sum += buffer[k] * buffer2[k];
			buffer3[j] = sum;
		}
		mb_write_row(output, i, buffer3);
	}
}

void feed_forward(FILE* in_params, FILE* out_params) {
	cv_create_copy();
	matrix_multiply_with_embeddings(in_params, &ff_matrix);
	float buffer[2048];
	float buffer2[2][2048];
	//Add bias
	fseek(in_params, 1024*2048*sizeof(float), SEEK_SET);
	fread(buffer2[0], 4, 2048, in_params);
	for(uint16_t i = 0; i < ff_matrix.rows; i++) {
		mb_read_row(&ff_matrix, i, buffer);
		for(uint16_t j = 0; j < ff_matrix.cols; j++) {
			buffer[j] += buffer2[0][j];
			buffer[j] = f_gelu(buffer[j]);
		}
		mb_write_row(&ff_matrix, i, buffer);
	}
	//Matmul
	for(uint16_t i = 0; i < 512; i++) {
		mb_read_row(&ff_matrix, i, buffer);
		rewind(out_params);
		for(uint16_t j = 0; j < 1024; j++) {
			fread(buffer2[0], 4, ff_matrix.cols, out_params);
			float sum = 0;
			for(uint16_t k = 0; k < ff_matrix.cols; k++) sum += buffer[k] * buffer2[0][k];
			buffer2[1][j] = sum;
		}
		cv_write_arr(i, buffer2[1], 1024);
	}
	//Add bias
	fseek(out_params, 2048*1024*sizeof(float), SEEK_SET);
	fread(buffer2[0], 4, 1024, out_params);
	for(uint16_t i = 0; i < 512; i++) {
		cv_read_arr(i, buffer, 1024);
		for(uint16_t j = 0; j < 1024; j++) buffer[j] += buffer2[0][j];
		cv_write_arr(i, buffer, 1024);
		//if(i == 508) for(uint16_t j = 0; j < 1024; j++) printf("%f\r\n", f_tofloat(buffer[j]));
	}
}

//sqrt(context_size) = sqrt(512)
#define SDK 22.627416998f

void self_attention(uint8_t which) {
	char fn_buffer[42];
	cv_create_copy();
	float buffer[64];
	float buffer2[64];
	float buffer3[512];
	float buffer4[512];
	for(uint8_t i = 0; i < 16; i++) {
		printf("\r%02X", i+1);
		fflush(stdout);
		sprintf(fn_buffer, "model/block%u/attention/%u/keys.bin", which, i);
		FILE* weights_file;
		weights_file = fopen(fn_buffer, "rb");
		matrix_multiply_with_embeddings(weights_file, &keys_matrix);
		fclose(weights_file);
		
		sprintf(fn_buffer, "model/block%u/attention/%u/queries.bin", which, i);
		weights_file = fopen(fn_buffer, "rb");
		matrix_multiply_with_embeddings(weights_file, &queries_matrix);
		fclose(weights_file);
		
		sprintf(fn_buffer, "model/block%u/attention/%u/values.bin", which, i);
		weights_file = fopen(fn_buffer, "rb");
		matrix_multiply_with_embeddings(weights_file, &values_matrix);
		fclose(weights_file);
		
		//Keys x queries
		const float div_by = SDK;
		for(uint16_t j = 0; j < 512; j++) {
			mb_read_row(&queries_matrix, j, buffer);
			for(uint16_t k = 0; k < j+1; k++) {
				mb_read_row(&keys_matrix, k, buffer2); //TODO: cache this
				float sum = 0;
				for(uint16_t l = 0; l < 64; l++) sum += buffer[l] * buffer2[l];
				buffer3[k] = sum / div_by;
			}
			for(uint16_t k = j+1; k < 512; k++) {
				buffer3[k] = 0;
			}
			//if(j == 511) for(uint16_t k = 0; k < 512; k++) printf("%f\r\n", f_tofloat(buffer3[k]));
			mb_write_row(&kq_matrix, j, buffer3);
		}
		
		//Softmax
		for(uint16_t j = 0; j < 512; j++) {
			mb_read_row(&kq_matrix, j, buffer3);
			if(j == 0) buffer3[0] = 1;
			else {
				float sum = 0;
				for(int k = 0; k < j+1; k++) {
					buffer3[k] = exp(buffer3[k]);
				}
				for(int k = 0; k < j+1; k++) sum += buffer3[k];
				if(sum == 0) printf("\r\n%d %d\r\n", which, j);
				for(int k = 0; k < j+1; k++) {
					buffer3[k] = buffer3[k] / sum;
					//if(which == 9 && j == 47) printf("%f\r\n", f_tofloat(buffer3[k]));
				}
			}
			
			//if(j == 32) for(int k = 0; k < 512; k++) printf("%f\r\n", f_tofloat(buffer3[k]));
			
			mb_write_row(&kq_matrix, j, buffer3);
		}
		
		//The values matrix needs to be transposed
		float paged_buffer[4][4096];
		float block_buffer[128];
		for(uint16_t j = 0; j < 512/128; j++) {
			uint16_t buffer_pos = 0;
			for(uint16_t k = 0; k < 128; k++) {
				mb_read_row(&values_matrix, j*128+k, paged_buffer[buffer_pos>>12]+(buffer_pos&0xFFF));
				buffer_pos += 64;
			}
			for(uint16_t k = 0; k < 64; k++) {
				for(uint16_t l = 0; l < 128; l++) {
					uint32_t val_addr = l * 64 + k;
					block_buffer[l] = paged_buffer[val_addr>>12][val_addr&0xFFF];
				}
				mb_write_block(&values_transposed_matrix, k, j, block_buffer);
			}
		}
		
		//mb_read_row(&values_transposed_matrix, 63, buffer3);
		//for(uint16_t k = 0; k < 512; k++) printf("%f\r\n", f_tofloat(buffer3[k]));
		
		//Final matrix multiply of kq_matrix times transposed values
		for(uint16_t j = 0; j < 512; j++) {
			mb_read_row(&kq_matrix, j, buffer4);
			for(uint16_t k = 0; k < 64; k++) {
				mb_read_row(&values_transposed_matrix, k, buffer3);
				float sum = 0;
				for(uint16_t l = 0; l < 512; l++) sum += buffer4[l] * buffer3[l];
				buffer2[k] = sum;
				//if(i == 10 && j == 200) printf("%f\r\n", f_tofloat(sum));
			}
			cv_write_arr64(j, i, buffer2);
		}
	}
	printf("\r\n");
}

void print_cv_sample() {
	float debug_buff[1024];
	printf("Context vec sample:\n\t");
	for(uint8_t i = 0; i < 2; i++) {
		cv_read_arr(i == 0 ? 0 : 511, debug_buff, 1024);
		for(int k2 = 0; k2 < 10; k2++) printf("%f ", debug_buff[k2]);
		printf("[...] ");
		for(int k2 = 0; k2 < 10; k2++) printf("%f ", debug_buff[1023-9+k2]);
		printf("\n");
		if(i == 0) printf("\t");
	}
}

void main(void) {
    bpe_init();
    uint16_t tbuff[512];
    uint16_t np = tokenize("Anna stared out the window and watched the monotonous structures of the city quickly being replaced by an expanse of green fields. Underneath her, the old wheels of the train screeched and shuddered, vibrating her in her seat, the only one that was perfectly aligned with a window, as most of the others had been removed long ago, leaving the carriage mostly empty and its plain metal walls and floor entirely exposed. She was probably", tbuff);
    for(int i = 0; i < np; i++) {
        tbuff[511-i] = tbuff[np-1-i];
    }
    for(int i = 0; i < 512-np; i++) tbuff[i] = 0;
    //for(int i = 0; i < 512; i++) printf("%u ", tbuff[i]);
    //printf("\r\n");
    //char obuff[512];
    //untokenize(tbuff+512-np, np, obuff);
    //puts(obuff);
    
    keys_matrix.rows = 512;
	keys_matrix.cols = 64;
	keys_matrix.start_block = 0;
	keys_matrix.data = (float*)malloc(64*512*sizeof(float));
	
	queries_matrix.rows = 512;
	queries_matrix.cols = 64;
	queries_matrix.start_block = 0;
	queries_matrix.data = (float*)malloc(64*512*sizeof(float));;
	
	values_matrix.rows = 512;
	values_matrix.cols = 64;
	values_matrix.start_block = 0;
	values_matrix.data = (float*)malloc(64*512*sizeof(float));
	
	values_transposed_matrix.rows = 64;
	values_transposed_matrix.cols = 512;
	values_transposed_matrix.start_block = 0;
	values_transposed_matrix.data = (float*)malloc(512*64*sizeof(float));
	
	kq_matrix.rows = 512;
	kq_matrix.cols = 512;
	kq_matrix.start_block = 0;
	kq_matrix.data = (float*)malloc(512*512*sizeof(float));
    
	ff_matrix.rows = 512;
	ff_matrix.cols = 2048;
	ff_matrix.start_block = 0;
	ff_matrix.data = (float*)malloc(512*2048*sizeof(float));
	
	if(!keys_matrix.data || !queries_matrix.data || !values_matrix.data || !kq_matrix.data || !values_transposed_matrix.data) {
		printf("Memory allocation fail\r\n");
		exit(1);
	}
	
	gelu_lut = fopen("gelu.bin", "rb");
	
    //Layers
	const uint8_t num_iterations = 1;
    for(uint8_t iteration = 0; iteration < num_iterations; iteration++) {
		printf("Iteration %d\r\n", iteration+1);
		//Embedding
		{
			FILE *emb_fil;
			emb_fil = fopen("model/token_embeddings.bin", "rb");
			float raw[1024];
			float raw2[1024];
			for(int i = 0; i < 512; i++) {
				fseek(emb_fil, tbuff[i]*1024*sizeof(float), SEEK_SET);
				fread(raw, 1024, 4, emb_fil);
				cv_write_arr(i, raw, 1024);
			}
			fclose(emb_fil);
			emb_fil = fopen("model/positional_embeddings.bin", "rb");
			for(int i = 0; i < 512; i++) {
				fread(raw, 1024, 4, emb_fil);
				cv_read_arr(i, raw2, 1024);
				for(int j = 0; j < 1024; j++) raw[j] = raw[j] + raw2[j];
				cv_write_arr(i, raw, 1024);
			}
			fclose(emb_fil);
		}
		print_cv_sample();
		printf("%u\r\n", cv_hash());
		
		const uint8_t dump_layer = 9;
		char fname_buff[42];
		FILE *bparams;
		for(uint8_t i = 0; i < 24; i++) {
			printf("%u/%u\r\n", i+1, 24);
			if(i == dump_layer) {
				FILE *outfile = fopen("layerdump.bin", "wb");
				float wbuff[1024];
				for(int j = 0; j < 512; j++) {
					cv_read_arr(j, wbuff, 1024);
					fwrite(wbuff, 4, 1024, outfile);
				}
				fclose(outfile);
				printf("CV dumped at layer %u\r\n", dump_layer);
			}
			sprintf(fname_buff, "model/block%u/norm1.bin", i);
			bparams = fopen(fname_buff, "rb");
			cv_create_shortcut();
			batch_norm(bparams);
			fclose(bparams);
			print_cv_sample();
			self_attention(i);
			print_cv_sample();
			cv_add_shortcut();
			cv_create_shortcut();
			sprintf(fname_buff, "model/block%u/norm2.bin", i);
			bparams = fopen(fname_buff, "rb");
			batch_norm(bparams);
			fclose(bparams);
			sprintf(fname_buff, "model/block%u/dense/ff0.bin", i);
			bparams = fopen(fname_buff, "rb");
			FILE* out_params;
			sprintf(fname_buff, "model/block%u/dense/ff1.bin", i);
			out_params = fopen(fname_buff, "rb");
			feed_forward(bparams, out_params);
			fclose(bparams);
			fclose(out_params);
			cv_add_shortcut();
			print_cv_sample();
		}
		bparams = fopen("model/norm.bin", "rb");
		batch_norm(bparams);
		fclose(bparams);
		print_cv_sample();
		printf("%u\r\n", cv_hash());
		
		bparams = fopen("model/unembedding.bin", "rb");
		float buff[1024];
		float buff2[1024];
		float buff3[50257]; //temp
		const float temperature = 0.621f;
		cv_read_arr(511, buff, 1024); //Only interested in the latest prediction
		for(uint16_t i = 0; i < 50257; i++) {
			fread(buff2, 4, 1024, bparams);
			float sum = 0;
			for(uint16_t j = 0; j < 1024; j++) sum += buff[j] * buff2[j];
			sum = sum / temperature;
			buff3[i] = sum;
		}
		fclose(bparams);
		//Needs topk first
		uint16_t topk_idx[10];
		float topk_vals[10];
		for(uint8_t i = 0; i < 10; i++) topk_vals[i] = -10000;
		for(uint16_t i = 0; i < 50257; i++) {
			float val = buff3[i];
			if(topk_vals[0] >= val) continue;
			for(uint8_t j = 9; j != 255; j--) {
				if(val > topk_vals[j]) {
					if(j == 0) {
						topk_vals[j] = val;
						topk_idx[j] = i;
					}else {
						for(uint8_t k = 0; k < j; k++) {
							topk_vals[k] = topk_vals[k + 1];
							topk_idx[k] = topk_idx[k + 1];
						}
						topk_vals[j] = val;
						topk_idx[j] = i;
					}
					break;
				}
			}
		}
		
		printf("\r\ntopk:\r\n");
		for(uint8_t i = 0; i < 10; i++) {
			printf("%d - %f\r\n", topk_idx[i], topk_vals[i]);
		}
		printf("\r\n");
		
		float sum = 0;
		for(uint16_t i = 0; i < 10; i++) {
			topk_vals[i] = exp(topk_vals[i]);
			sum += topk_vals[i];
		}
		for(uint16_t i = 0; i < 10; i++) {
			topk_vals[i] = topk_vals[i] / sum;
			printf("%f\r\n", topk_vals[i]);
		}
		float random = (float)(rand() & 0xFFFF) / 65536.0f;
		uint16_t token;
		sum = 0;
		for(token = 0; token < 10; token++) {
			sum += topk_vals[token];
			if(sum >= random) break;
		}
		for(int i = 0; i < 511; i++) tbuff[i] = tbuff[i + 1];
		tbuff[511] = token == 10 ? 0 : topk_idx[token];
	}
    char obuff[512];
    untokenize(tbuff+512-np-num_iterations, np+num_iterations, obuff);
    puts(obuff);
	
	free(keys_matrix.data);
	free(queries_matrix.data);
	free(values_matrix.data);
	free(kq_matrix.data);
	free(values_transposed_matrix.data);
	free(ff_matrix.data);
	
	fclose(gelu_lut);
}
