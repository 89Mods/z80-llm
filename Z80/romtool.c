#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void main() {
	FILE* bootloaderfile = fopen("./bootloader.bin", "rb");
	FILE* bootfile = fopen("./boot.bin", "rb");
	FILE* romfile = fopen("./EPROM.bin", "wb");
	FILE* diskfile = fopen("../disk.img", "rb+");
	if(!bootfile || !romfile || !diskfile) {
		printf("Could not open required files!\r\n");
		exit(1);
		return;
	}
	const uint16_t data_start = 0x9700;
	const uint16_t bootloader_size = 0x0200;
	uint8_t buffer[32768];
	memset(buffer, 0, 32768);
	fread(buffer, 1, bootloader_size, bootloaderfile);
	fclose(bootloaderfile);
	fwrite(buffer, 1, bootloader_size, romfile);
	fread(buffer, 1, 32768-bootloader_size, bootfile);
	fwrite(buffer, 1, 32768-bootloader_size, romfile);
	fclose(romfile);
	
	int read = fread(buffer, 1, 32768, bootfile);
	fclose(bootfile);
	printf("Overflow: %d bytes\r\n", read);
	if(read + 32768 > data_start) {
		printf("WARNING: Code goes past data start\r\n");
	}
	printf("Code end: %04x\r\n", read + 32768);
	fseek(diskfile, 512, SEEK_SET);
	fwrite(buffer, 1, read, diskfile);
	fclose(diskfile);
}
