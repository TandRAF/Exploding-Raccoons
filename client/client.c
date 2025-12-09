// client.c - Exploding Racoons cu interfață grafică + networking real
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
#include <time.h>

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
    CARD_EXPLODING_RACOON, CARD_DEFUSE, CARD_ATTACK, CARD_SKIP, CARD_FAVOR,
    CARD_SHUFFLE, CARD_SEE_THE_FUTURE, CARD_NOPE, CARD_RACOON_TACO,
    CARD_RACOON_MELON, CARD_RACOON_POTATO, CARD_RACOON_BEARD, CARD_RACOON_RAINBOW,
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

    int cards_in_deck;
    Card last_played_card;

    char game_log[LOG_HEIGHT][LOG_WIDTH];

    // network
    int sock;
    int connected;
} GameState;

// === UI IDENTIC CU gui.c ===
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
        case CARD_RACOON_TACO:      return "RACOON TACO";
        case CARD_RACOON_MELON:     return "RACOON MELON";
        case CARD_RACOON_POTATO:    return "POTATO RACOON";
        case CARD_RACOON_BEARD:     return "BEARD RACOON";
        case CARD_RACOON_RAINBOW:   return "RAINBOW RACOON";
        default: return "UNKNOWN";
    }
}

int get_card_color(CardType t) {
    switch(t) {
        case CARD_EXPLODING_RACOON: return 1;
        case CARD_DEFUSE:           return 2;
        case CARD_ATTACK: case CARD_SKIP: case CARD_FAVOR: case CARD_SHUFFLE: return 3;
        case CARD_SEE_THE_FUTURE:   return 4;
        case CARD_NOPE:             return 1;
        default:                    return 5;
    }
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
    mvprintw(y,   x, "+------------------+");
    mvprintw(y+1, x, "|                  |");
    const char* n = get_card_name(t);
    size_t len = strlen(n);
    int pad = (18 - (int)len) / 2;
    if(pad < 0) pad = 0;
    mvprintw(y+2, x, "|%*s%s%*s|", 
             pad, "", 
             n, 
             (int)(18 - len - pad), "");
    mvprintw(y+3, x, "|                  |");
    mvprintw(y+4, x, "+------------------+");
    attroff(COLOR_PAIR(c));
    if(sel) attroff(A_REVERSE);
}

// === LOGICĂ JOC SIMULAT (ADĂUGATĂ DIN gui.c) ===
void play_card(GameState *state) {
    if(state->hand_size == 0) return;
    Card played = state->hand[state->selected_card_idx];
    for(int i = state->selected_card_idx; i < state->hand_size - 1; i++) {
        state->hand[i] = state->hand[i+1];
    }
    state->hand_size--;
    if(state->selected_card_idx >= state->hand_size && state->selected_card_idx > 0) {
        state->selected_card_idx--;
    }
    state->last_played_card = played;
    char buf[50];
    sprintf(buf, "You played %s", get_card_name(played.type));
    add_log(state, buf);
    // TODO: AICI SE VA TRIMITE UN MESAJ LA SERVER: "PLAY_CARD:..."
}

void draw_card_action(GameState *state) {
    if(state->cards_in_deck > 0) {
        state->cards_in_deck--;
        add_log(state, "You drew a card.");
        if(state->hand_size < MAX_HAND_SIZE) {
            // Aici se va cere cardul de la server, nu se genereaza random!
            state->hand[state->hand_size].type = rand() % 12; 
            state->hand_size++;
        }
    }
    // TODO: AICI SE VA TRIMITE UN MESAJ LA SERVER: "DRAW_CARD"
}

// === DESENARE ECRANE (100% identic cu gui.c) ===
void draw_name_input(GameState *s) {
    clear();
    attron(A_BOLD | COLOR_PAIR(3));
    center_text(5, "  /\\___/\\  ");
    center_text(6, " (  @.@  ) ");
    center_text(7, "  \\  _  /  ");
    attroff(COLOR_PAIR(3));
    attron(A_BOLD | A_UNDERLINE);
    center_text(9, " EXPLODING RACOONS ");
    attroff(A_BOLD | A_UNDERLINE);

    mvprintw(12, (COLS-30)/2, "Enter Name: ");
    echo(); curs_set(1); timeout(-1);
    char buf[32]; mvgetnstr(12, (COLS-30)/2 + 12, buf, 30);
    strcpy(s->player_name, buf);
    noecho(); curs_set(0); timeout(100);

    // Trimite numele la server, dar nu schimba ecranul aici.
    char msg[64]; snprintf(msg, sizeof(msg), "%s", buf);
    send(s->sock, msg, strlen(msg), 0);
    send(s->sock, "\n", 1, 0); 
    
    // Serverul va trimite confirmarea și lista de lobby-uri, 
    // care vor fi citite sincronizat în main().
}

