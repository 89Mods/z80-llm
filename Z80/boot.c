#include <stdint.h>
#include <stdio.h>
#include "defines.h"
#include "ata.h"
#include "tokenizer.h"
#include "ext4.h"
#include "llm.h"

const char* initial_text = "Anna stared out the window and watched the monotonous structures of the city quickly being replaced by an expanse of green fields. Underneath her, the old wheels of the train screeched and shuddered, vibrating her in her seat, the only one that was perfectly aligned with a window, as most of the others had been removed long ago, leaving the carriage mostly empty and its plain metal walls and floor entirely exposed. She was probably";

#define UART_MAIN 1
#define UART_DEBUG 0

#define UART_MODE_WORD 0b11001111
#define UART_COMMAND_WORD 0b00010111

char* text_buff = (char*)0xC000;
char* text_buff2 = (char*)0xE000;
uint16_t text_pos;
uint16_t text_pos2;

uint8_t curr_uart = UART_DEBUG;
uint8_t buffer_enabled = 1;
uint8_t hidden_main_buffer_enabled = 1;

void disable_text_buffer() { buffer_enabled = 0; }
void enable_text_buffer() { buffer_enabled = 1; }

void put_text_buff(char* buff, uint16_t* pos, uint8_t c) {
	uint8_t p = get_page();
	set_page(2);
	if(*pos >= 0x1FFF) {
		for(uint16_t i = 0; i < 0x1FFF; i++) buff[i] = buff[i+1];
		*pos = *pos - 1;
	}
	buff[*pos] = c;
	*pos = *pos + 1;
	buff[*pos] = 0;
	set_page(p);
}

int putchar(int c) {
	if(c == 0) return 0;
	if(curr_uart) {
		if(hidden_main_buffer_enabled) put_text_buff(text_buff, &text_pos, c);
		while((io_inp(ADDR_UART1|UART_COMMAND)&4) == 0) {}
		io_outp(ADDR_UART1|UART_DATA, c);
	}else {
		if(buffer_enabled) put_text_buff(text_buff2, &text_pos2, c);
		while((io_inp(ADDR_UART0|UART_COMMAND)&4) == 0) {}
		io_outp(ADDR_UART0|UART_DATA, c);
	}
	return c;
}

char getchar() {
	char res = 0;
	if(curr_uart) {
		if((io_inp(ADDR_UART1|UART_COMMAND)&2) == 0) return 0;
		res = io_inp(ADDR_UART1|UART_DATA);
		io_outp(ADDR_UART1|UART_COMMAND, UART_COMMAND_WORD);
	}else {
		if((io_inp(ADDR_UART0|UART_COMMAND)&2) == 0) return 0;
		res = io_inp(ADDR_UART0|UART_DATA);
		io_outp(ADDR_UART0|UART_COMMAND, UART_COMMAND_WORD);
	}
	return res;
}

void check_uart_in() {
	uint8_t prev_uart = curr_uart;
	curr_uart = UART_MAIN;
	char c = getchar();
	uint8_t p = get_page();
	if(c == 'r') {
		hidden_main_buffer_enabled = 0;
		set_page(2);
		puts(text_buff);
		set_page(p);
		hidden_main_buffer_enabled = 1;
	}
	curr_uart = UART_DEBUG;
	c = getchar();
	if(c == 'p') {
		disable_text_buffer();
		puthex(p);
		newl();
		enable_text_buffer();
	}else if(c == 'r') {
		disable_text_buffer();
		set_page(2);
		puts(text_buff2);
		set_page(p);
		enable_text_buffer();
	}
	curr_uart = prev_uart;
}

int puts(char *c) {
	while(*c != '\0') {
		putchar(*c);
		c++;
	}
	return 1;
}

const char hexchars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
void puthex(uint8_t c) {
	putchar(hexchars[c >> 4]);
	putchar(hexchars[c & 15]);
}

void fatal() {
	io_outp(ADDR_PIO1|PIO_DAT|PIO_B, LED_ERR);
	//*exit = 0x55;
	while(1) volatile asm("halt");
}

