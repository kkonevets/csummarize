/*!==========================================================================
* \file
* - Program:       gtest-csummerize
* - File:          gtest-pregex.h
* - Created:       09/09/2021
* - Author:
* - Description:
* - Comments:
*
-----------------------------------------------------------------------------
*
* - History:
*
===========================================================================*/
#pragma once
//-------------------------------------------------------------------------//
#ifndef __GTEST_PREGEX_H_ABA884D5_28F7_4B5F_8DCD_AA954D952D61__
#define __GTEST_PREGEX_H_ABA884D5_28F7_4B5F_8DCD_AA954D952D61__
//-------------------------------------------------------------------------//
#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/string_view.h>
#include <algorithm>
#include <bits/types/__FILE.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <re2/filtered_re2.h>
#include <re2/re2.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../pregex.hpp"
#include <baalbek/babylon/document.hpp>
#include <baalbek/babylon/lingproc.hpp>
#include <moonycode/codes.h>
#include <mtc/config.h>
#include <mtc/platform.h>
//-------------------------------------------------------------------------//
using namespace pregex;
//-------------------------------------------------------------------------//
std::string CurrentDir() {
  std::string cur_file = __FILE__;
  size_t found = cur_file.find_last_of("/");
  return std::string(cur_file, 0, found);
}

const std::string kCurrentDir = CurrentDir(); // directory of this file

std::vector<std::string> GetLines(absl::string_view fname) {
  std::vector<std::string> sents;
  std::ifstream infile(fname.data());
  if (infile.fail()) {
    throw std::runtime_error("could't open file");
  }
  std::string line;
  while (std::getline(infile, line)) {
    sents.emplace_back(line);
  }

  return sents;
}

std::vector<std::string> GetPatternStrings() {
  std::vector<std::string> ret;
  const std::string pat = R"((?:мы|он|она|они)\word{0,2}\&XXX)";

  auto words = GetLines(kCurrentDir + "/../data/words.txt");

  for (const auto &w : words) {
    ret.emplace_back(absl::StrReplaceAll(pat, {{"XXX", w}}));
  }

  return ret;
}

template <class D = std::chrono::milliseconds,
          class T = std::chrono::high_resolution_clock::time_point>
void print_duration(T start, T end) {
  printf("%lu ms\n", std::chrono::duration_cast<D>(end - start).count());
}

TEST(Pregex, Speed) {
  PatternSet col;

  std::ifstream infile(kCurrentDir + "/../data/input.txt");
  ASSERT_TRUE(!infile.fail());
  std::stringstream corpus;
  corpus << infile.rdbuf();
  auto doci = col.BuildImage(corpus.str());

  std::vector<size_t> breaks;
  size_t ix = 0;
  for (const auto &w : doci) {
    if (w.pwsstr[0] == 46) { //.
      breaks.push_back(ix);
    }
    ++ix;
  }

  // добавляем шаблоны в коллекцию, которые можно будет использовать повторно
  for (const auto &s : GetPatternStrings()) {
    col.Add(s);
  }

  // col.Add(R"((\p{Cyrillic}\-\pN+))");

  auto start = std::chrono::high_resolution_clock::now();
  col.MatchedIndexes(col.PrepareTxt(corpus.str(), doci, breaks));
  auto end = std::chrono::high_resolution_clock::now();
  print_duration(start, end);
}

TEST(Pregex, OneBreaks) {
  std::string sent =
      "Привет, Вася! Они сегодня все придут к тебе. Я тоже наверно приду.";

  PatternSet col;
  col.Add(R"((\pL+. \pL+ )тоже)");
  col.Add(R"((вася! они))");

  auto doci = col.BuildImage(sent);

  {
    auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
    std::set<size_t> expected{2, 3, 4, 9, 10, 11};
    ASSERT_EQ(ixs, expected);
  }

  {
    auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {3, 10, 15}));
    ASSERT_TRUE(ixs.empty());
  }
}

