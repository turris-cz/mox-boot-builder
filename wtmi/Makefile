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

CFLAGS   = -g -gdwarf-2 -Wall -fno-common -ffreestanding -fno-stack-protector $(INCLUDE) -Os $(CPUOPTS)
CPPFLAGS =
ASFLAGS  = -g --gdwarf-2 --warn $(INCLUDE) $(CPUOPTS)
LDFLAGS  = -nostdlib -nostartfiles -T $(LDSCRIPT) -Xlinker "--build-id=none"

ifeq ($(DEPLOY), 1)
	CPPFLAGS += -DDEPLOY=1
endif

ifeq ($(DEPLOY), 1)
	CSRC = $(filter-out ddr.c,$(wildcard *.c))
else
	CSRC = $(filter-out deploy.c,$(wildcard *.c ddr/*.c))
endif
ASRC = $(wildcard *.S)

COBJ   = $(CSRC:.c=.o)
AOBJ   = $(ASRC:.S=.o)

.SILENT:

all: tools $(COBJ) $(AOBJ)
	$(ECHO) "  LD    $(MAKECMDGOALS)"
	$(CC) $(LDFLAGS)  $(AOBJ)  $(COBJ) -o wtmi.elf
	$(OBJCOPY) -S -O binary wtmi.elf wtmi.bin
	@if [ `uname -m` = "x86_64" ]; then $(OBJDUMP) -D -S wtmi.elf > wtmi.dis; fi

%.o: %.c
	$(ECHO) "  CC    $<"
	$(CC) -MMD -MP -c $(CFLAGS) $(CPPFLAGS) -o $@ $(subst .o,.c,$@)

%.o: %.S
	$(ECHO) "  AS    $<"
	$(AS) $(ASFLAGS) -o $@ $(subst .o,.S,$@)

-include $(COBJ:.o=.d)

clean:
	$(ECHO) "  CLEAN"
	@$(RM) -f $(COBJ) $(AOBJ) $(COBJ:.o=.d) wtmi.elf wtmi.dis wtmi.bin wtmi_h.bin

tools:
	@if [ -z "$(shell which $(CC))" ]; then $(ECHO) "$(CC) not found"; exit 1; fi;
