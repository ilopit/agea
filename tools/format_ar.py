#!/usr/bin/env python3
"""
Format KRG_ar_class / KRG_ar_struct / KRG_ar_property / KRG_ar_function macros.

Convention
  Empty:        KRG_ar_class();
  Single-line:  KRG_ar_class(key = value);          when <= 100 cols
  Multi-line:   // clang-format off
                KRG_ar_class(
                    key1     = val1,
                    long_key = val2
                );
                // clang-format on

  - = signs aligned within each macro
  - Long double-quoted strings wrapped at column 100 with trailing space, not leading
  - Continuation strings aligned to the opening "
  - Indentation of the macro is preserved

Usage
    python tools/format_ar.py              Format all .h files
    python tools/format_ar.py --check      Report without modifying
    python tools/format_ar.py file.h ...   Format specific files
"""

import re
import sys
from pathlib import Path

COLUMN_LIMIT = 100
INDENT = "    "

TYPE_MACROS = ["KRG_ar_class", "KRG_ar_struct"]
MEMBER_MACROS = ["KRG_ar_property", "KRG_ar_function"]
ALL_MACROS = TYPE_MACROS + MEMBER_MACROS

QUOTED_VALUE_KEYS = {"category", "mcp_hint", "hint"}


def main():
    check_only = "--check" in sys.argv
    paths = [a for a in sys.argv[1:] if not a.startswith("--")]

    if not paths:
        root = Path(__file__).resolve().parent.parent
        paths = sorted(_find_headers(root))
    else:
        paths = [Path(p) for p in paths]

    changed = 0
    for p in paths:
        if _process_file(p, check_only):
            changed += 1

    verb = "need" if check_only else "formatted"
    print(f"{changed} file(s) {verb}")
    if check_only:
        sys.exit(1 if changed else 0)


def _find_headers(root):
    for d in ("packages", "libs"):
        p = root / d
        if p.exists():
            yield from p.rglob("*.h")


# ── Macro detection ──────────────────────────────────────────────


def _macro_on_line(line):
    if line.lstrip().startswith("#"):
        return None
    for m in ALL_MACROS:
        if re.search(rf"\b{m}\s*\(", line):
            return m
    return None


def _find_close_paren(lines, start):
    depth = 0
    for i in range(start, len(lines)):
        for ch in lines[i]:
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    return i
    return None


def _leading_indent(line):
    return line[: len(line) - len(line.lstrip())]


def _is_decl_end(stripped, macro):
    if macro in TYPE_MACROS:
        return stripped.startswith("class ") or stripped.startswith("struct ")
    if macro == "KRG_ar_function":
        return ")" in stripped
    return ";" in stripped


# ── Argument parsing ─────────────────────────────────────────────


def _extract_inner(text, macro):
    idx = text.index(macro) + len(macro)
    while text[idx] != "(":
        idx += 1
    idx += 1
    depth, end = 1, idx
    while depth:
        if text[end] == "(":
            depth += 1
        elif text[end] == ")":
            depth -= 1
        if depth:
            end += 1
    return text[idx:end]


def _split_args(inner):
    args, buf = [], []
    in_quote = None
    for ch in inner:
        if in_quote is None and ch in ('"', "'"):
            in_quote = ch
        elif ch == in_quote:
            in_quote = None
        if ch == "," and in_quote is None:
            args.append("".join(buf).strip())
            buf = []
        else:
            buf.append(ch)
    tail = "".join(buf).strip()
    if tail:
        args.append(tail)
    return args


def _merge_strings(s):
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', s)
    if len(parts) > 1:
        return '"' + "".join(parts) + '"'
    return s


def _classify(raw):
    raw = re.sub(r"\s+", " ", raw).strip()
    if raw.startswith('"'):
        return (None, _merge_strings(raw))
    if raw.startswith("'"):
        return (None, raw)
    m = re.match(r"(\w+)\s*=\s*(.*)", raw, re.DOTALL)
    if m:
        val = m.group(2).strip()
        if '"' in val:
            val = _merge_strings(val)
        return (m.group(1), val)
    return (None, raw)


def _expand_kv_string(key, val):
    """Convert a positional "key=value" string into a (key, value) pair."""
    if key is not None:
        return (key, val)
    if not (val.startswith('"') and "=" in val):
        return (key, val)
    content = val[1:-1]
    k, _, v = content.partition("=")
    k = k.strip()
    v = v.strip()
    if k in QUOTED_VALUE_KEYS:
        return (k, f'"{v}"')
    return (k, v)


