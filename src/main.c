#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_image.h"

#define internal static

// TODO(bkaylor): There's still a bug somewhere where, what looks like it should be a match doesn't let you swap. 

// TODO(bkaylor): Grid size and timer should be selectable via an options menu.
#define GRID_X 6
#define GRID_Y 5
#define SYMBOL_PADDING 2
#define RESET_SECONDS 30
#define POP_SECONDS 0.7
#define MOVE_SECONDS 0.3
#define SWAP_SECONDS 0.3
#define NUMBER_OF_COLORS 5
#define NUMBER_OF_SHAPES 1

// #define DEBUG
// #define CAP_FRAMERATE

// Global textures for now.
SDL_Texture *crosshair_texture;

// TODO(bkaylor): Shapes?
typedef enum {
    RECTANGLE
} Shape;

// TODO(bkaylor): Auto-generate a color scheme?
typedef enum {
    GREEN,
    YELLOW,
    RED,
    BLUE,
    PURPLE
} Color;

typedef struct {
    int x;
    int y;
} Position;

typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

// TODO(bkaylor): Having both a symbol state and an animation type is confusing to me.
typedef enum {
    ACTIVE,
    MOVING,
    POPPING,
    POPPED
} Symbol_State;

typedef enum {
    NONE,
    MOVE,
    FAILED_MOVE,
    POP,
    SPAWN
} Animation_Type;

typedef struct {
    Animation_Type animation_type;
    int timer;
    int timer_initial_value;
    int should_be_drawn_on_top;
    int move_distance;
    Direction direction;
} Animation;

typedef struct {
    Symbol_State state;
    Shape shape;
    Color color;
    Position position;
    int marked_for_popping;
    int hovered;
    int selected;
    Animation animation;
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
    Position position;
    Direction direction;
    int length;
} Match_Record;

