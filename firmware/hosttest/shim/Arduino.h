// Just enough of Arduino for rendering the setup page on a computer.
#pragma once
#include <string>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#define PROGMEM
#define FPSTR(p) String(p)
class String {
 public:
  std::string s;
  String() {}
  String(const char* c) : s(c ? c : "") {}
  String(const std::string& x) : s(x) {}
  String(int v) : s(std::to_string(v)) {}
  String(unsigned v) : s(std::to_string(v)) {}
  String(char c) : s(1, c) {}
  unsigned length() const { return s.size(); }
  const char* c_str() const { return s.c_str(); }
  char operator[](unsigned i) const { return s[i]; }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* o) { s += o; return *this; }
  String& operator+=(char c) { s += c; return *this; }
  friend String operator+(const String& a, const String& b) { return String(a.s + b.s); }
  friend String operator+(const String& a, const char* b) { return String(a.s + b); }
  friend String operator+(const char* a, const String& b) { return String(std::string(a) + b.s); }
  bool operator==(const String& o) const { return s == o.s; }
  bool operator==(const char* o) const { return s == o; }
  bool operator!=(const String& o) const { return s != o.s; }
  int indexOf(const String& x, unsigned from = 0) const { auto p = s.find(x.s, from); return p == std::string::npos ? -1 : (int)p; }
  int indexOf(char c, unsigned from = 0) const { auto p = s.find(c, from); return p == std::string::npos ? -1 : (int)p; }
  String substring(unsigned a, unsigned b = (unsigned)-1) const { if (a > s.size()) return String(); return String(s.substr(a, b == (unsigned)-1 ? std::string::npos : b - a)); }
  void trim() { size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n"); s = a == std::string::npos ? "" : s.substr(a, b - a + 1); }
  long toInt() const { return atol(s.c_str()); }
};
template <class T> T constrain(T v, T a, T b) { return v < a ? a : v > b ? b : v; }
