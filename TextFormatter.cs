using System.Text;

namespace DC3DDMesTextTool;

/// <summary>
/// Text formatter with word wrap functionality.
/// Ported from C++ MesTextTool's formater class.
/// </summary>
public class TextFormatter
{
    // Characters that should not appear at the start of a line
    private static readonly HashSet<char> DisallowedAtStart = new()
    {
        '\u3002', '\u3001', '\uff1f', '\u2019', '\u201d', '\uff0c', '\uff01', '\uff5e', '\u3011', '\uff1b', '\uff1a', '\uff09', '\u300d', '\u300f', '\u2026', ' ', '\u3000'
    };
    
    // Characters that should not appear at the end of a line
    private static readonly HashSet<char> DisallowedAtEnd = new()
    {
        '\uff08', '(', '\u300c', '\u300e', '\u3010', '\u2018', '\u201c'
    };
    
    public int MinLineLength { get; set; } = 18;
    public int MaxLineLength { get; set; } = 67;
    
    /// <summary>
    /// Check if text is dialogue (starts and ends with quote marks)
    /// </summary>
    private static bool IsTalking(string text)
    {
        if (text.Length < 2) return false;
        
        var quotes = new[] { ("\u300c", "\u300d"), ("\u300e", "\u300f"), ("\u201c", "\u201d") };
        foreach (var (open, close) in quotes)
        {
            if (text.StartsWith(open) && text.EndsWith(close))
                return true;
        }
        return false;
    }
    
    /// <summary>
    /// Check if character is half-width (ASCII range)
    /// </summary>
    private static bool IsHalfWidth(char c)
    {
        return c >= 0x0000 && c <= 0x007F;
    }
    
    /// <summary>
    /// Get character width for line counting (half-width = 1.0, full-width = 2.0)
    /// </summary>
    private static float GetCharWidth(char c)
    {
        return IsHalfWidth(c) ? 1.0f : 2.0f;
    }
    
    /// <summary>
    /// Format text with word wrapping.
    /// </summary>
    public string Format(string text)
    {
        if (string.IsNullOrEmpty(text))
            return text;
        
        // Remove existing line breaks first
        text = text.Replace("\\n　", "").Replace("\\n", "");
        
        if (text.Length <= MinLineLength)
            return text;
        
        bool isTalking = IsTalking(text);
        string lineBreak = "\n";
        float lineBreakCount = 0.0f;
        
        var result = new StringBuilder();
        float lineCharCount = 0;
        int index = 0;
        
        while (index < text.Length)
        {
            char thisChar = text[index];
            
            // Check if we should insert a line break
            if (lineCharCount >= MinLineLength)
            {
                if (!DisallowedAtStart.Contains(thisChar))
                {
                    result.Append(lineBreak);
                    lineCharCount = lineBreakCount;
                }
            }
            
            // Handle control codes like @t90@d1 - skip them for width calculation
            if (thisChar == '@' && index + 1 < text.Length)
            {
                // Find end of control code
                int codeEnd = index;
                while (codeEnd < text.Length && text[codeEnd] != ' ' && !char.IsWhiteSpace(text[codeEnd]))
                {
                    if (codeEnd > index && !char.IsLetterOrDigit(text[codeEnd]) && text[codeEnd] != '@')
                        break;
                    codeEnd++;
                    if (codeEnd - index > 10) break; // Safety limit
                }
                
                // Copy control code without counting width
                string code = text.Substring(index, codeEnd - index);
                result.Append(code);
                index = codeEnd;
                continue;
            }
            
            // Handle half-width character sequences (English words)
            if (IsHalfWidth(thisChar) && thisChar != ' ')
            {
                // Find end of half-width sequence
                int seqEnd = index;
                while (seqEnd < text.Length && IsHalfWidth(text[seqEnd]) && text[seqEnd] != ' ')
                {
                    seqEnd++;
                }
                
                string word = text.Substring(index, seqEnd - index);
                float wordWidth = (float)word.Length;
                
                // Check if word fits on current line
                if (lineCharCount + wordWidth >= MaxLineLength && lineCharCount > 0)
                {
                    result.Append(lineBreak);
                    lineCharCount = lineBreakCount;
                }
                
                result.Append(word);
                lineCharCount += wordWidth;
                index = seqEnd;
                continue;
            }
            
            // Full-width character
            if (!IsHalfWidth(thisChar))
            {
                lineCharCount += 2.0f;
            }
            else if (thisChar == ' ')
            {
                lineCharCount += 1.0f;
            }
            
            // Check max length for forced break
            if (lineCharCount >= MaxLineLength)
            {
                if (!DisallowedAtStart.Contains(thisChar) && !DisallowedAtEnd.Contains(thisChar))
                {
                    result.Append(lineBreak);
                    lineCharCount = lineBreakCount;
                }
            }
            
            // Check for line break before opening brackets
            if (lineCharCount >= MinLineLength && DisallowedAtEnd.Contains(thisChar))
            {
                result.Append(lineBreak);
                lineCharCount = isTalking ? 2.0f : 1.0f;
            }
            
            result.Append(thisChar);
            index++;
        }
        
        return result.ToString();
    }
}

