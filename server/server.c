#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "expogame.h"

#define PORT 8080
#define MATCH_NUM 3
#define BUFFER_SIZE 1024

Match_t matches[MATCH_NUM];
Player_t Players[MATCH_NUM*5];
int players_count = 0;

int send_data(int sock, const void *data, uint32_t len) {
    uint32_t net_len = htonl(len);
    if (send(sock, &net_len, sizeof(net_len), 0) != sizeof(net_len)) return -1;

    size_t total = 0;
    const char *ptr = (const char*)data;
    while (total < len) {
        ssize_t sent = send(sock, ptr + total, len - total, 0);
        if (sent <= 0) return -1;
        total += sent;
    }
    return 0;
}

int recv_data(int sock, void *buffer, uint32_t max_len, uint32_t *out_len) {
    uint32_t net_len;
    if (recv(sock, &net_len, sizeof(net_len), MSG_WAITALL) != sizeof(net_len)) return -1;

    uint32_t len = ntohl(net_len);
    if (len > max_len) return -1;

    if (recv(sock, buffer, len, MSG_WAITALL) != len) return -1;
    if (out_len) *out_len = len;
    return 0;
}

// Enter player name
void enter_name(int client_sock) {
    char buffer[BUFFER_SIZE];
    while (1) {
        const char *prompt = "Enter your name:";
        send_data(client_sock, prompt, strlen(prompt)+1);

        if (recv_data(client_sock, buffer, BUFFER_SIZE, NULL) < 0) {
            perror("recv_data");
            return;
        }

        if (!exist_player(Players, buffer, players_count)) {
            Player_t user = create_user(buffer, client_sock);
            add_player(Players, user, &players_count);
            const char *msg = "You Join!";
            send_data(client_sock, msg, strlen(msg)+1);
            break;
        } else {
            const char *msg = "Name already taken. Try again:";
            send_data(client_sock, msg, strlen(msg)+1);
        }
    }
}

// Select match
void select_match(int client_sock) {
    char buffer[BUFFER_SIZE];
    Player_t player = get_player(Players, client_sock, players_count);

    while (1) {
        // Build match list
        char match_list[BUFFER_SIZE];
        print_matches(matches, MATCH_NUM, match_list);
        send_data(client_sock, match_list, strlen(match_list)+1);

        if (recv_data(client_sock, buffer, BUFFER_SIZE, NULL) < 0) {
            perror("recv_data");
            return;
        }

        int id = atoi(buffer) - 1;
        if (id < 0 || id >= MATCH_NUM || is_match_full(matches[id])) {
            const char *msg = "Invalid Match ID or Match is full. Try again:";
            send_data(client_sock, msg, strlen(msg)+1);
            continue;
        } else {
            add_match_player(matches[id], player);
            const char *msg = "You Join!";
            send_data(client_sock, msg, strlen(msg)+1);
            break;
        }
    }
}

void* client_thread(void *arg) {
    int client_sock = *(int*)arg;
    free(arg);

    enter_name(client_sock);
    select_match(client_sock);

    close(client_sock);
    return NULL;
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    int opt = 1;

    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, pclient);
        pthread_detach(tid);
    }

    close(server_sock);
    return 0;
}
