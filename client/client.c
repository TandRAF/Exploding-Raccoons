#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "expogame.h"

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
        if (received == 0) {
            return -1;
        }
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

int recv_multiline(int sock) {
    char buffer[BUFFER_SIZE];
    while (1) {
        if (recv_string(sock, buffer, BUFFER_SIZE) < 0)
            return -1;

        if (strcmp(buffer, "END") == 0)
            break;

        printf("%s\n", buffer);
    }
    return 0;
}


void enter_name(int sock, Player_t *user) {
    char name[BUFFER_SIZE];
    char resp[BUFFER_SIZE];
    while (1) {
        if (recv_string(sock, resp, BUFFER_SIZE) < 0) {
            perror("recv_string");
            exit(1);
        }
        printf("%s\n", resp);

        if (fgets(name, sizeof name, stdin) == NULL) {
            perror("fgets");
            exit(1);
        }
        name[strcspn(name, "\n")] = '\0';

        if (send_string(sock, name) < 0) {
            perror("send_string");
            exit(1);
        }

        if (recv_string(sock, resp, BUFFER_SIZE) < 0) {
            perror("recv_string");
            exit(1);
        }

        if (strstr(resp, "Name already taken. Try again:") != NULL) {
            printf("%s\n", resp);
            continue;
        } else {
            *user = create_user(name, sock);
            printf("You Joined as %s\n", user->name); // e.g. "You Join!"
        }
        break;
    }
}
void select_match(int sock, Player_t player) {
    char buffer[BUFFER_SIZE];
    while (1) {
        if (recv_string(sock, buffer, BUFFER_SIZE) < 0) {
            perror("recv_string");
            exit(1);
        }
        printf("Available Matches:\n");
        replace_space_with_newline(buffer);
        printf("%s\n", buffer);
        printf("Select Match ID: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            perror("fgets");
            exit(1);
        }
        buffer[strcspn(buffer, "\n")] = '\0';

        if (send_string(sock, buffer) < 0) {
            perror("send_string");
            exit(1);
        }

        if (recv_string(sock, buffer, BUFFER_SIZE) < 0) {
            perror("recv_string");
            exit(1);
        }

        if (strcmp(buffer, "Invalid Match ID Or Match is already full. Try again:") == 0) {
            printf("%s\n", buffer);
            continue;
        } else {
            printf("%s\n", buffer);  // e.g., "You Join!"
            break;
        }
    }
}

// int setReady(int sock, Player_t player){
//     char buffer[BUFFER_SIZE];
//     char input[2];
//     printf("click r for ready or q to quit\n");
//     scanf("%s",input);
//     if(input[0] == 'q'){
//         return 0;
//     }
//     snprintf(buffer,4,"%d %d",player.matchid,player.id);
//     send_string(sock,buffer);
//     return 1;
// }

int main() {
    int sock;
    Player_t player;
    struct sockaddr_in server_addr;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket error");
        exit(1);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect error");
        exit(1);
    }

    printf("Connected to server. Listening for messages...\n");
    enter_name(sock,&player);
    select_match(sock,player);
    close(sock);
    return 0;
}

