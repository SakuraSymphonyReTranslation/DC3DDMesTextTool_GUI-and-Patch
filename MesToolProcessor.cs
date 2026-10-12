using System.Text;
using System.Text.Json;

namespace DC3DDMesTextTool;

public class MesToolProcessor
{
    private static readonly Encoding ShiftJIS;
    private static readonly JsonSerializerOptions JsonOptions = new() {
        WriteIndented = true,
        Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping
    };
    static MesToolProcessor()
    {
        Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
        ShiftJIS = Encoding.GetEncoding(932, EncoderFallback.ExceptionFallback, DecoderFallback.ExceptionFallback);
    }

    public event Action<string>? Log;
    private void LogMessage(string message) => Log?.Invoke(message);
    private void LogError(string message) => LogMessage("Error: " + message);

    public int ExportMesToJson(string inputPath, string outputPath, bool flat = false)
    {
        try
        {
            EnsureDifferent(outputPath, inputPath);
            var script = new MesScript();
            script.Load(inputPath);
            var entries = script.ExtractText(ShiftJIS);
            var rows = new List<TextEntry>();
            for (int i = 0; i < entries.Count; i++)
            {
                var entry = entries[i];
                if (!flat && script.Info.Name == "dc3dd" && entry.Opcode == 0x64 &&
                    i + 1 < entries.Count && entries[i + 1].Opcode == 0x65)
                    rows.Add(new TextEntry { Name = entry.Message, Message = entries[++i].Message });
                else rows.Add(new TextEntry { Message = entry.Message });
            }
            WriteAtomic(outputPath, new UTF8Encoding(false).GetBytes(JsonSerializer.Serialize(rows, JsonOptions)));
            LogMessage($"Exported {Path.GetFileName(inputPath)}: {rows.Count} JSON entries ({entries.Count} MES text tokens).");
            return 0;
        }
        catch (Exception ex) { LogError($"Export {inputPath}: {ex.Message}"); return 1; }
    }

    public int ImportJsonToMes(string jsonPath, string originalMesPath, string outputPath, int wrapWidth = TextFormatter.DefaultLineLength)
    {
        try
        {
            EnsureDifferent(outputPath, originalMesPath, jsonPath);
            if (wrapWidth != 0 && (wrapWidth < 10 || wrapWidth > 1000))
                throw new ArgumentException("Wrap width must be 0 (preserve line breaks), or 10..1000.");
            var script = new MesScript();
            script.Load(originalMesPath);
            var originals = script.ExtractText(ShiftJIS);
            using var document = JsonDocument.Parse(File.ReadAllText(jsonPath, Encoding.UTF8));
            if (document.RootElement.ValueKind != JsonValueKind.Array)
                throw new InvalidDataException("JSON root must be an array.");
            var expanded = new List<TextEntry>();
            var substitutions = new SortedSet<string>();
            int rowNumber = 0;
            foreach (var row in document.RootElement.EnumerateArray())
            {
                rowNumber++;
                if (row.ValueKind != JsonValueKind.Object ||
                    !row.TryGetProperty("message", out var message) || message.ValueKind != JsonValueKind.String)
                    throw new InvalidDataException($"JSON entry {rowNumber}: message must be a string.");
                if (row.TryGetProperty("name", out var name) && name.ValueKind != JsonValueKind.Null)
                {
                    if (name.ValueKind != JsonValueKind.String)
                        throw new InvalidDataException($"JSON entry {rowNumber}: name must be a string or null.");
                    if (!string.IsNullOrEmpty(name.GetString()))
                    {
                        int at = expanded.Count;
                        if (script.Info.Name == "dc3dd" &&
                            (at + 1 >= originals.Count || originals[at].Opcode != 0x64 || originals[at + 1].Opcode != 0x65))
                            throw new InvalidDataException($"JSON entry {rowNumber}: name does not match a MES speaker/dialogue pair.");
                        expanded.Add(new TextEntry { Message = Normalize(name.GetString()!, substitutions) });
                    }
                }
                expanded.Add(new TextEntry { Message = Normalize(message.GetString()!, substitutions) });
            }
            TextFormatter? formatter = wrapWidth == 0 ? null : new TextFormatter { MaxLineLength = wrapWidth };
            byte[] rebuilt = script.ImportText(expanded, ShiftJIS, formatter);
            // Parsing the rebuilt file catches malformed token boundaries before it is published.
            var check = new MesScript();
            check.Load(rebuilt, script.Info);
            if (check.Tokens.Count != script.Tokens.Count || check.ExtractText(ShiftJIS).Count != originals.Count)
                throw new InvalidDataException("Rebuilt MES failed token/text count verification.");
            WriteAtomic(outputPath, rebuilt);
            int longMessages = check.ExtractText(ShiftJIS).Count(e => e.Opcode == 0x65 && e.Message.Count(c => c == '\n') >= 4);
            if (longMessages > 0)
                LogMessage($"Layout note in {Path.GetFileName(jsonPath)}: {longMessages} messages exceed four lines; review them in game or adjust wrap width.");
            if (substitutions.Count > 0)
                LogMessage($"CP932 substitutions in {Path.GetFileName(jsonPath)}: {string.Join(", ", substitutions)}");
            LogMessage($"Imported {Path.GetFileName(jsonPath)}: {rowNumber} JSON entries -> {expanded.Count} MES text tokens.");
            return 0;
        }
        catch (Exception ex) { LogError($"Import {jsonPath}: {ex.Message}"); return 1; }
    }

