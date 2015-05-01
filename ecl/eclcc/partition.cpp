/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

#include <platform.h>
#include "jlib.hpp"

//#define TRACE_PROGRESS
//#define TRACE_RESULT
//#define VERIFY_RESULT

#define FNV1_64_INIT I64C(0xcbf29ce484222325)
#define FNV_64_PRIME I64C(0x100000001b3U)

typedef unsigned __int64 rowkey_t;
typedef UInt64Array KeyArray;
typedef unsigned __int64 rowcount_t;

struct InputPosition : public CInterface
{
public:
    InputPosition(unsigned _numChunks)
    {
        numChunks = _numChunks;
        globalPos = 0;
        localPos = new rowcount_t[numChunks];
    }
    ~InputPosition()
    {
        delete [] localPos;
    }

    InputPosition & set(InputPosition & other)
    {
        memcpy(localPos, other.localPos, numChunks * sizeof(rowcount_t));
        globalPos = other.globalPos;
        return *this;
    }

    rowcount_t calcGlobalPosition()
    {
        rowcount_t total = 0;
        for (unsigned i=0; i < numChunks; i++)
            total += localPos[i];
        globalPos = total;
        return total;
    }

    bool equals(const InputPosition & other) const { return globalPos == other.globalPos; }

    void incPart(unsigned which)
    {
        localPos[which]++;
        globalPos++;
    }

    void trace()
    {
        StringBuffer s;
        s.appendf("%10"I64F"u: [", globalPos);
        for (unsigned i=0; i < numChunks; i++)
        {
            if (i) s.append(",");
            s.append(localPos[i]);
        }
        s.append("]");
        puts(s.str());
    }

public:
    unsigned numChunks;
    rowcount_t globalPos;
    rowcount_t * localPos;
};
typedef CIArrayOf<InputPosition> PositionArray;

int compareKey(const void * _left, const void * _right)
{
    const rowkey_t * left = static_cast<const rowkey_t *>(_left);
    const rowkey_t * right = static_cast<const rowkey_t *>(_right);
    if (*left < *right)
        return -1;
    if (*left > *right)
        return +1;
    return 0;
}

int compareKeyElement(const rowkey_t * left, const rowkey_t * right)
{
    if (*left < *right)
        return -1;
    if (*left > *right)
        return +1;
    return 0;
}


//It is vitally important these two functions agree when rounding is significant
unsigned mapPositionToPartition(unsigned numPartitions, rowcount_t globalPos, rowcount_t totalRows)
{
    //rounds down
    return (unsigned)((globalPos * numPartitions) / totalRows);
}

rowcount_t getIdealPartition(unsigned numPartitions, rowcount_t totalRows, unsigned partition)
{
    //must round up
    return (partition * totalRows + numPartitions-1) / numPartitions;
}

// A single chunk of records - could be multiple chunks on each slave node
class Chunk : public CInterface
{
public:
    Chunk(rowcount_t _numRows)
    {
        numRows = _numRows;
        rows = new rowkey_t[(size_t)numRows];
    }
    ~Chunk()
    {
        delete [] rows;
    }

    rowkey_t get(rowcount_t i) const { return rows[i]; }
    void set(rowcount_t i, rowkey_t value) { rows[i] = value; }
    void setRows(rowcount_t num) { numRows = num; } // only for testing

    rowkey_t getMedian(rowcount_t low, rowcount_t high) const
    {
        assertex(low != high);
        return rows[(low+high)/2];
    }

    rowcount_t searchGE(rowkey_t seek, rowcount_t low, rowcount_t high) const
    {
        while (low + 1< high)
        {
            rowcount_t mid = (low + high) / 2;
            int c = compareKey(&seek, rows + mid);
            if (c <= 0)
                high = mid;
            else
                low = mid;
        }
        if (compareKey(&seek, rows + low) > 0)
            return low+1;
        return low;
    }
    inline rowcount_t ordinality() const { return numRows; }

    void sort()
    {
        qsort(rows, (size_t)numRows, sizeof(rowkey_t), compareKey);
    }

    void trace()
    {
        for (unsigned i =0; i < numRows; i++)
            printf("%u: %20"I64F"u\n", i, rows[i]);
    }
public:
    rowcount_t numRows;
    rowkey_t * rows;
};

typedef CIArrayOf<Chunk> ChunkArray;

