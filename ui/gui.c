#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 600

// compile with: gcc gui.c -o gui -lncurses -ltinfo

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define MAX_PLAYERS 5
#define MAX_HAND_SIZE 20
#define LOG_HEIGHT 10
#define LOG_WIDTH 40

// screens
#define SCREEN_NAME_INPUT 0
#define SCREEN_LOBBY_SELECT 1
#define SCREEN_LOBBY_WAIT 2
#define SCREEN_GAME 3

typedef enum {
    CARD_EXPLODING_RACOON,
    CARD_DEFUSE,
    CARD_ATTACK,
    CARD_SKIP,
    CARD_FAVOR,
    CARD_SHUFFLE,
    CARD_SEE_THE_FUTURE,
    CARD_NOPE,
    CARD_RACOON_TACO,
    CARD_RACOON_MELON,
    CARD_RACOON_POTATO,
    CARD_RACOON_BEARD,
    CARD_RACOON_RAINBOW,
    CARD_NONE 
} CardType;

typedef struct {
    CardType type;
    int id;
} Card;

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

    // lobby info
    int player_count;
    PlayerInfo players[MAX_PLAYERS];
    int is_ready;
    
    // game info
    Card hand[MAX_HAND_SIZE];
    int hand_size;
    int selected_card_idx;
    
    int cards_in_deck;
    Card last_played_card;
    
    char game_log[LOG_HEIGHT][LOG_WIDTH];
} GameState;

