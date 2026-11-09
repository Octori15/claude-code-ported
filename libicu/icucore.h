/*
 * icucore.h - declarations for part of the ICU C API in Apple's
 * /usr/lib/libicucore.A.dylib.
 *
 * macOS ships the library without ICU's headers. This file declares a few of its
 * functions, with ICU's names and types. macOS 12 (Monterey) has Apple ICU 68.2.
 */
#ifndef ICUCORE_H
#define ICUCORE_H

#include <stdint.h>

typedef struct UBreakIterator UBreakIterator;
typedef uint16_t UChar;
typedef int32_t UErrorCode; /* an int-sized enum in ICU: warnings < 0, errors > 0 */

#define U_ZERO_ERROR 0
#define U_ILLEGAL_ARGUMENT_ERROR 1
#define U_FAILURE(x) ((x) > U_ZERO_ERROR)

#define UBRK_WORD 1 /* a UBreakIteratorType, also an int-sized enum */
#define UBRK_DONE (-1)

UBreakIterator *ubrk_open(int type, const char *locale, const UChar *text, int32_t textLength,
                          UErrorCode *status);
void ubrk_setText(UBreakIterator *bi, const UChar *text, int32_t textLength, UErrorCode *status);
int32_t ubrk_first(UBreakIterator *bi);
int32_t ubrk_next(UBreakIterator *bi);
void ubrk_close(UBreakIterator *bi);

/* In ICU since 2.0; deprecated in ICU 69 in favor of ubrk_clone(), not removed.
 * On success it sets *status to U_SAFECLONE_ALLOCATED_WARNING. */
UBreakIterator *ubrk_safeClone(const UBreakIterator *bi, void *stackBuffer,
                               int32_t *pBufferSize, UErrorCode *status);

/* New in ICU 69, so not in macOS 12. On success it leaves *status as it was. */
UBreakIterator *ubrk_clone(const UBreakIterator *bi, UErrorCode *status);

const char *u_errorName(UErrorCode code);
void u_getVersion(uint8_t versionArray[4]);

#endif
