"""Utility functions for the AGEA reflection API parser.

This module provides common utility functions used across the parser,
including error output and string manipulation utilities.
"""
import sys
from typing import Any

# Characters to remove from strings during parsing
_STRIP_CHARACTERS = (" ", "\t", "\n", "\r", "\"")


def eprint(*args: Any, **kwargs: Any) -> None:
  """Print to stderr.
    
    Convenience function for printing error and debug messages to stderr.
    Accepts the same arguments as the standard print() function.
    
    Args:
        *args: Variable length argument list to print
        **kwargs: Arbitrary keyword arguments (e.g., file, sep, end)
    """
  print(*args, file=sys.stderr, **kwargs)


def extstrip(value: str) -> str:
  """Extract and strip whitespace and quotes from a string.
    
    Removes common whitespace characters (spaces, tabs, newlines, carriage returns)
    and double quotes from the input string. This is useful for cleaning parsed
    values from configuration files and macro arguments.
    
    Args:
        value: Input string to clean
        
    Returns:
        String with whitespace and quotes removed
        
    Examples:
        >>> extstrip('  "value"  ')
        'value'
        >>> extstrip('key=value\\n')
        'key=value'
    """

  result = value
  for char in _STRIP_CHARACTERS:
    result = result.replace(char, "")

  return result


def replace_blocks_with_sorted_insert(path: str, replacements: dict) -> bool:
  """
    Replace existing blocks and insert missing non-root blocks alphabetically above root.
    Root block always exists and remains last.

    Returns:
        True  = file changed
        False = no changes applied
    """
  start_prefix = "// block start "
  end_prefix = "// block end "

  # Load original file
  with open(path, "r") as f:
    original = f.readlines()

  output = []
  found_blocks = set()
  i = 0

  # -----------------------
  # Step 1: Process existing blocks
  # -----------------------
  while i < len(original):
    line = original[i]

    if line.strip().startswith(start_prefix):
      block_name = line.strip()[len(start_prefix):]
      found_blocks.add(block_name)

      output.append(line)    # keep start marker
      replacement = replacements.get(block_name)
      indent = line[:len(line) - len(line.lstrip())]

      if replacement is not None:
        # Replace block content
        for rl in replacement.splitlines():
          output.append(indent + rl + "\n")
        # Skip old content until end marker
        i += 1
        while i < len(original) and not original[i].strip().startswith(end_prefix):
          i += 1
      else:
        # Preserve old content
        i += 1
        while i < len(original) and not original[i].strip().startswith(end_prefix):
          output.append(original[i])
          i += 1

      # Write end marker
      if i < len(original):
        output.append(original[i])

      i += 1
      continue

    # Non-block line
    output.append(line)
    i += 1

  # -----------------------
  # Step 2: Locate root block
  # -----------------------
  root_start_idx = None
  root_end_idx = None
  for idx, line in enumerate(output):
    if line.strip().startswith(start_prefix + "root"):
      root_start_idx = idx
    if line.strip().startswith(end_prefix + "root"):
      root_end_idx = idx
      break

  if root_start_idx is None or root_end_idx is None:
    raise ValueError("Root block must exist in the file!")

  # -----------------------
  # Step 3: Add missing non-root blocks ABOVE root
  # -----------------------
  missing = set(replacements.keys()) - found_blocks
  non_root_missing = sorted([b for b in missing if b != "root"])

  def build_block(name):
    lines = [f"// block start {name}\n"]
    for rl in replacements[name].splitlines():
      lines.append("    " + rl + "\n")
    lines.append(f"// block end {name}\n")
    return lines

  new_blocks = []
  for b in non_root_missing:
    new_blocks.extend(build_block(b))

  # Insert missing non-root blocks immediately before root
  output = output[:root_start_idx] + new_blocks + output[root_start_idx:]

  # -----------------------
  # Step 4: Replace root content if needed
  # -----------------------
  if "root" in replacements:
    indent = output[root_start_idx][:len(output[root_start_idx]) -
                                    len(output[root_start_idx].lstrip())]
    new_content = [indent + rl + "\n" for rl in replacements["root"].splitlines()]
    output = output[:root_start_idx + 1] + new_content + output[root_end_idx:]

  # -----------------------
  # Step 5: Write only if changed
  # -----------------------
  if output == original:
    return False

  with open(path, "w") as f:
    f.writelines(output)

  return True


class FileBuffer:

  def __init__(self, path: str):
    self.path = path
    self._original = self._load_file()
    self._buffer: str = ""

  def _load_file(self) -> str:
    try:
      with open(self.path, "r", encoding="utf-8") as f:
        return f.read()
    except FileNotFoundError:
      return ""    # file does not exist yet

  def append(self, content: str | list[str]) -> None:
    if isinstance(content, str):
      self._buffer += content
    else:
      for c in content:
        self._buffer += c

  def write_if_changed(self) -> None:
    if self._original != self._buffer:
      print(f"Writing changes to {self.path}")
      with open(self.path, "w", encoding="utf-8") as f:
        f.write(self._buffer)
