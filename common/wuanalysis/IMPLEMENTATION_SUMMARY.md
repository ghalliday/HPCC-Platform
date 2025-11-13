# Thor Queue Recommendation - Implementation Summary

## What Was Delivered

This deliverable provides comprehensive planning documentation for implementing a Thor queue recommendation feature in the HPCC Platform. As requested in the problem statement: **"Do not implement the code. Instead create a detailed plan of how you would implement this functionality, and generate a prompt as the output that would be suitable for supplying to the github copilot to implement the functionality."**

## Delivered Artifacts

### 1. QUEUE_RECOMMENDATION_PLAN.md
**Comprehensive implementation plan including:**

- **Architecture Overview**: How the feature fits into existing `common/wuanalysis/` infrastructure
- **Detailed Design**: Complete class specifications for all components
  - `QueueConfiguration` - Stores queue resource limits and costs
  - `SubgraphResourceUsage` - Extracts and represents actual resource usage
  - `QueueEstimation` - Holds estimated performance for a candidate queue
  - `WorkunitQueueAnalyser` - Main analysis orchestrator
  
- **Algorithm Details**: Step-by-step time estimation and cost calculation algorithms with pseudocode
- **Configuration Loading**: Proposed XML format for queue definitions
- **API Design**: New public functions to integrate with existing analysis framework
- **Output Format**: Detailed report format with example output
- **Implementation Phases**: Breaking down the work into manageable chunks
- **Testing Strategy**: Unit, integration, and validation testing approaches
- **Open Questions**: 7 critical information gaps that need resolution before implementation

### 2. GITHUB_COPILOT_PROMPT.md
**Comprehensive prompt for GitHub Copilot including:**

- **Context Setting**: Background on HPCC Platform and existing codebase
- **Detailed Requirements**: Precise specification of what to build
- **Implementation Tasks**: 6 major task areas with specific guidance for each
- **Code Templates**: Class skeletons with method signatures and usage patterns
- **Algorithm Pseudocode**: Step-by-step time estimation logic
- **Critical TODOs**: Clear placeholders for missing information (StatisticKind enums, worker counts, etc.)
- **Code Style Requirements**: HPCC Platform conventions to follow
- **Testing Approach**: How to validate the implementation
- **Success Criteria**: Checklist of completion requirements
- **Example Usage**: How the feature will be used once implemented
- **Implementation Phases**: Suggested build order from skeleton to complete feature

### 3. Key Features of the Design

#### Smart Resource Scaling
The algorithm accounts for different worker counts across queues:
- Memory usage scales proportionally with workers
- CPU time scales inversely with workers  
- Blocked time (I/O, network) remains constant
- Spill behavior changes based on available memory

#### Cost-Aware Recommendations
- Uses existing `calcCostNs()` cost calculation functions
- Factors in worker count and VM costs per hour
- Provides cost vs. performance trade-off analysis
- Identifies "best value" vs "fastest" options

#### Robust Handling of Unknown Information
The design explicitly identifies 7 critical information gaps:
1. Exact StatisticKind enum values for Thor statistics
2. Queue configuration source and format
3. How to determine actual worker count
4. Memory unit conventions
5. Spill detection mechanisms
6. Integration points and triggers
7. Result storage location

Each gap is documented with:
- What information is needed
- Where to look for it
- Placeholder approaches to use until resolved

#### Integration with Existing Infrastructure
- Extends `WorkunitAnalyserBase` class
- Uses existing `WuScope` for statistics extraction
- Follows patterns from `WorkunitRuleAnalyser` and `WorkunitStatsAnalyser`
- Integrates with existing API functions in `anawu.hpp/cpp`
- Compatible with wutool command-line interface

## How to Use These Documents

### For Developers Implementing the Feature

1. **Read QUEUE_RECOMMENDATION_PLAN.md first** to understand the complete design
2. **Review the open questions section** and resolve information gaps before coding
3. **Use GITHUB_COPILOT_PROMPT.md** as input to GitHub Copilot or as implementation guide
4. **Follow the phased approach** - build skeleton first, then fill in details
5. **Document additional TODOs** discovered during implementation

### For Code Reviewers

1. Check implementation against the design specifications
2. Verify all critical TODOs are addressed or documented
3. Ensure algorithm matches the pseudocode
4. Validate cost calculations use correct formulas
5. Check adherence to HPCC Platform coding conventions

### For Project Managers

1. Use implementation phases for sprint planning
2. Reference open questions for dependency tracking
3. Use success criteria for acceptance testing
4. Plan for configuration management (queue definitions)
5. Consider user documentation needs

## Next Steps

