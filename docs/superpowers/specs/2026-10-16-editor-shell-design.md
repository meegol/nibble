# Editor shell — design

Sub-project 3. Lets nibble switch between running a cart and editing it, PICO-8 style,
and gives the code and sprite editors (sub-projects 4 and 5) a place to live.

## Cart in memory

`Cart` (in `cart.h`) holds the code and the sprite sheet, flags and map as stored in the
file. Editors change the `Cart`; running copies its data into the live `gfx_*` memory and
loads its code from memory, so a game that calls `sset`/`mset` never touches what the editors see.

- `cart_read(path, &cart, err)` reads into a `Cart` (fails without changing it).
- `cart_write(path, &cart, err)` saves it.
- `cart.version` goes up on every edit. The cart is unsaved while `version != saved_version`.

## Modes

| | Run mode | Edit mode |
|---|---|---|
| Esc | pause, open the editor | back to the game: resumes if nothing was edited, restarts otherwise |
| Ctrl+R | restart | run from the start |
| Ctrl+S | save | save |

The window close button quits. With unsaved changes, the first close shows a warning on the
status bar and the second one quits.

Hot reload: if the file changes on disk and the cart has no unsaved changes, it's re-read
(and the game restarts if in run mode). With unsaved changes, the status bar says the file
changed on disk and nothing is reloaded.

## Screen layout (edit mode)

- Rows 0–6: top bar: cart file name on the left, tabs on the right (`CODE`, `SPR`).
  Unsaved carts show `*` after the name.
- Rows 7–120: the active editor.
- Rows 121–127: status bar: messages (saved, errors), otherwise editor info such as the cursor position.

Tabs switch on click or Ctrl+Tab / Ctrl+Shift+Tab.

## Input

Mouse events are converted to 128×128 screen coordinates with
`SDL_ConvertEventToRenderCoordinates`. The shell handles its own keys and passes all other
events to the active editor. SDL text input is on only in the code editor.

## Components

- `src/editor.c` / `editor.h`: mode-independent shell: tabs, top bar, status bar, routing.
  ```c
  void editor_init(Cart *cart);
  void editor_event(const SDL_Event *ev);   /* already in screen coordinates */
  void editor_draw(void);
  void editor_status(const char *msg);      /* shown for ~3 seconds */
  ```
- Each editor exposes `*_event`, `*_draw` and is given the `Cart`. In this sub-project
  they're placeholders that only draw their name.
- `src/ui.c` / `ui.h`: small drawing helpers shared by the editors (text in a box,
  button hit-testing).
- `main.c`: owns the `Cart`, the mode and the run/restart/resume logic.

## Testing

- `cart_test.c` updated for the `Cart` API.
- Manual: Esc toggles modes, tabs switch by click and Ctrl+Tab, Ctrl+S writes the file, the
  close warning shows with unsaved changes.
