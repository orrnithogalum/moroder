# MusicServer
# - Python server that exposes YTMusicAPI and mpv player control to cpp via JSON over stdin/stdout
# - Handles search, streaming, playback control, and fetching detailed song info
# - Maintains player state and optionally a logged-in user instance

from modules.radio import radio_next
from modules.radio import radio

from modules.playlist import playlist
from modules.search import search
from modules.album import album
from modules.song import song

from ytmusicapi import YTMusic

import asyncio
import json
import os


class MusicServer:
    # - Wraps YTMusicAPI for default or user credentials
    # - Handles streaming via mpv over a Unix IPC socket
    # - Provides player control and search / song info

    def __init__(self, app_name: str = "", cookies_path: str = ""):
        self.login(cookies_path)

        # player state
        self.player_process: asyncio.subprocess.Process | None = None
        self.player_reader: asyncio.StreamReader | None = None
        self.player_writer: asyncio.StreamWriter | None = None
        self.player_socket: str = f"/tmp/{app_name}_socket"
        self.current_song: str | None = None

        if os.path.exists(self.player_socket):
            os.remove(self.player_socket)

        # mpv
        self.mpv_pending: dict[int, asyncio.Future] = {}
        self.mpv_reader_task: asyncio.Task | None = None
        self.mpv_request_id = 0

        # radio
        self.radio_sessions = {}

    async def send_cmd(self, cmd: list):
        # Send a command to mpv
        if not self.player_writer:
            raise RuntimeError("mpv not running")

        self.mpv_request_id += 1
        rid = self.mpv_request_id

        payload = {"command": cmd, "request_id": rid}

        fut = asyncio.get_running_loop().create_future()
        self.mpv_pending[rid] = fut

        self.player_writer.write((json.dumps(payload) + "\n").encode())
        await self.player_writer.drain()

        return await fut

    def login(self, credentials_path: str):
        # login
        # - Creates a YTMusic instance using a credentials file
        # - Marks server as logged in if successful
        # - Currently unused
        try:
            self.ytm = YTMusic(credentials_path)
            self.logged_in = True

        except Exception:
            self.ytm = YTMusic()
            self.logged_in = False

    async def handle_request(self, req: dict):
        # handle_request
        # - Main entrypoint for JSON commands from cpp side
        # - Dispatches actions to corresponding server methods
        # - Returns JSON response with status and result
        action = req.get("action")

        if action == "search":
            query = req.get("query", "")
            limit = req.get("limit", 10)

            async for result in search(self, query, limit):
                print((json.dumps(result) + "\n"), flush=True)

        elif action == "radio":
            id = req.get("id", "")
            limit = req.get("limit", "")
            type = req.get("type", "song")

            if not id.startswith("PL") and type == "playlist":
                id += "PL"

            if id.startswith("VLPL") and type == "playlist":
                id = id[2:]

            if type == "song":
                results = radio(self, id, None, limit)
            else:
                results = radio(self, None, "RDAM" + id, limit)

            async for result in results:
                print((json.dumps(result) + "\n"), flush=True)

        elif action == "radio_next":
            id = req.get("id", "")
            ctoken = req.get("continuation", "")
            limit = req.get("limit", "")
            type = req.get("type", "song")

            if not id.startswith("PL") and type == "playlist":
                id += "PL"

            if type == "song":
                results = radio_next(self, id, None, ctoken, limit)
            else:
                results = radio_next(self, None, "RDAM" + id, ctoken, limit)

            async for result in results:
                print((json.dumps(result) + "\n"), flush=True)

        elif action == "get_album":
            id = req.get("id", "")

            async for result in album(self, id):
                print((json.dumps(result) + "\n"), flush=True)

        elif action == "get_playlist":
            id = req.get("id", "")

            async for result in playlist(self, id):
                print((json.dumps(result) + "\n"), flush=True)

        elif action == "get_song":
            return song(self, req.get("song_title", ""), req.get("song_artist", ""))

        else:
            return {"status": "error", "message": f"Unknown action: {action}"}
