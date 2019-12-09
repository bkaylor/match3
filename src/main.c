#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_image.h"

#define internal static

// TODO(bkaylor): Grid size and timer should be selectable via an options menu.
#define GRID_X 6
#define GRID_Y 5
#define RESET_SECONDS 30
#define POP_TIMER_SECONDS 0.3
#define SYMBOL_PADDING 2
#define MOVE_SECONDS 0.3
#define SWAP_SECONDS 0.3

// #define DEBUG
#define NICE_COLORS

// Global textures for now.
SDL_Texture *crosshair_texture;

// TODO(bkaylor): Shapes?
typedef enum {
    RECTANGLE = 0
} Shape;

// TODO(bkaylor): Auto-generate a color scheme?
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

typedef enum {
    UP = 0,
    DOWN = 1,
    LEFT = 2,
    RIGHT = 3
} Direction;

typedef struct {
    int is_moving;
    int is_starting_from_top;
    Direction direction;
    int timer;
    int timer_initial_value;
    int should_be_drawn_on_top;
} Animation_Info;

typedef struct {
    Shape shape;
    Color color;
    Position position;
    int matched;
    int popped;
    int hovered;
    int selected;
    int popping;
    int pop_timer;
    Animation_Info animation;
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
    Symbol *symbol;
} Selection_Info;

typedef struct {
    Symbol *starting_at;
    int length;
} Match_Record;

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
    int last_score;
    int need_to_look_for_matches;
    int symbol_width;
    int symbol_height;
    int grid_outer_padding;
    int board_count;
} Game_State;

internal void load_textures(SDL_Renderer *renderer) 
{
    SDL_Surface *surface = IMG_Load("../assets/crosshair.png");
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    crosshair_texture = texture;
}

