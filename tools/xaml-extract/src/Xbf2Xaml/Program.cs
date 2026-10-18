static class Program
{
    static int Main(string[] args)
    {
        if (args.Length > 0 && args[0] == "--probe")
            return Probe.Run(args.Skip(1).ToArray());
        Console.Error.WriteLine("usage: Xbf2Xaml --probe <files...>");
        return 2;
    }
}
