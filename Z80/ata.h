#ifndef ATA_H_
#define ATA_H_

typedef struct {
	uint8_t id;
	uint32_t start;
	uint32_t size;
	uint32_t end;
	uint8_t type;
} partition;

partition* drive_partition(uint8_t x);
void ata_init();
void ata_reset();
uint8_t ata_runDiag();
uint8_t ata_readSectors(uint32_t lba, uint8_t count);
uint8_t ata_writeSectors(uint32_t lba, uint8_t count);
void ata_writeByte(uint8_t addr, uint8_t dat);
uint8_t ata_readByte(uint8_t addr);
void ata_busyWait();
void ata_waitForData();
void ata_readBuffer(uint8_t *buffer);
void ata_writeBuffer(uint8_t *buffer);
void ata_set_write_protect(uint32_t wp);
void newl();

#endif
