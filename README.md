# Turris MOX boot-builder

Tools to prepare *U-Boot*, *ARM Trusted Firmware (ATF)* and *secure firmware* for
Turris MOX.


## Build

### Requirements

Dependencies:
- *openssl* or *libressl* for *mox-imager*

Cross-GCC needed:
- `armv7m-softfloat-eabi-` for the secure firmware runnning on internal
  Cortex-M3 secure processor (armv7m-softfloat-eabi-)
  - armv7a compiler will not work, because even though one could use the
    `-mthumb` option, compiler's internal `libgcc` was not compiled with this,
    and some functions from `libgcc` are needed
  - non-softfloat compiler will also not work, because even though one could
    use the `-msoft-float` option, again compiler's internal `libgcc` was not
    compiler with this
  - simplest solution to compile this compiler is using Gentoo's `crossdev`
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


### Other variables

Secure firmware compilation can be configured by these variables:

- `LTO=1` will compile secure firmware wit link time optimizations enabled. This
  will lead to smalled binary
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
