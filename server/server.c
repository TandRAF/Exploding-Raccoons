#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "expogame.h" // Presupunem că Player_t etc. sunt aici
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

// === FUNCȚII DE COMUNICARE (Neschimbate) ===

int send_string(int sock, const char *str) {
    char message[BUFFER_SIZE];
    size_t len = snprintf(message, sizeof(message), "%s\n", str);
    if (len >= sizeof(message)) return -1;

    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(sock, message + total, len - total, 0);
        if (sent < 0) return -1;
        total += sent;
    }
    return 0;
}

int recv_string(int sock, char *buffer, size_t max_len) {
    size_t total = 0;
    while (total < max_len - 1) {
        char c;
        ssize_t received = recv(sock, &c, 1, 0);
        if (received <= 0) return -1;
        if (c == '\n') break;
        buffer[total++] = c;
    }
    buffer[total] = '\0';
    return 0;
}

// === FUNCȚIE DE BROADCAST (Neschimbată) ===

void broadcast_lobby_update(Match_t *match) {
    if (!match) return;
    char buffer[1024] = "LOBBY_UPDATE:";
    
    // Asigură-te că numele și starea sunt extrase corect
    for(int i = 0; i < match->count; i++) {
        char temp[128];
        snprintf(temp, sizeof(temp), "%s:%d|", 
                 match->players[i].name, 
                 match->players[i].isready);
        strcat(buffer, temp);
    }
    
    // Trimitem la toți jucătorii din match
    for(int i = 0; i < match->count; i++) {
        send_string(match->players[i].id, buffer);
    }
}

// === ETAPELE DE CONECTARE (Corectate) ===

void enter_name(int client_sock) { 
    char buffer[BUFFER_SIZE];
    while (1) {       
        send_string(client_sock, "Enter your name:");
        recv_string(client_sock, buffer, BUFFER_SIZE);

        pthread_mutex_lock(&lock);
        if (!exist_player(Players, buffer, players_count)) {
            send_string(client_sock, "You Join!");
            Player_t user = create_user(buffer, client_sock);
            printf("%s joined the game.\n", user.name);
            add_player(Players, user, &players_count);
            pthread_mutex_unlock(&lock);
            break;
        } else {
            pthread_mutex_unlock(&lock);
            send_string(client_sock, "Name already taken. Try again:");
        }
    }
}

void select_match(int client_sock) {
    char buffer[BUFFER_SIZE] = {0};
    print_matches(matches, MATCH_NUM, buffer);
    
    // Găsim jucătorul proaspăt conectat din lista globală
    Player_t player;
    pthread_mutex_lock(&lock);
    for(int i = 0; i < players_count; i++) {
        if (Players[i].id == client_sock) {
            player = Players[i];
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    
    while (1) {       
        send_string(client_sock, buffer); // Trimite lista de lobby-uri
        recv_string(client_sock, buffer, BUFFER_SIZE);
        int id = atoi(buffer) - 1;
        
        if (id >= 0 && id < MATCH_NUM) {
            
            // Re-verificăm dacă match-ul e plin sub lock
            pthread_mutex_lock(&lock);
            if (!is_match_full(matches[id])) {
                send_string(client_sock, "You Join!");
                printf("%s joined Match %d.\n", player.name, id);
                
                // Actualizăm matchid în lista globală Players
                for(int i = 0; i < players_count; i++) {
                    if (Players[i].id == client_sock) {
                        Players[i].matchid = id;
                        // Adăugăm jucătorul actualizat în match
                        add_match_player(&matches[id], Players[i]); 
                        break;
                    }
                }
                
                broadcast_lobby_update(&matches[id]);  // Trimite starea actuală a lobby-ului
                pthread_mutex_unlock(&lock);
                break;
            } else {
                pthread_mutex_unlock(&lock);
                send_string(client_sock, "Match is already full. Try again:");
            }
        } else {
            send_string(client_sock, "Invalid Match ID. Try again:");
        }
    }
}


void setReady(int client_sock, const char *status) {
    int is_ready_val = strcmp(status, "READY") == 0 ? 1 : 0;
    
    pthread_mutex_lock(&lock);
    
    // 1. Actualizează starea în lista globală Players
    for (int i = 0; i < players_count; i++) {
        if (Players[i].id == client_sock) {
            Players[i].isready = is_ready_val;
            printf("%s set Ready status to %d.\n", Players[i].name, is_ready_val);
            break;
        }
    }
    
    Match_t *match = get_player_match(matches, client_sock);
    if (match) {
        // 2. Actualizează starea în structura Match_t
        for(int i = 0; i < match->count; i++) {
            if(match->players[i].id == client_sock) {
                match->players[i].isready = is_ready_val;
                break;
            }
        }
        
        // 3. Trimite actualizarea la toți clienții din match
        broadcast_lobby_update(match);
    }
    
    pthread_mutex_unlock(&lock);
}

void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    
    enter_name(client_sock);
    select_match(client_sock);
    
    // Bucla principală de așteptare a comenzilor
    while (1) {
        if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) {
            // Conexiunea s-a închis sau eroare
            break;
        }
        
        if (strcmp(buffer, "READY") == 0 || strcmp(buffer, "UNREADY") == 0) {
            setReady(client_sock, buffer);
        } else {
            // Logica jocului (PLAY_CARD, DRAW, etc.) va veni aici
            printf("Received command from %d: %s\n", client_sock, buffer);
        }
    }
    
    // Deconectare (TODO: Implementarea logicăi de deconectare a jucătorului din Players și Match)
    printf("Client %d disconnected.\n", client_sock);
    
    close(client_sock);
    return NULL;
}

int main() {
    pthread_mutex_init(&lock, NULL);
    for(int i = 0; i < MATCH_NUM; i++) {
        matches[i] = create_match(i, 5);
    }

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 15);

    printf("Server listening on port %d...\n", PORT);
    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock < 0) continue;
        int* sock_ptr = malloc(sizeof(int));
        *sock_ptr = client_sock;
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, sock_ptr);
        pthread_detach(tid);
    }
    return 0;
}