internal void draw_text(SDL_Renderer *renderer, int x, int y, char *string, TTF_Font *font, SDL_Color font_color) {
    SDL_Surface *surface = TTF_RenderText_Solid(font, string, font_color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    int x_from_texture, y_from_texture;
    SDL_QueryTexture(texture, NULL, NULL, &x_from_texture, &y_from_texture);
    SDL_Rect rect = {x, y, x_from_texture, y_from_texture};

    SDL_RenderCopy(renderer, texture, NULL, &rect);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

internal void render(SDL_Renderer *renderer, Game_State *game_state, TTF_Font *font, SDL_Color font_color)
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

            // if (symbol.popped) { continue; }

            SDL_Color tile_color;

            if (symbol.color == GREEN) {
                tile_color.r = 0; 
                tile_color.g = 255; 
                tile_color.b = 0;
            } else if (symbol.color == YELLOW) {
                tile_color.r = 255; 
                tile_color.g = 255; 
                tile_color.b = 0;
            } else if (symbol.color == RED){
                tile_color.r = 255; 
                tile_color.g = 0; 
                tile_color.b = 0;
            } else if (symbol.color == BLUE) {
                tile_color.r = 0; 
                tile_color.g = 0; 
                tile_color.b = 255;
            } else if (symbol.color == PURPLE) {
                tile_color.r = 255; 
                tile_color.g = 0; 
                tile_color.b = 255;
            }

#ifdef NICE_COLORS
            // r g b (255)
            if (symbol.color == GREEN) {
                tile_color.r = 64; 
                tile_color.g = 97; 
                tile_color.b = 69;
            } else if (symbol.color == YELLOW) {
                tile_color.r = 221; 
                tile_color.g = 193; 
                tile_color.b = 117;
            } else if (symbol.color == RED){
                tile_color.r = 212; 
                tile_color.g = 98; 
                tile_color.b = 117;
            } else if (symbol.color == BLUE) {
                tile_color.r = 60; 
                tile_color.g = 120; 
                tile_color.b = 255;
            } else if (symbol.color == PURPLE) {
                tile_color.r = 180; 
                tile_color.g = 98; 
                tile_color.b = 191;
            }
#endif

            // Draw popping symbols as white.
            if (game_state->grid[i][j].popping) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                tile_color.r = 200;
                tile_color.g = 200;
                tile_color.b = 200;
            }

            SDL_SetRenderDrawColor(renderer, tile_color.r, tile_color.g, tile_color.b, 255);

            // TODO(bkaylor): When animating some swapping tiles, the selected tile in the swap should always be on top.
            // Animation.
            int x_adjustment = 0, y_adjustment = 0;
            if (game_state->grid[i][j].animation.is_moving) {
                int adjustment_factor_x = 0, adjustment_factor_y = 0;
                if (game_state->grid[i][j].animation.is_starting_from_top) {
                    adjustment_factor_x = ((float)game_state->grid[i][j].animation.timer / (game_state->grid[i][j].animation.timer_initial_value)) * rect.w;
                    adjustment_factor_y = ((float)game_state->grid[i][j].animation.timer / (game_state->grid[i][j].animation.timer_initial_value)) * rect.h * j;
                } else {
                    adjustment_factor_x = ((float)game_state->grid[i][j].animation.timer / (game_state->grid[i][j].animation.timer_initial_value)) * rect.w;
                    adjustment_factor_y = ((float)game_state->grid[i][j].animation.timer / (game_state->grid[i][j].animation.timer_initial_value)) * rect.h;
                }

                switch (game_state->grid[i][j].animation.direction) {
                    case UP:
                        y_adjustment -= adjustment_factor_y;
                    break;

                    case DOWN:
                        y_adjustment += adjustment_factor_y;
                    break;

                    case LEFT:
                        x_adjustment -= adjustment_factor_x;
                    break;

                    case RIGHT:
                        x_adjustment += adjustment_factor_x;
                    break;

                    default:
                    break;
                }
            }

            // TODO(bkaylor): For big grids (100 x 100), padding is uneven.
            rect.x = i * rect.w + game_state->grid_outer_padding + x_adjustment;
            rect.y = j * rect.h + game_state->grid_outer_padding + y_adjustment;

            SDL_Rect symbol_rect;
            symbol_rect.x = rect.x + SYMBOL_PADDING;
            symbol_rect.y = rect.y + SYMBOL_PADDING;
            symbol_rect.w = rect.w - SYMBOL_PADDING;
            symbol_rect.h = rect.h - SYMBOL_PADDING;

            // Draw the rect.
            SDL_RenderFillRect(renderer, &symbol_rect);

            // Draw crosshair on selected symbol.
            if (game_state->grid[i][j].selected) {
                SDL_RenderCopy(renderer, crosshair_texture, NULL, &symbol_rect);
            }
        }
    }

    // UI
    char score_string[20];
    sprintf(score_string, "%dpts (%d last)", game_state->score, game_state->last_score);
    char timer_string[10];
    sprintf(timer_string, "%ds", game_state->timer / 1000);

#ifdef DEBUG
    char hovered_string[20];
    char hovered_line_2_string[20];
    if (game_state->hovered->active) {
        sprintf(hovered_string, "%d, %d hovered", game_state->hovered->x, game_state->hovered->y);
    } else {
        sprintf(hovered_string, "nothing hovered");
    }


    char selected_string[20];
    if (game_state->selected->active) {
        sprintf(selected_string, "%d, %d selected", game_state->selected->x, game_state->selected->y);
    } else {
        sprintf(selected_string, "nothing selected");
    }
#endif

    int x = 5, y = -5;
    draw_text(renderer, x, y+=10, score_string, font, font_color);
    draw_text(renderer, x, y+=10, timer_string, font, font_color);

#ifdef DEBUG
    draw_text(renderer, x, y+=10, hovered_string, font, font_color);
    draw_text(renderer, x, y+=10, selected_string, font, font_color);
#endif

    // Show
    SDL_RenderPresent(renderer);
}

