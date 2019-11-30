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
    Shape shape;
    Color color;
} Symbol;

typedef struct {
    Symbol grid[GRID_X][GRID_Y];
    int reset;
    int quit;
    int window_w;
    int window_h;
    int timer;
} Game_State;

/*
 
   Given the window's width and height are w and h.
   We want 50 px padding in all directions.
   The grid rect is w-100 by h-100.
   Bottom left corner is 50, 50. Top right corner is w-50, h-50.
   Each rectangle in the grid is w-100/GRID_X, h-100/GRID_Y.
   This assumes that the shapes are touching, flush against each other (might want to add padding later).

*/
void render(SDL_Renderer *renderer, Game_State *game_state)
{
    SDL_RenderClear(renderer);

    // Set background color.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderFillRect(renderer, NULL);

    // Draw shapes.

    int grid_outer_padding = 50;

    SDL_Rect rect;
    rect.w = (game_state->window_w - 2*grid_outer_padding) / GRID_X;
    rect.h = (game_state->window_h - 2*grid_outer_padding) / GRID_Y;

    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            Symbol symbol = game_state->grid[i][j];

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

            rect.x = i * rect.w + grid_outer_padding;
            rect.y = j * rect.h + grid_outer_padding;

            // Draw the rect.
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    // TODO(bkaylor): UI! Print the count of symbols popped and the timer in seconds.

    SDL_RenderPresent(renderer);
}

int update(Game_State *game_state) 
{
    // Reset the board if needed.
    if (game_state->reset) {
        for (int i = 0; i < GRID_X; i++)
        {
            for (int j = 0; j < GRID_Y; j++)
            {
                game_state->grid[i][j].color = rand() % 5; 
                game_state->grid[i][j].shape = rand() % 1;
                printf("%d ", game_state->grid[i][j].color);
            }

            printf("\n");
        }

        printf("\n");

        game_state->timer = RESET_SECONDS * 1000;
        game_state->reset = 0;
    }

    // TODO(bkaylor): User input.
    // You should be able to click and drag a symbol to an adjacent spot to swap them.
    // Process player move.

    // Check for matches.
    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            // TODO(bkaylor): Um, this could use some compression.
            // Also, it doesn't have to be done every frame ... 

            // TODO(bkaylor): A match should be able to be more than length 3.

            // TODO(bkaylor): Pop symbols when match.
            // When a match is found, the three symbols should be popped and the pieces above should move down,
            // and any new needed symbols to keep the grid to GRID_X * GRID_Y should be generated.
            int x_inc, y_inc, x_del, y_del, length; 

            // Up
            x_inc = 0;      y_inc = 1;
            x_del = 0;      y_del = 0;
            x_del += x_inc; y_del += y_inc;
            length = 1;
            while ((i + x_del < GRID_Y) && (j + y_del < GRID_Y) && 
                   (i + x_del >= 0) && (j + y_del >= 0) && 
                   (length < 3) &&
                   (game_state->grid[i][j].color == game_state->grid[i + x_del][j + y_del].color)) {
                x_del += x_inc;
                y_del += y_inc;
                length++;
                if (length >= 3) {
                    printf("up match at (%d,%d)\n", i, j);
                }
            }
            // Down
            x_inc = 0;      y_inc = -1;
            x_del = 0;      y_del = 0;
            x_del += x_inc; y_del += y_inc;
            length = 1;
            while ((i + x_del < GRID_Y) && (j + y_del < GRID_Y) && 
                   (i + x_del >= 0) && (j + y_del >= 0) && 
                   (length < 3) &&
                   (game_state->grid[i][j].color == game_state->grid[i + x_del][j + y_del].color)) {
                x_del += x_inc;
                y_del += y_inc;
                length++;
                if (length >= 3) {
                    printf("down match at (%d,%d)\n", i, j);
                }
            }
            // Left
            x_inc = -1;      y_inc = 0;
            x_del = 0;      y_del = 0;
            x_del += x_inc; y_del += y_inc;
            length = 1;
            while ((i + x_del < GRID_Y) && (j + y_del < GRID_Y) && 
                   (i + x_del >= 0) && (j + y_del >= 0) && 
                   (length < 3) &&
                   (game_state->grid[i][j].color == game_state->grid[i + x_del][j + y_del].color)) {
                x_del += x_inc;
                y_del += y_inc;
                length++;
                if (length >= 3) {
                    printf("left match at (%d,%d)\n", i, j);
                }
            }
            // Right
            x_inc = 1;      y_inc = 0;
            x_del = 0;      y_del = 0;
            x_del += x_inc; y_del += y_inc;
            length = 1;
            while ((i + x_del < GRID_Y) && (j + y_del < GRID_Y) && 
                   (i + x_del >= 0) && (j + y_del >= 0) && 
                   (length < 3) &&
                   (game_state->grid[i][j].color == game_state->grid[i + x_del][j + y_del].color)) {
                x_del += x_inc;
                y_del += y_inc;
                length++;
                if (length >= 3) {
                    printf("right match at (%d,%d)\n", i, j);
                }
            }
        }
    }

    // Reset the board if the timer is up.
    if (game_state->timer < 0) {
        game_state->reset = 1;
    }

    return 0;
}

// TODO(bkaylor): Mouse input
void get_input(SDL_Renderer *ren, Game_State *game_state)
{
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

    // Setup main loop
    srand(time(NULL));

    // Build game state
    Game_State game_state;
    game_state.reset = 1;
    game_state.quit = 0;

    // Main loop
    const float FPS_INTERVAL = 1.0f;
    Uint64 frame_time_start, frame_time_finish, delta_t;

    while (!game_state.quit)
    {
        frame_time_start = SDL_GetTicks();

        SDL_PumpEvents();
        get_input(ren, &game_state);

        if (!game_state.quit)
        {
            SDL_GetWindowSize(win, &game_state.window_w, &game_state.window_h);

            update(&game_state);
            render(ren, &game_state);

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
