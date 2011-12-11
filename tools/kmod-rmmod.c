/*
 * kmod-rmmod - remove modules from linux kernel using libkmod.
 *
 * Copyright (C) 2011  ProFUSION embedded systems
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation version 2.1.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>
#include "libkmod.h"


static const char cmdopts_s[] = "fsvVwh";
static const struct option cmdopts[] = {
	{"force", no_argument, 0, 'f'},
	{"syslog", no_argument, 0, 's'},
	{"verbose", no_argument, 0, 'v'},
	{"version", no_argument, 0, 'V'},
	{"wait", no_argument, 0, 'w'},
	{"help", no_argument, 0, 'h'},
	{NULL, 0, 0, 0}
};

static void help(const char *progname)
{
	fprintf(stderr,
		"Usage:\n"
		"\t%s [options] modulename ...\n"
		"Options:\n"
		"\t-f, --force       forces a module unload and may crash your\n"
		"\t                  machine. This requires Forced Module Removal\n"
		"\t                  option in your kernel. DANGEROUS\n"
		"\t-s, --syslog      print to syslog, not stderr\n"
		"\t-v, --verbose     enables more messages\n"
		"\t-V, --version     show version\n"
		"\t-w, --wait        begins module removal even if it is used and\n"
		"\t                  will stop new users from accessing it.\n"
		"\t-h, --help        show this help\n",
		progname);
}

static void log_syslog(void *data, int priority, const char *file, int line,
				const char *fn, const char *format,
				va_list args)
{
	char *str, buf[32];
	const char *prioname;

	switch (priority) {
	case LOG_CRIT:
		prioname = "FATAL";
		break;
	case LOG_ERR:
		prioname = "ERROR";
		break;
	case LOG_WARNING:
		prioname = "WARNING";
		break;
	case LOG_NOTICE:
		prioname = "NOTICE";
		break;
	case LOG_INFO:
		prioname = "INFO";
		break;
	case LOG_DEBUG:
		prioname = "DEBUG";
		break;
	default:
		snprintf(buf, sizeof(buf), "LOG-%03d", priority);
		prioname = buf;
	}

	if (vasprintf(&str, format, args) < 0)
		return;
#ifdef ENABLE_DEBUG
	syslog(LOG_NOTICE, "%s: %s:%d %s() %s", prioname, file, line, fn, str);
#else
	syslog(LOG_NOTICE, "%s: %s", prioname, str);
#endif
	free(str);
	(void)data;
}

int main(int argc, char *argv[])
{
	struct kmod_ctx *ctx;
	int flags = KMOD_REMOVE_NOWAIT;
	int use_syslog = 0;
	int verbose = 0;
	int i, err;

	for (;;) {
		int c, idx = 0;
		c = getopt_long(argc, argv, cmdopts_s, cmdopts, &idx);
		if (c == -1)
			break;
		switch (c) {
		case 'f':
			flags |= KMOD_REMOVE_FORCE;
			break;
		case 's':
			use_syslog = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			flags &= ~KMOD_REMOVE_NOWAIT;
			break;
		case 'h':
			help(argv[0]);
			return EXIT_SUCCESS;
		case 'V':
			puts(PACKAGE " version " VERSION);
			return EXIT_SUCCESS;
		case '?':
			return EXIT_FAILURE;
		default:
			fprintf(stderr,
				"Error: unexpected getopt_long() value '%c'.\n",
				c);
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Error: missing module name.\n");
		return EXIT_FAILURE;
	}

	ctx = kmod_new(NULL);
	if (!ctx) {
		fputs("Error: kmod_new() failed!\n", stderr);
		return EXIT_FAILURE;
	}

	kmod_set_log_priority(ctx, kmod_get_log_priority(ctx) + verbose);
	if (use_syslog) {
		openlog("rmmod", LOG_CONS, LOG_DAEMON);
		kmod_set_log_fn(ctx, log_syslog, NULL);
	}

	for (i = optind; i < argc; i++) {
		struct kmod_module *mod;
		const char *arg = argv[i];
		struct stat st;
		if (stat(arg, &st) == 0)
			err = kmod_module_new_from_path(ctx, arg, &mod);
		else
			err = kmod_module_new_from_name(ctx, arg, &mod);

		if (err < 0) {
			fprintf(stderr, "Error: could not use module %s: %s\n",
				arg, strerror(-err));
			break;
		}

		err = kmod_module_remove_module(mod, flags);
		if (err < 0) {
			fprintf(stderr,
				"Error: could not remove module %s: %s\n",
				arg, strerror(-err));
		}
		kmod_module_unref(mod);
		if (err < 0)
			break;
	}

	kmod_unref(ctx);

	if (use_syslog)
		closelog();

	return err >= 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
