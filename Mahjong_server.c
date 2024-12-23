/*
    This is a server that plays the role as an virtual online mahjong table.

    Sadly, 連莊、
    is not implemented in this code yet.

    The server automatically starts with the first player, and will start with the next player in the next round, regardless of the result from the first game.

    After four rounds, the game is over and final scores (if implemented QQ) will be calculated and displayed.

*/

#include "unp.h"

// the following variables are for connection purposes;
int listenfd;
struct sockaddr_in serverinfo;
socklen_t serverlen;
pid_t childpid;
char sendline[MAXLINE] = {0}, recvline[MAXLINE] = {0};
fd_set rset, testset;
int maxfd = 0;
struct timeval timeout;
void sig_chld(int signo) {
    pid_t pid;
    int stat;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {
        printf("child %d terminated\n", pid);
    }
    return;
}

// the following variables are for game purposes;

struct mj {
    int type;
    int number;
    int priority;
}; // all the numbers here should be 1-indexed;

const int NO_TYPE = 0; // serves as null mj
const int TONG = 1;
const int TIAO = 2;
const int WAN = 3;
const int DAZI = 4;   // it goes with 東南西北中發白;
const int FLOWER = 5; // it goes with 春夏秋冬梅蘭竹菊;

int winner = -1; // will be the index of the winner, or -1 before the game has set, or -2 if 流局。

struct mj shuffled_mjs[144], discarded_mj;

int take_index = 0;

struct player {
    int fd, flower_index, door_index, sea_index;
    struct sockaddr_in info;
    socklen_t len;
    struct mj decks[20], flowers[8], door[20], sea[150];
} *players[4];

struct player *player_init() {
    struct player *p = malloc(sizeof(struct player));
    if (!p)
    {
        printf("malloc failed, exiting...\n");
        exit(1);
    }
    memset(p, 0, sizeof(struct player));
    p->fd = -1;
    p->flower_index = 0;
    p->door_index = 0;
    p->sea_index = 0;
    p->len = sizeof(struct sockaddr_in);
    return p;
}

int connection_establish() {
    // set up serverinfo and listenfd
    memset(&serverinfo, 0, sizeof(serverinfo));
    serverinfo.sin_family = AF_INET;
    serverinfo.sin_addr.s_addr = htonl(INADDR_ANY);
    serverinfo.sin_port = htons(SERV_PORT + 10);
    serverlen = sizeof(serverinfo);
    FD_ZERO(&rset);
    timeout.tv_sec = 1; // set the timeout to 1 second;

    // enable server
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("ERROR OCCURED IN socket(), exiting...\n");
        exit(1);
    }

    if ((bind(listenfd, (SA *)&serverinfo, serverlen)) < 0)
    {
        printf("ERROR OCCURED IN bind(), errno = %d, exiting...\n", errno);
        exit(1);
    }

    if ((listen(listenfd, LISTENQ)) < 0)
    {
        printf("ERROR OCCURED IN listen(), exiting...\n");
        exit(1);
    }

    printf("Server listening on port %d...\n", SERV_PORT + 10);

    // wait until 4 stable connection is present
    for (;;)
    {
        int connection_count = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (players[i] && players[i]->fd == -1)
            {
                if ((players[i]->fd = accept(listenfd, (SA *)&(players[i]->info), &(players[i]->len))) < 0)
                {
                    if (errno == EINTR)
                    {
                        continue; // Retry on interrupt
                    }
                    else
                    {
                        perror("accept");
                        exit(1);
                    }
                }

                // Player successfully connected
                printf("A client is connected!\n");
                FD_SET(players[i]->fd, &rset);
                if (players[i]->fd > maxfd)
                {
                    maxfd = players[i]->fd;
                }
            }

            // Increment connection count for each player slot processed
            connection_count++;
        }
        testset = rset;
        select(maxfd + 1, &testset, NULL, NULL, &timeout);
        for (int i = 0; i < 4; ++i)
        {
            if (FD_ISSET(players[i]->fd, &testset))
            {
                if (read(players[i]->fd, recvline, MAXLINE) == 0)
                {
                    // this client just diconnected;
                    printf("A client just disconnected!\n");
                    connection_count--;
                    close(players[i]->fd);
                    free(players[i]);
                    players[i] = player_init();
                }
            }
        }
        if (connection_count == 4)
        {
            // ready to go!!! send start signal;
            sprintf(sendline, "start!\n");
            for (int i = 0; i < 4; ++i)
            {
                write(players[i]->fd, sendline, strlen(sendline));
            }
            memset(sendline, 0, strlen(sendline));
            break;
        }
        else
        {
            sprintf(sendline, "wait...\n");
            for (int i = 0; i < 4; ++i)
            {
                write(players[i]->fd, sendline, strlen(sendline));
            }
            memset(sendline, 0, strlen(sendline));
        }
    }
    return 0;
}

