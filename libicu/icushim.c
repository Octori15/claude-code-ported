/*
 * libicushim.dylib - port APIs back to macOS 12 (Monterey):
 *     - `ubrk_clone()`: first appeared in ICU 69, and macOS 12 has Apple ICU 68.2.
 *       Implemented with `ubrk_safeClone()`, which ICU 68 has.
 *
 * Every other symbol comes from the system /usr/lib/libicucore.A.dylib, which this
 * library re-exports, so it can be loaded in place of libicucore.
 *
 * Build: see build.sh (needs the Xcode Command Line Tools).
 */
#include <stddef.h>

#include "icucore.h"

/* ICU 69+ API: clone the iterator; on success *status is left as it was. */
__attribute__((visibility("default")))
UBreakIterator *ubrk_clone(const UBreakIterator *bi, UErrorCode *status)
{
    if (status == NULL || U_FAILURE(*status))
        return NULL;

    UErrorCode local = U_ZERO_ERROR;
    UBreakIterator *clone = ubrk_safeClone(bi, NULL, NULL, &local);
    if (U_FAILURE(local)) {
        *status = local; /* U_MEMORY_ALLOCATION_ERROR or U_ILLEGAL_ARGUMENT_ERROR */
        return NULL;
    }
    return clone; /* don't leak ubrk_safeClone's U_SAFECLONE_ALLOCATED_WARNING */
}
