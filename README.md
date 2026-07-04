<div align="center">

```
ooooo    oooo  ooooooo  oooooooooo    ooooooo  ooooooooo  ooooooooooo oooooooooo
 8888o   888 o888   888o 888    888 o888   888o 888    88o 888    88   888    888
 88 888o8 88 888     888 888oooo88  888     888 888    888 888ooo8     888oooo88
 88  888  88 888o   o888 888  88o   888o   o888 888    888 888    oo   888  88o
o88o  8  o88o  88ooo88  o888o  88o8   88ooo88  o888ooo88  o888ooo8888 o888o  88o8
```

**A YouTube Music client for the terminal.**

Album art, radio, your library, MPRIS, and Discord rich presence - in a TUI.

</div>

---

## Contents

- [What it is](#what-it-is)
- [Requirements](#requirements)
- [Building](#building)
- [Getting started](#getting-started)
- [Signing in](#signing-in)
- [Using it](#using-it)
- [Configuration](#configuration)
- [Keybinds](#keybinds)
- [Integrations](#integrations)
- [Architecture](#architecture)
- [Talking to YouTube Music](#talking-to-youtube-music)
- [Logging](#logging)
- [Troubleshooting](#troubleshooting)
- [Limitations](#limitations)

---

## What it is

Moroder is a terminal client for YouTube Music, written in C++17. It plays your
library, runs radios, searches, and renders album art as character art directly
in the terminal.

It talks to YouTube Music's internal InnerTube API through its own C++
implementation - Audio goes through libmpv, which shells out to `yt-dlp` to resolve
streams.

**Features**

- Home page built from your account's shelves, in an order you control
- Library browsing with filter chips for albums, playlists, songs, artists and podcasts
- Search across songs, albums, artists, playlists and podcasts
- Radio and autoplay, with continuations fetched ahead of the queue
- A queue you can add to, skip through and remove from
- Album covers rendered as character art
- Full MPRIS support, so `playerctl` and desktop widgets control it
- Discord rich presence, with a configurable headline field
- Runs signed-in or anonymously

---

## Requirements

### Build

| Dependency | Purpose |
|---|---|
| C++17 compiler | GCC or Clang |
| [FTXUI](https://github.com/ArthurSonzogni/FTXUI) | terminal UI |
| `ftxui-grid-container` | grid layout for the library and Quick Picks |
| libmpv | playback |
| libcurl | HTTP |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON |
| [spdlog](https://github.com/gabime/spdlog) | logging |
| [sdbus-c++](https://github.com/Kistler-Group/sdbus-cpp) | MPRIS over D-Bus |
| Discord Social SDK (`discordpp`) | rich presence |

### Runtime

| Program | Purpose | Required |
|---|---|---|
| `mpv` | plays the audio | yes |
| `yt-dlp` | resolves YouTube streams, mpv calls it internally | yes |
| `xdg-open` | opens the browser during setup | optional |

`moroder setup` checks for these and tells you what is missing.

> **Note**
> Moroder is Linux-only. MPRIS over sdbus-c++, the XDG config paths and the
> browser profile discovery all assume it.

---

## Building

```bash
git clone https://github.com/orrnithogalum/moroder.git
cd moroder
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
```

The default config is embedded into the binary at build time: `moroder.conf` is
compiled into `build/gen/defaults.hpp` as a byte array, which is what
`config.hpp` writes out on first run. If you edit the default config 
the header will be automatically regenerated.

---

## Getting started

```bash
moroder setup     # sign in, once
moroder           # start the player
```

`setup` writes two files and never touches anything else:

```
~/.config/moroder/browser.json    your YouTube Music session   (cookies)
~/.config/moroder/moroder.conf    everything else
```

To run without an account:

```bash
moroder --anonymous     # or -a
```

### Commands

| Command | What it does |
|---|---|
| `moroder` | starts the player |
| `moroder setup` | signs in and fills in the config |
| `moroder --anonymous`, `-a` | starts without an account |
| `moroder --help`, `-h` | usage |

---

## Signing in

Moroder talks to YouTube Music the way your browser does, by reusing the
session cookie your browser already holds. No password is involved and nothing
is sent anywhere except YouTube.

`moroder setup` walks through it:

1. Checks that `mpv` and `yt-dlp` are installed
2. Opens `music.youtube.com` so you can confirm you are signed in
3. Reads the cookie, automatically or by paste
4. Checks the credentials against a live request
5. Writes `browser.json` and records the paths in `moroder.conf`

Nothing is saved until step 4 passes. The new session is staged as
`browser.json.new` and only moved into place once YouTube Music has answered,
so a failed sign-in can never replace a working one. The previous session is
kept as `browser.json.bak`.

### Automatic (default)

yt-dlp reads the cookie straight out of your browser's cookie store:

```bash
moroder setup --browser librewolf
```

Supported: `firefox`, `librewolf`, `floorp`, `waterfox`, `zen`, `mercury`,
`icecat`, `tor`, `chrome`, `chromium`, `brave`, `edge`, `opera`, `vivaldi`,
`whale`.

yt-dlp only knows a fixed list of browser names, so the Firefox forks are
handed to it as `firefox:<profile path>` - the cookie store format is identical
and only the profile tells them apart. Setup finds that profile by reading
`profiles.ini` the way Firefox itself does, checking the `[Install…]` section
first (it names the profile actually in use, which often disagrees with the
`Default=1` flag left over from an older install). If there is no ini, it scans
for directories holding a `cookies.sqlite` and takes the busiest one.

If your profile lives somewhere unusual:

```bash
moroder setup --profile ~/.config/librewolf/librewolf/xxxxxxxx.default
```

### Manual paste (recommended)

More reliable, and slightly faster at runtime - see the note below.

```bash
moroder setup --manual
```

1. Open `music.youtube.com`, signed in
2. F12 → Network tab
3. Reload, filter requests for `/youtubei/`
4. Click any POST request
5. Copy everything under **Request Headers**, paste into the terminal, `Ctrl-D`

Both DevTools' "Copy All" (`name: value` lines) and "Copy as cURL"
(`-H 'name: value'`) are accepted.

> **Tip**
> The paste path captures your `x-goog-visitor-id` and real `user-agent`; the
> automatic path recovers only the cookie. Without a stored visitor id, the
> client scrapes one from the YouTube Music homepage each time it starts. If
> requests feel slow, re-run with `--manual`.

### Setup options

| Flag | Meaning |
|---|---|
| `--browser <name>` | browser to read, saved as `BROWSER` |
| `--profile <dir>` | explicit Firefox-family profile directory |
| `--manual` | skip yt-dlp, paste headers instead |
| `--no-colour` | plain output |
| `--help` | usage |

With no `--browser`, setup uses whatever `BROWSER` says in your config, so a
second run needs no flags.

---

## Using it

The interface is a sidebar, a search bar, a content area and a playback bar.

**Home** - your account's shelves, ordered by `HOME_ORDER`. Quick Picks renders
as a grid, everything else as a horizontal carousel. Signed out, this page says
so instead: search and playback still work, they are just slower.

**Library** - everything you have saved, with filter chips across the top.
Songs are excluded from the unfiltered view (there are too many) and appear
under their own chip.

**Search** - type and press <kbd>Enter</kbd>.

**Queue** - what is playing and what is next, with your queue and the radio
queue shown together.

### The search bar doubles as a library filter

On the library page, typing without pressing <kbd>Enter</kbd> narrows the grid
in place, matching titles and artist or author names. Press <kbd>Enter</kbd> and
it becomes a real search, switching to the search page. Nothing needs to be
invalidated on a keystroke: the query is folded into the grid's identity, so the
keystroke's own redraw rebuilds it.

### When something fails

Failed requests show a centred message with a **Retry** button, on home, search
and library alike. Retry is only offered for errors worth retrying - an expired
session is not one of them, so it says what to do instead.

The library is fed by five independent requests (playlists, albums, songs,
artists, podcasts). Each keeps its own error, and the page shows the first one
that failed. Retry clears all five up front and re-fires them, so the box
disappears the moment you press it.

---

## Configuration

`~/.config/moroder/moroder.conf`, written on first run.

```ini
KEY = value        # text and paths in quotes, # starts a comment
```

Delete any line to fall back to its built-in default. Unknown keys are ignored,
so a config from an older build still parses.

### Account

| Key | Default | Meaning |
|---|---|---|
| `BROWSER` | `"firefox"` | which browser holds your session |
| `YTM_COOKIES_PATH` | `"~/.config/moroder"` | directory holding `browser.json` |
| `MPV_COOKIES_PATH` | `""` | browser profile directory, for stream cookies |
| `LASTFM_API_KEY` | `""` | optional, improves album lookups |

`MPV_COOKIES_PATH` is the directory holding `cookies.sqlite`, not a cookie
file. It is combined with `BROWSER` into a `--cookies-from-browser` spec for
yt-dlp. Required for the Firefox forks, which are indistinguishable from
Firefox by name alone; stock Firefox and the Chromium browsers find their
default profile without it. Without working stream cookies, age-restricted and
premium tracks will not play.

Album names come from last.fm when a key is present and iTunes when it is not.
A free key: <https://www.last.fm/api/account/create>

### Playback

| Key | Default | Meaning |
|---|---|---|
| `FETCH_ALBUMS` | `true` | look up album details for tracks that arrive without them |
| `SEARCH_RESULT_LIMIT` | `40` | results per search |
| `RADIO_RESULT_LIMIT` | `50` | tracks per radio continuation |

`FETCH_ALBUMS` costs an extra request per track. Turn it off if queueing feels
slow.

### Appearance

| Key | Default | Meaning |
|---|---|---|
| `HOME_ORDER` | see below | home shelves in display order, case-insensitive |
| `EXTRA_BOTTOM_PADDING` | `false` | extra line under the player, for terminals that clip the last row |

```ini
HOME_ORDER = "from your library, quick picks, forgotten favorites, listen again, morning sunshine"
```

Anything not listed follows in the order YouTube Music sent it.

### Cache

Counts, not bytes. Raise them if covers you have already seen get fetched
again, lower them on a memory-constrained machine.

| Key | Default | Meaning |
|---|---|---|
| `MAX_IMAGE_CACHE_SIZE` | `800` | full-size covers held in memory |
| `MAX_RESIZED_IMAGE_CACHE_SIZE` | `3200` | resized variants |
| `MAX_IMAGE_CHAR_CACHE_SIZE` | `800000` | rendered character-art frames |
| `MAX_CONCURRENT_IMAGE_LOADS` | `200` | cover downloads running at once |

### Discord

| Key | Default | Meaning |
|---|---|---|
| `ENABLE_DISCORD_RICH_PRESENCE` | `true` | show what you are listening to |
| `RICH_PRESENCE_STATUS_LABEL` | `"song"` | headline field: `song`, `artist` or `album` |

When disabled, no Discord client is created and no background thread runs.

---

## Keybinds

A single character (`"q"`, `"/"`), a named key, or `"ctrl+<letter>"`. Named
keys: `space`, `enter`, `tab`, `escape`, `backspace`, `delete`, `up`, `down`,
`left`, `right`, `home`, `end`, `pageup`, `pagedown`. Set a binding to `""` to
unbind it.

| Key | Default | Action |
|---|---|---|
| `KEY_QUIT` | `q` | exit |
| `KEY_QUEUE_VIEW` | `a` | show the queue, while something is playing |
| `KEY_TOGGLE_SIDEBAR` | `s` | hide or show the sidebar |
| `KEY_FOCUS_SEARCH` | `f` | jump into the search bar |
| `KEY_SKIP_BACKWARD` | `z` | previous track |
| `KEY_SKIP_FORWARD` | `x` | next track |
| `KEY_TOGGLE_PAUSE` | `space` | play / pause |

These act on whatever the cursor is over:

| Key | Default | Action |
|---|---|---|
| `KEY_PLAY_NOW` | `enter` | play now, replacing the queue and starting a radio |
| `KEY_ADD_TO_QUEUE` | `d` | append to the queue, leaving playback alone |
| `KEY_REMOVE_FROM_QUEUE` | `c` | remove from the queue, queue view only |

---

## Integrations

### MPRIS

Moroder registers `org.mpris.MediaPlayer2.moroder` on the session bus and
implements the full `MediaPlayer2` and `MediaPlayer2.Player` interfaces:
`Play`, `Pause`, `PlayPause`, `Stop`, `Next`, `Previous`, `Seek`,
`SetPosition`, `Quit`, plus `Metadata`, `Position`, `PlaybackStatus`,
`LoopStatus`, `Shuffle` and the `Can*` properties.

```bash
playerctl -p moroder play-pause
playerctl -p moroder metadata
```

Media keys and desktop widgets work without configuration. Every incoming call
is logged, including refusals, so "the button did nothing" is diagnosable.

### Discord

Rich presence shows the track as a *Listening* activity with the cover as the
large image and the album as its tooltip. Timestamps track real playback
position, so the progress bar in Discord follows seeks and pauses.

`RICH_PRESENCE_STATUS_LABEL` picks which field is the headline; the other
becomes the second line, and a duplicate is dropped rather than printed twice.

---

## Architecture

```
src/
├── main.cpp                  entry point, argv dispatch, the render loop
├── setup/setup.cpp           the `moroder setup` command
├── services/
│   ├── player.cpp            orchestration, queue, state
│   ├── music.cpp             YouTube Music, models out of raw JSON
│   ├── mpv.cpp               playback via libmpv
│   ├── mpris.cpp             D-Bus / MPRIS
│   └── social.cpp            Discord rich presence
├── ytm/
│   ├── ytmusic.cpp           InnerTube client: auth, endpoints, continuations
│   ├── parsers.cpp           renderer JSON to flat structures
│   ├── nav.cpp               safe navigation into deeply nested JSON
│   ├── http.cpp              libcurl wrapper
│   ├── metadata.cpp          last.fm / iTunes album lookups
│   └── sha1.cpp              SAPISIDHASH
└── ui/components/            sidebar, search bar, content, grid, carousel,
                              chips, playback bar, error box, image view
include/
├── config/config.hpp         config parsing, writing and defaults
└── utils/utils.hpp           path, string and directory helpers
```

### How a request flows

```
UI ──▶ Player ──▶ command queue ──▶ worker thread
                                        │
                                        ▼
                                     Music ──▶ ytm::YTMusic ──▶ ytm::Http
                                        │
                                        ▼
                                  PlayerState (mutex)
                                        │
                                        ▼
                              screen.PostEvent ──▶ redraw
```

`Player` is the only thing the UI talks to. Every request becomes a command on
a queue, a single worker thread drains it, and results land in `PlayerState`
behind a mutex. The UI copies that state once per render and reads nothing else,
so no UI code ever blocks on the network.

`PlayerState` carries the user queue, the radio queue, the current track, home
and library results, and two maps that drive the interface: `flags` (`Ongoing`,
`Done`, `Error`, `True`, `False`) and `errors`, which is what turns into an
error box with a retry button.

### Threads

| Thread | Job |
|---|---|
| main | ftxui event loop and rendering |
| player worker | drains the command queue, one request at a time |
| position tick | wakes each second to update the progress bar |
| mpv event loop | translates mpv events into stream start/end/error callbacks |
| sdbus | MPRIS method calls |
| discord | runs the Social SDK callbacks |

### Queue and radio

Queueing uses mpv's own playlist so the next track is buffered before the
current one ends, which means the queue position advances whether or not you
skipped manually. When the user queue runs out, tracks come from the radio
queue; when that runs low and `autoplay` is on, a continuation is fetched
ahead of time.

---

## Talking to YouTube Music

`ytm::YTMusic` is a C++ implementation of the parts of `ytmusicapi` this client
needs. It speaks to InnerTube directly.

- **Auth** - `browser.json` is a flat object of request headers. The cookie
  must carry `__Secure-3PAPISID` (or `SAPISID`), from which each request's
  `Authorization: SAPISIDHASH` is derived.
- **Visitor id** - reused from the headers when present, otherwise scraped from
  the homepage on first use.
- **Endpoints** - `search`, `getHome`, `getAlbum`, `getPlaylist`,
  `getLibraryPlaylists` / `Albums` / `Songs` / `Artists` / `Podcasts`,
  `getWatchPlaylist` and its continuation.
- **Errors** - every failure funnels through one place and is classified as
  `Network`, `Auth`, `RateLimit`, `Server`, `Request`, `Parse`, `NotFound` or
  `Cancelled`. Only some are retryable, which is what decides whether the UI
  offers a Retry button.
- **Parsing** - renderer JSON is navigated through a path-based helper that
  returns null rather than throwing on a missing key. A malformed item is
  skipped and logged, never fatal to the rest of the page.

Album metadata that YouTube Music does not return is looked up from last.fm,
falling back to iTunes.

---

## Logging

Logs go to `$MORODER_LOG_PATH/moroder.log`, truncated at each start. mpv writes
its own log alongside it.

Default level is `info`. `HTTP:` request lines and `YTM:` per-endpoint lines are
at `debug` - raise the level in `main.cpp` to see them:

```cpp
logger->set_level(spdlog::level::debug);
```

URLs are redacted before being written: the InnerTube key and your last.fm key
both travel in the query string, so both are stripped. The session cookie is
never logged, only a short prefix and its length.

Prefixes: `MAIN`, `PLAYER`, `MUSIC`, `MPV`, `MPRIS`, `SOCIAL`, `YTM`, `HTTP`,
`CONTENT`, `GRID`, `SIDEBAR`, `METADATA`.

---

## Troubleshooting

<details>
<summary><b>The library is empty, or shows an auth error</b></summary>

Your session expired. Cookies do not last forever.

```bash
moroder setup
```
</details>

<details>
<summary><b>LibreWolf: setup finds no cookies, or the session dies constantly</b></summary>

LibreWolf clears cookies on shutdown by default, which wipes the session every
time you close it. Under **Settings → Privacy & Security**, either turn that off
or add an exception for `youtube.com`, then sign in again and re-run setup.
</details>

<details>
<summary><b>Setup cannot find my browser profile</b></summary>

Point it at the directory holding `cookies.sqlite`:

```bash
moroder setup --profile ~/.mozilla/firefox/xxxxxxxx.default-release
```

If yt-dlp cannot read your browser at all, use `moroder setup --manual`.
</details>

<details>
<summary><b>Some tracks refuse to play</b></summary>

Age-restricted and premium tracks need stream cookies. Check `MPV_COOKIES_PATH`
points at your browser profile directory and `BROWSER` names the right browser.
The log line to look for is `MPV: stream cookies from …`; if it says
`stream cookies disabled`, it tells you which of the two is wrong.
</details>

<details>
<summary><b>Everything is slow</b></summary>

If setup used the automatic path, your `browser.json` has no
`x-goog-visitor-id`, so one is scraped from the homepage on each start. Re-run
with `moroder setup --manual` and paste the headers.
</details>

<details>
<summary><b>Covers do not render</b></summary>

Character-art covers need a terminal with truecolour support. Check
`$COLORTERM` is `truecolor` or `24bit`.
</details>

<details>
<summary><b>playerctl does not see it</b></summary>

Check the service registered:

```bash
busctl --user list | grep moroder
```

If it is missing, the log will say why - `MPRIS: could not claim …` names the
D-Bus error. A second instance cannot claim the name while the first holds it.
</details>

---

## Limitations

- Linux only
- Playlist editing, likes and subscriptions are read-only
- The **New playlist** and **Sign in** sidebar buttons are not wired up yet
- Anonymous mode has no library and no personalised home
- One instance at a time owns the MPRIS name

---

## Credits

The MPRIS implementation is adapted from
[mpris-server](https://github.com/chrg127/mpris-server).

The YouTube Music client is a C++ reimplementation of the endpoints and parsers
in [ytmusicapi](https://github.com/sigma67/ytmusicapi).

Named after Giorgio.
