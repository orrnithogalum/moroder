# SEARCH
# - Performs a search using ytmusicapi
# - Limits results to prevent large payloads that could overflow the buffer
# - Returns status and a list of results

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from main import MusicServer

async def search(server: MusicServer, query: str, limit: int):
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
