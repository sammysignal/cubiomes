//
// Created by Sammy Mehra on 4/9/21.
//

#include "finders.h"

#ifndef CUBIOMES_VILLAGEPOSLIST_H
#define CUBIOMES_VILLAGEPOSLIST_H

#endif //CUBIOMES_VILLAGEPOSLIST_H

#define STRUCT(S) typedef struct S S; struct S


STRUCT(Node) {
    Pos p;
    Node* next;
};

STRUCT(PosList) {
    Node* list;
    Node* lastElement;
};

void plAppend(PosList* posListA, Pos* pA);

void plFree(PosList* posListA);

void printPosList(PosList p);