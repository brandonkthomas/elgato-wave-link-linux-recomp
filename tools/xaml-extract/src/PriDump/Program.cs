// PriDump: extracts every resource candidate from one or more PRI files.
//
//   PriDump <outRoot> <file.pri>...
//
// Writes
//   <outRoot>/raw/<pri-name>/<resource path>          embedded data (xbf, images, ...)
//   <outRoot>/raw/<pri-name>/_manifest.json           every item/candidate with qualifiers
//   <outRoot>/strings/<pri-name>/<lang>.json          string resources per language (full resource names)
//   <outRoot>/strings/<lang>.json                     merged string resources (resw-style keys, see README)
//
// Built on chausner/PriTools (PriFormat). Read-only on the input files.

using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;
using PriFormat;

static class Program
{
    static readonly JsonSerializerOptions JsonOpts = new()
    {
        WriteIndented = true,
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    sealed record CandidateInfo(
        string Qualifiers,
        string Type,
        int? Size,
        string? Value,
        string? OutputPath,
        string? ExternalFile);

    sealed record ItemInfo(string ResourceMap, string Name, List<CandidateInfo> Candidates);

    // merged: lang -> key -> value
    static readonly SortedDictionary<string, SortedDictionary<string, string>> MergedStrings = new(StringComparer.Ordinal);
    static readonly List<string> MergeConflicts = new();

    static int Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        if (args.Length < 2)
        {
            Console.Error.WriteLine("usage: PriDump <outRoot> <file.pri>...");
            return 2;
        }

        string outRoot = args[0];
        int failures = 0;
        // resources.pri (the app) first so that its keys win in the merged view
        var priFiles = args.Skip(1)
            .OrderBy(p => Path.GetFileName(p).Equals("resources.pri", StringComparison.OrdinalIgnoreCase) ? 0 : 1)
            .ThenBy(p => Path.GetFileName(p), StringComparer.OrdinalIgnoreCase)
            .ToList();

        foreach (string priPath in priFiles)
        {
            try
            {
                DumpPri(priPath, outRoot);
            }
            catch (Exception e)
            {
                failures++;
                Console.Error.WriteLine($"FAILED {priPath}: {e}");
            }
        }

        BuildResolvedViews();
        string stringsDir = Path.Combine(outRoot, "strings");
        Directory.CreateDirectory(stringsDir);
        foreach (var (lang, dict) in MergedStrings)
            File.WriteAllText(Path.Combine(stringsDir, lang + ".json"), JsonSerializer.Serialize(dict, JsonOpts) + "\n");
        if (MergeConflicts.Count > 0)
            File.WriteAllLines(Path.Combine(stringsDir, "_merge_conflicts.txt"), MergeConflicts);

        return failures == 0 ? 0 : 1;
    }

