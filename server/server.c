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

int players_count = 0;
int players_capacity = MATCH_NUM * 5;

int send_string(int sock, const char *str) {
    char message[BUFFER_SIZE];

    // Prevent overflow:
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

        if (c == '\n') break;   // end of message
        if (c == '\0') break;   // just in case

        buffer[total++] = c;
    }

    buffer[total] = '\0';
    return 0;
}



// Enter player name and create user
void enter_name(int client_sock) { 
    char buffer[BUFFER_SIZE];
    while (1)
    {       
        if (send_string(client_sock, "Enter your name:") < 0) {
            perror("send_string");
            exit(1);
        }

        if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) {
            perror("recv_string");
            exit(1);
        }

        if (!exist_player(Players, buffer, players_count)) {
            send_string(client_sock, "You Join!");
            Player_t user = create_user(buffer,client_sock);
            printf("%s joined the game.\n", user.name);
            add_player(Players, user, &players_count);
        } else {
            send_string(client_sock, "Name already taken. Try again:");
            printf("%d : Continue Loop for name\n",client_sock);
            continue;
        }
        break;
    }
}
void select_match(int client_sock) {
    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';
    print_matches(matches, MATCH_NUM, buffer);
    Player_t player = get_player(Players,client_sock,players_count);
    while (1)
    {       
        if (send_string(client_sock, buffer) < 0) {
            perror("send_string");
            exit(1);
        }
        printf("Available Matches:\n");
        replace_space_with_newline(buffer);
        printf("\n%s\n", buffer);

        if (recv_string(client_sock, buffer, BUFFER_SIZE) < 0) {
            perror("recv_string");
            exit(1);
        }
        int id = atoi(buffer);
        printf("Client %d selected Match ID: %d\n", client_sock, id);
        id = id - 1;
        if (id < 0 || id >= MATCH_NUM || is_match_full(matches[id])) {
            send_string(client_sock, "Invalid Match ID Or Match is already full. Try again:");
            continue;
        }else{
            send_string(client_sock, "You Join!");
            printf("%s joined Match %d.\n", player.name, id);
            add_match_player(&matches[id], player);
            break;
        }
    }
}

// void setReady(int client_sock,Match_t** mathches){
//     char buffer[BUFFER_SIZE];
//     int matchid;
//     int playerid;
//     int ready;
//     if (recv_string(client_sock, buffer, BUFFER_SIZE) == 0) {
//         sscanf(buffer,"%d %d %d",&matchid,&playerid,&ready);
//         if(!ready){
//             mathches[matchid]->count--;
//             close(client_sock);
//         }
//     }
// }

void* client_thread(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    enter_name(client_sock);
    select_match(client_sock);
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock;
    struct sockaddr_in server_addr;
    int opt = 1;
    for(int i=0;i<MATCH_NUM;i++){
        matches[i] = create_match(i,5);
    }
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("Socket error"); exit(1); }

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
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

    if (listen(server_sock, 5) < 0) {
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
        pthread_detach(tid); // prevents memory leaks
    }
    close(server_sock);
    return 0;
}
