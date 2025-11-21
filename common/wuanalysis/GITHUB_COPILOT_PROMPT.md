# GitHub Copilot Implementation Prompt: Thor Queue Recommendation Analysis

## Context

You are working on the HPCC Platform, a big data processing system. You need to implement a post-execution analysis feature that recommends which Thor queue (cluster configuration) would have been most efficient for a completed workunit.

## Location
- **Directory**: `/common/wuanalysis/`
- **New files to create**: `queuerecommender.hpp` and `queuerecommender.cpp`
- **Existing files to modify**: `anawu.hpp` and `anawu.cpp` (add new API functions)

## Background

### Existing Infrastructure
The directory `common/wuanalysis/` contains:
- `anacommon.hpp/cpp`: Common interfaces (`IWuScope`, `IWuActivity`, `IWuSubGraph`, `PerformanceIssue`)
- `anawu.hpp/cpp`: Main analysis classes (`WorkunitAnalyserBase`, `WorkunitRuleAnalyser`, `WorkunitStatsAnalyser`, `WuScope`)
- `anarule.hpp/cpp`: Rule-based analysis framework

### Key Existing Classes
- `WorkunitAnalyserBase`: Base class for workunit analysis with methods like `analyse()`, `collateWorkunitStats()`
- `WuScope`: Represents a scope in workunit execution tree, has methods `getStatRaw()`, `queryScopeType()`
- Available helper functions:
  - `calcCostNs(double ratePerHour, stat_type ns)` from `jstats.h` - calculates cost from hourly rate and nanoseconds
  - `money2cost_type(double money)` - converts dollars to internal cost representation
  - `cost_type2money(cost_type cost)` - converts internal cost to dollars

## Requirements

Implement a system that:

1. **Analyzes a completed Thor workunit** to extract resource usage from each subgraph
2. **Estimates execution time and cost** for running the same workunit on different Thor queue configurations
3. **Recommends the optimal queue** based on cost efficiency

### Thor Queue Configuration
Each queue has:
- Maximum row memory (bytes)
- Maximum temp disk memory (bytes)  
- Number of CPUs
- Number of workers (parallelism level)
- VM cost per hour (dollars)

### Subgraph Statistics to Extract

