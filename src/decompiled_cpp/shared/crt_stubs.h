/**
 * crt_stubs.h — Declarations for all statically-linked MSVC CRT functions
 *
 * Lego Loco (loco.exe, 1998, MSVC x86)
 * Reverse engineered via Ghidra decompilation.
 *
 * This file declares ALL CRT functions that were statically linked into
 * loco.exe from the MSVC 6.0 runtime library. These were NOT dynamically
 * linked to msvcrt.dll — they are inlined into the .text section and would
 * need to be provided by any reimplementation.
 *
 * Categories:
 *   1. String manipulation (strncpy, strcat, strstr, wcscpy, etc.)
 *   2. Math (fmod, fabs, ceil, floor, log, cos, sin, tan, etc.)
 *   3. Memory management (malloc, free, HeapAlloc wrappers)
 *   4. File I/O (fopen, fclose, fseek, read, write, etc.)
 *   5. Console I/O (printf, fprintf, fputc, fgetc, wide variants)
 *   6. Time (time, difftime, localtime, strftime, etc.)
 *   7. Locale/ctype (isctype, tolower, iswctype, towlower, etc.)
 *   8. Random (srand, rand)
 *   9. Process/startup (mainCRTStartup, exit, abort, etc.)
 *   10. C++ exception handling (global_unwind2, local_unwind2, etc.)
 *   11. SEH unwind handlers (Unwind trampolines)
 *   12. Compiler intrinsics (__allmul, __aulldiv, __allshl, etc.)
 *   13. Threading (_beginthreadex, _endthreadex, TLS)
 *   14. Locking/synchronization (_lock, _unlock, lock_file, etc.)
 *   15. RTTI (type_info)
 *   16. Debug helpers (_CrtDbgReport support)
 *   17. FPU control (_controlfp, _clearfp, _statusfp)
 *   18. DLL import stubs (COMDLG32, DDRAW, DPLAYX, DSOUND, VERSION, MSVFW32)
 */

#pragma once

#include "types.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* MSVC type shims for GCC/MingW compatibility                         */
/* ================================================================== */

#ifndef _INC_CRTDEFS  /* not using real MSVC headers */
typedef long long __int64;
#endif

/* wint_t — MSVC defines this in <wchar.h> or <crtdefs.h>.
   We define it manually to avoid pulling in POSIX-wide-char declarations
   that conflict with the MSVC-specific signatures below. */
typedef unsigned short wint_t;

typedef struct _FILETIME { uint32_t dwLowDateTime; uint32_t dwHighDateTime; } FILETIME;
typedef void* FARPROC;

typedef unsigned int UINT_PTR;
struct tm;

/* ================================================================== */
/*  String manipulation                                                */
/* ================================================================== */

char*  _strncpy(char* dest, const char* src, size_t count);             /* 0x4661F0 */
char*  strncpy(char* dest, const char* src, size_t count);              /* 0x466EA0 */
char*  _strncat(char* dest, const char* src, size_t count);             /* 0x467D30 */
char*  _strrchr(const char* str, int ch);                               /* 0x467E60 */
char*  _strstr(const char* str, const char* substr);                    /* 0x468060 */
char*  _strpbrk(const char* str, const char* charset);                  /* 0x4676D0 */
size_t _strcspn(const char* str, const char* charset);                  /* 0x472B60 */
int    _strncmp(const char* s1, const char* s2, size_t n);              /* 0x472BA0 */
int    strncmp(const char* s1, const char* s2, size_t n);               /* 0x467330 */
int    strcoll(const char* s1, const char* s2);                         /* 0x46A9A0 */
size_t strxfrm(char* dest, const char* src, size_t count);              /* 0x46AB60 */
int    atoi(const char* str);                                           /* 0x466390 */
int    _atoi_l(const char* str);                                        /* 0x4662F0 */
char*  strtok(char* str, const char* delim);                            /* 0x4663A0 */
char*  strtok_s(char* str, const char* delim, char** ctx);              /* 0x4742D0 */
char*  itoa(int val, char* buf, int radix);                             /* 0x467EA0 */
char*  ltoa(long val, char* buf, int radix);                            /* 0x467EE0 */
char*  ultoa(unsigned long val, char* buf, int radix);                  /* 0x467F50 */
char*  strupr(char* str);                                               /* 0x474C70 */
char*  strerror(int errnum);                                            /* 0x4748D0 */
int    strerror_s(char* buf, size_t size, int errnum);                  /* 0x474A00 */
size_t strerrorlen(int errnum);                                         /* 0x474B40 */
int    toupper(int ch);                                                 /* 0x467710 */
int    tolower(int ch);                                                 /* 0x470900 */
int    isdigit(int ch);                                                 /* 0x469810 */
int    ispunct(int ch);                                                 /* 0x469840 */
int    isspace(int ch);                                                 /* 0x469870 */
int    isctype(int ch, int mask);                                       /* 0x4698A0 */
int    tolower(int ch);                                                 /* 0x469C40 */

