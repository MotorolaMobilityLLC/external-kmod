#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "testsuite.h"

static void *nextlib;
static const char *rootpath;
static size_t rootpathlen;

static inline bool need_trap(const char *path)
{
	return path != NULL && path[0] == '/';
}

static const char *trap_path(const char *path, char buf[PATH_MAX * 2])
{
	size_t len;

	if (!need_trap(path))
		return path;

	len = strlen(path);

	if (len + rootpathlen > PATH_MAX * 2) {
		errno = ENAMETOOLONG;
		return NULL;
	}

	memcpy(buf, rootpath, rootpathlen);
	strcpy(buf + rootpathlen, path);
	return buf;
}

static bool get_rootpath(const char *f)
{
	if (rootpath != NULL)
		return true;

	rootpath = getenv(S_TC_ROOTFS);
	if (rootpath == NULL) {
		ERR("TRAP %s(): missing export %s?\n", f, S_TC_ROOTFS);
		errno = ENOENT;
		return false;
	}

	rootpathlen = strlen(rootpath);

	return true;
}

static void *get_libc_func(const char *f)
{
	void *fp;

	if (nextlib == NULL) {
#ifdef RTLD_NEXT
		nextlib = RTLD_NEXT;
#else
		nextlib = dlopen("libc.so.6", RTLD_LAZY);
#endif
	}

	fp = dlsym(nextlib, f);
	assert(fp);

	return fp;
}

TS_EXPORT FILE *fopen(const char *path, const char *mode)
{
	const char *p;
	char buf[PATH_MAX * 2];
	static int (*_fopen)(const char *path, const char *mode);

	if (!get_rootpath(__func__))
		return NULL;

	_fopen = get_libc_func("fopen");

	p = trap_path(path, buf);
	if (p == NULL)
		return NULL;

	return (void *) (long) (*_fopen)(p, mode);
}

TS_EXPORT int open(const char *path, int flags, ...)
{
	const char *p;
	char buf[PATH_MAX * 2];
	static int (*_open)(const char *path, int flags, ...);

	if (!get_rootpath(__func__))
		return -1;

	_open = get_libc_func("open");
	p = trap_path(path, buf);
	if (p == NULL)
		return -1;

	if (flags & O_CREAT) {
		mode_t mode;
		va_list ap;

		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
		_open(p, flags, mode);
	}

	return _open(p, flags);
}

TS_EXPORT int stat(const char *path, struct stat *st)
{
	const char *p;
	char buf[PATH_MAX * 2];
	static int (*_stat)(const char *path, struct stat *buf);

	if (!get_rootpath(__func__))
		return -1;

	_stat = get_libc_func("stat");

	p = trap_path(path, buf);
	if (p == NULL)
		return -1;

	return _stat(p, st);
}

TS_EXPORT int stat64(const char *path, struct stat64 *st)
{
	const char *p;
	char buf[PATH_MAX * 2];
	static int (*_stat64)(const char *path, struct stat64 *buf);

	if (!get_rootpath(__func__))
		return -1;

	_stat64 = get_libc_func("stat64");

	p = trap_path(path, buf);
	if (p == NULL)
		return -1;

	return _stat64(p, st);
}

TS_EXPORT int __xstat(int ver, const char *path, struct stat *st)
{
	const char *p;
	char buf[PATH_MAX * 2];
	static int (*_xstat)(int __ver, const char *__filename,
						struct stat *__stat_buf);

	if (!get_rootpath(__func__))
		return -1;

	_xstat = get_libc_func("__xstat");

	p = trap_path(path, buf);
	if (p == NULL)
		return -1;

	return _xstat(ver, p, st);
}

TS_EXPORT int access(const char *path, int mode)
{
	const char *p;
	char buf[PATH_MAX * 2];
	static int (*_access)(const char *path, int mode);

	if (!get_rootpath(__func__))
		return -1;

	_access = get_libc_func("access");

	p = trap_path(path, buf);
	if (p == NULL)
		return -1;

	return _access(p, mode);
}

TS_EXPORT DIR *opendir(const char *path)
{
	const char *p;
	char buf[PATH_MAX * 2];
	static int (*_opendir)(const char *path);

	if (!get_rootpath(__func__))
		return NULL;

	_opendir = get_libc_func("opendir");

	p = trap_path(path, buf);
	if (p == NULL)
		return NULL;

	return (void *)(long)(*_opendir)(p);
}
