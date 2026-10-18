#!/usr/bin/env python3
"""Turn an installed build into a relocatable directory: binaries + the Qt libraries, plugins and QML
modules they need, with RPATHs pointing inside the bundle. System libraries (glibc, PipeWire, libusb,
graphics stacks) stay external.

usage: bundle.py <staged install prefix> <Qt prefix> <output dir>
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

PLUGINS = ["platforms/libqwayland-generic.so", "platforms/libqwayland-egl.so", "platforms/libqxcb.so",
           "platforms/libqoffscreen.so", "wayland-shell-integration", "wayland-decoration-client",
           "wayland-graphics-integration-client", "xcbglintegrations", "platformthemes/libqxdgdesktopportal.so",
           "imageformats/libqico.so", "imageformats/libqsvg.so", "iconengines", "egldeviceintegrations",
           "platforminputcontexts", "tls/libqopensslbackend.so"]
QML = ["QtQml", "QtQuick", "QtCore"]


def run(*args, **kw):
    return subprocess.run(args, check=True, capture_output=True, text=True, **kw).stdout


def qt_deps(path, qt_lib):
    """Libraries from the Qt prefix that `path` needs (transitively, via ldd)."""
    env = dict(os.environ, LD_LIBRARY_PATH=str(qt_lib))
    out = subprocess.run(["ldd", str(path)], capture_output=True, text=True, env=env).stdout
    deps = set()
    for line in out.splitlines():
        if "=>" in line:
            target = line.split("=>")[1].split("(")[0].strip()
            if target.startswith(str(qt_lib)):
                deps.add(Path(target))
    return deps


def set_rpath(path, rpath):
    subprocess.run(["patchelf", "--set-rpath", rpath, str(path)], check=True)


def main():
    stage, qt, out = map(Path, sys.argv[1:4])
    qt_lib = qt / "lib"
    if out.exists():
        shutil.rmtree(out)
    (out / "bin").mkdir(parents=True)
    (out / "lib").mkdir()

    elfs = []
    for name in ("wavelink", "wavelinkd"):
        dst = out / "bin" / name
        shutil.copy2(stage / "bin" / name, dst)
        set_rpath(dst, "$ORIGIN/../lib")
        elfs.append(dst)

    for rel in PLUGINS:
        src = qt / "plugins" / rel
        if not src.exists():
            print("skip missing plugin", rel)
            continue
        dst = out / "plugins" / rel
        if src.is_dir():
            shutil.copytree(src, dst, symlinks=True)
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
    for rel in QML:
        shutil.copytree(qt / "qml" / rel, out / "qml" / rel, symlinks=True)

    for so in list(out.rglob("*.so")) + list(out.rglob("*.so.*")):
        if so.is_file() and not so.is_symlink() and so.parent != out / "lib":
            depth = len(so.relative_to(out).parts) - 1
            set_rpath(so, "$ORIGIN/" + "/".join([".."] * depth) + "/lib")
            elfs.append(so)

    seen, queue = set(), list(elfs)
    while queue:
        for dep in qt_deps(queue.pop(), qt_lib):
            if dep.name in seen:
                continue
            seen.add(dep.name)
            dst = out / "lib" / dep.name
            shutil.copy2(dep.resolve(), dst)
            set_rpath(dst, "$ORIGIN")
            queue.append(dst)

    (out / "bin" / "qt.conf").write_text("[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\nQmlImports=qml\n")
    print(f"bundled {len(seen)} Qt libraries into {out}")


if __name__ == "__main__":
    main()
