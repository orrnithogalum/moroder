# RADIO
# - Get the list, for a song, of similar songs on YouTube Music using ytmusicapi
# - Limits results to prevent large payloads that could overflow the buffer
# - Returns status and a list of results

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer

from ytmusicapi.parsers.playlists import validate_playlist_id
from ytmusicapi.parsers.watch import parse_watch_playlist
from ytmusicapi.parsers.watch import TAB_CONTENT
from ytmusicapi.parsers.watch import nav

def _build_body(radio: bool = True) -> dict:
    body = {
        "enablePersistentPlaylistPanel": True,
        "isAudioOnly": True,
        "tunerSettingValue": "AUTOMIX_SETTING_NORMAL",
    }

    if radio:
        body["params"] = "wAEB"

    return body

async def radio(server: MusicServer, video_id: str | None, playlist_id: str | None, limit: int = 25):
    try:
        if not video_id and not playlist_id:
            raise Exception("Invalid arguments")

        body = _build_body(radio=True)

        if video_id:
            body["videoId"] = video_id
            body["playlistId"] = "RDAMVM" + video_id

        elif playlist_id:
            validated_playlist_id = validate_playlist_id(playlist_id)
            body["playlistId"] = validated_playlist_id

        server.radio_sessions[video_id if video_id else playlist_id] = body

        response = server.ytm._send_request("next", body)

        watch_next = nav(response, [
            "contents",
            "singleColumnMusicWatchNextResultsRenderer",
            "tabbedRenderer",
            "watchNextTabbedResultsRenderer",
        ])

        results = nav(watch_next, [
            *TAB_CONTENT,
            "musicQueueRenderer",
            "content",
            "playlistPanelRenderer"
        ], False)

        tracks = parse_watch_playlist(results["contents"]) or []

        # Extract continuation token
        ctoken = None
        if "continuations" in results:
            cont_key = (
                "nextRadioContinuationData"
                if not body.get("playlistId", "").startswith("PL")
                else "nextContinuationData"
            )
            ctoken = results["continuations"][0].get(cont_key, {}).get("continuation")

        # Stream tracks
        start = 1 if video_id else 0
        for track in tracks[start:limit+start]:
            yield {
                "type": "radio-track",
                "status": "ok",
                "data": track
            }

        # Final message with continuation
        yield {
            "type": "radio-done",
            "status": "ok",
            "continuation": ctoken,
            "id": video_id if video_id else playlist_id
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }


async def radio_next(server: MusicServer, video_id: str | None, playlist_id: str | None, ctoken: str, limit: int = 25):
    try:
        body = server.radio_sessions.get(video_id if video_id else playlist_id) or {}

        additional_params = f"&ctoken={ctoken}&continuation={ctoken}"
        response = server.ytm._send_request("next", body, additional_params)

        if "continuationContents" not in response:
            yield {
                "type": "radio-done",
                "status": "ok",
                "continuation": None,
                "id": video_id
            }
            return

        results = response["continuationContents"]["playlistPanelContinuation"]

        tracks = parse_watch_playlist(
            results.get("contents", results.get("items", []))
        ) or []

        # Extract next continuation token
        next_ctoken = None
        if "continuations" in results:
            for key in ["nextRadioContinuationData", "nextContinuationData"]:
                if key in results["continuations"][0]:
                    next_ctoken = results["continuations"][0][key]["continuation"]
                    break

        # Stream tracks
        for track in tracks[:limit]:
            yield {
                "type": "radio-track",
                "status": "ok",
                "data": track
            }

        # Final message with next continuation
        yield {
            "type": "radio-done",
            "status": "ok",
            "continuation": next_ctoken,
            "id": video_id
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
