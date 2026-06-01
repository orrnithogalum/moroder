# LIBRARY ALBUMS
# - Fetches user albums using ytmusicapi
# - Requires authentication
# - Streams albums one by one
# - Returns status and album data

from typing import TYPE_CHECKING

from ytmusicapi.exceptions import YTMusicUserError

if TYPE_CHECKING:
    from main import MusicServer


async def library_albums(server: "MusicServer"):
    try:
        albums = server.ytm.get_library_albums() or []

        for album in albums:
            yield {
                "type": "library-albums-item",
                "status": "ok",
                "data": album
            }

        yield {
            "type": "library-albums-done",
            "status": "ok"
        }

    except YTMusicUserError:
        # user not logged in / auth issue → treat as empty result
        yield {
            "type": "library-albums-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
