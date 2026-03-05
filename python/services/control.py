import json

async def send_cmd(writer, cmd):
    writer.write((json.dumps({"command": cmd}) + "\n").encode())
    await writer.drain()

async def control(server, command: str):
    if not server.player_writer:
        return {"status": "error", "message": "No active player"}

    if command == "pause":
        await send_cmd(server.player_writer, ["cycle", "pause"])

    elif command == "forward":
        await send_cmd(server.player_writer, ["seek", 10, "relative"])

    elif command == "back":
        await send_cmd(server.player_writer, ["seek", -10, "relative"])

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