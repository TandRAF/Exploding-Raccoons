#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "expogame.h" 
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024

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

// -------------------------------------------------
// LOBBY PHASE
// -------------------------------------------------
void enter_name(int sock, Player_t *user) {
    char name[BUFFER_SIZE];
    char resp[BUFFER_SIZE];
    
    while (1) {
        if (recv_string(sock, resp, BUFFER_SIZE) < 0) exit(1);
        printf("%s\n", resp);

        if (fgets(name, sizeof(name), stdin) == NULL) exit(1);
        name[strcspn(name, "\n")] = '\0'; 

        if (send_string(sock, name) < 0) exit(1);
        if (recv_string(sock, resp, BUFFER_SIZE) < 0) exit(1);

        if (strstr(resp, "Name already taken") != NULL) {
            printf("%s\n", resp);
            continue;
        } else {
            *user = create_user(name, sock);
            printf("%s\n", resp); 
            break;
        }
    }
}

void select_match(int sock, Player_t player) {
    char buffer[BUFFER_SIZE];
    while (1) {
        if (recv_string(sock, buffer, BUFFER_SIZE) < 0) exit(1);
        printf("Available Matches:\n");
        replace_space_with_newline(buffer); 
        printf("%s\n", buffer);
        
        printf("Select Match ID: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) exit(1);
        buffer[strcspn(buffer, "\n")] = '\0';

        if (send_string(sock, buffer) < 0) exit(1);
        if (recv_string(sock, buffer, BUFFER_SIZE) < 0) exit(1);

        if (strstr(buffer, "You Join") != NULL) {
            printf("%s\n", buffer);
            break;
        } else {
            printf("%s\n", buffer); 
            continue;
        }
    }
}

void setReady(int sock, Player_t player){
    char buffer[BUFFER_SIZE];
    if (recv_string(sock, buffer, BUFFER_SIZE) < 0) exit(1);
    printf("%s\n", buffer);
    
    while (1) {
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) exit(1);
        buffer[strcspn(buffer, "\n")] = '\0';
        if (strcmp(buffer, "y") == 0) break;
        printf("Please type 'y' to start: ");
    }
    if (send_string(sock, buffer) < 0) exit(1);
    printf("Waiting for other players...\n");
}

// -------------------------------------------------
// MAIN
// -------------------------------------------------
int main() {
    int sock;
    Player_t player;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("Socket error"); exit(1); }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect error");
        exit(1);
    }

    printf("Connected to server.\n");

    enter_name(sock, &player);
    select_match(sock, player);
    setReady(sock, player);

    char buffer[BUFFER_SIZE];
    while (1) {
        if (recv_string(sock, buffer, BUFFER_SIZE) < 0) {
            printf("Server disconnected.\n");
            break;
        }

        printf("%s\n", buffer);
        if (strstr(buffer, "YOUR TURN") != NULL || 
            strstr(buffer, "Enter Target") != NULL ||
            strstr(buffer, "Enter Card Index") != NULL) {
            
            printf("> ");
            char input[100];
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = '\0';
                send_string(sock, input);
            }
        }
        else if (strstr(buffer, "GAME OVER") != NULL) {
            printf("Exiting...\n");
            break;
        }
    }
    
    close(sock);
    return 0;
}