internal void initialize_symbol(Symbol *symbol, int i, int j)
{
    symbol->color = rand() % 5; 
    symbol->shape = rand() % 1;
    symbol->position.x = i;
    symbol->position.y = j;
    symbol->matched = 0;
    symbol->popping = 0;
    symbol->popped = 0;
    symbol->pop_timer = 0;
    symbol->hovered = 0;
    symbol->selected = 0;

    // symbol->animation = {0};
}

// TODO(bkaylor): Other match3 games have special effects when you make a long or interesting match.
// TODO(bkaylor): Add notes about the different Bejewled special effects.
Match_Record check_direction_for_match(Symbol grid[GRID_X][GRID_Y], Position starting_position, int x_increment, int y_increment)
{
    int match_found = 0;
    int match_length = 1;

    // Find length match in the (x_increment, y_increment) direction.
    {
        int i = starting_position.x + x_increment;
        int j = starting_position.y + y_increment;

        while ((i < GRID_X) && (j < GRID_Y) && (i >= 0) && (j >= 0) && 
               (!grid[i][j].popped) &&
               (grid[i][j].color == grid[starting_position.x][starting_position.y].color)) {
            match_length++;

            if (match_length >= 3) {
                match_found = 1;
            }

            i += x_increment;
            j += y_increment;
        }
    }

    // Build the list of matched symbols.
    // TODO(bkaylor): What should the size of the matched_symbols_list array actually be? Whatever the maximum match is.
    Symbol *matched_symbols_list[10];
    if (match_found) {
        int i = starting_position.x;
        int j = starting_position.y;
        int k = 0;
        while (k < match_length) {
            matched_symbols_list[k] = &grid[i][j];

            grid[i][j].matched = 1;

            i += x_increment;
            j += y_increment;
            k++;
        }
    }

#ifdef DEBUG
    if (match_found) {
        printf("match of length %d at (%d,%d)\n", match_length, starting_position.x, starting_position.y);
        for (int k = 0; k < match_length; k++)
        {
            printf("(%d,%d) ", matched_symbols_list[k]->position.x, matched_symbols_list[k]->position.y);
        }
        printf("\n");
    }
#endif

    Match_Record match;
    match.starting_at = &grid[starting_position.x][starting_position.y];
    match.length = match_length;

    return match;
}

