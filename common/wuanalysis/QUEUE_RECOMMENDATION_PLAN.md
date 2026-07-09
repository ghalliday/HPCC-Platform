# Thor Queue Recommendation Analysis - Implementation Plan

## Overview
This document provides a detailed implementation plan for adding queue recommendation analysis functionality to the HPCC Platform's workunit analysis system. The goal is to analyze completed workunits and determine which Thor queue configuration would have been most efficient for execution.

## Problem Statement

When a workunit executes in Thor, the costs of running the job are closely linked to the resources that the underlying processes have available. The system allows multiple different Thor configurations (or queues), and ideally a job should target the queue that uses the minimum resources that can efficiently process a job.

### Queue Configuration Constraints

Each Thor queue has the following constraints:
- **Maximum memory available for rows** - memory limit for processing data
- **Maximum memory available for temp disk** - memory before spilling to disk
- **Number of CPUs** - processing power available
- **Number of workers** - parallelism level

### Subgraph Statistics Available

Each subgraph in a Thor job records the following statistics:
- **SizePeakRowMemory** - Maximum memory required to run the subgraph
- **SizePeakTempDisk** - Amount of data spilled to disk (0 if no spilling occurred)
- **SizePeakEphemeralDisk** - Maximum local disk space consumed by temporary files
- **TimeUser** - Amount of userspace CPU time consumed
- **TimeSystem** - Amount of kernel/system CPU time consumed
- **TimeElapsed** - Total wall-clock time for the subgraph

### Key Calculations

Average CPU utilization: `(TimeSystem + TimeUser) / TimeElapsed`

## Architecture Overview

### Location in Codebase
- **Directory**: `common/wuanalysis/`
- **New Files to Create**:
  - `queuerecommender.hpp` - Header file with class definitions and interfaces
  - `queuerecommender.cpp` - Implementation of queue recommendation logic
  - Unit tests (if test infrastructure exists)

### Integration Points
- Extend `WorkunitAnalyserBase` or create a new analyzer class
- Integrate with existing analysis functions in `anawu.hpp/cpp`
- Add new API function: `analyseQueueRecommendation()`

## Detailed Design

### 1. Data Structures

#### QueueConfiguration Class
Represents a single Thor queue configuration with its resource constraints.

```cpp
class QueueConfiguration
{
public:
    QueueConfiguration(const char * name, 
                      stat_type maxRowMemory, 
                      stat_type maxTempDiskMemory,
                      unsigned numCpus,
                      unsigned numWorkers,
                      double vmCostPerHour);
    
    // Getters
    const char * queryName() const;
    stat_type queryMaxRowMemory() const;
    stat_type queryMaxTempDiskMemory() const;
    unsigned queryNumCpus() const;
    unsigned queryNumWorkers() const;
    double queryCostPerHour() const;
    
    // Setters (for configuration loading)
    void setMaxRowMemory(stat_type memory);
    void setMaxTempDiskMemory(stat_type memory);
    void setNumCpus(unsigned cpus);
    void setNumWorkers(unsigned workers);
    void setVMCostPerHour(double cost);
    
private:
    StringAttr name;
    stat_type maxRowMemory;
    stat_type maxTempDiskMemory;
    unsigned numCpus;
    unsigned numWorkers;
    double vmCostPerHour;  // Cost per hour for this queue configuration
};
```

**Implementation Notes**:
- Store memory values in bytes (stat_type)
- Cost should be in dollars per hour (double)
- Name should identify the queue uniquely

#### SubgraphResourceUsage Class
Captures the resource usage statistics from a completed subgraph execution.

```cpp
class SubgraphResourceUsage
{
public:
    SubgraphResourceUsage(WuScope * subgraph, unsigned actualWorkers);
    
    // Extract statistics from the WuScope
    void extractStatistics();
    
    // Getters for actual resource usage
    stat_type queryPeakRowMemory() const;
    stat_type queryPeakTempDisk() const;
    stat_type queryPeakEphemeralDisk() const;
    stat_type queryTimeUser() const;
    stat_type queryTimeSystem() const;
    stat_type queryTimeElapsed() const;
    bool didSpill() const;
    
    // Calculated values
    double queryCpuUtilization() const;
    stat_type queryCpuTime() const;  // TimeUser + TimeSystem
    stat_type queryBlockedTime() const;  // TimeElapsed - CpuTime (if positive, else 0)
    
private:
    Linked<WuScope> subgraph;
    unsigned actualWorkers;
    
    // Extracted statistics (TODO: Determine the exact StatisticKind enums to use)
    // These need to be mapped to the actual statistics available in Thor
    stat_type peakRowMemory;      // StatisticKind: StSizePeakRowMemory (?)
    stat_type peakTempDisk;       // StatisticKind: StSizePeakTempDisk (?)
    stat_type peakEphemeralDisk;  // StatisticKind: StSizePeakEphemeralDisk (?)
    stat_type timeUser;           // StatisticKind: StTimeUser
    stat_type timeSystem;         // StatisticKind: StTimeSystem (?)
    stat_type timeElapsed;        // StatisticKind: StTimeElapsed
};
```

