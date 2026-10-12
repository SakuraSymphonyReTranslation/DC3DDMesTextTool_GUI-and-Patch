#pragma once

namespace dc3dd_backlog {
// Names aligned with 190 DC3DD Japanese/Indonesian scripts; see name_audit.json.
// Keep the native lookup (including scenario-dependent Otome) responsible for atlas indices.
struct Name { const char *translated; const char *japanese; };
inline constexpr Name names[] = {
    {"Aisia", "\x83\x41\x83\x43\x83\x56\x83\x41"}, // アイシア
    {"Akane", "\x88\xa9"}, // 茜
    {"Anzu", "\x88\xc7"}, // 杏
    {"Aoi", "\x88\xa8"}, // 葵
    {"Charles", "\x83\x56\x83\x83\x83\x8b\x83\x8b"}, // シャルル
    {"Himeno", "\x95\x50\x94\x54"}, // 姫乃
    {"Jill", "\x83\x57\x83\x8b"}, // ジル
    {"Kiyokaka", "\x90\xb4\x97\xb2"}, // 清隆
    {"Kiyota", "\x90\xb4\x97\xb2"}, // 清隆
    {"Kiyotaka", "\x90\xb4\x97\xb2"}, // 清隆
    {"Koko", "\x8f\xac\x97\xf6"}, // 小恋
    {"Kousuke", "\x8d\x6b\x8f\x95"}, // 耕助
    {"Mika", "\x94\xfc\x89\xc4"}, // 美夏
    {"Mikoto", "\x94\xfc\x8b\xd5"}, // 美琴
    {"Minatsu", "\x94\xfc\x89\xc4"}, // 美夏
    {"Nanaka", "\x82\xc8\x82\xc8\x82\xa9"}, // ななか
    {"Otome", "\x89\xb9\x95\x50"}, // 音姫
    {"Rikka", "\x97\xa7\x89\xc4"}, // 立夏
    {"Sakura", "\x82\xb3\x82\xad\x82\xe7"}, // さくら
    {"Sara", "\x82\xb3\x82\xe7"}, // さら
    {"Shiki", "\x8e\x6c\x8b\x47"}, // 四季
    {"Suginami", "\x90\x99\x95\xc0"}, // 杉並
    {"Sumomo", "\x82\xb7\x82\xe0\x82\xe0"}, // すもも
    {"Youko", "\x97\x7a\x8e\x71"}, // 陽子
    {"Yume", "\x97\x52\x96\xb2"}, // 由夢
    {"Yuuhi", "\x97\x5b\x97\x7a"}, // 夕陽
    {"Yuuki", "\x8d\xf7\x95\x50"}, // 桜姫
    {"Yuzu", "\x82\xe4\x82\xb8"}, // ゆず
};

inline const char *ResolveName(const char *name) {
  if (!name || !*name) return nullptr;
  for (const auto &entry : names) {
    unsigned int i = 0;
    for (; i < 32 && name[i] && entry.translated[i]; ++i) {
      if ((name[i] == '_' ? ' ' : name[i]) != entry.translated[i]) break;
    }
    if (i < 32 && name[i] == '\0' && entry.translated[i] == '\0')
      return entry.japanese;
  }
  return nullptr;
}
} // namespace dc3dd_backlog
