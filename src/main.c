#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_image.h"


// TODO(bkaylor): Grid size and timer should be selectable.
#define GRID_X 6
#define GRID_Y 5
#define RESET_SECONDS 10 

// TODO(bkaylor): Shapes?
typedef enum {
    RECTANGLE = 0
} Shape;

// TODO(bkaylor): Auto-generate the colors based on color theory.
typedef enum {
    GREEN = 0,
    YELLOW = 1,
    RED = 2,
    BLUE = 3,
    PURPLE = 4
} Color;

typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    Shape shape;
    Color color;
    Position position;
    int matched;
    int popped;
    int hovered;
    int selected;
} Symbol;

typedef struct {
    int x;
    int y;
    int pressed;
} Mouse_State;

typedef struct {
    int x;
    int y;
    int active;
} Selection_Info;

typedef struct {
    Symbol grid[GRID_X][GRID_Y];
    Selection_Info *hovered;
    Selection_Info *selected;
    int reset;
    int quit;
    int window_w;
    int window_h;
    int timer;
    int score;
    int need_to_look_for_matches;
    int symbol_width;
    int symbol_height;
    int grid_outer_padding;
    int board_count;
} Game_State;

/*
 
   Given the window's width and height are w and h.
   We want 50 px padding in all directions.
   The grid rect is w-100 by h-100.
   Bottom left corner is 50, 50. Top right corner is w-50, h-50.
   Each rectangle in the grid is w-100/GRID_X, h-100/GRID_Y.
   This assumes that the shapes are touching, flush against each other (might want to add padding later).

*/
void render(SDL_Renderer *renderer, Game_State *game_state, TTF_Font *font, SDL_Color font_color)
{
    SDL_RenderClear(renderer);

    // Set background color.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderFillRect(renderer, NULL);
    SDL_Rect rect;
    rect.w = game_state->symbol_width;
    rect.h = game_state->symbol_height;

    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            Symbol symbol = game_state->grid[i][j];

            if (symbol.popped) { continue; }

            // r g b (255)
            if (symbol.color == GREEN) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            } else if (symbol.color == YELLOW) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            } else if (symbol.color == RED){
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            } else if (symbol.color == BLUE) {
                SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
            } else if (symbol.color == PURPLE) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
            }

            rect.x = i * rect.w + game_state->grid_outer_padding;
            rect.y = j * rect.h + game_state->grid_outer_padding;

            // Draw the rect.
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    // UI
    char score_string[10];
    sprintf(score_string, "%dpts", game_state->score);
    char timer_string[10];
    sprintf(timer_string, "%dms", game_state->timer);
    char hovered_string[20];
    if (game_state->hovered->active) {
        sprintf(hovered_string, "%d, %d hovered", game_state->hovered->x, game_state->hovered->y);
    } else {
        sprintf(hovered_string, "nothing hovered");
    }

    // Score
    SDL_Surface *score_surface = TTF_RenderText_Solid(font, score_string, font_color);
    SDL_Texture *score_texture = SDL_CreateTextureFromSurface(renderer, score_surface);
    int score_x, score_y;
    SDL_QueryTexture(score_texture, NULL, NULL, &score_x, &score_y);
    SDL_Rect score_rect = {5 , 5, score_x, score_y};

    SDL_RenderCopy(renderer, score_texture, NULL, &score_rect);

    // Timer
    SDL_Surface *timer_surface = TTF_RenderText_Solid(font, timer_string, font_color);
    SDL_Texture *timer_texture = SDL_CreateTextureFromSurface(renderer, timer_surface);
    int timer_x, timer_y;
    SDL_QueryTexture(timer_texture, NULL, NULL, &timer_x, &timer_y);
    SDL_Rect timer_rect = {5 , 5 + 10, timer_x, timer_y};

    SDL_RenderCopy(renderer, timer_texture, NULL, &timer_rect);

    // Hovered
    SDL_Surface *hovered_surface = TTF_RenderText_Solid(font, hovered_string, font_color);
    SDL_Texture *hovered_texture = SDL_CreateTextureFromSurface(renderer, hovered_surface);
    int hovered_x, hovered_y;
    SDL_QueryTexture(hovered_texture, NULL, NULL, &hovered_x, &hovered_y);
    SDL_Rect hovered_rect = {5 , 5 + 20, hovered_x, hovered_y};

    SDL_RenderCopy(renderer, hovered_texture, NULL, &hovered_rect);

    // Show
    SDL_RenderPresent(renderer);
}

