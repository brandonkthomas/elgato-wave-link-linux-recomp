using Microsoft.Xaml.WidgetSpinner;
using Microsoft.Xaml.WidgetSpinner.XBF;

static class Probe
{
    public static int Run(string[] files)
    {
        var nodeHist = new SortedDictionary<string, int>();
        var crdHist = new SortedDictionary<string, int>();
        var versions = new SortedDictionary<string, int>();
        int ok = 0, fail = 0;
        foreach (var path in files)
        {
            var msgs = new List<string>();
            EventHandler<EventArgsWithMessage> w = (s, e) => msgs.Add("W: " + e.Message);
            EventHandler<EventArgsWithMessage> er = (s, e) => msgs.Add("E: " + e.Message);
            ErrorService.Instance.Warning += w; ErrorService.Instance.Error += er;
            try
            {
                var ws = new WidgetSpinner();
                var f = ws.DeserializeXbfFile(path);
                if (f == null) { fail++; Console.WriteLine($"DESER-FAIL {path}: {string.Join(" | ", msgs)}"); continue; }
                versions[f.Metadata.XbfVersion.ToString()] = versions.GetValueOrDefault(f.Metadata.XbfVersion.ToString()) + 1;
                foreach (var ns in f.NodeStreams)
                    foreach (var n in ns)
                    {
                        nodeHist[n.NodeType.ToString()] = nodeHist.GetValueOrDefault(n.NodeType.ToString()) + 1;
                        if (n is XbfNode<CustomRuntimeData, int> c)
                            crdHist[c.Values.Item1.Version.ToString()] = crdHist.GetValueOrDefault(c.Values.Item1.Version.ToString()) + 1;
                    }
                var root = ws.CreateObjectGraph(f);
                if (root == null || msgs.Any(m => m.StartsWith("E:"))) { fail++; Console.WriteLine($"GRAPH-FAIL {path}: {string.Join(" | ", msgs)}"); }
                else { ok++; if (msgs.Count > 0) Console.WriteLine($"WARN {path}: {string.Join(" | ", msgs.Distinct())}"); }
            }
            finally { ErrorService.Instance.Warning -= w; ErrorService.Instance.Error -= er; }
        }
        Console.WriteLine($"ok={ok} fail={fail}");
        Console.WriteLine("versions: " + string.Join(", ", versions.Select(kv => $"{kv.Key}={kv.Value}")));
        Console.WriteLine("node types: " + string.Join(", ", nodeHist.Select(kv => $"{kv.Key}={kv.Value}")));
        Console.WriteLine("custom runtime data: " + string.Join(", ", crdHist.Select(kv => $"{kv.Key}={kv.Value}")));
        return fail == 0 ? 0 : 1;
    }
}
