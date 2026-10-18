// Type metadata harvested (read-only, via System.Reflection.Metadata) from the managed assemblies that ship
// next to the app: the WinUI/SDK projections (Microsoft.WinUI.dll, Microsoft.Windows.SDK.NET.dll, ...) and the
// app/library assemblies. Nothing is loaded for execution.
//
// Used by the XAML emitter for:
//   - base-type chains of custom types (decides whether a property is attached or an instance property)
//   - [ContentProperty] (to write content-property values as element children, like hand-written XAML)
//   - attached-property owners (static GetX/SetX + XProperty)
//   - enum value names

using System.Collections.Immutable;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;

sealed class TypeMeta
{
    public string FullName;
    public string BaseFullName;
    public string Assembly;
    public bool IsEnum;
    public bool IsFlags;
    public readonly List<KeyValuePair<long, string>> EnumValues = new();
    public string ContentProperty;
    public readonly HashSet<string> AttachedProperties = new(StringComparer.Ordinal);
    public readonly HashSet<string> InstanceProperties = new(StringComparer.Ordinal);
}

sealed class MetadataIndex
{
    public readonly Dictionary<string, TypeMeta> Types = new(StringComparer.Ordinal);
    public readonly List<string> LoadedAssemblies = new();

    public void LoadDirectory(string dir)
    {
        foreach (var path in Directory.EnumerateFiles(dir, "*.dll").OrderBy(p => p, StringComparer.OrdinalIgnoreCase))
        {
            try { LoadAssembly(path); }
            catch (BadImageFormatException) { }
            catch (InvalidOperationException) { }
        }
    }

    public void LoadAssembly(string path)
    {
        using var fs = File.OpenRead(path);
        using var pe = new PEReader(fs);
        if (!pe.HasMetadata) return;
        var md = pe.GetMetadataReader();
        if (!md.IsAssembly) return;
        string asmName = md.GetString(md.GetAssemblyDefinition().Name);
        LoadedAssemblies.Add(asmName);

        foreach (var h in md.TypeDefinitions)
        {
            var td = md.GetTypeDefinition(h);
            if (td.GetDeclaringType().IsNil == false) continue; // nested types never appear in XAML
            string ns = md.GetString(td.Namespace);
            string name = md.GetString(td.Name);
            if (name == "<Module>") continue;
            string full = string.IsNullOrEmpty(ns) ? name : ns + "." + name;
            if (Types.ContainsKey(full)) continue; // first assembly wins

            var tm = new TypeMeta { FullName = full, Assembly = asmName, BaseFullName = GetTypeName(md, td.BaseType) };
            tm.IsEnum = tm.BaseFullName == "System.Enum";

            foreach (var ah in td.GetCustomAttributes())
            {
                var attr = md.GetCustomAttribute(ah);
                string attrType = GetAttributeTypeName(md, attr);
                if (attrType == "System.FlagsAttribute")
                    tm.IsFlags = true;
                else if (attrType != null && attrType.EndsWith(".ContentPropertyAttribute", StringComparison.Ordinal))
                {
                    try
                    {
                        var val = attr.DecodeValue(new CaTypeProvider());
                        var named = val.NamedArguments.FirstOrDefault(a => a.Name == "Name");
                        if (named.Value is string s) tm.ContentProperty = s;
                        else if (val.FixedArguments.Length > 0 && val.FixedArguments[0].Value is string s2) tm.ContentProperty = s2;
                    }
                    catch { }
                }
            }

            if (tm.IsEnum)
            {
                foreach (var fh in td.GetFields())
                {
                    var fd = md.GetFieldDefinition(fh);
                    if ((fd.Attributes & FieldAttributes.Literal) == 0) continue;
                    var ch = fd.GetDefaultValue();
                    if (ch.IsNil) continue;
                    long? v = ReadConstant(md, md.GetConstant(ch));
                    if (v != null) tm.EnumValues.Add(new KeyValuePair<long, string>(v.Value, md.GetString(fd.Name)));
                }
            }
            else
            {
                var staticMembers = new HashSet<string>(StringComparer.Ordinal);
                var staticGetters = new HashSet<string>(StringComparer.Ordinal);
                foreach (var fh in td.GetFields())
                {
                    var fd = md.GetFieldDefinition(fh);
                    if ((fd.Attributes & FieldAttributes.Static) != 0) staticMembers.Add(md.GetString(fd.Name));
                }
                foreach (var ph in td.GetProperties())
                {
                    var pd = md.GetPropertyDefinition(ph);
                    string pname = md.GetString(pd.Name);
                    var acc = pd.GetAccessors();
                    var mh = !acc.Getter.IsNil ? acc.Getter : acc.Setter;
                    bool isStatic = !mh.IsNil && (md.GetMethodDefinition(mh).Attributes & MethodAttributes.Static) != 0;
                    if (isStatic) staticMembers.Add(pname); else tm.InstanceProperties.Add(pname);
                }
                foreach (var mh in td.GetMethods())
                {
                    var mdef = md.GetMethodDefinition(mh);
                    if ((mdef.Attributes & MethodAttributes.Static) == 0) continue;
                    string mname = md.GetString(mdef.Name);
                    if (mname.StartsWith("Get", StringComparison.Ordinal) && mname.Length > 3 && mdef.GetParameters().Count >= 1)
                        staticGetters.Add(mname.Substring(3));
                }
                foreach (var g in staticGetters)
                    if (staticMembers.Contains(g + "Property"))
                        tm.AttachedProperties.Add(g);
            }

            Types[full] = tm;
        }
    }

    public TypeMeta Get(string fullName) => fullName != null && Types.TryGetValue(fullName, out var t) ? t : null;

