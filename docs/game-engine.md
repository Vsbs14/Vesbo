# 2D Game Engine & Raylib API

Vesbo includes built-in 2D graphics, windowing, input, and collision bindings powered by **Raylib 6.0** (`src/raylib_bindings.c`). You can write complete 2D games in pure `.vsb` or compile them into standalone `.vbo` bytecode binaries.

---

## Minimal Game Loop Example

```vsb
fnc main()
	rl_init_window(800, 450, "Vesbo 2D Game")
	rl_set_target_fps(60)

	var x is 400
	var y is 225
	var speed is 250

	loop while not rl_window_should_close()
		var dt is rl_get_frame_time()

		if rl_is_key_down("right") or rl_is_key_down("d")
			x = x + speed * dt
		if rl_is_key_down("left") or rl_is_key_down("a")
			x = x - speed * dt

		rl_begin_drawing()
		rl_clear_background("#0f172a")
		rl_draw_circle(x, y, 24, "skyblue")
		rl_draw_text("Use Arrow Keys or A/D to Move", 20, 20, 20, "raywhite")
		rl_end_drawing()

	rl_close_window()
```

---

## Running the Built-in Breakout Game

A complete Breakout game written in pure Vesbo is located in `games/breakout/breakout.vsb`.

- **Windows**:
  - Tree-walking interpreter: `games\breakout\run.bat`
  - Bytecode VM: `games\breakout\run_bytecode.bat`
- **Linux & macOS** *(requires Raylib installed)*:
  - Tree-walking interpreter: `./games/breakout/run.sh`
  - Bytecode VM: `./games/breakout/run_bytecode.sh`

---

## API Reference

### Window & Frame Timing

| Function | Description |
| :--- | :--- |
| `rl_init_window(width, height, title)` | Open a window and initialize the OpenGL context. |
| `rl_close_window()` | Close the window and unload graphics resources. |
| `rl_window_should_close()` | Return `true` if `ESC` or the window close button was pressed. |
| `rl_set_target_fps(fps)` | Set target frame rate (e.g., `60`). |
| `rl_get_fps()` | Return current frames per second. |
| `rl_get_frame_time()` | Return delta time (`dt`) in seconds since the last frame. |
| `rl_get_screen_width()` | Return current window width in pixels. |
| `rl_get_screen_height()` | Return current window height in pixels. |
| `rl_set_window_title(title)` | Update the window title bar text. |

### Drawing & Primitives

| Function | Description |
| :--- | :--- |
| `rl_begin_drawing()` | Setup canvas framebuffer for drawing a frame. |
| `rl_end_drawing()` | Swap buffers and poll input events at end of frame. |
| `rl_clear_background(color)` | Clear screen background to `color`. |
| `rl_draw_rectangle(x, y, w, h, color)` | Draw a filled rectangle. |
| `rl_draw_rectangle_lines(x, y, w, h, color)` | Draw a rectangle outline. |
| `rl_draw_rectangle_rounded(x, y, w, h, roundness, segments, color)` | Draw a filled rectangle with rounded corners. |
| `rl_draw_circle(cx, cy, radius, color)` | Draw a filled circle. |
| `rl_draw_circle_lines(cx, cy, radius, color)` | Draw a circle outline. |
| `rl_draw_line(x1, y1, x2, y2, color)` | Draw a line between `(x1, y1)` and `(x2, y2)`. |
| `rl_draw_text(text, x, y, font_size, color)` | Render text at `(x, y)` with `font_size`. |
| `rl_measure_text(text, font_size)` | Return width in pixels of `text` at `font_size`. |

### Colors

Any `color` parameter accepts:
- Named strings (`"raywhite"`, `"black"`, `"red"`, `"green"`, `"blue"`, `"skyblue"`, `"gold"`, `"orange"`, `"purple"`, `"darkgray"`, etc.)
- Hex strings (`"#38bdf8"`, `"#0f172a"`, `"#ff5500ff"`)
- RGBA arrays (`[255, 120, 0, 255]`) created directly or via `rl_color(r, g, b, [a])`
- `rl_fade(color, alpha)` — Return a copy of `color` with alpha multiplied by `0.0 .. 1.0`.

### Keyboard & Mouse Input

| Function | Description |
| :--- | :--- |
| `rl_is_key_down(key)` | Return `true` while `key` is held down (`"left"`, `"right"`, `"up"`, `"down"`, `"space"`, `"enter"`, `"escape"`, `"a"`..`"z"`, `"0"`..`"9"`). |
| `rl_is_key_pressed(key)` | Return `true` if `key` was pressed this frame. |
| `rl_is_key_released(key)` | Return `true` if `key` was released this frame. |
| `rl_get_mouse_x()` / `rl_get_mouse_y()` | Return current mouse cursor X or Y coordinate. |
| `rl_is_mouse_button_down(btn)` | Return `true` while mouse button (`"left"`, `"right"`, `"middle"`) is held. |
| `rl_is_mouse_button_pressed(btn)` | Return `true` if mouse button was clicked this frame. |

### Collision & Random Helpers

| Function | Description |
| :--- | :--- |
| `rl_check_collision_recs(x1, y1, w1, h1, x2, y2, w2, h2)` | Check AABB overlap between two rectangles. |
| `rl_check_collision_circle_rec(cx, cy, radius, rx, ry, rw, rh)` | Check overlap between a circle and a rectangle. |
| `rl_check_collision_circles(x1, y1, r1, x2, y2, r2)` | Check overlap between two circles. |
| `rl_get_random_value(min, max)` | Return a random integer in `[min, max]` inclusive. |
