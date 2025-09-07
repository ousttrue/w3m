#!/usr/bin/python3
import sys
import os
import pathlib
import re
from typing import NamedTuple, Union, List, Optional
from enum import Enum, auto


HERE = pathlib.Path(__file__).absolute().parent


CONTEXT = {
    "HAVE_SGTTY_H": False,
    "HAVE_STROQ": False,
    "HAVE_ATOQ": False,
    "HAVE_GETPASSPHRASE": False,
    "HAVE_TERMIO_H": False,
    "HAVE_FLOAT_H": True,
    "HAVE_SYS_SELECT_H": True,
    "HAVE_SIGSETJMP": True,
    "HAVE_TERMIOS_H": True,
    "HAVE_DIRENT_H": True,
    "HAVE_LOCALE_H": True,
    "HAVE_STDINT_H": True,
    "HAVE_INTTYPES_H": True,
    "HAVE_STRTOLL": True,
    "HAVE_ATOLL": True,
    "HAVE_STRCASECMP": True,
    "HAVE_STRCASESTR": True,
    "HAVE_STRCHR": True,
    "HAVE_STRERROR": True,
    "HAVE_BCOPY": True,
    "HAVE_WAITPID": True,
    "HAVE_WAIT3": True,
    "HAVE_STRFTIME": True,
    "HAVE_GETCWD": True,
    "HAVE_GETWD": True,
    "HAVE_SYMLINK": True,
    "HAVE_READLINK": True,
    "HAVE_LSTAT": True,
    "HAVE_SETENV": True,
    "HAVE_PUTENV": True,
    "HAVE_SRAND48": True,
    "HAVE_SRANDOM": True,
    "HAVE_CHDIR": True,
    "HAVE_MKDTEMP": True,
    "HAVE_FACCESSAT": True,
    "HAVE_SETPGRP": True,
    "HAVE_SETLOCALE": True,
    "HAVE_LANGINFO_CODESET": True,
    #
    "ENABLE_REMOVE_TRAILINGSPACES": True,
    "TABLE_EXPAND": False,
    "TABLE_NO_COMPACT": False,
    "NOWRAP": False,
    "MENU_THIN_FRAME": False,
    "USE_EGD": False,
    "CLEAR_BUF": False,
    "INET6": True,
    "HAVE_SOCKLEN_T": True,
    "HAVE_OLD_SS_FAMILY": False,
    "USE_W3MIMG_WIN": False,
    "W3MIMGDISPLAY_SETUID ": False,
    "USE_IMLIB2 ": False,
    "USE_XFACE": False,
    "SIGWINCH": True,
    "SIGPIPE": True,
    "SIGCHLD": True,
    "SIGTSTP": True,
    # 'SIGSTOP': False,
    "USE_DIGEST_AUTH": True,
    "USE_COOKIE": True,
    "USE_ALARM": True,
    "USE_SSL": True,
    "USE_SSL_VERIFY": True,
    "USE_HELP_CGI": True,
    "USE_DICT": True,
    "USE_BUFINFO": True,
    "USE_HISTORY": True,
    "USE_EXTERNAL_URI_LOADER": False,
    # 'HAVE_WAITPID': True,
    #
    # 'USE_INCLUDED_SRAND48': False,
    "USE_MIGEMO": False,
    # "USE_W3MMAILER": False,
    "USE_MARK": True,
    "__CYGWIN__": False,
    "SUPPORT_WIN9X_CONSOLE_MBCS": False,
    "__MINGW32_VERSION": False,
    "__EMX__": False,
    "__WATT32__": False,
    "USE_BINMODE_STREAM": False,
    #
    "USE_M17N": True,
    "USE_UNICODE": True,
    "ENABLE_NLS": True,
    #
    "USE_ANSI_COLOR": True,
    "USE_COLOR": True,
    "USE_BG_COLOR": True,
    "USE_RAW_SCROLL": False,
    "USE_MENU": True,
    "MENU_MAP": True,
    "MENU_SELECT": True,
    "USE_IMAGE": True,
    #
    "USE_MOUSE": False,
    "USE_SYSMOUSE": False,
    "USE_GPM": False,
    "USE_NNTP": False,
    "USE_GOPHER": False,
    "ID_EXT": True,
    "MATRIX": True,
    "FORMAT_NICE": True,
    "DONT_CALL_GC_AFTER_FORK": False,
}

MACRO_PATTERN = re.compile(r"^#\s*(\S+)\s*(\S.*)?")


class Include(NamedTuple):
    value: str


class Define(NamedTuple):
    value: str


class If(NamedTuple):
    value: str

    def eval(self, context) -> Optional[bool]:
        match self.value:
            case "0":
                return False
            case "1":
                return True
        return None


class Ifdef(NamedTuple):
    value: str

    def eval(self, context) -> Optional[bool]:
        return context.get(self.value)


