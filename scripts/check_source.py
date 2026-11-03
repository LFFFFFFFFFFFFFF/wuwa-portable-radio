"""Check tracked publication content, without printing any matched secret values."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
result = subprocess.run(["git", "ls-files", "-z"], cwd=root, capture_output=True)
names = result.stdout.decode("utf-8").split("\0") if result.returncode == 0 else []
files = [root / name for name in names if name]
if not files:
    files = [p for p in root.rglob("*") if p.is_file() and not any(x in {".git", "build", "dist"} for x in p.relative_to(root).parts)]
forbidden = {".dll", ".exe", ".asi", ".pak", ".sig", ".ucas", ".utoc", ".log", ".backup", ".pdb", ".obj"}
patterns = [
    re.compile(r"[A-Za-z]:[\\/]+Users[\\/]+[^\\/\s]+", re.I),
    re.compile(r"gh[pousr]_" + r"[A-Za-z0-9]{20,}"),
    re.compile(r"github_pat_" + r"[A-Za-z0-9_]{20,}"),
    re.compile(r"-----BEGIN " + r"(?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    re.compile(r"[?&]uct2=" + r"[^\s]+"),
]
issues = []
for p in files:
    relative = p.relative_to(root)
    if p.suffix.lower() in forbidden or p.name in {"hosts.yml", ".env", "entry-state.json", "probe-state.json"}:
        issues.append(str(relative) + ": forbidden artifact")
        continue
    try:
        text = p.read_text(encoding="utf-8-sig")
    except UnicodeDecodeError:
        issues.append(str(relative) + ": unexpected binary or encoding")
        continue
    if any(pattern.search(text) for pattern in patterns):
        issues.append(str(relative) + ": publication boundary match")
if issues:
    print("\n".join(issues))
    sys.exit(1)
print(f"PASS: {len(files)} source files; no game binaries, private state, local-user paths or token patterns")
