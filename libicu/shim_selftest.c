/*
 * Self-test for libicushim.dylib; build.sh builds and runs it, and keeps the
 * library only if the test passes.
 *
 * It links against libicushim only, so ubrk_open() and the other ICU calls resolve
 * through the shim's re-export of /usr/lib/libicucore.A.dylib, as they do in any
 * program that loads the shim in place of libicucore.
 */
#include <stdint.h>
#include <stdio.h>

#include "icucore.h"

static int boundaries(UBreakIterator *bi, int32_t *out, int max)
{
    int n = 0;
    for (int32_t b = ubrk_first(bi); b != UBRK_DONE && n < max; b = ubrk_next(bi))
        out[n++] = b;
    return n;
}

static int same(const int32_t *got, int n, const int32_t *want, int m)
{
    if (n != m)
        return 0;
    for (int i = 0; i < n; i++)
        if (got[i] != want[i])
            return 0;
    return 1;
}

static int fail(const char *what, UErrorCode status)
{
    fprintf(stderr, "libicushim self-test FAILED: %s (%s)\n", what, u_errorName(status));
    return 1;
}

int main(void)
{
    static const UChar hello[] = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!'};
    static const UChar ab[] = {'a', ' ', 'b'};
    static const int32_t want_hello[] = {0, 5, 6, 7, 12, 13};
    static const int32_t want_ab[] = {0, 1, 2, 3};
    int32_t got[32];
    int n;

    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator *orig = ubrk_open(UBRK_WORD, "en_US", hello, 13, &status);
    if (orig == NULL || U_FAILURE(status))
        return fail("ubrk_open", status);

    /* A clone must succeed and, like ICU 69's ubrk_clone, leave the status at U_ZERO_ERROR. */
    UErrorCode cstatus = U_ZERO_ERROR;
    UBreakIterator *copy = ubrk_clone(orig, &cstatus);
    if (copy == NULL || cstatus != U_ZERO_ERROR)
        return fail("ubrk_clone", cstatus);

    n = boundaries(copy, got, 32);
    if (!same(got, n, want_hello, 6))
        return fail("clone gives different word boundaries", cstatus);

    /* The clone is independent: new text on the clone leaves the original alone. */
    ubrk_setText(copy, ab, 3, &cstatus);
    if (U_FAILURE(cstatus))
        return fail("ubrk_setText on the clone", cstatus);
    n = boundaries(copy, got, 32);
    if (!same(got, n, want_ab, 4))
        return fail("clone after ubrk_setText", cstatus);
    n = boundaries(orig, got, 32);
    if (!same(got, n, want_hello, 6))
        return fail("original changed when the clone did", status);

    /* An incoming failure status is returned untouched, with no clone. */
    UErrorCode failed = U_ILLEGAL_ARGUMENT_ERROR;
    if (ubrk_clone(orig, &failed) != NULL || failed != U_ILLEGAL_ARGUMENT_ERROR)
        return fail("ubrk_clone ignored an incoming failure", failed);

    uint8_t v[4];
    u_getVersion(v);
    printf("libicushim self-test passed (system ICU %d.%d)\n", v[0], v[1]);
    ubrk_close(copy);
    ubrk_close(orig);
    return 0;
}
