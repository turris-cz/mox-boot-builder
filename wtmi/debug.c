#include "types.h"
#include "uart.h"
#include "io.h"
#include "string.h"
#include "debug.h"

#define BETWEEN(x,l,h) ((l) <= (x) && (x) <= (h))

extern const u8 debug_cmds_start, debug_cmds_end;

static u8 cmd[128];
static int cmdlen, cmdpos;

static u8 escape_seq[32];
static int escape_seq_len, escape_state;

static char history[2048];
static int history_len, history_cmds, history_pos;
static u8 orig_cmd[128];
static int orig_cmdlen;

static inline void prompt(void)
{
	printf("debug cmd> ");
}

static void cmd_append(u8 c)
{
	int i;

	if (cmdlen >= sizeof(cmd)-1)
		return;

	for (i = cmdlen; i > cmdpos; --i)
		cmd[i] = cmd[i-1];
	cmd[cmdpos] = c;
	++cmdpos;
	++cmdlen;

	putchar(cmd[cmdpos - 1]);
	printf("\033[s");
	for (i = cmdpos; i < cmdlen; ++i)
		putchar(cmd[i]);
	printf("\033[u");
}

static void cmd_del(void)
{
	int i;

	if (cmdpos == cmdlen)
		return;

	printf("\033[s");
	for (i = cmdpos; i < cmdlen-1; ++i) {
		cmd[i] = cmd[i+1];
		putchar(cmd[i]);
	}
	printf("\033[K\033[u");

	--cmdlen;
}

static void escape_seq_append(u8 c)
{
	if (escape_seq_len >= sizeof(escape_seq)-1)
		return;

	escape_seq[escape_seq_len++] = c;
}

static void escape_seq_error()
{
	printf("[invalid escape seq]");
	escape_state = escape_seq_len = 0;
}

static void history_add(void)
{
	int free_space;

	if (!cmdlen)
		return;

	free_space = sizeof(history) - history_len;
	while (free_space < cmdlen + 1) {
		int len = strnlen(history, sizeof(history)) + 1;

		history_len -= len;
		memmove(history, history + len, history_len);

		--history_cmds;
		free_space = sizeof(history) - history_len;
	}

	memcpy(history + history_len, cmd, cmdlen + 1);
	history_len += cmdlen + 1;
	++history_cmds;
}

static u8 *history_get(int idx, int *lenp)
{
	int pos, i, len;

	idx = history_cmds - idx - 1;
	for (i = 0, pos = 0; pos < history_len; pos += len + 1, ++i) {
		len = strnlen(&history[pos], history_len - pos);
		if (idx == i) {
			if (lenp)
				*lenp = len;
			return (u8 *)&history[pos];
		}
	}

	return NULL;
}

static void cmd_set(u8 *newcmd, int newpos)
{
	if (newcmd != cmd) {
		cmdlen = strlen((char *)newcmd);
		memcpy(cmd, newcmd, cmdlen);
	}

	cmdpos = MAX(MIN(newpos, cmdlen), 0);
	cmd[cmdlen] = '\0';

	printf("\r\033[2K");
	prompt();
	printf("%s", (char *)cmd);
	if (cmdlen - cmdpos > 0)
		printf("\033[%dD", cmdlen - cmdpos);
}

static void history_prev(void)
{
	u8 *hcmd;
	int hcmdlen;

	if (!history_pos) {
		memcpy(orig_cmd, cmd, cmdlen);
		orig_cmdlen = cmdlen;
	}

	hcmd = history_get(history_pos, &hcmdlen);
	if (!hcmd)
		return;
	++history_pos;

	cmd_set(hcmd, hcmdlen);
}

static void history_next(void)
{
	u8 *hcmd;
	int hcmdlen;

	if (!history_pos)
		return;

	--history_pos;
	if (!history_pos) {
		hcmd = orig_cmd;
		hcmdlen = orig_cmdlen;
	} else {
		hcmd = history_get(history_pos - 1, &hcmdlen);
	}

	cmd_set(hcmd, hcmdlen);
}

