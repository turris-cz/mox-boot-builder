#include "types.h"
#include "irq.h"
#include "reload.h"
#include "zlib_defs.h"

int zunzip(void *dst, u32 dstlen, u8 *src, u32 *lenp, int stoponerr, int offset)
{
	z_stream s;
	int err = 0;
	int r;

	r = inflateInit2(&s, -MAX_WBITS);
	if (r != Z_OK) {
		return -1;
	}
	s.next_in = src + offset;
	s.avail_in = *lenp - offset;
	s.next_out = dst;
	s.avail_out = dstlen;
	do {
		r = inflate(&s, Z_FINISH);
		if (stoponerr == 1 && r != Z_STREAM_END &&
		    (s.avail_in == 0 || s.avail_out == 0 || r != Z_BUF_ERROR)) {
			err = -1;
			break;
		}
	} while (r == Z_BUF_ERROR);
	*lenp = s.next_out - (unsigned char *) dst;
	inflateEnd(&s);

	return err;
}

static void die(void)
{
	while (1)
		wait_for_irq();
}

void main (void)
{
	extern u8 compressed_start, compressed_end;
	void *src;
	void *dst;
	u32 addr, len;

	src = &compressed_start;
	len = &compressed_end - &compressed_start;
	addr = 0x20000000;
	dst = (void *)addr;

	if (zunzip(dst, 65536, src, &len, 1, 0))
		die();

	if (len % 4)
		len += 4 - (len % 4);

	do_reload(addr, len);
	die();
}
