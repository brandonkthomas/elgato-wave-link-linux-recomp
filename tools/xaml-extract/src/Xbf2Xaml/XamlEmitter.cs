// Writes a WidgetSpinner XamlObject graph back out as XAML text.
//
// Conventions (chosen to read like hand-written WinUI XAML while staying lossless):
//   - simple values (numbers, strings, enums, colors, thickness, markup extensions) become attributes
//   - complex values become property elements <Owner.Property>
//   - the type's [ContentProperty] value is written as direct children (after property elements)
//   - collections created implicitly by the XBF (PushScopeGetValue) are not written as elements
//   - floats are written in shortest round-trip form; private-use glyphs as &#xE700; escapes
//   - x:ConnectionId is kept (it links elements to the generated code-behind / x:Bind code)

using System.Globalization;
using System.Text;
using Microsoft.Xaml.WidgetSpinner.Metadata;
using Microsoft.Xaml.WidgetSpinner.Metadata.KnownTypes;
using Microsoft.Xaml.WidgetSpinner.Model;

sealed class XElem
{
    public string Name;
    public readonly List<KeyValuePair<string, string>> Attrs = new();
    public readonly List<object> Children = new(); // XElem | XComment
    public string Text;                             // simple text content (mutually exclusive with Children)
    public bool PreserveSpace;
}

sealed class XComment
{
    public string Text;
}

sealed class XamlEmitter
{
    public const string PresentationNs = "http://schemas.microsoft.com/winfx/2006/xaml/presentation";
    public const string XamlNs = "http://schemas.microsoft.com/winfx/2006/xaml";

    static readonly CultureInfo Inv = CultureInfo.InvariantCulture;

    // XBF primitive type names -> XAML language intrinsics
    static readonly Dictionary<string, string> XPrimitives = new(StringComparer.Ordinal)
    {
        ["Windows.Foundation.String"] = "String",
        ["Windows.Foundation.Double"] = "Double",
        ["Windows.Foundation.Boolean"] = "Boolean",
        ["Windows.Foundation.Int32"] = "Int32",
        ["Windows.Foundation.Int64"] = "Int64",
        ["Windows.Foundation.Int16"] = "Int16",
        ["Windows.Foundation.UInt16"] = "UInt16",
        ["Windows.Foundation.UInt32"] = "UInt32",
        ["Windows.Foundation.UInt64"] = "UInt64",
        ["Windows.Foundation.Byte"] = "Byte",
        ["Windows.Foundation.Char16"] = "Char",
        ["Windows.Foundation.Float"] = "Single",
        ["Windows.Foundation.Object"] = "Object",
        ["Windows.Foundation.TimeSpan"] = "TimeSpan",
        ["Windows.Foundation.DateTime"] = "DateTime",
        ["Windows.Foundation.Guid"] = "Guid",
        ["Windows.Foundation.Uri"] = "Uri",
        ["Windows.UI.Xaml.Interop.TypeName"] = "Type",
    };

