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

3 vs 3 vs 3 (`B_DH_TDM3`, Team-Setup `B_DH_TeamSetup_ThreeTeams`, grünes Team `TeamDA_Green`): Wertung `UDHTeamScoring` und Score-HUD `UDHFreeForAllHud` statt Lyras Zwei-Team-Teilen (siehe Free-for-all). Lyras Scoreboard und Namensschilder kommen mit 3 Teams klar genug.

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

## Waffen AK-47, MP5, M1911
- Meshes aus Blacksites „Classic Weapons Pack“ (Stein Games, CC0) unter `/Game/Weapons/Classic`, Mündungs-Socket `Muzzle` am Wurzelknochen (C++-Helfer `UDHEditorHelpers::SetMeshSocket`, Python darf Socket-Namen nicht setzen).
- Ketten sind Kopien von Lyras Rifle/Pistol (ID → WID → Instance → AbilitySet → GA_Weapon_Fire → GE_Damage → Waffen-Actor). Schritt 1 `make_weapons.py` (Commandlet), Schritt 2 `mcp_weapon_values.py` (MCP im laufenden Editor: EditDefaultsOnly-Werte, Schaden, Magazin, HUD-Icon). `make_weapons.py refresh` setzt nur Mesh und Socket neu.
- Das Mesh im Waffen-Actor ist eine geerbte Komponente aus `B_Weapon`: `SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint` liefert den Override der Kopie, `get_object` das Template der Elternklasse (Fehler im ersten Anlauf: Lyra-Gewehr in der Hand).
- Unveränderte Kopien im Commandlet nicht neu speichern: Absturz in AnimationData.
- HUD-Icons (`make_weapon_icons.py`): Hintergrund = breiter Median der Himmels-Aufnahme, größter Fleck, Löcher gefüllt. Weiß auf **schwarzem** transparentem Grund: Lyras Schnellleisten-Material nutzt die Farbe, nicht nur Alpha.
- Loadout ersetzt Lyras Startpistole, außer sie gehört zum Loadout (`UDHPlayerRulesComponent::GiveLoadout`). Bots: zufällig Rifle/Shotgun/AK47/MP5 + Pistol/M1911.

## Regeln ScoreLimit, TimeLimit, GunGame
- Lyras Wertung liegt in Blueprints am GameState (`B_TeamDeathMatchScoring`, `B_ControlPointScoring`): `TargetScore` (int, Lyra rechnet ihn bei Spielstart aus der Spielerzahl neu, deshalb setzt `UDHGameRulesComponent` ihn jede Sekunde) und `CountDownTime` (double, Sekunden; erst 15 = Aufwärmen, dann 720 = Spieluhr). Domination hat keine Uhr.
- Gun Game hört auf `Lyra.Elimination.Message`, Stufe pro PlayerState. Waffentausch einen Tick später über `UDHPlayerRulesComponent::RefreshLoadout` (die Feuer-Ability des Kill-Schusses läuft noch). Sieg: Ziel auf Teampunkte + 1 und den Kill erneut senden, Lyras Wertung beendet das Match (zählt den letzten Kill doppelt in der Statistik).

## Spielername, Killfeed
- Offline/LAN setzt Unreals NULL-Subsystem den Rechnernamen als Spielernamen. `UDHPlayerChoicesComponent::ServerSetPlayerName` ersetzt ihn durch `PlayerName` aus `[DowntownHunting]` in GameUserSettings.ini (oder `-DHName=`), sonst „Hunter <Zahl aus Rechnername>“. Online-Namen (Steam) bleiben, wenn nichts eingestellt ist.
- Killfeed (`W_EliminationFeed`, unten links), Hitmarker und Schadenszahlen kommen von Lyra und laufen.

## Free-for-all
- Statt Lyras 45 Zwei-Team-Knoten umzubauen: jeder Spieler bekommt ein eigenes Team (`B_DH_TeamSetup_FFA`, 16 Teams, alle rot). Lyras Freund/Feind-Logik (Bots, Namensschilder, Killfeed) passt damit unverändert.
- Ersetzt werden nur die Teile, die genau zwei Teams kennen: `B_TeamDeathMatchScoring` → `UDHFreeForAllScoring` (Phasen Warmup → Playing → PostGame wie Lyra, Kills/Tode als Stat-Tags am PlayerState, Sieg bei Kill-Limit oder Zeitende) und `W_ScoreWidget_Elimination` → `UDHFreeForAllHud` (Slot `HUD.Slot.TeamScore`). Musik-Manager fällt weg (liest die Team-Wertung).
- Experience `B_DH_FFA` entsteht aus `B_DH_TDM3` per `Tools/Python/mcp_make_experiences.py` (MCP im Editor). MCP-Eigenheit: Array verkleinern und Elemente ändern geht nicht in einem `set_properties`-Aufruf.
- `ULyraGamePhaseSubsystem` ist nicht exportiert: `K2_StartPhase` per `ProcessEvent`.
- Offen: Lyras Meldung „assigned to the Red Team“ erscheint auch im FFA.

## Steam online (Stand)
- `Play-Online.cmd` (`-CustomConfig=Steam`, App-ID 480): Steam-SDK und Client-API initialisieren, Hosten über das Menü (Start a Game, Network Online) erzeugt eine Steam-Lobby, der `SteamSocketsNetDriver` lauscht auf 7777, Match läuft.
- Suche: App-ID 480 teilen sich viele Entwickler. Lyras Suchfilter `OSSv1=true` ist ein Bool, den Steam nicht filtern kann; die Suche bekam fremde Lobbys und lief nach 15 s in einen Timeout. Fix in `Plugins/CommonUser/.../CommonSessionSubsystem.cpp`: Host und Suche setzen zusätzlich `PROJECT=<Projektname>` (String). Danach findet die Suche nur noch die eigene Lobby.
- Nicht prüfbar mit einem Steam-Account: Host und Sucher als derselbe Account bekommen die Lobby-Daten nicht (Timeout), Beitritt zu sich selbst geht über Steam P2P nicht. Test braucht einen zweiten Rechner mit anderem Steam-Account.
- Erster Start einer Map: Figuren kurz schwarz, bis Texturen/Shader geladen sind (auch offline).

## Shader/Texturen vorladen
- `UDHWarmupSubsystem` (DowntownUI) meldet sich bei Lyras `ULoadingScreenManager` als Loading-Prozessor und hält den Ladebildschirm nach jedem Map-Wechsel, bis PSO-Precaching (`PipelineStateCache::NumActivePrecacheRequests`, `FShaderPipelineCache`), Shader-Kompilierung (nur Editor-Spiel) und Textur-Streaming (≤ `DH.Warmup.StreamingThreshold` offene Anfragen) fertig sind, höchstens `DH.Warmup.MaxSeconds` (20 s). Waterfront im Editor-Spiel: 19,6 s, danach keine schwarzen Figuren mehr. Gepackt sind die Shader gekocht, der Halt fällt kürzer aus.

