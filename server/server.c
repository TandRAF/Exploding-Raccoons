#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "expogame.h"
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MATCH_NUM 3  

Match_t matches[MATCH_NUM];
Player_t Players[MATCH_NUM*5];

pthread_mutex_t lock; 

int players_count = 0;
int players_capacity = MATCH_NUM * 5; 

// --- NETWORK HELPERS ---
// NOTĂ: send_string și recv_string au fost mutate în expogame.c
// Am păstrat doar broadcast_msg aici pentru că este specifică logicii de server

void broadcast_msg(Match_t *m, const char* msg) {
    for (int i = 0; i < m->count; i++) {
        if (m->players[i].id > 0) {
            send_string(m->players[i].id, msg);
        }
    }
}

// ------------------------------------------------------------------
// LOBBY LOGIC
// ------------------------------------------------------------------

void enter_name(int client_sock) { 
    char buffer[BUFFER_SIZE];
    while (1) {       
        if (send_string(client_sock, "Enter your name:") < 0) exit(1);
        if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) exit(1);

        pthread_mutex_lock(&lock);
        if (!exist_player(Players, buffer, players_count)) {
            Player_t user = create_user(buffer, client_sock);
            print_player(user);
            add_player(Players, user, &players_count);
            pthread_mutex_unlock(&lock);
            
            send_string(client_sock, "You Join!");
            printf("%s joined the game.\n", user.name);
            break;
        } else {
            pthread_mutex_unlock(&lock);
            send_string(client_sock, "Name already taken. Try again:");
            continue;
        }
    }
}

void select_match(int client_sock) {
    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';
    
    print_matches(matches, MATCH_NUM, buffer);
    
    pthread_mutex_lock(&lock);
    Player_t player = get_player(Players, client_sock, players_count);
    pthread_mutex_unlock(&lock);

    while (1) {       
        if (send_string(client_sock, buffer) < 0) exit(1);
        
        char display_buf[BUFFER_SIZE];
        strcpy(display_buf, buffer);
        replace_space_with_newline(display_buf);
        printf("Available Matches:\n%s\n", display_buf);

        if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) exit(1);
        
        int id = atoi(buffer);
        id = id - 1; 

        if (id < 0 || id >= MATCH_NUM) {
            send_string(client_sock, "Invalid Match ID. Try again:");
            continue;
        }

        pthread_mutex_lock(&lock);
        if (is_match_full(matches[id])) {
            pthread_mutex_unlock(&lock);
            send_string(client_sock, "Match is full. Try again:");
            continue;
        }

        send_string(client_sock, "You Join!");
        printf("%s joined Match %d.\n", player.name, id);
        add_match_player(&matches[id], player);
        pthread_mutex_unlock(&lock);
        break;
    }
}

