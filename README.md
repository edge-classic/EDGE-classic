# WARNING! This branch is tracking implementation of SDL2 and is not fully stable! Please use the main branch for regular use!
Known bugs:
- EDGE Initialization/Nodebuilding progress bars don't show in fullscreen unless at native resolution; no effect on gameplay
- If the player drops the console while holding a movement key, that input will be repeated until they raise the console and press the same key again (this seems to occur in the
original SDL 1.x branch as well)

# EDGE-classic
Fork of the EDGE 1.35 release.

# Current Status
Program builds successfully for both Windows and Linux. Windows executables are currently compiled using MSYS2.

Please refer to CHANGELOG-135.1.txt for more detailed changes.
