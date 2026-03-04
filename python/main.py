from server import YTMusicServer

import json
import sys

def main():
    server = YTMusicServer()

    for line in sys.stdin:
        try:
            request = json.loads(line)
            response = server.handle_request(request)

        except Exception as e:
            response = {"status": "error", "message": str(e)}

        print(json.dumps(response), flush=True)


if __name__ == "__main__":
    main()