#include "../../cgo_api.h"
#include "../../cgo_annotator.h"
#include "../../cgo_tools.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

AnnotatorInitResult AnnotatorInit(AnnotatorInitParams params) {
  AnnotatorInitResult ret{.ptr = nullptr, .error = nullptr};
  try {
    ret.ptr = new cgo::Annotator(params);
  } catch (const std::exception &e) {
    cgo::FillStr(&ret.error, e.what());
    return ret;
  }

  if (ret.ptr == nullptr) {
    cgo::FillStr(&ret.error, "could't allocate memory");
  }
  return ret;
}

void AnnotatorFree(AnnotatorPtr aPtr) { delete (cgo::Annotator *)aPtr; }

void AnnotateResultFree(AnnotateResultPtr rPtr) {
  if (rPtr != nullptr) {
    AnnotateResult *res = (AnnotateResult *)rPtr;
    cgo::StringFree(res->model);
    cgo::StringArrayFree(res->abstracts);
    cgo::StringArrayFree(res->keywords);
    cgo::StringFree(res->error);
    delete res;
  }
}

AnnotateResult Annotate(AnnotateParams aParams) {
  auto *res = new AnnotateResult;
  res->self = res; // сохраняем себя чтобы потом вызвать delete

  cgo::Annotator *ann = (cgo::Annotator *)aParams.aPtr;
  if (ann == nullptr) {
    res->error = cgo::StringNew("annotator is nil");
    return *res;
  }

  try {
    ann->Annotate(aParams, res);
  } catch (const std::exception &e) {
    res->error = cgo::StringNew(e.what());
  } catch (...) {
    res->error = cgo::StringNew("Annotate(): Unknown failure occurred");
  }

  return *res;
}
