# Compatibility Layer

This directory will hold the small embedded compatibility surface used to satisfy residual `sdltrs` dependencies without compiling the SDL desktop frontend.

Planned contents:

- `SDL.h`
- `SDL_types.h`
- `SDL_joystick.h`
- `sdl_compat.c`

The goal is to provide only the tiny subset needed by the embedded build, not a full SDL reimplementation.