static void escape_seq_process()
{
	if (escape_seq_len >= sizeof(escape_seq)-1)
		goto end;

	escape_seq[escape_seq_len] = 0;
	if (escape_seq[0] == 'D') {
		/* cursor left */
		if (cmdpos > 0) {
			--cmdpos;
			printf("\033[D");
		}
	} else if (escape_seq[0] == 'C') {
		/* cursor right */
		if (cmdpos < cmdlen) {
			++cmdpos;
			printf("\033[C");
		}
	} else if (escape_seq[0] == 'A') {
		/* cursor up */
		history_prev();
	} else if (escape_seq[0] == 'B') {
		/* cursor down */
		history_next();
	} else if (escape_seq[0] == '3' && escape_seq[1] == '~') {
		/* delete key */
		cmd_del();
	} else {
		/* printf("[escape seq %s]\n", escape_seq); */
	}

end:
	escape_state = escape_seq_len = 0;
}

static char *skip_spaces(char *p)
{
	while (*p == ' ')
		++p;

	return p;
}

static int parse_args(char *work, int *argc, char **argv)
{
	char *p = work;

	*argc = 0;
	p = skip_spaces(p);
	while (*p) {
		argv[*argc] = p;
		++*argc;
		while (*p && *p != ' ')
			++p;
		if (!*p)
			break;
		*p = '\0';
		if (*argc == 32) {
			printf("too many arguments");
			return -1;
		}
		p = skip_spaces(p + 1);
	}

	return 0;
}

static void cmd_exec(void)
{
	static const struct debug_cmd *s = (const void *)&debug_cmds_start;
	static const struct debug_cmd *e = (const void *)&debug_cmds_end;
	const struct debug_cmd *i;
	char parsed[sizeof(cmd)];
	char *argv[32];
	int argc;

	history_add();

	memcpy(parsed, cmd, cmdlen + 1);
	if (parse_args(parsed, &argc, argv))
		goto end;
	if (!argc)
		goto end;

	for (i = s; i < e; ++i) {
		if (!strcmp(i->name, argv[0])) {
			i->cb(argc, argv);
			break;
		}
	}

	if (i == e)
		printf("unknown command %s\n", argv[0]);
end:
	cmdlen = cmdpos = history_pos = 0;
	prompt();
}

static void consume_normal(u8 c)
{
	switch (c) {
	case 1:
		/* CTRL + A */
		cmd_set(cmd, 0);
		break;
	case 3:
		/* CTRL + C */
		cmdlen = cmdpos = history_pos = 0;
		printf("^C\n");
		prompt();
		break;
	case 5:
		/* CTRL + E */
		cmd_set(cmd, cmdlen);
		break;
	case '\b':
		if (cmdpos) {
			printf("\033[D");
			--cmdpos;
			cmd_del();
		}
		break;
	case '\r':
	case '\n':
		putchar('\n');
		cmd[cmdlen] = '\0';
		cmd_exec();
		cmdlen = cmdpos = 0;
		break;
	case 27:
		escape_state = 1;
		break;
	case 32 ... 126:
		cmd_append(c);
		break;
	default:
		printf("[%d]", c);
	}

}

void debug_init(void)
{
	printf("\nCZ.NIC Turris Mox Secure Firmware debug command line\n");
	prompt();
}

void debug_process(void)
{
	int c;

	while ((c = getc()) >= 0) {
		if (escape_state == 1 && BETWEEN(c, 0x40, 0x5f)) {
			if (c == '[') {
				escape_state = 2;
			} else {
				printf("[escape %c]", c);
				escape_state = 0;
			}
		} else if (escape_state == 2) {
			if (BETWEEN(c, 0x20, 0x7e)) {
				escape_seq_append(c);
			} else {
				escape_seq_error();
				continue;
			}
			if (BETWEEN(c, 0x40, 0x7e))
				escape_seq_process();
			else if (BETWEEN(c, 0x20, 0x2f))
				escape_state = 3;
		} else if (escape_state == 3) {
			if (BETWEEN(c, 0x40, 0x7e)) {
				escape_seq_append(c);
				escape_seq_process();
			} else if (BETWEEN(c, 0x20, 0x2f)) {
				escape_seq_append(c);
			} else {
				escape_seq_error();
				continue;
			}
		} else {
			consume_normal(c);
		}
	}
}