Chunk * createRandomChunk(rowcount_t size, rowkey_t & seedKey)
{
    Chunk * chunk = new Chunk(size);
    rowkey_t prevKey = seedKey;
    for (rowcount_t i=0; i < size; i++)
    {
        rowkey_t key = (prevKey * FNV_64_PRIME) + i;
        chunk->set(i, key);
        prevKey = key;
    }
    chunk->sort();
    seedKey = prevKey;
    return chunk;
}

void createRandomChunks(ChunkArray & chunks, rowcount_t size, rowcount_t chunkSize)
{
    rowkey_t key = FNV1_64_INIT;
    while (size > chunkSize)
    {
        chunks.append(*createRandomChunk(chunkSize, key));
        size -= chunkSize;
    }
    chunks.append(*createRandomChunk((rowcount_t)size, key));
}


Chunk * createSequentialChunk(rowcount_t size, rowkey_t initial, rowkey_t step)
{
    Chunk * chunk = new Chunk(size);
    for (rowcount_t i=0; i < size; i++)
        chunk->set(i, initial + step * i);
    return chunk;
}

void createSequentialChunks(ChunkArray & chunks, rowcount_t size, rowcount_t chunkSize, rowkey_t chunkStep, rowkey_t step)
{
    rowkey_t key = 1;
    while (size > chunkSize)
    {
        chunks.append(*createSequentialChunk(chunkSize, key, step));
        size -= chunkSize;
        key += chunkStep;
    }
    chunks.append(*createSequentialChunk((rowcount_t)size, key, step));
}


//----------------------------------------------------------------------------------

struct PartitionRange : public CInterface
{
public:
    PartitionRange(unsigned _numChunks, unsigned _from, unsigned _to)
        : lowPos(_numChunks), highPos(_numChunks), split(_numChunks), minPart(_from), maxPart(_to)
    {
        hasKey = false;
    }

    inline bool goodEnough(unsigned maxSplitDelta) const
    {
        if (!hasKey)
            return false;
        return (highPos.globalPos - lowPos.globalPos) <= maxSplitDelta+1;
    }

    void setMinimumKey(rowkey_t _key) { key = _key; hasKey = true; }

    void trace()
    {
        printf("{%d..%d} = ", minPart, maxPart);
        if (hasKey)
            printf("%"I64F"u\n", key);
        else
            printf("?\n");
        lowPos.trace();
        highPos.trace();
    }

public:
    unsigned minPart;
    unsigned maxPart; // inclusive
    InputPosition lowPos;
    InputPosition highPos; // exclusive
    InputPosition split; // split from the median
    rowkey_t key;
    bool hasKey;
};

typedef CIArrayOf<PartitionRange> PartitionArray;

int comparePartition(CInterface * const * _left, CInterface * const * _right)
{
    PartitionRange * left = (PartitionRange *)*_left;
    PartitionRange * right = (PartitionRange *)*_right;
    if (left->minPart < right->minPart)
        return -1;
    return +1;
}

void trace(PartitionArray & partition)
{
    ForEachItemIn(i, partition)
        partition.item(i).trace();
}

void traceResult(PartitionArray & partition)
{
    ForEachItemIn(i, partition)
    {
        PartitionRange & curPartition = partition.item(i);
        printf("%d=%"I64F"u", curPartition.minPart, curPartition.key);
        curPartition.lowPos.trace();
    }
}

//----------------------------------------------------------------------------------

//Bettwe
struct MedianSet : public CInterface
{
public:
    void calcMedian() // not const.. could modify the array
    {
        KeyArray copyValues;
        copyValues.ensure(values.ordinality());
        ForEachItemIn(i, values)
            copyValues.append(values.item(i));
        //simple but O(n log(n))
        copyValues.sort(compareKeyElement);
        median = copyValues.item(values.ordinality()/2);
    }

    //Work out how to do this more efficiently later.
    void append(unsigned chunk, rowkey_t value)
    {
        chunks.append(chunk);
        values.append(value);
    }

    void ensure(unsigned value) { chunks.ensure(value); values.ensure(value); }

    unsigned getMatchingChunk() const
    {
        unsigned match = values.find(median);
        assertex(match != NotFound);
        return chunks.item(match);
    }

    inline rowkey_t getPivot() const { return median; }

protected:
    UnsignedArray chunks;
    KeyArray values;
    rowkey_t median;
};

class MedianArray
{
public:
    void calcPivots()
    {
        ForEachItemIn(i, values)
            values.item(i).calcMedian();
    }

    void ensure(unsigned value) { values.ensure(value); }