/* Wide char */
wchar_t* wcslen(const wchar_t* str);                                     /* 0x469E60 */
wchar_t* wcscat(wchar_t* dest, const wchar_t* src);                     /* 0x46A120 */
wchar_t* wcsncat(wchar_t* dest, const wchar_t* src, size_t n);          /* 0x46A350 */
wchar_t* wcscpy(wchar_t* dest, const wchar_t* src);                     /* 0x46A4E0 */
wchar_t* wcsncpy(wchar_t* dest, const wchar_t* src, size_t n);          /* 0x46AD30 */
int      wcscmp(const wchar_t* s1, const wchar_t* s2);                  /* 0x46B8F0 */
int      wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n);       /* 0x46C520 */
int      wcscoll(const wchar_t* s1, const wchar_t* s2);                 /* 0x46A7F0 */
size_t   wcsxfrm(wchar_t* dest, const wchar_t* src, size_t n);          /* 0x46C690 */
wchar_t* wcschr(const wchar_t* str, wchar_t ch);                        /* 0x46C880 */
wchar_t* wcsrchr(const wchar_t* str, wchar_t ch);                       /* 0x46CAC0 */
size_t   wcscspn(const wchar_t* str, const wchar_t* charset);           /* 0x46A8F0 */
size_t   wcsspn(const wchar_t* str, const wchar_t* charset);            /* 0x46AEA0 */
wchar_t* wcspbrk(const wchar_t* str, const wchar_t* charset);           /* 0x46AF60 */
wchar_t* wcsstr(const wchar_t* str, const wchar_t* substr);             /* 0x471480 */
wchar_t* wcstok(wchar_t* str, const wchar_t* delim);                    /* 0x468380 */
wchar_t* wcstok_s(wchar_t* str, const wchar_t* delim, wchar_t** ctx);  /* 0x4745D0 */
wchar_t* wcsdup(const wchar_t* str);                                    /* 0x468510 */
size_t   wcsnlen(const wchar_t* str, size_t maxlen);                    /* 0x468650 */
wchar_t* wmemset(wchar_t* dest, wchar_t ch, size_t count);             /* 0x46C6F0 */
wchar_t* wmemcpy(wchar_t* dest, const wchar_t* src, size_t count);    /* 0x46CC40 */
long     wcstol(const wchar_t* str, wchar_t** end, int base);           /* 0x468D70 */
unsigned long wcstoul(const wchar_t* str, wchar_t** end, int base);     /* 0x469000 */
double   wcstod(const wchar_t* str, wchar_t** end);                     /* 0x469330 */
size_t   mbstowcs(wchar_t* dest, const char* src, size_t max);          /* 0x4722B0 */
size_t   wcstombs(char* dest, const wchar_t* src, size_t max);          /* 0x4726A0 */
int      mbtowc(wchar_t* pwc, const char* pmb, size_t max);             /* 0x46F030 */
int      wctomb(char* mb, wchar_t wc);                                  /* 0x46A6F0 */
int      mblen(const char* pmb, size_t max);                            /* 0x472C90 */
int      mbtowc_s(wchar_t* pwc, const char* pmb, size_t max, size_t* ret); /* 0x472FE0 */
int      wctomb_s(char* mb, size_t size, wchar_t wc);                  /* 0x473CC0 */
size_t   mbstowcs_s(size_t* ret, wchar_t* dest, size_t size, const char* src, size_t max); /* 0x473E70 */
size_t   wcstombs_s(size_t* ret, char* dest, size_t size, const wchar_t* src, size_t max); /* 0x473F80 */

