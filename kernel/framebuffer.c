#include "include/framebuffer.h"

typedef struct {
    u8 *pixels;
    u8 *display_pixels;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bytes_per_pixel;
    u8 red_position;
    u8 red_size;
    u8 green_position;
    u8 green_size;
    u8 blue_position;
    u8 blue_size;
} framebuffer_t;

typedef struct {
    u8 app;
    u8 open;
    u8 minimized;
    u32 x;
    u32 y;
    u32 width;
    u32 height;
} app_window_t;

#define MAX_APP_WINDOWS 6
#define WINDOW_TITLE_HEIGHT 28
#define TASKBAR_HEIGHT 36
#define BACKBUFFER_CAPACITY (1280u * 1024u * 4u)

static framebuffer_t framebuffer;
static u8 framebuffer_backbuffer[BACKBUFFER_CAPACITY];
static int file_menu_open;
static int power_menu_open;
static u8 requested_power_action;
static app_window_t app_windows[MAX_APP_WINDOWS];
static i32 focused_window = -1;
static i32 dragged_window = -1;
static i32 drag_offset_x;
static i32 drag_offset_y;
static char note_text[256];
static u32 note_length;
static i32 calculator_value;
static i32 calculator_left;
static char calculator_operator;
static int calculator_entering;
static int calculator_error;
static int cursor_visible;
static i32 cursor_x_saved;
static i32 cursor_y_saved;
static u8 cursor_saved_pixels[7 * 7 * 4];
static int clip_enabled;
static u32 clip_left;
static u32 clip_top;
static u32 clip_right;
static u32 clip_bottom;

static const u8 cursor_shape[7] = {
    0x1C, 0x3E, 0x7F, 0x7F, 0x7F, 0x3E, 0x1C
};

enum {
    APP_DESKTOP,
    APP_FILES,
    APP_NOTES,
    APP_ABOUT,
    APP_CALCULATOR,
    APP_TERMINAL
};

#define DESKTOP_RED 24
#define DESKTOP_GREEN 34
#define DESKTOP_BLUE 43

static void draw_app_window(u32 index);
static void draw_app_panel(u8 app, u32 x, u32 y, u32 width, u32 height);

static void fill_rect(u32 x, u32 y, u32 width, u32 height,
                      u8 red, u8 green, u8 blue);

static void flush_region(u32 x, u32 y, u32 width, u32 height)
{
    if (framebuffer.pixels == framebuffer.display_pixels || width == 0 || height == 0) {
        return;
    }

    u32 right = x + width;
    u32 bottom = y + height;
    if (right < x || right > framebuffer.width) right = framebuffer.width;
    if (bottom < y || bottom > framebuffer.height) bottom = framebuffer.height;
    if (clip_enabled) {
        if (x < clip_left) x = clip_left;
        if (y < clip_top) y = clip_top;
        if (right > clip_right) right = clip_right;
        if (bottom > clip_bottom) bottom = clip_bottom;
    }
    if (x >= right || y >= bottom) {
        return;
    }

    u32 bytes = (right - x) * framebuffer.bytes_per_pixel;
    for (u32 row = y; row < bottom; ++row) {
        const u8 *source = framebuffer.pixels + row * framebuffer.pitch +
                           x * framebuffer.bytes_per_pixel;
        u8 *destination = framebuffer.display_pixels + row * framebuffer.pitch +
                          x * framebuffer.bytes_per_pixel;
        for (u32 column = 0; column < bytes; ++column) {
            destination[column] = source[column];
        }
    }
}

