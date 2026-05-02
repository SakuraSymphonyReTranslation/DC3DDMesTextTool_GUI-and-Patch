# DC3DD Patch Build Notes

Patch ini ditargetkan untuk **D.C.III Dream Days (`DC3DD.EXE`)**.

## Output build default
- `DC3DDLauncher.exe`
- `DC3DDPatch.dll`
- Folder output: `build_dc3dd\`

## Build cepat (NMake)
```bat
build_dc3dd.bat
```

## Build manual
```bat
mkdir build_dc3dd
cd build_dc3dd
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
nmake
```

## Offset hook
- Offset default tersimpan di `DC3DDPatch.ini` bagian `[Offsets]`.
- Nilai referensi juga ada di `tools/dc3dd_offsets.txt`.
- Untuk verifikasi cepat terhadap `DC3DD.EXE`, jalankan:
```bat
python tools/get_addresses.py
```

## Catatan
- Launcher Windows otomatis mencoba Locale Emulator embedded (JP locale),
  lalu fallback ke mode biasa jika LE gagal load.
- Hook `CheckIcon`, `CheckIconConfig`, dan `BacklogIconHandler` sekarang membaca offset dari INI.
- Jika offset tidak valid (bukan executable), hook tersebut otomatis tidak dipasang agar patch tetap aman dijalankan.
