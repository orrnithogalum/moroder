# GET_SONG
# - Fetches full song details from iTunes Search API using song title and song artist

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer

import requests
import os


def get_song(server: "MusicServer", song_title: str, song_artist: str):
    api_key = os.getenv("LASTFM_API_KEY")

    headers = {"User-Agent": "moroder/1.0 (LittleBigOwI@github.com)"}

    if api_key:
        try:
            url = "http://ws.audioscrobbler.com/2.0/"
            params = {
                "method": "track.getInfo",
                "api_key": api_key,
                "artist": song_artist,
                "track": song_title,
                "format": "json",
            }

            response = requests.get(url, params=params, headers=headers)
            data = response.json()

            if "error" not in data:
                track = data.get("track")
                if track:
                    album = track.get("album")
                    if album and album.get("title"):
                        return {
                            "status": "ok",
                            "song": {"album": album["title"]},
                            "source": "lastfm",
                        }

        except Exception:
            pass

    try:
        url = "https://itunes.apple.com/search"
        params = {
            "term": f"{song_title} {song_artist}",
            "entity": "song",
            "country": "US",
            "limit": 10,
        }

        response = requests.get(url, params=params, headers=headers)
        data = response.json()

        if not data.get("results"):
            raise Exception("Song not found")

        first_result = data["results"][0]
        album_title = first_result.get("collectionName")

        if not album_title:
            raise Exception("No valid album found")

        return {
            "status": "ok",
            "song": {"album": album_title},
            "source": "itunes",
        }

    except Exception as e:
        return {"status": "error", "message": str(e)}
