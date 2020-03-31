ECDSA_PRIV_KEY	:= ecdsa_priv_key.txt
CROSS_CM3	:= armv7m-softfloat-eabi-
CROSS_COMPILE	:= aarch64-unknown-linux-gnu-

ATF_PLAT = a3700
ATF_LOG_LEVEL = 0
BUILD_PLAT = build/$(ATF_PLAT)/release

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

trusted-flash-image.bin: trusted-secure-firmware.bin u-boot.bin
	cat trusted-secure-firmware.bin u-boot.bin >$@

trusted-uart-image.bin: trusted-secure-firmware-uart.bin u-boot.bin
	cat trusted-secure-firmware-uart.bin u-boot.bin >$@

trusted-emmc-image.bin: trusted-secure-firmware-emmc.bin u-boot.bin
	cat trusted-secure-firmware-emmc.bin u-boot.bin >$@

untrusted-flash-image.bin: untrusted-secure-firmware.bin u-boot.bin
	cat untrusted-secure-firmware.bin u-boot.bin >$@

untrusted-emmc-image.bin: untrusted-secure-firmware-emmc.bin u-boot.bin
	cat untrusted-secure-firmware-emmc.bin u-boot.bin >$@

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

wtmi_h.bin:
	echo $(WTMI_PATH)
	make -C $(WTMI_PATH) clean
	make -C $(WTMI_PATH) CROSS_CM3=$(CROSS_CM3)
	echo -ne "IMTW" >wtmi_h.bin
	cat $(WTMI_PATH)/wtmi.bin >>wtmi_h.bin

mox-imager/mox-imager:
	make -C wtmi/compressed clean
	make -C mox-imager

arm-trusted-firmware/$(BUILD_PLAT)/bl1.bin:
	make -C arm-trusted-firmware \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		DEBUG=0 LOG_LEVEL=$(ATF_LOG_LEVEL) \
		USE_COHERENT_MEM=0 PLAT=$(ATF_PLAT) \
		$(BUILD_PLAT)/bl1.bin

arm-trusted-firmware/$(BUILD_PLAT)/fip.bin: u-boot/u-boot.bin
	make -C arm-trusted-firmware \
		CROSS_COMPILE=$(CROSS_COMPILE) \
		BL33=$(shell pwd)/u-boot/u-boot.bin \
		DEBUG=0 LOG_LEVEL=$(ATF_LOG_LEVEL) \
		USE_COHERENT_MEM=0 PLAT=$(ATF_PLAT) \
		$(BUILD_PLAT)/fip.bin

u-boot.bin: arm-trusted-firmware/$(BUILD_PLAT)/bl1.bin arm-trusted-firmware/$(BUILD_PLAT)/fip.bin
	truncate -s %128K arm-trusted-firmware/$(BUILD_PLAT)/bl1.bin
	cat arm-trusted-firmware/$(BUILD_PLAT)/bl1.bin arm-trusted-firmware/$(BUILD_PLAT)/fip.bin >$@
	truncate -s %4 $@

u-boot/u-boot.bin:
	make -C u-boot turris_mox_defconfig
	make -C u-boot CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	make -C u-boot clean
	make -C wtmi/compressed clean
	make -C mox-imager clean
	rm -rf arm-trusted-firmware/build wtmi_h.bin u-boot.bin \
		$(UNTRUSTED_IMAGES) $(TRUSTED_IMAGES) \
		trusted-secure-firmware.bin trusted-secure-firmware-uart.bin trusted-secure-firmware-emmc.bin \
		untrusted-secure-firmware.bin untrusted-secure-firmware-emmc.bin