static void flush_cursor_region(i32 x, i32 y)
{
    i32 right = x + 7;
    i32 bottom = y + 7;
    if (right <= 0 || bottom <= 0 || x >= (i32)framebuffer.width ||
        y >= (i32)framebuffer.height) {
        return;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (right > (i32)framebuffer.width) right = (i32)framebuffer.width;
    if (bottom > (i32)framebuffer.height) bottom = (i32)framebuffer.height;
    flush_region((u32)x, (u32)y, (u32)(right - x), (u32)(bottom - y));
}

static const u8 *glyph_rows(char character)
{
    static const u8 glyphs[36][7] = {
        {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
        {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
        {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
        {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17},
        {14, 4, 4, 4, 4, 4, 14}, {7, 2, 2, 2, 18, 18, 12},
        {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
        {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
        {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
        {15, 16, 16, 14, 1, 1, 30}, {31, 4, 4, 4, 4, 4, 4},
        {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
        {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4}, {31, 1, 2, 4, 8, 16, 31},
        {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
        {14, 17, 1, 2, 4, 8, 31}, {30, 1, 1, 14, 1, 1, 30},
        {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
        {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}
    };
    static const u8 space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const u8 plus[7] = {0, 4, 4, 31, 4, 4, 0};
    static const u8 minus[7] = {0, 0, 0, 31, 0, 0, 0};
    static const u8 multiply[7] = {0, 17, 10, 4, 10, 17, 0};
    static const u8 divide[7] = {1, 2, 4, 8, 16, 0, 4};
    static const u8 equals[7] = {0, 0, 31, 0, 31, 0, 0};
    static const u8 period[7] = {0, 0, 0, 0, 0, 12, 12};

    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character == ' ') {
        return space;
    }
    if (character >= '0' && character <= '9') {
        return glyphs[26 + character - '0'];
    }
    if (character == '+') {
        return plus;
    }
    if (character == '-') {
        return minus;
    }
    if (character == '*') {
        return multiply;
    }
    if (character == '/') {
        return divide;
    }
    if (character == '=') {
        return equals;
    }
    if (character == '.') {
        return period;
    }
    if (character < 'A' || character > 'Z') {
        return space;
    }
    return glyphs[character - 'A'];
}

static void draw_text(u32 x, u32 y, const char *text, u8 red, u8 green, u8 blue)
{
    for (; *text != '\0'; ++text, x += 12) {
        const u8 *rows = glyph_rows(*text);
        for (u32 row = 0; row < 7; ++row) {
            for (u32 column = 0; column < 5; ++column) {
                if ((rows[row] & (1u << (4 - column))) != 0) {
                    fill_rect(x + column * 2, y + row * 2, 2, 2, red, green, blue);
                }
            }
        }
    }
}

static u32 append_number(char *text, u32 offset, u32 value)
{
    char digits[10];
    u32 count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0 && count < sizeof(digits));

    while (count != 0) {
        text[offset++] = digits[--count];
    }
    return offset;
}

static u32 channel_value(u8 value, u8 size, u8 position)
{
    u32 maximum = (1u << size) - 1u;
    return (((u32)value * maximum) / 255u) << position;
}

static u32 pack_color(u8 red, u8 green, u8 blue)
{
    return channel_value(red, framebuffer.red_size, framebuffer.red_position) |
           channel_value(green, framebuffer.green_size, framebuffer.green_position) |
           channel_value(blue, framebuffer.blue_size, framebuffer.blue_position);
}

static void put_pixel(u32 x, u32 y, u8 red, u8 green, u8 blue)
{
    if (x >= framebuffer.width || y >= framebuffer.height) {
        return;
    }

    u8 *pixel = framebuffer.pixels + y * framebuffer.pitch + x * framebuffer.bytes_per_pixel;
    u32 color = pack_color(red, green, blue);
    for (u8 byte = 0; byte < framebuffer.bytes_per_pixel; ++byte) {
        pixel[byte] = (u8)(color >> (byte * 8));
    }
}

static void fill_rect(u32 x, u32 y, u32 width, u32 height,
                      u8 red, u8 green, u8 blue)
{
    u32 right = x + width;
    u32 bottom = y + height;
    if (right < x || right > framebuffer.width) {
        right = framebuffer.width;
    }
    if (bottom < y || bottom > framebuffer.height) {
        bottom = framebuffer.height;
    }

    if (clip_enabled) {
        if (x < clip_left) x = clip_left;
        if (y < clip_top) y = clip_top;
        if (right > clip_right) right = clip_right;
        if (bottom > clip_bottom) bottom = clip_bottom;
    }
    if (x >= right || y >= bottom) {
        return;
    }

    for (u32 row = y; row < bottom; ++row) {
        for (u32 column = x; column < right; ++column) {
            put_pixel(column, row, red, green, blue);
        }
    }
}

static void redraw_desktop_region(u32 x, u32 y, u32 width, u32 height)
{
    if (width == 0 || height == 0 || x >= framebuffer.width || y >= framebuffer.height) {
        return;
    }

    clip_left = x;
    clip_top = y;
    clip_right = x + width;
    clip_bottom = y + height;
    if (clip_right < x || clip_right > framebuffer.width) clip_right = framebuffer.width;
    if (clip_bottom < y || clip_bottom > framebuffer.height) clip_bottom = framebuffer.height;
    clip_enabled = 1;
    framebuffer_draw_desktop();
    clip_enabled = 0;
    flush_region(x, y, width, height);
}

static void redraw_window_region(u32 index)
{
    if (index >= MAX_APP_WINDOWS) {
        return;
    }

    app_window_t *window = &app_windows[index];
    u32 x = window->x > 6 ? window->x - 6 : 0;
    u32 y = window->y > 6 ? window->y - 6 : 0;
    redraw_desktop_region(x, y, window->width + 12, window->height + 12);
}

static void redraw_window_bounds(u32 x, u32 y, u32 width, u32 height)
{
    u32 left = x > 6 ? x - 6 : 0;
    u32 top = y > 6 ? y - 6 : 0;
    redraw_desktop_region(left, top, width + 12, height + 12);
}

static void redraw_taskbar(void)
{
    redraw_desktop_region(0, framebuffer.height - TASKBAR_HEIGHT,
                          framebuffer.width, TASKBAR_HEIGHT);
}

static void redraw_app_drawer(void)
{
    u32 x = framebuffer.width > 520 ? (framebuffer.width - 480) / 2 : 16;
    redraw_desktop_region(x - (x >= 4 ? 4 : 0), 64, 480, 392);
}

int framebuffer_initialize(u64 multiboot_info_address)
{
    const u32 *info = (const u32 *)(uptr)multiboot_info_address;
    u32 total_size = info[0];
    if (total_size < 16) {
        return 0;
    }

    u32 offset = 8;
    while (offset + 8 <= total_size) {
        const u32 *tag = (const u32 *)(uptr)(multiboot_info_address + offset);
        u32 type = tag[0];
        u32 size = tag[1];
        if (size < 8 || size > total_size - offset) {
            return 0;
        }

        if (type == 8 && size >= 32) {
            u64 address = (u64)tag[2] | ((u64)tag[3] << 32);
            u32 pitch = tag[4];
            u32 width = tag[5];
            u32 height = tag[6];
            u8 bits_per_pixel = ((const u8 *)tag)[28];
            u8 framebuffer_type = ((const u8 *)tag)[29];
            if (address > UINT32_MAX || width == 0 || height == 0 ||
                (bits_per_pixel != 24 && bits_per_pixel != 32) ||
                framebuffer_type != 1 || size < 38) {
                return 0;
            }

            framebuffer.display_pixels = (u8 *)(uptr)address;
            framebuffer.pixels = framebuffer.display_pixels;
            framebuffer.pitch = pitch;
            framebuffer.width = width;
            framebuffer.height = height;
            framebuffer.bytes_per_pixel = bits_per_pixel / 8;
            framebuffer.red_position = ((const u8 *)tag)[32];
            framebuffer.red_size = ((const u8 *)tag)[33];
            framebuffer.green_position = ((const u8 *)tag)[34];
            framebuffer.green_size = ((const u8 *)tag)[35];
            framebuffer.blue_position = ((const u8 *)tag)[36];
            framebuffer.blue_size = ((const u8 *)tag)[37];
            if (pitch < width * framebuffer.bytes_per_pixel ||
                framebuffer.red_size == 0 || framebuffer.green_size == 0 ||
                framebuffer.blue_size == 0) {
                return 0;
            }
            if ((u64)pitch * height <= sizeof(framebuffer_backbuffer)) {
                framebuffer.pixels = framebuffer_backbuffer;
            }
            return 1;
        }

        if (type == 0) {
            break;
        }
        offset = (offset + size + 7u) & ~7u;
    }

    return 0;
}

u32 framebuffer_width(void)
{
    return framebuffer.width;
}

u32 framebuffer_height(void)
{
    return framebuffer.height;
}

void framebuffer_draw_boot_screen(void)
{
    fill_rect(0, 0, framebuffer.width, framebuffer.height, 18, 27, 35);

    u32 panel_width = framebuffer.width > 480 ? 420 : framebuffer.width - 32;
    u32 panel_height = 230;
    u32 panel_x = (framebuffer.width - panel_width) / 2;
    u32 panel_y = (framebuffer.height - panel_height) / 2;
    fill_rect(panel_x + 5, panel_y + 6, panel_width, panel_height, 9, 15, 20);
    fill_rect(panel_x, panel_y, panel_width, panel_height, 33, 47, 57);
    fill_rect(panel_x, panel_y, panel_width, 4, 49, 175, 130);
    fill_rect(panel_x + 32, panel_y + 34, 52, 52, 49, 175, 130);
    fill_rect(panel_x + 45, panel_y + 47, 26, 26, 230, 238, 240);
    draw_text(panel_x + 102, panel_y + 42, "NOVA OS", 230, 238, 240);
    draw_text(panel_x + 34, panel_y + 112, "STARTING SYSTEM", 220, 230, 232);
    draw_text(panel_x + 34, panel_y + 144, "LOADING DESKTOP", 157, 174, 181);
    draw_text(panel_x + 34, panel_y + 168, "VERSION " NOVA_VERSION, 157, 174, 181);
    fill_rect(panel_x + 34, panel_y + 184, panel_width - 68, 12, 17, 25, 33);
    flush_region(0, 0, framebuffer.width, framebuffer.height);
}

void framebuffer_draw_boot_progress(u32 step)
{
    u32 panel_width = framebuffer.width > 480 ? 420 : framebuffer.width - 32;
    u32 panel_x = (framebuffer.width - panel_width) / 2;
    u32 panel_y = (framebuffer.height - 230) / 2;
    u32 progress_width = panel_width - 72;
    u32 filled = (progress_width * step) / 20;
    fill_rect(panel_x + 36, panel_y + 186, progress_width, 8, 17, 25, 33);
    fill_rect(panel_x + 36, panel_y + 186, filled, 8, 49, 175, 130);
    flush_region(panel_x + 34, panel_y + 184, panel_width - 68, 12);
}

static void draw_top_bar(void)
{
    fill_rect(0, 0, framebuffer.width, 40, 17, 25, 33);
    fill_rect(16, 11, 18, 18, 49, 175, 130);
    fill_rect(21, 16, 8, 8, 230, 238, 240);
    draw_text(44, 16, "NOVA OS", 220, 230, 232);
    fill_rect(148, 4, 96, 32, file_menu_open ? 49 : 31,
             file_menu_open ? 175 : 44, file_menu_open ? 130 : 55);
    draw_text(164, 13, "APPS", 245, 247, 250);
    u32 power_x = framebuffer.width > 140 ? framebuffer.width - 128 : 252;
    fill_rect(power_x, 4, 112, 32, power_menu_open ? 49 : 31,
              power_menu_open ? 175 : 44, power_menu_open ? 130 : 55);
    draw_text(power_x + 14, 13, "POWER", 245, 247, 250);
}

static u32 power_menu_x(void)
{
    return framebuffer.width > 200 ? framebuffer.width - 192 : 8;
}

static void draw_power_menu(void)
{
    u32 x = power_menu_x();
    fill_rect(x + 4, 45, 180, 90, 10, 16, 22);
    fill_rect(x, 40, 180, 90, 33, 47, 57);
    draw_text(x + 14, 50, "RESTART", 220, 230, 232);
    fill_rect(x + 12, 78, 154, 1, 70, 87, 96);
    draw_text(x + 14, 91, "SHUTDOWN", 220, 230, 232);
}

static const char *app_name(u8 app)
{
    switch (app) {
    case APP_FILES: return "FILES";
    case APP_NOTES: return "NOTES";
    case APP_ABOUT: return "ABOUT";
    case APP_CALCULATOR: return "CALCULATOR";
    case APP_TERMINAL: return "TERMINAL";
    default: return "DESKTOP";
    }
}

static int find_app_window(u8 app)
{
    for (u32 index = 0; index < MAX_APP_WINDOWS; ++index) {
        if (app_windows[index].open && app_windows[index].app == app) {
            return (int)index;
        }
    }
    return -1;
}

static void launch_app(u8 app)
{
    if (app == APP_DESKTOP) {
        file_menu_open = 0;
        redraw_desktop_region(0, 0, framebuffer.width, 40);
        redraw_app_drawer();
        return;
    }

    i32 old_focus = focused_window;
    int drawer_was_open = file_menu_open;
    int power_was_open = power_menu_open;

    int index = find_app_window(app);
    if (index < 0) {
        for (u32 candidate = 0; candidate < MAX_APP_WINDOWS; ++candidate) {
            if (!app_windows[candidate].open) {
                index = (int)candidate;
                break;
            }
        }
    }
    if (index < 0) {
        return;
    }

    app_window_t *window = &app_windows[index];
    if (!window->open) {
        u32 available_width = framebuffer.width > 48 ? framebuffer.width - 48 : framebuffer.width;
        u32 available_height = framebuffer.height > 112 ? framebuffer.height - 112 : framebuffer.height;
        window->width = available_width > 440 ? 440 : available_width;
        window->height = available_height > 270 ? 270 : available_height;
        u32 x_room = framebuffer.width > window->width + 24
            ? framebuffer.width - window->width - 24 : 0;
        u32 y_room = framebuffer.height > window->height + TASKBAR_HEIGHT + 48
            ? framebuffer.height - window->height - TASKBAR_HEIGHT - 48 : 0;
        window->x = x_room == 0 ? 8 : 12 + ((u32)index * 36) % x_room;
        window->y = y_room == 0 ? 44 : 44 + ((u32)index * 30) % y_room;
        window->app = app;
        window->open = 1;
    }
    window->minimized = 0;
    focused_window = index;
    file_menu_open = 0;
    power_menu_open = 0;
    if (drawer_was_open) {
        redraw_desktop_region(0, 0, framebuffer.width, 40);
        redraw_app_drawer();
    }
    if (power_was_open) {
        u32 menu_x = power_menu_x();
        redraw_desktop_region(menu_x > 4 ? menu_x - 4 : 0, 0, 196, 140);
    }
    if (old_focus >= 0 && old_focus != index) {
        redraw_window_region((u32)old_focus);
    }
    redraw_window_region((u32)index);
    redraw_taskbar();
}

static u32 append_signed(char *text, u32 offset, i32 value)
{
    u32 magnitude;
    if (value < 0) {
        text[offset++] = '-';
        magnitude = (u32)(-(i64)value);
    } else {
        magnitude = (u32)value;
    }
    return append_number(text, offset, magnitude);
}

static void draw_calculator(u32 panel_x, u32 panel_y, u32 panel_width)
{
    static const char *labels[4][4] = {
        {"7", "8", "9", "/"},
        {"4", "5", "6", "*"},
        {"1", "2", "3", "-"},
        {"CLR", "0", "=", "+"}
    };

    draw_text(panel_x + 22, panel_y + 26, "CALCULATOR", 220, 230, 232);
    fill_rect(panel_x + 22, panel_y + 54, panel_width - 44, 36, 17, 25, 33);

    char display[32];
    u32 length = 0;
    if (calculator_error) {
        display[0] = 'E'; display[1] = 'R'; display[2] = 'R';
        display[3] = 'O'; display[4] = 'R'; display[5] = '\0';
    } else {
        if (calculator_operator != 0) {
            length = append_signed(display, length, calculator_left);
            display[length++] = ' ';
            display[length++] = calculator_operator;
            display[length++] = ' ';
        }
        length = append_signed(display, length, calculator_value);
        display[length] = '\0';
    }
    draw_text(panel_x + 34, panel_y + 68, display, 230, 238, 240);

    u32 button_width = (panel_width - 44) / 4;
    for (u32 row = 0; row < 4; ++row) {
        for (u32 column = 0; column < 4; ++column) {
            u32 button_x = panel_x + 22 + column * button_width;
            u32 button_y = panel_y + 98 + row * 28;
            int is_operator = column == 3 ||
                              (row == 3 && (column == 0 || column == 2));
            fill_rect(button_x + 2, button_y, button_width - 6, 24,
                      is_operator ? 49 : 24,
                      is_operator ? 96 : 34,
                      is_operator ? 85 : 43);
            draw_text(button_x + 14, button_y + 7, labels[row][column],
                      240, 244, 246);
        }
    }
}

static void calculator_input(char character)
{
    if (character >= '0' && character <= '9') {
        if (calculator_error) {
            calculator_value = 0;
            calculator_left = 0;
            calculator_operator = 0;
            calculator_entering = 0;
            calculator_error = 0;
        }
        u32 digit = (u32)(character - '0');
        if (!calculator_entering) {
            calculator_value = 0;
            calculator_entering = 1;
        }
        if (calculator_value > 214748364 ||
            (calculator_value == 214748364 && digit > 7)) {
            calculator_error = 1;
        } else {
            calculator_value = calculator_value * 10 + (i32)digit;
        }
    } else if (character == 'c' || character == 'C' || character == '\b') {
        calculator_value = 0;
        calculator_left = 0;
        calculator_operator = 0;
        calculator_entering = 0;
        calculator_error = 0;
    } else if (character == '=' && calculator_operator != 0 && !calculator_error) {
        i64 result;
        if (calculator_operator == '+') {
            result = (i64)calculator_left + calculator_value;
        } else if (calculator_operator == '-') {
            result = (i64)calculator_left - calculator_value;
        } else if (calculator_operator == '*') {
            result = (i64)calculator_left * calculator_value;
        } else {
            if (calculator_value == 0) {
                calculator_error = 1;
                result = 0;
            } else {
                result = calculator_left / calculator_value;
            }
        }
        if (result > 2147483647LL || result < -2147483647LL - 1) {
            calculator_error = 1;
        } else if (!calculator_error) {
            calculator_value = (i32)result;
        }
        calculator_operator = 0;
        calculator_entering = 0;
    } else if ((character == '+' || character == '-' || character == '*' || character == '/') &&
               !calculator_error) {
        if (calculator_operator != 0 && calculator_entering) {
            calculator_left = calculator_value;
        } else {
            calculator_left = calculator_value;
        }
        calculator_operator = character;
        calculator_entering = 0;
    }
}

void framebuffer_calculator_input(char character)
{
    if (focused_window < 0 || !app_windows[focused_window].open ||
        app_windows[focused_window].app != APP_CALCULATOR) {
        return;
    }
    calculator_input(character);
    draw_app_window((u32)focused_window);
    flush_region(app_windows[focused_window].x, app_windows[focused_window].y,
                 app_windows[focused_window].width + 6,
                 app_windows[focused_window].height + 6);
}

static void draw_note_text(u32 panel_x, u32 panel_y, u32 panel_width, u32 panel_height)
{
    u32 editor_x = panel_x + 22;
    u32 editor_y = panel_y + 62;
    u32 editor_width = panel_width - 44;
    u32 editor_height = panel_height - 82;
    fill_rect(editor_x, editor_y, editor_width, editor_height, 24, 34, 43);

    u32 x = editor_x + 12;
    u32 y = editor_y + 10;
    u32 line_start = x;
    u32 bottom = editor_y + editor_height;
    for (u32 index = 0; index < note_length && y + 14 < bottom; ++index) {
        char character = note_text[index];
        if (character == '\n' || x + 12 > editor_x + editor_width - 8) {
            x = line_start;
            y += 22;
            if (character == '\n') {
                continue;
            }
        }
        char single_character[2] = {character, '\0'};
        draw_text(x, y, single_character, 220, 230, 232);
        x += 12;
    }
    fill_rect(x, y + 15, 2, 12, 49, 175, 130);
}

static void draw_app_tile(u32 x, u32 y, const char *name, char icon,
                          u8 red, u8 green, u8 blue)
{
    fill_rect(x, y, 198, 76, 24, 34, 43);
    fill_rect(x + 12, y + 12, 48, 48, red, green, blue);
    char icon_text[2] = {icon, '\0'};
    draw_text(x + 28, y + 27, icon_text, 250, 250, 250);
    draw_text(x + 72, y + 25, name, 220, 230, 232);
}

static void draw_status_bar(void)
{
    u32 bar_y = framebuffer.height - TASKBAR_HEIGHT;
    fill_rect(0, bar_y, framebuffer.width, TASKBAR_HEIGHT, 17, 25, 33);
    fill_rect(10, bar_y + 5, 58, 26, 49, 175, 130);
    draw_text(19, bar_y + 14, "NOVA", 245, 247, 250);

    u32 task_x = 76;
    for (u32 index = 0; index < MAX_APP_WINDOWS; ++index) {
        app_window_t *window = &app_windows[index];
        if (!window->open) {
            continue;
        }
        u32 button_x = task_x;
        fill_rect(button_x, bar_y + 5, 76, 26,
                  (i32)index == focused_window ? 49 : 31,
                  (i32)index == focused_window ? 96 : 44,
                  (i32)index == focused_window ? 85 : 55);
        const char *label = window->app == APP_CALCULATOR ? "CALC" : app_name(window->app);
        draw_text(button_x + 8, bar_y + 14, label, 220, 230, 232);
        task_x += 80;
    }
    framebuffer_draw_pointer_status(400, 300);
}

static void draw_app_panel(u8 app, u32 panel_x, u32 panel_y,
                           u32 panel_width, u32 panel_height)
{
    if (panel_width < 1 || panel_height < 1) {
        return;
    }
    fill_rect(panel_x, panel_y, panel_width, panel_height, 33, 47, 57);
    fill_rect(panel_x, panel_y, panel_width, 4, 49, 175, 130);

    if (app == APP_FILES) {
        draw_text(panel_x + 22, panel_y + 26, "FILES", 220, 230, 232);
        draw_text(panel_x + 22, panel_y + 68, "BOOT", 157, 174, 181);
        draw_text(panel_x + 190, panel_y + 68, "GRUB CFG", 220, 230, 232);
        draw_text(panel_x + 22, panel_y + 98, "KERNEL", 157, 174, 181);
        draw_text(panel_x + 190, panel_y + 98, "KERNEL ELF", 220, 230, 232);
        draw_text(panel_x + 22, panel_y + 128, "INPUT", 157, 174, 181);
        draw_text(panel_x + 190, panel_y + 128, "PS2 MOUSE", 220, 230, 232);
    } else if (app == APP_NOTES) {
        draw_text(panel_x + 22, panel_y + 26, "NOTES", 220, 230, 232);
        draw_note_text(panel_x, panel_y, panel_width, panel_height);
    } else if (app == APP_ABOUT) {
        draw_text(panel_x + 22, panel_y + 26, "ABOUT NOVA OS", 220, 230, 232);
        draw_text(panel_x + 22, panel_y + 68, "64 BIT HOBBY SYSTEM", 157, 174, 181);
        draw_text(panel_x + 22, panel_y + 98, "FRAMEBUFFER DESKTOP", 157, 174, 181);
        draw_text(panel_x + 22, panel_y + 128, "VERSION " NOVA_VERSION, 157, 174, 181);
    } else if (app == APP_CALCULATOR) {
        draw_calculator(panel_x, panel_y, panel_width);
    } else if (app == APP_TERMINAL) {
        draw_text(panel_x + 22, panel_y + 26, "TERMINAL", 220, 230, 232);
        fill_rect(panel_x + 22, panel_y + 62, panel_width - 44, panel_height - 82,
                  17, 25, 33);
        draw_text(panel_x + 36, panel_y + 78, "NOVA OS READY", 105, 220, 172);
        draw_text(panel_x + 36, panel_y + 108, "SYSTEM ONLINE", 157, 174, 181);
    } else {
        draw_text(panel_x + 22, panel_y + 26, "WORKSPACE READY", 220, 230, 232);
        draw_text(panel_x + 22, panel_y + 68, "MOVE THE POINTER TO BEGIN", 157, 174, 181);
    }

}

static void draw_app_window(u32 index)
{
    app_window_t *window = &app_windows[index];
    if (!window->open || window->minimized) {
        return;
    }

    fill_rect(window->x + 5, window->y + 6, window->width, window->height,
              9, 15, 20);
    fill_rect(window->x, window->y, window->width, window->height,
              33, 47, 57);
    fill_rect(window->x, window->y, window->width, WINDOW_TITLE_HEIGHT,
              (i32)index == focused_window ? 17 : 25,
              (i32)index == focused_window ? 25 : 36,
              (i32)index == focused_window ? 33 : 46);
    draw_text(window->x + 12, window->y + 10, "NOVA OS", 220, 230, 232);
    draw_text(window->x + 112, window->y + 10, app_name(window->app), 157, 174, 181);

    u32 minimize_x = window->x + window->width - 54;
    u32 close_x = window->x + window->width - 28;
    fill_rect(minimize_x, window->y + 5, 22, 18, 70, 87, 96);
    fill_rect(minimize_x + 6, window->y + 14, 10, 2, 245, 247, 250);
    fill_rect(close_x, window->y + 5, 22, 18, 134, 54, 58);
    draw_text(close_x + 6, window->y + 10, "X", 250, 250, 250);

    draw_app_panel(window->app, window->x, window->y + WINDOW_TITLE_HEIGHT,
                   window->width, window->height - WINDOW_TITLE_HEIGHT);
}

void framebuffer_note_input(char character)
{
    if (focused_window < 0 || !app_windows[focused_window].open ||
        app_windows[focused_window].app != APP_NOTES) {
        return;
    }

    if (character == '\b') {
        if (note_length != 0) {
            --note_length;
            note_text[note_length] = '\0';
        }
    } else if (character == '\n' || character == ' ' ||
               (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9')) {
        if (note_length + 1 < sizeof(note_text)) {
            note_text[note_length++] = character;
            note_text[note_length] = '\0';
        }
    }

    draw_app_window((u32)focused_window);
    flush_region(app_windows[focused_window].x, app_windows[focused_window].y,
                 app_windows[focused_window].width + 6,
                 app_windows[focused_window].height + 6);
}

static void draw_app_menu(void)
{
    u32 drawer_x = framebuffer.width > 520 ? (framebuffer.width - 480) / 2 : 16;
    u32 drawer_y = 68;
    fill_rect(drawer_x + 4, drawer_y + 5, 472, 382, 10, 16, 22);
    fill_rect(drawer_x, drawer_y, 472, 382, 33, 47, 57);
    fill_rect(drawer_x, drawer_y, 472, 4, 49, 175, 130);
    draw_text(drawer_x + 22, drawer_y + 22, "APPLICATIONS", 220, 230, 232);
    draw_text(drawer_x + 344, drawer_y + 22, "6 APPS", 157, 174, 181);

    draw_app_tile(drawer_x + 22, drawer_y + 58, "FILES", 'F', 49, 135, 175);
    draw_app_tile(drawer_x + 250, drawer_y + 58, "NOTES", 'N', 49, 175, 130);
    draw_app_tile(drawer_x + 22, drawer_y + 146, "CALCULATOR", 'C', 200, 130, 55);
    draw_app_tile(drawer_x + 250, drawer_y + 146, "TERMINAL", 'T', 75, 120, 92);
    draw_app_tile(drawer_x + 22, drawer_y + 234, "ABOUT OS", 'A', 145, 105, 165);
    draw_app_tile(drawer_x + 250, drawer_y + 234, "DESKTOP", 'D', 85, 110, 125);
}

void framebuffer_draw_desktop(void)
{
    fill_rect(0, 0, framebuffer.width, framebuffer.height,
              DESKTOP_RED, DESKTOP_GREEN, DESKTOP_BLUE);
    draw_top_bar();
    draw_status_bar();
    for (u32 index = 0; index < MAX_APP_WINDOWS; ++index) {
        if ((i32)index != focused_window) {
            draw_app_window(index);
        }
    }
    if (focused_window >= 0) {
        draw_app_window((u32)focused_window);
    }
    if (file_menu_open) {
        draw_app_menu();
    }
    if (power_menu_open) {
        draw_power_menu();
    }
    if (!clip_enabled) {
        flush_region(0, 0, framebuffer.width, framebuffer.height);
    }
}

void framebuffer_draw_pointer_status(i32 x, i32 y)
{
    if (framebuffer.width <= 200 || framebuffer.height < 28) {
        return;
    }

    char position[24];
    u32 length = 0;
    position[length++] = 'X';
    position[length++] = ' ';
    length = append_number(position, length, x < 0 ? 0u : (u32)x);
    position[length++] = ' ';
    position[length++] = 'Y';
    position[length++] = ' ';
    length = append_number(position, length, y < 0 ? 0u : (u32)y);
    position[length] = '\0';

    fill_rect(framebuffer.width - 144, framebuffer.height - TASKBAR_HEIGHT,
              144, TASKBAR_HEIGHT, 17, 25, 33);
    draw_text(framebuffer.width - 136, framebuffer.height - 22,
              position, 157, 174, 181);
    if (!clip_enabled) {
        flush_region(framebuffer.width - 144, framebuffer.height - TASKBAR_HEIGHT,
                     144, TASKBAR_HEIGHT);
    }
}

u8 framebuffer_take_power_action(void)
{
    u8 action = requested_power_action;
    requested_power_action = POWER_ACTION_NONE;
    return action;
}

void framebuffer_handle_click(i32 x, i32 y)
{
    u32 power_x = framebuffer.width > 140 ? framebuffer.width - 128 : 252;
    if (x >= (i32)power_x && x < (i32)(power_x + 112) && y >= 4 && y < 36) {
        power_menu_open = !power_menu_open;
        file_menu_open = 0;
        redraw_desktop_region(power_x > 4 ? power_x - 4 : 0, 0, 196, 140);
        redraw_desktop_region(0, 0, framebuffer.width, 40);
        return;
    }

    if (power_menu_open) {
        u32 menu_x = power_menu_x();
        if (x >= (i32)menu_x && x < (i32)(menu_x + 180) && y >= 40 && y < 130) {
            requested_power_action = y < 84
                ? POWER_ACTION_RESTART
                : POWER_ACTION_SHUTDOWN;
            power_menu_open = 0;
            redraw_desktop_region(menu_x > 4 ? menu_x - 4 : 0, 0, 196, 140);
            return;
        }
        power_menu_open = 0;
        redraw_desktop_region(menu_x > 4 ? menu_x - 4 : 0, 0, 196, 140);
    }

    if (x >= 148 && x < 244 && y >= 4 && y < 36) {
        file_menu_open = !file_menu_open;
        redraw_desktop_region(0, 0, 252, 40);
        redraw_app_drawer();
        return;
    }

    u32 drawer_x = framebuffer.width > 520 ? (framebuffer.width - 480) / 2 : 16;
    if (file_menu_open && x >= (i32)drawer_x && x < (i32)(drawer_x + 472) &&
        y >= 126 && y < 374) {
        u32 row = (u32)(y - 126) / 88;
        u32 column = (u32)(x - (i32)drawer_x) < 236 ? 0 : 1;
        u8 app = APP_DESKTOP;
        if (row == 0) {
            app = column == 0 ? APP_FILES : APP_NOTES;
        } else if (row == 1) {
            app = column == 0 ? APP_CALCULATOR : APP_TERMINAL;
        } else if (row == 2) {
            app = column == 0 ? APP_ABOUT : APP_DESKTOP;
        }
        launch_app(app);
        return;
    }

    if (file_menu_open) {
        file_menu_open = 0;
        redraw_app_drawer();
        redraw_desktop_region(0, 0, 252, 40);
        return;
    }

    u32 bar_y = framebuffer.height - TASKBAR_HEIGHT;
    if (y >= (i32)bar_y && x >= 76) {
        u32 task_x = 76;
        for (u32 index = 0; index < MAX_APP_WINDOWS; ++index) {
            app_window_t *window = &app_windows[index];
            if (!window->open) {
                continue;
            }
            if (x >= (i32)task_x && x < (i32)(task_x + 76)) {
                i32 old_focus = focused_window;
                if ((i32)index == focused_window && !window->minimized) {
                    window->minimized = 1;
                    focused_window = -1;
                    for (i32 other = MAX_APP_WINDOWS - 1; other >= 0; --other) {
                        if (app_windows[other].open && !app_windows[other].minimized) {
                            focused_window = other;
                            break;
                        }
                    }
                } else {
                    window->minimized = 0;
                    focused_window = (i32)index;
                }
                if (old_focus >= 0) {
                    redraw_window_region((u32)old_focus);
                }
                redraw_window_region(index);
                redraw_taskbar();
                return;
            }
            task_x += 80;
        }
    }

    for (i32 pass = 0; pass <= MAX_APP_WINDOWS; ++pass) {
        i32 index = pass == 0 ? focused_window : (i32)MAX_APP_WINDOWS - pass;
        if (index < 0 || index >= MAX_APP_WINDOWS || (pass != 0 && index == focused_window)) {
            continue;
        }
        app_window_t *window = &app_windows[index];
        if (!window->open || window->minimized ||
            x < (i32)window->x || x >= (i32)(window->x + window->width) ||
            y < (i32)window->y || y >= (i32)(window->y + window->height)) {
            continue;
        }

        i32 old_focus = focused_window;
        focused_window = index;
        if (y < (i32)(window->y + WINDOW_TITLE_HEIGHT)) {
            u32 close_x = window->x + window->width - 28;
            u32 minimize_x = window->x + window->width - 54;
            if (x >= (i32)close_x) {
                window->open = 0;
                if (focused_window == index) {
                    focused_window = -1;
                    for (i32 other = MAX_APP_WINDOWS - 1; other >= 0; --other) {
                        if (app_windows[other].open && !app_windows[other].minimized) {
                            focused_window = other;
                            break;
                        }
                    }
                }
            } else if (x >= (i32)minimize_x) {
                window->minimized = 1;
                focused_window = -1;
                for (i32 other = MAX_APP_WINDOWS - 1; other >= 0; --other) {
                    if (app_windows[other].open && !app_windows[other].minimized) {
                        focused_window = other;
                        break;
                    }
                }
            } else {
                dragged_window = index;
                drag_offset_x = x - (i32)window->x;
                drag_offset_y = y - (i32)window->y;
            }
            if (old_focus >= 0 && old_focus != index) {
                redraw_window_region((u32)old_focus);
            }
            redraw_window_region((u32)index);
            if (focused_window >= 0 && focused_window != index && focused_window != old_focus) {
                redraw_window_region((u32)focused_window);
            }
            redraw_taskbar();
            return;
        }

        if (window->app == APP_CALCULATOR) {
            u32 panel_y = window->y + WINDOW_TITLE_HEIGHT;
            u32 panel_width = window->width;
            if (x >= (i32)(window->x + 22) &&
                x < (i32)(window->x + panel_width - 22) &&
                y >= (i32)(panel_y + 98) && y < (i32)(panel_y + 210)) {
                static const char buttons[4][4] = {
                    {'7', '8', '9', '/'},
                    {'4', '5', '6', '*'},
                    {'1', '2', '3', '-'},
                    {'c', '0', '=', '+'}
                };
                u32 button_width = (panel_width - 44) / 4;
                u32 column = (u32)(x - (i32)(window->x + 22)) / button_width;
                u32 button_offset_y = (u32)(y - (i32)(panel_y + 98));
                u32 row = button_offset_y / 28;
                if (row < 4 && column < 4 && button_offset_y % 28 < 24) {
                    calculator_input(buttons[row][column]);
                }
                redraw_window_region((u32)index);
                if (old_focus >= 0 && old_focus != index) {
                    redraw_window_region((u32)old_focus);
                }
                redraw_taskbar();
                return;
            }
        }
        if (old_focus != index) {
            if (old_focus >= 0) {
                redraw_window_region((u32)old_focus);
            }
            redraw_window_region((u32)index);
            redraw_taskbar();
        }
        return;
    }

    dragged_window = -1;
}

void framebuffer_pointer_move(i32 x, i32 y, u8 buttons)
{
    if (dragged_window < 0 || (buttons & 1u) == 0) {
        if ((buttons & 1u) == 0) {
            dragged_window = -1;
        }
        return;
    }

    app_window_t *window = &app_windows[dragged_window];
    u32 old_x = window->x;
    u32 old_y = window->y;
    u32 window_width = window->width;
    u32 window_height = window->height;
    i32 next_x = x - drag_offset_x;
    i32 next_y = y - drag_offset_y;
    i32 max_x = (i32)(framebuffer.width - window->width);
    i32 max_y = (i32)(framebuffer.height - window->height - TASKBAR_HEIGHT);
    if (next_x < 0) next_x = 0;
    if (next_y < 40) next_y = 40;
    if (next_x > max_x) next_x = max_x;
    if (next_y > max_y) next_y = max_y;
    if ((u32)next_x == old_x && (u32)next_y == old_y) {
        return;
    }

    redraw_window_bounds(old_x, old_y, window_width, window_height);
    window->x = (u32)next_x;
    window->y = (u32)next_y;
    redraw_window_region((u32)dragged_window);
}

void framebuffer_xor_cursor(i32 x, i32 y)
{
    if (cursor_visible) {
        for (u32 row = 0; row < 7; ++row) {
            for (u32 column = 0; column < 7; ++column) {
                i32 pixel_x = cursor_x_saved + (i32)column;
                i32 pixel_y = cursor_y_saved + (i32)row;
                if (pixel_x < 0 || pixel_y < 0 ||
                    (u32)pixel_x >= framebuffer.width || (u32)pixel_y >= framebuffer.height) {
                    continue;
                }
                u8 *pixel = framebuffer.pixels + (u32)pixel_y * framebuffer.pitch +
                            (u32)pixel_x * framebuffer.bytes_per_pixel;
                u32 saved_offset = (row * 7 + column) * framebuffer.bytes_per_pixel;
                for (u8 byte = 0; byte < framebuffer.bytes_per_pixel; ++byte) {
                    pixel[byte] = cursor_saved_pixels[saved_offset + byte];
                }
            }
        }
        cursor_visible = 0;
        flush_cursor_region(cursor_x_saved, cursor_y_saved);
        if (x == cursor_x_saved && y == cursor_y_saved) {
            return;
        }
    }

    cursor_x_saved = x;
    cursor_y_saved = y;
    for (u32 row = 0; row < 7; ++row) {
        for (u32 column = 0; column < 7; ++column) {
            i32 pixel_x = x + (i32)column;
            i32 pixel_y = y + (i32)row;
            if (pixel_x < 0 || pixel_y < 0 ||
                (u32)pixel_x >= framebuffer.width || (u32)pixel_y >= framebuffer.height) {
                continue;
            }
            u8 *pixel = framebuffer.pixels + (u32)pixel_y * framebuffer.pitch +
                        (u32)pixel_x * framebuffer.bytes_per_pixel;
            u32 saved_offset = (row * 7 + column) * framebuffer.bytes_per_pixel;
            for (u8 byte = 0; byte < framebuffer.bytes_per_pixel; ++byte) {
                cursor_saved_pixels[saved_offset + byte] = pixel[byte];
                if ((cursor_shape[row] & (1u << (6 - column))) != 0) {
                    u32 white = pack_color(255, 255, 255);
                    pixel[byte] = (u8)(white >> (byte * 8));
                }
            }
        }
    }
    cursor_visible = 1;
    flush_cursor_region(cursor_x_saved, cursor_y_saved);
}