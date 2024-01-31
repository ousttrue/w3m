---
title: line editor を分離したい
date: 2024-02-05
tags: [lineeditor]
---

main loop を libuv にする前に、
line editor を整理したい。

<!-- truncate -->

line editor は自ら getch して while loop を回します。
また term に直接出力します。
入出力両方にレイヤーをひとつ挟みたい。
libuv に乗るように event driven 型に改造を試みます。

## inputLineHistSearch

この関数がブロックして結果を返す。
await に置きかえるような改造をしたい。

```c title="linein.c"
char *inputLineHistSearch(const char *prompt, char *def_str, int flag, Hist *hist,
                          int (*incfunc)(int ch, Str *buf, Lineprop *prop));
```

これを返り値じゃなくて返り値を使うコールバック引数に変えればよいのだけど、
分岐の中で使っている場合に書き換えが難しい。
あと、普通に使っているところが多い。
どうやって書き変えるか…

とりあえず、分岐が合流しないようにすることで少し楽にできそうか。
