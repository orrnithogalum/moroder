# RADIO
# - Get the list, for a song or playlist, of similar songs using ytmusicapi
# - Limits results to prevent large payloads that could overflow the buffer
# - Returns status and a list of results

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer

from ytmusicapi.exceptions import YTMusicServerError
from ytmusicapi.parsers.playlists import validate_playlist_id
from ytmusicapi.parsers.watch import (
    NAVIGATION_PLAYLIST_ID,
    TAB_CONTENT,
    nav,
    parse_watch_playlist,
)


async def radio(
    server: MusicServer, videoId: str | None, playlistId: str | None, limit: int = 50
):
    try:
        body = {
            "enablePersistentPlaylistPanel": True,
            "isAudioOnly": True,
            "tunerSettingValue": "AUTOMIX_SETTING_NORMAL",
        }

        if videoId:
            body["videoId"] = videoId
            if not playlistId:
                playlistId = "RDAMVM" + videoId

            body["watchEndpointMusicSupportedConfigs"] = {
                "watchEndpointMusicConfig": {
                    "hasPersistentPlaylistPanel": True,
                    "musicVideoType": "MUSIC_VIDEO_TYPE_ATV",
                }
            }
            body["params"] = "wAEB"

        if playlistId:
            playlist_id = validate_playlist_id(playlistId)
            body["playlistId"] = playlist_id

        endpoint = "next"
        response = server.ytm._send_request(endpoint, body)

        server.radio_sessions[videoId if videoId else playlistId] = body

        watchNextRenderer = nav(
            response,
            [
                "contents",
                "singleColumnMusicWatchNextResultsRenderer",
                "tabbedRenderer",
                "watchNextTabbedResultsRenderer",
            ],
        )

        results = nav(
            watchNextRenderer,
            [*TAB_CONTENT, "musicQueueRenderer", "content", "playlistPanelRenderer"],
            True,
        )

        if not results:
            msg = "No content returned by the server."
            if playlistId:
                msg += f"\nEnsure you have access to {playlistId} - a private playlist may cause this."
            raise YTMusicServerError(msg)

        playlist = next(
            filter(
                bool,
                map(
                    lambda x: nav(
                        x, ["playlistPanelVideoRenderer", *NAVIGATION_PLAYLIST_ID], True
                    ),
                    results["contents"],
                ),
            ),
            None,
        )
        tracks = parse_watch_playlist(results["contents"])

        if videoId:
            tracks = tracks[1:]

        # Extract continuation token
        ctoken = None
        if "continuations" in results:
            cont_key = (
                "nextRadioContinuationData"
                if not body.get("playlistId", "").startswith(("PL", "OLA"))
                else "nextContinuationData"
            )
            ctoken = results["continuations"][0].get(cont_key, {}).get("continuation")

        # Stream tracks
        for track in tracks:
            yield {"type": "radio-track", "status": "ok", "data": track}

        # Final message with continuation
        yield {
            "type": "radio-done",
            "status": "ok",
            "continuation": ctoken,
            "id": videoId if videoId else playlist,
        }

    except Exception as e:
        yield {"type": "error", "message": str(e)}


async def radio_next(
    server: MusicServer,
    video_id: str | None,
    playlist_id: str | None,
    ctoken: str,
    limit: int = 50,
):
    try:
        body = server.radio_sessions.get(video_id if video_id else playlist_id) or {}

        additional_params = f"&ctoken={ctoken}&continuation={ctoken}"
        response = server.ytm._send_request("next", body, additional_params)

        if "continuationContents" not in response:
            yield {
                "type": "radio-done",
                "status": "ok",
                "continuation": None,
                "id": video_id,
            }
            return

        results = response["continuationContents"]["playlistPanelContinuation"]

        tracks = (
            parse_watch_playlist(results.get("contents", results.get("items", [])))
            or []
        )

        # Extract next continuation token
        next_ctoken = None
        if "continuations" in results:
            for key in ["nextRadioContinuationData", "nextContinuationData"]:
                if key in results["continuations"][0]:
                    next_ctoken = results["continuations"][0][key]["continuation"]
                    break

        # Stream tracks
        for track in tracks[:limit]:
            yield {"type": "radio-track", "status": "ok", "data": track}

        # Final message with next continuation
        yield {
            "type": "radio-done",
            "status": "ok",
            "continuation": next_ctoken,
            "id": video_id,
        }

    except Exception as e:
        yield {"type": "error", "message": str(e)}
