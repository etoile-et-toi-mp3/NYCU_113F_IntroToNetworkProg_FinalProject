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
    int type;   // all the numbers here should be 1-indexed;
    int number; // all the numbers here should be 1-indexed;
    int priority;
} decks[20], flowers[8], door[20], sea[150], discarded_mj;

int flower_index = 0, door_index = 0, sea_index = 0, start_player = 0;

const int NO_TYPE = 0;
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
    for (;;)
    {
        read(fd, recvline, MAXLINE);
        if (strcmp(recvline, "transfer complete\n") == 0)
        {
            break;
        }
        sscanf(recvline, "%d", &n);
        flowers[flower_index].type = FLOWER;
        flowers[flower_index++].number = n;
        memset(recvline, 0, strlen(recvline));

        sprintf(sendline, "ACK\n");
        write(fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));
    }
    return 0;
}

void swap(struct mj *a, struct mj *b) {
    struct mj temp = *a;
    *a = *b;
    *b = temp;
    return;
}

void decks_quick_sort(struct mj *mjs, int start, int end) {
    if (start >= end)
    {
        return; // Base case: single element or invalid range
    }

    int pivot = mjs[start].type * 10 + mjs[start].number;
    int left = start + 1, right = end;

    while (left <= right)
    {
        // Compare based on `type` and `number` hierarchically
        while (left <= right && mjs[left].type * 10 + mjs[left].number <= pivot)
        {
            left++;
        }
        while (left <= right && mjs[right].type * 10 + mjs[right].number >= pivot)
        {
            right--;
        }

        if (left < right)
        {
            swap(&mjs[left], &mjs[right]);
        }
    }

    // Place the pivot in the correct position
    swap(&mjs[start], &mjs[right]);

    // Recursively sort elements before and after the pivot
    decks_quick_sort(mjs, start, right - 1);
    decks_quick_sort(mjs, right + 1, end);
}

int client_draw() {
    // get new mj;
    read(fd, recvline, MAXLINE);
    sscanf(recvline, "%d %d", &decks[16].type, &decks[16].number);
    memset(recvline, 0, strlen(recvline));

    // flower regrab
    while (decks[16].type == FLOWER)
    {
        flowers[flower_index++] = decks[16];

        read(fd, recvline, MAXLINE);
        sscanf(recvline, "%d %d\n", &decks[16].type, &decks[16].number);
        memset(recvline, 0, strlen(recvline));
    }
    return 0;
}

int client_is_hu() {
    read(fd, recvline, MAXLINE);
    if (strcmp(recvline, "can hu\n") == 0)
    {
        char answer[64];
        for (;;)
        {
            printf("You already suffice the conditions to win, proceed? [Y/n]\n");
            scanf("%s", answer);
            if (strcmp(answer, "Y\n") == 0 || strcmp(answer, "\n") == 0)
            {
                // want to hu;
                sprintf(sendline, "YES!\n");
                write(fd, sendline, strlen(sendline));
                memset(sendline, 0, strlen(sendline));
                break;
            }
            else if (strcmp(answer, "n\n") == 0)
            {
                // don't want to hu;
                sprintf(sendline, "NO!\n");
                write(fd, sendline, strlen(sendline));
                memset(sendline, 0, strlen(sendline));
                break;
            }
            else
            {
                // unexpected chars received.
                // back into the loop until correct response is given.
                memset(answer, 0, strlen(answer));
            }
        }
    }
    else
    {
        // cannot hu, just continue the game;
    }
    memset(recvline, 0, strlen(recvline));
    return 0;
}

int client_discard() {
    printf("Choose a mj and discard it...!\n");
    int index;
    scanf("%d", &index);
    discarded_mj = decks[index];

    if (decks[index].type == TONG)
    {
        printf("You chose to discard %d TONG.\n", decks[index].number);
    }
    else if (decks[index].type == TIAO)
    {
        printf("You chose to discard %d TIAO.\n", decks[index].number);
    }
    else if (decks[index].type == WAN)
    {
        printf("You chose to discard %d WAN.\n", decks[index].number);
    }
    else if (decks[index].type == DAZI)
    {
        if (decks[index].number == 1)
        {
            printf("You chose to discard EAST.\n");
        }
        else if (decks[index].number == 2)
        {
            printf("You chose to discard SOUTH.\n");
        }
        else if (decks[index].number == 3)
        {
            printf("You chose to discard WEST.\n");
        }
        else if (decks[index].number == 4)
        {
            printf("You chose to discard NORTH.\n");
        }
        else if (decks[index].number == 5)
        {
            printf("You chose to discard ZHONG.\n");
        }
        else if (decks[index].number == 6)
        {
            printf("You chose to discard FA.\n");
        }
        else if (decks[index].number == 7)
        {
            printf("You chose to discard BAI.\n");
        }
    }

    sprintf(sendline, "%d", index);
    write(fd, sendline, strlen(sendline));
    memset(sendline, 0, strlen(sendline));

    swap(&decks[16], &decks[index]);
    decks[16].type = 0;
    decks[16].number = 0;
    decks_quick_sort(decks, 0, 15);
    return 0;
}

