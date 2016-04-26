/*
 * System.cpp
 *
 *  Created on: 23 Oct 2015
 *      Author: hieu
 */
#include <string>
#include <iostream>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include "System.h"
#include "FF/FeatureFunction.h"
#include "TranslationModel/UnknownWordPenalty.h"
#include "legacy/Util2.h"
#include "util/exception.hh"

using namespace std;

namespace Moses2
{

System::System(const Parameter &paramsArg) :
    params(paramsArg), featureFunctions(*this)
{
  options.init(paramsArg);

  IsPb();

  bestCollector.reset(new OutputCollector());

  params.SetParameter(cpuAffinityOffset, "cpu-affinity-offset", 0);
  params.SetParameter(cpuAffinityOffsetIncr, "cpu-affinity-increment", 1);

  const PARAM_VEC *section;

  // output collectors
  if (options.nbest.nbest_size) {
    nbestCollector.reset(new OutputCollector(options.nbest.output_file_path));
  }

  featureFunctions.Create();
  LoadWeights();
  cerr << "START featureFunctions.Load()" << endl;
  featureFunctions.Load();
  cerr << "START LoadMappings()" << endl;
  LoadMappings();
  cerr << "END LoadMappings()" << endl;
}

System::~System()
{
}

void System::LoadWeights()
{
  const PARAM_VEC *vec = params.GetParam("weight");
  UTIL_THROW_IF2(vec == NULL, "Must have [weight] section");

  weights.Init(featureFunctions);
  BOOST_FOREACH(const std::string &line, *vec){
  cerr << "line=" << line << endl;
  weights.CreateFromString(featureFunctions, line);
}
}

void System::LoadMappings()
{
  const PARAM_VEC *vec = params.GetParam("mapping");
  UTIL_THROW_IF2(vec == NULL, "Must have [mapping] section");

  BOOST_FOREACH(const std::string &line, *vec){
  vector<string> toks = Tokenize(line);
  assert( (toks.size() == 2 && toks[0] == "T") || (toks.size() == 3 && toks[1] == "T") );

  size_t ptInd;
  if (toks.size() == 2) {
    ptInd = Scan<size_t>(toks[1]);
  }
  else {
    ptInd = Scan<size_t>(toks[2]);
  }
  const PhraseTable *pt = featureFunctions.GetPhraseTablesExcludeUnknownWordPenalty(ptInd);
  mappings.push_back(pt);
}

// unk pt
  const UnknownWordPenalty *unkWP =
      dynamic_cast<const UnknownWordPenalty*>(featureFunctions.FindFeatureFunction(
          "UnknownWordPenalty0"));
  if (unkWP) {
    mappings.push_back(unkWP);
  }
}

MemPool &System::GetSystemPool() const
{
  MemPool &ret = GetThreadSpecificObj(m_systemPool);
  return ret;
}

MemPool &System::GetManagerPool() const
{
  MemPool &ret = GetThreadSpecificObj(m_managerPool);
  return ret;
}

FactorCollection &System::GetVocab() const
{
  return m_vocab;
}

Recycler<HypothesisBase*> &System::GetHypoRecycler() const
{
  return GetThreadSpecificObj(m_hypoRecycler);
}

void System::IsPb()
{
  switch (options.search.algo) {
  case Normal:
  case CubePruning:
  case CubePruningPerMiniStack:
  case CubePruningPerBitmap:
  case CubePruningCardinalStack:
  case CubePruningBitmapStack:
  case CubePruningMiniStack:
    isPb = true;
    break;
  case CYKPlus:
    isPb = false;
    break;
  default:
    abort();
    break;
  }
}

}

