# CirrusFlight v21 — cockpit refinement from image references

Google Images searches for **Cirrus SR22 G1000 cockpit** and **Cirrus Vision SF50 2017 cockpit** were viewed and used as visual references for separate interior layouts. Photos guide panel shape, proportions, color grouping, vents and control placement. The delivered mesh and small material texture are original; no Google result thumbnails or source photograph pixels are embedded in the application.

## References and changes

The SR22 reference set shows a rounded glare shield, two main displays, standby gauges, circular air vents and a center control/keypad stack. Those features replace the generic flat panel and blank copilot block. Google results mix simulator hardware, SR22/SR22T and different generations; this remains a Perspective/G1000-era visual approximation rather than a serial-number-specific replica. The [Laminar SR22 cockpit manual](https://x-plane.com/manuals/Cirrus_SR22_Pilot_Operating_Manual.pdf?v=2024-02-09) provides additional visual layout context, including its documented SR22T variant; no engine or mass parameters were changed.

The early SF50 reference directly corrects v20's three-main-screen arrangement: there are two large main displays with three smaller landscape controllers beneath them. See [Cirrus Sales & Service's 2015 cockpit reveal and photo](https://www.cirrus-sas.com/news/2015/9/sf50-cockpit-released?page=1), also found through Google Images. The model now includes that layout, a thin windshield divider, larger display proportions, circular vents, a lower switch panel and curved glare shield. The primary manufacturer reference is the [early Vision SF50 brochure](https://cirrusaircraft.com/wp-content/uploads/2014/11/SF50-brochure-digital.pdf).

A single original 128 x 128 RGB565 matte-grain texture adds subtle surface variation to trim and panel surfaces (32 KiB). Instrument faces sample a white atlas corner, preserving their live graphics. `tools/build_cockpit_material.py` regenerates the material deterministically. This is a procedural material, not photographic texturing or a scan of the real cabin. The software previews show geometry and base colors; they omit native material sampling.

## Behavior and verification

ZR still selects the interior/exterior camera; C-stick looks around, and tapping ZL recenters. The lower PFD, menus, three saves and physics are unchanged. The small top-screen PFDs are live cues. Controller tiles, standby-gauge representations, keypad buttons, vents and switches remain simplified decorative features; this does not implement the real Garmin or standby instrument systems.

Representative meshes are 785 SR22 and 699 SF50 triangles, below the unchanged 1024-triangle cap. Shared stereo preparation, bounded buffers and the near-cockpit depth pass remain from v20. Representative uploads are approximately 85/75 KiB per frame, separate from scenery's 64-KiB upload budget. No measured hardware frame-rate claim is made.

60,198 compiled ARM cockpit/camera checks passed, including geometry validity, bounds, changing flight state, extreme attitudes and camera axes. Recorder tests and menu layout checks passed with v21 identifiers. Native GPU material appearance, stereo comfort and device frame rate remain unverified. Existing broader v20 regression reports apply to the unchanged flight/scenery/camera code.

Install the update by copying `sdmc/` contents onto the existing SD card. Scenery stays under `/3ds/CanyonFlight/scenery/`; all three save files remain compatible.
