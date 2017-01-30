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

#include "jliball.hpp"

#include "workunit.hpp"
#include "anarule.hpp"
#include "eclhelper.hpp"
#include "commonext.hpp"

class ActivityKindRule : public AActivityRule
{
public:
    ActivityKindRule(ThorActivityKind _kind) : kind(_kind) {}

    virtual bool isCandidate(IWuActivity & activity) const override;

protected:
    ThorActivityKind kind;
};
//--------------------------------------------------------------------------------------------------------------------

int compareIssues(CInterface * const * _l, CInterface * const * _r)
{
    const PerformanceIssue * l = static_cast<const PerformanceIssue *>(*_l);
    const PerformanceIssue * r = static_cast<const PerformanceIssue *>(*_r);
    return l->compare(*r);
}


void PerformanceIssue::clear()
{
    scope.clear();
    cost = 0;
    comment.clear();
}

int PerformanceIssue::compare(const PerformanceIssue & other) const
{
    //Sort in descending order of cost, then ascending order of scope name
    if (cost != other.cost)
        return cost > other.cost ? -1 : +1;
    return strcmp(scope, other.scope);
}


void PerformanceIssue::print()
{
    printf("[%" I64F "ums] %s: %s\n", cost/1000000, scope.str(), comment.str());
}

void PerformanceIssue::set(__uint64 _cost, const char * msg, ...)
{
    cost = _cost;
    va_list args;
    va_start(args, msg);
    comment.valist_appendf(msg, args);
    va_end(args);
}

//--------------------------------------------------------------------------------------------------------------------

bool AActivityRule::isCandidate(IWuActivity & activity) const
{
    return true;
}

//--------------------------------------------------------------------------------------------------------------------

bool ActivityKindRule::isCandidate(IWuActivity & activity) const
{
    return (activity.getAttr(WaKind) == kind);
}

//--------------------------------------------------------------------------------------------------------------------

class IoSkewRule : public AActivityRule
{
public:
    IoSkewRule(StatisticKind _stat, const char * _category) : stat(_stat), category(_category) {}

    virtual bool check(PerformanceIssue & results, IWuActivity & activity, const WuAnalyseOptions & options) override
    {
        stat_t ioAvg = activity.getStatRaw(stat, StAvgX);
        stat_t ioMaxSkew = activity.getStatRaw(stat, StSkewMax);
        if ((ioMaxSkew > options.skewThreshold) && (ioAvg > options.minInterestingTime))
        {
            stat_t ioMax = activity.getStatRaw(stat, StMaxX);
            __uint64 cost;
            //If one node didn't spill then it is possible the skew caused all the lost time
            if (activity.getStatRaw(stat, StSkewMin) == 0)
                cost = ioMax;
            else
                cost = (ioMax - ioAvg);

            IWuEdge * edge = activity.queryInput(0);
            if (!edge)
                edge = activity.queryOutput(0);
            if (edge)
            {
                stat_t rowsMaxSkew = edge->getStatRaw(StNumRowsProcessed, StSkewMax);
                if (rowsMaxSkew < ioMaxSkew)
                    results.set(cost, "Significant skew in child records causes uneven %s time", category);
                else
                    results.set(cost, "Significant skew in records causes uneven %s time", category);
            }
            else
                results.set(cost, "Significant skew in records causes uneven %s time", category);
            return true;
        }
        return false;
    }

protected:
    StatisticKind stat;
    const char * category;
};


class ProcessingSkewRule : public AActivityRule
{
public:
    ProcessingSkewRule(StatisticKind _stat, const char * _category) : stat(_stat), category(_category) {}

    virtual bool isCandidate(IWuActivity & activity) const override
    {
        switch (activity.getAttr(WaKind))
        {
        case TAKproject:
        case TAKparse:
        case TAKsoap_datasetdataset:
            return true;
        }
        return false;
    }

    virtual bool check(PerformanceIssue & results, IWuActivity & activity, const WuAnalyseOptions & options) override
    {
        ThorActivityKind kind = (ThorActivityKind)activity.getAttr(WaKind);
        const char * activityText = activityKindStr(kind);

        stat_t timeAvg = activity.getStatRaw(stat, StAvgX);
        stat_t timeMaxSkew = activity.getStatRaw(stat, StSkewMax);
        if ((timeMaxSkew > options.skewThreshold) && (timeAvg > options.minInterestingTime))
        {
            stat_t timeMax = activity.getStatRaw(stat, StMaxX);
            __uint64 cost = (timeMax - timeAvg);

            IWuEdge * leftEdge = activity.queryInput(0);
            assertex(leftEdge);
            stat_t rowsMaxSkew = leftEdge->getStatRaw(StNumRowsProcessed, StSkewMax);
            //If skew in number of inputs rows is at least 75% of the skew in timings, then that is the likely cause
            if (rowsMaxSkew < options.skewThreshold * 3 / 4)
            {
                //HOW?
                //Look at child queries - if significant skew in child query processing that that is likely to be the cause
                //To know the skew in the total number of records processed the information needs to be calculated in thor
                //and stored in the results, rather than derived.  (Unless all results were saved!)
                results.set(cost, "Skew in %s processing time", activityText);
            }
            else
                results.set(cost, "Skew in record counts causes skew in %s processing time", activityText);
            return true;
        }
        return false;
    }

protected:
    StatisticKind stat;
    const char * category;
};

