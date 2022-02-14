#ifndef PREGEX_H_
#define PREGEX_H_

#include <absl/container/flat_hash_map.h>
#include <absl/strings/string_view.h>
#include <algorithm>
#include <memory>
#include <re2/filtered_re2.h>
#include <re2/re2.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "baalbek/babylon/document.hpp"
#include "baalbek/babylon/languages/rus.hpp"
#include "baalbek/babylon/lingproc.hpp"

// TODO: вынести множество шаблонов без \&слово в отдельный FilteredRE2
// TODO: создать отдельную функцию, которая будет делать только match

namespace pregex {

// представляет подстроку, начинающуюся с байта pos длиной len байт
struct SubString {
  size_t offset = 0;
  size_t len = 0;
  SubString(size_t offset, size_t len) : offset(offset), len(len) {}
};

// хранит замену подстроки на лексему
struct Substitution {
  SubString ss;
  absl::string_view term;
  Substitution(const SubString &ss, absl::string_view term)
      : ss(ss), term(term) {}
};

using s2s_t = absl::flat_hash_map<std::string, std::string>;
// maps term to it's words
using s2ss_t = absl::flat_hash_map<std::string, std::vector<SubString>>;

struct PreparedTxt {
  PreparedTxt() = default;
  PreparedTxt(s2ss_t &&term2words, std::vector<absl::string_view> &&sents);

  s2ss_t term2words;
  std::vector<absl::string_view> sents;
};

// печатает лемму в шестнадцатеричном формате
std::string HexTerm(absl::string_view term);

// хранит вспомогательные данные одного шаблона
struct PatValue {
  std::unique_ptr<re2::FilteredRE2> filter;
  s2s_t word2term; // соответствие слова его единственной лемме
  std::vector<std::string> atoms; // слова из шаблона
  PatValue(const std::string &pat, s2s_t &&w2t);
};

// Хранит набор регулярных выражений.
// "original pattern" -> PatValue
class PatternSet : public absl::flat_hash_map<std::string, PatValue> {
public:
  explicit PatternSet();

  const Baalbek::language::processor &lingproc() const { return lingproc_; }

  // build document image from string
  Baalbek::language::docimage BuildImage(const std::string &txt) const;

  // Заменить амперсанд на лемму в шаблоне и сохранить регулярное выражение
  void Add(const std::string &pat);

  // Подготовить текст для последующих вызовов MatchedIndexes
  // `breaks` - ОТСОРТИРОВАННЫЙ массив, содержащий индексы символов концов
  // предложений из docimage
  static PreparedTxt PrepareTxt(const std::string &txt,
                         const Baalbek::language::docimage &doci,
                         const std::vector<size_t> &breaks);

  // Вернуть индексы слов из docimage, удовлетворяющие хотябы одному регулярному
  // выражению, учитывая границы предложений
  std::set<size_t> MatchedIndexes(const PreparedTxt &pTxt) const;

private:
  Baalbek::document CreateDoc(absl::string_view txt) const;

  // Для оптимизации скорости, перед выполнением регулярного выражения,
  // проверяется что все леммы из шаблона содержатся в `txt`,
  // неуспех означает что шаблону не найдено соответствие в `txt`.
  bool FastFall(const std::string &new_txt, const PatValue &pval) const;

  // Заменить в источнике `txt` слова на термы из шаблона, из омонимов
  // оставить только один терм
  std::string Substitute(absl::string_view txt, size_t sent_offset,
                         const std::vector<Substitution> &subs) const;

  // Построить соответствие терм->слова по документу
  static s2ss_t GetTerm2Words(absl::string_view txt,
                       const Baalbek::language::docimage &doci);

  // обновляет индексы ixs по предложению
  void UpdateSentenceIndexes(const re2::RE2 &re, absl::string_view new_sent,
                             size_t nwords, std::set<size_t> &ixs) const;

  // подстановки для кадого регулярного выражения
  std::vector<std::vector<Substitution>>
  SubstitutionsPerRegex(const s2ss_t &term2words) const;

  Baalbek::language::processor lingproc_;
};

} // namespace pregex

#endif // PREGEX_H_
