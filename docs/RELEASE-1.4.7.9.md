# TuinDOOM RT 1.4.7.9

This release adds proper Final Doom support for TNT: Evilution and The Plutonia Experiment.

## Changes

- Added automatic Steam and common GOG scanning for `TNT.WAD` and `PLUTONIA.WAD`.
- Added clear Final Doom labels in the launcher IWAD selector.
- Added dedicated engine launch modes for TNT and Plutonia, avoiding the redundant IWAD selection dialog.
- Removed the inaccurate unsupported-WAD warning for TNT and Plutonia.
- Enabled the Doom II-family renderer path and automatic ray-traced lighting for both Final Doom games.
- Kept the hand-authored Doom II MAP01–32 sky and lighting preset disabled for Final Doom maps.
- Preserved each game's own IWAD data, skies, music, and map definitions.

## Requirements

- Windows 10/11 64-bit.
- An RTX/Vulkan-capable GPU with current drivers.
- A legally owned Doom, Doom II, TNT, or Plutonia IWAD.
