# Lego Loco (loco.exe) Function Map

Auto-generated from Ghidra headless decompiler index.
Known mappings are based on manual analysis; remaining names are best-guess from address range.

## Address Range to Subsystem

| Range | Subsystem |
|---|---|
| 0x401000–0x404fff | graphics (LOCOBITMAP, surface management) |
| 0x406000–0x413fff | core (CGWND methods, game setup) |
| 0x414000–0x42bfff | input/cursor/UI |
| 0x42c000–0x43cfff | game world (town, buildings, trains) |
| 0x43d000–0x445fff | network (NETMAN, DirectPlay) |
| 0x446000–0x44ffff | resource system |
| 0x450000–0x45ffff | audio/DirectSound + DirectDraw |
| 0x460000–0x465fff | Win32 platform layer (WndProc, etc.) |
| 0x466000+ | CRT (string functions, math, etc.) |

## Function Table

| Address | Original Name | Suggested Name | Subsystem | Notes |
|---|---|---|---|---|
| 00401000 | FUN_00401000 | LOCOBITMAP_LoadFromFile | graphics | Loads bitmap from file |
| 00401170 | FUN_00401170 | LOCOBITMAP_BlitBitmap | graphics | Blits bitmap to surface |
| 00401280 | FUN_00401280 | LOCOBITMAP_00401280 | graphics |  |
| 004014e0 | FUN_004014e0 | LOCOBITMAP_004014e0 | graphics |  |
| 00401540 | FUN_00401540 | LOCOBITMAP_00401540 | graphics |  |
| 00401620 | FUN_00401620 | LOCOBITMAP_00401620 | graphics |  |
| 00401650 | FUN_00401650 | LOCOBITMAP_00401650 | graphics |  |
| 00401680 | thunk_FUN_00401c90 | thunk_00401c90 | graphics | Thunk to thunk_FUN_00401c90 |
| 00401690 | FUN_00401690 | LOCOBITMAP_00401690 | graphics |  |
| 00401760 | FUN_00401760 | LOCOBITMAP_00401760 | graphics |  |
| 00401810 | FUN_00401810 | LOCOBITMAP_00401810 | graphics |  |
| 00401820 | FUN_00401820 | LOCOBITMAP_00401820 | graphics |  |
| 00401850 | FUN_00401850 | LOCOBITMAP_00401850 | graphics |  |
| 00401aa0 | FUN_00401aa0 | LOCOBITMAP_00401aa0 | graphics |  |
| 00401c10 | FUN_00401c10 | LOCOBITMAP_00401c10 | graphics |  |
| 00401c90 | FUN_00401c90 | LOCOBITMAP_00401c90 | graphics |  |
| 00401df0 | FUN_00401df0 | LOCOBITMAP_00401df0 | graphics |  |
| 00401f50 | FUN_00401f50 | LOCOBITMAP_00401f50 | graphics |  |
| 00401fb0 | FUN_00401fb0 | LOCOBITMAP_00401fb0 | graphics |  |
| 00401fd0 | FUN_00401fd0 | LOCOBITMAP_00401fd0 | graphics |  |
| 00402380 | FUN_00402380 | LOCOBITMAP_00402380 | graphics |  |
| 00402520 | FUN_00402520 | LOCOBITMAP_00402520 | graphics |  |
| 00402660 | FUN_00402660 | LOCOBITMAP_00402660 | graphics |  |
| 00402690 | FUN_00402690 | LOCOBITMAP_00402690 | graphics |  |
| 00403ba0 | FUN_00403ba0 | SURFACE_00403ba0 | graphics |  |
| 00403cd0 | FUN_00403cd0 | SURFACE_00403cd0 | graphics |  |
| 00403e80 | FUN_00403e80 | SURFACE_00403e80 | graphics |  |
| 00404720 | FUN_00404720 | GFX_00404720 | graphics |  |
| 00404770 | FUN_00404770 | GFX_00404770 | graphics |  |
| 00404830 | FUN_00404830 | GFX_00404830 | graphics |  |
| 004048e0 | FUN_004048e0 | GFX_004048e0 | graphics |  |
| 00404ac0 | FUN_00404ac0 | GFX_00404ac0 | graphics |  |
| 00405520 | FUN_00405520 | UNK_00405520 | unknown |  |
| 004055e0 | FUN_004055e0 | UNK_004055e0 | unknown |  |
| 00405680 | FUN_00405680 | UNK_00405680 | unknown |  |
| 00405790 | FUN_00405790 | UNK_00405790 | unknown |  |
| 00405850 | FUN_00405850 | UNK_00405850 | unknown |  |
| 00405870 | FUN_00405870 | UNK_00405870 | unknown |  |
| 00405900 | FUN_00405900 | UNK_00405900 | unknown |  |
| 00405a20 | FUN_00405a20 | UNK_00405a20 | unknown |  |
| 00405a50 | FUN_00405a50 | UNK_00405a50 | unknown |  |
| 00405ab0 | FUN_00405ab0 | UNK_00405ab0 | unknown |  |
| 00405c00 | FUN_00405c00 | UNK_00405c00 | unknown |  |
| 00405c40 | FUN_00405c40 | UNK_00405c40 | unknown |  |
| 00405de0 | FUN_00405de0 | UNK_00405de0 | unknown |  |
| 00405e20 | FUN_00405e20 | UNK_00405e20 | unknown |  |
| 00405e60 | FUN_00405e60 | UNK_00405e60 | unknown |  |
| 00405fd0 | FUN_00405fd0 | UNK_00405fd0 | unknown |  |
| 004061b0 | FUN_004061b0 | CGWND_004061b0 | core |  |
| 004061e0 | FUN_004061e0 | CGWND_constructor | core | Game window object constructor |
| 004062a0 | FUN_004062a0 | CGWND_004062a0 | core |  |
| 004062e0 | FUN_004062e0 | CGWND_004062e0 | core |  |
| 00406480 | FUN_00406480 | CGWND_00406480 | core |  |
| 00406680 | FUN_00406680 | CGWND_00406680 | core |  |
| 00406790 | FUN_00406790 | CGWND_00406790 | core |  |
| 004068d0 | FUN_004068d0 | CGWND_004068d0 | core |  |
| 00406ba0 | FUN_00406ba0 | GameLoop_Setup | core | Initializes main game loop |
| 00406e80 | FUN_00406e80 | CGWND_00406e80 | core |  |
| 00406ed0 | FUN_00406ed0 | CGWND_00406ed0 | core |  |
| 00406f90 | FUN_00406f90 | CGWND_00406f90 | core |  |
| 004077a0 | FUN_004077a0 | CGWND_004077a0 | core |  |
| 00407ae0 | FUN_00407ae0 | CGWND_00407ae0 | core |  |
| 00407bf0 | FUN_00407bf0 | CGWND_00407bf0 | core |  |
| 00407d00 | FUN_00407d00 | CGWND_00407d00 | core |  |
| 00407d20 | FUN_00407d20 | CGWND_00407d20 | core |  |
| 00408130 | FUN_00408130 | CGWND_00408130 | core |  |
| 00408350 | FUN_00408350 | CGWND_00408350 | core |  |
| 004085e0 | FUN_004085e0 | CGWND_004085e0 | core |  |
| 004086f0 | FUN_004086f0 | CGWND_004086f0 | core |  |
| 004089d0 | FUN_004089d0 | CGWND_004089d0 | core |  |
| 00408a30 | FUN_00408a30 | CGWND_00408a30 | core |  |
| 00408aa0 | FUN_00408aa0 | CGWND_00408aa0 | core |  |
| 00408b00 | FUN_00408b00 | CGWND_00408b00 | core |  |
| 00408b20 | FUN_00408b20 | CGWND_00408b20 | core |  |
| 00408d10 | FUN_00408d10 | CGWND_00408d10 | core |  |
| 00408f00 | FUN_00408f00 | CGWND_00408f00 | core |  |
| 00409280 | FUN_00409280 | CGWND_00409280 | core |  |
| 00409360 | FUN_00409360 | CGWND_00409360 | core |  |
| 004094b0 | FUN_004094b0 | CGWND_004094b0 | core |  |
| 00409770 | FUN_00409770 | CGWND_00409770 | core |  |
| 00409970 | thunk_FUN_00409980 | thunk_00409980 | core | Thunk to thunk_FUN_00409980 |
| 00409980 | FUN_00409980 | CGWND_00409980 | core |  |
| 00409db0 | FUN_00409db0 | CGWND_00409db0 | core |  |
| 00409e70 | FUN_00409e70 | CGWND_00409e70 | core |  |
| 0040a0d4 | Catch@0040a0d4 | CRT_catch_handler_0040a0d4 | core | C++ catch block handler |
| 0040a150 | FUN_0040a150 | GAMESTATE_0040a150 | core |  |
| 0040a220 | FUN_0040a220 | GAMESTATE_0040a220 | core |  |
| 0040a260 | FUN_0040a260 | GAMESTATE_0040a260 | core |  |
| 0040a300 | FUN_0040a300 | GAMESTATE_0040a300 | core |  |
| 0040a350 | FUN_0040a350 | GAMESTATE_0040a350 | core |  |
| 0040a3d0 | FUN_0040a3d0 | GAMESTATE_0040a3d0 | core |  |
| 0040a4a0 | FUN_0040a4a0 | GAMESTATE_0040a4a0 | core |  |
| 0040a4e0 | FUN_0040a4e0 | GAMESTATE_0040a4e0 | core |  |
| 0040aa20 | FUN_0040aa20 | GAMESTATE_0040aa20 | core |  |
| 0040aaf0 | FUN_0040aaf0 | GAMESTATE_0040aaf0 | core |  |
| 0040aba0 | FUN_0040aba0 | GAMESTATE_0040aba0 | core |  |
| 0040ac50 | FUN_0040ac50 | GAMESTATE_0040ac50 | core |  |
| 0040b4c0 | FUN_0040b4c0 | GAMESTATE_0040b4c0 | core |  |
| 0040b500 | FUN_0040b500 | GAMESTATE_0040b500 | core |  |
| 0040b550 | FUN_0040b550 | GAMESTATE_0040b550 | core |  |
| 0040b5a0 | FUN_0040b5a0 | GAMESTATE_0040b5a0 | core |  |
| 0040b5d0 | FUN_0040b5d0 | GAMESTATE_0040b5d0 | core |  |
| 0040b610 | FUN_0040b610 | GAMESTATE_0040b610 | core |  |
| 0040b740 | FUN_0040b740 | GAMESTATE_0040b740 | core |  |
| 0040b880 | FUN_0040b880 | GAMESTATE_0040b880 | core |  |
| 0040bbd0 | FUN_0040bbd0 | GAMESTATE_0040bbd0 | core |  |
| 0040c3d0 | FUN_0040c3d0 | CGWND_0040c3d0 | core |  |
| 0040c460 | FUN_0040c460 | CGWND_0040c460 | core |  |
| 0040c580 | FUN_0040c580 | CGWND_0040c580 | core |  |
| 0040cb10 | FUN_0040cb10 | CGWND_0040cb10 | core |  |
| 0040cc20 | FUN_0040cc20 | CGWND_0040cc20 | core |  |
| 0040cc90 | FUN_0040cc90 | CGWND_0040cc90 | core |  |
| 0040cd60 | FUN_0040cd60 | CGWND_0040cd60 | core |  |
| 0040cfa0 | FUN_0040cfa0 | CGWND_0040cfa0 | core |  |
| 0040d020 | FUN_0040d020 | CGWND_0040d020 | core |  |
| 0040d040 | FUN_0040d040 | CGWND_0040d040 | core |  |
| 0040d0b0 | FUN_0040d0b0 | CGWND_0040d0b0 | core |  |
| 0040d170 | FUN_0040d170 | CGWND_0040d170 | core |  |
| 0040d2a0 | FUN_0040d2a0 | CGWND_0040d2a0 | core |  |
| 0040d2f0 | FUN_0040d2f0 | CGWND_0040d2f0 | core |  |
| 0040d340 | FUN_0040d340 | CGWND_0040d340 | core |  |
| 0040d470 | FUN_0040d470 | CGWND_0040d470 | core |  |
| 0040d500 | FUN_0040d500 | CGWND_0040d500 | core |  |
| 0040d660 | FUN_0040d660 | CGWND_0040d660 | core |  |
| 0040d680 | FUN_0040d680 | CGWND_0040d680 | core |  |
| 0040d750 | FUN_0040d750 | CGWND_0040d750 | core |  |
| 0040d770 | FUN_0040d770 | CGWND_0040d770 | core |  |
| 0040d890 | FUN_0040d890 | CGWND_0040d890 | core |  |
| 0040d8e0 | FUN_0040d8e0 | CGWND_0040d8e0 | core |  |
| 0040d940 | FUN_0040d940 | CGWND_0040d940 | core |  |
| 0040db90 | FUN_0040db90 | CGWND_0040db90 | core |  |
| 0040dc20 | FUN_0040dc20 | CGWND_0040dc20 | core |  |
| 0040df80 | FUN_0040df80 | CGWND_0040df80 | core |  |
| 0040e0d0 | FUN_0040e0d0 | CGWND_0040e0d0 | core |  |
| 0040e130 | FUN_0040e130 | CGWND_0040e130 | core |  |
| 0040e160 | FUN_0040e160 | CGWND_0040e160 | core |  |
| 0040e250 | FUN_0040e250 | CGWND_0040e250 | core |  |
| 0040e2a0 | FUN_0040e2a0 | CGWND_0040e2a0 | core |  |
| 0040e340 | FUN_0040e340 | CGWND_0040e340 | core |  |
| 0040e440 | FUN_0040e440 | CGWND_0040e440 | core |  |
| 0040e520 | FUN_0040e520 | CGWND_0040e520 | core |  |
| 0040e600 | FUN_0040e600 | CGWND_0040e600 | core |  |
| 0040e660 | FUN_0040e660 | CGWND_0040e660 | core |  |
| 0040e680 | FUN_0040e680 | CGWND_0040e680 | core |  |
| 0040e690 | FUN_0040e690 | CGWND_0040e690 | core |  |
| 0040e8b0 | FUN_0040e8b0 | CGWND_0040e8b0 | core |  |
| 0040e950 | FUN_0040e950 | CGWND_0040e950 | core |  |
| 0040eb60 | FUN_0040eb60 | CGWND_0040eb60 | core |  |
| 0040ec70 | FUN_0040ec70 | CGWND_0040ec70 | core |  |
| 0040eca0 | FUN_0040eca0 | CGWND_0040eca0 | core |  |
| 0040ecf0 | FUN_0040ecf0 | CGWND_0040ecf0 | core |  |
| 0040ed10 | FUN_0040ed10 | CGWND_0040ed10 | core |  |
| 0040ed20 | FUN_0040ed20 | CGWND_0040ed20 | core |  |
| 0040ee00 | FUN_0040ee00 | CGWND_0040ee00 | core |  |
| 0040ee20 | FUN_0040ee20 | CGWND_0040ee20 | core |  |
| 0040eea0 | FUN_0040eea0 | CGWND_0040eea0 | core |  |
| 0040eeb0 | FUN_0040eeb0 | CGWND_0040eeb0 | core |  |
| 0040ef00 | FUN_0040ef00 | CGWND_0040ef00 | core |  |
| 0040ef20 | FUN_0040ef20 | CGWND_0040ef20 | core |  |
| 0040f040 | FUN_0040f040 | CGWND_0040f040 | core |  |
| 0040f070 | FUN_0040f070 | CGWND_0040f070 | core |  |
| 0040f090 | FUN_0040f090 | CGWND_0040f090 | core |  |
| 0040f1c0 | FUN_0040f1c0 | CGWND_0040f1c0 | core |  |
| 0040f270 | FUN_0040f270 | CGWND_0040f270 | core |  |
| 0040f290 | FUN_0040f290 | CGWND_0040f290 | core |  |
| 0040f3c0 | FUN_0040f3c0 | CGWND_0040f3c0 | core |  |
| 0040f480 | FUN_0040f480 | CGWND_0040f480 | core |  |
| 0040f510 | FUN_0040f510 | CGWND_0040f510 | core |  |
| 0040f6a0 | FUN_0040f6a0 | CGWND_0040f6a0 | core |  |
| 0040f980 | FUN_0040f980 | CGWND_0040f980 | core |  |
| 0040fe3f | Catch@0040fe3f | CRT_catch_handler_0040fe3f | core | C++ catch block handler |
| 0040fe50 | FUN_0040fe50 | CGWND_0040fe50 | core |  |
| 00410240 | FUN_00410240 | GAME_00410240 | core |  |
| 00410260 | FUN_00410260 | GAME_00410260 | core |  |
| 00410280 | FUN_00410280 | GAME_00410280 | core |  |
| 00410510 | FUN_00410510 | GAME_00410510 | core |  |
| 00410660 | FUN_00410660 | GAME_00410660 | core |  |
| 00410680 | FUN_00410680 | GAME_00410680 | core |  |
| 00410700 | FUN_00410700 | GAME_00410700 | core |  |
| 00410750 | FUN_00410750 | GAME_00410750 | core |  |
| 00410840 | FUN_00410840 | GAME_00410840 | core |  |
| 00410a20 | FUN_00410a20 | GAME_00410a20 | core |  |
| 00410a40 | FUN_00410a40 | GAME_00410a40 | core |  |
| 00410d20 | FUN_00410d20 | GAME_00410d20 | core |  |
| 00411000 | FUN_00411000 | GAME_00411000 | core |  |
| 00411230 | FUN_00411230 | GAME_00411230 | core |  |
| 004113a0 | FUN_004113a0 | GAME_004113a0 | core |  |
| 00411580 | FUN_00411580 | GAME_00411580 | core |  |
| 00411760 | FUN_00411760 | GAME_00411760 | core |  |
| 004117b0 | FUN_004117b0 | GAME_004117b0 | core |  |
| 00411ae0 | FUN_00411ae0 | GAME_00411ae0 | core |  |
| 00411c50 | FUN_00411c50 | GAME_00411c50 | core |  |
| 00411d10 | FUN_00411d10 | GAME_00411d10 | core |  |
| 00411dc0 | FUN_00411dc0 | GAME_00411dc0 | core |  |
| 00411fb0 | FUN_00411fb0 | GAME_00411fb0 | core |  |
| 00412060 | FUN_00412060 | GAME_00412060 | core |  |
| 00412410 | FUN_00412410 | GAME_00412410 | core |  |
| 00412540 | FUN_00412540 | GAME_00412540 | core |  |
| 00412580 | FUN_00412580 | GAME_00412580 | core |  |
| 004125c0 | FUN_004125c0 | GAME_004125c0 | core |  |
| 004125e0 | FUN_004125e0 | GAME_004125e0 | core |  |
| 00412600 | FUN_00412600 | GAME_00412600 | core |  |
| 00412620 | FUN_00412620 | GAME_00412620 | core |  |
| 00412660 | FUN_00412660 | GAME_00412660 | core |  |
| 00412670 | FUN_00412670 | GAME_00412670 | core |  |
| 00412710 | FUN_00412710 | GAME_00412710 | core |  |
| 00412790 | FUN_00412790 | GAME_00412790 | core |  |
| 00412870 | FUN_00412870 | GAME_00412870 | core |  |
| 004128b0 | FUN_004128b0 | GAME_004128b0 | core |  |
| 004128d0 | FUN_004128d0 | GAME_004128d0 | core |  |
| 004129c0 | FUN_004129c0 | GAME_004129c0 | core |  |
| 00412a80 | FUN_00412a80 | GAME_00412a80 | core |  |
| 00412af0 | FUN_00412af0 | GAME_00412af0 | core |  |
| 00412b50 | FUN_00412b50 | GAME_00412b50 | core |  |
| 00412bd0 | FUN_00412bd0 | GAME_00412bd0 | core |  |
| 00412c20 | FUN_00412c20 | GAME_00412c20 | core |  |
| 00412c50 | FUN_00412c50 | GAME_00412c50 | core |  |
| 00412ee0 | FUN_00412ee0 | GAME_00412ee0 | core |  |
| 00413070 | FUN_00413070 | GAME_00413070 | core |  |
| 004130a0 | FUN_004130a0 | GAME_004130a0 | core |  |
| 004130f0 | FUN_004130f0 | GAME_004130f0 | core |  |
| 00413140 | FUN_00413140 | GAME_00413140 | core |  |
| 00413180 | FUN_00413180 | GAME_00413180 | core |  |
| 004131c0 | FUN_004131c0 | GAME_004131c0 | core |  |
| 00413210 | FUN_00413210 | GAME_00413210 | core |  |
| 00413530 | FUN_00413530 | GAME_00413530 | core |  |
| 004135b0 | FUN_004135b0 | GAME_004135b0 | core |  |
| 00413630 | FUN_00413630 | GAME_00413630 | core |  |
| 00413660 | FUN_00413660 | GAME_00413660 | core |  |
| 00413971 | Catch@00413971 | CRT_catch_handler_00413971 | core | C++ catch block handler |
| 00413980 | FUN_00413980 | GAME_00413980 | core |  |
| 00413ab0 | FUN_00413ab0 | GAME_00413ab0 | core |  |
| 00413b50 | FUN_00413b50 | GAME_00413b50 | core |  |
| 00413b70 | FUN_00413b70 | GAME_00413b70 | core |  |
| 00413c10 | FUN_00413c10 | GAME_00413c10 | core |  |
| 00413d10 | FUN_00413d10 | GAME_00413d10 | core |  |
| 00413d90 | FUN_00413d90 | GAME_00413d90 | core |  |
| 00413de0 | FUN_00413de0 | GAME_00413de0 | core |  |
| 004140a0 | FUN_004140a0 | CURSOR_004140a0 | input/cursor/UI |  |
| 00414130 | FUN_00414130 | CURSOR_00414130 | input/cursor/UI |  |
| 00414290 | FUN_00414290 | CURSOR_00414290 | input/cursor/UI |  |
| 00414340 | FUN_00414340 | CURSOR_00414340 | input/cursor/UI |  |
| 00414a80 | FUN_00414a80 | CURSOR_00414a80 | input/cursor/UI |  |
| 00414b80 | FUN_00414b80 | CURSOR_00414b80 | input/cursor/UI |  |
| 00414bb0 | FUN_00414bb0 | CURSOR_00414bb0 | input/cursor/UI |  |
| 00414c20 | FUN_00414c20 | CURSOR_00414c20 | input/cursor/UI |  |
| 00414ef0 | FUN_00414ef0 | CURSOR_00414ef0 | input/cursor/UI |  |
| 00414fb0 | FUN_00414fb0 | CURSOR_00414fb0 | input/cursor/UI |  |
| 00415440 | FUN_00415440 | CURSOR_00415440 | input/cursor/UI |  |
| 00415980 | FUN_00415980 | CURSOR_00415980 | input/cursor/UI |  |
| 004159e0 | FUN_004159e0 | CURSOR_004159e0 | input/cursor/UI |  |
| 00415a00 | FUN_00415a00 | CURSOR_00415a00 | input/cursor/UI |  |
| 00416460 | FUN_00416460 | CURSOR_00416460 | input/cursor/UI |  |
| 004166b0 | FUN_004166b0 | CURSOR_004166b0 | input/cursor/UI |  |
| 004169e0 | FUN_004169e0 | CURSOR_004169e0 | input/cursor/UI |  |
| 00416b80 | FUN_00416b80 | CURSOR_00416b80 | input/cursor/UI |  |
| 00416e00 | FUN_00416e00 | CURSOR_00416e00 | input/cursor/UI |  |
| 00416f70 | FUN_00416f70 | CURSOR_00416f70 | input/cursor/UI |  |
| 00417f20 | FUN_00417f20 | CURSOR_00417f20 | input/cursor/UI |  |
| 004180a0 | FUN_004180a0 | CURSOR_004180a0 | input/cursor/UI |  |
| 00418210 | FUN_00418210 | CURSOR_00418210 | input/cursor/UI |  |
| 00418340 | FUN_00418340 | CURSOR_00418340 | input/cursor/UI |  |
| 00418450 | FUN_00418450 | CURSOR_00418450 | input/cursor/UI |  |
| 00418780 | FUN_00418780 | CURSOR_00418780 | input/cursor/UI |  |
| 004189a0 | FUN_004189a0 | CURSOR_004189a0 | input/cursor/UI |  |
| 00418a90 | FUN_00418a90 | CURSOR_00418a90 | input/cursor/UI |  |
| 00418e20 | FUN_00418e20 | CURSOR_00418e20 | input/cursor/UI |  |
| 00419260 | FUN_00419260 | CURSOR_00419260 | input/cursor/UI |  |
| 00419560 | FUN_00419560 | CURSOR_00419560 | input/cursor/UI |  |
| 00419680 | FUN_00419680 | CURSOR_00419680 | input/cursor/UI |  |
| 004198b0 | FUN_004198b0 | CURSOR_004198b0 | input/cursor/UI |  |
| 00419a60 | FUN_00419a60 | CURSOR_00419a60 | input/cursor/UI |  |
| 00419b10 | FUN_00419b10 | CURSOR_00419b10 | input/cursor/UI |  |
| 0041a050 | FUN_0041a050 | INPUT_0041a050 | input/cursor/UI |  |
| 0041a0e0 | FUN_0041a0e0 | INPUT_0041a0e0 | input/cursor/UI |  |
| 0041a210 | FUN_0041a210 | INPUT_0041a210 | input/cursor/UI |  |
| 0041a360 | FUN_0041a360 | INPUT_0041a360 | input/cursor/UI |  |
| 0041a460 | FUN_0041a460 | INPUT_0041a460 | input/cursor/UI |  |
| 0041a650 | FUN_0041a650 | INPUT_0041a650 | input/cursor/UI |  |
| 0041aa40 | FUN_0041aa40 | INPUT_0041aa40 | input/cursor/UI |  |
| 0041aae0 | FUN_0041aae0 | INPUT_0041aae0 | input/cursor/UI |  |
| 0041d250 | FUN_0041d250 | INPUT_0041d250 | input/cursor/UI |  |
| 0041d2b0 | FUN_0041d2b0 | INPUT_0041d2b0 | input/cursor/UI |  |
| 0041d2d0 | FUN_0041d2d0 | INPUT_0041d2d0 | input/cursor/UI |  |
| 0041d310 | FUN_0041d310 | INPUT_0041d310 | input/cursor/UI |  |
| 0041d320 | FUN_0041d320 | INPUT_0041d320 | input/cursor/UI |  |
| 0041d5c0 | FUN_0041d5c0 | INPUT_0041d5c0 | input/cursor/UI |  |
| 0041d8f0 | FUN_0041d8f0 | INPUT_0041d8f0 | input/cursor/UI |  |
| 0041d920 | FUN_0041d920 | INPUT_0041d920 | input/cursor/UI |  |
| 0041d950 | FUN_0041d950 | INPUT_0041d950 | input/cursor/UI |  |
| 0041d980 | FUN_0041d980 | INPUT_0041d980 | input/cursor/UI |  |
| 0041d9b0 | FUN_0041d9b0 | INPUT_0041d9b0 | input/cursor/UI |  |
| 0041dd40 | FUN_0041dd40 | INPUT_0041dd40 | input/cursor/UI |  |
| 0041dd80 | FUN_0041dd80 | INPUT_0041dd80 | input/cursor/UI |  |
| 0041def0 | FUN_0041def0 | INPUT_0041def0 | input/cursor/UI |  |
| 0041e100 | FUN_0041e100 | INPUT_0041e100 | input/cursor/UI |  |
| 0041e120 | FUN_0041e120 | INPUT_0041e120 | input/cursor/UI |  |
| 0041e1f0 | FUN_0041e1f0 | INPUT_0041e1f0 | input/cursor/UI |  |
| 0041e570 | FUN_0041e570 | INPUT_0041e570 | input/cursor/UI |  |
| 0041e600 | FUN_0041e600 | INPUT_0041e600 | input/cursor/UI |  |
| 0041e620 | FUN_0041e620 | INPUT_0041e620 | input/cursor/UI |  |
| 0041e6e0 | FUN_0041e6e0 | INPUT_0041e6e0 | input/cursor/UI |  |
| 0041e9f0 | FUN_0041e9f0 | INPUT_0041e9f0 | input/cursor/UI |  |
| 0041efa0 | FUN_0041efa0 | INPUT_0041efa0 | input/cursor/UI |  |
| 0041f0c0 | FUN_0041f0c0 | INPUT_0041f0c0 | input/cursor/UI |  |
| 0041f2b0 | FUN_0041f2b0 | INPUT_0041f2b0 | input/cursor/UI |  |
| 0041f430 | FUN_0041f430 | INPUT_0041f430 | input/cursor/UI |  |
| 0041f480 | FUN_0041f480 | INPUT_0041f480 | input/cursor/UI |  |
| 0041f4e0 | FUN_0041f4e0 | INPUT_0041f4e0 | input/cursor/UI |  |
| 0041f540 | FUN_0041f540 | INPUT_0041f540 | input/cursor/UI |  |
| 0041f590 | FUN_0041f590 | INPUT_0041f590 | input/cursor/UI |  |
| 0041f5e0 | FUN_0041f5e0 | INPUT_0041f5e0 | input/cursor/UI |  |
| 0041f6e0 | FUN_0041f6e0 | INPUT_0041f6e0 | input/cursor/UI |  |
| 0041f7e0 | FUN_0041f7e0 | INPUT_0041f7e0 | input/cursor/UI |  |
| 0041f8e0 | FUN_0041f8e0 | INPUT_0041f8e0 | input/cursor/UI |  |
| 0041f970 | FUN_0041f970 | INPUT_0041f970 | input/cursor/UI |  |
| 0041fb20 | FUN_0041fb20 | INPUT_0041fb20 | input/cursor/UI |  |
| 0041fbe0 | FUN_0041fbe0 | INPUT_0041fbe0 | input/cursor/UI |  |
| 0041fd00 | FUN_0041fd00 | INPUT_0041fd00 | input/cursor/UI |  |
| 0041ff20 | FUN_0041ff20 | INPUT_0041ff20 | input/cursor/UI |  |
| 00420000 | FUN_00420000 | UI_00420000 | input/cursor/UI |  |
| 004202f0 | FUN_004202f0 | UI_004202f0 | input/cursor/UI |  |
| 004203a0 | FUN_004203a0 | UI_004203a0 | input/cursor/UI |  |
| 004203c0 | FUN_004203c0 | UI_004203c0 | input/cursor/UI |  |
| 004204d0 | FUN_004204d0 | UI_004204d0 | input/cursor/UI |  |
| 004206b0 | FUN_004206b0 | UI_004206b0 | input/cursor/UI |  |
| 00420860 | FUN_00420860 | UI_00420860 | input/cursor/UI |  |
| 004208f0 | FUN_004208f0 | UI_004208f0 | input/cursor/UI |  |
| 00421200 | FUN_00421200 | UI_00421200 | input/cursor/UI |  |
| 00421500 | FUN_00421500 | UI_00421500 | input/cursor/UI |  |
| 004216f0 | FUN_004216f0 | UI_004216f0 | input/cursor/UI |  |
| 00421ae0 | FUN_00421ae0 | UI_00421ae0 | input/cursor/UI |  |
| 00422010 | FUN_00422010 | UI_00422010 | input/cursor/UI |  |
| 00422440 | FUN_00422440 | UI_00422440 | input/cursor/UI |  |
| 00422570 | FUN_00422570 | UI_00422570 | input/cursor/UI |  |
| 00422660 | FUN_00422660 | UI_00422660 | input/cursor/UI |  |
| 00422820 | FUN_00422820 | UI_00422820 | input/cursor/UI |  |
| 00422d80 | FUN_00422d80 | UI_00422d80 | input/cursor/UI |  |
| 00422ea0 | FUN_00422ea0 | UI_00422ea0 | input/cursor/UI |  |
| 00422ec0 | FUN_00422ec0 | UI_00422ec0 | input/cursor/UI |  |
| 004234e0 | FUN_004234e0 | UI_004234e0 | input/cursor/UI |  |
| 00423500 | FUN_00423500 | UI_00423500 | input/cursor/UI |  |
| 00423560 | FUN_00423560 | UI_00423560 | input/cursor/UI |  |
| 00423840 | FUN_00423840 | UI_00423840 | input/cursor/UI |  |
| 00423870 | FUN_00423870 | UI_00423870 | input/cursor/UI |  |
| 00423890 | FUN_00423890 | UI_00423890 | input/cursor/UI |  |
| 004238c0 | FUN_004238c0 | UI_004238c0 | input/cursor/UI |  |
| 004239c0 | FUN_004239c0 | UI_004239c0 | input/cursor/UI |  |
| 004239e0 | FUN_004239e0 | UI_004239e0 | input/cursor/UI |  |
| 00423a90 | FUN_00423a90 | UI_00423a90 | input/cursor/UI |  |
| 00423ab0 | FUN_00423ab0 | UI_00423ab0 | input/cursor/UI |  |
| 00423c50 | FUN_00423c50 | UI_00423c50 | input/cursor/UI |  |
| 00423d00 | FUN_00423d00 | UI_00423d00 | input/cursor/UI |  |
| 00423d20 | FUN_00423d20 | UI_00423d20 | input/cursor/UI |  |
| 00423d70 | FUN_00423d70 | UI_00423d70 | input/cursor/UI |  |
| 00423e00 | FUN_00423e00 | UI_00423e00 | input/cursor/UI |  |
| 00423e80 | FUN_00423e80 | UI_00423e80 | input/cursor/UI |  |
| 00423f00 | FUN_00423f00 | UI_00423f00 | input/cursor/UI |  |
| 00423f80 | FUN_00423f80 | UI_00423f80 | input/cursor/UI |  |
| 00424040 | FUN_00424040 | UI_00424040 | input/cursor/UI |  |
| 004241e0 | FUN_004241e0 | UI_004241e0 | input/cursor/UI |  |
| 00424250 | FUN_00424250 | UI_00424250 | input/cursor/UI |  |
| 00424270 | FUN_00424270 | UI_00424270 | input/cursor/UI |  |
| 00424460 | FUN_00424460 | UI_00424460 | input/cursor/UI |  |
| 00424490 | FUN_00424490 | UI_00424490 | input/cursor/UI |  |
| 00424510 | FUN_00424510 | UI_00424510 | input/cursor/UI |  |
| 00424550 | FUN_00424550 | UI_00424550 | input/cursor/UI |  |
| 00424820 | FUN_00424820 | UI_00424820 | input/cursor/UI |  |
| 00424a00 | FUN_00424a00 | UI_00424a00 | input/cursor/UI |  |
| 00424a30 | FUN_00424a30 | UI_00424a30 | input/cursor/UI |  |
| 00424a70 | FUN_00424a70 | UI_00424a70 | input/cursor/UI |  |
| 00424a90 | FUN_00424a90 | UI_00424a90 | input/cursor/UI |  |
| 00424ad0 | FUN_00424ad0 | UI_00424ad0 | input/cursor/UI |  |
| 00424af0 | FUN_00424af0 | UI_00424af0 | input/cursor/UI |  |
| 00424b40 | FUN_00424b40 | UI_00424b40 | input/cursor/UI |  |
| 00424ba0 | FUN_00424ba0 | UI_00424ba0 | input/cursor/UI |  |
| 00424bf0 | FUN_00424bf0 | UI_00424bf0 | input/cursor/UI |  |
| 00424e00 | FUN_00424e00 | UI_00424e00 | input/cursor/UI |  |
| 004255f0 | FUN_004255f0 | UI_004255f0 | input/cursor/UI |  |
| 00425670 | FUN_00425670 | UI_00425670 | input/cursor/UI |  |
| 004257f0 | FUN_004257f0 | UI_004257f0 | input/cursor/UI |  |
| 00425870 | FUN_00425870 | UI_00425870 | input/cursor/UI |  |
| 004258f0 | FUN_004258f0 | UI_004258f0 | input/cursor/UI |  |
| 00425910 | FUN_00425910 | UI_00425910 | input/cursor/UI |  |
| 00425990 | FUN_00425990 | UI_00425990 | input/cursor/UI |  |
| 004259c0 | FUN_004259c0 | UI_004259c0 | input/cursor/UI |  |
| 00425a50 | FUN_00425a50 | UI_00425a50 | input/cursor/UI |  |
| 00425ac0 | FUN_00425ac0 | UI_00425ac0 | input/cursor/UI |  |
| 00425b70 | FUN_00425b70 | UI_00425b70 | input/cursor/UI |  |
| 00425d30 | FUN_00425d30 | UI_00425d30 | input/cursor/UI |  |
| 00425dc0 | FUN_00425dc0 | CGWND_SetupCursorSurface | input/cursor/UI | Sets up cursor rendering surface |
| 00425f20 | FUN_00425f20 | UI_00425f20 | input/cursor/UI |  |
| 00426900 | FUN_00426900 | UIPANEL_00426900 | input/cursor/UI |  |
| 00426a90 | FUN_00426a90 | UIPANEL_00426a90 | input/cursor/UI |  |
| 00426b00 | FUN_00426b00 | UIPANEL_00426b00 | input/cursor/UI |  |
| 00426b70 | FUN_00426b70 | UIPANEL_00426b70 | input/cursor/UI |  |
| 00426b90 | FUN_00426b90 | UIPANEL_00426b90 | input/cursor/UI |  |
| 00426eb0 | FUN_00426eb0 | UIPANEL_00426eb0 | input/cursor/UI |  |
| 00427370 | FUN_00427370 | UIPANEL_00427370 | input/cursor/UI |  |
| 00427440 | FUN_00427440 | UIPANEL_00427440 | input/cursor/UI |  |
| 00427460 | FUN_00427460 | UIPANEL_00427460 | input/cursor/UI |  |
| 00427520 | FUN_00427520 | UIPANEL_00427520 | input/cursor/UI |  |
| 00427580 | FUN_00427580 | UIPANEL_00427580 | input/cursor/UI |  |
| 004277d0 | FUN_004277d0 | UIPANEL_004277d0 | input/cursor/UI |  |
| 00428400 | FUN_00428400 | UIPANEL_00428400 | input/cursor/UI |  |
| 00428550 | FUN_00428550 | UIPANEL_00428550 | input/cursor/UI |  |
| 00428770 | FUN_00428770 | UIPANEL_00428770 | input/cursor/UI |  |
| 004287b0 | FUN_004287b0 | UIPANEL_004287b0 | input/cursor/UI |  |
| 004289a0 | FUN_004289a0 | UIPANEL_004289a0 | input/cursor/UI |  |
| 00428f90 | FUN_00428f90 | UIPANEL_00428f90 | input/cursor/UI |  |
| 00429490 | FUN_00429490 | UIPANEL_00429490 | input/cursor/UI |  |
| 00429820 | FUN_00429820 | UIPANEL_00429820 | input/cursor/UI |  |
| 00429830 | FUN_00429830 | UIPANEL_00429830 | input/cursor/UI |  |
| 00429850 | FUN_00429850 | UIPANEL_00429850 | input/cursor/UI |  |
| 00429a10 | FUN_00429a10 | UIPANEL_00429a10 | input/cursor/UI |  |
| 00429b20 | FUN_00429b20 | UIPANEL_00429b20 | input/cursor/UI |  |
| 00429dd0 | FUN_00429dd0 | UIPANEL_00429dd0 | input/cursor/UI |  |
| 00429ef0 | FUN_00429ef0 | UIPANEL_00429ef0 | input/cursor/UI |  |
| 0042a110 | FUN_0042a110 | UIPANEL_0042a110 | input/cursor/UI |  |
| 0042a140 | FUN_0042a140 | UIPANEL_0042a140 | input/cursor/UI |  |
| 0042a1c0 | FUN_0042a1c0 | UIPANEL_0042a1c0 | input/cursor/UI |  |
| 0042a370 | FUN_0042a370 | UIPANEL_0042a370 | input/cursor/UI |  |
| 0042a3d0 | FUN_0042a3d0 | UIPANEL_0042a3d0 | input/cursor/UI |  |
| 0042a540 | FUN_0042a540 | UIPANEL_0042a540 | input/cursor/UI |  |
| 0042a5f0 | FUN_0042a5f0 | UIPANEL_0042a5f0 | input/cursor/UI |  |
| 0042a610 | FUN_0042a610 | UIPANEL_0042a610 | input/cursor/UI |  |
| 0042a850 | FUN_0042a850 | UIPANEL_0042a850 | input/cursor/UI |  |
| 0042a980 | FUN_0042a980 | UIPANEL_0042a980 | input/cursor/UI |  |
| 0042aa90 | FUN_0042aa90 | UIPANEL_0042aa90 | input/cursor/UI |  |
| 0042ab10 | FUN_0042ab10 | UIPANEL_0042ab10 | input/cursor/UI |  |
| 0042af01 | Catch@0042af01 | CRT_catch_handler_0042af01 | input/cursor/UI | C++ catch block handler |
| 0042af30 | FUN_0042af30 | UIPANEL_0042af30 | input/cursor/UI |  |
| 0042b050 | FUN_0042b050 | UIPANEL_0042b050 | input/cursor/UI |  |
| 0042b960 | FUN_0042b960 | UIPANEL_0042b960 | input/cursor/UI |  |
| 0042b9c0 | FUN_0042b9c0 | UIPANEL_0042b9c0 | input/cursor/UI |  |
| 0042ba90 | FUN_0042ba90 | UIPANEL_0042ba90 | input/cursor/UI |  |
| 0042bb90 | FUN_0042bb90 | UIPANEL_0042bb90 | input/cursor/UI |  |
| 0042bc80 | FUN_0042bc80 | UIPANEL_0042bc80 | input/cursor/UI |  |
| 0042bd70 | FUN_0042bd70 | UIPANEL_0042bd70 | input/cursor/UI |  |
| 0042bec0 | FUN_0042bec0 | UIPANEL_0042bec0 | input/cursor/UI |  |
| 0042c050 | FUN_0042c050 | TOWN_0042c050 | game world |  |
| 0042c130 | FUN_0042c130 | TOWN_0042c130 | game world |  |
| 0042c220 | FUN_0042c220 | TOWN_0042c220 | game world |  |
| 0042c330 | FUN_0042c330 | TOWN_0042c330 | game world |  |
| 0042c3d0 | FUN_0042c3d0 | TOWN_0042c3d0 | game world |  |
| 0042c470 | FUN_0042c470 | TOWN_0042c470 | game world |  |
| 0042c590 | FUN_0042c590 | TOWN_0042c590 | game world |  |
| 0042c700 | FUN_0042c700 | TOWN_0042c700 | game world |  |
| 0042c890 | FUN_0042c890 | TOWN_0042c890 | game world |  |
| 0042c950 | FUN_0042c950 | TOWN_0042c950 | game world |  |
| 0042c9f0 | FUN_0042c9f0 | TOWN_0042c9f0 | game world |  |
| 0042cb10 | FUN_0042cb10 | TOWN_0042cb10 | game world |  |
| 0042cce0 | FUN_0042cce0 | TOWN_0042cce0 | game world |  |
| 0042cd60 | FUN_0042cd60 | TOWN_0042cd60 | game world |  |
| 0042cd80 | FUN_0042cd80 | TOWN_0042cd80 | game world |  |
| 0042cdd0 | FUN_0042cdd0 | TOWN_0042cdd0 | game world |  |
| 0042ce10 | FUN_0042ce10 | TOWN_0042ce10 | game world |  |
| 0042cf90 | FUN_0042cf90 | TOWN_0042cf90 | game world |  |
| 0042d040 | FUN_0042d040 | TOWN_0042d040 | game world |  |
| 0042d1a0 | FUN_0042d1a0 | TOWN_0042d1a0 | game world |  |
| 0042d280 | FUN_0042d280 | TOWN_0042d280 | game world |  |
| 0042d3a0 | FUN_0042d3a0 | TOWN_0042d3a0 | game world |  |
| 0042d400 | FUN_0042d400 | TOWN_0042d400 | game world |  |
| 0042d670 | FUN_0042d670 | TOWN_0042d670 | game world |  |
| 0042d6b0 | FUN_0042d6b0 | TOWN_0042d6b0 | game world |  |
| 0042d770 | FUN_0042d770 | TOWN_0042d770 | game world |  |
| 0042d8a0 | FUN_0042d8a0 | TOWN_0042d8a0 | game world |  |
| 0042da10 | FUN_0042da10 | TOWN_0042da10 | game world |  |
| 0042db30 | FUN_0042db30 | TOWN_0042db30 | game world |  |
| 0042dc50 | FUN_0042dc50 | TOWN_0042dc50 | game world |  |
| 0042dd50 | FUN_0042dd50 | TOWN_0042dd50 | game world |  |
| 0042de70 | FUN_0042de70 | TOWN_0042de70 | game world |  |
| 0042e150 | FUN_0042e150 | TOWN_0042e150 | game world |  |
| 0042e420 | FUN_0042e420 | TOWN_0042e420 | game world |  |
| 0042e4e0 | FUN_0042e4e0 | TOWN_0042e4e0 | game world |  |
| 0042e5e0 | FUN_0042e5e0 | TOWN_0042e5e0 | game world |  |
| 0042e760 | FUN_0042e760 | TOWN_0042e760 | game world |  |
| 0042e900 | FUN_0042e900 | TOWN_0042e900 | game world |  |
| 0042e960 | FUN_0042e960 | TOWN_0042e960 | game world |  |
| 0042e980 | FUN_0042e980 | TOWN_0042e980 | game world |  |
| 0042ec10 | FUN_0042ec10 | TOWN_0042ec10 | game world |  |
| 0042edb0 | FUN_0042edb0 | TOWN_0042edb0 | game world |  |
| 0042ee20 | FUN_0042ee20 | TOWN_0042ee20 | game world |  |
| 0042eea0 | FUN_0042eea0 | TOWN_0042eea0 | game world |  |
| 0042f250 | FUN_0042f250 | TOWN_0042f250 | game world |  |
| 0042f6c0 | FUN_0042f6c0 | TOWN_0042f6c0 | game world |  |
| 0042f810 | FUN_0042f810 | TOWN_0042f810 | game world |  |
| 0042fdf0 | FUN_0042fdf0 | TOWN_0042fdf0 | game world |  |
| 0042fe30 | FUN_0042fe30 | TOWN_0042fe30 | game world |  |
| 0042fff0 | FUN_0042fff0 | TOWN_0042fff0 | game world |  |
| 00430090 | FUN_00430090 | TOWN_00430090 | game world |  |
| 004309b0 | FUN_004309b0 | TOWN_004309b0 | game world |  |
| 00430a90 | FUN_00430a90 | TOWN_00430a90 | game world |  |
| 00430af0 | FUN_00430af0 | TOWN_00430af0 | game world |  |
| 00430b10 | FUN_00430b10 | TOWN_00430b10 | game world |  |
| 00430c20 | FUN_00430c20 | TOWN_00430c20 | game world |  |
| 00430c60 | FUN_00430c60 | TOWN_00430c60 | game world |  |
| 00430e00 | FUN_00430e00 | TOWN_00430e00 | game world |  |
| 00431270 | FUN_00431270 | BUILDING_00431270 | game world |  |
| 00431560 | FUN_00431560 | BUILDING_00431560 | game world |  |
| 004316f0 | FUN_004316f0 | BUILDING_004316f0 | game world |  |
| 00431a10 | FUN_00431a10 | BUILDING_00431a10 | game world |  |
| 00431b30 | FUN_00431b30 | BUILDING_00431b30 | game world |  |
| 00431ed0 | FUN_00431ed0 | BUILDING_00431ed0 | game world |  |
| 004324f0 | FUN_004324f0 | BUILDING_004324f0 | game world |  |
| 004326f0 | FUN_004326f0 | BUILDING_004326f0 | game world |  |
| 00432720 | FUN_00432720 | BUILDING_00432720 | game world |  |
| 00432740 | FUN_00432740 | BUILDING_00432740 | game world |  |
| 004327b0 | FUN_004327b0 | BUILDING_004327b0 | game world |  |
| 00433160 | FUN_00433160 | BUILDING_00433160 | game world |  |
| 004331b0 | FUN_004331b0 | BUILDING_004331b0 | game world |  |
| 00433370 | FUN_00433370 | BUILDING_00433370 | game world |  |
| 00433530 | FUN_00433530 | BUILDING_00433530 | game world |  |
| 004336a0 | FUN_004336a0 | BUILDING_004336a0 | game world |  |
| 00433a20 | FUN_00433a20 | BUILDING_00433a20 | game world |  |
| 00433bc0 | FUN_00433bc0 | BUILDING_00433bc0 | game world |  |
| 00433be0 | FUN_00433be0 | BUILDING_00433be0 | game world |  |
| 00433c50 | FUN_00433c50 | BUILDING_00433c50 | game world |  |
| 00433dc0 | FUN_00433dc0 | BUILDING_00433dc0 | game world |  |
| 00433ec0 | FUN_00433ec0 | BUILDING_00433ec0 | game world |  |
| 00434040 | FUN_00434040 | BUILDING_00434040 | game world |  |
| 00434100 | FUN_00434100 | BUILDING_00434100 | game world |  |
| 00434260 | FUN_00434260 | BUILDING_00434260 | game world |  |
| 00434500 | FUN_00434500 | BUILDING_00434500 | game world |  |
| 004345d0 | FUN_004345d0 | BUILDING_004345d0 | game world |  |
| 004345f0 | FUN_004345f0 | BUILDING_004345f0 | game world |  |
| 00434690 | FUN_00434690 | BUILDING_00434690 | game world |  |
| 00434720 | FUN_00434720 | BUILDING_00434720 | game world |  |
| 00434800 | FUN_00434800 | BUILDING_00434800 | game world |  |
| 00434870 | FUN_00434870 | BUILDING_00434870 | game world |  |
| 004348a0 | FUN_004348a0 | BUILDING_004348a0 | game world |  |
| 00434970 | FUN_00434970 | BUILDING_00434970 | game world |  |
| 004349d0 | FUN_004349d0 | BUILDING_004349d0 | game world |  |
| 00434b60 | FUN_00434b60 | BUILDING_00434b60 | game world |  |
| 00434c50 | FUN_00434c50 | BUILDING_00434c50 | game world |  |
| 00435020 | FUN_00435020 | BUILDING_00435020 | game world |  |
| 00435200 | FUN_00435200 | BUILDING_00435200 | game world |  |
| 00435580 | FUN_00435580 | BUILDING_00435580 | game world |  |
| 004356b0 | FUN_004356b0 | BUILDING_004356b0 | game world |  |
| 00435700 | FUN_00435700 | BUILDING_00435700 | game world |  |
| 00435a10 | FUN_00435a10 | BUILDING_00435a10 | game world |  |
| 00435aa0 | FUN_00435aa0 | BUILDING_00435aa0 | game world |  |
| 00435ca0 | FUN_00435ca0 | BUILDING_00435ca0 | game world |  |
| 00435cd0 | FUN_00435cd0 | BUILDING_00435cd0 | game world |  |
| 00435d10 | FUN_00435d10 | BUILDING_00435d10 | game world |  |
| 00435db0 | FUN_00435db0 | BUILDING_00435db0 | game world |  |
| 004360b0 | FUN_004360b0 | BUILDING_004360b0 | game world |  |
| 00436280 | FUN_00436280 | BUILDING_00436280 | game world |  |
| 00436320 | FUN_00436320 | BUILDING_00436320 | game world |  |
| 00436360 | FUN_00436360 | BUILDING_00436360 | game world |  |
| 00436380 | FUN_00436380 | BUILDING_00436380 | game world |  |
| 004363c0 | FUN_004363c0 | BUILDING_004363c0 | game world |  |
| 004363e0 | FUN_004363e0 | BUILDING_004363e0 | game world |  |
| 00436400 | FUN_00436400 | BUILDING_00436400 | game world |  |
| 00436460 | FUN_00436460 | BUILDING_00436460 | game world |  |
| 00436480 | FUN_00436480 | BUILDING_00436480 | game world |  |
| 00436490 | FUN_00436490 | BUILDING_00436490 | game world |  |
| 00436960 | FUN_00436960 | BUILDING_00436960 | game world |  |
| 004369a0 | FUN_004369a0 | BUILDING_004369a0 | game world |  |
| 004369d0 | FUN_004369d0 | BUILDING_004369d0 | game world |  |
| 00436a00 | FUN_00436a00 | BUILDING_00436a00 | game world |  |
| 00436a10 | FUN_00436a10 | BUILDING_00436a10 | game world |  |
| 00436a40 | FUN_00436a40 | BUILDING_00436a40 | game world |  |
| 00436a60 | FUN_00436a60 | BUILDING_00436a60 | game world |  |
| 00436ab0 | FUN_00436ab0 | BUILDING_00436ab0 | game world |  |
| 00436b20 | FUN_00436b20 | BUILDING_00436b20 | game world |  |
| 00436b90 | FUN_00436b90 | BUILDING_00436b90 | game world |  |
| 00436bb0 | FUN_00436bb0 | BUILDING_00436bb0 | game world |  |
| 00436c50 | FUN_00436c50 | BUILDING_00436c50 | game world |  |
| 00436d60 | FUN_00436d60 | BUILDING_00436d60 | game world |  |
| 00436ec0 | FUN_00436ec0 | BUILDING_00436ec0 | game world |  |
| 00436f70 | FUN_00436f70 | BUILDING_00436f70 | game world |  |
| 004370f0 | FUN_004370f0 | TRAIN_004370f0 | game world |  |
| 00437670 | FUN_00437670 | TRAIN_00437670 | game world |  |
| 00437900 | FUN_00437900 | TRAIN_00437900 | game world |  |
| 00437cf0 | FUN_00437cf0 | TRAIN_00437cf0 | game world |  |
| 00438280 | FUN_00438280 | TRAIN_00438280 | game world |  |
| 00438590 | FUN_00438590 | TRAIN_00438590 | game world |  |
| 00438890 | FUN_00438890 | TRAIN_00438890 | game world |  |
| 00438ad0 | FUN_00438ad0 | TRAIN_00438ad0 | game world |  |
| 00438bc0 | FUN_00438bc0 | TRAIN_00438bc0 | game world |  |
| 00438ca0 | FUN_00438ca0 | TRAIN_00438ca0 | game world |  |
| 00438cc0 | FUN_00438cc0 | TRAIN_00438cc0 | game world |  |
| 00438e40 | FUN_00438e40 | TRAIN_00438e40 | game world |  |
| 004391a0 | FUN_004391a0 | TRAIN_004391a0 | game world |  |
| 004393d0 | FUN_004393d0 | TRAIN_004393d0 | game world |  |
| 004394e0 | FUN_004394e0 | TRAIN_004394e0 | game world |  |
| 00439550 | FUN_00439550 | TRAIN_00439550 | game world |  |
| 004396c0 | FUN_004396c0 | TRAIN_004396c0 | game world |  |
| 00439d00 | FUN_00439d00 | TRAIN_00439d00 | game world |  |
| 00439df0 | FUN_00439df0 | TRAIN_00439df0 | game world |  |
| 0043a140 | FUN_0043a140 | TRAIN_0043a140 | game world |  |
| 0043a4b0 | FUN_0043a4b0 | TRAIN_0043a4b0 | game world |  |
| 0043a5c0 | FUN_0043a5c0 | TRAIN_0043a5c0 | game world |  |
| 0043a6d0 | FUN_0043a6d0 | TRAIN_0043a6d0 | game world |  |
| 0043a760 | FUN_0043a760 | TRAIN_0043a760 | game world |  |
| 0043a8b0 | FUN_0043a8b0 | TRAIN_0043a8b0 | game world |  |
| 0043aa00 | FUN_0043aa00 | TRAIN_0043aa00 | game world |  |
| 0043ac10 | FUN_0043ac10 | TRAIN_0043ac10 | game world |  |
| 0043ad00 | FUN_0043ad00 | TRAIN_0043ad00 | game world |  |
| 0043ae20 | FUN_0043ae20 | TRAIN_0043ae20 | game world |  |
| 0043b220 | FUN_0043b220 | TRAIN_0043b220 | game world |  |
| 0043b240 | FUN_0043b240 | TRAIN_0043b240 | game world |  |
| 0043b6d0 | FUN_0043b6d0 | TRAIN_0043b6d0 | game world |  |
| 0043b770 | FUN_0043b770 | TRAIN_0043b770 | game world |  |
| 0043b8c0 | FUN_0043b8c0 | TRAIN_0043b8c0 | game world |  |
| 0043bb00 | FUN_0043bb00 | TRAIN_0043bb00 | game world |  |
| 0043c160 | FUN_0043c160 | TRAIN_0043c160 | game world |  |
| 0043c410 | FUN_0043c410 | TRAIN_0043c410 | game world |  |
| 0043c860 | FUN_0043c860 | TRAIN_0043c860 | game world |  |
| 0043cbe0 | FUN_0043cbe0 | TRAIN_0043cbe0 | game world |  |
| 0043cc40 | FUN_0043cc40 | TRAIN_0043cc40 | game world |  |
| 0043ccc0 | FUN_0043ccc0 | TRAIN_0043ccc0 | game world |  |
| 0043ce10 | FUN_0043ce10 | TRAIN_0043ce10 | game world |  |
| 0043d0a0 | FUN_0043d0a0 | NETMAN_constructor | network | Network manager constructor |
| 0043d110 | FUN_0043d110 | NETMAN_0043d110 | network |  |
| 0043d130 | FUN_0043d130 | NETMAN_0043d130 | network |  |
| 0043d210 | FUN_0043d210 | NETMAN_0043d210 | network |  |
| 0043d230 | FUN_0043d230 | NETMAN_0043d230 | network |  |
| 0043d250 | FUN_0043d250 | NETMAN_0043d250 | network |  |
| 0043d2b0 | FUN_0043d2b0 | NETMAN_0043d2b0 | network |  |
| 0043d350 | FUN_0043d350 | NETMAN_0043d350 | network |  |
| 0043d520 | FUN_0043d520 | NETMAN_0043d520 | network |  |
| 0043d620 | FUN_0043d620 | NETMAN_0043d620 | network |  |
| 0043d6c0 | FUN_0043d6c0 | NETMAN_0043d6c0 | network |  |
| 0043d820 | FUN_0043d820 | NETMAN_0043d820 | network |  |
| 0043dba7 | Catch@0043dba7 | CRT_catch_handler_0043dba7 | network | C++ catch block handler |
| 0043dbb4 | FUN_0043dbb4 | NETMAN_0043dbb4 | network |  |
| 0043dc30 | FUN_0043dc30 | NETMAN_0043dc30 | network |  |
| 0043ddf0 | FUN_0043ddf0 | NETMAN_0043ddf0 | network |  |
| 0043de00 | FUN_0043de00 | NETMAN_0043de00 | network |  |
| 0043de10 | FUN_0043de10 | NETMAN_0043de10 | network |  |
| 0043de20 | FUN_0043de20 | NETMAN_0043de20 | network |  |
| 0043de30 | FUN_0043de30 | NETMAN_0043de30 | network |  |
| 0043ded0 | FUN_0043ded0 | NETMAN_0043ded0 | network |  |
| 0043e010 | FUN_0043e010 | NETMAN_0043e010 | network |  |
| 0043e1d0 | FUN_0043e1d0 | NETMAN_0043e1d0 | network |  |
| 0043e2e0 | FUN_0043e2e0 | NETMAN_0043e2e0 | network |  |
| 0043e370 | FUN_0043e370 | NETMAN_0043e370 | network |  |
| 0043e560 | FUN_0043e560 | NETMAN_0043e560 | network |  |
| 0043e690 | FUN_0043e690 | NETMAN_0043e690 | network |  |
| 0043e900 | FUN_0043e900 | NETMAN_0043e900 | network |  |
| 0043ee80 | FUN_0043ee80 | NETMAN_0043ee80 | network |  |
| 0043eec0 | FUN_0043eec0 | NETMAN_0043eec0 | network |  |
| 0043efa0 | FUN_0043efa0 | NETMAN_0043efa0 | network |  |
| 0043f000 | FUN_0043f000 | NETMAN_0043f000 | network |  |
| 0043f030 | FUN_0043f030 | NETMAN_0043f030 | network |  |
| 0043f070 | FUN_0043f070 | NETMAN_0043f070 | network |  |
| 0043f0c0 | FUN_0043f0c0 | NETMAN_Update | network | Network manager per-frame update |
| 0043f140 | FUN_0043f140 | NETMAN_0043f140 | network |  |
| 0043f2b0 | FUN_0043f2b0 | NETMAN_0043f2b0 | network |  |
| 0043f7b0 | FUN_0043f7b0 | NETMAN_0043f7b0 | network |  |
| 0043f880 | FUN_0043f880 | NETMAN_0043f880 | network |  |
| 0043f940 | FUN_0043f940 | NETMAN_RemoveInboundTrain | network | Removes incoming train from network |
| 0043fb50 | FUN_0043fb50 | NETMAN_0043fb50 | network |  |
| 0043fc50 | FUN_0043fc50 | NETMAN_0043fc50 | network |  |
| 0043fe30 | FUN_0043fe30 | NETMAN_0043fe30 | network |  |
| 00440070 | FUN_00440070 | NETMAN_00440070 | network |  |
| 00440150 | FUN_00440150 | NETMAN_00440150 | network |  |
| 00440310 | FUN_00440310 | NETMAN_00440310 | network |  |
| 00440390 | FUN_00440390 | NETMAN_00440390 | network |  |
| 00440410 | FUN_00440410 | NETMAN_00440410 | network |  |
| 004404c0 | FUN_004404c0 | NETMAN_004404c0 | network |  |
| 00440610 | FUN_00440610 | NETMAN_00440610 | network |  |
| 00440750 | FUN_00440750 | NETMAN_00440750 | network |  |
| 00440820 | FUN_00440820 | NETMAN_00440820 | network |  |
| 004408b0 | FUN_004408b0 | NETMAN_004408b0 | network |  |
| 00440a50 | FUN_00440a50 | NETMAN_00440a50 | network |  |
| 00440a80 | FUN_00440a80 | NETMAN_00440a80 | network |  |
| 00440c60 | FUN_00440c60 | NETMAN_00440c60 | network |  |
| 00440cc0 | FUN_00440cc0 | NETMAN_00440cc0 | network |  |
| 00440d00 | FUN_00440d00 | NETMAN_00440d00 | network |  |
| 00440ea0 | FUN_00440ea0 | NETMAN_00440ea0 | network |  |
| 00440f20 | FUN_00440f20 | NETMAN_00440f20 | network |  |
| 00440f80 | FUN_00440f80 | NETMAN_00440f80 | network |  |
| 00440fa0 | FUN_00440fa0 | NETMAN_00440fa0 | network |  |
| 00441190 | FUN_00441190 | NETMAN_00441190 | network |  |
| 004412f0 | FUN_004412f0 | NETMAN_004412f0 | network |  |
| 00441720 | FUN_00441720 | NETMAN_00441720 | network |  |
| 00441870 | FUN_00441870 | NETMAN_00441870 | network |  |
| 004419c0 | FUN_004419c0 | NETMAN_004419c0 | network |  |
| 00441a00 | FUN_00441a00 | NETMAN_00441a00 | network |  |
| 00441a90 | FUN_00441a90 | NETMAN_00441a90 | network |  |
| 00441b40 | FUN_00441b40 | NETMAN_00441b40 | network |  |
| 00441c80 | FUN_00441c80 | NETMAN_00441c80 | network |  |
| 00441f80 | FUN_00441f80 | NETMAN_00441f80 | network |  |
| 004421d0 | FUN_004421d0 | DPLAY_004421d0 | network |  |
| 004426d0 | FUN_004426d0 | DPLAY_004426d0 | network |  |
| 00442750 | FUN_00442750 | DPLAY_00442750 | network |  |
| 004427d0 | FUN_004427d0 | DPLAY_004427d0 | network |  |
| 00442850 | FUN_00442850 | DPLAY_00442850 | network |  |
| 004428e0 | FUN_004428e0 | DPLAY_004428e0 | network |  |
| 00442a00 | FUN_00442a00 | DPLAY_00442a00 | network |  |
| 00442a10 | FUN_00442a10 | DPLAY_00442a10 | network |  |
| 00442a70 | FUN_00442a70 | DPLAY_00442a70 | network |  |
| 00442b50 | FUN_00442b50 | DPLAY_00442b50 | network |  |
| 00442bf0 | FUN_00442bf0 | DPLAY_00442bf0 | network |  |
| 00442c90 | FUN_00442c90 | DPLAY_00442c90 | network |  |
| 00442d30 | FUN_00442d30 | DPLAY_00442d30 | network |  |
| 00442e00 | FUN_00442e00 | DPLAY_00442e00 | network |  |
| 00442ec0 | FUN_00442ec0 | DPLAY_00442ec0 | network |  |
| 00442fa0 | FUN_00442fa0 | DPLAY_00442fa0 | network |  |
| 00443000 | FUN_00443000 | DPLAY_00443000 | network |  |
| 004431f0 | FUN_004431f0 | DPLAY_004431f0 | network |  |
| 00443260 | FUN_00443260 | DPLAY_00443260 | network |  |
| 00443440 | FUN_00443440 | DPLAY_00443440 | network |  |
| 00443470 | FUN_00443470 | DPLAY_00443470 | network |  |
| 00443550 | FUN_00443550 | DPLAY_00443550 | network |  |
| 00443670 | FUN_00443670 | DPLAY_00443670 | network |  |
| 004436c0 | FUN_004436c0 | DPLAY_004436c0 | network |  |
| 004437c0 | FUN_004437c0 | DPLAY_004437c0 | network |  |
| 00443f00 | FUN_00443f00 | DPLAY_00443f00 | network |  |
| 00443ff0 | FUN_00443ff0 | DPLAY_00443ff0 | network |  |
| 004440a0 | FUN_004440a0 | NET_004440a0 | network |  |
| 004441c0 | FUN_004441c0 | NET_004441c0 | network |  |
| 004442b0 | FUN_004442b0 | NET_004442b0 | network |  |
| 004446f0 | FUN_004446f0 | NET_004446f0 | network |  |
| 00444c70 | FUN_00444c70 | NET_00444c70 | network |  |
| 00444d00 | FUN_00444d00 | NET_00444d00 | network |  |
| 00444fb0 | FUN_00444fb0 | NET_00444fb0 | network |  |
| 00445000 | FUN_00445000 | NET_00445000 | network |  |
| 00445170 | FUN_00445170 | NET_00445170 | network |  |
| 004451a0 | FUN_004451a0 | NET_004451a0 | network |  |
| 00445400 | FUN_00445400 | NET_00445400 | network |  |
| 00445510 | FUN_00445510 | NET_00445510 | network |  |
| 00445620 | FUN_00445620 | NET_00445620 | network |  |
| 00445700 | FUN_00445700 | NET_00445700 | network |  |
| 00445910 | FUN_00445910 | NET_00445910 | network |  |
| 00445930 | FUN_00445930 | NET_00445930 | network |  |
| 00445a40 | FUN_00445a40 | NET_00445a40 | network |  |
| 00445bd0 | FUN_00445bd0 | NET_00445bd0 | network |  |
| 00445f20 | FUN_00445f20 | NET_00445f20 | network |  |
| 00445f70 | FUN_00445f70 | NET_00445f70 | network |  |
| 00445fc0 | FUN_00445fc0 | NET_00445fc0 | network |  |
| 00445fe0 | FUN_00445fe0 | NET_00445fe0 | network |  |
| 00446030 | FUN_00446030 | RESMGR_00446030 | resource system |  |
| 00446050 | FUN_00446050 | ResourceManager_Init | resource system | Initializes resource manager |
| 00446340 | FUN_00446340 | RESMGR_00446340 | resource system |  |
| 004463c0 | FUN_004463c0 | RESMGR_004463c0 | resource system |  |
| 004467e0 | FUN_004467e0 | RESMGR_004467e0 | resource system |  |
| 00446840 | FUN_00446840 | RESMGR_00446840 | resource system |  |
| 00446cc0 | FUN_00446cc0 | RESMGR_00446cc0 | resource system |  |
| 00446ea0 | FUN_00446ea0 | ResourceManager_GetById | resource system | Gets resource by ID |
| 004470b0 | FUN_004470b0 | RESMGR_004470b0 | resource system |  |
| 00447290 | FUN_00447290 | RESMGR_00447290 | resource system |  |
| 004472b0 | FUN_004472b0 | RESMGR_004472b0 | resource system |  |
| 00447330 | FUN_00447330 | FormatResourceString | resource system | Formats localized resource string |
| 00447400 | FUN_00447400 | RESMGR_00447400 | resource system |  |
| 00447930 | FUN_00447930 | RESMGR_00447930 | resource system |  |
| 004479d0 | FUN_004479d0 | RESMGR_004479d0 | resource system |  |
| 00447a70 | FUN_00447a70 | RESMGR_00447a70 | resource system |  |
| 00447b20 | FUN_00447b20 | RESMGR_00447b20 | resource system |  |
| 00447b60 | FUN_00447b60 | RESMGR_00447b60 | resource system |  |
| 00447b90 | FUN_00447b90 | RESMGR_00447b90 | resource system |  |
| 00447ba0 | FUN_00447ba0 | RESMGR_00447ba0 | resource system |  |
| 00447d84 | Catch@00447d84 | CRT_catch_handler_00447d84 | resource system | C++ catch block handler |
| 00447d96 | Catch@00447d96 | CRT_catch_handler_00447d96 | resource system | C++ catch block handler |
| 00447db0 | FUN_00447db0 | RESMGR_00447db0 | resource system |  |
| 00447df0 | FUN_00447df0 | RESMGR_00447df0 | resource system |  |
| 00447e30 | FUN_00447e30 | RESMGR_00447e30 | resource system |  |
| 00447f50 | FUN_00447f50 | RESMGR_00447f50 | resource system |  |
| 00447f80 | FUN_00447f80 | RESMGR_00447f80 | resource system |  |
| 00447fb0 | FUN_00447fb0 | RESMGR_00447fb0 | resource system |  |
| 00448030 | FUN_00448030 | RESMGR_00448030 | resource system |  |
| 00448040 | FUN_00448040 | RESMGR_00448040 | resource system |  |
| 00448080 | FUN_00448080 | RESMGR_00448080 | resource system |  |
| 004480c0 | FUN_004480c0 | RESMGR_004480c0 | resource system |  |
| 00448120 | FUN_00448120 | RESMGR_00448120 | resource system |  |
| 004481b0 | FUN_004481b0 | RESMGR_004481b0 | resource system |  |
| 00448350 | FUN_00448350 | RESMGR_00448350 | resource system |  |
| 00448390 | FUN_00448390 | RESMGR_00448390 | resource system |  |
| 004487f0 | FUN_004487f0 | RESMGR_004487f0 | resource system |  |
| 00448970 | FUN_00448970 | RESMGR_00448970 | resource system |  |
| 00448990 | FUN_00448990 | RESMGR_00448990 | resource system |  |
| 004489d0 | FUN_004489d0 | RESMGR_004489d0 | resource system |  |
| 00448a20 | FUN_00448a20 | RESMGR_00448a20 | resource system |  |
| 00448a70 | FUN_00448a70 | RESMGR_00448a70 | resource system |  |
| 00448d60 | FUN_00448d60 | RESMGR_00448d60 | resource system |  |
| 00448ee0 | FUN_00448ee0 | RESMGR_00448ee0 | resource system |  |
| 00448f30 | FUN_00448f30 | RESMGR_00448f30 | resource system |  |
| 00448fe0 | FUN_00448fe0 | RESMGR_00448fe0 | resource system |  |
| 00449000 | FUN_00449000 | RESDATA_00449000 | resource system |  |
| 00449070 | FUN_00449070 | RESDATA_00449070 | resource system |  |
| 004490d0 | FUN_004490d0 | RESDATA_004490d0 | resource system |  |
| 004490e0 | FUN_004490e0 | RESDATA_004490e0 | resource system |  |
| 00449100 | FUN_00449100 | RESDATA_00449100 | resource system |  |
| 00449190 | FUN_00449190 | RESDATA_00449190 | resource system |  |
| 004493a0 | FUN_004493a0 | RESDATA_004493a0 | resource system |  |
| 004493c0 | FUN_004493c0 | RESDATA_004493c0 | resource system |  |
| 004493f0 | FUN_004493f0 | RESDATA_004493f0 | resource system |  |
| 00449410 | FUN_00449410 | RESDATA_00449410 | resource system |  |
| 00449420 | FUN_00449420 | RESDATA_00449420 | resource system |  |
| 00449430 | FUN_00449430 | RESDATA_00449430 | resource system |  |
| 004494c0 | FUN_004494c0 | RESDATA_004494c0 | resource system |  |
| 004494e0 | FUN_004494e0 | RESDATA_004494e0 | resource system |  |
| 004495b0 | FUN_004495b0 | RESDATA_004495b0 | resource system |  |
| 00449600 | FUN_00449600 | RESDATA_00449600 | resource system |  |
| 004497a0 | FUN_004497a0 | RESDATA_004497a0 | resource system |  |
| 00449c00 | FUN_00449c00 | RESDATA_00449c00 | resource system |  |
| 00449ce0 | FUN_00449ce0 | RESDATA_00449ce0 | resource system |  |
| 00449d00 | FUN_00449d00 | RESDATA_00449d00 | resource system |  |
| 00449d80 | FUN_00449d80 | RESDATA_00449d80 | resource system |  |
| 00449dc0 | FUN_00449dc0 | RESDATA_00449dc0 | resource system |  |
| 0044a0c0 | FUN_0044a0c0 | RESDATA_0044a0c0 | resource system |  |
| 0044a250 | FUN_0044a250 | RESDATA_0044a250 | resource system |  |
| 0044a9d0 | FUN_0044a9d0 | RESDATA_0044a9d0 | resource system |  |
| 0044ab80 | FUN_0044ab80 | RESDATA_0044ab80 | resource system |  |
| 0044ac20 | FUN_0044ac20 | RESDATA_0044ac20 | resource system |  |
| 0044adf0 | FUN_0044adf0 | RESDATA_0044adf0 | resource system |  |
| 0044ae80 | FUN_0044ae80 | RESDATA_0044ae80 | resource system |  |
| 0044b030 | FUN_0044b030 | RESDATA_0044b030 | resource system |  |
| 0044b050 | FUN_0044b050 | RESDATA_0044b050 | resource system |  |
| 0044b0b0 | FUN_0044b0b0 | RESDATA_0044b0b0 | resource system |  |
| 0044b190 | FUN_0044b190 | RESDATA_0044b190 | resource system |  |
| 0044b200 | FUN_0044b200 | RESDATA_0044b200 | resource system |  |
| 0044b220 | FUN_0044b220 | RESDATA_0044b220 | resource system |  |
| 0044b290 | FUN_0044b290 | RESDATA_0044b290 | resource system |  |
| 0044bcd0 | FUN_0044bcd0 | RESDATA_0044bcd0 | resource system |  |
| 0044bd10 | FUN_0044bd10 | RESDATA_0044bd10 | resource system |  |
| 0044bd30 | FUN_0044bd30 | RESDATA_0044bd30 | resource system |  |
| 0044bd50 | FUN_0044bd50 | RESDATA_0044bd50 | resource system |  |
| 0044bd70 | FUN_0044bd70 | RESDATA_0044bd70 | resource system |  |
| 0044bd90 | FUN_0044bd90 | RESDATA_0044bd90 | resource system |  |
| 0044bdb0 | FUN_0044bdb0 | RESDATA_0044bdb0 | resource system |  |
| 0044be50 | FUN_0044be50 | RESDATA_0044be50 | resource system |  |
| 0044c0b0 | FUN_0044c0b0 | RESDATA_0044c0b0 | resource system |  |
| 0044c0d0 | FUN_0044c0d0 | RESDATA_0044c0d0 | resource system |  |
| 0044c150 | FUN_0044c150 | RESDATA_0044c150 | resource system |  |
| 0044c170 | FUN_0044c170 | RESDATA_0044c170 | resource system |  |
| 0044c220 | FUN_0044c220 | RESDATA_0044c220 | resource system |  |
| 0044c310 | FUN_0044c310 | RESDATA_0044c310 | resource system |  |
| 0044c370 | FUN_0044c370 | RESDATA_0044c370 | resource system |  |
| 0044c3a0 | FUN_0044c3a0 | RESDATA_0044c3a0 | resource system |  |
| 0044c9b0 | FUN_0044c9b0 | RESDATA_0044c9b0 | resource system |  |
| 0044ca50 | FUN_0044ca50 | RESDATA_0044ca50 | resource system |  |
| 0044cab0 | FUN_0044cab0 | RESDATA_0044cab0 | resource system |  |
| 0044caf0 | FUN_0044caf0 | RESDATA_0044caf0 | resource system |  |
| 0044cb10 | FUN_0044cb10 | RESDATA_0044cb10 | resource system |  |
| 0044ce10 | FUN_0044ce10 | RESDATA_0044ce10 | resource system |  |
| 0044d4c0 | FUN_0044d4c0 | RESDATA_0044d4c0 | resource system |  |
| 0044d500 | FUN_0044d500 | RESDATA_0044d500 | resource system |  |
| 0044d5e0 | FUN_0044d5e0 | RESDATA_0044d5e0 | resource system |  |
| 0044d630 | FUN_0044d630 | RESDATA_0044d630 | resource system |  |
| 0044d6c0 | FUN_0044d6c0 | RESDATA_0044d6c0 | resource system |  |
| 0044d720 | FUN_0044d720 | RESDATA_0044d720 | resource system |  |
| 0044d740 | FUN_0044d740 | RESDATA_0044d740 | resource system |  |
| 0044d800 | FUN_0044d800 | RESDATA_0044d800 | resource system |  |
| 0044d830 | FUN_0044d830 | RESDATA_0044d830 | resource system |  |
| 0044d870 | FUN_0044d870 | RESDATA_0044d870 | resource system |  |
| 0044d8a0 | FUN_0044d8a0 | RESDATA_0044d8a0 | resource system |  |
| 0044d9b0 | FUN_0044d9b0 | RESDATA_0044d9b0 | resource system |  |
| 0044da50 | FUN_0044da50 | RESDATA_0044da50 | resource system |  |
| 0044dad0 | FUN_0044dad0 | RESDATA_0044dad0 | resource system |  |
| 0044dbb0 | FUN_0044dbb0 | RESDATA_0044dbb0 | resource system |  |
| 0044dbd0 | FUN_0044dbd0 | RESDATA_0044dbd0 | resource system |  |
| 0044dc10 | FUN_0044dc10 | RESDATA_0044dc10 | resource system |  |
| 0044dea0 | FUN_0044dea0 | RESDATA_0044dea0 | resource system |  |
| 0044df40 | FUN_0044df40 | RESDATA_0044df40 | resource system |  |
| 0044e020 | FUN_0044e020 | RESDATA_0044e020 | resource system |  |
| 0044e160 | FUN_0044e160 | RESDATA_0044e160 | resource system |  |
| 0044e200 | FUN_0044e200 | RESDATA_0044e200 | resource system |  |
| 0044e2d0 | FUN_0044e2d0 | RESDATA_0044e2d0 | resource system |  |
| 0044e2e0 | FUN_0044e2e0 | RESDATA_0044e2e0 | resource system |  |
| 0044e3f0 | FUN_0044e3f0 | RESDATA_0044e3f0 | resource system |  |
| 0044e630 | FUN_0044e630 | RESDATA_0044e630 | resource system |  |
| 0044e800 | FUN_0044e800 | RESDATA_0044e800 | resource system |  |
| 0044e830 | FUN_0044e830 | RESDATA_0044e830 | resource system |  |
| 0044e8d0 | FUN_0044e8d0 | RESDATA_0044e8d0 | resource system |  |
| 0044e910 | FUN_0044e910 | RESDATA_0044e910 | resource system |  |
| 0044e930 | FUN_0044e930 | RESDATA_0044e930 | resource system |  |
| 0044ef10 | FUN_0044ef10 | RESDATA_0044ef10 | resource system |  |
| 0044f210 | FUN_0044f210 | RESDATA_0044f210 | resource system |  |
| 0044f2a0 | FUN_0044f2a0 | RESDATA_0044f2a0 | resource system |  |
| 0044f2c0 | FUN_0044f2c0 | RESDATA_0044f2c0 | resource system |  |
| 0044f340 | FUN_0044f340 | RESDATA_0044f340 | resource system |  |
| 0044f3a0 | FUN_0044f3a0 | RESDATA_0044f3a0 | resource system |  |
| 0044f410 | FUN_0044f410 | RESDATA_0044f410 | resource system |  |
| 0044f490 | FUN_0044f490 | RESDATA_0044f490 | resource system |  |
| 0044f4f0 | FUN_0044f4f0 | RESDATA_0044f4f0 | resource system |  |
| 0044f510 | FUN_0044f510 | RESDATA_0044f510 | resource system |  |
| 0044f560 | FUN_0044f560 | RESDATA_0044f560 | resource system |  |
| 0044f750 | FUN_0044f750 | RESDATA_0044f750 | resource system |  |
| 0044f9a0 | FUN_0044f9a0 | RESDATA_0044f9a0 | resource system |  |
| 0044fb10 | FUN_0044fb10 | RESDATA_0044fb10 | resource system |  |
| 0044fc80 | FUN_0044fc80 | RESDATA_0044fc80 | resource system |  |
| 004500a0 | FUN_004500a0 | DSOUND_004500a0 | audio/DirectDraw |  |
| 00450240 | FUN_00450240 | DSOUND_00450240 | audio/DirectDraw |  |
| 00450450 | FUN_00450450 | DSOUND_00450450 | audio/DirectDraw |  |
| 00450520 | FUN_00450520 | DSOUND_00450520 | audio/DirectDraw |  |
| 00450850 | FUN_00450850 | DSOUND_00450850 | audio/DirectDraw |  |
| 00450ae0 | FUN_00450ae0 | DSOUND_00450ae0 | audio/DirectDraw |  |
| 00450ca0 | FUN_00450ca0 | DSOUND_00450ca0 | audio/DirectDraw |  |
| 00451180 | FUN_00451180 | DSOUND_00451180 | audio/DirectDraw |  |
| 00451440 | FUN_00451440 | DSOUND_00451440 | audio/DirectDraw |  |
| 00451540 | FUN_00451540 | DSOUND_00451540 | audio/DirectDraw |  |
| 004517b0 | FUN_004517b0 | DSOUND_004517b0 | audio/DirectDraw |  |
| 004518b0 | FUN_004518b0 | DSOUND_004518b0 | audio/DirectDraw |  |
| 00451920 | FUN_00451920 | DSOUND_00451920 | audio/DirectDraw |  |
| 00451c60 | FUN_00451c60 | DSOUND_00451c60 | audio/DirectDraw |  |
| 00451e90 | FUN_00451e90 | DSOUND_00451e90 | audio/DirectDraw |  |
| 00451fb0 | FUN_00451fb0 | DSOUND_00451fb0 | audio/DirectDraw |  |
| 00452170 | FUN_00452170 | DSOUND_00452170 | audio/DirectDraw |  |
| 00452230 | FUN_00452230 | DSOUND_00452230 | audio/DirectDraw |  |
| 00452570 | FUN_00452570 | DSOUND_00452570 | audio/DirectDraw |  |
| 004526b0 | FUN_004526b0 | DSOUND_004526b0 | audio/DirectDraw |  |
| 004527b0 | FUN_004527b0 | DSOUND_004527b0 | audio/DirectDraw |  |
| 00452b00 | FUN_00452b00 | DSOUND_00452b00 | audio/DirectDraw |  |
| 00452c00 | FUN_00452c00 | DSOUND_00452c00 | audio/DirectDraw |  |
| 00452ce0 | FUN_00452ce0 | DSOUND_00452ce0 | audio/DirectDraw |  |
| 00452d50 | FUN_00452d50 | DSOUND_00452d50 | audio/DirectDraw |  |
| 00452d60 | FUN_00452d60 | DSOUND_00452d60 | audio/DirectDraw |  |
| 00452d80 | FUN_00452d80 | DSOUND_00452d80 | audio/DirectDraw |  |
| 00452db0 | FUN_00452db0 | DSOUND_00452db0 | audio/DirectDraw |  |
| 00452df0 | FUN_00452df0 | DSOUND_00452df0 | audio/DirectDraw |  |
| 00452e10 | FUN_00452e10 | DSOUND_00452e10 | audio/DirectDraw |  |
| 00452fc0 | FUN_00452fc0 | DSOUND_00452fc0 | audio/DirectDraw |  |
| 004530c0 | FUN_004530c0 | DSOUND_004530c0 | audio/DirectDraw |  |
| 004532a0 | FUN_004532a0 | DSOUND_004532a0 | audio/DirectDraw |  |
| 00453320 | FUN_00453320 | DSOUND_00453320 | audio/DirectDraw |  |
| 004533d0 | FUN_004533d0 | DSOUND_004533d0 | audio/DirectDraw |  |
| 004533f0 | FUN_004533f0 | DSOUND_004533f0 | audio/DirectDraw |  |
| 00453450 | FUN_00453450 | DSOUND_00453450 | audio/DirectDraw |  |
| 00453fb0 | FUN_00453fb0 | DSOUND_00453fb0 | audio/DirectDraw |  |
| 00454250 | FUN_00454250 | DSOUND_00454250 | audio/DirectDraw |  |
| 00454330 | FUN_00454330 | DSOUND_00454330 | audio/DirectDraw |  |
| 00454380 | FUN_00454380 | DSOUND_00454380 | audio/DirectDraw |  |
| 004544a0 | FUN_004544a0 | DSOUND_004544a0 | audio/DirectDraw |  |
| 004544e0 | FUN_004544e0 | DSOUND_004544e0 | audio/DirectDraw |  |
| 00454580 | FUN_00454580 | DSOUND_00454580 | audio/DirectDraw |  |
| 004545a0 | FUN_004545a0 | DSOUND_004545a0 | audio/DirectDraw |  |
| 00454630 | FUN_00454630 | DSOUND_00454630 | audio/DirectDraw |  |
| 00454680 | FUN_00454680 | DSOUND_00454680 | audio/DirectDraw |  |
| 004546d0 | FUN_004546d0 | DSOUND_004546d0 | audio/DirectDraw |  |
| 00454820 | FUN_00454820 | DSOUND_00454820 | audio/DirectDraw |  |
| 00454890 | FUN_00454890 | DSOUND_00454890 | audio/DirectDraw |  |
| 00454900 | FUN_00454900 | DSOUND_00454900 | audio/DirectDraw |  |
| 004549e0 | FUN_004549e0 | DSOUND_004549e0 | audio/DirectDraw |  |
| 00454ae0 | FUN_00454ae0 | DSOUND_00454ae0 | audio/DirectDraw |  |
| 00454b50 | FUN_00454b50 | DSOUND_00454b50 | audio/DirectDraw |  |
| 00454b70 | FUN_00454b70 | DSOUND_00454b70 | audio/DirectDraw |  |
| 00454bc0 | FUN_00454bc0 | DSOUND_00454bc0 | audio/DirectDraw |  |
| 00454bf0 | FUN_00454bf0 | DSOUND_00454bf0 | audio/DirectDraw |  |
| 00454c30 | FUN_00454c30 | DSOUND_00454c30 | audio/DirectDraw |  |
| 00454cf0 | FUN_00454cf0 | DSOUND_00454cf0 | audio/DirectDraw |  |
| 00454de0 | FUN_00454de0 | DSOUND_00454de0 | audio/DirectDraw |  |
| 00454e60 | FUN_00454e60 | DSOUND_00454e60 | audio/DirectDraw |  |
| 00454fa0 | FUN_00454fa0 | DSOUND_00454fa0 | audio/DirectDraw |  |
| 00454fe0 | FUN_00454fe0 | DSOUND_00454fe0 | audio/DirectDraw |  |
| 004550c0 | FUN_004550c0 | DSOUND_004550c0 | audio/DirectDraw |  |
| 004553e0 | FUN_004553e0 | DSOUND_004553e0 | audio/DirectDraw |  |
| 00455620 | FUN_00455620 | DSOUND_00455620 | audio/DirectDraw |  |
| 00455670 | FUN_00455670 | DSOUND_00455670 | audio/DirectDraw |  |
| 004556f0 | FUN_004556f0 | DSOUND_004556f0 | audio/DirectDraw |  |
| 00455740 | FUN_00455740 | DSOUND_00455740 | audio/DirectDraw |  |
| 004557c0 | FUN_004557c0 | DSOUND_004557c0 | audio/DirectDraw |  |
| 00455840 | FUN_00455840 | DSOUND_00455840 | audio/DirectDraw |  |
| 00455960 | FUN_00455960 | DSOUND_00455960 | audio/DirectDraw |  |
| 00455ab0 | FUN_00455ab0 | DSOUND_00455ab0 | audio/DirectDraw |  |
| 00455d60 | FUN_00455d60 | DSOUND_00455d60 | audio/DirectDraw |  |
| 00456140 | FUN_00456140 | DSOUND_00456140 | audio/DirectDraw |  |
| 00456150 | FUN_00456150 | DSOUND_00456150 | audio/DirectDraw |  |
| 00456700 | FUN_00456700 | DSOUND_00456700 | audio/DirectDraw |  |
| 00456c60 | FUN_00456c60 | DSOUND_00456c60 | audio/DirectDraw |  |
| 00456d10 | FUN_00456d10 | DSOUND_00456d10 | audio/DirectDraw |  |
| 00456d90 | FUN_00456d90 | DSOUND_00456d90 | audio/DirectDraw |  |
| 00457080 | FUN_00457080 | DSOUND_00457080 | audio/DirectDraw |  |
| 00457320 | FUN_00457320 | DSOUND_00457320 | audio/DirectDraw |  |
| 00457380 | FUN_00457380 | DSOUND_00457380 | audio/DirectDraw |  |
| 004573e0 | FUN_004573e0 | DSOUND_004573e0 | audio/DirectDraw |  |
| 004576b0 | FUN_004576b0 | DSOUND_004576b0 | audio/DirectDraw |  |
| 00457830 | FUN_00457830 | DSOUND_00457830 | audio/DirectDraw |  |
| 00457900 | FUN_00457900 | DSOUND_00457900 | audio/DirectDraw |  |
| 004579d0 | FUN_004579d0 | DSOUND_004579d0 | audio/DirectDraw |  |
| 00457b60 | FUN_00457b60 | DSOUND_00457b60 | audio/DirectDraw |  |
| 00457c20 | FUN_00457c20 | DSOUND_00457c20 | audio/DirectDraw |  |
| 00457ce0 | FUN_00457ce0 | DSOUND_00457ce0 | audio/DirectDraw |  |
| 004580a0 | FUN_004580a0 | DSOUND_004580a0 | audio/DirectDraw |  |
| 00458270 | FUN_00458270 | DSOUND_00458270 | audio/DirectDraw |  |
| 00458310 | FUN_00458310 | DSOUND_00458310 | audio/DirectDraw |  |
| 00458350 | FUN_00458350 | DSOUND_00458350 | audio/DirectDraw |  |
| 004583c0 | FUN_004583c0 | DSOUND_004583c0 | audio/DirectDraw |  |
| 00458820 | FUN_00458820 | DSOUND_00458820 | audio/DirectDraw |  |
| 004588b0 | FUN_004588b0 | DSOUND_004588b0 | audio/DirectDraw |  |
| 00458940 | FUN_00458940 | DSOUND_00458940 | audio/DirectDraw |  |
| 004589b0 | FUN_004589b0 | DSOUND_004589b0 | audio/DirectDraw |  |
| 00458ad0 | FUN_00458ad0 | DSOUND_00458ad0 | audio/DirectDraw |  |
| 00458b00 | FUN_00458b00 | DSOUND_00458b00 | audio/DirectDraw |  |
| 00458bb0 | FUN_00458bb0 | DSOUND_00458bb0 | audio/DirectDraw |  |
| 00458c90 | FUN_00458c90 | DSOUND_00458c90 | audio/DirectDraw |  |
| 00459180 | FUN_00459180 | DDRAW_00459180 | audio/DirectDraw |  |
| 00459720 | FUN_00459720 | DDRAW_00459720 | audio/DirectDraw |  |
| 004597e0 | FUN_004597e0 | DDRAW_004597e0 | audio/DirectDraw |  |
| 00459d40 | FUN_00459d40 | DDRAW_00459d40 | audio/DirectDraw |  |
| 00459d60 | FUN_00459d60 | DDRAW_00459d60 | audio/DirectDraw |  |
| 00459da0 | FUN_00459da0 | DDRAW_00459da0 | audio/DirectDraw |  |
| 0045a1a0 | FUN_0045a1a0 | DDRAW_0045a1a0 | audio/DirectDraw |  |
| 0045a330 | FUN_0045a330 | DDRAW_0045a330 | audio/DirectDraw |  |
| 0045a400 | FUN_0045a400 | DDRAW_0045a400 | audio/DirectDraw |  |
| 0045a480 | FUN_0045a480 | DDRAW_0045a480 | audio/DirectDraw |  |
| 0045a740 | FUN_0045a740 | DDRAW_0045a740 | audio/DirectDraw |  |
| 0045b3a0 | FUN_0045b3a0 | DDRAW_0045b3a0 | audio/DirectDraw |  |
| 0045b500 | FUN_0045b500 | DDRAW_0045b500 | audio/DirectDraw |  |
| 0045b7e0 | FUN_0045b7e0 | DDRAW_0045b7e0 | audio/DirectDraw |  |
| 0045b940 | FUN_0045b940 | DDRAW_0045b940 | audio/DirectDraw |  |
| 0045b9b0 | FUN_0045b9b0 | DDRAW_0045b9b0 | audio/DirectDraw |  |
| 0045ba50 | FUN_0045ba50 | DDRAW_0045ba50 | audio/DirectDraw |  |
| 0045baa0 | FUN_0045baa0 | DDRAW_0045baa0 | audio/DirectDraw |  |
| 0045bb20 | FUN_0045bb20 | DDRAW_0045bb20 | audio/DirectDraw |  |
| 0045bbc0 | FUN_0045bbc0 | DDRAW_0045bbc0 | audio/DirectDraw |  |
| 0045c2e0 | FUN_0045c2e0 | DDRAW_0045c2e0 | audio/DirectDraw |  |
| 0045c3c0 | FUN_0045c3c0 | GameLoop_FrameUpdate | audio/DirectDraw | Per-frame game loop update |
| 0045c7a0 | FUN_0045c7a0 | DDRAW_0045c7a0 | audio/DirectDraw |  |
| 0045c7c0 | FUN_0045c7c0 | DDRAW_0045c7c0 | audio/DirectDraw |  |
| 0045c820 | FUN_0045c820 | DDRAW_0045c820 | audio/DirectDraw |  |
| 0045c830 | FUN_0045c830 | DDRAW_0045c830 | audio/DirectDraw |  |
| 0045c8a0 | FUN_0045c8a0 | DDRAW_0045c8a0 | audio/DirectDraw |  |
| 0045c970 | FUN_0045c970 | DDRAW_0045c970 | audio/DirectDraw |  |
| 0045ca10 | FUN_0045ca10 | DDRAW_0045ca10 | audio/DirectDraw |  |
| 0045ca20 | FUN_0045ca20 | DDRAW_0045ca20 | audio/DirectDraw |  |
| 0045caa0 | FUN_0045caa0 | DDRAW_0045caa0 | audio/DirectDraw |  |
| 0045cd00 | FUN_0045cd00 | DDRAW_0045cd00 | audio/DirectDraw |  |
| 0045cdf0 | FUN_0045cdf0 | DDRAW_0045cdf0 | audio/DirectDraw |  |
| 0045ce10 | FUN_0045ce10 | DDRAW_0045ce10 | audio/DirectDraw |  |
| 0045ce40 | FUN_0045ce40 | DDRAW_0045ce40 | audio/DirectDraw |  |
| 0045d1c0 | FUN_0045d1c0 | DDRAW_0045d1c0 | audio/DirectDraw |  |
| 0045d560 | FUN_0045d560 | DDRAW_0045d560 | audio/DirectDraw |  |
| 0045d5f0 | FUN_0045d5f0 | DDRAW_0045d5f0 | audio/DirectDraw |  |
| 0045d6c0 | FUN_0045d6c0 | DDRAW_0045d6c0 | audio/DirectDraw |  |
| 0045d810 | FUN_0045d810 | DDRAW_0045d810 | audio/DirectDraw |  |
| 0045d850 | FUN_0045d850 | DDRAW_0045d850 | audio/DirectDraw |  |
| 0045d8c0 | FUN_0045d8c0 | DDRAW_0045d8c0 | audio/DirectDraw |  |
| 0045d980 | FUN_0045d980 | DDRAW_0045d980 | audio/DirectDraw |  |
| 0045da40 | FUN_0045da40 | DDRAW_0045da40 | audio/DirectDraw |  |
| 0045da70 | FUN_0045da70 | DDRAW_0045da70 | audio/DirectDraw |  |
| 0045dad0 | FUN_0045dad0 | DDRAW_0045dad0 | audio/DirectDraw |  |
| 0045dbc0 | FUN_0045dbc0 | DDRAW_0045dbc0 | audio/DirectDraw |  |
| 0045dd80 | FUN_0045dd80 | DDRAW_0045dd80 | audio/DirectDraw |  |
| 0045dde0 | FUN_0045dde0 | DDRAW_0045dde0 | audio/DirectDraw |  |
| 0045e090 | FUN_0045e090 | DDRAW_0045e090 | audio/DirectDraw |  |
| 0045e1e0 | FUN_0045e1e0 | DDRAW_0045e1e0 | audio/DirectDraw |  |
| 0045e490 | FUN_0045e490 | DDRAW_0045e490 | audio/DirectDraw |  |
| 0045e4b0 | FUN_0045e4b0 | DDRAW_0045e4b0 | audio/DirectDraw |  |
| 0045e5a0 | FUN_0045e5a0 | DDRAW_0045e5a0 | audio/DirectDraw |  |
| 0045e700 | FUN_0045e700 | DDRAW_0045e700 | audio/DirectDraw |  |
| 0045e730 | FUN_0045e730 | DDRAW_0045e730 | audio/DirectDraw |  |
| 0045eab0 | FUN_0045eab0 | DDRAW_0045eab0 | audio/DirectDraw |  |
| 0045ee60 | FUN_0045ee60 | DDRAW_0045ee60 | audio/DirectDraw |  |
| 0045eec0 | FUN_0045eec0 | DDRAW_0045eec0 | audio/DirectDraw |  |
| 0045f090 | FUN_0045f090 | DDRAW_0045f090 | audio/DirectDraw |  |
| 0045f390 | FUN_0045f390 | DDRAW_0045f390 | audio/DirectDraw |  |
| 0045fbd0 | FUN_0045fbd0 | DDRAW_0045fbd0 | audio/DirectDraw |  |
| 0045fc30 | FUN_0045fc30 | DDRAW_0045fc30 | audio/DirectDraw |  |
| 0045fd80 | FUN_0045fd80 | DDRAW_0045fd80 | audio/DirectDraw |  |
| 0045ff30 | FUN_0045ff30 | DDRAW_0045ff30 | audio/DirectDraw |  |
| 00460360 | FUN_00460360 | WIN32_00460360 | Win32 platform |  |
| 004606d0 | FUN_004606d0 | WIN32_004606d0 | Win32 platform |  |
| 00460d40 | FUN_00460d40 | WIN32_00460d40 | Win32 platform |  |
| 00460ea0 | FUN_00460ea0 | WIN32_00460ea0 | Win32 platform |  |
| 00461610 | FUN_00461610 | WIN32_00461610 | Win32 platform |  |
| 00461640 | FUN_00461640 | WIN32_00461640 | Win32 platform |  |
| 00461690 | FUN_00461690 | WIN32_00461690 | Win32 platform |  |
| 004616c0 | FUN_004616c0 | WIN32_004616c0 | Win32 platform |  |
| 00461710 | FUN_00461710 | WIN32_00461710 | Win32 platform |  |
| 00461740 | FUN_00461740 | WIN32_00461740 | Win32 platform |  |
| 00461790 | FUN_00461790 | WIN32_00461790 | Win32 platform |  |
| 00462e90 | FUN_00462e90 | WinMain | Win32 platform | Main application entry point |
| 00463600 | FUN_00463600 | WIN32_00463600 | Win32 platform |  |
| 00463670 | FUN_00463670 | WIN32_00463670 | Win32 platform |  |
| 004637c0 | GetOpenFileNameA | COMDLG32_GetOpenFileNameA | Win32 platform | DLL stub: COMDLG32.DLL |
| 004637c6 | GetSaveFileNameA | COMDLG32_GetSaveFileNameA | Win32 platform | DLL stub: COMDLG32.DLL |
| 004637cc | DirectDrawCreate | DDRAW_DirectDrawCreate | Win32 platform | DLL stub: DDRAW.DLL |
| 004637d2 | Ordinal_4 | DPLAYX_Ordinal_4 | Win32 platform | DLL stub: DPLAYX.DLL |
| 004637d8 | Ordinal_1 | DPLAYX_Ordinal_1 | Win32 platform | DLL stub: DPLAYX.DLL |
| 004637de | Ordinal_1 | DSOUND_Ordinal_1 | Win32 platform | DLL stub: DSOUND.DLL |
| 004637e4 | Ordinal_2 | DSOUND_Ordinal_2 | Win32 platform | DLL stub: DSOUND.DLL |
| 004637ea | VerQueryValueA | VERSION_VerQueryValueA | Win32 platform | DLL stub: VERSION.DLL |
| 004637f0 | GetFileVersionInfoA | VERSION_GetFileVersionInfoA | Win32 platform | DLL stub: VERSION.DLL |
| 004637f6 | GetFileVersionInfoSizeA | VERSION_GetFileVersionInfoSizeA | Win32 platform | DLL stub: VERSION.DLL |
| 004637fc | MCIWndRegisterClass | MSVFW32_MCIWndRegisterClass | Win32 platform | DLL stub: MSVFW32.DLL |
| 00463810 | FUN_00463810 | WIN32_00463810 | Win32 platform |  |
| 00463890 | FUN_00463890 | WIN32_00463890 | Win32 platform |  |
| 00463970 | FUN_00463970 | WIN32_00463970 | Win32 platform |  |
| 00463a60 | FUN_00463a60 | WIN32_00463a60 | Win32 platform |  |
| 00463a80 | FUN_00463a80 | WIN32_00463a80 | Win32 platform |  |
| 00463aa0 | FUN_00463aa0 | WIN32_00463aa0 | Win32 platform |  |
| 00463b10 | FUN_00463b10 | WIN32_00463b10 | Win32 platform |  |
| 00463b70 | FUN_00463b70 | WIN32_00463b70 | Win32 platform |  |
| 00463b90 | FUN_00463b90 | WIN32_00463b90 | Win32 platform |  |
| 00463bb0 | FUN_00463bb0 | WIN32_00463bb0 | Win32 platform |  |
| 00463c30 | FUN_00463c30 | WIN32_00463c30 | Win32 platform |  |
| 00463cb0 | FUN_00463cb0 | WIN32_00463cb0 | Win32 platform |  |
| 00463e50 | FUN_00463e50 | WIN32_00463e50 | Win32 platform |  |
| 00463f50 | FUN_00463f50 | WIN32_00463f50 | Win32 platform |  |
| 00463fd0 | FUN_00463fd0 | WIN32_00463fd0 | Win32 platform |  |
| 00463ff0 | FUN_00463ff0 | WIN32_00463ff0 | Win32 platform |  |
| 004640b0 | FUN_004640b0 | WNDPROC_004640b0 | Win32 platform |  |
| 00464120 | FUN_00464120 | WNDPROC_00464120 | Win32 platform |  |
| 00464260 | FUN_00464260 | WNDPROC_00464260 | Win32 platform |  |
| 004642f0 | FUN_004642f0 | WNDPROC_004642f0 | Win32 platform |  |
| 00464490 | FUN_00464490 | WNDPROC_00464490 | Win32 platform |  |
| 00464550 | FUN_00464550 | WNDPROC_00464550 | Win32 platform |  |
| 00464570 | FUN_00464570 | WNDPROC_00464570 | Win32 platform |  |
| 00464590 | FUN_00464590 | WNDPROC_00464590 | Win32 platform |  |
| 00464600 | FUN_00464600 | WNDPROC_00464600 | Win32 platform |  |
| 00464620 | FUN_00464620 | WNDPROC_00464620 | Win32 platform |  |
| 00464680 | FUN_00464680 | WNDPROC_00464680 | Win32 platform |  |
| 004646c0 | FUN_004646c0 | WNDPROC_004646c0 | Win32 platform |  |
| 00464750 | FUN_00464750 | WNDPROC_00464750 | Win32 platform |  |
| 00464840 | FUN_00464840 | WNDPROC_00464840 | Win32 platform |  |
| 004648e0 | FUN_004648e0 | WNDPROC_004648e0 | Win32 platform |  |
| 004648f0 | FUN_004648f0 | WNDPROC_004648f0 | Win32 platform |  |
| 004649f0 | FUN_004649f0 | WNDPROC_004649f0 | Win32 platform |  |
| 00464b10 | FUN_00464b10 | WNDPROC_00464b10 | Win32 platform |  |
| 00464bc0 | FUN_00464bc0 | WNDPROC_00464bc0 | Win32 platform |  |
| 00464c70 | FUN_00464c70 | WNDPROC_00464c70 | Win32 platform |  |
| 00464d70 | FUN_00464d70 | WNDPROC_00464d70 | Win32 platform |  |
| 00464d80 | FUN_00464d80 | WNDPROC_00464d80 | Win32 platform |  |
| 00464d90 | FUN_00464d90 | WNDPROC_00464d90 | Win32 platform |  |
| 00464da0 | FUN_00464da0 | WNDPROC_00464da0 | Win32 platform |  |
| 00464db0 | FUN_00464db0 | WNDPROC_00464db0 | Win32 platform |  |
| 00464e50 | FUN_00464e50 | WNDPROC_00464e50 | Win32 platform |  |
| 00464ef0 | FUN_00464ef0 | WNDPROC_00464ef0 | Win32 platform |  |
| 00464f70 | FUN_00464f70 | WNDPROC_00464f70 | Win32 platform |  |
| 00465010 | FUN_00465010 | WNDPROC_00465010 | Win32 platform |  |
| 00465090 | FUN_00465090 | WNDPROC_00465090 | Win32 platform |  |
| 00465180 | FUN_00465180 | WNDPROC_00465180 | Win32 platform |  |
| 004651a0 | FUN_004651a0 | WNDPROC_004651a0 | Win32 platform |  |
| 00465200 | FUN_00465200 | WNDPROC_00465200 | Win32 platform |  |
| 00465250 | FUN_00465250 | WNDPROC_00465250 | Win32 platform |  |
| 004652a0 | FUN_004652a0 | WNDPROC_004652a0 | Win32 platform |  |
| 004652d0 | FUN_004652d0 | WNDPROC_004652d0 | Win32 platform |  |
| 00465470 | FUN_00465470 | WNDPROC_00465470 | Win32 platform |  |
| 004654c0 | FUN_004654c0 | WNDPROC_004654c0 | Win32 platform |  |
| 004654e0 | FUN_004654e0 | WNDPROC_004654e0 | Win32 platform |  |
| 00465560 | FUN_00465560 | WNDPROC_00465560 | Win32 platform |  |
| 004656b0 | FUN_004656b0 | WNDPROC_004656b0 | Win32 platform |  |
| 004656d0 | FUN_004656d0 | WNDPROC_004656d0 | Win32 platform |  |
| 004656f0 | FUN_004656f0 | WNDPROC_004656f0 | Win32 platform |  |
| 00465730 | FUN_00465730 | WNDPROC_00465730 | Win32 platform |  |
| 004657a0 | FUN_004657a0 | WNDPROC_004657a0 | Win32 platform |  |
| 00465810 | FUN_00465810 | WNDPROC_00465810 | Win32 platform |  |
| 00465890 | FUN_00465890 | WNDPROC_00465890 | Win32 platform |  |
| 00465960 | FUN_00465960 | WNDPROC_00465960 | Win32 platform |  |
| 00465a30 | FUN_00465a30 | WNDPROC_00465a30 | Win32 platform |  |
| 00465ac0 | FUN_00465ac0 | WNDPROC_00465ac0 | Win32 platform |  |
| 00465ad0 | FUN_00465ad0 | WNDPROC_00465ad0 | Win32 platform |  |
| 00465cd0 | FUN_00465cd0 | WNDPROC_00465cd0 | Win32 platform |  |
| 00465ce0 | FUN_00465ce0 | operator_new | Win32 platform | C++ operator new |
| 00465cf0 | FUN_00465cf0 | WNDPROC_00465cf0 | Win32 platform |  |
| 00465d30 | FUN_00465d30 | WNDPROC_00465d30 | Win32 platform |  |
| 00465d40 | FUN_00465d40 | WNDPROC_00465d40 | Win32 platform |  |
| 00465da0 | FUN_00465da0 | WNDPROC_00465da0 | Win32 platform |  |
| 00465de0 | FUN_00465de0 | WNDPROC_00465de0 | Win32 platform |  |
| 00465e40 | FUN_00465e40 | WNDPROC_00465e40 | Win32 platform |  |
| 00465e70 | FUN_00465e70 | WNDPROC_00465e70 | Win32 platform |  |
| 00465f40 | FUN_00465f40 | WNDPROC_00465f40 | Win32 platform |  |
| 00465fd0 | FUN_00465fd0 | WNDPROC_00465fd0 | Win32 platform |  |
| 00466050 | __global_unwind2 | global_unwind2 | CRT | Named by Ghidra analysis |
| 00466092 | __local_unwind2 | local_unwind2 | CRT | Named by Ghidra analysis |
| 004660fa | __abnormal_termination | abnormal_termination | CRT | Named by Ghidra analysis |
| 0046611d | __NLG_Notify1 | NLG_Notify1 | CRT | Named by Ghidra analysis |
| 00466126 | FUN_00466126 | CRT_00466126 | CRT |  |
| 00466140 | FUN_00466140 | CRT_00466140 | CRT |  |
| 00466150 | FUN_00466150 | CRT_00466150 | CRT |  |
| 00466180 | __fpmath | fpmath | CRT | Named by Ghidra analysis |
| 004661a0 | FUN_004661a0 | CRT_004661a0 | CRT |  |
| 004661b0 | FUN_004661b0 | CRT_004661b0 | CRT |  |
| 004661f0 | _strncpy | strncpy | CRT | Named by Ghidra analysis |
| 004662f0 | FUN_004662f0 | CRT_004662f0 | CRT |  |
| 00466390 | FUN_00466390 | CRT_00466390 | CRT |  |
| 004663a0 | FUN_004663a0 | CRT_004663a0 | CRT |  |
| 00466490 | FUN_00466490 | CRT_00466490 | CRT |  |
| 004664c0 | FUN_004664c0 | CRT_004664c0 | CRT |  |
| 00466590 | FUN_00466590 | CRT_00466590 | CRT |  |
| 004668d0 | FUN_004668d0 | CRT_004668d0 | CRT |  |
| 00466950 | FUN_00466950 | CRT_00466950 | CRT |  |
| 00466980 | FUN_00466980 | CRT_00466980 | CRT |  |
| 00466ab0 | FUN_00466ab0 | CRT_00466ab0 | CRT |  |
| 00466af0 | FUN_00466af0 | CRT_00466af0 | CRT |  |
| 00466c20 | FUN_00466c20 | CRT_00466c20 | CRT |  |
| 00466c50 | FUN_00466c50 | CRT_00466c50 | CRT |  |
| 00466c70 | FUN_00466c70 | CRT_00466c70 | CRT |  |
| 00466ce0 | FUN_00466ce0 | CRT_00466ce0 | CRT |  |
| 00466d30 | __ftol | ftol | CRT | Named by Ghidra analysis |
| 00466d60 | FUN_00466d60 | CRT_00466d60 | CRT |  |
| 00466de0 | _strchr | strchr | CRT | Named by Ghidra analysis |
| 00466ea0 | FUN_00466ea0 | CRT_00466ea0 | CRT |  |
| 004671e0 | FUN_004671e0 | CRT_004671e0 | CRT |  |
| 00467258 | FUN_00467258 | CRT_00467258 | CRT |  |
| 00467280 | FUN_00467280 | CRT_00467280 | CRT |  |
| 004672f9 | FUN_004672f9 | CRT_004672f9 | CRT |  |
| 00467330 | FUN_00467330 | CRT_00467330 | CRT |  |
| 004673c0 | FUN_004673c0 | CRT_004673c0 | CRT |  |
| 004673e0 | FUN_004673e0 | CRT_004673e0 | CRT |  |
| 00467430 | FUN_00467430 | CRT_00467430 | CRT |  |
| 00467490 | FUN_00467490 | CRT_00467490 | CRT |  |
| 004674e0 | FUN_004674e0 | CRT_004674e0 | CRT |  |
| 004676d0 | _strpbrk | strpbrk | CRT | Named by Ghidra analysis |
| 00467710 | FUN_00467710 | CRT_00467710 | CRT |  |
| 004677a0 | FUN_004677a0 | CRT_004677a0 | CRT |  |
| 004678a0 | FUN_004678a0 | CRT_004678a0 | CRT |  |
| 00467a20 | FUN_00467a20 | CRT_00467a20 | CRT |  |
| 00467b50 | FUN_00467b50 | CRT_00467b50 | CRT |  |
| 00467c70 | FUN_00467c70 | CRT_00467c70 | CRT |  |
| 00467ca0 | FUN_00467ca0 | CRT_00467ca0 | CRT |  |
| 00467d30 | _strncat | strncat | CRT | Named by Ghidra analysis |
| 00467e60 | _strrchr | strrchr | CRT | Named by Ghidra analysis |
| 00467ea0 | FUN_00467ea0 | CRT_00467ea0 | CRT |  |
| 00467ee0 | FUN_00467ee0 | CRT_00467ee0 | CRT |  |
| 00467f50 | FUN_00467f50 | CRT_00467f50 | CRT |  |
| 00467fd0 | FUN_00467fd0 | CRT_00467fd0 | CRT |  |
| 00467fe0 | FUN_00467fe0 | CRT_00467fe0 | CRT |  |
| 00467ff0 | FUN_00467ff0 | CRT_00467ff0 | CRT |  |
| 00468060 | _strstr | strstr | CRT | Named by Ghidra analysis |
| 004680e0 | FUN_004680e0 | CRT_004680e0 | CRT |  |
| 00468170 | FUN_00468170 | CRT_00468170 | CRT |  |
| 004681d0 | FUN_004681d0 | CRT_004681d0 | CRT |  |
| 00468210 | FUN_00468210 | CRT_00468210 | CRT |  |
| 00468280 | FUN_00468280 | CRT_00468280 | CRT |  |
| 004682c0 | FUN_004682c0 | CRT_004682c0 | CRT |  |
| 00468300 | FUN_00468300 | CRT_00468300 | CRT |  |
| 00468380 | FUN_00468380 | CRT_00468380 | CRT |  |
| 00468440 | FUN_00468440 | CRT_00468440 | CRT |  |
| 00468480 | FUN_00468480 | CRT_00468480 | CRT |  |
| 004684a0 | FUN_004684a0 | CRT_004684a0 | CRT |  |
| 004684d0 | FUN_004684d0 | CRT_004684d0 | CRT |  |
| 004684f0 | __exit | exit | CRT | Named by Ghidra analysis |
| 00468510 | FUN_00468510 | CRT_00468510 | CRT |  |
| 004685d0 | FUN_004685d0 | CRT_004685d0 | CRT |  |
| 004685e0 | FUN_004685e0 | CRT_004685e0 | CRT |  |
| 004685f0 | FUN_004685f0 | CRT_004685f0 | CRT |  |
| 00468610 | FUN_00468610 | CRT_00468610 | CRT |  |
| 00468650 | FUN_00468650 | CRT_00468650 | CRT |  |
| 00468790 | FUN_00468790 | CRT_00468790 | CRT |  |
| 004687d0 | FUN_004687d0 | CRT_004687d0 | CRT |  |
| 00468870 | FUN_00468870 | CRT_00468870 | CRT |  |
| 004688f0 | FUN_004688f0 | CRT_004688f0 | CRT |  |
| 004689a0 | FUN_004689a0 | CRT_004689a0 | CRT |  |
| 004689e0 | entry | crt_entry | CRT | CRT startup entry point |
| 00468b90 | __amsg_exit | amsg_exit | CRT | Named by Ghidra analysis |
| 00468bc0 | FUN_00468bc0 | CRT_00468bc0 | CRT |  |
| 00468bf0 | FUN_00468bf0 | CRT_00468bf0 | CRT |  |
| 00468c60 | FUN_00468c60 | CRT_00468c60 | CRT |  |
| 00468cf0 | FUN_00468cf0 | CRT_00468cf0 | CRT |  |
| 00468d70 | FUN_00468d70 | CRT_00468d70 | CRT |  |
| 00468f80 | FUN_00468f80 | CRT_00468f80 | CRT |  |
| 00469000 | FUN_00469000 | CRT_00469000 | CRT |  |
| 00469230 | FUN_00469230 | CRT_00469230 | CRT |  |
| 004692b0 | FUN_004692b0 | CRT_004692b0 | CRT |  |
| 00469330 | FUN_00469330 | CRT_00469330 | CRT |  |
| 00469540 | FUN_00469540 | CRT_00469540 | CRT |  |
| 00469560 | FUN_00469560 | CRT_00469560 | CRT |  |
| 004697f0 | FUN_004697f0 | CRT_004697f0 | CRT |  |
| 00469810 | FUN_00469810 | CRT_00469810 | CRT |  |
| 00469840 | FUN_00469840 | CRT_00469840 | CRT |  |
| 00469870 | FUN_00469870 | CRT_00469870 | CRT |  |
| 004698a0 | FUN_004698a0 | CRT_004698a0 | CRT |  |
| 00469c40 | FUN_00469c40 | CRT_00469c40 | CRT |  |
| 00469d90 | FUN_00469d90 | CRT_00469d90 | CRT |  |
| 00469e60 | FUN_00469e60 | CRT_00469e60 | CRT |  |
| 0046a120 | FUN_0046a120 | CRT_0046a120 | CRT |  |
| 0046a200 | FUN_0046a200 | CRT_0046a200 | CRT |  |
| 0046a2c0 | FUN_0046a2c0 | CRT_0046a2c0 | CRT |  |
| 0046a350 | FUN_0046a350 | CRT_0046a350 | CRT |  |
| 0046a448 | FUN_0046a448 | CRT_0046a448 | CRT |  |
| 0046a4e0 | FUN_0046a4e0 | CRT_0046a4e0 | CRT |  |
| 0046a6f0 | FUN_0046a6f0 | CRT_0046a6f0 | CRT |  |
| 0046a770 | FUN_0046a770 | CRT_0046a770 | CRT |  |
| 0046a7a0 | __CallSettingFrame@12 | CallSettingFrame@12 | CRT | Named by Ghidra analysis |
| 0046a7f0 | FUN_0046a7f0 | CRT_0046a7f0 | CRT |  |
| 0046a850 | FUN_0046a850 | CRT_0046a850 | CRT |  |
| 0046a870 | FUN_0046a870 | CRT_0046a870 | CRT |  |
| 0046a8f0 | FUN_0046a8f0 | CRT_0046a8f0 | CRT |  |
| 0046a9a0 | FUN_0046a9a0 | CRT_0046a9a0 | CRT |  |
| 0046aa17 | _abort | CRT_0046aa17 | CRT |  |
| 0046aa30 | FUN_0046aa30 | CRT_0046aa30 | CRT |  |
| 0046aa9e | FUN_0046aa9e | CRT_0046aa9e | CRT |  |
| 0046aac0 | __setdefaultprecision | setdefaultprecision | CRT | Named by Ghidra analysis |
| 0046aae0 | FUN_0046aae0 | CRT_0046aae0 | CRT |  |
| 0046ab30 | FUN_0046ab30 | CRT_0046ab30 | CRT |  |
| 0046ab60 | FUN_0046ab60 | CRT_0046ab60 | CRT |  |
| 0046acb0 | FUN_0046acb0 | CRT_0046acb0 | CRT |  |
| 0046ad30 | FUN_0046ad30 | CRT_0046ad30 | CRT |  |
| 0046ae30 | FUN_0046ae30 | CRT_0046ae30 | CRT |  |
| 0046aea0 | FUN_0046aea0 | CRT_0046aea0 | CRT |  |
| 0046af60 | FUN_0046af60 | CRT_0046af60 | CRT |  |
| 0046b090 | FUN_0046b090 | CRT_0046b090 | CRT |  |
| 0046b0c0 | FUN_0046b0c0 | CRT_0046b0c0 | CRT |  |
| 0046b160 | __allmul | allmul | CRT | Named by Ghidra analysis |
| 0046b1a0 | FUN_0046b1a0 | CRT_0046b1a0 | CRT |  |
| 0046b350 | FUN_0046b350 | CRT_0046b350 | CRT |  |
| 0046b3e0 | FUN_0046b3e0 | CRT_0046b3e0 | CRT |  |
| 0046b4d0 | FUN_0046b4d0 | CRT_0046b4d0 | CRT |  |
| 0046b5a0 | FUN_0046b5a0 | CRT_0046b5a0 | CRT |  |
| 0046b5f0 | FUN_0046b5f0 | CRT_0046b5f0 | CRT |  |
| 0046b690 | FUN_0046b690 | CRT_0046b690 | CRT |  |
| 0046b740 | FUN_0046b740 | CRT_0046b740 | CRT |  |
| 0046b770 | FUN_0046b770 | CRT_0046b770 | CRT |  |
| 0046b7f0 | FUN_0046b7f0 | CRT_0046b7f0 | CRT |  |
| 0046b810 | FUN_0046b810 | CRT_0046b810 | CRT |  |
| 0046b850 | FUN_0046b850 | CRT_0046b850 | CRT |  |
| 0046b880 | FUN_0046b880 | CRT_0046b880 | CRT |  |
| 0046b8c0 | FUN_0046b8c0 | CRT_0046b8c0 | CRT |  |
| 0046b8f0 | FUN_0046b8f0 | CRT_0046b8f0 | CRT |  |
| 0046ba20 | FUN_0046ba20 | CRT_0046ba20 | CRT |  |
| 0046c3b0 | FUN_0046c3b0 | CRT_0046c3b0 | CRT |  |
| 0046c400 | FUN_0046c400 | CRT_0046c400 | CRT |  |
| 0046c440 | FUN_0046c440 | CRT_0046c440 | CRT |  |
| 0046c480 | FUN_0046c480 | CRT_0046c480 | CRT |  |
| 0046c4a0 | FUN_0046c4a0 | CRT_0046c4a0 | CRT |  |
| 0046c4c0 | FUN_0046c4c0 | CRT_0046c4c0 | CRT |  |
| 0046c4e0 | FUN_0046c4e0 | CRT_0046c4e0 | CRT |  |
| 0046c520 | FUN_0046c520 | CRT_0046c520 | CRT |  |
| 0046c690 | FUN_0046c690 | CRT_0046c690 | CRT |  |
| 0046c6f0 | FUN_0046c6f0 | CRT_0046c6f0 | CRT |  |
| 0046c7c0 | FUN_0046c7c0 | CRT_0046c7c0 | CRT |  |
| 0046c820 | FUN_0046c820 | CRT_0046c820 | CRT |  |
| 0046c880 | FUN_0046c880 | CRT_0046c880 | CRT |  |
| 0046cac0 | FUN_0046cac0 | CRT_0046cac0 | CRT |  |
| 0046cc40 | FUN_0046cc40 | CRT_0046cc40 | CRT |  |
| 0046cd10 | FUN_0046cd10 | CRT_0046cd10 | CRT |  |
| 0046ce65 | FUN_0046ce65 | CRT_0046ce65 | CRT |  |
| 0046ce80 | FUN_0046ce80 | CRT_0046ce80 | CRT |  |
| 0046cea0 | FUN_0046cea0 | CRT_0046cea0 | CRT |  |
| 0046dbe0 | FUN_0046dbe0 | CRT_0046dbe0 | CRT |  |
| 0046dc20 | FUN_0046dc20 | CRT_0046dc20 | CRT |  |
| 0046dc50 | FUN_0046dc50 | CRT_0046dc50 | CRT |  |
| 0046dc70 | FUN_0046dc70 | CRT_0046dc70 | CRT |  |
| 0046dcc0 | FUN_0046dcc0 | CRT_0046dcc0 | CRT |  |
| 0046dd00 | FUN_0046dd00 | CRT_0046dd00 | CRT |  |
| 0046dfe0 | __isindst | isindst | CRT | Named by Ghidra analysis |
| 0046e010 | FUN_0046e010 | CRT_0046e010 | CRT |  |
| 0046e280 | FUN_0046e280 | CRT_0046e280 | CRT |  |
| 0046e420 | FUN_0046e420 | CRT_0046e420 | CRT |  |
| 0046e5a0 | FUN_0046e5a0 | CRT_0046e5a0 | CRT |  |
| 0046e7d0 | FUN_0046e7d0 | CRT_0046e7d0 | CRT |  |
| 0046ea00 | FUN_0046ea00 | CRT_0046ea00 | CRT |  |
| 0046ea50 | FUN_0046ea50 | CRT_0046ea50 | CRT |  |
| 0046eab0 | FUN_0046eab0 | CRT_0046eab0 | CRT |  |
| 0046eaf0 | FUN_0046eaf0 | CRT_0046eaf0 | CRT |  |
| 0046ecd0 | FUN_0046ecd0 | CRT_0046ecd0 | CRT |  |
| 0046ece0 | FUN_0046ece0 | CRT_0046ece0 | CRT |  |
| 0046ed90 | __aulldiv | aulldiv | CRT | Named by Ghidra analysis |
| 0046ee00 | __aullrem | aullrem | CRT | Named by Ghidra analysis |
| 0046ee80 | FUN_0046ee80 | CRT_0046ee80 | CRT |  |
| 0046f030 | FUN_0046f030 | CRT_0046f030 | CRT |  |
| 0046f0a0 | FUN_0046f0a0 | CRT_0046f0a0 | CRT |  |
| 0046f0e0 | FUN_0046f0e0 | CRT_0046f0e0 | CRT |  |
| 0046f180 | FUN_0046f180 | CRT_0046f180 | CRT |  |
| 0046f350 | FUN_0046f350 | CRT_0046f350 | CRT |  |
| 0046f430 | FUN_0046f430 | CRT_0046f430 | CRT |  |
| 0046f520 | FUN_0046f520 | CRT_0046f520 | CRT |  |
| 0046f6d0 | FUN_0046f6d0 | CRT_0046f6d0 | CRT |  |
| 0046f9d0 | FUN_0046f9d0 | CRT_0046f9d0 | CRT |  |
| 0046f9f0 | FUN_0046f9f0 | CRT_0046f9f0 | CRT |  |
| 0046fa30 | FUN_0046fa30 | CRT_0046fa30 | CRT |  |
| 0046fb20 | FUN_0046fb20 | CRT_0046fb20 | CRT |  |
| 0046fbc0 | FUN_0046fbc0 | CRT_0046fbc0 | CRT |  |
| 0046fdd0 | FUN_0046fdd0 | CRT_0046fdd0 | CRT |  |
| 0046ff30 | FUN_0046ff30 | CRT_0046ff30 | CRT |  |
| 0046ff70 | FUN_0046ff70 | CRT_0046ff70 | CRT |  |
| 00470150 | FUN_00470150 | CRT_00470150 | CRT |  |
| 004702c0 | FUN_004702c0 | CRT_004702c0 | CRT |  |
| 00470370 | FUN_00470370 | CRT_00470370 | CRT |  |
| 00470410 | FUN_00470410 | CRT_00470410 | CRT |  |
| 00470460 | FUN_00470460 | CRT_00470460 | CRT |  |
| 004704d0 | FUN_004704d0 | CRT_004704d0 | CRT |  |
| 00470500 | FUN_00470500 | CRT_00470500 | CRT |  |
| 00470650 | FUN_00470650 | CRT_00470650 | CRT |  |
| 004706f0 | FUN_004706f0 | CRT_004706f0 | CRT |  |
| 00470710 | FUN_00470710 | CRT_00470710 | CRT |  |
| 00470730 | FUN_00470730 | CRT_00470730 | CRT |  |
| 00470750 | _abort | abort | CRT | Named by Ghidra analysis |
| 00470770 | FUN_00470770 | CRT_00470770 | CRT |  |
| 004707b0 | FUN_004707b0 | CRT_004707b0 | CRT |  |
| 004707d0 | FUN_004707d0 | CRT_004707d0 | CRT |  |
| 00470870 | FUN_00470870 | CRT_00470870 | CRT |  |
| 00470900 | FUN_00470900 | CRT_00470900 | CRT |  |
| 00470990 | FUN_00470990 | CRT_00470990 | CRT |  |
| 00470a90 | FUN_00470a90 | CRT_00470a90 | CRT |  |
| 00470b00 | FUN_00470b00 | CRT_00470b00 | CRT |  |
| 00470b70 | FUN_00470b70 | CRT_00470b70 | CRT |  |
| 00470c10 | FUN_00470c10 | CRT_00470c10 | CRT |  |
| 00470c30 | FUN_00470c30 | CRT_00470c30 | CRT |  |
| 00470c40 | FUN_00470c40 | CRT_00470c40 | CRT |  |
| 00470c60 | FUN_00470c60 | CRT_00470c60 | CRT |  |
| 00470d20 | FUN_00470d20 | CRT_00470d20 | CRT |  |
| 00470ef0 | FUN_00470ef0 | CRT_00470ef0 | CRT |  |
| 00470f10 | FUN_00470f10 | CRT_00470f10 | CRT |  |
| 00470f30 | FUN_00470f30 | CRT_00470f30 | CRT |  |
| 00470f70 | FUN_00470f70 | CRT_00470f70 | CRT |  |
| 00470fb0 | FUN_00470fb0 | CRT_00470fb0 | CRT |  |
| 00471050 | FUN_00471050 | CRT_00471050 | CRT |  |
| 004710e0 | FUN_004710e0 | CRT_004710e0 | CRT |  |
| 004711a0 | FUN_004711a0 | CRT_004711a0 | CRT |  |
| 004711b0 | FUN_004711b0 | CRT_004711b0 | CRT |  |
| 00471340 | FUN_00471340 | CRT_00471340 | CRT |  |
| 00471480 | FUN_00471480 | CRT_00471480 | CRT |  |
| 00471550 | FUN_00471550 | CRT_00471550 | CRT |  |
| 004715b0 | FUN_004715b0 | CRT_004715b0 | CRT |  |
| 004715e0 | FUN_004715e0 | CRT_004715e0 | CRT |  |
| 00471650 | FUN_00471650 | CRT_00471650 | CRT |  |
| 004716d0 | FUN_004716d0 | CRT_004716d0 | CRT |  |
| 00471750 | FUN_00471750 | CRT_00471750 | CRT |  |
| 00471850 | __allshl | allshl | CRT | Named by Ghidra analysis |
| 00471870 | FUN_00471870 | CRT_00471870 | CRT |  |
| 00471900 | FUN_00471900 | CRT_00471900 | CRT |  |
| 00471980 | FUN_00471980 | CRT_00471980 | CRT |  |
| 00471b70 | FUN_00471b70 | CRT_00471b70 | CRT |  |
| 00471bb0 | FUN_00471bb0 | CRT_00471bb0 | CRT |  |
| 00471cf0 | FUN_00471cf0 | CRT_00471cf0 | CRT |  |
| 00472070 | FUN_00472070 | CRT_00472070 | CRT |  |
| 004722b0 | FUN_004722b0 | CRT_004722b0 | CRT |  |
| 004726a0 | FUN_004726a0 | CRT_004726a0 | CRT |  |
| 004727f0 | FUN_004727f0 | CRT_004727f0 | CRT |  |
| 00472830 | FUN_00472830 | CRT_00472830 | CRT |  |
| 00472b60 | _strcspn | strcspn | CRT | Named by Ghidra analysis |
| 00472ba0 | _strncmp | strncmp | CRT | Named by Ghidra analysis |
| 00472be0 | FUN_00472be0 | CRT_00472be0 | CRT |  |
| 00472c00 | FUN_00472c00 | CRT_00472c00 | CRT |  |
| 00472c90 | FUN_00472c90 | CRT_00472c90 | CRT |  |
| 00472ea0 | FUN_00472ea0 | CRT_00472ea0 | CRT |  |
| 00472ee0 | FUN_00472ee0 | CRT_00472ee0 | CRT |  |
| 00472f10 | FUN_00472f10 | CRT_00472f10 | CRT |  |
| 00472f80 | FUN_00472f80 | CRT_00472f80 | CRT |  |
| 00472fb0 | FUN_00472fb0 | CRT_00472fb0 | CRT |  |
| 00472fe0 | FUN_00472fe0 | CRT_00472fe0 | CRT |  |
| 004730e0 | FUN_004730e0 | CRT_004730e0 | CRT |  |
| 00473870 | FUN_00473870 | CRT_00473870 | CRT |  |
| 00473c00 | FUN_00473c00 | CRT_00473c00 | CRT |  |
| 00473c40 | FUN_00473c40 | CRT_00473c40 | CRT |  |
| 00473cc0 | FUN_00473cc0 | CRT_00473cc0 | CRT |  |
| 00473e70 | FUN_00473e70 | CRT_00473e70 | CRT |  |
| 00473f80 | FUN_00473f80 | CRT_00473f80 | CRT |  |
| 00474240 | FUN_00474240 | CRT_00474240 | CRT |  |
| 004742d0 | FUN_004742d0 | CRT_004742d0 | CRT |  |
| 004745a0 | FUN_004745a0 | CRT_004745a0 | CRT |  |
| 004745d0 | FUN_004745d0 | CRT_004745d0 | CRT |  |
| 004747e0 | FUN_004747e0 | CRT_004747e0 | CRT |  |
| 00474860 | FUN_00474860 | CRT_00474860 | CRT |  |
| 004748d0 | FUN_004748d0 | CRT_004748d0 | CRT |  |
| 00474a00 | FUN_00474a00 | CRT_00474a00 | CRT |  |
| 00474b40 | FUN_00474b40 | CRT_00474b40 | CRT |  |
| 00474c10 | FUN_00474c10 | CRT_00474c10 | CRT |  |
| 00474c60 | RtlUnwind | KERNEL32_RtlUnwind | CRT | DLL stub: KERNEL32.DLL |
| 00474c70 | FUN_00474c70 | CRT_00474c70 | CRT |  |
| 00474df0 | Unwind@00474df0 | CRT_unwind_handler_00474df0 | CRT | SEH/C++ unwind handler trampoline |
| 00474e10 | Unwind@00474e10 | CRT_unwind_handler_00474e10 | CRT | SEH/C++ unwind handler trampoline |
| 00474e1b | Unwind@00474e1b | CRT_unwind_handler_00474e1b | CRT | SEH/C++ unwind handler trampoline |
| 00474e26 | Unwind@00474e26 | CRT_unwind_handler_00474e26 | CRT | SEH/C++ unwind handler trampoline |
| 00474e31 | Unwind@00474e31 | CRT_unwind_handler_00474e31 | CRT | SEH/C++ unwind handler trampoline |
| 00474e3c | Unwind@00474e3c | CRT_unwind_handler_00474e3c | CRT | SEH/C++ unwind handler trampoline |
| 00474e47 | Unwind@00474e47 | CRT_unwind_handler_00474e47 | CRT | SEH/C++ unwind handler trampoline |
| 00474e52 | Unwind@00474e52 | CRT_unwind_handler_00474e52 | CRT | SEH/C++ unwind handler trampoline |
| 00474e5d | Unwind@00474e5d | CRT_unwind_handler_00474e5d | CRT | SEH/C++ unwind handler trampoline |
| 00474e68 | Unwind@00474e68 | CRT_unwind_handler_00474e68 | CRT | SEH/C++ unwind handler trampoline |
| 00474e73 | Unwind@00474e73 | CRT_unwind_handler_00474e73 | CRT | SEH/C++ unwind handler trampoline |
| 00474e7e | Unwind@00474e7e | CRT_unwind_handler_00474e7e | CRT | SEH/C++ unwind handler trampoline |
| 00474e89 | Unwind@00474e89 | CRT_unwind_handler_00474e89 | CRT | SEH/C++ unwind handler trampoline |
| 00474e94 | Unwind@00474e94 | CRT_unwind_handler_00474e94 | CRT | SEH/C++ unwind handler trampoline |
| 00474eb0 | Unwind@00474eb0 | CRT_unwind_handler_00474eb0 | CRT | SEH/C++ unwind handler trampoline |
| 00474ed0 | Unwind@00474ed0 | CRT_unwind_handler_00474ed0 | CRT | SEH/C++ unwind handler trampoline |
| 00474ef0 | Unwind@00474ef0 | CRT_unwind_handler_00474ef0 | CRT | SEH/C++ unwind handler trampoline |
| 00474f10 | Unwind@00474f10 | CRT_unwind_handler_00474f10 | CRT | SEH/C++ unwind handler trampoline |
| 00474f30 | Unwind@00474f30 | CRT_unwind_handler_00474f30 | CRT | SEH/C++ unwind handler trampoline |
| 00474f3b | Unwind@00474f3b | CRT_unwind_handler_00474f3b | CRT | SEH/C++ unwind handler trampoline |
| 00474f46 | Unwind@00474f46 | CRT_unwind_handler_00474f46 | CRT | SEH/C++ unwind handler trampoline |
| 00474f51 | Unwind@00474f51 | CRT_unwind_handler_00474f51 | CRT | SEH/C++ unwind handler trampoline |
| 00474f5c | Unwind@00474f5c | CRT_unwind_handler_00474f5c | CRT | SEH/C++ unwind handler trampoline |
| 00474f67 | Unwind@00474f67 | CRT_unwind_handler_00474f67 | CRT | SEH/C++ unwind handler trampoline |
| 00474f80 | Unwind@00474f80 | CRT_unwind_handler_00474f80 | CRT | SEH/C++ unwind handler trampoline |
| 00474f8b | Unwind@00474f8b | CRT_unwind_handler_00474f8b | CRT | SEH/C++ unwind handler trampoline |
| 00474f96 | Unwind@00474f96 | CRT_unwind_handler_00474f96 | CRT | SEH/C++ unwind handler trampoline |
| 00474fa1 | Unwind@00474fa1 | CRT_unwind_handler_00474fa1 | CRT | SEH/C++ unwind handler trampoline |
| 00474fac | Unwind@00474fac | CRT_unwind_handler_00474fac | CRT | SEH/C++ unwind handler trampoline |
| 00474fb7 | Unwind@00474fb7 | CRT_unwind_handler_00474fb7 | CRT | SEH/C++ unwind handler trampoline |
| 00474fc2 | Unwind@00474fc2 | CRT_unwind_handler_00474fc2 | CRT | SEH/C++ unwind handler trampoline |
| 00474fcd | Unwind@00474fcd | CRT_unwind_handler_00474fcd | CRT | SEH/C++ unwind handler trampoline |
| 00474ff0 | Unwind@00474ff0 | CRT_unwind_handler_00474ff0 | CRT | SEH/C++ unwind handler trampoline |
| 00475010 | Unwind@00475010 | CRT_unwind_handler_00475010 | CRT | SEH/C++ unwind handler trampoline |
| 0047501b | Unwind@0047501b | CRT_unwind_handler_0047501b | CRT | SEH/C++ unwind handler trampoline |
| 00475026 | Unwind@00475026 | CRT_unwind_handler_00475026 | CRT | SEH/C++ unwind handler trampoline |
| 00475031 | Unwind@00475031 | CRT_unwind_handler_00475031 | CRT | SEH/C++ unwind handler trampoline |
| 0047503c | Unwind@0047503c | CRT_unwind_handler_0047503c | CRT | SEH/C++ unwind handler trampoline |
| 00475047 | Unwind@00475047 | CRT_unwind_handler_00475047 | CRT | SEH/C++ unwind handler trampoline |
| 00475060 | Unwind@00475060 | CRT_unwind_handler_00475060 | CRT | SEH/C++ unwind handler trampoline |
| 00475080 | Unwind@00475080 | CRT_unwind_handler_00475080 | CRT | SEH/C++ unwind handler trampoline |
| 0047508b | Unwind@0047508b | CRT_unwind_handler_0047508b | CRT | SEH/C++ unwind handler trampoline |
| 004750a0 | Unwind@004750a0 | CRT_unwind_handler_004750a0 | CRT | SEH/C++ unwind handler trampoline |
| 004750c0 | Unwind@004750c0 | CRT_unwind_handler_004750c0 | CRT | SEH/C++ unwind handler trampoline |
| 004750e0 | Unwind@004750e0 | CRT_unwind_handler_004750e0 | CRT | SEH/C++ unwind handler trampoline |
| 004750e8 | Unwind@004750e8 | CRT_unwind_handler_004750e8 | CRT | SEH/C++ unwind handler trampoline |
| 004750f6 | Unwind@004750f6 | CRT_unwind_handler_004750f6 | CRT | SEH/C++ unwind handler trampoline |
| 00475101 | Unwind@00475101 | CRT_unwind_handler_00475101 | CRT | SEH/C++ unwind handler trampoline |
| 00475120 | Unwind@00475120 | CRT_unwind_handler_00475120 | CRT | SEH/C++ unwind handler trampoline |
| 00475128 | Unwind@00475128 | CRT_unwind_handler_00475128 | CRT | SEH/C++ unwind handler trampoline |
| 00475140 | Unwind@00475140 | CRT_unwind_handler_00475140 | CRT | SEH/C++ unwind handler trampoline |
| 00475160 | Unwind@00475160 | CRT_unwind_handler_00475160 | CRT | SEH/C++ unwind handler trampoline |
| 0047516b | Unwind@0047516b | CRT_unwind_handler_0047516b | CRT | SEH/C++ unwind handler trampoline |
| 00475190 | Unwind@00475190 | CRT_unwind_handler_00475190 | CRT | SEH/C++ unwind handler trampoline |
| 0047519e | Unwind@0047519e | CRT_unwind_handler_0047519e | CRT | SEH/C++ unwind handler trampoline |
| 004751c0 | Unwind@004751c0 | CRT_unwind_handler_004751c0 | CRT | SEH/C++ unwind handler trampoline |
| 004751e0 | Unwind@004751e0 | CRT_unwind_handler_004751e0 | CRT | SEH/C++ unwind handler trampoline |
| 00475210 | Unwind@00475210 | CRT_unwind_handler_00475210 | CRT | SEH/C++ unwind handler trampoline |
| 0047521b | Unwind@0047521b | CRT_unwind_handler_0047521b | CRT | SEH/C++ unwind handler trampoline |
| 00475240 | Unwind@00475240 | CRT_unwind_handler_00475240 | CRT | SEH/C++ unwind handler trampoline |
| 00475248 | Unwind@00475248 | CRT_unwind_handler_00475248 | CRT | SEH/C++ unwind handler trampoline |
| 00475260 | Unwind@00475260 | CRT_unwind_handler_00475260 | CRT | SEH/C++ unwind handler trampoline |
| 00475280 | Unwind@00475280 | CRT_unwind_handler_00475280 | CRT | SEH/C++ unwind handler trampoline |
| 004752a0 | Unwind@004752a0 | CRT_unwind_handler_004752a0 | CRT | SEH/C++ unwind handler trampoline |
| 004752c0 | Unwind@004752c0 | CRT_unwind_handler_004752c0 | CRT | SEH/C++ unwind handler trampoline |
| 004752cb | Unwind@004752cb | CRT_unwind_handler_004752cb | CRT | SEH/C++ unwind handler trampoline |
| 004752e0 | Unwind@004752e0 | CRT_unwind_handler_004752e0 | CRT | SEH/C++ unwind handler trampoline |
| 00475300 | Unwind@00475300 | CRT_unwind_handler_00475300 | CRT | SEH/C++ unwind handler trampoline |
| 0047530e | Unwind@0047530e | CRT_unwind_handler_0047530e | CRT | SEH/C++ unwind handler trampoline |
| 0047531c | Unwind@0047531c | CRT_unwind_handler_0047531c | CRT | SEH/C++ unwind handler trampoline |
| 0047532a | Unwind@0047532a | CRT_unwind_handler_0047532a | CRT | SEH/C++ unwind handler trampoline |
| 00475338 | Unwind@00475338 | CRT_unwind_handler_00475338 | CRT | SEH/C++ unwind handler trampoline |
| 00475346 | Unwind@00475346 | CRT_unwind_handler_00475346 | CRT | SEH/C++ unwind handler trampoline |
| 00475354 | Unwind@00475354 | CRT_unwind_handler_00475354 | CRT | SEH/C++ unwind handler trampoline |
| 00475362 | Unwind@00475362 | CRT_unwind_handler_00475362 | CRT | SEH/C++ unwind handler trampoline |
| 00475370 | Unwind@00475370 | CRT_unwind_handler_00475370 | CRT | SEH/C++ unwind handler trampoline |
| 0047537e | Unwind@0047537e | CRT_unwind_handler_0047537e | CRT | SEH/C++ unwind handler trampoline |
| 0047538c | Unwind@0047538c | CRT_unwind_handler_0047538c | CRT | SEH/C++ unwind handler trampoline |
| 0047539a | Unwind@0047539a | CRT_unwind_handler_0047539a | CRT | SEH/C++ unwind handler trampoline |
| 004753a8 | Unwind@004753a8 | CRT_unwind_handler_004753a8 | CRT | SEH/C++ unwind handler trampoline |
| 004753b6 | Unwind@004753b6 | CRT_unwind_handler_004753b6 | CRT | SEH/C++ unwind handler trampoline |
| 004753c4 | Unwind@004753c4 | CRT_unwind_handler_004753c4 | CRT | SEH/C++ unwind handler trampoline |
| 004753d2 | Unwind@004753d2 | CRT_unwind_handler_004753d2 | CRT | SEH/C++ unwind handler trampoline |
| 004753e0 | Unwind@004753e0 | CRT_unwind_handler_004753e0 | CRT | SEH/C++ unwind handler trampoline |
| 004753ee | Unwind@004753ee | CRT_unwind_handler_004753ee | CRT | SEH/C++ unwind handler trampoline |
| 004753fc | Unwind@004753fc | CRT_unwind_handler_004753fc | CRT | SEH/C++ unwind handler trampoline |
| 0047540a | Unwind@0047540a | CRT_unwind_handler_0047540a | CRT | SEH/C++ unwind handler trampoline |
| 00475418 | Unwind@00475418 | CRT_unwind_handler_00475418 | CRT | SEH/C++ unwind handler trampoline |
| 00475426 | Unwind@00475426 | CRT_unwind_handler_00475426 | CRT | SEH/C++ unwind handler trampoline |
| 00475434 | Unwind@00475434 | CRT_unwind_handler_00475434 | CRT | SEH/C++ unwind handler trampoline |
| 00475442 | Unwind@00475442 | CRT_unwind_handler_00475442 | CRT | SEH/C++ unwind handler trampoline |
| 00475450 | Unwind@00475450 | CRT_unwind_handler_00475450 | CRT | SEH/C++ unwind handler trampoline |
| 0047545e | Unwind@0047545e | CRT_unwind_handler_0047545e | CRT | SEH/C++ unwind handler trampoline |
| 0047546c | Unwind@0047546c | CRT_unwind_handler_0047546c | CRT | SEH/C++ unwind handler trampoline |
| 00475490 | Unwind@00475490 | CRT_unwind_handler_00475490 | CRT | SEH/C++ unwind handler trampoline |
| 004754b0 | Unwind@004754b0 | CRT_unwind_handler_004754b0 | CRT | SEH/C++ unwind handler trampoline |
| 004754d0 | Unwind@004754d0 | CRT_unwind_handler_004754d0 | CRT | SEH/C++ unwind handler trampoline |
| 004754f0 | Unwind@004754f0 | CRT_unwind_handler_004754f0 | CRT | SEH/C++ unwind handler trampoline |
| 00475510 | Unwind@00475510 | CRT_unwind_handler_00475510 | CRT | SEH/C++ unwind handler trampoline |
| 00475530 | Unwind@00475530 | CRT_unwind_handler_00475530 | CRT | SEH/C++ unwind handler trampoline |
| 0047553b | Unwind@0047553b | CRT_unwind_handler_0047553b | CRT | SEH/C++ unwind handler trampoline |
| 00475546 | Unwind@00475546 | CRT_unwind_handler_00475546 | CRT | SEH/C++ unwind handler trampoline |
| 00475551 | Unwind@00475551 | CRT_unwind_handler_00475551 | CRT | SEH/C++ unwind handler trampoline |
| 00475570 | Unwind@00475570 | CRT_unwind_handler_00475570 | CRT | SEH/C++ unwind handler trampoline |
| 00475578 | Unwind@00475578 | CRT_unwind_handler_00475578 | CRT | SEH/C++ unwind handler trampoline |
| 00475586 | Unwind@00475586 | CRT_unwind_handler_00475586 | CRT | SEH/C++ unwind handler trampoline |
| 004755a0 | Unwind@004755a0 | CRT_unwind_handler_004755a0 | CRT | SEH/C++ unwind handler trampoline |
| 004755a8 | Unwind@004755a8 | CRT_unwind_handler_004755a8 | CRT | SEH/C++ unwind handler trampoline |
| 004755b6 | Unwind@004755b6 | CRT_unwind_handler_004755b6 | CRT | SEH/C++ unwind handler trampoline |
| 004755d0 | Unwind@004755d0 | CRT_unwind_handler_004755d0 | CRT | SEH/C++ unwind handler trampoline |
| 004755db | Unwind@004755db | CRT_unwind_handler_004755db | CRT | SEH/C++ unwind handler trampoline |
| 00475600 | Unwind@00475600 | CRT_unwind_handler_00475600 | CRT | SEH/C++ unwind handler trampoline |
| 00475620 | Unwind@00475620 | CRT_unwind_handler_00475620 | CRT | SEH/C++ unwind handler trampoline |
| 00475640 | Unwind@00475640 | CRT_unwind_handler_00475640 | CRT | SEH/C++ unwind handler trampoline |
| 00475660 | Unwind@00475660 | CRT_unwind_handler_00475660 | CRT | SEH/C++ unwind handler trampoline |
| 00475680 | Unwind@00475680 | CRT_unwind_handler_00475680 | CRT | SEH/C++ unwind handler trampoline |
| 004756a0 | Unwind@004756a0 | CRT_unwind_handler_004756a0 | CRT | SEH/C++ unwind handler trampoline |
| 004756ab | Unwind@004756ab | CRT_unwind_handler_004756ab | CRT | SEH/C++ unwind handler trampoline |
| 004756c0 | Unwind@004756c0 | CRT_unwind_handler_004756c0 | CRT | SEH/C++ unwind handler trampoline |
| 004756cb | Unwind@004756cb | CRT_unwind_handler_004756cb | CRT | SEH/C++ unwind handler trampoline |
| 004756e0 | Unwind@004756e0 | CRT_unwind_handler_004756e0 | CRT | SEH/C++ unwind handler trampoline |
| 00475700 | Unwind@00475700 | CRT_unwind_handler_00475700 | CRT | SEH/C++ unwind handler trampoline |
| 00475720 | Unwind@00475720 | CRT_unwind_handler_00475720 | CRT | SEH/C++ unwind handler trampoline |
| 0047572b | Unwind@0047572b | CRT_unwind_handler_0047572b | CRT | SEH/C++ unwind handler trampoline |
| 00475740 | Unwind@00475740 | CRT_unwind_handler_00475740 | CRT | SEH/C++ unwind handler trampoline |
| 0047574e | Unwind@0047574e | CRT_unwind_handler_0047574e | CRT | SEH/C++ unwind handler trampoline |
| 00475770 | Unwind@00475770 | CRT_unwind_handler_00475770 | CRT | SEH/C++ unwind handler trampoline |
| 00475790 | Unwind@00475790 | CRT_unwind_handler_00475790 | CRT | SEH/C++ unwind handler trampoline |
| 004757b0 | Unwind@004757b0 | CRT_unwind_handler_004757b0 | CRT | SEH/C++ unwind handler trampoline |
| 004757bb | Unwind@004757bb | CRT_unwind_handler_004757bb | CRT | SEH/C++ unwind handler trampoline |
| 004757d0 | Unwind@004757d0 | CRT_unwind_handler_004757d0 | CRT | SEH/C++ unwind handler trampoline |
| 004757f0 | Unwind@004757f0 | CRT_unwind_handler_004757f0 | CRT | SEH/C++ unwind handler trampoline |
| 00475810 | Unwind@00475810 | CRT_unwind_handler_00475810 | CRT | SEH/C++ unwind handler trampoline |
| 0047581b | Unwind@0047581b | CRT_unwind_handler_0047581b | CRT | SEH/C++ unwind handler trampoline |
| 00475826 | Unwind@00475826 | CRT_unwind_handler_00475826 | CRT | SEH/C++ unwind handler trampoline |
| 00475840 | Unwind@00475840 | CRT_unwind_handler_00475840 | CRT | SEH/C++ unwind handler trampoline |
| 0047584b | Unwind@0047584b | CRT_unwind_handler_0047584b | CRT | SEH/C++ unwind handler trampoline |
| 00475860 | Unwind@00475860 | CRT_unwind_handler_00475860 | CRT | SEH/C++ unwind handler trampoline |
| 00475880 | Unwind@00475880 | CRT_unwind_handler_00475880 | CRT | SEH/C++ unwind handler trampoline |
| 004758a0 | Unwind@004758a0 | CRT_unwind_handler_004758a0 | CRT | SEH/C++ unwind handler trampoline |
| 004758ab | Unwind@004758ab | CRT_unwind_handler_004758ab | CRT | SEH/C++ unwind handler trampoline |
| 004758d0 | Unwind@004758d0 | CRT_unwind_handler_004758d0 | CRT | SEH/C++ unwind handler trampoline |
| 004758f0 | Unwind@004758f0 | CRT_unwind_handler_004758f0 | CRT | SEH/C++ unwind handler trampoline |
| 00475910 | Unwind@00475910 | CRT_unwind_handler_00475910 | CRT | SEH/C++ unwind handler trampoline |
| 00475918 | Unwind@00475918 | CRT_unwind_handler_00475918 | CRT | SEH/C++ unwind handler trampoline |
| 00475930 | Unwind@00475930 | CRT_unwind_handler_00475930 | CRT | SEH/C++ unwind handler trampoline |
| 00475938 | Unwind@00475938 | CRT_unwind_handler_00475938 | CRT | SEH/C++ unwind handler trampoline |
| 00475946 | Unwind@00475946 | CRT_unwind_handler_00475946 | CRT | SEH/C++ unwind handler trampoline |
| 00475960 | Unwind@00475960 | CRT_unwind_handler_00475960 | CRT | SEH/C++ unwind handler trampoline |
| 0047596b | Unwind@0047596b | CRT_unwind_handler_0047596b | CRT | SEH/C++ unwind handler trampoline |
| 00475990 | Unwind@00475990 | CRT_unwind_handler_00475990 | CRT | SEH/C++ unwind handler trampoline |
| 004759b0 | Unwind@004759b0 | CRT_unwind_handler_004759b0 | CRT | SEH/C++ unwind handler trampoline |
| 004759d0 | Unwind@004759d0 | CRT_unwind_handler_004759d0 | CRT | SEH/C++ unwind handler trampoline |
| 004759db | Unwind@004759db | CRT_unwind_handler_004759db | CRT | SEH/C++ unwind handler trampoline |
| 004759f0 | Unwind@004759f0 | CRT_unwind_handler_004759f0 | CRT | SEH/C++ unwind handler trampoline |
| 00475a10 | Unwind@00475a10 | CRT_unwind_handler_00475a10 | CRT | SEH/C++ unwind handler trampoline |
| 00475a30 | Unwind@00475a30 | CRT_unwind_handler_00475a30 | CRT | SEH/C++ unwind handler trampoline |
| 00475a50 | Unwind@00475a50 | CRT_unwind_handler_00475a50 | CRT | SEH/C++ unwind handler trampoline |
| 00475a70 | Unwind@00475a70 | CRT_unwind_handler_00475a70 | CRT | SEH/C++ unwind handler trampoline |
| 00475a7b | Unwind@00475a7b | CRT_unwind_handler_00475a7b | CRT | SEH/C++ unwind handler trampoline |
| 00475a86 | Unwind@00475a86 | CRT_unwind_handler_00475a86 | CRT | SEH/C++ unwind handler trampoline |
| 00475a91 | Unwind@00475a91 | CRT_unwind_handler_00475a91 | CRT | SEH/C++ unwind handler trampoline |
| 00475a9c | Unwind@00475a9c | CRT_unwind_handler_00475a9c | CRT | SEH/C++ unwind handler trampoline |
| 00475aa7 | Unwind@00475aa7 | CRT_unwind_handler_00475aa7 | CRT | SEH/C++ unwind handler trampoline |
| 00475ab2 | Unwind@00475ab2 | CRT_unwind_handler_00475ab2 | CRT | SEH/C++ unwind handler trampoline |
| 00475abd | Unwind@00475abd | CRT_unwind_handler_00475abd | CRT | SEH/C++ unwind handler trampoline |
| 00475ae0 | Unwind@00475ae0 | CRT_unwind_handler_00475ae0 | CRT | SEH/C++ unwind handler trampoline |
| 00475b00 | Unwind@00475b00 | CRT_unwind_handler_00475b00 | CRT | SEH/C++ unwind handler trampoline |
| 00475b20 | Unwind@00475b20 | CRT_unwind_handler_00475b20 | CRT | SEH/C++ unwind handler trampoline |
| 00475b2b | Unwind@00475b2b | CRT_unwind_handler_00475b2b | CRT | SEH/C++ unwind handler trampoline |
| 00475b36 | Unwind@00475b36 | CRT_unwind_handler_00475b36 | CRT | SEH/C++ unwind handler trampoline |
| 00475b50 | Unwind@00475b50 | CRT_unwind_handler_00475b50 | CRT | SEH/C++ unwind handler trampoline |
| 00475b70 | Unwind@00475b70 | CRT_unwind_handler_00475b70 | CRT | SEH/C++ unwind handler trampoline |
| 00475b90 | Unwind@00475b90 | CRT_unwind_handler_00475b90 | CRT | SEH/C++ unwind handler trampoline |
| 00475bb0 | Unwind@00475bb0 | CRT_unwind_handler_00475bb0 | CRT | SEH/C++ unwind handler trampoline |
| 00475bd0 | Unwind@00475bd0 | CRT_unwind_handler_00475bd0 | CRT | SEH/C++ unwind handler trampoline |
| 00475bdb | Unwind@00475bdb | CRT_unwind_handler_00475bdb | CRT | SEH/C++ unwind handler trampoline |
| 00475be6 | Unwind@00475be6 | CRT_unwind_handler_00475be6 | CRT | SEH/C++ unwind handler trampoline |
| 00475bf1 | Unwind@00475bf1 | CRT_unwind_handler_00475bf1 | CRT | SEH/C++ unwind handler trampoline |
| 00475c10 | Unwind@00475c10 | CRT_unwind_handler_00475c10 | CRT | SEH/C++ unwind handler trampoline |
| 00475c1b | Unwind@00475c1b | CRT_unwind_handler_00475c1b | CRT | SEH/C++ unwind handler trampoline |
| 00475c26 | Unwind@00475c26 | CRT_unwind_handler_00475c26 | CRT | SEH/C++ unwind handler trampoline |
| 00475c40 | Unwind@00475c40 | CRT_unwind_handler_00475c40 | CRT | SEH/C++ unwind handler trampoline |
| 00475c4b | Unwind@00475c4b | CRT_unwind_handler_00475c4b | CRT | SEH/C++ unwind handler trampoline |
| 00475c60 | Unwind@00475c60 | CRT_unwind_handler_00475c60 | CRT | SEH/C++ unwind handler trampoline |
| 00475c80 | Unwind@00475c80 | CRT_unwind_handler_00475c80 | CRT | SEH/C++ unwind handler trampoline |
| 00475c8b | Unwind@00475c8b | CRT_unwind_handler_00475c8b | CRT | SEH/C++ unwind handler trampoline |
| 00475cb0 | Unwind@00475cb0 | CRT_unwind_handler_00475cb0 | CRT | SEH/C++ unwind handler trampoline |
| 00475cd0 | Unwind@00475cd0 | CRT_unwind_handler_00475cd0 | CRT | SEH/C++ unwind handler trampoline |
| 00475cf0 | Unwind@00475cf0 | CRT_unwind_handler_00475cf0 | CRT | SEH/C++ unwind handler trampoline |
| 00475d10 | Unwind@00475d10 | CRT_unwind_handler_00475d10 | CRT | SEH/C++ unwind handler trampoline |
| 00475d1b | Unwind@00475d1b | CRT_unwind_handler_00475d1b | CRT | SEH/C++ unwind handler trampoline |
| 00475d40 | Unwind@00475d40 | CRT_unwind_handler_00475d40 | CRT | SEH/C++ unwind handler trampoline |
| 00475d4e | Unwind@00475d4e | CRT_unwind_handler_00475d4e | CRT | SEH/C++ unwind handler trampoline |
| 00475d70 | Unwind@00475d70 | CRT_unwind_handler_00475d70 | CRT | SEH/C++ unwind handler trampoline |
| 00475d90 | Unwind@00475d90 | CRT_unwind_handler_00475d90 | CRT | SEH/C++ unwind handler trampoline |
| 00475db0 | Unwind@00475db0 | CRT_unwind_handler_00475db0 | CRT | SEH/C++ unwind handler trampoline |
| 00475dd0 | Unwind@00475dd0 | CRT_unwind_handler_00475dd0 | CRT | SEH/C++ unwind handler trampoline |
| 00475df0 | Unwind@00475df0 | CRT_unwind_handler_00475df0 | CRT | SEH/C++ unwind handler trampoline |
| 00475dfb | Unwind@00475dfb | CRT_unwind_handler_00475dfb | CRT | SEH/C++ unwind handler trampoline |
| 00475e10 | Unwind@00475e10 | CRT_unwind_handler_00475e10 | CRT | SEH/C++ unwind handler trampoline |
| 00475e30 | Unwind@00475e30 | CRT_unwind_handler_00475e30 | CRT | SEH/C++ unwind handler trampoline |
| 00475e50 | Unwind@00475e50 | CRT_unwind_handler_00475e50 | CRT | SEH/C++ unwind handler trampoline |
| 00475e5b | Unwind@00475e5b | CRT_unwind_handler_00475e5b | CRT | SEH/C++ unwind handler trampoline |
| 00475e66 | Unwind@00475e66 | CRT_unwind_handler_00475e66 | CRT | SEH/C++ unwind handler trampoline |
| 00475e71 | Unwind@00475e71 | CRT_unwind_handler_00475e71 | CRT | SEH/C++ unwind handler trampoline |
| 00475e7c | Unwind@00475e7c | CRT_unwind_handler_00475e7c | CRT | SEH/C++ unwind handler trampoline |
| 00475e87 | Unwind@00475e87 | CRT_unwind_handler_00475e87 | CRT | SEH/C++ unwind handler trampoline |
| 00475e92 | Unwind@00475e92 | CRT_unwind_handler_00475e92 | CRT | SEH/C++ unwind handler trampoline |
| 00475eb0 | Unwind@00475eb0 | CRT_unwind_handler_00475eb0 | CRT | SEH/C++ unwind handler trampoline |
| 00475ed0 | Unwind@00475ed0 | CRT_unwind_handler_00475ed0 | CRT | SEH/C++ unwind handler trampoline |
| 00475ef0 | Unwind@00475ef0 | CRT_unwind_handler_00475ef0 | CRT | SEH/C++ unwind handler trampoline |
| 00475f10 | Unwind@00475f10 | CRT_unwind_handler_00475f10 | CRT | SEH/C++ unwind handler trampoline |
| 00475f1e | Unwind@00475f1e | CRT_unwind_handler_00475f1e | CRT | SEH/C++ unwind handler trampoline |
| 00475f2c | Unwind@00475f2c | CRT_unwind_handler_00475f2c | CRT | SEH/C++ unwind handler trampoline |
| 00475f3a | Unwind@00475f3a | CRT_unwind_handler_00475f3a | CRT | SEH/C++ unwind handler trampoline |
| 00475f60 | Unwind@00475f60 | CRT_unwind_handler_00475f60 | CRT | SEH/C++ unwind handler trampoline |
| 00475f80 | Unwind@00475f80 | CRT_unwind_handler_00475f80 | CRT | SEH/C++ unwind handler trampoline |
| 00475fa0 | Unwind@00475fa0 | CRT_unwind_handler_00475fa0 | CRT | SEH/C++ unwind handler trampoline |
| 00475fab | Unwind@00475fab | CRT_unwind_handler_00475fab | CRT | SEH/C++ unwind handler trampoline |
| 00475fb6 | Unwind@00475fb6 | CRT_unwind_handler_00475fb6 | CRT | SEH/C++ unwind handler trampoline |
| 00475fc1 | Unwind@00475fc1 | CRT_unwind_handler_00475fc1 | CRT | SEH/C++ unwind handler trampoline |
| 00475fcc | Unwind@00475fcc | CRT_unwind_handler_00475fcc | CRT | SEH/C++ unwind handler trampoline |
| 00475fd7 | Unwind@00475fd7 | CRT_unwind_handler_00475fd7 | CRT | SEH/C++ unwind handler trampoline |
| 00475fe2 | Unwind@00475fe2 | CRT_unwind_handler_00475fe2 | CRT | SEH/C++ unwind handler trampoline |
| 00475fed | Unwind@00475fed | CRT_unwind_handler_00475fed | CRT | SEH/C++ unwind handler trampoline |
| 00475ff8 | Unwind@00475ff8 | CRT_unwind_handler_00475ff8 | CRT | SEH/C++ unwind handler trampoline |
| 00476003 | Unwind@00476003 | CRT_unwind_handler_00476003 | CRT | SEH/C++ unwind handler trampoline |
| 0047600e | Unwind@0047600e | CRT_unwind_handler_0047600e | CRT | SEH/C++ unwind handler trampoline |
| 00476019 | Unwind@00476019 | CRT_unwind_handler_00476019 | CRT | SEH/C++ unwind handler trampoline |
| 00476024 | Unwind@00476024 | CRT_unwind_handler_00476024 | CRT | SEH/C++ unwind handler trampoline |
| 0047602f | Unwind@0047602f | CRT_unwind_handler_0047602f | CRT | SEH/C++ unwind handler trampoline |
| 00476050 | Unwind@00476050 | CRT_unwind_handler_00476050 | CRT | SEH/C++ unwind handler trampoline |
| 00476070 | Unwind@00476070 | CRT_unwind_handler_00476070 | CRT | SEH/C++ unwind handler trampoline |
| 00476090 | Unwind@00476090 | CRT_unwind_handler_00476090 | CRT | SEH/C++ unwind handler trampoline |
| 0047609b | Unwind@0047609b | CRT_unwind_handler_0047609b | CRT | SEH/C++ unwind handler trampoline |
| 004760b0 | Unwind@004760b0 | CRT_unwind_handler_004760b0 | CRT | SEH/C++ unwind handler trampoline |
| 004760b8 | Unwind@004760b8 | CRT_unwind_handler_004760b8 | CRT | SEH/C++ unwind handler trampoline |
| 004760d0 | Unwind@004760d0 | CRT_unwind_handler_004760d0 | CRT | SEH/C++ unwind handler trampoline |
| 004760db | Unwind@004760db | CRT_unwind_handler_004760db | CRT | SEH/C++ unwind handler trampoline |
| 00476100 | Unwind@00476100 | CRT_unwind_handler_00476100 | CRT | SEH/C++ unwind handler trampoline |
| 00476120 | Unwind@00476120 | CRT_unwind_handler_00476120 | CRT | SEH/C++ unwind handler trampoline |
| 00476140 | Unwind@00476140 | CRT_unwind_handler_00476140 | CRT | SEH/C++ unwind handler trampoline |
| 00476148 | Unwind@00476148 | CRT_unwind_handler_00476148 | CRT | SEH/C++ unwind handler trampoline |
| 00476156 | Unwind@00476156 | CRT_unwind_handler_00476156 | CRT | SEH/C++ unwind handler trampoline |
| 00476170 | Unwind@00476170 | CRT_unwind_handler_00476170 | CRT | SEH/C++ unwind handler trampoline |
| 00476178 | Unwind@00476178 | CRT_unwind_handler_00476178 | CRT | SEH/C++ unwind handler trampoline |
| 00476186 | Unwind@00476186 | CRT_unwind_handler_00476186 | CRT | SEH/C++ unwind handler trampoline |
| 00476194 | Unwind@00476194 | CRT_unwind_handler_00476194 | CRT | SEH/C++ unwind handler trampoline |
| 004761b0 | Unwind@004761b0 | CRT_unwind_handler_004761b0 | CRT | SEH/C++ unwind handler trampoline |
| 004761d0 | Unwind@004761d0 | CRT_unwind_handler_004761d0 | CRT | SEH/C++ unwind handler trampoline |
| 004761f0 | Unwind@004761f0 | CRT_unwind_handler_004761f0 | CRT | SEH/C++ unwind handler trampoline |
| 00476210 | Unwind@00476210 | CRT_unwind_handler_00476210 | CRT | SEH/C++ unwind handler trampoline |
| 00476230 | Unwind@00476230 | CRT_unwind_handler_00476230 | CRT | SEH/C++ unwind handler trampoline |
| 0047623b | Unwind@0047623b | CRT_unwind_handler_0047623b | CRT | SEH/C++ unwind handler trampoline |
| 00476260 | Unwind@00476260 | CRT_unwind_handler_00476260 | CRT | SEH/C++ unwind handler trampoline |
| 0047626b | Unwind@0047626b | CRT_unwind_handler_0047626b | CRT | SEH/C++ unwind handler trampoline |
| 00476280 | Unwind@00476280 | CRT_unwind_handler_00476280 | CRT | SEH/C++ unwind handler trampoline |
| 004762a0 | Unwind@004762a0 | CRT_unwind_handler_004762a0 | CRT | SEH/C++ unwind handler trampoline |
| 004762ab | Unwind@004762ab | CRT_unwind_handler_004762ab | CRT | SEH/C++ unwind handler trampoline |
| 004762c0 | Unwind@004762c0 | CRT_unwind_handler_004762c0 | CRT | SEH/C++ unwind handler trampoline |
| 004762e0 | Unwind@004762e0 | CRT_unwind_handler_004762e0 | CRT | SEH/C++ unwind handler trampoline |
| 00476300 | Unwind@00476300 | CRT_unwind_handler_00476300 | CRT | SEH/C++ unwind handler trampoline |
| 00476320 | Unwind@00476320 | CRT_unwind_handler_00476320 | CRT | SEH/C++ unwind handler trampoline |
| 0047632b | Unwind@0047632b | CRT_unwind_handler_0047632b | CRT | SEH/C++ unwind handler trampoline |
| 00476350 | Unwind@00476350 | CRT_unwind_handler_00476350 | CRT | SEH/C++ unwind handler trampoline |
| 0047635b | Unwind@0047635b | CRT_unwind_handler_0047635b | CRT | SEH/C++ unwind handler trampoline |
| 00476366 | Unwind@00476366 | CRT_unwind_handler_00476366 | CRT | SEH/C++ unwind handler trampoline |
| 00476371 | Unwind@00476371 | CRT_unwind_handler_00476371 | CRT | SEH/C++ unwind handler trampoline |
| 0047637c | Unwind@0047637c | CRT_unwind_handler_0047637c | CRT | SEH/C++ unwind handler trampoline |
| 00476387 | Unwind@00476387 | CRT_unwind_handler_00476387 | CRT | SEH/C++ unwind handler trampoline |
| 00476392 | Unwind@00476392 | CRT_unwind_handler_00476392 | CRT | SEH/C++ unwind handler trampoline |
| 0047639d | Unwind@0047639d | CRT_unwind_handler_0047639d | CRT | SEH/C++ unwind handler trampoline |
| 004763a8 | Unwind@004763a8 | CRT_unwind_handler_004763a8 | CRT | SEH/C++ unwind handler trampoline |
| 004763c0 | Unwind@004763c0 | CRT_unwind_handler_004763c0 | CRT | SEH/C++ unwind handler trampoline |
| 004763e0 | Unwind@004763e0 | CRT_unwind_handler_004763e0 | CRT | SEH/C++ unwind handler trampoline |
| 00476400 | Unwind@00476400 | CRT_unwind_handler_00476400 | CRT | SEH/C++ unwind handler trampoline |
| 00476420 | Unwind@00476420 | CRT_unwind_handler_00476420 | CRT | SEH/C++ unwind handler trampoline |
| 0047642b | Unwind@0047642b | CRT_unwind_handler_0047642b | CRT | SEH/C++ unwind handler trampoline |
| 00476436 | Unwind@00476436 | CRT_unwind_handler_00476436 | CRT | SEH/C++ unwind handler trampoline |
| 00476441 | Unwind@00476441 | CRT_unwind_handler_00476441 | CRT | SEH/C++ unwind handler trampoline |
| 00476460 | Unwind@00476460 | CRT_unwind_handler_00476460 | CRT | SEH/C++ unwind handler trampoline |
| 0047646b | Unwind@0047646b | CRT_unwind_handler_0047646b | CRT | SEH/C++ unwind handler trampoline |
| 00476480 | Unwind@00476480 | CRT_unwind_handler_00476480 | CRT | SEH/C++ unwind handler trampoline |
| 004764a0 | Unwind@004764a0 | CRT_unwind_handler_004764a0 | CRT | SEH/C++ unwind handler trampoline |
| 004764c0 | Unwind@004764c0 | CRT_unwind_handler_004764c0 | CRT | SEH/C++ unwind handler trampoline |
| 004764c8 | Unwind@004764c8 | CRT_unwind_handler_004764c8 | CRT | SEH/C++ unwind handler trampoline |
| 004764d6 | Unwind@004764d6 | CRT_unwind_handler_004764d6 | CRT | SEH/C++ unwind handler trampoline |
| 004764f1 | Unwind@004764f1 | CRT_unwind_handler_004764f1 | CRT | SEH/C++ unwind handler trampoline |
| 004764ff | Unwind@004764ff | CRT_unwind_handler_004764ff | CRT | SEH/C++ unwind handler trampoline |
| 00476520 | Unwind@00476520 | CRT_unwind_handler_00476520 | CRT | SEH/C++ unwind handler trampoline |
| 00476528 | Unwind@00476528 | CRT_unwind_handler_00476528 | CRT | SEH/C++ unwind handler trampoline |
| 00476536 | Unwind@00476536 | CRT_unwind_handler_00476536 | CRT | SEH/C++ unwind handler trampoline |
| 00476551 | Unwind@00476551 | CRT_unwind_handler_00476551 | CRT | SEH/C++ unwind handler trampoline |
| 0047655f | Unwind@0047655f | CRT_unwind_handler_0047655f | CRT | SEH/C++ unwind handler trampoline |
| 0047656d | Unwind@0047656d | CRT_unwind_handler_0047656d | CRT | SEH/C++ unwind handler trampoline |
| 00476590 | Unwind@00476590 | CRT_unwind_handler_00476590 | CRT | SEH/C++ unwind handler trampoline |
| 004765b0 | Unwind@004765b0 | CRT_unwind_handler_004765b0 | CRT | SEH/C++ unwind handler trampoline |
| 004765d0 | Unwind@004765d0 | CRT_unwind_handler_004765d0 | CRT | SEH/C++ unwind handler trampoline |
| 004765f0 | Unwind@004765f0 | CRT_unwind_handler_004765f0 | CRT | SEH/C++ unwind handler trampoline |
| 00476610 | Unwind@00476610 | CRT_unwind_handler_00476610 | CRT | SEH/C++ unwind handler trampoline |
| 00476630 | Unwind@00476630 | CRT_unwind_handler_00476630 | CRT | SEH/C++ unwind handler trampoline |
| 00476650 | Unwind@00476650 | CRT_unwind_handler_00476650 | CRT | SEH/C++ unwind handler trampoline |
| 0047666a | Unwind@0047666a | CRT_unwind_handler_0047666a | CRT | SEH/C++ unwind handler trampoline |
| 00476680 | Unwind@00476680 | CRT_unwind_handler_00476680 | CRT | SEH/C++ unwind handler trampoline |
| 0047669a | Unwind@0047669a | CRT_unwind_handler_0047669a | CRT | SEH/C++ unwind handler trampoline |
| 004766a5 | Unwind@004766a5 | CRT_unwind_handler_004766a5 | CRT | SEH/C++ unwind handler trampoline |
| 004766c0 | Unwind@004766c0 | CRT_unwind_handler_004766c0 | CRT | SEH/C++ unwind handler trampoline |
| 004766e0 | Unwind@004766e0 | CRT_unwind_handler_004766e0 | CRT | SEH/C++ unwind handler trampoline |
| 00476700 | Unwind@00476700 | CRT_unwind_handler_00476700 | CRT | SEH/C++ unwind handler trampoline |
| 00476720 | Unwind@00476720 | CRT_unwind_handler_00476720 | CRT | SEH/C++ unwind handler trampoline |
| 0047673a | Unwind@0047673a | CRT_unwind_handler_0047673a | CRT | SEH/C++ unwind handler trampoline |
| 00476750 | Unwind@00476750 | CRT_unwind_handler_00476750 | CRT | SEH/C++ unwind handler trampoline |
| 00476780 | Unwind@00476780 | CRT_unwind_handler_00476780 | CRT | SEH/C++ unwind handler trampoline |
| 0047679a | Unwind@0047679a | CRT_unwind_handler_0047679a | CRT | SEH/C++ unwind handler trampoline |
| 004767a5 | Unwind@004767a5 | CRT_unwind_handler_004767a5 | CRT | SEH/C++ unwind handler trampoline |
| 004767c0 | Unwind@004767c0 | CRT_unwind_handler_004767c0 | CRT | SEH/C++ unwind handler trampoline |
