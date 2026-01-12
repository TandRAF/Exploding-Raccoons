#pragma once
#include <stdlib.h>

// -----------------
// STRUCTS
// -----------------
typedef struct {
    int id; // Card Type ID (0=Exploding, 1=Defuse, etc.)
} Card_t;

typedef struct {
    int current;
    int count;
    int capacity;
    Card_t* cards;
} Bunch;

typedef struct {
    int matchid;
    int id;        // Socket FD
    char* name;
    int isready;
    int is_eliminated; // New: Track elimination
    Bunch hand;        // New: Player specific cards
} Player_t;

typedef struct {
    int id;
    int count;
    int capacity;
    int ready;
    // New Game State Fields
    int started;       
    int turn_index;    
    Bunch deck;        
    Bunch discard;     
    Player_t players[5];
} Match_t;

// -----------------
// FUNCTION DECLARATIONS
// -----------------
Player_t create_user(const char* name, int sock);
int exist_player(Player_t *players, const char* name, int capacity);
Card_t create_card(int id);
void print_player(Player_t player);
void add_player(Player_t *players, Player_t player, int *cappacity);
Player_t get_player(Player_t *players, int id, int capacity);
void remove_player(Player_t *players, int id, int *capacity);

Bunch create_bunch(int capacity);
void bunch_push(Bunch* b, Card_t card);
Card_t bunch_pop(Bunch* b);
void free_bunch(Bunch* b);
void shuffle_bunch(Bunch *b); // New: Shuffle function

Match_t create_match(int id, int capacity);
char* print_matches(Match_t* matches,int len,char* buffer);
void replace_space_with_newline(char *str);
int is_match_full(Match_t match);
void add_match_player(Match_t *match, Player_t player);
void remove_match_player(Match_t *match, int playerid);
void verify_ready(Match_t *match);
Match_t* get_player_match(Match_t* matches, int playerid);
