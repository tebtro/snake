#include <windows.h>
#include <stdio.h>

#include "iml_general.h"
#include "iml_types.h"


#define SCALE_FACTOR 1
#define GAME_GRID_SIZE       (32 * SCALE_FACTOR)
#define PIXELS_PER_GRID_CELL (22 * SCALE_FACTOR)
#define WIDTH  (GAME_GRID_SIZE * PIXELS_PER_GRID_CELL)
#define HEIGHT (GAME_GRID_SIZE * PIXELS_PER_GRID_CELL)


struct Win32_Offscreen_Buffer {
    BITMAPINFO info;
    void *memory;
    int width;
    int height;
    int pitch;
};

struct Win32_Window_Dimension {
    int width;
    int height;
};

enum struct Game_Mode {
    CLASSIC,
    NO_WALLS
};

enum struct Direction {
    UP = 1,
    RIGHT = 2,
    DOWN = 3,
    LEFT = 4
};

struct Vector2 {
    int x;
    int y;
};

struct Game_State {
    enum Game_Mode game_mode;
    int snake_speed;
    Vector2 snake_pos;
    Vector2 fruit_pos;
    int fruits_eaten;
    enum Direction input_direction;
    enum Direction snake_direction;
    f32 snake_pixels_pos_x;
    f32 snake_pixels_pos_y;
    int grid[GAME_GRID_SIZE][GAME_GRID_SIZE];
};

global Game_State *game_state;

global Win32_Offscreen_Buffer global_backbuffer;
global b32 global_running;


void
reset_game_state() {
    game_state = new Game_State();
    game_state->game_mode = Game_Mode::NO_WALLS;
    game_state->snake_speed = 6;
    game_state->snake_pos.x = 16;
    game_state->snake_pos.y = 16;
    game_state->grid[game_state->snake_pos.y][game_state->snake_pos.x] = 1;
    game_state->fruit_pos.x = -1;
    game_state->fruit_pos.y = -1;
    game_state->fruits_eaten = 1;
    game_state->snake_pixels_pos_x = 16 * PIXELS_PER_GRID_CELL;
    game_state->snake_pixels_pos_y = 16 * PIXELS_PER_GRID_CELL;
    game_state->snake_direction = Direction::RIGHT;
    game_state->snake_direction = (Direction)0;
}

internal void
win32_resize_dib_section(Win32_Offscreen_Buffer *buffer, int width, int height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }
    
    buffer->width  = width;
    buffer->height = height;
    
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;
    
    int bytes_per_pixel = 4;
    int bitmap_memory_size = (buffer->width * buffer->height) * bytes_per_pixel;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    buffer->pitch = buffer->width * bytes_per_pixel;
}

internal Win32_Window_Dimension
win32_get_window_dimension(HWND window) {
    Win32_Window_Dimension result;
    
    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;
    
    return result;
}