## Hauptmenü
- `UDHMainMenu` (DowntownUI, in Code gebaut) ersetzt `W_LyraFrontEnd` als `MainScreenClass` von `/Game/UI/B_LyraFrontendStateComponent`: Logo und Einträge rechts über einem dunklen Verlauf; Fokus (Gamepad/Tastatur/Maus) schiebt den Eintrag ein, zeigt Akzentbalken und Untertitel.
- Hintergrund `UDHMenuBackground` auf `UI.Layer.Game` (unter allen Menü-Screens, bleibt hinter Play/Character/Options sichtbar): pro Eintrag eine Kamerafahrt durch DH_Block als Video, Wechsel mit Überblendung und leichtem Zoom. Zwei MediaPlayer im Wechsel; beendet sich beim Verlassen des Front-Ends.
- Videos: `Tools/Python/make_menu_backgrounds.py` (Sequenz `LS_DH_MenuBackgrounds`, 6 Kameras, Movie Render Queue, ~40 min) → `Tools/Branding/make_menu_movies.py` (12-s-Loops, letzte 1,5 s in den Anfang überblendet) → `Content/Movies/Menu/DH_Menu_<Eintrag>.mp4` (Git LFS, beim Packen als NonUFS gestaged).

## Elimination (Runden)
- `UDHRoundScoring` (Unterklasse der FFA/Team-Wertung, über virtuelle Hooks `StartClock`/`OnKill`/`OnDeath`/`OnClockExpired`): Rundensieg, wenn von den anderen Teams niemand mehr lebt, bei Zeitende (TimeLimit, Standard 2 min) mehr Überlebende; Rundensiege sind der Team-Punktestand, ScoreLimit (Standard 4) gewinnt. Ansagen („ROUND 2“, „BLUE TEAM WINS ROUND 1“) im Score-HUD.
- Kein Respawn: `AbilitySet_DH_Rounds` = Lyras `AbilitySet_Elimination` ohne `GA_AutoRespawn`. Rundenstart: Pawns zerstören, `ALyraGameMode::RequestPlayerRestartNextFrame` per Reflection.
- Lyras AutoRespawn setzt sonst die Schnellleiste zurück; ohne ihn stand die Figur nach dem Neustart ohne Waffe in A-Pose. `GiveLoadout` schaltet den aktiven Slot jetzt kurz weg und zurück, das rüstet die Waffe am neuen Pawn aus.
- Experience `B_DH_Elimination` (Kopie von `B_DH_TDM`) und das AbilitySet entstehen per `Tools/Python/mcp_make_experiences.py`.

## Search & Destroy
- `UDHSearchAndDestroyScoring` (Unterklasse von `UDHRoundScoring`): Angriff/Verteidigung wechselt jede Runde. Bombenplätze `ADHBombSite` (Strichkreis 4,5 m, Buchstabe, rotes Blinklicht wenn scharf) spawnt der Server am ersten und letzten Kontrollpunkt der Map. Legen: Angreifer 5 s allein im Platz; Entschärfen: Verteidiger 7 s allein am gelegten Platz; Fortschritt baut sich ab, wenn niemand drin ist. Bombe 40 s (Rundenuhr zeigt den Countdown), Explosion tötet alle im Umkreis 12 m (`DamageSelfDestruct`), Effekt/Sound von Lyras Granate.
- Sieg: Verteidiger tot → Angreifer; Angreifer tot vor dem Legen → Verteidiger; nach dem Legen müssen die Verteidiger noch entschärfen; Zeit ohne Bombe → Verteidiger.
- HUD `UDHObjectiveHud` (vom PlayerRulesComponent in den Viewport gelegt): Aufgabe, Marker mit Entfernung über A/B, Fortschrittsbalken.
- Bots: `UDHSearchAndDestroyScoring::GuideBots` (alle 0,5 s) überschreibt den Blackboard-Wert `MoveGoal` von Lyras Bot-Baum (dessen MoveTo folgt Änderungen): Angreifer in ihren Platz, Verteidiger in 6–9 m Ring um ihren, nach dem Legen alle zur Bombe; Platz pro Bot stabil über die Player-ID. Nahkämpfe (Gegner < 15 m) bleiben beim Baum, sonst schießen Bots weiter, ziehen aber zum Ziel. Untätige Bots (Baum findet kein Ziel) bekommen ein eigenes `MoveToLocation`. Ergebnis im Bot-Match: Bombe gelegt und explodiert, daneben Auslöschungen und Zeitabläufe. Erste Versuche ohne das (nur `MoveToLocation` / nur Kontrollpunkte) blieben ohne Bombe.
- DH_Block und DH_Park ohne World-Partition-Streaming (`Tools/Python/disable_streaming.py`, Helfer `UDHEditorHelpers::SetWorldPartitionStreaming`): vorher war die Navmesh nur um den Spieler geladen, weit entfernte Bots bekamen Teilpfade. Kosten: keine spürbaren, TDM Downtown mit 8 Spielern 118 FPS (RTX 4090, Editor-Spiel, `StartFPSChart`).
- Testhilfe: `DH.Teleport X Y Z [Yaw]` (Dev-Konsole).
- Effekte, die erst beim Ereignis geladen werden, kompiliert das Editor-Spiel dann erst: im BeginPlay vorladen.
- Experience `B_DH_SearchAndDestroy` per `Tools/Python/mcp_make_experiences.py`. Fix dabei: FFA wurde aus dem schon umgebauten TDM3 kopiert und bekam die Team-Wertung; die Ersetzungstabelle deckt das jetzt ab.

## Play-Screen
- `UDHPlayScreen` (DowntownUI) ersetzt Lyras `W_ExperienceSelectionScreen` hinter „PLAY“ im Hauptmenü: alle Modi als Karten (Kachel, Titel, Map) in der Reihenfolge von `Modes.json` (`sync_modes.py` schreibt `MenuOrder` in die ExtraArgs, das Spiel ignoriert die Option), Beschreibung des fokussierten Modus, Schalter NETWORK (Offline/LAN/Online) und BOTS, „JOIN A GAME“ öffnet Lyras Server-Browser. Online meldet erst per `UCommonUserSubsystem::TryToLoginForOnlinePlay` an.
- Lyra-Typen (`ULyraUserFacingExperienceDefinition`, `CreateHostingRequest`) per Reflection, DowntownUI linkt LyraGame nicht.
- `UDHJoinScreen` („JOIN A GAME“) ersetzt Lyras Server-Browser: NETWORK LAN/ONLINE, REFRESH, Zeilen mit Host, Modus, Map, Spieler, Ping; Auswahl tritt bei. Online-Suche meldet erst an.
- Getestet LAN über die Menüs (`Tools/Test-LanMenus.ps1`, zwei Instanzen): Host wählt im Play-Screen LAN + TDM Downtown, Client findet „TDM Downtown, Downtown, 1/16, 95 ms“, tritt bei und spielt mit.
- Gamepad im Play-Screen: oberste Kartenreihe ↑ führt zu den Schaltern, Schalter ↓ zur ersten Karte (explizite Navigationsregeln).