    CIArrayOf<MedianSet> values;
};

//----------------------------------------------------------------------------------

class RemotePartitioner
{
public:
    RemotePartitioner(const ChunkArray & _chunks, unsigned _numPartitions, unsigned _maxSplitDelta)
        : chunks(_chunks), numPartitions(_numPartitions), maxSplitDelta(_maxSplitDelta)
    {
        numIterations = 0;
    }

    void calcPartition(PartitionArray & result);
    inline unsigned getNumIterations() const { return numIterations; }

protected:
    void getMedians(MedianArray & medians, const PartitionArray & partitions);
    void getSplits(const MedianArray & medians, const PartitionArray & partitions);

protected:
    const ChunkArray & chunks;
    unsigned numPartitions;
    unsigned maxSplitDelta;
    unsigned numIterations;
};

void RemotePartitioner::getMedians(MedianArray & medians, const PartitionArray & partitions)
{
    medians.ensure(partitions.ordinality());
    ForEachItemIn(i1, partitions)
    {
        medians.values.append(*new MedianSet);

        unsigned numActiveChunks = 0;
        ForEachItemIn(iChunk, chunks)
        {
            const Chunk & curChunk = chunks.item(iChunk);

            //For this chunk, find the median value from each of the active partitions
            const PartitionRange & curPartition = partitions.item(i1);
            rowcount_t from = curPartition.lowPos.localPos[iChunk];
            rowcount_t to = curPartition.highPos.localPos[iChunk];
            if (from != to)
                numActiveChunks++;
        }
        medians.values.item(i1).ensure(numActiveChunks);
    }

    //This should be done asynchronusly in parallel, batching all chunks for a given target node.
    //Need to be careful about aggregating results
    ForEachItemIn(iChunk, chunks)
    {
        const Chunk & curChunk = chunks.item(iChunk);
        ForEachItemIn(iPart, partitions)
        {
            const PartitionRange & curPartition = partitions.item(iPart);
            rowcount_t from = curPartition.lowPos.localPos[iChunk];
            rowcount_t to = curPartition.highPos.localPos[iChunk];
            if (from != to)
            {
                rowkey_t median = curChunk.getMedian(from, to);
                medians.values.item(iPart).append(iChunk, median);
            }
        }
    }
}

//For each pivot, determine how it breaks down each partition
void RemotePartitioner::getSplits(const MedianArray & medians, const PartitionArray & partitions)
{
    //This should be done asynchronously in parallel, batching all chunks for a given target node.
    //The structure is not very cache efficient for iterating in this way, and later iterations of the partition
    //are likely to only update a few elements.
    ForEachItemIn(iChunk, chunks)
    {
        const Chunk & curChunk = chunks.item(iChunk);
        //For this chunk, find the median value from each of the active partitions
        ForEachItemIn(iPart, partitions)
        {
            const PartitionRange & curPartition = partitions.item(iPart);
            rowcount_t from = curPartition.lowPos.localPos[iChunk];
            rowcount_t to = curPartition.highPos.localPos[iChunk];
            rowcount_t split = from;
            if (from != to)
                split = curChunk.searchGE(medians.values.item(iPart).getPivot(), from, to);

            //Thread safe so no need to protect if asynchronous
            //would be good if we could optimize so the assignment didn't need to happen for later iterations.
            curPartition.split.localPos[iChunk] = split;
        }
    }

    ForEachItemIn(iGlobal, partitions)
        partitions.item(iGlobal).split.calcGlobalPosition();
}


