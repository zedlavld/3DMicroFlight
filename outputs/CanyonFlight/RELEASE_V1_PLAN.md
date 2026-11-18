# v1.0 GitHub release plan

Status 2026-11-17: v35.24 (fb8703c) passed its F-16 hardware test; v1.0 is the same code with the version set to v1.0. All assets are now freely
licensed (MIT + CC BY 4.0 + public-domain data). The user-facing guide is
[INSTALL.md](../../INSTALL.md).

## What ships

| Asset | Source | Size (approx.) |
|---|---|---|
| `3DMicroFlight-v1.0.cia` | makerom, as for v35.x | 16 MB |
| `3DMicroFlight-v1.0.3dsx` | build output (Homebrew Launcher alternative) | 27 MB |
| `3DMicroFlight-scenery-01-of-NN.zip` ... (~13-14 parts) | `scenery/elevation36/`, `ground_us4000.bin`, the 24 region `.cft` files, then `scenery/stream32/` (manifest + 1,239 tile folders), in that order; each zip < 1.9 GB | ~24 GB compressed (29 GB raw) |
| `SHA256SUMS.txt` | all of the above | |

Not shipped: `scenery/elevation`, `elevation32`, `elevation35` (no longer read by the
game; 1 GB on the current card).

What the game reads (from the source): `scenery/stream32/manifest.json`,
`stream32/<c|f>X_Y/*.cft`, `scenery/elevation36/`, `scenery/ground_us4000.bin`,
`scenery/<region>_<n>.cft`. Everything is under `sdmc:/3ds/CanyonFlight/`.

Each zip holds full paths from the SD root (`3ds/CanyonFlight/scenery/...`), so users
extract every zip to the card root and folders merge. Parts are self-contained (no split
archives), so a damaged part can be re-downloaded alone. Deflate saves about 17% on the
tiles (tested on 200 coarse tiles).

## GitHub limits that shape this

- Release assets: at most 2 GiB per file; no total size limit, no bandwidth charge.
- Repository files: 100 MB hard limit; Git LFS has small free quotas. So scenery goes in
  release assets, never in the repository.
- Releases of a private repository are only visible to collaborators: the repository
  must be **public** for users to download.

## Steps

1. **Hardware check of v35.24** (F-16 test on the card, ~12 min). Done when the new F-16
   looks right on the runway, in cruise, at night and from the cockpit.
2. **Pick the scenery source.** The card (`I:\3ds\CanyonFlight\scenery`) is the copy every
   hardware test used: 136,013 stream32 files, 27.1 GB. `F:\CirrusFlightData\build35\stream32`
   has 124,843 files, 24.8 GB, so it is not the same set. Copy the card's `stream32`,
   `elevation36` and loose files to `F:\CirrusFlightData\release-v1.0\` (about 30-45 min
   at card read speed) and package from that copy.
3. **Release build v1.0**: set `AUTOTEST_BUILD` to `v1.0` (title, HOME menu and test
   header follow it), build the CIA and 3DSX. Run the 159-case long test once more on
   hardware (about 2 h 45 min) or at least the short script.
4. **README**: rewrite the top of the root README for v1.0 (five aircraft, 48 states,
   link to INSTALL.md and the release). It still describes v29.
5. **Package**: `python tools/package_release.py F:\CirrusFlightData\release-v1.0
   F:\CirrusFlightData\release-v1.0-zips --extra <cia> <3dsx> --verify`. Fills each zip
   up to 1.9 GB, writes SHA256SUMS.txt and contents.json, and checks every file is in
   exactly one zip. Tested on a small fake card (extracting all zips rebuilds the tree).
6. **Install test from the zips**: extract all zips into a blank Azahar `sdmc` folder and
   run the short Azahar script; on hardware, extract to a spare or freshly formatted card
   if one is available.
7. **Publish** (needs the user's go-ahead for each):
   - push `v34-daynight` (102 commits, ~132 MB of new objects, no file over 40 MB) and
     fast-forward `main` to it;
   - make `zedlavld/3DMicroFlight` public;
   - `gh release create v1.0` with the notes from INSTALL.md, then upload the assets.
     Upload time: ~24 GB, about 3 h at 20 Mbit/s up, 1 h at 60 Mbit/s.

## Decisions for the user

- Making the repository public (required for downloads).
- Whether old history stays: earlier commits contain the CC BY-NC-SA F-16. Those files
  were shared under their licence (non-commercial, credited), so keeping the history is
  allowed; rewriting it is possible but changes every commit hash.
- Whether to also offer a single torrent or an Internet Archive mirror of the scenery
  (one download instead of ~14).

## Progress (2026-11-17)

- Steps 1-6 done. v35.24 F-16 hardware test: 10/10 clean. v1.0 built (same code as v35.24):
  `D:\CirrusFlight-v31-gear\3DMicroFlight-v1.0.cia` and `.3dsx`.
- Scenery copied from the card to `F:\CirrusFlightData\release-v1.0` (identical file counts
  and byte totals). Packed into `F:\CirrusFlightData\release-v1.0-zips`: 13 zips, 23.2 GB,
  12 x 1.90 GB + 0.33 GB, 151,198 files, verified (each file in one zip, CRCs good), with
  SHA256SUMS.txt (zips, CIA, 3DSX) and contents.json. Packing took 23 min.
- Install test: all 13 zips extracted into Azahar's sdmc in 5 min; the tree matched the
  source exactly. v1.0 in Azahar loaded photo scenery in Seattle, Denver, Big Bend, the
  Smokies, the Adirondacks and Pennsylvania. The KLAS night approach (night_ext) stalled
  in Azahar after its first shot with full scenery and city lights; the same files ran
  the KLAS night cases fine on hardware (v35.22, v35.24), so this looks Azahar-only, but
  it is not explained yet. The extracted copy is kept as
  `%APPDATA%\Azahar\sdmc\3ds\CanyonFlight-v1.0-release`; Azahar's own folder is restored.
- Left: step 7 (push, make public, create the release and upload 23.2 GB).