int get_result() {
    printf("%s", recvline);
    memset(recvline, 0, strlen(recvline));

    char answer[64];
    for (;;)
    {
        scanf("%s", answer);
        if (strcmp(answer, "Y\n") == 0 || strcmp(answer, "\n") == 0)
        {
            // want to pong;
            sprintf(sendline, "YES!\n");
            write(fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));
            return 1;
        }
        else if (strcmp(answer, "n\n") == 0)
        {
            // don't want to pong;
            sprintf(sendline, "NO!\n");
            write(fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));
            return 0;
        }
        else
        {
            // unexpected chars received.
            // back into the loop until correct response is given.
            memset(answer, 0, strlen(answer));
        }
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
        get_decks();
        for (;;)
        {
            if (read(fd, recvline, MAXLINE) <= 0)
            {
                printf("Server terminated prematurely!? Exiting...\n");
                exit(1);
            }
            else
            {
                if (strcmp(recvline, "your turn\n") == 0)
                {
                    memset(recvline, 0, strlen(recvline));
                    // yeah it's your turn;
                    client_draw();
                    client_is_hu();
                    client_discard();
                    continue;
                }
                else if (strncmp(recvline, "player", 6) == 0)
                {
                    // someone discarded out something
                    printf("%s", recvline);

                    // record the discarded mj
                    if ('A' <= recvline[19] && recvline[19] <= 'Z')
                    {
                        // this is a DAZI;
                        discarded_mj.type = DAZI;
                        if (recvline[19] == 'E')
                        {
                            discarded_mj.number = 1;
                        }
                        else if (recvline[19] == 'S')
                        {
                            discarded_mj.number = 2;
                        }
                        else if (recvline[19] == 'W')
                        {
                            discarded_mj.number = 3;
                        }
                        else if (recvline[19] == 'N')
                        {
                            discarded_mj.number = 4;
                        }
                        else if (recvline[19] == 'Z')
                        {
                            discarded_mj.number = 5;
                        }
                        else if (recvline[19] == 'F')
                        {
                            discarded_mj.number = 6;
                        }
                        else if (recvline[19] == 'B')
                        {
                            discarded_mj.number = 7;
                        }
                    }
                    else
                    {
                        discarded_mj.number = recvline[19] - '0';
                        if (recvline[21] == 'W')
                        {
                            discarded_mj.type = WAN;
                        }
                        else
                        {
                            if (recvline[22] == 'O')
                            {
                                discarded_mj.type == TONG;
                            }
                            else
                            {
                                discarded_mj.type == TIAO;
                            }
                        }
                    }
                    memset(recvline, 0, strlen(recvline));
                }
                else if (strncmp(recvline, "(Announce) ", 11) == 0)
                {
                    // this is an announcement, just print it onto stdout;
                    printf("%s", recvline);
                    memset(recvline, 0, strlen(recvline));
                }
                else if (strcmp(recvline, "You can pong.\n") == 0)
                {
                    memset(recvline, 0, strlen(recvline));
                    // yeah someone discarded something that you can pong;

                    char answer[64];
                    for (;;)
                    {
                        printf("You can pong, proceed? [Y/n]\n");
                        scanf("%s", answer);
                        if (strcmp(answer, "Y\n") == 0 || strcmp(answer, "\n") == 0)
                        {
                            // want to pong;
                            sprintf(sendline, "YES!\n");
                            write(fd, sendline, strlen(sendline));
                            memset(sendline, 0, strlen(sendline));

                            decks[16] = discarded_mj;

                            client_is_hu();
                            client_discard();
                            break;
                        }
                        else if (strcmp(answer, "n\n") == 0)
                        {
                            // don't want to pong;
                            sprintf(sendline, "NO!\n");
                            write(fd, sendline, strlen(sendline));
                            memset(sendline, 0, strlen(sendline));
                            break;
                        }
                        else
                        {
                            // unexpected chars received.
                            // back into the loop until correct response is given.
                            memset(answer, 0, strlen(answer));
                        }
                    }
                }
                else if (strcmp(recvline, "You can eat.\n") == 0)
                {
                    memset(recvline, 0, strlen(recvline));
                    // yeah someone discarded something that you can eat;

                    char answer[64];
                    for (;;)
                    {
                        printf("You can eat, proceed? [Y/n]\n");
                        scanf("%s", answer);
                        if (strcmp(answer, "Y\n") == 0 || strcmp(answer, "\n") == 0)
                        {
                            // want to eat;
                            sprintf(sendline, "YES!\n");
                            write(fd, sendline, strlen(sendline));
                            memset(sendline, 0, strlen(sendline));

                            decks[16] = discarded_mj;

                            client_is_hu();
                            client_discard();
                            break;
                        }
                        else if (strcmp(answer, "n\n") == 0)
                        {
                            // don't want to eat;
                            sprintf(sendline, "NO!\n");
                            write(fd, sendline, strlen(sendline));
                            memset(sendline, 0, strlen(sendline));
                            break;
                        }
                        else
                        {
                            // unexpected chars received.
                            // back into the loop until correct response is given.
                            memset(answer, 0, strlen(answer));
                        }
                    }
                }
                else if (strcmp(recvline, "no one wants it.\n") == 0)
                {
                    sea[sea_index++] = discarded_mj;
                    discarded_mj.type = 0;
                    discarded_mj.number = 0;
                }
                else if (strncmp(recvline, "(Game) ", 7) == 0)
                {
                    break;
                }
            }
        }
        if(get_result() == 1)
        {
            // continue the game
        }
        else 
        {
            // stop the game
            break;
        }
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