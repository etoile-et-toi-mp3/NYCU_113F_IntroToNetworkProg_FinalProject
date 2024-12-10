include ./my.defines

PROGS =	Mahjong_server Mahjong_client

all:	${PROGS}

tcpcli04:	tcpcli04.o
		${CC} ${CFLAGS} -o $@ tcpcli04.o ${LIBS}

tcpserv09:	tcpserv09.o
		${CC} ${CFLAGS} -o $@ tcpserv09.o ${LIBS}

Mahjong_server:	Mahjong_server.o
		${CC} ${CFLAGS} -o $@ Mahjong_server.o ${LIBS}

Mahjong_client: Mahjong_client.o
		${CC} ${CFLAGS} -o $@ Mahjong_client.o ${LIBS}

clean:
		rm -f ${PROGS} ${CLEANFILES}
