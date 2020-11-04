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


#include "jiface.hpp"
#include "jmetrics.h"
#include "jexcept.hpp"
#include "jfile.hpp"
#include "jptree.hpp"

//MORE: Could implement a const version of this with a constexpr constructor...
class CMetric : public CInterfaceOf<IMetric>
{
public:
    CMetric(StatisticKind _kind, const char * _scope, const char * _desc, MetricType _type)
    : kind(_kind), type(_type), scope(_scope), desc(_desc)
    {
        key.append(scope).append(".").append(queryStatisticName(kind));
    }
    virtual StatisticKind queryKind() const { return kind; }
    virtual const char * queryScope() const override { return scope; }
    virtual const char * queryDescription() const override { return desc; }
    virtual MetricType queryType() const override { return type; }
    virtual StatisticMeasure queryUnits() const override { return queryMeasure(kind); }
    virtual const char * queryKey() const override { return key; }
protected:
    StatisticKind kind;
    MetricType type;
    StringAttr scope;
    StringAttr desc;
    StringBuffer key;
};

class CounterMetric : public CMetric
{
public:
    CounterMetric(StatisticKind _kind, const char * _scope, const char * _desc) : CMetric(_kind, _scope, _desc, MetricType::Counter)
    {
    }

    void inc(metric_value delta)
    {
        value.add_fetch(delta);
    }

    virtual void report(IMetricReceiver & target) const override
    {
        target.noteScalar(this, value);
    }

protected:
    RelaxedAtomic<metric_value> value;
};

class MetricRegistry : public CInterfaceOf<IMetricRegistry>
{
public:
    virtual void report(IMetricReceiver & target) const override
    {
        CriticalBlock block(cs);
        ForEachItemIn(i, metrics)
            metrics.item(i).report(target);
    }

    virtual void registerReporter(IMetricReporter * reporter) override
    {
        CriticalBlock block(cs);
        metrics.append(*LINK(reporter));
    }

    virtual void removeReporter(IMetricReporter * reporter) override
    {
        CriticalBlock block(cs);
        metrics.zap(*reporter);
    }

protected:
    mutable CriticalSection cs;
    IArrayOf<IMetricReporter> metrics;
};

static std::atomic<IMetricRegistry *> registry {nullptr};
static CriticalSection registryLock;
IMetricRegistry & queryMetricRegistry()
{
    return *querySingleton(registry, registryLock, []{ return new MetricRegistry(); });
}


//The following class is used to filter the set of metrics that are passed onto a metric sink (usd internall by metric sinks)
class FilteredMetricReceiver : public IMetricReceiver
{
public:
    FilteredMetricReceiver(IMetricReceiver & _target) : target(_target)
    {
    }

    virtual void noteScalar(const IMetric * metric, metric_value value) override
    {
        if (include(metric))
            target.noteScalar(metric, value);
    }
    //Not sure of the best way of processing historgrams - it needs more investigation.  Two possible ideas below.
    virtual bool beginHistogram(const IMetric * metric, const char * grouping) override
    {
        if (!include(metric))
            return false;
        return target.beginHistogram(metric, grouping);
    }
    virtual void noteHistogramValue(const char * threshold, metric_value value) override
    {
        target.noteHistogramValue(threshold, value);
    }
    virtual void endHistogram() override
    {
        target.endHistogram();
    }

    virtual bool include(const IMetric * metric) const = 0;

protected:
    IMetricReceiver & target;
};

//An example sink which directly streams the metrics to a file.
class FileMetricSink : public CInterfaceOf<IMetricSink>
{
    class FileMetricReceiver : public IMetricReceiver
    {
    public:
        virtual void noteScalar(const IMetric * metric, metric_value value) override
        {
            output.append(metric->queryScope()).append("_").append(queryStatisticName(metric->queryKind())).append(": ");
            formatStatistic(output, value, metric->queryUnits());
            output.newline();
        }
        virtual bool beginHistogram(const IMetric * metric, const char * grouping) override { UNIMPLEMENTED; }
        virtual void noteHistogramValue(const char * threshold, metric_value value) override { UNIMPLEMENTED; }
        virtual void endHistogram() override { UNIMPLEMENTED; }

        void write(const char * filename)
        {
            Owned<IFile> outfile = createIFile(filename);
            Owned<IFileIO> io = outfile->open(IFOcreate);
            io->write(0, output.length(), output.str());
        }
    protected:
        StringBuffer output;
    };
public:
    FileMetricSink(IPropertyTree * _options)
    {
        filename.set(_options->queryProp("@filename"));
    }
    virtual void start()
    {
        //MORE: Create a thread which reports metrics to the file peridically
    }
    virtual void stop()
    {
        //MORE: join the thread and finish
    }

    void gather()
    {
        FileMetricReceiver target;
        queryMetricRegistry().report(target);
        target.write(filename);
    }

protected:
    StringAttr filename;
};
