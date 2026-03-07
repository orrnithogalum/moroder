from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import YTMusicServer

def get_song(server: YTMusicServer, video_id: str):
    try:
        data = server.ytm_default.get_song(video_id)

        video = data.get("videoDetails", {})
        micro = data.get("microformat", {}).get("microformatDataRenderer", {})

        # extract thumbnail
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