#pragma once
#include <stdlib.h>

// -----------------
// STRUCTS
// -----------------
typedef struct {
    int matchid;
    int id;
    char* name;
} Player_t;

typedef struct {
    int id;
} Card_t;

typedef struct {
    int current;
    int count;
    int capacity;
    Card_t* cards;
} Bunch;

typedef struct {
    int id;
    int count;
    int capacity;
    int ready;
    Player_t* players;
} Match_t;

// -----------------
// FUNCTION DECLARATIONS
// -----------------
Player_t create_user(const char* name);
int exist_player(Player_t *players, const char* name, int capacity);
Card_t create_card(int id);
void add_player(Player_t *players, Player_t player, int *cappacity);

Bunch create_bunch(int capacity);
void bunch_push(Bunch* b, Card_t card);
Card_t bunch_pop(Bunch* b);
void free_bunch(Bunch* b);

Match_t *create_match(int id, int capacity);
void print_matches(Match_t* matches,int len);
