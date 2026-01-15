#include "expogame.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h> // Necesar pentru send/recv

// ------------------------------------------------------------------
// NETWORK IMPLEMENTATION (MUTAT DIN SERVER.C AICI)
// ------------------------------------------------------------------
int send_string(int sock, const char *str) {
    char message[1024]; 
    // Folosim un buffer local pentru a adauga \n
    size_t len = snprintf(message, sizeof(message), "%s\n", str);

    if (len >= sizeof(message)) {
        fprintf(stderr, "send_string: message too long\n");
        return -1;
    }

    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(sock, message + total, len - total, 0);
        if (sent < 0) {
            perror("send_string error");
            return -1;
        }
        total += sent;
    }
    return 0;
}

int recv_string(int sock, char *buffer, size_t max_len) {
    size_t total = 0;
    while (total < max_len - 1) {
        char c;
        ssize_t received = recv(sock, &c, 1, 0);

        if (received == 0) return -1; // Connection closed
        if (received < 0) {
            perror("recv error");
            return -1;
        }

        if (c == '\n') break;
        if (c == '\0') break;

        buffer[total++] = c;
    }
    buffer[total] = '\0';
    return 0;
}

// -----------------
// CARD HELPERS
// -----------------
const char* get_card_name(CardType type) {
    switch(type) {
        case CARD_ATTACK: return "Attack";
        case CARD_SKIP: return "Skip";
        case CARD_DEFUSE: return "Defuse";
        case CARD_EXPLODING_KITTEN: return "EXPLODING KITTEN";
        case CARD_SHUFFLE: return "Shuffle";
        case CARD_SEE_FUTURE: return "See Future";
        case CARD_FAVOR: return "Favor (Steal)";
        default: return "Cat Card";
    }
}

// -----------------
// USER
// -----------------
Player_t create_user(const char* name, int sock) {
    Player_t u;
    u.name = strdup(name);
    u.id = sock;
    u.matchid = -1;
    u.isready = 0;
    u.is_eliminated = 0;
    u.hand = create_bunch(10); 
    return u;
}

int exist_player(Player_t *players, const char* name, int capacity) {
    for (int i = 0; i < capacity; i++) {
        if (strcmp(players[i].name, name) == 0) return 1;
    }
    return 0;
}

Player_t get_player(Player_t *players, int id, int capacity) {
    for (int i = 0; i < capacity; i++) {
        if (players[i].id == id) return players[i];
    }
    Player_t empty = { .matchid = -1, .id = -1, .name = NULL, .isready = 0 };
    return empty;
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
// CARD & BUNCH
// -----------------
Card_t create_card(int id) {
    Card_t c;
    c.id = id;
    c.type = CARD_GENERIC; 
    return c;
}

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
    if (b->count == 0) return empty;
    Card_t card = b->cards[b->current];
    b->current = (b->current + 1) % b->capacity;
    b->count--;
    return card;
}

void free_bunch(Bunch* b) {
    if (b->cards) {
        free(b->cards);
        b->cards = NULL;
    }
}

char* print_hand(Bunch* hand, char* buffer) {
    buffer[0] = '\0'; 
    char temp[100];
    if (hand->count == 0) {
        strcat(buffer, "Your hand is empty.");
        return buffer;
    }
    strcat(buffer, "--- YOUR HAND ---\n");
    for (int i = 0; i < hand->count; i++) {
        int idx = (hand->current + i) % hand->capacity;
        Card_t c = hand->cards[idx];
        snprintf(temp, 100, "[%d] %s\n", i, get_card_name(c.type));
        strcat(buffer, temp);
    }
    strcat(buffer, "-----------------");
    return buffer;
}

// -----------------
// MATCH HELPER
// -----------------
void replace_space_with_newline(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ' ') str[i] = '\n';
    }
}

Match_t create_match(int id, int capacity) {
    Match_t m;
    m.id = id;
    m.count = 0;
    m.capacity = capacity;
    m.state = STATE_LOBBY;
    m.ready = 0;
    return m;
}

