/* SETUP
- Implements `moroder setup`, the one command a new user has to run
- Signs in to YouTube Music by borrowing the browser's session cookie, either
  through yt-dlp or by pasting the request headers from DevTools
- Writes browser.json next to the config and records both cookie paths in
  moroder.conf, so the app itself never has to guess where anything lives
- Config::app_name must be set before run() is called
*/

#pragma once

namespace setup {

// Returns a process exit code.
int run(int argc, char* argv[]);

}
