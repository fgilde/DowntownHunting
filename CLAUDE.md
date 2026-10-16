# Downtown Hunting

Multiplayer-Shooter auf Basis von Lyra (UE 5.8.3). Plan: `docs/PLAN.md`.

- Editor über `Start-Editor.cmd` starten. Der Unreal-MCP-Server läuft auf Port **8765**, nicht 8000, weil Docker 8000 belegt (`.mcp.json`).
- Der MCP stellt die Meta-Tools `list_toolsets`, `describe_toolset` und `call_tool` bereit.
- Lyra-Kern (`Source/LyraGame`, Lyra-Plugins) möglichst nicht ändern. Eigenes kommt ins Game Feature Plugin `DowntownCore`.
- City-Sample-Quelle: `C:\Users\User\Documents\Unreal Projects\CitySample`. Nur benötigte Assets migrieren.
- Asset-Quelle Vorgänger: `C:\dev\privat\github\callOfShooter` (Blacksite). Lizenzen in dessen `THIRD_PARTY_ASSETS.md` prüfen.
- Modulnamen bleiben `LyraGame`/`LyraEditor`. Nur `.uproject` und `ProjectName` wurden umbenannt.
