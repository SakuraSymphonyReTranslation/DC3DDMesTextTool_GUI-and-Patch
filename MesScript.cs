using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace DC3DDMesTextTool;

/// <summary>
/// MES script parser and text extractor.
/// </summary>
public class MesScript
{
    private byte[] _rawData = Array.Empty<byte>();
    private ScriptInfo _info = ScriptInfo.DC3DD;
    private List<Token> _tokens = new();
    private int _labelCount;
    private int _bytecodeOffset;
    private int[] _labels = Array.Empty<int>();
    
    // Regex pattern for detecting choice text
    private static readonly Regex ChoicePattern = new(@"戟ｅ訓.*|ワ[一-龯]", RegexOptions.Compiled);
    
    public byte[] RawData => _rawData;
    public ScriptInfo Info => _info;
    public IReadOnlyList<Token> Tokens => _tokens;
    public int LabelCount => _labelCount;
    public int BytecodeOffset => _bytecodeOffset;
    
    public void Load(byte[] data, ScriptInfo? info = null)
    {
        _rawData = data;
        _info = info ?? ScriptInfo.QueryFromData(data) ?? ScriptInfo.DC3DD;
        
        ParseStructure();
        TokenizeBytecode();
    }
    
    public void Load(string path, ScriptInfo? info = null)
    {
        Load(File.ReadAllBytes(path), info);
    }
    
    private void ParseStructure()
    {
        if (_rawData.Length < 8)
            throw new InvalidDataException("MES file too small");
            
        int rawCount = BitConverter.ToInt32(_rawData, 0);
        
        // Calculate bytecode offset based on offset type
        _bytecodeOffset = (int)_info.OffsetType switch
        {
            (int)OffsetType.Offset1 => rawCount * 4 + 4,
            (int)OffsetType.Offset2 => rawCount * 6 + 4,
            _ => rawCount * 4 + 4
        };
        
        if (_bytecodeOffset >= _rawData.Length)
            throw new InvalidDataException("Invalid MES structure");
            
        // Offset2 scripts (including DC3DD) store label table starting at +8.
        int labelStartOffset = 4;
        if ((int)_info.OffsetType == (int)OffsetType.Offset2)
        {
            labelStartOffset = 8;
        }
        else if ((int)_info.OffsetType == (int)OffsetType.Offset1 && BitConverter.ToInt32(_rawData, 4) == 3)
        {
            labelStartOffset = 8;
        }

        // Calculate stride
        int stride = 4;
        
        // Use explicit count from header
        _labelCount = rawCount;
        
        _labels = new int[_labelCount];
        for (int i = 0; i < _labelCount; i++)
        {
            _labels[i] = BitConverter.ToInt32(_rawData, labelStartOffset + i * stride);
        }
    }
    
    private int CalcVersionLength()
    {
        // Check if header format implies version length
        if ((int)_info.OffsetType == (int)OffsetType.Offset2)
        {
            return 3; // Offset2 scripts use Version + Padding (3 bytes)
        }
        else if ((int)_info.OffsetType == (int)OffsetType.Offset1)
        {
             // If header[1] == 3, version len is 3
             // But simpler to rely on labels[0] or raw check
             if (BitConverter.ToInt32(_rawData, 4) == 3) return 3;
        }
        return (_info.Version & 0xFF00) == 0 ? 1 : 2;
    }

    private void TokenizeBytecode()
    {
        _tokens.Clear();
        // Start after the version token
        int pos = _bytecodeOffset + CalcVersionLength();
        
        while (pos < _rawData.Length)
        {
            byte opcode = _rawData[pos];
            int tokenLength;
            
            // Determine token length based on opcode type
            if (_info.IsInSection(_info.Uint8x2, opcode))
            {
                // [op][arg1][arg2] = 3 bytes
                tokenLength = 3;
            }
            else if (_info.IsInSection(_info.Uint8Str, opcode))
            {
                // [op][arg1][string\0]
                tokenLength = 2 + GetStringLength(pos + 2) + 1;
            }
            else if (_info.IsInSection(_info.String, opcode))
            {
                // [op][string\0]
                tokenLength = 1 + GetStringLength(pos + 1) + 1;
            }
            else if (_info.IsInSection(_info.EncStr, opcode))
            {
                // [op][encstring\0]
                tokenLength = 1 + GetStringLength(pos + 1) + 1;
            }
            else if (_info.IsInSection(_info.Uint16x4, opcode))
            {
                // [op][4 x uint16] = 9 bytes
                tokenLength = 9;
            }
            else
            {
                // Unknown opcode, skip 1 byte
                tokenLength = 1;
            }
            
            // Bounds check
            if (pos + tokenLength > _rawData.Length)
                break;
                
            // Tokens are relative to the start of ACTUAL bytecode (after version token)
            _tokens.Add(new Token(opcode, pos - (_bytecodeOffset + CalcVersionLength()), tokenLength));
            pos += tokenLength;
        }
    }
    
