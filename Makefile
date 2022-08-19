CFLAGS= -O3 -W -Wformat=2 -g -Wall -Wextra -Werror -pedantic
LDFLAGS= 
FILES=main.c
OUTFILE=snake
CC=gcc

all: ${OUTFILE}

${OUTFILE}: ${FILES}
	${CC} ${FILES} -o ${OUTFILE} ${CFLAGS} ${LDFLAGS}

run: ${OUTFILE}
	./${OUTFILE}
