# CZ.NIC's Armada 3720 Secure Firmware

## CZ.NIC secure processor eFuse content

CZ.NIC's Armada 3720 based devices use rows 42 and 43 of secure processor OTP
to store device info (serial number, MAC addresses, ...).

```
row 42
  63:58	reserved
  57:56	00 512 MiB RAM
	01 1024 MiB RAM
	10 2048 MiB RAM
	11 4096 MiB RAM
  55:54	00 - Turris MOX
	01 - reserved
	10 - RIPE Atlas Probe
	11 - reserved
  53:48	board version
  47:00	first MAC address - bits 47:24 are D8:58:D7 (CZ.NIC's prefix)

row 43
  63:00	device serial number
```
