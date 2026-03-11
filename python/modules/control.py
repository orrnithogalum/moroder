# CONTROL
# - Handles player control commands for the running mpv process
# - Sends JSON commands over asyncio StreamWriter to the player
# - Supports pause, resume, seek forward/backward, and stop

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import YTMusicServer

import asyncio
import json

async def send_cmd(reader: asyncio.StreamReader, writer: asyncio.StreamWriter, cmd: list[str | int | float | bool]):
    # send command
    writer.write((json.dumps({"command": cmd}) + "\n").encode())
    await writer.drain()

    while True:
        line = await reader.readline()
        if not line:
            return None

        try:
            data = json.loads(line)
        except json.JSONDecodeError:
            continue

        # Only return if this is a response to the command
        if "error" in data:
            return data
        # Otherwise, it's an event, loop and wait for the real response

async def get_property(reader: asyncio.StreamReader, writer: asyncio.StreamWriter, prop: str):
    # get_property
    # - get player data and return it
    resp = await send_cmd(reader, writer, ["get_property", prop])
    
    if resp and resp.get("error") == "success":
        return resp.get("data")
    
    return None

async def control(server: YTMusicServer, command: str):
    # control
    # - Executes a player control command on the given server
    # - Commands: pause, resume, forward <seconds>, backward <seconds>, stop
    # - Returns a JSON-like status dictionary
    if not server.player_writer or not server.player_reader:
        return {"status": "error", "message": "No active player"}

    reader = server.player_reader
    writer = server.player_writer

    async def current_position():
        pos = await get_property(reader, writer, "playback-time")
        return int(pos * 1000 * 1000) if pos is not None else 0
    
    pos = await current_position()

    if command == "pause":
        # Pause playback
        await send_cmd(reader, writer, ["set_property", "pause", True])

    elif command == "resume":
        # Resume playback
        await send_cmd(reader, writer, ["set_property", "pause", False])

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

        await send_cmd(reader, writer, ["seek", seconds, "relative"])

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

        await send_cmd(reader, writer, ["seek", seconds, "relative"])

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

        await send_cmd(reader, writer, ["seek", seconds, "absolute"])

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