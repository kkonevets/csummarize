// функции для работы с памятью для cgo

#ifndef _CGO_TOOLS_HPP_
#define _CGO_TOOLS_HPP_

#include "cgo_api.h"

namespace cgo {

// функции для выделения и освобождения памяти для строк

String StringNew();

String StringNew(size_t size);

String StringNew(const char *from, size_t size);

String StringNew(const char *from);

StringArray StringArrayNew(size_t size);

void StringFree(String s);

void StringArrayFree(StringArray sa);

// использую malloc так как освобождать на стороне Go
void FillStr(char **dst, const char *src);

} // namespace cgo

#endif //_CGO_TOOLS_HPP_
