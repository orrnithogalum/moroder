# Moroder

A terminal music player for YouTube Music, with album art, a full queue view, and MPRIS integration.

Moroder is built on [FTXUI](https://github.com/ArthurSonzogni/FTXUI) and plays through [mpv](https://mpv.io/). It renders thumbnails inline, so the home feed, search results and queue all look like a music app rather than a list of filenames.

<!-- TODO: screenshot or asciinema cast here. This is the single highest-value
     thing you can add to this README — a TUI music player is entirely sold on
     what it looks like. -->

## Features

- **Home feed** — your YouTube Music recommendations, in carousels and a Quick Picks grid, with the category order configurable
- **Search** — songs, albums, artists, playlists, podcasts and episodes
- **Queue** — a dedicated queue view with the current album art alongside it; skip to any row or remove entries in place
- **Autoplay radio** — when the queue runs dry, Moroder keeps going with a radio continuation seeded from what you were listening to
- **Library playlists** — your saved playlists in the sidebar, playable or queueable directly
- **MPRIS** — play/pause, next/previous, seek and metadata work with `playerctl`, media keys, and desktop widgets
- **Rich presence** — shows what you're listening to on Discord
- **Configurable** — keybinds, home category order and result limits all live in a plain-text config file

## Requirements

- A terminal that supports your image protocol of choice <!-- TODO: name which — kitty, sixel, iTerm2? -->
- `mpv`
- Python 3 <!-- TODO: minimum version -->
- A YouTube Music account, exported as cookies (see [Authentication](#authentication))

Build-time dependencies:

- A C++20 compiler
- CMake <!-- TODO: minimum version -->
- FTXUI, spdlog, sdbus-c++ <!-- TODO: note which are vendored/fetched vs. expected on the system -->

## Building

```sh
git clone https://github.com/TODO/moroder.git
cd moroder
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary lands in `build/moroder`. <!-- TODO: confirm path, and add an install target / AUR / nixpkgs line if you have one -->

## Authentication

Moroder talks to YouTube Music through your browser cookies. Export them from a
logged-in session and point the config at the file:

<!-- TODO: spell out the actual export steps — which browser extension, what
     format (Netscape cookies.txt?), and whether YTM_COOKIES_PATH and
     MPV_COOKIES_PATH can be the same file. -->

```
YTM_COOKIES_PATH="~/.config/moroder/cookies.txt"
MPV_COOKIES_PATH="~/.config/moroder/cookies.txt"
```

The indicator in the top right of the search bar turns green once you're
authenticated.

## Configuration

On first run Moroder writes a commented default config to
`~/.config/moroder/moroder.conf`. Values are `KEY=value`, one per line, with
`#` starting a comment. Strings and paths are quoted; `~` is expanded.

| Key | Default | What it does |
|---|---|---|
| `LASTFM_API_KEY` | — | Last.fm API key <!-- TODO: what it's used for — scrobbling, artist art, both? --> |
| `PYTHON_PATH` | — | Where Moroder keeps its Python environment |
| `YTM_COOKIES_PATH` | — | Cookies used for the YouTube Music API |
| `MPV_COOKIES_PATH` | — | Cookies passed to mpv for stream playback |
| `SEARCH_RESULT_LIMIT` | `20` | How many results a search returns |
| `RADIO_RESULT_LIMIT` | `20` | How many tracks each radio continuation fetches |
| `FETCH_ALBUMS` | `true` | Fetch full album data when queueing a song. Richer metadata, slower queueing |
| `HOME_ORDER` | see below | Order of home page categories |

### Home category order

The API returns your home feed in whatever order it likes. `HOME_ORDER` overrides
that. Categories are matched case-insensitively, and anything not listed appears
after the ones that are, in the order the API gave them.

```
HOME_ORDER="quick picks, listen again, forgotten favorites, from your library, morning sunshine"
```

### Keybinds

Every binding is a single printable character (`"q"`, `"/"`, `"*"`), a named key,
or `"ctrl+<letter>"`. Set a value to `""` to unbind the action.

Named keys: `space`, `enter`, `tab`, `escape`, `backspace`, `delete`, `up`,
`down`, `left`, `right`, `home`, `end`, `pageup`, `pagedown`.

| Key | Default | Action |
|---|---|---|
| `KEY_QUIT` | `q` | Exit Moroder |
| `KEY_QUEUE_VIEW` | `a` | Jump to the queue view (only while something is playing) |
| `KEY_TOGGLE_SIDEBAR` | `s` | Hide or show the playlist sidebar |
| `KEY_FOCUS_SEARCH` | `f` | Move the cursor into the search bar |
| `KEY_SKIP_BACKWARD` | `z` | Previous track |
| `KEY_SKIP_FORWARD` | `x` | Next track |
| `KEY_TOGGLE_PAUSE` | `space` | Play / pause |
| `KEY_PLAY_NOW` | `enter` | Play the highlighted item now, replacing the queue and starting a radio from it |
| `KEY_ADD_TO_QUEUE` | `d` | Append the highlighted item to the queue, leaving playback alone |
| `KEY_REMOVE_FROM_QUEUE` | `c` | Remove the highlighted row (queue view only) |

While the search bar has focus every other binding is inactive, so you can type
freely. Press <kbd>Enter</kbd> to search, or <kbd>Tab</kbd> to leave the field.

## Usage

Move around with the arrow keys. <kbd>Enter</kbd> plays what's under the cursor
and starts a radio from it; `d` adds it to the queue instead and leaves whatever
is playing alone. That distinction holds everywhere — home, search results and
the sidebar.

In the queue view, <kbd>Enter</kbd> skips to a row and `c` removes it. The
divider marks where your queue ends and autoplay takes over.

## Troubleshooting

**Logs** live at `TODO/moroder.log` and are flushed on every info-level message,
so they're useful even after a crash. <!-- TODO: fill in the real MORODER_LOG_PATH -->

**No album art** — your terminal probably doesn't support the image protocol
Moroder uses. Everything else works fine without it.

**Login indicator stays red** — your cookies have expired or weren't exported
from a logged-in session. Re-export and restart.

**A keybind does nothing** — check the config for two actions bound to the same
key. Duplicates aren't detected; the first match wins.

## Contributing

Issues and pull requests are welcome. <!-- TODO: link a CONTRIBUTING.md, or say a
     word here about build setup / code style if you'd rather keep it inline. -->

## License

TODO