// helper stuff
const char* get_card_name(CardType type) {
    switch(type) {
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

int get_card_color(CardType type) {
    switch(type) {
        case CARD_EXPLODING_RACOON: return 1; // red
        case CARD_DEFUSE:           return 2; // green
        case CARD_ATTACK:           
        case CARD_SKIP:             
        case CARD_FAVOR:            
        case CARD_SHUFFLE:          return 3; // yellow
        case CARD_SEE_THE_FUTURE:   return 4; // magenta
        case CARD_NOPE:             return 1; 
        default:                    return 5; // cyan
    }
}

void add_log(GameState *state, const char *msg) {
    for(int i = 0; i < LOG_HEIGHT - 1; i++) {
        strncpy(state->game_log[i], state->game_log[i+1], LOG_WIDTH);
    }
    strncpy(state->game_log[LOG_HEIGHT-1], msg, LOG_WIDTH-1);
    state->game_log[LOG_HEIGHT-1][LOG_WIDTH-1] = '\0';
}

void center_text(int y, const char *text) {
    mvprintw(y, (COLS - (int)strlen(text)) / 2, "%s", text);
}

// TODO: replace this with actual networking code later
void mock_connect_lobby(GameState *state, int lobby_id) {
    state->player_count = 0;
    state->is_ready = 0;

    // add myself
    strcpy(state->players[0].name, state->player_name);
    state->players[0].id = 0;
    state->players[0].is_ready = 0;
    state->player_count++;

    // fake players
    if (lobby_id == 0) {
        strcpy(state->players[1].name, "Raccoon_King");
        state->players[1].id = 1;
        state->players[1].is_ready = 1; 
        
        strcpy(state->players[2].name, "Trash_Panda");
        state->players[2].id = 2;
        state->players[2].is_ready = 0; 

        state->player_count = 3;
    } 
    else if (lobby_id == 1) {
        strcpy(state->players[1].name, "Sneaky_Bandit");
        state->players[1].id = 1;
        state->players[1].is_ready = 1;
        state->player_count = 2;
    }
}

void mock_init_game(GameState *state) {
    for(int i=0; i<state->player_count; i++) state->players[i].is_ready = 1;

    state->players[0].card_count = 5;
    state->players[0].is_turn = 1; 
    
    for(int i=1; i<state->player_count; i++) {
        state->players[i].card_count = 4;
    }

    state->cards_in_deck = 45;
    state->last_played_card.type = CARD_NONE;
    
    state->hand[0].type = CARD_DEFUSE;
    state->hand[1].type = CARD_ATTACK;
    state->hand[2].type = CARD_RACOON_TACO;
    state->hand[3].type = CARD_SEE_THE_FUTURE;
    state->hand[4].type = CARD_NOPE;
    state->hand_size = 5;
    state->selected_card_idx = 0;
    
    for(int i=0; i<LOG_HEIGHT; i++) memset(state->game_log[i], 0, LOG_WIDTH);
    add_log(state, "Game Started!");
    add_log(state, "It is your turn.");
}

void draw_name_input(GameState *state) {
    clear();
    attron(A_BOLD | COLOR_PAIR(3));
    center_text(5, "  /\\___/\\  ");
    center_text(6, " (  @.@  ) ");
    center_text(7, "  \\  _  /  ");
    attroff(COLOR_PAIR(3));
    
    attron(A_BOLD | A_UNDERLINE);
    center_text(9, " EXPLODING RACOONS - CLIENT ");
    attroff(A_BOLD | A_UNDERLINE);

    mvprintw(12, (COLS-30)/2, "Enter Name: ");
    
    timeout(-1); 
    echo();
    curs_set(1);
    refresh(); 
    
    char buf[32];
    mvgetnstr(12, (COLS-30)/2 + 12, buf, 30);
    strcpy(state->player_name, buf);
    
    noecho();
    curs_set(0);
    timeout(100); 
    
    state->current_screen = SCREEN_LOBBY_SELECT;
}

void draw_lobby_select(GameState *state) {
    clear();
    attron(A_BOLD);
    center_text(3, " SELECT A LOBBY ");
    attroff(A_BOLD);

    int start_y = 6;
    int center_x = (COLS - 30) / 2;

    const char* lobby_names[] = {"Lobby 1 (Alpha)", "Lobby 2 (Beta)", "Lobby 3 (Gamma)"};

    for(int i=0; i<3; i++) {
        if(i == state->selected_lobby_index) {
            attron(A_REVERSE | COLOR_PAIR(3));
            mvprintw(start_y + (i*2), center_x, " > %s < ", lobby_names[i]);
            attroff(A_REVERSE | COLOR_PAIR(3));
        } else {
            mvprintw(start_y + (i*2), center_x, "   %s   ", lobby_names[i]);
        }
    }

    mvprintw(15, (COLS-50)/2, "Use UP/DOWN to select, ENTER to join.");
}

void draw_lobby_wait(GameState *state) {
    clear();
    attron(A_BOLD | COLOR_PAIR(5));
    mvprintw(2, (COLS-30)/2, " LOBBY %d - WAITING ROOM ", state->selected_lobby_index + 1);
    attroff(A_BOLD | COLOR_PAIR(5));
    
    mvprintw(4, 5, "My Name: %s", state->player_name);
    mvprintw(6, 5, "Connected Players (%d/%d):", state->player_count, MAX_PLAYERS);
    
    attron(A_UNDERLINE);
    mvprintw(8, 8, "%-20s %-15s", "PLAYER NAME", "STATUS");
    attroff(A_UNDERLINE);

    for(int i=0; i<state->player_count; i++) {
        int y_pos = 10 + i;
        char name_display[40];
        
        if(strcmp(state->players[i].name, state->player_name) == 0) {
            sprintf(name_display, "%s (You)", state->players[i].name);
            state->players[i].is_ready = state->is_ready;
        } else {
            strcpy(name_display, state->players[i].name);
        }

        mvprintw(y_pos, 8, "%-20s", name_display);

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
    
    attron(A_REVERSE);
    center_text(18, state->is_ready ? " WAITING FOR OTHERS (Press 'R' to Unready) " : " PRESS 'R' TO READY UP ");
    attroff(A_REVERSE);
    
    mvprintw(20, (COLS-30)/2, "Press 'Q' to Leave Lobby");
}

void draw_card_graphic(int y, int x, CardType type, int selected) {
    int color = get_card_color(type);
    if(selected) attron(A_REVERSE);
    attron(COLOR_PAIR(color));
    
    mvprintw(y, x,   "+------------------+");
    mvprintw(y+1, x, "|                  |");
    const char* name = get_card_name(type);
    int pad = (int)(18 - strlen(name)) / 2;
    if (pad < 0) pad = 0;
    mvprintw(y+2, x, "|%*s%s%*s|", pad, "", name, (int)(18 - strlen(name) - pad), "");
    mvprintw(y+3, x, "|                  |");
    mvprintw(y+4, x, "+------------------+");
    
    attroff(COLOR_PAIR(color));
    if(selected) attroff(A_REVERSE);
}

void draw_game_screen(GameState *state) {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);

    // opponents
    mvprintw(1, 2, "OPPONENTS:");
    int opp_spacing = width / MAX_PLAYERS;
    for(int i = 0; i < state->player_count; i++) {
        if(strcmp(state->players[i].name, state->player_name) == 0) continue; 
        int x_pos = 2 + (i * opp_spacing);
        
        if (state->players[i].is_turn) attron(A_BLINK | A_BOLD);
        mvprintw(2, x_pos, "%s", state->players[i].name);
        attroff(A_BLINK | A_BOLD);
        
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
    mvprintw(height - 9, 2, "YOUR HAND (%s) - Use Arrow Keys to Select, ENTER to Play", state->player_name);

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
}

void draw_card_action(GameState *state) {
    if(state->cards_in_deck > 0) {
        state->cards_in_deck--;
        add_log(state, "You drew a card.");
        if(state->hand_size < MAX_HAND_SIZE) {
            state->hand[state->hand_size].type = rand() % 12; 
            state->hand_size++;
        }
    }
}

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    srand(time(NULL));

    timeout(100); 

    init_pair(1, COLOR_RED, COLOR_BLACK);    
    init_pair(2, COLOR_GREEN, COLOR_BLACK);  
    init_pair(3, COLOR_YELLOW, COLOR_BLACK); 
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(5, COLOR_CYAN, COLOR_BLACK);   

    GameState state;
    memset(&state, 0, sizeof(state));
    state.current_screen = SCREEN_NAME_INPUT;
    state.selected_lobby_index = 0;

    int running = 1;
    int ch;

    while(running) {
        
        if (state.current_screen == SCREEN_NAME_INPUT) {
            draw_name_input(&state);
        }

        else if (state.current_screen == SCREEN_LOBBY_SELECT) {
            draw_lobby_select(&state);
            ch = getch();
            
            if (ch != ERR) {
                switch(ch) {
                    case KEY_UP:
                        if(state.selected_lobby_index > 0) state.selected_lobby_index--;
                        break;
                    case KEY_DOWN:
                        if(state.selected_lobby_index < 2) state.selected_lobby_index++;
                        break;
                    case '\n': 
                        mock_connect_lobby(&state, state.selected_lobby_index);
                        state.current_screen = SCREEN_LOBBY_WAIT;
                        break;
                    case 'q':
                        running = 0;
                        break;
                }
            }
        }

        else if (state.current_screen == SCREEN_LOBBY_WAIT) {
            draw_lobby_wait(&state);
            ch = getch(); 
            
            if (ch != ERR) {
                if (ch == 'q') {
                    state.current_screen = SCREEN_LOBBY_SELECT; 
                }
                if (ch == 'r') {
                    state.is_ready = !state.is_ready;
                }
            }
            
            if (state.is_ready) {
                int all_ready = 1;
                for(int i=1; i<state.player_count; i++) {
                    if(!state.players[i].is_ready) all_ready = 0;
                }
                
                if(all_ready && state.player_count > 0) {
                    clear();
                    center_text(10, "EVERYONE IS READY! STARTING GAME...");
                    refresh();
                    sleep(1);
                    mock_init_game(&state);
                    state.current_screen = SCREEN_GAME;
                }
            }
        }

        else if (state.current_screen == SCREEN_GAME) {
            draw_game_screen(&state);
            ch = getch(); 

            if (ch != ERR) {
                switch(ch) {
                    case KEY_LEFT:
                        if (state.selected_card_idx > 0) 
                            state.selected_card_idx--;
                        break;
                    case KEY_RIGHT:
                        if (state.selected_card_idx < state.hand_size - 1) 
                            state.selected_card_idx++;
                        break;
                    case '\n': 
                        play_card(&state);
                        break;
                    case 'd': 
                        draw_card_action(&state);
                        break;
                    case 'q':
                        running = 0;
                        break;
                }
            }
        }
        
        refresh(); 
    }

    endwin();
    return 0;
}