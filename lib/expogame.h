#pragma once
#include <stdlib.h>

// -----------------
// ENUMS & CONSTANTS
// -----------------
typedef enum {
    CARD_NOPE,
    CARD_ATTACK,
    CARD_SKIP,
    CARD_FAVOR,
    CARD_SHUFFLE,
    CARD_SEE_FUTURE,
    CARD_DEFUSE,
    CARD_EXPLODING_KITTEN,
    CARD_GENERIC
} CardType;

typedef enum {
    STATE_LOBBY,
    STATE_SETUP,
    STATE_PLAYER_TURN,
    STATE_RESOLVE_ACTION,
    STATE_DRAW_PHASE,
    STATE_EXPLOSION,
    STATE_GAME_OVER
} GameState;

// -----------------
// STRUCTS
// -----------------
typedef struct {
    int id; // Unique ID
    CardType type;
    char name[32];
} Card_t;

typedef struct {
    int current;
    int count;
    int capacity;
    Card_t* cards;
} Bunch; 

typedef struct {
    int matchid;
    int id; // Socket ID
    char* name;
    int isready;
    int is_eliminated;
    Bunch hand; 
} Player_t;

typedef struct {
    int id;
    int count;
    int capacity;
    int ready;
    
    GameState state;
    int current_player_idx;
    int attack_turns_accumulated; 
    
    Bunch deck;
    Bunch discard_pile;
    Player_t players[5];
} Match_t;

// -----------------
// FUNCTION DECLARATIONS
// -----------------
Player_t create_user(const char* name,int sock);
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

Match_t create_match(int id, int capacity);
char* print_matches(Match_t* matches,int len,char* buffer);
void replace_space_with_newline(char *str);
int is_match_full(Match_t match);
void add_match_player(Match_t *match, Player_t player);
void remove_match_player(Match_t *match, int playerid);
void verify_ready(Match_t *match);
Match_t* get_player_match(Match_t* matches, int playerid);

void run_game_fsm(Match_t *m, int player_id, int action_code, int card_idx, int target_id);
void shuffle_bunch(Bunch *b);
const char* get_card_name(CardType type);
char* print_hand(Bunch* hand, char* buffer);


int send_string(int sock, const char *str);
int recv_string(int sock, char *buffer, size_t max_len);