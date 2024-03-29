ECDSA_PRIV_KEY	:= ecdsa_priv_key.txt
CROSS_CM3	:= arm-none-eabi-
CROSS_COMPILE	:= aarch64-unknown-linux-gnu-
LTO 		:= 1

ifeq ($(COMPRESS_WTMI), 1)
	WTMI_PATH := wtmi/compressed
else
	WTMI_PATH := wtmi
endif

TRUSTED_IMAGES = trusted-flash-image.bin trusted-uart-image.bin trusted-emmc-image.bin
UNTRUSTED_IMAGES = untrusted-flash-image.bin untrusted-emmc-image.bin

all: trusted untrusted

trusted: $(TRUSTED_IMAGES)

untrusted: $(UNTRUSTED_IMAGES)

trusted-flash-image.bin: trusted-secure-firmware.bin a53-firmware.bin
	cat trusted-secure-firmware.bin a53-firmware.bin >$@

trusted-uart-image.bin: trusted-secure-firmware-uart.bin a53-firmware.bin
	cat trusted-secure-firmware-uart.bin a53-firmware.bin >$@

trusted-emmc-image.bin: trusted-secure-firmware-emmc.bin a53-firmware.bin
	cat trusted-secure-firmware-emmc.bin a53-firmware.bin >$@

untrusted-flash-image.bin: untrusted-secure-firmware.bin a53-firmware.bin
	cat untrusted-secure-firmware.bin a53-firmware.bin >$@

untrusted-emmc-image.bin: untrusted-secure-firmware-emmc.bin a53-firmware.bin
	cat untrusted-secure-firmware-emmc.bin a53-firmware.bin >$@

trusted-secure-firmware.bin: mox-imager/mox-imager wtmi_h.bin $(ECDSA_PRIV_KEY)
	mox-imager/mox-imager --create-trusted-image=SPI -k $(ECDSA_PRIV_KEY) -o $@ wtmi_h.bin

trusted-secure-firmware-uart.bin: mox-imager/mox-imager wtmi_h.bin $(ECDSA_PRIV_KEY)
	mox-imager/mox-imager --create-trusted-image=UART -k $(ECDSA_PRIV_KEY) -o $@ wtmi_h.bin

trusted-secure-firmware-emmc.bin: mox-imager/mox-imager wtmi_h.bin $(ECDSA_PRIV_KEY)
	mox-imager/mox-imager --create-trusted-image=EMMC -k $(ECDSA_PRIV_KEY) -o $@ wtmi_h.bin

untrusted-secure-firmware.bin: mox-imager/mox-imager wtmi_h.bin
	mox-imager/mox-imager --create-untrusted-image=SPI -o $@ wtmi_h.bin

untrusted-secure-firmware-emmc.bin: mox-imager/mox-imager wtmi_h.bin
	mox-imager/mox-imager --create-untrusted-image=EMMC -o $@ wtmi_h.bin

$(WTMI_PATH)/%.bin: FORCE
	$(MAKE) -C $(WTMI_PATH) CROSS_CM3=$(CROSS_CM3) LTO=$(LTO) $(notdir $@)

wtmi_h.bin: $(WTMI_PATH)/wtmi.bin
	printf "IMTW" >wtmi_h.bin
	cat $(WTMI_PATH)/wtmi.bin >>wtmi_h.bin

mox-imager/mox-imager: FORCE
	$(MAKE) -C mox-imager

trusted-firmware-a/build/a3700/release/boot-image.bin: u-boot/u-boot.bin FORCE
	$(MAKE) -C trusted-firmware-a \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		PLAT=a3700 CM3_SYSTEM_RESET=1 USE_COHERENT_MEM=0 FIP_ALIGN=0x100 \
		BL33=$(shell pwd)/u-boot/u-boot.bin \
		mrvl_bootimage

a53-firmware.bin: trusted-firmware-a/build/a3700/release/boot-image.bin
	cp -a $< $@
	od -v -tu8 -An -j 131184 -N 8 $@ | LC_ALL=C awk '{ for (i = 0; i < 64; i += 8) printf "%c", and(rshift(1441792-131072-$$1, i), 255) }' | dd of=$@ bs=1 seek=131192 count=8 conv=notrunc 2>/dev/null

u-boot/u-boot.bin: FORCE
	$(MAKE) -C u-boot CROSS_COMPILE=$(CROSS_COMPILE) turris_mox_defconfig
	$(MAKE) -C u-boot CROSS_COMPILE=$(CROSS_COMPILE) u-boot.bin

wtmi_app.bin: $(WTMI_PATH)/wtmi_app.bin
	cp -a $< $@

clean:
	-$(MAKE) -C u-boot clean
	-$(MAKE) -C trusted-firmware-a realclean
	-$(MAKE) -C wtmi clean
	-$(MAKE) -C wtmi/compressed clean
	-$(MAKE) -C mox-imager clean
	rm -f wtmi_h.bin wtmi_app.bin a53-firmware.bin \
		$(UNTRUSTED_IMAGES) $(TRUSTED_IMAGES) \
		trusted-secure-firmware.bin trusted-secure-firmware-uart.bin trusted-secure-firmware-emmc.bin \
		untrusted-secure-firmware.bin untrusted-secure-firmware-emmc.bin

.PHONY: all trusted untrusted clean FORCE
FORCE:;
