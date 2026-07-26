# Lego Loco CRT Function Map

This file documents ALL CRT functions statically linked into loco.exe
from the MSVC 6.0 runtime library (0x460000–0x4767D6 range).

**This replaces lines 1182-1788 of the main FUNCTION_MAP.md.**
The main FUNCTION_MAP.md's CRT section should be updated with these entries.

## Legend

| Column | Meaning |
|---|---|
| Address | Virtual address in loco.exe |
| Ghidra Name | Current name in the Ghidra database |
| CRT Name | Standard CRT function name |
| Source | MSVC 6.0 CRT object file (where identified) |
| Notes | Details, calling convention, key callers |

---

## C++ Exception Handling (0x466050–0x46613E)

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x466050 | `__global_unwind2` | `__global_unwind2` | _WINXFLTR.OBJ | SEH global unwind — unwinds frames to target |
| 0x466092 | `__local_unwind2` | `__local_unwind2` | _WINXFLTR.OBJ | SEH local unwind with destructor calls |
| 0x4660FA | `__abnormal_termination` | `__abnormal_termination` | _WINXFLTR.OBJ | Returns non-zero during SEH unwind |
| 0x46611D | `__NLG_Notify1` | `__NLG_Notify1` | _WINXFLTR.OBJ | Stores ESP for setjmp/longjmp tracking |
| 0x466126 | `CRT_0x466126` | `__NLG_Notify2` | _WINXFLTR.OBJ | Stores EAX/EBP/retaddr for non-local-goto |
| 0x466140 | `CRT_srand` | `srand` | _RAND.OBJ | Seed PRNG (13-byte thunk) |
| 0x466150 | `CRT_rand` | `rand` | _RAND.OBJ | Pseudo-random int |
| 0x466180 | `__fpmath` | `__fpmath` | _FPREM.OBJ | FPU init — sets extended precision |
| 0x4661A0 | `CRT_0x4661A0` | *(alignment)* | — | 1-byte filler, never called |
| 0x4661B0 | `CRT_0x4661B0` | `___lc_locale_init` | _LOCALE.OBJ | Initializes locale strcoll/wcscoll ptrs |

