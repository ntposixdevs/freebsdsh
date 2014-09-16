/*
 * This file was generated by the mkbuiltins program.
 */

#include <sys/cdefs.h>
#define BLTINCMD 0
#define ALIASCMD 1
#define BGCMD 2
#define BINDCMD 3
#define BREAKCMD 4
#define CDCMD 5
#define COMMANDCMD 6
#define DOTCMD 7
#define ECHOCMD 8
#define EVALCMD 9
#define EXECCMD 10
#define EXITCMD 11
#define LETCMD 12
#define EXPORTCMD 13
#define FALSECMD 14
#define FGCMD 15
#define GETOPTSCMD 16
#define HASHCMD 17
#define HISTCMD 18
#define JOBIDCMD 19
#define JOBSCMD 20
#define KILLCMD 21
#define LOCALCMD 22
#define PRINTFCMD 23
#define PWDCMD 24
#define READCMD 25
#define RETURNCMD 26
#define SETCMD 27
#define SETVARCMD 28
#define SHIFTCMD 29
#define TESTCMD 30
#define TIMESCMD 31
#define TRAPCMD 32
#define TRUECMD 33
#define TYPECMD 34
#define ULIMITCMD 35
#define UMASKCMD 36
#define UNALIASCMD 37
#define UNSETCMD 38
#define WAITCMD 39
#define WORDEXPCMD 40

struct builtincmd
{
	const char* name;
	int code;
	int special;
};

extern int (*const builtinfunc[])(int, char**);
extern const struct builtincmd builtincmd[];

int bltincmd(int, char**);
int aliascmd(int, char**);
int bgcmd(int, char**);
int bindcmd(int, char**);
int breakcmd(int, char**);
int cdcmd(int, char**);
int commandcmd(int, char**);
int dotcmd(int, char**);
int echocmd(int, char**);
int evalcmd(int, char**);
int execcmd(int, char**);
int exitcmd(int, char**);
int letcmd(int, char**);
int exportcmd(int, char**);
int falsecmd(int, char**);
int fgcmd(int, char**);
int getoptscmd(int, char**);
int hashcmd(int, char**);
int histcmd(int, char**);
int jobidcmd(int, char**);
int jobscmd(int, char**);
int killcmd(int, char**);
int localcmd(int, char**);
int printfcmd(int, char**);
int pwdcmd(int, char**);
int readcmd(int, char**);
int returncmd(int, char**);
int setcmd(int, char**);
int setvarcmd(int, char**);
int shiftcmd(int, char**);
int testcmd(int, char**);
int timescmd(int, char**);
int trapcmd(int, char**);
int truecmd(int, char**);
int typecmd(int, char**);
int ulimitcmd(int, char**);
int umaskcmd(int, char**);
int unaliascmd(int, char**);
int unsetcmd(int, char**);
int waitcmd(int, char**);
int wordexpcmd(int, char**);