void check_direction_for_matches(Symbol grid[GRID_X][GRID_Y], Position starting_position, int x_increment, int y_increment)
{
    int x_delta = 0, y_delta = 0;
    x_delta += x_increment; 
    y_delta += y_increment;

    int length = 1;

    int i = starting_position.x;
    int j = starting_position.y;

    int match = 0;
    while ((i + x_delta < GRID_X) && (j + y_delta < GRID_Y) && 
           (i + x_delta >= 0) && (j + y_delta >= 0) && 
           (!grid[i][j].popped) &&
           (grid[i][j].color == grid[i + x_delta][j + y_delta].color)) {
        x_delta += x_increment;
        y_delta += y_increment;
        length++;

        if (length >= 3) {
            match = 1;
            grid[i + x_delta][j + y_delta].matched = 1;
        }
    }

    if (match) {
        grid[starting_position.x][starting_position.y].matched = 1;
        grid[starting_position.x + x_increment][starting_position.y + y_increment].matched = 1;
    }

    if (match) {
        printf("match of length %d at (%d,%d)\n", length, starting_position.x, starting_position.y);
    }
}

int update(Game_State *game_state, Mouse_State *mouse_state) 
{
    // Reset the board if needed.
    if (game_state->reset) {
        game_state->board_count++;
        printf("**********\n");
        printf("Board %d\n", game_state->board_count);
        for (int i = 0; i < GRID_X; i++)
        {
            for (int j = 0; j < GRID_Y; j++)
            {
                game_state->grid[i][j].color = rand() % 5; 
                game_state->grid[i][j].shape = rand() % 1;
                game_state->grid[i][j].position.x = i;
                game_state->grid[i][j].position.y = j;
                game_state->grid[i][j].matched = 0;
                game_state->grid[i][j].popped = 0;
                printf("%d ", game_state->grid[i][j].color);
            }

            printf("\n");
        }

        printf("**********\n");
        printf("\n");

        game_state->timer = RESET_SECONDS * 1000;
        game_state->reset = 0;
        game_state->score = 0;
        game_state->need_to_look_for_matches = 1;
        game_state->hovered->active = 0;
    }

    // Get sizes of board.
    game_state->grid_outer_padding = 50;
    game_state->symbol_width = (game_state->window_w - 2*game_state->grid_outer_padding) / GRID_X; 
    game_state->symbol_height = (game_state->window_h - 2*game_state->grid_outer_padding) / GRID_Y;


#if 1
    // TODO(bkaylor): User input.
    // You should be able to click and drag a symbol to an adjacent spot to swap them.
    // Process player move.

    // Since everything is an integer, we need to do some special case around zero when 
    // the mouse is off the grid.
    int grid_mouse_state_x = (mouse_state->x - game_state->grid_outer_padding);
    int grid_mouse_state_y = (mouse_state->y - game_state->grid_outer_padding);
    if (grid_mouse_state_x > 0) {
        game_state->hovered->x = (grid_mouse_state_x) / game_state->symbol_width; 
    } else {
        game_state->hovered->x = -1;
    }
    if (grid_mouse_state_y > 0) {
        game_state->hovered->y = (grid_mouse_state_y) / game_state->symbol_height; 
    } else {
        game_state->hovered->y = -1;
    }

    if ((0 <= game_state->hovered->x && game_state->hovered->x < GRID_X) && 
        (0 <= game_state->hovered->y && game_state->hovered->y < GRID_Y)) {
        // game_state->grid[game_state->hovered->x][game_state->hovered->y].hovered = 1;
        game_state->hovered->active = 1;
    } else {
        if (game_state->hovered->active == 1) {
            // game_state->grid[game_state->hovered->x][game_state->hovered->y].hovered = 0;
            game_state->hovered->active = 0;
        }
    }
#endif

    // TODO(bkaylor): Exceptions coming from here on ~5% of boards.
#if 0
    // Check for matches.
    if (game_state->need_to_look_for_matches) {
        for (int i = 0; i < GRID_X; i++)
        {
            for (int j = 0; j < GRID_Y; j++)
            {
                // Up
                printf("checking up\n");
                check_direction_for_matches(game_state->grid, game_state->grid[i][j].position, 0, 1);
                printf("checked up\n");
                // Down
                printf("checking down\n");
                check_direction_for_matches(game_state->grid, game_state->grid[i][j].position, 0, -1);
                printf("checked down\n");
                // Left
                printf("checking left\n");
                check_direction_for_matches(game_state->grid, game_state->grid[i][j].position, -1, 0);
                printf("checked left\n");
                // Right
                printf("checking right\n");
                check_direction_for_matches(game_state->grid, game_state->grid[i][j].position, 1, 0);
                printf("checked right\n");
            }
        }

        // TODO(bkaylor): Pop symbols when match.
        // When a match is found, the three symbols should be popped and the pieces above should move down,
        // and any new needed symbols to keep the grid to GRID_X * GRID_Y should be generated.
        for (int i = 0; i < GRID_X; i++)
        {
            for (int j = 0; j < GRID_Y; j++)
            {
                if (game_state->grid[i][j].matched && !game_state->grid[i][j].popped) {
                    game_state->grid[i][j].popped = 1;
                    game_state->score++;
                }
            }
        }

        game_state->need_to_look_for_matches = 0;
    }
#endif 

    // Reset the board if the timer is up.
    if (game_state->timer < 0) {
        game_state->reset = 1;
    }

    return 0;
}

