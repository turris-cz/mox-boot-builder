CROSS_COMPILE ?= aarch64-unknown-linux-gnu-

LD       = $(CROSS_COMPILE)ld
CC       = $(CROSS_COMPILE)gcc
AS       = $(CROSS_COMPILE)as
STRIP    = $(CROSS_COMPILE)strip
OBJCOPY  = $(CROSS_COMPILE)objcopy
OBJDUMP  = $(CROSS_COMPILE)objdump

RM       = @rm -rf
ECHO     = @echo

CPUOPTS  = -mcpu=cortex-a53 -mlittle-endian

LDSCRIPT = a53_helper.ld

CFLAGS   = -Wall -fno-common -ffreestanding -fno-stack-protector -O2 $(CPUOPTS)
CPPFLAGS =
LDFLAGS  = -nostdlib -nostartfiles -T $(LDSCRIPT) -Xlinker "--build-id=none"

CSRC   = main.c
COBJ   = $(CSRC:.c=.o)

ASRC   = start.S
AOBJ   = $(ASRC:.S=.o)

.SILENT:

all: a53_helper.c

a53_helper.c: a53_helper.bin
	$(ECHO) "  BIN2C a53_helper.c"
	$(ECHO) "static const unsigned char a53_helper_code[$(shell stat -c %s a53_helper.bin)] = {" >a53_helper.c
	@../bin2c <a53_helper.bin >>a53_helper.c
	$(ECHO) "};" >>a53_helper.c

a53_helper.bin: $(COBJ) $(AOBJ) $(LDSCRIPT)
	$(ECHO) "  LD    $@"
	$(CC) $(LDFLAGS) $(CFLAGS) $(AOBJ) $(COBJ) -o a53_helper.elf
	$(OBJCOPY) -S -O binary a53_helper.elf a53_helper.bin
	$(OBJDUMP) -D -S a53_helper.elf > a53_helper.dis

%.o: %.c
	$(ECHO) "  CC    $<"
	$(CC) -MMD -MP -c $(CFLAGS) $(CPPFLAGS) -frandom-seed=$< -o $@ $(subst .o,.c,$@)

%.o: %.S
	$(ECHO) "  AS    $<"
	$(AS) $(ASFLAGS) -o $@ $(subst .o,.S,$@)

clean:
	$(ECHO) "  CLEAN"
	@$(RM) -f $(COBJ) $(COBJ:.o=.d) $(AOBJ) $(AOBJ:.o=.d) a53_helper.elf a53_helper.bin a53_helper.dis a53_helper.c

disasm: a53_helper.bin
	$(OBJDUMP) -m aarch64 -b binary --adjust-vma=0x04100000 -D a53_helper.bin

.PHONY: all clean disasm
