# PLAYLIST
# - Get the list of songs from a playlist using ytmusicapi
# - Streams results one by one to avoid large payloads
# - Returns status and track data

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer


async def playlist(server: "MusicServer", playlist_id: str):
    try:
        data = server.ytm.get_playlist(playlist_id, limit=None)

        tracks = data.get("tracks", []) or []

        for track in tracks:
            yield {
                "type": "playlist-track",
                "status": "ok",
                "data": track
            }

        yield {
            "type": "playlist-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
