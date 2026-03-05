from server import YTMusicServer

import asyncio
import json
import sys

async def main():
    server = YTMusicServer()

    while True:
        try:
            line = await asyncio.to_thread(sys.stdin.readline)

            if not line:
                break

            request = json.loads(line)
            response = await server.handle_request(request)

        except Exception as e:
            response = {"status": "error", "message": str(e)}

        print(json.dumps(response), flush=True)

if __name__ == "__main__":
    asyncio.run(main())