class DistributeSkewRule : public ActivityKindRule
{
public:
    DistributeSkewRule() : ActivityKindRule(TAKhashdistribute) {}

    virtual bool check(PerformanceIssue & results, IWuActivity & activity, const WuAnalyseOptions & options) override
    {
        IWuEdge * outputEdge = activity.queryOutput(0);
        if (!outputEdge)
            return false;

        stat_t rowsMaxSkew = outputEdge->getStatRaw(StNumRowsProcessed, StSkewMax); // MORE
        stat_t rowsAvg = outputEdge->getStatRaw(StNumRowsProcessed, StAvgX);
        if ((rowsMaxSkew > options.skewThreshold) && (rowsAvg > rowsThreshold))
        {
            IWuEdge * inputEdge = activity.queryInput(0);

            //MORE: How can we provide an estimate for the cost of the skewed output?
            //This is more a marker of a cause than an actual cost - should possibly be indicated in a different way
            __uint64 cost = I64C(999999999999999);
            if (inputEdge && (inputEdge->getStatRaw(StNumRowsProcessed, StSkewMax) < rowsMaxSkew))
                results.set(cost, "DISTRIBUTE output is significantly skewed and worse than input");
            else
                results.set(cost, "Significant skew in DISTRIBUTE output");
            return true;
        }
        return false;
    }

protected:
    static const stat_t rowsThreshold = 100;                // avg rows per node.
};

class ParallelDiskReadRule : public ActivityKindRule
{
public:
    ParallelDiskReadRule() : ActivityKindRule(TAKdiskread) {}

    virtual bool check(PerformanceIssue & results, IWuActivity & activity, const WuAnalyseOptions & options) override
    {
        stat_t timeAvg = activity.getStatRaw(StTimeLocalExecute, StAvgX);
        stat_t ioAvg = activity.getStatRaw(StTimeDiskReadIO, StAvgX);
        if ((timeAvg > ioAvg * timeIoRatio) && (timeAvg > timeThreshold))
        {
            unsigned numStrands = 4;
            __uint64 cost = (ioAvg * timeIoRatio - timeAvg) * (numStrands - 1) / numStrands;

            results.set(cost, "Executing DISKREAD in parallel could improve speed");
            return true;
        }
        return false;
    }

protected:
    static const stat_t timeIoRatio = 2;            // slowest is 20% slower than average
    static const stat_t timeThreshold = statSec(10);
};

class ParallelInlineRule : public ActivityKindRule
{
public:
    ParallelInlineRule() : ActivityKindRule(TAKinlinetable) {}

    virtual bool check(PerformanceIssue & results, IWuActivity & activity, const WuAnalyseOptions & options) override
    {
        stat_t rowsAvg = activity.getStatRaw(StNumRowsProcessed, StAvgX);
        stat_t timeAvg = activity.getStatRaw(StTimeLocalExecute, StAvgX);
        if ((rowsAvg > rowsThreshold) && (timeAvg > timeThreshold))
        {
            stat_t timeMinSkew = activity.getStatRaw(StTimeLocalExecute, StSkewMin);
            if (timeMinSkew == 0)
            {
                stat_t timeMax = activity.getStatRaw(StTimeLocalExecute, StMinX);
                __uint64 cost = (timeMax - timeAvg);

                results.set(cost, "Suggest executing INLINE table in parallel");
                return true;
            }
        }
        return false;
    }

protected:
    static const stat_t rowsThreshold = 5;       // 5 rows per node
    static const stat_t timeThreshold = statSec(10);  // 10s
};

