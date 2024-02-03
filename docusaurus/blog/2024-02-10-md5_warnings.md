---
title: MD5 deprecated warning
date: 2024-02-10
tags: [cpp]
---

MD5 警告が多くてメッセージが埋もれるのがつらいので対処を試みる。

<!-- truncate -->

```c
OSSL_DEPRECATEDIN_3_0 unsigned char *MD5(const unsigned char *d, size_t n,
                                         unsigned char *md);
```

## 情報収集…

- [&#39;MD5&#39; is deprecated: Since OpenSSL 3.0 · Issue #4243 · gluster/glusterfs · GitHub](https://github.com/gluster/glusterfs/issues/4243)

```c
EVP_md5 ?
```