void RemotePartitioner::calcPartition(PartitionArray & result)
{
    const unsigned numChunks = chunks.ordinality();

    //Start off with (1..N) [0..maxpos]
    //Start off with a single partition entry covering all the partitions,
    //for each node, record the range of filepositions within that node i.e. [0..chunk(i).#rows)]
    PartitionRange * initialRange = new PartitionRange(numChunks, 1, numPartitions-1);
    rowcount_t totalRows = 0;
    for (unsigned i1 = 0; i1 < numChunks; i1++)
    {
        rowcount_t thisSize = chunks.item(i1).ordinality();
        initialRange->lowPos.localPos[i1] = 0;
        initialRange->highPos.localPos[i1] = thisSize;
    }
    totalRows = initialRange->highPos.calcGlobalPosition();
    assertex(totalRows >= numPartitions);   // otherwise silly to do it this way and multiple exact matches possible.

#ifdef TRACE_PROGRESS
    initialRange->trace();
#endif
    PartitionArray active;
    active.append(*initialRange);

    //Keep looping until all the partition points have been sufficiently resolved
    while (active.ordinality())
    {
        MedianArray medians;
        //For each of the active partitions, find the medians from each of the chunks
        getMedians(medians, active);

        //Find the median of the medians from each partition point
        medians.calcPivots();

        //Find out where each of those medians maps in each of the chunks.
        getSplits(medians, active);

        PartitionArray again;
        ForEachItemIn(i, active)
        {
            PartitionRange & curPartition = active.item(i);
            InputPosition & split = curPartition.split;
            MedianSet & curMedian = medians.values.item(i);

            //In an ideal world, which partition point does this position map to?
            unsigned whichPartition = mapPositionToPartition(numPartitions, split.globalPos, totalRows);
            assertex((whichPartition + 1 >= curPartition.minPart) && (whichPartition <= curPartition.maxPart));

            //So we started of with the range lowPos..highPos, and have now split it in three.
            //we have the ranges [low..split] [split..split+1], [split+1..high]
            //which correspond to the partition ranges [min..whichPartion] [] [whichPartion+1..high]
            // or if an exact match to one partition   [min..whichPartion-1] [whichPartion] [whichPartion+1..high]

            //MORE: This should be optimized to reuse the partition objects if there is only a single partition.

            bool extactMatch = false;
            if (whichPartition >= curPartition.minPart)
            {
                if (getIdealPartition(numPartitions, totalRows, whichPartition) == split.globalPos)
                    extactMatch = true;
            }
            bool splitLow = extactMatch ? (curPartition.minPart < whichPartition) : (curPartition.minPart <= whichPartition);

            if (curPartition.minPart == curPartition.maxPart)
            {
                //Optimize a single item
                if (extactMatch) // could be an exact match on an earlier part if many dups
                {
                    PartitionRange * exact = LINK(&curPartition);
                    exact->setMinimumKey(curMedian.getPivot());
                    exact->lowPos.set(split);
                    exact->highPos.set(split).incPart(curMedian.getMatchingChunk());
                    result.append(*exact);
                }
                else
                {
                    if (splitLow)
                    {
                        PartitionRange * below = LINK(&curPartition);
                        below->highPos.set(split);
                        again.append(*below);
                    }
                    else
                    {
                        assertex(whichPartition+1 <= curPartition.maxPart);
                        PartitionRange * above = LINK(&curPartition);
                        if (maxSplitDelta > 0)
                            above->setMinimumKey(curMedian.getPivot());
                        above->lowPos.set(split).incPart(curMedian.getMatchingChunk());
                        again.append(*above);
                    }
                }
            }
            else
            {
                if (splitLow)
                {
                    unsigned maxPart = extactMatch ? whichPartition-1 : whichPartition;
                    PartitionRange * below = new PartitionRange(numChunks, curPartition.minPart, maxPart);
                    below->hasKey = curPartition.hasKey;
                    below->key = curPartition.key;
                    below->lowPos.set(curPartition.lowPos);
                    below->highPos.set(split);
                    again.append(*below);
                }

                if (extactMatch) // could be an exact match on an earlier part if many dups
                {
                    PartitionRange * exact = new PartitionRange(numChunks, whichPartition, whichPartition);
                    exact->setMinimumKey(curMedian.getPivot());
                    exact->lowPos.set(split);
                    exact->highPos.set(split).incPart(curMedian.getMatchingChunk());
                    result.append(*exact);
                }

                if (whichPartition+1 <= curPartition.maxPart)
                {
                    PartitionRange * above = new PartitionRange(numChunks, whichPartition+1, curPartition.maxPart);
                    //This value is one two low, but if we allow some skew then that is a reasonable starting point
                    if (!extactMatch && maxSplitDelta > 0)
                        above->setMinimumKey(curMedian.getPivot());
                    above->lowPos.set(split).incPart(curMedian.getMatchingChunk());
                    above->highPos.set(curPartition.highPos);
                    again.append(*above);
                }
            }
        }

        active.kill();
        ForEachItemIn(iAgain, again)
        {
            PartitionRange & curPartition = again.item(iAgain);
            if (curPartition.goodEnough(maxSplitDelta))
                result.append(OLINK(curPartition));
            else
                active.append(OLINK(curPartition));
        }

        numIterations++;
#ifdef TRACE_PROGRESS
        printf("----------- Iter %d --------------\n", numIterations);
        trace(active);
        printf("result\n");
        traceResult(result);
#endif
    }
}

