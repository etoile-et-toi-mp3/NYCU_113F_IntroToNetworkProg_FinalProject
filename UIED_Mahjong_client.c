/*

    This is a client that connects to a Mahjong server and plays the game.

*/

#include "unp.h"
#include <stdarg.h>

// the following variables are for game purposes;
const int NO_TYPE = 0;
const int TONG = 1;
const int TIAO = 2;
const int WAN = 3;
const int DAZI = 4;   // it goes with 東南西北中發白;
const int FLOWER = 5; // it goes with 春夏秋冬梅蘭竹菊;

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
} decks[20], flowers[8], door[20], sea[150], discarded_mj, EMPTY_MJ = {0, 0, 0};
typedef struct mj mj;

int flower_index = 0, door_index = 0, sea_index = 0, start_player = 0, normal_capacity = 16;

// the following variables are for cards;
char number[10][10] = {"NULL", "一", "二", "三", "四", "五", "六", "七", "八", "九"};
char wind_number[8][10] = {"NULL", "東", "南", "西", "北", "中", "發", "白"};
char type[10][10] = {"NULL", "筒", "條", "萬", "  ", "花"};

int write_message_wait_ack(int fd, const char *format, ...) {
    // construct the content in sendline
    va_list args;
    va_start(args, format);
    vsnprintf(sendline, MAXLINE, format, args);
    va_end(args);

    // write message to the client
    write(fd, sendline, strlen(sendline));
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
        printf("Server exited prematurely. (Maybe it's because some other players disconnected.)\n");
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
        printf("Server exited prematurely. (Maybe it's because some other players disconnected.)\n");
        exit(1);
    }
    return 1;
}

