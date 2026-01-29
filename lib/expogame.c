// lib/expogame.c
#include "expogame.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>

// -----------------
// CARD HELPERS
// -----------------
const char* get_card_name(CardType type) {
    switch(type) {
        case CARD_NOPE:return "Nope(Cancel attack)";
        case CARD_ATTACK: return "Attack";
        case CARD_SKIP: return "Skip";
        case CARD_DEFUSE: return "Defuse";
        case CARD_EXPLODING_KITTEN: return "EXPLODING KITTEN";
        case CARD_SHUFFLE: return "Shuffle";
        case CARD_SEE_FUTURE: return "See Future (3x)";
        case CARD_FAVOR: return "Favor (Steal Specific)";
        case CARD_RACOON_TACO: return "Raccoon Taco";
        case CARD_RACOON_MELON: return "Raccoon Melon";
        case CARD_RACOON_POTATO: return "Raccoon Potato";
        case CARD_RACOON_BEARD: return "Raccoon Beard";
        case CARD_RACOON_RAINBOW: return "Raccoon Rainbow";
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
        if (strcmp(players[i].name, name) == 0) {
            return 1; 
        }
    }
    return 0; 
}

Player_t get_player(Player_t *players, int id, int capacity) {
    for (int i = 0; i < capacity; i++) {
        if (players[i].id == id) {
            return players[i]; 
        }
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
// CARD
// -----------------
Card_t create_card(int id) {
    Card_t c;
    c.id = id;
    c.type = CARD_GENERIC; 
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
        // printf("Bunch is full!\n"); //debug
        return;
    }
    int index = (b->current + b->count) % b->capacity;
    b->cards[index] = card;
    b->count++;
}

Card_t bunch_pop(Bunch* b) {
    Card_t empty = { -1 };
    if (b->count == 0) {
        return empty;
    }
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
        if(match->players[i].isready){
            ready_count++;
        }
    }
    match->ready = ready_count;
}

// -----------------
// FSM LOGIC
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

int remove_card_by_type(Bunch *hand, CardType type) {
    for (int i = 0; i < hand->count; i++) {
        int idx = (hand->current + i) % hand->capacity;
        if (hand->cards[idx].type == type) {
            Card_t temp = hand->cards[hand->current];
            hand->cards[hand->current] = hand->cards[idx];
            hand->cards[idx] = temp;
            bunch_pop(hand);
            return 1;
        }
    }
    return 0;
}

int get_player_id_by_name(Match_t *m, const char* name) {
    for(int i=0; i<m->count; i++) {
        if(strcmp(m->players[i].name, name) == 0) {
            return m->players[i].id;
        }
    }
    return -1;
}

int card_needs_target(CardType type) {
    if (type == CARD_FAVOR) return 1;
    if (type >= CARD_RACOON_TACO && type <= CARD_RACOON_RAINBOW) return 1;
    return 0;
}

