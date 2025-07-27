/*
 * main.c - A classic Pac-Man style game using SDL2
 *
 * This application is designed to be cross-compiled on a Linux system
 * to generate a standalone executable for Windows.
 * It uses custom-drawn characters and numbers, requiring no font files.
 */

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// --- Game Constants ---
#define MAP_COLS 19
#define MAP_ROWS 21
#define TILE_SIZE 24
#define SCOREBOARD_HEIGHT 60
#define SCREEN_WIDTH (MAP_COLS * TILE_SIZE)
#define SCREEN_HEIGHT (MAP_ROWS * TILE_SIZE + SCOREBOARD_HEIGHT)
#define PACMAN_SPEED 2
#define GHOST_SPEED 1
#define TUNNEL_ROW 10
#define SAMPLE_RATE 44100

// --- Enums and Structs ---
typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_NONE } Direction;

typedef struct {
    int x, y; // Position in pixel coordinates
    Direction dir;
    Direction next_dir; // Buffered direction from input
    int mouth_animation_timer;
} Pacman;

typedef struct {
    int x, y; // Position in pixel coordinates
    Direction dir;
    SDL_Color color;
} Ghost;

// --- Map Layout ---
// 0 = Empty/Tunnel, 1 = Wall, 2 = Pellet
int g_map[MAP_ROWS][MAP_COLS] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,2,1},
    {1,2,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,2,1,1,1,1,1,2,1,2,1,1,2,1},
    {1,2,2,2,2,1,2,2,2,1,2,2,2,1,2,2,2,2,1},
    {1,1,1,1,2,1,1,1,0,1,0,1,1,1,2,1,1,1,1},
    {0,0,0,1,2,1,0,0,0,0,0,0,0,1,2,1,0,0,0},
    {1,1,1,1,2,1,0,1,1,0,1,1,0,1,2,1,1,1,1},
    {0,2,2,2,2,0,0,1,0,0,0,1,0,0,2,2,2,2,0}, // Tunnel Row
    {1,1,1,1,2,1,0,1,1,1,1,1,0,1,2,1,1,1,1},
    {0,0,0,1,2,1,0,0,0,0,0,0,0,1,2,1,0,0,0},
    {1,1,1,1,2,1,0,1,1,1,1,1,0,1,2,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,2,1,1,1,2,1,2,1,1,1,2,1,1,2,1},
    {1,2,2,1,2,2,2,2,2,0,2,2,2,2,2,1,2,2,1},
    {1,1,2,1,2,1,2,1,1,1,1,1,2,1,2,1,2,1,1},
    {1,2,2,2,2,1,2,2,2,1,2,2,2,1,2,2,2,2,1},
    {1,2,1,1,1,1,1,1,2,1,2,1,1,1,1,1,1,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// --- Global Variables ---
SDL_Window* g_window = NULL;
SDL_Renderer* g_renderer = NULL;
SDL_Texture* g_pacman_textures[2]; // 0 = closed, 1 = open
SDL_Texture* g_ghost_texture;
Mix_Chunk* g_pellet_sound = NULL;
Mix_Chunk* g_death_sound = NULL;

Pacman g_pacman;
Ghost g_ghosts[4];
int g_pellets_left = 0;
int g_score = 0;
int g_lives = 3;
int g_game_over = 0;

// --- Function Prototypes ---
int initialize();
void create_character_textures();
void create_sounds();
void setup_game();
void handle_input(int* is_running);
void update_game();
void render_game();
void cleanup();
int is_wall(int x, int y);
void reset_characters();
void draw_circle(int cx, int cy, int radius, int fill);
void draw_number(int number, int x, int y);

// --- Main Function ---
int main(int argc, char* argv[]) {
    if (!initialize()) {
        cleanup();
        return 1;
    }

    setup_game();

    int is_running = 1;
    while (is_running && !g_game_over) {
        handle_input(&is_running);
        update_game();
        render_game();
        SDL_Delay(16); // Cap frame rate
    }

    cleanup();
    return 0;
}

// --- Function Implementations ---

int initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        return 0;
    }
    if(Mix_OpenAudio(SAMPLE_RATE, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        return 0;
    }

    g_window = SDL_CreateWindow("SDL Pac-Man", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!g_window) { return 0; }
    
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) { return 0; }

    srand(time(0));
    create_character_textures();
    create_sounds();
    return 1;
}