/* _mbs (MBCS) */
wchar_t* _mbsrchr(const unsigned char* str, unsigned int ch);           /* 0x46B350 */

/* Wide char ctype */
int iswalnum(wint_t ch);                                                 /* 0x469D90 */
int iswctype(wint_t ch, int mask);                                       /* 0x46B0C0 */
int iswspace(wint_t ch);                                                 /* 0x46B3E0 */
int iswdigit(wint_t ch);                                                 /* 0x46B4D0 */
int iswxdigit(wint_t ch);                                                /* 0x46B5F0 */
int iswalpha(wint_t ch);                                                 /* 0x46B690 */
wint_t towlower(wint_t ch);                                              /* 0x46B1A0 */

/* ================================================================== */
/*  Math functions                                                     */
/* ================================================================== */

double fmod(double x, double y);                                        /* 0x464EF0 */
double fabs(double x);                                                  /* 0x464F70 */
/* 0x465010 is the WIN32 stream WRITE function (WIN32_StreamWrite,
 * ResDataSave.cpp) and 0x465090 is the WIN32 WRITE-stream constructor
 * (WIN32_StreamOpenWriteFile) — neither is CRT ceil/floor (the old
 * labels were decompiler misidentifications; corrected in
 * crt_stubs.cpp / ResourceManager.cpp). */
double log10(double x);                                                 /* 0x465180 */
double log(double x);                                                   /* 0x4651A0 */
double cos(double x);                                                   /* 0x465200 */
double sin(double x);                                                   /* 0x465250 */
double tan(double x);                                                   /* 0x4652A0 */
double exp(double x);                                                   /* 0x4652D0 */
double atan(double x);                                                  /* 0x4654C0 */
double acos(double x);                                                  /* 0x465560 */
float  sqrtf(float x);                                                  /* 0x465890 */
float  powf(float x, float y);                                          /* 0x465E70 */
double _ftol(double x);                                                 /* 0x465AD0 */
int    _ftolf(float x);                                                 /* 0x465960 */
long   __ftol(double x);                                                /* 0x466D30 */

/* Compiler intrinsics (x86) */
__int64 __allmul(__int64 a, __int64 b);                                 /* 0x46B160 */
__int64 __aulldiv(__int64 a, __int64 b);                                /* 0x46ED90 */
__int64 __aullrem(__int64 a, __int64 b);                                /* 0x46EE00 */
__int64 __allshl(__int64 a, int shift);                                  /* 0x471850 */

/* FPU control */
unsigned int _controlfp(unsigned int newval, unsigned int mask);        /* 0x470770 */
void _clearfp(void);                                                     /* 0x4707B0 */
int _statusfp(void);                                                     /* 0x46AAE0 */
void __setdefaultprecision(void);                                        /* 0x46AAC0 */
void __fpmath(void);                                                     /* 0x466180 */

/* ================================================================== */
/*  Memory management                                                  */
/* ================================================================== */

void* malloc(size_t size);                                              /* 0x466DE0 */
void  free(void* ptr);                                                  /* 0x466C70 */
void* _malloc_base(size_t size, int retry);                             /* 0x4673E0 */
void* _nh_malloc(size_t size);                                          /* 0x467430 */
void* malloc_zero(size_t size);                                         /* 0x4673C0 */
void* operator_new(size_t size);                                        /* 0x465CE0 */
void  operator_delete(void* ptr);                                       /* 0x465D30 */
void* GLOBAL_alloc(size_t size);                                        /* 0x465DE0 */
void  GLOBAL_free(void* ptr);                                           /* 0x465CD0 */
int   _callnewh(size_t size);                                           /* 0x46CE80 */
int   _heap_init(void);                                                 /* 0x46C4E0 */

/* Pattern allocator (MSVC specific) */
void* CRT_memset_pattern(void* dest, int c, size_t count);              /* 0x4671E0 */
void  CRT_free_pattern(void* ptr);                                      /* 0x467280 */

/* ================================================================== */
/*  File I/O                                                           */
/* ================================================================== */

