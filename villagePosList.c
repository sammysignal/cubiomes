//
// Created by Sammy Mehra on 4/9/21.
//

#include "villagePosList.h"
#include<stdlib.h>

/**
 * Amortized linked list impl (append is O(1))
 */


/**
 * Append a pos element to the end of the list
 * @param posListA
 * @param pA
 */
void plAppend(PosList* posListA, Pos* pA) {
    PosList posList = *posListA;

    // set up last node
    Node* newLastElement = (Node*) malloc(sizeof(Node));
    newLastElement->next = NULL;

    Pos newPos = { pA->x, pA->z };
    newLastElement->p = newPos;

    // add last element
    if (posList.lastElement == NULL) {
        posListA->list = newLastElement;
        posListA->lastElement = newLastElement;
    } else {
        posListA->lastElement->next = newLastElement;
        posListA->lastElement = newLastElement;
    }
}

/**
 * Free the entire PosList
 * @param posListA
 */
void plFree(PosList* posListA) {
    Node* curr = posListA->list;
    while (curr != NULL) {
        Node* next = curr->next;
        free(curr);
        curr = next;
    }
}

void printPosList(PosList p) {
    printf("Position List:\n");

    Node* curr = p.list;

    if (curr == NULL) {
        printf("---\n");
        return;
    }

    while (curr != NULL) {
        printf("Pos(%d,%d)->", curr->p.x, curr->p.z);
        curr = curr->next;
    }

    printf("---\n");
}