    private static string Normalize(string text, ISet<string> substitutions)
    {
        // Explicit substitutions for characters present in this project's Indonesian JSON.
        // All other unsupported characters produce an error, never a silent '?'.
        var result = new StringBuilder(text.Length);
        foreach (char c in text)
        {
            char replacement = c switch { '—' => '―', 'û' => 'u', 'ë' => 'e', 'é' => 'e', 'à' => 'a', _ => c };
            if (replacement != c) substitutions.Add($"{c} → {replacement}");
            result.Append(replacement);
        }
        return result.ToString();
    }

    private static void EnsureDifferent(string output, params string[] inputs)
    {
        if (inputs.Any(input => string.Equals(Path.GetFullPath(output), Path.GetFullPath(input), StringComparison.OrdinalIgnoreCase)))
            throw new IOException("Output must be different from the input/template file.");
    }

    private static void WriteAtomic(string path, byte[] bytes)
    {
        string fullPath = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
        string temporary = fullPath + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try {
            File.WriteAllBytes(temporary, bytes);
            File.Move(temporary, fullPath, true);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }

    public int BatchExport(string inputFolder, string outputFolder, bool flat = false)
    {
        if (!Directory.Exists(inputFolder)) { LogError($"MES folder not found: {inputFolder}"); return 1; }
        var files = Directory.GetFiles(inputFolder, "*.mes").OrderBy(x => x).ToArray();
        if (files.Length == 0) { LogError("No MES files found."); return 1; }
        int failed = 0;
        foreach (var file in files)
            failed += ExportMesToJson(file, Path.Combine(outputFolder, Path.GetFileNameWithoutExtension(file) + ".json"), flat);
        LogMessage($"Batch export: {files.Length - failed} succeeded, {failed} failed.");
        return failed == 0 ? 0 : 1;
    }

    public int BatchImport(string jsonFolder, string mesFolder, string outputFolder, int wrapWidth = TextFormatter.DefaultLineLength)
    {
        if (!Directory.Exists(jsonFolder) || !Directory.Exists(mesFolder)) {
            LogError("JSON and original MES folders must exist."); return 1;
        }
        if (string.Equals(Path.GetFullPath(mesFolder).TrimEnd('\\', '/'),
                          Path.GetFullPath(outputFolder).TrimEnd('\\', '/'), StringComparison.OrdinalIgnoreCase)) {
            LogError("Output folder must differ from the original MES folder."); return 1;
        }
        var files = Directory.GetFiles(jsonFolder, "*.json").OrderBy(x => x).ToArray();
        if (files.Length == 0) { LogError("No JSON files found."); return 1; }
        int failed = 0, missing = 0;
        foreach (var file in files)
        {
            string filename = Path.GetFileNameWithoutExtension(file) + ".mes";
            string template = Path.Combine(mesFolder, filename);
            if (!File.Exists(template)) { LogError($"Missing original MES: {filename}"); missing++; continue; }
            failed += ImportJsonToMes(file, template, Path.Combine(outputFolder, filename), wrapWidth);
        }
        LogMessage($"Batch import: {files.Length - failed - missing} succeeded, {failed} failed, {missing} missing templates.");
        return failed + missing == 0 ? 0 : 1;
    }
}

