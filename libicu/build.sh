#!/bin/sh
# Build libicushim.dylib and test it against the system ICU. Only a library that
# passes the test is left in build/.
set -eu

src=$(cd "$(dirname "$0")" && pwd -P)
out="$src/build"

if [ "$(uname -s)" != Darwin ]; then
    echo "build.sh: this is for macOS" >&2
    exit 1
fi
if ! xcrun --find clang >/dev/null 2>&1; then
    echo "build.sh: needs the Xcode Command Line Tools (clang). Run:  xcode-select --install" >&2
    exit 1
fi

# A failed build must not leave an older library behind.
rm -f "$out/libicushim.dylib"

work=$(mktemp -d "${TMPDIR:-/tmp}/libicushim.XXXXXX")
trap 'rm -rf "$work"' EXIT

build() {
    xcrun clang -arch arm64 -arch x86_64 -mmacosx-version-min=12.0 -O2 -Wall -Wextra "$@"
}

echo "Building libicushim.dylib"
build -dynamiclib -o "$work/libicushim.dylib" "$src/icushim.c" \
    -install_name @loader_path/libicushim.dylib \
    -compatibility_version 1.0.0 -current_version 68.2.0 \
    -Wl,-reexport-licucore

echo "Testing it against the system ICU"
build -o "$work/shim_selftest" "$src/shim_selftest.c" -L"$work" -licushim
"$work/shim_selftest"

mkdir -p "$out"
cp "$work/libicushim.dylib" "$out/.libicushim.dylib.new"
mv -f "$out/.libicushim.dylib.new" "$out/libicushim.dylib"
echo "Built $out/libicushim.dylib"
