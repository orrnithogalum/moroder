# SEARCH
# - Performs a search on YouTube Music using ytmusicapi
# - Limits results to prevent large payloads that could overflow the buffer
# - Returns status and a list of results

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import YTMusicServer

def search(server: YTMusicServer, query: str):
    # search
    # - Uses server.ytm_default to perform a search
    # - result_limit caps the number of results returned to prevent cpp buffer overflow
    # - Returns a dictionary with status and results
    result_limit = 10
    try:
        results = server.ytm_default.search(
            query,
            limit=result_limit
        )

        # slice to ensure we never exceed the limit
        results = results[:result_limit]

        return {"status": "ok", "results": results}

    except Exception as e:
        return {"status": "error", "message": str(e)}