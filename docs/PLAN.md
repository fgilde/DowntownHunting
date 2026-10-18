# Downtown Hunting – Plan

Multiplayer-Shooter in einer großen Stadt. Basis: **Lyra Starter Game** (Gameplay-Framework) + **City Sample** (Stadt-Assets). Engine: UE 5.8.
Vorgänger `callOfShooter` (Blacksite) dient nur als Asset- und Ideenquelle, Code wird nicht übernommen.

Legende: ✅ erledigt · 🔶 begonnen · ⬜ offen. Details und Belege (Screenshots) in `docs/NOTES.md`.

## Leitlinien

- Lyra-Kern möglichst nicht anfassen. Eigenes Zeug in ein eigenes Game Feature Plugin `DowntownCore` (Muster: `ShooterCore`). Hält Lyra-Upgrades machbar.
- Erst ein kleiner, spielbarer Stadtausschnitt, dann die große Stadt. Kein Schritt ohne lauffähigen Stand.
- Jeder Meilenstein endet mit: Build läuft, Match mit Bots spielbar, Listen-Server mit 2. Client getestet, Screenshot/Notiz in `docs/`.
- Im Projektordner gibt es immer etwas Startbares: `Play.cmd` (Menü), `Play-Downtown.cmd` (direkt in die Stadt), `Package.cmd` (eigenständige EXE).

## Phase 0 – Setup ✅

- ✅ Lyra-Projekt `DowntownHunting` (UE 5.8), git + LFS, privates GitHub-Repo.
- ✅ Unreal MCP (Port 8765), Python-Skripte über `PythonScriptPlugin` (`Tools/Python`).
- ✅ Lyra unverändert getestet: Elimination mit Bots, Listen-Server + Client.

## Phase 1 – Fundament

- ✅ Plugin `DowntownCore`, eigene Experiences (`B_DH_TDM`, `B_DH_TDM_FP`).
- ✅ Branding: Titel, Hauptmenü, Name „Downtown Hunting“.
- 🔶 **Perspektive Ego/Third-Person**: Ego-Kamera mit Körper-/Waffen-Kopien, Zielen mit Zoom, Rückstoß, kein Clipping ✅. Umschalten mit V / D-Pad oben, wenn die Modus-Regel es erlaubt (`Perspective=Choice`) ✅. Offen: Nachlade-/Feuer-Animationen im Ego-Bild; eigene Ego-Arme (die Ego-Pose läuft auf dem City-Sample-Skelett, dessen Handknochen anders ausgerichtet sind als die des Lyra-Mannequins: die Pistole schwebt neben der Hand, Gewehr passt).
- 🔶 **Menüs und Einstellungen** (von Lyra): ✅ DH-Look (Bernstein statt Cyan) für Menü, Pausenmenü, Einstellungen und HUD, einheitlich Englisch. ⬜ alle Einstellungsseiten durchgehen (Video/Grafik, Audio, Maus/Tastatur, Controller).
- ⬜ **Controller-Support** durchgehend testen: Menüs, Ego- und Third-Person-Steuerung, Aim-Assist.
- ✅ **Präsentation**: Splash-Screen beim Start, Intro (20 s Kamerafahrt durch Downtown mit Titel und Sound, überspringbar), danach Hauptmenü mit Stadt-Hintergrund. ⬜ Musik statt synthetischem Klangteppich.

## Phase 2 – Vertical Slice: Stadtausschnitt

- ✅ City-Sample-Assets selektiv übernommen (Import-Skript + Manifeste, nicht in git).
- ✅ Map `DH_Block` (2 × 2 Blöcke) mit Spawnpunkten, NavMesh, Randwänden; TDM + Domination darauf.
- ✅ City-Sample-Hauptfiguren als Charaktere, Teamfarbe über Armbinden (Armbinden schmaler, für den eigenen Spieler ausgeblendet).
- 🔶 **Mehr Charaktere + Auswahl**: ✅ 12 Figuren aus City Samples Crowd-Baukasten (`Characters.json`, `Sync-Characters.cmd`) + 2 Spielerfiguren; Auswahl gespeichert pro Spieler, per RPC zum Server, Bots zufällig. ✅ Auswahl-Bildschirm im Hauptmenü mit Porträts. ✅ auch im Pausenmenü. ⬜ 3D-Vorschau.
- ⬜ Begehbare Gebäude/Dachzugänge, zusätzliche Deckung, Map-Rand kaschieren (Nebel, Kulisse, Absperrungen statt Leere).
- ⬜ Ziel: stabile 60+ FPS auf eigenem Rechner messen, Spielgefühl bewerten. **Entscheidungspunkt:** passt City Sample fürs Gameplay?

## Phase 3 – Waffen

- ✅ Waffenwechsel mit mehreren Startwaffen (Lyra-Quickbar: 1–3, Mausrad, Gamepad), abschaltbar per Modus-Regel (`WeaponSwitch`, `Loadout`). ⬜ Waffen-Pickups auf den Stadt-Maps.
- ✅ Blacksite-Waffen AK-47, MP5, M1911 (Classic Weapons Pack, CC0) als eigene Lyra-Waffenketten unter `/DowntownCore/Weapons`: `Tools/Python/make_weapons.py` (Kopien, Mesh, Mündungs-Socket) + `Tools/Python/mcp_weapon_values.py` (Werte, Verknüpfung, HUD-Icon). Icons: `Tools/Branding/make_weapon_icons.py`. ⬜ eigene Feuer-/Nachlade-Sounds und Animationen.
- 🔶 **Loadout-Builder**: ✅ Primär-/Sekundärwaffe im Haupt- und Pausenmenü, gilt bei `Loadout=Player`. ✅ neue Waffen wählbar. ⬜ mehrere gespeicherte Loadouts, Aufsätze/Gunsmith.