    // Walks the base chain using assembly metadata, falling back to the supplied resolver (XBF framework metadata).
    public IEnumerable<string> BaseChain(string fullName, Func<string, string> fallbackBase)
    {
        var seen = new HashSet<string>(StringComparer.Ordinal);
        string cur = fullName;
        while (cur != null && seen.Add(cur))
        {
            yield return cur;
            string next = Get(cur)?.BaseFullName;
            if (next == null || next == "System.Object" || next == "WinRT.IWinRTObject")
                next = fallbackBase?.Invoke(cur);
            cur = next;
        }
    }

    static string GetAttributeTypeName(MetadataReader md, CustomAttribute attr)
    {
        if (attr.Constructor.Kind == HandleKind.MemberReference)
        {
            var mr = md.GetMemberReference((MemberReferenceHandle)attr.Constructor);
            return GetTypeName(md, mr.Parent);
        }
        if (attr.Constructor.Kind == HandleKind.MethodDefinition)
        {
            var mdef = md.GetMethodDefinition((MethodDefinitionHandle)attr.Constructor);
            return GetTypeName(md, mdef.GetDeclaringType());
        }
        return null;
    }

    static string GetTypeName(MetadataReader md, EntityHandle h)
    {
        if (h.IsNil) return null;
        switch (h.Kind)
        {
            case HandleKind.TypeDefinition:
                {
                    var td = md.GetTypeDefinition((TypeDefinitionHandle)h);
                    string ns = md.GetString(td.Namespace);
                    string n = md.GetString(td.Name);
                    return string.IsNullOrEmpty(ns) ? n : ns + "." + n;
                }
            case HandleKind.TypeReference:
                {
                    var tr = md.GetTypeReference((TypeReferenceHandle)h);
                    string ns = md.GetString(tr.Namespace);
                    string n = md.GetString(tr.Name);
                    return string.IsNullOrEmpty(ns) ? n : ns + "." + n;
                }
            case HandleKind.TypeSpecification:
                {
                    var ts = md.GetTypeSpecification((TypeSpecificationHandle)h);
                    try { return ts.DecodeSignature(new NameProvider(md), null); } catch { return null; }
                }
            case HandleKind.MemberReference:
                {
                    var mr = md.GetMemberReference((MemberReferenceHandle)h);
                    return GetTypeName(md, mr.Parent);
                }
        }
        return null;
    }

    static long? ReadConstant(MetadataReader md, Constant c)
    {
        var br = md.GetBlobReader(c.Value);
        return c.TypeCode switch
        {
            ConstantTypeCode.SByte => br.ReadSByte(),
            ConstantTypeCode.Byte => br.ReadByte(),
            ConstantTypeCode.Int16 => br.ReadInt16(),
            ConstantTypeCode.UInt16 => br.ReadUInt16(),
            ConstantTypeCode.Int32 => br.ReadInt32(),
            ConstantTypeCode.UInt32 => br.ReadUInt32(),
            ConstantTypeCode.Int64 => br.ReadInt64(),
            ConstantTypeCode.UInt64 => (long)br.ReadUInt64(),
            _ => null,
        };
    }

    // Generic instantiations are reported by their generic type definition name ("Foo`1").
    sealed class NameProvider : ISignatureTypeProvider<string, object>
    {
        readonly MetadataReader _md;
        public NameProvider(MetadataReader md) { _md = md; }
        public string GetArrayType(string elementType, ArrayShape shape) => elementType + "[]";
        public string GetByReferenceType(string elementType) => elementType;
        public string GetFunctionPointerType(MethodSignature<string> signature) => "fnptr";
        public string GetGenericInstantiation(string genericType, ImmutableArray<string> typeArguments) => genericType;
        public string GetGenericMethodParameter(object genericContext, int index) => "!!" + index;
        public string GetGenericTypeParameter(object genericContext, int index) => "!" + index;
        public string GetModifiedType(string modifier, string unmodifiedType, bool isRequired) => unmodifiedType;
        public string GetPinnedType(string elementType) => elementType;
        public string GetPointerType(string elementType) => elementType + "*";
        public string GetPrimitiveType(PrimitiveTypeCode typeCode) => "System." + typeCode;
        public string GetSZArrayType(string elementType) => elementType + "[]";
        public string GetTypeFromDefinition(MetadataReader reader, TypeDefinitionHandle handle, byte rawTypeKind) => GetTypeName(reader, handle);
        public string GetTypeFromReference(MetadataReader reader, TypeReferenceHandle handle, byte rawTypeKind) => GetTypeName(reader, handle);
        public string GetTypeFromSpecification(MetadataReader reader, object genericContext, TypeSpecificationHandle handle, byte rawTypeKind) => GetTypeName(reader, handle);
    }

    sealed class CaTypeProvider : ICustomAttributeTypeProvider<string>
    {
        public string GetPrimitiveType(PrimitiveTypeCode typeCode) => typeCode.ToString();
        public string GetSystemType() => "System.Type";
        public string GetSZArrayType(string elementType) => elementType + "[]";
        public string GetTypeFromDefinition(MetadataReader reader, TypeDefinitionHandle handle, byte rawTypeKind) => GetTypeName(reader, handle);
        public string GetTypeFromReference(MetadataReader reader, TypeReferenceHandle handle, byte rawTypeKind) => GetTypeName(reader, handle);
        public string GetTypeFromSerializedName(string name) => name;
        public PrimitiveTypeCode GetUnderlyingEnumType(string type) => PrimitiveTypeCode.Int32;
        public bool IsSystemType(string type) => type == "System.Type";
    }
}