void create_sounds() {
    int pellet_sound_len = SAMPLE_RATE / 20;
    Sint16* pellet_data = malloc(pellet_sound_len * sizeof(Sint16));
    for (int i = 0; i < pellet_sound_len; ++i) {
        double time = (double)i / SAMPLE_RATE;
        pellet_data[i] = (Sint16)(3000 * sin(2.0 * M_PI * 988.0 * time));
    }
    g_pellet_sound = Mix_QuickLoad_RAW((Uint8*)pellet_data, pellet_sound_len * sizeof(Sint16));

    int death_sound_len = SAMPLE_RATE;
    Sint16* death_data = malloc(death_sound_len * sizeof(Sint16));
    for (int i = 0; i < death_sound_len; ++i) {
        double time = (double)i / SAMPLE_RATE;
        double freq = 440.0 - (time * 300.0);
        death_data[i] = (Sint16)(5000 * sin(2.0 * M_PI * freq * time) * (1.0 - time));
    }
    g_death_sound = Mix_QuickLoad_RAW((Uint8*)death_data, death_sound_len * sizeof(Sint16));
}

void create_character_textures() {
    for (int i = 0; i < 2; i++) {
        g_pacman_textures[i] = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, TILE_SIZE, TILE_SIZE);
        SDL_SetTextureBlendMode(g_pacman_textures[i], SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(g_renderer, g_pacman_textures[i]);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 0);
        SDL_RenderClear(g_renderer);
        SDL_SetRenderDrawColor(g_renderer, 255, 255, 0, 255);
        if (i == 0) {
            draw_circle(TILE_SIZE/2, TILE_SIZE/2, TILE_SIZE/2 - 2, 1);
        } else {
            for (int angle = 45; angle < 315; angle++) {
                float rad = angle * M_PI / 180.0;
                int x = TILE_SIZE/2 + cos(rad) * (TILE_SIZE/2 - 2);
                int y = TILE_SIZE/2 + sin(rad) * (TILE_SIZE/2 - 2);
                SDL_RenderDrawLine(g_renderer, TILE_SIZE/2, TILE_SIZE/2, x, y);
            }
        }
    }
    g_ghost_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, TILE_SIZE, TILE_SIZE);
    SDL_SetTextureBlendMode(g_ghost_texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(g_renderer, g_ghost_texture);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 0);
    SDL_RenderClear(g_renderer);
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    draw_circle(TILE_SIZE/2, TILE_SIZE/2, TILE_SIZE/2 - 2, 1);
    SDL_Rect body = {2, TILE_SIZE/2, TILE_SIZE - 4, TILE_SIZE/2};
    SDL_RenderFillRect(g_renderer, &body);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 200, 255);
    SDL_Rect eye1 = {TILE_SIZE/4, TILE_SIZE/3, TILE_SIZE/4, TILE_SIZE/4};
    SDL_Rect eye2 = {TILE_SIZE/2, TILE_SIZE/3, TILE_SIZE/4, TILE_SIZE/4};
    SDL_RenderFillRect(g_renderer, &eye1);
    SDL_RenderFillRect(g_renderer, &eye2);
    SDL_SetRenderTarget(g_renderer, NULL);
}

void setup_game() {
    g_pellets_left = 0;
    g_score = 0;
    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            if (g_map[r][c] == 2) {
                g_pellets_left++;
            }
        }
    }
    reset_characters();
}

void reset_characters() {
    g_pacman.x = 9 * TILE_SIZE + TILE_SIZE / 2;
    g_pacman.y = (16 * TILE_SIZE + TILE_SIZE / 2) + SCOREBOARD_HEIGHT;
    g_pacman.dir = DIR_RIGHT;
    g_pacman.next_dir = DIR_RIGHT;
    g_pacman.mouth_animation_timer = 0;

    g_ghosts[0] = (Ghost){ 9 * TILE_SIZE + TILE_SIZE/2, (8 * TILE_SIZE + TILE_SIZE/2) + SCOREBOARD_HEIGHT, DIR_LEFT, {255, 0, 0, 255} };
    g_ghosts[1] = (Ghost){ 9 * TILE_SIZE + TILE_SIZE/2, (10 * TILE_SIZE + TILE_SIZE/2) + SCOREBOARD_HEIGHT, DIR_RIGHT, {255, 184, 222, 255} };
    g_ghosts[2] = (Ghost){ 8 * TILE_SIZE + TILE_SIZE/2, (10 * TILE_SIZE + TILE_SIZE/2) + SCOREBOARD_HEIGHT, DIR_UP, {0, 255, 255, 255} };
    g_ghosts[3] = (Ghost){ 10 * TILE_SIZE + TILE_SIZE/2, (10 * TILE_SIZE + TILE_SIZE/2) + SCOREBOARD_HEIGHT, DIR_UP, {255, 184, 82, 255} };
}

