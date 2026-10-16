# Notizen

## Starten

- `Play.cmd`: Spiel mit Hauptmenü (ohne Editor-Oberfläche, nutzt die installierte UE 5.8).
- `Play-Downtown.cmd`: direkt Team Deathmatch in der Stadt mit Bots. `Play-Downtown.cmd FP` für Ego.
- `Package.cmd`: baut eine eigenständige `DowntownHunting.exe` nach `Build\Packaged\Windows` (dauert beim ersten Mal lange).
- `Start-Editor.cmd`: Editor mit Unreal-MCP auf Port 8765.

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
- Ego: Nachlade-Animation fehlt im Ego-Bild (Pose bleibt statisch). ADS-Position ist geschätzt, nicht mit gehaltener rechter Maustaste geprüft.
- Gewehr/Schrotflinte-Posen sind eingetragen, aber nur die Pistole wurde angesehen.
- Ego-ADS und Menüeinträge sind nur angelegt, nicht per Eingabe durchgeklickt.

### Ego-Waffe (2026-10-16)

Lyras Animationen halten die Waffe auf Brusthöhe, unterhalb des Blickfelds (auch beim Schießen). `UDHCameraMode_FirstPerson` (Blueprint `/DowntownCore/Camera/CM_DH_FirstPerson`) zeigt dem lokalen Spieler deshalb Kopien:
- Körper-Kopie spielt eine Halte-Pose (`MM_Pistol/Shotgun/Rifle_Idle_ADS`, gewählt nach Klassenname der Waffe). Sie wird so gedreht, dass der Lauf (Griff → Socket `Muzzle`) in Blickrichtung zeigt, und so verschoben, dass der Griff bei `HipWeaponOffset` bzw. beim Zielen bei `ADSWeaponOffset` im Blickraum sitzt. Kopf und Beine ausgeblendet.
- Waffen-Kopien hängen am selben Socket der Körper-Kopie, nur für den Besitzer sichtbar.
- Echter Körper und echte Waffe werden für den Besitzer per `FirstPersonPrimitiveType=WorldSpaceRepresentation` ausgeblendet; Schatten und Sicht für andere bleiben. Hitboxen und replizierte Attachments bleiben unangetastet.
- Zielen (Tag `Event.Movement.ADS`): FOV 80 → 60, Waffe wandert zur Bildmitte.
- Rückstoß: kurzer Kick nach hinten/oben pro Schuss (erkannt über `ULyraWeaponInstance::GetTimeSinceLastInteractedWith`).
- Gegen Clipping in Wände: UE-First-Person-Rendering (`FirstPersonScale` 0.5). Dafür reicht `LyraCameraComponent` jetzt die First-Person-Parameter an den View durch (einzige Änderung am Lyra-Kern, 3 Zeilen, standardmäßig aus).
- Hinweis: Werte am CDO zur Laufzeit zu ändern wirkte im Test nicht auf neue PIE-Sitzungen; Tuning daher über Code-Defaults bzw. das Blueprint mit anschließendem Speichern und Editor-Neustart.
![Ego-Waffe](img/phase1-firstperson-weapon.png)

## Spielmodi (2026-10-16)

Im Menü (Play) stehen nur noch die DH-Modi, Lyras eigene Einträge sind ausgeblendet (`bShowInFrontEnd=false`):

| Modus | Experience | Map |
| --- | --- | --- |
| Team Deathmatch | `B_DH_TDM` | L_Expanse |
| Team Deathmatch (Ego) | `B_DH_TDM_FP` | L_Expanse |
| Domination | `B_DH_Domination` | L_Convolution_Blockout |
| Domination (Ego) | `B_DH_Domination_FP` | L_Convolution_Blockout |

3 vs 3 vs 3 (`B_DH_TDM3`, Team-Setup `B_DH_TeamSetup_ThreeTeams`, grünes Team `TeamDA_Green`) ist angelegt, aber ausgeblendet: Lyras Team-UI (Namensschilder, Score-HUD, Scoreboard, Killfeed; ~45 `Select`-Knoten in 8 Widgets) und die TDM-Wertung (`B_TeamDeathMatchScoring`) kennen nur 2 Teams. Umbau gehört zu Phase 4.

