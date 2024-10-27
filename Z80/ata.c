#include <stdint.h>
#include <stdio.h>
#include "defines.h"
#include "ata.h"

uint32_t size_in_sectors = 0;
partition drive_partitions[4];
uint32_t write_protect_limit = 0xFFFFFFFF;

void newl() {putchar('\r'); putchar('\n');}

partition* drive_partition(uint8_t x) {
	return drive_partitions+x;
}

void ata_set_write_protect(uint32_t wp) {
	write_protect_limit = wp;
}

void ata_init() {
	uint8_t buff[512];
	ata_reset();
	puts("ATA Reset\r\n");
	//Enable LBA mode
	ata_writeByte(ATA_REG_DRIVE_HEAD, ATA_DRV_HEAD_BASE);
	ata_busyWait();
	uint8_t res = ata_runDiag();
	puts("ATA Diag returned "); puthex(res); newl();
	if(res != 0x01) {
		puts("\033[1;31mFATAL:\033[0m Ata Diag failed. Drive bad.\r\n");
		fatal();
	}
	
	ata_writeByte(ATA_REG_STAT_CMD, ATA_CMD_IDENT);
	ata_busyWait();
	ata_waitForData();
	ata_readBuffer(buff);
	
	if((buff[49 * 2 + 1] & 2) != 2) {
		puts("\033[1;31mFATAL:\033[0m Drive does not support LBA.\r\n");
		fatal();
	}
	size_in_sectors = *((uint32_t*) (buff + 120));
	puts("Drive serial: ");
	for(uint8_t i = 0; i < 20; i+=2) {
		putchar(buff[20 + i + 1]);
		putchar(buff[20 + i]);
	}
	newl();
	puts("Drive model: ");
	for(uint8_t i = 0; i < 40; i+=2) {
		putchar(buff[54 + i + 1]);
		putchar(buff[54 + i]);
	}
	newl();
	sprintf(buff, "Drive capacity: %luKiB\r\n", size_in_sectors>>1);
	puts(buff);
	if((buff[53 * 2] & 1) == 0) puts("\033[1;31mWARNING:\033[0m Drive is a liar.\r\n");
	
	//Detect partitions
	if(ata_readSectors(0, 1)) {
		puts("\033[1;31mFATAL:\033[0m Could not read sector 0.\r\n");
		fatal();
	}
	ata_readBuffer(buff);
	if(buff[510] != 0x55 || buff[511] != 0xAA) {
		puts("\033[1;31mFATAL:\033[0m Invalid MBR\r\n");
		fatal();
	}
	for(uint8_t i = 0; i < 4; i++) {
		drive_partitions[i].id = i;
		drive_partitions[i].start = *((uint32_t*) (buff + (446 + i * 16 + 8)));
		drive_partitions[i].size = *((uint32_t*) (buff + (446 + i * 16 + 12)));
		drive_partitions[i].end = drive_partitions[i].start + drive_partitions[i].size;
		drive_partitions[i].type = buff[446 + i * 16 + 4];
		char clear = 0;
		if(drive_partitions[i].start > size_in_sectors || drive_partitions[i].end > size_in_sectors) {
			puts("\033[1;31mWARNING:\033[0m Invalid partition found on drive, MBR may be bad\r\n");
			clear = 1;
		}
		if(drive_partitions[i].size == 0 || drive_partitions[i].type == 0 || clear) {
			drive_partitions[i].start = 0;
			drive_partitions[i].end = 0;
			drive_partitions[i].size = 0;
			drive_partitions[i].type = 0;
		}
	}
}

void ata_reset() {
	io_outp(ADDR_PIO1|PIO_DAT|PIO_B, io_inp(ADDR_PIO1|PIO_DAT|PIO_B)&(~PATA_RESET));
	for(uint8_t i = 0; i < 200; i++) volatile asm("nop");
	io_outp(ADDR_PIO1|PIO_DAT|PIO_B, io_inp(ADDR_PIO1|PIO_DAT|PIO_B)|PATA_RESET);
	for(uint8_t i = 0; i < 200; i++) volatile asm("nop");
	//Wait for IORDY
	while((io_inp(ADDR_PIO1|PIO_DAT|PIO_A)&PATA_IORDY) == 0) {}
	ata_writeByte(ATA_REG_ASTA_CTRL, 0x06);
	for(uint16_t i = 0; i < 20000; i++) volatile asm("nop");
	ata_writeByte(ATA_REG_ASTA_CTRL, 0x00);
	for(uint16_t i = 0; i < 20000; i++) volatile asm("nop");
	ata_busyWait();
}

uint8_t ata_runDiag() {
	ata_writeByte(ATA_REG_STAT_CMD, ATA_CMD_DIAG);
	for(uint8_t i = 0; i < 200; i++) volatile asm("nop");
	ata_busyWait();
	return ata_readByte(ATA_REG_ERR_FEAT);
}

