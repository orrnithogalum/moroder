# ALBUM
# - Get the list of songs from an album using ytmusicapi
# - Streams results one by one to avoid large payloads
# - Returns status and track data

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer


async def album(server: "MusicServer", album_id: str):
    try:
        data = server.ytm.get_album(album_id)

        tracks = data.get("tracks", []) or []

        for track in tracks:
            yield {
                "type": "album-track",
                "status": "ok",
                "data": track
            }

        yield {
            "type": "album-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