## String Functions (0x4661F0+)

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x4661F0 | `_strncpy` | `strncpy` | _STRNCPY.OBJ | Copy with null-padding (minimal) |
| 0x4662F0 | `CRT_0x4662F0` | `_atoi_l` | _ATOI.OBJ | Locale-aware atoi |
| 0x466390 | `CRT_atoi` | `atoi` | _ATOI.OBJ | Standard atoi (14-byte thunk) |
| 0x4663A0 | `CRT_strtok` | `strtok` | _STRTOK.OBJ | Tokenizer with internal state |
| 0x466EA0 | `CRT_strncpy` | `strncpy` | _STRNCPY.OBJ | Full implementation (664 bytes) |
| 0x467330 | `CRT_strncmp` | `strncmp` | _STRNCMP.OBJ | Compare up to n chars |
| 0x4676D0 | `_strpbrk` | `strpbrk` | _STRPBRK.OBJ | Find first char from charset |
| 0x467710 | `CRT_0x467710` | `toupper` | _TOLOWER.OBJ | Locale-aware uppercase |
| 0x4677A0 | `CRT_strncpy_s` | `strncpy_s` | _STRNCPY_S.OBJ | Secure bounded copy |
| 0x4678A0 | `CRT_strncat_s` | `strncat_s` | _STRNCAT_S.OBJ | Secure bounded append |
| 0x467D30 | `_strncat` | `strncat` | _STRNCAT.OBJ | Append at most n chars |
| 0x467E60 | `_strrchr` | `strrchr` | _STRRCHR.OBJ | Find last occurrence of char |
| 0x467EA0 | `CRT_itoa` | `itoa` | _ITOA.OBJ | Integer to ASCII |
| 0x467EE0 | `CRT_ltoa` | `ltoa` | _ITOA.OBJ | Long to ASCII |
| 0x467F50 | `CRT_ultoa` | `ultoa` | _ITOA.OBJ | Unsigned long to ASCII |
| 0x468060 | `_strstr` | `strstr` | _STRSTR.OBJ | Find substring |
| 0x468380 | `CRT_wcstok` | `wcstok` | _WCSTOK.OBJ | Wide tokenizer |
| 0x468510 | `CRT_wcsdup` | `wcsdup` | _WCSDUP.OBJ | Wide string allocate+copy |
| 0x468650 | `CRT_wcsnlen` | `wcsnlen` | _WCSNLEN.OBJ | Wide string bounded length |
| 0x468D70 | `CRT_wcstol` | `wcstol` | _WCSTOL.OBJ | Wide string to long |
| 0x469000 | `CRT_wcstoul` | `wcstoul` | _WCTOUL.OBJ | Wide string to unsigned long |
| 0x469330 | `CRT_wcstod` | `wcstod` | _WCSTOD.OBJ | Wide string to double |
| 0x469E60 | `CRT_wcslen` | `wcslen` | _WCSLEN.OBJ | Wide string length |
| 0x46A120 | `CRT_wcscat` | `wcscat` | _WCSCAT.OBJ | Wide string concatenation |
| 0x46A350 | `CRT_wcsncat` | `wcsncat` | _WCSNCAT.OBJ | Wide string n-concatenation |
| 0x46A4E0 | `CRT_wcscpy` | `wcscpy` | _WCSCPY.OBJ | Wide string copy |
| 0x46A6F0 | `CRT_wctomb` | `wctomb` | _WCTOMB.OBJ | Wide char to multi-byte |
| 0x46A7F0 | `CRT_wcscoll` | `wcscoll` | _WCSCOLL.OBJ | Locale-aware wide compare |
| 0x46A8F0 | `CRT_wcscspn` | `wcscspn` | _WCSCSPN.OBJ | Wide complement span |
| 0x46A9A0 | `CRT_strcoll` | `strcoll` | _STRCOLL.OBJ | Locale-aware string compare |
| 0x46AB60 | `CRT_strxfrm` | `strxfrm` | _STRXFRM.OBJ | Transform string for locale compare |
| 0x46AD30 | `CRT_wcsncpy` | `wcsncpy` | _WCSNCPY.OBJ | Wide string n-copy |
| 0x46AEA0 | `CRT_wcsspn` | `wcsspn` | _WCSSPN.OBJ | Wide string span |
| 0x46AF60 | `CRT_wcspbrk` | `wcspbrk` | _WCSPBRK.OBJ | Wide string pointer to break |
| 0x46B090 | `CRT_0x46B090` | `_strshift` | — | Memory move with strlen |
| 0x46B350 | `CRT_0x46B350` | `_mbsrchr` | _MBS.OBJ | Multi-byte reverse char search |
| 0x46B8F0 | `CRT_wcscmp` | `wcscmp` | _WCSCMP.OBJ | Wide string compare |
| 0x46C520 | `CRT_wcsncmp` | `wcsncmp` | _WCSNCMP.OBJ | Wide string n-compare |
| 0x46C690 | `CRT_wcsxfrm` | `wcsxfrm` | _STRXFRM.OBJ | Wide string locale transform |
| 0x46C6F0 | `CRT_wmemset` | `wmemset` | _WMEMSET.OBJ | Wide memory set |
| 0x46C880 | `CRT_wcschr` | `wcschr` | _WCSCHR.OBJ | Wide char find first |
| 0x46CAC0 | `CRT_wcsrchr` | `wcsrchr` | _WCSRCHR.OBJ | Wide char find last |
| 0x46CC40 | `CRT_wmemcpy` | `wmemcpy` | _WMEMCPY.OBJ | Wide memory copy |
| 0x471480 | `CRT_wcsstr` | `wcsstr` | _WCSSTR.OBJ | Find wide substring |
| 0x472B60 | `_strcspn` | `strcspn` | _STRCSPN.OBJ | Complement span |
| 0x472BA0 | `_strncmp` | `strncmp` | _STRNCMP.OBJ | String n-compare (small) |
| 0x472C90 | `CRT_mblen` | `mblen` | _MBLEN.OBJ | Multi-byte char length |
| 0x472FE0 | `CRT_mbtowc_s` | `mbtowc_s` | _MBTOWC_S.OBJ | Secure multi-byte to wide |
| 0x473CC0 | `CRT_wctomb_s` | `wctomb_s` | _WCTOMB_S.OBJ | Secure wide to multi-byte |
| 0x473E70 | `CRT_mbstowcs_s` | `mbstowcs_s` | _MSTOWCS_S.OBJ | Secure multi-byte to wide string |
| 0x473F80 | `CRT_wcstombs_s` | `wcstombs_s` | _WCSTOMBS_S.OBJ | Secure wide to multi-byte string |
| 0x4742D0 | `CRT_strtok_s` | `strtok_s` | _STRTOK_S.OBJ | Thread-safe tokenizer |
| 0x4745D0 | `CRT_wcstok_s` | `wcstok_s` | _WCSTOK_S.OBJ | Thread-safe wide tokenizer |
| 0x4748D0 | `CRT_strerror` | `strerror` | _STRERROR.OBJ | Error message string |
| 0x474A00 | `CRT_strerror_s` | `strerror_s` | _STRERROR_S.OBJ | Secure error message |
| 0x474B40 | `CRT_strerrorlen` | `strerrorlen` | _STRERROR.OBJ | Error message length |
| 0x474C70 | `CRT_strupr` | `strupr` | _STRUPR.OBJ | String to uppercase |