class JoinRule : public AActivityRule
{
public:
    virtual bool isCandidate(IWuActivity & activity) const override
    {
        ThorActivityKind kind = (ThorActivityKind)activity.getAttr(WaKind);
        return isSimpleJoin(kind) || isDenormalizeJoin(kind) || isDenormalizeGroupJoin(kind);
    }
    virtual bool check(PerformanceIssue & results, IWuActivity & activity, const WuAnalyseOptions & options) override
    {
        stat_t timeMax = activity.getStatRaw(StTimeLocalExecute, StMaxX);
        stat_t timeMaxSkew = activity.getStatRaw(StTimeLocalExecute, StSkewMax);
        if ((timeMax < options.minInterestingTime) || (timeMaxSkew < options.skewThreshold))
            return false;

        stat_t timeAvg = activity.getStatRaw(StTimeLocalExecute, StAvgX);
        __uint64 cost = (timeMax - timeAvg);
        IWuEdge * leftEdge = activity.queryInput(0);
        if (!leftEdge)
        {
            results.set(cost, "INTERNAL: Missing input.  JOIN has significant skew");
            return true;
        }
        stat_t inputMaxSkew = leftEdge->getStatRaw(StNumRowsProcessed, StSkewMax);

        //HOW?
        //Need an attribute which is the skew in the left input distribution. Once join has done any redistribution.
        stat_t leftMaxSkew = statSkewPercent(0);
        if (leftMaxSkew > options.skewThreshold)
        {
            if (leftMaxSkew > inputMaxSkew)
                results.set(cost, "JOIN condition creates significant skew - check for very common values");
            else
                results.set(cost, "JOIN distribution has significant skew - check for very common values");
            return true;
        }

        stat_t compareMaxSkew = activity.getStatRaw(StNumCompares, StSkewMax);
        if (compareMaxSkew > options.skewThreshold)
        {
            results.set(cost, "Some values in LEFT match a disproportionate number of records");
            return true;
        }

        //Check if transform time is greater than average - if so suggest parallel.
        return false;
    }
};


class ExcessiveSkewRule : public AActivityRule
{
public:
    virtual bool check(PerformanceIssue & results, IWuActivity & activity, const WuAnalyseOptions & options) override
    {
        ThorActivityKind kind = (ThorActivityKind)activity.getAttr(WaKind);
        const char * activityText = activityKindStr(kind);

        const StatisticKind stat = StTimeLocalExecute;
        stat_t timeAvg = activity.getStatRaw(stat, StAvgX);
        stat_t timeMaxSkew = activity.getStatRaw(stat, StSkewMax);
        if ((timeMaxSkew > 2* options.skewThreshold) && (timeAvg > options.minInterestingTime))
        {
            stat_t timeMax = activity.getStatRaw(stat, StMaxX);
            __uint64 cost = (timeMax - timeAvg);

            results.set(cost, "Excessive skew in %s processing time", activityText);
            return true;
        }
        return false;
    }
};


void gatherRules(CIArrayOf<AActivityRule> & rules, bool includeAll)
{
    //Only one issue is reported per activity - order the issues most to least specific

    rules.append(*new IoSkewRule(StTimeDiskReadIO, "disk read"));
    rules.append(*new IoSkewRule(StTimeDiskWriteIO, "disk write"));
    rules.append(*new IoSkewRule(StTimeSpillElapsed, "spill"));
    rules.append(*new DistributeSkewRule);
    rules.append(*new ParallelInlineRule);
    rules.append(*new JoinRule);
    rules.append(*new ProcessingSkewRule(StTimeLocalExecute, "execute"));
    rules.append(*new ExcessiveSkewRule);
    //    rules.append(*new ParallelDiskReadRule);
}



/*
 * The following are a list of possible rules:
 *
 * Issues that may cause timing problems elsewhere
 * - DISTRIBUTE with skewed output rows:  skew > x
 * - GLOBAL SORT with skewed output rows: skew > x
 * - Skew in the maxmimum group size for a group activity
 * - Any operation that executes sequentially.  (Tag and flag if took > certain time).
 *
 * Skew timing problems (all if skew>threashold)
 * - Some nodes spilling, and others not - lost time is potentially max-min not just max-avg: minSpill=0 and max != 0
 * - Any 0:1 1:1 or 1:0 activity with skewed time.  Correlate with the number of input/output rows.  (Is it a skew in the data, or something else.)
 * - inline with high skew (as above) - suggest use DISTRIBUTED
 * - Skewed disk IO time - all on one node or poor distribution.
 * - Unbalanced JOIN: skew(numcompares) > n.  For global needs access to the input row skew after sort/distribute.  For local, compare with #input rows
 * - Skew in number of spills.
 *
 * Other timing problems.
 * - Projected disk read with local time >> disk read time - use parallel (when implemented!)
 * - Slow join.  Could make parallel.
 * - Ratio of diskio time to size read is out of line.  Especially if ratio is skewed.
 * - index read/keyed join with large number of rejected rows
 * - Large amount of time spend in function calls or soapcalls
 *
 * Other suggestions
 * - ROLLUP non group with large number of rows may be better as AGGREGATE/ROLLUP(,GROUP)
 *
 * Useful for platform developers:
 * - Gaps in timing - e.g., activities << subgraph time.
 * - long time spent waiting for queues (summarise it?)  WhenQueued (graph/wu?)
 * - Summary of IO/spilling as a totoal of the job.
 * - Summary of idle times as a % of total time
 *
 * Other useful information:
 * - What was the avg %cpu while running a graph/subgraph (and number of cpus)
 * - Total os disk read bytes that occured when running a subgraph (and compare with activity numbers)
 * - How many other colocated thors were active while this one was
 *
 * Where do nested/aggregated stats fit into all of this
 */
