#!/usr/bin/python3

from struct import unpack

__all__ = ['get_flash_image_parts']

class tim(object):
	@staticmethod
	def id2name(i):
		return chr((i >> 24) & 0xff) + chr((i >> 16) & 0xff) + chr((i >> 8) & 0xff) + chr(i & 0xff)

	def __init__(self, data):
		self.version, self.identifier, self.trusted = unpack('<LLL', data[0:12])
		self.numimages, = unpack('<L', data[44:48])

		if self.version < 0x30600 or (self.identifier != 0x54494d48 and self.identifier != 0x54494d4e) or self.numimages < 1:
			raise

	def get_images(self, data, base = 0):
		result = []

		addr = 56 + base

		for i in range(self.numimages):
			id, nextid, flashentry, loadaddr, size = unpack('<LLLLL', data[addr:addr + 20])
			result.append((self.id2name(id), flashentry, data[flashentry:flashentry + size]))
			addr += 108

		return result

def get_flash_image_parts(data):
	timh = tim(data)

	result = timh.get_images(data)

	if timh.trusted:
		timn = tim(data[0x1000:])
		result += timn.get_images(data, 0x1000)

	return result


if __name__ == '__main__':
	data = open('trusted-flash-image.bin', 'rb').read()

	for name, entry, data in get_flash_image_parts(data):
		print('%s %x %x' % (name, entry, len(data)))
