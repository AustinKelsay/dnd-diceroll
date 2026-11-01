#!/usr/bin/env python3
"""Claude Code PreToolUse hook: refuse destructive Git commands.

Reads the tool call as JSON on stdin and exits 2 with a reason when the Bash
command would discard history or work. Standard library only, so it runs on
Apple's default Python as well as the Homebrew interpreter.
"""
import json
import re
import sys

# Each pattern matches a whole-tree or history-destroying operation. Ordinary
# pushes, --force-with-lease, single-path restores, and plain branch deletion
# (-d, recoverable from the reflog) stay allowed.
PATTERNS = [
    r"git(\s+\S+)*\s+reset\s+(\S+\s+)*--hard",
    r"git(\s+\S+)*\s+clean(\s+\S+)*\s+-[A-Za-z]*[fdx]",
    r"git(\s+\S+)*\s+(checkout|restore)(\s+\S+)*\s+(\.|:/)(\s|;|\||&|\)|$)",
    r"git(\s+\S+)*\s+push(\s+\S+)*\s+(--force|-[A-Za-z]*f[A-Za-z]*)(\s|=|$)",
    r"git(\s+\S+)*\s+push(\s+\S+)*\s+\+\S",
    r"git(\s+\S+)*\s+branch(\s+\S+)*\s+(-[A-Za-z]*[Df][A-Za-z]*|--force|--delete\s+--force)(\s|$)",
    r"git(\s+\S+)*\s+stash\s+(drop|clear)(\s|$)",
    r"git(\s+\S+)*\s+worktree\s+remove(\s+\S+)*\s+(--force|-f)(\s|$)",
]


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError):
        return 0
    command = (payload.get("tool_input") or {}).get("command") or ""
    for pattern in PATTERNS:
        if re.search(pattern, command):
            sys.stderr.write(
                "BLOCKED by .claude/hooks/block_dangerous_git.py: this command can discard "
                "work or history. The contribution workflow preserves unfinished work and "
                "never force-pushes; ask the checkout owner to do this by hand if it is "
                f"really intended. Matched: {pattern}\n"
            )
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
