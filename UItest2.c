#include<stdio.h>

struct mj
{
    int type;
    int number;
    int priority;
};typedef struct mj mj;

mj empty[1] = {0, 0, 0};

char number[10][10]={"NULL","一", "二", "三", "四", "五", "六", "七", "八", "九"};
char wind_number[8][10]={"NULL","東", "南", "西", "北", "中", "發", "白"};
char type[10][10]={"NULL","萬", "筒", "條", "風", "花"};

void print_deck(mj *hands, mj *doors, mj on_board, int separate, int show_index) {
    //print the index
    if(show_index) {
        for(int i = 0; i < 20; i++) {
            if(hands[i].type == 0 && hands[i].number == 0)break;
            if(hands[i+1].type == 0 && hands[i+1].number == 0 && separate)printf("   ");
            if(i >= 10)printf(" %d", i);
            else printf(" 0%d", i);
        }
        printf("\n");
    }

    //print the cap
    printf("_");
    for(int i = 0; i < 20; i++) {
        if(hands[i].type == 0 && hands[i].number == 0)break;
        if(hands[i+1].type == 0 && hands[i+1].number == 0 && separate)printf("  _");
        printf("___");
    }
    printf("\t\t_");
    for(int i = 0; i < 20; i++) {
        if(doors[i].type == 0 && doors[i].number == 0)break;
        printf("___");
    }
    printf("\n");

    //print the number
    printf("|");
    for(int i = 0; i < 20; i++) {
        if(hands[i].type == 0 && hands[i].number == 0) {
            break;
        }
        if(hands[i+1].type == 0 && hands[i+1].number == 0 && separate)printf("  |");
        if(hands[i].type == 4) {
            printf("%s|", wind_number[hands[i].number]);
        } else printf("%s|", number[hands[i].number]);
    }
    printf("\t\t|");
    for (int i = 0; i < 20; ++i){
        if(doors[i].type == 0 && doors[i].number == 0) {
            break;
        }
        if(doors[i].type == 4) {
            printf("%s|", wind_number[doors[i].number]);
        } else printf("%s|", number[doors[i].number]);
    }
    printf("\n");

    //print the type
    printf("|");
    for (int i = 0; i < 20; ++i)
    {
        if(hands[i].type == 0 && hands[i].number == 0)break;
        if(hands[i+1].type == 0 && hands[i+1].number == 0 && separate)printf("  |");
        if(hands[i].type == 4 && hands[i].number > 4) printf("  |");
        else printf("%s|", type[hands[i].type]);
    }
    printf("\t\t|");
    for(int i = 0; i < 20; i++) {
        if(doors[i].type == 0 && doors[i].number == 0) {
            break;
        }
        if(doors[i].type == 4 && doors[i].number > 4) printf("  |");
        else printf("%s|", type[doors[i].type]);
    }
    printf("\n");

    //print under
    printf("‾");
    for(int i = 0; i < 20; i++) {
        if(hands[i].type == 0 && hands[i].number == 0)break;
        if(hands[i+1].type == 0 && hands[i+1].number == 0 && separate)printf("  ‾");
        printf("‾‾‾");
    }
    printf("\t\t‾");
    for(int i = 0; i < 20; i++) {
        if(doors[i].type == 0 && doors[i].number == 0)break;
        printf("‾‾‾");
    }
    printf("\n");

}

int main() {
    system("clear");
    for(int i = 0; i < 10; i++)printf("\n");
    mj hand[20], door[20], last;
    bzero(hand, sizeof(hand));
    bzero(door, sizeof(door));
    last.type = 3;
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

    print_deck(hand, door, last, 1, 1);
    return 0;
}