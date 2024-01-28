---
title: C++化
date: 2024-01-29
tags: [w3m]
---

C では多少型が違っても `void*` や `int` になってわりとコンパイルが通る。
関数 signature が無くてもコンパイルが通る。
C++ ではそうはいかない。
そこがいい。

なので C++ 化を最初にやって、それから改造に入る。
先にマクロ確定によってコード量を減らしているので、少し楽ができるはず。

## extern "C"

main.c を main.cpp にするところから。
main 関数以外を `extern "C"` で囲う。

思ったより難航した。
容易にコンパイルが通らない。
C++ になることによって暗黙の型変換に対処する必要がある。
`void*` を任意の型に変換してくれません。

## signal.h

signal handler の関数ポインターの型が混乱の元なので
きっちり局所化して隔離した。
今回は、POSIX 一本で決め打ちだし。

```c
void handler(int);

// わかりづらい
void *MySignal(int signal, void(*handler)(int))(int);

// たぶんこう
typedef void(*MyHandlerType)(int);
MyHandlerType MySignal(int signal, MyHandlerType handler);
```

## const char*

これは適当に。諦め(cast)も必要。

