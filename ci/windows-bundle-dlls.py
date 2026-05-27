#!/usr/bin/env python3
import subprocess
import shutil
import os
import sys
import re

def get_deps(binary, msys2_root):
    try:
        result = subprocess.run(
            ['ldd', binary],
            capture_output=True, text=True, timeout=30
        )
        deps = []
        for line in result.stdout.splitlines():
            match = re.search(r'=>\s+(/\S+)', line)
            if match:
                path = match.group(1)
                if msys2_root in path:
                    deps.append(path)
        return deps
    except Exception as e:
        print(f"  Warning: ldd failed for {binary}: {e}")
        return []

def copy_with_dirs(src, dst_dir, msys2_root):
    rel = os.path.relpath(src, msys2_root)
    dst = os.path.join(dst_dir, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        return True
    return False

def main():
    if len(sys.argv) < 3:
        print("Usage: windows-bundle-dlls.py <exe_path> <dist_dir> [msys2_root]")
        sys.exit(1)

    exe_path = os.path.abspath(sys.argv[1])
    dist_dir = os.path.abspath(sys.argv[2])
    msys2_root = sys.argv[3] if len(sys.argv) > 3 else "/ucrt64"

    if not os.path.exists(exe_path):
        print(f"Error: executable not found: {exe_path}")
        sys.exit(1)

    os.makedirs(dist_dir, exist_ok=True)

    print(f"Bundling DLLs for: {exe_path}")
    print(f"Output directory: {dist_dir}")
    print(f"MSYS2 root: {msys2_root}")

    bin_dir = os.path.join(dist_dir, "bin")
    os.makedirs(bin_dir, exist_ok=True)
    shutil.copy2(exe_path, bin_dir)
    print(f"  Copied: {os.path.basename(exe_path)}")

    processed = set()
    queue = [exe_path]

    while queue:
        current = queue.pop(0)
        if current in processed:
            continue
        processed.add(current)

        deps = get_deps(current, msys2_root)
        for dep in deps:
            if dep not in processed:
                basename = os.path.basename(dep)
                dst = os.path.join(bin_dir, basename)
                if not os.path.exists(dst):
                    shutil.copy2(dep, dst)
                    print(f"  Copied DLL: {basename}")
                queue.append(dep)

    msys2_bin = os.path.join(msys2_root, "bin")
    extra_dlls = [
        "gdbus.exe",
        "gspawn-win64-helper.exe",
        "gspawn-win64-helper-console.exe",
    ]
    for dll in extra_dlls:
        src = os.path.join(msys2_bin, dll)
        if os.path.exists(src):
            shutil.copy2(src, bin_dir)
            print(f"  Copied helper: {dll}")

    share_dir = os.path.join(dist_dir, "share")
    os.makedirs(share_dir, exist_ok=True)

    gtk4_share = os.path.join(msys2_root, "share", "gtk-4.0")
    if os.path.exists(gtk4_share):
        shutil.copytree(gtk4_share, os.path.join(share_dir, "gtk-4.0"), dirs_exist_ok=True)
        print("  Copied: GTK4 theme data")

    glib_schemas = os.path.join(msys2_root, "share", "glib-2.0", "schemas")
    if os.path.exists(glib_schemas):
        dst_schemas = os.path.join(share_dir, "glib-2.0", "schemas")
        os.makedirs(dst_schemas, exist_ok=True)
        for f in os.listdir(glib_schemas):
            if f.endswith('.gschema.xml') or f == 'gschemas.compiled':
                shutil.copy2(os.path.join(glib_schemas, f), dst_schemas)
        print("  Copied: GLib schemas")

    adwaita_icons = os.path.join(msys2_root, "share", "icons", "Adwaita")
    if os.path.exists(adwaita_icons):
        shutil.copytree(adwaita_icons, os.path.join(share_dir, "icons", "Adwaita"), dirs_exist_ok=True)
        print("  Copied: Adwaita icons")

    hicolor_icons = os.path.join(msys2_root, "share", "icons", "hicolor")
    if os.path.exists(hicolor_icons):
        shutil.copytree(hicolor_icons, os.path.join(share_dir, "icons", "hicolor"), dirs_exist_ok=True)
        print("  Copied: hicolor icons")

    etc_dir = os.path.join(dist_dir, "etc", "gtk-4.0")
    os.makedirs(etc_dir, exist_ok=True)
    settings_ini = os.path.join(etc_dir, "settings.ini")
    with open(settings_ini, 'w') as f:
        f.write("[Settings]\n")
        f.write("gtk-theme-name=Default\n")
        f.write("gtk-font-name=Segoe UI 10\n")
    print("  Created: etc/gtk-4.0/settings.ini")

    gdk_pixbuf_lib = os.path.join(msys2_root, "lib", "gdk-pixbuf-2.0")
    if os.path.exists(gdk_pixbuf_lib):
        shutil.copytree(gdk_pixbuf_lib, os.path.join(dist_dir, "lib", "gdk-pixbuf-2.0"), dirs_exist_ok=True)
        print("  Copied: GDK Pixbuf loaders")

    gdk_pixbuf_query = os.path.join(msys2_bin, "gdk-pixbuf-query-loaders.exe")
    if os.path.exists(gdk_pixbuf_query):
        shutil.copy2(gdk_pixbuf_query, bin_dir)
        print("  Copied: gdk-pixbuf-query-loaders.exe")

    glib_compile = os.path.join(msys2_bin, "glib-compile-schemas.exe")
    if os.path.exists(glib_compile):
        shutil.copy2(glib_compile, bin_dir)
        print("  Copied: glib-compile-schemas.exe")

    print(f"\nDone! Bundled {len(processed)} binaries to {dist_dir}")

if __name__ == "__main__":
    main()
