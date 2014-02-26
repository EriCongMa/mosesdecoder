/*
 * Rules.cpp
 *
 *  Created on: 20 Feb 2014
 *      Author: hieu
 */

#include <sstream>
#include "Rules.h"
#include "ConsistentPhrase.h"
#include "ConsistentPhrases.h"
#include "AlignedSentence.h"
#include "Rule.h"
#include "Parameter.h"
#include "moses/Util.h"

using namespace std;

extern bool g_debug;

Rules::Rules(const AlignedSentence &alignedSentence)
:m_alignedSentence(alignedSentence)
{
}

Rules::~Rules() {
	Moses::RemoveAllInColl(m_keepRules);
}

void Rules::CreateRules(const ConsistentPhrase &cp,
		const Parameter &params)
{
	const ConsistentPhrase::NonTerms &nonTerms = cp.GetNonTerms();
	for (size_t i = 0; i < nonTerms.size(); ++i) {
		const NonTerm &nonTerm = nonTerms[i];
		Rule *rule = new Rule(nonTerm, m_alignedSentence);

		rule->Prevalidate(params);
		rule->CreateTarget(params);


		if (rule->CanRecurse()) {
			Extend(*rule, params);
		}

		if (rule->IsValid()) {
			m_keepRules.insert(rule);
		}
		else {
			delete rule;
		}
	}
}

void Rules::Extend(const Parameter &params)
{
	const ConsistentPhrases &allCPS = m_alignedSentence.GetConsistentPhrases();

	size_t size = m_alignedSentence.GetPhrase(Moses::Input).size();
	for (size_t sourceStart = 0; sourceStart < size; ++sourceStart) {
		for (size_t sourceEnd = sourceStart; sourceEnd < size; ++sourceEnd) {
			const ConsistentPhrases::Coll &cps = allCPS.GetColl(sourceStart, sourceEnd);

			ConsistentPhrases::Coll::const_iterator iter;
			for (iter = cps.begin(); iter != cps.end(); ++iter) {
				const ConsistentPhrase &cp = *iter;
				CreateRules(cp, params);
			}
		}
	}
}

void Rules::Extend(const Rule &rule, const Parameter &params)
{
	const ConsistentPhrases &allCPS = m_alignedSentence.GetConsistentPhrases();
	int sourceMin = rule.GetNextSourcePosForNonTerm();

	int ruleStart = rule.GetConsistentPhrase().corners[0];
	int ruleEnd = rule.GetConsistentPhrase().corners[1];

	for (int sourceStart = sourceMin; sourceStart <= ruleEnd; ++sourceStart) {
		for (int sourceEnd = sourceStart; sourceEnd <= ruleEnd; ++sourceEnd) {
			if (sourceStart == ruleStart && sourceEnd == ruleEnd) {
				// don't cover whole rule with 1 non-term
				continue;
			}

			const ConsistentPhrases::Coll &cps = allCPS.GetColl(sourceStart, sourceEnd);
			Extend(rule, cps, params);
		}
	}
}

void Rules::Extend(const Rule &rule, const ConsistentPhrases::Coll &cps, const Parameter &params)
{
	ConsistentPhrases::Coll::const_iterator iter;
	for (iter = cps.begin(); iter != cps.end(); ++iter) {
		const ConsistentPhrase &cp = *iter;
		Extend(rule, cp, params);
	}
}

void Rules::Extend(const Rule &rule, const ConsistentPhrase &cp, const Parameter &params)
{
	const ConsistentPhrase::NonTerms &nonTerms = cp.GetNonTerms();
	for (size_t i = 0; i < nonTerms.size(); ++i) {
		const NonTerm &nonTerm = nonTerms[i];

		Rule *newRule = new Rule(rule, nonTerm);
		newRule->Prevalidate(params);
		newRule->CreateTarget(params);

		if (newRule->CanRecurse()) {
			// recursively extend
			Extend(*newRule, params);
		}

		if (newRule->IsValid()) {
			m_keepRules.insert(newRule);
		}
		else {
			delete newRule;
		}
	}
}

std::string Rules::Debug() const
{
	stringstream out;

	std::set<Rule*>::const_iterator iter;
	out << "m_keepRules:" << endl;
	for (iter = m_keepRules.begin(); iter != m_keepRules.end(); ++iter) {
		const Rule &rule = **iter;
		out << rule.Debug() << endl;
	}

	return out.str();
}

void Rules::Output(std::ostream &out) const
{
	std::set<Rule*, CompareRules>::const_iterator iter;
	for (iter = m_mergeRules.begin(); iter != m_mergeRules.end(); ++iter) {
		const Rule &rule = **iter;
		rule.Output(out);
		out << endl;
	}
}

void Rules::Consolidate(const Parameter &params)
{
	if (params.fractionalCounting) {

	}
	else {
		std::set<Rule*>::iterator iter;
		for (iter = m_keepRules.begin(); iter != m_keepRules.end(); ++iter) {
			Rule &rule = **iter;
			rule.SetCount(1);
		}
	}

	MergeRules(params);
}

void Rules::MergeRules(const Parameter &params)
{
	typedef std::set<Rule*, CompareRules> MergeRules;

	std::set<Rule*>::const_iterator iterOrig;
	for (iterOrig = m_keepRules.begin(); iterOrig != m_keepRules.end(); ++iterOrig) {
		Rule *origRule = *iterOrig;

		const ConsistentPhrase &cp = origRule->GetConsistentPhrase();
		if (cp.corners[0] == 0 && (cp.corners[1] == 5 || cp.corners[1] == 6)) {
			cerr << origRule->Debug() << endl;
			g_debug = true;
		}
		else {
			g_debug = false;
		}

		pair<MergeRules::iterator, bool> inserted = m_mergeRules.insert(origRule);
		if (!inserted.second) {
			// already there, just add count
			Rule &rule = **inserted.first;
			float newCount = rule.GetCount() + origRule->GetCount();
			rule.SetCount(newCount);
		}

	}

}