FILE* fopen(const char* path, const char* mode);                        /* 0x46E420 */
int   fclose(FILE* stream);                                             /* 0x46E280 */
int   fseek(FILE* stream, long offset, int origin);                     /* 0x46E010 */
int   fflush(FILE* stream);                                             /* 0x46DC50 */
int   fputc(int ch, FILE* stream);                                      /* 0x46DC70 */
int   fgetc(FILE* stream);                                              /* 0x46DD00 */
int   setvbuf(FILE* stream, char* buf, int mode, size_t size);          /* 0x46E5A0 */
void  setbuf(FILE* stream, char* buf);                                  /* 0x46EAB0 */
FILE* tmpfile(void);                                                    /* 0x46E7D0 */
char* tmpnam(char* buf);                                                /* 0x46EA00 */
void  perror(const char* msg);                                          /* 0x46EAF0 */
int   fwide(FILE* stream, int mode);                                    /* 0x46ECE0 */

/* Low-level I/O (file descriptors) */
int    _close(int fd);                                                  /* 0x468BF0 */
int    _close_nolock(int fd);                                           /* 0x468C60 */
int    _write(int fd, const void* buf, unsigned int count);             /* 0x468CF0 */
int    _read(int fd, void* buf, unsigned int count);                    /* 0x468F80 */
long   _lseek(int fd, long offset, int origin);                         /* 0x469230 */
long   _lseek_nolock(int fd, long offset, int origin);                  /* 0x4692B0 */
intptr_t _get_osfhandle(int fd);                                        /* 0x470410 */
int     _free_osfhnd(int fd);                                           /* 0x470370 */
void    _lock_fh(int fd);                                               /* 0x470460 */
void    _unlock_fh(int fd);                                             /* 0x4704D0 */

/* Formatted I/O */
int    printf(const char* format, ...);                                 /* 0x46CEA0 */
int    fprintf(FILE* stream, const char* format, ...);                   /* 0x46BA20 */
int    sprintf(char* buf, const char* format, ...);                      /* 0x466D60 */
int    vsnprintf(char* buf, size_t size, const char* format, va_list ap); /* 0x467FF0 */
int    sprintf_s(char* buf, size_t size, const char* format, ...);       /* 0x466980 */

/* Wide char file I/O */
FILE*   _wfopen(const wchar_t* path, const wchar_t* mode);              /* 0x46FDD0 wrapper area */
wint_t  fgetwc(FILE* stream);                                           /* 0x46FDD0 */
wint_t  fputwc(wint_t ch, FILE* stream);                                /* 0x46FF70 */
wint_t  ungetwc(wint_t ch, FILE* stream);                               /* 0x470150 */
wint_t  fgetwc_nolock(FILE* stream);                                    /* 0x4702C0 */
wint_t  ungetwc_nolock(wint_t ch, FILE* stream);                        /* 0x4710E0 */
wchar_t* fgetws(wchar_t* str, int n, FILE* stream);                     /* 0x470500 */
int     fputws(const wchar_t* str, FILE* stream);                       /* 0x470990 */
wint_t  getwchar(void);                                                 /* 0x470B70 */
wint_t  putwchar(wint_t ch);                                            /* 0x470C60 */
wint_t  getwc(FILE* stream);                                            /* 0x470D20 */
wint_t  putwc(wint_t ch, FILE* stream);                                 /* 0x4711B0 */
wchar_t* getws(wchar_t* str);                                           /* 0x471340 */
int     putws(const wchar_t* str);                                      /* 0x471750 */

/* Wide char formatted I/O */
int fwprintf(FILE* stream, const wchar_t* format, ...);                 /* 0x46EE80 */
int fwprintf_s(FILE* stream, const wchar_t* format, ...);               /* 0x46F0E0 */
int fwscanf(FILE* stream, const wchar_t* format, ...);                  /* 0x46F180 */
int fwscanf_s(FILE* stream, const wchar_t* format, ...);                /* 0x46F350 */
int swprintf(wchar_t* buf, size_t size, const wchar_t* format, ...);    /* 0x46F430 */
int swscanf(const wchar_t* buf, const wchar_t* format, ...);            /* 0x46F520 */
int swprintf_s(wchar_t* buf, size_t size, const wchar_t* format, ...);  /* 0x46F6D0 */
int swscanf_s(const wchar_t* buf, const wchar_t* format, ...);          /* 0x46FB20 */
int wprintf(const wchar_t* format, ...);                                 /* 0x46FA30 */
int wscanf(const wchar_t* format, ...);                                 /* 0x46FBC0 */

