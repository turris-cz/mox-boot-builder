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

CROSS_CM3 ?= armv7m-softfloat-eabi-

HOSTCC    ?= gcc
HOSTCFLAGS = -O2

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

override WTMI_VERSION = $(shell git describe --always --dirty --tags)
ifndef WTMI_VERSION
$(error Repository is without git tags, please do a full git clone again)
endif

CFLAGS   = -g -gdwarf-2 -Wall -fno-common -ffreestanding -fno-stack-protector -fno-stack-clash-protection -fno-pie $(LTO_FLAGS) $(INCLUDE) -Os $(CPUOPTS)
CPPFLAGS = -DWTMI_VERSION='"'$(WTMI_VERSION)'"'
ASFLAGS  = -g --gdwarf-2 --warn $(INCLUDE) $(CPUOPTS)
LDFLAGS  = -nostdlib -nostartfiles -T $(LDSCRIPT) $(CFLAGS) -no-pie -Xlinker "--build-id=none"

ifeq ($(DEPLOY), 1)
	CPPFLAGS += -DDEPLOY=1
endif

ifeq ($(DEPLOY), 1)
	CSRC = $(filter-out ddr.c,$(wildcard *.c))
else
	CSRC = $(filter-out deploy.c,$(wildcard *.c ddr/*.c))
endif

ifeq ($(shell echo "$(DEBUG_UART)" | tr 2 1), 1)
	CPPFLAGS += -DDEBUG_UART=$(DEBUG_UART)
else
	CSRC := $(filter-out debug.c,$(CSRC))
endif

ifeq ($(A53_HELPER), 1)
	CPPFLAGS += -DA53_HELPER=1
	a53_helper_src := a53_helper/a53_helper.c
else
	a53_helper_src :=
endif

CSRC := $(filter-out bin2c.c,$(CSRC))

ASRC = $(wildcard *.S)

COBJ = $(CSRC:.c=.o)
AOBJ = $(ASRC:.S=.o)

.SILENT:

all: wtmi.bin

bin2c: bin2c.c
	$(ECHO) "  HOSTCC   $<"
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

.SECONDARY: wtmi.elf wtmi_app.elf
.SECONDEXPANSION:
%.elf: $$(patsubst main.o,main$$(findstring _app,$$@).o,$$(COBJ)) $(AOBJ) $(LDSCRIPT)
	$(ECHO) "  LD       $@"
	@if ! $(CC) $(LDFLAGS) $(filter %.o,$^) -o $@; then	\
		echo "           FAILED, trying with -lgcc";	\
		echo "  LD -lgcc $@";				\
		$(CC) $(LDFLAGS) $(filter %.o,$^) -lgcc -o $@;	\
	fi

%.bin: %.elf
	$(ECHO) "  OBJCOPY  $@"
	$(OBJCOPY) -S -O binary $< $@
	$(OBJDUMP) -D -S $< > $(patsubst %.elf,%.dis,$<)

main_app.o: main.c
	$(ECHO) "  CC       $<"
	$(CC) -MMD -MP -c $(CFLAGS) $(CPPFLAGS) -DWTMI_APP -frandom-seed=$< -o $@ $<

%.o: %.c
	$(ECHO) "  CC       $<"
	$(CC) -MMD -MP -c $(CFLAGS) $(CPPFLAGS) -frandom-seed=$< -o $@ $(subst .o,.c,$@)

%.o: %.S
	$(ECHO) "  AS       $<"
	$(AS) $(ASFLAGS) -o $@ $(subst .o,.S,$@)

main.o main_app.o: $(filter-out main.o,$(COBJ)) $(AOBJ) $(LDSCRIPT)

-include $(COBJ:.o=.d)
-include main_app.d

$(a53_helper_src): a53_helper/a53_helper.ld a53_helper/main.c bin2c
	$(MAKE) -C a53_helper

reload_helper/reload_helper.c: reload_helper/reload_helper.ld reload_helper/reload.c bin2c
	$(MAKE) -C reload_helper

soc.c: $(a53_helper_src)

reload.c: reload_helper/reload_helper.c

clean:
	$(ECHO) "  CLEAN"
	@$(RM) -f $(COBJ) $(AOBJ) $(COBJ:.o=.d)				\
		main_app.d main_app.o deploy.d deploy.o debug.d debug.o	\
		wtmi.elf wtmi.dis wtmi.bin				\
		wtmi_app.elf wtmi_app.dis wtmi_app.bin			\
		bin2c
	@$(MAKE) -C a53_helper clean
	@$(MAKE) -C reload_helper clean

disasm: wtmi.bin
	$(OBJDUMP) -m arm -M force-thumb -b binary --adjust-vma=0x1fff0000 -D wtmi.bin

.PHONY: all clean disasm
