#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

/* exit(), etc */
#include <stdlib.h>

/* getopt */
#include <unistd.h>

#include <syslog.h>

/* open() */
#include <sys/stat.h>
#include <fcntl.h>

struct fbuf {
	size_t bytes_in_buf;
	uint8_t buf[4096];
};

static void *fbuf_space_ptr(struct fbuf *f)
{
	return f->buf + f->bytes_in_buf;
}

static size_t fbuf_space(struct fbuf *f)
{
	return sizeof(f->buf) - f->bytes_in_buf;
}

static void fbuf_feed(struct fbuf *f, size_t n)
{
	assert(fbuf_space(f) <= n);
	f->bytes_in_buf += n;
}

static void *fbuf_data_ptr(struct fbuf *f)
{
	return f->buf;
}

static size_t fbuf_data(struct fbuf *f)
{
	return f->bytes_in_buf;
}

static void fbuf_eat(struct fbuf *f, size_t n)
{
	assert(n <= fbuf_data(f));
	memmove(f->buf, f->buf + n, f->bytes_in_buf - n);
	f->bytes_in_buf -= n;
}

static void fbuf_init(struct fbuf *f)
{
	/* NOTE: for perf, we do not zero the buffer */
	f->bytes_in_buf = 0;
}


static
const char *opts = ":hkns";
#define PRGMNAME_DEFAULT "logto"

static
void usage_(const char *prgmname, int e)
{
	FILE *f;
	if (e != EXIT_SUCCESS)
		f = stderr;
	else
		f = stdout;

	fprintf(f,
"Usage: %s [options] -- <program> [<args>...]\n"
"\n"
"Options: [%s]\n"
" -k    send output to /dev/kmsg\n"
" -n    send output to netconsole (udp)\n"
" -s    send output to syslog (local)\n"
" -h    show this help text\n",
	prgmname, opts);
	exit(e);
}
#define usage(e) usage_(prgmname, e)

int main(int argc, char **argv)
{
	const char *prgmname = argc?argv[0]:PRGMNAME_DEFAULT;
	int opt, err = 0;

	bool use_kmsg = false, use_netconsole = false, use_syslog = false;

	while ((opt = getopt(argc, argv, opts)) != -1) {
		switch (opt) {
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		case 'k':
			use_kmsg = true;
			break;
		case 'n':
			use_netconsole = true;
			break;
		case 's':
			use_syslog = true;
			break;
		case '?':
			err++;
			break;
		default:
			fprintf(stderr, "Error: programmer screwed up argument -%c\n", opt);
			err++;
			break;
		}
	}

	if (!use_netconsole && !use_kmsg && !use_syslog) {
		fprintf(stderr, "Error: no destination selected, but one is required\n");
		err++;
	}

	if ((use_netconsole + use_kmsg + use_syslog) > 1) {
		fprintf(stderr, "Sorry, right now we only support one destination at a time\n");
		err++;
	}

	if (err)
		usage(EXIT_FAILURE);


	/* TODO: check that we don't have any extra file descriptors open */

	/*
	 * we're going to exec another program with special handling for
	 * STDIN_FILENO, STDOUT_FILENO, and STDERR_FILENO
	 */
	argc -= optind;
	argv += optind;

	/* NOTE: error checking throughout this is a very fiddly problem:
	 * because of the type of program this is, unless we're able to log
	 * messages over the medium we're trying to open, the output may never
	 * be seen. We'll try to log failures anyhow, but there might be room
	 * for improvement. */

	/* 0 = read, 1 = write */
	int new_stdout[] = {-1, -1};
	if (use_kmsg) {
		new_stdout[1] = open("/dev/kmsg", O_RDWR);
		if (new_stdout[1] == -1) {
			fprintf(stderr, "could not open /dev/kmsg: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else if (use_netconsole || use_syslog) {
		int r = pipe(new_stdout);
		if (r == -1) {
			fprintf(stderr, "could not setup pipe(): %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* if required, fork */
	bool should_exec = true;
	pid_t child = -1;
	if (new_stdout[0] != -1) {
		child = fork();
		if (child == -1) {
			fprintf(stderr, "fork failed: %s\n", strerror(errno));
		}
		should_exec = child == 0;
	}

	if (should_exec) {
		/* dup new fds into place, close old ones */
		dup2(new_stdout[1], STDOUT_FILENO);
		dup2(new_stdout[1], STDERR_FILENO);

		close(new_stdout[0]);
		close(new_stdout[1]);

		int r = execvp(argv[0], argv);
		if (r == -1) {
			fprintf(stderr, "exec failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	close(new_stdout[1]);

	struct fbuf buf;

	/* NOTE: we also need to wait() for the child to die and exit (or
	 * preform another appropriate action) when it does.
	 */

	/* XXX: consider allowing control of prefixing the output (or otherwise formatting it) */
	/* XXX: consider allowing the printing of informational messages when
	 * the child is started and when it stops */

	/* TODO: if required, process data over pipes */
	for (;;) {
		ssize_t r = read(new_stdout[0], fbuf_space_ptr(&buf), fbuf_space(&buf));
		if (r == 0) {
			/* Bad things
			 */
		}

		/* TODO: read data */

		/* TODO: write data to syslog or netconsole */
	}

	return 0;
}
