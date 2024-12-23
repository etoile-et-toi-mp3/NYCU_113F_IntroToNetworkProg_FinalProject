include ./my.defines

PROGS =	Mahjong_server Mahjong_client

all:	${PROGS}

Mahjong_server:	Mahjong_server.o
		${CC} ${CFLAGS} -o $@ Mahjong_server.o ${LIBS}

Mahjong_client: Mahjong_client.o
		${CC} ${CFLAGS} -o $@ Mahjong_client.o ${LIBS}

clean:
		rm -f ${PROGS} ${CLEANFILES}