void draw_lobby_select(GameState *s) {
    clear();
    attron(A_BOLD); center_text(3, " SELECT A LOBBY "); attroff(A_BOLD);
    int y = 6, x = (COLS-30)/2;
    const char* n[] = {"Lobby 1 (Alpha)", "Lobby 2 (Beta)", "Lobby 3 (Gamma)"};
    for(int i=0;i<3;i++) {
        if(i==s->selected_lobby_index) { attron(A_REVERSE|COLOR_PAIR(3)); }
        mvprintw(y+i*2, x, i==s->selected_lobby_index ? " > %s < " : "   %s   ", n[i]);
        if(i==s->selected_lobby_index) attroff(A_REVERSE|COLOR_PAIR(3));
    }
    mvprintw(15, (COLS-50)/2, "UP/DOWN + ENTER");
}

 void draw_lobby_wait(GameState *state) {
    static int waiting_start = 0;
    static int waiting_active = 0;

    clear();

    attron(A_BOLD | COLOR_PAIR(5));
    mvprintw(2, (COLS - 30)/2, " LOBBY %d - WAITING ROOM ", state->selected_lobby_index + 1);
    attroff(A_BOLD | COLOR_PAIR(5));

    mvprintw(4, 5, "My Name: %s", state->player_name);
    mvprintw(6, 5, "Connected Players (%d/%d):", state->player_count, MAX_PLAYERS);

    attron(A_UNDERLINE);
    mvprintw(8, 8, "%-20s %-15s", "PLAYER NAME", "STATUS");
    attroff(A_UNDERLINE);

    // Găsim starea mea actuală de ready din lista primită de la server
    int my_ready_state = 0;
    for (int i = 0; i < state->player_count; i++) {
        if (strcmp(state->players[i].name, state->player_name) == 0) {
            my_ready_state = state->players[i].is_ready;
            break;
        }
    }
    
    for(int i = 0; i < state->player_count; i++) {
        char name_display[40];
        // Folosim numele stocat în PlayerInfo
        if(strcmp(state->players[i].name, state->player_name) == 0)
            sprintf(name_display, "%s (You)", state->players[i].name);
        else
            strcpy(name_display, state->players[i].name);

        mvprintw(10 + i, 8, "%-20s", name_display);

        if(state->players[i].is_ready) {
            attron(COLOR_PAIR(2) | A_BOLD);
            printw(" [ READY ] ");
            attroff(COLOR_PAIR(2) | A_BOLD);
        } else {
            attron(COLOR_PAIR(1));
            printw(" [NOT READY]");
            attroff(COLOR_PAIR(1));
        }
    }

    // === LOGICA DE START ===
    int all_ready = 1;
    for(int i = 0; i < state->player_count; i++) {
        if(!state->players[i].is_ready) all_ready = 0;
    }

    if(my_ready_state && all_ready && state->player_count >= 2) {

        if(!waiting_active) {
            waiting_active = 1;
            waiting_start = (int)time(NULL);
            clear();
            attron(A_BOLD | COLOR_PAIR(3));
            center_text(10, "TOȚI JUCĂTORII SUNT READY!");
            center_text(12, "Jocul începe în 2 secunde...");
            center_text(14, "Așteptăm confirmarea de start de la server...");
            attroff(A_BOLD | COLOR_PAIR(3));
            refresh();
        }

        int elapsed = (int)time(NULL) - waiting_start;
        int remaining = 2 - elapsed;

        if(remaining > 0) {
            char msg[50];
            sprintf(msg, "Începe în %d...", remaining);
            attron(A_BLINK | A_BOLD | COLOR_PAIR(2));
            center_text(16, msg);
            attroff(A_BLINK | A_BOLD | COLOR_PAIR(2));
        } else {
            // === JOCUL ÎNCEPE ACUM! (fără mock) ===
            state->current_screen = SCREEN_GAME;

            // Inițializăm UI-ul de joc
            state->hand_size = 5;
            state->cards_in_deck = 45;
            state->selected_card_idx = 0;
            state->last_played_card.type = CARD_NONE;

            state->hand[0].type = CARD_DEFUSE;
            state->hand[1].type = CARD_ATTACK;
            state->hand[2].type = CARD_RACOON_TACO;
            state->hand[3].type = CARD_SEE_THE_FUTURE;
            state->hand[4].type = CARD_NOPE;

            for(int i = 0; i < LOG_HEIGHT; i++) strcpy(state->game_log[i], "");
            add_log(state, "Jocul a început!");
            add_log(state, "Este rândul tău.");

            // Resetăm starea de așteptare
            waiting_active = 0;
        }
    } else {
        waiting_active = 0;
    }

    // Mesaj ready/unready (doar dacă nu suntem în countdown)
    if(!waiting_active) {
        attron(A_REVERSE);
        center_text(18, my_ready_state ? " WAITING... (R = unready) " : " PRESS 'R' TO READY UP ");
        attroff(A_REVERSE);
    }
    // Sincronizăm starea locală de 'is_ready' pentru a ști ce mesaj să trimitem
    state->is_ready = my_ready_state; 

    mvprintw(20, (COLS-30)/2, "Press 'Q' to leave lobby");
    refresh();
}