Chunk * qsortPartitions(rowcount_t totalRows, const ChunkArray & chunks)
{
    Owned<Chunk> chunk = new Chunk((rowcount_t)totalRows);
    rowcount_t delta = 0;
    ForEachItemIn(i, chunks)
    {
        Chunk & cur = chunks.item(i);
        unsigned num = (size_t)cur.ordinality();
        for (unsigned j=0; j < num; j++)
            chunk->set(delta+j, cur.get(j));
        delta += num;
    }
    chunk->sort();
    return chunk.getClear();
}

void testqsortPartitions(rowcount_t totalRows, const ChunkArray & chunks)
{
    unsigned now1 = msTick();
    Owned<Chunk> chunk = qsortPartitions(totalRows, chunks);
    printf("Sort time = %u\n", msTick()-now1);
}

void verifyPartition(rowcount_t totalRows, unsigned numPartitions, const PartitionArray & partition, const ChunkArray & chunks)
{
    unsigned failures = 0;
    Owned<Chunk> allSorted = qsortPartitions(totalRows, chunks);
    ForEachItemIn(i, partition)
    {
        PartitionRange & curPartition = partition.item(i);
        rowcount_t offset = getIdealPartition(numPartitions, totalRows, curPartition.minPart);
        rowkey_t key = allSorted->get(offset);
        if (key != curPartition.key)
        {
            printf("partitions %d %"I64F"u [%"I64F"u @%"I64F"u] failed to match\n", curPartition.minPart, curPartition.key, key, offset);
            failures++;
        }
    }
    if (failures)
        printf("%u partitions failed to match exact\n", failures);
    else
        printf("Partition matches exact split\n");
}

void testPartition(rowcount_t totalRows, unsigned numPartitions, const ChunkArray & chunks)
{
    unsigned startPartition = msTick();

    //the number the partition can be away from the ideal, 0 = exact, 0.1 = 0.1% skew
    double maxSkew = 0.0;
    const unsigned maxSplitDelta = (unsigned)(totalRows/chunks.ordinality()*(maxSkew/100));
    RemotePartitioner partitioner(chunks, numPartitions, maxSplitDelta);

    PartitionArray result;
    partitioner.calcPartition(result);
    printf("Partition time %d, %"I64F"u = %u (%d iterations)\n", numPartitions, totalRows, msTick()-startPartition, partitioner.getNumIterations());

#ifdef VERIFY_RESULT
    unsigned startVerify = msTick();
    verifyPartition(totalRows, numPartitions, result, chunks);
    printf("Verify time = %u\n", msTick()-startVerify);
#endif

    result.sort(comparePartition);
#ifdef TRACE_RESULT
    printf("----------- Result --------------\n");
    traceResult(result);
#endif
}

void testSplitFunctions()
{
    unsigned totalRows = 100;
    unsigned numPartitions = 7;
    for (unsigned i = 0; i < 100; i++)
    {
        unsigned part1 = mapPositionToPartition(numPartitions, i, totalRows);
        unsigned part2 = mapPositionToPartition(numPartitions, i+1, totalRows);
        rowcount_t pos2 = getIdealPartition(numPartitions, totalRows, part2);
        if (part1 != part2)
            assertex(pos2 == i+1);
    }
}

void testPartition()
{
    const rowcount_t totalRows = 100000000;
    const unsigned numChunks = 400;
    const unsigned maxPartitions = 400;

    //---------------------------
    const rowcount_t chunkSize = (rowcount_t)(totalRows / numChunks);
    const rowcount_t requestedRows = numChunks * chunkSize;

    testSplitFunctions();
    ChunkArray chunks;
    unsigned startCreate = msTick();
//    createSequentialChunks(chunks, totalRows, chunkSize, 10763, 17);
    printf("Creating %" I64F "u rows...\n", (unsigned __int64)totalRows);
    createRandomChunks(chunks, totalRows, chunkSize);
    printf("Created rows time = %u\n", msTick()-startCreate);

    for (unsigned numPartitions = 10; numPartitions <= maxPartitions; numPartitions += 10)
    {
        testPartition(totalRows, numPartitions, chunks);
    }

    for (unsigned size = chunkSize / 256; size < chunkSize; size += size)
    {
        ForEachItemIn(i, chunks)
            chunks.item(i).setRows(size);
        testPartition(size*numChunks, maxPartitions, chunks);
    }
}
