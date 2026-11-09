#!/bin/sh
# Install the shim built by libicu/build.sh, and the scripts, into
# ~/.vscode/extensions/anthropic.claude-code-darwin-arm64-patch (or $PREFIX), the
# folder where patch.sh then makes the patched copy. Changes nothing outside it.
set -eu

src=$(cd "$(dirname "$0")" && pwd -P)
prefix=${PREFIX:-$HOME/.vscode/extensions/anthropic.claude-code-darwin-arm64-patch}
shim="$src/libicu/build/libicushim.dylib"

if [ "$(uname -s)" != Darwin ]; then
    echo "install.sh: this is for macOS" >&2
    exit 1
fi
if [ ! -f "$shim" ]; then
    echo "install.sh: $shim not found. Run  sh '$src/libicu/build.sh'  first." >&2
    exit 1
fi

mkdir -p "$prefix"
for f in "$shim" "$src/libicu/patch_icu_load.py" \
    "$src/patch.sh" "$src/claude-libicu-wrapper" "$src/claude"; do
    name=$(basename "$f")
    # Copy, then rename into place, so running Claude processes keep their mapped files.
    cp "$f" "$prefix/.$name.new"
    xattr -c "$prefix/.$name.new" 2>/dev/null || true
    mv -f "$prefix/.$name.new" "$prefix/$name"
done
chmod 755 "$prefix/patch.sh" "$prefix/claude-libicu-wrapper" "$prefix/claude" "$prefix/patch_icu_load.py"
echo "Installed into $prefix"

cat <<EOF

Next steps
  1. Patch the binary of the installed VS Code extension. Run this again after
     each extension update:
         sh "$prefix/patch.sh"
  2. VS Code: open "Preferences: Open User Settings (JSON)" and add
         "claudeCode.claudeProcessWrapper": "$prefix/claude-icu-wrapper"
     then run "Developer: Reload Window".
  3. Terminal (optional): add this line to ~/.zshrc
         alias claude="$prefix/claude"

  Undo: remove that setting and alias, then
      rm -rf "$prefix"
EOF
