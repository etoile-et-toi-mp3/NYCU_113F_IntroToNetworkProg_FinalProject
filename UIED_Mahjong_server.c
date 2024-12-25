/*
    This is a server that plays the role as an virtual online mahjong table.

    Sadly, 連莊、
    is not implemented in this code yet.

    The server automatically starts with the first player, and will start with the next player in the next round, regardless of the result from the first game.

    After four rounds, the game is over and final scores (if implemented QQ) will be calculated and displayed.

*/

#include "unp.h"
#include <stdarg.h>

// the following variables are for connection purposes;
int listenfd = -1;
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
typedef struct mj mj;

const int NO_TYPE = 0; // serves as null mj
const int TONG = 1;
const int TIAO = 2;
const int WAN = 3;
const int DAZI = 4;   // it goes with 東南西北中發白;
const int FLOWER = 5; // it goes with 春夏秋冬梅蘭竹菊;

// the following variables are for cards;
char number[10][10] = {"NULL", "一", "二", "三", "四", "五", "六", "七", "八", "九"};
char wind_number[8][10] = {"NULL", "東", "南", "西", "北", "中", "發", "白"};
char type[10][10] = {"NULL", "筒", "條", "萬", "風", "花"};

int winner = -1; // will be the index of the winner, or -1 before the game has set, or -2 if 流局。

struct mj shuffled_mjs[144], discarded_mj;

int take_index = 0;

struct player {
    int fd, flower_index, door_index, sea_index, normal_capacity;
    struct sockaddr_in info;
    socklen_t len;
    struct mj decks[20], flowers[8], door[20], sea[150];
} *pre_players[4], *players[4];
// two player arrays can ensure that listening server
// won't free the players too early s.t. child process
// couldn't access anything.

// initialize player's in-game info
int player_gameinfo_init(struct player *p) {
    p->flower_index = 0;
    p->door_index = 0;
    p->sea_index = 0;
    p->normal_capacity = 16;
    return 0;
}

// initialize player
struct player *player_init() {
    struct player *p = malloc(sizeof(struct player));
    if (!p)
    {
        printf("malloc failed, exiting...\n");
        exit(1);
    }
    memset(p, 0, sizeof(struct player));
    p->len = sizeof(struct sockaddr_in);
    p->fd = -1;
    player_gameinfo_init(p);
    return p;
}

int write_message_wait_ack(int fd, const char *format, ...) {
    // construct the content in sendline
    va_list args;
    va_start(args, format);
    vsnprintf(sendline, MAXLINE, format, args);
    va_end(args);

    // write message to the client
    write(fd, sendline, strlen(sendline));
    printf("Sending: %s", sendline);
    memset(sendline, 0, strlen(sendline));

    // wait ACK from the client
    int n = read(fd, recvline, MAXLINE);
    if (n > 0)
    {
        if (strncmp(recvline, "ACK\n", 4) == 0)
        {
#ifdef DEBUG
            printf("ACK Received\n");
#endif
            memset(recvline, 0, strlen(recvline));
            return 0;
        }
        else
        {
            fprintf(stdout, "Unexpected ACK message: %s\n", recvline);
            exit(1);
        }
    }
    else
    {
        perror("Failed to read ACK");
        exit(1);
    }
    return 1;
}

int read_and_ack(int fd) { // no memset yet, do it yourself
    int n = read(fd, recvline, MAXLINE);
    if (n > 0)
    {
#ifdef DEBUG
        printf("Received: %s\n", recvline);
#endif
        // Send the ACK
        write(fd, "ACK\n", 4);

#ifdef DEBUG
        printf("ACK sent.\n");
#endif
        return 0;
    }
    else
    {
        perror("Failed to receive message");
        exit(1);
    }
    return 1;
}

// initialize connection
int connection_preparation() {
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
        printf("ERROR OCCURRED IN socket(), exiting...\n");
        exit(1);
    }

    if ((bind(listenfd, (SA *)&serverinfo, serverlen)) < 0)
    {
        printf("ERROR OCCURRED IN bind(), errno = %d, PID = %d, exiting...\n", errno, getpid());
        exit(1);
    }

    if ((listen(listenfd, LISTENQ)) < 0)
    {
        printf("ERROR OCCURED IN listen(), exiting...\n");
        exit(1);
    }

    printf("Server listening on port %d...\n", SERV_PORT + 10);
    return 0;
}

