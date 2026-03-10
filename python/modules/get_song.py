# GET_SONG
# - Fetches full song details from musicbrainz api using song title and song artist

from typing import TYPE_CHECKING
import requests

if TYPE_CHECKING:
    from server import YTMusicServer


def get_song(server: YTMusicServer, song_title: str, song_artist: str):
    # get_song
    # - This filtering is necessary because the full musicbrainz api response can be very large and overflow the cpp buffer
    # - Extracts key fields: album title.
    # - Returns a simplified JSON dictionary with status and data
    try:
        url = "https://musicbrainz.org/ws/2/recording/"

        params = {
            "query": f'recording:"{song_title}" AND artist:"{song_artist}"',
            "fmt": "json",
            "limit": 20
        }

        headers = {
            "User-Agent": "moroder/1.0 (LittleBigOwI@github.com)"
        }

        response = requests.get(url, params=params, headers=headers)
        data = response.json()

        if not data.get("recordings"):
            raise Exception("Song not found")

        album_title = None
        # publish_date = None
        # track_count = None
        # length_seconds = None

        for rec in data["recordings"]:

            # length_seconds = int(rec.get("length", 0) / 1000) if rec.get("length") else None

            for release in rec.get("releases", []):

                rg = release.get("release-group", {})

                primary_type = rg.get("primary-type")
                secondary_types = rg.get("secondary-types", [])

                release_artist = release.get("artist-credit", [{}])[0].get("name", "")

                if primary_type != "Album":
                    continue

                if secondary_types:
                    continue

                if release_artist.lower() != song_artist.lower():
                    continue

                album_title = rg.get("title")
                # publish_date = release.get("date")

                # media = release.get("media", [])
                # if media:
                #     track_count = media[0].get("track-count")

                break

            if album_title:
                break

        if not album_title:
            raise Exception("No valid album release found")

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