void swap(struct mj *a, struct mj *b) {
    struct mj temp = *a;
    *a = *b;
    *b = temp;
    return;
}

void priority_quick_sort(struct mj *mjs, int start, int end) {
    if (start >= end)
    {
        return; // Base case: single element or invalid range
    }

    int pivotnum = mjs[start].priority;
    int left = start + 1, right = end;

    while (left <= right)
    {
        // Move `left` forward while it's less than or equal to the pivot
        while (left <= right && mjs[left].priority <= pivotnum)
        {
            left++;
        }
        // Move `right` backward while it's greater than or equal to the pivot
        while (left <= right && mjs[right].priority >= pivotnum)
        {
            right--;
        }

        // Swap elements if `left` is less than `right`
        if (left < right)
        {
            swap(&mjs[left], &mjs[right]);
        }
    }

    // Place the pivot in the correct position
    swap(&mjs[start], &mjs[right]);

    // Recursively sort elements before and after the pivot
    priority_quick_sort(mjs, start, right - 1);
    priority_quick_sort(mjs, right + 1, end);
    return;
}

int shuffle() {
    int nowindex = 0;

    // TONG first
    for (int i = 1; i <= 9; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            shuffled_mjs[nowindex].type = TONG;
            shuffled_mjs[nowindex].number = i;
            shuffled_mjs[nowindex].priority = rand();
            nowindex++;
        }
    }

    // TIAO
    for (int i = 1; i <= 9; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            shuffled_mjs[nowindex].type = TIAO;
            shuffled_mjs[nowindex].number = i;
            shuffled_mjs[nowindex].priority = rand();
            nowindex++;
        }
    }

    // WAN
    for (int i = 1; i <= 9; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            shuffled_mjs[nowindex].type = WAN;
            shuffled_mjs[nowindex].number = i;
            shuffled_mjs[nowindex].priority = rand();
            nowindex++;
        }
    }

    // DAZI
    for (int i = 1; i <= 7; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            shuffled_mjs[nowindex].type = DAZI;
            shuffled_mjs[nowindex].number = i;
            shuffled_mjs[nowindex].priority = rand();
            nowindex++;
        }
    }

    // FLOWER
    for (int i = 1; i <= 8; ++i)
    {
        shuffled_mjs[nowindex].type = FLOWER;
        shuffled_mjs[nowindex].number = i;
        shuffled_mjs[nowindex].priority = rand();
        nowindex++;
    }

    priority_quick_sort(shuffled_mjs, 0, 143);
    return 0;
}

