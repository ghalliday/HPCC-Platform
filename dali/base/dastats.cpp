/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2025 HPCC Systems®.

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


#include "platform.h"
#include "jlib.hpp"
#include "jfile.hpp"
#include "jlzw.hpp"

#include "dastats.hpp"
#include "dasds.hpp"

static bool hourGranularity = true;
static constexpr unsigned STATS_CONNECTION_TIMEOUT = 5000;
static constexpr const char * GlobalMetricsDaliRoot = "/GlobalMetrics";

    //MORE: Ideally this would be implememted by the connect() operation in dali
IPropertyTree * ensureFilteredPTree(IPropertyTree * root, const char * element, const QualifierList & qualifiers)
{
    if (qualifiers.size())
    {
        StringBuffer childPath;
        childPath.append(element);
        if (qualifiers.size())
        {
            childPath.append('[');
            bool first = true;
            for (auto & [name, value] : qualifiers)
            {
                if (!first)
                    childPath.append(',');

                childPath.append(name).append('=');
                childPath.append('"').append(value).append('"');
                first = false;
            }
            childPath.append(']');
        }

        IPropertyTree * match = root->queryPropTree(childPath);
        if (!match)
        {
            match = root->addPropTree(element, createPTree());
            for (auto & [name, value] : qualifiers)
                match->setProp(name.c_str(), value.c_str());
        }
        return match;
    }
    else
    {
        IPropertyTree * match = root->queryPropTree(element);
        if (!match)
            match = root->addPropTree(element, createPTree());
        return match;
    }
}


