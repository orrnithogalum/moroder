# CONTROL
# - Handles player control commands for the running mpv process
# - Sends JSON commands over asyncio StreamWriter to the player
# - Supports pause, resume, seek forward/backward, and stop

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import YTMusicServer

import asyncio
import json

async def send_cmd(writer: asyncio.StreamWriter, cmd: list[str | int]):
    # send_cmd
    # - Serialize a command as JSON and write it to the player
    # - Flushes the writer with drain()
    writer.write((json.dumps({"command": cmd}) + "\n").encode())
    await writer.drain()

async def control(server: YTMusicServer, command: str):
    # control
    # - Executes a player control command on the given server
    # - Commands: pause, resume, forward <seconds>, backward <seconds>, stop
    # - Returns a JSON-like status dictionary
    if not server.player_writer:
        return {"status": "error", "message": "No active player"}

    if command == "pause":
        # Pause playback
        await send_cmd(server.player_writer, ["set_property", "pause", True])

    elif command == "resume":
        # Resume playback
        await send_cmd(server.player_writer, ["set_property", "pause", False])

    elif command.startswith("forward"):
        # Seek forward a given number of seconds (0-255)
        parts = command.split()
        if len(parts) != 2:
            return {"status": "error", "message": "Usage: forward <0-255>"}

        try:
            seconds = int(parts[1])
        except ValueError:
            return {"status": "error", "message": "Invalid number"}

        if not 0 <= seconds <= 255:
            return {"status": "error", "message": "Value must be between 0 and 255"}

        await send_cmd(server.player_writer, ["seek", seconds, "relative"])

    elif command.startswith("backward"):
        # Seek backward a given number of seconds (0-255)
        parts = command.split()
        if len(parts) != 2:
            return {"status": "error", "message": "Usage: backward <0-255>"}

        try:
            seconds = int(parts[1])
        except ValueError:
            return {"status": "error", "message": "Invalid number"}

        if not 0 <= seconds <= 255:
            return {"status": "error", "message": "Value must be between 0 and 255"}

        await send_cmd(server.player_writer, ["seek", -seconds, "relative"])

    elif command == "stop":
        # Terminate the mpv player process
        server.player_process.terminate()
        await server.player_process.wait()
        return {"status": "ok", "stopped": True}

    else:
        # Unknown command
        return {
            "status": "error",
            "message": f"Unknown control: {command}"
        }

    # Command succeeded
    return {"status": "ok"}