/* File search (FindFirstFile/FindNextFile CRT wrappers) */
long _findfirst(const char* pattern, struct _finddata_t* data);         /* 0x467A20 */
int  _findnext(long handle, struct _finddata_t* data);                  /* 0x467B50 */
int  _findclose(long handle);                                           /* 0x467C70 */

/* Path helpers */
int  _isunccpath(const char* path);                                     /* 0x4668D0 */
int  _isvaliddrive(unsigned int drive);                                 /* 0x466AB0 */
int  _getdrive(void);                                                   /* 0x46B5A0 */
int  _mkdir(const char* path);                                          /* 0x466590 */

/* Time helpers */
int _filetime_to_time_t(FILETIME* ft);                                  /* 0x467CA0 */

/* ================================================================== */
/*  Time functions                                                     */
/* ================================================================== */

time_t time(time_t* timer);                                             /* 0x466490 */
double difftime(time_t t1, time_t t0);                                  /* 0x4664C0 */
struct tm* localtime(const time_t* timer);                               /* 0x4674E0 */
size_t strftime(char* str, size_t max, const char* fmt, const struct tm* tm); /* 0x471CF0, 0x4730E0 */
size_t wcsftime(wchar_t* str, size_t max, const wchar_t* fmt, const struct tm* tm); /* 0x472070, 0x473870 */
char* asctime(const struct tm* tm);                                      /* 0x46ACB0 */

/* ================================================================== */
/*  Random number generation                                           */
/* ================================================================== */

void srand(unsigned int seed);                                          /* 0x466140 */
int   rand(void);                                                       /* 0x466150 */
void  srand(unsigned int seed);                                         /* 0x469560 (larger) */

/* ================================================================== */
/*  Process / Startup                                                  */
/* ================================================================== */

void _mainCRTStartup(void);                                              /* 0x4689E0 */
void exit(int code);                                                    /* 0x466CE0 */
void __exit(int code);                                                  /* 0x4684F0 */
void abort(void);                                                       /* 0x468BC0 */
void _abort(void);                                                      /* 0x46AA17, 0x470750 */
void __amsg_exit(int msg);                                              /* 0x468B90 */
void _amsg_exit(int msg);                                               /* 0x465DA0 */
void _cexit(void);                                                      /* 0x4656B0 */
void _initterm(void* start, void* end);                                  /* 0x465F40 */
void _initterm_e(void* start, void* end);                               /* 0x465FD0 */
int  _CRT_INIT(void);                                                   /* 0x4684A0 */
void _exit_handler(void);                                                /* 0x4656F0 */
void _except_handler(void);                                              /* 0x465A30 */
int  _seh_filter(void);                                                  /* 0x465AC0 */
void _flushall(void);                                                    /* 0x46ECD0 */
void _flush_console_io(void);                                            /* 0x46FF30 */

/* ================================================================== */
/*  C++ exception handling / SEH                                       */
/* ================================================================== */

void __global_unwind2(void* frame);                                      /* 0x466050 */
void __local_unwind2(void* frame, int target);                          /* 0x466092 */
int  __abnormal_termination(void);                                       /* 0x4660FA */
void __NLG_Notify1(void);                                               /* 0x46611D */
void __NLG_Notify2(void);                                               /* 0x466126 */
void __CallSettingFrame_12(int val, void* frame, int code);             /* 0x46A7A0 */
void _CxxFrameHandler(void* frame, ...);                                 /* 0x46A200 */
void _CxxExceptionFilter(...);                                          /* 0x465E40, 0x46A2C0 */
void __CxxFrameHandler_addr(void* base, int* offsets);                  /* 0x46A770 */
void _terminate(void);                                                   /* 0x46AA30 */
void _local_unwind2_helper(int param);                                   /* 0x46CE65 */
void _purecall(void);                                                    /* 0x465D40 */
void _sig_check(int param1, int param2);                                 /* 0x470A90 */
void _sigdeleteset(int param1, int param2);                              /* 0x470B00 */

