#ifndef NO_RAYLIB
#include "raylib.h"
#endif
#include "../include/raylib_bindings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef NO_RAYLIB
/* Case-insensitive string comparison helper */
static int str_case_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static Color parse_color(Value val) {
    if (val.type == VAL_STRING) {
        const char *s = val.as.string;
        if (!s) return WHITE;

        /* Named colors */
        if (str_case_eq(s, "black"))        return BLACK;
        if (str_case_eq(s, "white"))        return WHITE;
        if (str_case_eq(s, "gray") || str_case_eq(s, "grey")) return GRAY;
        if (str_case_eq(s, "dark_gray") || str_case_eq(s, "darkgray")) return DARKGRAY;
        if (str_case_eq(s, "light_gray") || str_case_eq(s, "lightgray")) return LIGHTGRAY;
        if (str_case_eq(s, "red"))          return RED;
        if (str_case_eq(s, "maroon"))       return MAROON;
        if (str_case_eq(s, "green"))        return GREEN;
        if (str_case_eq(s, "lime"))         return LIME;
        if (str_case_eq(s, "dark_green") || str_case_eq(s, "darkgreen")) return DARKGREEN;
        if (str_case_eq(s, "blue"))         return BLUE;
        if (str_case_eq(s, "dark_blue") || str_case_eq(s, "darkblue")) return DARKBLUE;
        if (str_case_eq(s, "sky_blue") || str_case_eq(s, "skyblue")) return SKYBLUE;
        if (str_case_eq(s, "yellow"))       return YELLOW;
        if (str_case_eq(s, "gold"))         return GOLD;
        if (str_case_eq(s, "orange"))       return ORANGE;
        if (str_case_eq(s, "pink"))         return PINK;
        if (str_case_eq(s, "purple"))       return PURPLE;
        if (str_case_eq(s, "violet"))       return VIOLET;
        if (str_case_eq(s, "dark_purple") || str_case_eq(s, "darkpurple")) return DARKPURPLE;
        if (str_case_eq(s, "beige"))        return BEIGE;
        if (str_case_eq(s, "brown"))        return BROWN;
        if (str_case_eq(s, "dark_brown") || str_case_eq(s, "darkbrown")) return DARKBROWN;
        if (str_case_eq(s, "raywhite"))     return RAYWHITE;
        if (str_case_eq(s, "magenta"))      return MAGENTA;
        if (str_case_eq(s, "blank"))        return BLANK;

        /* Hex colors: #RRGGBB or #RRGGBBAA or #RGB */
        if (s[0] == '#') {
            size_t len = strlen(s + 1);
            if (len == 6) {
                unsigned int r, g, b;
                if (sscanf(s + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
                    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
                }
            } else if (len == 8) {
                unsigned int r, g, b, a;
                if (sscanf(s + 1, "%02x%02x%02x%02x", &r, &g, &b, &a) == 4) {
                    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
                }
            } else if (len == 3) {
                unsigned int r, g, b;
                if (sscanf(s + 1, "%1x%1x%1x", &r, &g, &b) == 3) {
                    return (Color){ (unsigned char)(r * 17), (unsigned char)(g * 17), (unsigned char)(b * 17), 255 };
                }
            }
        }
    } else if (val.type == VAL_ARRAY) {
        if (val.as.array.count >= 4) {
            return (Color){
                (unsigned char)val.as.array.items[0].as.number,
                (unsigned char)val.as.array.items[1].as.number,
                (unsigned char)val.as.array.items[2].as.number,
                (unsigned char)val.as.array.items[3].as.number
            };
        } else if (val.as.array.count == 3) {
            return (Color){
                (unsigned char)val.as.array.items[0].as.number,
                (unsigned char)val.as.array.items[1].as.number,
                (unsigned char)val.as.array.items[2].as.number,
                255
            };
        }
    }
    return WHITE;
}

static int parse_key(Value val) {
    if (val.type == VAL_NUMBER) {
        return (int)val.as.number;
    }
    if (val.type == VAL_STRING) {
        const char *s = val.as.string;
        if (!s) return 0;
        if (str_case_eq(s, "left"))      return KEY_LEFT;
        if (str_case_eq(s, "right"))     return KEY_RIGHT;
        if (str_case_eq(s, "up"))        return KEY_UP;
        if (str_case_eq(s, "down"))      return KEY_DOWN;
        if (str_case_eq(s, "space"))     return KEY_SPACE;
        if (str_case_eq(s, "enter") || str_case_eq(s, "return")) return KEY_ENTER;
        if (str_case_eq(s, "escape") || str_case_eq(s, "esc"))   return KEY_ESCAPE;
        if (str_case_eq(s, "backspace")) return KEY_BACKSPACE;
        if (str_case_eq(s, "tab"))       return KEY_TAB;
        if (str_case_eq(s, "r"))         return KEY_R;
        if (str_case_eq(s, "p"))         return KEY_P;
        if (str_case_eq(s, "a"))         return KEY_A;
        if (str_case_eq(s, "d"))         return KEY_D;
        if (str_case_eq(s, "w"))         return KEY_W;
        if (str_case_eq(s, "s"))         return KEY_S;

        if (strlen(s) == 1) {
            char c = (char)tolower((unsigned char)s[0]);
            if (c >= 'a' && c <= 'z') return KEY_A + (c - 'a');
            if (c >= '0' && c <= '9') return KEY_ZERO + (c - '0');
        }
    }
    return 0;
}

static int parse_mouse_button(Value val) {
    if (val.type == VAL_NUMBER) return (int)val.as.number;
    if (val.type == VAL_STRING) {
        if (str_case_eq(val.as.string, "left"))   return MOUSE_BUTTON_LEFT;
        if (str_case_eq(val.as.string, "right"))  return MOUSE_BUTTON_RIGHT;
        if (str_case_eq(val.as.string, "middle")) return MOUSE_BUTTON_MIDDLE;
    }
    return MOUSE_BUTTON_LEFT;
}
#endif

/* Check if a given name matches a raylib function (supports both "rl_foo" and "foo") */
static int matches(const char *name, const char *func) {
    if (strcmp(name, func) == 0) return 1;
    if (strncmp(func, "rl_", 3) == 0 && strcmp(name, func + 3) == 0) return 1;
    return 0;
}

int is_raylib_builtin(const char *name) {
    return matches(name, "rl_init_window") ||
           matches(name, "rl_close_window") ||
           matches(name, "rl_window_should_close") ||
           matches(name, "rl_set_target_fps") ||
           matches(name, "rl_get_fps") ||
           matches(name, "rl_get_frame_time") ||
           matches(name, "rl_get_screen_width") ||
           matches(name, "rl_get_screen_height") ||
           matches(name, "rl_set_window_title") ||
           matches(name, "rl_set_window_focused") ||
           matches(name, "rl_set_window_position") ||
           matches(name, "rl_begin_drawing") ||
           matches(name, "rl_end_drawing") ||
           matches(name, "rl_clear_background") ||
           matches(name, "rl_draw_rectangle") ||
           matches(name, "rl_draw_rectangle_lines") ||
           matches(name, "rl_draw_rectangle_rounded") ||
           matches(name, "rl_draw_circle") ||
           matches(name, "rl_draw_circle_lines") ||
           matches(name, "rl_draw_line") ||
           matches(name, "rl_draw_line_ex") ||
           matches(name, "rl_draw_text") ||
           matches(name, "rl_measure_text") ||
           matches(name, "rl_is_key_down") ||
           matches(name, "rl_is_key_pressed") ||
           matches(name, "rl_is_key_released") ||
           matches(name, "rl_get_mouse_x") ||
           matches(name, "rl_get_mouse_y") ||
           matches(name, "rl_is_mouse_button_down") ||
           matches(name, "rl_is_mouse_button_pressed") ||
           matches(name, "rl_check_collision_recs") ||
           matches(name, "rl_check_collision_circle_rec") ||
           matches(name, "rl_check_collision_circles") ||
           matches(name, "rl_check_collision_point_rec") ||
           matches(name, "rl_get_random_value") ||
           matches(name, "rl_color") ||
           matches(name, "rl_fade") ||
           matches(name, "rl_init_audio_device") ||
           matches(name, "rl_close_audio_device") ||
           matches(name, "rl_is_audio_device_ready");
}

#ifndef NO_RAYLIB
int call_raylib_builtin(const char *name, Value *args, int arg_count, Value *out,
                        const char **error_message) {
    /* Window Lifecycle */
    if (matches(name, "rl_init_window")) {
        if (arg_count < 3) {
            *error_message = "rl_init_window(width, height, title) expects 3 arguments";
            return -1;
        }
        int w = (int)args[0].as.number;
        int h = (int)args[1].as.number;
        char *title = value_to_display_string(args[2]);
        InitWindow(w, h, title);
        SetWindowFocused();
        free(title);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_set_window_focused")) {
        SetWindowFocused();
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_set_window_position")) {
        if (arg_count < 2) {
            *error_message = "rl_set_window_position(x, y) expects 2 arguments";
            return -1;
        }
        SetWindowPosition((int)args[0].as.number, (int)args[1].as.number);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_close_window")) {
        CloseWindow();
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_window_should_close")) {
        *out = make_bool_value(WindowShouldClose() ? 1 : 0);
        return 1;
    }

    if (matches(name, "rl_set_target_fps")) {
        if (arg_count < 1) {
            *error_message = "rl_set_target_fps(fps) expects 1 argument";
            return -1;
        }
        SetTargetFPS((int)args[0].as.number);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_get_fps")) {
        *out = make_number_value((double)GetFPS());
        return 1;
    }

    if (matches(name, "rl_get_frame_time")) {
        *out = make_number_value((double)GetFrameTime());
        return 1;
    }

    if (matches(name, "rl_get_screen_width")) {
        *out = make_number_value((double)GetScreenWidth());
        return 1;
    }

    if (matches(name, "rl_get_screen_height")) {
        *out = make_number_value((double)GetScreenHeight());
        return 1;
    }

    if (matches(name, "rl_set_window_title")) {
        if (arg_count < 1) {
            *error_message = "rl_set_window_title(title) expects 1 argument";
            return -1;
        }
        char *title = value_to_display_string(args[0]);
        SetWindowTitle(title);
        free(title);
        *out = make_none_value();
        return 1;
    }

    /* Drawing */
    if (matches(name, "rl_begin_drawing")) {
        BeginDrawing();
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_end_drawing")) {
        EndDrawing();
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_clear_background")) {
        Color c = (arg_count >= 1) ? parse_color(args[0]) : BLACK;
        ClearBackground(c);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_draw_rectangle")) {
        if (arg_count < 5) {
            *error_message = "rl_draw_rectangle(x, y, w, h, color) expects 5 arguments";
            return -1;
        }
        int x = (int)args[0].as.number;
        int y = (int)args[1].as.number;
        int w = (int)args[2].as.number;
        int h = (int)args[3].as.number;
        Color c = parse_color(args[4]);
        DrawRectangle(x, y, w, h, c);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_draw_rectangle_lines")) {
        if (arg_count < 5) {
            *error_message = "rl_draw_rectangle_lines(x, y, w, h, color) expects 5 arguments";
            return -1;
        }
        int x = (int)args[0].as.number;
        int y = (int)args[1].as.number;
        int w = (int)args[2].as.number;
        int h = (int)args[3].as.number;
        Color c = parse_color(args[4]);
        DrawRectangleLines(x, y, w, h, c);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_draw_rectangle_rounded")) {
        if (arg_count < 7) {
            *error_message = "rl_draw_rectangle_rounded(x, y, w, h, roundness, segments, color) expects 7 arguments";
            return -1;
        }
        Rectangle rec = {
            (float)args[0].as.number,
            (float)args[1].as.number,
            (float)args[2].as.number,
            (float)args[3].as.number
        };
        float roundness = (float)args[4].as.number;
        int segments = (int)args[5].as.number;
        Color c = parse_color(args[6]);
        DrawRectangleRounded(rec, roundness, segments, c);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_draw_circle")) {
        if (arg_count < 4) {
            *error_message = "rl_draw_circle(cx, cy, radius, color) expects 4 arguments";
            return -1;
        }
        int cx = (int)args[0].as.number;
        int cy = (int)args[1].as.number;
        float r = (float)args[2].as.number;
        Color c = parse_color(args[3]);
        DrawCircle(cx, cy, r, c);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_draw_circle_lines")) {
        if (arg_count < 4) {
            *error_message = "rl_draw_circle_lines(cx, cy, radius, color) expects 4 arguments";
            return -1;
        }
        int cx = (int)args[0].as.number;
        int cy = (int)args[1].as.number;
        float r = (float)args[2].as.number;
        Color c = parse_color(args[3]);
        DrawCircleLines(cx, cy, r, c);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_draw_line")) {
        if (arg_count < 5) {
            *error_message = "rl_draw_line(x1, y1, x2, y2, color) expects 5 arguments";
            return -1;
        }
        int x1 = (int)args[0].as.number;
        int y1 = (int)args[1].as.number;
        int x2 = (int)args[2].as.number;
        int y2 = (int)args[3].as.number;
        Color c = parse_color(args[4]);
        DrawLine(x1, y1, x2, y2, c);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_draw_line_ex")) {
        if (arg_count < 6) {
            *error_message = "rl_draw_line_ex(x1, y1, x2, y2, thick, color) expects 6 arguments";
            return -1;
        }
        Vector2 start = { (float)args[0].as.number, (float)args[1].as.number };
        Vector2 end = { (float)args[2].as.number, (float)args[3].as.number };
        float thick = (float)args[4].as.number;
        Color c = parse_color(args[5]);
        DrawLineEx(start, end, thick, c);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_draw_text")) {
        if (arg_count < 5) {
            *error_message = "rl_draw_text(text, x, y, size, color) expects 5 arguments";
            return -1;
        }
        char *text = value_to_display_string(args[0]);
        int x = (int)args[1].as.number;
        int y = (int)args[2].as.number;
        int size = (int)args[3].as.number;
        Color c = parse_color(args[4]);
        DrawText(text, x, y, size, c);
        free(text);
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_measure_text")) {
        if (arg_count < 2) {
            *error_message = "rl_measure_text(text, size) expects 2 arguments";
            return -1;
        }
        char *text = value_to_display_string(args[0]);
        int size = (int)args[1].as.number;
        int width = MeasureText(text, size);
        free(text);
        *out = make_number_value((double)width);
        return 1;
    }

    /* Input */
    if (matches(name, "rl_is_key_down")) {
        if (arg_count < 1) {
            *error_message = "rl_is_key_down(key) expects 1 argument";
            return -1;
        }
        int key = parse_key(args[0]);
        *out = make_bool_value(IsKeyDown(key) ? 1 : 0);
        return 1;
    }

    if (matches(name, "rl_is_key_pressed")) {
        if (arg_count < 1) {
            *error_message = "rl_is_key_pressed(key) expects 1 argument";
            return -1;
        }
        int key = parse_key(args[0]);
        *out = make_bool_value(IsKeyPressed(key) ? 1 : 0);
        return 1;
    }

    if (matches(name, "rl_is_key_released")) {
        if (arg_count < 1) {
            *error_message = "rl_is_key_released(key) expects 1 argument";
            return -1;
        }
        int key = parse_key(args[0]);
        *out = make_bool_value(IsKeyReleased(key) ? 1 : 0);
        return 1;
    }

    if (matches(name, "rl_get_mouse_x")) {
        *out = make_number_value((double)GetMouseX());
        return 1;
    }

    if (matches(name, "rl_get_mouse_y")) {
        *out = make_number_value((double)GetMouseY());
        return 1;
    }

    if (matches(name, "rl_is_mouse_button_down")) {
        int btn = (arg_count >= 1) ? parse_mouse_button(args[0]) : MOUSE_BUTTON_LEFT;
        *out = make_bool_value(IsMouseButtonDown(btn) ? 1 : 0);
        return 1;
    }

    if (matches(name, "rl_is_mouse_button_pressed")) {
        int btn = (arg_count >= 1) ? parse_mouse_button(args[0]) : MOUSE_BUTTON_LEFT;
        *out = make_bool_value(IsMouseButtonPressed(btn) ? 1 : 0);
        return 1;
    }

    /* Collisions */
    if (matches(name, "rl_check_collision_recs")) {
        if (arg_count < 8) {
            *error_message = "rl_check_collision_recs(x1, y1, w1, h1, x2, y2, w2, h2) expects 8 arguments";
            return -1;
        }
        Rectangle r1 = { (float)args[0].as.number, (float)args[1].as.number, (float)args[2].as.number, (float)args[3].as.number };
        Rectangle r2 = { (float)args[4].as.number, (float)args[5].as.number, (float)args[6].as.number, (float)args[7].as.number };
        *out = make_bool_value(CheckCollisionRecs(r1, r2) ? 1 : 0);
        return 1;
    }

    if (matches(name, "rl_check_collision_circle_rec")) {
        if (arg_count < 7) {
            *error_message = "rl_check_collision_circle_rec(cx, cy, r, rx, ry, rw, rh) expects 7 arguments";
            return -1;
        }
        Vector2 center = { (float)args[0].as.number, (float)args[1].as.number };
        float radius = (float)args[2].as.number;
        Rectangle rec = { (float)args[3].as.number, (float)args[4].as.number, (float)args[5].as.number, (float)args[6].as.number };
        *out = make_bool_value(CheckCollisionCircleRec(center, radius, rec) ? 1 : 0);
        return 1;
    }

    if (matches(name, "rl_check_collision_circles")) {
        if (arg_count < 6) {
            *error_message = "rl_check_collision_circles(x1, y1, r1, x2, y2, r2) expects 6 arguments";
            return -1;
        }
        Vector2 c1 = { (float)args[0].as.number, (float)args[1].as.number };
        float r1 = (float)args[2].as.number;
        Vector2 c2 = { (float)args[3].as.number, (float)args[4].as.number };
        float r2 = (float)args[5].as.number;
        *out = make_bool_value(CheckCollisionCircles(c1, r1, c2, r2) ? 1 : 0);
        return 1;
    }

    if (matches(name, "rl_check_collision_point_rec")) {
        if (arg_count < 6) {
            *error_message = "rl_check_collision_point_rec(px, py, rx, ry, rw, rh) expects 6 arguments";
            return -1;
        }
        Vector2 pt = { (float)args[0].as.number, (float)args[1].as.number };
        Rectangle rec = { (float)args[2].as.number, (float)args[3].as.number, (float)args[4].as.number, (float)args[5].as.number };
        *out = make_bool_value(CheckCollisionPointRec(pt, rec) ? 1 : 0);
        return 1;
    }

    /* Random */
    if (matches(name, "rl_get_random_value")) {
        if (arg_count < 2) {
            *error_message = "rl_get_random_value(min, max) expects 2 arguments";
            return -1;
        }
        int min = (int)args[0].as.number;
        int max = (int)args[1].as.number;
        *out = make_number_value((double)GetRandomValue(min, max));
        return 1;
    }

    /* Color constructor: rl_color(r, g, b, [a]) */
    if (matches(name, "rl_color")) {
        if (arg_count < 3) {
            *error_message = "rl_color(r, g, b, [a]) expects at least 3 arguments";
            return -1;
        }
        Value arr = make_array_value();
        value_array_push(&arr.as.array, make_number_value(args[0].as.number));
        value_array_push(&arr.as.array, make_number_value(args[1].as.number));
        value_array_push(&arr.as.array, make_number_value(args[2].as.number));
        double alpha = (arg_count >= 4) ? args[3].as.number : 255.0;
        value_array_push(&arr.as.array, make_number_value(alpha));
        *out = arr;
        return 1;
    }

    /* Color fade: rl_fade(color, alpha) where alpha is 0.0 to 1.0 */
    if (matches(name, "rl_fade")) {
        if (arg_count < 2) {
            *error_message = "rl_fade(color, alpha) expects 2 arguments";
            return -1;
        }
        Color c = parse_color(args[0]);
        float alpha = (float)args[1].as.number;
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;
        Color faded = Fade(c, alpha);
        Value arr = make_array_value();
        value_array_push(&arr.as.array, make_number_value(faded.r));
        value_array_push(&arr.as.array, make_number_value(faded.g));
        value_array_push(&arr.as.array, make_number_value(faded.b));
        value_array_push(&arr.as.array, make_number_value(faded.a));
        *out = arr;
        return 1;
    }

    /* Audio */
    if (matches(name, "rl_init_audio_device")) {
        InitAudioDevice();
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_close_audio_device")) {
        CloseAudioDevice();
        *out = make_none_value();
        return 1;
    }

    if (matches(name, "rl_is_audio_device_ready")) {
        *out = make_bool_value(IsAudioDeviceReady() ? 1 : 0);
        return 1;
    }

    return 0; /* Not a raylib builtin */
}
#else
int call_raylib_builtin(const char *name, Value *args, int arg_count, Value *out,
                        const char **error_message) {
    (void)name;
    (void)args;
    (void)arg_count;
    (void)out;
    *error_message = "Raylib support is not compiled into this binary. Recompile with Raylib enabled to use Raylib functions.";
    return -1;
}
#endif
