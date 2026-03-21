# STREAM
# - Handles streaming audio of a YouTube video using yt-dlp and mpv
# - Resolves the best audio URL, stops any existing playback, and starts mpv asynchronously
# - Uses a local IPC socket to control playback
# - Returns status and song id, errors if anything fails

from typing import TYPE_CHECKING
from yt_dlp import YoutubeDL

if TYPE_CHECKING:
    from server import MusicServer

import asyncio


def get_audio_url(id: str):
    url = f"https://www.youtube.com/watch?v={id}"

    try:
        with YoutubeDL(
            {"format": "bestaudio/best", "quiet": True, "no_warnings": True}
        ) as ydl:
            info = ydl.extract_info(url, download=False)

            duration = info.get("duration", 0)

            if not duration:
                duration = 0

            duration_micros = int(duration * 1000 * 1000)
            return url, duration_micros

    except Exception as e:
        raise RuntimeError(f"yt-dlp extraction failed: {e}")

async def stream(server: "MusicServer", id: str):
    try:
        # resolve audio URL
        try:
            audio_url, duration_micros = await asyncio.to_thread(get_audio_url, id)
        except Exception as e:
            return {"status": "error", "message": str(e)}

        return {
            "status": "ok",
            "id": id,
            "url": audio_url,
            "duration": duration_micros,
        }

    except Exception as e:
        return {"status": "error", "message": str(e)}
