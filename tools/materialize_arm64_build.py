#!/usr/bin/env python3
from __future__ import annotations

import re
import shutil
import sys
from pathlib import Path

RISCV = re.compile(r"riscv64|RISCV64|riscv_to_x86_64|riscv64_to_", re.IGNORECASE)


def matching_brace(text: str, open_pos: int) -> int:
    depth = 0
    string = False
    escape = False
    line = False
    block = False
    i = open_pos
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ""
        if line:
            if c == "\n":
                line = False
        elif block:
            if c == "*" and n == "/":
                block = False
                i += 1
        elif string:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                string = False
        elif c == "/" and n == "/":
            line = True
            i += 1
        elif c == "/" and n == "*":
            block = True
            i += 1
        elif c == '"':
            string = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced Blueprint braces")


def top_blocks(text: str):
    blocks = []
    depth = 0
    string = False
    escape = False
    line = False
    block = False
    start = None
    i = 0
    while i < len(text):
        c = text[i]
        n = text[i + 1] if i + 1 < len(text) else ""
        if line:
            if c == "\n":
                line = False
        elif block:
            if c == "*" and n == "/":
                block = False
                i += 1
        elif string:
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                string = False
        elif c == "/" and n == "/":
            line = True
            i += 1
        elif c == "/" and n == "*":
            block = True
            i += 1
        elif c == '"':
            string = True
        elif c == "{":
            if depth == 0:
                start = text.rfind("\n", 0, i) + 1
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0 and start is not None:
                blocks.append((start, i + 1))
                start = None
        i += 1
    return blocks


def prune_bp(path: Path) -> None:
    text = path.read_text(errors="ignore")
    original = text

    # Remove complete top-level modules whose name is explicitly RISC-V.
    spans = []
    for start, end in top_blocks(text):
        block = text[start:end]
        m = re.search(r'(?m)^\s*name:\s*"([^"]+)"', block)
        if m and RISCV.search(m.group(1)):
            spans.append((start, end))
    for start, end in reversed(spans):
        text = text[:start] + text[end:]

    # Remove arch-specific RISC-V branches from otherwise shared modules.
    while True:
        m = re.search(r'(?m)^\s*riscv64\s*:\s*\{', text)
        if not m:
            break
        open_pos = text.find("{", m.start())
        close = matching_brace(text, open_pos) + 1
        if close < len(text) and text[close] == ",":
            close += 1
        if close < len(text) and text[close] == "\n":
            close += 1
        text = text[:m.start()] + text[close:]

    # Remove standalone RISC-V string entries from simple lists.
    kept = []
    for line in text.splitlines(True):
        stripped = line.strip()
        if RISCV.search(line) and (
            stripped.startswith('"')
            or stripped.startswith("#")
            or stripped.startswith("//")
            or stripped.startswith("/*")
            or stripped.startswith("*")
        ):
            continue
        if RISCV.search(line) and ":" in line and stripped.endswith(","):
            continue
        kept.append(line)
    text = "".join(kept)

    if text != original:
        path.write_text(text)


def prune_tree(root: Path) -> None:
    # Delete guest-specific RISC-V directories/files. Keep tools and metadata
    # outside this imported tree untouched.
    for path in sorted(root.rglob("*"), key=lambda p: len(p.parts), reverse=True):
        if path == root:
            continue
        rel = path.relative_to(root)
        if any(RISCV.search(part) for part in rel.parts):
            if path.is_dir():
                shutil.rmtree(path, ignore_errors=True)
            else:
                path.unlink(missing_ok=True)

    for path in root.rglob("Android.bp"):
        prune_bp(path)


def validate(root: Path) -> None:
    required = [
        "Android.bp",
        "decoder/arm64_test/decoder_test.cc",
        "interpreter/arm64/interpreter-main.cc",
        "runtime/arm64/translator.cc",
        "guest_state/arm64/get_cpu_state.cc",
        "native_bridge/arm64/native_bridge.cc",
        "lite_translator/arm64_to_x86_64/lite_translator.cc",
        "code_gen_lib/arm64_to_x86_64/gen_wrapper.cc",
    ]
    missing = [p for p in required if not (root / p).is_file()]
    if missing:
        raise RuntimeError("missing ARM64 implementation files: " + ", ".join(missing))

    for p in root.rglob("*"):
        if any(part in {".git", ".github"} for part in p.parts):
            continue
        if p.is_file() and p.suffix in {".bp", ".mk", ".cc", ".h", ".hh", ".inc", ".json"}:
            text = p.read_text(errors="ignore")
            # Ignore comments; fail on executable/build references.
            text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
            if RISCV.search(text):
                raise RuntimeError(f"RISC-V reference remains in executable/build file: {p.relative_to(root)}")


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: materialize_arm64_build.py <imported-tree>", file=sys.stderr)
        return 2
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        print(f"not a directory: {root}", file=sys.stderr)
        return 2
    prune_tree(root)
    validate(root)
    print(f"ARM64-only materialized tree: {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
