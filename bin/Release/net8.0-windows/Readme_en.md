# DC3DD MES Text Tool — GUI and CLI

[Bahasa Indonesia](README.md) | **English**

An importer/exporter for **D.C.III Dream Days**.

This distribution supports **Da Capo 3 Dream Days** only (`dc3dd`, MES version `0xA5A8`). The GUI, CLI, C++ reference tables, patch, and launcher are specific to this game. The main patch source is `DC3DDPatch a/src/dc3ddpatch.cpp`.

## Downloads and game folder

Download both packages from [GitHub Releases](https://github.com/SakuraSymphonyReTranslation/DC3DDMesTextTool_GUI-and-Patch/releases/latest):

- **DC3DDMesTextTool_GUI.rar**: MES ↔ JSON importer/exporter with a GUI and CLI. Includes Indonesian and English documentation.
- **Released_patch.rar**: launcher, patch DLL, INI configuration, and supporting runtime files.

`<GAME_DIR>` means **your game's installation folder**, which contains `DC3DD.exe` and `AdvData`. It is a documentation placeholder: replace it with your actual game folder, without the `<` and `>` characters. No particular drive or installation path is required.

1. Close the game and back up any files that will be replaced, especially the executable and `DC3DDPatch.ini`.
2. Extract the contents of `Released_patch.rar` directly into `<GAME_DIR>`, so that `DC3DDLauncher.exe`, `DC3DDPatch.dll`, and `DC3DDPatch.ini` are next to `DC3DD.exe`.
3. Run **`<GAME_DIR>\DC3DDLauncher.exe`** with `<GAME_DIR>` as the working directory. If you use a shortcut, set its **Start in** field to that folder.
4. Select the appropriate language in the patch settings. Place Indonesian MES files in `<GAME_DIR>\id_Data\MES`; for English, use `<GAME_DIR>\eng_data\MES`.

```text
<GAME_DIR>/
├── DC3DD.exe
├── DC3DDLauncher.exe
├── DC3DDPatch.dll
├── DC3DDPatch.ini
├── LEProc.exe
├── LoaderDll.dll
├── LocaleEmulator.dll
├── AdvData/
└── id_Data/
    └── MES/
```

The two archives provide the tool and patch runtime; they do not include the complete translated scripts. The repository's source folder can be placed anywhere. The patch package is also available under `DC3DDPatch a/build/Released` in the source tree.

## Opening the application

Extract `DC3DDMesTextTool_GUI.rar` to a folder of your choice and run `DC3DDMesTextTool_GUI.exe`. If you are using the source repository, run `bin\Release\net8.0-windows\DC3DDMesTextTool_GUI.exe`.
The ready-to-use package, `DC3DDMesTextTool_GUI.rar`, is in the same folder.
The application requires the .NET 8 Desktop Runtime.

## Single File

**MES → JSON (export)**

1. Set **Input MES/JSON** to the original MES file.
2. Set **Output**, or leave it empty to create the JSON file next to the input.
3. Click **Export (MES -> JSON)**.

The default output combines character names and dialogue according to the DC3DD opcodes:

```json
[
  { "message": "Narration." },
  { "name": "Kousuke", "message": "「Character dialogue.」" }
]
```

Both the Single File and Batch tabs always export the combined name/dialogue format shown above. Export preserves text, Japanese/Indonesian names, and line breaks. It does not translate names.

**JSON → MES (import)**

1. Set **Input MES/JSON** to the translated JSON file.
2. Select the original MES file for the same scenario in **Original MES**. This file can be in another folder. If the field is empty, the application looks for an MES file with the same base name next to the JSON file.
3. Choose an **Output** file different from the original MES file.
4. Set **Word Wrap**: the default is `46` in Single File, Batch, and the CLI; `0` preserves text and line breaks without reflowing them.
5. Click **Import (JSON -> MES)**.

The importer accepts combined JSON, legacy JSON, and a mixture of both. The number of text tokens must match the original MES file. Each `name` field must match the name/dialogue positions in the MES file. The original template is required because JSON does not contain all instructions, labels, assets, or scenario flow.

## Batch

- **Export:** set **MES Folder** and **Output Folder**, then click **Batch Export**.
- **Import:** set **JSON Folder**, the original **MES Folder**, and a separate **Output Folder**, then click **Batch Import**.
- File base names must match: `scene.json` uses `scene.mes` as its template.
- Failed files are reported in the log; the batch continues with the remaining files. Missing templates count as failures.
- Buttons are disabled during conversion to prevent simultaneous writes from multiple operations.

## Layout and backlog icons

Word wrapping applies only to dialogue/narration in opcode `0x65`. Character names (`0x64`), choices, control sequences, and ruby text are not split like ordinary dialogue. Names are limited to 31 CP932 bytes and one line. Translated names are preserved so that the patch DLL's lookup of 28 aliases continues to work.

Wrapping counts ASCII characters and half-width katakana as one unit, and full-width characters as two units. Explicit line breaks are preserved. Words longer than the limit are split. Messages exceeding four lines are reported in the log; font size and panel layout still need to be checked in the game.

**`DC3DDPatch.dll` handles vertical spacing and icon selection at runtime.** The importer does not insert icon images or font settings into JSON/MES files. To play imported scripts with the patch, start the game through `DC3DDLauncher.exe` from `<GAME_DIR>`. See the [English patch README](https://github.com/SakuraSymphonyReTranslation/DC3DDMesTextTool_GUI-and-Patch/blob/main/DC3DDPatch%20a/Readme_en.md) for technical details.

The patch package uses `DialogueLineSpacing=-10`, `BacklogLineSpacing=-10`, and `ShowBacklogIcon=1`. The fixes support 28 Indonesian name aliases and normal four-line positions of `[0, 24, 48, 72]` pixels. Wrap `46` controls text width during import; DLL spacing controls vertical positions while the game is running. Wrapping does not guarantee that every message will fit within four lines. Older scripts that have not been reimported may still extend past the right edge of the backlog.

## Validation and encoding

- MES labels are calculated relative to the start of the version field, including the 3-byte prefix; label flags and other tables are preserved.
- Malformed input, mismatched entry counts, overlong names, NUL characters, and characters that cannot be encoded are rejected before the output is replaced.
- Empty native text entries remain in the template and are not exported, matching the legacy JSON format. Nonempty messages must not be emptied during import.
- CP932 does not support some characters used in the project's translations. The following explicit substitutions are logged: `— → ―`, `û → u`, `ë → e`, `é → e`, `à → a`. Other unsupported characters cause an error instead of silently becoming `?`.
- Files are written through a temporary file after validation. The input or template must not also be the output.

## CLI

From the executable's folder:

```powershell
.\DC3DDMesTextTool_GUI.exe export "scene.mes" "scene.json"
.\DC3DDMesTextTool_GUI.exe export "scene.mes" "legacy.json" --flat
.\DC3DDMesTextTool_GUI.exe import "scene.json" "original\scene.mes" "new\scene.mes" -w 46
.\DC3DDMesTextTool_GUI.exe import "scene.json" "original\scene.mes" "roundtrip\scene.mes" --no-wrap
.\DC3DDMesTextTool_GUI.exe batch-export "original" "json"
.\DC3DDMesTextTool_GUI.exe batch-import "json" "original" "mes_output" -w 46
```

`-w 0` is equivalent to `--no-wrap`. When wrapping is enabled, the width range is 10–1000. `--flat` is available for both export commands. Exit code 0 means all conversions succeeded; 1 means a failure occurred or a template was missing. Use `--help` for the command list.

## Verification results

- 214 original MES files: export → import without wrapping produced byte-identical files.
- 190 of 194 Indonesian JSON files were successfully imported and exported again; text, names, non-text opcodes, labels, and flags were checked.
- Four source JSON files were rejected: two contained names exceeding 31 bytes, and two had mismatched name/dialogue positions.
- Tests covered wrapping, control sequences, ruby text, legacy formats, malformed input, encoding, CLI options, and missing templates.
- The patch DLL passed 444 hook emulation cases: 56 line-spacing, 96 context, and 292 icon cases.

## Building and testing

From the project's source folder:

```powershell
dotnet build -c Release
python -X utf8 tools\verify_converter.py "<GAME_DIR>" "<TRANSLATED_JSON_DIR>" "<VALIDATION_OUTPUT_DIR>"
```

Replace the placeholders with your own folders before running the commands. Building the GUI requires the .NET 8 SDK. The verification script uses the development corpus under `<GAME_DIR>\Translated\MES` and translated JSON files, and checks the four rejected JSON cases. This corpus is not included in the repository; adjust the expected cases if you use different or corrected source files. Use a separate test output folder, away from the game's active scripts.

See the [English patch README](https://github.com/SakuraSymphonyReTranslation/DC3DDMesTextTool_GUI-and-Patch/blob/main/DC3DDPatch%20a/Readme_en.md) for patch DLL and launcher build instructions.