void run_game_fsm(Match_t *m, int player_id, int action_code, int card_idx, int target_id) {
    Player_t *curr_p = &m->players[m->current_player_idx];

    if (player_id != -1 && m->state != STATE_SETUP && m->state != STATE_LOBBY && curr_p->id != player_id) {
        printf("[WARN] Player %d tried to act out of turn.\n", player_id);
        return;
    }

    switch (m->state) {
        
        case STATE_SETUP:
            printf("[FSM] Setting up game...\n");
            m->deck = create_bunch(80); 
            m->discard_pile = create_bunch(80);
            srand(time(NULL));

            CardType start_pool[] = { 
                CARD_NOPE,CARD_ATTACK, CARD_SKIP, CARD_FAVOR, CARD_SHUFFLE, CARD_SEE_FUTURE,
                CARD_RACOON_TACO, CARD_RACOON_MELON, CARD_RACOON_POTATO, CARD_RACOON_BEARD, CARD_RACOON_RAINBOW 
            };
            int pool_size = 11;

            for(int i=0; i<m->count; i++) {
                for(int j=0; j<4; j++) {
                    int r = rand() % pool_size;
                    Card_t c; c.type = start_pool[r]; strcpy(c.name, get_card_name(c.type));
                    bunch_push(&m->players[i].hand, c);
                }
                Card_t defuse; defuse.type = CARD_DEFUSE; strcpy(defuse.name, "Defuse");
                bunch_push(&m->players[i].hand, defuse);
            }

            for(int i=0; i < m->count - 1; i++) {
                Card_t bomb; bomb.type = CARD_EXPLODING_KITTEN; strcpy(bomb.name, "BOMB");
                bunch_push(&m->deck, bomb);
            }
            
            CardType all_types[] = { CARD_NOPE,CARD_ATTACK, CARD_SKIP, CARD_FAVOR, CARD_SHUFFLE, CARD_SEE_FUTURE,
                                     CARD_RACOON_TACO, CARD_RACOON_MELON, CARD_RACOON_POTATO, 
                                     CARD_RACOON_BEARD, CARD_RACOON_RAINBOW };
            for(int i=0; i < 5; i++) { 
                 for(int t=0; t<10; t++) {
                     Card_t c; c.type = all_types[t]; strcpy(c.name, get_card_name(c.type));
                     bunch_push(&m->deck, c);
                 }
            }
            
            shuffle_bunch(&m->deck);
            m->current_player_idx = 0;
            m->attack_turns_accumulated = 0;
            m->state = STATE_PLAYER_TURN;
            break;

        // --- STATE 2: PLAYER DECISION ---
        case STATE_PLAYER_TURN:
            if (action_code == 0) {
                m->state = STATE_DRAW_PHASE;
                run_game_fsm(m, player_id, 0, 0, -1); 
            } 
            else if (action_code == 1) {
                
                if (card_idx < 0 || card_idx >= curr_p->hand.count) {
                    char err[] = "Invalid card index!\n";
                    send(curr_p->id, err, strlen(err), 0);
                    return;
                }

                int actual_idx = (curr_p->hand.current + card_idx) % curr_p->hand.capacity;
                Card_t temp = curr_p->hand.cards[curr_p->hand.current];
                curr_p->hand.cards[curr_p->hand.current] = curr_p->hand.cards[actual_idx];
                curr_p->hand.cards[actual_idx] = temp;

                Card_t played = bunch_pop(&curr_p->hand); 
                bunch_push(&m->discard_pile, played);
                
                printf("Player %s played %s\n", curr_p->name, get_card_name(played.type));
                
                if (played.type == CARD_SKIP) {
                    next_turn(m);
                }else if (played.type == CARD_NOPE) {
                    if (m->attack_turns_accumulated > 0) {
                        printf("NOPE! Player %s cancelled the attack!\n", curr_p->name);

                       
                        m->attack_turns_accumulated = 0;
        
                        char msg[] = "You used NOPE! You are no longer under attack. You can now Draw safely.\n";
                         send(curr_p->id, msg, strlen(msg), 0);
                    } else {
                         char msg[] = "You wasted a NOPE! (Only works when you are Attacked in this simplified version)\n";
                         send(curr_p->id, msg, strlen(msg), 0);
                         }
                    }
                else if (played.type == CARD_ATTACK) {
                    printf("ATTACK! %s ends turn without drawing.\n", curr_p->name);
                    int extra_turns = (m->attack_turns_accumulated == 0) ? 1 : (m->attack_turns_accumulated + 2);
                    
                    m->attack_turns_accumulated = extra_turns;

                    do {
                        m->current_player_idx = (m->current_player_idx + 1) % m->count;
                    } while (m->players[m->current_player_idx].is_eliminated);

                    Player_t *victim = &m->players[m->current_player_idx];
                    printf("Attack target is now %s! They must take %d turns.\n", victim->name, extra_turns + 1);
                    
                    char msg[128];
                    snprintf(msg, 128, "You were ATTACKED by %s! You have %d turns.", curr_p->name, extra_turns + 1);
                    send(victim->id, msg, strlen(msg), 0);

                    m->state = STATE_PLAYER_TURN;
                }
                else if (played.type == CARD_SHUFFLE) {
                    shuffle_bunch(&m->deck);
                    char msg[] = "Deck Shuffled!\n";
                    send(curr_p->id, msg, strlen(msg), 0);
                }
                else if (played.type == CARD_SEE_FUTURE) {
                    char future_msg[256] = "FUTURE: ";
                    for(int i=0; i<3 && i<m->deck.count; i++) {
                        int idx = (m->deck.current + i) % m->deck.capacity;
                        strcat(future_msg, get_card_name(m->deck.cards[idx].type));
                        if(i < 2) strcat(future_msg, ", ");
                    }
                    strcat(future_msg, "\n");
                    send(curr_p->id, future_msg, strlen(future_msg), 0);
                }
                else if (played.type == CARD_FAVOR) {
                    if (target_id != -1 && target_id != curr_p->id) {
                        Player_t *target_p = NULL;
                        for(int i=0; i<m->count; i++) if(m->players[i].id == target_id) target_p = &m->players[i];
                        
                        if (target_p && target_p->hand.count > 0 && !target_p->is_eliminated) {
                            Card_t stolen = bunch_pop(&target_p->hand);
                            bunch_push(&curr_p->hand, stolen);
                            printf("%s stole %s from %s using Favor!\n", curr_p->name, get_card_name(stolen.type), target_p->name);
                        }
                    }
                }
                else if (played.type >= CARD_RACOON_TACO && played.type <= CARD_RACOON_RAINBOW) {
                    
                    if (remove_card_by_type(&curr_p->hand, played.type)) {
                        printf("%s played a PAIR of %ss!\n", curr_p->name, get_card_name(played.type));
                        
                        if (target_id != -1 && target_id != curr_p->id) {
                             Player_t *target_p = NULL;
                             for(int i=0; i<m->count; i++) if(m->players[i].id == target_id) target_p = &m->players[i];
                             
                             if (target_p && target_p->hand.count > 0 && !target_p->is_eliminated) {
                                 int rand_idx = rand() % target_p->hand.count;
                                 int idx_to_steal = (target_p->hand.current + rand_idx) % target_p->hand.capacity;
                                 
                                 Card_t t_temp = target_p->hand.cards[target_p->hand.current];
                                 target_p->hand.cards[target_p->hand.current] = target_p->hand.cards[idx_to_steal];
                                 target_p->hand.cards[idx_to_steal] = t_temp;
                                 
                                 Card_t stolen = bunch_pop(&target_p->hand);
                                 bunch_push(&curr_p->hand, stolen);
                                 
                                 char msg[100]; snprintf(msg, 100, "You stole %s from %s!\n", get_card_name(stolen.type), target_p->name);
                                 send(curr_p->id, msg, strlen(msg), 0);
                             } else {
                                 send(curr_p->id, "Target has no cards.\n", 22, 0);
                             }
                        } else {
                            send(curr_p->id, "Invalid target for Pair.\n", 26, 0);
                        }
                    } else {
                        char msg[] = "Single Raccoon played (useless without a pair).\n";
                        send(curr_p->id, msg, strlen(msg), 0);
                    }
                }
            }
            break;

       
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

        // --- STATE 4: EXPLOSION ---
        case STATE_EXPLOSION: {
            int has_defuse = 0; 
            int defuse_idx = -1;
            for(int i=0; i < curr_p->hand.count; i++) {
                int idx = (curr_p->hand.current + i) % curr_p->hand.capacity;
                if (curr_p->hand.cards[idx].type == CARD_DEFUSE) {
                    has_defuse = 1; defuse_idx = idx; break;
                }
            }

            if (has_defuse) {
                printf("Player %s used DEFUSE!\n", curr_p->name);
                
                Card_t temp = curr_p->hand.cards[curr_p->hand.current];
                curr_p->hand.cards[curr_p->hand.current] = curr_p->hand.cards[defuse_idx];
                curr_p->hand.cards[defuse_idx] = temp;
                bunch_pop(&curr_p->hand); 
                
                Card_t bomb; bomb.type = CARD_EXPLODING_KITTEN; strcpy(bomb.name, "BOMB");
            
                bunch_push(&m->deck, bomb);
                shuffle_bunch(&m->deck); 
                
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
            
        default: break;
    }
}