// TODO(bkaylor): Use bools.
typedef struct {
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
    int pop_all;
    float game_speed_factor;
    Symbol grid[GRID_X][GRID_Y];
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

    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            Symbol symbol = game_state->grid[i][j];

            if (symbol.state == POPPED) { continue; }

            rect.w = game_state->symbol_width;
            rect.h = game_state->symbol_height;

            SDL_Color tile_color;

            // These are nicer colors.
            switch (symbol.color) 
            {
                case GREEN:
                    tile_color = (SDL_Color){64, 97, 69};
                break;
                case YELLOW:
                    tile_color = (SDL_Color){221, 193, 117};
                break;
                case RED:
                    tile_color = (SDL_Color){212, 98, 117};
                break;
                case BLUE:
                    tile_color = (SDL_Color){60, 120, 255};
                break;
                case PURPLE:
                    tile_color = (SDL_Color){180, 98, 191};
                break;
                default:
                break;
            }

            int delta_width = 0, delta_height = 0;

            // Animate popping symbols as shrinking.
            if (symbol.state == POPPING) {
                float percent_through_popping = (float)symbol.animation.timer / (POP_SECONDS * 1000);

                int new_width = rect.w * percent_through_popping;
                int new_height = rect.h * percent_through_popping;

                delta_width = rect.w - new_width; 
                delta_height = rect.h - new_height;

                rect.w = new_width;
                rect.h = new_height;
            }

            SDL_SetRenderDrawColor(renderer, tile_color.r, tile_color.g, tile_color.b, 255);

            // TODO(bkaylor): When animating some swapping tiles, the selected tile in the swap should always be on top.
            // TODO(bkaylor): Animate failed swaps! (Right now just silently fails)
            // TODO(bkaylor): Currently, when something is falling and another tile below it pops, there is some teleporting.
            //
            // Handle animations.
            //

            // The animation system is simple- we calculate an X and Y offset in pixels from the real "position" of the symbol.
            // After popping symbols in a column, we want the column to drop as a unit, all setting into place at the same time.
            // Therefore, when setting the animation data for a column, the timers should be the same, and the move_distance should be the same.
            // The only difference is the position that the symbol starts in the fall.
            // In a column, you want the lower (higher j value) pieces to have a LOWER Y offset than the higher (lower j value) pieces. 
            // For a spawning symbol, the Y offset should be height * (distance * animation_factor)
            int x_adjustment = 0, y_adjustment = 0;
            float percent_through_animation = 0;
            if (symbol.animation.animation_type != NONE) 
            {
                int adjustment_factor_x = 0, adjustment_factor_y = 0;
                percent_through_animation = (float)symbol.animation.timer / (float)(symbol.animation.timer_initial_value);
                int move_distance = symbol.animation.move_distance;

                switch (symbol.animation.animation_type) 
                {
                    case SPAWN:
                        adjustment_factor_x = percent_through_animation * game_state->symbol_width;
                        adjustment_factor_y = percent_through_animation * game_state->symbol_height * (move_distance + 1); 
                    break;
                    case MOVE:
                        adjustment_factor_x = percent_through_animation * game_state->symbol_width;
                        adjustment_factor_y = percent_through_animation * game_state->symbol_height * move_distance; 
                    break;

                    case FAILED_MOVE:
                        adjustment_factor_x = percent_through_animation * (game_state->symbol_width/10);
                        adjustment_factor_y = percent_through_animation * (game_state->symbol_height/10);
                    break;

                    default:
                    break;
                }

                switch (symbol.animation.direction) {
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
            rect.x = i * game_state->symbol_width + game_state->grid_outer_padding + x_adjustment;
            rect.y = j * game_state->symbol_height + game_state->grid_outer_padding + y_adjustment;

            SDL_Rect symbol_rect;
            symbol_rect.x = rect.x + (delta_width/2) + SYMBOL_PADDING;
            symbol_rect.y = rect.y + (delta_height/2) + SYMBOL_PADDING;
            symbol_rect.w = rect.w - SYMBOL_PADDING;
            symbol_rect.h = rect.h - SYMBOL_PADDING;

            // Draw the rect.
            SDL_RenderFillRect(renderer, &symbol_rect);

#ifdef DEBUG 
            // Draw debug data on each square.
            char symbol_string[30];
            char symbol_string_line_2[30];
            sprintf(symbol_string, "%dms/%dms(%.3f)", symbol.animation.timer, symbol.animation.timer_initial_value, percent_through_animation, symbol.animation.animation_type);
            sprintf(symbol_string_line_2, "t%d,s%d", symbol.animation.animation_type, symbol.state);
            draw_text(renderer, symbol_rect.x, symbol_rect.y, symbol_string, font, font_color);
            draw_text(renderer, symbol_rect.x, symbol_rect.y+10, symbol_string_line_2, font, font_color);
#endif

            // TODO(bkaylor): Do something better than the crosshair ...
            // Draw crosshair on selected symbol.
            if (symbol.selected) {
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
    symbol->color = rand() % NUMBER_OF_COLORS; 
    symbol->shape = rand() % 1;
    symbol->position.x = i;
    symbol->position.y = j;
    symbol->marked_for_popping = 0;
    symbol->hovered = 0;
    symbol->selected = 0;
    symbol->state = ACTIVE;
    symbol->animation.animation_type = NONE;
}

void apply_match_to_grid_state(Symbol grid[GRID_X][GRID_Y], Match_Record match_record)
{
    int x_increment = 0, y_increment = 0;
    switch (match_record.direction) {
        case UP:
            x_increment = 0;
            y_increment = 1;
        break;
        case DOWN:
            x_increment = 0;
            y_increment = -1;
        break;
        case LEFT:
            x_increment = -1;
            y_increment = 0;
        break;
        case RIGHT:
            x_increment = 1;
            y_increment = 0;
        break;
        default:
            x_increment = 0;
            y_increment = 0;
        break;
    }

    // Build the list of matched symbols.
    // TODO(bkaylor): What should the size of the matched_symbols_list array actually be? Whatever the maximum match is.
    Symbol *matched_symbols_list[32];
    int i = match_record.position.x;
    int j = match_record.position.y;
    int k = 0;
    while (k < match_record.length) {
        matched_symbols_list[k] = &grid[i][j];

        grid[i][j].marked_for_popping = 1;

        i += x_increment;
        j += y_increment;
        k++;
    }

#ifdef DEBUG
    printf("match of length %d at (%d,%d)\n", match_record.length, match_record.position.x, match_record.position.y);
    for (int k = 0; k < match_record.length; k++)
    {
        printf("(%d,%d) ", matched_symbols_list[k]->position.x, matched_symbols_list[k]->position.y);
    }
    printf("\n");
#endif

}

// TODO(bkaylor): Other match3 games have special effects when you make a long or interesting match.
// TODO(bkaylor): Add notes about the different Bejewled special effects.
Match_Record check_direction_for_match(Symbol grid[GRID_X][GRID_Y], Position starting_position, Direction direction)
{
    Match_Record match = {0};
    Symbol *starting_symbol = &grid[starting_position.x][starting_position.y];
    if (starting_symbol->state != ACTIVE && starting_symbol->animation.animation_type != NONE) return match;

    int match_found = 0;
    int match_length = 1;

    int x_increment = 0, y_increment = 0;
    switch (direction) {
        case UP:
            x_increment = 0;
            y_increment = 1;
        break;
        case DOWN:
            x_increment = 0;
            y_increment = -1;
        break;
        case LEFT:
            x_increment = -1;
            y_increment = 0;
        break;
        case RIGHT:
            x_increment = 1;
            y_increment = 0;
        break;
        default:
            x_increment = 0;
            y_increment = 0;
        break;
    }

    // Find length match in the (x_increment, y_increment) direction.
    {
        int i = starting_position.x + x_increment;
        int j = starting_position.y + y_increment;

        while ((i < GRID_X) && (j < GRID_Y) && (i >= 0) && (j >= 0) && 
               (grid[i][j].state == ACTIVE) &&
               (grid[i][j].animation.animation_type == NONE) &&
               (grid[i][j].color == grid[starting_position.x][starting_position.y].color)) {
            match_length++;

            if (match_length >= 3) {
                match_found = 1;
            }

            i += x_increment;
            j += y_increment;
        }
    }

    match.position = starting_position;
    match.length = match_length;
    match.direction = direction;

    return match;
}

void update(Game_State *game_state, Mouse_State *mouse_state) 
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

    Match_Record matches[GRID_X*GRID_Y/3]; // TODO(bkaylor): What size should this be?
    int match_count = 0;

    // On left click, select the hovered tile.
    // TODO(bkaylor): Try clicking and dragging control scheme instead of left click to select and left click to swap.
    {
        Selection_Info *hovered = game_state->hovered;
        Selection_Info *selected = game_state->selected;
        if (hovered->active && mouse_state->pressed == SDL_BUTTON_LEFT) {
            if (selected->active) {
                if ((hovered->x == selected->x && hovered->y == selected->y-1) || (hovered->x == selected->x && hovered->y == selected->y+1) ||
                    (hovered->x == selected->x-1 && hovered->y == selected->y) || (hovered->x == selected->x+1 && hovered->y == selected->y)) { 
                    //
                    // Check if the move will result in a new match.
                    //
 
                    // Swap the tiles.
                    Symbol temp = game_state->grid[hovered->x][hovered->y];
                    game_state->grid[hovered->x][hovered->y] = game_state->grid[selected->x][selected->y];
                    game_state->grid[selected->x][selected->y] = temp;

                    Position temp_position = game_state->grid[hovered->x][hovered->y].position;
                    game_state->grid[hovered->x][hovered->y].position = game_state->grid[selected->x][selected->y].position; 
                    game_state->grid[selected->x][selected->y].position = temp_position;

                    // Check if there would be a new match in the grid.
                    int match_found = 0;
                    for (int i = 0; i < GRID_X; i++)
                    {
                        for (int j = 0; j < GRID_Y; j++)
                        {
                            if (1) {
                                Position position = {i, j};
                                Match_Record match_up = check_direction_for_match(game_state->grid, position, UP);
                                Match_Record match_right = check_direction_for_match(game_state->grid, position, RIGHT);

                                if (match_up.length >= 3) {
                                    match_found = 1;
                                    break;
                                }
                                if (match_right.length >= 3) {
                                    match_found = 1;
                                    break;
                                }
                            }
                        }
                    }

                    // Setup animation directions.
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

                    if (match_found == 0) {
                        // Failed swap- move tiles back.
                        Symbol temp = game_state->grid[hovered->x][hovered->y];
                        game_state->grid[hovered->x][hovered->y] = game_state->grid[selected->x][selected->y];
                        game_state->grid[selected->x][selected->y] = temp;

                        Position temp_position = game_state->grid[hovered->x][hovered->y].position;
                        game_state->grid[hovered->x][hovered->y].position = game_state->grid[selected->x][selected->y].position; 
                        game_state->grid[selected->x][selected->y].position = temp_position;

                        // Set animation state of a failed move.
                        hovered->symbol->animation.animation_type = FAILED_MOVE;
                        hovered->symbol->animation.direction = move_direction_of_hovered;
                        hovered->symbol->animation.timer = 1000 * SWAP_SECONDS;
                        hovered->symbol->animation.timer_initial_value = hovered->symbol->animation.timer;
                        hovered->symbol->animation.move_distance = 1;

                        selected->symbol->animation.animation_type = FAILED_MOVE;
                        selected->symbol->animation.direction = move_direction_of_selected;
                        selected->symbol->animation.timer = 1000 * SWAP_SECONDS;
                        selected->symbol->animation.timer_initial_value = selected->symbol->animation.timer;
                        selected->symbol->animation.should_be_drawn_on_top = 1;
                        selected->symbol->animation.move_distance = 1;
                    } else {
                        // Swap is valid.
                        // Set animation state of moving tiles.
                        hovered->symbol->state = MOVING;
                        hovered->symbol->animation.animation_type = MOVE;
                        hovered->symbol->animation.direction = move_direction_of_hovered;
                        hovered->symbol->animation.timer = 1000 * SWAP_SECONDS;
                        hovered->symbol->animation.timer_initial_value = hovered->symbol->animation.timer;
                        hovered->symbol->animation.move_distance = 1;

                        selected->symbol->state = MOVING;
                        selected->symbol->animation.animation_type = MOVE;
                        selected->symbol->animation.direction = move_direction_of_selected;
                        selected->symbol->animation.timer = 1000 * SWAP_SECONDS;
                        selected->symbol->animation.timer_initial_value = selected->symbol->animation.timer;
                        selected->symbol->animation.should_be_drawn_on_top = 1;
                        selected->symbol->animation.move_distance = 1;
                    }

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

    // Pop-all hack for testing.
    if (game_state->pop_all) {
        for (int i = 0; i < GRID_X; i++)
        {
            for (int j = 0; j < GRID_Y; j++)
            {
                game_state->grid[i][j].marked_for_popping = 1;
            }
        }

        game_state->pop_all = 0;
    }

    // Check for matches.
    if (game_state->need_to_look_for_matches) {
        for (int i = 0; i < GRID_X; i++)
        {
            for (int j = 0; j < GRID_Y; j++)
            {
                Symbol *symbol = &game_state->grid[i][j];
                if (!symbol->marked_for_popping) {
                    Position position = {i, j};
                    Match_Record match_up = check_direction_for_match(game_state->grid, position, UP);
                    Match_Record match_right = check_direction_for_match(game_state->grid, position, RIGHT);

                    if (match_up.length >= 3) {
                        matches[match_count] = match_up;
                        apply_match_to_grid_state(game_state->grid, match_up);
                        ++match_count;
                    }
                    if (match_right.length >= 3) {
                        matches[match_count] = match_right;
                        apply_match_to_grid_state(game_state->grid, match_right);
                        ++match_count;
                    }
                }
            }
        }

        game_state->need_to_look_for_matches = 0;
    }

#if 0
    // Apply matches.
    if (match_count > 0) {
        for (int k = 0; k < match_count; ++k)
        {
            apply_match_to_grid_state(game_state->grid, matches[k]);
        }
    }
#endif

    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            Symbol *symbol = &game_state->grid[i][j];
            if (symbol->marked_for_popping) {
                symbol->state = POPPING;
                symbol->animation.animation_type = POP;
                symbol->animation.timer = POP_SECONDS * 1000;
                symbol->marked_for_popping = 0;
            }

            if (symbol->state == POPPING) {
                if (symbol->animation.timer <= 0) {
                    symbol->state = POPPED;
                    symbol->animation.animation_type = NONE;
                    game_state->score++;
                }
            }
        }
    }

    // Check how many popped symbols are in each column.
    for (int i = 0; i < GRID_X; i++)
    {
        int popped_count = 0;
        int y_index_of_highest_symbol_popped = -1;

        for (int j = GRID_Y-1; j >= 0; j--)
        {
            if (game_state->grid[i][j].state == POPPED) 
            {
                y_index_of_highest_symbol_popped = j;
                popped_count++;
            }
        }

        if (popped_count > 0 && y_index_of_highest_symbol_popped >= 0)
        {
            // Move and set the animation state of the symbols above the popped region, from bottom to top.
            for (int j = y_index_of_highest_symbol_popped-1; j >= 0; j--)
            {
                int new_y_index = j+popped_count;
                game_state->grid[i][new_y_index] = game_state->grid[i][j]; 
                game_state->grid[i][new_y_index].position = (Position){i, new_y_index};

                game_state->grid[i][new_y_index].state = MOVING;
                game_state->grid[i][new_y_index].animation.animation_type = MOVE;
                game_state->grid[i][new_y_index].animation.direction = UP; // TODO(bkaylor): Why up instead of down?
                game_state->grid[i][new_y_index].animation.timer = 1000 * MOVE_SECONDS * popped_count;
                game_state->grid[i][new_y_index].animation.timer_initial_value = game_state->grid[i][new_y_index].animation.timer; 
                game_state->grid[i][new_y_index].animation.move_distance = popped_count;
            }

            // Spawn new tiles at the top of the column.
            for (int k = 0; k < popped_count; k++)
            {
                int new_y_index = k;
                initialize_symbol(&game_state->grid[i][new_y_index], i, new_y_index);
                game_state->grid[i][new_y_index].state = MOVING;
                game_state->grid[i][new_y_index].animation.animation_type = SPAWN;
                game_state->grid[i][new_y_index].animation.direction = UP; // TODO(bkaylor): Why up instead of down?
                game_state->grid[i][new_y_index].animation.timer = 1000 * MOVE_SECONDS * popped_count;
                game_state->grid[i][new_y_index].animation.timer_initial_value = game_state->grid[i][new_y_index].animation.timer; 
                game_state->grid[i][new_y_index].animation.move_distance = popped_count;

            }

        }

        if (popped_count > 0) game_state->need_to_look_for_matches = 1;
    }

    // Cleanup animation state.
    for (int i = 0; i < GRID_X; i++)
    {
        for (int j = 0; j < GRID_Y; j++)
        {
            Symbol *symbol = &game_state->grid[i][j];
            if (symbol->animation.animation_type == MOVE && symbol->animation.timer <= 0) {
                symbol->animation.animation_type = NONE;
                symbol->state = ACTIVE;

                // State has changed- look for matches again.
                game_state->need_to_look_for_matches = 1;
            }

            if (symbol->animation.animation_type == SPAWN && symbol->animation.timer <= 0) {
                symbol->animation.animation_type = NONE;
                symbol->state = ACTIVE;

                // State has changed- look for matches again.
                game_state->need_to_look_for_matches = 1;
            }

            if (symbol->animation.animation_type == FAILED_MOVE && symbol->animation.timer <= 0) {
                symbol->animation.animation_type = NONE;
            }
        }
    }

    // Reset the board if the timer is up.
    if (game_state->timer <= 0) {
        game_state->reset = 1;
    }
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

                    case SDLK_g:
                        game_state->game_speed_factor *= 2.0f ;
                        break;

                    case SDLK_l:
                        game_state->game_speed_factor /= 2.0f ;
                        break;

                    case SDLK_p:
                        game_state->pop_all = 1;

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

    //
    // Setup main loop
    //
    srand(time(NULL));

    // Build game state
    Game_State game_state = {0};
    Selection_Info hovered = {0};
    Selection_Info selected = {0};
    game_state.hovered = &hovered;
    game_state.selected = &selected;
    game_state.reset = 1;
    game_state.game_speed_factor = 1.0f;

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

            frame_time_finish = SDL_GetTicks();
            delta_t = frame_time_finish - frame_time_start;

#ifdef CAP_FRAMERATE
            // Cap at 60ish
            int delta_t_difference = 16 - delta_t; 
            if (delta_t_difference > 0) {
                SDL_Delay(delta_t_difference);
                delta_t += delta_t_difference;
            }
#endif 

            // Update timers.
            delta_t *= game_state.game_speed_factor;
            game_state.timer -= delta_t; 

            for (int i = 0; i < GRID_X; i++)
            {
                for (int j = 0; j < GRID_Y; j++)
                {
                    if (game_state.grid[i][j].animation.animation_type != NONE) {
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