int has_flower(struct player *p) {
    int flowercount = 0;
    for (int i = 0; i < 20; ++i)
    {
        if (p->decks[i].type == FLOWER)
        {
            flowercount++;
            p->flowers[p->flower_index++] = p->decks[i];
            p->decks[i] = shuffled_mjs[take_index++];
        }
    }
    return flowercount;
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

int deal(int playernow) {
    for (int time = 0; time < 4; ++time)
    {
        for (int playeradd = 0; playeradd < 4; ++playeradd)
        {
            for (int grab = 0; grab < 4; ++grab)
            {
                players[(playernow + playeradd) % 4]->decks[time * 4 + grab] = shuffled_mjs[take_index++];
            }
        }
    }

    // for the simplicity of implementation, we will not be "opening the door" here（開門）.
    // instead, all players will be having 16 mjs at first.

    // flower-regrab
    int no_flower_count = 0, playeradd = 0;
    while (no_flower_count < 4)
    {
        if (has_flower(players[(playernow + playeradd) % 4]) > 0)
        {
            no_flower_count = 0;
        }
        else
        {
            no_flower_count++;
        }
        playeradd++;
    }

    // test the deal
    decks_quick_sort(players[0]->decks, 0, 15);
    decks_quick_sort(players[1]->decks, 0, 15);
    decks_quick_sort(players[2]->decks, 0, 15);
    decks_quick_sort(players[3]->decks, 0, 15);

    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 16; ++j)
        {
            sprintf(sendline, "%d %d\n", players[i]->decks[j].type, players[i]->decks[j].number); // i-th mj, TYPE, NUMBER.
            write(players[i]->fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));

            if (read(players[i]->fd, recvline, MAXLINE) <= 0)
            {
                printf("ERROR OCCURED when transferring decks. exiting...\n");
                exit(1);
            }
            else if (strcmp(recvline, "ACK\n") == 0)
            {
                memset(recvline, 0, strlen(recvline));
            }
        }
        for (int j = 0; j < players[i]->flower_index; ++j)
        {
            sprintf(sendline, "%d\n", players[i]->flowers[j].number); // i-th mj, TYPE, NUMBER.
            write(players[i]->fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));

            if (read(players[i]->fd, recvline, MAXLINE) <= 0)
            {
                printf("ERROR OCCURED when transferring flowers. exiting...\n");
                exit(1);
            }
            else if (strcmp(recvline, "ACK\n") == 0)
            {
                memset(recvline, 0, strlen(recvline));
            }
        }
        sprintf(sendline, "transfer complete\n");
        write(players[i]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));
    }
    return 0;
}

int shuffle_n_deal(int playernow) {
    shuffle();
    deal(playernow);
    return 0;
}

int mj_compare(struct mj a, struct mj b) {
    // if a > b => return -1
    // if a = b => return 0
    // if a < b => return 1

    if (a.type == b.type && a.number == b.number)
    {
        return 0;
    }
    if (a.type > b.type || (a.type == b.type && a.number > b.number))
    {
        return -1;
    }
    return 1;
}

int hu_recursive_check(int *count, int n) {
    if (n == 0)
    {
        return 1;
    }

    for (int i = 0; i < 34; ++i)
    {
        // if there is 順子
        if (count[i] >= 1 &&
            i + 1 < 34 && count[i + 1] >= 1 &&
            i + 2 < 34 && count[i + 2] >= 1 &&
            i / 9 == (i + 1) / 9 && i / 9 == (i + 2) / 9)
        {
            count[i]--;
            count[i + 1]--;
            count[i + 2]--;
            if (hu_recursive_check(count, n - 3) == 1)
            {
                return 1;
            }
            count[i]++;
            count[i + 1]++;
            count[i + 2]++;
        }

        // if there is 刻子
        if (count[i] >= 3)
        {
            count[i] -= 3;
            if (hu_recursive_check(count, n - 3) == 1)
            {
                return 1;
            }
            count[i] += 3;
        }
    }
    return 0;
}

int hu_check(struct mj *decks) {
    // hu_check can sort the decks for its own purpose, but shouldn't modify the real deck;
    struct mj carbon[17];
    memset(carbon, 0, 17*sizeof(struct mj));
    memcpy(carbon, decks, 17);

    struct mj mj_gotten = carbon[16];
    int insertindex = 15;
    while (mj_compare(carbon[insertindex], mj_gotten) == -1)
    {
        carbon[insertindex + 1] = carbon[insertindex];
        insertindex--;
        if (insertindex == -1)
        {
            break;
        }
    }

    insertindex++;
    carbon[insertindex] = mj_gotten;

    for (int i = 0; i < 17; ++i)
    {
        if (i + 1 < 17 && mj_compare(carbon[i], carbon[i + 1]) == 0)
        {
            // there exist a 一對 in this deck;
            int count[34]; // because there are totally 34 kinds of mjs in total (excluding FLOWER).
            memset(count, 0, 34*sizeof(int));
            for (int j = 0; j < 17; ++j)
            {
                if (j == i || j == i + 1)
                {
                    continue;
                }
                count[(carbon[j].type - 1) * 9 + carbon[j].number]++;
            }

            if (hu_recursive_check(count, 15) == 1)
            {
                // this deck can hu!
                return 1;
            }
        }
    }
    return 0;
}

