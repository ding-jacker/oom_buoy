#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>

static char *progname = NULL;
static void usage()
{
	printf("usage:\n");
	printf("    %s -s size[PTGMKB]\n", progname);
	return ;
}

int
mkdaemon(void)
{
	int fd = -1;
	int ret = 0;
	
	fd = open("/dev/null", O_RDWR, 0);
	if (fd < 0) {
		return fd;
	}

	ret = fork();
	if (ret < 0) {
		close(fd);
		return ret;
	} else if (ret > 0) {
		exit(0);
	}

	/* reset stdin/stdout/stderr */
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	close(fd);

	/* dettach control terminal */
	ret = setsid();
	if (ret < 0) {
		exit(1);
	}

	/* second fork(), daemon can never attach control terminal */
	ret = fork();
	if (ret < 0) {
		exit(1);
	} else if (ret > 0) {
		exit(0);
	}

	chdir("/");

	return 0;
}
static int
oom_adj_set_disable(int value)
{
	int i = 0;
	int fd = -1;
	int disabled = !value;

	const char *oom_score_adj = "/proc/self/oom_score_adj";
	const char *oom_adj = "/proc/self/oom_adj";

	struct {
		const char *fname;
		int value;
	} oom_setting[2][2] = {
		/* DISABLE: can not be oom_killed */
		{
			{
				.fname = oom_score_adj,
				.value = -1000,
			},
			{
				.fname = oom_adj,
				.value = -17,
			},
		},
		/* ENABLE: can be oom_killed */
		{
			{
				.fname = oom_score_adj,
				.value = 1000,
			},
			{
				.fname = oom_adj,
				.value = 15,
			},
		},
	};

	for (i = 0; i < 2; i ++) {
		int n = 0;
		int r = 0;
		char buf[32] = {};
		fd = open(oom_setting[disabled][i].fname, O_WRONLY, 0);
		if (fd < 0) {
			continue;
		}
		n = sprintf(buf, "%d", oom_setting[disabled][i].value);
		r = write(fd, buf, n);
		close(fd);
		if (r == n) {
			break;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	size_t len = 0;
	char *p = NULL;

	enum {
		FIRST,
		SLASH,
		OTHER,
	} ST = FIRST;
	progname = p = argv[0];
	while (*p) {
		if (*p == '/')
			ST = SLASH;
		else if (ST == SLASH)
			ST = FIRST;
		else
			ST = OTHER;
		
		if (ST == FIRST)
			progname = p;

		p ++;
	}

	for (i = 1; i < argc; i ++) {
		if (strcmp(argv[i], "-s") == 0) {
			if (++i < argc) {
				p = argv[i];
				while (*p) {
					if (isdigit(*p)) {
						len = len * 10 + *p - '0';
					} else {
						switch (*p) {
						case 'p': case 'P':
							len <<= 10;
						case 't': case 'T':
							len <<= 10;
						case 'g': case 'G':
							len <<= 10;
						case 'm': case 'M':
							len <<= 10;
						case 'k': case 'K':
							len <<= 10;
						case 'b': case 'B':
							break;
						default:
							usage();
							exit(0);
						}
						break;
					}
					p ++;
				}
			} else {
				usage();
				exit(0);
			}
		}
	}
	if (len == 0) {
		usage();
		exit(0);
	}

	if (strcmp(progname, "oom_buoy") == 0) {
		oom_adj_set_disable(0);
	} else {
		oom_adj_set_disable(1);
	}
	p = (char *)mmap(NULL, len, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED) {
		printf("mmap failed: %m\n");
		exit(1);
	}
	memset(p, 0, len);
	if (mlock(p, len) < 0) {
		printf("warning: mlock failed: %m\n");
		exit(1);
	}
	mkdaemon();
	while (1) {
		sleep(60);
	}

	return 0;
}
