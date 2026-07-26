Lego Loco — Easter Eggs & Hidden Events: Complete Analysis

1. Command-Line Season Themes (5 modes)

Location: CGWND_ParseCmdLine at 0x406790

The game accepts hidden command-line keywords that force seasonal visual themes. The global g_easter_egg at 0x485230 drives the behavior:

┌────────────┬──────┬─────────────┬──────────────────────┐
│  Keyword   │ Mode │ Forced Date │        Theme         │
├────────────┼──────┼─────────────┼──────────────────────┤
│ /Easter    │ 1    │ April 1     │ Easter / April Fools │
├────────────┼──────┼─────────────┼──────────────────────┤
│ /Desert    │ 2    │ June 11     │ Desert / Summer      │
├────────────┼──────┼─────────────┼──────────────────────┤
│ /Halloween │ 3    │ October 31  │ Halloween            │
├────────────┼──────┼─────────────┼──────────────────────┤
│ /Winter    │ 4    │ December 2  │ Winter               │
├────────────┼──────┼─────────────┼──────────────────────┤
│ /XMas      │ 5    │ December 25 │ Christmas            │
└────────────┴──────┴─────────────┴──────────────────────┘

How it works: INPUT_SetMouse (0x41F970) reads g_easter_egg and overrides the system date returned by localtime() to the forced date above. The game then uses this date to determine which seasonal assets (backdrops, postcards, etc.) to load.

Special Halloween-only behavior: When mode 3 is active, resource IDs 0x1800–0x198F get field +0x164 set to 0x400, and the entire viewport tilemap is invalidated. This almost certainly gives buildings/tiles a "spooky" visual treatment.

2. Easter Egg Collection System ([EasterEggs] in INI)

Location: INPUT_SetKeyboard at 0x41F7E0, INPUT_EditScrollHandler at 0x41F8E0, INPUT_SetMouse at 0x41F970

The game tracks which easter egg objects the player has found/clicked on:

- [EasterEggs] section in loco.ini stores object IDs as incrementally numbered keys (1, 2, 3...)
- Each game object has a flag at offset +0x163 — when set to 1, the easter egg is "collected"
- INPUT_SetKeyboard loads previously collected eggs from INI on startup
- INPUT_EditScrollHandler and INPUT_SetMouse record newly found eggs
- Easter egg objects appear only during the correct season (date-gated by Game_IsObjectBetween)

easter.usr files: Language-specific Easter postcard character lists at POSTBAG/Easter/{Eng,Swe,Spa,Nor,Ita,Ger,Fre,Dut,Dan}/easter.usr contain 11 characters including Santa, Nessie (Loch Ness Monster), a skeleton, fairy, snowman, martians, and more.

3. PARTY Mode 🎉

Location: Building_BaseCtor at 0x433A20, Building_Update at 0x4327B0, Building_HandleAction at 0x434100

Trigger: Build any building with a custom name (non-empty name in the .dat file). If the name does NOT contain the substring "PARTY", party mode activates. (Putting "PARTY" in the name explicitly suppresses it.)

Global variables: g_is_party_mode at 0x48548C, g_party_start_time at 0x485490

Effects:
1. Building_Update — Normal AI update is bypassed; a special PartyUpdate virtual method (vtable[0x5C]) is called instead
2. Building_HandleAction — During wander action (3), happiness increases by +2 instead of the normal rate, and the idle timer is skipped entirely — occupants cycle through actions much faster
3. INPUT_SaveWorld — PARTY-named objects get special save treatment via vtable[0x34]

This is a substantial gameplay modifier — buildings with custom names make their occupants hyperactive and happier.

4. Dinosaur Sound Click (Hidden Hit Target)

Location: GAMESTATE_HandleClick at 0x40A4E0

On the game setup screen (where you choose scenario/layout), there's a hidden clickable rectangle at object offset +0x20C. When clicked:

- Generates rand() % 3 → plays sound ID 0x50F3, 0x50F4, or 0x50F5 (dinosaur roars)
- Sound plays with 3D positional audio at the click coordinates
- No visual feedback — it's a purely audio easter egg

The FUNCTION_MAP.md explicitly calls this the "dinosaur egg" hit target. The backdrop DINOSKIN.BMP exists in the art resources.

5. Developer Names Hidden in Save Files

23 developer names from Intelligent Games (the game's developer) are embedded in all 8 shipped .sav files with a 6 prefix:

6Dan, 6Duck, 6Ducky, 6Fabio, 6Florian, 6Genevieve, 6Gilles,
6Henrik, 6Javier, 6Johan, 6Juan, 6Lee, 6Luis, 6Maddi, 6Mark,
6Mary, 6Olav, 6Pete, 6Rabbit, 6Ruggero, 6Sally, 6Simon, 6Tom

6. ee.ini — The Easter Egg INI File

Location: src/ui/ui.c

The player's name is persisted to ee.ini (not the main LEGO.INI):
WritePrivateProfileStringA("USER", "Name", buf, "ee.ini");
The filename itself — "Easter Egg INI" — is a meta-easter-egg.

7. EasterEgg Directive in .dat Tile Format

Location: src/game/tile_desc.h, src/game/tile_desc.c, tools/parse_dat.py

The .dat tile descriptor format supports EasterEgg directives embedded in tiles:
- InsertSeq EasterEgg — sequence played when tile is first placed
- MobileSeq EasterEgg — sequence played for moving entities on this tile

Format: EasterEgg <seq> <n> <seq> <count> <delay> <seq> <n> R <x> <y>

Each TileDescriptor struct stores up to 64 bytes for each type.

8. Credits Screen

Location: CGWND_AboutDialog_LoadCredits at 0x40FE50

The game loads Credits.dat from the install directory, parses it as a stream, and displays scrolling credits. The credits screen is game state 9 (GAME_STATE_CREDITS) and uses the same rendering pipeline as the running game.

9. Demo Mode Detection

The three strings at 0x47E200 ("/s"), 0x47E1FC ("-s"), and 0x47E1F8 ("s") act as a demo-mode gate in CGWND_ParseCmdLine. If a command-line token doesn't contain all three substrings, g_demo_mode is set at 0x4A9918 — limiting functionality for demo builds.

---
Summary: The game has a surprisingly rich easter egg system — 5 seasonal visual themes, a collection mechanic for hidden objects, a party mode that changes AI behavior, a hidden dinosaur sound trigger, and developer names embedded in save data. The season system in particular is date-driven, meaning the game world would naturally change appearance around real-world holidays.
