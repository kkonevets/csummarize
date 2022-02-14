#include "../../pregex.hpp"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/string_view.h>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <re2/filtered_re2.h>
#include <re2/re2.h>
#include <regex.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <vector>

#include "baalbek/babylon/document.hpp"
#include "baalbek/babylon/languages/rus.hpp"
#include "baalbek/babylon/lingproc.hpp"
#include "moonycode/codes.h"

namespace pregex {

std::string HexTerm(absl::string_view term) {
  std::string hex;
  hex.reserve(term.size() * 2);
  char temp[3];
  for (size_t i = 0; i < term.size(); ++i) {
    auto n = sprintf(temp, "%02x", static_cast<unsigned char>(term[i]));
    if (n < 0) {
      throw std::runtime_error("could't convert term to hex");
    }
    hex.append(temp, temp + 2);
  }
  return hex;
}

PatValue::PatValue(const std::string &pat, s2s_t &&w2t)
    : word2term{std::move(w2t)} {
  re2::RE2::Options ops;
  ops.set_case_sensitive(false);
  ops.set_log_errors(false);
  filter = std::make_unique<re2::FilteredRE2>();

  int id;
  re2::RE2::ErrorCode err = filter->Add(pat, ops, &id);
  if (err != re2::RE2::NoError) {
    ops.set_log_errors(true);
    re2::RE2 re(pat, ops);
    std::ostringstream ss;
    ss << pat << ": " << re.error();
    throw std::runtime_error(ss.str());
  }
  filter->Compile(&atoms);
}

PatternSet::PatternSet() {
  lingproc_.AddLanguageModule(0, new Baalbek::language::Russian());
}

Baalbek::document PatternSet::CreateDoc(absl::string_view txt) const {
  Baalbek::document doc;

  if (txt.empty()) {
    return doc;
  }

  doc.add_str(txt.data(), txt.size()).set_codepage(codepages::codepage_utf8);
  return lingproc_.NormalizeEncoding(doc);
}

Baalbek::language::docimage
PatternSet::BuildImage(const std::string &txt) const {
  return lingproc_.MakeDocumentImage(CreateDoc(txt));
}

void PatternSet::Add(const std::string &pat) {
  s2s_t word2term;

  auto new_pat = absl::StrReplaceAll(
      pat, {{"\\word", "(\\s*((\\p{Cyrillic}|0-9A-Za-z_\"'-)+|,)\\s*)"}});

  // replace all consecutive spaces with [ ]+
  static const re2::RE2 spaces_re{R"([ ]+)"};
  re2::RE2::GlobalReplace(&new_pat, spaces_re, "[ ]+");

  // найдем все подстроки вида \&слово
  static const LazyRE2 re{R"(\&(\p{Cyrillic}+))"};
  std::string word;
  re2::StringPiece input(new_pat);
  while (re2::RE2::FindAndConsume(&input, *re, &word)) {
    word2term.try_emplace(word);
  }

  std::string corpus;
  for (const auto &el : word2term) {
    absl::StrAppend(&corpus, el.first, " ");
  }

  auto doci = BuildImage(corpus);

  std::vector<std::pair<std::string, std::string>> pairs;
  auto it = word2term.begin();
  for (const auto &w : doci) {
    if (w.size() > 1) {
      std::ostringstream ss;
      ss << "у \'" << it->first << "\' есть омонимы, неоднозначность в шаблоне";
      throw std::runtime_error(ss.str());
    }
    auto &term = w.back();
    it->second = HexTerm({term.data(), term.size()});
    pairs.emplace_back(absl::StrCat("\\&", it->first), it->second);
    ++it;
  }

  this->try_emplace(pat, absl::StrReplaceAll(new_pat, pairs),
                    std::move(word2term));
}

// передвинуть указатель на utf-8 строку на woffset рун вперед
template <class I> I Utf8Advance(I begin, I end, size_t woffset) {
  auto it = begin;
  size_t offset = 0;
  while (it < end && offset != woffset) {
    it += codepages::utf8::cbchar(it);
    ++offset; // увеличить счетчик рун
  }
  return it;
};

s2ss_t PatternSet::GetTerm2Words(absl::string_view txt,
                                 const Baalbek::language::docimage &doci) {
  s2ss_t ret;

  auto it = txt.begin();
  size_t offset = 0;
  for (const auto &w : doci) {
    if (w.IsPunct()) {
      continue;
    }
    it = Utf8Advance(it, txt.end(), w.offset - offset);
    offset = w.offset;
    auto mbcs_size = codepages::utf8::cbchar(w.pwsstr, w.length);
    for (const auto &term : w) {
      auto p = ret.try_emplace(HexTerm({term.data(), term.size()}));
      auto &val = p.first->second;
      val.emplace_back(it - txt.begin(), mbcs_size);
    }
  }

  return ret;
}

std::vector<std::vector<Substitution>>
PatternSet::SubstitutionsPerRegex(const s2ss_t &term2words) const {
  std::vector<std::vector<Substitution>> ret;
  ret.reserve(this->size());

  auto cmp = [](const Substitution &a, const Substitution &b) -> bool {
    return a.ss.offset < b.ss.offset;
  };

  for (const auto &ps : *this) {
    auto &pval = ps.second;
    std::set<Substitution, decltype(cmp)> subs(cmp);
    for (const auto &el : pval.word2term) {
      auto it = term2words.find(el.second);
      if (it != term2words.end()) { // term found
        // replace all word occurances with term
        for (const auto &ss : it->second) {
          subs.emplace(ss, el.second);
        }
      }
    }

    ret.emplace_back(subs.begin(),subs.end());
  }

  return ret;
}

std::string
PatternSet::Substitute(absl::string_view txt, size_t sent_offset,
                       const std::vector<Substitution> &subs) const {

  // Замена слов на лексемы с учетом начала и конца слова чтобы избежать такой
  // ситуации: для шаблона "\&ходить" и строки "ходить и приходить" замена
  // выдаст что-то вроде "ab2c0f32 и приab2c0f32", а для шаблона "\&поход" и
  // строки "поход и походить" выдаст "cb32da4 и cb32da4ить".

  std::string ret;
  size_t offset = 0;
  for (const auto &sub : subs) {
    size_t ss_end = sub.ss.offset - sent_offset + sub.ss.len;
    // начало или конец подстановки не попали в предложение
    if (sub.ss.offset < sent_offset || ss_end > txt.size()) {
      continue;
    }

    ret.append(txt.begin() + offset, txt.begin() + sub.ss.offset - sent_offset);
    ret.append(sub.term.begin(), sub.term.end());
    offset = ss_end;
  }

  ret.append(txt.begin() + offset, txt.end());

  return ret;
}

// аргументы для удобного вызова re2::RE2::FindAndConsumeN
struct CapturingGroupArgs {
  std::vector<RE2::Arg> args;
  std::vector<RE2::Arg *> args_ptrs;
  std::vector<re2::StringPiece> subs; // substrings
  int n_args;
  CapturingGroupArgs(int n_args)
      : args(n_args), args_ptrs(n_args), subs(n_args), n_args(n_args) {
    for (int i = 0; i < n_args; ++i) {
      args[i] = &subs[i];      // Bind argument to string from vector.
      args_ptrs[i] = &args[i]; // Save pointer to argument.
    }
  }
};

bool PatternSet::FastFall(const std::string &new_txt,
                          const PatValue &pval) const {
  std::vector<char> lower(new_txt.size());
  codepages::strtolower(codepages::codepage_utf8, lower.data(), lower.size(),
                        new_txt.data(), new_txt.size());
  absl::string_view lower_view(lower.data(), lower.size());

  std::vector<int> atom_indices;
  for (size_t i = 0; i < pval.atoms.size(); ++i) { // заменить на Aho-Corasick
    if (lower_view.find(pval.atoms[i]) != std::string::npos) {
      atom_indices.push_back(i);
    }
  }

  return pval.filter->FirstMatch({lower.data(), lower.size()}, //
                                 atom_indices) == -1;
}

std::vector<absl::string_view>
BreakOnSentences(const std::string &txt,
                 const Baalbek::language::docimage &doci,
                 const std::vector<size_t> &breaks) {
  absl::string_view txt_view(txt);
  if (breaks.empty()) {
    return {txt_view};
  }

  std::vector<absl::string_view> sents;

  auto it = txt_view.begin();
  size_t offset = 0;
  for (const auto &bix : breaks) {
    auto &w = doci.at(bix);
    auto ait = Utf8Advance(it, txt_view.end(), w.offset + w.length - offset);
    sents.emplace_back(it, ait - it);
    it = ait;
    offset = w.offset + w.length;
  }

  return sents;
}

void PatternSet::UpdateSentenceIndexes(const re2::RE2 &re,
                                       absl::string_view new_sent,
                                       size_t nwords,
                                       std::set<size_t> &ixs) const {
  re2::StringPiece input(new_sent.data(), new_sent.size());
  const auto words = lingproc_.WordBreakDocument(CreateDoc(new_sent));
  auto wit = words.begin();
  size_t from = 0; // номер руны с которой начинается группа

  // множественный захват групп
  auto cga = CapturingGroupArgs(re.NumberOfCapturingGroups());
  for (auto prev = new_sent.data();
       re2::RE2::FindAndConsumeN(&input, re, cga.args_ptrs.data(),
                                 cga.n_args) &&
       prev != nullptr;) {
    for (const auto &sub : cga.subs) { // захваченные группы
      if (prev == nullptr) {
        continue;
      }
      from += codepages::utf8::strlen(prev, sub.data() - prev);
      auto till = from + codepages::utf8::strlen(sub.data(), sub.size());
      prev = sub.data();

      // printf("%lu %lu %.*s\n", from, till, (int)sub.size(), sub.data());

      auto comp = [](auto &word, size_t value) { return word.offset < value; };
      // найдем слово с которого начинается подстрока
      wit = std::lower_bound(wit, words.end(), from, comp);
      if (wit != words.begin()) {
        auto pwit = std::prev(wit);
        if (pwit->offset + pwit->length > from) {
          // зашли во внутренность предыдущего слова
          // например, шаблон "(\\&в деньги)" для "вагонов деньги!"
          continue;
        }
      }
      // используя offset получим соответствующие индексы подстроки
      for (; wit != words.end() && wit->offset < till; ++wit) {
        ixs.insert(nwords + (wit - words.begin()));
      }
    }
  }
}

PreparedTxt::PreparedTxt(s2ss_t &&term2words,
                         std::vector<absl::string_view> &&sents)
    : term2words(std::move(term2words)), sents(std::move(sents)) {}

PreparedTxt PatternSet::PrepareTxt(const std::string &txt,
                                   const Baalbek::language::docimage &doci,
                                   const std::vector<size_t> &breaks) {
  // запомним из каких слов получены лексемы: term -> [w1, w2, w3, ...]
  return PreparedTxt(GetTerm2Words(txt, doci),
                     BreakOnSentences(txt, doci, breaks));
}

std::set<size_t> PatternSet::MatchedIndexes(const PreparedTxt &pTxt) const {
  std::set<size_t> ixs;
  if (this->empty()) {
    return ixs;
  }

  auto subs = SubstitutionsPerRegex(pTxt.term2words);

  size_t nwords = 0;
  size_t sent_offset = 0;
  for (const auto &sent : pTxt.sents) {
    // printf("%.*s\n", (int)sent.size(), sent.data());
    size_t regex_i = 0;
    for (const auto &el : *this) {
      // подставим все лексемы из шаблона вместо слов
      auto new_sent = Substitute(sent, sent_offset, subs[regex_i++]);

      const auto &pval = el.second;
      if (FastFall(new_sent, pval)) {
        continue;
      }

      UpdateSentenceIndexes(pval.filter->GetRE2(0), new_sent, nwords, ixs);
    }

    nwords += lingproc_.WordBreakDocument(CreateDoc(sent)).size();
    sent_offset += sent.size();
  }

  return ixs;
}

} // namespace pregex
