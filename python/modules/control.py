# CONTROL
# - Handles player control commands for the running mpv process
# - Sends JSON commands over asyncio StreamWriter to the player
# - Supports pause, resume, seek forward/backward, and stop

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import MusicServer

async def get_property(server: MusicServer, prop: str):
    # get_property
    # - get player data and return it
    resp = await server.send_cmd(["get_property", prop])

    if resp and resp.get("error") == "success":
        return resp.get("data")
    
    return None

async def control(server: MusicServer, command: str):
    # control
    # - Executes a player control command on the given server
    # - Commands: pause, resume, forward <seconds>, backward <seconds>, stop
    # - Returns a JSON-like status dictionary
    if not server.player_writer or not server.player_reader:
        return {"status": "error", "message": "No active player"}

    async def current_position():
        pos = await get_property(server, "playback-time")
        return int(pos * 1000 * 1000) if pos is not None else 0
    
    pos = await current_position()

    if command == "pause":
        # Pause playback
        await server.send_cmd(["set_property", "pause", True])

    elif command == "resume":
        # Resume playback
        await server.send_cmd(["set_property", "pause", False])

    elif command.startswith("forward"):
        # Seek forward a given number of microseconds (int)
        parts = command.split()
        if len(parts) != 2:
            return {"status": "error", "message": "Usage: forward <microseconds>"}

        try:
            # convert input to float seconds; if int > 1000 it might be microseconds
            value = float(parts[1])
            seconds = value / 1000000 if value > 1000 else value
        except ValueError:
            return {"status": "error", "message": "Invalid number"}

        await server.send_cmd(["seek", seconds, "relative"])

    elif command.startswith("backward"):
        # Seek backward a given number of microseconds (int)
        parts = command.split()
        if len(parts) != 2:
            return {"status": "error", "message": "Usage: backward <microseconds>"}

        try:
            value = float(parts[1])
            seconds = -value / 1000000 if value > 1000 else -value
        except ValueError:
            return {"status": "error", "message": "Invalid number"}

        await server.send_cmd(["seek", seconds, "relative"])

    elif command.startswith("setpos"):
        # Set position in song for a given position in microseconds (int)
        parts = command.split()
        if len(parts) != 2:
            return {"status": "error", "message": "Usage: backward <microseconds>"}

        try:
            value = float(parts[1])
            seconds = value / 1000000
        except ValueError:
            return {"status": "error", "message": "Invalid number"}

        await server.send_cmd(["seek", seconds, "absolute"])

    elif command == "stop":
        # Terminate the mpv player process
        server.player_process.terminate()
        await server.player_process.wait()

        server.send_event("stop")
        return {"status": "ok", "stopped": True}

    else:
        # Unknown command
        return {
            "status": "error",
            "message": f"Unknown control: {command}"
        }
    
    # Command succeeded
    return {
        "status": "ok",
        "position": pos
    }