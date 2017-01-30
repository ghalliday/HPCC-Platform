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
#include "anacommon.hpp"
#include "anarule.hpp"
#include "anawu.hpp"

class WuScope;
class WorkunitAnalyser;

class WuScopeHashTable : public SuperHashTableOf<WuScope, const char>
{
public:
    ~WuScopeHashTable() { _releaseAll(); }

    WuScope * create(const char * name, WuScope * parent);
    virtual void     onAdd(void *et);
    virtual void     onRemove(void *et);
    virtual unsigned getHashFromElement(const void *et) const;
    virtual unsigned getHashFromFindParam(const void *fp) const;
    virtual const void * getFindParam(const void *et) const;
    virtual bool matchesFindParam(const void *et, const void *key, unsigned fphash) const;
    virtual bool matchesElement(const void *et, const void *searchET) const;
};

inline unsigned hashScope(const char * name) { return hashc((const byte *)name, strlen(name), 0); }


class WuScope : public CInterface, implements IWuEdge, implements IWuActivity
{
public:
    WuScope(const char * _name, WuScope * _parent) : name(_name), parent(_parent)
    {
        attrs.setown(createPTree());
    }

    void applyRules(WorkunitAnalyser & analyser);
    void connectActivities();
    StatisticScopeType queryScopeType();
    const char * queryScopeTypeName() { return ::queryScopeTypeName(queryScopeType()); }
    WuScope * select(const char * scope);
    void setInput(unsigned i, WuScope * scope);
    void setOutput(unsigned i, WuScope * scope);
    WuScope * source();
    WuScope * target();
    void trace();

//
    virtual IWuActivity & querySource();
    virtual IWuEdge * queryInput(unsigned idx);
    virtual IWuEdge * queryOutput(unsigned idx);

    virtual stat_t getStatRaw(StatisticKind kind, StatisticKind variant = StKindNone);
    virtual unsigned getAttr(WuAttr kind);
    virtual void getAttr(StringBuffer & result, WuAttr kind);

    inline IPropertyTree * queryAttrs() const { return attrs; }
    inline const char * queryName() const { return name; }

protected:
    StringAttr name;
    WuScope * parent = nullptr;
    Owned<IPropertyTree> attrs;
    StatisticScopeType stype;
    WuScopeHashTable scopes;
    std::vector<WuScope *> inputs;
    std::vector<WuScope *> outputs;
};

//---------------------------------------------------------------------------------------------------------------------


class WorkunitAnalyser
{
public:
    WorkunitAnalyser(WuAnalyseOptions & _options);

    void check(const char * scope, IWuActivity & activity);
    void check(IConstWorkUnit * wu);
    void trace();

protected:
    void applyRules();
    void collateWorkunitStats(IConstWorkUnit * workunit, const WuScopeFilter & filter);
    void printWarnings();
    WuScope * selectFullScope(const char * scope);

protected:
    CIArrayOf<AActivityRule> rules;
    CIArrayOf<PerformanceIssue> issues;
    WuScope root;
    WuAnalyseOptions & options;
};

//---------------------------------------------------------------------------------------------------------------------

WuScope * WuScopeHashTable::create(const char * name, WuScope * parent)
{
    WuScope * next = new WuScope(name, parent);
    addNew(next);
    return next;
}

void WuScopeHashTable::onAdd(void *et)
{
}
void WuScopeHashTable::onRemove(void *et)
{
    WuScope * elem = reinterpret_cast<WuScope *>(et);
    elem->Release();
}
unsigned WuScopeHashTable::getHashFromElement(const void *et) const
{
    const WuScope * elem = reinterpret_cast<const WuScope *>(et);
    return hashScope(elem->queryName());
}
unsigned WuScopeHashTable::getHashFromFindParam(const void *fp) const
{
    const char * search = reinterpret_cast<const char *>(fp);
    return hashScope(search);
}
const void * WuScopeHashTable::getFindParam(const void *et) const
{
    const WuScope * elem = reinterpret_cast<const WuScope *>(et);
    return elem->queryName();
}
bool WuScopeHashTable::matchesFindParam(const void *et, const void *key, unsigned fphash) const
{
    const WuScope * elem = reinterpret_cast<const WuScope *>(et);
    const char * search = reinterpret_cast<const char *>(key);
    return streq(elem->queryName(), search);
}
bool WuScopeHashTable::matchesElement(const void *et, const void *searchET) const
{
    const WuScope * elem = reinterpret_cast<const WuScope *>(et);
    const WuScope * searchElem = reinterpret_cast<const WuScope *>(searchET);
    return streq(elem->queryName(), searchElem->queryName());
}

