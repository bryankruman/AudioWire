#!/usr/bin/env python3
"""Bundle corresponding AudioWire source and optionally the actual dependency checkouts."""
import argparse
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FOLDERS = ("app", "cmake", "core", "platform", "driver", "installer", "scripts", "tests", "docs", ".github")
FILES = ("CMakeLists.txt", ".clang-format", ".gitattributes", ".gitignore", "README.md", "CONTRIBUTING.md",
         "LICENSE", "LICENSE_EXCEPTION", "THIRD_PARTY_NOTICES.md")
DEPENDENCIES = ("JUCE", "aoo", "opus", "libsodium", "mdns", "qrcodegen")
SENSITIVE_SUFFIXES = {".pfx", ".p12", ".p8", ".pem", ".key", ".mobileprovision"}

def add_tree(archive, directory, prefix):
    base = directory.resolve()
    for path in sorted(directory.rglob("*")):
        relative = path.relative_to(directory)
        if path.is_file() and ".git" not in relative.parts and "__pycache__" not in relative.parts:
            if path.suffix.lower() in SENSITIVE_SUFFIXES or path.name == ".env" or path.name.startswith(".env."):
                continue
            if not path.resolve().is_relative_to(base):
                raise ValueError(f"Source symlink leaves dependency: {relative}")
            archive.write(path, f"{prefix}/{relative.as_posix()}")

def dependency_tree(directory, name):
    direct = directory/name
    return direct if direct.is_dir() else directory/(name.lower() + "-src")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT/"build/package/AudioWire-0.1.0-source.zip")
    parser.add_argument("--vendor", type=Path, help="Include actual pinned dependency checkouts or a CMake _deps directory")
    args = parser.parse_args()
    if args.vendor:
        for name in DEPENDENCIES:
            if not dependency_tree(args.vendor, name).is_dir():
                parser.error(f"Missing dependency {name}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name in FILES:
            archive.write(ROOT/name, f"AudioWire/{name}")
        for name in FOLDERS:
            add_tree(archive, ROOT/name, f"AudioWire/{name}")
        if args.vendor:
            for name in DEPENDENCIES:
                add_tree(archive, dependency_tree(args.vendor, name), f"AudioWire/vendor/{name}")
        archive.writestr("AudioWire/SOURCE_BUILD.txt",
            "This archive contains the exact AudioWire development source.\n"
            "Bundled vendor sources include the applied compatibility fixes used by the build.\n"
            "See docs/BUILDING.md. For bundled sources, add -DAUDIOWIRE_VENDOR_DIR=<root>/vendor.\n"
            "Driver/iOS validation and release gates remain in docs/IMPLEMENTATION.md.\n")
    print(args.output)

if __name__ == "__main__":
    main()
