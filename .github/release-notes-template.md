RedShipBlueShip combines Ocarina of Time (Ship of Harkinian) and Majora's Mask (2Ship2Harkinian) into a single executable with cross-game randomization inspired by OoTMM.

**This is an alpha build.** Expect bugs, missing features, and breaking changes between releases.

## Downloads

- **Windows (x64):** `redshipblueship-{{TAG}}-windows.zip`
- **Linux (x86_64):** `redshipblueship-{{TAG}}-linux.AppImage`
- **macOS (Universal, macOS 15+):** `redshipblueship-{{TAG}}-macos.dmg`

All builds bundle `soh.o2r`, `2ship.o2r`, and the asset extraction tools: the `ZAPD_OoT` and `ZAPD_MM` extractors and `assets/xml/` (on Windows and Linux the extractors are in `assets/extractor/`; in the macOS app they are in `Contents/MacOS/`, beside the game). The macOS dmg is attached by its own job once the macOS build succeeds, so it can appear after the other downloads. You need your own original Ocarina of Time and Majora's Mask ROMs: on first launch the game asks for them and extracts the remaining assets.

## Running

- **Windows:** extract the zip anywhere and run `redship.exe`.
- **Linux:** `chmod +x redshipblueship-{{TAG}}-linux.AppImage`, then run it.
- **macOS:** open `redshipblueship-{{TAG}}-macos.dmg`, copy `RedShipBlueShip.app` to a writable folder (such as `Applications`), and run it.

## Before you play: saves from earlier builds

- **Back up first.** Copy your `Save/` and `Randomizer/` folders aside before running this build on them.
- **Saves are version 6.** A file saved by `v0.5.0-alpha` or any earlier build, plain (non-randomizer) files included, is refused ("Refused: File made by an older version") and is not converted. It stays on disk: keep the old build to finish it, or erase it to start a new file in that slot. Saves made with development builds between releases are not supported.
- **Old seed packages are refused.** This build's seed generator is version {{GENERATOR_VERSION}}: a seed package made by a build with another generator version is refused; generate the seed again.
- **Old seeds make new worlds.** A seed and settings from an older build generate a different world on this one.
- **Rollback:** install into a new folder and keep the old one; earlier releases, `v0.1.1-prealpha` among them, stay downloadable from the releases page.

## Known issues and changes

- Known issues: [`docs/known-issues.md`](https://github.com/penelopeduncan/redshipblueship/blob/{{TAG}}/docs/known-issues.md)
- What changed in this release: [`docs/release-notes-next.md`](https://github.com/penelopeduncan/redshipblueship/blob/{{TAG}}/docs/release-notes-next.md)

Found something else? Report it at https://github.com/penelopeduncan/redshipblueship/issues.
