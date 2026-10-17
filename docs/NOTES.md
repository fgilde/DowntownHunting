# Notizen

## Starten

- `Play.cmd`: Spiel mit Hauptmenü (ohne Editor-Oberfläche, nutzt die installierte UE 5.8).
- `Play-Downtown.cmd`: direkt Team Deathmatch in der Stadt mit Bots. Optionen kombinierbar: `FP` (Ego), `DOM` (Domination), `PARK` (Map Waterfront), `DEPOT` (Map Depot).
- `Package.cmd`: baut eine eigenständige `DowntownHunting.exe` nach `Build\Packaged\Windows` (dauert beim ersten Mal lange).
- `Start-Editor.cmd`: Editor mit Unreal-MCP auf Port 8765.
- `Sync-Modes.cmd`: schreibt die Spielmodi aus `Modes.json` ins Menü (Editor vorher schließen).

## Modi (2026-10-17)

- Modi stehen in `Modes.json`: Titel, Beschreibung, Spielart (`TDM`, `Domination`), Map und Regeln (`Perspective`, `Loadout`, `WeaponSwitch`, `NumBots`). `Sync-Modes.cmd` erzeugt daraus `/DowntownCore/Playlists/DA_DH_<id>` und entfernt veraltete Einträge.
- Regeln wirken über `UDHGameRulesComponent` (GameState, liest URL-Optionen, repliziert) und `UDHPlayerRulesComponent` (Pawn: Loadout in die Schnellleiste, Kamera-Modus, Umschalten mit V / D-Pad oben bei `Perspective=Choice`). Getestet im Spielprozess: Loadout (Pistole, Gewehr, Schrotflinte), Waffenwechsel, Umschalten Third-Person ↔ Ego.
- Im Editor-PIE kommen keine URL-Optionen an; Regeln dort testen mit `Play-Downtown.cmd` (setzt die Optionen in der Start-URL).

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
  2. `Tools\Import-CitySample.ps1` (Map-Assets), dann mit `-Manifest Tools\CitySample_Characters.txt` und `-Manifest Tools\CitySample_DH_Block_Ring.txt`.
  3. `UnrealEditor-Cmd.exe DowntownHunting.uproject -run=pythonscript -script=Tools/Python/setup_citysample.py -nullrhi`
- Hilfsskripte: `Tools/Python/save_dh_block.py` (Save-As der ausgedünnten Map), `flatten_dh_block.py` (Streaming aus, Data-Layer raus).
- `Plugins/Traffic`: Content-only-Ersatz für City Samples Traffic-Plugin, enthält nur die Material-Funktion, die die Auto-Materialien brauchen.
- NavMesh gebaut (WP-Builder, Editor vorher schließen spart RAM), Bots laufen. ![Downtown](img/phase2-downtown.png)
- Beim Builder-Lauf melden 24 Texturen fehlende Quell-Payloads (City-Sample-Pakete ohne eingebettete Editor-Daten); für Navigation und Spiel ohne Folgen.
- Domination auf DH_Block: Kontrollpunkte A (NW-Kreuzung), B (Mitte), C (SO-Kreuzung). ![Domination](img/phase2-domination.png)
- Neu platzierte Actors sind in WP-Maps standardmäßig räumlich gestreamt; danach `flatten_dh_block.py` erneut laufen lassen.
- Kulisse: Ring aus 1284 City-Sample-Actors (Gebäude, Boden, jeweils mit Kollision) von 125 bis 400 m um die Spielfläche, Ordner `CitySample_Backdrop`, damit die Stadt hinter den Randwänden weitergeht. Manifest `Tools/CitySample_DH_Block_Ring.txt` (+5,5 GB lokal), Übertragung mit `Tools/Python/add_ring_to_dh_block.py` (läuft im normalen Editor: `UnrealEditor.exe DowntownHunting.uproject -ExecutePythonScript=Tools/Python/add_ring_to_dh_block.py -quitafter`, Copy/Paste ist im Commandlet gesperrt). ![Kulisse](img/phase2-backdrop-ring.png)
- Offen: FPS messen, Licht abstimmen, Dachzugänge/Innenräume.