int update(Game_State *game_state, Mouse_State *mouse_state) 
{
    // Reset the board if needed.
    if (game_state->reset) {
        game_state->board_count++;

#ifdef DEBUG
        printf("**********\n");
        printf("Board %d\n", game_state->board_count);
#endif

        for (int i = 0; i < GRID_X; i++)
        {
            for (int j = 0; j < GRID_Y; j++)
            {
                initialize_symbol(&game_state->grid[i][j], i, j);
#ifdef DEBUG
                printf("%d ", game_state->grid[i][j].color);
#endif
            }

#ifdef DEBUG
            printf("\n");
#endif
        }

#ifdef DEBUG
        printf("**********\n");
        printf("\n");
#endif

        game_state->timer = RESET_SECONDS * 1000;
        game_state->reset = 0;
        game_state->last_score = game_state->score;
        game_state->score = 0;
        game_state->need_to_look_for_matches = 1;
        game_state->hovered->active = 0;
        game_state->selected->active = 0;
    }

    // Get sizes of board.
    game_state->grid_outer_padding = 80;
    game_state->symbol_width = (game_state->window_w - 2*game_state->grid_outer_padding) / GRID_X; 
    game_state->symbol_height = (game_state->window_h - 2*game_state->grid_outer_padding) / GRID_Y;

    // Process input.
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
        game_state->hovered->active = 1;
        game_state->hovered->symbol = &game_state->grid[game_state->hovered->x][game_state->hovered->y];
        game_state->hovered->symbol->hovered = 1;
    } else {
        if (game_state->hovered->active) {
            game_state->hovered->symbol->hovered = 0;
        }

        game_state->hovered->active = 0;
        game_state->hovered->symbol = NULL;
    }

    // On left click, select the hovered tile.
    // TODO(bkaylor): You shouldn't be able to just move tiles to an adjacent space. Moves should only 
    //                be valid when they result in a match, and the game moves your piece back on invalid swap.
    // TODO(bkaylor): Try clicking and dragging control scheme instead of left click to select and left click to swap.
    {
        Selection_Info *hovered = game_state->hovered;
        Selection_Info *selected = game_state->selected;
        if (hovered->active && mouse_state->pressed == SDL_BUTTON_LEFT) {
            if (selected->active) {
                if ((hovered->x == selected->x && hovered->y == selected->y-1) ||
                    (hovered->x == selected->x && hovered->y == selected->y+1) ||
                    (hovered->x == selected->x-1 && hovered->y == selected->y) ||
                    (hovered->x == selected->x+1 && hovered->y == selected->y)) { 
                    // Swap the tiles.
                    Symbol temp = game_state->grid[hovered->x][hovered->y];
                    game_state->grid[hovered->x][hovered->y] = game_state->grid[selected->x][selected->y];
                    game_state->grid[selected->x][selected->y] = temp;

                    Position temp_position = game_state->grid[hovered->x][hovered->y].position;
                    game_state->grid[hovered->x][hovered->y].position = game_state->grid[selected->x][selected->y].position; 
                    game_state->grid[selected->x][selected->y].position = temp_position;

                    // Setup swap animation.
                    Direction move_direction_of_selected;
                    Direction move_direction_of_hovered;
                    if (selected->x - hovered->x > 0) {
                        move_direction_of_selected = LEFT;
                        move_direction_of_hovered = RIGHT;
                    } else if (selected->x - hovered->x < 0) {
                        move_direction_of_selected = RIGHT;
                        move_direction_of_hovered = LEFT;
                    } else if (selected->y - hovered->y > 0) {
                        move_direction_of_selected = UP;
                        move_direction_of_hovered = DOWN;
                    } else if (selected->y - hovered->y < 0) {
                        move_direction_of_selected = DOWN;
                        move_direction_of_hovered = UP;
                    }

                    // Set animation state of moving tiles.
                    hovered->symbol->animation.is_moving = 1;
                    hovered->symbol->animation.direction = move_direction_of_hovered;
                    hovered->symbol->animation.timer = 1000 * SWAP_SECONDS;
                    hovered->symbol->animation.timer_initial_value = hovered->symbol->animation.timer;

                    selected->symbol->animation.is_moving = 1;
                    selected->symbol->animation.direction = move_direction_of_selected;
                    selected->symbol->animation.timer = 1000 * SWAP_SECONDS;
                    selected->symbol->animation.timer_initial_value = selected->symbol->animation.timer;
                    selected->symbol->animation.should_be_drawn_on_top = 1;

                    // Remove selection.
                    hovered->symbol->selected = 0;

                    selected->active = 0;
                    selected->symbol->selected = 0;
                    selected->symbol = NULL;
                }
            } else {
                selected->active = 1;
                selected->x = hovered->x;
                selected->y = hovered->y;
                selected->symbol = hovered->symbol;
                selected->symbol->selected = 1;
            }
        }

        // On right click, unselect.
        if (selected->active && mouse_state->pressed == SDL_BUTTON_RIGHT) {
            if (selected->active) {
                selected->active = 0;
                selected->symbol->selected = 0;
                selected->symbol = NULL;
            }
        }
    }

    // Check for matches.
    if (game_state->need_to_look_for_matches) {
        for (int i = 0; i < GRID_X; i++)
        {
            for (int j = 0; j < GRID_Y; j++)
            {
                if (!game_state->grid[i][j].matched) {
                    Position position = {i, j};
                    check_direction_for_match(game_state->grid, position, 0, 1);
                    check_direction_for_match(game_state->grid, position, 0, -1);
                    check_direction_for_match(game_state->grid, position, -1, 0);
                    check_direction_for_match(game_state->grid, position, 1, 0);
                }
            }
        }

        game_state->need_to_look_for_matches = 0;
    }

    // TODO(bkaylor): Should popped, matched, popping, etc be a state enum?
    // Handle any popped tiles.
    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            if (game_state->grid[i][j].matched && !game_state->grid[i][j].popped && !game_state->grid[i][j].animation.is_moving) {
                if (game_state->grid[i][j].popping) {
                    if (game_state->grid[i][j].pop_timer <= 0) {
                        game_state->grid[i][j].popped = 1;
                        game_state->grid[i][j].popping = 0;
                        game_state->score++;
                    }
                } else {
                    game_state->grid[i][j].popping = 1;
                    game_state->grid[i][j].pop_timer = POP_TIMER_SECONDS * 1000;
                }
            }
        }
    }

    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            if (game_state->grid[i][j].popped) {
                int k = j;
                while (k > 0) {
                    game_state->grid[i][k] = game_state->grid[i][k-1];

                    Position temp_position = game_state->grid[i][k].position;
                    game_state->grid[i][k].position = game_state->grid[i][k-1].position; 
                    game_state->grid[i][k-1].position = temp_position;

                    // Set the symbol to move downwards.
                    game_state->grid[i][k].animation.is_moving = 1;
                    game_state->grid[i][k].animation.direction = UP; // TODO(bkaylor): Why up instead of down?
                    game_state->grid[i][k].animation.timer = 1000 * MOVE_SECONDS;
                    game_state->grid[i][k].animation.timer_initial_value = game_state->grid[i][k].animation.timer; 

                    k -= 1;
                }

                initialize_symbol(&game_state->grid[i][0], i, 0);
                game_state->grid[i][0].animation.is_moving = 1;
                game_state->grid[i][0].animation.direction = UP; // TODO(bkaylor): Why up instead of down?
                game_state->grid[i][0].animation.timer = 1000 * MOVE_SECONDS;
                game_state->grid[i][0].animation.timer_initial_value = game_state->grid[i][0].animation.timer; 
                game_state->grid[i][0].animation.is_starting_from_top = 1;

                game_state->need_to_look_for_matches = 1;
            }
        }
    }

    // Cleanup animation state.
    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            Symbol *symbol = &game_state->grid[i][j];
            if (symbol->animation.is_moving && symbol->animation.timer <= 0) {
                symbol->animation.is_moving = 0;
                symbol->animation.is_starting_from_top = 0;

                // State has changed- look for matches again.
                game_state->need_to_look_for_matches = 1;
            }
        }
    }

    // Reset the board if the timer is up.
    if (game_state->timer < 0) {
        game_state->reset = 1;
    }

    return 0;
}

internal void get_input(SDL_Renderer *ren, Game_State *game_state, Mouse_State *mouse_state)
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
			800, 600,
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

    // Load images.
    load_textures(ren);

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

            // Update timers.
            frame_time_finish = SDL_GetTicks();
            delta_t = frame_time_finish - frame_time_start;

            // Cap at 60ish
            int delta_t_difference = 16 - delta_t; 
            if (delta_t_difference > 0) {
                SDL_Delay(delta_t_difference);
                delta_t += delta_t_difference;
            }

            game_state.timer -= delta_t; 

            // TODO(bkaylor): It doesn't feel good to do this here.
            for (int i = 0; i < GRID_X; i++)
            {
                for (int j = 0; j < GRID_Y; j++)
                {
                    if (game_state.grid[i][j].popping) {
                        game_state.grid[i][j].pop_timer -= delta_t;
                    }

                    if (game_state.grid[i][j].animation.is_moving) {
                        game_state.grid[i][j].animation.timer -= delta_t;
                    }
                }
            }
        }
    }

	SDL_DestroyRenderer(ren);
	SDL_DestroyWindow(win);
	SDL_Quit();
    return 0;
}
