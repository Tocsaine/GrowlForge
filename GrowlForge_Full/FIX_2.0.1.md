# GrowlForge 2.0.1 build fix

This patch fixes MSVC errors C2666 in `src/GrowlForgeGUI.h`.

Cause: several GDI+ `FillRectangle` and `DrawRectangle` calls mixed `float` and integer arguments. MSVC could not choose between the integer and `Gdiplus::REAL` overloads.

Fix: all rectangle drawing calls now use explicit `Gdiplus::RectF` objects, selecting the floating-point overload unambiguously.

No DSP, parameter, state, or sound behavior was changed.