For each subgraph, gather these statistics (you'll need to identify the correct `StatisticKind` enum values):
- **SizePeakRowMemory**: Maximum memory for row processing
- **SizePeakTempDisk**: Memory spilled to disk (0 if no spill)
- **SizePeakEphemeralDisk**: Temporary file disk usage
- **TimeUser**: User-space CPU time (nanoseconds)
- **TimeSystem**: Kernel CPU time (nanoseconds)
- **TimeElapsed**: Wall-clock time (nanoseconds)

**TODO**: Research and identify the actual `StatisticKind` enum values for these statistics. They may be named differently in the codebase.

### Time Estimation Algorithm

For each subgraph on each candidate queue:

```
ScaledMemory = PeakRowMemory * (CandidateWorkers / ActualWorkers)

IF ScaledMemory <= QueueMaxMemory AND !OriginallySpilled THEN
    // Memory fits and didn't spill - use actual time
    EstimatedElapsed = ActualElapsed
ELSE IF OriginallySpilled THEN
    // Already spilled - time stays same
    EstimatedElapsed = ActualElapsed  
ELSE
    // Will spill on candidate but didn't originally - apply penalty
    EstimatedElapsed = ActualElapsed * SpillPenaltyFactor
END IF

// Scale based on CPU utilization and workers
CpuTime = TimeUser + TimeSystem
BlockedTime = MAX(0, TimeElapsed - CpuTime)
EstimatedCpuTime = CpuTime * (CandidateWorkers / ActualWorkers)
FinalEstimatedTime = EstimatedCpuTime + BlockedTime
```

**Default spill penalty factor**: 10.0 (configurable)

### Cost Calculation

```
ElapsedTimeNs = SUM(all subgraph estimated times)
CostInDollars = calcCostNs(QueueVMCostPerHour, ElapsedTimeNs) * NumWorkers
TotalCost = money2cost_type(CostInDollars)
```

## Implementation Tasks

### 1. Create `queuerecommender.hpp`

Define these classes with proper HPCC Platform conventions:

#### QueueConfiguration
```cpp
class WUANALYSIS_API QueueConfiguration : public CInterface
{
    // Stores queue configuration: name, memory limits, workers, cost
    // Constructor: QueueConfiguration(name, maxRowMemory, maxTempMemory, numCpus, numWorkers, costPerHour)
    // Add getters for all properties
    // Add setters for configuration
};
```

#### SubgraphResourceUsage  
```cpp
class SubgraphResourceUsage
{
    // Extracts and stores subgraph statistics
    // Constructor: SubgraphResourceUsage(WuScope * subgraph, unsigned actualWorkers)
    // Method: extractStatistics() - use WuScope::getStatRaw() to extract stats
    // Getters: queryPeakRowMemory(), queryPeakTempDisk(), queryTimeElapsed(), etc.
    // Calculated: queryCpuTime(), queryBlockedTime(), didSpill()
};
```

#### QueueEstimation
```cpp  
class WUANALYSIS_API QueueEstimation : public CInterface
{
    // Stores estimation results for one queue
    // Accumulates subgraph estimates
    // Calculates total time and cost
    // Tracks if workunit will fit (memory constraints)
    // Comparison methods for sorting by cost/time
};
```

#### WorkunitQueueAnalyser
```cpp
class WUANALYSIS_API WorkunitQueueAnalyser : public WorkunitAnalyserBase
{
    // Main analysis class
    // Methods:
    //   - addQueueConfiguration(QueueConfiguration * queue)
    //   - setSpillPenaltyFactor(double factor)
    //   - analyseQueues(IConstWorkUnit * wu)
    //   - queryBestQueue() - returns lowest cost queue that fits
    //   - printReport() - outputs formatted analysis
    //   - addReportToWorkunit(IWorkUnit * wu) - stores results in WU
    
    // Protected helper methods:
    //   - gatherSubgraphUsages() - collect from all subgraphs
    //   - estimateForQueue(queue, estimation) - run estimation algorithm
    //   - estimateSubgraphTime(usage, queue) - per-subgraph estimation
};
```

### 2. Implement `queuerecommender.cpp`

Implement all methods following HPCC Platform coding style:
- Use `LINK()`/`Release()` for reference counting
- Use `StringBuffer` for string building
- Use `CIArrayOf<>` for collections of CInterface objects
- Follow error handling patterns from existing code
- Add comprehensive logging via DBGLOG

#### Key Implementation Details

**Finding Subgraphs**:
```cpp
void WorkunitQueueAnalyser::gatherSubgraphUsages()
{
    // Iterate through root->scopes recursively
    // Filter for queryScopeType() == SSTsubgraph
    // For each subgraph, create SubgraphResourceUsage and extract stats
}
```

**Extracting Statistics**:
```cpp
void SubgraphResourceUsage::extractStatistics()
{
    // Use subgraph->getStatRaw(StatisticKind) for each needed stat
    // TODO: Identify correct StatisticKind enum values
    // Example: peakRowMemory = subgraph->getStatRaw(StSizePeakMemory);
}
```

**Time Estimation** (implement algorithm from above):
```cpp
stat_type WorkunitQueueAnalyser::estimateSubgraphTime(
    const SubgraphResourceUsage & usage,
    const QueueConfiguration & queue,
    bool & willSpill) const
{
    // Implement the algorithm described in requirements
    // Scale memory by worker ratio
    // Determine if spilling will occur
    // Apply penalty if newly spilling
    // Scale CPU time, add blocked time
    // Return estimated elapsed time
}
```

**Report Generation**:
```cpp
void WorkunitQueueAnalyser::printReport() const
{
    // Print formatted table with:
    // - Queue name, worker count
    // - Estimated time and cost
    // - Spill indication
    // - Recommendation
    // Include comparison to actual execution
    // Suggest alternatives (faster but more expensive, etc.)
}
```

### 3. Add API Functions to `anawu.hpp/cpp`

```cpp
// In anawu.hpp (add at end with other API functions)
void WUANALYSIS_API analyseQueueRecommendation(
    IConstWorkUnit * wu,
    IPropertyTree * queueConfig,
    IPropertyTree * options);

// In anawu.cpp
void WUANALYSIS_API analyseQueueRecommendation(
    IConstWorkUnit * wu,
    IPropertyTree * queueConfig, 
    IPropertyTree * options)
{
    WorkunitQueueAnalyser analyser;
    
    // Load queue configurations from queueConfig IPropertyTree
    // Expected format: <Queues><Queue name="..."><MaxRowMemory>...</...>
    Owned<IPropertyTreeIterator> queues = queueConfig->getElements("Queue");
    ForEach(*queues)
    {
        IPropertyTree & queue = queues->query();
        // Parse configuration and create QueueConfiguration
        // Add to analyser
    }
    
    // Get spill penalty from options (default 10.0)
    double spillPenalty = options ? options->getPropReal("spillPenaltyFactor", 10.0) : 10.0;
    analyser.setSpillPenaltyFactor(spillPenalty);
    
    // Run analysis
    analyser.analyse(wu, nullptr);
    analyser.analyseQueues(wu);
    
    // Output results
    analyser.printReport();
}
```

### 4. Configuration Loading

Implement loading queue configurations from IPropertyTree with this expected XML structure:

```xml
<Queues>
  <Queue name="small">
    <MaxRowMemory>4294967296</MaxRowMemory>  <!-- 4GB in bytes -->
    <MaxTempDiskMemory>10737418240</MaxTempDiskMemory>  <!-- 10GB -->
    <NumCpus>8</NumCpus>
    <NumWorkers>10</NumWorkers>
    <VMCostPerHour>2.50</VMCostPerHour>
  </Queue>
  <Queue name="medium">
    <MaxRowMemory>17179869184</MaxRowMemory>  <!-- 16GB -->
    <MaxTempDiskMemory>53687091200</MaxTempDiskMemory>  <!-- 50GB -->
    <NumCpus>16</NumCpus>
    <NumWorkers>40</NumWorkers>
    <VMCostPerHour>10.00</VMCostPerHour>
  </Queue>
</Queues>
```

Handle parsing with proper error checking and defaults.

### 5. Error Handling

- Validate queue configurations (positive values, reasonable ranges)
- Handle missing statistics gracefully
- Warn if critical statistics are unavailable
- Don't fail if some subgraphs lack complete stats
- Log warnings for queues that can't handle the workload

### 6. Output Format

Generate a report similar to:

```
Queue Recommendation Analysis
Workunit: W20231115-120000
Actual: queue=medium, workers=40, time=1234.56s, cost=$12.34

Candidate Queues:
Queue    Workers  Est.Time(s)  Est.Cost($)  Spills  Status
------------------------------------------------------------
small         10      2000.00         5.56     Yes   Too small
medium        40      1234.56        12.34      No   Best match
large        100       800.00        32.00      No   Faster/costlier

Recommendation: medium (current queue)
Cost-effective: Estimated $12.34 vs actual $12.34 (0% difference)

Alternative: 'large' queue would be 35% faster but cost 159% more
```

## Critical TODOs / Information Gaps

Before full implementation, you MUST research and document:

1. **Statistic Kind Enums**: Find the actual enum values in the codebase for:
   - Peak row memory (search for "Memory", "Peak" in jstats.h or workunit headers)
   - Peak temp disk (search for "Disk", "Temp", "Spill")
   - Peak ephemeral disk
   - System CPU time (search for "System", "Kernel")
   - Verify TimeUser and TimeElapsed enum names

2. **Worker Count**: How to get actual worker count from workunit?
   - Search for statistics about cluster size or workers
   - May be in workunit attributes rather than statistics

3. **Statistics Availability**: 
   - Are these stats at subgraph or activity level?
   - How to aggregate if at activity level?
   - What to do if statistics are missing?

4. **Configuration Source**:
   - Where should default queue configs come from?
   - Hard-code for now or require external config?
   - Add placeholder comments for configuration loading

## Implementation Guidance

### Phase 1: Skeleton (Do This First)
1. Create header file with all class declarations
2. Add placeholder comments: `// TODO: [Description of what's needed]`
3. Implement constructors and basic getters/setters
4. Get code to compile (empty methods are OK)

### Phase 2: Statistics (Critical)
1. Research actual StatisticKind enum values
2. Implement SubgraphResourceUsage::extractStatistics()
3. Test with a real workunit to verify stats are extracted
4. Document which statistics are available and which are not

### Phase 3: Algorithm
1. Implement estimateSubgraphTime() with full algorithm
2. Add unit tests for time estimation
3. Implement estimateForQueue()
4. Test with various scenarios (spill/no-spill, different worker counts)

### Phase 4: Integration  
1. Implement gatherSubgraphUsages()
2. Implement analyseQueues()
3. Add API function
4. Test end-to-end with real workunit

### Phase 5: Reporting
1. Implement printReport()
2. Add formatting and tables
3. Include comparisons and recommendations
4. Test output readability

## Code Style Requirements

Follow HPCC Platform conventions:
- Use `WUANALYSIS_API` macro for exported functions
- Use `CInterface` base class with reference counting
- Use `Linked<T>` and `Owned<T>` for automatic reference management
- Use `CIArrayOf<T>` for collections
- Use `StringBuffer` for string building
- Use `ForEach()` macros for iteration
- Add DBGLOG for debugging output
- Use `assertex()` for invariant checks
- Follow existing naming conventions (camelCase for methods, member variables)

## Testing Approach

Create test cases in `testing/wuanalysis/` directory:
1. Test with synthetic workunit data
2. Test estimation accuracy vs. real executions
3. Test edge cases (zero workers, no spills, all spills, missing stats)
4. Validate cost calculations
5. Test configuration parsing

## Example Usage

After implementation, usage would look like:

```cpp
// In wutool or other analysis tool
Owned<IConstWorkUnit> wu = getWorkunit("W20231115-120000");
Owned<IPropertyTree> queueConfig = createPTreeFromXMLFile("queues.xml");
Owned<IPropertyTree> options = createPTree();
options->setPropReal("spillPenaltyFactor", 10.0);

analyseQueueRecommendation(wu, queueConfig, options);
```

## Documentation Requirements

Add comprehensive documentation:
1. Class and method doc comments (doxygen style)
2. Algorithm explanation in code comments  
3. TODO comments for information gaps
4. Example usage in header file comments
5. README.md in common/wuanalysis explaining the feature

## Success Criteria

Implementation is complete when:
- [ ] Code compiles without errors
- [ ] All classes implemented with documented methods
- [ ] Can extract statistics from a real workunit
- [ ] Time estimation algorithm works correctly
- [ ] Cost calculation produces reasonable results
- [ ] Report generation produces readable output
- [ ] TODOs are documented for missing information
- [ ] Code follows HPCC Platform style guidelines
- [ ] Basic testing passes

## Notes for Implementer

- **Start simple**: Get basic structure working first, add sophistication later
- **Document unknowns**: Use TODO comments extensively where information is missing
- **Test incrementally**: Test each component as you build it
- **Ask for clarification**: If StatisticKind enums can't be found, document what you tried
- **Placeholder values**: Use reasonable defaults where configuration is unclear
- **Error handling**: Fail gracefully when statistics are missing

This is a complex feature. Focus on getting a working skeleton with clear TODOs, then progressively fill in the implementation as information becomes available.

## References

Study these existing files for patterns:
- `common/wuanalysis/anawu.cpp`: WorkunitRuleAnalyser, WorkunitStatsAnalyser
- `common/wuanalysis/anarule.cpp`: PerformanceIssue, rule checking patterns
- `system/jlib/jstats.h`: Statistics types and helper functions
- `common/workunit/workunit.hpp`: Workunit interfaces

Good luck with the implementation!
