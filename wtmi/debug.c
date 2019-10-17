#include "types.h"
#include "uart.h"
#include "io.h"
#include "string.h"
#include "printf.h"
#include "mbox.h"
#include "crypto_hash.h"
#include "spi.h"

struct debug_cmd {
	char name[16];
//	char help[64];
	void (*cb)(int argc, char **argv);
};

#define CONCAT_(a,b) a##b
#define CONCAT(a,b) CONCAT_(a,b)
#define DEBUG_CMD(n,h,f) \
	static struct debug_cmd CONCAT(__debug_cmd_info, __COUNTER__) __attribute__((section(".debug_cmds"), used)) = { \
		.name = (n), \
		.cb = (f), \
	};

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

static inline void putc(u8 c)
{
	uart_putc((void *)&uart2_info, c);
}

static inline int getc(void)
{
	return uart_getc(&uart2_info);
}

static void puts(const char *s)
{
	while (*s)
		putc(*s++);
}

static void prompt(void)
{
	puts("debug cmd> ");
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

	putc(cmd[cmdpos - 1]);
	puts("\033[s");
	for (i = cmdpos; i < cmdlen; ++i)
		putc(cmd[i]);
	puts("\033[u");
}

static void cmd_del(void)
{
	int i;

	if (cmdpos == cmdlen)
		return;

	puts("\033[s");
	for (i = cmdpos; i < cmdlen-1; ++i) {
		cmd[i] = cmd[i+1];
		putc(cmd[i]);
	}
	puts("\033[K\033[u");

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
	puts((char *)cmd);
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
			puts("\033[D");
		}
	} else if (escape_seq[0] == 'C') {
		/* cursor right */
		if (cmdpos < cmdlen) {
			++cmdpos;
			puts("\033[C");
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
	static struct debug_cmd *s = (void *)&debug_cmds_start;
	static struct debug_cmd *e = (void *)&debug_cmds_end;
	struct debug_cmd *i;
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
		puts("^C\n");
		prompt();
		break;
	case 5:
		/* CTRL + E */
		cmd_set(cmd, cmdlen);
		break;
	case '\b':
		if (cmdpos) {
			puts("\033[D");
			--cmdpos;
			cmd_del();
		}
		break;
	case '\r':
	case '\n':
		putc('\n');
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
	puts("\nCZ.NIC Turris Mox Secure Firmware debug command line\n");
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

/*
static void help(int argc, char **argv)
{
	static struct debug_cmd *s = (void *)&debug_cmds_start;
	static struct debug_cmd *e = (void *)&debug_cmds_end;
	struct debug_cmd *i;

	for (i = s; i < e; ++i)
		printf("%s - %s\n", i->name, i->help);
	putc('\n');
}

DEBUG_CMD("help", "Display this help", help);
*/

static int number(const char *str, u32 *pres)
{
	const char *p = str;
	u32 res = 0;

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
		p += 2;

	while (*p) {
		res <<= 4;
		switch (*p) {
		case '0' ... '9':
			res += *p - '0';
			break;
		case 'a' ... 'f':
			res += *p - 'a' + 10;
			break;
		case 'A' ... 'F':
			res += *p - 'A' + 10;
			break;
		default:
			printf("invalid number %s\n", str);
			return -1;
		}
		++p;
	}

	if (pres)
		*pres = res;
	return 0;
}

static void md(int argc, char **argv)
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
		puts("usage: md[.l, .w, .b] address [count]\n");
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
			putc('\n');
	
		addr += sz;
	}

	if ((i % (16 / sz)) != 0)
		putc('\n');
}

DEBUG_CMD("md", "Memory display (longs)", md);
DEBUG_CMD("md.l", "Memory display (longs)", md);
DEBUG_CMD("md.w", "Memory display (half-words)", md);
DEBUG_CMD("md.b", "Memory display (bytes)", md);

static void mw(int argc, char **argv)
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
		puts("usage: mw[.l, .w, .b] address value [count]\n");
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

