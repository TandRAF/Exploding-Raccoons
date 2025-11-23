#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "expogame.h"

#define PORT 8080
#define BUFFER_SIZE 1024

// Generic send for any data
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

// Generic receive for any data
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
void enter_name(int sock, Player_t *user) {
    char buffer[BUFFER_SIZE];
    while (1) {
        recv_data(sock, buffer, BUFFER_SIZE, NULL);
        printf("%s\n", buffer); // prompt from server

        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        send_data(sock, buffer, strlen(buffer)+1);

        recv_data(sock, buffer, BUFFER_SIZE, NULL);
        if (strstr(buffer, "Name already taken") != NULL) {
            printf("%s\n", buffer);
            continue;
        } else {
            *user = create_user(buffer, sock);
            printf("You Joined as %s\n", user->name);
            break;
        }
    }
}

// Select match
void select_match(int sock, Player_t player) {
    char buffer[BUFFER_SIZE];
    while (1) {
        recv_data(sock, buffer, BUFFER_SIZE, NULL);

        // Replace spaces with newlines for better display
        for (int i=0; buffer[i]; i++) {
            if (buffer[i] == ' ') buffer[i] = '\n';
        }
        printf("%s\n", buffer);

        printf("Select Match ID: ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        send_data(sock, buffer, strlen(buffer)+1);

        recv_data(sock, buffer, BUFFER_SIZE, NULL);
        if (strstr(buffer, "Invalid Match ID") != NULL) {
            printf("%s\n", buffer);
            continue;
        } else {
            printf("%s\n", buffer); // e.g., "You Join!"
            break;
        }
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    Player_t player;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("Socket error"); exit(1); }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect error"); exit(1);
    }

    printf("Connected to server.\n");

    enter_name(sock, &player);
    select_match(sock, player);

    close(sock);
    return 0;
}
