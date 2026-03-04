from ytmusicapi import YTMusic

def search_song(ytm: YTMusic, query):
    try:
        results = ytm.search(
            query,
            filter="songs",
            limit=10
        )

        songs = [
            {
                "title": r["title"],
                "artist": r["artists"][0]["name"] if r.get("artists") else "",
                "videoId": r["videoId"],
            }
            for r in results
            if "videoId" in r
        ]

        return {"status": "ok", "results": songs}

    except Exception as e:
        return {"status": "error", "message": str(e)}