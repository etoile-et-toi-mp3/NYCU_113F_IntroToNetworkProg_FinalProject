#include "unp.h"

#define NO_TYPE 0
#define TONG 1
#define TIAO 2
#define WAN 3
#define DAZI 4      // it goes with 東南西北中發白;
#define FLOWER 5    // it goes with 春夏秋冬梅蘭竹菊;

#define DONG 1
#define NAN 2
#define XI 3
#define BEI 4
#define ZHONG 5
#define FA 6
#define BAI 7

struct mj {
    int type;   // all the numbers here should be 1-indexed;
    int number; // all the numbers here should be 1-indexed;
    int priority;
};
typedef struct mj mj;

int mjsame(mj a, mj b) {
    return a.type == b.type && a.number == b.number;
}

int point(int base, int mul, mj *decks, mj last, int doorwind) {
    sort(decks, decks + 16, [](mj a, mj b) {
        return a.type * 100 + a.number < b.type * 100 + b.number;
    });

    int count=0, tmpcnt=0;
    //中发白
    tmpcnt = 0;
    for (int i = 2; i < 16; i++) {
        if (decks[i].type == DAZI && decks[i].number >= 5 && decks[i].number <= 7) {
            tmpcnt++;
        }else tmpcnt = 0;

        if(tmpcnt == 2 && last.type == DAZI && last.number == decks[i].number) {
            count += 1;
            tmpcnt = 0;
        }
        if(tmpcnt == 3) {
            count += 1;
            tmpcnt = 0;
        }
    }
    //东南西北
    tmpcnt = 0;
    for (int i = 2; i < 16; i++) {
        if (decks[i].type == DAZI && decks[i].number >= 1 && decks[i].number <= 4) {
            tmpcnt++;
        }else tmpcnt = 0;

        if(tmpcnt == 2 && last.type == DAZI && last.number == decks[i].number) {
            count += (doorwind == decks[i].number ? 1 : 0);
            count += decks[i].number == TONG ? 1 : 0;
            tmpcnt = 0;
        }
        if(tmpcnt == 3) {
            count += (doorwind == decks[i].number ? 1 : 0);
            count += decks[i].number == TONG ? 1 : 0;
            count += 1;
            tmpcnt = 0;
        }
    }
    //中洞
    tmpcnt = 0;
    for (int i = 1; i < 16; i++) {
        if(decks[i].number == decks[i-1].number-2 && last.type == decks[i].type && last.number == decks[i].number-1) {
            count++;
        }
    }
    //边张
    for(int i = 1; i < 16; i++) {
        if(decks[i].number == 2 && decks[i-1].number == 1 && decks[i].type == last.type && last.number == 3) {
            count++;
        }
        if(decks[i].number == 9 && decks[i-1].number == 8 && decks[i].type == last.type && last.number == 7) {
            count++;
        }
    }
    //碰碰胡 4台
    tmpcnt = 0;
    for (int i = 2; i < 16; i++) {
        if(mjsame(decks[i], decks[i-1]) && mjsame(decks[i], decks[i-2])) 
            tmpcnt++;
        if(mjsame(decks[i], decks[i-1]) && mjsame(decks[i], last))
            tmpcnt++;
    }
    if(tmpcnt == 5) {
        count += 4;
    }

    //flowers
    // for (int i = 0; i < 16; i++) {
    //     if (decks[i].type == FLOWER && decks[i].number%4 == doorwind) {
    //         count++;
    //     }
    // }

    return base + count * mul;
}