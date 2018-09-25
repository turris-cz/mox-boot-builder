ECDSA_PRIV_KEY	:= ecdsa_priv_key.txt
CROSS_CM3	:= armv7m-none-eabi-
CROSS_COMPILE	:= aarch64-unknown-linux-gnu-

BUILD_PLAT = build/a3700/release

all: flash-image.bin

flash-image.bin: secure-image.bin u-boot.bin
	cat secure-image.bin u-boot.bin >flash-image.bin

secure-image.bin: mox-imager/mox-imager wtmi_h.bin $(ECDSA_PRIV_KEY)
	mox-imager/mox-imager --create-secure-image -k $(ECDSA_PRIV_KEY) -o secure-image.bin wtmi_h.bin

wtmi_h.bin:
	make -C wtmi clean
	make -C wtmi
	echo -ne "IMTW" >wtmi_h.bin
	cat wtmi/wtmi.bin >>wtmi_h.bin

mox-imager/mox-imager:
	make -C wtmi clean
	make -C mox-imager

atf-marvell/$(BUILD_PLAT)/bl1.bin:
	make -C atf-marvell \
		PATH=$(ATF_GCC_PATH):$(PATH) \
		CROSS_CM3=$(CROSS_CM3) CROSS_COMPILE=$(CROSS_COMPILE) \
		DEBUG=0 LOG_LEVEL=0 USE_COHERENT_MEM=0 PLAT=a3700 \
		$(BUILD_PLAT)/bl1.bin

atf-marvell/$(BUILD_PLAT)/fip.bin: u-boot/u-boot.bin
	make -C atf-marvell \
		PATH=$(ATF_GCC_PATH):$(PATH) \
		CROSS_CM3=$(CROSS_CM3) CROSS_COMPILE=$(CROSS_COMPILE) \
		BL33=$(shell pwd)/u-boot/u-boot.bin DEBUG=0 LOG_LEVEL=0 USE_COHERENT_MEM=0 PLAT=a3700 \
		$(BUILD_PLAT)/fip.bin

u-boot.bin: atf-marvell/$(BUILD_PLAT)/bl1.bin atf-marvell/$(BUILD_PLAT)/fip.bin
	truncate -s %128K atf-marvell/$(BUILD_PLAT)/bl1.bin
	cat atf-marvell/$(BUILD_PLAT)/bl1.bin atf-marvell/$(BUILD_PLAT)/fip.bin >u-boot.bin
	truncate -s %4 u-boot.bin

u-boot/u-boot.bin:
#	make -C u-boot turris_mox_defconfig
	make -C u-boot CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	make -C wtmi clean
	make -C mox-imager clean
	rm -rf atf-marvell/build secure-image.bin wtmi_h.bin u-boot.bin flash-image.bin