static void hash(int argc, char **argv)
{
	u32 digest[16];
	u32 addr, len;
	int i, id, dlen;

	if (argc != 3)
		goto usage;
	if (number(argv[1], &addr))
		goto usage;
	if (number(argv[2], &len))
		goto usage;

	id = hash_id(argv[0]);
	if (id == HASH_NA)
		goto usage;

	dlen = hw_hash(id, (void *)addr, len, digest);

	for (i = 0; i < dlen; ++i)
		printf("%08x", __builtin_bswap32(digest[i]));
	puts("\n\n");

	return;
usage:
	printf("usage: %s addr len\n", argv[0]);
}

DEBUG_CMD("md5", "MD5 hash", hash);
DEBUG_CMD("sha1", "SHA1 hash", hash);
DEBUG_CMD("sha224", "SHA224 hash", hash);
DEBUG_CMD("sha256", "SHA256 hash", hash);
DEBUG_CMD("sha384", "SHA384 hash", hash);
DEBUG_CMD("sha512", "SHA512 hash", hash);

#include "a53_helper/a53_helper.c"

static void kick(int argc, char **argv)
{
	u32 addr = 0x04100000;
	enum { None, Helper, Uboot } load = None;

	if (argc > 1) {
		if (argv[1][0] == 'l') {
			load = Helper;
		} else if (argv[1][0] == 'u') {
			load = Uboot;
		} else {
			puts("Invalid argument\n");
			return;
		}
		++argv;
		--argc;
	}

	if (argc > 1 && number(argv[1], &addr)) {
		puts("Invalid input number\n");
		return;
	}

	switch (load) {
	case Helper:
		memcpy((void *)(AP_RAM + addr), a53_helper_code,
		       sizeof(a53_helper_code));
		break;
	case Uboot:
		spi_nor_read(&nordev, (void *)(AP_RAM + addr), 0x20000,
			     0x160000);
		break;
	default:
		break;
	}

	start_ap_at(addr);
}

DEBUG_CMD("kick", "Kick AP", kick);

#include "clock.h"

#define SMI		0xc0032004
#define SMI_BUSY	BIT(28)
#define SMI_READ_VALID	BIT(27)
#define SMI_READ	BIT(26)
#define SMI_DEV(x)	(((x) & 0x1f) << 16)
#define SMI_REG(x)	(((x) & 0x1f) << 21)

static inline void mdio_wait(void)
{
	while (readl(SMI) & SMI_BUSY)
		wait_ns(10);
}

static void mdio_write(int dev, int reg, u16 val)
{
	mdio_wait();
	writel(SMI_DEV(dev) | SMI_REG(reg) | val, SMI);
}

static int mdio_read(int dev, int reg)
{
	u32 val;

	mdio_wait();
	writel(SMI_DEV(dev) | SMI_REG(reg) | SMI_READ, SMI);
	mdio_wait();

	val = readl(SMI);
	if (val & SMI_READ_VALID)
		return val & 0xffff;
	else
		return -1;
}

static void swrst(int argc, char **argv)
{
	u32 reg;

	mdio_write(1, 22, 0);
	mdio_write(1, 0, BIT(15));

	writel(0xa1b2c3d4, 0xc000d060);
	writel(0x4000, 0xc000d010);
	writel(0x66ffc030, 0xc000d00c);
//	setbitsl(0xc000d00c, 0, BIT(31));

	writel(0xffffffff, 0xc0008a04);
	writel(0xffffffff, 0xc0008a08);
	writel(0, 0xc0008a0c);
	writel(0, 0xc0008a00);

	writel(0xffffffff, 0xc0008a14);
	writel(0xffffffff, 0xc0008a18);
	writel(0xf003c000, 0xc0008a1c);
	writel(0, 0xc0008a10);

	for (reg = 0xc0008a30; reg < 0xc0008a64; reg += 4)
		writel(0, reg);

	writel(0, 0xc000d064);
	writel(0x0200, 0xc0008300);
	writel(0x0200, 0xc0008310);
	writel(0x0200, 0xc0008320);
	writel(0x0204, 0xc0008330);
	writel(0x0205, 0xc0008330);

	/* clocks to default values */
	writel(0x03cfccf2, 0xc0013000);
	writel(0x1296202c, 0xc0013004);
	writel(0x21061aa9, 0xc0013008);
	writel(0x20543084, 0xc001300c);
	writel(0x00009fff, 0xc0013010);
	writel(0x00000000, 0xc0013014);
	writel(0x003f8f40, 0xc0018000);
	writel(0x02515508, 0xc0018004);
	writel(0x00300880, 0xc0018008);
	writel(0x00000540, 0xc001800c);
	writel(0x000007aa, 0xc0018010);
	writel(0x00180000, 0xc0018014);
}

