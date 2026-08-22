# TuinDOOM RT 1.4.7.7

This maintenance release fixes a crash after pressing Apply on the first-start sound-volume page.

## Changes

- Removed the optional post-setup RTX intro scene, which exceeded the RTGL 1.6.3 dynamic-vertex limit and could crash clean installations.
- Kept the first-start settings pages and normal ray-traced main menu.
- Removed the unused intro model from the Windows installer, reducing its installed payload.

## Requirements

- Windows 10/11 64-bit.
- An RTX/Vulkan-capable GPU with current drivers.
- A legally owned Doom or Doom II IWAD.