## Match-Ende
- `UDHMatchEndScreen` (vom Score-HUD eingeblendet, sobald `UDHFreeForAllScoring` das Match beendet): VICTORY/DEFEAT (Team bzw. Spieler, repliziert `WinnerTeam`/`WinnerPlayer`), Sieger, beste 8 Spieler mit Kills/Toden, eigener Spieler orange; blendet mit leichtem Zoom ein. Gilt für FFA, Three Teams, Elimination, S&D und alle Modi mit eigener Wertung; Lyras TDM/Domination zeigen weiter Lyras Ende.
- Unity-Build: gleichnamige Hilfsfunktionen in anonymen Namespaces kollidieren, sobald Dateien zusammen kompiliert werden; gemeinsamer Text-Helfer `DHScreenStyle::MakeText`.

## Scoreboard (Tab / Select)
- Gehalten `IA_ShowScoreboard` (ShooterCore, Tab und Gamepad Select): `UDHPlayerRulesComponent` blendet `UDHMatchEndScreen` im Live-Modus ein (alle Spieler, nach Team in Teamfarbe, Kills, Tode, K/D; aktualisiert 2×/s, Zeilen werden nur bei Spielerwechsel neu gebaut). Läuft in allen Modi (Kills/Tode sind Stat-Tags am PlayerState, die auch Lyras Wertung schreibt).
- Lyras Leaderboard-Ability (`GA_ShowLeaderboard_*`, kennt nur 2 Teams) ist raus: DH-Sets `AbilitySet_DH_Match`, `AbilitySet_DH_Rounds`, `AbilitySet_DH_ControlPoint` in allen DH-Experiences (`mcp_make_experiences.py`).
- Testhilfe: `DH.Key Gamepad_Special_Left:3` hält die Taste 3 s.

## Minimap
- `UDHMinimap` (oben rechts, 220 px, ~70 m Ausschnitt, Norden oben): einmal pro Map nimmt eine orthografische Scene-Capture (`SCS_BaseColor`, 1024²) den Bereich aller Spawns + Bombenplätze von 250 m Höhe auf; Pfeil = Blickrichtung, Punkte = Teammitglieder in Teamfarbe, Buchstaben = S&D-Plätze (rot, wenn scharf). Gegner werden nicht gezeigt.
- Scene-Capture schreibt Alpha 0 (invertierte Deckkraft) → Bild war unsichtbar; die Pixel werden einmal ausgelesen und aufgehellt in eine normale Textur kopiert (gecacht für spätere Pawns derselben Map).
- Erste Versuche: `SCS_FinalColorLDR` aus großer Höhe überbelichtet; Kamera bei +500 m sah nur Grau (Himmelskuppel/Alpha).

## Killcam
- Server (`UDHGameRulesComponent`, hört auf `Lyra.Elimination.Message`) ruft beim Opfer `UDHPlayerChoicesComponent::ClientShowKiller(Killer, HP, Entfernung)`. Client: `UDHKillcam` wartet 1 s (eigener Tod), fährt dann eine eigene Kamera (CameraActor, weich nachgeführt) hinter den Killer und zeigt „KILLED BY Name · x m away · y HP left“; Ende nach 4,5 s oder beim Respawn.
- Eigene Kamera statt Pawn-Kamera des Killers: Lyras Kamera-Modi sind nur für den eigenen Spieler gebunden.

## Map Overpass / DH_Freeway (2026-10-18)
- City-Sample-Ausschnitt 250 × 250 m um (125 m, -125 m): unter und neben der Hochstraße (Pfeiler, Bauzäune, geparkte Autos), Bürotürme ringsum. Gefunden mit `Tools/Python/cs_survey.py` (zählt Freeway-/Baum-/Gebäude-Actors in 250-m-Zellen der City-Sample-Stadt, höchste Gebäude).
- `cs_region.py` nimmt jetzt die Freeway-Data-Layer mit (`Freeway_Deck`, `_Supports`, `_Props`, `_Decals`, `_Collision`); vorher fehlte die Hochstraße.
- Gameplay per `Tools/Python/mcp_region_gameplay.py` (verallgemeinert aus dem Waterfront-Skript: 16 Starts auf Straßenhöhe – Fahrbahndecks und Dächer liegen höher und fallen raus –, 3 Kontrollpunkte, Navmesh-Bounds, Randwände), Navmesh mit dem WorldPartitionNavigationDataBuilder, Streaming aus (`disable_streaming.py`).
- Modi: TDM, FFA, Domination, Search & Destroy auf Overpass; `Play-Downtown.cmd OVERPASS`.
- Stabilität: Commandlets in City Sample und hier stürzten mehrfach an wechselnden Stellen ab (StructuredArchive-Assertion, privilegierte Instruktion, Zugriffsverletzung, Array-Index mit Müllwerten), dazu zwei „Internal compiler error“ und ein Ausfall des PCs. Muster passt zu instabiler CPU (i9-13900K, bekanntes Raptor-Lake-Problem): BIOS mit aktuellem Intel-Microcode prüfen.

## Map Rooftops / DH_Roofs (2026-10-18)
- City-Sample-Ausschnitt 160 × 160 m um (-342 m, 181 m), gefunden mit `Tools/Python/cs_roofs.py` (bewertet 120-m-Fenster nach Dachfläche in einem 10-m-Höhenband: viele gleich hohe Nachbardächer).
- Gameplay mit `Tools/Python/roof_map.py`, läuft lokal gegen den offenen Editor (MCP): Editor macht nur Traces und Actors (Skript-Sandbox ist langsam, ~17 min für ~1400 Raster- und alle Profil-Traces), Rechnung lokal. 4-m-Raster → Dachinseln (≥ 12 m über Straße, Höhensprünge ≤ 60 cm) → Stege zwischen Nachbardächern über genau eine Lücke, mit Rampe über die Brüstung → größte verbundene Gruppe wird gespielt (4 Dächer 24–33 m, 12 Stegteile).
- Absturz: `KillZ` der WorldSettings 6 m über der Straße; Lyra `FellOutOfWorld` → `DamageSelfDestruct`, zählt als Tod (getestet).
- Stege: `/Game/DH/Meshes/SM_DH_Plank` (Würfelkopie mit `MI_DH_Plank_Steel`, Kopie von Depots Triplanar-Stahl). Eigenes Mesh, weil Property-Änderungen an Actors über MCP nicht gespeichert werden (`save_assets` speichert die External-Actor-Pakete nicht, `save_actor` findet sie nicht); neue Actors und Assets werden gespeichert.
- Modi: TDM, FFA, Domination; `Play-Downtown.cmd ROOFS`.

