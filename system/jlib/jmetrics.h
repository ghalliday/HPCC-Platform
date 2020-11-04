/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2020 HPCC Systems®.

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


#ifndef JMETRICS_H
#define JMETRICS_H

#include <vector>
#include "jlib.hpp"
#include "jstats.h"

//---------------------------------------------------

enum class MetricType { Counter, Gauge, Histogram };
using metric_value = stat_type;

//An interface that metrics are rported to - common examples write to disk, save in an array, filter or calculate deltas
interface IMetric;
interface IMetricReceiver
{
    virtual void noteScalar(const IMetric * metric, metric_value value) = 0;
    //Not sure of the best way of processing historgrams - it needs more investigation.  Two possible ideas below.
    virtual bool beginHistogram(const IMetric * metric, const char * grouping) = 0;
    virtual void noteHistogramValue(const char * threshold, metric_value value) = 0;
    virtual void endHistogram() = 0;
    //This may possibly be a better interface for histograms
    //virtual void noteHistogram(const IMetric * metric, const char * grouping, const std::vector<std::string> & thresholds, const std::vector<metric_value> & values) = 0;

};

//An interface for a class that will report 1 or more metric values.
interface IMetricReporter : public IInterface
{
    virtual void report(IMetricReceiver & target) const = 0;
};

//This might be overkill.  A class/struct with public members might be just as good.
//There isn't a strong reason for this to be derived from a metric reporter. Only to reduce the number of vmts.
//There may even be a good reason for it not to be derived from IMertricReporter if it is common for a single reporter
//to report multiple metrics.  I am inclined to change...
interface IMetric : public IMetricReporter
{
    virtual StatisticKind queryKind() const = 0;
    virtual const char * queryScope() const = 0;
    virtual const char * queryDescription() const = 0;
    virtual const char * queryKey() const = 0;
    virtual MetricType queryType() const = 0;
    virtual StatisticMeasure queryUnits() const = 0;
};

interface IMetricRegistry : public IMetricReporter
{
    virtual void registerReporter(IMetricReporter * reporter) = 0;
    virtual void removeReporter(IMetricReporter * reporter) = 0;
};

interface IMetricSink : public IInterface
{
    virtual void start();
    virtual void stop();
};

//A function of this type will be exported from a dll that implements a metric sink
typedef IMetricSink * metricSinkFactory(IPropertyTree * options);

extern jlib_decl IMetricRegistry & queryMetricRegistry();

#endif