//MORE: Ideally this would be implememted as an atomic operation in dasever.  Until then it is coded less efficiently.
void daliAtomicUpdate(IPropertyTree * entry, const std::vector<std::string> & attributes, const std::vector<stat_type> & values)
{
    for (unsigned i = 0; i < attributes.size(); i++)
    {
        if (values[i])
        {
            const char * attribute = attributes[i].c_str();
            stat_type prevValue = entry->getPropInt64(attribute);
            entry->setPropInt64(attribute, prevValue + values[i]);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------

/*
 * Currently global metrics are represented in dali in the following way
 *
 * /GlobalMetrics
 *   /Component
 *     /Instance[@dimension1=...]
 *       /Timeslot[@start="yyyymmddhh" @end="yyyymmddhh"]
*           /Metrics[@metric1=... @metric2=...]
*/


static void convertDimensionsToQualifiers(QualifierList & qualifiers, const StatsDimensionList & dimensions)
{
    for (auto & [name, value] : dimensions)
    {
        std::string attributeName;
        attributeName = '@' + name;
        qualifiers.emplace_back(std::move(attributeName), value);
    }
}

static void getTimeslotValue(StringBuffer & value, const CDateTime & when)
{
    //MORE: Introduce CDateTime::getUtcYear() etc.??
    unsigned year, month, day;
    when.getDate(year, month, day, false);

    if (hourGranularity)
    {
        unsigned hour, minute, second, nano;
        when.getTime(hour, minute, second, nano, false);
        value.appendf("%04d%02d%02d%02d", year, month, day, hour);
    }
    else
        value.appendf("%04d%02d%02d", year, month, day);
}

inline std::string getStatisticAttribute(StatisticKind kind)
{
    return '@' + queryStatisticName(kind);
}

static void recordGlobalMetrics(const char * category, const StatsDimensionList & dimensions, const std::vector<std::string> &attributes, const std::vector<stat_type> & deltas)
{
    //Ideally this would connect to:
    //
    //  /GlobalMetrics/<category>/Instance[@dimension1=...]/Timeslot[@start="yyyymmddhh" @rollup="h"]/Metrics
    //
    // but that would require connect to support the creation of tags with attributes if they did not exist (should be possible)
    // and the initial values for @rollup would need to be provided as extra search critia or something similar.
    //

    QualifierList qualifiers;
    convertDimensionsToQualifiers(qualifiers, dimensions);

    StringBuffer timeslotValue;
    CDateTime now;
    now.setNow();
    getTimeslotValue(timeslotValue, now);


    StringBuffer rootPath;
    rootPath.append(GlobalMetricsDaliRoot).append('/').append(category);
    Owned<IRemoteConnection> conn = querySDS().connect(rootPath, myProcessSession(), RTM_LOCK_WRITE|RTM_CREATE_QUERY, STATS_CONNECTION_TIMEOUT);

    IPropertyTree * instance = ensureFilteredPTree(conn->queryRoot(), "Instance", qualifiers);
    IPropertyTree * timeslot = ensureFilteredPTree(instance, "Timeslot", QualifierList{{"@startTime", timeslotValue.str()},{"endtime",timeslotValue.str()}});
    IPropertyTree * metrics = ensurePTree(timeslot, "Metrics");

    daliAtomicUpdate(metrics, attributes, deltas);
}

void recordGlobalMetrics(const char * category, const StatsDimensionList & dimensions, const CRuntimeStatisticCollection & stats, const StatisticsMapping * optMapping)
{
    const StatisticsMapping * mapping = optMapping ? optMapping : &stats.queryMapping();
    std::vector<std::string> attributes;
    std::vector<stat_type> deltas;
    for (unsigned i = 0; i < mapping->numStatistics(); i++)
    {
        StatisticKind kind = mapping->getKind(i);
        attributes.emplace_back(getStatisticAttribute(kind));
        deltas.emplace_back(stats.getStatisticValue(kind));
    }

    recordGlobalMetrics(category, dimensions, attributes, deltas);
}

void recordGlobalMetrics(const char * category, const StatsDimensionList & dimensions, const std::initializer_list<StatisticKind> & stats, const std::initializer_list<stat_type> & values)
{
    dbgassertex(stats.size() == values.size());
    std::vector<std::string> attributes;
    std::vector<stat_type> deltas;
    for (unsigned i = 0; i < stats.size(); i++)
    {
        StatisticKind kind = stats.begin()[i];
        attributes.emplace_back(getStatisticAttribute(kind));
        deltas.emplace_back(values.begin()[i]);
    }

    recordGlobalMetrics(category, dimensions, attributes, deltas);
}

//---------------------------------------------------------------------------------------------------------------------

static void getInstanceFilter(StringBuffer & xpath, const StatsDimensionList & optDimensions)
{
    if (optDimensions.size())
    {
        xpath.append('[');
        bool first = true;
        for (const auto & x : optDimensions)
        {
            if (!first)
                xpath.append(',');
            xpath.append('@').append(x.first).append('=').append('"').append(x.second).append('"');
            first = false;
        }
        xpath.append(']');
    }
}

interface xxIGlobalStatisicWalker
{
    virtual void processGlobalStatistics(const char * category, const StatsDimensionList & dimensions, const GlobalStatasticsList & stats) = 0;
};


static void gatherInstanceStatistics(IPropertyTree & categoryTree, const StatsDimensionList & optDimensions, const CDateTime & from, const CDateTime & to, IGlobalStatisicWalker & walker)
{
    StringBuffer instancePath;
    instancePath.append("Instance");
    getInstanceFilter(instancePath, optDimensions);

    //Timeslot star and end times are inclusive, search end time is exclusive
    //The timeslot is a match if endTime >= searchStartTime and startTime < searchEndTime
    StringBuffer timeslotXPath;
    timeslotXPath.append("Timeslot[@startTime>=");
    getTimeslotValue(timeslotXPath, to);
    timeslotXPath.append(",@endTime<");
    getTimeslotValue(timeslotXPath, from);
    timeslotXPath.append("]/Metrics");

    Owned<IPropertyTreeIterator> iter = categoryTree.getElements(instancePath, iptiter_remote);
    ForEach(*iter)
    {
        IPropertyTree & instance = iter->query();
        //Gather the dimensions...

        Owned<IPropertyTreeIterator> timeslots = instance.getElements(timeslotXPath, iptiter_remote);
        ForEach(*timeslots)
        {
            //Gather the statistics
        }
    }
}

extern da_decl void gatherGlobalStatistics(const char * optCategory, const StatsDimensionList & optDimensions, const CDateTime & from, const CDateTime & to, IGlobalStatisicWalker & walker)
{
    if (optCategory)
    {
        StringBuffer xpath;
        xpath.append(GlobalMetricsDaliRoot).append('/').append(optCategory);

        Owned<IRemoteConnection> conn = querySDS().connect(xpath, myProcessSession(), RTM_LOCK_READ, STATS_CONNECTION_TIMEOUT);
        if (!conn)
            return;

        gatherInstanceStatistics(*conn->queryRoot(), optDimensions, from, to, walker);
    }
    else
    {
        Owned<IRemoteConnection> conn = querySDS().connect(GlobalMetricsDaliRoot, myProcessSession(), RTM_LOCK_READ, STATS_CONNECTION_TIMEOUT);
        if (!conn)
            return;

        Owned<IPropertyTreeIterator> iter = conn->queryRoot()->getElements("*");
        ForEach(*iter)
        gatherInstanceStatistics(iter->query(), optDimensions, from, to, walker);
    }
}

void resetGlobalMetrics(const char * category, const StatsDimensionList & optDimensions)
{
    assertex(category);

    StringBuffer xpath;
    xpath.append(GlobalMetricsDaliRoot).append('/').append(category);

    Owned<IRemoteConnection> conn = querySDS().connect(xpath, myProcessSession(), RTM_LOCK_WRITE, STATS_CONNECTION_TIMEOUT);
    if (!conn)
        return;

    StringBuffer childPath;
    childPath.append("Instance");
    getInstanceFilter(childPath, optDimensions);

    IPropertyTree * root = conn->queryRoot();
    ICopyArrayOf<IPropertyTree> toRemove;
    Owned<IPropertyTreeIterator> it = root->getElements(childPath);
    ForEach(*it)
        toRemove.append(it->query());

    ForEachItemIn(i, toRemove)
        root->removeTree(&toRemove.item(i));
}
