# DC3DD MES Text Tool — GUI dan CLI

**Bahasa Indonesia** | [English](Readme_en.md)

Importer/exporter untuk **D.C.III Dream Days**.

Distribusi ini hanya mendukung **Da Capo 3 Dream Days** (`dc3dd`, versi MES `0xA5A8`). GUI, CLI, tabel referensi C++, patch, dan launcher dikhususkan untuk game ini. Source utama patch: `DC3DDPatch a/src/dc3ddpatch.cpp`.

## Unduhan dan folder instalasi

Unduh kedua paket terbaru dari [GitHub Releases](https://github.com/SakuraSymphonyReTranslation/DC3DDMesTextTool_GUI-and-Patch/releases/latest):

- **DC3DDMesTextTool_GUI.rar**: importer/exporter MES ↔ JSON, GUI dan CLI. Termasuk dokumentasi bahasa Indonesia dan Inggris.
- **Released_patch.rar**: launcher, DLL patch, INI, dan berkas pendukung runtime.

`<GAME_DIR>` berarti **folder utama instalasi game pilihan pemain**, yaitu folder yang berisi `DC3DD.exe` dan `AdvData`. Ini placeholder dokumentasi: ganti dengan lokasi game Anda, tanpa tanda `<` dan `>`. Tidak ada drive atau lokasi instalasi wajib.

1. Tutup game dan simpan salinan berkas lama yang akan diganti, terutama executable dan `DC3DDPatch.ini`.
2. Ekstrak isi `Released_patch.rar` langsung ke `<GAME_DIR>`, sehingga `DC3DDLauncher.exe`, `DC3DDPatch.dll`, dan `DC3DDPatch.ini` berada di sebelah `DC3DD.exe`.
3. Jalankan **`<GAME_DIR>\DC3DDLauncher.exe`** dengan working directory `<GAME_DIR>`. Jika memakai shortcut, isi **Start in** dengan folder tersebut.
4. Pilih bahasa yang sesuai pada pengaturan patch. Berkas MES Indonesia ditempatkan di `<GAME_DIR>\id_Data\MES`; untuk bahasa Inggris, gunakan `<GAME_DIR>\eng_data\MES`.

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

Kedua RAR menyediakan tool dan runtime patch; keduanya tidak berisi seluruh naskah terjemahan. Folder source repositori boleh ditempatkan di mana saja. Paket patch juga tersedia di `DC3DDPatch a/build/Released` pada source.

## Buka aplikasi

Ekstrak `DC3DDMesTextTool_GUI.rar` ke folder pilihan Anda dan jalankan `DC3DDMesTextTool_GUI.exe`. Jika memakai source repositori, jalankan `bin\Release\net8.0-windows\DC3DDMesTextTool_GUI.exe`.
Paket siap pakai berada di folder yang sama: `DC3DDMesTextTool_GUI.rar`.
Aplikasi membutuhkan .NET 8 Desktop Runtime.

## Single File

**MES → JSON (exporter)**

1. Isi **Input MES/JSON** dengan MES asli.
2. Isi **Output**, atau kosongkan untuk membuat JSON di sebelah input.
3. Klik **Export (MES -> JSON)**.

Hasil default menggabungkan nama karakter dan dialog berdasarkan opcode DC3DD:

```json
[
  { "message": "Narasi." },
  { "name": "Kousuke", "message": "「Dialog karakter.」" }
]
```

Ekspor dari tab Single File dan Batch selalu menggunakan format gabungan nama/dialog di atas. Teks, nama Jepang/Indonesia, dan pemisah baris dipertahankan saat ekspor. Ekspor tidak menerjemahkan nama.

**JSON → MES (importer)**

1. Isi **Input MES/JSON** dengan JSON terjemahan.
2. Pilih **Original MES** dengan MES asli untuk skenario yang sama. File ini boleh berada di folder lain. Jika kolom kosong, aplikasi mencoba MES bernama sama di sebelah JSON.
3. Tentukan file **Output** yang berbeda dari MES asli.
4. Pilih **Word Wrap**: default `46` pada Single File, Batch, dan CLI; `0` mempertahankan teks/pemisah baris tanpa reflow.
5. Klik **Import (JSON -> MES)**.

Importer menerima JSON gabungan, JSON lama, serta campuran keduanya. Jumlah token teks harus sama dengan MES asli. Field `name` harus cocok dengan posisi nama/dialog pada MES. Template asli dibutuhkan karena JSON tidak memuat seluruh instruksi, label, aset, dan alur skenario.

## Batch

- **Export:** isi **MES Folder** dan **Output Folder**, lalu klik **Batch Export**.
- **Import:** isi **JSON Folder**, **MES Folder** asli, dan **Output Folder** yang berbeda, lalu klik **Batch Import**.
- Nama file harus sama: `scene.json` memakai template `scene.mes`.
- File gagal ditampilkan di log; batch tetap memproses file lain. Template yang hilang dihitung sebagai kegagalan.
- Tombol dinonaktifkan selama konversi untuk mencegah dua pekerjaan menulis bersamaan.

## Layout dan ikon backlog

Word wrap hanya diterapkan pada isi dialog/narasi opcode `0x65`. Nama karakter `0x64`, pilihan, kontrol, dan ruby tidak dipotong seperti dialog biasa. Nama dibatasi 31 byte CP932 dan satu baris. Nama terjemahan dipertahankan agar lookup 28 alias dalam patch DLL tetap bekerja.

Wrap memakai satu unit untuk huruf ASCII/katakana setengah lebar dan dua unit untuk karakter lebar penuh. Pemisah baris eksplisit dipertahankan. Kata yang lebih panjang dari batas dipotong. Pesan lebih dari empat baris dicatat di log; ukuran font dan panel tetap perlu diperiksa di game.

**Jarak vertikal dan pemilihan gambar ikon ditangani `DC3DDPatch.dll` saat runtime.** Importer tidak menyisipkan gambar ikon atau pengaturan font ke JSON/MES. Untuk memainkan hasil impor dengan patch, buka game melalui `DC3DDLauncher.exe` dari `<GAME_DIR>`. Petunjuk teknis tersedia di [README patch](https://github.com/SakuraSymphonyReTranslation/DC3DDMesTextTool_GUI-and-Patch/blob/main/DC3DDPatch%20a/README.md).

Paket patch memakai `DialogueLineSpacing=-10`, `BacklogLineSpacing=-10`, dan `ShowBacklogIcon=1`. Perbaikan mendukung 28 alias nama Indonesia dan jarak empat baris normal `[0, 24, 48, 72]` piksel. Wrap `46` mengatur lebar teks saat impor; spacing DLL mengatur jarak vertikal saat game berjalan. Wrap tidak menjamin semua pesan muat dalam empat baris. Teks lama yang belum diimpor ulang masih dapat melewati batas kanan backlog.

## Validasi dan encoding

- Label MES dihitung relatif terhadap awal versi, termasuk prefix 3 byte; flag label dan tabel lain dipertahankan.
- Input rusak, jumlah entri tidak cocok, nama terlalu panjang, NUL, dan karakter yang tidak bisa dienkode ditolak sebelum output diganti.
- Entri teks native yang kosong tetap disimpan dalam template dan tidak diekspor, sesuai format JSON lama. Pesan nonkosong tidak boleh dikosongkan saat impor.
- CP932 tidak mendukung beberapa karakter di terjemahan proyek. Penggantian eksplisit yang dicatat di log: `— → ―`, `û → u`, `ë → e`, `é → e`, `à → a`. Karakter lain yang tidak didukung menghasilkan error, bukan penggantian diam-diam menjadi `?`.
- File ditulis melalui file sementara setelah validasi. Input/template tidak boleh menjadi output.

## CLI

Dari folder executable:

```powershell
.\DC3DDMesTextTool_GUI.exe export "scene.mes" "scene.json"
.\DC3DDMesTextTool_GUI.exe export "scene.mes" "legacy.json" --flat
.\DC3DDMesTextTool_GUI.exe import "scene.json" "original\scene.mes" "new\scene.mes" -w 46
.\DC3DDMesTextTool_GUI.exe import "scene.json" "original\scene.mes" "roundtrip\scene.mes" --no-wrap
.\DC3DDMesTextTool_GUI.exe batch-export "original" "json"
.\DC3DDMesTextTool_GUI.exe batch-import "json" "original" "mes_output" -w 46
```

`-w 0` setara `--no-wrap`. Lebar aktif: 10–1000. `--flat` tersedia untuk kedua perintah ekspor. Exit code 0 berarti seluruh konversi berhasil; 1 berarti gagal atau ada template yang hilang. Gunakan `--help` untuk daftar perintah.

## Hasil verifikasi

- 214 MES asli: ekspor → impor tanpa wrap menghasilkan byte yang identik.
- 190 dari 194 JSON Indonesia berhasil diimpor dan diekspor kembali; teks, nama, opcode nonteks, label dan flag diperiksa.
- Empat JSON sumber ditolak: dua memuat nama melebihi 31 byte; dua memiliki posisi nama/dialog yang tidak cocok.
- Kasus wrap, kontrol, ruby, format lama, input rusak, encoding, opsi CLI, dan template hilang diuji.

- DLL patch lulus 444 kasus emulasi hook: 56 jarak baris, 96 konteks, dan 292 ikon.

## Build dan uji

Dari folder source proyek:

```powershell
dotnet build -c Release
python -X utf8 tools\verify_converter.py "<GAME_DIR>" "<TRANSLATED_JSON_DIR>" "<VALIDATION_OUTPUT_DIR>"
```

Ganti placeholder dengan folder Anda sebelum menjalankan perintah. Build GUI membutuhkan .NET 8 SDK. Skrip verifikasi memakai korpus pengembangan di `<GAME_DIR>\Translated\MES` dan JSON terjemahan, serta memeriksa empat kasus JSON yang ditolak. Korpus tersebut tidak disertakan dalam repositori; sesuaikan ekspektasi kasus bila memakai sumber yang berbeda atau sudah diperbaiki. Gunakan folder output pengujian yang terpisah dari script aktif game.

Build patch DLL/launcher memiliki petunjuk terpisah di [README patch](https://github.com/SakuraSymphonyReTranslation/DC3DDMesTextTool_GUI-and-Patch/blob/main/DC3DDPatch%20a/README.md).
