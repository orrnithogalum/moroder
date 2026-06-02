# LIBRARY SONGS
# - Fetches user songs using ytmusicapi
# - Requires authentication
# - Streams songs one by one
# - Returns status and song data

from typing import TYPE_CHECKING

from ytmusicapi.exceptions import YTMusicUserError

if TYPE_CHECKING:
    from main import MusicServer


async def library_songs(server: "MusicServer"):
    try:
        songs = server.ytm.get_library_songs(limit=None) or []

        for song in songs:
            yield {
                "type": "library-songs-item",
                "status": "ok",
                "data": song
            }

        yield {
            "type": "library-songs-done",
            "status": "ok"
        }

    except YTMusicUserError:
        # user not logged in / auth issue → treat as empty result
        yield {
            "type": "library-songs-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
