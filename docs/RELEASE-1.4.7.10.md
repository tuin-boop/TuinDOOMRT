# TuinDOOM RT 1.4.7.10

This release restores custom music from user-loaded WADs and PK3s.

## Changes

- Custom map music now takes priority over the bundled ray-traced Doom and Doom II soundtrack replacements.
- Supports classic `D_*` music lumps in WADs and replacements placed in a PK3 `music/` folder.
- Keeps the bundled remastered soundtrack as the fallback when a loaded mod does not provide its own music.
- Preserves the existing priority of all non-music ray-tracing resources.
- Creates each generated RT scene's material junction synchronously, preventing a first-launch race on previously unseen custom maps.
- Fixes TNT MAP27 exhausting system memory while analyzing its circular staircase-sector chain for RT geometry.

## Requirements

- Windows 10/11 64-bit.
- An RTX/Vulkan-capable GPU with current drivers.
- A legally owned Doom, Doom II, TNT, or Plutonia IWAD.
