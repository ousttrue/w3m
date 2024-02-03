#pragma once

template <typename T> inline T operator|(T l, T r) {
  return static_cast<T>((int)l | (int)r);
}
template <typename T> inline T &operator|=(T &l, T r) {
  l = l | r;
  return l;
}
template <typename T> inline T operator~(T l) {
  return static_cast<T>(~(int)l);
}
template <typename T> inline T operator&(T l, T r) {
  return static_cast<T>((int)l & (int)r);
}
template <typename T> inline T &operator&=(T &l, T r) {
  l = l & r;
  return l;
}
#define ENUM_OP_INSTANCE(T)                                                    \
  template <typename T> inline T operator|(T l, T r);                          \
  template <typename T> inline T &operator|=(T &l, T r);                       \
  template <typename T> inline T operator~(T l);                               \
  template <typename T> inline T operator&(T l, T r);                          \
  template <typename T> inline T &operator&=(T &l, T r);
