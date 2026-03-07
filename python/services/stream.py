from typing import TYPE_CHECKING
from yt_dlp import YoutubeDL

if TYPE_CHECKING:
    from server import YTMusicServer

import asyncio
import os


def get_audio_url(id):
    url = f"https://www.youtube.com/watch?v={id}"

    try:
        with YoutubeDL({"format": "best", "quiet": True, "no_warnings": True}) as ydl:
            info = ydl.extract_info(url, download=False)
            return info["url"]
    except Exception as e:
        raise RuntimeError(f"yt-dlp extraction failed: {e}")


async def stream(server: YTMusicServer, id: str):
    try:
        # stop existing playback if active
        if server.player_process:
            try:
                server.player_process.terminate()
                await server.player_process.wait()
            except Exception:
                pass

            try:
                if server.player_writer:
                    server.player_writer.close()
                    await server.player_writer.wait_closed()
            except Exception:
                pass

            server.player_process = None
            server.player_reader = None
            server.player_writer = None
            server.current_song = None

        # resolve audio URL
        try:
            audio_url = await asyncio.to_thread(get_audio_url, id)
        except Exception as e:
            return {"status": "error", "message": str(e)}

        # remove old socket if present
        try:
            if os.path.exists(server.player_socket):
                os.unlink(server.player_socket)
        except FileNotFoundError:
            pass
        except Exception as e:
            return {"status": "error", "message": f"socket cleanup failed: {e}"}

        # start mpv
        try:
            proc = await asyncio.create_subprocess_exec(
                "mpv",
                "--no-video",
                "--no-config",
                f"--input-ipc-server={server.player_socket}",
                audio_url,
                stdout=asyncio.subprocess.DEVNULL,
                stderr=asyncio.subprocess.DEVNULL
            )
        except FileNotFoundError:
            return {"status": "error", "message": "mpv not installed"}
        except Exception as e:
            return {"status": "error", "message": f"mpv launch failed: {e}"}

        # wait for IPC socket
        try:
            for _ in range(20):  # ~1 second
                if os.path.exists(server.player_socket):
                    break
                await asyncio.sleep(0.05)
            else:
                proc.terminate()
                return {"status": "error", "message": "mpv IPC socket not created"}
        except Exception as e:
            proc.terminate()
            return {"status": "error", "message": f"socket wait failed: {e}"}

        # connect IPC
        try:
            reader, writer = await asyncio.open_unix_connection(server.player_socket)
        except Exception as e:
            proc.terminate()
            return {"status": "error", "message": f"IPC connection failed: {e}"}

        # update server state
        server.player_process = proc
        server.player_reader = reader
        server.player_writer = writer
        server.current_song = id

        return {
            "status": "ok",
            "id": id
        }

    except Exception as e:
        return {"status": "error", "message": str(e)}