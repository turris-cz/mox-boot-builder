# Turris MOX boot-builder

Tools to prepare *U-Boot*, *ARM Trusted Firmware (ATF)* and *secure firmware* for
Turris MOX.


## Build

### Requirements

Dependencies:
- *openssl* or *libressl* for *mox-imager*

Cross-GCC needed:
- `arm-linux-gnueabi-` or `armv7m-softfloat-eabi-` for the secure firmware
  runnning on internal Cortex-M3 secure processor
  - `armv7m-softfloat-eabi-` can be built by [Gentoo's crossdev tool](https://wiki.gentoo.org/wiki/Crossdev)
    and should always work, since it's internal `libgcc` is compiled with
    `-mthumb` option
- `aarch64` for ATF and U-Boot

For trusted image you will also need ECDSA private key with which the image will
be signed.


### Before compilation

Before compilation you have to update git submodules:

```
git submodule init
git submodule update
```

and compile `mox-imager`:

```
make mox-imager/mox-imager
make -C wtmi clean
```

### Compiling only a53-firmware (TF-A + U-Boot) (without secure-firmware)

Run

```
make a53-firmware.bin
```

to compile TF-A and U-Boot which will be combined into one image, `a53-firmware.bin`.
This can then be flashed onto the SPI-NOR memory (into partition `a53-firmware`, which
is at offset `0x20000`).


### Compiling untrusted image

*NOTE*: untrusted image can only be used on prototypes for testing purposes.

Running

```
make untrusted
```

will build untrusted secure firmware together with ATF and U-Boot and combine
them into one image (`untrusted-flash-image.bin`), which can then be flashed
into the SPI-NOR memory at offset `0x0`. This image overlaps 2 mtd partitions:
`secure-firmware` and `a53-firmware`.


### Compiling trusted image

Deployed devices will only boot image signed with a specific ECDSA private key.
Use the `ECDSA_PRIV_KEY` variable to specify path to the private key:

Running

```
make trusted ECDSA_PRIV_KEY=/path/to/ecdsa_priv_key.txt
```

will create the same as untrusted image in section above, but signed with this
key. Thus it is called `trusted-flash-image.bin`. This will also create
`trusted-secure-firmware.bin`, which contains only the part for
`secure-firmware` SPI-NOR partition.

*NOTE*: U-Boot/a53-firmware is not part of the image checked by this signature,
thus users can compile U-Boot/a53-firmware on their own. The only part of the
image that users cannot build themselves is the secure firmware.


### Using on ESPRESSObin / other Armada 3720 devices

Our firmware can also be used on other Armada 3720 devices, bringing support
for hardware random number generator which can only be accessed from the secure
processor.

Build the `wtmi_app.bin` binary with

```
make wtmi_app.bin
```

and then use the `WTMI_IMG` variable when building flash image for the target
device (such as ESPRESSObin) from `trusted-firmware-a` sources.

Example for ESPRESSObin (with 1GHz CPU and DDR3 2CS 1GB RAM):

```
git clone https://review.trustedfirmware.org/TF-A/trusted-firmware-a
git clone https://gitlab.denx.de/u-boot/u-boot.git
git clone https://github.com/weidai11/cryptopp.git
git clone https://github.com/MarvellEmbeddedProcessors/mv-ddr-marvell.git -b master
git clone https://github.com/MarvellEmbeddedProcessors/A3700-utils-marvell.git -b master
git clone https://gitlab.nic.cz/turris/mox-boot-builder.git

make -C u-boot CROSS_COMPILE=aarch64-linux-gnu- mvebu_espressobin-88f3720_defconfig u-boot.bin
make -C mox-boot-builder CROSS_CM3=arm-linux-gnueabi- wtmi_app.bin
make -C trusted-firmware-a				\
	CROSS_COMPILE=aarch64-linux-gnu-		\
	CROSS_CM3=arm-linux-gnueabi-			\
	USE_COHERENT_MEM=0				\
	PLAT=a3700					\
	CLOCKSPRESET=CPU_1000_DDR_800			\
	DDR_TOPOLOGY=2					\
	MV_DDR_PATH=$PWD/mv-ddr-marvell/		\
	WTP=$PWD/A3700-utils-marvell/			\
	CRYPTOPP_PATH=$PWD/cryptopp/			\
	BL33=$PWD/u-boot/u-boot.bin			\
	WTMI_IMG=$PWD/mox-boot-builder/wtmi_app.bin	\
	FIP_ALIGN=0x100					\
	mrvl_flash
```

The resulting `flash-image.bin` will be stored in
`trusted-firmware-a/build/a3700/release/` directory.

For other boards or different CPU frequencies or different RAM types please
change U-Boot's `defconfig` file and the `CLOCKSPRESET` and `DDR_TOPOLOGY`
variables according to
[this `trusted-firmware-a` documentation](https://trustedfirmware-a.readthedocs.io/en/latest/plat/marvell/armada/build.html).

### Other variables

Secure firmware compilation can be configured by these variables:

- `LTO=1` will compile secure firmware with link time optimizations enabled. This
  will lead to smaller binary. This is now default. Use `LTO=0` to disable
- `DEBUG_UART=1` or `DEBUG_UART=2` will start a debug console on UART1/UART2.
  This is useful for debugging secure firmware
- `COMPRESS_WTMI=1` will compress secure-firmware and add code to decompress it
  before running. Useful when `DEBUG_UART` is used


### Outputs

Produced images:

- `(un)trusted-flash-image.bin`
    - whole flash image (secure firmware + TF-A + U-Boot)
    - Flash via U-Boot:
        ```
        sf probe
        sf update $addr 0 $size
        ```

- `(un)trusted-secure-firmware.bin`
    - Only secure firmware
    - Flash via Linux:
        ```
        mtd write (un)trusted-secure-firmware.bin secure-firmware
        ```
    - Flash via U-Boot:
        ```
        sf probe
        sf update $addr 0 $size
        ```

- `a53-firmware.bin`
    - Only U-Boot (with ATF)
    - Flash via Linux:
        ```
        mtd write a53-firmware.bin a53-firmware
        ```
    - Flash via U-Boot:
        ```
        sf probe
        sf update $addr 0x20000 $size
        ```
