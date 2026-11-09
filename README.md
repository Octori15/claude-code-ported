# Claude Code on macOS 12 (Monterey): `_ubrk_clone` fix

An unofficial workaround. It makes the current Claude Code native binary (checked
against 2.1.319, darwin-arm64) start on macOS 12. It changes no system files and
leaves the VS Code extension's own files alone.

## The problem

```
dyld[…]: Symbol not found: _ubrk_clone
  Referenced from: …/resources/native-binary/claude
  Expected in: /usr/lib/libicucore.A.dylib
```

The native binary is built with Bun, with `minos 13.0`. Bun's JavaScriptCore calls the
ICU function `ubrk_clone()`, which `Intl.Segmenter` relies on. That function first
appeared in ICU 69, and macOS 12 ships Apple ICU 68.2. Checked against the macOS 12.1 SDK
stubs, `_ubrk_clone` is the only one of the binary's 850 imports that is missing. libc++
and libSystem have everything else it needs.

## The fix

* `libicushim.dylib`, built from `libicu/`, implements `ubrk_clone()` with
  `ubrk_safeClone()`, which ICU 68 has, and re-exports the system `libicucore` for every
  other ICU function. macOS ships no ICU headers, so `libicu/icucore.h` declares the
  ICU functions it uses.
* `patch.sh` makes a **patched copy** of the extension's binary. In that copy,
  `libicu/patch_icu_load.py` rewrites the path in its one `libicucore` load command to
  `@loader_path/libicushim.dylib` (29 bytes, in place), and the copy is re-signed ad hoc.
  The original binary is not modified.
* `claude-libicu-wrapper` starts that copy. VS Code's `claudeCode.claudeProcessWrapper`
  setting points at it, so the extension runs
  `claude-libicu-wrapper <bundled binary> <args…>` for every Claude process. The wrapper
  changes nothing on disk. If the copy was not made from that exact binary, for example
  after an extension update, it prints the `patch.sh` command to run and exits.

Everything goes in one folder,
`~/.vscode/extensions/anthropic.claude-code-darwin-arm64-patch/`: the shim, the scripts
and the patched copy, `claude-patched`. VS Code leaves that folder alone, because it
isn't listed as an installed extension.

## Install

Requires the Xcode Command Line Tools (`xcode-select --install`), for `clang` and `python3`.
Copy this folder to the Mac (or clone the repository there), then run from inside it:

```sh
sh libicu/build.sh
sh install.sh
sh ~/.vscode/extensions/anthropic.claude-code-darwin-arm64-patch/patch.sh
```

`libicu/build.sh` builds the shim and tests it against the real system ICU. Only a
shim that passes is left in `libicu/build/`. `install.sh` installs that shim and the
scripts into `~/.vscode/extensions/anthropic.claude-code-darwin-arm64-patch/` (set
`PREFIX=` to change that). `patch.sh` makes the patched copy of the newest installed
extension's binary and checks that it starts. Run it again after each extension update.
Then:

1. In VS Code, open **Preferences: Open User Settings (JSON)** and add the line that
   `install.sh` printed (an absolute path, because `~` is not expanded):
   ```json
   "claudeCode.claudeProcessWrapper": "/Users/<you>/.vscode/extensions/anthropic.claude-code-darwin-arm64-patch/claude-libicu-wrapper"
   ```
2. Run **Developer: Reload Window**.
3. Optional, for the terminal, add this line to `~/.zshrc`. It runs the binary bundled
   with the newest installed extension.
   ```sh
   alias claude="$HOME/.vscode/extensions/anthropic.claude-code-darwin-arm64-patch/claude"
   ```

## Undo

Remove the setting and the alias, then
`rm -rf ~/.vscode/extensions/anthropic.claude-code-darwin-arm64-patch`.

## Notes and limits

* **Signature.** The patched copy has an ad-hoc signature, not Anthropic's Developer ID
  signature. Without the hardened runtime it doesn't need the original entitlements. You
  may be asked again for permissions such as Automation that you granted to the original.
  Keychain access is unaffected: the CLI reads and writes its credentials through
  `/usr/bin/security`.
* **Terminal mode.** With `claudeCode.useTerminal` on, the extension types `claude` into a
  terminal instead of using the wrapper, so it needs the alias above (or the launcher on
  your PATH).
* **Optional native features.** The binary carries three native add-ons that load only
  when used. On macOS 12.1 all three load. Voice input should work. Computer Use takes
  screenshots with ScreenCaptureKit, which arrived in macOS 12.3, so expect that part not
  to work on 12.1.
* **Self-updater.** The wrapper sets `DISABLE_AUTOUPDATER=1` unless you set it yourself,
  so the CLI does not download an unpatched build. The extension still updates normally.
* **Disk space.** The patched copy is about 320 MB. There is only one, because `patch.sh`
  replaces it each time.
* **Future versions.** If a later version imports another symbol that macOS 12 lacks, dyld
  will name it in the same kind of error. If it is an ICU function, its backport goes in
  `libicu/icushim.c` and its declaration in `libicu/icucore.h`.
* **Scope.** Checked for macOS 12.1 on Apple Silicon. macOS 11 also needs a libc++ shim
  (see anthropics/claude-code#50456), so this alone is not enough there.
* **Not official.** Anthropic's supported minimum is macOS 13. This is an unofficial
  workaround.
