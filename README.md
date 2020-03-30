# Turris MOX boot-builder

Tools to prepare *U-Boot* and *secure firmware* for Turris MOX.


## Build

### Requirements

Dependencies:
- *libressl*

GCC needed:
- *armv7m* for *Cortex-M3* firmware (`armv7m-softfloat-eabi-`)
    - Version 5.4.0 (strange bugs occur with newer GCC)
- *aarch64* for *ATF* and *U-Boot*


### Compilation

To compile, run:

```
git submodule init
git submodule update
make untrusted-flash-image.bin
```

The default `PATH` should contain newer aarch64 GCC.


### Outputs

Produced images:

- `untrusted-flash-image.bin`
    - Whole flash image (secure firmware + U-Boot)
    - Flash via U-Boot:
        ```
        sf probe
        sf update $addr 0 $size
        ```

- `untrusted-secure-firmware.bin`
    - Only secure firmware
    - Flash via Linux:
        ```
        mtd write untrusted-secure-firmware.bin secure-firmware
        ```
    - Flash via U-Boot:
        ```
        sf probe
        sf update $addr 0 $size
        ```

- `u-boot.bin`
    - Only U-Boot
    - Flash via Linux:
        ```
        mtd write u-boot.bin u-boot
        ```
    - Flash via U-Boot:
        ```
        sf probe
        sf update $addr 0x20000 $size
        ```