## C++ Exception Handling (0x46A200+)

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x46A200 | `CRT_0x46A200` | `_CxxFrameHandler` | _WINXFRM.OBJ | Walks catch table, invokes destructors |
| 0x46A2C0 | `CRT_0x46A2C0` | `__CxxExceptionFilter` | _WINXFLTR.OBJ | Examines exception, routes to handler |
| 0x46A448 | `CRT_0x46A448` | `_CxxFrameHandler epilogue` | _WINXFRM.OBJ | Saves exception info, checks 0xE06D7363 |
| 0x46A770 | `CRT_0x46A770` | `_CxxFrameHandler addr` | _WINXFRM.OBJ | Resolves relative addr from EH table |
| 0x46A7A0 | `__CallSettingFrame@12` | `__CallSettingFrame@12` | _WINXFRM.OBJ | Invokes catch handler/destructor |
| 0x46AA30 | `CRT_0x46AA30` | `_terminate` | _TERM.OBJ | C++ terminate handler |
| 0x46AA9E | `CRT_0x46AA9E` | `_abort_helper` | _TERM.OBJ | Calls abort from terminate |
| 0x46CD10 | `CRT_0x46CD10` | `_CxxExceptionFilter type check` | _WINXFLTR.OBJ | Detects 0xE06D7363 C++ exceptions |
| 0x46CE65 | `CRT_0x46CE65` | `_local_unwind2 helper` | _WINXFLTR.OBJ | Calls __local_unwind2 from EH node |
| 0x465E40 | `CRT_0x465E40` | `__CxxExceptionFilter` | _WINXFLTR.OBJ | SEH filter for C++ exceptions |
| 0x465A30 | `CRT_except_handler` | `_except_handler` | _WINXFLTR.OBJ | Top-level SEH filter |
| 0x465AC0 | `CRT_seh_filter` | `_seh_filter` | _WINXFLTR.OBJ | Returns EXCEPTION_EXECUTE_HANDLER |
| 0x465D40 | `CRT_purecall` | `_purecall` | _WINXPHTR.OBJ | Pure virtual function call handler |

## Memory Management (0x465C+)

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x465CD0 | `GLOBAL_free` | `GLOBAL_free` | *(game)* | Game allocator: calls CRT_free |
| 0x465CE0 | `operator_new` | `operator new(size_t)` | _NEW.OBJ | C++ new → malloc |
| 0x465CF0 | `CRT_operator_new` | `operator new (nothrow)` | _NEW.OBJ | C++ new with nothrow |
| 0x465D30 | `CRT_operator_delete` | `operator delete(void*)` | _NEW.OBJ | C++ delete → free |
| 0x465DE0 | `GLOBAL_alloc` | `GLOBAL_alloc` | *(game)* | Game allocator: calls CRT_malloc |
| 0x466C70 | `CRT_free` | `free` | _FREE.OBJ | HeapFree on CRT private heap |
| 0x466DE0 | `CRT_malloc` | `malloc` | _MALLOC.OBJ | HeapAlloc or small-block cache |
| 0x4671E0 | `CRT_memset_pattern` | `_memset_pattern` | _MEMSET.OBJ | Fill with 4-byte pattern |
| 0x467280 | `CRT_free_pattern` | `_free_pattern` | _MEMSET.OBJ | Free pattern-allocated mem |
| 0x4673C0 | `CRT_malloc_zero` | `_malloc_zero` | _MALLOC.OBJ | Allocate + zero-fill |
| 0x4673E0 | `CRT_0x4673E0` | `_malloc_base` | _MALLOC.OBJ | Core malloc with retry via _callnewh |
| 0x467430 | `CRT_0x467430` | `_nh_malloc` | _NHMALLOC.OBJ | Small-block allocator |
| 0x46C4E0 | `CRT_0x46C4E0` | `_heap_init` | _HEAP.OBJ | HeapCreate for CRT private heap |
| 0x46C7C0 | `CRT_0x46C7C0` | `_heap_lookup_region` | _HEAP.OBJ | Find small-block region for ptr |
| 0x46C820 | `CRT_0x46C820` | `_heap_free_block` | _HEAP.OBJ | Decrement refcount, coalesce |
| 0x46CE80 | `CRT_0x46CE80` | `_callnewh` | _NEW.OBJ | Call C++ new handler |

## Memory Helpers / Pointer Validation

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x4706F0 | `CRT_0x4706F0` | `_isvalidreadptr` | — | IsBadReadPtr wrapper |
| 0x470710 | `CRT_0x470710` | `_isvalidwriteptr` | — | IsBadWritePtr wrapper |
| 0x470730 | `CRT_0x470730` | `_isvalidcodeptr` | — | IsBadCodePtr wrapper |

## Errno / Error Handling

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x467FD0 | `CRT_errno` | `_errno` | _ERRNO.OBJ | Thread-local errno pointer |
| 0x467FE0 | `CRT_0x467FE0` | `__doserrno` | _ERRNO.OBJ | Thread-local DOS error |

## Random Number Generation

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x466140 | `CRT_srand` | `srand` | _RAND.OBJ | Seed PRNG (13-byte thunk) |
| 0x466150 | `CRT_rand` | `rand` | _RAND.OBJ | Get next pseudo-random int |
| 0x469560 | `CRT_srand` | `srand` (full) | _RAND.OBJ | Full srand with lock (641 bytes) |
| 0x469540 | `CRT_0x469540` | `rand wrapper` | _RAND.OBJ | Calls srand(0) |
| 0x4697F0 | `CRT_0x4697F0` | `srand wrapper` | _RAND.OBJ | Calls srand(seed, 1) |