## Phase 4 – Modi und Maps

- 🔶 **Modi aus Konfiguration.** Ein Modus ist ein Eintrag (`LyraUserFacingExperienceDefinition` in `/DowntownCore/Playlists`): Spielart + Map + Regeln. Regeln stehen in `ExtraArgs` und kommen als URL-Optionen im Spiel an, damit Bots, LAN und Online gleich funktionieren:

  | Option | Werte | Wirkung |
  | --- | --- | --- |
  | `Perspective` | `TP`, `FP`, `Choice` | feste Perspektive oder frei umschaltbar |
  | `WeaponSwitch` | `1`/`0` | Waffenwechsel erlaubt |
  | `Loadout` | z. B. `Pistol,Rifle,Shotgun,AK47,MP5,M1911` | Startwaffen; ersetzen Lyras Startpistole |
  | `ScoreLimit` | Zahl | Punkte zum Sieg (sonst Lyra: aus Spielerzahl) |
  | `TimeLimit` | Minuten | Spieldauer (nur TDM; Lyra: 12) |
  | `GunGame` | Waffenliste | jeder Kill = nächste Waffe, Kill mit der letzten gewinnt fürs Team; kein Waffenwechsel |
  | `NumBots` | Zahl | Anzahl Bots (liest Lyra bereits) |

  Werkzeug: kleines Editor-Tool, das aus Vorlage + Regeln einen neuen Modus erzeugt; das Menü listet alle Modi automatisch, der Host-Screen bietet Bots/LAN/Online.
- ✅ TDM, Domination (Lyra Control), jeweils Third-Person und Ego. ✅ Gun Game (Downtown, Depot), Quick Match (10 Kills, 5 Minuten). ✅ Gun-Game-Stufe im HUD. ✅ eigene Menü-Kacheln. ✅ Free-for-all (Downtown, Depot): jeder Spieler eigenes Team, eigene Wertung und eigenes Score-HUD statt Lyras 2-Team-Teilen.
- ✅ 3 vs 3 vs 3 („Three Teams“, Downtown): eigene Wertung für beliebig viele Teams, Score-HUD in Teamfarben. ✅ Elimination (Runden ohne Respawn, Downtown, Depot). ✅ Search & Destroy (Downtown, Depot; Bots legen und entschärfen), Lyras Meldung „assigned to the Red Team“ im FFA.
- 🔶 Mehr Maps: ✅ `DH_Park` „Waterfront“ (Bürotürme am Stadtrand, TDM + Domination; Pipeline `cs_region.py` → `Import-CitySample.ps1` → `Build-RegionMap.ps1`). ✅ `DH_Freeway` „Overpass“ (unter der Hochstraße, `cs_survey.py` → `cs_region.py` → `Build-RegionMap.ps1` → `mcp_region_gameplay.py`). ✅ `DH_Depot` „Depot“ (Lyras Convolution mit Sichtbeton statt Sci-Fi-Raster, `restyle_depot.py`). ⬜ weitere Stadtausschnitte (andere Viertel und Größen); Depot: Sprungpads/Teleporter und Team-Leuchtstreifen noch im Lyra-Look.
- 🔶 Spielgefühl: ✅ Trefferfeedback/Killfeed (Lyra), ✅ Scoreboard auf Tab/Select, ✅ Match-Ende-Screen, ✅ Minimap, ✅ Killcam (Kamera folgt dem Killer).

## Phase 5 – Große Stadt

- ⬜ World-Partition-Map für große Modi (Ground-War-artig, viele Spieler + Bots).
- ⬜ Server-Streaming, Performance-Budget, LODs/Nanite/Lumen-Settings für Shooter.
- ⬜ Optional: Fahrzeuge, ambiente Crowds nur clientseitig.

## Phase 6 – Online

- ✅ Listen-Server/LAN: aus dem Menü getestet (Play-Screen hostet LAN, Join-Screen findet und tritt bei, `Tools/Test-LanMenus.ps1`).
- 🔶 Online über Steam (`Play-Online.cmd`, Dev-App-ID 480): ✅ Steam-Login, Hosten mit Steam-Lobby und Steam-Sockets, Suche filtert auf eigene Lobbys. ⬜ Beitritt mit zweitem Rechner/Account testen, eigene App-ID (Steamworks), Freunde einladen, Packaged Build mit Steam.
- ⬜ Dedizierte Server brauchen eine aus dem Quellcode gebaute Engine (Launcher-Engine kann keinen Server-Target bauen). Bis dahin Listen-Server.

## Risiken

- City Sample: kaum Innenräume, riesig (Download/Platte zig GB, Editor will viel RAM), auf Optik statt Shooter-FPS getrimmt.
- Lyra ist komplex (GAS, Game Features, Experiences): Einarbeitung kostet Zeit, zahlt sich bei Modi aus.
- RAM auf dem Entwicklungsrechner knapp, solange WSL/Docker laufen.

## Entscheidungen

- 2026-10-16: Perspektive Ego und Third-Person, festgelegt pro Modus.
- 2026-10-17: Perspektive als Modus-Regel `Perspective=TP|FP|Choice`; bei `Choice` schaltet der Spieler selbst um.
- Offen: Online-Dienst EOS oder Steam?
