# LIBRARY PODCASTS
# - Fetches user podcasts using ytmusicapi
# - Requires authentication
# - Streams podcasts one by one
# - Returns status and podcast data

from typing import TYPE_CHECKING

from ytmusicapi.exceptions import YTMusicUserError

if TYPE_CHECKING:
    from main import MusicServer


async def library_podcasts(server: "MusicServer"):
    try:
        podcasts = server.ytm.get_library_podcasts() or []

        for podcast in podcasts:
            yield {
                "type": "library-podcasts-item",
                "status": "ok",
                "data": podcast
            }

        yield {
            "type": "library-podcasts-done",
            "status": "ok"
        }

    except YTMusicUserError:
        # user not logged in / auth issue → treat as empty result
        yield {
            "type": "library-podcasts-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
