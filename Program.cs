using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using DC3DDMesTextTool;
using DC3DDMesTextTool_GUI;

namespace DC3DDMesTextTool_GUI;

static class Program
{
    [STAThread]
    static int Main(string[] args)
    {
        // Check if running as GUI (no args)
        if (args.Length == 0)
        {
            Application.SetHighDpiMode(HighDpiMode.SystemAware);
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
            return 0;
        }

        // CLI Mode — safe to use console here
        try { Console.OutputEncoding = Encoding.UTF8; } catch { /* no console handle */ }
        PrintBanner();
        
        string command = args[0].ToLowerInvariant();
        var processor = new MesToolProcessor();
        processor.Log += Console.WriteLine;
        
        try
        {
            return command switch
            {
                "export" => HandleExport(processor, args.Skip(1).ToArray()),
                "import" => HandleImport(processor, args.Skip(1).ToArray()),
                "batch-export" => HandleBatchExport(processor, args.Skip(1).ToArray()),
                "batch-import" => HandleBatchImport(processor, args.Skip(1).ToArray()),
                "help" or "--help" or "-h" => PrintHelp(),
                _ when File.Exists(args[0]) => HandleDragDrop(processor, args[0]),
                _ when Directory.Exists(args[0]) => HandleDragDropFolder(processor, args[0]),
                _ => ShowError($"Unknown command: {command}")
            };
        }
        catch (Exception ex)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.WriteLine($"Error: {ex.Message}");
            Console.ResetColor();
            return 1;
        }
    }
    
    static void PrintBanner()
    {
        Console.ForegroundColor = ConsoleColor.Cyan;
        Console.WriteLine("╔════════════════════════════════════════════════╗");
        Console.WriteLine("║       DC3DD MES Text Tool v1.3 (GUI+CLI)         ║");
        Console.WriteLine("║  Based on CircusEditor and MesTextTool         ║");
        Console.WriteLine("╚════════════════════════════════════════════════╝");
        Console.ResetColor();
        Console.WriteLine();
    }
    
    static int HandleExport(MesToolProcessor processor, string[] args)
    {
        var options = ParseOptions(args, false, true);
        args = options.Paths;
        if (args.Length < 1)
        {
            Console.WriteLine("Usage: DC3DDMesTextTool export <input.mes> [output.json]");
            return 1;
        }
        
        string inputPath = args[0];
        string outputPath = args.Length > 1 ? args[1] : Path.ChangeExtension(inputPath, ".json");
        if (args.Length > 2) return ShowError("Too many export paths.");
        return processor.ExportMesToJson(inputPath, outputPath, options.Flat);
    }
    
    static int HandleImport(MesToolProcessor processor, string[] args)
    {
        var options = ParseOptions(args, true, false);
        args = options.Paths;
        if (args.Length < 2)
        {
            Console.WriteLine($"Usage: DC3DDMesTextTool import <input.json> <original.mes> [output.mes] [-w {TextFormatter.DefaultLineLength} | --no-wrap]");
            return 1;
        }
        
        string jsonPath = args[0];
        string originalMesPath = args[1];
        string outputPath = args.Length > 2 ? args[2] : Path.Combine(Path.GetDirectoryName(originalMesPath) ?? "", "output_" + Path.GetFileName(originalMesPath));
        if (args.Length > 3) return ShowError("Too many import paths.");
        return processor.ImportJsonToMes(jsonPath, originalMesPath, outputPath, options.Width);
    }
    
    static int HandleBatchExport(MesToolProcessor processor, string[] args)
    {
        var options = ParseOptions(args, false, true);
        args = options.Paths;
        if (args.Length < 1)
        {
            Console.WriteLine("Usage: DC3DDMesTextTool batch-export <mes_folder> [json_folder]");
            return 1;
        }
        
        string inputFolder = args[0];
        string outputFolder = args.Length > 1 ? args[1] : Path.Combine(inputFolder, "json_output");
        if (args.Length > 2) return ShowError("Too many batch-export paths.");
        return processor.BatchExport(inputFolder, outputFolder, options.Flat);
    }
    
    static int HandleBatchImport(MesToolProcessor processor, string[] args)
    {
        var options = ParseOptions(args, true, false);
        args = options.Paths;
        if (args.Length < 2)
        {
            Console.WriteLine("Usage: DC3DDMesTextTool batch-import <json_folder> <mes_folder> [output_folder] [-w <wrap_width>]");
            return 1;
        }
        
        string jsonFolder = args[0];
        string mesFolder = args[1];
        string outputFolder = args.Length > 2 && !args[2].StartsWith("-") ? args[2] : Path.Combine(mesFolder, "mes_output");
        
        if (args.Length > 3) return ShowError("Too many batch-import paths.");
        return processor.BatchImport(jsonFolder, mesFolder, outputFolder, options.Width);
    }

    static (string[] Paths, int Width, bool Flat) ParseOptions(string[] args, bool allowWrap, bool allowFlat)
    {
        var paths = new System.Collections.Generic.List<string>();
        int width = TextFormatter.DefaultLineLength;
        bool flat = false;
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--flat" && allowFlat) flat = true;
            else if (args[i] == "--no-wrap" && allowWrap) width = 0;
            else if ((args[i] == "-w" || args[i] == "--wrap") && allowWrap)
            {
                if (++i >= args.Length || !int.TryParse(args[i], out width) ||
                    (width != 0 && (width < 10 || width > 1000)))
                    throw new ArgumentException("Wrap width must be 0 or 10..1000.");
            }
            else if (args[i].StartsWith('-')) throw new ArgumentException($"Unknown option: {args[i]}");
            else paths.Add(args[i]);
        }
        return (paths.ToArray(), width, flat);
    }

    static int PrintHelp()
    {
        Console.WriteLine("export <input.mes> [output.json] [--flat]");
        Console.WriteLine($"import <input.json> <original.mes> [output.mes] [-w {TextFormatter.DefaultLineLength} | --no-wrap]");
        Console.WriteLine("batch-export <mes_folder> [json_folder] [--flat]");
        Console.WriteLine($"batch-import <json_folder> <mes_folder> [output_folder] [-w {TextFormatter.DefaultLineLength} | --no-wrap]");
        Console.WriteLine("Default JSON groups name/message. --flat exports the legacy message-only array.");
        Console.WriteLine("Wrap applies to dialogue bodies. Names and choices are preserved. Original MES is required.");
        return 0;
    }
    
    static int HandleDragDrop(MesToolProcessor processor, string filePath)
    {
        // Drag drop on EXE behaves like legacy CLI drag drop
        if (filePath.EndsWith(".mes", StringComparison.OrdinalIgnoreCase))
        {
            string outputPath = Path.ChangeExtension(filePath, ".json");
            return processor.ExportMesToJson(filePath, outputPath);
        }
        else if (filePath.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
        {
            // For JSON drop, we can't easily guess the MES, so better to launch GUI?
            // "To import JSON, please also provide the original MES file."
            // Let's just launch GUI pre-filled if we can, but simpler to just show error or open GUI.
            // Opening GUI is friendlier.
             Application.SetHighDpiMode(HighDpiMode.SystemAware);
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
            return 0;
        }
        return 1;
    }
    
    static int HandleDragDropFolder(MesToolProcessor processor, string folderPath)
    {
        // If folder dropped, process it OR open GUI?
        // Legacy behavior: process it.
        // Let's keep legacy behavior for now.
        var mesFiles = Directory.GetFiles(folderPath, "*.mes");
        if (mesFiles.Length > 0)
        {
            return processor.BatchExport(folderPath, Path.Combine(folderPath, "json_output"));
        }
        return 1;
    }

    static int ShowError(string message)
    {
        Console.ForegroundColor = ConsoleColor.Red;
        Console.WriteLine(message);
        Console.ResetColor();
        return 1;
    }
}

