# Installing 3DMicroFlight

3DMicroFlight is a free flight simulator for the New Nintendo 3DS family. It has five
aircraft (Cirrus SR22, Cirrus SF50, Boeing 737, Airbus A321 and F-16), aerial-photo
scenery of all 48 contiguous US states, and day and night flying.

Installing takes about 1 to 2 hours, most of it copying the scenery to the SD card.

## What you need

- **A New Nintendo 3DS, New 3DS XL or New 2DS XL.** The original 3DS, 3DS XL and 2DS
  are not supported: the game needs the New models' faster processor and extra memory.
- **Custom firmware (Luma3DS) with FBI installed.** If your console doesn't have it yet,
  follow the guide at <https://3ds.hacks.guide> first. 3DMicroFlight is not sold or
  distributed through Nintendo.
- **A microSD card of 64 GB or more**, with at least **35 GB free**. The card must be
  formatted **FAT32** (see "Formatting a large SD card" below).
- **A computer with a microSD reader**, and about **30 GB of free disk space** for the
  downloads.

## Step 1: Download the files

Open the latest release on the
[Releases page](https://github.com/zedlavld/3DMicroFlight/releases) and download:

| File | What it is |
|---|---|
| `3DMicroFlight-v1.0.cia` | The game, installed to the HOME Menu |
| `3DMicroFlight-scenery-01-of-NN.zip` ... `-NN-of-NN.zip` | The scenery: elevation, ground colours and aerial photos of the 48 states, split into parts under 2 GB. Download **all** of them. |
| `3DMicroFlight-v1.0.3dsx` | Optional: the same game for the Homebrew Launcher, if you don't want a HOME Menu icon |
| `SHA256SUMS.txt` | Optional: checksums to confirm the downloads aren't damaged |

The scenery parts can be downloaded in any order and at different times.

## Step 2: Copy the scenery to the SD card

1. Turn the console off and take out the SD card. Put it in your computer.
2. Extract every scenery `.zip` to the **root of the SD card** (the top level,
   for example `E:\` on Windows or `/Volumes/3DS` on a Mac).
   - Every zip contains the same top folder, `3ds`, and the folders merge on the card.
     If your computer asks whether to merge or replace, choose **merge** or **Yes to all**:
     each file is in only one zip, so nothing is overwritten.
   - Extract the zips themselves, not a folder named after the zip: the card's top level
     must have `3ds`, not `3DMicroFlight-scenery-01-of-NN\3ds`.
   - On Windows, 7-Zip (<https://www.7-zip.org>) is much faster than the built-in
     extractor: right-click the zip, choose 7-Zip > Extract files..., and set the
     destination to the SD card root.
3. When you're done, the card should contain:

   ```
   SD card
   └── 3ds
       └── CanyonFlight
           └── scenery
               ├── elevation36\        (about 15,000 files)
               ├── stream32\           (about 136,000 files in ~1,240 folders)
               ├── ground_us4000.bin
               └── (24 .cft files)
   ```

   The folder is called `CanyonFlight` for historical reasons. Don't rename it.

Copying about 150,000 files takes a while: expect 30 to 90 minutes depending on the card.

## Step 3: Install the game

1. Copy `3DMicroFlight-v1.0.cia` to the root of the SD card.
2. Safely eject the card, put it back in the console and turn it on.
3. Open **FBI**, choose **SD**, select `3DMicroFlight-v1.0.cia`, then
   **Install and delete CIA** (or **Install CIA** to keep the file).
4. Go back to the HOME Menu. 3DMicroFlight appears as a new icon. Start it.

## Sound

The game's engine and wind sounds need the console's DSP firmware dump. If the game says
"Audio unavailable on this system", run **DSP1** once (included in the 3ds.hacks.guide
setup, or from <https://github.com/zoogie/DSP1/releases>), which creates
`/3ds/dspfirm.cdc`. Then restart the game.

## Updating

- To update the game, install the new `.cia` with FBI over the old one. Your saves are kept.
- Only download new scenery when the release notes say the scenery changed.

## Saves and settings

Saves, control settings and flight logs are stored in `/3ds/3DMicroFlight/` on the SD
card. Back up that folder to keep your saves.

## Troubleshooting

| Problem | Fix |
|---|---|
| The ground is plain colour or the game reports tile errors | Some scenery is missing. Check that every scenery zip was extracted to the SD root and that the folders match the layout in Step 2. |
| FBI says the CIA is invalid | The download is damaged. Download it again (or compare it with `SHA256SUMS.txt`). |
| The game doesn't start on an original 3DS / 2DS | Not supported: the game needs a New 3DS, New 3DS XL or New 2DS XL. |
| No sound | See "Sound" above. |
| A "Run autotest" item appears on the title menu | There is an `autotest.txt` in `/3ds/3DMicroFlight/` from testing. Delete that file. |

## Formatting a large SD card

Cards of 64 GB and more usually come formatted as exFAT, which the 3DS cannot read.
Back up the card first, then format it FAT32:

- **Windows:** Windows can't format large cards as FAT32 itself. Use **GUIFormat**
  (search for "guiformat fat32") with allocation unit size **32768**.
- **macOS:** Disk Utility > select the card > Erase > Format **MS-DOS (FAT)**,
  Scheme **Master Boot Record**.
- **Linux:** `mkfs.fat -F 32 -s 64 /dev/sdX1` (check the device name first).

## Removing the game

Uninstall it in **System Settings > Data Management > Nintendo 3DS > Software**, or in
FBI under Titles. Then delete `/3ds/CanyonFlight/` and `/3ds/3DMicroFlight/` from the SD
card to free the space.

## Licence and credits

3DMicroFlight is free software under the MIT licence. The aircraft models and map data
are by other people and are credited in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
3DMicroFlight is an unofficial fan project, not affiliated with Nintendo or with any
aircraft manufacturer, and not for real-world navigation or flight training.
