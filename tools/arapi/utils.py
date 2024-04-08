import sys


def eprint(*args, **kwargs):
  print(*args, file=sys.stderr, **kwargs)


def extstrip(value: str):
  removal_list = [" ", "\t", "\n", "\r", "\""]
  for s in removal_list:
    value = value.replace(s, "")
  return value
