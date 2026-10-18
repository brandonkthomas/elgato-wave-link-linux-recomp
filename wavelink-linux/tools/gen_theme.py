#!/usr/bin/env python3
"""Generate the QML Theme singleton from WinUI's theme resources and Wave Link's design tokens.

Wave Link's XAML references WinUI theme resources (ControlFillColorDefaultBrush, ...) and its own
design-system tokens (variables-dark.xaml, variables-light.xaml, variables-core.xaml, ...). This script
reads both, resolves StaticResource/ThemeResource chains per theme, and writes a QML singleton whose
properties switch between the dark and light values.

usage: gen_theme.py <winui-dir> <wavelink-xaml-dir> <out Theme.qml>
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

X = "{http://schemas.microsoft.com/winfx/2006/xaml}"
REF = re.compile(r"^\{(?:StaticResource|ThemeResource)\s+([^}]+)\}$")


def local(tag):
    return tag.split("}")[-1]


class Loader:
    """Follows a XAML resource graph: Source= references, theme dictionaries and resource classes."""

    def __init__(self, root_dir, themes):
        self.root_dir = root_dir
        self.themes = themes
        self.loaded = set()
        self.classes = {}
        for f in root_dir.rglob("*.xaml"):
            m = re.search(r'x:Class="([^"]+)"', f.read_text(errors="replace")[:4000])
            if m:
                self.classes[m.group(1).split(".")[-1]] = f

    def source(self, uri):
        return self.root_dir / uri.replace("ms-appx:///", "").lstrip("/")

    def load(self, path, bucket="shared"):
        key = (str(path), bucket)
        if key in self.loaded or not path.exists():
            return
        self.loaded.add(key)
        collect(path, self.themes, bucket, self)


def collect(path, themes, bucket="shared", loader=None):
    """Add the resources of one XAML file to themes = {"dark": {}, "light": {}, "shared": {}}."""
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as e:
        print(f"skip {path}: {e}", file=sys.stderr)
        return

    def visit(node, bucket):
        for child in node:
            tag = local(child.tag)
            key = child.get(X + "Key")
            if tag == "ResourceDictionary.ThemeDictionaries":
                for td in child:
                    name = td.get(X + "Key")
                    target = {"Default": "dark", "Dark": "dark", "Light": "light"}.get(name)
                    if not target:
                        continue
                    if td.get("Source") and loader:
                        loader.load(loader.source(td.get("Source")), target)
                    visit(td, target)
                continue
            if tag == "ResourceDictionary.MergedDictionaries":
                for md in child:
                    if md.get("Source") and loader:
                        loader.load(loader.source(md.get("Source")), bucket)
                    elif loader and local(md.tag) in loader.classes:
                        loader.load(loader.classes[local(md.tag)], bucket)
                    else:
                        visit(md, bucket)
                continue
            if tag.endswith(".Resources") or (tag == "ResourceDictionary" and not key):
                visit(child, bucket)
                continue
            if not key:
                continue
            value = None
            if tag == "Color":
                value = ("color", (child.text or child.get("Value") or "").strip())
            elif tag == "SolidColorBrush":
                value = ("brush", (child.get("Color") or (child.text or "")).strip(), child.get("Opacity"))
            elif tag in ("Double", "Int32"):
                value = ("number", (child.text or child.get("Value") or "").strip())
            elif tag in ("Thickness", "CornerRadius"):
                value = ("text", (child.text or child.get("Value") or "").strip())
            elif tag in ("StaticResource", "ThemeResource") and child.get("ResourceKey"):
                value = ("alias", child.get("ResourceKey"))
            elif tag == "FontFamily":
                value = ("text", (child.text or child.get("Value") or "").strip())
            if value and value[1]:
                themes[bucket][key] = value

    if local(root.tag) == "ResourceDictionary":
        visit(root, bucket)
    else:  # App.xaml: only its .Resources section is global
        visit(root, bucket)


def lookup(themes, theme, key):
    return themes[theme].get(key) or themes["shared"].get(key)


def resolve(themes, theme, key, depth=0):
    """Return ("color", "#aarrggbb") / ("number", float) / ("text", str) or None."""
    entry = lookup(themes, theme, key)
    if not entry or depth > 20:
        return None
    kind = entry[0]
    raw = entry[1]
    if kind == "alias":
        return resolve(themes, theme, raw, depth + 1)
    m = REF.match(raw)
    if kind in ("color", "brush"):
        if m:
            inner = resolve(themes, theme, m.group(1).strip(), depth + 1)
            if not inner or inner[0] != "color":
                return None
            argb = inner[1]
        else:
            argb = normalize_color(raw)
            if argb is None:
                return None
        if kind == "brush" and entry[2]:
            a = int(argb[1:3], 16) * float(entry[2])
            argb = "#%02x%s" % (round(a), argb[3:])
        return ("color", argb)
    if m:
        return resolve(themes, theme, m.group(1).strip(), depth + 1)
    if kind == "number":
        try:
            return ("number", float(raw))
        except ValueError:
            return None
    return ("text", raw)


NAMED = {"Transparent": "#00ffffff", "White": "#ffffffff", "Black": "#ff000000"}


def normalize_color(raw):
    raw = raw.strip()
    if raw in NAMED:
        return NAMED[raw]
    if re.fullmatch(r"#[0-9a-fA-F]{6}", raw):
        return "#ff" + raw[1:].lower()
    if re.fullmatch(r"#[0-9a-fA-F]{8}", raw):
        return raw.lower()
    return None


def prop_name(key):
    parts = [p for p in re.split(r"[^A-Za-z0-9]+", key) if p]
    if not parts:
        return None
    name = parts[0][0].lower() + parts[0][1:] + "".join(p[0].upper() + p[1:] for p in parts[1:])
    if not name[0].isalpha():
        name = "t" + name
    return name


def main():
    winui, wavelink, out = map(Path, sys.argv[1:4])
    themes = {"dark": {}, "light": {}, "shared": {}}
    for f in sorted(winui.glob("*.xaml")):
        collect(f, themes)
    Loader(wavelink, themes).load(wavelink / "App.xaml")

    keys = sorted(set(themes["dark"]) | set(themes["light"]) | set(themes["shared"]))
    lines, seen = [], set()
    for key in keys:
        name = prop_name(key)
        if not name or name in seen:
            continue
        dark = resolve(themes, "dark", key)
        light = resolve(themes, "light", key) or dark
        dark = dark or light
        if not dark:
            continue
        seen.add(name)
        if dark[0] == "color":
            if dark[1] == light[1]:
                lines.append(f'    readonly property color {name}: "{dark[1]}"')
            else:
                lines.append(f'    readonly property color {name}: dark ? "{dark[1]}" : "{light[1]}"')
        elif dark[0] == "number":
            if dark[1] == light[1]:
                lines.append(f"    readonly property real {name}: {dark[1]:g}")
            else:
                lines.append(f"    readonly property real {name}: dark ? {dark[1]:g} : {light[1]:g}")
        else:
            esc = dark[1].replace("\\", "\\\\").replace('"', '\\"')
            lines.append(f'    readonly property string {name}: "{esc}"')

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        "// Generated by tools/gen_theme.py from WinUI theme resources and Wave Link's design tokens.\n"
        "// Do not edit by hand.\npragma Singleton\nimport QtQuick\n\nQtObject {\n"
        "    // Follows the desktop color scheme unless set explicitly.\n"
        "    property bool dark: Qt.styleHints.colorScheme !== Qt.ColorScheme.Light\n\n"
        + "\n".join(lines) + "\n}\n")
    print(f"{len(lines)} theme properties -> {out}")


if __name__ == "__main__":
    main()