void decr64(uint64_t* p) __naked __z88dk_fastcall __preserves_regs(iyl,iyh,de,bc) {
#asm
	ld a,(hl)
	add a, 0xFF
	ld (hl),a
	inc hl
	ld a,(hl)
	adc a, 0xFF
	ld (hl),a
	inc hl
	ld a,(hl)
	adc a, 0xFF
	ld (hl),a
	inc hl
	ld a,(hl)
	adc a, 0xFF
	ld (hl),a
	inc hl
	ld a,(hl)
	adc a, 0xFF
	ld (hl),a
	inc hl
	ld a,(hl)
	adc a, 0xFF
	ld (hl),a
	inc hl
	ld a,(hl)
	adc a, 0xFF
	ld (hl),a
	inc hl
	ld a,(hl)
	adc a, 0xFF
	ld (hl),a
	ret
#endasm
}

void main(void) {
	//Init IO devices
	io_outp(ADDR_PIO0|PIO_CMD|PIO_A, 0);
	io_outp(ADDR_PIO0|PIO_CMD|PIO_B, 0);
	io_outp(ADDR_PIO1|PIO_CMD|PIO_A, 0);
	io_outp(ADDR_PIO1|PIO_CMD|PIO_B, 0);
	
	io_outp(ADDR_PIO0|PIO_CMD|PIO_A, 0b11001111);
	io_outp(ADDR_PIO0|PIO_CMD|PIO_A, 0xFF);
	io_outp(ADDR_PIO0|PIO_CMD|PIO_B, 0b11001111);
	io_outp(ADDR_PIO0|PIO_CMD|PIO_B, 0xFF);
	io_outp(ADDR_PIO1|PIO_CMD|PIO_A, 0b11001111);
	io_outp(ADDR_PIO1|PIO_CMD|PIO_A, 0b00000100);
	io_outp(ADDR_PIO1|PIO_CMD|PIO_B, 0b11001111);
	io_outp(ADDR_PIO1|PIO_CMD|PIO_B, 0);
	
	io_outp(ADDR_PIO0|PIO_CMD|PIO_A, 0b00000111);
	io_outp(ADDR_PIO0|PIO_CMD|PIO_B, 0b00000111);
	io_outp(ADDR_PIO1|PIO_CMD|PIO_A, 0b00000111);
	io_outp(ADDR_PIO1|PIO_CMD|PIO_B, 0b00000111);
	
	io_outp(ADDR_PIO0|PIO_DAT|PIO_A, 0);
	io_outp(ADDR_PIO0|PIO_DAT|PIO_B, 0);
	io_outp(ADDR_PIO1|PIO_DAT|PIO_A, PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1);
	io_outp(ADDR_PIO1|PIO_DAT|PIO_B, UART_RESET|LED_ERR);
	set_page(2);
	*text_buff = 0;
	text_pos = 0;
	text_pos2 = 0;
	*text_buff2 = 0;
	set_page(0);
	
	//Delay
	for(uint16_t i = 0; i < 60000; i++) volatile asm("nop");
	io_outp(ADDR_PIO1|PIO_DAT|PIO_B, LED_WHITE);
	for(uint8_t i = 0; i < 100; i++) volatile asm("nop");
	
	//Configure UARTs for 38400 baud
	io_outp(ADDR_UART0|UART_COMMAND, UART_MODE_WORD); // 1 stop bit, no parity, 8-bit, 64X baud rate divisor
	io_outp(ADDR_UART0|UART_COMMAND, UART_COMMAND_WORD); // Enable TX, RX
	for(uint16_t i = 0; i < 5200; i++) volatile asm("nop");
	//Again for the other UART
	io_outp(ADDR_UART1|UART_COMMAND, UART_MODE_WORD);
	io_outp(ADDR_UART1|UART_COMMAND, UART_COMMAND_WORD);
	
	puts("IO Bus hardware initialized\r\n");
	ata_init();
	io_outp(ADDR_PIO1|PIO_DAT|PIO_B, PATA_RESET);
	puts("All hardware initialized\r\n\n");
	
	uint8_t first_ext4 = 255;
	for(uint8_t i = 0; i < 4; i++) {
		if(drive_partition(i)->type == 0x83) {
			first_ext4 = i;
			break;
		}
	}
	if(first_ext4 == 255) {
		puts("\033[1;31mFATAL:\033[0m No partition of type ext4 found.\r\n");
		fatal();
	}
	
	int e4ret = ext4_mount(drive_partition(first_ext4)->start);
	if(e4ret != EXT4_OKAY) {
		puts("\033[1;31mFATAL:\033[0m Failed to mount ext4 partition. 0x"); puthex(e4ret); newl();
		fatal();
	}
	puts("FS Mounted Successfully\r\n\n");
	curr_uart = UART_MAIN;
	puts("HARDWARE READY!\r\n");
	curr_uart = UART_DEBUG;
	/*uint64_t inode_num;
	uint8_t file_type;
	e4ret = ext4_find_file("/test.txt", &inode_num, &file_type);
	char tbuff[64];
	sprintf(tbuff, "Found file /test.txt at inode %d of type %d (Regular file) with ", (int)inode_num, file_type);
	puts(tbuff);
	ext4_inode inode;
	ext4_read_inode(inode_num, &inode);
	const char aaaAAAAaa[3] = {'r', 'w', 'x'};
	for(uint8_t i = 0; i < 9; i++) {
		uint8_t perm = (inode.i_mode & 0x100) != 0;
		inode.i_mode <<= 1;
		if(perm) putchar(aaaAAAAaa[i % 3]);
		else putchar('-');
	}
	puts("\r\n\r\nContents:\r\n\r\n");
	ext4_FIL fhandle;
	e4ret = ext4_open_read(inode_num, &fhandle);
	uint32_t read;
	while(1) {
		e4ret = ext4_read(&fhandle, tbuff, 63, &read);
		if(read == 0) break;
		tbuff[read] = 0;
		puts(tbuff);
	}
	puts("\r\n");
	fatal();*/
	/*uint16_t tbuff[30];
	char buffa[88];
	bpe_init();
	uint16_t len = tokenize("I am a cute and soft Avali, I am a birb from outer space, lalalala, underfloofs!", tbuff);
	for(uint16_t i = 0; i < len; i++) {
		uint16_t val = tbuff[i];
		sprintf(buffa, "%u ", val);
		puts(buffa);
	}
	putchar('\r');
	putchar('\n');
	untokenize(tbuff, len, buffa);
	puts(buffa);
	putchar('\r');
	putchar('\n');*/
	bpe_init();
	newl();
	
	curr_uart = UART_MAIN;
	puts("BPE READY!\r\n");
	curr_uart = UART_DEBUG;
	
	e4ret = llm_init(initial_text, drive_partition(first_ext4)->end + 32);
	if(e4ret) fatal();
	curr_uart = UART_MAIN;
	puts("LLM INITIALIZED!\r\n");
	puts("\033[1;30m\033[1;47m");
	{
		const char* c = initial_text;
		while(*c != '\0') {
			putchar(*c);
			c++;
		}
	}
	puts("\033[0m");
	curr_uart = UART_DEBUG;
	
	while(1) {
		uint16_t next_token = 0;
		llm_step(&next_token);
		char str_buff[20];
		untokenize(next_token, str_buff);
		curr_uart = UART_MAIN;
		puts(str_buff);
		curr_uart = UART_DEBUG;
	}
}

void set_page(uint8_t page) {
	//page &= 0x0F;
	uint8_t orig = io_inp(ADDR_PIO1|PIO_DAT|PIO_B);
	orig &= 0xF0;
	orig |= page;
	io_outp(ADDR_PIO1|PIO_DAT|PIO_B, orig);
}

uint8_t get_page() {
	return io_inp(ADDR_PIO1|PIO_DAT|PIO_B) & 0x0F;
}

void io_outp(uint16_t addr, uint8_t val) __naked __preserves_regs(iyl,iyh) {
#asm
	ld hl,2
	add hl,sp
	ld d,(hl)
	inc hl
	inc hl
	ld c,(hl)
	ld b,0
	out (c),d
	ret
#endasm
}

unsigned char io_inp(uint16_t addr) __z88dk_fastcall __naked __preserves_regs(de,a,iyl,iyh) {
#asm
	ld c,l
	ld b,0
	in b,(c)
	ld h,0
	ld l,b
	ret
#endasm
}
