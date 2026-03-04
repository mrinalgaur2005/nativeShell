# nativeShell

A keyboard-driven tiling web browser. Open multiple websites side by side, navigate between them without touching the mouse, and keep everything organized with vim-style controls.

Built from scratch in C with SDL2 and WebKit.

---

## Getting Started

### Install dependencies

On Debian / Ubuntu:

```bash
sudo apt install build-essential pkg-config \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
  libgtk-3-dev libwebkit2gtk-4.1-dev
```

### Build and run

```bash
make
make run
```

You'll see a single browser pane with your startup page loaded. From here, everything is controlled through the keyboard.

---

## The Basics

nativeShell has three modes, like vim:

**WM mode** is the default. You move between panes, split the screen, close panes, and resize things. Think of it as the window manager.

**View mode** is where you actually use the browser — type in search bars, click links, scroll pages. Press `i` to enter it, `Esc` to leave.

**Command mode** opens a prompt at the bottom of the screen. Press `Shift + ;` (that's `:`) to open it, type a command, hit `Enter`.

---

## Navigating

Everything starts in WM mode. Here's how you move around:

| Key | What it does |
|-----|-------------|
| `h` | Focus the pane to the left |
| `j` | Focus the pane below |
| `k` | Focus the pane above |
| `l` | Focus the pane to the right |

The focused pane has a visible accent border. When you press `i`, that pane becomes interactive — you can type, click, scroll, use the web like normal. Press `Esc` to go back to WM mode.

---

## Splitting and Closing

You can tile your screen however you want:

| Key | What it does |
|-----|-------------|
| `v` | Split the focused pane vertically (side by side) |
| `s` | Split the focused pane horizontally (stacked) |
| `x` | Close the focused pane |
| `Shift+H` | Detach the webview from the pane (hides it without closing) |

Split as many times as you like. Each new pane starts empty — use `:open` or press `o` to load a page.

---

## Resizing

Hold `Ctrl` and use the directional keys to resize splits:

| Key | What it does |
|-----|-------------|
| `Ctrl+h` | Shrink split leftward |
| `Ctrl+l` | Expand split rightward |
| `Ctrl+k` | Shrink split upward |
| `Ctrl+j` | Expand split downward |

You can also drag split borders with the mouse.

---

## Commands

Press `Shift + ;` to open the command bar, then type:

| Command | What it does |
|---------|-------------|
| `:open youtube.com` | Open a URL in the focused pane |
| `:open how to cook pasta` | Search Google (anything that isn't a URL) |
| `:new github.com` | Open a URL in a new split pane |
| `:only` | Close every pane except the focused one |
| `:clear` | Reset to a single blank pane |
| `:help` | Quick reference of all commands |

Aliases: `:o` for open, `:s` for search, `:n` for new, `:h` for help.

Press `Tab` to autocomplete commands and arguments. Press `Esc` to cancel.

---

## Tab View

When you hide or detach a webview, it doesn't disappear. It goes to the tab list — a registry of every webview that's still alive.

Press `t` in WM mode to open the tab view:

| Key | What it does |
|-----|-------------|
| `j` / `k` | Move selection down / up |
| `y` or `Enter` | Attach the selected webview to a pane |
| `x` | Close the selected webview permanently |
| `Esc` | Exit the tab view |

You can prefix `j` or `k` with a number (e.g. `5j`) to jump multiple entries.

---

## Profiles

Profiles let you run completely separate browser environments — different bookmarks, cookies, history, layout, and settings.

| Command | What it does |
|---------|-------------|
| `:profile work` | Switch to the "work" profile |
| `:profile` | Show which profile is active |
| `:profiles` | List all available profiles |

Each profile stores its data independently:

```
~/.config/nativeshell/profiles/<name>/config.json      # settings
~/.local/share/nativeshell/profiles/<name>/session.json # saved layout
~/.local/share/nativeshell/profiles/<name>/webkit-data/ # cookies, cache
```

The first time you switch to a new profile name, it's created automatically. The default profile is called `default`.

Profile info is shown directly in the command bar — no tabs get disrupted.

---

## Configuration

Each profile has its own `config.json`. If the file doesn't exist, a default one is created automatically.

```json
{
  "keys": {
    "h": "focus_left",
    "l": "focus_right",
    "k": "focus_up",
    "j": "focus_down",
    "v": "split_vertical",
    "s": "split_horizontal",
    "x": "close_pane",
    "o": "open_webview",
    ";": "enter_cmd",
    "u": "web_back",
    "r": "web_reload",
    ".": "web_stop",
    "t": "tab_enter"
  },
  "startup": {
    "url": "https://www.youtube.com",
    "restore_session": true
  }
}
```

**`keys`** — Map single characters to actions. You can rebind anything.

**`startup.url`** — The page that loads when there's no session to restore.

**`startup.restore_session`** — Set to `false` to always start fresh instead of restoring your last layout.

### All available actions

| Action | Description |
|--------|-------------|
| `focus_left` / `right` / `up` / `down` | Move focus between panes |
| `split_vertical` / `split_horizontal` | Split the focused pane |
| `close_pane` | Close the focused pane |
| `hide_webview` | Detach the webview without closing it |
| `open_webview` | Load the startup URL in the focused pane |
| `enter_cmd` | Open the command bar |
| `web_back` / `web_reload` / `web_stop` | Browser navigation |
| `tab_enter` | Open the tab view |

---

## Sessions

Your layout is saved automatically:

- **Autosave** every 30 seconds
- **On exit** via `Ctrl+C` or window close

Next time you launch (with `restore_session: true`), the exact layout and every open tab are restored.

If the session file is corrupted or missing, nativeShell silently falls back to a single pane with your startup URL. Nothing crashes.

---

## Quick Reference

```
MODE SWITCHING
  Esc .............. Return to WM mode
  i ................ Enter View mode (interact with browser)
  Shift+; (:) ..... Enter Command mode

WM MODE
  h j k l ......... Move focus
  Ctrl+h/j/k/l .... Resize splits
  v / s ............ Split vertical / horizontal
  x ................ Close pane
  Shift+H .......... Hide webview
  o ................ Open webview
  t ................ Tab view
  u ................ Back
  r ................ Reload
  . ................ Stop loading

COMMANDS
  :open <url|query>     Open or search
  :new <url|query>      Open in new pane
  :only                 Keep only focused pane
  :profile <name>       Switch profile
  :profiles             List profiles
  :help                 Show help
```

---

## Under the Hood

nativeShell is a single-process application written in C. It uses SDL2 for the window and input, GTK3 for offscreen widget hosting, and WebKit2GTK for rendering web content. Each webview renders into a Cairo surface that gets uploaded to an SDL texture every frame.

The layout is a binary tree of splits and leaves. Splits animate smoothly when resizing. The render effects layer adds gaps between panes, rounded corners, soft shadows, opacity transitions on focus change, and fade-in/fade-out animations when panes appear or disappear — all without modifying the layout algorithm.

```
src/
├── core/       Entry point, focus, sessions, profiles
├── config/     JSON config and keybind resolution
├── layout/     Binary tree layout engine
├── render/     SDL2 rendering + visual effects
├── command/    Command bar, parsing, autocomplete
└── view/
    ├── pane/   Pane containers
    ├── web/    WebKit integration
    └── tab/    Tab list view
```

### Platform

Linux with X11. Works under WSL2 with an X server. The `make run` target sets the necessary environment variables for X11/WebKit compatibility.

---

## License

See repository for license details.
