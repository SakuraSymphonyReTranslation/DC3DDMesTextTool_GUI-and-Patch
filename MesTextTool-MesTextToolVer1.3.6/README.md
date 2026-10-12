# MesTextTool — Da Capo 3 Dream Days

This bundled C++ reference is adapted from MesTextTool 1.3.6 by cokkeijigen. Its script table contains only **Da Capo 3 Dream Days** (`dc3dd`, version `0xA5A8`). The original license is retained in `LICENSE`.

For the supported JSON importer/exporter, use [DC3DD MES Text Tool](../README.md) and `../bin/Release/net8.0-windows/DC3DDMesTextTool_GUI.exe`.

The reference C++ tool uses its own text format and `.MesTextTool` configuration. Build it from the `MesTextTool` directory with CMake and Visual Studio. Export with:

```bat
MesTextTool.exe -log -cp932 -dc3dd "<GAME_DIR>\Translated\MES"
```

Replace `<GAME_DIR>` with the player's game installation folder (the folder containing `DC3DD.exe`). `Translated\MES` is a development corpus folder containing extracted original scripts; supply your own directory if different.

To import its text format, pass an exported text directory containing `.MesTextTool`. JSON conversion is provided by the GUI/CLI above.
