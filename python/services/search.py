def search(server, query):
    result_limit = 10
    try:
        results = server.ytm_default.search(
            query,
            limit=result_limit
        )

        results = results[:result_limit]

        # songs = [
        #     {
        #         "title": r["title"],
        #         "artist": r["artists"][0]["name"] if r.get("artists") else "",
        #         "videoId": r["videoId"],
        #     }
        #     for r in results
        #     if "videoId" in r
        # ]

        return {"status": "ok", "results": results}

    except Exception as e:
        return {"status": "error", "message": str(e)}