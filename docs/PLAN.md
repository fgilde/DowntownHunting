# Downtown Hunting – Plan

Multiplayer-Shooter in einer großen Stadt. Basis: **Lyra Starter Game** (Gameplay-Framework) + **City Sample** (Stadt-Assets). Engine: UE 5.8.
Vorgänger `callOfShooter` (Blacksite) dient nur als Asset- und Ideenquelle, Code wird nicht übernommen.

## Leitlinien

- Lyra-Kern möglichst nicht anfassen. Eigenes Zeug in ein eigenes Game Feature Plugin `DowntownCore` (Muster: `ShooterCore`). Hält Lyra-Upgrades machbar.
- Erst ein kleiner, spielbarer Stadtausschnitt, dann die große Stadt. Kein Schritt ohne lauffähigen Stand.
- Jeder Meilenstein endet mit: Build läuft, Match mit Bots spielbar, Listen-Server mit 2. Client getestet, Screenshot/Notiz in `docs/`.

## Phase 0 – Setup

1. Lyra auf Fab zur Bibliothek, im Launcher Projekt `DowntownHunting` erstellen (5.8, falls verfügbar; sonst neueste Version und nach Epic-Anleitung hochziehen).
2. git + Git LFS (`*.uasset`, `*.umap`, Texturen, Audio), `.gitignore` für `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`.
3. **Unreal MCP einrichten** (KI steuert den Editor):
   - Bevorzugt: Epics offizieller Unreal MCP (Editor-Plugins `ModelContextProtocol` + `AllToolsets`, Server auf Port 8000) plus Epics Claude-Code-Plugin `EpicGames/unreal-engine-skills-for-claude-code-plugin`.
   - Fallback: `ChiR24/Unreal_mcp` (im alten Projekt vorgeschlagen).
   - Zusätzlich wie in Blacksite: Python-Skripte über `PythonScriptPlugin` für wiederholbare Batch-Aufgaben (Import, Map-Bau, Validierung).
4. Lyra unverändert testen: Elimination mit Bots, Listen-Server + Client lokal.

## Phase 1 – Fundament

- Plugin `DowntownCore`, eigene Experience `DH_TDM` auf einer Lyra-Testmap.
- Branding: Titel, Hauptmenü (CommonUI aus Lyra), Name „Downtown Hunting“.
- **Perspektive umschaltbar (Ego/Third-Person)**, entschieden 2026-10-16. Third-Person kommt von Lyra. Ego dazu: eigener Lyra-Kamera-Modus, First-Person-Arme + FP-Animationen, UE-First-Person-Rendering (verhindert Waffen-Clipping). Pro Match festlegbar (Server-Regel), damit niemand per Third-Person um Ecken späht.

## Phase 2 – Vertical Slice: Stadtausschnitt

- City Sample laden, nur benötigte Assets migrieren (Gebäude, Straßenmodule, Props). Kein MassTraffic/Crowd, keine Fahrzeuge vorerst.
- Map `DH_Block` aus 2–3 Blöcken: einige Gebäude begehbar, Dachzugänge, zusätzliche Deckung, Sperren an den Rändern.
- NavMesh für Bots, Spawnpunkte, Elimination + Control darauf.
- Ziel: stabile 60+ FPS auf eigenem Rechner, Spielgefühl bewerten. **Entscheidungspunkt:** passt City Sample fürs Gameplay? Sonst Asset-Strategie ändern.

## Phase 3 – Waffen

- Blacksite-Waffen (AK-47, MP5, M1911) migrieren, Lizenzen laut `THIRD_PARTY_ASSETS.md` prüfen.
- Als Lyra-Weapon-Definitions mit eigenen Abilities (GAS) aufsetzen.
- Gunsmith/Aufsätze neu bauen (Lyra hat das nicht), Loadouts speichern.

## Phase 4 – Modi (kleine Maps)

- TDM, 3 vs 3 vs 3 (Lyra-Teams auf 3 erweitern), Domination (Lyra Control), später Search & Destroy / Free-for-all.
- Weitere Stadtausschnitte als eigene Maps (Level Instances aus der großen Stadt wiederverwenden).

## Phase 5 – Große Stadt

- World-Partition-Map für große Modi (Ground-War-artig, viele Spieler + Bots).
- Server-Streaming, Performance-Budget, LODs/Nanite/Lumen-Settings für Shooter.
- Optional: Fahrzeuge, ambiente Crowds nur clientseitig.

## Phase 6 – Online

- Lyra bringt CommonUser/Online-Subsystem-Anbindung (EOS/Steam möglich): Lobby, Freunde einladen, Serversuche.
- **Achtung:** Dedizierte Server-Builds brauchen eine aus dem Quellcode gebaute Engine (Launcher-Engine kann keinen Server-Target bauen). Bis dahin Listen-Server.

## Risiken

- City Sample: kaum Innenräume, riesig (Download/Platte zig GB, Editor will viel RAM), auf Optik statt Shooter-FPS getrimmt.
- Lyra ist komplex (GAS, Game Features, Experiences): Einarbeitung kostet Zeit, zahlt sich bei Modi aus.
- Lyra für 5.8 noch nicht bestätigt.

## Offene Entscheidungen

- Perspektive: pro Match vom Host festgelegt oder vom Spieler frei wählbar?
- Online-Dienst später: EOS oder Steam?
