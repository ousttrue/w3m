---
title: content-encoding
date: 2024-02-20
tags: [http]
---

Content-Encoding 処理が input_stream と密結合だった。

<!-- truncate -->

わりと深く結合していて迂闊に変えると動かなくなる。

## Content-Encoding 

最低限 gz の動作を確保したい。

[Content-Encoding](https://developer.mozilla.org/ja/docs/Web/HTTP/Headers/Content-Encoding)

```c
// Content-Encoding: gzip

enum CompressionType {
  CMP_NOCOMPRESS = 0,
  CMP_COMPRESS = 1,
  CMP_GZIP = 2, // 👈 gz
  CMP_BZIP2 = 3,
  CMP_DEFLATE = 4,
  CMP_BROTLI = 5,
};
```

子プロセスで `gzip -d` している。
ライブラリで組込むとすると zlib でできるようだ。
Windows に対応に備えて組み込んでみようかしら。

https://stackoverflow.com/questions/1838699/how-can-i-decompress-a-gzip-stream-with-zlib