// TODO(bkaylor): Mouse input
void get_input(SDL_Renderer *ren, Game_State *game_state, Mouse_State *mouse_state)
{
    // Get mouse info.
    mouse_state->pressed = 0;
    SDL_GetMouseState(&mouse_state->x, &mouse_state->y);

    // Handle events.
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        game_state->quit = 1;
                        break;

                    case SDLK_r:
                        game_state->reset = 1;
                        break;

                    default:
                        break;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                mouse_state->pressed = event.button.button;
                break;

            case SDL_QUIT:
                game_state->quit = 1;
                break;

            default:
                break;
        }
    }
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_EVERYTHING);
    IMG_Init(IMG_INIT_PNG);

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL_Init video error: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_Init(SDL_INIT_AUDIO) != 0)
    {
        printf("SDL_Init audio error: %s\n", SDL_GetError());
        return 1;
    }

    // SDL_ShowCursor(SDL_DISABLE);

	// Setup window
	SDL_Window *win = SDL_CreateWindow("Match3",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			400, 400,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	// Setup renderer
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	// Setup font
	TTF_Init();
	TTF_Font *font = TTF_OpenFont("liberation.ttf", 12);
	if (!font)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error: Font", TTF_GetError(), win);
		return -666;
	}

	SDL_Color font_color = {255, 255, 255};

    // Setup main loop
    srand(time(NULL));

    // Build game state
    Selection_Info hovered = {0};
    Selection_Info selected = {0};
    Game_State game_state = {0};
    game_state.hovered = &hovered;
    game_state.selected = &selected;
    game_state.reset = 1;
    game_state.quit = 0;
    game_state.board_count = 0;

    Mouse_State mouse_state = {0};

    // Main loop
    const float FPS_INTERVAL = 1.0f;
    Uint64 frame_time_start, frame_time_finish, delta_t;

    while (!game_state.quit)
    {
        frame_time_start = SDL_GetTicks();

        SDL_PumpEvents();
        get_input(ren, &game_state, &mouse_state);

        if (!game_state.quit)
        {
            SDL_GetWindowSize(win, &game_state.window_w, &game_state.window_h);

            update(&game_state, &mouse_state);
            render(ren, &game_state, font, font_color);

            // Update timers;
            frame_time_finish = SDL_GetTicks();
            delta_t = frame_time_finish - frame_time_start;

            game_state.timer -= delta_t; 
        }
    }

	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
    return 0;
}
