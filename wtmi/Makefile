#/*
#* ***************************************************************************
#* Copyright (C) 2017 Marvell International Ltd.
#* ***************************************************************************
#*
#* Redistribution and use in source and binary forms, with or without
#* modification, are permitted provided that the following conditions are met:
#*
#* Redistributions of source code must retain the above copyright notice, this
#* list of conditions and the following disclaimer.
#*
#* Redistributions in binary form must reproduce the above copyright notice,
#* this list of conditions and the following disclaimer in the documentation
#* and/or other materials provided with the distribution.
#*
#* Neither the name of Marvell nor the names of its contributors may be used
#* to endorse or promote products derived from this software without specific
#* prior written permission.
#*
#* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
#* OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#* POSSIBILITY OF SUCH DAMAGE.
#*
#***************************************************************************
#*/

CROSS_CM3 ?= arm-none-eabi-

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
	LTO_FLAGS = -flto -flto-partition=none
else
	LTO_FLAGS =
endif

override WTMI_VERSION = $(shell git describe --always --dirty --tags)
ifndef WTMI_VERSION
$(error Repository is without git tags, please do a full git clone again)
endif

CFLAGS   = -g -gdwarf-2 -Wall -fno-common -ffreestanding -fno-stack-protector \
	   -ffunction-sections -fdata-sections -fno-stack-clash-protection \
	   -fno-pie $(LTO_FLAGS) $(INCLUDE) -Os $(CPUOPTS)
CPPFLAGS = -DWTMI_VERSION='"'$(WTMI_VERSION)'"'
ASFLAGS  = -g --gdwarf-2 --warn $(INCLUDE) $(CPUOPTS)
LDFLAGS  = -nostdlib -nostartfiles -T $(LDSCRIPT) $(CFLAGS) -no-pie \
	   -Xlinker "--build-id=none" -Xlinker "--gc-sections"

ifeq ($(DEPLOY), 1)
	CPPFLAGS += -DDEPLOY=1
endif

ifeq ($(WITHOUT_OTP_WRITE), 1)
	CPPFLAGS += -DWITHOUT_OTP_WRITE=1
endif

ifeq ($(WITHOUT_OTP_READ), 1)
	CPPFLAGS += -WITHOUT_OTP_READ=1
endif

ifeq ($(DEPLOY), 1)
	CSRC = $(wildcard *.c ddr/*.c)
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

a53_helper/a53_helper.c: a53_helper/a53_helper.ld a53_helper/main.c bin2c
	$(MAKE) -C a53_helper

reload_helper/reload_helper.c: reload_helper/reload_helper.ld reload_helper/reload.c bin2c
	$(MAKE) -C reload_helper

soc.c: a53_helper/a53_helper.c

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
