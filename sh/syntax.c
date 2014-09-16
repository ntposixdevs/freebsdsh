/*
 * This file was generated by the mksyntax program.
 */

#include <sys/types.h>
#include "parser.h"
#include "shell.h"
#include "syntax.h"

/* syntax table used when not in quotes */
const char basesyntax[SYNBASE + CHAR_MAX + 1] =
{
	[SYNBASE + PEOF] = CEOF,
	[SYNBASE + CTLESC] = CCTL,
	[SYNBASE + CTLVAR] = CCTL,
	[SYNBASE + CTLENDVAR] = CCTL,
	[SYNBASE + CTLBACKQ] = CCTL,
	[SYNBASE + CTLBACKQ + CTLQUOTE] = CCTL,
	[SYNBASE + CTLARI] = CCTL,
	[SYNBASE + CTLENDARI] = CCTL,
	[SYNBASE + CTLQUOTEMARK] = CCTL,
	[SYNBASE + CTLQUOTEEND] = CCTL,
	[SYNBASE + '\n'] = CNL,
	[SYNBASE + '\\'] = CBACK,
	[SYNBASE + '\''] = CSQUOTE,
	[SYNBASE + '"'] = CDQUOTE,
	[SYNBASE + '`'] = CBQUOTE,
	[SYNBASE + '$'] = CVAR,
	[SYNBASE + '}'] = CENDVAR,
	[SYNBASE + '<'] = CSPCL,
	[SYNBASE + '>'] = CSPCL,
	[SYNBASE + '('] = CSPCL,
	[SYNBASE + ')'] = CSPCL,
	[SYNBASE + ';'] = CSPCL,
	[SYNBASE + '&'] = CSPCL,
	[SYNBASE + '|'] = CSPCL,
	[SYNBASE + ' '] = CSPCL,
	[SYNBASE + '\t'] = CSPCL,
};

/* syntax table used when in double quotes */
const char dqsyntax[SYNBASE + CHAR_MAX + 1] =
{
	[SYNBASE + PEOF] = CEOF,
	[SYNBASE + CTLESC] = CCTL,
	[SYNBASE + CTLVAR] = CCTL,
	[SYNBASE + CTLENDVAR] = CCTL,
	[SYNBASE + CTLBACKQ] = CCTL,
	[SYNBASE + CTLBACKQ + CTLQUOTE] = CCTL,
	[SYNBASE + CTLARI] = CCTL,
	[SYNBASE + CTLENDARI] = CCTL,
	[SYNBASE + CTLQUOTEMARK] = CCTL,
	[SYNBASE + CTLQUOTEEND] = CCTL,
	[SYNBASE + '\n'] = CNL,
	[SYNBASE + '\\'] = CBACK,
	[SYNBASE + '"'] = CENDQUOTE,
	[SYNBASE + '`'] = CBQUOTE,
	[SYNBASE + '$'] = CVAR,
	[SYNBASE + '}'] = CENDVAR,
	[SYNBASE + '!'] = CCTL,
	[SYNBASE + '*'] = CCTL,
	[SYNBASE + '?'] = CCTL,
	[SYNBASE + '['] = CCTL,
	[SYNBASE + ']'] = CCTL,
	[SYNBASE + '='] = CCTL,
	[SYNBASE + '~'] = CCTL,
	[SYNBASE + ':'] = CCTL,
	[SYNBASE + '/'] = CCTL,
	[SYNBASE + '-'] = CCTL,
	[SYNBASE + '^'] = CCTL,
};

/* syntax table used when in single quotes */
const char sqsyntax[SYNBASE + CHAR_MAX + 1] =
{
	[SYNBASE + PEOF] = CEOF,
	[SYNBASE + CTLESC] = CCTL,
	[SYNBASE + CTLVAR] = CCTL,
	[SYNBASE + CTLENDVAR] = CCTL,
	[SYNBASE + CTLBACKQ] = CCTL,
	[SYNBASE + CTLBACKQ + CTLQUOTE] = CCTL,
	[SYNBASE + CTLARI] = CCTL,
	[SYNBASE + CTLENDARI] = CCTL,
	[SYNBASE + CTLQUOTEMARK] = CCTL,
	[SYNBASE + CTLQUOTEEND] = CCTL,
	[SYNBASE + '\n'] = CNL,
	[SYNBASE + '\\'] = CSBACK,
	[SYNBASE + '\''] = CENDQUOTE,
	[SYNBASE + '!'] = CCTL,
	[SYNBASE + '*'] = CCTL,
	[SYNBASE + '?'] = CCTL,
	[SYNBASE + '['] = CCTL,
	[SYNBASE + ']'] = CCTL,
	[SYNBASE + '='] = CCTL,
	[SYNBASE + '~'] = CCTL,
	[SYNBASE + ':'] = CCTL,
	[SYNBASE + '/'] = CCTL,
	[SYNBASE + '-'] = CCTL,
	[SYNBASE + '^'] = CCTL,
};

