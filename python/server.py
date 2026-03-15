# MusicServer
# - Python server that exposes YTMusicAPI and mpv player control to cpp via JSON over stdin/stdout
# - Handles search, streaming, playback control, and fetching detailed song info
# - Maintains player state and optionally a logged-in user instance

from ytmusicapi import YTMusic

from modules.get_song import get_song
from modules.control import control
from modules.stream import stream
from modules.search import search

import asyncio
import json
import os

class MusicServer:
    # - Wraps YTMusicAPI for default or user credentials
    # - Handles streaming via mpv over a Unix IPC socket
    # - Provides player control and search / song info

    def __init__(self, event_fd: int = None):
        self.ytm_default: YTMusic = YTMusic()
        self.ytm_user: YTMusic | None = None
        self.logged_in: bool = False

        # player state
        self.player_process: asyncio.subprocess.Process | None = None
        self.player_reader: asyncio.StreamReader | None = None
        self.player_writer: asyncio.StreamWriter | None = None
        self.player_socket: str = "/tmp/mpv_socket"
        self.current_song: str | None = None

        # mpv
        self.mpv_pending: dict[int, asyncio.Future] = {}
        self.mpv_reader_task: asyncio.Task = None
        self.mpv_request_id = 0
        
        # events
        self.event_pipe = os.fdopen(event_fd, "wb", buffering=0) if event_fd is not None else None

    async def mpv_reader_loop(self):
        reader = self.player_reader

        # Parse line by line, command or event
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break

                try:
                    data = json.loads(line)
                except json.JSONDecodeError:
                    continue

                if "request_id" in data:
                    rid = data["request_id"]
                    fut = self.mpv_pending.pop(rid, None)
                    if fut and not fut.done():
                        fut.set_result(data)

                elif "event" in data:
                    if data["event"] == "end-file":
                        reason = data.get("reason")

                        if reason == "eof":
                            self.send_event("song-end")
        
        except asyncio.CancelledError:
            pass
        
        finally:
            # Cleanup in case of mpv instance killed
            self.player_reader = None
            self.player_writer = None

    async def send_cmd(self, cmd: list):
        # Send a command to mpv
        if not self.player_writer:
            raise RuntimeError("mpv not running")

        self.mpv_request_id += 1
        rid = self.mpv_request_id

        payload = {
            "command": cmd,
            "request_id": rid
        }

        fut = asyncio.get_running_loop().create_future()
        self.mpv_pending[rid] = fut

        self.player_writer.write((json.dumps(payload) + "\n").encode())
        await self.player_writer.drain()

        return await fut

    def send_event(self, name: str):
        # Send an event to cpp as JSON. cpp parses this in a seperate thread.
        if not self.event_pipe:
            return
        
        payload = json.dumps({
            "status": "ok",
            "name": name
        })

        self.event_pipe.write(payload.encode("utf-8") + b"\n")
        self.event_pipe.flush()

    def login(self, credentials_path: str):
        # login
        # - Creates a YTMusic instance using a credentials file
        # - Marks server as logged in if successful
        # - Currently unused
        try:
            self.ytm_user = YTMusic(credentials_path)
            self.logged_in = True
            return {"status": "ok"}
        
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def search(self, query: str):
        # search
        # - Performs a search via YTMusicAPI
        return search(self, query)
    
    def get_song(self, song_title: str, song_artist: str):
        #  get_song
        # - Fetches detailed song info via MusicBrainz API
        return get_song(self, song_title, song_artist)
    
    async def stream(self, song_id: str):
        # stream
        # - Starts playback of a song via mpv
        # - Stops any existing playback
        return await stream(self, song_id)

    async def control(self, command: str):
        # control
        # - Executes playback commands (pause, resume, seek, stop, etc.)
        return await control(self, command)

    async def handle_request(self, req: dict):
        # handle_request
        # - Main entrypoint for JSON commands from cpp side
        # - Dispatches actions to corresponding server methods
        # - Returns JSON response with status and result
        action = req.get("action")

        if action == "search":
            return self.search(req.get("query", ""))

        elif action == "login":
            return self.login(req.get("credentials_path", ""))

        elif action == "stream":
            return await self.stream(req.get("id", ""))

        elif action == "control":
            return await self.control(req.get("command", ""))
        
        elif action == "get_song":
            return self.get_song(req.get("song_title", ""), req.get("song_artist", ""))

        else:
            return {
                "status": "error",
                "message": f"Unknown action: {action}"
            }