DECL_DEBUG_CMD(help)
{
	static const struct debug_cmd *s = (const void *)&debug_cmds_start;
	static const struct debug_cmd *e = (const void *)&debug_cmds_end;
	const struct debug_cmd *i;

	for (i = s; i < e; ++i)
		printf("%s - %s\n", i->name, i->help);
	printf("\n");
}

DEBUG_CMD("help", "Display this help", help);

int _number(const char *str, u32 *pres, int base)
{
	const char *p = str;
	u32 res = 0;

	if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
		p += 2;

	while (*p) {
		int cur;

		if (*p)
			res *= base;

		switch (*p) {
		case '0' ... '9':
			cur = *p - '0';
			break;
		case 'a' ... 'f':
			cur = *p - 'a' + 10;
			break;
		case 'A' ... 'F':
			cur = *p - 'A' + 10;
			break;
		default:
			cur = -1;
		}

		if (cur < 0 || cur >= base) {
			printf("invalid number %s\n", str);
			return -1;
		}

		res += cur;
		++p;
	}

	if (pres)
		*pres = res;
	return 0;
}

DECL_DEBUG_CMD(md)
{
	u32 addr, cnt, i;
	int sz = 4;

	if (argv[0][2] == '.') {
		if (argv[0][3] == 'w')
			sz = 2;
		else if (argv[0][3] == 'b')
			sz = 1;
	}

	if (argc < 2 || argc > 3) {
		printf("usage: md[.l, .w, .b] address [count]\n");
		return;
	}

	if (number(argv[1], &addr))
		return;

	cnt = 1;
	if (argc == 3 && number(argv[2], &cnt))
		return;

	for (i = 0; i < cnt; ++i) {
		u32 val;

		if ((i % (16 / sz)) == 0)
			printf("%08x: ", addr);

		switch (sz) {
		case 1:
			val = readb(addr);
			printf(" %02x", val);
			break;
		case 2:
			val = readw(addr);
			printf(" %04x", val);
			break;
		case 4:
			val = readl(addr);
			printf(" %08x", val);
			break;
		}

		if (((i+1) % (16 / sz)) == 0)
			printf("\n");
	
		addr += sz;
	}

	if ((i % (16 / sz)) != 0)
		printf("\n");
}

DEBUG_CMD("md", "Memory display (longs)", md);
DEBUG_CMD("md.l", "Memory display (longs)", md);
DEBUG_CMD("md.w", "Memory display (half-words)", md);
DEBUG_CMD("md.b", "Memory display (bytes)", md);

DECL_DEBUG_CMD(mw)
{
	u32 addr, val, cnt, i;
	int sz = 4;

	if (argv[0][2] == '.') {
		if (argv[0][3] == 'w')
			sz = 2;
		else if (argv[0][3] == 'b')
			sz = 1;
	}

	if (argc < 3 || argc > 4) {
		printf("usage: mw[.l, .w, .b] address value [count]\n");
		return;
	}

	if (number(argv[1], &addr))
		return;

	if (number(argv[2], &val))
		return;

	cnt = 1;
	if (argc == 4 && number(argv[3], &cnt))
		return;

	for (i = 0; i < cnt; ++i) {
		switch (sz) {
		case 1:
			writeb(val, addr);
			break;
		case 2:
			writew(val, addr);
			break;
		case 4:
			writel(val, addr);
			break;
		}

		addr += sz;
	}
}

DEBUG_CMD("mw", "Memory write (longs)", mw);
DEBUG_CMD("mw.l", "Memory write (longs)", mw);
DEBUG_CMD("mw.w", "Memory write (half-words)", mw);
DEBUG_CMD("mw.b", "Memory write (bytes)", mw);
