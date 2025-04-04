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

static constexpr unsigned STATS_CONNECTION_TIMEOUT = 5000;
static constexpr const char * GlobalMetricsDaliRoot = "/GlobalMetrics";

//MORE: Ideally this would be implememted by the connect() operation in dali
Owned<IPropertyTree> connectToQualifiedElement(const char * xpath, const char * element, const QualifierList & qualifiers)
{
    Owned<IRemoteConnection> conn = querySDS().connect(xpath, myProcessSession(), RTM_LOCK_WRITE|RTM_CREATE_QUERY, STATS_CONNECTION_TIMEOUT);

    //Potential optimization 1: allow the qualifiers to
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

    Owned<IPropertyTree> match = conn->queryRoot()->getPropTree(childPath);
    if (!match)
    {
        match.set(conn->queryRoot()->addPropTree(childPath, nullptr));
        for (auto & [name, value] : qualifiers)
            match->setProp(name.c_str(), value.c_str());
    }

    return std::move(match);
}

//MORE: Ideally this would be implememted as an atomic operation in dasever.  Until then it is coded less efficiently.
void atomicDaliUpdate(const char * xpath, const char * element, const QualifierList & qualifiers, const std::vector<std::string> & attributes, const std::vector<stat_type> & values)
{
    dbgassertex(attributes.size() == values.size());

    Owned<IPropertyTree> match = connectToQualifiedElement(xpath, element, qualifiers);

    for (unsigned i = 0; i < attributes.size(); i++)
    {
        if (values[i])
        {
            const char * attribute = attributes[i].c_str();
            stat_type prevValue = match->getPropInt64(attribute);
            match->setPropInt64(attribute, prevValue + values[i]);
        }
    }
}


static void convertDimensionsToQualifiers(QualifierList & qualifiers, const DimensionList & dimensions)
{
    for (auto & [name, value] : dimensions)
    {
        std::string attributeName;
        attributeName = '@' + name;
        qualifiers.emplace_back(std::move(attributeName), value);
    }
}

inline std::string getStatisticAttribute(StatisticKind kind)
{
    return '@' + queryStatisticName(kind);
}

void recordGlobalMetrics(const char * category, const DimensionList & dimensions, const CRuntimeStatisticCollection & stats, const StatisticsMapping * optMapping)
{
    const StatisticsMapping * mapping = optMapping ? optMapping : &stats.queryMapping();
    QualifierList qualifiers;
    convertDimensionsToQualifiers(qualifiers, dimensions);

    std::vector<std::string> attributes;
    std::vector<stat_type> deltas;
    for (unsigned i = 0; i < mapping->numStatistics(); i++)
    {
        StatisticKind kind = mapping->getKind(i);
        attributes.emplace_back(getStatisticAttribute(kind));
        deltas.emplace_back(stats.getStatisticValue(kind));
    }

    atomicDaliUpdate(GlobalMetricsDaliRoot, category, qualifiers, attributes, deltas);
}

void recordGlobalMetrics(const char * category, const DimensionList & dimensions, const std::initializer_list<StatisticKind> & stats, const std::initializer_list<stat_type> & values)
{
    dbgassertex(stats.size() == values.size());

    QualifierList qualifiers;
    convertDimensionsToQualifiers(qualifiers, dimensions);

    std::vector<std::string> attributes;
    std::vector<stat_type> deltas;
    for (unsigned i = 0; i < stats.size(); i++)
    {
        StatisticKind kind = stats.begin()[i];
        attributes.emplace_back(getStatisticAttribute(kind));
        deltas.emplace_back(values.begin()[i]);
    }

    atomicDaliUpdate(GlobalMetricsDaliRoot, category, qualifiers, attributes, deltas);

}
