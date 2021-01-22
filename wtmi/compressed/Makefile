CROSS_CM3 ?= armv7m-softfloat-eabi-

LD       = $(CROSS_CM3)ld
CC       = $(CROSS_CM3)gcc
AS       = $(CROSS_CM3)as
OBJCOPY  = $(CROSS_CM3)objcopy
OBJDUMP  = $(CROSS_CM3)objdump

RM       = @rm -rf
ECHO     = @echo

CPUOPTS  = -mthumb -mcpu=cortex-m3 -mlittle-endian

LDSCRIPT = wtmi.ld
INCLUDE  = -I. -I..

CFLAGS   = -g -gdwarf-2 -Wall -fno-common -ffreestanding -fno-stack-protector -fno-stack-clash-protection -fno-pie $(INCLUDE) -Os $(CPUOPTS)
CPPFLAGS =
ASFLAGS  = -g --gdwarf-2 --warn $(INCLUDE) $(CPUOPTS)
LDFLAGS  = -nostdlib -nostartfiles -T $(LDSCRIPT) -no-pie -Xlinker "--build-id=none"

LIBGCC_A = $(shell $(CC) -print-libgcc-file-name)

CSRC = main.c malloc.c zlib.c string.c startup.c reload.c main_wtmi.c
ASRC = startup_asm.S

COBJ   = $(CSRC:.c=.o)
AOBJ   = $(ASRC:.S=.o)

.SILENT:

all: wtmi.bin

../bin2c: FORCE
	$(MAKE) -C .. bin2c

wtmi.bin: $(COBJ) $(AOBJ)
	$(ECHO) "  LD    $@"
	$(CC) $(LDFLAGS) $(AOBJ) $(COBJ) $(LIBGCC_A) -o wtmi.elf
	$(OBJCOPY) -S -O binary wtmi.elf wtmi.bin
	@if [ `uname -m` = "x86_64" ]; then $(OBJDUMP) -D -S wtmi.elf > wtmi.dis; fi

main_wtmi.c: main_wtmi.zlib ../bin2c
	$(ECHO) "  BIN2C main_wtmi.c"
	$(ECHO) "const unsigned char main_wtmi_zlib[$(shell stat -c %s main_wtmi.zlib)] __attribute__((section(\".compressed_data\"), used)) = {" >main_wtmi.c
	@../bin2c <main_wtmi.zlib >>main_wtmi.c
	$(ECHO) "};" >>main_wtmi.c

main_wtmi.zlib: ../wtmi.bin
	gzip -cnkf9 ../wtmi.bin | dd bs=10 skip=1 >main_wtmi.zlib

../wtmi.bin: FORCE
	$(MAKE) -C ../ wtmi.bin

%.o: %.c
	$(ECHO) "  CC    $<"
	$(CC) -MMD -MP -c $(CFLAGS) $(CPPFLAGS) -frandom-seed=$< -o $@ $(subst .o,.c,$@)

%.o: %.S
	$(ECHO) "  AS    $<"
	$(AS) $(ASFLAGS) -o $@ $(subst .o,.S,$@)

-include $(COBJ:.o=.d)

../reload_helper/reload_helper.c: ../reload_helper/reload_helper.ld ../reload_helper/reload.c ../bin2c
	$(MAKE) -C ../reload_helper

reload.c: ../reload_helper/reload_helper.c

clean:
	$(ECHO) "  CLEAN"
	@$(RM) -f $(COBJ) $(AOBJ) $(COBJ:.o=.d) wtmi.elf wtmi.dis wtmi.bin main_wtmi.zlib main_wtmi.c
	$(MAKE) -C ../ clean

disasm: wtmi.bin
	$(OBJDUMP) -m arm -M force-thumb -b binary --adjust-vma=0x1fff0000 -D wtmi.bin

.PHONY: all clean disasm FORCE
FORCE:;