/* syntax table used when in arithmetic */
const char arisyntax[SYNBASE + CHAR_MAX + 1] =
{
	[SYNBASE + PEOF] = CEOF,
	[SYNBASE + CTLESC] = CCTL,
	[SYNBASE + CTLVAR] = CCTL,
	[SYNBASE + CTLENDVAR] = CCTL,
	[SYNBASE + CTLBACKQ] = CCTL,
	[SYNBASE + CTLBACKQ + CTLQUOTE] = CCTL,
	[SYNBASE + CTLARI] = CCTL,
	[SYNBASE + CTLENDARI] = CCTL,
	[SYNBASE + CTLQUOTEMARK] = CCTL,
	[SYNBASE + CTLQUOTEEND] = CCTL,
	[SYNBASE + '\n'] = CNL,
	[SYNBASE + '\\'] = CBACK,
	[SYNBASE + '`'] = CBQUOTE,
	[SYNBASE + '"'] = CIGN,
	[SYNBASE + '$'] = CVAR,
	[SYNBASE + '}'] = CENDVAR,
	[SYNBASE + '('] = CLP,
	[SYNBASE + ')'] = CRP,
};

/* character classification table */
const char is_type[SYNBASE + CHAR_MAX + 1] =
{
	[SYNBASE + '0'] = ISDIGIT,
	[SYNBASE + '1'] = ISDIGIT,
	[SYNBASE + '2'] = ISDIGIT,
	[SYNBASE + '3'] = ISDIGIT,
	[SYNBASE + '4'] = ISDIGIT,
	[SYNBASE + '5'] = ISDIGIT,
	[SYNBASE + '6'] = ISDIGIT,
	[SYNBASE + '7'] = ISDIGIT,
	[SYNBASE + '8'] = ISDIGIT,
	[SYNBASE + '9'] = ISDIGIT,
	[SYNBASE + 'a'] = ISLOWER,
	[SYNBASE + 'b'] = ISLOWER,
	[SYNBASE + 'c'] = ISLOWER,
	[SYNBASE + 'd'] = ISLOWER,
	[SYNBASE + 'e'] = ISLOWER,
	[SYNBASE + 'f'] = ISLOWER,
	[SYNBASE + 'g'] = ISLOWER,
	[SYNBASE + 'h'] = ISLOWER,
	[SYNBASE + 'i'] = ISLOWER,
	[SYNBASE + 'j'] = ISLOWER,
	[SYNBASE + 'k'] = ISLOWER,
	[SYNBASE + 'l'] = ISLOWER,
	[SYNBASE + 'm'] = ISLOWER,
	[SYNBASE + 'n'] = ISLOWER,
	[SYNBASE + 'o'] = ISLOWER,
	[SYNBASE + 'p'] = ISLOWER,
	[SYNBASE + 'q'] = ISLOWER,
	[SYNBASE + 'r'] = ISLOWER,
	[SYNBASE + 's'] = ISLOWER,
	[SYNBASE + 't'] = ISLOWER,
	[SYNBASE + 'u'] = ISLOWER,
	[SYNBASE + 'v'] = ISLOWER,
	[SYNBASE + 'w'] = ISLOWER,
	[SYNBASE + 'x'] = ISLOWER,
	[SYNBASE + 'y'] = ISLOWER,
	[SYNBASE + 'z'] = ISLOWER,
	[SYNBASE + 'A'] = ISUPPER,
	[SYNBASE + 'B'] = ISUPPER,
	[SYNBASE + 'C'] = ISUPPER,
	[SYNBASE + 'D'] = ISUPPER,
	[SYNBASE + 'E'] = ISUPPER,
	[SYNBASE + 'F'] = ISUPPER,
	[SYNBASE + 'G'] = ISUPPER,
	[SYNBASE + 'H'] = ISUPPER,
	[SYNBASE + 'I'] = ISUPPER,
	[SYNBASE + 'J'] = ISUPPER,
	[SYNBASE + 'K'] = ISUPPER,
	[SYNBASE + 'L'] = ISUPPER,
	[SYNBASE + 'M'] = ISUPPER,
	[SYNBASE + 'N'] = ISUPPER,
	[SYNBASE + 'O'] = ISUPPER,
	[SYNBASE + 'P'] = ISUPPER,
	[SYNBASE + 'Q'] = ISUPPER,
	[SYNBASE + 'R'] = ISUPPER,
	[SYNBASE + 'S'] = ISUPPER,
	[SYNBASE + 'T'] = ISUPPER,
	[SYNBASE + 'U'] = ISUPPER,
	[SYNBASE + 'V'] = ISUPPER,
	[SYNBASE + 'W'] = ISUPPER,
	[SYNBASE + 'X'] = ISUPPER,
	[SYNBASE + 'Y'] = ISUPPER,
	[SYNBASE + 'Z'] = ISUPPER,
	[SYNBASE + '_'] = ISUNDER,
	[SYNBASE + '#'] = ISSPECL,
	[SYNBASE + '?'] = ISSPECL,
	[SYNBASE + '$'] = ISSPECL,
	[SYNBASE + '!'] = ISSPECL,
	[SYNBASE + '-'] = ISSPECL,
	[SYNBASE + '*'] = ISSPECL,
	[SYNBASE + '@'] = ISSPECL,
};
