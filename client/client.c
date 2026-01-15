#define _DEFAULT_SOURCE
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "expogame.h"

#define PORT 8080
#define BUFFER_SIZE 8192 

#define SCREEN_NAME_INPUT 0
#define SCREEN_LOBBY_SELECT 1
#define SCREEN_LOBBY_WAIT 2
#define SCREEN_GAME 3
#define SCREEN_GAME_OVER 4

typedef struct {
    int current_screen;
    char my_name[32];
    int is_my_turn;
    char game_logs[12][80];
    int log_count;
    Card_t hand[20];
    int hand_count;
    int selected_card_idx;
    int selected_lobby_idx; 
    int sock;
} ClientState;

ClientState state;

// --- SINCRONIZARE MANA (Cartile apar/dispar la refresh) ---
void update_hand_from_message(char* buffer) {
    if (strstr(buffer, "--- YOUR HAND ---")) {
        state.hand_count = 0; 
        for (int i = 0; i <= CARD_GENERIC; i++) {
            const char* card_name = get_card_name((CardType)i);
            char* ptr = buffer;
            while ((ptr = strstr(ptr, card_name)) != NULL) {
                if (state.hand_count < 20) {
                    state.hand[state.hand_count].type = (CardType)i;
                    state.hand_count++;
                }
                ptr += strlen(card_name);
            }
        }
    }
}

void add_log(const char* msg) {
    if (state.log_count < 12) {
        strncpy(state.game_logs[state.log_count++], msg, 79);
    } else {
        for (int i = 0; i < 11; i++) strcpy(state.game_logs[i], state.game_logs[i+1]);
        strncpy(state.game_logs[11], msg, 79);
    }
}

void center_text(int y, const char* text) {
    mvprintw(y, (COLS - (int)strlen(text)) / 2, "%s", text);
}

// --- RENDERERE ECRANE (Design gui.c) ---

void draw_name_input() {
    clear(); box(stdscr, 0, 0);
    attron(A_BOLD | COLOR_PAIR(3));
    center_text(5, "  /\\___/\\  "); 
    center_text(6, " (  @.@  ) "); 
    center_text(7, "  \\  _  /  ");
    attroff(COLOR_PAIR(3));
    center_text(9, " EXPLODING RACOONS ");
    mvprintw(12, (COLS-30)/2, "Enter Name: %s_", state.my_name);
    refresh();
}

void draw_lobby_select() {
    clear(); box(stdscr, 0, 0);
    attron(A_BOLD | COLOR_PAIR(5));
    center_text(3, " SELECTEAZA UN MECI ");
    attroff(A_BOLD | COLOR_PAIR(5));
    const char* lobbies[] = {"Meci 1 (Alpha)", "Meci 2 (Beta)", "Meci 3 (Gamma)"};
    for(int i=0; i<3; i++) {
        if(i == state.selected_lobby_idx) attron(A_REVERSE | COLOR_PAIR(3));
        mvprintw(7 + (i*2), (COLS-30)/2, " > %s < ", lobbies[i]);
        attroff(A_REVERSE | COLOR_PAIR(3));
    }
    center_text(15, "Foloseste sagetile si apasa ENTER");
    refresh();
}

void draw_lobby_wait() {
    clear(); box(stdscr, 0, 0);
    attron(COLOR_PAIR(5));
    center_text(LINES/2 - 2, "TE-AI ALATURAT MECIULUI!");
    attroff(COLOR_PAIR(5));
    center_text(LINES/2, "Asteptam ceilalti jucatori...");
    attron(A_BLINK | A_REVERSE | COLOR_PAIR(2));
    center_text(LINES/2 + 3, " APASA 'R' PENTRU READY ");
    attroff(A_BLINK | A_REVERSE | COLOR_PAIR(2));
    refresh();
}

