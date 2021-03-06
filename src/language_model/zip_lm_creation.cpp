// This file is part of korektor <http://github.com/ufal/korektor/>.
//
// Copyright 2015 by Institute of Formal and Applied Linguistics, Faculty
// of Mathematics and Physics, Charles University in Prague, Czech Republic.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under 3-clause BSD licence.

#include <cmath>
#include <fstream>
#include <set>

#include "language_model/ngram.h"
#include "morphology/morphology.h"
#include "persistent_structures/comp_increasing_array.h"
#include "persistent_structures/mapped_double_array.h"
#include "persistent_structures/packed_array.h"
#include "utils/io.h"
#include "utils/parse.h"
#include "zip_lm.h"

namespace ufal {
namespace korektor {

ZipLM::ZipLM(const string &_factor_name, uint32_t _order, double _not_in_lm_cost, vector<vector<double> > &_probs, vector<vector<double> > &_bows,
             vector<vector<uint32_t> > &_ids, vector<vector<uint32_t> > &_offsets)
{
  factor_name = _factor_name;
  lm_order = _order;
  not_in_lm_cost = _not_in_lm_cost;

  max_unigram_id = _probs[0].size() - 1;
  cerr << "LM order: " << _order << endl;

  ids.push_back(PackedArrayP());

  for (uint32_t i = 0; i < _order; i++)
  {
    uint32_t bits_per_prob;

    if (i == 0)
      bits_per_prob = bits_per_unigram_prob;
    else if (i == 1)
      bits_per_prob = bits_per_bigram_prob;
    else
      bits_per_prob = bits_per_higher_order_prob;

    cerr << "creating probs order " << i << endl;

    probs.push_back(MappedDoubleArrayP(new MappedDoubleArray(_probs[i], bits_per_prob)));
  }

  for (uint32_t i = 0; i < _order - 1; i++)
  {
    uint32_t bits_per_bow;

    if (i == 0)
      bits_per_bow = bits_per_unigram_bow;
    else if (i == 1)
      bits_per_bow = bits_per_bigram_bow;
    else
      bits_per_bow = bits_per_higher_order_bow;

    cerr << "creating bows order " << i << endl;
    bows.push_back(MappedDoubleArrayP(new MappedDoubleArray(_bows[i], bits_per_bow)));
  }

  for (uint32_t i = 1; i < _order; i++)
  {
    cerr << "creating ids order " << i << endl;
    ids.push_back(PackedArrayP(new PackedArray(_ids[i])));
  }

  for (uint32_t i = 0; i < _order - 1; i++)
  {

    offsets.push_back(CompIncreasingArrayP(new CompIncreasingArray(_offsets[i], ids[i + 1]->GetSize() - 1)));
  }
}

ZipLM::ZipLM(string bin_file)
{
  filename = bin_file;
  ifstream ifs;
  ifs.open(bin_file.c_str(), ios::in | ios::binary);

//  if (! ifs.is_open())
//  {
//    cerr << "Opening: " << bin_file << endl;
//  }
  assert(ifs.is_open());

  factor_name = IO::ReadString(ifs);

  ifs.read((char*)&lm_order, sizeof(uint32_t));
  ifs.read((char*)&not_in_lm_cost, sizeof(double));

  ids.push_back(PackedArrayP());

  for (uint32_t i = 0; i < lm_order; i++)
  {
    probs.push_back(MappedDoubleArrayP(new MappedDoubleArray(ifs)));
  }

  max_unigram_id = probs[0]->GetSize() - 1;

  for (uint32_t i = 0; i < lm_order - 1; i++)
  {
    bows.push_back(MappedDoubleArrayP(new MappedDoubleArray(ifs)));
  }

  for (uint32_t i = 1; i < lm_order; i++)
  {
    ids.push_back(PackedArrayP(new PackedArray(ifs)));
  }

  for (uint32_t i = 0; i < lm_order - 1; i++)
  {
    offsets.push_back(CompIncreasingArrayP(new CompIncreasingArray(ifs)));
  }

  ifs.close();

}


void ZipLM::SaveInBinaryForm(string out_file)
{
  cerr << "Saving in binary form..." << endl;
  ofstream ofs;
  ofs.open(out_file.c_str(), ios::out | ios::binary);
  assert(ofs.is_open());
  IO::WriteString(ofs, factor_name);
  ofs.write((char*)&lm_order, sizeof(uint32_t));
  ofs.write((char*)&not_in_lm_cost, sizeof(double));

  for (uint32_t i = 0; i < lm_order; i++)
  {
    probs[i]->WriteToStream(ofs);
  }

  for (uint32_t i = 0; i < lm_order - 1; i++)
  {
    bows[i]->WriteToStream(ofs);
  }

  for (uint32_t i = 1; i < lm_order; i++)
  {
    ids[i]->WriteToStream(ofs);
  }

  for (uint32_t i = 0; i < lm_order - 1; i++)
  {
    offsets[i]->WriteToStream(ofs);
  }


  ofs.close();
}

/// @brief Creates the language model instance from the ARPA format language model stored in 'text_file'
///
/// @param text_file N-gram text file in ARPA format
/// @param morphology Morphology
/// @param _factor_name Factor name
/// @param lm_order LM order
/// @param not_in_lm_cost @todo variable for ?
/// @return Language model object of type @ref ZipLM
ZipLMP ZipLM::createFromTextFile(string text_file, MorphologyP &morphology, string _factor_name, unsigned lm_order, double not_in_lm_cost)
{
  cerr << "Creating from text file..." << endl;
  ifstream ifs;
  ifs.open(text_file.c_str());
  assert(ifs.is_open());

  string s;
  uint32_t max_id = 0;

  int last_unigram_id = -1;
  int unigram_id;
  unsigned factorIndex = morphology->GetFactorMap()[_factor_name];

  cerr << "factorIndex = " << factorIndex << endl;

  vector<vector<double> > probs;
  vector<vector<double> > bows;
  vector<vector<uint32_t> > ids;
  vector<vector<uint32_t> > offsets;

  ids.push_back(vector<uint32_t>());

  for (uint32_t i = 0; i < lm_order; i++)
  {
    probs.push_back(vector<double>());

    //if (i > 0)
    ids.push_back(vector<uint32_t>());

    if (i < lm_order - 1)
    {
      offsets.push_back(vector<uint32_t>());
      bows.push_back(vector<double>());
    }
  }

  set<NGram, NGram_compare> ngrams;

  unsigned ngram_order = 0;
  vector<string> toks;

  while (IO::ReadLine(ifs, s))
  {
    if (s.empty()) continue;
    else if (s.find("ngram") == 0) continue;
    else if (s.find("-grams:") != string::npos)
    {
      ngram_order = s[1] - '0';
      cerr << "reading " << ngram_order << "grams...\n";
    }
    else if (s[0] == '\\') continue;
    else
    {
      assert(ngram_order > 0);
      IO::Split(s, " \t", toks);
      if (toks.size() != ngram_order + 1 && toks.size() != ngram_order + 2)
        runtime_failure("Corrupted line '" << s << "' in file '" << text_file << "'!");

      double bow = 0;
      double prob = -Parse::Double(toks[0], "ngram probability");

      if (prob > 90) prob = not_in_lm_cost;

      uint32_t *ids = new uint32_t[ngram_order];

      bool all_known = true;
      for (unsigned i = 0; i < ngram_order; i++)
      {
        int _id = morphology->GetFactorID(factorIndex, toks[i + 1]);
        if (_id == -1)
          all_known = false;
        else
        {
          //!!! Order of storing IDs is changed to the original way (like in the master thesis submission) - i.e. reversed order (next word, history new, history old...)
          //It allows more convenient look-up
          ids[ngram_order - i - 1] = (unsigned)_id;
        }

        if (ngram_order == 1 && all_known == true && _id > (int)max_id)
          max_id = _id;
      }

      if (all_known == false)
      {
        delete[] ids;
        continue;
      }

      if (toks.size() == ngram_order + 2)
        bow = - Parse::Double(toks.back(), "ngram bow");

      if (bow > 90) bow = not_in_lm_cost;

      ngrams.insert(NGram(ngram_order, ids, prob, bow));
    }
  }

  set<NGram, NGram_compare> added_ngrams;

  cerr << "adding aux nodex..." << endl;

  unsigned num_added_ngrams = 0;

  for (auto it = ngrams.begin(); it!= ngrams.end(); it++)
  {
    NGram aux = *it;
    if (aux.order > 1)
    {
      aux.order--;
      while (aux.order > 0 && ngrams.find(aux) == ngrams.end() && added_ngrams.find(aux) == added_ngrams.end())
      {
        NGram aux2 = aux;
        aux2.order--;

        while (ngrams.find(aux2) == ngrams.end())
        {
          assert(aux2.order > 0);
          aux2.order--;
        }

        aux.prob = ngrams.find(aux2)->prob;
        aux.backoff = 0;

        added_ngrams.insert(aux);
        aux.order--;
        num_added_ngrams++;
      }
    }
  }

  cerr << "!!!!!!!!! num_added_ngrams = " << num_added_ngrams << endl;

  for (auto it = added_ngrams.begin(); it != added_ngrams.end(); it++)
  {
    ngrams.insert(*it);
  }

  cerr << "inserting missing unigrams... (maxID = " << max_id << ")" << endl;

  NGram aux_unigram = NGram(1);
  aux_unigram.prob = not_in_lm_cost;
  aux_unigram.backoff = 0.0;

  for (unsigned i = 0; i < max_id; i++)
  {
    aux_unigram.word_ids[0] = i;

    if (ngrams.find(aux_unigram) == ngrams.end())
      ngrams.insert(aux_unigram);
  }

  cerr << "creating ZipTBO structures..." << endl;

  uint32_t counter = 0;
  for (auto it = ngrams.begin(); it != ngrams.end(); it++)
  {
    counter++;
    if (counter % 10000 == 0) cerr << counter << endl;
    NGram ngram = *it;
    assert(ngram.order > 0);
    unsigned array_index = ngram.order - 1;
    if (ngram.order < lm_order)
    {
      offsets[array_index].push_back(ids[array_index + 1].size());
      bows[array_index].push_back(ngram.backoff);
    }

    probs[array_index].push_back(ngram.prob);

    uint32_t new_id = ngram.word_ids[ngram.order - 1];

    if (ngram.order > 1)
    {
      ids[array_index].push_back(new_id);
    }
    else
    {
      unigram_id = new_id;
      assert(unigram_id == last_unigram_id + 1);
      last_unigram_id = unigram_id;
    }

  }

  cerr << "binarization..." << endl;

  ZipLMP ret_lm = ZipLMP(new ZipLM(_factor_name, lm_order, not_in_lm_cost, probs, bows, ids, offsets));


  cerr << "testing..." << endl;
  unsigned test_counter = 0;
  for (auto it = ngrams.begin(); it != ngrams.end(); it++)
  {
    if (test_counter % 50000 == 0) cerr << "testing: " << test_counter << endl;
    test_counter++;

    //if (test_counter > 3800000) cerr << "finish: " << test_counter << endl;

    NGram key = *it;
    NGram val = NGram(key.order);
    ret_lm->GetNGramForNGramKey(key, val);

    assert(val.order == key.order);
    assert(fabs(val.prob - key.prob) < 0.5);
    assert(fabs(val.backoff - key.backoff) < 0.5);
  }

  cerr << "OK!" << endl;
  return ret_lm;
}

} // namespace korektor
} // namespace ufal
