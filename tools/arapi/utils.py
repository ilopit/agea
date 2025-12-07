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
