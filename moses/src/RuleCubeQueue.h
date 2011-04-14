// $Id$
// vim:tabstop=2
/***********************************************************************
 Moses - factored phrase-based language decoder
 Copyright (C) 2010 Hieu Hoang

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***********************************************************************/
#include <queue>
#include <vector>
#include <set>
#include "RuleCube.h"

#ifdef HAVE_BOOST
#include <boost/functional/hash.hpp>
#include <boost/unordered_set.hpp>
#include <boost/version.hpp>
#endif

namespace Moses
{

#ifdef HAVE_BOOST
class RuleCubeUniqueHasher
{
public:
  size_t operator()(const RuleCube * p) const {
    size_t seed = 0;
    boost::hash_combine(seed, &(p->GetTranslationOption()));
    boost::hash_combine(seed, p->GetCube());
    return seed;
  }
};

class RuleCubeUniqueEqualityPred
{
public:
  bool operator()(const RuleCube * p, const RuleCube * q) const {
    return ((&(p->GetTranslationOption()) == &(q->GetTranslationOption()))
            && (p->GetCube() == q->GetCube()));
  }
};
#endif

class RuleCubeUniqueOrderer
{
public:
  bool operator()(const RuleCube* entryA, const RuleCube* entryB) const {
    return (*entryA) < (*entryB);
  }
};

class RuleCubeScoreOrderer
{
public:
  bool operator()(const RuleCube* entryA, const RuleCube* entryB) const {
    return (entryA->GetCombinedScore() < entryB->GetCombinedScore());
  }
};


class RuleCubeQueue
{
protected:
#if defined(BOOST_VERSION) && (BOOST_VERSION >= 104200)
  typedef boost::unordered_set<RuleCube*,
          RuleCubeUniqueHasher,
          RuleCubeUniqueEqualityPred> UniqueCubeEntry;
#else
  typedef std::set<RuleCube*, RuleCubeUniqueOrderer> UniqueCubeEntry;
#endif
  UniqueCubeEntry m_uniqueEntry;

  typedef std::priority_queue<RuleCube*, std::vector<RuleCube*>, RuleCubeScoreOrderer> SortedByScore;
  SortedByScore m_sortedByScore;


public:
  ~RuleCubeQueue();
  bool IsEmpty() const {
    return m_sortedByScore.empty();
  }

  RuleCube *Pop();
  bool Add(RuleCube *ruleCube);
};


};

