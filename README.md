# Gilded Compass

Windows C++ source for Gilded Compass 1.0.0, a Skyrim-style compass for
Crimson Desert with decorative gold rails and woven end caps. The original
minimap stays in place. The bar displays heading during exploration and the
bearing/distance of one selected quest destination or manual map pin.

This repository contains the working native UI bridge, camera observers,
navigation logic, decorative HUD generator, build-specific bindings and tests.
It supersedes the earlier layered-window prototypes. No DirectX overlay is used.

## Status and compatibility

The author playtested fullscreen, heading alignment, main and non-main selected
quests, selected manual pins, menu transitions, save/reload, bow aiming,
Axiom Force aiming, and L1 NPC talk/interaction. Broader compatibility is not
claimed. Borderless has not been separately validated. DLSS/frame generation
were disabled during testing and are not dependencies.

Supported Steam executable **file version 1.0.0.3576**, SHA-256:
`c94ad6c3ea54970a6d2da7a2719753210a7728a91698ed3405dd9125470ed98f`.
This file version is not a claim of support for every advertised game patch.
The ASI refuses another executable hash, changed function bytes or occupied
required observer slots. Game updates need reviewed bindings and new playtests.

## Build the ASI

Use Windows x64, Visual Studio 2022 with Desktop development with C++, the x64
MSVC/MASM tools, Windows SDK, and CMake 3.24+. From a developer shell:

```powershell
cmake -S . -B out/build -G "Visual Studio 17 2022" -A x64
cmake --build out/build --config Release --parallel
ctest --test-dir out/build -C Release --output-on-failure
```

Output: `out/build/Release/GildedCompassHeading.asi`. The six tests cover
navigation, direction quantization, objective association, source freshness,
50,000 mixed-argument calls through five native thunks, and disabled ASI load.
They do not simulate the game or validate undocumented layouts on other builds.
No game, DMM, research checkout or network dependency is needed to compile.

The enabled configuration is in `config/GildedCompassHeading.ini`. It is not
automatically copied beside the build output, preserving disabled-load testing.
Building does not install or alter any game files. A newly compiled binary is
not promised to be byte-identical to the separately distributed playtested ASI.

## HUD source and local resource generation

`tools/prepare-direction-bar.py` contains the compass's original markup/CSS and
the native-shape woven ornament generator. It adds only custom elements and
preserves the original minimap HTML and CSS. Python 3 is required for this step.

The original game HUD is **not included**. Obtain the matching baseline HTML
and CSS from your own legal installation using an appropriate resource tool,
such as [CDKit](https://github.com/Moon-yungg/CDKit-Releases), then put these two
files into the ignored `local-game-ui/` directory:

```
local-game-ui/minimaphudview2.html
local-game-ui/minimaphudview2.css
```

Run `python tools/prepare-direction-bar.py`. Generated HTML and appended CSS
are under `out/native-heading/`. To compile with CDKit 0.4.1:

```powershell
cdkit-cli.exe ui compile out/native-heading/minimaphudview2.html out/native-heading/compiled --path UI/GameMain/Play/MinimapHudView2.html --templates binary
```

Use the generated game-panel binary and CSS as the HUD loose-file component.
Keep the original component binary from the matching game; it does not need
replacement. CDKit and extracted/compiled game resources are not redistributed
in this source repository. Baseline extraction is a separate local preparation
step, so this source checkout alone does not produce a complete DMM HUD package.

## Player installation

Players need the separately distributed DMM download, not this source checkout.
Extract that bundle and import both inner archives:

1. HUD ZIP through DMM's **Mods** tab.
2. ASI ZIP through **ASI Mods**, keeping its INI alongside the plugin.
3. Enable both, configure DMM's working ASI loader, mount/apply and launch normally.

DMM 4.7.4 was used for testing. Players need a compatible Microsoft Visual C++
x64 runtime, but do not need Visual Studio or CDKit. Disable older Gilded Compass
test entries and keep only one HUD and one plugin active. No loader is bundled.
Other mods replacing the same minimap panel/CSS may conflict; load order does
not merge full resource replacements. The runtime log is `GildedCompassHeading.log`
in the game's `bin64` directory; copy it before relaunching when reporting issues.

## Implementation

- `src/native/HeadingBridge.cpp`: guarded ASI startup, UI-thread class/text updates,
  bounded navigation reads and separate camera samples.
- `src/native/HeadingThunk.asm`: register-preserving observers forwarding original
  calls. There are no inline game-code patches or D3D/DXGI hooks.
- `include/GildedCompass/`: navigation association, quantization and source priority.
- `bindings/`: frozen executable hash, checked function bytes and surveyed RVAs.
- `tools/prepare-direction-bar.py`: rail, direction strip, ticks and filigree source.
- `tests/`: local regression and forwarding tests.

These are reverse-engineered build-specific observations, not official game APIs.
The renderer shows one unambiguous selected navigation target. It does not claim
all objectives or every simultaneous marker. Targets outside the 120-degree
bar are hidden until you turn toward them. Stale heading is not held indefinitely.

## Credits and permissions

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the full MIT notices in
`third_party/licenses/` for Glint Spotter (Seth) and Outfit Manager (Strahlimeier).
Thanks to Daebak and Dofo7777 for CDKit and the DMM contributors for mod management.
Skyrim inspired the compass format; no Skyrim assets are included. Crimson Desert
and its original content belong to Pearl Abyss and their respective owners.

No blanket license for the project's original contributions is selected here.
Public visibility permits viewing this source; it does not override copyright
or grant game-content redistribution permission. Third-party MIT terms remain
in force for their respective contributions. This is an unofficial fan project.

Code and UI were largely generated with Codex/AI and refined through hands-on
playtesting. AI concept art informed the design; shipped filigree uses native
UI geometry. No generated concept images are included in this repository.
