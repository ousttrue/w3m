# w3m改 zig

w3m を改造する(5週目くらいw)。

以前は c++ で改造を試みていたのだがうまくいかなかった。
今回は zig 化を視野に繋ぎに `-std=gnu23` で作業。

- [x] zig cc で build する
- [x] windows で動作する
- [ ] utf-8 が表示できる
- [ ] dom 入れたい

## dependencies

[dependencies](./build.zig.zon)

- https://github.com/ivmai/bdwgc build.zig
- https://github.com/allyourcodebase/openssl build.zig
  - https://kb.firedaemon.com/support/solutions/articles/4000121705 windows版はこちらの prebuilt

## 量を減らすため web browser のコア機能(独断)以外を削除

- [x] 多言語 libwc
- [x] pager, pipe 連結
- [x] dump, backend, commandline. url 引数一個決め打ち
- [x] color
- [x] image
- [x] frame

## windows portable

- [x] ncurses/termca/terminfo 依存削除。xterm 決めうちの escape sequence
- [x] tty, ioctl, SIGWINCH / conpty
- [x] socket / winsock
- [ ] ssl
- [ ] gzip, inflate
- [ ] fork