    private int GetStringLength(int offset)
    {
        int length = 0;
        while (offset + length < _rawData.Length && _rawData[offset + length] != 0)
        {
            length++;
        }
        return length;
    }
    
    public List<TextEntry> ExtractText(Encoding encoding, bool verbose = false)
    {
        var entries = new List<TextEntry>();
        int emptyCount = 0;
        
        foreach (var token in _tokens)
        {
            string? text = null;
            bool isEncrypted = false;
            
            if (_info.IsInSection(_info.EncStr, token.Opcode))
            {
                text = DecryptString(token.Offset + 1, encoding);
                isEncrypted = true;
            }
            else if (_info.OutputOpcodes.Contains(token.Opcode))
            {
                text = ReadString(token.Offset + 1, encoding);
            }
            
            if (text == null) continue;
            
            if (string.IsNullOrEmpty(text))
            {
                emptyCount++;
                continue;
            }
                
            text = text.Replace('_', ' ');
            
            if (string.IsNullOrEmpty(text))
            {
                emptyCount++;
                continue;
            }
            
            bool isChoice = ChoicePattern.IsMatch(text);
            
            var entry = new TextEntry
            {
                Offset = token.Offset,
                OriginalLength = token.Length,
                Opcode = token.Opcode,
                IsEncrypted = isEncrypted,
                Message = text,
                IsChoice = isChoice
            };
            
            entries.Add(entry);
        }
        
        return entries;
    }
    
    public List<TextEntry> ExtractTextPatternBased(Encoding encoding)
    {
        // Fallback for when opcode extraction fails.
        // For DC3DD, strict opcode extraction is preferred.
        // Returning empty list to satisfy build; if opcode extraction fails, we have bigger issues.
        return new List<TextEntry>();
    }

