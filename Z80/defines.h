#ifndef DEFINES_H_
#define DEFINES_H_
#define ADDR_PIO0 0
#define ADDR_PIO1 32
#define ADDR_UART0 64
#define ADDR_UART1 96

#define UART_DATA 0
#define UART_COMMAND 1

#define PIO_DAT 0
#define PIO_CMD 1
#define PIO_A 0
#define PIO_B 2

#define LED_ERR 128
#define UART_RESET 64
#define LED_WHITE 32
#define PATA_RESET 16

#define PATA_DIOW 1
#define PATA_DIOR 2
#define PATA_IORDY 4
#define PATA_DA1 8
#define PATA_DA0 16
#define PATA_CS0 32
#define PATA_CS1 64
#define PATA_DA2 128

#define ATA_NOP (PATA_CS0|PATA_CS1)
#define ATA_DATA PATA_CS1
#define ATA_REG_STAT_CMD (PATA_CS1|PATA_DA0|PATA_DA1|PATA_DA2)
#define ATA_REG_ASTA_CTRL (PATA_CS0|PATA_DA2|PATA_DA1)
#define ATA_REG_ERR_FEAT (PATA_CS1|PATA_DA0)
#define ATA_REG_SECTOR_COUNT (PATA_CS1|PATA_DA1)
#define ATA_REG_ADDR (PATA_CS0|PATA_DA0|PATA_DA1|PATA_DA2)
#define ATA_REG_SECTOR_NUM (PATA_CS1|PATA_DA1|PATA_DA0)
#define ATA_REG_CYLINDER_LOW (PATA_CS1|PATA_DA2)
#define ATA_REG_CYLINDER_HIGH (PATA_CS1|PATA_DA2|PATA_DA0)
#define ATA_REG_DRIVE_HEAD (PATA_CS1|PATA_DA1|PATA_DA2)

#define ATA_CMD_DIAG 0x90
#define ATA_CMD_IDENT 0xEC
#define ATA_CMD_READ_SEC 0x21
#define ATA_CMD_WRITE_SEC 0x31

#define ATA_DRV_HEAD_BASE 0b11100000

void io_outp(uint16_t addr, uint8_t val);
unsigned char io_inp(uint16_t addr);
void puthex(uint8_t c);
void fatal();
void set_page(uint8_t page);
uint8_t get_page();
char getchar();
void check_uart_in();
void decr64(uint64_t* p);
void disable_text_buffer();
void enable_text_buffer();

#define HIGHMEM_START 0xC000

#endif
