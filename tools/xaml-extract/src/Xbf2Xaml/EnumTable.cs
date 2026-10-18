// Enum value names. Primary source: enum definitions in the shipped projection assemblies (MetadataIndex).
// Fallback: WinUI's own XAML parser tables (dxaml/xcp/components/metadata/inc/EnumValueTable.g.h), keyed by
// enum short name.

using System.Globalization;
using System.Text.RegularExpressions;

sealed class EnumTable
{
    readonly MetadataIndex _meta;
    readonly Dictionary<string, List<KeyValuePair<long, string>>> _fallback = new(StringComparer.Ordinal);

    public EnumTable(MetadataIndex meta, string enumValueTableHeader)
    {
        _meta = meta;
        if (enumValueTableHeader != null && File.Exists(enumValueTableHeader))
        {
            string text = File.ReadAllText(enumValueTableHeader);
            foreach (Match m in Regex.Matches(text, @"const\s+XTABLE\s+sat(\w+)\[\]\s*=\s*\{(.*?)\};", RegexOptions.Singleline))
            {
                var list = new List<KeyValuePair<long, string>>();
                foreach (Match e in Regex.Matches(m.Groups[2].Value, @"L""([^""]*)""\)\s*,\s*([-0-9xXa-fA-F]+)\s*\}"))
                {
                    string v = e.Groups[2].Value;
                    long n = v.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
                        ? (long)ulong.Parse(v.Substring(2), NumberStyles.HexNumber)
                        : long.Parse(v, CultureInfo.InvariantCulture);
                    list.Add(new KeyValuePair<long, string>(n, e.Groups[1].Value));
                }
                _fallback[m.Groups[1].Value] = list;
            }
        }
    }

    public int FallbackTableCount => _fallback.Count;

    // Returns null if the value cannot be named.
    public string Name(string enumFullName, long value)
    {
        var tm = _meta.Get(enumFullName);
        List<KeyValuePair<long, string>> values = tm != null && tm.IsEnum ? tm.EnumValues : null;
        bool flags = tm?.IsFlags ?? false;
        if (values == null || values.Count == 0)
        {
            string shortName = enumFullName.Substring(enumFullName.LastIndexOf('.') + 1);
            _fallback.TryGetValue(shortName, out values);
        }
        if (values == null) return null;

        // XBF stores the value as uint32; projection enums may be signed (e.g. -1) - compare in both spaces
        foreach (var kv in values)
            if (kv.Key == value || (uint)kv.Key == (uint)value)
                return kv.Value;

        if (flags && value != 0)
        {
            var parts = new List<string>();
            long remaining = value;
            foreach (var kv in values.Where(kv => kv.Key != 0 && (kv.Key & (kv.Key - 1)) == 0).OrderBy(kv => kv.Key))
                if ((remaining & kv.Key) == kv.Key) { parts.Add(kv.Value); remaining &= ~kv.Key; }
            if (remaining == 0 && parts.Count > 0)
                return string.Join(",", parts);
        }
        return null;
    }
}
