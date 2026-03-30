# RADIO
# - Get the list, for a song, of similar songs on YouTube Music using ytmusicapi
# - Limits results to prevent large payloads that could overflow the buffer
# - Returns status and a list of results

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer

from ytmusicapi.parsers.watch import parse_watch_playlist
from ytmusicapi.parsers.watch import TAB_CONTENT
from ytmusicapi.parsers.watch import nav

def _build_body(video_id: str, radio: bool = True) -> dict:
    body = {
        "enablePersistentPlaylistPanel": True,
        "isAudioOnly": True,
        "tunerSettingValue": "AUTOMIX_SETTING_NORMAL",
        "videoId": video_id,
        "playlistId": "RDAMVM" + video_id,
    }
    if radio:
        body["params"] = "wAEB"
    return body

async def radio(server: MusicServer, video_id: str, limit: int = 25):
    try:
        body = _build_body(video_id, radio=True)
        server.radio_sessions[video_id] = body

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
        ])

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
        for track in tracks[:limit]:
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
            "id": video_id
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }


async def radio_next(server: MusicServer, video_id: str, ctoken: str, limit: int = 25):
    try:
        body = server.radio_sessions.get(video_id) or {}

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
