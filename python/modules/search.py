# SEARCH
# - Performs a search on YouTube Music using ytmusicapi
# - Limits results to prevent large payloads that could overflow the buffer
# - Returns status and a list of results

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer


def search(server: MusicServer, query: str):
    # search
    # - Uses server.ytm to perform a search
    # - result_limit caps the number of results returned to prevent cpp buffer overflow
    # - Returns a dictionary with status and results
    result_limit = 10
    try:
        results = server.ytm.search(query, limit=result_limit)

        # slice to ensure we never exceed the limit
        results = results[:result_limit]

        return {"status": "ok", "results": results, "authed": server.logged_in}

    except Exception as e:
        return {"status": "error", "message": str(e)}