void draw_game_screen() {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    // 1. Jurnal de Lupta - Aici vor aparea mesajele "Jucatorul X a jucat Y"
    attron(COLOR_PAIR(5));
    mvprintw(0, 2, "--- JURNAL DE LUPTA ---");
    for(int i=0; i<state.log_count; i++) {
        // Evidențiem mesajele despre rândul curent sau acțiuni importante
        if (strstr(state.game_logs[i], "randul") || strstr(state.game_logs[i], "jucat")) {
            attron(A_BOLD | COLOR_PAIR(3)); 
            mvprintw(1+i, 4, "> %s", state.game_logs[i]);
            attroff(A_BOLD | COLOR_PAIR(3));
        } else {
            mvprintw(1+i, 4, "> %s", state.game_logs[i]);
        }
    }
    attroff(COLOR_PAIR(5));

    mvhline(height / 2, 0, ACS_HLINE, width);

    // 2. Afisare carti proprii
    int start_y = height / 2 + 2;
    mvprintw(start_y - 1, 2, "MANA TA (%s):", state.my_name);

    for(int i=0; i < state.hand_count; i++) {
        int x_pos = 2 + (i * 20);
        if(x_pos + 18 > width) break;
        
        if(i == state.selected_card_idx) attron(A_REVERSE | COLOR_PAIR(2));
        mvprintw(start_y,     x_pos, "+----------------+");
        mvprintw(start_y + 1, x_pos, "| %-14s |", get_card_name(state.hand[i].type));
        mvprintw(start_y + 2, x_pos, "+----------------+");
        attroff(A_REVERSE | COLOR_PAIR(2));
    }

    // 3. Mesaj pentru randul curent (Status Bar)
    if (state.is_my_turn) {
        attron(A_BOLD | COLOR_PAIR(2) | A_BLINK);
        center_text(height - 1, "!!! ESTE RANDUL TAU !!!");
        attroff(A_BOLD | COLOR_PAIR(2) | A_BLINK);
    } else {
        attron(A_DIM);
        center_text(height - 1, "Asteapta mutarea adversarului...");
        attroff(A_DIM);
    }
    refresh();
}