## Map Gridlock / DH_Gridlock (2026-10-18)
- City-Sample-Ausschnitt 250 × 250 m um (-500 m, -125 m) auf der Hochstraße (Freeway-Zellen aus `cs_survey.py`).
- `Tools/Python/freeway_map.py` (nutzt die MCP-Helfer aus `roof_map.py`): 5-m-Raster, Deck = längste zusammenhängende Fläche 5–30 m über der Straße (240 m, Hauptachse per PCA), Stau auf allen Spuren (Gegenrichtung gedreht, Lücken zum Durchlaufen), quergestellte Vans an beiden Enden, 16 Starts zwischen den Autos, 3 Kontrollpunkte entlang des Decks, unsichtbare Wände an den Auffahrten, KillZ unter dem Deck.
- Nur `vehCar_vehicle03`, `vehCar_vehicle05`, `vehVan_vehicle01` haben Kollision (die anderen City-Sample-Autos sind reine Optik); Glas als zweites Mesh (`Transparent/SM_All_Trans_*`) an gleicher Stelle.
- Modi: TDM, FFA, Domination, Search & Destroy; `Play-Downtown.cmd GRIDLOCK`. Getestet: TDM mit Bots, Killcam.

## Intro mit Bots (2026-10-18)
- `Content/Movies/DH_Intro.mp4` neu: echte Bot-Gefechte statt leerer Kamerafahrt. Aufnahme im Spiel mit `ADHIntroDirector` (DowntownCore, Konsole `DH.Intro [Sekunden]`): eigener Pawn wird entfernt, Kamera schneidet alle 3 s zwischen Bots, die einen Gegner (`TargetEnemy` im Blackboard) in Sichtlinie haben (über die Schulter / flach seitlich, Seite gespiegelt wenn eine Wand im Weg ist), die letzten 6 s Kranfahrt: erst in die Straßenmitte (Seiten-Traces zu den Fassaden), dann senkrecht hoch. Jeder Frame als PNG ohne UI nach `Saved/IntroFrames`.
- Aufnahme (Ordner `Saved/IntroFrames` vorher löschen):
  `UnrealEditor.exe DowntownHunting.uproject /Game/DH/Maps/DH_Block?Experience=B_DH_TDM?NumBots=10 -game -windowed -ResX=1920 -ResY=1080 -UseFixedTimeStep -FPS=30 "-ExecCmds=DisableAllScreenMessages, DH.Run wait 45; DH.Intro 20"`
  Fester Zeitschritt: Spielzeit läuft pro Frame 1/30 s weiter, egal wie lange der Screenshot dauert.
- Schnitt: `Tools/Branding/make_intro_movie.py` (Tagline, Logo, Drone, synthetische Schusssalven bis 14 s, Whoosh zur Kranfahrt). `Tools/Python/make_intro.py` (MRQ-Kamerafahrt) ist damit abgelöst.
- Hinlegen: keine Liege-Animation in Lyra, City Sample, Blacksite oder Fab-Cache. Ohne Animation nicht gebaut.

## Killstreaks (2026-10-19)
- `DHKillstreaks` (DowntownCore): Kills ohne Tod zählen im Stat-Tag `DH.Streak` (Lyra-PlayerState, repliziert). 3 = Radar (Gegner 12 s rot auf der Minimap, fürs ganze Team, entfernte am Rand), 5 = Luftschlag (`ADHAirstrike` auf die größte Gegnergruppe: 3 s blinkende rote Leuchtfackel, 6 Einschläge in 9 m Umkreis; Schaden über Lyras Damage-GE mit dem Spieler als Instigator, also zählen die Kills; wer ein Dach über dem Kopf hat, ist sicher), 7 = Hunter-Modus (volle Gesundheit, 20 s +30 % Tempo, gesetzt auf Server und Client).
- Zeitbelohnungen als Server-Zeit in Stat-Tags (`DH.Streak.RadarUntil`, `DH.Streak.HunterUntil`), Banner beim Spieler über `UDHPlayerChoicesComponent::ClientStreak` (`UDHStreakBanner`).
- Test: Konsole `DH.Streak N` schreibt dem ersten Spieler N Kills gut.

## Treffer-Feedback (2026-10-19)
- Lyra hat schon Hitmarker und Treffer-/Kopfschuss-/Kill-Sounds (GameplayCues) sowie Kopfschuss-Schaden (Weak-Spot-Material, doppelter Schaden). Dazu neu: rotes ✕ am Fadenkreuz beim Kill und „HEADSHOT“-Schriftzug (`UDHHitMarker`, über `UDHPlayerChoicesComponent::ClientHitFeedback`), Kopfschuss-Kills als Stat-Tag `DH.Headshots`.
- Kopfschuss-Erkennung auf dem Server (`UDHGameRulesComponent::OnDamage`): Lyras `Lyra.Damage.Message` hat kein Trefferergebnis. Schaden wird durch die Distanzabnahme der aktiven Waffe (`DistanceDamageFalloff`, per Reflection) geteilt und mit dem niedrigsten bisherigen Wert dieser Waffeninstanz (= Körpertreffer) verglichen; > 1,45-fach = Kopf. Ein Kopfschuss als allererster Treffer einer Waffe wird nicht erkannt. Ein Strahl-Test (Blick vs. Kopfknochen) scheiterte an der Streuung.

## Waffen-Aufsätze (2026-10-19)
- Pro Waffe im Loadout ein Aufsatz (Loadout-Bildschirm, Zeile unter jeder Waffe): Großes Magazin (+50 %, voll geladen), Lasermodul (Streuung enger: `SpreadExponent` ×2,5, beim Zielen ×0,75), Langer Lauf (Schadensabfall 60 % weiter: Zeitachse der `DistanceDamageFalloff`-Kurve ×1,6).
- Gespeichert als `Waffe:Aufsatz` im Loadout (GameUserSettings), Server prüft die Namen (`UDHPlayerChoicesComponent::ServerSetLoadout`). Magazin über Item-Stats (`Lyra.ShooterGame.Weapon.MagazineSize`/`MagazineAmmo`), Markierung `DH.Attachment.*` am Inventar-Item; Streuung und Reichweite an jeder neuen Waffeninstanz (Lyra erzeugt sie bei jedem Ausrüsten) auf Server und eigenem Client (`UDHPlayerRulesComponent::ApplyAttachments`). Lyras Waffenklassen sind nicht exportiert: Zugriff per Reflection (`DHWeaponAccess.h`).

