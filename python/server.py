from ytmusicapi import YTMusic

from services.search import search

class YTMusicServer:
    def __init__(self):
        self.ytm_default = YTMusic()
        self.ytm_user = None
        self.logged_in = False


    def login(self, credentials_path: str):
        try:
            self.ytm_user = YTMusic(credentials_path)
            self.logged_in = True
            return {"status": "ok"}
        
        except Exception as e:
            return {"status": "error", "message": str(e)}


    def search(self, query: str):
        return search(self.ytm_default, query)


    def handle_request(self, req: dict):
        action = req.get("action")

        if action == "search":
            return self.search(req.get("query", ""))

        elif action == "login":
            return self.login(req.get("credentials_path", ""))

        else:
            return {
                "status": "error",
                "message": f"Unknown action: {action}"
            }