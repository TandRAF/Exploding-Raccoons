#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "expogame.h"
#include <arpa/inet.h>
#include <errno.h>
#include <ctype.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MATCH_NUM 3  

Match_t matches[MATCH_NUM];
Player_t Players[MATCH_NUM*5];

pthread_mutex_t lock; 

int players_count = 0;
int players_capacity = MATCH_NUM * 5; 

// ------------------------------------------------------------------
// NETWORK HELPERS
// ------------------------------------------------------------------

int send_string(int sock, const char *str) {
    char message[BUFFER_SIZE];
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

        if (received == 0) return -1;
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
        id = id - 1; // Adjust for 0-index

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
        if (my_match->state == STATE_LOBBY) {
            if (my_match->ready == my_match->count && my_match->count >= 2) {
                my_match->state = STATE_SETUP;
                // Run Setup logic (System call -1)
                run_game_fsm(my_match, -1, 0, 0, -1);
                
                char msg[100];
                snprintf(msg, 100, "Game Started! Player %s goes first.", 
                         my_match->players[my_match->current_player_idx].name);
                broadcast_msg(my_match, msg);
            }
        }
        else if (my_match->state == STATE_PLAYER_TURN) {
            int current_idx = my_match->current_player_idx;
            if (my_match->players[current_idx].id == client_sock) {
                
                pthread_mutex_unlock(&lock);
                send_string(client_sock, "YOUR TURN:\n[0] Draw\n[1] Play Specific Card\n[2] See Hand\n[3] List Players");
                
                char buffer[BUFFER_SIZE];
                if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) break;
                
                if (strlen(buffer) == 0 || !isdigit(buffer[0])) {
                    send_string(client_sock, "Invalid input. Please enter a number [0-3].");
                    continue;
                }

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
                else if (action == 3) {
                    char list_msg[1024] = "Players in Match:\n";
                    pthread_mutex_lock(&lock);
                    for(int i=0; i<my_match->count; i++) {
                        char line[100];
                        snprintf(line, 100, "ID: %d - %s\n", my_match->players[i].id, my_match->players[i].name);
                        strcat(list_msg, line);
                    }
                    pthread_mutex_unlock(&lock);
                    send_string(client_sock, list_msg);
                    continue;
                }

                if (action == 1) {
                    send_string(client_sock, "Enter Card Index (look at [2] See Hand):");
                    if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) break;
                    
                    if (!isdigit(buffer[0])) {
                        send_string(client_sock, "Invalid index format.");
                        continue;
                    }
                    card_idx = atoi(buffer);

                    pthread_mutex_lock(&lock);
                    Player_t *p = &my_match->players[current_idx];
                    
                    if (card_idx < 0 || card_idx >= p->hand.count) {
                        pthread_mutex_unlock(&lock);
                        send_string(client_sock, "Invalid card index number.");
                        continue;
                    }

                    int actual_idx = (p->hand.current + card_idx) % p->hand.capacity;
                    CardType type = p->hand.cards[actual_idx].type;
                    
                    int needs_target = card_needs_target(type);
                    pthread_mutex_unlock(&lock);

                    if (needs_target) {
                        send_string(client_sock, "Enter Target Player NAME:");
                        if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) break;
                        
                        pthread_mutex_lock(&lock);
                        target_id = get_player_id_by_name(my_match, buffer);
                        pthread_mutex_unlock(&lock);

                        if (target_id == -1) {
                            send_string(client_sock, "Player not found! Check spelling (use [3] List Players).");
                            continue;
                        }
                    } else {
                        target_id = -1;
                    }
                }

                pthread_mutex_lock(&lock);
                run_game_fsm(my_match, client_sock, action, card_idx, target_id);
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

// ------------------------------------------------------------------
// MAIN
// ------------------------------------------------------------------

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