## Charaktere (2026-10-16)

- City Samples Hauptfiguren (MetaHuman-basiert) als Lyra-Cosmetics: `/DowntownCore/Characters/B_DH_PlayerMale`, `B_DH_PlayerFemale` (Body + Head). Zufallsauswahl `B_DH_PickRandomCharacter` in allen DH-Experiences.
- Animation: Lyras `ABP_Mannequin_CopyPose` läuft auf den City-Sample-Skeletten, weil `SK_Base`/`SK_Base_Head` als kompatibel zu `SK_Mannequin` markiert sind (`setup_citysample.py`).
- Teamfarbe: `UDHTeamBandComponent` hängt Armbinden mit `M_TeamColorBasic` an beide Oberarme; Lyras Teamsystem färbt sie wie seine eigenen Cosmetics. Im Ego-Modus sind sie für den Besitzer ausgeblendet. ![Teamband](img/phase2-teamband.png)
- Offen: männliche Figur ohne Haare (Groom nicht angehängt), Gesichter ohne Mimik.

## Branding (2026-10-17)

- Logo `T_DH_Logo` (`Tools/Branding/make_logo.py`): „DOWNTOWN“ mit Fadenkreuz-O, „HUNTING“ in Stencil, Skyline. Ersetzt das Lyra-Logo im Hauptmenü, in den Credits, im Start- und Ladebildschirm (`W_Credits`, `W_LyraStartup`, `W_LyraLogo_LoadingScreen`, `W_LyraLogo_Small`, `W_LyraFrontEnd`).
- Credits: eigener Text statt 100 Platzhalterzeilen (Lyra, City Sample, Unreal Engine genannt).
- Menü-Kacheln pro Modus aus Aufnahmen von DH_Block (`Tools/Branding/make_tiles.py`, `/Game/DH/UI/Tiles`), in `Modes.json` per `image`.
- Startkarten (Schnellspiel / Spiel starten / Durchsuchen) zeigen Stadtbilder statt Lyra-Roboter (`Tools/Python/apply_branding.py` leitet Lyras Texturen auf `/Game/DH/UI/Cards` um).
- Ladebildschirme `W_DH_LoadingScreen_Streets` (TDM) und `_Aerial` (Domination) mit Logo, gesetzt von `sync_modes.py`.
- Hauptmenü-Hintergrund: Stadtbild `T_DH_Menu_Background` statt Lyras Halle. ![Menü](img/branding-menu.png)
- Alle Branding-Assets liegen in `/Game/DH/UI` (nicht im Game-Feature-Plugin): Lyras Basis-Widgets in `/Game` dürfen nicht auf Plugin-Content verweisen (Asset-Prüfung „Unzulässige Referenz“, im gepackten Spiel nicht sicher geladen).

## Map Waterfront / DH_Park (2026-10-17)

- City-Sample-Ausschnitt 250 × 250 m um (550 m, -225 m): Bürotürme bis ~115 m, Plaza, Baustellenzaun, Ostrand der Stadt. Kulisse: Gebäude bis 400 m um die Mitte. 1222 Actors + Gameplay (16 Starts, 3 Kontrollpunkte, Randwände ±120 m, NavMesh).
- Modi: `TDM_Waterfront`, `DOM_Waterfront` (Kacheln `/Game/DH/UI/Tiles/T_DH_Tile_*_Park`).
- Getestet: Domination mit Bots im Spielprozess, Bots laufen zu den Punkten und erobern.
- Pipeline für weitere Ausschnitte (Editor geschlossen):
  1. `UnrealEditor-Cmd.exe <CitySample>.uproject -run=pythonscript -script="<repo>/Tools/Python/cs_region.py <Name> <x_cm> <y_cm>" -nullrhi` → `Tools/Maps/<Name>.json` + `Tools/CitySample_<Name>.txt`
  2. `Tools\Import-CitySample.ps1 -Manifest Tools\CitySample_<Name>.txt`
  3. `Tools\Build-RegionMap.ps1 <Name>` (Save-As, fremde Regionen raus, Streaming aus)
  4. Gameplay-Actors per Editor-Skript (Spawns per Boden-Trace, Kontrollpunkte, Wände), NavMesh mit `WorldPartitionNavigationDataBuilder`, Modi in `Modes.json` + `Sync-Modes.cmd`.
