---
title: source と header をペアにする
date: 2024-01-31
tags: [cpp]
---

巨大なヘッダー proto.h に大量の関数が詰め込んである。
fm.h に主要な型が詰め込んである。
あと file.cpp(元 file.c)が異常に巨大で 7000 行くらいある。
これらを適当に分割して、source と header のペアのモジュールに分割したい。

<!-- truncate -->

この分割作業に前段で実施した、typedef struct の除去が効果がある。
ポインター参照しかない型を安心して前方宣言できる。

