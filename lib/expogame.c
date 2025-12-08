#include "expogame.h"
#include <string.h>
#include <stdio.h>

// -----------------
// USER
// -----------------
Player_t create_user(const char* name,int sock) {
    Player_t u;
    u.name = strdup(name);
    u.id = sock;
    u.matchid = -1;
    u.isready = 0;
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
Player_t get_player(Player_t *players, int id, int capacity) {
    for (int i = 0; i < capacity; i++) {
        if (players[i].id == id) {
            return players[i]; // Return the found player
        }
    }
    Player_t empty = { .matchid = -1, .id = -1, .name = NULL, .isready = 0 };
    return empty; // Return an empty player if not found
}
void add_player(Player_t *players, Player_t player, int *cappacity) {
    players[*cappacity] = player;
    *cappacity += 1;
}
void remove_player(Player_t *players, int id, int *capacity) {
    for (int i = 0; i < *capacity; i++) {
        if (players[i].id == id) {
            for (int j = i; j < *capacity - 1; j++) {
                players[j] = players[j + 1];
            }
            (*capacity)--;
            return;
        }
    }
}
void print_player(Player_t player) {
    printf("Player ID: %d, Name: %s, Match ID: %d, Is Ready: %d\n",
           player.id, player.name, player.matchid, player.isready);
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
void replace_space_with_newline(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ' ')
            str[i] = '\n';
    }
}

Match_t create_match(int id, int capacity) {
    Match_t m;
    m.id = id;
    m.count = 0;
    m.capacity = capacity;
    return m;
}

char* print_matches(Match_t* matches,int len,char* buffer){
    char temp[100];
    for(int i =0; i<len; i++){
        snprintf(temp,100,"Match-%d:%d/%d ",i,matches[i].count,matches[i].capacity);
        strcat(buffer,temp);
    }
    return buffer;
}
int is_match_full(Match_t match){
    return match.count == match.capacity;
}
void add_match_player(Match_t *match, Player_t player){
    match->players[match->count] = player;
    match->count++;
}
void remove_match_player(Match_t *match, int playerid){
    for(int i = 0; i < match->count; i++){
        if(match->players[i].id == playerid){
            for(int j = i; j < match->count - 1; j++){
                match->players[j] = match->players[j + 1];
            }
            match->count--;
            return;
        }
    }
}

Match_t* get_player_match(Match_t* matches, int playerid){
    for(int i=0; i<3; i++){
        for(int j=0; j<matches[i].count; j++){
            if(matches[i].players[j].id == playerid){
                return &matches[i];
            }
        }
    }
    return NULL;
}

void verify_ready(Match_t *match){
    int ready_count =0;
    for(int i=0; i<match->count; i++){
        if(match->players[i].isready){
            ready_count++;
        }
    }
    match->ready = ready_count;
}