---
title: mainloop
date: 2024-12-31
tags: [w3m]
---

メインループを起きかえる。

- key 入力
- socket / io 入出力
- term resize
- signal handling
- stream キャンセル
- alarm timer

あたりを libuv 管理にする。
副作用で、tab の非同期化と msvc 対応への道が開く。

tab の非同期化は global 変数を整理して reentrant 可能にする必要があるので、
わりと険しい。

signal / longjump / select などが libuv 起き替えで解決するので、
msvc 対応はわりと前進する。

