#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "expogame.h"

#define PORT 8080
#define BUFFER_SIZE 1024

int send_string(int sock, const char* str) {
    ssize_t sent = send(sock, str, strlen(str) + 1, 0);
    if (sent < 0) {
        perror("send_string error");
        return -1;
    }
    return 0;
}

int recv_string(int sock, char* buffer, size_t max_len) {
    ssize_t received = recv(sock, buffer, max_len, 0);
    if (received <= 0) {
        return -1;
    }
    buffer[received < max_len ? received : max_len - 1] = '\0';
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
            *user = create_user(name);
            printf("You Joined as %s\n", user->name); // e.g. "You Join!"
        }
        break;
    }
}
// void select_match(int sock, Player_t player) {
//     char buffer[BUFFER_SIZE];
//     // Get the list of matches
//     recv_string(sock, buffer, BUFFER_SIZE);
//     printf("%s\n", buffer);
//     // Enter your match choice
//     printf("Enter match number: ");
//     scanf("%s", buffer);
//     send_string(sock, buffer);
//     // Get match ID and player ID (or "0" if invalid)
//     recv_string(sock, buffer, BUFFER_SIZE);
//     printf("got %s\n",buffer);
//     if (strcmp(buffer, "0\n") == 0) {
//         printf("Match is full or doesn't exist.\n");
//         select_match(sock, player); // retry
//         return;
//     }
//     // Parse "matchid playerid"
//     sscanf(buffer, "%d %d", &player.matchid, &player.id);
//     printf("Joined match %d as player %s.\n", player.matchid, player.name);
// }
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
    close(sock);
    return 0;
}

