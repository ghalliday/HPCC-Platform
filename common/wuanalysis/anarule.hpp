/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2019 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#ifndef ANARULE_HPP
#define ANARULE_HPP

#include "anacommon.hpp"
#include "jstatcodes.h"
#include "wuattr.hpp"

struct WuAnalyseOptions
{
    unsigned __int64 minInterestingTime = 1000000; // ignore anything under a millisecond
    unsigned __int64 minCost = 1000000000;         // only interested in costs of > 1s
    unsigned __int64 skewThreshold = statSkewPercent(20); // minimum interesting skew measurment
};


interface IWuScope
{
    virtual stat_t getStatRaw(StatisticKind kind, StatisticKind variant = StKindNone) = 0;
    virtual unsigned getAttr(WuAttr kind) = 0;
    virtual void getAttr(StringBuffer & result, WuAttr kind) = 0;
};

interface IWuEdge;
interface IWuActivity;
interface IWuEdge : public IWuScope
{
    virtual IWuActivity & querySource() = 0;
};

interface IWuActivity : public IWuScope
{
    virtual IWuEdge * queryInput(unsigned idx) = 0;
    virtual IWuEdge * queryOutput(unsigned idx) = 0;

    inline IWuActivity * queryInputActivity(unsigned idx)
    {
        IWuEdge * edge = queryInput(idx);
        return edge ? &edge->querySource() : nullptr;
    }
};




int compareIssues(CInterface * const * _l, CInterface * const * _r);
class PerformanceIssue : public CInterface
{
public:
    void clear();
    int compare(const PerformanceIssue & other) const;  //Sort in descending order of cost, then ascending order of scope name
    void print();
    void set(__uint64 _cost, const char * msg, ...);

public:
    StringAttr scope;
    __uint64 cost = 0;      // number of nanoseconds lost as a result.
    StringBuffer comment;
};

class AActivityRule : public CInterface
{
public:
    virtual bool isCandidate(IWuActivity & attributes) const;
    virtual bool check(PerformanceIssue & results, IWuActivity & activity, const WuAnalyseOptions & options) = 0;
};

void gatherRules(CIArrayOf<AActivityRule> & rules, bool includeAll);

#endif
