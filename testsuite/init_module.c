/*
 * Copyright (C) 2012  ProFUSION embedded systems
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* kmod_elf_get_section() is not exported, we need the private header */
#include <libkmod-private.h>

/* FIXME: hack, change name so we don't clash */
#undef ERR
#include "mkdir.h"
#include "testsuite.h"
#include "stripped-module.h"

struct mod {
	struct mod *next;
	int ret;
	int errcode;
	char name[];
};

static struct mod *modules;
static bool need_init = true;

static void parse_retcodes(struct mod *_modules, const char *s)
{
	const char *p;

	if (s == NULL)
		return;

	for (p = s;;) {
		struct mod *mod;
		const char *modname;
		char *end;
		size_t modnamelen;
		int ret, errcode;
		long l;

		modname = p;
		if (modname == NULL || modname[0] == '\0')
			break;

		modnamelen = strcspn(s, ":");
		if (modname[modnamelen] != ':')
			break;

		p = modname + modnamelen + 1;
		if (p == NULL)
			break;

		l = strtol(p, &end, 0);
		if (end == p || *end != ':')
			break;
		ret = (int) l;
		p = end + 1;

		l = strtol(p, &end, 0);
		if (*end == ':')
			p = end + 1;
		else if (*end != '\0')
			break;

		errcode = (int) l;

		mod = malloc(sizeof(*mod) + modnamelen + 1);
		if (mod == NULL)
			break;

		memcpy(mod->name, modname, modnamelen);
		mod->name[modnamelen] = '\0';
		mod->ret = ret;
		mod->errcode = errcode;
		mod->next = _modules;
		_modules = mod;
	}
}

static int write_one_line_file(const char *fn, const char *line, int len)
{
        FILE *f;
        int r;

        assert(fn);
        assert(line);

        f = fopen(fn, "we");
        if (!f)
                return -errno;

        errno = 0;
        if (fputs(line, f) < 0) {
                r = -errno;
                goto finish;
        }

        fflush(f);

        if (ferror(f)) {
                if (errno != 0)
                        r = -errno;
                else
                        r = -EIO;
        } else
                r = 0;

finish:
        fclose(f);
        return r;
}

static int create_sysfs_files(const char *modname)
{
	char buf[PATH_MAX];
	const char *sysfsmod = "/sys/module/";
	int len = strlen(sysfsmod);

	memcpy(buf, sysfsmod, len);
	strcpy(buf + len, modname);
	len += strlen(modname);

	mkdir_p(buf, 0755);

	strcpy(buf + len, "/initstate");
	return write_one_line_file(buf, "live\n", strlen("live\n"));
}

static struct mod *find_module(struct mod *_modules, const char *modname)
{
	struct mod *mod;

	for (mod = _modules; mod != NULL; mod = mod->next) {
		if (strcmp(mod->name, modname) == 0)
			return mod;
	}

	return NULL;
}

static void init_retcodes(void)
{
	const char *s;

	if (!need_init)
		return;

	need_init = false;
	s = getenv(S_TC_INIT_MODULE_RETCODES);
	if (s == NULL) {
		fprintf(stderr, "TRAP init_module(): missing export %s?\n",
						S_TC_INIT_MODULE_RETCODES);
	}

	parse_retcodes(modules, s);
}

TS_EXPORT long init_module(void *mem, unsigned long len, const char *args);

/*
 * Default behavior is to try to mimic init_module behavior inside the kernel.
 * If it is a simple test that you know the error code, set the return code
 * in TESTSUITE_INIT_MODULE_RETCODES env var instead.
 *
 * The exception is when the module name is not find in the memory passed.
 * This is because we want to be able to pass dummy modules (and not real
 * ones) and it still work.
 */
long init_module(void *mem, unsigned long len, const char *args)
{
	const char *modname;
	struct kmod_elf *elf;
	struct mod *mod;
	const void *buf;
	uint64_t bufsize;
	int err;

	init_retcodes();

	elf = kmod_elf_new(mem, len);
	if (elf == NULL)
		return 0;

	err = kmod_elf_get_section(elf, ".gnu.linkonce.this_module", &buf,
								&bufsize);
	kmod_elf_unref(elf);

	/*
	 * We couldn't find the module's name inside the ELF file. Just exit
	 * as if it was successful
	 */
	if (err < 0)
		return 0;

	modname = (char *)buf + offsetof(struct module, name);
	mod = find_module(modules, modname);
	if (mod != NULL) {
		errno = mod->errcode;
		err = mod->ret;
	} else {
		/* mimic kernel behavior here */
		err = 0;
	}

	if (err == 0)
		create_sysfs_files(modname);

	return err;
}

/* the test is going away anyway, but lets keep valgrind happy */
void free_resources(void) __attribute__((destructor));
void free_resources(void)
{
	while (modules) {
		struct mod *mod = modules->next;
		free(modules);
		modules = mod;
	}
}
