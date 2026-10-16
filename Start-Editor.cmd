@echo off
rem Port 8000 is taken by Docker, so the Unreal MCP server runs on 8765 (see .mcp.json).
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0DowntownHunting.uproject" -ModelContextProtocolStartServer -ModelContextProtocolPort=8765
