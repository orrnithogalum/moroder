# GET_SONG
# - Fetches full song details from ytmusicapi using video_id
# - Filters videoDetails and microformatDataRenderer from the full JSON response

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import YTMusicServer

def get_song(server: YTMusicServer, video_id: str):
    # get_song
    # - This filtering is necessary because the full ytmusicapi response can be very large and overflow the cpp buffer
    # - Extracts key fields: videoId, title, lengthSeconds, viewCount, thumbnail, publishDate, uploadDate, category
    # - Returns a simplified JSON dictionary with status and song data
    try:
        data = server.ytm_default.get_song(video_id)

        video = data.get("videoDetails", {})
        micro = data.get("microformat", {}).get("microformatDataRenderer", {})

        # extract thumbnail list
        thumbnails = video.get("thumbnail", {}).get("thumbnails", [])

        result = {
            "videoDetails": {
                "videoId": video.get("videoId"),
                "title": video.get("title"),
                "lengthSeconds": video.get("lengthSeconds"),
                "viewCount": video.get("viewCount"),
                "thumbnail": {
                    "thumbnails": thumbnails
                }
            },
            "microformat": {
                "microformatDataRenderer": {
                    "publishDate": micro.get("publishDate"),
                    "uploadDate": micro.get("uploadDate"),
                    "category": micro.get("category")
                }
            }
        }

        return {
            "status": "ok",
            "song": result
        }

    except Exception as e:
        return {
            "status": "error",
            "message": str(e)
        }