char* print_matches(Match_t* matches, int len, char* buffer){
    char temp[100];
    for(int i =0; i<len; i++){
        snprintf(temp, 100, "Match-%d:%d/%d ", i+1, matches[i].count, matches[i].capacity);
        strcat(buffer, temp);
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
    int ready_count = 0;
    for(int i=0; i<match->count; i++){
        if(match->players[i].isready) ready_count++;
    }
    match->ready = ready_count;
}

// -----------------
// GAME LOGIC FSM
// -----------------

void shuffle_bunch(Bunch *b) {
    if (b->count <= 1) return;
    srand(time(NULL));
    for (int i = 0; i < b->count - 1; i++) {
        int j = i + rand() / (RAND_MAX / (b->count - i) + 1);
        Card_t temp = b->cards[i];
        b->cards[i] = b->cards[j];
        b->cards[j] = temp;
    }
}

void next_turn(Match_t *m) {
    if (m->attack_turns_accumulated > 0) {
        m->attack_turns_accumulated--;
    } else {
        do {
            m->current_player_idx = (m->current_player_idx + 1) % m->count;
        } while (m->players[m->current_player_idx].is_eliminated);
    }
    m->state = STATE_PLAYER_TURN;
    printf("[GAME] It is now %s's turn.\n", m->players[m->current_player_idx].name);
}

void run_game_fsm(Match_t *m, int player_id, int action_code, int card_idx, int target_id) {
    Player_t *curr_p = &m->players[m->current_player_idx];

    // Security Check
    if (player_id != -1 && m->state != STATE_SETUP && m->state != STATE_LOBBY && curr_p->id != player_id) {
        printf("[WARN] Player %d tried to act out of turn.\n", player_id);
        return;
    }

    switch (m->state) {
        case STATE_SETUP:
            printf("[FSM] Setting up game...\n");
            m->deck = create_bunch(56); 
            m->discard_pile = create_bunch(56);
            
            // Populare Deck
            for(int i = 0; i < 4; i++) {
                Card_t c; 
                strcpy(c.name, "Attack"); c.type = CARD_ATTACK; bunch_push(&m->deck, c);
                strcpy(c.name, "Skip"); c.type = CARD_SKIP; bunch_push(&m->deck, c);
                strcpy(c.name, "Favor"); c.type = CARD_FAVOR; bunch_push(&m->deck, c);
                strcpy(c.name, "Shuffle"); c.type = CARD_SHUFFLE; bunch_push(&m->deck, c);
            }
            for(int i = 0; i < 5; i++) {
                Card_t c; 
                strcpy(c.name, "See Future"); c.type = CARD_SEE_FUTURE; bunch_push(&m->deck, c);
                strcpy(c.name, "Nope"); c.type = CARD_NOPE; bunch_push(&m->deck, c);
            }
            const char* cats[] = {"Tacocat", "Cattermellon", "Potato Cat", "Beard Cat", "Rainbow Cat"};
            for(int i = 0; i < 5; i++) {
                for(int j = 0; j < 4; j++) {
                    Card_t c; strcpy(c.name, cats[i]); c.type = CARD_GENERIC; bunch_push(&m->deck, c);
                }
            }

            shuffle_bunch(&m->deck);

            // Distribuire
            for(int i = 0; i < m->count; i++) {
                Card_t defuse; defuse.type = CARD_DEFUSE; strcpy(defuse.name, "Defuse");
                bunch_push(&m->players[i].hand, defuse);
                for(int j = 0; j < 4; j++) {
                    bunch_push(&m->players[i].hand, bunch_pop(&m->deck));
                }
            }

            // Adaugare Bombe
            for(int i = 0; i < m->count - 1; i++) {
                Card_t bomb; bomb.type = CARD_EXPLODING_KITTEN; strcpy(bomb.name, "EXPLODING RACOON");
                bunch_push(&m->deck, bomb);
            }
            int defuse_ramase = 6 - m->count;
            for(int i = 0; i < defuse_ramase; i++) {
                Card_t d; d.type = CARD_DEFUSE; strcpy(d.name, "Defuse");
                bunch_push(&m->deck, d);
            }

            shuffle_bunch(&m->deck);
            m->state = STATE_PLAYER_TURN;
            break;

        case STATE_PLAYER_TURN:
            if (action_code == 0) {
                // DRAW
                m->state = STATE_DRAW_PHASE;
                run_game_fsm(m, player_id, 0, 0, -1); 
            } 
            else if (action_code == 1) {
                // PLAY CARD
                if (card_idx >= 0 && card_idx < curr_p->hand.count) {
                    Card_t played = curr_p->hand.cards[card_idx];
                    
                    // Remove card
                    for (int s = card_idx; s < curr_p->hand.count - 1; s++) {
                        curr_p->hand.cards[s] = curr_p->hand.cards[s+1];
                    }
                    curr_p->hand.count--;
                    
                    bunch_push(&m->discard_pile, played);
                    printf("Player %s played %s\n", curr_p->name, get_card_name(played.type));

                    switch (played.type) {
                        case CARD_SKIP:
                            next_turn(m);
                            break;
                        case CARD_ATTACK:
                            m->attack_turns_accumulated += 2;
                            next_turn(m);
                            break;
                        case CARD_FAVOR:
                            if (target_id != -1 && target_id != curr_p->id) {
                                Player_t *target_p = NULL;
                                for (int i = 0; i < m->count; i++) {
                                    if (m->players[i].id == target_id) target_p = &m->players[i];
                                }
                                if (target_p && target_p->hand.count > 0 && !target_p->is_eliminated) {
                                    Card_t stolen = bunch_pop(&target_p->hand);
                                    bunch_push(&curr_p->hand, stolen);
                                    printf("Stolen %s from %s\n", get_card_name(stolen.type), target_p->name);
                                }
                            }
                            break;
                        case CARD_SHUFFLE:
                            shuffle_bunch(&m->deck);
                            // Shuffle usually doesn't end turn in official rules, but if you want it to:
                            // next_turn(m); 
                            break;
                        case CARD_SEE_FUTURE: {
                            char future_msg[1024] = "LOG: [SEE FUTURE] Urmatoarele 3 carti:\n";
                            int limit = (m->deck.count < 3) ? m->deck.count : 3;
                            for(int i = 0; i < limit; i++) {
                                int idx = (m->deck.current + i) % m->deck.capacity;
                                Card_t c = m->deck.cards[idx];
                                char line[100];
                                snprintf(line, 100, "   %d. %s\n", i + 1, get_card_name(c.type));
                                strcat(future_msg, line);
                            }
                            // AICI ERA EROAREA - ACUM VA MERGE
                            send_string(curr_p->id, future_msg);
                            break;
                        }
                        default:
                            printf("Played generic %s.\n", get_card_name(played.type));
                            // Generic cards don't usually end turn unless used in pairs
                            break;
                    }
                }
            }
            break; // Break pentru STATE_PLAYER_TURN

        case STATE_DRAW_PHASE: {
            Card_t drawn = bunch_pop(&m->deck);
            printf("Player %s drew: %s\n", curr_p->name, get_card_name(drawn.type));

            if (drawn.type == CARD_EXPLODING_KITTEN) {
                m->state = STATE_EXPLOSION;
                run_game_fsm(m, player_id, 0, 0, -1);
            } else {
                bunch_push(&curr_p->hand, drawn);
                next_turn(m);
            }
            break;
        }

        case STATE_EXPLOSION: {
            int has_defuse = 0; 
            // Cautam Defuse in mana
            int defuse_idx = -1;
            for(int i=0; i<curr_p->hand.count; i++){
                if(curr_p->hand.cards[i].type == CARD_DEFUSE) {
                    has_defuse = 1;
                    defuse_idx = i;
                    break;
                }
            }

            if (has_defuse) {
                printf("Player used DEFUSE!\n");
                // Scoatem Defuse din mana
                for (int s = defuse_idx; s < curr_p->hand.count - 1; s++) {
                    curr_p->hand.cards[s] = curr_p->hand.cards[s+1];
                }
                curr_p->hand.count--;

                // Punem bomba inapoi in pachet
                Card_t bomb; bomb.type = CARD_EXPLODING_KITTEN; strcpy(bomb.name, "EXPLODING RACOON");
                bunch_push(&m->deck, bomb); 
                next_turn(m);
            } else {
                printf("BOOM! Player %s is ELIMINATED!\n", curr_p->name);
                curr_p->is_eliminated = 1;
                
                int alive = 0;
                for(int i=0; i<m->count; i++) if(!m->players[i].is_eliminated) alive++;
                
                if (alive <= 1) {
                    m->state = STATE_GAME_OVER;
                } else {
                    next_turn(m);
                }
            }
            break;
        }

        case STATE_GAME_OVER:
            printf("GAME OVER.\n");
            break;
            
        default:
            break;
    }
}