//---------------------------------------------------------------------------------------------------------------------

void WuScope::applyRules(WorkunitAnalyser & analyser)
{
    for (auto & cur : scopes)
    {
        if (cur.queryScopeType() == SSTactivity)
            analyser.check(cur.queryName(), cur);
        cur.applyRules(analyser);
    }
}

void WuScope::connectActivities()
{
    //Yuk - scopes can be added to while they are being iterated, so need to create a list first
    CICopyArrayOf<WuScope> toWalk;
    for (auto & cur : scopes)
        toWalk.append(cur);

    ForEachItemIn(i, toWalk)
    {
        WuScope & cur = toWalk.item(i);
        if (cur.queryScopeType() == SSTedge)
        {
            WuScope * source = cur.source();
            WuScope * sink = cur.target();
            if (source)
                source->setOutput(cur.getAttr(WaSourceIndex), &cur);
            if (sink)
                sink->setInput(cur.getAttr(WaTargetIndex), &cur);
        }
        cur.connectActivities();
    }
}
StatisticScopeType WuScope::queryScopeType()
{
    return (StatisticScopeType)attrs->getPropInt("@stype");
}

void WuScope::setInput(unsigned i, WuScope * scope)
{
    while (inputs.size() <= i)
        inputs.push_back(nullptr);
    inputs[i] = scope;
}

void WuScope::setOutput(unsigned i, WuScope * scope)
{
    while (outputs.size() <= i)
        outputs.push_back(nullptr);
    outputs[i] = scope;
}

WuScope * WuScope::source()
{
    const char * source = attrs->queryProp("@IdSource");
    if (!source)
        return nullptr;
    return parent->select(source);
}

WuScope * WuScope::target()
{
    const char * target = attrs->queryProp("@IdTarget");
    if (!target)
        return nullptr;
    return parent->select(target);
}

IWuActivity & WuScope::querySource()
{
    return *source();
}

IWuEdge * WuScope::queryInput(unsigned idx)
{
    if (idx < inputs.size())
        return inputs[idx];
    else
        return nullptr;
}

IWuEdge * WuScope::queryOutput(unsigned idx)
{
    if (idx < outputs.size())
        return outputs[idx];
    else
        return nullptr;
}

stat_t WuScope::getStatRaw(StatisticKind kind, StatisticKind variant)
{
    StringBuffer name;
    name.append('@').append(queryStatisticName(kind | variant));
    return attrs->getPropInt64(name);
}

unsigned WuScope::getAttr(WuAttr attr)
{
    StringBuffer name;
    name.append('@').append(queryWuAttributeName(attr));
    return attrs->getPropInt64(name);
}

void WuScope::getAttr(StringBuffer & result, WuAttr attr)
{
    StringBuffer name;
    name.append('@').append(queryWuAttributeName(attr));
    attrs->getProp(result, name);
}

WuScope * WuScope::select(const char * scope)
{
    assertex(scope);
    WuScope * match = scopes.find(scope);
    if (match)
        return match;
    match = scopes.create(scope, this);
    return match;
}

void WuScope::trace()
{
    printf("%s: \"%s\" (", queryScopeTypeName(), queryName());
    for (auto in : inputs)
        printf("%s ", in->queryName());
    printf("->");
    for (auto out : outputs)
        printf("%s ", out->queryName());
    printf(") {\n");

    printXML(queryAttrs());
    for (auto & cur : scopes)
    {
        cur.trace();
    }
    printf("}\n");
}

