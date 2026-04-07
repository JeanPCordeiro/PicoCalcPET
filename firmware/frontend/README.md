# Frontend Layer

This directory will contain the PicoCalc-facing replacements for the `sdltrs` desktop frontend.

Expected responsibilities:

- screen initialization and redraw
- text cell rendering
- keyboard event translation
- runtime event pumping
- LED and status indicator updates
- optional mouse stubs

This layer should satisfy the host-facing functions the `sdltrs` core expects.
