# Thor Queue Recommendation Feature - Documentation Index

## Overview

This directory contains comprehensive planning documentation for implementing a Thor queue recommendation feature in the HPCC Platform. The feature will analyze completed workunits to determine which Thor queue configuration would have been most cost-effective.

## Purpose

As stated in the requirements: **"Do not implement the code. Instead create a detailed plan of how you would implement this functionality, and generate a prompt as the output that would be suitable for supplying to the github copilot to implement the functionality."**

This documentation fulfills that requirement by providing:
- Complete architectural design
- Detailed implementation plan
- GitHub Copilot-ready prompt
- Clear identification of information gaps

## Document Index

### 📋 [IMPLEMENTATION_SUMMARY.md](./IMPLEMENTATION_SUMMARY.md)
**Start here** - Executive summary and quick reference

**Contents:**
- What was delivered
- How to use these documents
- Next steps and timeline
- Design decisions and rationale
- Success metrics

**Audience:** Project managers, technical leads, code reviewers

---

### 📐 [QUEUE_RECOMMENDATION_PLAN.md](./QUEUE_RECOMMENDATION_PLAN.md)
**Complete technical design** - Deep dive into architecture and algorithms

**Contents:**
- Architecture overview and integration points
- Detailed class specifications with method signatures
- Time estimation algorithm (pseudocode and explanation)
- Cost calculation formulas
- Configuration format (XML examples)
- API design
- Testing strategy
- Implementation phases
- 7 documented information gaps with resolution guidance

**Audience:** Software engineers, architects, implementers

**Key Sections:**
- Section 1: Data Structures (4 classes)
- Section 2: Core Algorithm Class (WorkunitQueueAnalyser)
- Section 3: Estimation Algorithm Details
- Section 4: Configuration Loading
- Section 5: API Functions
- Section 6: Output Report Format
- Section 7: Integration

---

### 🤖 [GITHUB_COPILOT_PROMPT.md](./GITHUB_COPILOT_PROMPT.md)
**Ready-to-use implementation guide** - Paste this into GitHub Copilot

**Contents:**
- Context setting and background
- Detailed requirements
- Implementation tasks (6 phases)
- Code templates and class skeletons
- Algorithm implementation guidance
- Critical TODOs with placeholders
- Code style requirements
- Testing approach
- Success criteria checklist

**Audience:** Developers implementing the feature, GitHub Copilot

**How to Use:**
1. Resolve critical TODOs (StatisticKind enums, worker counts)
2. Copy relevant sections to GitHub Copilot
3. Follow the phased implementation approach
4. Use code templates as starting point
5. Refer back to QUEUE_RECOMMENDATION_PLAN.md for details

---

## Quick Start Guide

### For Implementers

1. **Read in order:**
   - IMPLEMENTATION_SUMMARY.md (15 minutes)
   - QUEUE_RECOMMENDATION_PLAN.md (45 minutes)
   - GITHUB_COPILOT_PROMPT.md (30 minutes)

2. **Before coding:**
   - Resolve the 7 critical information gaps
   - Set up test environment with sample workunits
   - Create skeleton branch

3. **During implementation:**
   - Follow phased approach from GITHUB_COPILOT_PROMPT.md
   - Use code templates as starting points
   - Document additional TODOs discovered
   - Test incrementally after each phase

### For Reviewers

1. **Design Review:**
   - Review QUEUE_RECOMMENDATION_PLAN.md architecture section
   - Validate algorithm correctness
   - Check integration points

2. **Code Review:**
   - Compare implementation against class specifications
   - Verify algorithm matches pseudocode
   - Check TODO resolution
   - Validate test coverage

### For Project Managers

1. **Planning:**
   - Use implementation phases for sprint planning
   - Track information gap resolution
   - Plan configuration management approach

2. **Tracking:**
   - Use success criteria from IMPLEMENTATION_SUMMARY.md
   - Monitor validation against real workunits
   - Track estimation accuracy improvements

## Feature Summary

### What It Does

Analyzes a completed Thor workunit and:
1. Extracts resource usage from each subgraph (memory, CPU, disk)
2. Estimates execution time and cost on different queue configurations
3. Recommends the most cost-effective queue
4. Provides trade-off analysis (faster vs. cheaper options)

### How It Works

```
Workunit (completed)
    ↓
Extract subgraph statistics
    ↓
For each queue configuration:
    - Scale memory by worker ratio
    - Detect if spilling will occur
    - Estimate execution time
    - Calculate cost
    ↓
Sort by cost (ascending)
    ↓
Recommend best fit
```

### Key Algorithm

For each subgraph on each candidate queue:

