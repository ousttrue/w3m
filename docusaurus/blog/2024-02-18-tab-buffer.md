---
title: tab と buffer
date: 2024-02-18
tags: [cpp]
---

データ構造

<!-- truncate -->

## TabBuffer

link list。たぶん左から順番。

```text
first         current      last
|             |            |
V             v            v
+TabBuffer+  +TabBuffer+  +TabBuffer+
|next     |->|next     |->|next     |--> nullptr
+---------+  +---------+  +---------+
```

## Buffer

TabBuffer 毎。
link list。

```text
TabBuffer  TabBuffer TabBuffer
first      current   last
|          |         |
V          v         v
+Buffer+  +Buffer+  +Buffer+
|next  |->|next  |->|next  |--> nullptr
+------+  +------+  +------+
```