## Fortschritt und Level (2026-10-19)
- `FDHProgression` (DowntownUI): XP lokal in GameUserSettings.ini (wie die anderen Menü-Wahlen; online behält jeder seinen Stand). Level n beginnt bei 500·(n-1)·n XP (1000, 3000, 6000, 10000 …).
- XP am Matchende (`DHXP`, DowntownCore): 200 fürs Spielen, 100 pro Kill, +50 pro Kopfschuss-Kill, 500 für den Sieg. Vom End-Screen der DH-Modi (Zeile „+XP · LEVEL n“, Level-up mit Freischaltungen) oder beim Verlassen der Map (`UDHGameRulesComponent::EndPlay`, Domination & Co.). Mindestens 60 s zwischen zwei Vergaben: bei Seamless Travel endet die Regel-Komponente ein zweites Mal in der Übergangswelt.
- Freischaltungen: MP5 und Großes Magazin Level 2, M1911 und Laser Level 3, AK47 Level 4, Langer Lauf Level 5 (Loadout-Bildschirm zeigt gesperrte Karten gedimmt mit Level). Hauptmenü oben links: Level, XP-Balken.

## Neue Modi: King of the Hill, Hardcore (2026-10-19)
- King of the Hill (`UDHHillScoring`, Experience `B_DH_Hill` = Kopie von B_DH_TDM, gebaut mit `Tools/Python/make_mode_experiences.py`, das die Helfer aus `mcp_make_experiences.py` mitschickt): ein Hügel (Ring wie die Bombenplätze, Buchstabe K, auch auf der Minimap) an einem Kontrollpunkt der Map, wandert jede Minute weiter. Steht nur ein Team drin, bekommt es 1 Punkt/s (Team-Score), umkämpft gibt es nichts, Kills zählen nicht. 120 Punkte oder Zeitende. Bots werden wie bei S&D über `MoveGoal` zum Hügel geschickt. Modi auf Downtown, Overpass, Gridlock.
- Hardcore: Regel `Hardcore=1` (repliziert, `UDHGameRulesComponent::IsHardcore`): 30 Lebenspunkte (Health/MaxHealth per Attributname gesetzt, Lyras HUD zeigt Prozent), keine Minimap. Modi „Hardcore TDM“ auf Downtown und Gridlock. `Play-Downtown.cmd KOTH` / `HC`.

## Neue Modi: Infected, Capture the Flag (2026-10-19)
- Infected (`UDHInfectedScoring`, `B_DH_Infected`): zu Beginn ist ein zufälliger Spieler infiziert (zweites Team), alle anderen überleben (erstes Team; Teamwechsel über `ULyraTeamSubsystem::ChangeTeamForActor`, neuer Pawn über `RestartPlayer`). Wer als Überlebender stirbt, wechselt zu den Infizierten. Infizierte haben nur die Schrotflinte (`UDHGameRulesComponent::GetLoadoutItems`) und laufen 20 % schneller. Infizierte gewinnen, wenn keiner mehr übrig ist, Überlebende bei Zeitende (Standard 5 min). Anzeige „SURVIVORS n · INFECTED m“. Getestet: 9 Bots, Ausbreitung bis auf 1 Überlebenden.
- Capture the Flag (`UDHFlagScoring`, `ADHFlag`, `B_DH_CTF`): Basen (Ring mit Team-Buchstaben) am ersten und letzten Kontrollpunkt, Flagge (3-m-Mast, Tuch in Teamfarbe, dreht sich langsam, Licht). Gegnerflagge berühren = aufnehmen (hängt am Rücken), eigene Basis erreichen, während die eigene Flagge daheim ist = Punkt. Träger stirbt = Flagge liegt, eigene liegende Flagge berühren = zurück, sonst nach 20 s automatisch. 3 Punkte gewinnen. Bots: Träger laufen heim, gerade Spieler-IDs greifen an, ungerade verteidigen bzw. jagen den Träger. Minimap: Basen und „F“ in Teamfarbe.
- `Tools/Python/make_mode_experiences.py` baut Hill, Infected und CTF aus B_DH_TDM. Tests im Spiel: `Tools/Test-Game.ps1`.

## Showcase-Video (2026-10-19)
- `Tools/Branding/make_showcase.py`: nimmt Clips im ungekochten Spiel auf und schneidet `Build/Showcase/DH_Showcase.mp4` (~2,5 min: Intro, Menüs, Spielfeatures mit HUD, Modi und Maps mit Regie-Kamera, Abspann; Titelkarten, Bauchbinden, synthetische Musik). `rec <Clip>` nimmt einen Clip neu auf, `cut` schneidet nur.
- Aufnahme mit fester Zeitschrittweite (`-UseFixedTimeStep -FPS=30`): jedes Bild ist 1/30 s Spielzeit, egal wie lange der PC braucht. `-nomovie`, sonst hängt der Start-Film. Konsole: `DH.Record <s> <Name> [noui]` (mit HUD, Echtzeiten in times.txt), `DH.Intro <s> <Name> nocrane` (Regie-Kamera, ohne HUD) nach `Saved/Record/<Name>`.
- Menü-Hintergrundvideos laufen in Echtzeit: `UDHMenuBackground` setzt bei fester Zeitschrittweite die Abspielrate auf Spielzeit/Echtzeit. Bildschirmaufnahme ging nicht (Remote-Desktop, Monitor kleiner als 1080p), Echtzeit-Screenshots schaffen nur ~5 Bilder/s.

## Intro neu: Kamerafahrt mit Gefechten, überspringbar (2026-10-20)
- 15 s statt 23,5: Kamerafahrt wie im ersten Intro (tief die Straße entlang, Kreuzung, Kran über den Block), aber im laufenden Bot-Match aufgenommen: `DH.Intro 15 intro flight` (`ADHIntroDirector`, Modus „flight“) stellt die Bots vorher entlang der Straße auf (Teams auf gegenüberliegenden Seiten) und schickt sie während der Fahrt per `MoveGoal` vor die Kamera. Aufnahme: `python Tools/Branding/make_showcase.py rec intro`, Schnitt: `Tools/Branding/make_intro_movie.py` („GILDE GAMES PRESENTS“ über der Straßenszene, Tagline, Logo-Einschlag bei 10 s).
- Kein Start-Film mehr: Unreals Startup-Movies lassen sich erst überspringen, wenn die Engine fertig geladen hat (`FDefaultGameMoviePlayer::OnAnyDown` prüft `IsLoadingFinished`), das dauerte etwa so lang wie der Film. Stattdessen spielt `UDHIntroScreen` (DowntownUI) das Video beim ersten Öffnen des Hauptmenüs pro Start ab (Ton über eine `UMediaSoundComponent` am PlayerController); jede Taste, Maustaste oder Gamepad-Taste überspringt (Slate-Input-Preprocessor, die Eingabe löst keinen Menüpunkt aus). `-nomovie` unterdrückt es. `Movies` wird komplett als Nicht-UFS-Datei mitgepackt.