- `p.Chaos.EnableAsyncInitBody=true` in `DefaultEngine.ini` (FastGeoStreaming verlangt es, sonst rote Bildschirmwarnung).

## Map Depot / DH_Depot (2026-10-17)

- Kopie von Lyras `L_Convolution_Blockout` in `/DowntownCore/Maps/DH_Depot` (Lyra selbst unverändert). `Tools/Python/restyle_depot.py` (Commandlet, beim ersten Mal zweimal laufen lassen: Kopie, dann Materialien) tauscht die Raster-Blockout-Materialien gegen Triplanar-Materialien ohne Gitterlinien (`/DowntownCore/Maps/Materials/MI_DH_Depot_*`: Sichtbeton, Estrich, lackierter Stahl, nasser Boden, Glas, warmweiße Leuchtröhren).
- Nur Lyras eigene Megascans-Texturen verwendet: City-Sample-Texturen sind Virtual Textures, Lyras Triplanar-Material sampelt normale Texturen, im Spiel kam schwarz heraus (im Editor sah es richtig aus).
- Die Kopie hat keine BuiltData, deshalb SkyLight auf Echtzeit-Capture (sonst wartet die Sky-Aufnahme im Spiel endlos, alles im Schatten schwarz).
- Alle Actors immer geladen (kleine Arena, im Editor sofort sichtbar). Maps in `/DowntownCore/Maps` werden über das GameFeatureData von DowntownCore gescannt.
- Modi `TDM_Depot`, `DOM_Depot`. Getestet: Domination mit Bots im Spielprozess.
- Leuchtstreifen werden zur Laufzeit in Teamfarbe umgefärbt (Lyra-Logik der Kontrollpunkte), Sprungpads/Teleporter bleiben Lyra-Optik.
- Kachelbilder ohne Editor-Symbole: Editor mit `-ExecCmds="py <skript>"` starten, das `LevelEditorSubsystem.editor_set_game_view(True)` setzt (`-ExecutePythonScript` beendet den Editor danach).

## Splash und Intro (2026-10-17)

- Splash: `Content/Splash/Splash.bmp` (Spielstart) und `EdSplash.bmp` (Editor), erzeugt von `Tools/Branding/make_splash.py` aus Menü-Hintergrund und Logo.
- Intro: `Content/Movies/DH_Intro.mp4` als Startup-Movie (`DefaultGame.ini`, MoviePlayerSettings, überspringbar). Lyras `CommonStartupLoadingScreen` ist dafür abgeschaltet: es registriert einen eigenen Startbildschirm, der Startup-Movies verdrängt (`LaunchEngineLoop.cpp`, `EngineLoadingScreen`).
- Herstellung: `Tools/Python/make_intro.py` (im GUI-Editor über `-ExecCmds="py ..."`) baut `/Game/DH/Cinematics/LS_DH_Intro` (Kamerafahrt, Straßenmitte per Boden-Traces bestimmt; City-Sample-Straßen liegen um 30°/120° gedreht) und rendert mit Movie Render Queue nach `Saved/IntroFrames`. Für den Render Lyras GameMode + Front-End-Experience (Kontrollpunkt-Blueprints brauchen Lyras Experience-System, sonst Assert). `Tools/Branding/make_intro_movie.py` schneidet mit ffmpeg: Blenden, Tagline, Logo, synthetischer Klang.
- Neu rendern: `LS_DH_Intro` löschen (Editor zu), Skript erneut starten. Löschen und Neuanlegen im selben Lauf ließ den Editor abstürzen.
- Start-Skripte blenden Bildschirmmeldungen aus (`DisableAllScreenMessages`): im ungekochten `-game`-Start melden die Editor-Toolset-Plugins Python-Fehler auf dem Bildschirm.

## Team-Armbinden (2026-10-17)

