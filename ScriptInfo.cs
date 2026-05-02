namespace DC3DDMesTextTool;

/// <summary>
/// Script configuration for different Circus engine games.
/// Based on MesTextTool's script_info.cpp
/// </summary>
public class ScriptInfo
{
    public string Name { get; init; } = "";
    public ushort Version { get; init; }
    public OffsetType OffsetType { get; init; }
    
    // Opcode ranges for different instruction types
    public (byte Begin, byte End) Uint8x2 { get; init; }   // [op][arg1:byte][arg2:byte]
    public (byte Begin, byte End) Uint8Str { get; init; }  // [op][arg1:byte][string]
    public (byte Begin, byte End) String { get; init; }    // [op][string] - unencrypted
    public (byte Begin, byte End) EncStr { get; init; }    // [op][encstring] - encrypted
    public (byte Begin, byte End) Uint16x4 { get; init; }  // [op][4 x uint16]
    
    public byte EncryptionKey { get; init; }
    public byte[] OutputOpcodes { get; init; } = Array.Empty<byte>();
    
    /// <summary>
    /// Check if an opcode falls within a section range
    /// </summary>
    public bool IsInSection((byte Begin, byte End) section, byte opcode)
    {
        if (section.Begin == 0xFF && section.End == 0xFF)
            return false;
        return opcode >= section.Begin && opcode <= section.End;
    }
    
    /// <summary>
    /// D.C.III Platinum Partner configuration
    /// </summary>
    public static readonly ScriptInfo DC3PP = new()
    {
        Name = "dc3pp",
        Version = 0x9872,
        OffsetType = OffsetType.Offset2,
        Uint8x2 = (0x00, 0x2A),
        Uint8Str = (0x2B, 0x32),
        String = (0x33, 0x4E),
        EncStr = (0x4F, 0x51),
        Uint16x4 = (0x52, 0xFF),
        EncryptionKey = 0x20,
        OutputOpcodes = new byte[] { 0x45 }
    };

    /// <summary>
    /// D.C.III DreamDays configuration.
    /// Values are aligned with upstream MesTextTool script_info.cpp.
    /// </summary>
    public static readonly ScriptInfo DC3DD = new()
    {
        Name = "dc3dd",
        Version = 0xA5A8,
        OffsetType = OffsetType.Offset2,
        Uint8x2 = (0x00, 0x38),
        Uint8Str = (0x39, 0x43),
        String = (0x44, 0x62),
        EncStr = (0x63, 0x67),
        Uint16x4 = (0x68, 0xFF),
        EncryptionKey = 0x20,
        OutputOpcodes = new byte[] { 0x58 }
    };

    /// <summary>
    /// D.C.4 ~Da Capo 4~ configuration
    /// </summary>
    public static readonly ScriptInfo DC4 = new()
    {
        Name = "dc4",
        Version = 0xAAB6,
        OffsetType = OffsetType.Offset2,
        Uint8x2 = (0x00, 0x3A),
        Uint8Str = (0x3B, 0x47),
        String = (0x48, 0x68),
        EncStr = (0x69, 0x6D),
        Uint16x4 = (0x6E, 0xFF),
        EncryptionKey = 0x20,
        OutputOpcodes = new byte[] { 0x5D }
    };
    
    /// <summary>
    /// D.C.4 Plus Harmony configuration
    /// </summary>
    public static readonly ScriptInfo DC4PH = new()
    {
        Name = "dc4ph",
        Version = 0xABB6,
        OffsetType = OffsetType.Offset2,
        Uint8x2 = (0x00, 0x3A),
        Uint8Str = (0x3B, 0x47),
        String = (0x48, 0x68),
        EncStr = (0x69, 0x6D),
        Uint16x4 = (0x6E, 0xFF),
        EncryptionKey = 0x20,
        OutputOpcodes = new byte[] { 0x5D }
    };
    
    /// <summary>
    /// All known script configurations
    /// </summary>
    public static readonly ScriptInfo[] AllInfos = { DC3DD, DC3PP, DC4, DC4PH };
    
    /// <summary>
    /// Query script info by name
    /// </summary>
    public static ScriptInfo? QueryByName(string name)
    {
        return AllInfos.FirstOrDefault(i => 
            i.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
    }
    
    /// <summary>
    /// Query script info from MES file data
    /// </summary>
    public static ScriptInfo? QueryFromData(byte[] data)
    {
        if (data.Length < 8)
            return null;

        int labelCount = BitConverter.ToInt32(data, 0);
        int offset1 = labelCount * 4 + 4;
        int offset2 = labelCount * 6 + 4;

        ushort version1 = 0;
        ushort version2 = 0;

        if (data.Length > offset1 + 1)
            version1 = BitConverter.ToUInt16(data, offset1);

        if (data.Length > offset2 + 1)
            version2 = BitConverter.ToUInt16(data, offset2);

        foreach (var info in AllInfos)
        {
            if (info.OffsetType == OffsetType.Offset1)
            {
                if (version1 == 0)
                    continue;

                if ((info.Version & 0xFF00) == 0)
                {
                    if ((version1 & 0x00FF) == info.Version)
                        return info;
                }
                else if (info.Version == version1)
                {
                    return info;
                }
            }
            else
            {
                if (version2 != 0 && info.Version == version2)
                    return info;
            }
        }

        // Default to DreamDays profile for this tool.
        return DC3DD;
    }
}

public enum OffsetType
{
    Offset1,  // head[0] * 4 + 4
    Offset2   // head[0] * 6 + 4
}
