---
title: w3m改造日記
tags: [w3m]
---

なんとなく最初からやりなおし。
長くなるので作業メモを作った。

# 目標

- c++ にして改造しやすくする
- colorscheme 付きの nvim みたいな感じに
- tab を非同期に
- Windows(msvc) でも動くように

# 事前作業

- [x] build system を meson に変更

とりあえずビルド通した。
生成ファイルに注意。
configure と Makefile のもの

- config.h 
- version.c
- funcname.c
- functable.c
- funcname1.h
- funcname2.h

あと一部 doc の中身にアクセスしていた(help 生成？)。

# 改造する前にコード量を減らす

- macro の分岐をカットして量を減らす
  - http と https 以外を削除する
  - frame を削除する
  - 文字コード utf-8 専用にして libwc を切り離す
    - 多言語化を utf-8 単言語化することでかなり単純化できる目算
  - escape sequence を削除する
  - platform は POSIX を主。副で msvc。unix や cygwin は削除
  - pager の PIPE api も削除
  - sixel とか画像系も削除

5000行くらいに減れば手に負えるのだが…

# c++ 化して改造する

- main loop を libuv
- command を lua に
  - キーマップの責務
- std::string にしてみる