void draw_game_screen(GameState *state) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    // opponents
    mvprintw(1, 2, "OPPONENTS:");
    int opp_spacing = width / MAX_PLAYERS;
    int opp_index = 0; // Contor pentru poziția pe ecran, ignoră jucătorul local
    for(int i = 0; i < state->player_count; i++) {
        if(strcmp(state->players[i].name, state->player_name) == 0) continue; 
        
        int x_pos = 2 + (opp_index * opp_spacing);
        opp_index++;
        
        // TODO: is_opp_turn trebuie să vină de la server
        int is_opp_turn = 0; 
        
        if (is_opp_turn) attron(A_BLINK | A_BOLD);
        mvprintw(2, x_pos, "%s", state->players[i].name);
        attroff(A_BLINK | A_BOLD);
        
        state->players[i].card_count = 4; // Mock value
        
        mvprintw(3, x_pos, "Cards: %d", state->players[i].card_count);
        if(state->players[i].is_exploded) {
             attron(COLOR_PAIR(1));
             mvprintw(4, x_pos, "[DEAD]");
             attroff(COLOR_PAIR(1));
        } else {
             mvprintw(4, x_pos, "[ALIVE]");
        }
    }

    // table stuff
    mvhline(6, 0, ACS_HLINE, width);
    mvprintw(8, width/2 - 20, "DRAW PILE");
    mvprintw(9, width/2 - 20, "[ %d Cards ]", state->cards_in_deck);
    
    mvprintw(8, width/2 + 5, "DISCARD PILE (Last Played)");
    if(state->last_played_card.type != CARD_NONE) {
        draw_card_graphic(9, width/2 + 5, state->last_played_card.type, 0);
    } else {
        mvprintw(10, width/2 + 5, "[ Empty ]");
    }

    // game log
    int log_x = width - LOG_WIDTH - 2;
    mvprintw(7, log_x, "GAME LOG:");
    for(int i=0; i<LOG_HEIGHT; i++) {
        mvprintw(8+i, log_x, "> %s", state->game_log[i]);
    }

    // my hand
    mvhline(height - 10, 0, ACS_HLINE, width);
    mvprintw(height - 9, 2, "YOUR HAND (%s) - Use Arrow Keys to Select, ENTER to Play (D=Draw)", state->player_name);

    if (state->hand_size > 0) {
        int start_x = 2;
        int gap = 22; 
        for(int i=0; i < state->hand_size; i++) {
            if(start_x + gap > width) break; 
            draw_card_graphic(height - 7, start_x, state->hand[i].type, (i == state->selected_card_idx));
            start_x += gap;
        }
    } else {
        mvprintw(height - 6, 5, "You have no cards! (Pray you don't explode)");
    }
}

// === NETWORKING ===
int send_str(int sock, const char *str) {
    char buf[1024];
    int len = snprintf(buf, sizeof(buf), "%s\n", str);
    return send(sock, buf, len, 0) == len ? 0 : -1;
}

