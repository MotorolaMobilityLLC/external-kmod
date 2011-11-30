/*
 * libkmod - interface to kernel module operations
 *
 * Copyright (C) 2011  ProFUSION embedded systems
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation version 2.1.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _LIBKMOD_MACRO_H_
#define _LIBKMOD_MACRO_H_

#include <stddef.h>

#define BUILD_ASSERT(cond) \
	do { (void) sizeof(char [1 - 2*!(cond)]); } while(0)

#define EXPR_BUILD_ASSERT(cond) \
	(sizeof(char [1 - 2*!(cond)]) - 1)

#if HAVE_TYPEOF
#define check_type(expr, type)			\
	((typeof(expr) *)0 != (type *)0)

#define check_types_match(expr1, expr2)		\
	((typeof(expr1) *)0 != (typeof(expr2) *)0)
#else
/* Without typeof, we can only test the sizes. */
#define check_type(expr, type)					\
	EXPR_BUILD_ASSERT(sizeof(expr) == sizeof(type))

#define check_types_match(expr1, expr2)				\
	EXPR_BUILD_ASSERT(sizeof(expr1) == sizeof(expr2))
#endif /* HAVE_TYPEOF */

#define container_of(member_ptr, containing_type, member)		\
	((containing_type *)						\
	 ((char *)(member_ptr) - offsetof(containing_type, member))	\
	 - check_types_match(*(member_ptr), ((containing_type *)0)->member))

/**
 * BUILD_ASSERT_OR_ZERO - assert a build-time dependency, as an expression.
 * @cond: the compile-time condition which must be true.
 *
 * Your compile will fail if the condition isn't true, or can't be evaluated
 * by the compiler.  This can be used in an expression: its value is "0".
 *
 * Example:
 *      #define foo_to_char(foo)                                            \
 *              ((char *)(foo)                                              \
 *              + BUILD_ASSERT_OR_ZERO(offsetof(struct foo, string) == 0))
 */
#define BUILD_ASSERT_OR_ZERO(cond) \
        (sizeof(char [1 - 2*!(cond)]) - 1)

/* Two gcc extensions.
 * &a[0] degrades to a pointer: a different type from an array */
#define _array_size_chk(arr)                                            \
        BUILD_ASSERT_OR_ZERO(!__builtin_types_compatible_p(typeof(arr), \
                                                        typeof(&(arr)[0])))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + _array_size_chk(arr))

/* Temporaries for importing index handling */
#define NOFAIL(x) (x)
#define fatal(x...) do { } while (0)

/* Attributes */

#define __must_check __attribute__((warn_unused_result))
#define __printf_format(a,b) __attribute__((format (printf, a, b)))
#if !defined(__always_inline)
#define __always_inline __inline__ __attribute__((always_inline))
#endif

#endif
