using System.Text;

namespace DC3DDMesTextTool;

/// <summary>Parses Circus bytecode and rebuilds text while preserving script instructions.</summary>
public class MesScript
{
    private byte[] _rawData = Array.Empty<byte>();
    private ScriptInfo _info = ScriptInfo.DC3DD;
    private readonly List<Token> _tokens = new();
    private int _bytecodeOffset;
    private int _labelCount;
    private int VersionLength => _info.OffsetType == OffsetType.Offset2 ? 3 :
        ((_info.Version & 0xFF00) == 0 ? 1 : 2);
    private int CodeStart => _bytecodeOffset + VersionLength;

    public byte[] RawData => _rawData;
    public ScriptInfo Info => _info;
    public IReadOnlyList<Token> Tokens => _tokens;
    public int LabelCount => _labelCount;
    public int BytecodeOffset => _bytecodeOffset;

    public void Load(string path, ScriptInfo? info = null) => Load(File.ReadAllBytes(path), info);

    public void Load(byte[] data, ScriptInfo? info = null)
    {
        _tokens.Clear();
        _rawData = data;
        _info = info ?? ScriptInfo.QueryFromData(data)
            ?? throw new InvalidDataException("Unsupported or invalid MES header/version. Expected Da Capo 3 Dream Days (0xA5A8).");
        if (data.Length < 8) throw new InvalidDataException("MES file too small.");
        int count = BitConverter.ToInt32(data, 0);
        long offset = (long)count * (_info.OffsetType == OffsetType.Offset2 ? 6 : 4) + 4;
        if (count < 1 || offset + VersionLength > data.Length)
            throw new InvalidDataException("Invalid MES label count or bytecode offset.");
        _bytecodeOffset = (int)offset;
        // Offset2 header includes the initial version label (3) at +4.
        // The following count-1 DWORDs are code labels; the tail is a separate WORD table.
        _labelCount = _info.OffsetType == OffsetType.Offset2 ? count - 1 : count;
        if (_info.OffsetType == OffsetType.Offset2 && BitConverter.ToInt32(data, 4) != 3)
            throw new InvalidDataException("Invalid Offset2 version label.");

        int pos = CodeStart;
        while (pos < data.Length)
        {
            byte op = data[pos];
            int length;
            if (_info.IsInSection(_info.Uint8x2, op)) length = 3;
            else if (_info.IsInSection(_info.Uint8Str, op)) length = StringEnd(pos + 2) - pos + 1;
            else if (_info.IsInSection(_info.String, op) || _info.IsInSection(_info.EncStr, op))
                length = StringEnd(pos + 1) - pos + 1;
            else if (_info.IsInSection(_info.Uint16x4, op)) length = 9;
            else throw new InvalidDataException($"Unknown opcode 0x{op:X2} at 0x{pos:X}.");
            if (length > data.Length - pos)
                throw new InvalidDataException($"Truncated instruction at 0x{pos:X}.");
            _tokens.Add(new Token(op, pos - CodeStart, length));
            pos += length;
        }
        if (_info.OffsetType == OffsetType.Offset2 &&
            _tokens.Count(t => t.Opcode is 3 or 4) != _labelCount)
            throw new InvalidDataException("MES label count does not match bytecode.");
    }

    private int StringEnd(int start)
    {
        if (start >= _rawData.Length) throw new InvalidDataException("Truncated MES string.");
        int end = Array.IndexOf(_rawData, (byte)0, start);
        return end >= 0 ? end : throw new InvalidDataException($"Unterminated MES string at 0x{start:X}.");
    }

