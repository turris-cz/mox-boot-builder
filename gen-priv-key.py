#!/usr/bin/python3

from os import umask
from sys import stderr

mask = umask(0o77)
if ~mask & 0o77 != 0:
	print('usage: umask 077\n       gen-priv-key.py >priv-key.txt', file=stderr)
	exit(1)

def getrandbits(n):
	b = (n + 7) // 8
	r = int.from_bytes(open('/dev/random', 'rb').read(b), 'big')
	r >>= b * 8 - n
	return r

order = 0x1fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffa51868783bf2f966b7fcc0148f709a5d03bb5c9b8899c47aebb6fb71e91386409

while True:
	priv = getrandbits(521)
	if priv < order:
		break

print(priv)