// establish connection with 4 players
int connection_establish() {
    // wait until 4 stable connection is present
    for (;;)
    {
        int connection_count = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (pre_players[i] && pre_players[i]->fd == -1)
            {
                if ((pre_players[i]->fd = accept(listenfd, (SA *)&(pre_players[i]->info), &(pre_players[i]->len))) < 0)
                {
                    if (errno == EINTR)
                    {
                        continue; // Retry on interrupt
                    }
                    else
                    {
                        perror("accept error\n");
                        exit(1);
                    }
                }

                // Player successfully connected
                printf("A client is connected!\n");
                FD_SET(pre_players[i]->fd, &rset);
                if (pre_players[i]->fd > maxfd)
                {
                    maxfd = pre_players[i]->fd;
                }
            }

            // Increment connection count for each player slot processed
            connection_count++;
        }
        testset = rset;
        select(maxfd + 1, &testset, NULL, NULL, &timeout);
        for (int i = 0; i < 4; ++i)
        {
            if (FD_ISSET(pre_players[i]->fd, &testset))
            {
                if (read(pre_players[i]->fd, recvline, MAXLINE) == 0)
                {
                    // this client just diconnected;
                    printf("A client just disconnected!\n");
                    connection_count--;
                    close(pre_players[i]->fd);
                    free(pre_players[i]);
                    pre_players[i] = player_init();
                }
            }
        }
        if (connection_count == 4)
        {
            // ready to go!!! send start signal;
            sprintf(sendline, "start!\n");
            for (int i = 0; i < 4; ++i)
            {
                write(pre_players[i]->fd, sendline, strlen(sendline));
            }
            memset(sendline, 0, strlen(sendline));
            break;
        }
        else
        {
            sprintf(sendline, "wait...\n");
            for (int i = 0; i < 4; ++i)
            {
                write(pre_players[i]->fd, sendline, strlen(sendline));
            }
            memset(sendline, 0, strlen(sendline));
        }
    }
    return 0;
}

// swap function
void swap(struct mj *a, struct mj *b) {
    struct mj temp = *a;
    *a = *b;
    *b = temp;
    return;
}

//  堆牌山
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

