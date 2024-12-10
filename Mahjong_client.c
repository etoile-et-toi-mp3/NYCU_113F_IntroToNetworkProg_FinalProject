/*

    This is a client that connects to a Mahjong server and plays the game.

*/

#include "unp.h"

// the following variables are for connection purposes;
int fd;
struct sockaddr_in serverinfo;
socklen_t serverlen;
char sendline[MAXLINE] = {0}, recvline[MAXLINE] = {0};
fd_set rset, testset;
int maxfd = 0, id_num;

// the following variables are for game purposes;
struct mj {
    int type;
    int number;
    int priority;
} decks[20]; // all the numbers here should be 1-indexed;

const int NO_TYPE = 1;
const int TONG = 1;
const int TIAO = 2;
const int WAN = 3;
const int DAZI = 4;   // it goes with 東南西北中發白;
const int FLOWER = 5; // it goes with 春夏秋冬梅蘭竹菊;

int connection_establish() {
    connect(fd, (SA *)&serverinfo, serverlen);
    FD_SET(fd, &rset);
    FD_SET(STDIN_FILENO, &rset);
    maxfd = (fd > STDIN_FILENO) ? fd : STDIN_FILENO;

    for (;;)
    {
        testset = rset;
        select(maxfd + 1, &testset, NULL, NULL, NULL);

        if (FD_ISSET(fd, &testset))
        {
            if (read(fd, recvline, MAXLINE) <= 0)
            {
                printf("Server terminated prematurely!? Exiting...\n");
                exit(1);
            }
            else if (strcmp(recvline, "wait...\n") == 0)
            {
                // well... just wait for others to join
                memset(recvline, 0, strlen(recvline));
            }
            else if (strcmp(recvline, "start!\n") == 0)
            {
                // game start!!!
                memset(recvline, 0, strlen(recvline));

                return 0;
            }
        }

        if (FD_ISSET(STDIN_FILENO, &testset))
        {
            if (fgets(recvline, MAXLINE, stdin) == NULL)
            {
                // client has pressed Ctrl+D
                close(fd);
                printf("You pressed Ctrl+D, leaving...\n");
                exit(0);
            }
            else
            {
                // client shouldn't be typing in this phase actually
                // so we just clear the buffer and do nothing
                memset(recvline, 0, strlen(recvline));
            }
        }
    }
}

int get_decks() {
    int t, n;
    for (int i = 0; i < 16; ++i)
    {
        read(fd, recvline, MAXLINE);
        sscanf(recvline, "%d %d", &t, &n);
        decks[i].type = t;
        decks[i].number = n;
        memset(recvline, 0, strlen(recvline));

        sprintf(sendline, "ACK\n");
        write(fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));
    }
    return 0;
}

int game() {
    read(fd, recvline, MAXLINE);
    id_num = (int)recvline[0] - '0';
    printf("This is your id: %d\n", id_num);
    memset(recvline, 0, strlen(recvline));
    for (int startplayer = 0; startplayer < 4; ++startplayer)
    {
        int playernow = startplayer;
        get_decks();
        for( ; ; )
        {
            if(read(fd, recvline, MAXLINE) <= 0)
            {
                printf("Server terminated prematurely!? Exiting...\n");
            }
            else 
            {
                if(strcmp(recvline, "your turn\n") == 0)
                {
                    // yeah it's your turn;
                    // the system will give you a mj and 
                    // regrab automatically if it's a flower;

                    // start here
                }
            }

        }
        get_result();
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2)
    {
        printf("Usage: ./Mahjong_client <ServerIP>\n");
        return 1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serverinfo, 0, (serverlen = sizeof(struct sockaddr_in)));
    serverinfo.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &serverinfo.sin_addr);
    serverinfo.sin_port = htons(SERV_PORT + 10);
    FD_ZERO(&rset);
    FD_ZERO(&testset);

    if (connection_establish() == 0)
    {
        game();
    }
    return 0;
}