### Before Implementation Begins

1. **Resolve Information Gaps**: Research and document answers to the 7 open questions
   - Most critical: Identify correct StatisticKind enum values
   - Determine queue configuration source
   - Find how to get actual worker count

2. **Set Up Configuration**: Decide on queue configuration approach
   - Configuration file format and location
   - Default queue definitions for testing
   - Configuration validation requirements

3. **Plan Testing**: Identify test workunits
   - Need workunits with known execution characteristics
   - Various resource usage patterns (memory-heavy, CPU-heavy, I/O-bound)
   - Both with and without spilling

### During Implementation

1. **Phase 1 (Week 1)**: Build skeleton with all classes and placeholder TODOs
2. **Phase 2 (Week 1-2)**: Research and implement statistics extraction
3. **Phase 3 (Week 2-3)**: Implement core estimation algorithm
4. **Phase 4 (Week 3)**: Add configuration loading and API integration
5. **Phase 5 (Week 4)**: Implement reporting and polish

### After Implementation

1. **Validation**: Test estimates against real workunit executions
2. **Calibration**: Tune spill penalty factor based on real data
3. **Documentation**: Create user guide and API documentation
4. **Integration**: Add to workunit completion workflow (optional)
5. **UI Enhancement**: Consider ECLWatch integration (future work)

## Design Decisions and Rationale

### Why Subgraph-Level Analysis?
- Problem statement specifies subgraphs as the unit of analysis
- Subgraphs capture meaningful resource usage patterns
- Aggregation to job level is straightforward
- Aligns with Thor's execution model

### Why Configurable Spill Penalty?
- Actual penalty varies by workload type
- Default of 10x is conservative but reasonable
- Allows tuning based on empirical data
- Documented as configuration option

### Why Cost-Based Recommendation?
- Primary goal is efficiency (cost-effectiveness)
- Time vs. cost trade-off is valuable information
- Aligns with cloud economics
- Supports budget-conscious decision making

### Why Separate Classes for Each Concept?
- Clear separation of concerns
- Easier to test individual components
- Follows existing patterns in wuanalysis
- Supports future enhancements

## Potential Challenges and Mitigations

### Challenge 1: Missing Statistics
**Risk**: Required statistics may not be recorded by Thor
**Mitigation**: Design includes graceful degradation; document what's available

### Challenge 2: Estimation Accuracy
**Risk**: Simple linear scaling may not match reality
**Mitigation**: Make algorithm tunable; validate against real data; iterate

### Challenge 3: Configuration Management
**Risk**: Queue definitions may be hard to maintain
**Mitigation**: Flexible configuration format; validation; defaults

### Challenge 4: Integration Complexity  
**Risk**: Integration with existing analysis may be challenging
**Mitigation**: Design extends existing classes; follows established patterns

## Success Metrics

The implementation will be successful if:

1. **Functional**: Can analyze any completed Thor workunit
2. **Accurate**: Estimates within 20% of actual for typical jobs
3. **Useful**: Identifies genuinely better queue choices when they exist
4. **Performant**: Analysis completes in < 5 seconds for typical workunits
5. **Maintainable**: Code is clear, well-documented, and testable
6. **Integrated**: Works seamlessly with existing wuanalysis tools

## References for Implementation

### Existing Code to Study
- `common/wuanalysis/anawu.cpp` - Pattern for analyzer classes
- `common/wuanalysis/anarule.cpp` - Pattern for issue detection and reporting
- `system/jlib/jstats.h` - Statistics types and cost calculation
- `common/workunit/workunit.cpp` - Workunit interface and Thor cost calculation

### Key Interfaces
- `IConstWorkUnit` - Accessing workunit data
- `IPropertyTree` - Configuration and result storage
- `WuScope` - Extracting statistics via `getStatRaw()`
- `IStatisticGatherer` - Collecting statistics

### Helpful Functions
- `calcCostNs(ratePerHour, nanoseconds)` - Cost calculation
- `money2cost_type()` / `cost_type2money()` - Currency conversion
- `formatStatistic()` - Formatting statistics for display

## Conclusion

This planning package provides everything needed to implement the Thor queue recommendation feature:

✅ Complete architectural design  
✅ Detailed class specifications  
✅ Step-by-step algorithms  
✅ Integration strategy  
✅ GitHub Copilot prompt  
✅ Testing approach  
✅ Clear identification of unknowns  

The design is practical, follows HPCC Platform conventions, and can be implemented incrementally. With the information gaps resolved, implementation can proceed with confidence.

---

**Document Version**: 1.0  
**Date**: 2024  
**Status**: Ready for Implementation (pending information gap resolution)
