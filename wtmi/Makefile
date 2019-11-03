#/*
# * ***************************************************************************
# * Copyright (C) 2015 Marvell International Ltd.
# * ***************************************************************************
# * This program is free software: you can redistribute it and/or modify it
# * under the terms of the GNU General Public License as published by the Free
# * Software Foundation, either version 2 of the License, or any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program.  If not, see <http://www.gnu.org/licenses/>.
# * ***************************************************************************
#*/

CROSS_CM3 ?= armv7m-none-eabi-

LD       = $(CROSS_CM3)ld
CC       = $(CROSS_CM3)gcc
AS       = $(CROSS_CM3)as
OBJCOPY  = $(CROSS_CM3)objcopy
OBJDUMP  = $(CROSS_CM3)objdump

RM       = @rm -rf
ECHO     = @echo

CPUOPTS  = -mthumb -mcpu=cortex-m3 -mlittle-endian

LDSCRIPT = wtmi.ld
INCLUDE  = -I.

ifeq ($(LTO), 1)
	LTO_FLAGS = -flto -flto-partition=none -Wl,-fuse-ld=gold
else
	LTO_FLAGS =
endif

CFLAGS   = -g -gdwarf-2 -Wall -fno-common -ffreestanding -fno-stack-protector -fno-stack-clash-protection -fno-pie $(LTO_FLAGS) $(INCLUDE) -Os $(CPUOPTS)
CPPFLAGS =
ASFLAGS  = -g --gdwarf-2 --warn $(INCLUDE) $(CPUOPTS)
LDFLAGS  = -nostdlib -nostartfiles -T $(LDSCRIPT) $(CFLAGS) -no-pie -Xlinker "--build-id=none"

LIBGCC_A = $(shell $(CC) -print-libgcc-file-name)

ifeq ($(DEPLOY), 1)
	CPPFLAGS += -DDEPLOY=1
endif

ifeq ($(DEPLOY), 1)
	CSRC = $(filter-out ddr.c,$(wildcard *.c))
else
	CSRC = $(filter-out deploy.c,$(wildcard *.c ddr/*.c))
endif

ifeq ($(DEBUG_UART2), 1)
	CPPFLAGS += -DDEBUG_UART2=1
else
	CSRC := $(filter-out debug.c,$(CSRC))
endif

ASRC = $(wildcard *.S)

COBJ   = $(CSRC:.c=.o)
AOBJ   = $(ASRC:.S=.o)

.SILENT:

all: wtmi.bin

wtmi.bin: $(COBJ) $(AOBJ)
	$(ECHO) "  LD    $(MAKECMDGOALS)"
	$(CC) $(LDFLAGS) $(AOBJ) $(COBJ) $(LIBGCC_A) -o wtmi.elf
	$(OBJCOPY) -S -O binary wtmi.elf wtmi.bin
	@if [ `uname -m` = "x86_64" ]; then $(OBJDUMP) -D -S wtmi.elf > wtmi.dis; fi

%.o: %.c
	$(ECHO) "  CC    $<"
	$(CC) -MMD -MP -c $(CFLAGS) $(CPPFLAGS) -frandom-seed=$< -o $@ $(subst .o,.c,$@)

%.o: %.S
	$(ECHO) "  AS    $<"
	$(AS) $(ASFLAGS) -o $@ $(subst .o,.S,$@)

-include $(COBJ:.o=.d)

a53_helper/a53_helper.c: a53_helper/a53_helper.ld a53_helper/main.c
	$(MAKE) -C a53_helper

reload_helper/reload_helper.c: reload_helper/reload_helper.ld reload_helper/reload.c
	$(MAKE) -C reload_helper

soc.c: a53_helper/a53_helper.c

reload.c: reload_helper/reload_helper.c

clean:
	$(ECHO) "  CLEAN"
	@$(RM) -f $(COBJ) $(AOBJ) $(COBJ:.o=.d) deploy.d deploy.o debug.d debug.o wtmi.elf wtmi.dis wtmi.bin wtmi_h.bin
	@$(MAKE) -C a53_helper clean
	@$(MAKE) -C reload_helper clean

disasm:
	$(OBJDUMP) -m arm -M force-thumb -b binary --adjust-vma=0x1fff0000 -D wtmi.bin
