CROSS_CM3 ?= arm-none-eabi-

LD       = $(CROSS_CM3)ld
CC       = $(CROSS_CM3)gcc
AS       = $(CROSS_CM3)as
OBJCOPY  = $(CROSS_CM3)objcopy
OBJDUMP  = $(CROSS_CM3)objdump

RM       = @rm -rf
ECHO     = @echo

CPUOPTS  = -mthumb -mcpu=cortex-m3 -mlittle-endian

LDSCRIPT = reload_helper.ld

CFLAGS   = -g -gdwarf-2 -Wall -fno-common -ffreestanding -fno-stack-protector \
	   -fno-stack-clash-protection -fno-pie $(INCLUDE) -Os $(CPUOPTS)
CPPFLAGS =
ASFLAGS  = -g --gdwarf-2 --warn $(INCLUDE) $(CPUOPTS)
LDFLAGS  = -nostdlib -nostartfiles -T $(LDSCRIPT) -no-pie \
	   -Xlinker "--build-id=none"

CSRC   = reload.c
COBJ   = $(CSRC:.c=.o)

.SILENT:

all: reload_helper.c

reload_helper.c: reload_helper.bin
	$(ECHO) "  BIN2C    $@"
	$(ECHO) "static const unsigned char reload_helper_code[$(shell stat -c %s reload_helper.bin)] = {" >$@
	@../bin2c <$< >>$@
	$(ECHO) "};" >>$@

reload_helper.elf: $(COBJ) $(LDSCRIPT)
	$(ECHO) "  LD       $@"
	$(CC) $(LDFLAGS) $(CFLAGS) $(COBJ) -o $@

%.bin: %.elf
	$(ECHO) "  OBJCOPY  $@"
	$(OBJCOPY) -S -O binary $< $@
	$(OBJDUMP) -D -S $< > $(patsubst %.elf,%.dis,$<)

%.o: %.c
	$(ECHO) "  CC       $<"
	$(CC) -MMD -MP -c $(CFLAGS) $(CPPFLAGS) -frandom-seed=$< -o $@ $(subst .o,.c,$@)

clean:
	$(ECHO) "  CLEAN"
	@$(RM) -f $(COBJ) $(COBJ:.o=.d) reload_helper.elf reload_helper.bin reload_helper.dis reload_helper.c

disasm: reload_helper.bin
	$(OBJDUMP) -m arm -M force-thumb -b binary --adjust-vma=0x1fffff00 -D reload_helper.bin

.PHONY: all clean disasm
