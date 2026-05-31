#!/usr/bin/env python3
"""PreToolUse guard: block any `git commit` from the Bash/PowerShell tools.

The assistant must stage changes and show the diff; the user commits manually.
This catches the common variants a prefix-based permission rule misses
(`git -c x.y=z commit`, `git -C dir commit`, mid-pipeline, PowerShell). Exit
code 2 tells Claude Code to block the tool call and surface the message.
"""
import json
import re
import sys

try:
    data = json.load(sys.stdin)
except Exception:
    sys.exit(0)  # unparseable input → don't block

cmd = str(data.get("tool_input", {}).get("command", ""))

# `git`, then zero or more options (a -flag optionally with its argument),
# then the `commit` subcommand. Matches `git commit`, `git -c k=v commit`,
# `git -C dir commit`, `... | git commit -F -`; ignores `git log --grep commit`.
if re.search(r"\bgit\b(\s+-\S+(\s+\S+)?)*\s+commit\b", cmd, re.IGNORECASE):
    sys.stderr.write(
        "Blocked: `git commit` is disabled for the assistant. Stage the changes "
        "and show the diff — the user commits manually.\n"
    )
    sys.exit(2)

sys.exit(0)