TEST(Pregex, One) {
  PatternSet col;
  col.Add(R"((?:мы|он|она|они) (\word{0,2}\&придти))");
  col.Add(R"(привет(?:[,.!])+\word)");
  col.Add(R"(\word захват \word)");

  std::string sent = "Привет, Вася! Они сегодня все придут к тебе. Он тоже "
                     "наверно придет. Множественный захват групп.";

  auto doci = col.BuildImage(sent);

  auto start = std::chrono::high_resolution_clock::now();
  auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {3, 10, 15, 19}));
  auto end = std::chrono::high_resolution_clock::now();
  print_duration(start, end);

  for (const auto &ix : ixs) {
    auto &w = doci[ix];
    auto mbcs =
        codepages::widetombcs(codepages::codepage_utf8, w.pwsstr, w.length);
    printf("%s ", mbcs.data());
  }
  printf("\n");

  std::set<size_t> expected{2, 5, 6, 7, 12, 13, 14, 16, 18};

  ASSERT_EQ(ixs, expected);
}

TEST(Pregex, SubString) {
  {
    std::string sent = "ходить и приходить";

    PatternSet col;
    col.Add(R"((\&ходить и [а-яА-Я]+))");

    auto doci = col.BuildImage(sent);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
    std::set<size_t> expected{0, 1, 2};
    ASSERT_EQ(ixs, expected);
  }

  {
    std::string sent = "поход и походить";

    PatternSet col;
    col.Add(R"((\&поход и [а-яА-Я]+))");

    auto doci = col.BuildImage(sent);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
    std::set<size_t> expected{0, 1, 2};
    ASSERT_EQ(ixs, expected);
  }
}

TEST(Pregex, Multiple) {
  std::string sent =
      "Привет, Вася! Они сегодня все придут к тебе. Я тоже наверно приду.";

  PatternSet col;
  col.Add(R"(\&сегодня ([а-яА-Я]+) \&придти)");

  auto doci = col.BuildImage(sent);

  auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {3, 10, 15}));
  std::set<size_t> expected{6};
  ASSERT_EQ(ixs, expected);
}

TEST(Pregex, Confluence) {
  {
    std::string sent =
        "Политики высказались за скорейшее окончание конфликта в Афганистане";

    PatternSet col;
    col.Add(R"(([а-яА-Я]+) \&высказались за (([а-яА-Я]+\s*)+))");

    auto doci = col.BuildImage(sent);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
    std::set<size_t> expected{0, 3, 4, 5, 6, 7};
    ASSERT_EQ(ixs, expected);
  }

  PatternSet col;
  col.Add(R"((((иван|\&сидор|п[а-яА-Я]+)\s*){2}))");

  {
    std::string sent = "сидору продали";
    auto doci = col.BuildImage(sent);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
    std::set<size_t> expected{0, 1};
    ASSERT_EQ(ixs, expected);
  }

  {
    std::string sent = "паровоз поехал";
    auto doci = col.BuildImage(sent);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
    std::set<size_t> expected{0, 1};
    ASSERT_EQ(ixs, expected);
  }

  {
    std::string sent = "сидор закурил";
    auto doci = col.BuildImage(sent);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
    std::set<size_t> expected{};
    ASSERT_EQ(ixs, expected);
  }
}

TEST(Pregex, Filtered) {
  re2::FilteredRE2 filter;
  std::vector<std::string> atoms;
  std::vector<int> atom_indices;
  std::vector<int> matches;

  re2::RE2::Options ops;
  ops.set_case_sensitive(false);
  int id;

  std::string sent = "сегодня мы";

  std::string pat = R"((они|мы))";

  if (filter.Add(pat, ops, &id) != re2::RE2::NoError) {
    printf("got error for %s\n", pat.data());
  }

  filter.Compile(&atoms);
  // for (const auto &s : atoms) {
  //   printf("%s\n", s.data());
  // }

  ASSERT_EQ(atoms.size(), 2);

  // aho-corasik
  atom_indices.push_back(1);

  filter.AllMatches(sent, atom_indices, &matches);
  // for (const auto &ix : matches) {
  //   printf("%s\n", filter.GetRE2(ix).pattern().data());
  // }
  ASSERT_EQ(matches.size(), 1);
}