    // [ContentProperty] for framework types where the projection does not carry the attribute
    // (or the type is internal to the XAML parser).
    static readonly Dictionary<string, string> ContentFallback = new(StringComparer.Ordinal)
    {
        ["Microsoft.UI.Xaml.FrameworkTemplate"] = "Template",
        ["Microsoft.UI.Xaml.Style"] = "Setters",
        ["Microsoft.UI.Xaml.VisualState"] = "Storyboard",
        ["Microsoft.UI.Xaml.VisualStateGroup"] = "States",
        ["Microsoft.UI.Xaml.Media.Animation.Storyboard"] = "Children",
        ["Microsoft.UI.Xaml.Controls.Panel"] = "Children",
        ["Microsoft.UI.Xaml.Controls.Border"] = "Child",
        ["Microsoft.UI.Xaml.Controls.Viewbox"] = "Child",
        ["Microsoft.UI.Xaml.Controls.Primitives.Popup"] = "Child",
        ["Microsoft.UI.Xaml.Controls.ContentControl"] = "Content",
        ["Microsoft.UI.Xaml.Controls.ContentPresenter"] = "Content",
        ["Microsoft.UI.Xaml.Controls.UserControl"] = "Content",
        ["Microsoft.UI.Xaml.Controls.Page"] = "Content",
        ["Microsoft.UI.Xaml.Window"] = "Content",
        ["Microsoft.UI.Xaml.Controls.Flyout"] = "Content",
        ["Microsoft.UI.Xaml.Controls.ItemsControl"] = "Items",
        ["Microsoft.UI.Xaml.Controls.MenuFlyout"] = "Items",
        ["Microsoft.UI.Xaml.Controls.TextBlock"] = "Inlines",
        ["Microsoft.UI.Xaml.Controls.RichTextBlock"] = "Blocks",
        ["Microsoft.UI.Xaml.Documents.Paragraph"] = "Inlines",
        ["Microsoft.UI.Xaml.Documents.Span"] = "Inlines",
        ["Microsoft.UI.Xaml.Documents.Run"] = "Text",
        ["Microsoft.UI.Xaml.Media.GradientBrush"] = "GradientStops",
        ["Microsoft.UI.Xaml.Media.PathGeometry"] = "Figures",
        ["Microsoft.UI.Xaml.Media.PathFigure"] = "Segments",
        ["Microsoft.UI.Xaml.Media.GeometryGroup"] = "Children",
        ["Microsoft.UI.Xaml.Media.TransformGroup"] = "Children",
        ["Microsoft.UI.Xaml.Media.Animation.DoubleAnimationUsingKeyFrames"] = "KeyFrames",
        ["Microsoft.UI.Xaml.Media.Animation.ColorAnimationUsingKeyFrames"] = "KeyFrames",
        ["Microsoft.UI.Xaml.Media.Animation.PointAnimationUsingKeyFrames"] = "KeyFrames",
        ["Microsoft.UI.Xaml.Media.Animation.ObjectAnimationUsingKeyFrames"] = "KeyFrames",
    };

    readonly MetadataIndex _meta;
    readonly EnumTable _enums;
    readonly Dictionary<string, string> _frameworkBase; // framework type full name -> base type full name
    readonly List<Dictionary<string, string>> _scopes = new(); // prefix -> uri, innermost last
    readonly Dictionary<string, string> _synthByUri = new(StringComparer.Ordinal);
    readonly Dictionary<string, string> _synthByPrefix = new(StringComparer.Ordinal);

    public readonly List<string> Issues = new();
    public readonly HashSet<string> ContentPropertyFallbacksUsed = new(StringComparer.Ordinal);