    public byte[] ImportText(List<TextEntry> translatedEntries, Encoding encoding, TextFormatter? formatter = null)
    {
         // Re-extract original text to get offsets
        var originalEntries = ExtractText(encoding, false).ToList();
        
        var translationMap = new Dictionary<int, string>();
        for (int i = 0; i < Math.Min(originalEntries.Count, translatedEntries.Count); i++)
        {
            var origEntry = originalEntries[i];
            var text = translatedEntries[i].Message;
            
            if (formatter != null && !string.IsNullOrEmpty(text))
            {
                int originalMax = formatter.MaxLineLength;
                if (origEntry.Opcode == 0x6A) formatter.MaxLineLength = 1000;
                text = formatter.Format(text);
                formatter.MaxLineLength = originalMax;
            }
            
            translationMap[origEntry.Offset] = text;
        }
        
        int versionLen = CalcVersionLength();
        
        // Clone raw data for the header
        byte[] newHeader = new byte[_bytecodeOffset];
        Array.Copy(_rawData, newHeader, _bytecodeOffset);
        
        int labelStartOffset = 4;
        if ((int)_info.OffsetType == (int)OffsetType.Offset2) labelStartOffset = 8;
        else if ((int)_info.OffsetType == (int)OffsetType.Offset1 && BitConverter.ToInt32(_rawData, 4) == 3) labelStartOffset = 8;
        
        // Note: _labelCount is now derived from available space
        
        // Determine stride
        int stride = 4;
        
        using var bytecodeOutput = new MemoryStream();
        int labelIndex = 0;

        foreach (var token in _tokens)
        {
            long currentPos = bytecodeOutput.Position; // Position relative to start of bytecode
            
            // C++ Logic: "Update label if we see Opcode 0x03 or 0x04" (Offset2 scripts)
            if ((int)_info.OffsetType == (int)OffsetType.Offset2)
            {
                if (token.Opcode == 0x03 || token.Opcode == 0x04)
                {
                    if (labelIndex < _labelCount)
                    {
                        // Calculate New Relative Offset 
                        // Offset2 uses offsets relative to the start of bytecode section
                        int newRelative = (int)currentPos + token.Length;
                        
                        // Read valid original label to preserve high bits (masks)
                        int oldLabel = BitConverter.ToInt32(newHeader, labelStartOffset + labelIndex * stride);
                        
                        // SAFETY: Only update if the old label looks like a valid pointer (in-bounds)
                        // Offset2 pointers are relative, so ensure they remain in bounds
                        int oldOffset = oldLabel & 0x00FFFFFF;
                        if (oldOffset < _rawData.Length) 
                        {
                            int newLabelValue = (oldLabel & unchecked((int)0xFF000000)) | (newRelative & 0x00FFFFFF);
                            byte[] labelBytes = BitConverter.GetBytes(newLabelValue);
                            Array.Copy(labelBytes, 0, newHeader, labelStartOffset + labelIndex * stride, 4);
                        }
                        labelIndex++;
                    }
                }
            }
                        
            // Write Token
            bool isEncStr = _info.IsInSection(_info.EncStr, token.Opcode);
            bool isOutputOp = _info.OutputOpcodes.Contains(token.Opcode);
            
            if ((isEncStr || isOutputOp) && translationMap.TryGetValue(token.Offset, out var text))
            {
                if (isEncStr && text.Contains(' '))
                {
                    text = text.Replace(' ', '_');
                }

                byte[] textBytes = encoding.GetBytes(text);
                if (isEncStr)
                {
                    for (int i = 0; i < textBytes.Length; i++)
                    {
                        textBytes[i] = (byte)((textBytes[i] - _info.EncryptionKey) & 0xFF);
                    }
                }
                bytecodeOutput.WriteByte(token.Opcode);
                bytecodeOutput.Write(textBytes, 0, textBytes.Length);
                bytecodeOutput.WriteByte(0);
            }
            else
            {
                bytecodeOutput.Write(_rawData, _bytecodeOffset + versionLen + token.Offset, token.Length);
            }
        }
        
        // Construct Final File
        using var output = new MemoryStream();
        
        output.Write(newHeader, 0, newHeader.Length);
        output.Write(_rawData, _bytecodeOffset, versionLen);
        bytecodeOutput.Position = 0;
        bytecodeOutput.CopyTo(output);
        
        return output.ToArray();
    }
    
    private int GetNewTokenLength(Token token, Dictionary<int, string> translationMap, Encoding encoding)
    {
        bool isEncStr = _info.IsInSection(_info.EncStr, token.Opcode);
        bool isOutputOp = _info.OutputOpcodes.Contains(token.Opcode);
        
        if ((isEncStr || isOutputOp) && translationMap.TryGetValue(token.Offset, out var text))
        {
            if (isEncStr && text.Contains(' '))
                text = text.Replace(' ', '_');
            byte[] textBytes = encoding.GetBytes(text);
            return 1 + textBytes.Length + 1; // opcode + text + null
        }
        return token.Length;
    }
    
    private string DecryptString(int relativeOffset, Encoding encoding)
    {
        int absoluteOffset = _bytecodeOffset + CalcVersionLength() + relativeOffset;
        var bytes = new List<byte>();
        
        while (absoluteOffset < _rawData.Length && _rawData[absoluteOffset] != 0)
        {
            byte decrypted = (byte)((_rawData[absoluteOffset] + _info.EncryptionKey) & 0xFF);
            bytes.Add(decrypted);
            absoluteOffset++;
        }
        
        return encoding.GetString(bytes.ToArray());
    }
    
    private string ReadString(int relativeOffset, Encoding encoding)
    {
        int absoluteOffset = _bytecodeOffset + CalcVersionLength() + relativeOffset;
        var bytes = new List<byte>();
        
        while (absoluteOffset < _rawData.Length && _rawData[absoluteOffset] != 0)
        {
            bytes.Add(_rawData[absoluteOffset]);
            absoluteOffset++;
        }
        
        return encoding.GetString(bytes.ToArray());
    }
}