## Process / Startup / Exit

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x4656B0 | `CRT__cexit` | `_cexit` | _EXIT.OBJ | Flush streams, call atexit handlers |
| 0x4656F0 | `CRT_exit_handler` | `_exit_handler` | _EXIT.OBJ | Call atexit callback table |
| 0x4684A0 | `CRT_0x4684A0` | `_CRT_INIT` | _INIT.OBJ | CRT startup — calls __fpmath + _initterm |
| 0x4684F0 | `__exit` | `__exit` | _EXIT.OBJ | ExitProcess |
| 0x4689E0 | `entry` | `_mainCRTStartup` | _CRT0.OBJ | CRT entry point |
| 0x468B90 | `__amsg_exit` | `__amsg_exit` | _AMSG.OBJ | Display runtime error + terminate |
| 0x468BC0 | `CRT_0x468BC0` | `abort` | _ABORT.OBJ | Flush stdout + ExitProcess(0xFF) |
| 0x465DA0 | `CRT_amsg_exit` | `_amsg_exit` | _AMSG.OBJ | Runtime error message |
| 0x466CE0 | `CRT_exit` | `exit` | _EXIT.OBJ | _cexit + ExitProcess |
| 0x46AA17 | `_abort` | `abort` (thunk) | _ABORT.OBJ | JMP to 0x468BC0 |
| 0x470750 | `_abort` | `abort` (alt) | _ABORT.OBJ | Alt abort with different message |

## Time Functions

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x466490 | `CRT_time` | `time` | _TIME.OBJ | GetSystemTime + math → epoch |
| 0x4664C0 | `CRT_difftime` | `difftime` | _TIME.OBJ | Double difference in seconds |
| 0x4674E0 | `CRT_localtime` | `localtime` | _TIME.OBJ | time_t → tm with DST (486 bytes) |
| 0x467CA0 | `CRT_0x467CA0` | `_filetime_to_time_t` | _TIME.OBJ | FILETIME → seconds since 1970 |
| 0x46ACB0 | `CRT_0x46ACB0` | `asctime helper` | _TIME.OBJ | Format time_t/tm into string |
| 0x46DFE0 | `__isindst` | `__isindst` | _TIME.OBJ | DST check for localtime |
| 0x471CF0 | `CRT_strftime` | `strftime` (v1) | _STRFTIME.OBJ | Format time string |
| 0x4730E0 | `CRT_strftime` | `strftime` (v2) | _STRFTIME.OBJ | Full strftime (1632 bytes) |
| 0x472070 | `CRT_wcsftime` | `wcsftime` (v1) | _STRFTIME.OBJ | Wide time formatting |
| 0x473870 | `CRT_wcsftime` | `wcsftime` (v2) | _STRFTIME.OBJ | Full wcsftime (898 bytes) |

## Math Functions

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x464EF0 | `CRT_fmod` | `fmod` | XMATH.OBJ | Floating-point remainder |
| 0x464F70 | `CRT_fabs` | `fabs` | XMATH.OBJ | Absolute value |
| 0x465010 | `CRT_ceil` | `ceil` | XMATH.OBJ | Smallest integer >= x |
| 0x465090 | `CRT_floor` | `floor` | XMATH.OBJ | Largest integer <= x |
| 0x465180 | `CRT_log10` | `log10` | XMATH.OBJ | Base-10 logarithm (thunk to log) |
| 0x4651A0 | `CRT_log` | `log` | XMATH.OBJ | Natural logarithm |
| 0x465200 | `CRT_cos` | `cos` | XMATH.OBJ | Cosine |
| 0x465250 | `CRT_sin` | `sin` | XMATH.OBJ | Sine |
| 0x4652A0 | `CRT_tan` | `tan` | XMATH.OBJ | Tangent |
| 0x4652D0 | `CRT_exp` | `exp` | XMATH.OBJ | Exponential |
| 0x4654C0 | `CRT_atan` | `atan` | XMATH.OBJ | Arctangent |
| 0x465560 | `CRT_acos` | `acos` | XMATH.OBJ | Arccosine |
| 0x465890 | `CRT_sqrtf` | `sqrtf` | XMATH.OBJ | Float square root |
| 0x465E70 | `CRT_powf` | `powf` | XMATH.OBJ | Float exponentiation |
| 0x465960 | `CRT_ftolf` | `_ftolf` | _FTOL.OBJ | Float-to-long truncation |
| 0x465AD0 | `CRT_ftol` | `_ftol` | _FTOL.OBJ | Double-to-long truncation (511 bytes) |
| 0x466D30 | `__ftol` | `__ftol` | _FTOL.OBJ | Compiler helper: double→long |