int draw(int playernow) {
    players[playernow]->decks[16] = shuffled_mjs[take_index++];

    sprintf(sendline, "%d %d\n", players[playernow]->decks[16].type, players[playernow]->decks[16].number);
    write(players[playernow]->fd, sendline, strlen(sendline));
    memset(sendline, 0, strlen(sendline));

    while (players[playernow]->decks[16].type == FLOWER)
    {
        players[playernow]->flowers[players[playernow]->flower_index++] = players[playernow]->decks[16];
        players[playernow]->decks[16] = shuffled_mjs[take_index++];

        sprintf(sendline, "%d %d\n", players[playernow]->decks[16].type, players[playernow]->decks[16].number);
        write(players[playernow]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));
    }
    return 0;
}

int is_hu(int playernow) {
    if (hu_check(players[playernow]->decks) == 1)
    {
        sprintf(sendline, "can hu\n");
        write(players[playernow]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));

        read(players[playernow]->fd, recvline, MAXLINE);
        if (strcmp(recvline, "YES!\n") == 0)
        {
            // we have a winner here!
            winner = playernow;
            memset(recvline, 0, strlen(recvline));
            return 1;
        }
        else
        {
            // do nothing, just let the game continue;
        }
        memset(recvline, 0, strlen(recvline));
    }
    else
    {
        sprintf(sendline, "cannot hu\n");
        write(players[playernow]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));
    }
    return 0;
}

int discard(int playernow) {
    read(players[playernow]->fd, recvline, MAXLINE);
    int index;
    sscanf(recvline, "%d", &index);
    memset(read, 0, strlen(recvline));

    discarded_mj.type = players[playernow]->decks[index].type;
    discarded_mj.number = players[playernow]->decks[index].number;
    swap(&players[playernow]->decks[16], &players[playernow]->decks[index]);
    players[playernow]->decks[16].type = 0;
    players[playernow]->decks[16].number = 0;
    decks_quick_sort(players[playernow]->decks, 0, 15);

    if (discarded_mj.type == TONG)
    {
        sprintf(sendline, "player %d discarded %d TONG.\n", playernow, discarded_mj.number);
    }
    else if (discarded_mj.type == TIAO)
    {
        sprintf(sendline, "player %d discarded %d TIAO.\n", playernow, discarded_mj.number);
    }
    else if (discarded_mj.type == WAN)
    {
        sprintf(sendline, "player %d discarded %d WAN.\n", playernow, discarded_mj.number);
    }
    else if (discarded_mj.type == DAZI)
    {
        if (discarded_mj.number == 1)
        {
            sprintf(sendline, "player %d discarded EAST.\n", playernow);
        }
        else if (discarded_mj.number == 2)
        {
            sprintf(sendline, "player %d discarded SOUTH.\n", playernow);
        }
        else if (discarded_mj.number == 3)
        {
            sprintf(sendline, "player %d discarded WEST.\n", playernow);
        }
        else if (discarded_mj.number == 4)
        {
            sprintf(sendline, "player %d discarded NORTH.\n", playernow);
        }
        else if (discarded_mj.number == 5)
        {
            sprintf(sendline, "player %d discarded ZHONG.\n", playernow);
        }
        else if (discarded_mj.number == 6)
        {
            sprintf(sendline, "player %d discarded FA.\n", playernow);
        }
        else if (discarded_mj.number == 7)
        {
            sprintf(sendline, "player %d discarded BAI.\n", playernow);
        }
    }

    write(players[(playernow + 1) % 4]->fd, sendline, strlen(sendline));
    write(players[(playernow + 2) % 4]->fd, sendline, strlen(sendline));
    write(players[(playernow + 3) % 4]->fd, sendline, strlen(sendline));
    memset(sendline, 0, strlen(sendline));
    return 0;
}

int draw_n_discard(int playernow) {
    sprintf(sendline, "your turn\n");
    write(players[playernow]->fd, sendline, strlen(sendline));
    memset(sendline, 0, strlen(sendline));

    draw(playernow);
    if(is_hu(playernow) == 1)
    {
        return 1;
    }
    discard(playernow);
    return 0;
}

