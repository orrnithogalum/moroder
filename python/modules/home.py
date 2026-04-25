# HOME
# - Fetches home sections using ytmusicapi
# - Streams items grouped by category (section title)
# - Sends each item individually to avoid large payloads

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from main import MusicServer


async def home(server: "MusicServer"):
    try:
        sections = server.ytm.get_home(limit=10) or []

        for section in sections:
            category = section.get("title", "") or ""
            contents = section.get("contents", []) or []

            for item in contents:
                yield {
                    "type": "home-item",
                    "status": "ok",
                    "category": category,
                    "data": item
                }

        yield {
            "type": "home-done",
            "status": "ok"
        }

    except Exception as e:
        yield {
            "type": "error",
            "message": str(e)
        }
