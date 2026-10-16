# Notizen

## Phase 0 – abgeschlossen (2026-10-16)

- Lyra Elimination (`/ShooterMaps/Maps/L_Expanse`) mit 3 Bots in PIE: läuft. ![Bots](img/phase0-elimination-bots.png)
- Listen-Server + 2. Client in PIE: Client tritt bei, Experience lädt auf beiden Seiten. ![Listen-Server](img/phase0-listenserver-client.png)

### Fix: Bots standen still

- Log: `RecastNavMesh-Default> Failed to add tile ..., 36 tile limit reached!`
- Ursache: Bei World-Partition-Maps wird der Tile-Pool beim Laden aus den Nav-Bounds berechnet, bevor die Nav-Chunks gestreamt sind. Ergebnis: 36 Tiles, der Rest fliegt raus.
- Fix projektweit in `Config/DefaultEngine.ini`: `[/Script/NavigationSystem.RecastNavMesh] bFixedTilePoolSize=True, TilePoolSize=4096`.
- Zusätzlich NavMesh von L_Expanse für 5.8 neu gebaut (die alten Daten passten nicht zur Engine-Version):
  ```
  UnrealEditor-Cmd.exe DowntownHunting.uproject L_Expanse -run=WorldPartitionBuilderCommandlet -AllowCommandletRendering -builder=WorldPartitionNavigationDataBuilder -SCCProvider=None
  ```
  Aus PowerShell aufrufen, Git-Bash verbiegt `/Pfad`-Argumente. Map vorher im Editor entladen.

### Werkzeug-Hinweise (Unreal MCP)

- PIE: `EditorAppToolset.StartPIE` / `StopPIE`. Listen-Server über Editor-Einstellung `LevelEditor/PlayIn`: `PlayNetMode=PIE_ListenServer`, `PlayNumberOfClients=2`.
- Screenshots (`CaptureEditorImage`) kommen als base64 in einer Datei, mit Python dekodieren.
- Laufzeit-Abfragen (Positionen von Bots usw.) gehen per `ProgrammaticToolset.execute_tool_script`.

## Phase 1 – Stand (2026-10-16)

Erledigt:
- Game Feature Plugin `Plugins/GameFeatures/DowntownCore` (C++-Modul `DowntownCoreRuntime`, hängt an `LyraGame` und `ShooterCore`). GameFeatureData `/DowntownCore/DowntownCore` scannt `Experiences/` und `Playlists/`.
- Experiences: `B_DH_TDM` (Third-Person) und `B_DH_TDM_FP` (Ego für alle). Beide Kopien von Lyras Elimination, aktivieren `ShooterCore` + `DowntownCore`. Perspektive pro Match = Wahl der Experience.
- Menü-Einträge: `/DowntownCore/Playlists/DA_DH_TDM_Expanse`, `DA_DH_TDM_FP_Expanse` (Map L_Expanse).
- Ego-Kamera: `UDHCameraMode_FirstPerson` (Lyra-Basismodus sitzt schon auf Augenhöhe, Klasse blendet nur den Kopfknochen aus). Pawn-Daten `DH_HeroData_FP`, eigenes Ability-Set mit `GA_DH_ADS_FP` (ADS wechselt nicht auf die TP-ADS-Kamera). ![Ego](img/phase1-firstperson-camera.png)
- Branding: Fenstertitel `Downtown Hunting`, Hauptmenü mit Text-Logo, Buttons „Play“/„Quit“ (`/Game/UI/Menu/W_LyraFrontEnd`). ![Menü](img/phase1-menu.png)

Testen im Editor: Editor-Einstellungen > DowntownHunting > Lyra Developer Settings > `ExperienceOverride` auf `B_DH_TDM` oder `B_DH_TDM_FP`, dann PIE auf L_Expanse.

Offen:
- Ego: Waffe und Arme sind nicht im Bild. Braucht First-Person-Arme + FP-Animationen und UE-First-Person-Rendering (`FirstPersonPrimitiveType`) gegen Clipping. ADS-Zoom (FOV) kommt mit den FP-Waffen.
- Ego-ADS und Menüeinträge sind nur angelegt, nicht per Eingabe durchgeklickt.
- Bots im Ego-Modus nutzen dieselben Pawn-Daten, betrifft sie nicht (keine Kamera).