DEBUG_CMD("swrst", "CPU Software Reset", swrst);

static void led(int argc, char **argv)
{
	if (argc != 2)
		goto usage;

	if (!strcmp(argv[1], "on") || !strcmp(argv[1], "1")) {
		setbitsl(0xc0018800, BIT(21), BIT(21));
		setbitsl(0xc0018818, 0, BIT(21));
	} else if (!strcmp(argv[1], "off") || !strcmp(argv[1], "0")) {
		setbitsl(0xc0018800, BIT(21), BIT(21));
		setbitsl(0xc0018818, BIT(21), BIT(21));
	} else {
		goto usage;
	}

	return;
usage:
	puts("usage: led <on|off>\n");
}

DEBUG_CMD("led", "led control", led);

static void info(int argc, char **argv)
{
	u32 reg;

	reg = readl(0xc0018818);
	printf("Led: %s\n", reg & BIT(21) ? "off" : "on");

	reg = readl(0xc000d064);
	printf("Watchdog: %s\n", reg ? "active" : "inactive");

	reg = readl(0xc0008310);
	printf("Watchdog counter: %s, %s\n",
	       (reg & BIT(0)) ? "enabled" : "disabled",
	       (reg & BIT(1)) ? "active" : "inactive");
	if (reg & BIT(1)) {
		u64 cntr;
		cntr = readl(0xc0008314);
		cntr |= ((u64)readl(0xc0008318)) << 32;
		cntr *= (reg >> 8) & 0xff;
		cntr /= 25000;
		printf("Counter value: %d ms\n", (u32)cntr);
	}

#define l(a) printf(#a " = %08x\n", readl((a)))

	l(0xc000d00c);
	l(0xc000d010);
	l(0xc000d044);
	l(0xc000d048);
	l(0xc000d0a0);
	l(0xc000d0a4);
	l(0xc000d0b0);
	l(0xc000d0c0);
}

DEBUG_CMD("info", "Show some CPU info", info);

static void spi(int argc, char **argv)
{
	spi_init(&nordev);

	if (argc < 2)
		goto usage;

	if (!strcmp(argv[1], "id")) {
		u8 id[6];
		spi_nor_read_id(&nordev, id);
		printf("ID: %02x %02x %02x %02x %02x %02x\n", id[0], id[1],
		       id[2], id[3], id[4], id[5]);
	} else if (!strcmp(argv[1], "read")) {
		u32 addr, pos, len, total;

		if (argc != 5)
			goto usage;
		if (number(argv[2], &addr))
			goto usage;
		if (number(argv[3], &pos))
			goto usage;
		if (number(argv[4], &len))
			goto usage;

		printf("Reading 0x%x bytes from SPI position 0x%x to address 0x%08x\n",
		       len, pos, addr);

		total = 0;
		while (len) {
			void *buf = (void *)addr;
			u32 rd;

			rd = len > 256 ? 256 : len;
			spi_nor_read(&nordev, buf, pos, rd);
			total += rd;
			if (total % 0x10000 == 0)
				printf("\r%x read", total);

			len -= rd;
			pos += rd;
			addr += rd;
		}
		printf("\r%x read", total);
		putc('\n');
	} else {
		goto usage;
	}

	return;
usage:
	puts("usage: spi id\n");
	puts("       spi read addr position length\n");
}

DEBUG_CMD("spi", "SPI flash", spi);