    public XamlEmitter(MetadataIndex meta, EnumTable enums)
    {
        _meta = meta;
        _enums = enums;
        _frameworkBase = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var kv in StableXbfIndexMetadata.StableXbfTypeIndexToTypeInfo)
        {
            var ti = kv.Value;
            if (ti.BaseTypeIndex != StableXbfTypeIndex.UnknownType &&
                StableXbfIndexMetadata.StableXbfTypeIndexToTypeInfo.TryGetValue(ti.BaseTypeIndex, out var bi))
                _frameworkBase[ti.FullName] = bi.FullName;
        }
    }

    // ------------------------------------------------------------------ entry point

    public string EmitDocument(object root, string headerComment)
    {
        _scopes.Clear();
        _synthByUri.Clear();
        _synthByPrefix.Clear();

        if (root is not XamlObject ro)
            throw new InvalidOperationException($"root is not an object ({root?.GetType().Name ?? "null"})");

        // a document-level scope that receives synthesized namespace prefixes
        _scopes.Add(new Dictionary<string, string>(StringComparer.Ordinal));
        XElem rootEl = EmitObject(ro, null);

        // namespaces that were needed but not declared in the source (or declared under another URI form)
        foreach (var kv in _synthByPrefix)
            rootEl.Attrs.Insert(0, new KeyValuePair<string, string>(kv.Key.Length == 0 ? "xmlns" : "xmlns:" + kv.Key, kv.Value));

        var sb = new StringBuilder();
        sb.Append("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
        if (headerComment != null)
            sb.Append("<!-- ").Append(headerComment.Replace("--", "- -")).Append(" -->\n");
        Write(sb, rootEl, 0);
        return sb.ToString();
    }

    // ------------------------------------------------------------------ objects

    XElem EmitObject(XamlObject o, object key)
    {
        var el = new XElem();
        var scope = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var ns in o.Namespaces)
            scope[ns.Key] = ns.Value.NamespaceUri;
        _scopes.Add(scope);
        try
        {
            foreach (var ns in o.Namespaces.OrderBy(n => n.Key.Length == 0 ? "" : n.Key, StringComparer.Ordinal))
                el.Attrs.Add(new(ns.Key.Length == 0 ? "xmlns" : "xmlns:" + ns.Key, ns.Value.NamespaceUri));

            el.Name = TypeQName(o.Type);

            string keyText = key switch
            {
                null => null,
                string s => s,
                XamlType => null, // implicit style key == TargetType
                _ => Convert.ToString(key, Inv),
            };

            string contentProp = ContentPropertyOf(o.Type);
            var directives = new List<KeyValuePair<string, string>>();
            var attrs = new List<KeyValuePair<string, string>>();
            var propElems = new List<XElem>();
            List<object> contentChildren = null;

            foreach (var kv in o.PropertyStore)
            {
                XamlProperty p = kv.Key;
                object v = kv.Value;

                if (p.DeclaringType == null)
                {
                    directives.Add(new(DirectiveName(p.Name), FormatDirectiveValue(v)));
                    continue;
                }
                if (IsDependencyObjectName(p))
                {
                    directives.Add(new(XPrefixed("Name"), Convert.ToString(v, Inv)));
                    continue;
                }

                bool attached = IsAttached(p, o.Type);
                if (v is not TypedConstant)
                {
                    string inline = TryFormatInline(v, false);
                    if (inline != null)
                    {
                        attrs.Add(new(AttrName(p, o.Type, attached), inline));
                        continue;
                    }
                }

                var children = EmitValueAsChildren(v);
                if (!attached && contentProp != null && PropShortName(p) == contentProp)
                    contentChildren = children;
                else
                {
                    var pe = new XElem { Name = PropElemName(p, o.Type, attached) };
                    pe.Children.AddRange(children);
                    propElems.Add(pe);
                }
            }

            // directive order: x:Class, x:Key, x:Name, x:Uid, then the rest (x:ConnectionId, x:Load, ...)
            string Rank(string n) => n.EndsWith(":Class") ? "0" : n.EndsWith(":Name") ? "2" : n.EndsWith(":Uid") ? "3" : "4" + n;
            var orderedDirectives = directives.OrderBy(d => Rank(d.Key), StringComparer.Ordinal).ToList();
            int insertAt = orderedDirectives.TakeWhile(d => Rank(d.Key) == "0").Count();
            if (keyText != null)
                orderedDirectives.Insert(insertAt, new(XPrefixed("Key"), keyText));
            el.Attrs.AddRange(orderedDirectives);
            el.Attrs.AddRange(attrs);

            el.Children.AddRange(propElems);

            foreach (var item in o.CollectionItems)
                el.Children.AddRange(EmitItem(item, null));
            foreach (var k in o.DictionaryKeysInOrder)
                el.Children.AddRange(EmitItem(o.DictionaryItems[k], k));

            if (contentChildren != null)
                el.Children.AddRange(contentChildren);

            return el;
        }
        finally
        {
            _scopes.RemoveAt(_scopes.Count - 1);
        }
    }

    List<object> EmitValueAsChildren(object v)
    {
        var list = new List<object>();
        switch (v)
        {
            case XamlObject xo when xo.IsImplicit && xo.PropertyStore.Count == 0:
                foreach (var item in xo.CollectionItems)
                    list.AddRange(EmitItem(item, null));
                foreach (var k in xo.DictionaryKeysInOrder)
                    list.AddRange(EmitItem(xo.DictionaryItems[k], k));
                break;
            case XamlObject xo:
                list.Add(EmitObject(xo, null));
                break;
            default:
                list.AddRange(EmitItem(v, null));
                break;
        }
        return list;
    }

    IEnumerable<object> EmitItem(object item, object key)
    {
        switch (item)
        {
            case XamlObject xo:
                yield return EmitObject(xo, key);
                break;
            case TypedConstant tc:
                yield return TypedElement(tc.Type, tc.Value, key);
                break;
            case TypeConvertedValue tcv:
                yield return TypedElement(tcv.TargetType, tcv.SourceValue, key);
                break;
            case XamlType xt:
                // e.g. a resolved type used as a collection item; write as x:Type-like text node
                yield return TypedElement(null, xt, key);
                break;
            default:
                yield return TypedElement(null, item, key);
                break;
        }
    }

    XElem TypedElement(XamlType type, object value, object key)
    {
        string typeName = type != null && !type.FullName.StartsWith("TypeOfProperty_", StringComparison.Ordinal)
            ? TypeQName(type)
            : InferPrimitiveElementName(value);
        var el = new XElem { Name = typeName };
        if (key is string ks)
            el.Attrs.Add(new(XPrefixed("Key"), ks));
        string text = value == null ? "" : TryFormatInline(value, false, asText: true) ?? Convert.ToString(value, Inv);
        el.Text = text;
        if (value is string s && (s.Length > 0 && (char.IsWhiteSpace(s[0]) || char.IsWhiteSpace(s[^1]) || s.Contains("  ") || s.Contains('\n') || s.Contains('\t'))))
            el.PreserveSpace = true;
        return el;
    }

    string InferPrimitiveElementName(object value) => value switch
    {
        string => XPrefixed("String"),
        float or double => XPrefixed("Double"),
        int or long => XPrefixed("Int32"),
        bool => XPrefixed("Boolean"),
        Color => DefaultPrefixed("Color"),
        Thickness => DefaultPrefixed("Thickness"),
        GridLength => DefaultPrefixed("GridLength"),
        EnumValue ev => TypeQName(XamlTypeRegistry.Instance.GetXamlTypeByFullName(ev.EnumName)),
        XamlType => XPrefixed("Type"),
        _ => XPrefixed("Object"),
    };

    // ------------------------------------------------------------------ values

    // Returns the attribute (or element text) form of a value, or null if it needs element syntax.
    string TryFormatInline(object v, bool inMarkup, bool asText = false)
    {
        switch (v)
        {
            case null:
                return "{" + XPrefixed("Null") + "}";
            case string s:
                if (inMarkup) return QuoteMarkupValue(s);
                if (!asText && s.StartsWith("{", StringComparison.Ordinal)) return "{}" + s;
                return s;
            case bool b:
                return b ? "True" : "False";
            case float f:
                return Num(f);
            case double d:
                return d.ToString("R", Inv);
            case int i:
                return i.ToString(Inv);
            case long l:
                return l.ToString(Inv);
            case uint ui:
                return ui.ToString(Inv);
            case Color c:
                return $"#{c.A:X2}{c.R:X2}{c.G:X2}{c.B:X2}";
            case Thickness t:
                return FormatThickness(t);
            case GridLength g:
                return g.GridUnitType switch
                {
                    GridUnitType.Auto => "Auto",
                    GridUnitType.Pixel => Num((float)g.Value),
                    GridUnitType.Star => g.Value == 1 ? "*" : Num((float)g.Value) + "*",
                    _ => Num((float)g.Value) + "?unit" + (int)g.GridUnitType,
                };
            case EnumValue ev:
                {
                    string n = _enums.Name(ev.EnumName, ev.Value);
                    if (n == null)
                    {
                        Issues.Add($"enum value without name: {ev.EnumName}={ev.Value} (written as number)");
                        return ev.Value.ToString(Inv);
                    }
                    return n;
                }
            case TypeConvertedValue tc:
                return TryFormatInline(tc.SourceValue, inMarkup, asText);
            case TypedConstant tcv:
                return TryFormatInline(tcv.Value, inMarkup, asText);
            case XamlType xt:
                return TypeQName(xt);
            case XamlProperty xp:
                return PropertyReference(xp);
            case XamlObject xo when IsMarkupExtension(xo) && !asText:
                return FormatMarkupExtension(xo);
            default:
                return null;
        }
    }

    bool IsMarkupExtension(XamlObject xo)
    {
        if (xo.Type.IsMarkupExtension) return true;
        foreach (var t in BaseChain(xo.Type.FullName))
            if (t == "Microsoft.UI.Xaml.Markup.MarkupExtension" || t == "Microsoft.UI.Xaml.Data.BindingBase" || t == "Microsoft.UI.Xaml.MarkupExtensionBase")
                return true;
        return false;
    }

    string FormatMarkupExtension(XamlObject xo)
    {
        switch (xo)
        {
            case XamlStaticResourceExtension sr:
                return "{StaticResource " + QuoteMarkupValue(sr.ResourceKey) + "}";
            case XamlThemeResourceExtension tr:
                return "{ThemeResource " + QuoteMarkupValue(tr.ResourceKey) + "}";
            case XamlCustomResourceExtension cr:
                return "{CustomResource " + QuoteMarkupValue(cr.ResourceKey) + "}";
            case XamlTemplateBindingExtension tb:
                return "{TemplateBinding " + (tb.Property != null ? PropertyReference(tb.Property) : "") + "}";
            case XamlNullExtension:
                return "{" + XPrefixed("Null") + "}";
        }

        string typeName = TypeQName(xo.Type);
        if (xo.Type.FullName == "Microsoft.UI.Xaml.Data.Binding") typeName = "Binding";
        if (xo.Type.FullName == "Microsoft.UI.Xaml.Data.RelativeSource") typeName = "RelativeSource";

        var parts = new List<string>();
        bool first = true;
        foreach (var kv in xo.PropertyStore)
        {
            string name = kv.Key.DeclaringType == null ? DirectiveName(kv.Key.Name) : PropShortName(kv.Key);
            string val = TryFormatInline(kv.Value, true);
            if (val == null) return null; // needs element syntax
            // positional argument for the common forms: {Binding Path}, {RelativeSource Mode}
            if (first && ((typeName == "Binding" && name == "Path") || (typeName == "RelativeSource" && name == "Mode")))
                parts.Add(val);
            else
                parts.Add(name + "=" + val);
            first = false;
        }
        return "{" + typeName + (parts.Count > 0 ? " " + string.Join(", ", parts) : "") + "}";
    }

    static string QuoteMarkupValue(string s)
    {
        if (s == null) return "''";
        if (s.Length > 0 && s.IndexOfAny(new[] { ',', '{', '}', '=', '\'', '"', '\\' }) < 0 && !char.IsWhiteSpace(s[0]) && !char.IsWhiteSpace(s[^1]))
            return s;
        return "'" + s.Replace("\\", "\\\\").Replace("'", "\\'") + "'";
    }

    static string Num(float f)
    {
        if (float.IsPositiveInfinity(f)) return "Infinity";
        if (float.IsNegativeInfinity(f)) return "-Infinity";
        if (float.IsNaN(f)) return "NaN";
        return f.ToString("R", Inv);
    }

    static string FormatThickness(Thickness t)
    {
        if (t.Left == t.Right && t.Top == t.Bottom)
            return t.Left == t.Top ? Num(t.Left) : $"{Num(t.Left)},{Num(t.Top)}";
        return $"{Num(t.Left)},{Num(t.Top)},{Num(t.Right)},{Num(t.Bottom)}";
    }

    string FormatDirectiveValue(object v) => v switch
    {
        bool b => b ? "True" : "False",
        TypeConvertedValue tc => Convert.ToString(tc.SourceValue, Inv),
        TypedConstant tcv => Convert.ToString(tcv.Value, Inv),
        float f => Num(f),
        _ => Convert.ToString(v, Inv),
    };

    // ------------------------------------------------------------------ names

    static bool IsDependencyObjectName(XamlProperty p) =>
        p.Name == "Name" && p.DeclaringType != null && p.DeclaringType.FullName == "Microsoft.UI.Xaml.DependencyObject";

    static string PropShortName(XamlProperty p)
    {
        int i = p.Name.LastIndexOf('.');
        return i < 0 ? p.Name : p.Name.Substring(i + 1);
    }

    string DirectiveName(string name)
    {
        // WidgetSpinner names directives "x:Class", "x:Uid", "x:ConnectionId", "x:Load", ...
        if (name.StartsWith("x:", StringComparison.Ordinal))
            return XPrefixed(name.Substring(2));
        return name;
    }

    string AttrName(XamlProperty p, XamlType owner, bool attached)
    {
        if (!attached) return PropShortName(p);
        return TypeQName(p.DeclaringType) + "." + PropShortName(p);
    }

    string PropElemName(XamlProperty p, XamlType owner, bool attached)
    {
        if (attached) return TypeQName(p.DeclaringType) + "." + PropShortName(p);
        return TypeQName(owner) + "." + PropShortName(p);
    }

    // Property used as a value (Setter.Property, TemplateBinding): "Background", "Grid.Row", "local:Foo.Bar"
    string PropertyReference(XamlProperty p)
    {
        if (p.DeclaringType == null) return p.Name;
        if (p.IsAttached || (_meta.Get(p.DeclaringType.FullName)?.AttachedProperties.Contains(p.Name) ?? false))
        {
            string owner = TypeQName(p.DeclaringType);
            return owner + "." + PropShortName(p);
        }
        return PropShortName(p);
    }

    bool IsAttached(XamlProperty p, XamlType owner)
    {
        if (p.DeclaringType == null) return false;
        if (p.IsAttached) return true; // framework attached property (name is "Owner.Prop")
        string decl = p.DeclaringType.FullName;
        bool derives = BaseChain(owner.FullName).Contains(decl);
        if (derives) return false;
        var dm = _meta.Get(decl);
        if (dm != null && dm.AttachedProperties.Contains(p.Name)) return true;
        // framework instance properties on custom types whose base chain we could not resolve
        if (IsFrameworkNamespace(Ns(decl))) return false;
        return true;
    }

    IEnumerable<string> BaseChain(string fullName) =>
        _meta.BaseChain(fullName, t => _frameworkBase.TryGetValue(t, out var b) ? b : null);

    string ContentPropertyOf(XamlType t)
    {
        foreach (var name in BaseChain(t.FullName))
        {
            var tm = _meta.Get(name);
            if (tm?.ContentProperty != null) return tm.ContentProperty;
            if (ContentFallback.TryGetValue(name, out var cp))
            {
                ContentPropertyFallbacksUsed.Add(name);
                return cp;
            }
        }
        return null;
    }

    static string Ns(string full)
    {
        int i = full.LastIndexOf('.');
        return i < 0 ? "" : full.Substring(0, i);
    }

    static string ShortName(string full)
    {
        int i = full.LastIndexOf('.');
        return i < 0 ? full : full.Substring(i + 1);
    }

    static bool IsFrameworkNamespace(string ns) =>
        ns.StartsWith("Microsoft.UI.Xaml", StringComparison.Ordinal) ||
        ns == "Microsoft.UI.Text" ||
        ns.StartsWith("Windows.UI", StringComparison.Ordinal) ||
        ns == "Windows.Foundation";

    string TypeQName(XamlType t)
    {
        string full = t.FullName;
        if (XPrimitives.TryGetValue(full, out var xn)) return XPrefixed(xn);
        if (full == "Microsoft.UI.Xaml.Internal.DeferredElement") return XPrefixed("DeferredElement");

        string ns = Ns(full);
        string name = ShortName(full);
        string usingUri = "using:" + ns;
        string p = FindPrefix(usingUri) ?? FindPrefix("clr-namespace:" + ns);
        if (p != null) return Join(p, name);
        if (IsFrameworkNamespace(ns)) return DefaultPrefixed(name);
        if (full.StartsWith("TypeOfProperty_", StringComparison.Ordinal))
        {
            Issues.Add($"unresolved type placeholder {full}");
            return name;
        }
        p = EnsurePrefix(usingUri, SuggestPrefix(ns));
        return Join(p, name);
    }

    static string Join(string prefix, string name) => string.IsNullOrEmpty(prefix) ? name : prefix + ":" + name;

    string XPrefixed(string name) => Join(FindPrefix(XamlNs) ?? EnsurePrefix(XamlNs, "x"), name);

    string DefaultPrefixed(string name) => Join(FindPrefix(PresentationNs) ?? EnsurePrefix(PresentationNs, ""), name);

    string FindPrefix(string uri)
    {
        for (int i = _scopes.Count - 1; i >= 0; i--)
            foreach (var kv in _scopes[i])
                if (kv.Value == uri && !IsShadowed(kv.Key, i))
                    return kv.Key;
        return _synthByUri.TryGetValue(uri, out var sp) ? sp : null;
    }

    bool IsShadowed(string prefix, int scopeIndex)
    {
        for (int j = scopeIndex + 1; j < _scopes.Count; j++)
            if (_scopes[j].ContainsKey(prefix)) return true;
        return false;
    }

    bool PrefixInUse(string prefix)
    {
        if (_synthByPrefix.ContainsKey(prefix)) return true;
        foreach (var s in _scopes)
            if (s.ContainsKey(prefix)) return true;
        return false;
    }

    string EnsurePrefix(string uri, string suggested)
    {
        if (_synthByUri.TryGetValue(uri, out var existing)) return existing;
        string prefix = suggested;
        int n = 2;
        while (PrefixInUse(prefix)) prefix = (suggested.Length == 0 ? "ns" : suggested) + n++;
        _synthByUri[uri] = prefix;
        _synthByPrefix[prefix] = uri;
        return prefix;
    }

    static string SuggestPrefix(string ns)
    {
        string last = ShortName(ns);
        var sb = new StringBuilder();
        foreach (char c in last) if (char.IsLetterOrDigit(c)) sb.Append(char.ToLowerInvariant(c));
        return sb.Length == 0 ? "ns" : sb.ToString();
    }

    // ------------------------------------------------------------------ serialization

    static void Write(StringBuilder sb, XElem el, int depth)
    {
        string ind = new string(' ', depth * 4);
        sb.Append(ind).Append('<').Append(el.Name);
        if (el.PreserveSpace) el.Attrs.Add(new("xml:space", "preserve"));
        bool multiline = el.Attrs.Count > 3 || el.Attrs.Sum(a => a.Key.Length + a.Value.Length + 4) > 110;
        foreach (var a in el.Attrs)
        {
            if (multiline) sb.Append('\n').Append(ind).Append("    ");
            else sb.Append(' ');
            sb.Append(a.Key).Append("=\"").Append(EscapeAttr(a.Value)).Append('"');
        }
        if (el.Children.Count == 0 && el.Text == null)
        {
            sb.Append(" />\n");
            return;
        }
        sb.Append('>');
        if (el.Text != null && el.Children.Count == 0)
        {
            sb.Append(EscapeText(el.Text)).Append("</").Append(el.Name).Append(">\n");
            return;
        }
        sb.Append('\n');
        if (el.Text != null)
            sb.Append(ind).Append("    ").Append(EscapeText(el.Text)).Append('\n');
        foreach (var c in el.Children)
        {
            if (c is XElem ce) Write(sb, ce, depth + 1);
            else if (c is XComment cm) sb.Append(ind).Append("    <!-- ").Append(cm.Text.Replace("--", "- -")).Append(" -->\n");
        }
        sb.Append(ind).Append("</").Append(el.Name).Append(">\n");
    }

    static bool NeedsCharRef(char c) =>
        (c < 0x20 && c != '\n' && c != '\t' && c != '\r') ||
        (c >= 0x7F && c <= 0x9F) ||
        (c >= 0xE000 && c <= 0xF8FF) ||                 // private use (icon font glyphs)
        c == ' ' || c == '​' || c == '‌' || c == '‍' || c == '⁠' || c == '﻿' ||
        char.IsSurrogate(c) || c == '￾' || c == '￿';

    static string EscapeAttr(string s)
    {
        var sb = new StringBuilder(s.Length + 8);
        foreach (char c in s)
        {
            switch (c)
            {
                case '&': sb.Append("&amp;"); break;
                case '<': sb.Append("&lt;"); break;
                case '>': sb.Append("&gt;"); break;
                case '"': sb.Append("&quot;"); break;
                case '\n': sb.Append("&#xA;"); break;
                case '\r': sb.Append("&#xD;"); break;
                case '\t': sb.Append("&#x9;"); break;
                default:
                    if (NeedsCharRef(c)) sb.Append("&#x").Append(((int)c).ToString("X", Inv)).Append(';');
                    else sb.Append(c);
                    break;
            }
        }
        return sb.ToString();
    }

    static string EscapeText(string s)
    {
        var sb = new StringBuilder(s.Length + 8);
        foreach (char c in s)
        {
            switch (c)
            {
                case '&': sb.Append("&amp;"); break;
                case '<': sb.Append("&lt;"); break;
                case '>': sb.Append("&gt;"); break;
                case '\r': sb.Append("&#xD;"); break;
                default:
                    if (NeedsCharRef(c)) sb.Append("&#x").Append(((int)c).ToString("X", Inv)).Append(';');
                    else sb.Append(c);
                    break;
            }
        }
        return sb.ToString();
    }
}