## Compiler Intrinsics

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x46B160 | `__allmul` | `__allmul` | _ALLMUL.OBJ | 64-bit signed multiply |
| 0x46ED90 | `__aulldiv` | `__aulldiv` | _AULLDIV.OBJ | 64-bit unsigned division |
| 0x46EE00 | `__aullrem` | `__aullrem` | _AULLREM.OBJ | 64-bit unsigned remainder |
| 0x471850 | `__allshl` | `__allshl` | _ALLSHL.OBJ | 64-bit left shift |

## FPU Control

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x46AAC0 | `__setdefaultprecision` | `__setdefaultprecision` | _FPCTRL.OBJ | Set FPU to 53-bit precision |
| 0x46AAE0 | `CRT_0x46AAE0` | `_statusfp` | _FPCTRL.OBJ | Check FPU status word sign bit |
| 0x46AB30 | `CRT_0x46AB30` | `_is_processor_feature_present` | _CPUID.OBJ | SSE support detection |
| 0x470770 | `CRT_0x470770` | `_controlfp` | _CONTROLFP.OBJ | Set/query FPU control word |
| 0x4707B0 | `CRT_0x4707B0` | `_clearfp` | _CONTROLFP.OBJ | Clear FPU exception flags |
| 0x4707D0 | `CRT_0x4707D0` | `_FPU_convert_cw` | _CONTROLFP.OBJ | x87 CW ↔ _controlfp conversion |
| 0x470870 | `CRT_0x470870` | `_controlfp stub` | _CONTROLFP.OBJ | Empty stub (debug/non-debug) |

## RTTI

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x466C20 | `CRT_0x466C20` | `type_info::~type_info` | _TINFO.OBJ | RTTI destructor |
| 0x466C50 | `CRT_0x466C50` | `type_info scalar dtor` | _TINFO.OBJ | RTTI scalar deleting dtor |

## Threading

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x468870 | `CRT_0x468870` | `_beginthreadex` | _THREAD.OBJ | Create thread with CRT TLS |
| 0x4688F0 | `CRT_0x4688F0` | `_threadstart` | _THREAD.OBJ | Thread startup routine |
| 0x4689A0 | `CRT_0x4689A0` | `_endthreadex` | _THREAD.OBJ | Thread cleanup + ExitThread |
| 0x46A850 | `CRT_0x46A850` | `_initptd` | _THREAD.OBJ | Init per-thread data block |
| 0x46A870 | `CRT_0x46A870` | `_getptd` | _THREAD.OBJ | Get/alloc per-thread data |

## Locking / Synchronization

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x46B740 | `CRT_0x46B740` | `_init_locks` | _LOCK.OBJ | Init 4 CRT critical sections |
| 0x46B770 | `CRT_0x46B770` | `_lock` | _LOCK.OBJ | Acquire CRT lock by index |
| 0x46B7F0 | `CRT_0x46B7F0` | `_unlock` | _LOCK.OBJ | Release CRT lock by index |
| 0x46B810 | `CRT_0x46B810` | `_lock_file` | _FILE.OBJ | Lock FILE stream by address |
| 0x46B850 | `CRT_0x46B850` | `_lock_file2` | _FILE.OBJ | Lock FILE by idx+ptr |
| 0x46B880 | `CRT_0x46B880` | `_unlock_file` | _FILE.OBJ | Unlock FILE stream |
| 0x46B8C0 | `CRT_0x46B8C0` | `_unlock_file2` | _FILE.OBJ | Unlock FILE (alt) |
| 0x4685D0 | `CRT_0x4685D0` | `_lock_ungetc` | _LOCK.OBJ | Lock for ungetc/ungetwc (idx 0xD) |
| 0x4685E0 | `CRT_0x4685E0` | `_unlock_ungetc` | _LOCK.OBJ | Unlock for ungetc/ungetwc |
| 0x470460 | `CRT_0x470460` | `_lock_fh` | _LOCK.OBJ | Lock file descriptor CS |
| 0x4704D0 | `CRT_0x4704D0` | `_unlock_fh` | _LOCK.OBJ | Unlock file descriptor CS |