## Sniper, Controller-Belegung (2026-10-25)
- Sniper „M700“ (Classic Weapons Pack, CC0, aus dem Blacksite-Quellordner): `Tools/Python/import_classic_weapons.py M700` (FBX + PBR-Material), `make_weapons.py Sniper` (Kopie von Lyras Gewehrkette), Werte per MCP aus `mcp_weapon_values.py` (`Editor().script(text, ONLY=['Sniper'])`): 80 Schaden (Kopf 160), 1,25 s zwischen Schüssen, 5 + 20 Schuss, kein Schadensabfall bis 600 m, aus der Hüfte ungenau, beim Zielen fast exakt. Icon aus einer Editor-Seitenansicht (`CaptureViewport`) mit `make_weapon_icons.py ... Sniper`. Freischaltung ab Level 3.
- Zielfernrohr: Zielen mit der Sniper schaltet auch aus der Schulteransicht in die Ego-Kamera (Zoom auf 14° FOV, Waffe und Arme ausgeblendet) und blendet `UDHScopeOverlay` ein (schwarzer Rand, runde Linse, Fadenkreuz, selbst erzeugte Textur).
- Loadout: die Primärwaffe wird beim Spawn ausgerüstet (vorher die Startpistole in Slot 1).
- Gamepad (in `IMC_DH_Perspective`, Priorität 10 über ShooterCore): Nahkampf rechter Stick, Y Waffe wechseln, RB reserviert für die zweite Fähigkeit (`IA_DH_Ability2`, blockiert Lyras RB-Nahkampf), Steuerkreuz oben Ego/Schulter.
- Spiel-Exe: `DowntownHunting.exe` (Target `Source/DowntownHunting.Target.cs`, Lyras Spiel-Target mit Steam-Konfiguration).

## Spielauswahl, Loadouts, Fähigkeiten (2026-10-25)
- Spielen-Bildschirm: erst Modus (jeder einmal), darunter die Maps, die den Modus haben; Karte anklicken startet. Gehostet wird weiter die passende Lyra-Experience (Modus × Map).
- 5 Loadouts (GameUserSettings `Loadout`, `Loadout2`..`Loadout5`, `ActiveLoadout`), Einträge: Primär, Sekundär (je mit Aufsatz), Fähigkeit, Perk, 2. Fähigkeit. Der gewählte Tab ist das aktive Loadout.
- Fähigkeiten (`DHAbilities`, Katalog in `UDHLoadoutSettings::GetAbilities`): Dash 6 s, Mega-Sprung 10 s (Level 2), Adrenalin 4 s +50 % Tempo, 20 s (Level 3), Recon-Puls 5 s Radar fürs Team, 30 s (Level 4), Tarnung 5 s unsichtbar für alle und für die Bots (AI-Sicht abgemeldet, Gedächtnis gelöscht), endet beim Schießen/Nahkampf/Granate, 35 s (Level 6). Perks: Schnell aufladen −25 % Abklingzeit (Level 5), Lang anhaltend +50 % Dauer (Level 7). 2. Fähigkeit ab Level 8.
- Auslösen: Lyras Dash-Taste (Shift) für die erste, RB / `IA_DH_Ability2` für die zweite. Lyras eigene Dash-Fähigkeit wird dem Pawn auf dem Server genommen. Client stößt sofort an (wie Slide), der Server prüft die Abklingzeit selbst; zeitliche Effekte als Server-Endzeit in Stat-Tags am Player State (wie die Killstreaks). HUD unten Mitte (`UDHAbilityHud`).
- Controller-Belegung: Hauptmenü „CONTROLLER“ (`UDHControllerScreen`): Aktion wählen, Taste drücken (Start bricht ab), belegte Taste wird getauscht, „Restore defaults“. Gespeichert als `Gamepad_<Aktion>` in GameUserSettings.ini. Angewendet (`ApplyBindings`, Hauptmenü und Pawn-Input): Lyras Aktionen sind „player mappable“, ihre Tasten kommen aus dem Enhanced-Input-Benutzerprofil, also dort gesetzt; DowntownCores Mappings im geladenen Asset. Zweitbelegungen der Assets (Lyras Nahkampf auf RB) bekommen dieselbe Taste, fremde Aktionen auf einer gewählten Taste fliegen raus. `DH.PadDump` loggt die wirklich gemappten Tasten. Tastatur: 2. Fähigkeit auf F (zur Laufzeit ergänzt), Rest über Lyras Optionen.

## Sound (2026-10-25)
- Bestand: Lyras Waffen-Cues haben schon HRTF-Attenuation mit Distanz-Tiefpass und Verdeckung, Early Reflections und Kugel-Zischen (`WeaponAudioFunctions`). Alle neuen Waffen liefen aber über Lyras Gewehr-Cue mit dem Innenraum-Set `Rifle2`.
- Eigene Schuss-Cues (Kopien von `GCN_Weapon_Rifle_Fire` in `/DowntownCore/GameplayCues`, `Tools/Python/mcp_weapon_sounds.py`): AK47 mit Lyras ungenutztem Außen-Set `MSS_Weapons_Rifle_Fire` (mit Fern-Layer), MP5 mit dem Pistolen-Set (9 mm), Sniper mit dem Außen-Set plus schwerem Knall und langem Nachhall durch die Straßen (`GameplayCue.DH.Sniper`, vom Server bei jedem Sniper-Schuss). Die Feuer-Fähigkeiten nutzen die neuen Tags (`Gameplay Cue TagFiring`).
- `UDHSoundCue` (C++, Blueprint `GCN_DH_Sounds` auf `GameplayCue.DH`, bekommt alle Unter-Tags): Fähigkeiten-Sounds für alle hörbar (Dash, Mega-Sprung, Adrenalin, Recon-Puls, Tarnung). Bombe und Luftschlag zusätzlich mit Lyras Explosions-Set (Außen-Fernlayer).
- DowntownCore hat keine „Add Gameplay Cue Path“-Action (per MCP nicht anlegbar), deshalb registriert `UDHGameRulesComponent::BeginPlay` den Pfad zur Laufzeit; gepackt über `DirectoriesToAlwaysCook`. Cues in `/Game` gingen nicht: die werden beim Start geladen, bevor ShooterCore/DowntownCore gemountet sind. Prüfen: `GameplayCue.PrintGameplayCueNotifyMap`, `AbilitySystem.DisplayGameplayCues 1`.

