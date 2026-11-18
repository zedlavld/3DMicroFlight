# Stereoscopic top screen — v07

Move the hardware 3D depth slider upward to enable stereo. At zero, only the
left-eye scene is rendered. The lower-screen instruments and top HUD remain
at screen depth. No glasses are required on New 3DS XL.

The renderer uses parallel views and asymmetric perspective projections, with
convergence at the exterior camera distance (14–160 m), or 120 m in cockpit
view. Depth strength follows the slider with a conservative, deliberately
scaled baseline. This is an artistic scene-depth scale, not physical human IPD.
Terrain behind the aircraft sits inside the screen. Very close geometry can
have stronger disparity; comfort and eye ordering need verification on hardware.

Each eye has its own color/depth target. Aircraft geometry, propeller animation,
terrain streaming and physics advance once per frame and are shared by both
passes. Stereo draws geometry twice; it can reduce FPS. Slider zero restores
one-pass rendering. Command-buffer capacity was doubled for the second pass.

Projection math tests verify zero disparity at convergence, symmetry, opposite
near/far disparity, and a zero-slider identity. The app compiles successfully,
but there has been no device test of visual comfort, frame rate, or stereo output.
