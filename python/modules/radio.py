# RADIO
# - Get the list, for a song, of similar songs on YouTube Music using ytmusicapi
# - Limits results to prevent large payloads that could overflow the buffer
# - Returns status and a list of results

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer


async def radio(server: MusicServer, id: str, limit:int):
    try:
        data = server.ytm.get_watch_playlist(videoId=id, radio=True, limit=limit)

        tracks = data.get("tracks", [])

        if not tracks:
            tracks = []

        for track in tracks[:limit]:
            yield {
                "type": "radio-track",
                "status": "ok",
                "data": track
            }

        yield {
            "type": "radio-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