internal void
win32_display_buffer_in_window(Win32_Offscreen_Buffer *buffer, HDC device_context, int window_width, int window_height) {
    StretchDIBits(device_context,
                  0, 0, window_width, window_height,
                  0, 0, buffer->width, buffer->height,
                  buffer->memory, &buffer->info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
win32_main_window_callback(HWND window,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam) {
    LRESULT result = 0;
    
    switch (message) {
        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        
        case WM_DESTROY:
        case WM_CLOSE: {
            global_running = false;
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            u32 vk_code  = wparam;
            b32 was_down = ((lparam & (1 << 30)) != 0);
            b32 is_down  = ((lparam & (1 << 31)) == 0);
            
            if (is_down) {
                if (vk_code == 'W') {
                    if (game_state->snake_direction != Direction::DOWN)  game_state->input_direction = Direction::UP;
                }
                else if (vk_code == 'A') {
                    if (game_state->snake_direction != Direction::RIGHT)  game_state->input_direction = Direction::LEFT;
                }
                else if (vk_code == 'S') {
                    if (game_state->snake_direction != Direction::UP)  game_state->input_direction = Direction::DOWN;
                }
                else if (vk_code == 'D') {
                    if (game_state->snake_direction != Direction::LEFT)  game_state->input_direction = Direction::RIGHT;
                }
                else if (vk_code == VK_ESCAPE) {
                    global_running = false;
                }
            }
            
            b32 alt_key_was_down = (lparam & (1 << 29));
            if ((vk_code == VK_F4) && alt_key_was_down) {
                global_running = false;
            }
        } break;
        
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width  = paint.rcPaint.right  - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            Win32_Window_Dimension dimension = win32_get_window_dimension(window);
            win32_display_buffer_in_window(&global_backbuffer, device_context, dimension.width, dimension.height);
            EndPaint(window, &paint);
        } break;
        
        default: {
            result = DefWindowProcA(window, message, wparam, lparam);
        } break;
    }
    
    return result;
}

internal void
win32_render_game(Win32_Offscreen_Buffer *buffer, Game_State *game_state) {
    u8 *row = (u8 *)buffer->memory;
    for (int y = 0; y < buffer->height; ++y) {
        u32 *pixel = (u32 *)row;
        for (int x = 0; x < buffer->width; ++x) {
            int grid_x = x / PIXELS_PER_GRID_CELL;
            int grid_y  = y / PIXELS_PER_GRID_CELL;
            int grid_value = game_state->grid[grid_y][grid_x];
            
            u8 red = 0;
            u8 blue = 0;
            u8 green = 0;
            
            if (grid_value == -1)  {
                red = 255;
                blue = 0;
                green = 0;
            }
            if (grid_value > 0)  {
                red = 255;
                blue = 255;
                green = 255;
            }
            
            *pixel++ = ((red << 16) | ((green << 8) | blue));
        }
        
        row += buffer->pitch;
    }
}

int CALLBACK
WinMain(HINSTANCE instance,
        HINSTANCE prev_instance,
        LPSTR     cmd_line,
        int       show_cmd) {
    reset_game_state();
    
    win32_resize_dib_section(&global_backbuffer, WIDTH, HEIGHT);
    
    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    window_class.lpfnWndProc = win32_main_window_callback;
    window_class.hInstance = instance;
    window_class.lpszClassName = "SnakeWindowClass";
    
    if (!RegisterClassA(&window_class))  {
        // @todo
    }
    HWND window = CreateWindowExA(0, window_class.lpszClassName,
                                  "Snake",
                                  WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  WIDTH, HEIGHT,
                                  0, 0, instance, 0);
    if (!window)  {
        // @todo
    }
    
    HDC device_context = GetDC(window);
    
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    s64 perf_count_frequency = perf_count_frequency_result.QuadPart;
    
    global_running = true;
    
    LARGE_INTEGER last_counter;
    QueryPerformanceCounter(&last_counter);
    u64 last_cycle_count = __rdtsc();
    while (global_running) {
        MSG message;
        while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                global_running = false;
            }
            
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        
        //
        // simulate
        //
        game_state->snake_direction = game_state->input_direction;
        
        if (game_state->snake_direction == Direction::UP) {
            game_state->snake_pos.y--;
        }
        if (game_state->snake_direction == Direction::RIGHT) {
            game_state->snake_pos.x++;
        }
        if (game_state->snake_direction == Direction::DOWN) {
            game_state->snake_pos.y++;
        }
        if (game_state->snake_direction == Direction::LEFT) {
            game_state->snake_pos.x--;
        }
        
        // boundscheck
        if (game_state->snake_pos.x > GAME_GRID_SIZE - 1) {
            if (game_state->game_mode == Game_Mode::NO_WALLS) game_state->snake_pos.x = 0;
            if (game_state->game_mode == Game_Mode::CLASSIC)  reset_game_state();
        }
        if (game_state->snake_pos.x < 0) {
            if (game_state->game_mode == Game_Mode::NO_WALLS) game_state->snake_pos.x = GAME_GRID_SIZE - 1;
            if (game_state->game_mode == Game_Mode::CLASSIC)  reset_game_state();
        }
        if (game_state->snake_pos.y > GAME_GRID_SIZE - 1) {
            if (game_state->game_mode == Game_Mode::NO_WALLS) game_state->snake_pos.y = 0;
            if (game_state->game_mode == Game_Mode::CLASSIC)  reset_game_state();
        }
        if (game_state->snake_pos.y < 0) {
            if (game_state->game_mode == Game_Mode::NO_WALLS) game_state->snake_pos.y = GAME_GRID_SIZE - 1;
            if (game_state->game_mode == Game_Mode::CLASSIC)  reset_game_state();
        }
        
        // check next player pos
        // @note Don't reset if the player is standing still (after a reset);
        if ((game_state->snake_direction != (Direction)0) && (game_state->grid[game_state->snake_pos.y][game_state->snake_pos.x] > 0))  {
            reset_game_state();
        }
        
        // move player
        game_state->grid[game_state->snake_pos.y][game_state->snake_pos.x] = game_state->fruits_eaten + 1;
        
        // check if fruit eaten
        if (game_state->snake_pos.x == game_state->fruit_pos.x && game_state->snake_pos.y == game_state->fruit_pos.y) {
            ++game_state->fruits_eaten;
            for (int y = 0; y < GAME_GRID_SIZE; ++y) {
                for (int x = 0; x < GAME_GRID_SIZE; ++x) {
                    if (game_state->grid[y][x] > 0)  {
                        ++game_state->grid[y][x];
                    }
                }
            }
            
            game_state->fruit_pos.x = -1;
            game_state->fruit_pos.y = -1;
        }
        
        // respawn fruit
        if (game_state->fruit_pos.x == -1 && game_state->fruit_pos.y == -1) {
            int x;
            int y;
            while (1) {
                x = rand() % GAME_GRID_SIZE;
                y = rand() % GAME_GRID_SIZE;
                if (game_state->grid[y][x] == 0) break;
            }
            game_state->fruit_pos.x = x;
            game_state->fruit_pos.y = y;
        }
        
        game_state->grid[game_state->fruit_pos.y][game_state->fruit_pos.x] = -1;
        
        //  update game grid
        for (int y = 0; y < GAME_GRID_SIZE; ++y) {
            for (int x = 0; x < GAME_GRID_SIZE; ++x) {
                if (game_state->grid[y][x] > 0)  {
                    --game_state->grid[y][x];
                }
            }
        }
        
        //
        // render
        //
        win32_render_game(&global_backbuffer, game_state);
        
        Win32_Window_Dimension dimension = win32_get_window_dimension(window);
        win32_display_buffer_in_window(&global_backbuffer, device_context, dimension.width, dimension.height);
        
        //
        // sleep
        //
        DWORD sleep_ms = 100;
        Sleep(sleep_ms);
        
        
        u64 end_cycle_count = __rdtsc();
        LARGE_INTEGER end_counter;
        QueryPerformanceCounter(&end_counter);
        
        u64 cycles_elapsed = end_cycle_count - last_cycle_count;
        s64 counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
        f64 ms_per_frame = (f64)((1000.0f * (f64)counter_elapsed) / (f64)perf_count_frequency);
        f64 fps = (f64)perf_count_frequency / (f64)counter_elapsed;
        f64 mcpf = (f64)cycles_elapsed / (1000.0f * 1000.0f);
        
        char string_buffer[256];
        sprintf(string_buffer, "%.02fms/f, %.02fFPS, %.02fmc/f\n", ms_per_frame, fps, mcpf);
        OutputDebugStringA(string_buffer);
        
        last_counter = end_counter;
        last_cycle_count = end_cycle_count;
    }
}