## File I/O (Stream)

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x4657A0 | `CRT_0x4657A0` | `_flsbuf` | _FLSBUF.OBJ | Flush write buffer then write char |
| 0x465810 | `CRT_0x465810` | `_fflush_nolock` | _FILE.OBJ | Flush stream without locking |
| 0x4681D0 | `CRT_0x4681D0` | `_fflush (locked)` | _FILE.OBJ | Locked flush |
| 0x468210 | `CRT_0x468210` | `_fflush_nolock (internal)` | _FILE.OBJ | Internal flush - handles close |
| 0x468280 | `CRT_0x468280` | `_fflush (external)` | _FILE.OBJ | Locked flush, NULL=all streams |
| 0x4682C0 | `CRT_0x4682C0` | `_fflush_nolock (external)` | _FILE.OBJ | External flush nolock |
| 0x468300 | `CRT_0x468300` | `_flush_write_buffer` | _FILE.OBJ | WriteFile buffered data |
| 0x46DC50 | `CRT_fflush` | `fflush` | _FFLUSH.OBJ | Standard fflush |
| 0x46DC70 | `CRT_fputc` | `fputc` | _FPUTC.OBJ | Write character to stream |
| 0x46DD00 | `CRT_fgetc` | `fgetc` | _FGETC.OBJ | Read character from stream |
| 0x46DC20 | `CRT_0x46DC20` | `_filbuf` | _FILBUF.OBJ | Refill input buffer |
| 0x46DCC0 | `CRT_0x46DCC0` | `_init_stdin` | _FILE.OBJ | Lazy stdin buffer init |
| 0x46E010 | `CRT_fseek` | `fseek` | _FSEEK.OBJ | Reposition file pointer |
| 0x46E280 | `CRT_fclose` | `fclose` | _FCLOSE.OBJ | Close FILE stream |
| 0x46E420 | `CRT_fopen` | `fopen` | _FOPEN.OBJ | Open file |
| 0x46E5A0 | `CRT_setvbuf` | `setvbuf` | _STB.OBJ | Set buffering mode |
| 0x46E7D0 | `CRT_tmpfile` | `tmpfile` | _TMPFILE.OBJ | Create temporary file |
| 0x46EA00 | `CRT_tmpnam` | `tmpnam` | _TMPNAM.OBJ | Generate temp filename |
| 0x46EAB0 | `CRT_setbuf` | `setbuf` | _STB.OBJ | Associate buffer with stream |
| 0x46EAF0 | `CRT_perror` | `perror` | _PERROR.OBJ | Print error to stderr |
| 0x46ECE0 | `CRT_fwide` | `fwide` | _FWIDE.OBJ | Set stream orientation |
| 0x46ECD0 | `CRT_0x46ECD0` | `_flushall` | _FFLUSH.OBJ | Flush all streams |
| 0x46F0A0 | `CRT_0x46F0A0` | `_freebuf` | _FILE.OBJ | Free FILE buffer on close |
| 0x470650 | `CRT_0x470650` | `_fcloseall` | _FCLOSE.OBJ | Close all streams on exit |
| 0x46FF30 | `CRT_0x46FF30` | `_flush_console_io` | _CONSOLE.OBJ | Flush console during abort/exit |

## File I/O (Low-level FD)

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x468BF0 | `CRT_0x468BF0` | `_close` | _CLOSE.OBJ | Close file descriptor |
| 0x468C60 | `CRT_0x468C60` | `_close_nolock` | _CLOSE.OBJ | Internal close |
| 0x468CF0 | `CRT_0x468CF0` | `_write` | _WRITE.OBJ | Write to fd |
| 0x468F80 | `CRT_0x468F80` | `_read` | _READ.OBJ | Read from fd |
| 0x469230 | `CRT_0x469230` | `_lseek` | _LSEEK.OBJ | Seek in fd |
| 0x4692B0 | `CRT_0x4692B0` | `_lseek_nolock` | _LSEEK.OBJ | Internal seek |
| 0x470370 | `CRT_0x470370` | `_free_osfhnd` | _CLOSE.OBJ | Clear fd entry |
| 0x470410 | `CRT_0x470410` | `_get_osfhandle` | _OPEN.OBJ | Get OS handle for fd |
| 0x468790 | `CRT_0x468790` | `_fseek (locked)` | _FSEEK.OBJ | Locked fseek via SetFilePointer |
| 0x4687D0 | `CRT_0x4687D0` | `_fseek_nolock` | _FSEEK.OBJ | Internal fseek |

## Path / Drive Helpers

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x466590 | `CRT_mkdir` | `_mkdir` | _MKDIR.OBJ | CreateDirectoryA wrapper |
| 0x4668D0 | `CRT_0x4668D0` | `_isunccpath` | _MKDIR.OBJ | UNC path detection |
| 0x466AB0 | `CRT_0x466AB0` | `_isvaliddrive` | _MKDIR.OBJ | GetDriveTypeA check |
| 0x46B5A0 | `CRT_0x46B5A0` | `_getdrive` | _DRIVE.OBJ | Get current drive letter |
| 0x467A20 | `CRT_FindFirstFile` | `_findfirst` | _FINDFILE.OBJ | FindFirstFileA wrapper |
| 0x467B50 | `CRT_FindNextFile` | `_findnext` | _FINDFILE.OBJ | FindNextFileA wrapper |
| 0x467C70 | `CRT_FindClose` | `_findclose` | _FINDFILE.OBJ | FindClose wrapper |

## Formatted I/O

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x46BA20 | `CRT_fprintf` | `fprintf` | _FPRINTF.OBJ | Formatted to FILE (2172 bytes) |
| 0x46CEA0 | `CRT_printf` | `printf` | _PRINTF.OBJ | Formatted to stdout (3193 bytes) |
| 0x466D60 | `CRT_sprintf_buf` | `vsprintf` | _SPRINTF.OBJ | Format to buffer via fprintf |
| 0x467FF0 | `CRT_0x467FF0` | `_vsnprintf` | _SPRINTF.OBJ | Bounded format to buffer |
| 0x466980 | `CRT_sprintf_s` | `sprintf_s` | _SPRINTF_S.OBJ | Secure sprintf |
| 0x466950 | `CRT_0x466950` | `sprintf_s (locked)` | _SPRINTF.OBJ | Locked sprintf_s |

