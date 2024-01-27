---
title: マクロ削除
date: 2024-01-28
tags: [w3m]
---

`#ifdef` を確定しながらコードを減らしていく。

## config.h に注意

build コマンドの引き数と config.h の内容を連動させる。
うっかり片方だけ作業するとビルドが通らないならまだしも、壊れるかもしれない。

## nvim の treesitter が便利

`#if` と `#endif` 間のジャンプができて便利。

https://github.com/andymass/vim-matchup

また作業用に `@keyword.directive.c` に目立つ色を指定した。

## 以前使った python スクリプトを発掘

`#ifdef X` `#elseif` `#endif` を対して define を指定して有効なブロックを残して、
無効ブロックを削除するスクリプト。
うまく処理できないケースもあるが無いよりは役に立つ。
うまくいかないところは手でなおす。

## ひとつずつ減らして動作確認する

まとめて削除したらビルドは通るが動かなくなった。
少しずつ減らして、都度最低限の動作確認をして進める。
`unittest` とかあるといいのだけど無いし。
将来的には作りたいですね。

## 文字コード系

## Term系

## cygwin系

## http以外のプロトコル

## mouse操作

## frame

## menu

## image系