**TODO - Information Needed**:
1. What are the exact `StatisticKind` enum values for these statistics?
   - `StSizePeakRowMemory` - does this exist or use different name?
   - `StSizePeakTempDisk` - does this exist or use different name?
   - `StSizePeakEphemeralDisk` - does this exist or use different name?
   - `StTimeSystem` - does this exist or use different name?
2. Are these statistics recorded at the subgraph level or activity level?
3. How to aggregate if statistics are at activity level?

#### QueueEstimation Class
Represents the estimated performance and cost for running on a specific queue.

```cpp
class QueueEstimation
{
public:
    QueueEstimation(const QueueConfiguration * queue);
    
    // Add a subgraph estimation
    void addSubgraphEstimate(const SubgraphResourceUsage & usage, 
                            stat_type estimatedElapsed,
                            bool willSpill);
    
    // Calculate final metrics
    void finalize();
    
    // Getters
    const char * queryQueueName() const;
    stat_type queryEstimatedElapsedTime() const;
    cost_type queryEstimatedCost() const;
    bool queryWillFit() const;  // Can this queue handle the job?
    unsigned queryNumSpills() const;  // How many subgraphs will spill?
    
    // Comparison for sorting
    int compareCost(const QueueEstimation & other) const;
    int compareTime(const QueueEstimation & other) const;
    
private:
    Linked<const QueueConfiguration> queue;
    stat_type totalEstimatedElapsed;
    cost_type estimatedCost;
    bool willFit;
    unsigned numSpills;
    std::vector<stat_type> subgraphEstimates;
};
```

**Implementation Notes**:
- Use `calcCost()` from `jstats.h` for cost calculation
- Formula: `cost = calcCostNs(vmCostPerHour, elapsedTimeNs) * numWorkers`
- Mark as "will not fit" if peak memory exceeds queue limits

### 2. Core Algorithm Class

#### WorkunitQueueAnalyser Class
Main class that performs the queue recommendation analysis.

```cpp
class WorkunitQueueAnalyser : public WorkunitAnalyserBase
{
public:
    WorkunitQueueAnalyser();
    
    // Configuration
    void addQueueConfiguration(QueueConfiguration * queue);
    void setSpillPenaltyFactor(double factor);  // Default: 10.0
    void setActualWorkerCount(unsigned workers);
    
    // Analysis execution
    void analyseQueues(IConstWorkUnit * wu);
    
    // Results
    const QueueEstimation * queryBestQueue() const;  // Lowest cost that fits
    void getRecommendations(CIArrayOf<QueueEstimation> & results) const;
    void printReport() const;
    void addReportToWorkunit(IWorkUnit * wu) const;
    
protected:
    // Internal methods
    void gatherSubgraphUsages();
    void estimateForQueue(const QueueConfiguration & queue, QueueEstimation & estimation);
    stat_type estimateSubgraphTime(const SubgraphResourceUsage & usage, 
                                  const QueueConfiguration & queue,
                                  bool & willSpill) const;
    
private:
    CIArrayOf<QueueConfiguration> queueConfigurations;
    CIArrayOf<SubgraphResourceUsage> subgraphUsages;
    CIArrayOf<QueueEstimation> estimations;
    double spillPenaltyFactor;
    unsigned actualWorkers;
};
```

### 3. Estimation Algorithm Details

#### Time Estimation for Each Subgraph

For each subgraph, estimate the execution time on a candidate queue:

```cpp
stat_type estimateSubgraphTime(
    const SubgraphResourceUsage & usage,
    const QueueConfiguration & queue,
    bool & willSpill) const
{
    unsigned actualWorkers = usage.queryActualWorkers();
    unsigned candidateWorkers = queue.queryNumWorkers();
    
    // Scale memory usage based on worker count
    stat_type scaledPeakMemory = usage.queryPeakRowMemory() * candidateWorkers / actualWorkers;
    
    // Determine if this subgraph will spill on the candidate queue
    bool originallySpilled = usage.didSpill();
    bool willSpillOnCandidate = (scaledPeakMemory > queue.queryMaxRowMemory());
    
    stat_type baseElapsed = usage.queryTimeElapsed();
    stat_type estimatedElapsed;
    
    if (scaledPeakMemory <= queue.queryMaxRowMemory() && !originallySpilled) {
        // Memory fits and didn't spill originally - time stays the same
        estimatedElapsed = baseElapsed;
    }
    else if (originallySpilled) {
        // Already spilled in original run - time stays the same
        estimatedElapsed = baseElapsed;
    }
    else {
        // Will spill on candidate queue but didn't originally
        // Apply penalty factor
        estimatedElapsed = baseElapsed * spillPenaltyFactor;
        willSpillOnCandidate = true;
    }
    
    // Scale elapsed time based on CPU utilization and worker count
    stat_type cpuTime = usage.queryCpuTime();
    stat_type blockedTime = usage.queryBlockedTime();
    
    // EstimatedCpuTime = cpuTime * candidateWorkers / actualWorkers
    stat_type estimatedCpuTime = cpuTime * candidateWorkers / actualWorkers;
    
    // Final estimated time = EstimatedCpuTime + BlockedTime
    stat_type finalEstimated = estimatedCpuTime + blockedTime;
    
    // If we had to apply spill penalty, use the larger of scaled time or penalty time
    if (willSpillOnCandidate && !originallySpilled) {
        finalEstimated = std::max(finalEstimated, estimatedElapsed);
    }
    else {
        finalEstimated = estimatedElapsed;  // Use the CPU-scaled time
    }
    
    willSpill = willSpillOnCandidate;
    return finalEstimated;
}
```

**Algorithm Summary**:
1. Scale memory usage by worker ratio
2. Check if subgraph will spill
3. Apply spill penalty if newly spilling
4. Scale CPU time by worker ratio
5. Add blocked time (unchanged)
6. Return maximum of scaled times

#### Cost Calculation

```cpp
cost_type calculateQueueCost(const QueueConfiguration & queue, stat_type elapsedTimeNs)
{
    // Use existing calcCostNs function from jstats.h
    double costInDollars = calcCostNs(queue.queryCostPerHour(), elapsedTimeNs) * queue.queryNumWorkers();
    return money2cost_type(costInDollars);
}
```

### 4. Configuration Loading

#### QueueConfiguration Source

**TODO - Information Needed**:
Where should queue configurations come from?
- Option 1: Configuration file (e.g., `queues.yaml` or `queues.xml`)
- Option 2: Dali/Environment configuration
- Option 3: Passed as parameter to analysis function
- Option 4: Hard-coded defaults for testing

Suggested structure for configuration file:
```xml
<ThorQueues>
  <Queue name="small">
    <MaxRowMemory>4GB</MaxRowMemory>
    <MaxTempDiskMemory>10GB</MaxTempDiskMemory>
    <NumCpus>8</NumCpus>
    <NumWorkers>10</NumWorkers>
    <VMCostPerHour>2.50</VMCostPerHour>
  </Queue>
  <Queue name="medium">
    <MaxRowMemory>16GB</MaxRowMemory>
    <MaxTempDiskMemory>50GB</MaxTempDiskMemory>
    <NumCpus>16</NumCpus>
    <NumWorkers>40</NumWorkers>
    <VMCostPerHour>10.00</VMCostPerHour>
  </Queue>
  <Queue name="large">
    <MaxRowMemory>64GB</MaxRowMemory>
    <MaxTempDiskMemory>200GB</MaxTempDiskMemory>
    <NumCpus>32</NumCpus>
    <NumWorkers>100</NumWorkers>
    <VMCostPerHour>40.00</VMCostPerHour>
  </Queue>
</ThorQueues>
```

### 5. API Functions

Add to `anawu.hpp`:

```cpp
// Analyze queue recommendations for a completed workunit
void WUANALYSIS_API analyseQueueRecommendation(
    IConstWorkUnit * wu, 
    IPropertyTree * queueConfig,
    IPropertyTree * options);

// Get queue recommendations and return structured results
void WUANALYSIS_API getQueueRecommendations(
    QueueRecommendationResults & results,
    IConstWorkUnit * wu,
    IPropertyTree * queueConfig, 
    IPropertyTree * options);
```

### 6. Output Report Format

The analysis should produce a report showing:

```
Queue Recommendation Analysis for Workunit: W20231115-120000
Actual Execution: queue=medium, workers=40, elapsed=1234.56s, cost=$12.34

Estimated Performance on Available Queues:
+---------+---------+---------------+----------+---------+----------+
| Queue   | Workers | Estimated     | Est.     | Will    | Recommend|
|         |         | Time (s)      | Cost ($) | Spill   |          |
+---------+---------+---------------+----------+---------+----------+
| small   |   10    |    2000.00    |   5.56   |   Yes   |          |
| medium  |   40    |    1234.56    |  12.34   |   No    | Best Cost|
| large   |  100    |     800.00    |  32.00   |   No    |          |
+---------+---------+---------------+----------+---------+----------+

Recommendation: Run on 'medium' queue (current queue)
- Estimated cost: $12.34 (actual: $12.34)
- Estimated time: 1234.56s (actual: 1234.56s)
- No spilling expected

Alternative: 'large' queue would complete 35% faster but cost 159% more

Subgraph Analysis:
  sg1: 456.78s on medium (40 workers) -> 182.71s on large (100 workers)
  sg2: 777.78s on medium (40 workers) -> 617.29s on large (100 workers)
  ...
```

