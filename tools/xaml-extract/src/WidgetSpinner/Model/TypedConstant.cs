// XAML-EXTRACT: added. Keeps the declared XAML type of a constant created via
// CreateTypeWithConstantBeginInit / PushScopeCreateTypeWithConstantBeginInit
// (e.g. <x:Double x:Key="Foo">12</x:Double>), which upstream WidgetSpinner discards.

using Microsoft.Xaml.WidgetSpinner.Metadata;

namespace Microsoft.Xaml.WidgetSpinner.Model
{
    public class TypedConstant
    {
        public XamlType Type { get; }
        public object Value { get; }

        public TypedConstant(XamlType type, object value)
        {
            Type = type;
            Value = value;
        }

        public override string ToString()
        {
            return $"TypedConstant({Type}, {Value})";
        }
    }
}