- Schmaler (3,5 cm) und für den eigenen Spieler unsichtbar (`SetOwnerNoSee`). Von hinten sah die dicke Binde wie eine rote Dose auf der Schulter aus.

## Charaktere aus dem City-Sample-Baukasten (2026-10-17)

- `Characters.json` (wie `Modes.json`): 12 Figuren aus City Samples Crowd (Geschlecht, Statur, Kopf, Outfit, Haare, Haut, Farbvarianten); `Tools/Crowd_catalog.txt` listet Köpfe und Outfits. Dazu bleiben die zwei City-Sample-Spielerfiguren (Hunter, Huntress).
- `Sync-Characters.cmd`: 1. `cs_crowd_export.py` im City-Sample-Projekt löst die Auswahl über City Samples `CrowdCharacterDataAsset` auf und rechnet die Materialwerte aus, die City Sample zur Laufzeit per MID setzt (Haut-Atlas, Outfit-Farben/Muster, Haar-Melanin; nachgebaut aus `CrowdBlueprintLibrary.cpp`). 2. Import der Pakete (`Tools/CitySample_Crowd.txt`, ~4 GB lokal). 3. `sync_characters.py` erzeugt pro Figur `/DowntownCore/Characters/Crowd/<Id>/B_DH_Char_<Id>` (Körper, Kopf, Oberteil, Hose, Schuhe je mit Lyras CopyPose-ABP, Haare als Groom am Kopf, Material-Instanzen mit den gebackenen Werten) und schreibt die Liste nach `DefaultGame.ini`.
- Kein City-Sample-C++ nötig. Plugins `HairStrands` und `RigLogic` an, `SkeletalMesh.UseExperimentalChunking=1` (sonst Dialog „Fehlende Projekteinstellungen“ beim Start).
- Auswahl: `UDHCharacterSelectComponent` am Controller (alle DH-Experiences, ersetzt `B_DH_PickRandomCharacter`). Spieler: gewählte Figur aus `GameUserSettings.ini` (`[DowntownHunting] Character=`) oder `-DHCharacter=<Id>`, per Server-RPC; Bots: zufällig. Setzt die Figur über Lyras `ULyraControllerComponent_CharacterParts` (per Reflection, Klasse nicht exportiert).
- Getestet: Viktor, Kim als Spieler; Bots bekommen gemischte Figuren.
- Armbinde: wird jetzt für den eigenen Spieler ausgeblendet, sobald der Pawn besessen ist (Lyras Kosmetik-Actor hat keinen Owner, `OwnerNoSee` griff deshalb nicht).
- Offen: Auswahl-Bildschirm im Menü, Statur/Kopf-Abgleich (Übergewicht nutzt den Normalgewicht-Kopf wie City Sample).

## Charakter-Auswahl im Menü (2026-10-17)

- Hauptmenü: Button „Character“ (`W_LyraFrontEnd`, OnButtonBaseClicked → PushContentToLayerForPlayer, Layer `UI.Layer.Menu`) öffnet `UDHCharacterSelectScreen`: Karten mit Porträt und Name, „Random“, Zurück mit Esc / Gamepad B. Wahl wird sofort gespeichert; in einem laufenden Match wechselt die Figur direkt.
- Neues normales Plugin `Plugins/DowntownUI` (lädt beim Start): Bildschirm und `UDHCharacterSettings` (Liste aus `DefaultGame.ini`, Wahl in `GameUserSettings.ini`, Ereignis `OnCharacterChosen`). Grund: das Game-Feature DowntownCore ist im Hauptmenü noch nicht geladen.
- Porträts: `Tools/Branding/Portraits` → `/Game/DH/UI/Portraits`; aufgenommen im Editor (Game-View, Figuren über den Dächern von Downtown in der Sonne).
- Armbinde rechts saß in der Schulter (rechte Armknochen zeigen mit X zur Körpermitte) – das war die „rote Dose“. Versatz für `_r`-Sockets wird jetzt gespiegelt.

## Loadout-Builder, Pausenmenü, UI-Look (2026-10-17)