void handle_input(int* is_running) {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            *is_running = 0;
        }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_UP:    g_pacman.next_dir = DIR_UP; break;
                case SDLK_DOWN:  g_pacman.next_dir = DIR_DOWN; break;
                case SDLK_LEFT:  g_pacman.next_dir = DIR_LEFT; break;
                case SDLK_RIGHT: g_pacman.next_dir = DIR_RIGHT; break;
            }
        }
    }
}

int is_wall(int x, int y) {
    int grid_y_map = (y - SCOREBOARD_HEIGHT) / TILE_SIZE;
    int grid_x = x / TILE_SIZE;
    
    if (grid_y_map == TUNNEL_ROW && (grid_x < 0 || grid_x >= MAP_COLS)) {
        return 0;
    }
    if (grid_x < 0 || grid_x >= MAP_COLS || grid_y_map < 0 || grid_y_map >= MAP_ROWS) {
        return 1;
    }
    return g_map[grid_y_map][grid_x] == 1;
}

void update_game() {
    // --- Pac-Man Movement ---
    // First, move Pac-Man based on his current direction
    if (g_pacman.dir == DIR_UP) g_pacman.y -= PACMAN_SPEED;
    else if (g_pacman.dir == DIR_DOWN) g_pacman.y += PACMAN_SPEED;
    else if (g_pacman.dir == DIR_LEFT) g_pacman.x -= PACMAN_SPEED;
    else if (g_pacman.dir == DIR_RIGHT) g_pacman.x += PACMAN_SPEED;

    // Second, handle the tunnel teleportation immediately after moving.
    // This is the key change to fix the teleport bug.
    if (g_pacman.x < -TILE_SIZE / 2) {
        g_pacman.x = SCREEN_WIDTH + TILE_SIZE / 2;
    } else if (g_pacman.x > SCREEN_WIDTH + TILE_SIZE / 2) {
        g_pacman.x = -TILE_SIZE / 2;
    }

    // Third, perform all grid-based logic (changing direction, stopping at walls)
    int pac_map_y = g_pacman.y - SCOREBOARD_HEIGHT;
    int pac_grid_x = g_pacman.x / TILE_SIZE;
    int pac_grid_y = pac_map_y / TILE_SIZE;
    int centered_x = (g_pacman.x % TILE_SIZE == TILE_SIZE / 2);
    int centered_y = (pac_map_y % TILE_SIZE == TILE_SIZE / 2);

    if (centered_x && centered_y) {
        // Check if the player's desired direction is valid.
        if (g_pacman.next_dir != DIR_NONE) {
            int check_x = pac_grid_x;
            int check_y = pac_grid_y;
            if (g_pacman.next_dir == DIR_UP) check_y--;
            else if (g_pacman.next_dir == DIR_DOWN) check_y++;
            else if (g_pacman.next_dir == DIR_LEFT) check_x--;
            else if (g_pacman.next_dir == DIR_RIGHT) check_x++;
            
            if (!is_wall(check_x * TILE_SIZE, (check_y * TILE_SIZE) + SCOREBOARD_HEIGHT)) {
                g_pacman.dir = g_pacman.next_dir;
            }
        }

        // Check if the path in the CURRENT direction is blocked. If so, stop.
        int check_x = pac_grid_x;
        int check_y = pac_grid_y;
        if (g_pacman.dir == DIR_UP) check_y--;
        else if (g_pacman.dir == DIR_DOWN) check_y++;
        else if (g_pacman.dir == DIR_LEFT) check_x--;
        else if (g_pacman.dir == DIR_RIGHT) check_x++;

        if (is_wall(check_x * TILE_SIZE, (check_y * TILE_SIZE) + SCOREBOARD_HEIGHT)) {
            g_pacman.dir = DIR_NONE;
        }
    }

    // Eat pellets
    pac_grid_x = g_pacman.x / TILE_SIZE;
    pac_grid_y = (g_pacman.y - SCOREBOARD_HEIGHT) / TILE_SIZE;
    if (pac_grid_x >= 0 && pac_grid_x < MAP_COLS && pac_grid_y >= 0 && pac_grid_y < MAP_ROWS) {
        if (g_map[pac_grid_y][pac_grid_x] == 2) {
            g_map[pac_grid_y][pac_grid_x] = 0;
            g_pellets_left--;
            g_score += 10;
            Mix_PlayChannel(-1, g_pellet_sound, 0);
            if (g_pellets_left == 0) { g_score += 1000; printf("You Win!\n"); g_game_over = 1; }
        }
    }
    
    // --- Ghost Movement & Collision ---
    for (int i = 0; i < 4; i++) {
        int ghost_map_y = g_ghosts[i].y - SCOREBOARD_HEIGHT;
        if (g_ghosts[i].x % TILE_SIZE == TILE_SIZE / 2 && ghost_map_y % TILE_SIZE == TILE_SIZE / 2) {
            Direction possible[4]; int count = 0;
            if (!is_wall(g_ghosts[i].x, g_ghosts[i].y - TILE_SIZE) && g_ghosts[i].dir != DIR_DOWN) possible[count++] = DIR_UP;
            if (!is_wall(g_ghosts[i].x, g_ghosts[i].y + TILE_SIZE) && g_ghosts[i].dir != DIR_UP) possible[count++] = DIR_DOWN;
            if (!is_wall(g_ghosts[i].x - TILE_SIZE, g_ghosts[i].y) && g_ghosts[i].dir != DIR_RIGHT) possible[count++] = DIR_LEFT;
            if (!is_wall(g_ghosts[i].x + TILE_SIZE, g_ghosts[i].y) && g_ghosts[i].dir != DIR_LEFT) possible[count++] = DIR_RIGHT;
            if (count > 0) g_ghosts[i].dir = possible[rand() % count];
        }

        if (g_ghosts[i].dir == DIR_UP) g_ghosts[i].y -= GHOST_SPEED;
        else if (g_ghosts[i].dir == DIR_DOWN) g_ghosts[i].y += GHOST_SPEED;
        else if (g_ghosts[i].dir == DIR_LEFT) g_ghosts[i].x -= GHOST_SPEED;
        else if (g_ghosts[i].dir == DIR_RIGHT) g_ghosts[i].x += GHOST_SPEED;

        if (g_ghosts[i].x < -TILE_SIZE/2) g_ghosts[i].x = SCREEN_WIDTH + TILE_SIZE/2;
        if (g_ghosts[i].x > SCREEN_WIDTH + TILE_SIZE/2) g_ghosts[i].x = -TILE_SIZE/2;

        SDL_Rect pac_rect = {g_pacman.x - TILE_SIZE/2 + 4, g_pacman.y - TILE_SIZE/2 + 4, TILE_SIZE-8, TILE_SIZE-8};
        SDL_Rect ghost_rect = {g_ghosts[i].x - TILE_SIZE/2 + 4, g_ghosts[i].y - TILE_SIZE/2 + 4, TILE_SIZE-8, TILE_SIZE-8};
        if (SDL_HasIntersection(&pac_rect, &ghost_rect)) {
            g_lives--;
            Mix_PlayChannel(-1, g_death_sound, 0);
            if (g_lives > 0) { reset_characters(); SDL_Delay(1000); }
            else { printf("Game Over!\n"); g_game_over = 1; }
        }
    }
}

