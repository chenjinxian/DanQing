#!/usr/bin/env python3
"""Apply clang-tidy --export-fixes yaml(s) to source files in one consistent pass.
Stands in for clang-apply-replacements (not in the pip clang-tidy wheel).
Usage: apply_tidy_fixes.py <fixes.yaml> [<fixes.yaml> ...]
Replacements are deduped by (file, offset); applied per-file in descending offset
order so earlier offsets are unaffected by later edits.
"""
import sys
import yaml

def main(yamls):
    # file -> { offset: (length, text) }
    by_file = {}
    conflicts = 0
    for path in yamls:
        try:
            data = yaml.safe_load(open(path)) or {}
        except Exception as e:
            print(f"skip {path}: {e}")
            continue
        for diag in data.get("Diagnostics", []) or []:
            msg = diag.get("DiagnosticMessage", {}) or {}
            for r in msg.get("Replacements", []) or []:
                fp = r["FilePath"]
                off = int(r["Offset"]); ln = int(r["Length"]); txt = r["ReplacementText"]
                slot = by_file.setdefault(fp, {})
                if off in slot and slot[off] != (ln, txt):
                    conflicts += 1
                    continue
                slot[off] = (ln, txt)
    total = 0
    for fp, slots in by_file.items():
        try:
            with open(fp, "rb") as f:
                content = f.read()
        except Exception as e:
            print(f"skip {fp}: {e}")
            continue
        for off in sorted(slots.keys(), reverse=True):
            ln, txt = slots[off]
            content = content[:off] + txt.encode("utf-8") + content[off + ln:]
        with open(fp, "wb") as f:
            f.write(content)
        total += len(slots)
    print(f"applied {total} replacements across {len(by_file)} files ({conflicts} conflicts dropped)")

if __name__ == "__main__":
    main(sys.argv[1:])