    public List<TextEntry> ExtractText(Encoding encoding, bool verbose = false)
    {
        var entries = new List<TextEntry>();
        foreach (var token in _tokens)
        {
            bool encrypted = _info.IsInSection(_info.EncStr, token.Opcode);
            if (!encrypted && !_info.OutputOpcodes.Contains(token.Opcode)) continue;
            var bytes = _rawData.AsSpan(CodeStart + token.Offset + 1, token.Length - 2).ToArray();
            if (encrypted)
                for (int i = 0; i < bytes.Length; i++)
                    bytes[i] = unchecked((byte)(bytes[i] + _info.EncryptionKey));
            string text = encoding.GetString(bytes);
            // Keep legacy JSON indexing: empty native tokens are preserved in the MES template.
            if (text.Length == 0) continue;
            if (encrypted) text = text.Replace('_', ' ');
            entries.Add(new TextEntry {
                Offset = token.Offset, OriginalLength = token.Length, Opcode = token.Opcode,
                IsEncrypted = encrypted, Message = text,
                IsChoice = _info.Name == "dc3dd" && token.Opcode is 0x58 or 0x66
            });
        }
        return entries;
    }

    public byte[] ImportText(List<TextEntry> translatedEntries, Encoding encoding, TextFormatter? formatter = null)
    {
        var originals = ExtractText(encoding);
        if (translatedEntries.Count != originals.Count)
            throw new InvalidDataException($"Text count mismatch: MES has {originals.Count}, JSON has {translatedEntries.Count}. No file written.");
        var replacements = new Dictionary<int, byte[]>();
        for (int i = 0; i < originals.Count; i++)
        {
            var original = originals[i];
            string text = translatedEntries[i].Message
                ?? throw new InvalidDataException($"Entry {i + 1}: message must be a string.");
            if (text.Length == 0)
                throw new InvalidDataException($"Entry {i + 1}: a nonempty MES text cannot be replaced with an empty message.");
            bool speaker = _info.Name == "dc3dd" && original.Opcode == 0x64;
            if (speaker && (text.Contains('\n') || text.Contains('\r') || text.Contains("\\n")))
                throw new InvalidDataException($"Entry {i + 1}: character name must be one line.");
            if (text.Contains('\0'))
                throw new InvalidDataException($"Entry {i + 1}: NUL is not allowed in MES text.");
            if (speaker && encoding.GetByteCount(text) > 31)
                throw new InvalidDataException($"Entry {i + 1}: character name exceeds the 31-byte field.");
            // Only the dialogue/narration opcode is wrapped. Names and choices keep their layout.
            if (_info.Name == "dc3dd" && original.Opcode == 0x65 && formatter != null)
                text = formatter.Format(text);
            // Preserve the original byte spelling (including CP932 aliases) on unchanged text.
            if (text == original.Message) continue;
            if (original.IsEncrypted) text = text.Replace(' ', '_');
            byte[] bytes;
            try { bytes = encoding.GetBytes(text); }
            catch (EncoderFallbackException ex) {
                throw new InvalidDataException($"Entry {i + 1}: text is not representable in CP932.", ex);
            }
            if (original.IsEncrypted)
                for (int j = 0; j < bytes.Length; j++)
                    bytes[j] = unchecked((byte)(bytes[j] - _info.EncryptionKey));
            if (bytes.Contains((byte)0))
                throw new InvalidDataException($"Entry {i + 1}: encoding would create an embedded MES terminator.");
            replacements[original.Offset] = bytes;
        }

        byte[] header = _rawData.AsSpan(0, CodeStart).ToArray();
        using var code = new MemoryStream();
        int label = 0;
        foreach (var token in _tokens)
        {
            if (_info.OffsetType == OffsetType.Offset2 && token.Opcode is 3 or 4)
            {
                // Label addresses include the three-byte version prefix. Preserve flag bits.
                long address = code.Position + token.Length + VersionLength;
                if (address > 0xFFFFFF) throw new InvalidDataException("MES label exceeds 24-bit address range.");
                int at = 8 + label++ * 4;
                uint old = BitConverter.ToUInt32(header, at);
                BitConverter.GetBytes((old & 0xFF000000) | (uint)address).CopyTo(header, at);
            }
            if (replacements.TryGetValue(token.Offset, out var bytes))
            {
                code.WriteByte(token.Opcode);
                code.Write(bytes);
                code.WriteByte(0);
            }
            else code.Write(_rawData, CodeStart + token.Offset, token.Length);
        }
        using var output = new MemoryStream();
        output.Write(header);
        code.Position = 0;
        code.CopyTo(output);
        return output.ToArray();
    }
}