    static void DumpPri(string priPath, string outRoot)
    {
        string priName = Path.GetFileNameWithoutExtension(priPath);
        string rawDir = Path.Combine(outRoot, "raw", priName);
        Directory.CreateDirectory(rawDir);

        using FileStream stream = File.OpenRead(priPath);
        PriFile priFile = PriFile.Parse(stream);

        var items = new List<ItemInfo>();
        // lang -> fullName -> value
        var strings = new SortedDictionary<string, SortedDictionary<string, string>>(StringComparer.Ordinal);
        int embedded = 0, xbf = 0, stringCount = 0, paths = 0;
        var notes = new List<string>();

        foreach (var mapRef in priFile.PriDescriptorSection.ResourceMapSections)
        {
            ResourceMapSection map = priFile.GetSectionByRef(mapRef);
            DecisionInfoSection decisions = priFile.GetSectionByRef(map.DecisionInfoSection);
            HierarchicalSchemaSection? schema = null;
            try { schema = priFile.GetSectionByRef(map.SchemaSection); } catch { }
            string mapName = schema?.UniqueName ?? map.HierarchicalSchemaReference?.UniqueName ?? $"map{mapRef.SectionIndex}";

            foreach (CandidateSet set in map.CandidateSets.Values)
            {
                string itemName;
                try
                {
                    // PRI names look like "\Files\Views\Foo.xbf" / "\Resources\Key"; normalize to "Files/Views/Foo.xbf"
                    itemName = priFile.GetResourceMapItemByRef(set.ResourceMapItem).FullName.Replace('\\', '/').TrimStart('/');
                }
                catch (Exception e)
                {
                    itemName = $"<unresolved item {set.ResourceMapItem.ItemIndex}>";
                    notes.Add($"map {mapName}: could not resolve item {set.ResourceMapItem.ItemIndex}: {e.Message}");
                }

                var cands = new List<CandidateInfo>();
                int embeddedInSet = set.Candidates.Count(c => c.Type == ResourceValueType.EmbeddedData && c.SourceFile == null);

                foreach (Candidate cand in set.Candidates)
                {
                    QualifierSet qs = decisions.QualifierSets[cand.QualifierSet];
                    string qualifiers = string.Join(",", qs.Qualifiers.Select(q => $"{q.Type}={q.Value}"));
                    string? lang = qs.Qualifiers.Where(q => q.Type == QualifierType.Language).Select(q => NormalizeLang(q.Value)).FirstOrDefault();

                    if (cand.SourceFile != null)
                    {
                        string ext = priFile.GetReferencedFileByRef(cand.SourceFile.Value).FullName;
                        cands.Add(new CandidateInfo(qualifiers, cand.Type.ToString(), null, null, null, ext));
                        continue;
                    }

                    ByteSpan span = cand.DataItem != null ? priFile.GetDataItemByRef(cand.DataItem.Value) : cand.Data!.Value;
                    byte[] data = new byte[span.Length];
                    stream.Seek(span.Offset, SeekOrigin.Begin);
                    stream.ReadExactly(data);

                    switch (cand.Type)
                    {
                        case ResourceValueType.String:
                        case ResourceValueType.AsciiString:
                        case ResourceValueType.Utf8String:
                            {
                                string value = Decode(cand.Type, data);
                                cands.Add(new CandidateInfo(qualifiers, cand.Type.ToString(), data.Length, value, null, null));
                                string langKey = lang ?? "neutral";
                                if (!strings.TryGetValue(langKey, out var d))
                                    strings[langKey] = d = new SortedDictionary<string, string>(StringComparer.Ordinal);
                                d[itemName] = value;
                                stringCount++;
                                AddMerged(langKey, priName, itemName, value);
                                break;
                            }
                        case ResourceValueType.Path:
                        case ResourceValueType.AsciiPath:
                        case ResourceValueType.Utf8Path:
                            {
                                string value = Decode(cand.Type, data);
                                cands.Add(new CandidateInfo(qualifiers, cand.Type.ToString(), data.Length, value, null, null));
                                paths++;
                                break;
                            }
                        case ResourceValueType.EmbeddedData:
                            {
                                string rel = SanitizeRelPath(itemName);
                                if (embeddedInSet > 1 && qs.Qualifiers.Count > 0)
                                    rel = InsertQualifierTag(rel, qs.Qualifiers);
                                string outPath = Path.Combine(rawDir, rel);
                                Directory.CreateDirectory(Path.GetDirectoryName(outPath)!);
                                File.WriteAllBytes(outPath, data);
                                cands.Add(new CandidateInfo(qualifiers, cand.Type.ToString(), data.Length, null, rel.Replace('\\', '/'), null));
                                embedded++;
                                if (rel.EndsWith(".xbf", StringComparison.OrdinalIgnoreCase)) xbf++;
                                break;
                            }
                    }
                }

                items.Add(new ItemInfo(mapName, itemName, cands));
            }
        }

        var manifest = new
        {
            pri = Path.GetFileName(priPath),
            version = priFile.Version,
            sections = priFile.Sections.Select(s => s.ToString()).ToList(),
            counts = new { items = items.Count, embedded, xbf, strings = stringCount, paths },
            notes,
            items = items.OrderBy(i => i.Name, StringComparer.Ordinal).ToList(),
        };
        File.WriteAllText(Path.Combine(rawDir, "_manifest.json"), JsonSerializer.Serialize(manifest, JsonOpts) + "\n");

        if (strings.Count > 0)
        {
            string sdir = Path.Combine(outRoot, "strings", "raw", priName);
            Directory.CreateDirectory(sdir);
            foreach (var (lang, d) in strings)
                File.WriteAllText(Path.Combine(sdir, lang + ".json"), JsonSerializer.Serialize(d, JsonOpts) + "\n");
        }

        Console.WriteLine($"{Path.GetFileName(priPath)}: version={priFile.Version} items={items.Count} embedded={embedded} xbf={xbf} strings={stringCount} paths={paths} langs=[{string.Join(",", strings.Keys)}]");
        foreach (var n in notes) Console.WriteLine("  note: " + n);
    }

