# LIBRARY ARTISTS
# - Fetches user artists using ytmusicapi
# - Requires authentication
# - Streams artists one by one
# - Returns status and artist data

from typing import TYPE_CHECKING

from ytmusicapi.exceptions import YTMusicUserError

if TYPE_CHECKING:
    from main import MusicServer


async def library_artists(server: "MusicServer"):
    try:
        artists = server.ytm.get_library_artists(limit=None) or []

        for artist in artists:
            yield {
                "type": "library-artists-item",
                "status": "ok",
                "data": artist
            }

        yield {
            "type": "library-artists-done",
            "status": "ok"
        }

    except YTMusicUserError:
        # user not logged in / auth issue → treat as empty result
        yield {
            "type": "library-artists-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
