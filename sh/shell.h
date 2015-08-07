/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)shell.h	8.2 (Berkeley) 5/4/95
 * $FreeBSD: head/bin/sh/shell.h 253658 2013-07-25 19:48:15Z jilles $
 */

#ifndef SHELL_H_
#define SHELL_H_

#include <inttypes.h>

/* local typedef of abstract string type. Using wide characters for UTF16 */
typedef unsigned short				char16_t;
typedef unsigned int				char32_t;
typedef const unsigned short		const_char16_t;
typedef const unsigned int			const_char32_t;
typedef char						cchar_t;
typedef char*   					cstring_t;
typedef const char* 				const_cstring_t;
typedef const char* __restrict  	const_cstring_t_restrict;
typedef void*						pvoid_t;
typedef const void*					const_pvoid_t;
typedef int32_t						status_t;
typedef uint8_t						boolean_t;

typedef union unionchar
{
	char32_t	char32;
	char16_t	char16[sizeof(char32_t)/sizeof(char16_t)];
	wchar_t		wchar[sizeof(char32_t)/sizeof(wchar_t)];
	uint8_t 	uint8[sizeof(char32_t)/sizeof(uint8_t)];
	int8_t  	int8[sizeof(char32_t)/sizeof(int8_t)];
} unionchar_t;

LIBC_IMPEXP1 cstring_t __cdecl _tcschr(_In_z_ const_cstring_t str,_In_ int32_t c);
LIBC_IMPEXP1 int32_t __cdecl _tcscmp(_In_z_ const_cstring_t str1,_In_z_ const_cstring_t str2) __attribute__((nonnull(1,2),__warn_unused_result__));
LIBC_IMPEXP1 int32_t __cdecl _tcsncmp(_In_reads_or_z_(nchars) const_cstring_t str1,_In_reads_or_z_(nchars) const_cstring_t str2,_In_ size_t nchars) __attribute__((nonnull(1,2),__warn_unused_result__));
//PSXDLL_IMPEXP int32_t __cdecl _tstat(_In_z_ const_cstring_t_restrict path,_Out_ stat_t* __restrict pstat) __attribute__((nonnull(1,2)));
LIBC_IMPEXP1 cstring_t __cdecl _tcsstr(_In_z_ const_cstring_t str,_In_z_ const_cstring_t find);
LIBC_IMPEXP2 cstring_t __cdecl _tcsdup(__in_z_opt const_cstring_t str);
LIBC_IMPEXP cstring_t __cdecl _tcsndup(__in_z_opt const_cstring_t str,_In_ size_t nelem);



/*
 * The follow should be set to reflect the type of system you have:
 *	JOBS -> 1 if you have Berkeley job control, 0 otherwise.
 *	define DEBUG=1 to compile in debugging (set global "debug" to turn on)
 *	define DEBUG=2 to compile in and turn on debugging.
 *
 * When debugging is on, debugging info will be written to ./trace and
 * a quit signal will generate a core dump.
 */


#define	JOBS 1
/* #define DEBUG 1 */

/*
 * Type of used arithmetics. SUSv3 requires us to have at least signed long.
 */
typedef intmax_t arith_t;
#define	ARITH_FORMAT_STR  "%" PRIdMAX
#define	atoarith_t(arg)  strtoimax(arg, NULL, 0)
#define	strtoarith_t(nptr, endptr, base)  strtoimax(nptr, endptr, base)
#define	ARITH_MIN INTMAX_MIN
#define	ARITH_MAX INTMAX_MAX

typedef void* pvoid_t;

#include <sys/cdefs.h>

extern char nullstr[1];		/* null string */

#ifdef DEBUG
#define TRACE(param)  sh_trace param
#else
#define TRACE(param)
#endif

#endif /* !SHELL_H_ */
