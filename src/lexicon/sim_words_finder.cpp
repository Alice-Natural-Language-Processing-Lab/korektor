// This file is part of korektor <http://github.com/ufal/korektor/>.
//
// Copyright 2015 by Institute of Formal and Applied Linguistics, Faculty
// of Mathematics and Physics, Charles University in Prague, Czech Republic.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under 3-clause BSD licence.

#include "error_model/error_model.h"
#include "lexicon.h"
#include "sim_words_finder.h"
#include "spellchecker/configuration.h"
#include "token/token.h"
#include "utils/utf.h"

namespace ufal {
namespace korektor {

// Capitalization classification
enum capitalization_type { ALL_UPPER_CASE, ALL_LOWER_CASE, FIRST_UPPER_CASE, WEIRD };

capitalization_type GetCapitalizationType(const u16string& ustr)
{
  if (UTF::IsUpper(ustr[0]))
  {
    for (unsigned i = 1; i < ustr.length(); i++)
    {
      if (UTF::IsUpper(ustr[i]) == false)
      {
        if (i > 1)
          return WEIRD;
        else
        {
          for (unsigned j = 2; j < ustr.length(); j++)
          {
            if (UTF::IsUpper(ustr[j]))
              return WEIRD;
          }
        }
        return FIRST_UPPER_CASE;
      }
    }
    return ALL_UPPER_CASE;
  }
  else
  {
    for (unsigned i = 1; i < ustr.length(); i++)
    {
      if (UTF::IsUpper(ustr[i]))
        return WEIRD;
    }
    return ALL_LOWER_CASE;
  }
}


// SimWordsFinder methods
void SimWordsFinder::Find_basic(const TokenP &token, uint32_t lookup_max_ed_dist, double lookup_max_cost, Similar_Words_Map &ret)
{
  u16string& word_u_str = token->str;

  if (token->correction_is_allowed == false)
  {
    ret[token->ID] = make_pair(word_u_str, 0.0);
  }
  else
  {

    ret = configuration->lexicon->GetSimilarWords(word_u_str, lookup_max_ed_dist, lookup_max_cost, configuration->errorModel, false);

    if (token->sentence_start && UTF::IsUpper(word_u_str[0])) //On the beginning of the sentence both lower case and upper case starting letter should be tried!
    {
      // Make sure all suggestions are capitalized.
      for (auto&& suggestion : ret) {
        auto& word = suggestion.second.first;
        if (!word.empty() && UTF::IsLower(word[0]))
          word[0] = UTF::ToUpper(word[0]);
      }

      // Try searching for word without first capital letter.
      u16string word_str_lc = word_u_str;
      word_str_lc[0] = UTF::ToLower(word_u_str[0]);

      Similar_Words_Map msw_lc = configuration->lexicon->GetSimilarWords(word_str_lc, lookup_max_ed_dist, lookup_max_cost, configuration->errorModel, false);

      for (Similar_Words_Map::iterator it = msw_lc.begin(); it != msw_lc.end(); it++)
      {
        //word form of found suggestion should be capitalized!
        auto& ustring = it->second.first;
        if (UTF::IsLower(ustring[0]))
          ustring[0] = UTF::ToUpper(ustring[0]);

        Similar_Words_Map::iterator fit = ret.find(it->first);
        if (fit == ret.end())
        {
          ret[it->first] = it->second;
        }
        else
        {
          if (it->second.second < ret[it->first].second)
          {
            ret[it->first].second = it->second.second;
            ret[it->first].first = it->second.first;
          }
        }
      }
    }

  }

}

void SimWordsFinder::Find_basic_ignore_case(const TokenP &token, bool keep_orig_casing, uint32_t lookup_max_ed_dist, double lookup_max_cost, Similar_Words_Map &ret)
{
  u16string& word_u_str = token->str;
  ret = configuration->lexicon->GetSimilarWords(word_u_str, lookup_max_ed_dist, lookup_max_cost, configuration->errorModel, true);

 capitalization_type ct = GetCapitalizationType(word_u_str);

  if (keep_orig_casing == true)
  {
    for (Similar_Words_Map::iterator it = ret.begin(); it != ret.end(); it++)
    {
      auto& sim_w_str = it->second.first;

      for (uint32_t j = 0; j < sim_w_str.length(); j++)
      {
        switch (ct)
        {
          case ALL_UPPER_CASE: sim_w_str[j] = UTF::ToUpper(sim_w_str[j]); break;
          case ALL_LOWER_CASE: sim_w_str[j] = UTF::ToLower(sim_w_str[j]); break;
          case FIRST_UPPER_CASE: if ( j == 0)
                                   sim_w_str[j] = UTF::ToUpper(sim_w_str[j]);
                                 else
                                   sim_w_str[j] = UTF::ToLower(sim_w_str[j]);
                                 break;
          case WEIRD: break;
        }
      }

    }

  }
  else if (token->sentence_start && UTF::IsUpper(word_u_str[0]))
  {
    for (Similar_Words_Map::iterator it = ret.begin(); it != ret.end(); it++)
    {
      auto& sim_w_str = it->second.first;

      if (UTF::IsLower(sim_w_str[0]))
      {
        sim_w_str[0] = UTF::ToUpper(sim_w_str[0]);
      }

    }

  }
}

Similar_Words_Map SimWordsFinder::Find(const TokenP &token)
{
  Similar_Words_Map swm;

  for (auto&& sc : search_configs) {
    // Skip non-matching search configs.
    if (sc.min_length && token->length < sc.min_length) continue;
    if (sc.max_length && token->length > sc.max_length) continue;

    // Search according to given sc
    if (sc.casing == case_sensitive)
    {
      Find_basic(token, sc.max_ed_dist, sc.max_cost, swm);
    }
    else if (sc.casing == ignore_case)
    {
      Find_basic_ignore_case(token, false, sc.max_ed_dist, sc.max_cost, swm);
    }
    else
    {
      Find_basic_ignore_case(token, true, sc.max_ed_dist, sc.max_cost, swm);
    }

    if (!swm.empty()) break;
  }

  return swm;
}

} // namespace korektor
} // namespace ufal
