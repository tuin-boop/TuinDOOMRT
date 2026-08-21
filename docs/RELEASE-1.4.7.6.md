# TuinDOOM RT 1.4.7.6

This maintenance release fixes the lightweight Spider Mastermind death sequence and removes non-working manual sun-intensity controls.

## Changes

- The Spider Mastermind now disappears after its explosion sequence instead of leaving the reused intact RTX mesh in the level.
- Boss-death map triggers still run before the Spider Mastermind is removed.
- Removed the non-working comma, period, and bracket sun-intensity bindings.
- Updated the launcher and documentation to list only working default controls.
- Restored the Doom II voxel event-handler entry point for clean installations.

## Default controls

- `N`: randomize the sun direction and intensity.
- `F`: toggle the flashlight.

## Requirements

- Windows 10/11 64-bit.
- An RTX/Vulkan-capable GPU with current drivers.
- A legally owned Doom or Doom II IWAD.
