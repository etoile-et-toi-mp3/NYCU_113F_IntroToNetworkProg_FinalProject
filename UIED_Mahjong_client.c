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
char type[10][10] = {"NULL", "筒", "條", "萬", "風", "花"};

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
            printf("ACK Received\n");
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
        printf("Received: %s\n", recvline);

        // Send the ACK
        write(fd, "ACK\n", 4);
        printf("ACK sent.\n");
        return 0;
    }
    else
    {
        perror("Failed to receive message");
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
        if (strncmp(recvline, "transfer complete\n", 18) == 0)
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
        printf("Your hand: \n");
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
    printf("enter client_draw\n");
    read_and_ack(fd);
    sscanf(recvline, "%d %d", &decks[normal_capacity].type, &decks[normal_capacity].number);
    printf("this is what we got in int : %d, %d\n", decks[normal_capacity].type, decks[normal_capacity].number);
    memset(recvline, 0, strlen(recvline));

    // flower regrab
    while (decks[normal_capacity].type == FLOWER)
    {
        printf("A flower is drawn.\n");
        flowers[flower_index++] = decks[normal_capacity];

        read_and_ack(fd);
        sscanf(recvline, "%d %d\n", &decks[normal_capacity].type, &decks[normal_capacity].number);
        memset(recvline, 0, strlen(recvline));
    }
    printf("client_draw ends\n");
    return 0;
}

int client_is_hu() {
    printf("enter client_is_hu\n");
    read_and_ack(fd);
    printf("IN client_is_hu recvline: %s", recvline);
    if (strncmp(recvline, "can hu\n", 7) == 0)
    {
        char answer[64] = {0};
        for (;;)
        {
            printf("You already suffice the conditions to win, proceed? [Y/n]\n");
            scanf("%s", answer);
            if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
            {
                // want to hu;
                write_message_wait_ack(fd, "YES!\n");
                break;
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
    else
    {
        // cannot hu, just continue the game;
    }
    memset(recvline, 0, strlen(recvline));
    return 0;
}

int client_discard() {
    print_deck(decks, door, EMPTY_MJ, 0, 1);
    int index = -1;
    for (;;)
    {
        printf("Choose a mj and discard it...!\n");
        scanf("%d", &index);
        if (0 <= index && index < 17)
        {
            break;
        }
        else
        {
            printf("Invalid index, please choose again.\n");
            index = -1;
        }
    }

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

    write_message_wait_ack(fd, "%d", index);

    swap(&decks[normal_capacity], &decks[index]);
    decks[normal_capacity].type = 0;
    decks[normal_capacity].number = 0;
    decks_quick_sort(decks, 0, normal_capacity - 1);
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

int get_result() {
    printf("%s", recvline);
    memset(recvline, 0, strlen(recvline));

    char answer[64];
    for (;;)
    {
        scanf("%s", answer);
        if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
        {
            // want to pong;
            sprintf(sendline, "YES!\n");
            write(fd, sendline, strlen(sendline));
            memset(sendline, 0, strlen(sendline));
            return 1;
        }
        else if (strncmp(answer, "n\n", 2) == 0)
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

int receive_id() {
    read(fd, recvline, MAXLINE);
    id_num = (int)recvline[0] - '0';
    printf("This is your id: %d\n", id_num);
    memset(recvline, 0, strlen(recvline));
    return 0;
}

int client_game_init() {
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
            // system("clear");
            printf("initial hand:\n");
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
                    printf("YOURTURN: %s\n======\n", recvline);
                    memset(recvline, 0, strlen(recvline));
                    // yeah it's your turn;
                    client_draw();
                    print_deck(decks, door, discarded_mj, 0, 1);
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
                else if (strncmp(recvline, "You can pong.\n", 15) == 0)
                {
                    memset(recvline, 0, strlen(recvline));
                    // yeah someone discarded something that you can pong;

                    char answer[64];
                    for (;;)
                    {
                        printf("You can pong, proceed? [Y/n]\n");
                        scanf("%s", answer);
                        if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
                        {
                            // want to pong;
                            sprintf(sendline, "YES!\n");
                            write(fd, sendline, strlen(sendline));
                            memset(sendline, 0, strlen(sendline));
                            door[door_index++] = discarded_mj;
                            int need = 2;
                            for (int i = 0; i < normal_capacity; ++i)
                            {
                                if (mj_compare(discarded_mj, decks[i]) == 0)
                                {
                                    door[door_index++] = discarded_mj;
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

                            client_is_hu();
                            client_discard();
                            break;
                        }
                        else if (strncmp(answer, "n\n", 2) == 0)
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
                else if (strncmp(recvline, "You can eat.\n", 13) == 0)
                {
                    memset(recvline, 0, strlen(recvline));
                    // yeah someone discarded something that you can eat;

                    char answer[64] = {0};
                    for (;;)
                    {
                        printf("You can eat, proceed? [Y/n]\n");
                        scanf("%s", answer);
                        if (strncmp(answer, "Y\n", 2) == 0 || strncmp(answer, "\n", 1) == 0)
                        {
                            memset(answer, 0, strlen(answer));

                            // want to eat;
                            sprintf(sendline, "YES!\n");
                            write(fd, sendline, strlen(sendline));
                            memset(sendline, 0, strlen(sendline));

                            int eatindex1, eatindex2;

                            for (;;)
                            {
                                read(fd, recvline, MAXLINE);
                                if (strncmp(recvline, "Type which 2 of the mjs you want to eat with: \n", 47) == 0)
                                {
                                    printf("%s", recvline);
                                    memset(recvline, 0, strlen(recvline));

                                    scanf("%s", answer);
                                    sscanf(answer, "%d %d", &eatindex1, &eatindex2);
                                    write(fd, answer, strlen(answer));
                                    memset(answer, 0, strlen(answer));

                                    read(fd, recvline, MAXLINE);
                                    if (strncmp(recvline, "eatable.\n", 10) == 0)
                                    {
                                        // good eat;
                                        memset(recvline, 0, strlen(recvline));
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

                                        normal_capacity -= 3;
                                        decks_quick_sort(decks, 0, normal_capacity - 1);
                                        break;
                                    }
                                    else
                                    {
                                        printf("%s", recvline);
                                        memset(recvline, 0, strlen(recvline));
                                    }
                                }
                            }

                            client_is_hu();
                            client_discard();
                            break;
                        }
                        else if (strncmp(answer, "n\n", 2) == 0)
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
                else if (strncmp(recvline, "no one wants it.\n", 17) == 0)
                {
                    sea[sea_index++] = discarded_mj;
                    discarded_mj.type = 0;
                    discarded_mj.number = 0;
                }
                else if (strncmp(recvline, "(Game) ", 7) == 0)
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
        if (get_result() == 1)
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
        close(fd);
    }
    return 0;
}