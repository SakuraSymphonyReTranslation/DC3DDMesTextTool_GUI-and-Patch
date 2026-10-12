# DC3DD Patch — instalasi dan build

**Bahasa Indonesia** | [English](Readme_en.md)

Patch dan launcher ini hanya mendukung **Da Capo 3 Dream Days**. Source utama: `src/dc3ddpatch.cpp`.

## Instalasi

Unduh `Released_patch.rar` dari [Releases](https://github.com/SakuraSymphonyReTranslation/DC3DDMesTextTool_GUI-and-Patch/releases/latest), atau gunakan paket di `build/Released`.

`<GAME_DIR>` berarti folder instalasi pilihan pemain yang berisi `DC3DD.exe` dan `AdvData`. Ganti placeholder ini dengan lokasi game Anda; tidak ada drive yang diwajibkan.

Tutup game, cadangkan berkas yang akan diganti, lalu ekstrak isi paket ke `<GAME_DIR>`. Jalankan **`<GAME_DIR>\DC3DDLauncher.exe`** dengan working directory `<GAME_DIR>`; shortcut harus memakai folder itu pada **Start in**. Launcher memuat DLL patch dan mencoba Locale Emulator JP, dengan fallback ke mode biasa jika pemuatan LE gagal.

Paket berisi `DC3DD.exe`, `DC3DDLauncher.exe`, `DC3DDPatch.dll`, `DC3DDPatch.ini`, `LEProc.exe`, `LoaderDll.dll`, dan `LocaleEmulator.dll`. Naskah terjemahan dipasang terpisah ke `id_Data\MES` atau `eng_data\MES` di dalam `<GAME_DIR>` sesuai pilihan bahasa.

Executable referensi yang diuji memiliki SHA256:

```text
d2a26be40e50b54b465bb8f907e0751969d057e7b2c626c710ac8c8977733384
```

## Jarak baris dan ikon backlog

- `DialogueLineSpacing=-10` dan `BacklogLineSpacing=-10` menyesuaikan posisi baris engine. Tinggi tekstur huruf tetap utuh.
- `AdvancedSettings=1`, `DisableBacklogSpacing=0`, dan `ShowBacklogIcon=1` mengaktifkan konfigurasi paket.
- Uji pitch normal menghasilkan 24 px, sehingga empat baris berada pada `[0, 24, 48, 72]` px. Kasus font lebih besar menghasilkan pitch 34 px.
- Lookup ikon memetakan 28 alias nama Indonesia ke nama Jepang dan memanggil lookup native agar varian skenario tetap berlaku. Nama tanpa ikon native memakai fallback teks.

Alias yang didukung: Aisia, Akane, Anzu, Aoi, Charles, Himeno, Jill, Kiyokaka, Kiyota, Kiyotaka, Koko, Kousuke, Mika, Mikoto, Minatsu, Nanaka, Otome, Rikka, Sakura, Sara, Shiki, Suginami, Sumomo, Youko, Yume, Yuuhi, Yuuki, Yuzu.

Word wrap dilakukan oleh importer MES, dengan default **46**. DLL mengatur jarak vertikal dan ikon, sehingga script lama yang belum diimpor ulang masih dapat melewati batas kanan backlog. Pesan lebih dari empat baris perlu ditinjau di game. Lihat [petunjuk tool](../README.md) untuk proses impor.

## Build (Visual Studio 2022, Win32)

Dari folder `DC3DDPatch a`, jalankan `build_dc3dd.bat`, atau:

```powershell
cmake -S . -B build_dc3dd -G "Visual Studio 17 2022" -A Win32
cmake --build build_dc3dd --config Release --target DC3DDPatch DC3DDLauncher
```

Build manual menghasilkan `build_dc3dd/Release/DC3DDPatch.dll` dan `DC3DDLauncher.exe`. Skrip BAT juga menyalin keduanya dan library LE ke `build_dc3dd`. Gunakan INI dan library runtime dari `build/Released` saat memasang build baru. Cache CMake dan berkas perantara dibuat lokal saat build.

## Hook dan verifikasi

`src/dc3dd_layout.h` berisi hook jarak baris dan lookup ikon; `src/dc3dd_backlog.h` berisi alias nama. Semua lokasi patch dan signature fungsi diperiksa sebelum pemasangan. Daftar alamat tersedia di `tools/dc3dd_offsets.txt`. Bagian `[Offsets]` dalam INI dipertahankan untuk kompatibilitas dan tidak dipakai hook layout/ikon ini.

DLL paket lulus 444 kasus emulasi hook: 56 jarak baris, 96 wrapper/konteks, dan 292 ikon. Pemeriksaan visual sebelumnya mengonfirmasi contoh dialog Indonesia empat baris dan ikon backlog, belum setiap rute/alias.

Untuk menjalankan verifier dari folder `DC3DDPatch a`:

```powershell
python -X utf8 tools/verify_dc3dd_layout.py "<GAME_DIR>\DC3DD.exe" "build_dc3dd\Release\DC3DDPatch.dll" "<AUDIT_DIR>"
```

Ganti placeholder dengan folder Anda. Verifier membutuhkan Python dengan `pefile`, `capstone`, dan `unicorn`, file `.map` dari build DLL yang sama, serta `name_audit.json` dan `supported_names.json` pada `<AUDIT_DIR>`. Data audit korpus pengembangan tidak disertakan; perintah ini membutuhkan data tersebut sebelum dapat dijalankan. Hasil ditulis ke `<AUDIT_DIR>/verification.json`.
