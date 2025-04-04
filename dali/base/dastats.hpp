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

#ifndef DASTATS_HPP
#define DASTATS_HPP

#ifdef DALI_EXPORTS
#define da_decl DECL_EXPORT
#else
#define da_decl DECL_IMPORT
#endif

#include <vector>
#include <initializer_list>
#include <utility>
#include <string_view>

#include "jstats.h"

using QualifierList = std::vector<std::pair<std::string, std::string>>;
//extern da_decl void daliAtomicUpdate(const char * xpath, const char * element, const QualifierList & qualifiers, const std::vector<std::string> & attributes, const std::vector<stat_type> & values);



using StatsDimensionList = std::vector<std::pair<const char *, const char *>>; // The list of dimeensions are likely to be fixed for each call so use a initializer_list
extern da_decl void recordGlobalMetrics(const char * category, const StatsDimensionList &  dimensions, const CRuntimeStatisticCollection & stats, const StatisticsMapping * optMapping);
extern da_decl void recordGlobalMetrics(const char * category, const StatsDimensionList &  dimensions, const std::initializer_list<StatisticKind> & stats, const std::initializer_list<stat_type> & values);

using GlobalStatasticsList = std::vector<std::pair<StatisticKind, stat_type>>;
interface IGlobalStatisicWalker
{
    virtual void processGlobalStatistics(const char * category, const StatsDimensionList & dimensions, const GlobalStatasticsList & stats) = 0;
};
extern da_decl void gatherGlobalStatistics(const char * optCategory, const StatsDimensionList & optDimensions);
extern da_decl void resetGlobalMetrics(const char * optCategory, const StatsDimensionList & optDimensions);

#endif
