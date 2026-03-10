# GET_SONG
# - Fetches full song details from iTunes Search API using song title and song artist

from typing import TYPE_CHECKING
import requests

if TYPE_CHECKING:
    from server import YTMusicServer


def get_song(server: YTMusicServer, song_title: str, song_artist: str):
    # get_song
    # - This filtering is necessary because the full iTunes API response can contain many results
    # - Extracts key fields: album title.
    # - Returns a simplified JSON dictionary with status and data
    try:
        url = "https://itunes.apple.com/search"

        params = {
            "term": f"{song_title} {song_artist}",  # search by song and artist
            "entity": "song",
            "country": "US",
            "limit": 10
        }

        headers = {
            "User-Agent": "moroder/1.0 (LittleBigOwI@github.com)"
        }

        response = requests.get(url, params=params, headers=headers)
        data = response.json()

        if not data.get("results"):
            raise Exception("Song not found")

        # Extract album title from the first matching result
        first_result = data["results"][0]
        album_title = first_result.get("collectionName")

        if not album_title:
            raise Exception("No valid album found in the first result")

        result = {
            "album": album_title,
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