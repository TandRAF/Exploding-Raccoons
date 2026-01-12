#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 600

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>

#define PORT 8080
#define MAX_PLAYERS 5
#define MAX_HAND_SIZE 20
#define LOG_HEIGHT 10
#define LOG_WIDTH 40

#define SCREEN_NAME_INPUT 0
#define SCREEN_LOBBY_SELECT 1
#define SCREEN_LOBBY_WAIT 2
#define SCREEN_GAME 3

typedef enum {
    CARD_EXPLODING_RACOON=0, CARD_DEFUSE=1, CARD_ATTACK=2, CARD_SKIP=3, CARD_FAVOR=4,
    CARD_SHUFFLE=5, CARD_SEE_THE_FUTURE=6, CARD_NOPE=7, CARD_RACOON_TACO=8,
    CARD_RACOON_MELON=9, CARD_RACOON_POTATO=10, CARD_RACOON_BEARD=11, CARD_RACOON_RAINBOW=12,
    CARD_NONE
} CardType;

typedef struct { CardType type; int id; } Card;

typedef struct {
    char name[32];
    int card_count;
    int is_turn;
    int is_exploded;
    int id;
    int is_ready;
} PlayerInfo;

typedef struct {
    int current_screen;
    char player_name[32];
    int my_id;
    int selected_lobby_index;

    // lobby
    int player_count;
    PlayerInfo players[MAX_PLAYERS];
    int is_ready;

    // joc
    Card hand[MAX_HAND_SIZE];
    int hand_size;
    int selected_card_idx;
    int is_turn; // New: Track if it's my turn

    int cards_in_deck;
    Card last_played_card;

    char game_log[LOG_HEIGHT][LOG_WIDTH];

    // network
    int sock;
    int connected;
} GameState;

// === UI FUNCTIONS (Keep Identical or Simplified) ===
const char* get_card_name(CardType t) {
    switch(t) {
        case CARD_EXPLODING_RACOON: return "EXPLODING RACOON";
        case CARD_DEFUSE:           return "DEFUSE";
        case CARD_ATTACK:           return "ATTACK (2x)";
        case CARD_SKIP:             return "SKIP";
        case CARD_FAVOR:            return "FAVOR";
        case CARD_SHUFFLE:          return "SHUFFLE";
        case CARD_SEE_THE_FUTURE:   return "SEE THE FUTURE (3x)";
        case CARD_NOPE:             return "NOPE";
        default: return "RACOON";
    }
}

int get_card_color(CardType t) {
    if(t == CARD_EXPLODING_RACOON) return 1;
    if(t == CARD_DEFUSE) return 2;
    if(t == CARD_ATTACK || t == CARD_SKIP) return 3;
    return 5;
}

void add_log(GameState *s, const char *msg) {
    for(int i=0; i<LOG_HEIGHT-1; i++) strncpy(s->game_log[i], s->game_log[i+1], LOG_WIDTH);
    strncpy(s->game_log[LOG_HEIGHT-1], msg, LOG_WIDTH-1);
    s->game_log[LOG_HEIGHT-1][LOG_WIDTH-1] = '\0';
}

void center_text(int y, const char *t) { mvprintw(y, (COLS - (int)strlen(t))/2, "%s", t); }

void draw_card_graphic(int y, int x, CardType t, int sel) {
    int c = get_card_color(t);
    if(sel) attron(A_REVERSE);
    attron(COLOR_PAIR(c));
    mvprintw(y, x, "+---+");
    mvprintw(y+1, x, "|%3d|", t); // Simplified graphic for space
    mvprintw(y+2, x, "+---+");
    attroff(COLOR_PAIR(c));
    if(sel) attroff(A_REVERSE);
}

// === NETWORKING & ACTIONS ===

int send_str(int sock, const char *str) {
    char buf[1024];
    int len = snprintf(buf, sizeof(buf), "%s\n", str);
    return send(sock, buf, len, 0) == len ? 0 : -1;
}

