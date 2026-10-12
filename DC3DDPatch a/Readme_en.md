# DC3DD Patch — installation and build

[Bahasa Indonesia](README.md) | **English**

This patch and launcher support **Da Capo 3 Dream Days** only. The main source file is `src/dc3ddpatch.cpp`.

## Installation

Download `Released_patch.rar` from [Releases](https://github.com/SakuraSymphonyReTranslation/DC3DDMesTextTool_GUI-and-Patch/releases/latest), or use the package in `build/Released`.

`<GAME_DIR>` means your game's installation folder, containing `DC3DD.exe` and `AdvData`. Replace this placeholder with your actual game folder; no particular drive is required.

Close the game, back up any files that will be replaced, and extract the package contents into `<GAME_DIR>`. Run **`<GAME_DIR>\DC3DDLauncher.exe`** with `<GAME_DIR>` as the working directory; shortcuts must use that folder in **Start in**. The launcher loads the patch DLL and attempts to use Locale Emulator with a Japanese locale, falling back to normal launch if LE fails to load.

The package contains `DC3DD.exe`, `DC3DDLauncher.exe`, `DC3DDPatch.dll`, `DC3DDPatch.ini`, `LEProc.exe`, `LoaderDll.dll`, and `LocaleEmulator.dll`. Install translated scripts separately under `id_Data\MES` or `eng_data\MES` within `<GAME_DIR>`, according to the language selected in the patch settings.

The tested reference executable has this SHA256:

```text
d2a26be40e50b54b465bb8f907e0751969d057e7b2c626c710ac8c8977733384
```

## Line spacing and backlog icons

- `DialogueLineSpacing=-10` and `BacklogLineSpacing=-10` adjust the engine's line positions. Character texture height is preserved.
- `AdvancedSettings=1`, `DisableBacklogSpacing=0`, and `ShowBacklogIcon=1` enable the package's settings.
- Normal line-pitch tests produce 24 px, placing four lines at `[0, 24, 48, 72]` px. Larger-font cases produce a 34 px pitch.
- Icon lookup maps 28 Indonesian name aliases to Japanese names and calls the native lookup, preserving scenario-specific icon variants. Names without a native icon fall back to text.

Supported aliases: Aisia, Akane, Anzu, Aoi, Charles, Himeno, Jill, Kiyokaka, Kiyota, Kiyotaka, Koko, Kousuke, Mika, Mikoto, Minatsu, Nanaka, Otome, Rikka, Sakura, Sara, Shiki, Suginami, Sumomo, Youko, Yume, Yuuhi, Yuuki, Yuzu.

The MES importer handles word wrapping, with a default of **46**. The DLL controls vertical spacing and icons, so older scripts that have not been reimported may still extend past the right edge of the backlog. Messages exceeding four lines need to be reviewed in the game. See the [English tool guide](../Readme_en.md) for the import workflow.

## Build (Visual Studio 2022, Win32)

From the `DC3DDPatch a` folder, run `build_dc3dd.bat`, or:

```powershell
cmake -S . -B build_dc3dd -G "Visual Studio 17 2022" -A Win32
cmake --build build_dc3dd --config Release --target DC3DDPatch DC3DDLauncher
```

A manual build produces `build_dc3dd/Release/DC3DDPatch.dll` and `DC3DDLauncher.exe`. The BAT script also copies both files and the LE libraries to `build_dc3dd`. Use the INI and runtime libraries from `build/Released` when installing a new build. CMake caches and intermediate files are generated locally during the build.

## Hooks and verification

`src/dc3dd_layout.h` contains the line-spacing and icon-lookup hooks; `src/dc3dd_backlog.h` contains the name aliases. All patch locations and function signatures are checked before the hooks are installed. The address list is in `tools/dc3dd_offsets.txt`. The INI's `[Offsets]` section is retained for compatibility and is not used by these layout/icon hooks.

The packaged DLL passed 444 hook emulation cases: 56 line-spacing, 96 wrapper/context, and 292 icon cases. Earlier visual checks confirmed sample four-line Indonesian dialogue and backlog icons, but did not cover every route or alias.

To run the verifier from the `DC3DDPatch a` folder:

```powershell
python -X utf8 tools/verify_dc3dd_layout.py "<GAME_DIR>\DC3DD.exe" "build_dc3dd\Release\DC3DDPatch.dll" "<AUDIT_DIR>"
```

Replace the placeholders with your own folders. The verifier requires Python with `pefile`, `capstone`, and `unicorn`, the `.map` file from the same DLL build, and `name_audit.json` and `supported_names.json` in `<AUDIT_DIR>`. The development corpus audit data is not included; this command requires those files before it can run. Results are written to `<AUDIT_DIR>/verification.json`.
