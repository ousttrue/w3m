import sys
import pathlib
import re
import argparse

defun_pattern = re.compile(r'^DEFUN\((\w+), ([^,]+), ("[^"]+")\)')


def proc_c(file: pathlib.Path, is_header: bool):
    if is_header:
        sys.stdout.write(
            """#pragma once

struct CmdArgs {
    void* p;
};

typedef void (*CmdFunc)(struct CmdArgs);

"""
        )

    for line in file.read_text().splitlines(keepends=True):
        m = defun_pattern.match(line)
        if m:
            line = f"void {m.group(1)}(struct CmdArgs args){';' if is_header else ''}\n"
            sys.stdout.write(line)
        else:
            if not is_header:
                sys.stdout.write(line)


def proc_zig(file: pathlib.Path):
    sys.stdout.write('#include "keybind.h"\n')
    sys.stdout.write("\n")

    for line in file.read_text().splitlines(keepends=True):
        m = defun_pattern.match(line)
        if m:
            sys.stdout.write(f"""
const {m.group(2)} = struct {{
    .func = c.{m.group(1)},
    .desc = {m.group(3)},
}};
         """)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog=sys.argv[0])
    parser.add_argument("type", choices=["c", "h", "zig"])
    parser.add_argument("file", type=pathlib.Path)
    parsed = parser.parse_args()
    match parsed.type:
        case "c":
            proc_c(parsed.file, False)
        case "h":
            proc_c(parsed.file, True)
        case "zig":
            proc_zig(parsed.file)
