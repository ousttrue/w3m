---
title: typedef struct の除去
date: 2024-01-30
tags: [w3m, cpp]
---

これからヘッダーを小分けにしていくのに、
前方宣言を使いたいのだけどその時に typedef に配慮が必要です。

```c title="C の typedef struct"
typedef struct NodeTag
{
  const char *name;
  sturct NodeTag *next; // 👈  
} Node;
// 前方宣言
struct Node; // これはだめ
struct NodeTag; // こっちが必要
```

```cpp title="C++ はこれで OK"
struct Node
{
  const char *name;
  Node *next;
};
// 前方宣言
struct Node; // OK
```

なので struct の typedef を削除します。
ほぼ単純作業です。