TEST(Pregex, Segflt) {
  PatternSet col;
  col.Add(R"((((иван|\&сидор|п[а-яА-Я]+)\s*){0,2}))");

  std::string sent = "сидору продали";
  auto doci = col.BuildImage(sent);

  auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
  std::set<size_t> expected{0, 1};
  ASSERT_EQ(ixs, expected);
}

TEST(Pregex, Segflt2) {
  PatternSet col;
  col.Add("(я )");

  std::string sent =
      "Коллеги, приветствую\n\n\nЯ тоже ничего не знаю про Калмыкию.\n";
  auto doci = col.BuildImage(sent);

  auto ixs = col.MatchedIndexes(col.PrepareTxt(sent, doci, {}));
  std::set<size_t> expected{3};
  ASSERT_EQ(ixs, expected);
}

TEST(Pregex, Experiment) {
  PatternSet col;
  col.Add("(тем не менее\\s*,*)");

  std::string text = "Идет дождь. Тем не менее, мы пойдем гулять";
  auto doci = col.BuildImage(text);

  auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {2, 9}));
  std::set<size_t> expected{3, 4, 5, 6};
  ASSERT_EQ(ixs, expected);
}

TEST(Pregex, Stopwords) {
  auto conf =
      mtc::config::Open(kCurrentDir + "/../../etc/csummerize.json.example",
                        {{"cohesion", mtc::zval::z_array_charstr},
                         {"spurious", mtc::zval::z_array_charstr},
                         {"pre-edit", mtc::zval::z_array_charstr},
                         {"post-edit", mtc::zval::z_array_charstr}});

  PatternSet col;
  const auto &zconf = conf.to_zmap();
  for (const auto &pat : *zconf.get_array_charstr("spurious")) {
    col.Add(pat);
  }

  for (const auto &pat : *zconf.get_array_charstr("pre-edit")) {
    col.Add(pat);
  }

  std::string text = "Здравствуйте, дорогой уважаемый наш человек,исходя  из "
                     "вышесказанного\n, получается что ...нупогоди";
  auto doci = col.BuildImage(text);

  auto start = std::chrono::high_resolution_clock::now();
  auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
  auto end = std::chrono::high_resolution_clock::now();
  print_duration(start, end);

  std::set<size_t> expected{0, 1, 2, 3, 4, 7, 8, 9};
  ASSERT_EQ(ixs, expected);

  // compile all
  for (const auto &pat : *zconf.get_array_charstr("post-edit")) {
    col.Add(pat);
  }
  for (const auto &pat : *zconf.get_array_charstr("cohesion")) {
    col.Add(pat);
  }
}

TEST(Pregex, Beginning) {
  {
    PatternSet col;
    col.Add("(шел)");

    std::string text = "шарф шелковый, нашел деньги!";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{1};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(в деньги)");

    std::string text = "вагонов деньги!";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(\\&в деньги)");

    std::string text = "вагонов деньги!";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{};
    ASSERT_EQ(ixs, expected);
  }
}

TEST(Pregex, Hello) {
  PatternSet col;
  col.Add("(привет\\word{0,4}[,.;:!?-]*)");

  {
    std::string text = "Привет наш многоуважаемый Олег Генадьевич!";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2, 3, 4, 5};
    ASSERT_EQ(ixs, expected);
  }

  {
    std::string text = "Привет наш многоуважаемый Олег Генадьевич";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2, 3, 4};
    ASSERT_EQ(ixs, expected);
  }

  {
    std::string text = "Привет,сегодня я из дома.";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2, 3, 4};
    ASSERT_EQ(ixs, expected);
  }

  col.Add("(добрый день\\word{0,4}[,.;:!?-]*)");

  {
    std::string text = "Привет! Добрый день! Сегодня я из дома..";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {1, 4, 9}));
    std::set<size_t> expected{0, 1, 2, 3, 4};
    ASSERT_EQ(ixs, expected);
  }
}

