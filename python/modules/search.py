# SEARCH
# - Performs a search on YouTube Music using ytmusicapi
# - Limits results to prevent large payloads that could overflow the buffer
# - Returns status and a list of results

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer


def search(server: MusicServer, query: str, limit: int):
    # search
    # - Uses server.ytm to perform a search
    # - limit caps the number of results returned
    # - Returns a dictionary with status and results
    try:
        results = server.ytm.search(query, limit=limit)

        # slice to ensure we never exceed the limit
        results = results[:limit]

        return {"status": "ok", "results": results, "authed": server.logged_in}

    except Exception as e:
        return {"status": "error", "message": str(e)}


async def search_stream(server: MusicServer, query: str, limit: int):
    try:
        results = server.ytm.search(query, limit=limit)

        for item in results[:limit]:
            yield {
                "type": "search-result",
                "status": "ok",
                "data": item
            }

        yield {
            "type": "search-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