void draw_digit(int digit, int x, int y) {
    int segments[10][7] = {
        {1,1,1,0,1,1,1}, {0,0,1,0,0,1,0}, {1,0,1,1,1,0,1}, {1,0,1,1,0,1,1}, {0,1,1,1,0,1,0},
        {1,1,0,1,0,1,1}, {1,1,0,1,1,1,1}, {1,0,1,0,0,1,0}, {1,1,1,1,1,1,1}, {1,1,1,1,0,1,1}
    };
    int seg_w = TILE_SIZE / 2;
    int seg_h = 3;
    if (segments[digit][0]) { SDL_Rect r = {x, y, seg_w, seg_h}; SDL_RenderFillRect(g_renderer, &r); }
    if (segments[digit][1]) { SDL_Rect r = {x, y, seg_h, seg_w}; SDL_RenderFillRect(g_renderer, &r); }
    if (segments[digit][2]) { SDL_Rect r = {x + seg_w - seg_h, y, seg_h, seg_w}; SDL_RenderFillRect(g_renderer, &r); }
    if (segments[digit][3]) { SDL_Rect r = {x, y + seg_w - seg_h, seg_w, seg_h}; SDL_RenderFillRect(g_renderer, &r); }
    if (segments[digit][4]) { SDL_Rect r = {x, y + seg_w, seg_h, seg_w}; SDL_RenderFillRect(g_renderer, &r); }
    if (segments[digit][5]) { SDL_Rect r = {x + seg_w - seg_h, y + seg_w, seg_h, seg_w}; SDL_RenderFillRect(g_renderer, &r); }
    if (segments[digit][6]) { SDL_Rect r = {x, y + 2*seg_w - seg_h, seg_w, seg_h}; SDL_RenderFillRect(g_renderer, &r); }
}