int is_pong_possible(struct mj *deck) {
    int count[34]; // because there are totally 34 kinds of mjs in total (excluding FLOWER).
    memset(count, 0, 34*sizeof(int));
    for (int j = 0; j < 16; ++j)
    {
        count[(deck[j].type - 1) * 9 + deck[j].number]++;
    }

    if (count[(discarded_mj.type - 1) * 9 + discarded_mj.number] >= 2)
    {
        // this deck can pong!
        return 1;
    }
    return 0;
}

int is_eat_possible(struct mj *deck) {
    if (discarded_mj.type == DAZI)
    {
        // of course you cant eat DAZI;
        return 0;
    }

    int count[34]; // because there are totally 34 kinds of mjs in total (excluding FLOWER).
    memset(count, 0, 34*sizeof(int));
    for (int j = 0; j < 16; ++j)
    {
        count[(deck[j].type - 1) * 9 + deck[j].number]++;
    }

    if (count[discarded_mj.number] >= 1 &&
        discarded_mj.number + 1 < 34 && count[discarded_mj.number + 1] >= 1 &&
        discarded_mj.number + 2 < 34 && count[discarded_mj.number + 2] >= 1 &&
        discarded_mj.number / 9 == (discarded_mj.number + 1) / 9 &&
        discarded_mj.number / 9 == (discarded_mj.number + 2) / 9)
    {
        // this deck can eat!
        return 1;
    }
    else if (count[discarded_mj.number] >= 1 &&
             discarded_mj.number + 1 < 34 && count[discarded_mj.number + 1] >= 1 &&
             discarded_mj.number - 1 < 34 && count[discarded_mj.number - 1] >= 1 &&
             discarded_mj.number / 9 == (discarded_mj.number + 1) / 9 &&
             discarded_mj.number / 9 == (discarded_mj.number - 1) / 9)
    {
        // this deck can eat!
        return 1;
    }
    else if (count[discarded_mj.number] >= 1 &&
             discarded_mj.number - 2 < 34 && count[discarded_mj.number - 2] >= 1 &&
             discarded_mj.number - 1 < 34 && count[discarded_mj.number - 1] >= 1 &&
             discarded_mj.number / 9 == (discarded_mj.number - 2) / 9 &&
             discarded_mj.number / 9 == (discarded_mj.number - 1) / 9)
    {
        // this deck can eat!
        return 1;
    }
    return 0;
}