void play_card(GameState *state) {
    if(!state->is_turn) { add_log(state, "Not your turn!"); return; }
    if(state->hand_size == 0) return;
    
    Card played = state->hand[state->selected_card_idx];
    char buf[64];
    snprintf(buf, sizeof(buf), "PLAY %d", played.type);
    send_str(state->sock, buf);
    
    // Optimistic local update removed: wait for HAND/EVENT update
}

void draw_card_action(GameState *state) {
    if(!state->is_turn) { add_log(state, "Not your turn!"); return; }
    send_str(state->sock, "DRAW");
}

int client_recv_string(int sock, char *buffer, size_t max_len) {
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

void process_server_message(GameState *state, char *msg) {
    if(strncmp(msg, "LOBBY_UPDATE:", 13) == 0) {
        char temp_msg[1024];
        strncpy(temp_msg, msg + 13, 1024);
        state->player_count = 0;
        char *token = strtok(temp_msg, "|");
        while(token && state->player_count < MAX_PLAYERS) {
            char *name = token;
            char *ready_str = strchr(token, ':');
            if(ready_str) {
                *ready_str = '\0'; ready_str++;
                strncpy(state->players[state->player_count].name, name, 31);
                state->players[state->player_count].is_ready = (ready_str[0] == '1');
                if(strcmp(name, state->player_name) == 0) 
                    state->is_ready = state->players[state->player_count].is_ready;
                state->player_count++;
            }
            token = strtok(NULL, "|");
        }
    } 
    else if(strncmp(msg, "GAME_START", 10) == 0) {
        state->current_screen = SCREEN_GAME;
        state->is_turn = 0;
        add_log(state, "Game Started!");
    }
    else if(strncmp(msg, "HAND:", 5) == 0) {
        state->hand_size = 0;
        char *p = msg + 5;
        char *token = strtok(p, ",");
        while(token) {
            int cid = atoi(token);
            state->hand[state->hand_size].type = cid;
            state->hand_size++;
            token = strtok(NULL, ",");
        }
    }
    else if(strncmp(msg, "TURN:", 5) == 0) {
        char *name = msg + 5;
        char log[64];
        snprintf(log, 64, "Turn: %s", name);
        add_log(state, log);
        
        if(strcmp(name, state->player_name) == 0) {
             state->is_turn = 1; 
             add_log(state, "IT IS YOUR TURN! (D to Draw)");
        } else {
             state->is_turn = 0;
        }
    }
    else if(strncmp(msg, "EVENT:", 6) == 0) {
        add_log(state, msg + 6);
    }
    else if(strncmp(msg, "GAME_OVER:", 10) == 0) {
        add_log(state, "YOU DIED.");
        // Could switch to dead screen
    }
}

// === DRAWING SCREENS === (Simplified for brevity, same structure)

void draw_name_input(GameState *s) {
    clear();
    mvprintw(5, 5, "Enter Name: ");
    echo(); char buf[32]; getnstr(buf, 30); noecho();
    strcpy(s->player_name, buf);
    send(s->sock, buf, strlen(buf), 0);
    send(s->sock, "\n", 1, 0); 
}

void draw_lobby_select(GameState *s) {
    clear();
    mvprintw(3, 5, "SELECT A LOBBY (1-3):");
    char* n[] = {"Lobby 1", "Lobby 2", "Lobby 3"};
    for(int i=0;i<3;i++) {
        if(i==s->selected_lobby_index) attron(A_REVERSE);
        mvprintw(5+i, 5, "%s", n[i]);
        attroff(A_REVERSE);
    }
}

void draw_lobby_wait(GameState *s) {
    clear();
    mvprintw(2, 2, "LOBBY WAIT - %s", s->player_name);
    for(int i=0; i<s->player_count; i++) {
        mvprintw(4+i, 4, "%s [%s]", s->players[i].name, s->players[i].is_ready ? "RDY" : "...");
    }
    mvprintw(15, 2, "Press R to Ready/Unready");
}

void draw_game_screen(GameState *state) {
    clear();
    mvprintw(1, 1, "Turn: %s", state->is_turn ? "YOU" : "Opponent");
    
    // Draw Log
    for(int i=0; i<LOG_HEIGHT; i++) mvprintw(3+i, 40, "%s", state->game_log[i]);

    // Draw Hand
    mvprintw(15, 1, "Your Hand:");
    for(int i=0; i < state->hand_size; i++) {
        draw_card_graphic(16, 1 + (i*6), state->hand[i].type, (i == state->selected_card_idx));
    }
}

int main() {
    initscr(); cbreak(); noecho(); keypad(stdscr,TRUE); curs_set(0);
    start_color(); timeout(100);
    init_pair(1,COLOR_RED,COLOR_BLACK);
    init_pair(2,COLOR_GREEN,COLOR_BLACK);
    init_pair(3,COLOR_YELLOW,COLOR_BLACK);
    init_pair(5,COLOR_CYAN,COLOR_BLACK);

    GameState state = {0};
    state.current_screen = SCREEN_NAME_INPUT;

    state.sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a; a.sin_family=AF_INET; a.sin_port=htons(PORT);
    inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);
    
    if(connect(state.sock,(struct sockaddr*)&a,sizeof(a))<0) {
        endwin(); printf("Server offline!\n"); return 1;
    }

    int running = 1;
    while(running) {
        // Init Sequence (Blocking)
        if (state.current_screen == SCREEN_NAME_INPUT) {
            char msg[1024];
            client_recv_string(state.sock, msg, sizeof(msg)); // "Enter name"
            draw_name_input(&state);
            client_recv_string(state.sock, msg, sizeof(msg)); // "You Join"
            client_recv_string(state.sock, msg, sizeof(msg)); // Lobby List
            state.current_screen = SCREEN_LOBBY_SELECT;
        }

        // Async Updates
        fd_set fds; FD_ZERO(&fds); FD_SET(state.sock,&fds);
        struct timeval tv = {0,0};
        if(select(state.sock+1,&fds,NULL,NULL,&tv)>0) {
            char buf[1024] = {0};
            if(recv(state.sock, buf, sizeof(buf)-1, MSG_DONTWAIT) > 0) {
                char *token = strtok(buf, "\n");
                while(token) {
                    process_server_message(&state, token);
                    token = strtok(NULL, "\n");
                }
            }
        }

        int ch = ERR;
        switch(state.current_screen) {
            case SCREEN_LOBBY_SELECT: 
                draw_lobby_select(&state);
                ch = getch();
                if(ch==KEY_UP && state.selected_lobby_index>0) state.selected_lobby_index--;
                if(ch==KEY_DOWN && state.selected_lobby_index<2) state.selected_lobby_index++;
                if(ch=='\n') {
                    char msg[16]; snprintf(msg,sizeof(msg),"%d",state.selected_lobby_index+1);
                    send_str(state.sock, msg);
                    char temp[1024];
                    client_recv_string(state.sock, temp, sizeof(temp)); // You Join
                    client_recv_string(state.sock, temp, sizeof(temp)); // First Lobby Update
                    process_server_message(&state, temp);
                    state.current_screen = SCREEN_LOBBY_WAIT;
                }
                break;
            case SCREEN_LOBBY_WAIT: 
                draw_lobby_wait(&state); 
                ch = getch(); 
                if((ch=='r'||ch=='R')) {
                    state.is_ready = !state.is_ready;
                    send_str(state.sock, state.is_ready ? "READY" : "UNREADY");
                }
                break;
            case SCREEN_GAME: 
                draw_game_screen(&state); 
                ch = getch(); 
                if (ch == KEY_LEFT && state.selected_card_idx > 0) state.selected_card_idx--;
                if (ch == KEY_RIGHT && state.selected_card_idx < state.hand_size - 1) state.selected_card_idx++;
                if (ch == '\n') play_card(&state);
                if (ch == 'd') draw_card_action(&state);
                break;
        }
        if(ch=='q' || ch=='Q') running=0;
        refresh();
    }
    close(state.sock); endwin();
    return 0;
}