## Wide Char Formatted I/O

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x46EE80 | `CRT_fwprintf` | `fwprintf` | — | Wide formatted to FILE |
| 0x46F0E0 | `CRT_fwprintf_s` | `fwprintf_s` | — | Secure wide formatted to FILE |
| 0x46F180 | `CRT_fwscanf` | `fwscanf` | — | Wide formatted input from FILE |
| 0x46F350 | `CRT_fwscanf_s` | `fwscanf_s` | — | Secure wide formatted input |
| 0x46F430 | `CRT_swprintf` | `swprintf` | — | Wide formatted to buffer |
| 0x46F520 | `CRT_swscanf` | `swscanf` | — | Wide formatted input from string |
| 0x46F6D0 | `CRT_swprintf_s` | `swprintf_s` | — | Secure wide formatted to buffer |
| 0x46FA30 | `CRT_wprintf` | `wprintf` | — | Wide formatted to stdout |
| 0x46FB20 | `CRT_swscanf_s` | `swscanf_s` | — | Secure wide formatted input |
| 0x46FBC0 | `CRT_wscanf` | `wscanf` | — | Wide formatted input from stdin |

## Wide Char File I/O

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x46FDD0 | `CRT_fgetwc` | `fgetwc` | — | Read wide char from FILE |
| 0x46FF70 | `CRT_fputwc` | `fputwc` | — | Write wide char to FILE |
| 0x470150 | `CRT_ungetwc` | `ungetwc` | — | Push back wide char |
| 0x4702C0 | `CRT_fgetwc_nolock` | `fgetwc_nolock` | — | Wide char read without lock |
| 0x470500 | `CRT_fgetws` | `fgetws` | — | Read wide string from FILE |
| 0x470990 | `CRT_fputws` | `fputws` | — | Write wide string to FILE |
| 0x470B70 | `CRT_getwchar` | `getwchar` | — | Read wide char from stdin |
| 0x470C60 | `CRT_putwchar` | `putwchar` | — | Write wide char to stdout |
| 0x470D20 | `CRT_getwc` | `getwc` | — | Read wide char (macro) |
| 0x4711B0 | `CRT_putwc` | `putwc` | — | Write wide char (macro) |
| 0x471340 | `CRT_getws` | `getws` | — | Read wide string from stdin |
| 0x471750 | `CRT_putws` | `putws` | — | Write wide string to stdout |
| 0x471980 | `CRT_putwchar` | `putwchar` (expanded) | — | putwchar expansion |
| 0x4680E0 | `CRT_0x4680E0` | `_ungetwc buffer` | _UNGETWC.OBJ | Pushback buffer manager |
| 0x4710E0 | `CRT_ungetwc_nolock` | `ungetwc_nolock` | — | ungetwc without lock |

## Locale / Ctype

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x4698A0 | `CRT_isctype` | `isctype` | _CTYPE.OBJ | Generic char classification |
| 0x469C40 | `CRT_tolower` | `tolower` | _TOLOWER.OBJ | Locale-aware lowercase |
| 0x469D90 | `CRT_iswalnum` | `iswalnum` | — | Wide alphanumeric check |
| 0x469810 | `CRT_0x469810` | `isdigit` | _CTYPE.OBJ | Digit check |
| 0x469840 | `CRT_0x469840` | `ispunct` | _CTYPE.OBJ | Punctuation check |
| 0x469870 | `CRT_0x469870` | `isspace` | _CTYPE.OBJ | Whitespace check |
| 0x470900 | `CRT_0x470900` | `tolower` (alt) | _TOLOWER.OBJ | Alt locale-aware lowercase |
| 0x46B0C0 | `CRT_iswctype` | `iswctype` | — | Wide classification |
| 0x46B1A0 | `CRT_towlower` | `towlower` | — | Wide char to lowercase |
| 0x46B3E0 | `CRT_iswspace` | `iswspace` | — | Wide whitespace check |
| 0x46B4D0 | `CRT_iswdigit` | `iswdigit` | — | Wide digit check |
| 0x46B5F0 | `CRT_iswxdigit` | `iswxdigit` | — | Wide hex digit check |
| 0x46B690 | `CRT_iswalpha` | `iswalpha` | — | Wide alphabetic check |
| 0x46F9F0 | `CRT_0x46F9F0` | `__isctype` | _CTYPE.OBJ | Generic isctype with mask |
| 0x46F9D0 | `CRT_0x46F9D0` | `iswdigit helper` | — | iswdigit via __isctype(4) |
| 0x46EA50 | `CRT_0x46EA50` | `___lc_codepage_func` | _LOCALE.OBJ | Locale→codepage mapping |