void setReady(int client_sock){
    char buffer[BUFFER_SIZE];
    if (send_string(client_sock, "Set Ready? (y):") < 0) return;
    if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) return;

    if (strcmp(buffer, "y") == 0) {
        pthread_mutex_lock(&lock);
        Match_t *m = get_player_match(matches, client_sock);
        if (m) {
            for (int i = 0; i < m->count; i++) {
                if (m->players[i].id == client_sock) {
                    m->players[i].isready = 1;
                    printf("%s is Ready.\n", m->players[i].name);
                    verify_ready(m);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&lock);
    }
}

// ------------------------------------------------------------------
// THREAD LOOP
// ------------------------------------------------------------------

void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);

    enter_name(client_sock);
    select_match(client_sock);
    setReady(client_sock);

    while (1) {
        pthread_mutex_lock(&lock);
        Match_t *my_match = get_player_match(matches, client_sock);

        if (!my_match) {
            pthread_mutex_unlock(&lock);
            break; 
        }

        // --- LOGICA DE START JOC ---
        if (my_match->state == STATE_LOBBY) {
            if (my_match->ready == my_match->count && my_match->count >= 2) {
                my_match->state = STATE_SETUP;
                run_game_fsm(my_match, -1, 0, 0, -1);
                
                char msg[100];
                snprintf(msg, 100, "Game Started! Player %s goes first.", 
                         my_match->players[my_match->current_player_idx].name);
                broadcast_msg(my_match, msg); 
            }
        }
        // --- LOGICA DE TURN ---
       else if (my_match->state == STATE_PLAYER_TURN) {
            int current_idx = my_match->current_player_idx;
            
            if (my_match->players[current_idx].id == client_sock) {
                pthread_mutex_unlock(&lock);
                
                send_string(client_sock, "YOUR TURN: [0] Draw, [1] Play Card, [2] See Hand");
                
                char buffer[BUFFER_SIZE];
                if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) break;
                
                int action = atoi(buffer);
                int card_idx = 0; 
                int target_id = -1;

                if (action == 2) {
                    char hand_msg[2048];
                    pthread_mutex_lock(&lock);
                    print_hand(&my_match->players[current_idx].hand, hand_msg);
                    pthread_mutex_unlock(&lock);
                    send_string(client_sock, hand_msg);
                    continue; 
                }

                if (action == 1) {
                    send_string(client_sock, "Enter Card Index:");
                    if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) break;
                    card_idx = atoi(buffer);
                    
                    // Citire Target ID (trimis mereu de client, chiar daca e -1)
                    if (recv_string(client_sock, buffer, BUFFER_SIZE) >= 0) {
                        target_id = atoi(buffer);
                    }
                }

                pthread_mutex_lock(&lock);
                
                char played_card_name[64] = "o carte";
                if (action == 1 && card_idx < my_match->players[current_idx].hand.count) {
                    Card_t c = my_match->players[current_idx].hand.cards[card_idx];
                    strcpy(played_card_name, get_card_name(c.type));
                }

                run_game_fsm(my_match, client_sock, action, card_idx, target_id);

                char broadcast_buf[256];
                if (action == 1) {
                    snprintf(broadcast_buf, 256, "LOG: Jucatorul %s a jucat %s", 
                             my_match->players[current_idx].name, played_card_name);
                } else {
                    snprintf(broadcast_buf, 256, "LOG: Jucatorul %s a tras o carte.", 
                             my_match->players[current_idx].name);
                }

                for (int i = 0; i < my_match->count; i++) {
                    send_string(my_match->players[i].id, broadcast_buf);
                    
                    char turn_info[128];
                    snprintf(turn_info, 128, "LOG: Este randul lui: %s", 
                             my_match->players[my_match->current_player_idx].name);
                    send_string(my_match->players[i].id, turn_info);

                    if (my_match->players[i].id == my_match->players[my_match->current_player_idx].id) {
                        send_string(my_match->players[i].id, "YOUR TURN");
                    }

                    if (my_match->players[i].id == client_sock) {
                        char hand_auto[2048];
                        print_hand(&my_match->players[i].hand, hand_auto);
                        send_string(my_match->players[i].id, hand_auto);
                    }
                }
            } else {
                pthread_mutex_unlock(&lock);
                usleep(500000); 
                continue;
            }
        }
        else if (my_match->state == STATE_GAME_OVER) {
            send_string(client_sock, "GAME OVER! Thanks for playing.");
            pthread_mutex_unlock(&lock);
            break;
        }

        pthread_mutex_unlock(&lock);
        usleep(200000);
    }

    close(client_sock);
    return NULL;
}

int main() {
    pthread_mutex_init(&lock, NULL);
    int server_sock;
    struct sockaddr_in server_addr;
    int opt = 1;

    for(int i=0; i<MATCH_NUM; i++){
        matches[i] = create_match(i, 5);
        matches[i].state = STATE_LOBBY; 
    }

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("Socket error"); exit(1); }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_sock);
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind error");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, MATCH_NUM*5) < 0) {
        perror("Listen error");
        close(server_sock);
        exit(1);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) {
            perror("Accept error");
            continue;
        }
        int* sock_ptr = malloc(sizeof(int));
        *sock_ptr = client_sock;
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, sock_ptr);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}