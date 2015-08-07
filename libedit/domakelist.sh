#!/bin/sh -

# makelist.sh: Automatically generate header files...

HOST_SH="/bin/sh"
LIBEDITDIR="."

# for i in \
# 	"chared.c" "chartype.c" "common.c" "el.c" "emacs.c" "fcns.c" "filecomplete.c"	\
# 	"help.c" "hist.c" "key.c" "map.c" "parse.c" "prompt.c" "read.c" "refresh.c"		\
# 	"search.c" "sig.c" "term.c" "tty.c" "vi.c" "eln.c"
# do 
# 	OSRCS[${#OSRCS[@]}]=$i; 
# done;unset i

# for i in "editline.c" "readline.c" "tokenizer.c" "history.c" "tokenizern.c" "historyn.c"
# 	do SRCS[${#SRCS[@]}]=$i; done;unset i

# for i in "vi.h" "emacs.h" "common.h"
#	do AHDR[${#AHDR[@]}]=$i; done;unset i

# for i in "${LIBEDITDIR}/vi.c" "${LIBEDITDIR}/emacs.c" "${LIBEDITDIR}/common.c"; 
# 	do ASRC[${#ASRC[@]}]=$i; done;unset i

OSRCS="chared.c common.c el.c emacs.c fcns.c filecomplete.c help.c hist.c key.c map.c chartype.c \
	parse.c prompt.c read.c refresh.c search.c sig.c term.c tty.c vi.c eln.c"

AHDR="vi.h emacs.h common.h"
ASRC="${LIBEDITDIR}/vi.c ${LIBEDITDIR}/emacs.c ${LIBEDITDIR}/common.c"


#--------------------------------------------------------------------------

# vi.h: vi.c makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -h ${LIBEDITDIR}/vi.c			> vi.h

# emacs.h: emacs.c makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -h ${LIBEDITDIR}/emacs.c		> emacs.h

# common.h: common.c makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -h ${LIBEDITDIR}/common.c 	> common.h

# fcns.h: ${AHDR} makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -fh ${AHDR}					> fcns.h

# fcns.c: ${AHDR} fcns.h help.h makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -fc ${AHDR}					> fcns.c

# help.h: ${ASRC} makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -bh ${ASRC}					> help.h

# help.c: ${ASRC} makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -bc ${ASRC}					> help.c

# editline.c: ${OSRCS} makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -e ${OSRCS}					> editline.c

# tokenizern.c: makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -n tokenizer.c				> tokenizern.c

# historyn.c: makelist Makefile
	${HOST_SH} ${LIBEDITDIR}/makelist -n history.c					> historyn.c

