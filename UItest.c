#include<stdio.h>

struct mj
{
    int type;
    int number;
    int priority;
};typedef struct mj mj;

char number[10][10]={"NULL","一", "二", "三", "四", "五", "六", "七", "八", "九"};
char wind_number[8][10]={"NULL","東", "南", "西", "北", "中", "發", "白"};
char type[10][10]={"NULL","萬", "筒", "條", "風", "花"};

void print_deck(mj *hands, mj *doors, mj last) {
    for (int i = 0; i < 16; ++i)
    {
        if(hands[i].type == 0 && hands[i].number == 0) {
            break;
        }
        printf("___");
    }
    printf("_\t");
    printf("____");
    printf("\t\t");
    for (int i = 0; i < 20; ++i)
    {
        if(doors[i].type == 0 && doors[i].number == 0) {
            break;
        }
        printf("___");
    }
    printf("_\n");

    // print the number
    printf("|");
    for (int i = 0; i < 16; ++i)
    {
        if(hands[i].type == 0 && hands[i].number == 0) {
            break;
        }if(hands[i].type == 4) {
            printf("%s|", wind_number[hands[i].number]);
        } else printf("%s|", number[hands[i].number]);
    }
    printf("\t");
    if(last.type == 4) {
        printf("|%s|", wind_number[last.number]);
    } else printf("|%s|", number[last.number]);

    printf("\t\t|");
    for (int i = 0; i < 20; ++i)
    {
        if(doors[i].type == 0 && doors[i].number == 0) {
            break;
        }
        if(doors[i].type == 4) {
            printf("%s|", wind_number[doors[i].number]);
        } else printf("%s|", number[doors[i].number]);
    }
    //end of print the number
    printf("\n");
    // print the type
    for (int i = 0; i < 16; ++i)
    {
        if(hands[i].type == 0 && hands[i].number == 0) {
            break;
        }
        if(hands[i].type == 4 && hands[i].number > 4) printf("|  ");
        else printf("|%s", type[hands[i].type]);
    }
    printf("|\t");
    if(last.type == 4 && last.number > 4) {
        printf("|  ");
    } else printf("|%s", type[last.type]);
    printf("|\t\t");
    for(int i = 0; i < 20; i++) {
        if(doors[i].type == 0 && doors[i].number == 0) {
            break;
        }
        if(doors[i].type == 4 && doors[i].number > 4) printf("|  ");
        else printf("|%s", type[doors[i].type]);
    }
    printf("|\n");
    //end of print the type
    for (int i = 0; i < 16; ++i)
    {
        if(hands[i].type == 0 && hands[i].number == 0) {
            break;
        }
        printf("‾‾‾");
    }
    printf("‾\t");
    printf("‾‾‾‾");
    printf("\t\t");
    for (int i = 0; i < 20; ++i)
    {
        if(doors[i].type == 0 && doors[i].number == 0) {
            break;
        }
        printf("‾‾‾");
    }
    printf("‾\n");

    

    return;
}

int main() {
    system("clear");
    for(int i = 0; i < 10; i++)printf("\n");
    mj hand[16], door[20], last;
    bzero(hand, sizeof(hand));
    bzero(door, sizeof(door));
    last.type = 4;
    last.number = 7;
    hand[0].type = 1;
    hand[0].number = 1;
    hand[1].type = 1;
    hand[1].number = 2;
    hand[2].type = 1;
    hand[2].number = 3;
    hand[3].type = 2;
    hand[3].number = 1;
    hand[4].type = 2;
    hand[4].number = 2;
    hand[5].type = 2;
    hand[5].number = 3;
    hand[6].type = 3;
    hand[6].number = 1;
    hand[7].type = 3;
    hand[7].number = 2;
    hand[8].type = 3;
    hand[8].number = 3;
    hand[9].type = 4;
    hand[9].number = 7;

    door[0].type = 1;
    door[0].number = 8;
    door[1].type = 1;
    door[1].number = 8;
    door[2].type = 1;
    door[2].number = 8;
    door[3].type = 2;
    door[3].number = 6;
    door[4].type = 2;
    door[4].number = 6;
    door[5].type = 2;
    door[5].number = 6;

    print_deck(hand, door, last);
    return 0;
}