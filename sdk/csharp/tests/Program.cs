using XxxXTeam.TempMail;

// 导出 ListChannels() 的有序 slug。
// 传入 --export <path> 时写入文件，否则打印到标准输出，供与 baseline 逐行比对。
var channels = TempMail.ListChannels();

var exportPath = "";
for (var i = 0; i < args.Length - 1; i++)
    if (args[i] == "--export") exportPath = args[i + 1];

var slugs = channels.Select(c => c.Channel).ToList();

if (!string.IsNullOrEmpty(exportPath))
{
    File.WriteAllText(exportPath, string.Join("\n", slugs) + "\n");
    Console.Error.WriteLine($"已导出 {slugs.Count} 个渠道到 {exportPath}");
}
else
{
    foreach (var s in slugs) Console.WriteLine(s);
    Console.Error.WriteLine($"共 {slugs.Count} 个渠道");
}
