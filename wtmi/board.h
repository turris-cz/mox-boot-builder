#ifndef _BOARD_H_
#define _BOARD_H_

enum board {
	BOARD_Unknown = 0,
	Turris_MOX,
	RIPE_Atlas,
	ESPRESSObin,
	ESPRESSObin_Ultra,
	Armada3720_DB,
	uDPU,
};

extern enum board get_board(void);
extern const char *get_board_name(void);

#endif /* !_BOARD_H_ */