# ── String wrapping ──────────────────────────────────────────────


def _wrap_string(prefix_len, s):
    if s[0] != '"':
        return [s]

    content = s[1:-1]
    max_w = COLUMN_LIMIT - prefix_len - 2

    if len(content) <= max_w:
        return [s]

    words = [w for w in content.split(" ") if w]
    chunks, cur, cur_len = [], [], 0

    for w in words:
        new_len = (cur_len + 1 + len(w)) if cur else len(w)
        if cur and new_len + 1 > max_w:
            chunks.append(" ".join(cur) + " ")
            cur, cur_len = [w], len(w)
        else:
            cur.append(w)
            cur_len = new_len

    if cur:
        chunks.append(" ".join(cur))

    return [f'"{c}"' for c in chunks]


# ── Formatting ───────────────────────────────────────────────────


def _format_macro(macro, raw_args, outer_indent=""):
    inner_indent = outer_indent + INDENT

    if not raw_args:
        return f"{outer_indent}{macro}();", False

    parsed = [_classify(a) for a in raw_args]

    if macro in MEMBER_MACROS:
        parsed = [_expand_kv_string(k, v) for k, v in parsed]

    single = (
        outer_indent
        + macro
        + "("
        + ", ".join((f"{k} = {v}" if k else v) for k, v in parsed)
        + ");"
    )
    if len(single) <= COLUMN_LIMIT and macro not in MEMBER_MACROS:
        return single, False

    max_key = max((len(k) for k, _ in parsed if k), default=0)
    lines = []

    for i, (key, val) in enumerate(parsed):
        comma = "," if i < len(parsed) - 1 else ""

        if key is None:
            lines.append(f"{inner_indent}{val}{comma}")
            continue

        prefix = f"{inner_indent}{key.ljust(max_key)} = "

        if val.startswith('"'):
            parts = _wrap_string(len(prefix), val)
            pad = " " * len(prefix)
            for j, part in enumerate(parts):
                tail = comma if j == len(parts) - 1 else ""
                pfx = prefix if j == 0 else pad
                lines.append(f"{pfx}{part}{tail}")
        else:
            lines.append(f"{prefix}{val}{comma}")

    body = "\n".join(lines)
    return f"{outer_indent}{macro}(\n{body}\n{outer_indent});", True


# ── File processing ──────────────────────────────────────────────


def _process_file(path, check_only):
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return False

    lines = text.split("\n")
    out, i, touched = [], 0, False

    while i < len(lines):
        macro = _macro_on_line(lines[i])
        if macro is None:
            out.append(lines[i])
            i += 1
            continue

        if out and out[-1].strip() == "// clang-format off":
            out.pop()

        end = _find_close_paren(lines, i)
        if end is None:
            out.append(lines[i])
            i += 1
            continue

        outer_indent = _leading_indent(lines[i])
        raw = "\n".join(lines[i : end + 1])
        args = _split_args(_extract_inner(raw, macro))
        formatted, guards = _format_macro(macro, args, outer_indent)

        if guards:
            out.append(f"{outer_indent}// clang-format off")
        out.extend(formatted.split("\n"))

        i = end + 1
        touched = True

        if guards:
            # argen reads lines after ");", so the guarded region
            # must include the declaration that follows (class line,
            # member variable, or function signature).  Collect lines
            # past stale guards until we hit ";" or "class "/"struct ".
            decl_lines = []
            scan = i
            while scan < len(lines) and scan < i + 10:
                s = lines[scan].strip()
                if s == "// clang-format on" or s == "":
                    scan += 1
                    continue
                decl_lines.append(lines[scan])
                scan += 1
                if _is_decl_end(s, macro):
                    break

            if decl_lines:
                out.extend(decl_lines)
            out.append(f"{outer_indent}// clang-format on")
            i = scan

            # skip any remaining stale // clang-format on
            while i < len(lines) and lines[i].strip() == "// clang-format on":
                i += 1

    if not touched:
        return False

    new_text = "\n".join(out)
    if new_text == text:
        return False

    if not check_only:
        path.write_text(new_text, encoding="utf-8")

    try:
        rel = path.relative_to(Path(__file__).resolve().parent.parent)
    except ValueError:
        rel = path
    print(f"  {'would format' if check_only else 'formatted'}: {rel}")
    return True


if __name__ == "__main__":
    main()
