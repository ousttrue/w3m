from os import write
from typing import Dict
import sys
import pathlib
import re
import argparse


def get_map(zig: pathlib.Path):
    pattern = re.compile(r"const (\w+) = struct \{\s*\.func = c\.(\w+),", re.MULTILINE)
    text = zig.read_text()
    map = {}
    for cmd, fn in pattern.findall(text):
        map[fn] = cmd
    return map


def gsub(keymap: pathlib.Path, map: Dict[str, str]):
    # print(map)

    def repl(m):
        return f'"{map[m.group(1)]}",'

    text = keymap.read_text()
    for line in text.splitlines(keepends=True):
        if re.search(r"^\s*/\*", line):
            pass
        else:
            line = re.sub(r"(\w+),", repl, line)
        sys.stdout.write(line)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog=sys.argv[0])
    parser.add_argument("zig", type=pathlib.Path)
    parser.add_argument("keymap", type=pathlib.Path)
    parsed = parser.parse_args()

    map = get_map(parsed.zig)
    # print(map)
    gsub(parsed.keymap, map)
