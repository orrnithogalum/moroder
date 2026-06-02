# LIBRARY PLAYLISTS
# - Fetches user playlists using ytmusicapi
# - Requires authentication
# - Streams playlists one by one
# - Returns status and playlist data

from typing import TYPE_CHECKING

from ytmusicapi.exceptions import YTMusicUserError

if TYPE_CHECKING:
    from main import MusicServer


async def library_playlists(server: "MusicServer"):
    try:
        playlists = server.ytm.get_library_playlists(limit=None) or []

        for playlist in playlists:
            yield {
                "type": "library-playlists-item",
                "status": "ok",
                "data": playlist
            }

        yield {
            "type": "library-playlists-done",
            "status": "ok"
        }

    except YTMusicUserError:
        # user not logged in / auth issue → treat as empty result
        yield {
            "type": "library-playlists-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