NavMesh von L_Convolution_Blockout ebenfalls mit dem WP-Builder neu gebaut (siehe Phase 0).

## Phase 2 – Stadtausschnitt DH_Block (2026-10-16, in Arbeit)

- Quelle: City Sample `Small_City_LVL`, Ausschnitt 250 × 250 m um (-500 m, 250 m) ≈ 2 × 2 Blöcke, Gebäude bis ~50 m.
- Übernommen: Gebäude (+ Kollision, Dächer, Fassaden-Props), Boden (+ Kollision, Decals), Straßenmöbel, geparkte Autos, Ampeln, Licht. Nicht übernommen: Freeway, Audio, HLOD, Mass/Crowd/Traffic-KI.
- Map: `/Game/DH/Maps/DH_Block` (812 Actors, alle immer geladen, City-Sample-Data-Layers entfernt). Dazu 16 `LyraPlayerStart`, `NavMeshBoundsVolume`, 4 unsichtbare Randwände (±120 m), `SkyAtmosphere` (Ordner `DH_Gameplay`).
- Menü: „Team Deathmatch / Downtown“ (Standard für Quickplay) und „Team Deathmatch (Ego) / Downtown“.
- **City-Sample-Content ist nicht in git** (~15 GB, 8K-Texturen). Einrichtung auf einem neuen Rechner:
  1. City Sample (5.8) von Fab laden.
  2. `Tools\Import-CitySample.ps1` (Map-Assets) und `Tools\Import-CitySample.ps1 -Manifest Tools\CitySample_Characters.txt`.
  3. `UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script=Tools/Python/setup_citysample.py -nullrhi`
- Hilfsskripte: `Tools/Python/save_dh_block.py` (Save-As der ausgedünnten Map), `flatten_dh_block.py` (Streaming aus, Data-Layer raus).
- `Plugins/Traffic`: Content-only-Ersatz für City Samples Traffic-Plugin, enthält nur die Material-Funktion, die die Auto-Materialien brauchen.
- NavMesh gebaut (WP-Builder, Editor vorher schließen spart RAM), Bots laufen. ![Downtown](img/phase2-downtown.png)
- Beim Builder-Lauf melden 24 Texturen fehlende Quell-Payloads (City-Sample-Pakete ohne eingebettete Editor-Daten); für Navigation und Spiel ohne Folgen.
- Domination auf DH_Block: Kontrollpunkte A (NW-Kreuzung), B (Mitte), C (SO-Kreuzung). ![Domination](img/phase2-domination.png)
- Neu platzierte Actors sind in WP-Maps standardmäßig räumlich gestreamt; danach `flatten_dh_block.py` erneut laufen lassen.
- Offen: Map-Rand (Stadt endet abrupt, weiße Leere), FPS messen, Licht abstimmen, Dachzugänge/Innenräume.

## Charaktere (2026-10-16)

- City Samples Hauptfiguren (MetaHuman-basiert) als Lyra-Cosmetics: `/DowntownCore/Characters/B_DH_PlayerMale`, `B_DH_PlayerFemale` (Body + Head). Zufallsauswahl `B_DH_PickRandomCharacter` in allen DH-Experiences.
- Animation: Lyras `ABP_Mannequin_CopyPose` läuft auf den City-Sample-Skeletten, weil `SK_Base`/`SK_Base_Head` als kompatibel zu `SK_Mannequin` markiert sind (`setup_citysample.py`).
- Teamfarbe: `UDHTeamBandComponent` hängt Armbinden mit `M_TeamColorBasic` an beide Oberarme; Lyras Teamsystem färbt sie wie seine eigenen Cosmetics. Im Ego-Modus sind sie für den Besitzer ausgeblendet. ![Teamband](img/phase2-teamband.png)
- Offen: männliche Figur ohne Haare (Groom nicht angehängt), Gesichter ohne Mimik.