1. **Scale memory:** `ScaledMemory = ActualMemory × (CandidateWorkers / ActualWorkers)`
2. **Detect spilling:** `WillSpill = (ScaledMemory > QueueMaxMemory)`
3. **Apply penalty if newly spilling:** `Time = Time × 10` (configurable)
4. **Scale CPU time:** `CpuTime = CpuTime × (CandidateWorkers / ActualWorkers)`
5. **Calculate cost:** `Cost = calcCostNs(hourlyRate, time) × workers`

### Example Output

```
Queue Recommendation Analysis
Workunit: W20231115-120000

Candidate Queues:
Queue    Workers  Est.Time(s)  Est.Cost($)  Spills  Status
------------------------------------------------------------
small         10      2000.00         5.56     Yes   Too small
medium        40      1234.56        12.34      No   ✓ Best match
large        100       800.00        32.00      No   Faster/costlier

Recommendation: medium queue
- Most cost-effective option
- Estimated: $12.34 vs actual: $12.34
- Alternative: 'large' is 35% faster but costs 159% more
```

## Critical Information Gaps

Before implementation, these must be resolved:

| # | Gap | Impact | Resolution Strategy |
|---|-----|--------|---------------------|
| 1 | StatisticKind enum values | **HIGH** | Search jstats.h, workunit headers |
| 2 | Queue configuration source | **HIGH** | Decide: file, Dali, or hardcoded |
| 3 | Actual worker count | **HIGH** | Find in workunit statistics |
| 4 | Memory units | Medium | Verify bytes vs other units |
| 5 | Spill detection | Medium | Confirm `PeakTempDisk > 0` |
| 6 | Integration points | Medium | Decide: automatic vs. on-demand |
| 7 | Result storage | Low | Document in workunit or separate |

Each gap is detailed in QUEUE_RECOMMENDATION_PLAN.md with guidance on resolution.

## Dependencies

### Code Dependencies
- `WorkunitAnalyserBase` (existing) - Base class for analyzers
- `WuScope` (existing) - Access to statistics
- `calcCostNs()` (existing) - Cost calculation
- `IPropertyTree` (existing) - Configuration and results

### Information Dependencies
- Thor statistics recording (must include memory, CPU, disk stats)
- Queue configuration data (must be available somewhere)
- Worker count information (must be recorded in workunit)

## Testing Strategy

### Unit Tests
- Time estimation with various scenarios
- Memory scaling calculations
- Cost calculations
- Spill detection logic

### Integration Tests
- Statistics extraction from real workunits
- Configuration loading
- End-to-end analysis
- Report generation

### Validation Tests
- Estimate accuracy vs. actual executions
- Various workload types (CPU-bound, memory-bound, I/O-bound)
- Edge cases (no spills, all spills, missing stats)

## Implementation Timeline

Estimated 4-week effort:

- **Week 1:** Skeleton + Statistics (Phases 1-2)
- **Week 2:** Algorithm Implementation (Phase 3)
- **Week 3:** Integration (Phase 4)
- **Week 4:** Reporting + Testing (Phase 5)

*Note: Timeline assumes information gaps are resolved before starting.*

## Success Criteria

✅ Code compiles and runs  
✅ Can extract statistics from workunits  
✅ Time estimation works correctly  
✅ Cost calculation is accurate  
✅ Report generation is readable  
✅ Estimates within 20% of actual  
✅ Identifies better queues when available  
✅ Analysis completes in < 5 seconds  

## Future Enhancements

- Machine learning for better predictions
- Historical analysis database
- Real-time recommendation during submission
- ECLWatch UI integration
- Automatic queue selection
- Multi-objective optimization

## Related Files

### In This Directory
- `anacommon.hpp/cpp` - Common interfaces
- `anawu.hpp/cpp` - Main workunit analysis
- `anarule.hpp/cpp` - Rule-based analysis

### External References
- `system/jlib/jstats.h` - Statistics types
- `common/workunit/workunit.cpp` - Thor cost calculation
- `common/thorhelper/commonext.hpp` - Helper functions

## Questions or Issues?

For questions about:
- **Design decisions:** See QUEUE_RECOMMENDATION_PLAN.md design rationale
- **Implementation details:** See GITHUB_COPILOT_PROMPT.md implementation tasks
- **Information gaps:** See QUEUE_RECOMMENDATION_PLAN.md open questions section
- **Testing:** See all three documents' testing sections

## Document Maintenance

These documents should be updated:
- When information gaps are resolved (mark as resolved, add details)
- When design decisions change (update rationale)
- During implementation (add discovered TODOs)
- After completion (add lessons learned)

---

**Last Updated:** 2024  
**Status:** Planning Complete, Ready for Implementation  
**Next Step:** Resolve critical information gaps, then begin Phase 1
