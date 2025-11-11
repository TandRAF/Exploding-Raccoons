#include <ncurses.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_PLAYERS 5

typedef struct {
    char name[32];
    int ready;
} Player;

typedef struct {
    int current_screen;
    char player_name[32];
    int lobby_choice;
    Player players[MAX_PLAYERS];
    int player_count;
    int is_ready;
    int self_index; // <- new field to track your player index
} GameState;

void center_text(int y, const char *text) {
    mvprintw(y, (COLS - (int)strlen(text)) / 2, "%s", text);
}

void draw_button(int y, const char *text, int selected) {
    if (selected) attron(A_REVERSE);
    center_text(y, text);
    if (selected) attroff(A_REVERSE);
}

void draw_lobby(GameState *state) {
    attron(A_BOLD);
    center_text(2, "🔸 LOBBY SCREEN 🔸");
    attroff(A_BOLD);

    mvprintw(4, (COLS - 40)/2, "Lobby %d - Player: %s",
             state->lobby_choice, state->player_name);

    mvprintw(6, (COLS - 20)/2, "Players connected:");

    for (int i = 0; i < state->player_count; i++) {
        if (state->players[i].ready)
            mvprintw(8 + i, (COLS - 30)/2, "%s%s (Ready)    ",
                     state->players[i].name,
                     (i == state->self_index) ? " (You)" : "");
        else
            mvprintw(8 + i, (COLS - 30)/2, "%s%s (Not ready)",
                     state->players[i].name,
                     (i == state->self_index) ? " (You)" : "");
    }

    draw_button(14, state->is_ready ? "✅ READY" : "❌ NOT READY", 0);
    mvprintw(16, (COLS - 50)/2, "Press 'r' to toggle ready, 'b' to go back, 'q' to quit");
}

void draw_game_start(GameState *state) {
    clear();
    center_text(6, "🎲 ALL PLAYERS READY! 🎲");
    center_text(8, "Starting the game...");
    refresh();
}

int main(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    GameState state = {0};
    state.current_screen = 0;
    state.lobby_choice = 1;
    state.player_count = 3;

    // Pre-existing players
    strcpy(state.players[0].name, "Lewis");
    strcpy(state.players[1].name, "Ana");
    strcpy(state.players[2].name, "Mihai");
    state.players[0].ready = 1;
    state.players[1].ready = 1;
    state.players[2].ready = 1;

    int ch;
    int selected_lobby = 0;
    char name_input[32] = "";

    while (1) {
        clear();

        if (state.current_screen == 0) {
            attron(A_BOLD);
            center_text(2, "🐱 EXPLODING KITTENS 🧨");
            attroff(A_BOLD);

            const char *prompt = "Enter your name: ";
            int prompt_y = 5;
            int prompt_x = (COLS - 40) / 2;
            mvprintw(prompt_y, prompt_x, "%s", prompt);

            int input_x = prompt_x + (int)strlen(prompt);
            int input_width = 20;
            mvprintw(prompt_y - 1, input_x - 1, "+----------------------+");
            mvprintw(prompt_y + 1, input_x - 1, "+----------------------+");
            mvprintw(prompt_y, input_x - 1, "|");
            mvprintw(prompt_y, input_x + input_width, "|");

            echo();
            curs_set(1);
            move(prompt_y, input_x);
            refresh();
            mvgetnstr(prompt_y, input_x, name_input, input_width - 1);
            noecho();
            curs_set(0);

            strncpy(state.player_name, name_input, sizeof(state.player_name)-1);
            state.player_name[sizeof(state.player_name)-1] = '\0';

            // Add your name as a new player
            state.self_index = state.player_count;
            strncpy(state.players[state.self_index].name, state.player_name, 31);
            state.players[state.self_index].ready = 0;
            state.player_count++;

            selected_lobby = 0;
            while (1) {
                clear();
                attron(A_BOLD);
                center_text(2, "Select a Lobby");
                attroff(A_BOLD);

                draw_button(6, "Lobby 1", selected_lobby == 0);
                draw_button(8, "Lobby 2", selected_lobby == 1);
                draw_button(10, "Lobby 3", selected_lobby == 2);
                mvprintw(13, (COLS - 35)/2, "Use ↑/↓ to move, ENTER to join, 'q' to quit");

                ch = getch();
                if (ch == KEY_UP && selected_lobby > 0) selected_lobby--;
                else if (ch == KEY_DOWN && selected_lobby < 2) selected_lobby++;
                else if (ch == '\n') {
                    state.lobby_choice = selected_lobby + 1;
                    state.current_screen = 1;
                    break;
                } else if (ch == 'q' || ch == 'Q') {
                    endwin();
                    return 0;
                }
            }
        }

        else if (state.current_screen == 1) {
            draw_lobby(&state);
            ch = getch();

            if (ch == 'r' || ch == 'R') {
                state.is_ready = !state.is_ready;
                state.players[state.self_index].ready = state.is_ready;
            } else if (ch == 'b' || ch == 'B') {
                state.current_screen = 0;
                continue;
            } else if (ch == 'q' || ch == 'Q') {
                break;
            }

            int all_ready = 1;
            for (int i = 0; i < state.player_count; i++) {
                if (!state.players[i].ready) {
                    all_ready = 0;
                    break;
                }
            }
            if (all_ready) {
                state.current_screen = 2;
                clear();
                draw_game_start(&state);
                sleep(2);
            }
        }

        else if (state.current_screen == 2) {
            clear();
            center_text(2, "🎮 GAME SCREEN 🎮");
            mvprintw(4, (COLS - 40)/2, "Welcome to the game, %s!", state.player_name);
            mvprintw(6, (COLS - 40)/2, "Here you'd see cards, etc.");
            mvprintw(10, (COLS - 30)/2, "Press 'q' to return to main menu");
            refresh();

            ch = getch();
            if (ch == 'q' || ch == 'Q') {
                state.current_screen = 0;
            }
        }

        refresh();
    }

    endwin();
    return 0;
}