## Arcade-Look, Charakter-Splash, Dev-Modus, Cook-Fix (2026-10-25)
- Gepackter Build hatte nur per String geladene Assets nicht dabei: Porträts, `/DowntownCore/Weapons` (AK47, MP5, Sniper), `/DowntownCore/Input` (Perspektive, 2. Fähigkeit), Credits. Jetzt in `DirectoriesToAlwaysCook` (`/DowntownCore/{GameplayCues,Weapons,Input,Camera}`, `/Game/DH/UI`, `/Game/UI/Credits`). Prüfen nach dem Packen: `Saved/Cooked/Windows/DowntownHunting/Metadata/ChunkManifest/pakchunk*.txt`.
- Palette (`DHScreenStyle`, gilt für alle Menüs und HUDs): Neon-Gelb `Accent` (Titel, Balken, Outline), Magenta `Selected` (Gewähltes, Titel-Schatten), Cyan (Klick). Orbitron ab 28 pt im Schnitt „Black“ mit hartem Magenta-Schatten.
- `UDHCardButton`: schräg (−6°), Neon-Outline bei Fokus/Hover, Feder-Animation mit Überschwinger (`Animate`, aus dem Tick der Screens), Blip bei Fokus, Klick-Sound mit zufälliger Tonhöhe. Hauptmenü: aktiver Eintrag auf schrägem gelbem Band, schwarzer Titel.
- Charakterauswahl (Clips für alle 15, LTX-2 aus der Splash-Art, vorwärts+rückwärts als nahtloser Loop): Splash-Art (Comic, MuseForge Flux 2 Klein mit dem Porträt als Referenz, `Tools/Branding/make_splash_art.py`) in Karten und als großes Panel links, das bei Fokus/Hover federnd hereinschwingt; Sprachzeile pro Figur nach 0,3 s Ruhe (`make_voice_lines.py`, Qwen3 Voice Design). Optional Clip `Content/Movies/Characters/DH_Char_<Id>.mp4` über dem Standbild. Import: `Tools/Python/import_menu_art.py` (Commandlet).
- MuseForge: REST `/api/v1/generate` mit flachem Body (wie das MCP-Tool): `image_refs` für Bild-Edits, `image_start` + `image_prompt_type: "S"` für Videos. Dateien per `docker cp museforge:/workspace/app/outputs/tmp-test/<name>`. `attention_mode` stand auf nicht installiertem `sage2` (alle Jobs scheiterten), auf `auto` gestellt.
- Dev-Modus: `fg.txt` mit `DEVDBG=true` neben `DowntownHunting.exe` (oder im Projektordner): alles freigeschaltet, Lyras Cheat-Manager für den lokalen Spieler (Host: `God`, `UnlimitedHealth` …), Hinweis im Menü.

## Packaging: UBT-Fehler (2026-10-25)
- UBT 5.8 stürzte in `GitSourceFileWorkingSet` ab (NullReferenceException). Lokal abgeschaltet über `Saved/UnrealBuildTool/BuildConfiguration.xml` mit `<SourceFileWorkingSet><Provider>None</Provider></SourceFileWorkingSet>`. Die Datei liegt nicht in Git, nach einem Löschen von `Saved` neu anlegen.
- `DowntownHunting.Target.cs` braucht `bOverrideBuildEnvironment = true`, weil `CustomConfig = "Steam"` sonst mit der installierten Engine abgelehnt wird.

## Feedback-Runde: Lyra raus, Steuerung, HUD, Dev-Tools, Loadouts, Menü (2026-10-25)
- Kein „Lyra“ mehr sichtbar: eigenes App-Icon (`Tools/Branding/make_icon.py` → `Build/Windows/Application.ico`), Firma/Copyright „Gilde Games“ in `DefaultGame.ini`, Credits-Zeile ersetzt (gleich lange Zeichenkette direkt im Paket, weil der Text als Pin-Default im Graph steht). `Package.cmd` leert `Build/Packaged` vorher (die alte `LyraGame.exe` lag noch im Archiv), Zip ohne `.pdb` und ohne `fg.txt`; die Projekt-`fg.txt` wird danach nur lokal neben die Exe kopiert.
- D-Pad übersprang Karten: Slate-Navigation sucht Nachbarn über das *Render*-Rechteck; geschert/gepoppte Buttons ragten über den Nachbarn. `UDHCardButton` bleibt jetzt ein gerades Rechteck, Schräge, Pop, Farbe und Outline liegen auf einer inneren `SBorder`.
- Y wechselte hin und zurück: `IMC_DH_Perspective` hatte Y → CycleForward ohne Trigger, Lyra feuerte jeden Frame. Alle Tasten-Aktionen dort jetzt mit `Pressed` (`Tools/Python/setup_input.py` schreibt das Mapping).
- Neue Belegung: Sprint LS / Shift (`IA_DH_Sprint`, Gamepad Umschalten, Tastatur Halten; endet bei Schuss, Zielen, Ducken, ohne Vorwärts), Ducken im Sprint = Rutschen, Fähigkeit LB / E, 2. Fähigkeit D-Pad rechts / F, Granate RB / Q, Nahkampf RS / C, Waffe Y. Lyras Tastatur-Profil: Dash Shift → E, E nicht mehr Nahkampf (nur solange nicht selbst umgelegt).
- Ego: neue Waffe schwingt beim Wechsel von unten ins Bild (`RaiseTime` usw. im Kamera-Modus). Im Spiel noch nicht Bild für Bild angesehen.
- Fähigkeiten-HUD: Kachel mit Icon (`/Game/DH/UI/Abilities`, MuseForge), Tastenname des aktuellen Geräts, Abklingschatten mit Sekunden, Cyan-Rahmen wenn aktiv, Pop + Ton wenn bereit.
- Dev-Tools (`UDHDevScreen`): F10 oder VIEW halten + Y, auch Hauptmenü „DEV TOOLS“. Schalter Keine Abklingzeit, God, Endlos-Munition (gelten auf dem Host), Zeitlupe, FPS; Level hoch, Killstreak +3, Bot +/−, Heilen, Respawn. Hinweis oben links im HUD.
- Loadouts: links 5 Slots mit Namen (umbenennen mit Y / R, Würfel für Namen), rechts Karten, unten Detailfeld (Werte-Balken der Waffen, Abklingzeit/Dauer, Freischaltung), LB/RB bzw. Q/E wechselt Slot, Tastenhinweise unten.
- Sprachzeilen wurden bei ~2 s abgeschnitten (MuseForge `/voices/.../speak`); `make_voice_lines.py` nutzt `/generate` und prüft das Ende.
- Hauptmenü: Bühne links (`UDHShowcase`: Figur mit Clip bzw. Waffen-/Controller-Comic `/Game/DH/UI/MenuArt`, federt rein, großes Wort, Farbbalken), Stadtvideo abendlich getönt mit Raster/Streifen/Vignette, eigene Musik (`/Game/DH/UI/Music`, ACE-Step, Sound-Klasse Music; Lyras mx_-Musik wird gestoppt). Musik nicht probegehört.
- MuseForge-Skripte schreiben in den Workspace `tmp-test` (sonst landet es im aktuellen Workspace des Nutzers).