## Signal Handling

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x470A90 | `CRT_0x470A90` | `_sig_check` | _SIGNAL.OBJ | Check pending signals in mask |
| 0x470B00 | `CRT_0x470B00` | `_sigdeleteset` | _SIGNAL.OBJ | Clear bit in signal mask |
| 0x472EA0 | `CRT_0x472EA0` | `_signal init helper` | _SIGNAL.OBJ | Initialize signal mask |
| 0x472F10 | `CRT_0x472F10` | `_signal range check` | _SIGNAL.OBJ | Validate signal number |
| 0x472F80 | `CRT_0x472F80` | `_signal lookup` | _SIGNAL.OBJ | Lookup signal handler ptr |
| 0x472FB0 | `CRT_0x472FB0` | `_signal set helper` | _SIGNAL.OBJ | Set signal handler |

## Initterm / Startup Helpers

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x465F40 | `CRT_initterm` | `_initterm` | _INIT.OBJ | Call function pointer table |
| 0x465FD0 | `CRT_initterm_e` | `_initterm_e` | _INIT.OBJ | Call table, stop on error |
| 0x4685F0 | `CRT_0x4685F0` | `_initterm helper` | _INIT.OBJ | Iterate table, call non-NULL |

## Debug / Formatting Helpers

| Address | Ghidra Name | CRT Name | Source | Notes |
|---|---|---|---|---|
| 0x467490 | `CRT_0x467490` | `_debug_fmt_helper` | _DBGRPT.OBJ | Debug format with 'I' specifier |
| 0x468440 | `CRT_0x468440` | `scanf buf alloc` | — | Allocate scanf buffer |
| 0x468480 | `CRT_0x468480` | `scanf buf alloc (64B)` | — | Convenience wrapper |
| 0x46C3B0 | `CRT_0x46C3B0` | `_putc_nolock (printf)` | _PUTCHAR.OBJ | Write single char to printf buffer |
| 0x46C400 | `CRT_0x46C400` | `_putchar (repeat)` | _PUTCHAR.OBJ | Write char N times |
| 0x46C440 | `CRT_0x46C440` | `_puts helper` | _PUTCHAR.OBJ | Output string char-by-char |
| 0x46C480 | `CRT_0x46C480` | `va_arg 4-byte` | — | va_arg helper (4-byte) |
| 0x46C4A0 | `CRT_0x46C4A0` | `va_arg 8-byte` | — | va_arg helper (8-byte) |
| 0x46C4C0 | `CRT_0x46C4C0` | `va_arg struct` | — | va_arg helper (struct) |
| 0x46DBE0 | `CRT_0x46DBE0` | `scanf hex digit` | — | Hex digit → numeric value |
| 0x470FB0 | `CRT_0x470FB0` | `fprintf int helper` | _FPRINTF.OBJ | fprintf internal helper |
| 0x471050 | `CRT_0x471050` | `fprintf num helper` | _FPRINTF.OBJ | fprintf numeric formatting |
| 0x46AE30 | `CRT_0x46AE30` | `strftime helper` | _STRFTIME.OBJ | strftime numeric formatting |

## DLL Import Stubs (not CRT)

| Address | Ghidra Name | DLL | Export |
|---|---|---|---|
| 0x474C60 | `RtlUnwind` | KERNEL32.DLL | `RtlUnwind` |

## SEH Unwind Trampolines (0x474DF0–0x4767D6)

The range 0x474DF0–0x4767D6 contains approximately 280 SEH unwind
trampolines. Each is a compiler-generated stub that performs local
unwinding for a specific try{} block.

**Do NOT re-implement these individually.**
They are MSVC-specific SEH table entries. A reimplementation should
use the target platform's native EH mechanism (e.g., Dwarf2, SjLj).

Pattern:
```
__local_unwind2(frame, target_offset)
```

Sizes range from 8 bytes (simple JMP) to 31 bytes (complex cleanup).
The unwind handlers are named `Unwind@0x474DF0` through `Unwind@0x4767D6`
in Ghidra.

| Address | Name | Size | Notes |
|---|---|---|---|
| 0x474DF0-0x4767D6 | ~280 Unwind handlers | 8-31 bytes each | MSVC SEH EH trampolines |

---

## Summary Statistics

| Category | Count |
|---|---|
| Standard string functions | ~45 |
| Math functions | ~17 |
| Compiler intrinsics | ~4 |
| FPU control | ~7 |
| Memory management | ~15 |
| File I/O (stream) | ~27 |
| File I/O (low-level FD) | ~12 |
| Path/drive helpers | ~7 |
| Formatted I/O | ~9 |
| Wide formatted I/O | ~10 |
| Wide char file I/O | ~14 |
| Locale/ctype | ~14 |
| Time functions | ~10 |
| Random | ~5 |
| Process/startup/exit | ~12 |
| C++ exception handling | ~15 |
| Threading | ~5 |
| Locking | ~11 |
| RTTI | ~2 |
| Signal handling | ~6 |
| Debug helpers | ~3 |
| DLL stubs | ~1 |
| SEH unwind trampolines | ~280 |
| **Total CRT (excluding SEH)** | **~230** |