//---------------------------------------------------------------------------------------------------------------------

/* Callback used to output the different scope properties as xml */
class AnalyserStatsGatherer : public IWuScopeVisitor
{
public:
    AnalyserStatsGatherer(IPropertyTree * _scope) : scope(_scope) {}

    virtual void noteStatistic(StatisticKind kind, unsigned __int64 value, IConstWUStatistic & cur) override
    {
        StringBuffer name;
        name.append('@').append(queryStatisticName(kind));
        scope->setPropInt64(name, value);
    }
    virtual void noteAttribute(WuAttr attr, const char * value)
    {
        StringBuffer name;
        name.append('@').append(queryWuAttributeName(attr));
        scope->setProp(name, value);
    }
    virtual void noteHint(const char * kind, const char * value)
    {
        throwUnexpected();
    }
    IPropertyTree * scope;
};

//---------------------------------------------------------------------------------------------------------------------

WorkunitAnalyser::WorkunitAnalyser(WuAnalyseOptions & _options) : root("", nullptr), options(_options)
{
    gatherRules(rules, true);
}

void WorkunitAnalyser::check(const char * scope, IWuActivity & activity)
{
    //Is this always valid - for instance skews created by distributes?
    if (activity.getStatRaw(StTimeLocalExecute, StMaxX) < options.minInterestingTime)
        return;

    Owned<PerformanceIssue> issue;
    ForEachItemIn(i, rules)
    {
        if (rules.item(i).isCandidate(activity))
        {
            if (!issue)
                issue.setown(new PerformanceIssue);
            if (rules.item(i).check(*issue, activity, options))
            {
                if (issue->cost >= options.minCost)
                {
                    //Only report one issue per activity - order the issues most to least specific
                    issue->scope.set(scope);
                    issues.append(*issue.getClear());
                    break;
                }
                else
                    issue.clear();
            }
        }
    }
}



void WorkunitAnalyser::applyRules()
{
    root.applyRules(*this);
    issues.sort(compareIssues);
}


void WorkunitAnalyser::check(IConstWorkUnit * wu)
{
    WuScopeFilter filter;
    filter.addOutputProperties(PTstatistics).addOutputProperties(PTattributes);
    filter.finishedFilter();
    collateWorkunitStats(wu, filter);
    root.connectActivities();
    //trace();
    applyRules();
    printWarnings();
}


void WorkunitAnalyser::collateWorkunitStats(IConstWorkUnit * workunit, const WuScopeFilter & filter)
{
    Owned<IConstWUScopeIterator> iter = &workunit->getScopeIterator(filter);
    ForEach(*iter)
    {
        try
        {
            WuScope * scope = selectFullScope(iter->queryScope());
            AnalyserStatsGatherer callback(scope->queryAttrs());
            scope->queryAttrs()->setPropInt("@stype", iter->getScopeType());
            iter->playProperties(callback);
        }
        catch (IException * e)
        {
            e->Release();
        }
    }
}


void WorkunitAnalyser::printWarnings()
{
    //MORE Suppress multiple warnings for a single
    ForEachItemIn(i, issues)
        issues.item(i).print();
}


WuScope * WorkunitAnalyser::selectFullScope(const char * scope)
{
    StringBuffer temp;
    WuScope * resolved = &root;
    for (;;)
    {
        const char * dot = strchr(scope, ':');
        if (!dot)
        {
            if (!*scope)
                return resolved;
            return resolved->select(scope);
        }

        temp.clear().append(dot-scope, scope);
        resolved = resolved->select(temp.str());
        scope = dot+1;
    }
}


void WorkunitAnalyser::trace()
{
    root.trace();
}


//---------------------------------------------------------------------------------------------------------------------


void analyseWorkunit(IConstWorkUnit * wu)
{
    WuAnalyseOptions options;
    options.minCost = 1000000000;  // Anything over a 1s
    options.skewThreshold = statSkewPercent(10);
    WorkunitAnalyser analyser(options);
    analyser.check(wu);
}
