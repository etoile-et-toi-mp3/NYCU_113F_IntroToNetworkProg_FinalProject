include ./my.defines

PROGS =	UIED_Mahjong_client UIED_Mahjong_server

all:	${PROGS}

debug:	CFLAGS += -D DEBUG
debug:	${PROGS}

UIED_Mahjong_client: UIED_Mahjong_client.o
		${CC} ${CFLAGS} -o $@ UIED_Mahjong_client.o ${LIBS}

UIED_Mahjong_server: UIED_Mahjong_server.o
		${CC} ${CFLAGS} -o $@ UIED_Mahjong_server.o ${LIBS}

clean:
		rm -f ${PROGS} ${CLEANFILES}
