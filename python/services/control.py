from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from server import YTMusicServer

import asyncio
import json

async def send_cmd(writer: asyncio.StreamWriter, cmd: list[str | int]):
    writer.write((json.dumps({"command": cmd}) + "\n").encode())
    await writer.drain()

async def control(server: YTMusicServer, command: str):
    if not server.player_writer:
        return {"status": "error", "message": "No active player"}

    if command == "pause":
        await send_cmd(server.player_writer, ["set_property", "pause", True])

    elif command == "resume":
        await send_cmd(server.player_writer, ["set_property", "pause", False])

    elif command.startswith("forward"):
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
        server.player_process.terminate()
        await server.player_process.wait()
        return {"status": "ok", "stopped": True}

    else:
        return {
            "status": "error",
            "message": f"Unknown control: {command}"
        }

    return {"status": "ok"}