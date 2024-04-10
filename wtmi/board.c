#include "types.h"
#include "efuse.h"
#include "mdio.h"
#include "spi.h"
#include "board.h"

static enum board _get_board_fallback(void)
{
	int reg02, reg12, reg32;
	int reg03, reg13, reg33;

	reg02 = mdio_read(0, 2);
	reg03 = mdio_read(0, 3);
	reg12 = mdio_read(1, 2);
	reg13 = mdio_read(1, 3);
	reg32 = mdio_read(3, 2);
	reg33 = mdio_read(3, 3);

	if (reg03 == 0xffff && reg13 == 0xffff) {
		return uDPU;
	} else if (reg02 == 0x0141 && reg13 == 0xffff) {
		return Armada3720_DB;
	} else if (reg03 == 0xffff && reg12 == 0x0141) {
		int id151x;
		u8 id[6];

		if (reg33 != 0xffff && reg32 == reg33)
			return ESPRESSObin_Ultra;

		mdio_write(1, 22, 18);
		id151x = mdio_read(1, 30);
		mdio_write(1, 22, 0);

		if (id151x == 0x0006)
			return Armada3720_DB;

		spi_init(&nordev);
		spi_nor_read_id(&nordev, id);

		if (id[0] == 0xef || id[0] == 0xc2 || id[0] == 0x01)
			return Turris_MOX;
		else
			return RIPE_Atlas;
	} else if (reg03 == 0xffff && reg13 != 0xffff && reg13 == reg12) {
		return ESPRESSObin;
	} else {
		return BOARD_Unknown;
	}
}

static enum board _get_board(void)
{
	enum board board;
	int lock;
	u64 val;

	/*
	 * First try determining board from eFuse - for boards from CZ.NIC
	 * eFuse row 42 contains MAC address and board version.
	 * If upper 3 bytes of MAC address are d8:58:d7, it is a CZ.NIC board,
	 * so either Turris MOX (bit 55 is not set) or Atlas RIPE (bit 55 is
	 * set).
	 */
	if (!efuse_read_row(42, &val, &lock) && lock) {
		if (((val >> 24) & 0xffffff) == 0xd858d7) {
			if ((val >> 55) & 1)
				return RIPE_Atlas;
			else
				return Turris_MOX;
		}
	}

	/*
	 * If eFuse is not burned, fallback to determination via SMI and SPI.
	 */
	mdio_begin();
	board = _get_board_fallback();
	mdio_end();

	return board;
}

enum board get_board(void)
{
	static enum board board;
	static int done;

	if (done)
		return board;

	board = _get_board();
	done = 1;

	return board;
}

const char *get_board_name(void)
{
	switch (get_board()) {
	case Turris_MOX:
		return "Turris MOX";
	case RIPE_Atlas:
		return "RIPE Atlas Probe";
	case ESPRESSObin:
		return "ESPRESSObin";
	case ESPRESSObin_Ultra:
		return "ESPRESSObin Ultra";
	case Armada3720_DB:
		return "Armada 3720 DB";
	case uDPU:
		return "Methode uDPU";
	default:
		return "unknown board";
	}
}