- Hauptmenü und Pausenmenü: „Character“ und „Loadout“. `UDHLoadoutScreen` (DowntownUI): Primär- und Sekundärwaffe als Karten mit den HUD-Icons; doppelt gewählte Waffe wird getauscht. Liste und Standard-Loadout in `DefaultGame.ini` (`UDHLoadoutSettings`), Wahl in `GameUserSettings.ini`.
- Modus-Regel `Loadout=Player` (alle Modi außer „Sidearms Only“): Spieler bringen ihr Loadout mit (`UDHPlayerChoicesComponent` → Server), Bots bekommen zufällig Gewehr oder Schrotflinte + Pistole. Neues Loadout gilt ab dem nächsten Spawn.
- `UDHCharacterSelectComponent` heißt jetzt `UDHPlayerChoicesComponent` (CoreRedirect in `DefaultEngine.ini`).
- UI-Look: `Tools/Python/restyle_ui.py` färbt 112 Material-Instanzen von Menü und HUD (`/Game/UI`) von Lyras Cyan/Blau auf Bernstein/warmes Graphit; Teamfarben bleiben.
- Sprache: Spiel startet auf Englisch (`[Internationalization] Culture=en`), vorher Mischmasch aus deutschen Lyra-Texten und englischen eigenen. In den Optionen umstellbar.

## UI-Tests ohne Maus/Tastatur (2026-10-17)

- Entwickler-Konsolenbefehle (`Plugins/DowntownUI/.../DHDevCommands.cpp`, nicht im Shipping-Build): `DH.OpenScreen Character|Loadout|Settings`, `DH.Key <Taste> ...` (über Slate, z. B. `Gamepad_DPad_Right`, `Gamepad_FaceButton_Bottom`), `DH.Shot <Name>` (Bild mit UI nach `Saved/DHShots`), `DH.Run befehl; wait 2; befehl`. Beispiel: `-ExecCmds="DH.Run wait 45; DH.OpenScreen Loadout; wait 2; DH.Shot loadout"`.
- Geprüft: Figuren-Bildschirm mit Gamepad (Startfokus auf aktueller Figur, Navigation, A wählt, B zurück), Loadout-Bildschirm, alle Einstellungs-Tabs (Gameplay/Sprache, Video, Audio, Maus+Tastatur inkl. Belegung, Gamepad inkl. Empfindlichkeit, Deadzones, Vibration).
- Karten-Klicks melden die Karte selbst (`UDHCardButton`); vorher wurde die Karte unter dem Mauszeiger genommen, auch wenn das Gamepad drückte. Fokussierte Karte wird hervorgehoben.
- Der erste Gamepad-Druck schaltet CommonUI nur auf Gamepad um (Lyra-Verhalten). Offen: D-Pad direkt nach dem Öffnen der Einstellungen verschiebt den Inhalt um einen Tab gegenüber der Markierung – mit echtem Controller prüfen.
- Ego-Ansicht mit Crowd-Figuren: Kopf und Groom-Haare werden lokal ausgeblendet (Grooms ignorieren den First-Person-Primitive-Typ). Crowd-Kleidung passt nicht auf die Posen der Ego-Ansicht (Jackenschultern im Bild), deshalb zeigen Crowd-Figuren in Ego die Arme der City-Sample-Spielerfigur gleichen Geschlechts. Hunter/Huntress unverändert (ihr Körper hat die Waffen-Sockets).
- Getestet per DH.Run: Perspektivwechsel mit D-Pad hoch, Pausenmenü über Start, dort Character/Loadout mit dem Gamepad.
- Pistole in Ego: schwebt neben der Hand (bei allen Figuren). Versuche, die Waffe relativ zum Handknochen bzw. zur Hand der sichtbaren Figur anzuhängen, haben es nicht behoben (bei Crowd-Figuren sogar verdreht) und wurden zurückgenommen. Ursache: Ego-Pose (Lyra-Mannequin-Animation) läuft auf dem City-Sample-Skelett mit anderer Knochenausrichtung. Richtige Lösung: eigene Ego-Arme mit eigenem Skelett/Posen oder Retargeting.
