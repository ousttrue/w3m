---
title: typedef struct の除去
date: 2024-01-30
tags: [cpp]
---

これからヘッダーを小分けにしていくのに、
前方宣言を使いたいのだけどその時に typedef に配慮が必要です。

```c title="C の typedef struct"
typedef struct NodeTag
{
  const char *name;
  struct NodeTag *next; // 👈  
} Node;
// 前方宣言
struct Node; // これはだめ
struct NodeTag; // こっちが必要

// ならば同名にすれば？
typedef struct Node
{
  const char *name;
  struct Node *next; // 👈  
} Node;
// 前方宣言
struct Node; 

// 参照するときに struct が要る
void addLink(struct Node* node, struct Node* next);
```

```cpp title="C++ はこれで OK"
struct Node
{
  const char *name;
  Node *next;
};
// 前方宣言
struct Node; // OK

// 楽
void addLink(Node* node, Node* next);
```

なので struct の typedef を削除します。
ほぼ単純作業です。

