from ytmusicapi import YTMusic

from services.control import control
from services.stream import stream
from services.search import search

class YTMusicServer:
    def __init__(self):
        self.ytm_default = YTMusic()
        self.ytm_user = None
        self.logged_in = False

        # player state
        self.player_process = None
        self.player_reader = None
        self.player_writer = None
        self.player_socket = "/tmp/mpv_socket"
        self.current_song = None


    def login(self, credentials_path: str):
        try:
            self.ytm_user = YTMusic(credentials_path)
            self.logged_in = True
            return {"status": "ok"}
        
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def search(self, query: str):
        return search(self, query)
    
    async def stream(self, song_id: str):
        return await stream(self, song_id)

    async def control(self, command: str):
        return await control(self, command)

    async def handle_request(self, req: dict):
        action = req.get("action")

        if action == "search":
            return self.search(req.get("query", ""))

        elif action == "login":
            return self.login(req.get("credentials_path", ""))

        elif action == "stream":
            return await self.stream(req.get("id"))

        elif action == "control":
            return await self.control(req.get("command"))

        else:
            return {
                "status": "error",
                "message": f"Unknown action: {action}"
            }