void draw_number(int number, int x, int y) {
    if (number == 0) {
        draw_digit(0, x, y);
        return;
    }
    char buffer[12];
    sprintf(buffer, "%d", number);
    for (int i = 0; buffer[i] != '\0'; i++) {
        draw_digit(buffer[i] - '0', x + i * (TILE_SIZE / 2 + 4), y);
    }
}

void render_game() {
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);
    
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    draw_number(g_score, 10, 10);
    draw_number(g_lives, SCREEN_WIDTH - 60, 10);

    for (int r = 0; r < MAP_ROWS; r++) {
        for (int c = 0; c < MAP_COLS; c++) {
            SDL_Rect rect = {c * TILE_SIZE, r * TILE_SIZE + SCOREBOARD_HEIGHT, TILE_SIZE, TILE_SIZE};
            if (g_map[r][c] == 1) {
                SDL_SetRenderDrawColor(g_renderer, 0, 0, 200, 255);
                SDL_RenderFillRect(g_renderer, &rect);
            } else if (g_map[r][c] == 2) {
                SDL_SetRenderDrawColor(g_renderer, 255, 255, 0, 255);
                SDL_Rect pellet = {c * TILE_SIZE + TILE_SIZE/2 - 2, r * TILE_SIZE + TILE_SIZE/2 - 2 + SCOREBOARD_HEIGHT, 4, 4};
                SDL_RenderFillRect(g_renderer, &pellet);
            }
        }
    }

    g_pacman.mouth_animation_timer = (g_pacman.mouth_animation_timer + 1) % 20;
    int mouth_state = (g_pacman.mouth_animation_timer < 10) ? 1 : 0;
    SDL_Rect pac_dest_rect = {g_pacman.x - TILE_SIZE/2, g_pacman.y - TILE_SIZE/2, TILE_SIZE, TILE_SIZE};
    double angle = 0;
    if (g_pacman.dir == DIR_DOWN) angle = 90;
    else if (g_pacman.dir == DIR_UP) angle = -90;
    else if (g_pacman.dir == DIR_LEFT) angle = 180;
    SDL_RenderCopyEx(g_renderer, g_pacman_textures[mouth_state], NULL, &pac_dest_rect, angle, NULL, SDL_FLIP_NONE);

    for (int i = 0; i < 4; i++) {
        SDL_SetTextureColorMod(g_ghost_texture, g_ghosts[i].color.r, g_ghosts[i].color.g, g_ghosts[i].color.b);
        SDL_Rect ghost_dest_rect = {g_ghosts[i].x - TILE_SIZE/2, g_ghosts[i].y - TILE_SIZE/2, TILE_SIZE, TILE_SIZE};
        SDL_RenderCopy(g_renderer, g_ghost_texture, NULL, &ghost_dest_rect);
    }

    SDL_RenderPresent(g_renderer);
}

void cleanup() {
    Mix_FreeChunk(g_pellet_sound);
    Mix_FreeChunk(g_death_sound);
    g_pellet_sound = NULL;
    g_death_sound = NULL;
    Mix_Quit();

    if (g_pacman_textures[0]) SDL_DestroyTexture(g_pacman_textures[0]);
    if (g_pacman_textures[1]) SDL_DestroyTexture(g_pacman_textures[1]);
    if (g_ghost_texture) SDL_DestroyTexture(g_ghost_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    g_pacman_textures[0] = NULL;
    g_pacman_textures[1] = NULL;
    g_ghost_texture = NULL;
    g_renderer = NULL;
    g_window = NULL;
    SDL_Quit();
}

void draw_circle(int cx, int cy, int radius, int fill) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                SDL_RenderDrawPoint(g_renderer, cx + x, cy + y);
            }
        }
    }
}