### 7. Integration with Existing Analysis

The queue recommendation analysis should be callable:
1. Standalone via wutool command
2. Automatically after workunit completion (optional)
3. Via ECLWatch UI (future enhancement)

Example wutool usage:
```bash
wutool queuerecommend W20231115-120000 daliserver=192.168.1.100 queueconfig=/etc/HPCCSystems/queues.xml
```

## Implementation Steps

### Phase 1: Core Data Structures
1. Create `queuerecommender.hpp` with all class definitions
2. Implement `QueueConfiguration` class
3. Implement `SubgraphResourceUsage` class
4. Add comprehensive documentation comments

### Phase 2: Algorithm Implementation
1. Implement `WorkunitQueueAnalyser` constructor and configuration methods
2. Implement `gatherSubgraphUsages()` method
3. Implement `estimateSubgraphTime()` algorithm
4. Implement `estimateForQueue()` method
5. Add unit tests for time estimation logic

### Phase 3: Configuration and Integration
1. Implement configuration file loading
2. Add API functions to `anawu.hpp/cpp`
3. Integrate with existing workunit analysis infrastructure
4. Add command-line support via wutool

### Phase 4: Reporting and Output
1. Implement `printReport()` method
2. Implement `addReportToWorkunit()` method
3. Add structured output support (JSON/XML)
4. Document usage examples

### Phase 5: Testing and Validation
1. Create test cases with known workunit statistics
2. Validate estimation accuracy against real workunit data
3. Test with various queue configurations
4. Performance testing for large workunits

## Open Questions / Information Needed

### Critical Information Gaps

1. **Statistics Naming**:
   - What are the exact `StatisticKind` enum values for:
     - Peak row memory usage
     - Peak temp disk usage
     - Peak ephemeral disk usage
     - System/kernel CPU time
   - Are these recorded at subgraph or activity level?

2. **Queue Configuration Source**:
   - Where should queue configurations be stored?
   - How should they be accessed at runtime?
   - Who maintains the configuration?

3. **Worker Count Information**:
   - How to determine the actual number of workers used?
   - Is this stored in workunit statistics?
   - StatisticKind enum value?

4. **Cost Calculation**:
   - Should we use existing `calculateThorCost()` function?
   - Does it need modification for this use case?
   - Are there separate manager/worker costs to consider?

5. **Memory Units**:
   - Are memory statistics stored in bytes?
   - Any conversion needed?

6. **Spill Detection**:
   - How to detect if spilling occurred?
   - Is `SizePeakTempDisk > 0` sufficient?
   - Are there other indicators?

7. **Integration Points**:
   - Should this run automatically after workunit completion?
   - Should it be opt-in via configuration?
   - Where should results be stored (workunit attributes, separate DB, etc.)?

## Testing Strategy

### Unit Tests
- Test time estimation algorithm with various scenarios
- Test memory scaling logic
- Test spill detection
- Test cost calculation

### Integration Tests
- Test with real workunit data
- Validate against known execution times
- Test configuration loading
- Test report generation

### Validation Tests
- Compare estimates vs actual for various workunits
- Measure estimation accuracy
- Identify edge cases where estimation fails

## Documentation Requirements

1. **Code Documentation**:
   - Comprehensive class and method documentation
   - Algorithm explanation comments
   - Example usage in code comments

2. **User Documentation**:
   - How to configure queue definitions
   - How to run queue recommendation analysis
   - How to interpret results
   - Wutool command reference

3. **Developer Documentation**:
   - Architecture overview
   - Extension points for future enhancements
   - Testing guidelines

## Future Enhancements (Out of Scope)

1. Machine learning model for better time prediction
2. Historical analysis to improve estimates
3. Real-time queue recommendation during submission
4. Automatic queue selection based on ECL code analysis
5. Integration with workunit scheduler for automatic routing
6. Cost optimization suggestions
7. Multi-dimensional optimization (cost vs time vs resource usage)

## Summary

This plan provides a comprehensive approach to implementing queue recommendation analysis in the HPCC Platform. The implementation should be modular, well-documented, and integrate cleanly with existing workunit analysis infrastructure. The main challenges are identifying the correct statistics to use and determining the source of queue configuration data.
