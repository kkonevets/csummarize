#include "../../cgo_tools.h"

#include <string.h>

namespace cgo {

String StringNew() { return String{.data = nullptr, .size = 0}; }

String StringNew(size_t size) {
  if (size == 0) {
    return StringNew();
  }
  char *data = new char[size]();
  return String{.data = data, .size = size};
}

String StringNew(const char *from, size_t size) {
  if (size == 0) {
    return StringNew();
  }
  char *data = new char[size]();
  memcpy(data, from, size);
  return String{.data = data, .size = size};
}

String StringNew(const char *from) { return StringNew(from, strlen(from)); }

StringArray StringArrayNew(size_t size) {
  String *data = new String[size]();
  return StringArray{.data = data, .size = size};
}

void StringFree(String s) { delete[] s.data; s.data = nullptr; s.size = 0; }

void StringArrayFree(StringArray sa) {
  if (sa.data != nullptr) {
    for (size_t i = 0; i < sa.size; ++i) {
      delete[] sa.data[i].data;
    }
    delete[] sa.data;
  }
  sa.data = nullptr;
  sa.size = 0;
}

void FillStr(char **dst, const char *src) {
  *dst = (char *)malloc(strlen(src) + 1);
  strcpy(*dst, src);
}

} // namespace cgo