/* ================================================================== */
/*  Threading                                                          */
/* ================================================================== */

uintptr_t _beginthreadex(void* sa, size_t stack, unsigned (*start)(void*),
                          void* arg, unsigned flags, unsigned* tid);     /* 0x468870 */
void _endthreadex(unsigned retcode);                                     /* 0x4689A0 */
void _threadstart(DWORD* param);                                         /* 0x4688F0 */
void* _getptd(void);                                                     /* 0x46A870 */
void _initptd(void* ptd);                                                /* 0x46A850 */
void _init_locks(void);                                                  /* 0x46B740 */

/* ================================================================== */
/*  Locking / synchronization                                          */
/* ================================================================== */

void _lock(int locknum);                                                /* 0x46B770 */
void _unlock(int locknum);                                              /* 0x46B7F0 */
void _lock_file(FILE* stream);                                          /* 0x46B810 */
void _unlock_file(FILE* stream);                                        /* 0x46B880 */
void _lock_file2(int idx, FILE* stream);                                /* 0x46B850 */
void _unlock_file2(int idx, FILE* stream);                              /* 0x46B8C0 */
void _lock_ungetc(void);                                                /* 0x4685D0 */
void _unlock_ungetc(void);                                              /* 0x4685E0 */

/* ================================================================== */
/*  RTTI                                                               */
/* ================================================================== */

void type_info_dtor(void* self);                                         /* 0x466C20 */
void* type_info_scalar_dtor(void* self, char flags);                    /* 0x466C50 */

/* ================================================================== */
/*  Locale                                                             */
/* ================================================================== */

void _lc_locale_init(void);                                              /* 0x4661B0 */
int  ___lc_codepage_func(int locale);                                   /* 0x46EA50 */
int  __isindst(struct tm* tm);                                           /* 0x46DFE0 */

/* ================================================================== */
/*  Pointer validation                                                 */
/* ================================================================== */

int _isvalidreadptr(void* ptr, UINT_PTR size);                          /* 0x4706F0 */
int _isvalidwriteptr(void* ptr, UINT_PTR size);                         /* 0x470710 */
int _isvalidcodeptr(FARPROC ptr);                                        /* 0x470730 */

/* ================================================================== */
/*  errno / error handling                                             */
/* ================================================================== */

int* _errno(void);                                                      /* 0x467FD0 */
int* __doserrno(void);                                                  /* 0x467FE0 */

/* ================================================================== */
/*  Debug helpers                                                      */
/* ================================================================== */

void _debug_fmt_helper(char* buf, const char* fmt);                     /* 0x467490 */

/* ================================================================== */
/*  vsscanf helpers (va_arg)                                           */
/* ================================================================== */

int  _scanf_hex_digit(int ch);                                          /* 0x46DBE0 */
int* _va_arg_4byte(int* va);                                            /* 0x46C480 */
long long _va_arg_8byte(int* va);                                       /* 0x46C4A0 */

/* ================================================================== */
/*  CRT lock indices (used by _lock/_unlock)                           */
/* ================================================================== */

#define CRT_LOCK_STREAM(n)   (0x1c + (n))   /* Stream lock 0..19 */
#define CRT_LOCK_FILES       2               /* File list */
#define CRT_LOCK_ENV         3               /* Environment */
#define CRT_LOCK_SIGNAL      4               /* Signal */
#define CRT_LOCK_TIME        5               /* Time */
#define CRT_LOCK_RAND        6               /* Rand */
#define CRT_LOCK_THREAD      7               /* Thread init */
#define CRT_LOCK_UNGETC      0x0d             /* ungetc/ungetwc  */
#define CRT_LOCK_CTYPE       0x13             /* Ctype/locale */
#define CRT_LOCK_HEAP        0x11             /* Heap */
#define CRT_LOCK_MB          0x19             /* MBCS */

/* ================================================================== */
/*  File descriptor table constants                                    */
/* ================================================================== */

#define MAX_FD 2048   /* Max file descriptors */
#define FD_BUSY 0x01  /* FD entry in-use flag */

#ifdef __cplusplus
}
#endif
