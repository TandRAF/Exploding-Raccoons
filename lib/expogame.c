#include "expogame.h"
#include <string.h>
#include <stdio.h>

// -----------------
// USER
// -----------------
Player_t create_user(const char* name) {
    Player_t u;
    u.name = strdup(name);
    return u;
}

int exist_player(Player_t *players, const char* name, int capacity) {
    for (int i = 0; i < capacity; i++) {
        if (strcmp(players[i].name, name) == 0) {
            return 1; // Player exists
        }
    }
    return 0; // Player does not exist
}
void add_player(Player_t *players, Player_t player, int *cappacity) {
    players[*cappacity] = player;
    *cappacity += 1;
}

// -----------------
// CARD
// -----------------
Card_t create_card(int id) {
    Card_t c;
    c.id = id;
    return c;
}

// -----------------
// BUNCH
// -----------------
Bunch create_bunch(int capacity) {
    Bunch b;
    b.capacity = capacity;
    b.current = 0;
    b.count = 0;
    b.cards = malloc(sizeof(Card_t) * capacity);
    return b;
}

void bunch_push(Bunch* b, Card_t card) {
    if (b->count >= b->capacity) {
        printf("Bunch is full!\n");
        return;
    }
    int index = (b->current + b->count) % b->capacity;
    b->cards[index] = card;
    b->count++;
}

Card_t bunch_pop(Bunch* b) {
    Card_t empty = { -1 };
    if (b->count == 0) {
        printf("Bunch is empty!\n");
        return empty;
    }
    Card_t card = b->cards[b->current];
    b->current = (b->current + 1) % b->capacity;
    b->count--;
    return card;
}

void free_bunch(Bunch* b) {
    free(b->cards);
    b->cards = NULL;
}

// -----------------
// MATCH
// -----------------
Match_t *create_match(int id, int capacity) {
    Match_t *m = NULL;
    m = malloc(sizeof(Match_t));
    m->id = id;
    m->count = 0;
    m->capacity = capacity;
    m->players = malloc(sizeof(Player_t) * capacity);
    return m;
}

void print_matches(Match_t* matches,int len){
    for(int i =0; i<len; i++){
        printf("Match %d:%d/%d\n",i,matches[i].count,matches[i].capacity);
    }
}