// 洗牌
int shuffle() {
    memset(shuffled_mjs, 0, 144 * sizeof(struct mj));
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

// 花牌的處理
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

// 理牌
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

// 發牌、花牌補花、理牌
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
            else if (strncmp(recvline, "ACK\n", 4) == 0)
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
            else if (strncmp(recvline, "ACK\n", 4) == 0)
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

// 印出手牌
void print_deck(mj *hands, mj *doors, mj last, int hu, int you_to_play) {
    // if last is played by other
    if (hu == 0 && last.type != 0)
    {
        printf("other player played: \n");
        printf("___\n");
        if (last.type == 4)
        {
            printf("|%s|\n", wind_number[last.number]);
        }
        else
            printf("|%s|\n", number[last.number]);
        if (last.type == 4 && last.number > 4)
        {
            printf("|  |\n");
        }
        else
            printf("|%s|\n", type[last.type]);
        printf("‾‾‾\n");
    }
    if (hu == 1)
        printf("You are really good at this game! \nresult:\n");
    else
        printf("Your decks: \n");
    // print index
    if (you_to_play)
    {
        for (int i = 0; i < 20; ++i)
        {
            if (hands[i].type == 0 && hands[i].number == 0)
            {
                break;
            }
            if (i >= 10)
                printf(" %d", i);
            else
                printf(" 0%d", i);
        }
        printf("\n");
    }

    for (int i = 0; i < 20; ++i)
    {
        if (hands[i].type == 0 && hands[i].number == 0)
        {
            break;
        }
        printf("___");
    }
    printf("_\t");
    if (hu && last.type != 0)
    {
        printf("___");
    }
    printf("\t\t");
    for (int i = 0; i < 20; ++i)
    {
        if (doors[i].type == 0 && doors[i].number == 0)
        {
            break;
        }
        printf("___");
    }
    printf("_\n");

    // print the number
    printf("|");
    for (int i = 0; i < 20; ++i)
    {
        if (hands[i].type == 0 && hands[i].number == 0)
        {
            break;
        }
        if (hands[i].type == 4)
        {
            printf("%s|", wind_number[hands[i].number]);
        }
        else
            printf("%s|", number[hands[i].number]);
    }
    printf("\t");
    if (hu && last.type != 0)
    {
        if (last.type == 4)
        {
            printf("|%s|", wind_number[last.number]);
        }
        else
            printf("|%s|", number[last.number]);
    }

    printf("\t\t|");
    for (int i = 0; i < 20; ++i)
    {
        if (doors[i].type == 0 && doors[i].number == 0)
        {
            break;
        }
        if (doors[i].type == 4)
        {
            printf("%s|", wind_number[doors[i].number]);
        }
        else
            printf("%s|", number[doors[i].number]);
    }
    // end of print the number
    printf("\n");
    // print the type
    for (int i = 0; i < 20; ++i)
    {
        if (hands[i].type == 0 && hands[i].number == 0)
        {
            break;
        }
        if (hands[i].type == 4 && hands[i].number > 4)
            printf("|  ");
        else
            printf("|%s", type[hands[i].type]);
    }
    printf("|\t");
    if (hu && last.type != 0)
    {
        if (last.type == 4 && last.number > 4)
        {
            printf("|  ");
        }
        else
            printf("|%s", type[last.type]);
    }
    if (hu)
        printf("|");
    printf("\t\t");
    for (int i = 0; i < 20; i++)
    {
        if (doors[i].type == 0 && doors[i].number == 0)
        {
            break;
        }
        if (doors[i].type == 4 && doors[i].number > 4)
            printf("|  ");
        else
            printf("|%s", type[doors[i].type]);
    }
    printf("|\n");
    // end of print the type
    for (int i = 0; i < 20; ++i)
    {
        if (hands[i].type == 0 && hands[i].number == 0)
        {
            break;
        }
        printf("‾‾‾");
    }
    printf("‾\t");
    if (hu && last.type != 0)
    {
        printf("‾‾‾");
    }
    printf("\t\t");
    for (int i = 0; i < 20; ++i)
    {
        if (doors[i].type == 0 && doors[i].number == 0)
        {
            break;
        }
        printf("‾‾‾");
    }
    printf("‾\n");
    return;
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

int draw(int playernow) {
    players[playernow]->decks[players[playernow]->normal_capacity] = shuffled_mjs[take_index++];
    write_message_wait_ack(players[playernow]->fd, "%d %d\n", players[playernow]->decks[players[playernow]->normal_capacity].type, players[playernow]->decks[players[playernow]->normal_capacity].number);

    while (players[playernow]->decks[players[playernow]->normal_capacity].type == FLOWER)
    {
        players[playernow]->flowers[players[playernow]->flower_index++] = players[playernow]->decks[players[playernow]->normal_capacity];
        players[playernow]->decks[players[playernow]->normal_capacity] = shuffled_mjs[take_index++];

        write_message_wait_ack(players[playernow]->fd, "%d %d\n", players[playernow]->decks[players[playernow]->normal_capacity].type, players[playernow]->decks[players[playernow]->normal_capacity].number);
    }
    return 0;
}

int hu_recursive_check(int *count, int n) {
    if (n == 0)
    {
        return 1;
    }

    for (int i = 0; i < 34; ++i)
    {
        // if there is 順子
        if (i < 27 && count[i] >= 1 &&
            i + 1 < 27 && count[i + 1] >= 1 &&
            i + 2 < 27 && count[i + 2] >= 1 &&
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

int hu_check(struct mj *decks, int nc) {
    // hu_check can sort the decks for its own purpose, but shouldn't modify the real deck;
    struct mj carbon[17];
    memset(carbon, 0, 17 * sizeof(struct mj));
    memcpy(carbon, decks, (nc + 1) * sizeof(struct mj));

    struct mj mj_gotten = carbon[nc];
    int insertindex = nc - 1;
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

    int count[34] = {0}; // because there are totally 34 kinds of mjs in total (excluding FLOWER).
    for (int j = 0; j < nc + 1; ++j)
    {
        count[(carbon[j].type - 1) * 9 + carbon[j].number - 1]++;
    }
    for (int i = 0; i < 34; ++i)
    {
        if (count[i] >= 2)
        {
            count[i] -= 2;
            if (hu_recursive_check(count, nc - 1) == 1)
            {
                // this deck can hu!
                return 1;
            }
            count[i] += 2;
        }
    }
    return 0;
}

int is_hu(int playernow) {
    if (hu_check(players[playernow]->decks, players[playernow]->normal_capacity) == 1)
    {
        write_message_wait_ack(players[playernow]->fd, "can hu\n");
        read_and_ack(players[playernow]->fd);
        if (strncmp(recvline, "YES!\n", 5) == 0)
        {
            // we have a winner here!
            memset(recvline, 0, strlen(recvline));
            winner = playernow;
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
        write_message_wait_ack(players[playernow]->fd, "cannot hu\n");
    }
    return 0;
}

int broadcast_discard_mj(int playernow) {
    if (discarded_mj.type == TONG)
    {
        write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded %d TONG.\n", playernow, discarded_mj.number);
        write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded %d TONG.\n", playernow, discarded_mj.number);
        write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded %d TONG.\n", playernow, discarded_mj.number);
    }
    else if (discarded_mj.type == TIAO)
    {
        write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded %d TIAO.\n", playernow, discarded_mj.number);
        write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded %d TIAO.\n", playernow, discarded_mj.number);
        write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded %d TIAO.\n", playernow, discarded_mj.number);
    }
    else if (discarded_mj.type == WAN)
    {
        write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded %d WAN.\n", playernow, discarded_mj.number);
        write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded %d WAN.\n", playernow, discarded_mj.number);
        write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded %d WAN.\n", playernow, discarded_mj.number);
    }
    else if (discarded_mj.type == DAZI)
    {
        if (discarded_mj.number == 1)
        {
            write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded EAST.\n", playernow);
            write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded EAST.\n", playernow);
            write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded EAST.\n", playernow);
        }
        else if (discarded_mj.number == 2)
        {
            write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded SOUTH.\n", playernow);
            write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded SOUTH.\n", playernow);
            write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded SOUTH.\n", playernow);
        }
        else if (discarded_mj.number == 3)
        {
            write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded WEST.\n", playernow);
            write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded WEST.\n", playernow);
            write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded WEST.\n", playernow);
        }
        else if (discarded_mj.number == 4)
        {
            write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded NORTH.\n", playernow);
            write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded NORTH.\n", playernow);
            write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded NORTH.\n", playernow);
        }
        else if (discarded_mj.number == 5)
        {
            write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded ZHONG.\n", playernow);
            write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded ZHONG.\n", playernow);
            write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded ZHONG.\n", playernow);
        }
        else if (discarded_mj.number == 6)
        {
            write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded FA.\n", playernow);
            write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded FA.\n", playernow);
            write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded FA.\n", playernow);
        }
        else if (discarded_mj.number == 7)
        {
            write_message_wait_ack(players[(playernow + 1) % 4]->fd, "(Discard) Player %d discarded BAI.\n", playernow);
            write_message_wait_ack(players[(playernow + 2) % 4]->fd, "(Discard) Player %d discarded BAI.\n", playernow);
            write_message_wait_ack(players[(playernow + 3) % 4]->fd, "(Discard) Player %d discarded BAI.\n", playernow);
        }
    }
}

int discard(int playernow) {
    read_and_ack(players[playernow]->fd);
    int index;
    sscanf(recvline, "%d", &index);
    discarded_mj.type = players[playernow]->decks[index].type;
    discarded_mj.number = players[playernow]->decks[index].number;
    memset(recvline, 0, strlen(recvline));

    swap(&players[playernow]->decks[players[playernow]->normal_capacity], &players[playernow]->decks[index]);
    players[playernow]->decks[players[playernow]->normal_capacity].type = 0;
    players[playernow]->decks[players[playernow]->normal_capacity].number = 0;

    decks_quick_sort(players[playernow]->decks, 0, players[playernow]->normal_capacity - 1);

    broadcast_discard_mj(playernow);

    return 0;
}

int draw_n_discard(int playernow) {
    write_message_wait_ack(players[playernow]->fd, "your turn\n");

    draw(playernow);
    if (is_hu(playernow) == 1)
    {
        return 1;
    }
    print_deck(players[playernow]->decks, players[playernow]->door, discarded_mj, 0, 1);
    discard(playernow);
    print_deck(players[playernow]->decks, players[playernow]->door, discarded_mj, 0, 0);
    return 0;
}

int is_pong_possible(struct mj *deck, int nc) {
    int count[34]; // because there are totally 34 kinds of mjs in total (excluding FLOWER).
    memset(count, 0, 34 * sizeof(int));

    for (int j = 0; j < nc; ++j)
    {
        count[(deck[j].type - 1) * 9 + deck[j].number - 1]++;
    }

    if (count[(discarded_mj.type - 1) * 9 + discarded_mj.number - 1] >= 2)
    {
        // this deck can pong!
        return 1;
    }
    return 0;
}

int is_eat_possible(struct mj *deck, int nc) {
    if (discarded_mj.type == DAZI)
    {
        // of course you cant eat DAZI;
        return 0;
    }

    int count[34]; // because there are totally 34 kinds of mjs in total (excluding FLOWER).
    memset(count, 0, 34 * sizeof(int));
    for (int j = 0; j < nc; ++j)
    {
        // printf("(in eat) this is recording mj's index: %d, and it's: %d, %d\n", j, deck[j].type, deck[j].number);
        count[(deck[j].type - 1) * 9 + deck[j].number - 1]++;
    }

    int target_index = (discarded_mj.type - 1) * 9 + discarded_mj.number - 1;

    if (target_index + 1 < 34 && count[target_index + 1] >= 1 &&
        target_index + 2 < 34 && count[target_index + 2] >= 1 &&
        target_index / 9 == (target_index + 1) / 9 &&
        target_index / 9 == (target_index + 2) / 9)
    {
        // this deck can eat!
        return 1;
    }
    else if (target_index + 1 < 34 && count[target_index + 1] >= 1 &&
             target_index - 1 >= 0 && count[target_index - 1] >= 1 &&
             target_index / 9 == (target_index + 1) / 9 &&
             target_index / 9 == (target_index - 1) / 9)
    {
        // this deck can eat!
        return 1;
    }
    else if (target_index - 2 >= 0 && count[target_index - 2] >= 1 &&
             target_index - 1 >= 0 && count[target_index - 1] >= 1 &&
             target_index / 9 == (target_index - 2) / 9 &&
             target_index / 9 == (target_index - 1) / 9)
    {
        // this deck can eat!
        return 1;
    }
    return 0;
}

int is_add_hu_possible(struct mj *deck, int nc) {
    int count[34]; // because there are totally 34 kinds of mjs in total (excluding FLOWER).
    memset(count, 0, 34 * sizeof(int));

    for (int j = 0; j < nc; ++j)
    {
        // printf("(in pong) this is recording mj's index: %d, and it's: %d, %d\n", j, deck[j].type, deck[j].number);
        count[(deck[j].type - 1) * 9 + deck[j].number - 1]++;
    }

    count[(discarded_mj.type - 1) * 9 + discarded_mj.number - 1]++;

    for (int i = 0; i < 34; ++i)
    {
        if (count[i] >= 2)
        {
            count[i] -= 2;
            if (hu_recursive_check(count, nc - 1) == 1)
            {
                // this deck can add_hu!
                return 1;
            }
            count[i] += 2;
        }
    }
    // cannot add_hu
    return 0;
}

int othersreaction(int *playernowp) {
    // only mention the players when they
    // really can have some reaction to it.
    // otherwise put this discarded mj to the sea of its original player;

    // Case 0: others may be able to hu
    if (is_add_hu_possible(players[(*playernowp + 1) % 4]->decks, players[(*playernowp + 1) % 4]->normal_capacity) == 1)
    {
        write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "(Hu) You actually can hu already, proceed? [Y/n]\n");
        read_and_ack(players[(*playernowp + 1) % 4]->fd);
        if (strncmp(recvline, "YES!\n", 5) == 0)
        {
            winner = (*playernowp + 1) % 4;
            return 1;
        }
    }
    if (is_add_hu_possible(players[(*playernowp + 2) % 4]->decks, players[(*playernowp + 2) % 4]->normal_capacity) == 1)
    {
        write_message_wait_ack(players[(*playernowp + 2) % 4]->fd, "(Hu) You actually can hu already, proceed? [Y/n]\n");
        read_and_ack(players[(*playernowp + 2) % 4]->fd);
        if (strncmp(recvline, "YES!\n", 5) == 0)
        {
            winner = (*playernowp + 2) % 4;
            return 1;
        }
    }
    if (is_add_hu_possible(players[(*playernowp + 3) % 4]->decks, players[(*playernowp + 3) % 4]->normal_capacity) == 1)
    {
        write_message_wait_ack(players[(*playernowp + 3) % 4]->fd, "(Hu) You actually can hu already, proceed? [Y/n]\n");
        read_and_ack(players[(*playernowp + 3) % 4]->fd);
        if (strncmp(recvline, "YES!\n", 5) == 0)
        {
            winner = (*playernowp + 3) % 4;
            return 1;
        }
    }
    // Case 1: others may be able to 碰
    if (is_pong_possible(players[(*playernowp + 1) % 4]->decks, players[(*playernowp + 1) % 4]->normal_capacity) == 1)
    {
        write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "You can pong.\n");
        read_and_ack(players[(*playernowp + 1) % 4]->fd);
        if (strncmp(recvline, "YES!\n", 5) == 0)
        {
            *playernowp = (*playernowp + 1) % 4;
            players[*playernowp]->door[players[*playernowp]->door_index++] = discarded_mj;
            int need = 2;
            for (int i = 0; i < players[*playernowp]->normal_capacity; ++i)
            {
                if (mj_compare(discarded_mj, players[*playernowp]->decks[i]) == 0)
                {
                    players[*playernowp]->door[players[*playernowp]->door_index++] = discarded_mj;
                    players[*playernowp]->decks[i].type = 0;
                    players[*playernowp]->decks[i].number = 0;
                    need--;
                    if (need == 0)
                    {
                        break;
                    }
                }
            }
            need = 2;
            for (int i = 0; i < players[*playernowp]->normal_capacity; ++i)
            {
                if (players[*playernowp]->decks[i].type == 0 && players[*playernowp]->decks[i].number == 0)
                {
                    swap(&players[*playernowp]->decks[i], &players[*playernowp]->decks[players[*playernowp]->normal_capacity - 3 + need]);
                    need--;
                    if (need == 0)
                    {
                        break;
                    }
                }
            }
            players[*playernowp]->normal_capacity -= 3;
            decks_quick_sort(players[*playernowp]->decks, 0, players[*playernowp]->normal_capacity);

            write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);
            write_message_wait_ack(players[(*playernowp + 2) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);
            write_message_wait_ack(players[(*playernowp + 3) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);

            discarded_mj.number = 0;
            discarded_mj.type = 0;

            if (is_hu(*playernowp) == 1)
            {
                return 1;
            }
            discard(*playernowp);
            othersreaction(playernowp);
            memset(recvline, 0, strlen(recvline));
            return 0;
        }
    }
    if (is_pong_possible(players[(*playernowp + 2) % 4]->decks, players[(*playernowp + 2) % 4]->normal_capacity) == 1)
    {
        write_message_wait_ack(players[(*playernowp + 2) % 4]->fd, "You can pong.\n");
        read_and_ack(players[(*playernowp + 2) % 4]->fd);
        if (strncmp(recvline, "YES!\n", 5) == 0)
        {
            *playernowp = (*playernowp + 2) % 4;
            players[*playernowp]->door[players[*playernowp]->door_index++] = discarded_mj;
            int need = 2;
            for (int i = 0; i < players[*playernowp]->normal_capacity; ++i)
            {
                if (mj_compare(discarded_mj, players[*playernowp]->decks[i]) == 0)
                {
                    players[*playernowp]->door[players[*playernowp]->door_index++] = discarded_mj;
                    players[*playernowp]->decks[i].type = 0;
                    players[*playernowp]->decks[i].number = 0;
                    need--;
                    if (need == 0)
                    {
                        break;
                    }
                }
            }
            need = 2;
            for (int i = 0; i < players[*playernowp]->normal_capacity; ++i)
            {
                if (players[*playernowp]->decks[i].type == 0 && players[*playernowp]->decks[i].number == 0)
                {
                    swap(&players[*playernowp]->decks[i], &players[*playernowp]->decks[players[*playernowp]->normal_capacity - 3 + need]);
                    need--;
                    if (need == 0)
                    {
                        break;
                    }
                }
            }
            players[*playernowp]->normal_capacity -= 3;

            decks_quick_sort(players[*playernowp]->decks, 0, players[*playernowp]->normal_capacity);

            write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);
            write_message_wait_ack(players[(*playernowp + 2) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);
            write_message_wait_ack(players[(*playernowp + 3) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);

            discarded_mj.number = 0;
            discarded_mj.type = 0;

            if (is_hu(*playernowp) == 1)
            {
                return 1;
            }
            discard(*playernowp);
            othersreaction(playernowp);
            memset(recvline, 0, strlen(recvline));
            return 0;
        }
    }
    if (is_pong_possible(players[(*playernowp + 3) % 4]->decks, players[(*playernowp + 3) % 4]->normal_capacity) == 1)
    {
        write_message_wait_ack(players[(*playernowp + 3) % 4]->fd, "You can pong.\n");
        read_and_ack(players[(*playernowp + 3) % 4]->fd);
        if (strncmp(recvline, "YES!\n", 5) == 0)
        {
            *playernowp = (*playernowp + 3) % 4;
            players[*playernowp]->door[players[*playernowp]->door_index++] = discarded_mj;
            int need = 2;
            for (int i = 0; i < players[*playernowp]->normal_capacity; ++i)
            {
                if (mj_compare(discarded_mj, players[*playernowp]->decks[i]) == 0)
                {
                    players[*playernowp]->door[players[*playernowp]->door_index++] = discarded_mj;
                    players[*playernowp]->decks[i].type = 0;
                    players[*playernowp]->decks[i].number = 0;
                    need--;
                    if (need == 0)
                    {
                        break;
                    }
                }
            }
            need = 2;
            for (int i = 0; i < players[*playernowp]->normal_capacity; ++i)
            {
                if (players[*playernowp]->decks[i].type == 0 && players[*playernowp]->decks[i].number == 0)
                {
                    swap(&players[*playernowp]->decks[i], &players[*playernowp]->decks[players[*playernowp]->normal_capacity - 3 + need]);
                    need--;
                    if (need == 0)
                    {
                        break;
                    }
                }
            }
            players[*playernowp]->normal_capacity -= 3;

            decks_quick_sort(players[*playernowp]->decks, 0, players[*playernowp]->normal_capacity);

            write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);
            write_message_wait_ack(players[(*playernowp + 2) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);
            write_message_wait_ack(players[(*playernowp + 3) % 4]->fd, "(Announce) player %d ponged it\n", *playernowp);

            discarded_mj.number = 0;
            discarded_mj.type = 0;

            if (is_hu(*playernowp) == 1)
            {
                return 1;
            }
            discard(*playernowp);
            othersreaction(playernowp);
            memset(recvline, 0, strlen(recvline));
            return 0;
        }
    }
    // Case 2: 下家 may be able to 吃
    if (is_eat_possible(players[(*playernowp + 1) % 4]->decks, players[(*playernowp + 1) % 4]->normal_capacity) == 1)
    {
        write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "You can eat.\n");
        read_and_ack(players[(*playernowp + 1) % 4]->fd);
        if (strncmp(recvline, "YES!\n", 5) == 0)
        {

            for (;;)
            {
                write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "Type which 2 of the mjs you want to eat with: \n");
                read_and_ack(players[(*playernowp + 1) % 4]->fd);
                int eatindex1 = -1, eatindex2 = -1;
                sscanf(recvline, "%d %d", &eatindex1, &eatindex2);
                memset(recvline, 0, strlen(recvline));
                struct mj eat_temp[3];
                eat_temp[0] = players[(*playernowp + 1) % 4]->decks[eatindex1];
                eat_temp[1] = players[(*playernowp + 1) % 4]->decks[eatindex2];
                eat_temp[2] = discarded_mj;
                decks_quick_sort(eat_temp, 0, 2);
                if (eat_temp[0].type == eat_temp[1].type &&
                    eat_temp[1].type == eat_temp[2].type &&
                    eat_temp[0].number == eat_temp[1].number - 1 &&
                    eat_temp[1].number == eat_temp[2].number - 1)
                {
                    write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "eatable.\n");

                    players[(*playernowp + 1) % 4]->door[players[(*playernowp + 1) % 4]->door_index++] = eat_temp[0];
                    players[(*playernowp + 1) % 4]->door[players[(*playernowp + 1) % 4]->door_index++] = eat_temp[1];
                    players[(*playernowp + 1) % 4]->door[players[(*playernowp + 1) % 4]->door_index++] = eat_temp[2];
                    players[(*playernowp + 1) % 4]->decks[eatindex1].type = 0;
                    players[(*playernowp + 1) % 4]->decks[eatindex1].number = 0;
                    players[(*playernowp + 1) % 4]->decks[eatindex2].type = 0;
                    players[(*playernowp + 1) % 4]->decks[eatindex2].number = 0;

                    swap(&players[(*playernowp + 1) % 4]->decks[eatindex1], &players[(*playernowp + 1) % 4]->decks[players[(*playernowp + 1) % 4]->normal_capacity - 1]);
                    swap(&players[(*playernowp + 1) % 4]->decks[eatindex2], &players[(*playernowp + 1) % 4]->decks[players[(*playernowp + 1) % 4]->normal_capacity - 2]);
                    break;
                }
                else
                {
                    write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "invalid choice.\n");
                }
            }

            *playernowp = (*playernowp + 1) % 4;
            players[*playernowp]->normal_capacity -= 3;
            decks_quick_sort(players[*playernowp]->decks, 0, players[*playernowp]->normal_capacity);

            write_message_wait_ack(players[(*playernowp + 1) % 4]->fd, "(Announce) player %d ate it\n", *playernowp);
            write_message_wait_ack(players[(*playernowp + 2) % 4]->fd, "(Announce) player %d ate it\n", *playernowp);
            write_message_wait_ack(players[(*playernowp + 3) % 4]->fd, "(Announce) player %d ate it\n", *playernowp);

            discarded_mj.number = 0;
            discarded_mj.type = 0;

            if (is_hu(*playernowp) == 1)
            {
                return 1;
            }
            discard(*playernowp);
            othersreaction(playernowp);
            memset(recvline, 0, strlen(recvline));
            return 0;
        }
    }
    // Case 3: no players can do anything
    write_message_wait_ack(players[*playernowp]->fd, "no one wants it.\n");
    players[*playernowp]->sea[players[*playernowp]->sea_index++] = discarded_mj;
    discarded_mj.type = 0;
    discarded_mj.number = 0;
    return 0;
}

int game_set_display() {
    printf("ENTERING GAMESETDISPLAY\n");

    int newgame_count = 0;

    read_and_ack(players[0]->fd);
    if (strncmp(recvline, "YES!\n", 5) == 0)
    {
        newgame_count++;
    }
    memset(recvline, 0, strlen(recvline));

    read_and_ack(players[1]->fd);
    if (strncmp(recvline, "YES!\n", 5) == 0)
    {
        newgame_count++;
    }
    memset(recvline, 0, strlen(recvline));

    read_and_ack(players[2]->fd);
    if (strncmp(recvline, "YES!\n", 5) == 0)
    {
        newgame_count++;
    }
    memset(recvline, 0, strlen(recvline));

    read_and_ack(players[3]->fd);
    if (strncmp(recvline, "YES!\n", 5) == 0)
    {
        newgame_count++;
    }
    memset(recvline, 0, strlen(recvline));

    if (newgame_count == 4)
    {
        // return 1 to restart the game (get back in the loop)
        write_message_wait_ack(players[0]->fd, "start!\n");
        write_message_wait_ack(players[1]->fd, "start!\n");
        write_message_wait_ack(players[2]->fd, "start!\n");
        write_message_wait_ack(players[3]->fd, "start!\n");
        return 1;
    }
    write_message_wait_ack(players[0]->fd, "no more game!\n");
    write_message_wait_ack(players[1]->fd, "no more game!\n");
    write_message_wait_ack(players[2]->fd, "no more game!\n");
    write_message_wait_ack(players[3]->fd, "no more game!\n");
    // otherwise, return 0 to stop the game
    return 0;
}

int game_init() {
    winner = -1;
    take_index = 0;
    discarded_mj.type = 0;
    discarded_mj.number = 0;
    player_gameinfo_init(players[0]);
    player_gameinfo_init(players[1]);
    player_gameinfo_init(players[2]);
    player_gameinfo_init(players[3]);
    return 0;
}

int game() {
    // let the players know which id they are
    for (int i = 0; i < 4; ++i)
    {
        sprintf(sendline, "%d-th\n", i);
        write(players[i]->fd, sendline, strlen(sendline));
        memset(sendline, 0, strlen(sendline));
    }

    for (int startplayer = 0; startplayer < 4; ++startplayer)
    {
        game_init();
        int playernow = startplayer;
        shuffle_n_deal(playernow);
        for (; winner == -1; playernow++, playernow %= 4)
        {
            if (draw_n_discard(playernow) == 1)
            {
                break;
            }
            if (othersreaction(&playernow) == 1) // note that this is a value_result argument. just think about it and you will know why we do this.
            {
                break;
            }
        }

#ifdef DEBUG
        printf("a player end their turn\n");
#endif
        write_message_wait_ack(players[0]->fd, "(Game) The game is set and we have a winner: %d!!! Continue for next round? [Y/n]\n", winner);
        write_message_wait_ack(players[1]->fd, "(Game) The game is set and we have a winner: %d!!! Continue for next round? [Y/n]\n", winner);
        write_message_wait_ack(players[2]->fd, "(Game) The game is set and we have a winner: %d!!! Continue for next round? [Y/n]\n", winner);
        write_message_wait_ack(players[3]->fd, "(Game) The game is set and we have a winner: %d!!! Continue for next round? [Y/n]\n", winner);

#ifdef DEBUG printf("ALL WRITTEN AND ACKED\n");
#endif
        // the game has set
        if (game_set_display() == 1)
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
    system("clear");
    signal(SIGCHLD, sig_chld);
    srand(time(NULL));
    for (int i = 0; i < 4; ++i)
    {
        pre_players[i] = player_init();
    }
    connection_preparation();
    while (connection_establish() == 0)
    {
        if ((childpid = fork()) == 0)
        {
            // Child process
            close(listenfd);

            // Duplicate players for the child
            for (int i = 0; i < 4; ++i)
            {
                players[i] = malloc(sizeof(struct player));
                memcpy(players[i], pre_players[i], sizeof(struct player));
            }

            game();

            // Clean up in the child process
            for (int i = 0; i < 4; ++i)
            {
                close(players[i]->fd);
                free(players[i]);
            }
            exit(0);
        }

        // Parent process: close and free players
        for (int i = 0; i < 4; ++i)
        {
            close(pre_players[i]->fd);
            free(pre_players[i]);
            pre_players[i] = player_init(); // Avoid dangling pointers
        }
    }
    close(listenfd);
    return 0;
}

// we havent implemented the 連莊、算台數 functionalities yet.