int othersreaction(int *playernowp) {
    // only broadcast the discarded mj when other players
    // can really have some reaction to it.
    // otherwise put this discarded mj to the sea of its original player;

    // Case 1: others may be able to 碰
    if (is_pong_possible(players[*playernowp + 1]->decks) == 1)
    {
        sprintf(sendline, "You can pong.\n");
        write(players[*playernowp + 1]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));

        read(players[*playernowp + 1]->fd, recvline, MAXLINE);
        if (strcmp(recvline, "YES!\n") == 0)
        {
            memset(recvline, 0, strlen(recvline));

            *playernowp = *playernowp + 1;
            players[*playernowp]->decks[16] = discarded_mj;

            sprintf(sendline, "(Announce) player %d ponged it\n", *playernowp);
            write(players[*playernowp]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 1]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 2]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 3]->fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));

            is_hu(*playernowp);
            discard(*playernowp);
            othersreaction(playernowp);
        }
        else
        {
            // the player don't want to pong
            memset(recvline, 0, strlen(recvline));
        }
    }
    else if (is_pong_possible(players[*playernowp + 2]->decks) == 1)
    {
        sprintf(sendline, "You can pong.\n");
        write(players[*playernowp + 2]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));

        read(players[*playernowp + 2]->fd, recvline, MAXLINE);
        if (strcmp(recvline, "YES!\n") == 0)
        {
            memset(recvline, 0, strlen(recvline));
            *playernowp = *playernowp + 2;
            players[*playernowp]->decks[16] = discarded_mj;

            sprintf(sendline, "(Announce) player %d ponged it\n", *playernowp);
            write(players[*playernowp]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 1]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 2]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 3]->fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));

            is_hu(*playernowp);
            discard(*playernowp);
            othersreaction(playernowp);
        }
        else
        {
            // the player don't want to pong
            memset(recvline, 0, strlen(recvline));
        }
    }
    else if (is_pong_possible(players[*playernowp + 3]->decks) == 1)
    {
        sprintf(sendline, "You can pong.\n");
        write(players[*playernowp + 3]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));

        read(players[*playernowp + 3]->fd, recvline, MAXLINE);
        if (strcmp(recvline, "YES!\n") == 0)
        {
            memset(recvline, 0, strlen(recvline));
            *playernowp = *playernowp + 3;
            players[*playernowp]->decks[16] = discarded_mj;

            sprintf(sendline, "(Announce) player %d ponged it\n", *playernowp);
            write(players[*playernowp]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 1]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 2]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 3]->fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));

            is_hu(*playernowp);
            discard(*playernowp);
            othersreaction(playernowp);
        }
        else
        {
            // the player don't want to pong
            memset(recvline, 0, strlen(recvline));
        }
    }
    // Case 2: 下家 may be able to 吃
    else if (is_eat_possible(players[*playernowp + 1]->decks) == 1)
    {
        sprintf(sendline, "You can pong.\n");
        write(players[*playernowp + 1]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));

        read(players[*playernowp + 1]->fd, recvline, MAXLINE);
        if (strcmp(recvline, "YES!\n") == 0)
        {
            memset(recvline, 0, strlen(recvline));
            *playernowp = *playernowp + 1;
            players[*playernowp]->decks[16] = discarded_mj;

            sprintf(sendline, "(Announce) player %d ate it\n", *playernowp);
            write(players[*playernowp]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 1]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 2]->fd, sendline, strlen(sendline));
            write(players[*playernowp + 3]->fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));

            is_hu(*playernowp);
            discard(*playernowp);
            othersreaction(playernowp);
        }
        else
        {
            // the player don't want to eat
            memset(recvline, 0, strlen(recvline));
        }
    }
    // Case 3: no players can do anything
    else
    {
        sprintf(sendline, "no one wants it.\n");
        write(players[*playernowp]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));

        players[*playernowp]->sea[players[*playernowp]->sea_index++] = discarded_mj;
        discarded_mj.type = 0;
        discarded_mj.number = 0;
    }
    return 0;
}

int game_set_display(){
    sprintf(sendline, "(Game) The game is set and we have a winner: %d!!!\nContinue for next round? [Y/n]", winner);
    write(players[0]->fd, sendline, strlen(sendline));
    write(players[1]->fd, sendline, strlen(sendline));
    write(players[2]->fd, sendline, strlen(sendline));
    write(players[3]->fd, sendline, strlen(sendline));
    memset(sendline, 0, strlen(sendline));

    // wait until everyone said yes
    // if so, return 1 to restart the game (get back in the loop)
    // otherwise, return 0 to stop the game

    return 0;
}

int game() {
    // let the players know which number they are
    for (int i = 0; i < 4; ++i)
    {
        sprintf(sendline, "%d-th\n", i);
        write(players[i]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));
    }
    for (int startplayer = 0; startplayer < 4; ++startplayer)
    {
        int playernow = startplayer;
        shuffle_n_deal(playernow);
        for (; winner == -1; playernow++, playernow %= 4)
        {
            if(draw_n_discard(playernow) == 1)
            {
                break;
            }
            othersreaction(&playernow); // note that this is a value_result argument. just think about it and you will know why we do this.
        }
        // the game has set
        if(game_set_display() == 1)
        {
            // continue the game;
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
    signal(SIGCHLD, sig_chld);
    srand(time(NULL));
    for (int i = 0; i < 4; ++i)
    {
        players[i] = player_init();
    }

    while (connection_establish() == 0)
    {
        if ((childpid = fork()) == 0)
        {
            // if the code reaches here,
            // this is the child;
            close(listenfd);
            game();
            exit(0);
        }
        for (int i = 0; i < 4; ++i)
        {
            close(players[i]->fd);
            free(players[i]);
        }
    }
    return 0;
}

// we havent implemented the 連莊、算台數 functionalities yet.