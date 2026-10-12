using System.Text;

namespace DC3DDMesTextTool;

/// <summary>Wrap body text in half-width units, preserving explicit breaks and control tokens.</summary>
public class TextFormatter
{
    public const int DefaultLineLength = 46;

    public int MinLineLength { get; set; } = 18; // Retained for source compatibility.
    public int MaxLineLength { get; set; } = DefaultLineLength;

    private static int Width(char c) => c <= 0x7F || c is >= '\uFF61' and <= '\uFF9F' ? 1 : 2;

    public string Format(string text)
    {
        if (MaxLineLength < 2) throw new ArgumentOutOfRangeException(nameof(MaxLineLength));
        text = text.Replace("\r\n", "\n").Replace('\r', '\n').Replace("\\n", "\n");
        // Ruby expressions use a separate engine layout; keep them intact.
        if (text.Contains('｛')) return text;
        return string.Join("\n", text.Split('\n').Select(WrapLine));
    }

    private string WrapLine(string text)
    {
        var output = new StringBuilder();
        int width = 0;
        var spaces = new StringBuilder();
        void Break()
        {
            output.Append('\n');
            width = 0;
            spaces.Clear();
        }
        int i = 0;
        while (i < text.Length)
        {
            char c = text[i];
            if (c is ' ' or '\t')
            {
                spaces.Append(c == '\t' ? ' ' : c);
                i++;
                continue;
            }
            // Engine controls such as @s, @l, @t90, @d1 are zero-width and indivisible.
            if (c == '@' && i + 1 < text.Length && char.IsAsciiLetter(text[i + 1]))
            {
                int end = i + 2;
                while (end < text.Length && char.IsAsciiDigit(text[end])) end++;
                output.Append(text.AsSpan(i, end - i));
                i = end;
                continue;
            }
            int endWord = i + 1;
            if (c is >= '!' and <= '~')
                while (endWord < text.Length && text[endWord] is >= '!' and <= '~' && text[endWord] != '@')
                    endWord++;
            string word = text[i..endWord];
            int wordWidth = word.Sum(Width);
            if (width > 0 && width + spaces.Length + wordWidth > MaxLineLength)
                Break();
            if (spaces.Length > 0)
            {
                if (width + spaces.Length + Math.Min(wordWidth, MaxLineLength) <= MaxLineLength)
                {
                    output.Append(spaces);
                    width += spaces.Length;
                }
                spaces.Clear();
            }
            foreach (char letter in word)
            {
                int units = Width(letter);
                if (width + units > MaxLineLength) Break();
                output.Append(letter);
                width += units;
            }
            i = endWord;
        }
        // Trailing whitespace is insignificant when reflow is enabled.
        return output.ToString();
    }
}

