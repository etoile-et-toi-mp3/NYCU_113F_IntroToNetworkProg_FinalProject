include ./my.defines

PROGS =	UIED_Mahjong_client UIED_Mahjong_server

all:	${PROGS}

Mahjong_server:	Mahjong_server.o
		${CC} ${CFLAGS} -o $@ Mahjong_server.o ${LIBS}

Mahjong_client: Mahjong_client.o
		${CC} ${CFLAGS} -o $@ Mahjong_client.o ${LIBS}

UIED_Mahjong_client: UIED_Mahjong_client.o
		${CC} ${CFLAGS} -o $@ UIED_Mahjong_client.o ${LIBS}

UIED_Mahjong_server: UIED_Mahjong_server.o
		${CC} ${CFLAGS} -o $@ UIED_Mahjong_server.o ${LIBS}

clean:
		rm -f ${PROGS} ${CLEANFILES}