    // All string candidates across all PRIs: key -> lang -> value (first PRI wins; resources.pri is processed first).
    // Keys: the app's own "Resources/<key>[/<sub>]" subtree is keyed relative to "Resources/" (the form used by
    // ResourceLoader.GetString / x:Uid, e.g. "Foo" or "Foo/Text"); other subtrees keep their full path
    // (e.g. "Elgato.BaseClasses.WinUI/Resources/Common_OK").
    static readonly SortedDictionary<string, SortedDictionary<string, string>> AllStrings = new(StringComparer.Ordinal);

    static void AddMerged(string lang, string priName, string fullName, string value)
    {
        string key = fullName.StartsWith("Resources/", StringComparison.Ordinal) ? fullName.Substring("Resources/".Length) : fullName;
        if (!AllStrings.TryGetValue(key, out var byLang))
            AllStrings[key] = byLang = new SortedDictionary<string, string>(StringComparer.Ordinal);
        if (byLang.TryGetValue(lang, out var existing))
        {
            if (existing != value)
                MergeConflicts.Add($"{lang}\t{key}\tkept={existing}\tdropped({priName})={value}");
            return;
        }
        byLang[lang] = value;
    }

    // Resolved per-language view, mimicking MRT language fallback: exact tag, then the base language
    // (e.g. en-US -> en), then any other region of the same base language (en -> en-US).
    static void BuildResolvedViews()
    {
        var langs = AllStrings.Values.SelectMany(d => d.Keys).Distinct().ToList();
        foreach (string target in langs)
        {
            string baseLang = target.Split('-')[0];
            var dict = new SortedDictionary<string, string>(StringComparer.Ordinal);
            foreach (var (key, byLang) in AllStrings)
            {
                if (byLang.TryGetValue(target, out var v) || byLang.TryGetValue(baseLang, out v))
                {
                    dict[key] = v;
                    continue;
                }
                var alt = byLang.Keys.Where(l => l.Split('-')[0] == baseLang).OrderBy(l => l, StringComparer.Ordinal).FirstOrDefault();
                if (alt != null)
                    dict[key] = byLang[alt];
            }
            MergedStrings[target] = dict;
        }
    }

    static string Decode(ResourceValueType type, byte[] data) => type switch
    {
        ResourceValueType.AsciiString or ResourceValueType.AsciiPath => Encoding.ASCII.GetString(data).TrimEnd('\0'),
        ResourceValueType.Utf8String or ResourceValueType.Utf8Path => Encoding.UTF8.GetString(data).TrimEnd('\0'),
        _ => Encoding.Unicode.GetString(data).TrimEnd('\0'),
    };

    static string NormalizeLang(string v)
    {
        var parts = v.Split('-', StringSplitOptions.RemoveEmptyEntries);
        for (int i = 0; i < parts.Length; i++)
        {
            string p = parts[i];
            parts[i] = i == 0 ? p.ToLowerInvariant()
                : p.Length == 4 ? char.ToUpperInvariant(p[0]) + p.Substring(1).ToLowerInvariant()
                : p.ToUpperInvariant();
        }
        return string.Join("-", parts);
    }

    static string SanitizeRelPath(string name)
    {
        var parts = name.Replace('\\', '/').Split('/', StringSplitOptions.RemoveEmptyEntries)
            .Select(p => p == ".." || p == "." ? "_" : string.Concat(p.Select(ch => Path.GetInvalidFileNameChars().Contains(ch) || ch == ':' ? '_' : ch)));
        return Path.Combine(parts.ToArray());
    }

    static string InsertQualifierTag(string rel, IReadOnlyList<Qualifier> qualifiers)
    {
        string tag = string.Join("_", qualifiers.Select(q => $"{q.Type.ToString().ToLowerInvariant()}-{q.Value}"));
        string dir = Path.GetDirectoryName(rel) ?? "";
        string file = Path.GetFileNameWithoutExtension(rel);
        string ext = Path.GetExtension(rel);
        return Path.Combine(dir, $"{file}.{tag}{ext}");
    }
}
