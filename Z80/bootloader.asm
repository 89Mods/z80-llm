ADDR_PIO0 equ 0
ADDR_PIO1 equ 32
ADDR_UART0 equ 64
ADDR_UART1 equ 96
PIO_DAT equ 0
PIO_CMD equ 1
PIO_A equ 0
PIO_B equ 2
LED_ERR equ 128
UART_RESET equ 64
LED_WHITE equ 32
PATA_RESET equ 16
PATA_DIOW equ 1
PATA_DIOR equ 2
PATA_IORDY equ 4
PATA_DA1 equ 8
PATA_DA0 equ 16
PATA_CS0 equ 32
PATA_CS1 equ 64
PATA_DA2 equ 128
ATA_NOP equ (PATA_CS0|PATA_CS0)
ATA_DATA equ PATA_CS1
ATA_REG_STAT_CMD equ (PATA_CS1|PATA_DA0|PATA_DA1|PATA_DA2)
ATA_REG_ASTA_CTRL equ (PATA_CS0|PATA_DA2|PATA_DA1)
ATA_REG_DRIVE_HEAD equ (PATA_CS1|PATA_DA1|PATA_DA2)
ATA_REG_SECTOR_NUM equ (PATA_CS1|PATA_DA1|PATA_DA0)
ATA_REG_CYLINDER_LOW equ (PATA_CS1|PATA_DA2)
ATA_REG_CYLINDER_HIGH equ (PATA_CS1|PATA_DA2|PATA_DA0)
ATA_REG_SECTOR_COUNT equ (PATA_CS1|PATA_DA1)
ATA_DRV_HEAD_BASE equ 0b11100000
ATA_CMD_READ_SEC equ 0x21

	org 0
init:
	di
	ld sp,65535
	ld a, 0
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	out (ADDR_PIO1|PIO_CMD|PIO_A),a
	out (ADDR_PIO1|PIO_CMD|PIO_B),a
	
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	ld a,0b11001111
	out (ADDR_PIO1|PIO_CMD|PIO_A),a
	ld a,0b00000100
	out (ADDR_PIO1|PIO_CMD|PIO_A),a
	ld a,0b11001111
	out (ADDR_PIO1|PIO_CMD|PIO_B),a
	ld a,0
	out (ADDR_PIO1|PIO_CMD|PIO_B),a
	ld a,0b00000111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	out (ADDR_PIO0|PIO_CMD|PIO_B),a
	out (ADDR_PIO1|PIO_CMD|PIO_A),a
	out (ADDR_PIO1|PIO_CMD|PIO_B),a
	ld a,0
	out (ADDR_PIO0|PIO_DAT|PIO_A),a
	out (ADDR_PIO0|PIO_DAT|PIO_B),a
	ld a,PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld a,UART_RESET|LED_ERR
	out (ADDR_PIO1|PIO_DAT|PIO_B),a
	
	ld b,255
delay:
	nop
	nop
	djnz delay
	
	ld a,UART_RESET|LED_ERR|PATA_RESET
	out (ADDR_PIO1|PIO_DAT|PIO_B),a
	ld b,255
delay2:
	nop
	nop
	nop
	nop
	djnz delay2
	
	;Wait for IORDY
iordy_wait:
	in a,(ADDR_PIO1|PIO_DAT|PIO_A)
	bit 2,a
	jp z,iordy_wait
	
	ld b,ATA_REG_ASTA_CTRL
	ld c,0x06
	call ata_write_byte
	call long_delay
	ld b,ATA_REG_ASTA_CTRL
	ld c,0
	call ata_write_byte
	call long_delay
	call ata_busy_wait
	
	ld b,ATA_REG_DRIVE_HEAD
	ld c,ATA_DRV_HEAD_BASE
	call ata_write_byte
	call ata_busy_wait
	
	ld b,ATA_REG_SECTOR_NUM
	ld c,0x01
	call ata_write_byte
	ld b,ATA_REG_CYLINDER_LOW
	ld c,0
	call ata_write_byte
	ld b,ATA_REG_CYLINDER_HIGH
	ld c,0
	call ata_write_byte
	ld b,ATA_REG_SECTOR_COUNT
	ld c,32
	call ata_write_byte
	ld b,ATA_REG_STAT_CMD
	ld c,ATA_CMD_READ_SEC
	call ata_write_byte
	call ata_busy_wait
	
	ld b,ATA_REG_STAT_CMD
	call ata_read_byte
	bit 0,b
	jp nz,halt
	
	ld e,32
	ld hl,0x8000
copy_loop_outer:
	call ata_busy_wait
	call ata_wait_for_data
	
	;Configure data port as inputs
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
	
	ld b,0
copy_loop_inner:
	;Read two words of data
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
	djnz copy_loop_inner
	dec e
	jp nz, copy_loop_outer
	;Idle
	ld a,ATA_DATA|PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	
	jp 0x0200

halt:
	ld hl,0xFFFF
	ld (hl),h
halt_loop:
	nop
	jp halt_loop

long_delay:
	push hl
	ld hl,0x4000
long_delay_loop:
long_delay_loop_inner:
	dec l
	jp nz, long_delay_loop_inner
	dec h
	jp nz, long_delay_loop
	pop hl
	ret
	
	; Addr in b, byte in c
ata_write_byte:
	ld a,b
	or PATA_DIOW|PATA_DIOR
	ld b,a
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	xor a
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,c
	out (ADDR_PIO0|PIO_DAT|PIO_A),a
	ld a,b
	xor PATA_DIOW
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld a,b
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	or PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ret

	; Addr in b, result in b
ata_read_byte:
	ld a,0b11001111
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,0xFF
	out (ADDR_PIO0|PIO_CMD|PIO_A),a
	ld a,PATA_DIOW|PATA_DIOR
	or b
	ld c,a
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	xor a,PATA_DIOR
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	in a,(ADDR_PIO0|PIO_DAT|PIO_A)
	ld b,a
	ld a,c
	or PATA_DIOR|PATA_DIOW|PATA_CS0|PATA_CS1
	out (ADDR_PIO1|PIO_DAT|PIO_A),a
	ret

ata_busy_wait:
	push bc
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
	pop bc
	ret

ata_wait_for_data:
	push bc
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
	pop bc
	ret