int main() {
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);    
    init_pair(2, COLOR_GREEN, COLOR_BLACK);  
    init_pair(3, COLOR_YELLOW, COLOR_BLACK); 
    init_pair(5, COLOR_CYAN, COLOR_BLACK);
    timeout(50);

    struct sockaddr_in serv_addr;
    state.sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    if (connect(state.sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        endwin(); printf("Server offline!\n"); return 1;
    }

    state.current_screen = SCREEN_NAME_INPUT;
    char net_buf[BUFFER_SIZE];
    char death_reason[128] = "";

    while(1) {
        if (state.current_screen == SCREEN_NAME_INPUT) draw_name_input();
        else if (state.current_screen == SCREEN_LOBBY_SELECT) draw_lobby_select();
        else if (state.current_screen == SCREEN_LOBBY_WAIT) draw_lobby_wait();
        else if (state.current_screen == SCREEN_GAME) draw_game_screen();
        else if (state.current_screen == SCREEN_GAME_OVER) {
            clear(); box(stdscr, 0, 0);
            attron(COLOR_PAIR(1) | A_BOLD);
            center_text(LINES/2, death_reason);
            attroff(COLOR_PAIR(1) | A_BOLD);
            refresh();
        }

        int n = recv(state.sock, net_buf, BUFFER_SIZE - 1, MSG_DONTWAIT);
        if (n > 0) {
            net_buf[n] = '\0';
            update_hand_from_message(net_buf); 
            
            if (strstr(net_buf, "BOOM!") || strstr(net_buf, "GAME OVER")) {
                state.current_screen = SCREEN_GAME_OVER;
                strncpy(death_reason, net_buf, 127);
            }
            if (strstr(net_buf, "You Join!")) {
                if (state.current_screen == SCREEN_NAME_INPUT) state.current_screen = SCREEN_LOBBY_SELECT;
                else if (state.current_screen == SCREEN_LOBBY_SELECT) state.current_screen = SCREEN_LOBBY_WAIT;
            }
            if (strstr(net_buf, "Game Started")) {
                state.current_screen = SCREEN_GAME;
                send(state.sock, "2\n", 2, 0); 
            }
            if (strstr(net_buf, "YOUR TURN")) {
                state.is_my_turn = 1;
                send(state.sock, "2\n", 2, 0); 
            }
            add_log(net_buf);
        }

        int ch = getch();
        if (ch == 'q') break;

        if (state.current_screen == SCREEN_NAME_INPUT) {
            if (ch == '\n' && strlen(state.my_name) > 0) {
                char msg[64]; sprintf(msg, "%s\n", state.my_name);
                send(state.sock, msg, strlen(msg), 0);
            } else if ((ch == KEY_BACKSPACE || ch == 127) && strlen(state.my_name) > 0) {
                state.my_name[strlen(state.my_name)-1] = '\0';
            } else if (strlen(state.my_name) < 30 && ch >= 32 && ch <= 126) {
                int l = strlen(state.my_name); state.my_name[l] = ch; state.my_name[l+1] = '\0';
            }
        }
        else if (state.current_screen == SCREEN_LOBBY_SELECT) {
            if (ch == KEY_UP && state.selected_lobby_idx > 0) state.selected_lobby_idx--;
            if (ch == KEY_DOWN && state.selected_lobby_idx < 2) state.selected_lobby_idx++;
            if (ch == '\n') { 
                char msg[32]; sprintf(msg, "%d\n", state.selected_lobby_idx + 1); 
                send(state.sock, msg, strlen(msg), 0); 
            }
        }
        else if (state.current_screen == SCREEN_LOBBY_WAIT) {
            if (ch == 'r' || ch == 'R') send(state.sock, "y\n", 2, 0);
        }
        else if (state.current_screen == SCREEN_GAME) {
            // Navigare stanga-dreapta
            if (ch == KEY_LEFT && state.selected_card_idx > 0) state.selected_card_idx--;
            if (ch == KEY_RIGHT && state.selected_card_idx < state.hand_count - 1) state.selected_card_idx++;
            
            // --- AICI ESTE MODIFICAREA PENTRU TASTA ENTER ---
            if (ch == '\n' && state.is_my_turn && state.hand_count > 0) {
                // 1. Trimitem comanda de a juca o carte (Actiunea 1)
                send(state.sock, "1\n", 2, 0); 
                usleep(50000); // Pauza mica pentru ca serverul sa proceseze comanda
                
                // 2. Trimitem indexul cartii selectate
                char idx_msg[16]; 
                sprintf(idx_msg, "%d\n", state.selected_card_idx);
                send(state.sock, idx_msg, strlen(idx_msg), 0);
                
                // 3. LOGICA NOUA: Verificam daca cartea selectata cere o tinta (ex: FAVOR)
                if (state.hand[state.selected_card_idx].type == CARD_FAVOR) {
                    // Afisam un mic input box pentru Target ID
                    echo();         // Activam afisarea tastelor
                    curs_set(1);    // Activam cursorul
                    
                    // Desenam o cutie mica peste joc pentru a cere ID-ul
                    attron(COLOR_PAIR(1) | A_BOLD);
                    mvprintw(LINES/2, (COLS-40)/2, " INTRODU ID JUCATOR TINTA: ");
                    attroff(COLOR_PAIR(1) | A_BOLD);
                    
                    char target_str[10];
                    getnstr(target_str, 5); // Asteapta input de la tastatura
                    
                    noecho();       // Dezactivam afisarea tastelor
                    curs_set(0);    // Ascundem cursorul
                    
                    // Trimitem Target ID la server
                    char target_msg[16]; 
                    sprintf(target_msg, "%s\n", target_str);
                    send(state.sock, target_msg, strlen(target_msg), 0);
                    
                    // Curatam mesajul de pe ecran (redesenam la urmatorul loop)
                } else {
                    // Trimitem -1 daca nu e nevoie de tinta (pentru orice alta carte)
                    send(state.sock, "-1\n", 3, 0);
                }

                state.is_my_turn = 0; // Blocam inputul pana vine randul iar
                
                // 4. Cerem refresh la mana (pentru ca a disparut cartea jucata)
                usleep(100000);
                send(state.sock, "2\n", 2, 0); 
            }
            // ------------------------------------------------

            if ((ch == 'd' || ch == 'D') && state.is_my_turn) { 
                send(state.sock, "0\n", 2, 0); 
                state.is_my_turn = 0; 
                usleep(200000); 
                send(state.sock, "2\n", 2, 0); 
            }
        }
    }
    endwin(); close(state.sock); 
    return 0;
}