## Mehr Leben: Dash, Tarnung, Sprint/Rutschen, Ego, Ansager, Musik, Stadtklang (2026-10-25)
- Dash ist wieder Lyras eigene Ability (`GA_Hero_Dash`: Richtungs-Montage mit Root Motion, Spur `NS_CharacterDash`, Sound; vorhergesagt und repliziert). Ihr Eingabe-Tag wird entfernt und ihr altes Widget abgemeldet (`UDHPlayerRulesComponent::PrepareLyraDash`); die Loadout-Fähigkeit „Dash“ aktiviert sie.
- Tarnung: Gegner sehen nichts, Besitzer und Team eine cyan Glas-Hülle (`/DowntownCore/Effects/M_DH_Cloak`, `Tools/Python/make_fx_materials.py`), Auflöse-Effekt beim An/Aus, im eigenen Bild cyan Rahmen. Adrenalin: roter Herzschlag-Rahmen und Speedlines, Recon: Sonar-Ringe, Mega-Sprung/Adrenalin/Recon mit Lyras Niagara-Effekten (über `UDHSoundCue`).
- Sprint/Rutschen: Lyra hat keine solchen Animationen. `Tools/Python/make_moves.py` leitet sie aus Lyras Jog bzw. Hock-Idle ab (Pose in Komponentenraum über Richtungen, `Tools/Python/plot_pose.py` zeichnet Strichmännchen zur Kontrolle) → `/DowntownCore/Animations/A_DH_{Sprint,Slide}_{Rifle,Pistol}`. Laufen als dynamische Montage im Slot `FullBody` auf allen Rechnern, aus Tempo/Ducken abgeleitet. Rutschen nur aus dem Lauf (ab 560 cm/s), Schub ×1,25, Sprung bricht ab. Das Commandlet stürzt beim Beenden ab (Kompressions-Assert), die Assets sind trotzdem gespeichert und laden.
- Ego: Sprint senkt/kippt die Waffe mit Wippen, Rutschen neigt die Kamera und weitet das Sichtfeld, Nachladen (aktive `GA_Weapon_Reload_*`) rollt die Waffe ab.
- Ansager (Englisch, eine Stimme: Qwen3 Voice Design als Referenz, IndexTTS2 klont jede Zeile, Whisper prüft den Text; `Tools/Branding/make_announcer.py`): Server meldet Ereignis + Team (`UDHGameRulesComponent::Announce`, Multicast), jeder Client formuliert aus seiner Sicht (`UDHAnnouncer`). Spielstart, letzte Minute, Führung, Sieg/Niederlage, Kontrollpunkte A/B/C, Bombe, Flagge, Hügel, Runden, First Blood, Doppel-/Dreifach-Kill, Headshot, Killstreaks, Infiziert, Letzter Überlebender, Finale Waffe.
- Musik neu (ACE-Step mit 4B-LM, Lautstärkeverlauf geprüft): drei Menü-Tracks, im Match Warmup, letzte Minute, Sieg-/Niederlage-Sting.
- Stadtklang (`UDHAmbience`): City-Sample-Ambience (`Tools/CitySample_Ambience.txt`, nicht in git) als Bett plus alle 3–9 s etwas in 25–90 m: Autos, Busse, Motorrad, Hupen, Sirenen, Hubschrauber, Jets. Schritte anderer Spieler zusätzlich weit hörbar (32 m, hinter Wänden gedämpft, im Sprint lauter, geduckt still).
- Nicht angehört: Musik, Ansager, Ambience. Rutsch-Pose nur von hinten im Spiel und als Strichmännchen gesehen.

## Klicks im Pausenmenü, Teamfarben, Kartengrenzen, Menümusik (2026-10-25)
- Pausen- und Dev-Menü im Match nahmen keine Eingaben an: das Fähigkeiten-HUD (mit den Vollbild-Effekten) lag über den Menüs und war treffbar. `UDHAbilityHud` und `UDHGunGameHud` sind jetzt `HitTestInvisible`.
- Teamfarben aus Sicht des Spielers: eigenes Team blau, alle anderen rot, auf jedem Rechner. Kleiner Hook im Lyra-Kern (`ULyraTeamSubsystem::DisplayAssetPerspective`, Lyra ignorierte den Viewer-Parameter), gesetzt in `DowntownCoreRuntimeModule`; `UDHGameRulesComponent` färbt neu, wenn sich das eigene Team ändert. Server-Texte wie „RED TEAM WINS ROUND“ kommen weiter aus Sicht des Hosts.
- Kartengrenzen: die unsichtbaren Wände (`BlockingVolume`) bekommen ein orangefarbenes Warnstreifen-Feld, das erst in ~20 m Nähe auftaucht (`/DowntownCore/Effects/M_DH_Boundary`, Pixel-Tiefe statt Weltdistanz; Emissive hoch wegen Lyras Tageslicht-Belichtung). Minimap: Wände orange, dahinter dunkelrot.
- Menümusik: Lyras eigene Musik (`mx_System`) hängt an der GameInstance und lief weiter; wird jetzt im Menü und im Match gestoppt. Drei neue K-Pop-artige Menütracks. Stimmen in der Charakterauswahl 2,5-fach lauter.

## Character outfits and hair

- `colors` in Characters.json (sRGB, per part Top/Bottom/Shoes) overrides the crowd colour variation so each hunter wears what the splash art shows. sync_characters.py converts to linear (^2.2) and sets `A_CrowdColor_main`, pattern colour = 55 % of it. Linear values straight in looked washed out (purple came out pink).
- Flying hair: the groom binding never deformed the hair here (skin cache on or off), so the hair stayed in the head's rest pose and floated away from the animated head (running, jumping, dying). UDHPlayerRulesComponent::AttachHairToHead hangs every groom rigidly on the `head` bone (relative transform from the rest pose) without its binding. Also: the crowd hair card materials need the AnimToTexture plugin (now enabled), and `r.HairStrands.Simulation=0`.
- Check: `Tools/Test-Game.ps1` with `Character=<Id>` in Saved/Config/WindowsEditor/GameUserSettings.ini, third-person shot after ~30 s.

## Voices and menu music

- make_voice_lines.py: Qwen3 voice design picks the gender from the description and got it wrong for several men. Each description now starts with "Male voice, a man." / "Female voice, a woman." and the median pitch is checked (men below 165 Hz). Shouted lines measure high even for men: keep lines calm-ish.
- Menu music: three K-pop style instrumentals from ElevenLabs' free library (Tools/Branding/Music/Licensed/README.md).