// Funcție de citire blocantă, similară cu recv_string din server.c
int client_recv_string(int sock, char *buffer, size_t max_len) {
    size_t total = 0;
    while (total < max_len - 1) {
        char c;
        // Citire blocantă (fără MSG_DONTWAIT)
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
        // Trebuie să facem o copie deoarece strtok modifică șirul original.
        char temp_msg[1024];
        strncpy(temp_msg, msg + 13, 1024);
        temp_msg[1023] = '\0';
        
        char *token = strtok(temp_msg, "|");
        state->player_count = 0;
        
        while(token && state->player_count < MAX_PLAYERS) {
            char *name = token;
            char *ready_str = strchr(token, ':');
            if(ready_str) {
                *ready_str = '\0'; // Terminăm numele
                ready_str++;
                
                // Atenție la copierea numelui (max 32)
                strncpy(state->players[state->player_count].name, name, 31);
                state->players[state->player_count].name[31] = '\0';
                
                state->players[state->player_count].is_ready = (ready_str[0] == '1');
                
                // Actualizăm starea locală 'is_ready' pe baza serverului
                if(strcmp(name, state->player_name) == 0) {
                    state->is_ready = state->players[state->player_count].is_ready;
                }
                state->player_count++;
            }
            token = strtok(NULL, "|");
        }
        return;
    }

    // Alte mesaje (opțional)
    char logmsg[LOG_WIDTH];
    snprintf(logmsg, sizeof(logmsg), "SERVER: %.30s", msg);
    add_log(state, logmsg);
}

int main() {
    // ncurses init
    initscr(); cbreak(); noecho(); keypad(stdscr,TRUE); curs_set(0);
    start_color(); timeout(100);
    init_pair(1,COLOR_RED,COLOR_BLACK);
    init_pair(2,COLOR_GREEN,COLOR_BLACK);
    init_pair(3,COLOR_YELLOW,COLOR_BLACK);
    init_pair(4,COLOR_MAGENTA,COLOR_BLACK);
    init_pair(5,COLOR_CYAN,COLOR_BLACK);

    GameState state = {0};
    state.current_screen = SCREEN_NAME_INPUT;

    // conectare
    state.sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a; a.sin_family=AF_INET; a.sin_port=htons(PORT);
    inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);
    if(connect(state.sock,(struct sockaddr*)&a,sizeof(a))<0) {
        endwin(); printf("Server offline!\n"); return 1;
    }
    state.connected = 1;

    int running = 1;
    while(running) {
        
        // Secțiune de sincronizare blocantă (înlocuiește logica veche din switch-case)
        if (state.current_screen == SCREEN_NAME_INPUT) {
            char welcome_msg[1024];
            client_recv_string(state.sock, welcome_msg, sizeof(welcome_msg)); // Preia "Enter your name:"
            
            draw_name_input(&state); // Afișează prompt-ul, citeste numele și-l trimite
            
            char ack_msg[1024];
            client_recv_string(state.sock, ack_msg, sizeof(ack_msg)); // Preia "You Join!" sau eroare

            // Preia lista de lobby-uri
            char lobby_list[1024];
            client_recv_string(state.sock, lobby_list, sizeof(lobby_list)); 
            
            state.current_screen = SCREEN_LOBBY_SELECT;
        }

        // primesc mesaje server în fundal (folosim MSG_DONTWAIT)
        fd_set fds; FD_ZERO(&fds); FD_SET(state.sock,&fds);
        struct timeval tv = {0,0};
        if(select(state.sock+1,&fds,NULL,NULL,&tv)>0) {
            char buf[1024] = {0};
            int n = recv(state.sock, buf, sizeof(buf)-1, MSG_DONTWAIT);
            if(n<=0) { state.connected=0; add_log(&state,"Server disconnected"); }
            else {
                // Previne suprapunerea de pachete prin citirea by-char (ca și cum ar fi o singură linie)
                char *token = strtok(buf, "\n");
                while(token) {
                    process_server_message(&state, token);
                    token = strtok(NULL, "\n");
                }
            }
        }

        // desenare + input
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
                    
                    // Sincronizare: Așteaptă confirmarea (You Join!) și LOBBY_UPDATE
                    char ack_msg[1024];
                    client_recv_string(state.sock, ack_msg, sizeof(ack_msg)); // You Join!
                    
                    // Serverul trimite imediat LOBBY_UPDATE către toți (inclusiv mine)
                    // Citim primul pachet de update trimis de server (blocant)
                    char lobby_update[1024];
                    client_recv_string(state.sock, lobby_update, sizeof(lobby_update));
                    process_server_message(&state, lobby_update); // Procesăm imediat
                    
                    state.current_screen = SCREEN_LOBBY_WAIT;
                }
                break;
                
            case SCREEN_LOBBY_WAIT: 
                draw_lobby_wait(&state); 
                ch = getch(); 
                if((ch=='r'||ch=='R')) {
                    // Trimit starea opusă celei actuale (state.is_ready este cea primită de la server)
                    state.is_ready = !state.is_ready;
                    send_str(state.sock, state.is_ready ? "READY" : "UNREADY");
                    // draw_lobby_wait va desena starea nouă când vine LOBBY_UPDATE de la server.
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

    close(state.sock);
    endwin();
    return 0;
}