class Ifndef(NamedTuple):
    value: str

    def eval(self, context) -> Optional[bool]:
        match context.get(self.value):
            case True:
                return False
            case False:
                return True
            case None:
                return None


class Elif(NamedTuple):
    value: str

    def eval(self, context) -> Optional[bool]:
        return None


class Else:
    def __str__(self) -> str:
        return "Else"


class Endif:
    def __str__(self) -> str:
        return "Endif"


def parse_macro(
    l: str,
) -> Union[None, Include, Define, If, Ifdef, Ifndef, Else, Elif, Endif]:
    m = MACRO_PATTERN.match(l)
    if m:
        match m.group(1):
            case "include":
                return Include(m.group(2).strip())
            case "define":
                return Define(m.group(2).strip())
            case "if":
                return If(m.group(2).strip())
            case "ifndef":
                return Ifndef(m.group(2).strip())
            case "ifdef":
                return Ifdef(m.group(2).strip())
            case "elif":
                return Elif(m.group(2).strip())
            case "else":
                return Else()
            case "endif":
                return Endif()


class MacroNode:
    def __init__(self, begin: int, begin_macro, prev=None) -> None:
        self.prev = prev
        self.begin = begin
        self.begin_macro = begin_macro
        self.end = begin
        self.end_macro = None
        self.children: List[MacroNode] = []
        self.result = None

    def close(self, end, end_macro):
        self.end = end
        if end_macro:
            self.end_macro = end_macro

    def get_prevs(self):
        prevs = []
        current = self
        while True:
            prevs.append(current.result)
            if not current.prev:
                break
            current = current.prev
        return prevs

    def eval(self, context):
        prevs = self.get_prevs()

        match self.begin_macro:
            case None:
                pass
            case If() | Ifndef() | Ifdef() as m:
                self.result = m.eval(context)
            case Elif() as m:
                if True in prevs:
                    pass
                else:
                    self.result = m.eval(context)
            case Else() as m:
                if any(prevs):
                    assert self.result == None
                    self.result = False
                elif any(prev == False for prev in prevs):
                    # has false
                    assert self.result == None
                    self.result = True
            case _:
                raise NotImplementedError()

        for child in self.children:
            child.eval(context)

    def print(self, indent=""):
        match self.result:
            case True:
                color = "green"
            case False:
                color = "red"
            case None:
                color = "grey"
            case _:
                raise NotImplementedError()

        print(f"{self.begin:04} ~ {self.end:04}:{indent}{self.begin_macro}")
        for child in self.children:
            child.print(indent + "  ")

    def apply(self, lines):
        removed = False
        # begin
        match self.result:
            case True | False:
                # lines[self.begin] = '// ' + lines[self.begin]
                lines[self.begin] = None
                removed = True
            case None:
                pass

        l = self.begin + 1
        for child in self.children:
            while l < child.begin:
                if self.result == False:
                    lines[l] = None
                l += 1
            child.apply(lines)
        while l < self.end:
            if self.result == False:
                lines[l] = None
            l += 1

        # end
        if self.end_macro and removed:
            # lines[self.end] = '// ' + lines[self.end]
            lines[self.end] = None


def main(path: pathlib.Path, debug=False):
    if path.suffix not in [".h", ".c", ".cpp", ".sym"]:
        return
    if path.name.startswith("euc"):
        return

    print(str(path))
    lines = path.read_text(encoding="utf-8").splitlines()
    root = MacroNode(0, None)
    stack = [root]
    for i, l in enumerate(lines):
        l = l.rstrip()
        if l.startswith("#"):
            match parse_macro(l):
                case Include() as m:
                    pass
                    # print(f'[INCLUDE] => {m}')
                case Define() as m:
                    pass
                    # print(f'[DEFINE] => {m}')
                case If() | Ifndef() | Ifdef() as m:
                    node = MacroNode(i, m)
                    stack[-1].children.append(node)
                    stack.append(node)
                case Elif() | Else() as m:
                    stack[-1].close(i, None)
                    prev = stack.pop()
                    # new node
                    node = MacroNode(i, m, prev)
                    stack[-1].children.append(node)
                    stack.append(node)
                case Endif() as m:
                    stack[-1].close(i, m)
                    stack.pop()
                    pass
                case _:
                    print(l)
                    # print(l)
    assert len(stack) == 1
    root.close(len(lines) - 1, None)
    root.eval(CONTEXT)

    if debug:
        root.print()
    else:
        root.apply(lines)
        path.write_text(
            "".join(l + "\n" for l in lines if l != None),
            encoding="utf-8",
            newline="\n",
        )


if __name__ == "__main__":
    debug = False
    # debug = True
    # for arg in sys.argv[1:]:
    #     main(pathlib.Path(arg), debug)

    dir = HERE
    if len(sys.argv) > 1:
        dir = pathlib.Path(sys.argv[1]).absolute()

    for root, dirs, files in os.walk(dir):
        for f in files:
            main(pathlib.Path(root) / f, debug)
