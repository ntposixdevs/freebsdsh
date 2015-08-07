/*
 * This file was generated by the mksyntax program.
 */

#include <sys/cdefs.h>
#include <limits.h>

/* Syntax classes */
#define CWORD 0			/* character is nothing special */
#define CNL 1			/* newline character */
#define CBACK 2			/* a backslash character */
#define CSBACK 3		/* a backslash character in single quotes */
#define CSQUOTE 4		/* single quote */
#define CDQUOTE 5		/* double quote */
#define CENDQUOTE 6		/* a terminating quote */
#define CBQUOTE 7		/* backwards single quote */
#define CVAR 8			/* a dollar sign */
#define CENDVAR 9		/* a '}' character */
#define CLP 10			/* a left paren in arithmetic */
#define CRP 11			/* a right paren in arithmetic */
#define CEOF 12			/* end of file */
#define CCTL 13			/* like CWORD, except it must be escaped */
#define CSPCL 14		/* these terminate a word */
#define CIGN 15			/* character should be ignored */

/* Syntax classes for is_ functions */
#define ISDIGIT 01		/* a digit */
#define ISUPPER 02		/* an upper case letter */
#define ISLOWER 04		/* a lower case letter */
#define ISUNDER 010		/* an underscore */
#define ISSPECL 020		/* the name of a special parameter */

#define SYNBASE (1 - CHAR_MIN)
#define PEOF -SYNBASE


#define BASESYNTAX (basesyntax + SYNBASE)
#define DQSYNTAX (dqsyntax + SYNBASE)
#define SQSYNTAX (sqsyntax + SYNBASE)
#define ARISYNTAX (arisyntax + SYNBASE)

#define is_digit(c)	((uint32_t)((c) - '0') <= 9)
#define is_eof(c)	((c) == PEOF)
#define is_alpha(c)	((is_type+SYNBASE)[(int32_t)c] & (ISUPPER|ISLOWER))
#define is_name(c)	((is_type+SYNBASE)[(int32_t)c] & (ISUPPER|ISLOWER|ISUNDER))
#define is_in_name(c)	((is_type+SYNBASE)[(int32_t)c] & (ISUPPER|ISLOWER|ISUNDER|ISDIGIT))
#define is_special(c)	((is_type+SYNBASE)[(int32_t)c] & (ISSPECL|ISDIGIT))
#define digit_val(c)	((c) - '0')

extern const char basesyntax[];
extern const char dqsyntax[];
extern const char sqsyntax[];
extern const char arisyntax[];
extern const char is_type[];