// establish connection with the server
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
            else if (strncmp(recvline, "(join) ", 7) == 0)
            {
                printf("%s", recvline);
                memset(recvline, 0, strlen(recvline));
            }
            else if (strncmp(recvline, "wait...\n", 8) == 0)
            {
                // well... just wait for others to join
                memset(recvline, 0, strlen(recvline));
            }
            else if (strncmp(recvline, "start!\n", 7) == 0)
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
        read_and_ack(fd);
        sscanf(recvline, "%d %d", &t, &n);
        decks[i].type = t;
        decks[i].number = n;
        memset(recvline, 0, strlen(recvline));
    }
    for (;;)
    {
        read_and_ack(fd);
        if (strncmp(recvline, "transfer complete\n", 18) == 0)
        {
            memset(recvline, 0, strlen(recvline));
            break;
        }
        sscanf(recvline, "%d", &n);
        flowers[flower_index].type = FLOWER;
        flowers[flower_index++].number = n;
        memset(recvline, 0, strlen(recvline));
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

void print_deck(mj *hands, mj *doors, mj on_board, int separate, int show_index) {

#ifndef DEBUG
    system("clear");
#endif

    // print the board card
    if (on_board.type != 0 && on_board.number != 0)
    {
        printf("card on table: \n");
        printf("____\n");
        if (on_board.type == 4)
        {
            printf("|%s|\n", wind_number[on_board.number]);
        }
        else
            printf("|%s|\n", number[on_board.number]);
        if (on_board.type == 4 && on_board.number > 4)
        {
            printf("|  |\n");
        }
        else
            printf("|%s|\n", type[on_board.type]);
        printf("‾‾‾‾\n");
    }
    // print the index
    if (show_index)
    {
        for (int i = 0; i < 20; i++)
        {
            if (hands[i].type == 0 && hands[i].number == 0)
                break;
            if (hands[i + 1].type == 0 && hands[i + 1].number == 0 && separate)
                printf("   ");
            if (i >= 10)
                printf(" %d", i);
            else
                printf(" 0%d", i);
        }
        printf("\n");
    }

    //print some msg
    printf("Your decks: \n");
    
    // print the cap
    printf("_");
    for (int i = 0; i < 20; i++)
    {
        if (hands[i].type == 0 && hands[i].number == 0)
            break;
        if (hands[i + 1].type == 0 && hands[i + 1].number == 0 && separate)
            printf("  _");
        printf("___");
    }
    printf("\t\t");
    if(doors[0].type != 0 && doors[0].number != 0) printf("_");
    for (int i = 0; i < 20; i++)
    {
        if (doors[i].type == 0 && doors[i].number == 0)
            break;
        printf("___");
    }
    printf("\n");

    // print the number
    printf("|");
    for (int i = 0; i < 20; i++)
    {
        if (hands[i].type == 0 && hands[i].number == 0)
        {
            break;
        }
        if (hands[i + 1].type == 0 && hands[i + 1].number == 0 && separate)
            printf("  |");
        if (hands[i].type == 4)
        {
            printf("%s|", wind_number[hands[i].number]);
        }
        else
            printf("%s|", number[hands[i].number]);
    }
    printf("\t\t");
    if(doors[0].type != 0 && doors[0].number != 0) printf("|");
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
    printf("\n");

    // print the type
    printf("|");
    for (int i = 0; i < 20; ++i)
    {
        if (hands[i].type == 0 && hands[i].number == 0)
            break;
        if (hands[i + 1].type == 0 && hands[i + 1].number == 0 && separate)
            printf("  |");
        if (hands[i].type == 4 && hands[i].number > 4)
            printf("  |");
        else
            printf("%s|", type[hands[i].type]);
    }
    printf("\t\t");
    if(doors[0].type != 0 && doors[0].number != 0) printf("|");
    for (int i = 0; i < 20; i++)
    {
        if (doors[i].type == 0 && doors[i].number == 0)
        {
            break;
        }
        if (doors[i].type == 4 && doors[i].number > 4)
            printf("  |");
        else
            printf("%s|", type[doors[i].type]);
    }
    printf("\n");

    // print under
    printf("‾");
    for (int i = 0; i < 20; i++)
    {
        if (hands[i].type == 0 && hands[i].number == 0)
            break;
        if (hands[i + 1].type == 0 && hands[i + 1].number == 0 && separate)
            printf("  ‾");
        printf("‾‾‾");
    }
    printf("\t\t");
    if(doors[0].type != 0 && doors[0].number != 0) printf("‾");
    for (int i = 0; i < 20; i++)
    {
        if (doors[i].type == 0 && doors[i].number == 0)
            break;
        printf("‾‾‾");
    }
    printf("\n");
}

void print_single_card(mj a) {
    printf("___\n|");
    if (a.type == 4)
    {
        printf("%s", wind_number[a.number]);
    }
    else
        printf("%s", number[a.number]);
    printf("|\n|");
    if (a.type == 4 && a.number > 4)
    {
        printf(" ");
    }
    else
        printf("%s", type[a.type]);
    printf("|\n‾‾‾\n");
    return;
}

int client_draw() {
    // get new mj;
    read_and_ack(fd);
    sscanf(recvline, "%d %d", &decks[normal_capacity].type, &decks[normal_capacity].number);
    memset(recvline, 0, strlen(recvline));

    // flower regrab
    while (decks[normal_capacity].type == FLOWER)
    {
        flowers[flower_index++] = decks[normal_capacity];

        read_and_ack(fd);
        sscanf(recvline, "%d %d\n", &decks[normal_capacity].type, &decks[normal_capacity].number);
        memset(recvline, 0, strlen(recvline));
    }
    return 0;
}

int client_is_hu() {
    read_and_ack(fd);
    if (strncmp(recvline, "can hu\n", 7) == 0)
    {
        char answer[64] = {0};
        for (;;)
        {
            printf("You already suffice the conditions to win, proceed? [Y/n]\n");
            if (read(STDIN_FILENO, answer, 64) == 0)
            {
                printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
                close(fd);
                exit(1);
            }
            if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
            {
                // want to hu;
                write_message_wait_ack(fd, "YES!\n");
                memset(recvline, 0, strlen(recvline));
#ifdef DEBUG
                printf("(line 445) exiting client_is_hu with 1\n");
#endif

                return 1;
            }
            else if (strncmp(answer, "n\n", 2) == 0)
            {
                // don't want to hu;
                write_message_wait_ack(fd, "NO!\n");
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
    else if (strncmp(recvline, "cannot hu\n", 7) == 0)
    {
        // do nothing;
    }
    memset(recvline, 0, strlen(recvline));
    return 0;
}

int client_discard() {
    print_deck(decks, door, discarded_mj, 1, 1);
#ifdef DEBUG
    printf("pd in 450.\n");
#endif
    int index = -1;
    for (;;)
    {
        printf("Choose a mj and discard it...!\n");
        if (scanf("%d", &index) == EOF)
        {
            printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
            close(fd);
            exit(1);
        }
        if (0 <= index && index < normal_capacity + 1)
        {
            break;
        }
        else
        {
            printf("Invalid index, please choose again.\n");
            index = -1;
        }
    }

    discarded_mj.type = decks[index].type;
    discarded_mj.number = decks[index].number;

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

    write_message_wait_ack(fd, "%d", index);

    swap(&decks[normal_capacity], &decks[index]);
    decks[normal_capacity].type = 0;
    decks[normal_capacity].number = 0;
    decks_quick_sort(decks, 0, normal_capacity - 1);
    return 0;
}

int client_quiet_gang() {
    read_and_ack(fd);
    if (strncmp(recvline, "You can quietly-gang.\n", 22) == 0)
    {
        memset(recvline, 0, strlen(recvline));
        // you grabbed something that can be quietly ganged;

        char answer[64];
        for (;;)
        {
            printf("You can quietly-gang, proceed? [Y/n]\n");
            if (read(STDIN_FILENO, answer, 64) == 0)
            {
                printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
                close(fd);
                exit(1);
            }
            if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
            {
                // want to quietly_gang;
                write_message_wait_ack(fd, "YES!\n");

                door[door_index++] = decks[normal_capacity];
                door[door_index++] = decks[normal_capacity];
                door[door_index++] = decks[normal_capacity];
                door[door_index++] = decks[normal_capacity];

                int need = 3;
                for (int i = 0; i < normal_capacity; ++i)
                {
                    if (mj_compare(decks[normal_capacity], decks[i]) == 0)
                    {
                        decks[i].type = 0;
                        decks[i].number = 0;
                        need--;
                        if (need == 0)
                        {
                            break;
                        }
                    }
                }

                need = 3;
                for (int i = 0; i < normal_capacity; ++i)
                {
                    if (decks[i].type == 0 && decks[i].number == 0)
                    {
                        swap(&decks[i], &decks[normal_capacity - 4 + need]);
                        need--;
                        if (need == 0)
                        {
                            break;
                        }
                    }
                }

                normal_capacity -= 3;

                discarded_mj.type = 0;
                discarded_mj.number = 0;

                decks_quick_sort(decks, 0, normal_capacity - 1);
                return 1;
            }
            else if (strncmp(answer, "n\n", 2) == 0)
            {
                // don't want to pong;
                write_message_wait_ack(fd, "NO!\n");
                return 0;
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
        memset(recvline, 0, strlen(recvline));
    }
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

int client_game_set() {
    printf("%s", recvline);
    memset(recvline, 0, strlen(recvline));

    char answer[64];
    for (;;)
    {
        if (read(STDIN_FILENO, answer, 64) == 0)
        {
            printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
            close(fd);
            exit(1);
        }
        if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
        {
            // want to play new game;
            write_message_wait_ack(fd, "YES!\n");
            printf("GOT SERVER APPROVAL, WAIT FOR OTHERS...\n");
            break;
        }
        else if (strncmp(answer, "n\n", 2) == 0)
        {
            // don't want to play new game;
            write_message_wait_ack(fd, "NO!\n");
            printf("GOT SERVER APPROVAL, WAIT FOR OTHERS...\n");
            break;
        }
        else
        {
            // unexpected chars received.
            // back into the loop until correct response is given.
            memset(answer, 0, strlen(answer));
        }
    }

    read_and_ack(fd);
    if (strncmp(recvline, "start!\n", 8) == 0)
    {
        // new game start
        memset(recvline, 0, strlen(recvline));
        return 1;
    }
    else if (strncmp(recvline, "no more game!\n", 14) == 0)
    {
        memset(recvline, 0, strlen(recvline));
        return 0;
    }
    return 0;
}

int receive_id() {
    read(fd, recvline, MAXLINE);
    id_num = (int)recvline[0] - '0';
    system("clear");
    printf("This is your id: %d\n", id_num);
    memset(recvline, 0, strlen(recvline));
    sleep(4);
    return 0;
}

int client_game_init() {
    memset(decks, 0, 20 * sizeof(struct mj));
    memset(flowers, 0, 8 * sizeof(struct mj));
    memset(door, 0, 20 * sizeof(struct mj));
    memset(sea, 0, 150 * sizeof(struct mj));
    discarded_mj.type = 0;
    discarded_mj.number = 0;
    flower_index = 0;
    door_index = 0;
    sea_index = 0;
    normal_capacity = 16;
    return 0;
}

int game() {
    receive_id();

    for (int startplayer = 0; startplayer < 4; ++startplayer)
    {
        client_game_init();
        get_decks();

        for (;;)
        {
            print_deck(decks, door, discarded_mj, 0, 0);

            if (read_and_ack(fd) != 0)
            {
                printf("Server terminated prematurely!? Exiting...\n");
                exit(1);
            }
            else
            {
                if (strncmp(recvline, "your turn\n", 10) == 0)
                {
                    printf("----------It's your turn!----------\n");
                    memset(recvline, 0, strlen(recvline));
                    // yeah it's your turn;
                    client_draw();
                    print_deck(decks, door, discarded_mj, 1, 1);
                    if (client_is_hu() == 1)
                    {
                        // really won
                        continue;
                    }
                    while (client_quiet_gang() == 1)
                    {
                        client_draw();
                        if (client_is_hu() == 1)
                        {
                            return 1;
                        }
                    }
                    client_discard();
                    continue;
                }
                else if (strncmp(recvline, "(Discard) ", 6) == 0)
                {
                    // some other player discarded out something
                    printf("%s", recvline);

                    // record the discarded mj
                    if ('A' <= recvline[29] && recvline[29] <= 'Z')
                    {
                        // this is a DAZI;
                        discarded_mj.type = DAZI;
                        if (recvline[29] == 'E')
                        {
                            discarded_mj.number = 1;
                        }
                        else if (recvline[29] == 'S')
                        {
                            discarded_mj.number = 2;
                        }
                        else if (recvline[29] == 'W')
                        {
                            discarded_mj.number = 3;
                        }
                        else if (recvline[29] == 'N')
                        {
                            discarded_mj.number = 4;
                        }
                        else if (recvline[29] == 'Z')
                        {
                            discarded_mj.number = 5;
                        }
                        else if (recvline[29] == 'F')
                        {
                            discarded_mj.number = 6;
                        }
                        else if (recvline[29] == 'B')
                        {
                            discarded_mj.number = 7;
                        }
                    }
                    else
                    {
                        discarded_mj.number = recvline[29] - '0';
                        if (recvline[31] == 'W')
                        {
                            discarded_mj.type = WAN;
                        }
                        else
                        {
                            if (recvline[32] == 'O')
                            {
                                discarded_mj.type = TONG;
                            }
                            else
                            {
                                discarded_mj.type = TIAO;
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
                else if (strncmp(recvline, "You can pong.\n", 15) == 0)
                {
                    memset(recvline, 0, strlen(recvline));
                    // yeah someone discarded something that you can pong;

                    char answer[64];
                    for (;;)
                    {
                        printf("You can pong, proceed? [Y/n]\n");
                        if (read(STDIN_FILENO, answer, 64) == 0)
                        {
                            printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
                            close(fd);
                            exit(1);
                        }
                        if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
                        {
                            // want to pong;
                            write_message_wait_ack(fd, "YES!\n");

                            door[door_index++] = discarded_mj;
                            door[door_index++] = discarded_mj;
                            door[door_index++] = discarded_mj;

                            int need = 2;
                            for (int i = 0; i < normal_capacity; ++i)
                            {
                                if (mj_compare(discarded_mj, decks[i]) == 0)
                                {
                                    decks[i].type = 0;
                                    decks[i].number = 0;
                                    need--;
                                    if (need == 0)
                                    {
                                        break;
                                    }
                                }
                            }

                            need = 2;
                            for (int i = 0; i < normal_capacity; ++i)
                            {
                                if (decks[i].type == 0 && decks[i].number == 0)
                                {
                                    swap(&decks[i], &decks[normal_capacity - 3 + need]);
                                    need--;
                                    if (need == 0)
                                    {
                                        break;
                                    }
                                }
                            }

                            normal_capacity -= 3;

                            discarded_mj.type = 0;
                            discarded_mj.number = 0;

                            decks_quick_sort(decks, 0, normal_capacity);

                            if (client_is_hu() == 1)
                            {
                                // really won
#ifdef DEBUG
                                printf("breaking in 784, hu kakutei after pong\n");
#endif

                                break;
                            }
                            client_discard();
                            break;
                        }
                        else if (strncmp(answer, "n\n", 2) == 0)
                        {
                            // don't want to pong;
                            write_message_wait_ack(fd, "NO!\n");
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
                else if (strncmp(recvline, "You can gang.\n", 15) == 0)
                {
                    memset(recvline, 0, strlen(recvline));
                    // yeah someone discarded something that you can gang;

                    char answer[64];
                    for (;;)
                    {
                        printf("You can gang, proceed? [Y/n]\n");
                        if (read(STDIN_FILENO, answer, 64) == 0)
                        {
                            printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
                            close(fd);
                            exit(1);
                        }
                        if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
                        {
                            // want to pong;
                            write_message_wait_ack(fd, "YES!\n");

                            door[door_index++] = discarded_mj;
                            door[door_index++] = discarded_mj;
                            door[door_index++] = discarded_mj;
                            door[door_index++] = discarded_mj;

                            int need = 3;
                            for (int i = 0; i < normal_capacity; ++i)
                            {
                                if (mj_compare(discarded_mj, decks[i]) == 0)
                                {
                                    decks[i].type = 0;
                                    decks[i].number = 0;
                                    need--;
                                    if (need == 0)
                                    {
                                        break;
                                    }
                                }
                            }

                            need = 3;
                            for (int i = 0; i < normal_capacity; ++i)
                            {
                                if (decks[i].type == 0 && decks[i].number == 0)
                                {
                                    swap(&decks[i], &decks[normal_capacity - 4 + need]);
                                    need--;
                                    if (need == 0)
                                    {
                                        break;
                                    }
                                }
                            }

                            normal_capacity -= 3;

                            discarded_mj.type = 0;
                            discarded_mj.number = 0;

                            decks_quick_sort(decks, 0, normal_capacity - 1);

                            client_draw();
                            if (client_is_hu() == 1)
                            {
                                // really won
#ifdef DEBUG
                                printf("breaking in 784, hu kakutei after pong\n");
#endif

                                break;
                            }
                            client_discard();
                            break;
                        }
                        else if (strncmp(answer, "n\n", 2) == 0)
                        {
                            // don't want to pong;
                            write_message_wait_ack(fd, "NO!\n");
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
                else if (strncmp(recvline, "You can eat.\n", 13) == 0)
                {
                    memset(recvline, 0, strlen(recvline));
                    // yeah someone discarded something that you can eat;

                    char answer[64] = {0};
                    for (;;)
                    {
                        printf("You can eat, proceed? [Y/n]\n");
                        if (read(STDIN_FILENO, answer, 64) == 0)
                        {
                            printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
                            close(fd);
                            exit(1);
                        }
                        if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
                        {
                            memset(answer, 0, strlen(answer));

                            // want to eat;
                            write_message_wait_ack(fd, "YES!\n");

                            int eatindex1 = -1, eatindex2 = -1;
                            print_deck(decks, door, discarded_mj, 0, 1);
#ifdef DEBUG
                            printf("pd in 844.\n");
#endif
                            for (;;)
                            {
                                read_and_ack(fd);
                                if (strncmp(recvline, "Type which 2 of the mjs you want to eat with: \n", 47) == 0)
                                {
                                    printf("%s", recvline);
                                    memset(recvline, 0, strlen(recvline));

                                    if (read(STDIN_FILENO, answer, 64) == 0)
                                    {
                                        printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
                                        close(fd);
                                        exit(1);
                                    }
                                    sscanf(answer, "%d %d", &eatindex1, &eatindex2);
                                    write_message_wait_ack(fd, answer);

                                    read_and_ack(fd);
                                    if (strncmp(recvline, "eatable.\n", 10) == 0)
                                    {
                                        // good eat;
                                        struct mj eat_temp[3];
                                        eat_temp[0] = decks[eatindex1];
                                        eat_temp[1] = decks[eatindex2];
                                        eat_temp[2] = discarded_mj;
                                        decks_quick_sort(eat_temp, 0, 2);
                                        door[door_index++] = eat_temp[0];
                                        door[door_index++] = eat_temp[1];
                                        door[door_index++] = eat_temp[2];
                                        decks[eatindex1].type = 0;
                                        decks[eatindex1].number = 0;
                                        decks[eatindex2].type = 0;
                                        decks[eatindex2].number = 0;
                                        swap(&decks[eatindex1], &decks[normal_capacity - 1]);
                                        swap(&decks[eatindex2], &decks[normal_capacity - 2]);

                                        break;
                                    }
                                    else
                                    {
                                        printf("%s", recvline);
                                    }
                                    memset(recvline, 0, strlen(recvline));
                                }
                            }

                            normal_capacity -= 3;

                            decks_quick_sort(decks, 0, normal_capacity);

                            if (client_is_hu() == 1)
                            {
                                // really won
#ifdef DEBUG
                                printf("breaking in 871, hu kakutei after eat\n");
#endif

                                break;
                            }
                            client_discard();
                            break;
                        }
                        else if (strncmp(answer, "n\n", 2) == 0)
                        {
                            // don't want to eat;
                            write_message_wait_ack(fd, "NO!\n");
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
                else if (strncmp(recvline, "no one wants it.\n", 17) == 0)
                {
                    memset(recvline, 0, strlen(recvline));

                    sea[sea_index++] = discarded_mj;
                }
                else if (strncmp(recvline, "(Game) ", 7) == 0)
                {
                    // the game has set;
                    break;
                }
                else if (strncmp(recvline, "(Hu) ", 5) == 0)
                {
                    // you actually can hu
                    memset(recvline, 0, strlen(recvline));
                    char answer[64];
                    for (;;)
                    {
                        printf("You actually can hu already, proceed? [Y/n]\n");
                        if (read(STDIN_FILENO, answer, 64) == 0)
                        {
                            printf("YOU HAVE PRESSED Ctrl-D. Exiting...\n");
                            close(fd);
                            exit(1);
                        }
                        if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
                        {
                            // want to pong;
                            write_message_wait_ack(fd, "YES!\n");
                            break;
                        }
                        else if (strncmp(answer, "n\n", 2) == 0)
                        {
                            // don't want to pong;
                            write_message_wait_ack(fd, "NO!\n");
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
                else if (strncmp(recvline, "(End) ", 6) == 0)
                {
                    break;
                }
                else
                {
                    printf("received this, please debug: %s", recvline);
                    memset(recvline, 0, strlen(recvline));
                }
            }
        }

        if (client_game_set() == 1)
        {
            // continue the game
        }
        else
        {
            // stop the game
            printf("Oh no! Some player don't want to play anymore! Exiting...\n");
            return 0;
        }
    }

    printf("4 Games has finished, exiting...\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2)
    {
        printf("Usage: ./Mahjong_client <ServerIP>\n");
        return 1;
    }

    system("clear");

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
        close(fd);
    }
    return 0;
}