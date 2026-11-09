#!/bin/sh
# patch.sh - make the patched copy of Claude Code's native binary that
# claude-libicu-wrapper starts. Run it after installing, and again after each update
# of the VS Code extension.
#
#   sh patch.sh [/path/to/claude]
#
# Without an argument it uses the binary bundled with the newest installed VS Code
# extension. The copy is claude-patched, next to this script, and replaces the copy
# made before. The original binary is never modified. The copy loads
# libicushim.dylib in place of /usr/lib/libicucore.A.dylib and is re-signed ad hoc.
set -eu
PATH="/usr/bin:/bin:/usr/sbin:/sbin:$PATH"

here=$(cd "$(dirname "$0")" && pwd -P)
shim="$here/libicushim.dylib"
patcher="$here/patch_icu_load.py"
copy="$here/claude-patched" # finds $shim beside it, at @loader_path

die() {
    printf 'patch.sh: %s\n' "$*" >&2
    exit 1
}

if [ $# -ge 1 ]; then
    orig=$1
else
    orig=$(ls -td "$HOME"/.vscode/extensions/anthropic.claude-code-*/resources/native-binary/claude \
        2>/dev/null | head -n 1 || true)
    [ -n "$orig" ] || die "no Claude Code binary found in ~/.vscode/extensions; pass its path"
fi
[ -f "$orig" ] || die "not a file: $orig"
[ -f "$shim" ] || die "missing $shim. Run install.sh, then the patch.sh that it installs."
command -v codesign >/dev/null 2>&1 || die "codesign not found (this is for macOS)"
python3 -c '' 2>/dev/null || die "needs python3 from the Xcode Command Line Tools. Run:  xcode-select --install"

# Identifies the original binary; claude-libicu-wrapper compares it before each start.
case $(uname -s) in
Darwin) stamp=$(stat -f '%z:%m:%i' "$orig") ;;
*) stamp=$(stat -c '%s:%Y:%i' "$orig") ;;
esac

tmp=$(mktemp "$here/.claude-patched.XXXXXX")
trap 'rm -f "$tmp"' EXIT
cp "$orig" "$tmp"
python3 "$patcher" "$tmp" || die "could not patch a copy of $orig"
out=$(codesign --force --sign - "$tmp" 2>&1) || die "codesign failed: $out"
xattr -c "$tmp" 2>/dev/null || true # drop com.apple.quarantine copied from the original
chmod 755 "$tmp"
# Rename into place, so running Claude processes keep the copy they started from.
mv -f "$tmp" "$copy"
trap - EXIT
printf '%s\n' "$orig|$stamp" >"$copy.source" # written last
echo "Patched copy of $orig"
echo "  is $copy"

if version=$(DISABLE_AUTOUPDATER=1 "$copy" --version 2>&1); then
    echo "It starts: $version"
else
    echo "It does not start:" >&2
    printf '%s\n' "$version" | sed 's/^/    /' >&2
    exit 1
fi