uint8_t ata_readSectors(uint32_t lba, uint8_t count) {
	ata_busyWait();
	ata_writeByte(ATA_REG_SECTOR_NUM, (uint8_t)lba);
	ata_writeByte(ATA_REG_CYLINDER_LOW, (uint8_t)(lba >> 8));
	ata_writeByte(ATA_REG_CYLINDER_HIGH, (uint8_t)(lba >> 16));
	ata_writeByte(ATA_REG_DRIVE_HEAD, ATA_DRV_HEAD_BASE | (uint8_t)(lba >> 24));
	ata_writeByte(ATA_REG_SECTOR_COUNT, count);
	ata_writeByte(ATA_REG_STAT_CMD, ATA_CMD_READ_SEC);
	ata_busyWait();
	if(ata_readByte(ATA_REG_STAT_CMD) & 0x01) return ata_readByte(ATA_REG_ERR_FEAT)|5;
	ata_waitForData();
	return 0;
}

uint8_t ata_writeSectors(uint32_t lba, uint8_t count) {
	if(lba < write_protect_limit) {
		puts("Write protect triggered!\r\n");
		return 0xFF;
	}
	ata_busyWait();
	ata_writeByte(ATA_REG_SECTOR_NUM, (uint8_t)lba);
	ata_writeByte(ATA_REG_CYLINDER_LOW, (uint8_t)(lba >> 8));
	ata_writeByte(ATA_REG_CYLINDER_HIGH, (uint8_t)(lba >> 16));
	ata_writeByte(ATA_REG_DRIVE_HEAD, ATA_DRV_HEAD_BASE | (uint8_t)(lba >> 24));
	ata_writeByte(ATA_REG_SECTOR_COUNT, count);
	ata_writeByte(ATA_REG_STAT_CMD, ATA_CMD_WRITE_SEC);
	ata_busyWait();
	if(ata_readByte(ATA_REG_STAT_CMD) & 0x01) return ata_readByte(ATA_REG_ERR_FEAT)|5;
	ata_waitForData();
	return 0;
}

void ata_writeByte(uint8_t addr, uint8_t dat) __naked __preserves_regs(iyl,iyh) {
#asm
	ld hl,4
	add hl,sp
	ld a,(hl)
	or PATA_DIOW|PATA_DIOR
	ld d,a
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	xor a
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	dec hl
	dec hl
	ld a,(hl)
	out (ADDR_PIO0|PIO_DAT|PIO_A),a
	ld a,d
	xor PATA_DIOW
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld a,d
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	or PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ret
#endasm
}	

uint8_t ata_readByte(uint8_t addr) __naked __z88dk_fastcall __preserves_regs(iyl,iyh,bc,de) {
#asm
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,PATA_DIOW|PATA_DIOR
	or l
	ld h,a
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	xor a,PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	in a,(ADDR_PIO0|PIO_DAT|PIO_A)
	ld l,a
	ld a,h
	or PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld h,0
	ret
#endasm	
}

void ata_busyWait() __naked __preserves_regs(iyl,iyh,de,hl) {
#asm
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,ATA_REG_STAT_CMD|PATA_DIOW|PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
ata_wait_loop:
	ld a,ATA_REG_STAT_CMD|PATA_DIOW
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	in a,(ADDR_PIO0|PIO_DAT|PIO_A)
	ld b,a
	ld a,ATA_REG_STAT_CMD|PATA_DIOW|PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	bit 7,b
	jp nz,ata_wait_loop
	bit 6,b
	jp z,ata_wait_loop
	ld a,ATA_REG_STAT_CMD|PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ret
#endasm
}

void ata_waitForData() __naked __preserves_regs(iyl,iyh,de,hl) {
#asm
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,ATA_REG_STAT_CMD|PATA_DIOW|PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
ata_wait_loop_1:
	ld a,ATA_REG_STAT_CMD|PATA_DIOW
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	in a,(ADDR_PIO0|PIO_DAT|PIO_A)
	ld b,a
	ld a,ATA_REG_STAT_CMD|PATA_DIOW|PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	bit 3,b
	jp z,ata_wait_loop_1
	ld a,ATA_REG_STAT_CMD|PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ret
#endasm
}

void ata_readBuffer(uint8_t *buffer) __naked __z88dk_fastcall __preserves_regs(iyl,iyh,de) {
#asm
	//Configure data port as inputs
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	ld a,ATA_DATA|PATA_DIOR|PATA_DIOW
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	//Loop
	ld b,0
ata_read_loop:
	//Read two words of data
	ld a,ATA_DATA|PATA_DIOW
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	in a,(ADDR_PIO0|PIO_DAT|PIO_A)
	ld (hl),a
	inc hl
	in a,(ADDR_PIO0|PIO_DAT|PIO_B)
	ld (hl),a
	inc hl
	ld a,ATA_DATA|PATA_DIOR|PATA_DIOW
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	djnz ata_read_loop
	//Idle
	ld a,ATA_DATA|PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ret
#endasm
}

void ata_writeBuffer(uint8_t *buffer) __naked __z88dk_fastcall __preserves_regs(iyl,iyh) {
#asm
	//Configure data ports as outputs
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	xor a
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	xor a
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	//Loop
	ld b,0
ata_write_loop:
	//Write two words of data
	ld a,ATA_DATA|PATA_DIOW|PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld a,(hl)
	out (ADDR_PIO0|PIO_DAT|PIO_A),a
	inc hl
	ld a,(hl)
	out (ADDR_PIO0|PIO_DAT|PIO_B),a
	inc hl
	ld a,ATA_DATA|PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	djnz ata_write_loop
	ld a,ATA_DATA|PATA_DIOW|PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	//Change data ports back to inputs
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	//Idle
	ld a,ATA_DATA|PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ret
#endasm
}
