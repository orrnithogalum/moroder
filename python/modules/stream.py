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
import os


def get_audio_url(id: str):
    url = f"https://www.youtube.com/watch?v={id}"

    try:
        with YoutubeDL({
            "format": "bestaudio/best",
            "quiet": True,
            "no_warnings": True
        }) as ydl:
            info = ydl.extract_info(url, download=False)

            duration_micros = int(info.get("duration", 0) * 1000 * 1000)

            return info["url"], duration_micros

    except Exception as e:
        raise RuntimeError(f"yt-dlp extraction failed: {e}")


async def ensure_mpv(server: "MusicServer"):
    # Start mpv only if not already running
    if server.player_process:
        return

    try:
        proc = await asyncio.create_subprocess_exec(
            "mpv",
            "--no-video",
            "--no-config",
            "--idle=yes",
            "--cache=yes",
            "--cache-secs=10",
            "--prefetch-playlist=yes",
            "--playlist-start=0",
            f"--input-ipc-server={server.player_socket}",
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.DEVNULL
        )
    except FileNotFoundError:
        raise RuntimeError("mpv not installed")
    except Exception as e:
        raise RuntimeError(f"mpv launch failed: {e}")

    # wait for IPC socket
    for _ in range(20):  # ~1s
        if os.path.exists(server.player_socket):
            break
        await asyncio.sleep(0.05)
    else:
        proc.terminate()
        raise RuntimeError("mpv IPC socket not created")

    try:
        reader, writer = await asyncio.open_unix_connection(server.player_socket)
    except Exception as e:
        proc.terminate()
        raise RuntimeError(f"IPC connection failed: {e}")

    server.player_process = proc
    server.player_reader = reader
    server.player_writer = writer

    server.mpv_reader_task = asyncio.create_task(server.mpv_reader_loop())


async def stream(server: "MusicServer", id: str):
    try:
        # resolve audio URL
        try:
            audio_url, duration_micros = await asyncio.to_thread(get_audio_url, id)
        except Exception as e:
            return {"status": "error", "message": str(e)}

        # ensure mpv is running
        try:
            await ensure_mpv(server)
        except Exception as e:
            return {"status": "error", "message": str(e)}

        try:
            if server.current_song is None:
                # nothing playing → replace
                await server.send_cmd(["loadfile", audio_url, "replace"])
                server.current_song = id
            else:
                # already playing → queue
                await server.send_cmd(["loadfile", audio_url, "append-play"])
        except Exception as e:
            return {"status": "error", "message": f"mpv command failed: {e}"}

        return {
            "status": "ok",
            "id": id,
            "duration": duration_micros,
        }

    except Exception as e:
        return {"status": "error", "message": str(e)}