TEST(Pregex, PostEdit) {
  {
    PatternSet col;
    col.Add("(однако\\s*,*)");

    std::string text = "Однако , надо бы";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(\\&как)");

    std::string text = "как говорится, какой вопрос ...";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(\\word{0,2}\\&говорить\\word{0,4})");

    std::string text = "Как уже говорилось ранее в переписке, мы не готовы "
                       "долго это обсуждать.";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2, 3, 4, 5, 6};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(как\\word{0,1}\\&знаем,*)");

    std::string text = "Как мы знаем, это плюс.";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2, 3};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(\\word{0,2}с уважением\\word{0,10})");

    std::string text = "Искренне, с уважением, Петя";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2, 3, 4, 5};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(пожалуйста\\word{0,1})");

    std::string text = "Пожалуйста, тише!";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(\\&напомнить\\s*,*  что)");

    std::string text = "Напомним, что нужно закомитить!";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(re:)");

    std::string text = "RE: согдасование";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1};
    ASSERT_EQ(ixs, expected);
  }

  {
    auto conf =
        mtc::config::Open(kCurrentDir + "/../../etc/csummerize.json.example",
                          {{"post-edit", mtc::zval::z_array_charstr}});

    PatternSet col;
    const auto &zconf = conf.to_zmap();
    for (const auto &pat : *zconf.get_array_charstr("post-edit")) {
      col.Add(pat);
    }

    std::string text =
        "С уважением Алексей Клопотовский ООО \"НОВЫЕ ОБЛАЧНЫЕ ТЕХНОЛОГИИ\"";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1};
    ASSERT_EQ(ixs, expected);
  }
}

TEST(Pregex, LastModify) {
  {
    PatternSet col;
    col.Add("(\\word{0,3}(\\&он|\\&она|\\&оно|\\&они))");

    std::string text = "Однако, она пошла домой";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(видимо\\s*,* (да|нет))");

    std::string text = "сегодня, видимо, нет";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{2, 3, 4};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(\\&приветствую (тебя|вас)*)");

    std::string text = "Вася, приветствуем вас!";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{2, 3};
    ASSERT_EQ(ixs, expected);
  }

  {
    PatternSet col;
    col.Add("(\\&напомнить\\s*,* (что){0,1})");

    std::string text = "Напомни, что ты говорил?";
    auto doci = col.BuildImage(text);

    auto ixs = col.MatchedIndexes(col.PrepareTxt(text, doci, {}));
    std::set<size_t> expected{0, 1, 2};
    ASSERT_EQ(ixs, expected);
  }
}

TEST(Pregex, Duplicates) {
  PatternSet col;
  col.Add("(\\word{0,3}(\\&он|\\&оно))");

  std::string text = "пожалуйста, прикрепите его к ответу в виде вложения";
  auto doci = col.BuildImage(text);

  auto pTxt = col.PrepareTxt(text, doci, {});
  auto ixs = col.MatchedIndexes(pTxt);
  std::set<size_t> expected{0, 1, 2, 3};
  ASSERT_EQ(ixs, expected);
}

TEST(Pregex, GoodDay) {
  PatternSet col;
  col.Add("(добрый день\\word{0,4}[,.;:!?]*)");

  // "Добрый день Семён Семёнович."
  std::string text = "Добрый день Семён Семёнович.";
  auto doci = col.BuildImage(text);

  auto pTxt = col.PrepareTxt(text, doci, {});
  auto ixs = col.MatchedIndexes(pTxt);
  std::set<size_t> expected{0, 1, 2, 3, 4};
  ASSERT_EQ(ixs, expected);
}

//-------------------------------------------------------------------------//
#endif // __GTEST_PREGEX_H_ABA884D5_28F7_4B5F_8DCD_AA954D952D61__
