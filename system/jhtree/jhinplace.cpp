/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <alloca.h>
#endif
#include <algorithm>

#include "jmisc.hpp"
#include "jset.hpp"
#include "hlzw.h"

#include "ctfile.hpp"
#include "jhinplace.hpp"
#include "jstats.h"

#ifdef _DEBUG
//#define SANITY_CHECK_INPLACE_BUILDER     // painfully expensive consistency check
#endif

static constexpr size32_t minRepeatCount = 2;       // minimum number of times a 0x00 or 0x20 is repeated to generate a special opcode
static constexpr size32_t minRepeatXCount = 3;      // minimum number of times a byte is repeated to generate a special opcode
static constexpr size32_t maxQuotedCount = 31 + 256; // maximum number of characters that can be quoted in a row
static constexpr size32_t repeatDelta = (minRepeatCount - 1);
static constexpr size32_t repeatXDelta = (minRepeatXCount - 1);

constexpr size32_t getMinRepeatCount(byte value)
{
    return (value == 0x00 || value == 0x20) ? minRepeatCount : minRepeatXCount;
}

enum SquashOp : byte
{
    SqZero      = 0x00,           // repeated zeros
    SqSpace     = 0x01,           // repeated spaces
    SqQuote     = 0x02,           // Quoted text
    SqRepeat    = 0x03,           // any character repeated
    SqOption    = 0x04,           // an set of option bytes
    SqNull      = 0x06,           // repeats that match the null row
    SqSpecial   = 0x07,           // special control flow
};

/*

How can you store non-leaf nodes that are compressed, don't need decompression to compare, and are efficient to resolve?

The following compression aims to implement that.  It aims to be cache efficient, rather than minimizing the number
of byte comparisons - in particular it scans blocks of bytes rather than binary chopping them.

The compressed format is an alternating tree of string matches and byte options.

Take the following list of names:
(abel, abigail, absalom, barry, barty, bill, brian, briany) (5, 8, 8, 6, 6, 5, 5, 6) = 49bytes

This is the suggested internal representation

match(0)
    opt(2, { a , b }, {3, 8})
        match(1, "b")
    opt(3, { e, i, s }, { 1, 2, 3})
        match(1, "l")                   #end
        match(4, "gail")                #end
        match(4, "alom")                #end
    opt(3, { a, i, r }, { 2, 3, 5 })
        match(1, "r")
        opt(2, { r, t}, { 1, 2 })
            match(1, "y")               #end
            match(1, "y")               #end
        match(3, "ill")                 #end
        match(3, "ian")
        opt(2, { ' ', 'y' }, { 1, 2})
            match(0)                    #end
            match(0)                    #end

The candidate in-memory layout would be:

     0x00
      0x02 0x11 A B count(0x03 0x08) offset{22 [57])
       0x01 B
        0x03 0x01 E I S offset(2 7 [12])
         0x01 L
         0x04 G A I L
         0x04 A L O M
       0x03 0x11 A I R count(0x02 0x03 0x05) offset(12 16 [24])
        0x01 R
         0x02 0x01 R T offset(0x02 [0x04])
          0x01 Y
          0x01 Y
        0x03 I L L
        0x03 I A N
         0x02 0x01 ' ' Y offset(0x01 [0x02])
          0x00
          0x00

Compression optimizations:
    All lengths use packed integers
    All arrays of counts or offsets use a number of bytes (nibbles?) required for the largest number they are storing.
    #End is implied by a range of 1 count rather than stored as a flag
    Store count-2 for options since there must be at least 2 alternatives
    Store count-1 for the counts of the option branches
    Don't store the offset after the last option branch since it is never needed.
    Use a scaled array for the filepositions, and use a bit to indicate scale by the node size.
    Use a special size code (0) for duplicates - option with nothing more to match on any branches.

Done:
  * Add code to trace the node cache sizes and the time taken to search branch nodes.
    Use this to compare i) normal ii) new format iii) new format with direct search
  * Ensure that the allocated node result is padded with 3 zero bytes to allow misaligned access.
  * Experiminent with updating offset only - code size and speed - seemed slower on first version
  * Move serialize code in builder inside a sanity check flag.

Next Steps:
  * Implement findGT (inline in terms of a common function). - delay until new format introduced
    Add to unit tests as well as node code.
=> Have a complete implementation that is useful for speed testing

  * Use leading bit count operations to implement packing/unpacking operations
  * Allow repeated values and matches to the null record to be compressed
      Use size bytes 0xF0-0xFF to represent special sequences.
      Bottom bit indicates if there is a following match (rather than opt)
      0xF0/F1 <count> <byte> - byte is repeated <count> times
      0xF2/F3 <count> - matches items from the null row
      0xF4 <count> - A compound repeat follows, don't assume end has been reached
  * Create null rows in index code and pass through
  * Use unsigned read to check for repeated spaces?
  * Check padding of file positions to 8 bytes
=> Complete implementation for leaf nodes.

  * Ensure that match sequences never span the key boundary
  * Make sure the correct key length is passed through
  * Add code in the comparison to spot a match once the keylength has been reached, even if more data follows.
  * Allow options where some are 0 length - not needed for comparison, only getValueAt().
    (Set a bit on the option, include the last offset, and check the offset delta != 0).  assert not set if in keyed portion
    or have a bitset of null lengths
  * Document all the assumptions/ideas in the design.  Fixed length compare key, variable length trailing.
=> Experiment with using the same compression for leaf nodes.
  o Using rows as they are
  o Using lz4 compression with a separate compressed index of offsets into the compressed data

Future potential optimizations
    compare generated code updating offset rather than search in compare code -> is it more efficient?
    Compare in 4byte chunks and use __builtin_bswap32 to reverse bytes when they differ
    Don't save the last count in an options list (it must = next-prev)
    Move the countBytes=0 processing into the caller function, same file filepos packing, and check not zero in extract function
    Allow nibbles for sizes of arrays
    Store offset-1 for the offsets of the option branches
    Use bits in the sizeinfo to indicate special cases...
    Could have varing sizes for lead and non-leaf nodes, but I suspect it wouldn't save much.
    Add stats on number of nodes, average null compare length, zero compare length option fan out etc.  Number of bytes
    saved by trailing null suppression.
    Optimized/simplified numExtraBytesFromFirst() that can only support 4 bytes.
    Is big or little endian better for the packed integers?
    ?Use LZ END to compress payload in leaf nodes?

NOTES:
    The format is assumed to have a fixed length key, potentially followed by a variable length payload that is never compared.
    Always guarantee 3 bytes after last item in an array to allow misaligned read access
    Once you finish building subtree the representation will not change.  Can cache the serialized size and only recalculate if the tree changes.
    Currently planned for non-leaf nodes only, but it could also be used for leaf nodes, with an offset table to take you to a separate payload.

*/


//For the following the efficiency of serialization isn't a big concern, speed of deserializion is.

void serializeOp(MemoryBuffer & out, SquashOp op, size32_t count)
{
    assertex(count != 0 && count < 32 + 255);
    if (count <= 31)
    {
        out.append((byte)((op << 5) | count));
    }
    else
    {
        out.append((byte)(op << 5));
        out.append((byte)(count - 32));
    }
}

unsigned sizeSerializedOp(size32_t count)
{
    return (count <= 31) ? 1 : 2;
}


//Packing using a top bit to indicate more.  Revisit if it isn't great.  Could alternatively use rtl packed format, but want to avoid the overhead of a call
static unsigned sizePacked(unsigned __int64 value)
{
    unsigned size = 1;
    while (value >= 0x80)
    {
        value >>= 7;
        size++;
    }
    return size;

    //The following is slower because of the division by 7, also likely to be slower for small values
    //    unsigned bits = getMostSignificantBit(value|1);
    //    return (bits + 6) / 7;
}


static void serializePacked(MemoryBuffer & out, unsigned __int64  value)
{
    constexpr unsigned maxBytes = 9;
    unsigned index = maxBytes;
    byte result[maxBytes];

    result[--index] = value & 0x7f;
    while (value >= 0x80)
    {
        value >>= 7;
        result[--index] = (value & 0x7f) | 0x80;
    }
    out.append(maxBytes - index, result + index);
}

inline unsigned readPacked32(const byte * & cur)
{
    byte next = *cur++;
    unsigned value = next;
    if (unlikely(next >= 0x80))
    {
        value = value & 0x7f;
        do
        {
            next = *cur++;
            value = (value << 7) | (next & 0x7f);
        } while (next & 0x80);
    }
    return value;
}

inline unsigned __int64 readPacked64(const byte * & cur)
{
    byte next = *cur++;
    unsigned value = next;
    if (unlikely(next >= 0x80))
    {
        value = value & 0x7f;
        do
        {
            next = *cur++;
            value = (value << 7) | (next & 0x7f);
        } while (next & 0x80);
    }
    return value;
}
//----- special packed format - code is untested

inline unsigned getLeadingMask(byte extraBytes) { return (0x100U - (1U << (8-extraBytes))); }
inline unsigned getLeadingValueMask(byte extraBytes) { return (~getLeadingMask(extraBytes)) >> 1; }

static void serializePacked2(MemoryBuffer & out, size32_t value)
{
    //Efficiency of serialization is not the key consideration
    byte mask = 0;
    unsigned size = sizePacked(value);
    constexpr unsigned maxBytes = 9;
    byte result[maxBytes];

    for (unsigned i=1; i < size; i++)
    {
        result[maxBytes - i] = value;
        value >>= 8;
        mask = 0x80 | (mask >> 1);
    }
    unsigned start = maxBytes - size;
    result[start] |= mask;
    out.append(size, result + start);
}

inline unsigned numExtraBytesFromFirst1(byte first)
{
    if (first >= 0xF0)
        if (first >= 0xFC)
            if (first >= 0xFE)
                if (first >= 0xFF)
                    return 8;
                else
                    return 7;
            else
                return 6;
        else
            if (first >= 0xF8)
                return 5;
            else
                return 4;
    else
        if (first >= 0xC0)
            if (first >= 0xE0)
                return 3;
            else
                return 2;
        else
            return (first >> 7);    // (first >= 0x80) ? 1 : 0
}

inline unsigned numExtraBytesFromFirst2(byte first)
{
    //Surely should be faster, but seems slower on AMD. Retest in its actual context
    unsigned value = first;
    return countLeadingUnsetBits(~(value << 24));
}

inline unsigned numExtraBytesFromFirst(byte first)
{
    return numExtraBytesFromFirst1(first);
}

inline unsigned readPacked2(const byte * & cur)
{
    byte first = *cur++;
    unsigned extraBytes = numExtraBytesFromFirst(first);
    unsigned value = first & getLeadingValueMask(extraBytes);
    while (extraBytes--)
    {
        value <<= 8;
        value |= *cur++;
    }
    return value;
}

//-------------------

static unsigned bytesRequired(unsigned __int64 value)
{
    unsigned bytes = 1;
    while (value >= 0x100)
    {
        value >>= 8;
        bytes++;
    }
    return bytes;
}

void serializeBytes(MemoryBuffer & out, unsigned __int64 value, unsigned bytes)
{
    for (unsigned i=0; i < bytes; i++)
    {
        out.append((byte)value);
        value >>= 8;
    }
    assertex(value == 0);
}

unsigned readBytesEntry32(const byte * address, unsigned index, unsigned bytes)
{
#if 0
    //Is the following quicker?  It avoids a multiply, but adds a condition.
    dbgassertex(bytes != 0);
    dbgassertex(bytes <= 2);

    if (bytes == 1)
        return *(const byte *)(address + index);
    else
        return *(const unsigned short *)(address + index * 2);

#else

    //Non aligned access, assumes little endian, but avoids a loop
    const byte * addrValue = address + index * bytes;
    unsigned value = *(const unsigned *)addrValue;

    //Experiment with the following optional implementations
#if 0
    unsigned shift = (4 - bytes) * 8;
    return (value << shift) >> shift;
#else
    if (bytes == 4)
        return value;
    unsigned mask = ((1U << (bytes * 8)) -1);
    return (value & mask);
#endif
#endif
}

unsigned __int64 readBytesEntry64(const byte * address, unsigned index, unsigned bytes)
{
    dbgassertex(bytes != 0);

    //Non aligned access, assumes little endian, but avoids a loop
    const unsigned __int64 * pValue = (const unsigned __int64 *)(address + index * bytes);
    unsigned __int64 value = *pValue;
    if (bytes == 8)
        return value;
    unsigned __int64 mask = ((U64C(1) << (bytes * 8)) -1);
    return (value & mask);
}

//=========================================================================================================

class PartialMatchBuilder;
//---------------------------------------------------------------------------------------------------------------------

bool PartialMatch::allNextAreEnd()
{
    ForEachItemIn(i, next)
    {
        if (!next.item(i).isEnd())
            return false;
    }
    return true;
}

void PartialMatch::cacheSizes()
{
    if (!dirty)
        return;

    squash();
    dirty = false;
    if (squashed && squashedData.length())
        size = squashedData.length();
    else
        size = 0;

    maxOffset = 0;
    unsigned numNext = next.ordinality();
    if (numNext)
    {
        if (allNextAreEnd())
        {
            size += sizeSerializedOp(numNext-1);  // count of options
            size += 1;                      // end marker
            maxCount = numNext;
        }
        else
        {
            size32_t offset = 0;
            maxCount = 0;
            for (unsigned i=0; i < numNext; i++)
            {
                maxCount += next.item(i).getCount();
                maxOffset = offset;
                offset += next.item(i).getSize();
            }

            size += sizeSerializedOp(numNext-1);  // count of options
            size += 1;                      // count and offset table information
            size += numNext;                // bytes of data

            //Space for the count table - if it is required
            if (maxCount != numNext)
                size += (numNext * bytesRequired(maxCount-1));

            //offset table.
            size += (numNext - 1) * bytesRequired(maxOffset);

            size += offset;                       // embedded tree
        }
    }
    else
        maxCount = 1;
}

bool PartialMatch::combine(size32_t newLen, const byte * newData)
{
    size32_t curLen = data.length();
    const byte * curData = (const byte *)data.toByteArray();
    unsigned compareLen = std::min(newLen, curLen);
    unsigned matchLen;
    for (matchLen = 0; matchLen < compareLen; matchLen++)
    {
        if (curData[matchLen] != newData[matchLen])
            break;
    }

    if (matchLen || isRoot)
    {
        dirty = true;
        if (next.ordinality() == 0)
        {
            next.append(*new PartialMatch(builder, curLen - matchLen, curData + matchLen, rowOffset + matchLen, false));
            next.append(*new PartialMatch(builder, newLen - matchLen, newData + matchLen, rowOffset + matchLen, false));
            data.setLength(matchLen);
            squashed = false;
            return true;
        }

        if (matchLen == curLen)
        {
            //Either add a new option, or combine with the last option
            if (next.tos().combine(newLen - matchLen, newData + matchLen))
                return true;
            next.append(*new PartialMatch(builder, newLen - matchLen, newData + matchLen, rowOffset + matchLen, false));
            return true;
        }

        //Split this node into two
        Owned<PartialMatch> childNode = new PartialMatch(builder, curLen-matchLen, curData + matchLen, rowOffset + matchLen, false);
        next.swapWith(childNode->next);
        next.append(*childNode.getClear());
        next.append(*new PartialMatch(builder, newLen - matchLen, newData + matchLen, rowOffset + matchLen, false));
        data.setLength(matchLen);
        squashed = false;
        return true;
    }
    return false;
}

size32_t PartialMatch::getSize()
{
    cacheSizes();
    return size;
}

size32_t PartialMatch::getCount()
{
    cacheSizes();
    return maxCount;
}

size32_t PartialMatch::getMaxOffset()
{
    cacheSizes();
    return maxOffset;
}

bool PartialMatch::removeLast()
{
    dirty = true;
    if (next.ordinality() == 0)
        return true; // merge with caller
    if (next.tos().removeLast())
    {
        if (next.ordinality() == 2)
        {
            //Remove the second entry, and merge this element with the first
            Linked<PartialMatch> first = &next.item(0);
            next.kill();
            data.append(first->data);
            next.swapWith(first->next);
            squashed = false;
            return false;
        }
        else
        {
            next.pop();
            return false;
        }
    }
    return false;
}

void PartialMatch::serialize(MemoryBuffer & out)
{
    squash();

    size32_t originalPos = out.length();
    if (squashed && squashedData.length())
        out.append(squashedData);
    else
    {
        unsigned skip = isRoot ? 0 : 1;
        assertex (data.length() <= skip);
    }

    unsigned numNext = next.ordinality();
    if (numNext)
    {
        if (allNextAreEnd())
        {
            serializeOp(out, SqOption, numNext-1);  // count of options
            out.append((byte)0);
        }
        else
        {
            byte offsetBytes = bytesRequired(getMaxOffset());
            byte countBytes = bytesRequired(getCount()-1);

            byte sizeInfo = offsetBytes;
            if (getCount() != numNext)
                sizeInfo |= (countBytes << 3);

            serializeOp(out, SqOption, numNext-1);  // count of options
            out.append(sizeInfo);

            for (unsigned iFirst = 0; iFirst < numNext; iFirst++)
            {
                next.item(iFirst).serializeFirst(out);
            }

            //Space for the count table - if it is required
            if (getCount() != numNext)
            {
                unsigned runningCount = 0;
                for (unsigned iCount = 0; iCount < numNext; iCount++)
                {
                    runningCount += next.item(iCount).getCount();
                    serializeBytes(out, runningCount-1, countBytes);
                }
                assertex(getCount() == runningCount);
            }

            unsigned offset = 0;
            for (unsigned iOff=1; iOff < numNext; iOff++)
            {
                offset += next.item(iOff-1).getSize();
                serializeBytes(out, offset, offsetBytes);
            }

            for (unsigned iNext=0; iNext < numNext; iNext++)
                next.item(iNext).serialize(out);
        }
    }

    size32_t newPos = out.length();
    assertex(newPos - originalPos == getSize());
}

unsigned PartialMatch::appendRepeat(size32_t offset, size32_t copyOffset, byte repeatByte, size32_t repeatCount)
{
    unsigned numOps = 0;
    const byte * source = data.bytes();
    size32_t copySize = (offset - copyOffset) - repeatCount;
    if (copySize)
    {
        while (copySize)
        {
            size32_t chunkSize = std::min(copySize, maxQuotedCount);
            serializeOp(squashedData, SqQuote, chunkSize);
            squashedData.append(chunkSize, source+copyOffset);
            copyOffset += chunkSize;
            copySize -= chunkSize;
            numOps++;
        }
    }
    if (repeatCount)
    {
        switch (repeatByte)
        {
        case 0x00:
            serializeOp(squashedData, SqZero, repeatCount - repeatDelta);
            break;
        case 0x20:
            serializeOp(squashedData, SqSpace, repeatCount - repeatDelta);
            break;
        default:
            serializeOp(squashedData, SqRepeat, repeatCount - repeatXDelta);
            squashedData.append(repeatByte);
            break;
        }
        numOps++;
    }
    return numOps;
}

bool PartialMatch::squash()
{
    if (!squashed)
    {
        squashed = true;
        squashedData.clear();

        //Always squash if you have some text - avoids length calculation elsewhere
        size32_t startOffset = isRoot ? 0 : 1;
        size32_t keyLen = builder->queryKeyLen();
        if (data.length() > startOffset)
        {
            const byte * source = data.bytes();
            size32_t maxOffset = data.length();

            unsigned copyOffset = startOffset;
            unsigned repeatCount = 0;
            byte prevByte = 0;
            const byte * nullRow = queryNullRow();
            for (unsigned offset = startOffset; offset < maxOffset; offset++)
            {
                byte nextByte = source[offset];

                //MORE Add support for compressing against the null row at somepoint
                //MORE: Ensure that quoted strings are do not span the key boundary?
                if (nextByte == prevByte)
                {
                    repeatCount++;
                }
                else
                {
                    //MORE: Revisit and m ake this more sophisticated.  Trade off between space and comparison time.
                    //  if space or /0 decrease the threshold by 1.
                    //  if the start of the string then reduce the threshold.
                    //  If no child entries increase the threshold (since it may require a special continuation byte)?
                    //  After the keyed component compress more aggressively
                    if (repeatCount > 3)
                    {
                        appendRepeat(offset, copyOffset, prevByte, repeatCount);
                        copyOffset = offset;
                    }
                    repeatCount = 1;
                    prevByte = nextByte;
                }
            }

            if (repeatCount < getMinRepeatCount(prevByte))
                repeatCount = 0;

            appendRepeat(maxOffset, copyOffset, prevByte, repeatCount);
        }
    }
    return dirty;
}

const byte * PartialMatch::queryNullRow()
{
    return builder->queryNullRow();
}

//MORE: Pass this in...
void PartialMatch::serializeFirst(MemoryBuffer & out)
{
    if (data.length())
        out.append(data.bytes()[0]);
    else
        out.append(queryNullRow()[rowOffset]);
}

void PartialMatch::trace(unsigned indent)
{
    StringBuffer squashedHex;
    for (unsigned i=0; i < squashedData.length(); i++)
    {
        byte next = squashedData.bytes()[i];
        squashedHex.append(' ').appendhex(next, true);
    }

    StringBuffer clean;
    StringBuffer dataHex;
    for (unsigned i=0; i < data.length(); i++)
    {
        byte next = data.bytes()[i];
        dataHex.appendhex(next, true);
        if (isprint(next))
            clean.append(next);
        else
            clean.append('.');
    }

    printf("%*s(%s[%s] %u:%u[%u] [%s]", indent, "", clean.str(), dataHex.str(), data.length(), squashedData.length(), getSize(), squashedHex.str());
    if (next.ordinality())
    {
        printf(", %u{\n", next.ordinality());
        ForEachItemIn(i, next)
            next.item(i).trace(indent+1);
        printf("%*s}", indent, "");
    }
    printf(")\n");
}

//---------------------------------------------------------------------------------------------------------------------

void PartialMatchBuilder::add(size32_t len, const void * data)
{
    //It is legal to add rows longer than keyLen, but cannot strip trailing nulls
    if (optimizeTrailing && (len == keyLen))
    {
        const byte * newRow = (const byte *)data;
        while (len && (newRow[len-1] == nullRow[len-1]))
            len--;
    }
    if (!root)
        root.set(new PartialMatch(this, len, data, 0, true));
    else
        root->combine(len, (const byte *)data);

#ifdef SANITY_CHECK_INPLACE_BUILDER
    MemoryBuffer out;
    root->serialize(out);
#endif
}

void PartialMatchBuilder::removeLast()
{
    root->removeLast();
}


void PartialMatchBuilder::serialize(MemoryBuffer & out)
{
    root->serialize(out);
}

void PartialMatchBuilder::trace()
{
    if (root)
        root->trace(0);
    printf("\n");
#if 0
    MemoryBuffer out;
    root->serialize(out);
    for (unsigned i=0; i < out.length(); i++)
        printf("%02x ", out.bytes()[i]);
    printf("\n\n");
#endif
}

unsigned PartialMatchBuilder::getCount()
{
    return root ? root->getCount() : 0;
}

unsigned PartialMatchBuilder::getSize()
{
    return root ? root->getSize() : 0;
}

//---------------------------------------------------------------------------------------------------------------------

InplaceNodeSearcher::InplaceNodeSearcher(unsigned _count, const byte * data, size32_t _keyLen, const byte * _nullRow)
: nodeData(data), nullRow(_nullRow), count(_count), keyLen(_keyLen)
{
}

void InplaceNodeSearcher::init(unsigned _count, const byte * data, size32_t _keyLen, const byte * _nullRow)
{
    count = _count;
    nodeData = data;
    keyLen = _keyLen;
    nullRow = _nullRow;
}

int InplaceNodeSearcher::compareValueAt(const char * search, unsigned int compareIndex) const
{
    unsigned resultPrev = 0;
    unsigned resultNext = count;
    const byte * finger = nodeData;
    unsigned offset = 0;

    while (offset < keyLen)
    {
        byte next = *finger++;
        SquashOp op = (SquashOp)(next >> 5);
        unsigned count = (next & 0x1f);
        if (count == 0)
            count = 32 + *finger++;

        switch (op)
        {
        case SqQuote:
        {
            unsigned numBytes = count;
            for (unsigned i=0; i < numBytes; i++)
            {
                const byte nextSearch = search[i];
                const byte nextFinger = finger[i];
                if (nextFinger != nextSearch)
                {
                    if (offset + i >= keyLen)
                        return 0;

                    if (nextFinger > nextSearch)
                    {
                        //This entry is larger than the search value => we have a match
                        return -1;
                    }
                    else
                    {
                        //This entry (and all children) are less than the search value
                        //=> the next entry is the match
                        return +1;
                    }
                }
            }
            search += numBytes;
            offset += numBytes;
            finger += numBytes;
            break;
        }
        case SqZero:
        case SqSpace:
        {
            const byte nextFinger = "\x00\x20"[op];
            unsigned numBytes = count + repeatDelta;
            for (unsigned i=0; i < numBytes; i++)
            {
                const byte nextSearch = search[i];
                if (nextFinger != nextSearch)
                {
                    if (offset + i >= keyLen)
                        return 0;
                    if (nextFinger > nextSearch)
                        return -1;
                    else
                        return +1;
                }
            }
            search += numBytes;
            offset += numBytes;
            break;
        }
        case SqRepeat:
        {
            const byte nextFinger = *finger++;
            unsigned numBytes = count + repeatXDelta;
            for (unsigned i=0; i < numBytes; i++)
            {
                const byte nextSearch = search[i];
                if (nextFinger != nextSearch)
                {
                    if (offset + i >= keyLen)
                        return 0;
                    if (nextFinger > nextSearch)
                        return -1;
                    else
                        return +1;
                }
            }
            search += numBytes;
            offset += numBytes;
            break;
        }
        case SqOption:
        {
            const unsigned numOptions = count+1;
            byte sizeInfo = *finger++;
            //Top two bits are currently spare - it may make sense to move before count and use them for repetition
            byte bytesPerCount = (sizeInfo >> 3) & 7; // could pack other information in....
            byte bytesPerOffset = (sizeInfo & 7);

            //MORE: Duplicates can occur on the last byte of the key - if so we have a match
            if (bytesPerOffset == 0)
            {
                dbgassertex(resultNext == resultPrev+numOptions);
                return 0;
            }

            const byte * counts = finger + numOptions; // counts follow the data
            const byte nextSearch = search[0];
            for (unsigned i=0; i < numOptions; i++)
            {
                const byte nextFinger = finger[i];
                if (nextFinger > nextSearch)
                {
                    //This entry is greater than search => item(i) is the correct entry
                    if (i == 0)
                        return -1;

                    unsigned delta;
                    if (bytesPerCount == 0)
                        delta = i;
                    else
                        delta = readBytesEntry32(counts, i-1, bytesPerCount) + 1;
                    unsigned matchIndex = resultPrev + delta;
                    return (compareIndex >= matchIndex) ? -1 : +1;
                }
                else if (nextFinger < nextSearch)
                {
                    //The entry is < search => keep looping
                }
                else
                {
                    if (bytesPerCount == 0)
                    {
                        resultPrev += i;
                        resultNext = resultPrev+1;
                    }
                    else
                    {
                        //Exact match.  Reduce the range of the match counts using the running counts
                        //stored for each of the options, and continue matching.
                        resultNext = resultPrev + readBytesEntry32(counts, i, bytesPerCount)+1;
                        if (i > 0)
                            resultPrev += readBytesEntry32(counts, i-1, bytesPerCount)+1;
                    }

                    //If the compareIndex is < the lower bound for the match index the search value must be higher
                    if (compareIndex < resultPrev)
                        return +1;

                    //If the compareIndex is >= the upper bound for the match index the search value must be lower
                    if (compareIndex >= resultNext)
                        return -1;

                    const byte * offsets = counts + numOptions * bytesPerCount;
                    const byte * next = offsets + (numOptions-1) * bytesPerOffset;
                    finger = next;
                    if (i > 0)
                        finger += readBytesEntry32(offsets, i-1, bytesPerOffset);
                    search++;
                    offset++;
                    //Use a goto because we can't continue the outer loop from an inner loop
                    goto nextTree;
                }
            }

            //Search is > all elements
            return +1;
        }
        }

    nextTree:
        ;
    }

    return 0;
}

inline unsigned reverseBytes(unsigned value) { return __builtin_bswap32(value); }

//Find the first row that is >= the search row
unsigned InplaceNodeSearcher::findGE(const unsigned len, const byte * search) const
{
    unsigned resultPrev = 0;
    unsigned resultNext = count;
    const byte * finger = nodeData;
    unsigned offset = 0;

    while (offset < keyLen)
    {
        byte next = *finger++;
        SquashOp op = (SquashOp)(next >> 5);
        unsigned count = (next & 0x1f);
        if (count == 0)
            count = 32 + *finger++;

        switch (op)
        {
        case SqQuote:
        {
            unsigned numBytes = count;
            for (unsigned i=0; i < numBytes; i++)
            {
                const byte nextSearch = search[i];
                const byte nextFinger = finger[i];
                if (nextFinger != nextSearch)
                {
                    if (offset + i >= keyLen)
                        return resultPrev;
                    if (nextFinger > nextSearch)
                    {
                        //This entry is larger than the search value => we have a match
                        return resultPrev;
                    }
                    else
                    {
                        //This entry (and all children) are less than the search value
                        //=> the next entry is the match
                        return resultNext;
                    }
                }
            }
            search += numBytes;
            offset += numBytes;
            finger += numBytes;
            break;
        }
        case SqZero:
        case SqSpace:
        {
            const byte nextFinger = "\x00\x20"[op];
            unsigned numBytes = count + repeatDelta;
            for (unsigned i=0; i < numBytes; i++)
            {
                const byte nextSearch = search[i];
                if (nextFinger != nextSearch)
                {
                    if (offset + i >= keyLen)
                        return resultPrev;
                    if (nextFinger > nextSearch)
                        return resultPrev;
                    else
                        return resultNext;
                }
            }
            search += numBytes;
            offset += numBytes;
            break;
        }
        case SqRepeat:
        {
            const byte nextFinger = *finger++;
            unsigned numBytes = count + repeatXDelta;
            for (unsigned i=0; i < numBytes; i++)
            {
                const byte nextSearch = search[i];
                if (nextFinger != nextSearch)
                {
                    if (offset + i >= keyLen)
                        return resultPrev;
                    if (nextFinger > nextSearch)
                        return resultPrev;
                    else
                        return resultNext;
                }
            }
            search += numBytes;
            offset += numBytes;
            break;
        }
        case SqOption:
        {
        const unsigned numOptions = count+1;
            byte sizeInfo = *finger++;
            //Top two bits are currently spare - it may make sense to move before count and use them for repetition
            byte bytesPerCount = (sizeInfo >> 3) & 7; // could pack other information in....
            byte bytesPerOffset = (sizeInfo & 7);

            //MORE: Duplicates can occur on the last byte of the key - if so we have a match
            if (bytesPerOffset == 0)
            {
                dbgassertex(resultNext == resultPrev+numOptions);
                break;
            }

            const byte * counts = finger + numOptions; // counts follow the data
            const byte nextSearch = search[0];
            for (unsigned i=0; i < numOptions; i++)
            {
                const byte nextFinger = finger[i];
                if (nextFinger > nextSearch)
                {
                    //This entry is greater than search => this is the correct entry
                    if (bytesPerCount == 0)
                        return resultPrev + i;
                    if (i == 0)
                        return resultPrev;
                    return resultPrev + readBytesEntry32(counts, i-1, bytesPerCount) + 1;
                }
                else if (nextFinger < nextSearch)
                {
                    //The entry is < search => keep looping
                }
                else
                {
                    if (bytesPerCount == 0)
                    {
                        resultPrev += i;
                        resultNext = resultPrev+1;
                    }
                    else
                    {
                        //Exact match.  Reduce the range of the match counts using the running counts
                        //stored for each of the options, and continue matching.
                        resultNext = resultPrev + readBytesEntry32(counts, i, bytesPerCount)+1;
                        if (i > 0)
                            resultPrev += readBytesEntry32(counts, i-1, bytesPerCount)+1;
                    }

                    const byte * offsets = counts + numOptions * bytesPerCount;
                    const byte * next = offsets + (numOptions-1) * bytesPerOffset;
                    finger = next;
                    if (i > 0)
                        finger += readBytesEntry32(offsets, i-1, bytesPerOffset);
                    search++;
                    offset++;
                    //Use a goto because we can't continue the outer loop from an inner loop
                    goto nextTree;
                }
            }

            //Did not match any => next value matches
            return resultNext;
        }
        }

    nextTree:
        ;
    }

    return resultPrev;
}

bool InplaceNodeSearcher::getValueAt(unsigned int searchIndex, char *key) const
{
    if (searchIndex >= count)
        return false;

    unsigned resultPrev = 0;
    unsigned resultNext = count;
    const byte * finger = nodeData;
    unsigned offset = 0;

    while (offset < keyLen)
    {
        byte next = *finger++;
        SquashOp op = (SquashOp)(next >> 5);
        unsigned count = (next & 0x1f);
        if (count == 0)
            count = 32 + *finger++;

        switch (op)
        {
        case SqQuote:
        {
            unsigned i=0;
            unsigned numBytes = count;
            for (; i < numBytes; i++)
                key[offset+i] = finger[i];
            offset += numBytes;
            finger += numBytes;
            break;
        }
        case SqZero:
        case SqSpace:
        {
            const byte nextFinger = "\x00\x20"[op];
            unsigned numBytes = count + repeatDelta;
            for (unsigned i=0; i < numBytes; i++)
                key[offset+i] = nextFinger;
            offset += numBytes;
            break;
    }
        case SqRepeat:
        {
            const byte nextFinger = *finger++;
            unsigned numBytes = count + repeatXDelta;
            for (unsigned i=0; i < numBytes; i++)
                key[offset+i] = nextFinger;

            offset += numBytes;
            break;
        }
        case SqOption:
        {
            const unsigned numOptions = count+1;
            byte sizeInfo = *finger++;
            //Top two bits are currently spare - it may make sense to move before count and use them for repetition
            byte bytesPerCount = (sizeInfo >> 3) & 7; // could pack other information in....
            byte bytesPerOffset = (sizeInfo & 7);

            //MORE: Duplicates can occur after the last byte of the key - bytesPerOffset is set to 0 if this occurs
            if (bytesPerOffset == 0)
                break;

            const byte * counts = finger + numOptions; // counts follow the data
            unsigned option = 0;
            unsigned countPrev = 0;
            unsigned countNext = 1;
            if (bytesPerCount == 0)
            {
                option = searchIndex - resultPrev;
                countPrev = option;
                countNext = option+1;
            }
            else
            {
                countPrev = 0;
                for (unsigned i=0; i < numOptions; i++)
                {
                    countNext = readBytesEntry32(counts, i, bytesPerCount)+1;
                    if (searchIndex < resultPrev + countNext)
                    {
                        option = i;
                        break;
                    }
                    countPrev = countNext;
                }
            }
            key[offset++] = finger[option];

            resultNext = resultPrev + countNext;
            resultPrev = resultPrev + countPrev;

            const byte * offsets = counts + numOptions * bytesPerCount;
            const byte * next = offsets + (numOptions-1) * bytesPerOffset;
            finger = next;
            if (option > 0)
                finger += readBytesEntry32(offsets, option-1, bytesPerOffset);
            break;
        }
        }
    }

    return true;
}

int InplaceNodeSearcher::compareValueAtFallback(const char *src, unsigned int index) const
{
    char temp[256] = { 0 };
    getValueAt(index, temp);
    return strcmp(src, temp);
}


//---------------------------------------------------------------------------------------------------------------------

int CJHInplaceTreeNode::compareValueAt(const char *src, unsigned int index) const
{
    dbgassertex(index < hdr.numKeys);
    return searcher.compareValueAt(src, index);
}

int CJHInplaceTreeNode::locateGE(const char * search, unsigned minIndex) const
{
#if 0
    return CJHTreeNode::locateGE(search, minIndex);
#else
    if (hdr.numKeys == 0) return 0;

    CCycleTimer timer;
    unsigned int match = searcher.findGE(keyLen, (const byte *)search);
    if (match < minIndex)
        match = minIndex;
    unsigned __int64 elapsed = timer.elapsedCycles();
    if (isBranch())
        branchSearchCycles += elapsed;
    else
        leafSearchCycles += elapsed;
    return match;
#endif
}

offset_t CJHInplaceTreeNode::getFPosAt(unsigned int index) const
{
    if (index >= hdr.numKeys) return 0;

    offset_t delta = 0;
    if ((bytesPerPosition > 0) && (index != 0))
        delta = readBytesEntry64(positionData, index-1, bytesPerPosition);
    else
        delta = index;

    if (scaleFposByNodeSize)
        delta *= getNodeSize();

    return firstSequence + delta;
}

bool CJHInplaceTreeNode::getValueAt(unsigned int index, char *dst) const
{
    if (index >= hdr.numKeys) return false;
    if (dst)
    {
        searcher.getValueAt(index, dst);
        throwUnexpected();  // The following logic needs to be checked once we are using this for leaf nodes.
#if 0
        if (keyHdr->hasSpecialFileposition())
        {
            //It would make sense to have the fileposition at the start of the row from the perspective of the
            //internal representation, but that would complicate everything else which assumes the keyed
            //fields start at the beginning of the row.
            const char * p = keyBuf + index*keyRecLen;
            memcpy(dst, p + sizeof(offset_t), keyLen);
            memcpy(dst+keyLen, p, sizeof(offset_t));
        }
        else
        {
            const char * p = keyBuf + index*keyRecLen;
            memcpy(dst, p, keyLen);
        }
    #endif
    }
    return true;
}


size32_t CJHInplaceTreeNode::getSizeAt(unsigned int index) const
{
    //MORE: Change for variable length keys
    if (keyHdr->hasSpecialFileposition())
        return keyLen + sizeof(offset_t);
    else
        return keyLen;
}

void CJHInplaceTreeNode::load(CKeyHdr *_keyHdr, const void *rawData, offset_t _fpos, bool needCopy)
{
    CJHTreeNode::load(_keyHdr, rawData, _fpos, needCopy);

    const byte * nullRow = nullptr; //MORE: This should be implemented
    unsigned numKeys = hdr.numKeys;
    if (numKeys)
    {
        size32_t len = hdr.keyBytes;
        const size32_t padding = 8 - 1; // Ensure that unsigned8 values can be read "legally"
        const byte * data = ((const byte *)rawData) + sizeof(hdr);
        keyBuf = (char *) allocMem(len + padding);
        memcpy(keyBuf, data, len);
        memset(keyBuf+len, 0, padding);
        expandedSize = len;

        //Filepositions are stored as a packed base and an (optional) list of scaled compressed deltas
        data = (const byte *)keyBuf;
        byte sizeMask = *data++;
        if (sizeMask & 0x80)
            scaleFposByNodeSize = true;
        bytesPerPosition = (sizeMask & 0x7f);

        firstSequence = readPacked64(data);
        positionData = data;

        if (bytesPerPosition != 0)
            data += (bytesPerPosition * (numKeys -1));

        searcher.init(numKeys, data, keyLen, nullRow);
    }
}


//=========================================================================================================

CInplaceBranchWriteNode::CInplaceBranchWriteNode(offset_t _fpos, CKeyHdr *_keyHdr, const byte * _nullRow)
: CWriteNode(_fpos, _keyHdr, false), builder(keyLen, _nullRow, false), nullRow(_nullRow)
{
    nodeSize = _keyHdr->getNodeSize();
}

bool CInplaceBranchWriteNode::add(offset_t pos, const void * _data, size32_t size, unsigned __int64 sequence)
{
    if (0xffff == hdr.numKeys)
        return false;

    const byte * data = (const byte *)_data;
    unsigned oldSize = getDataSize();
    builder.add(size, data);
    if (positions.ordinality())
        assertex(positions.tos() <= pos);
    positions.append(pos);
    if (getDataSize() > maxBytes-hdr.keyBytes)
    {
        if (getDataSize() > maxBytes-hdr.keyBytes)
        {
            builder.removeLast();
            positions.pop();
            unsigned nowSize = getDataSize();
            assertex(oldSize == nowSize);
            builder.trace();
            return false;
        }
    }
    if (scaleFposByNodeSize && ((pos % nodeSize) != 0))
        scaleFposByNodeSize = false;

    saveLastKey(data, size, sequence);
    return true;
}

unsigned CInplaceBranchWriteNode::getDataSize()
{
    if (positions.ordinality() == 0)
        return 0;

    //MORE: Cache this, and calculate the incremental increase in size

    //Compress the filepositions by
    //a) storing them as deltas from the first
    //b) scaling by nodeSize if possible.
    //c) storing in the minimum number of bytes possible.
    unsigned __int64 firstPosition = positions.item(0);
    unsigned __int64 maxDeltaPosition = positions.tos() - firstPosition;
    if (scaleFposByNodeSize)
        maxDeltaPosition /= nodeSize;
    unsigned bytesPerPosition = 0;
    if (maxDeltaPosition + 1 != positions.ordinality())
        bytesPerPosition = bytesRequired(maxDeltaPosition);

    unsigned posSize = 1 + sizePacked(firstPosition) + bytesPerPosition * (positions.ordinality() - 1);
    return posSize + builder.getSize();
}

void CInplaceBranchWriteNode::write(IFileIOStream *out, CRC32 *crc)
{
    hdr.keyBytes = getDataSize();

    MemoryBuffer data;
    data.setBuffer(maxBytes, keyPtr, false);
    data.setWritePos(0);

    if (positions.ordinality())
    {
        //Pack these by scaling and reducing the number of bytes
        unsigned __int64 firstPosition = positions.item(0);
        unsigned __int64 maxPosition = positions.tos() - firstPosition;
        if (scaleFposByNodeSize)
            maxPosition /= nodeSize;

        unsigned bytesPerPosition = 0;
        if (maxPosition + 1 != positions.ordinality())
            bytesPerPosition = bytesRequired(maxPosition);

        byte sizeMask = (byte)bytesPerPosition | (scaleFposByNodeSize ? 0x80 : 0);
        data.append(sizeMask);
        serializePacked(data, firstPosition);

        if (bytesPerPosition != 0)
        {
            for (unsigned i=1; i < positions.ordinality(); i++)
            {
                unsigned __int64 delta = positions.item(i) - firstPosition;
                if (scaleFposByNodeSize)
                    delta /= nodeSize;
                serializeBytes(data, delta, bytesPerPosition);
            }
        }

        builder.serialize(data);

        assertex(data.length() == getDataSize());
    }

    CWriteNode::write(out, crc);
}


//=========================================================================================================

CInplaceLeafWriteNode::CInplaceLeafWriteNode(offset_t _fpos, CKeyHdr *_keyHdr, const byte * _nullRow)
: CWriteNode(_fpos, _keyHdr, false), builder(keyLen, _nullRow, false), nullRow(_nullRow)
{
    nodeSize = _keyHdr->getNodeSize();
}

bool CInplaceLeafWriteNode::add(offset_t pos, const void * _data, size32_t size, unsigned __int64 sequence)
{
    if (0xffff == hdr.numKeys)
        return false;

    __uint64 savedMinPosition = minPosition;
    __uint64 savedMaxPosition = maxPosition;
    const byte * data = (const byte *)_data;
    unsigned oldSize = getDataSize();
    builder.add(size, data);
    if (positions.ordinality())
    {
        if (pos < minPosition)
            minPosition = pos;
        if (pos > maxPosition)
            maxPosition = pos;
    }
    else
    {
        minPosition = pos;
        maxPosition = pos;
    }

    positions.append(pos);
    if (getDataSize() > maxBytes-hdr.keyBytes)
    {
        if (getDataSize() > maxBytes-hdr.keyBytes)
        {
            builder.removeLast();
            positions.pop();
            minPosition = savedMinPosition;
            maxPosition = savedMaxPosition;
            unsigned nowSize = getDataSize();
            assertex(oldSize == nowSize);
            builder.trace();
            return false;
        }
    }

    saveLastKey(data, keyLen, sequence);
    return true;
}

unsigned CInplaceLeafWriteNode::getDataSize()
{
    if (positions.ordinality() == 0)
        return 0;

    //MORE: Cache this, and calculate the incremental increase in size

    //Compress the filepositions by
    //a) storing them as deltas from the first
    //b) scaling by nodeSize if possible.
    //c) storing in the minimum number of bytes possible.
    unsigned bytesPerPosition = 0;
    if (minPosition != maxPosition)
        bytesPerPosition = bytesRequired(maxPosition-minPosition);

    unsigned posSize = 1 + sizePacked(minPosition) + bytesPerPosition * positions.ordinality();
    return posSize + builder.getSize();
}

void CInplaceLeafWriteNode::write(IFileIOStream *out, CRC32 *crc)
{
    hdr.keyBytes = getDataSize();

    MemoryBuffer data;
    data.setBuffer(maxBytes, keyPtr, false);
    data.setWritePos(0);

    if (positions.ordinality())
    {
        //Pack these by scaling and reducing the number of bytes
        unsigned bytesPerPosition = 0;
        if (minPosition != maxPosition)
            bytesPerPosition = bytesRequired(maxPosition-minPosition);

        byte sizeMask = (byte)bytesPerPosition | (scaleFposByNodeSize ? 0x80 : 0);
        data.append(sizeMask);
        serializePacked(data, minPosition);

        if (bytesPerPosition != 0)
        {
            for (unsigned i=0; i < positions.ordinality(); i++)
            {
                unsigned __int64 delta = positions.item(i) - minPosition;
                serializeBytes(data, delta, bytesPerPosition);
            }
        }

        builder.serialize(data);

        assertex(data.length() == getDataSize());
    }

    CWriteNode::write(out, crc);
}


//=========================================================================================================

#ifdef _USE_CPPUNIT
#include "unittests.hpp"
#include "eclrtl.hpp"

class TestInplaceNodeSearcher : public InplaceNodeSearcher
{
public:
    TestInplaceNodeSearcher(unsigned _count, const byte * data, size32_t _keyLen, const byte * _nullRow) : InplaceNodeSearcher(_count,  data, _keyLen, _nullRow)
    {
    }

    void doFind(const char * search)
    {
        unsigned match = findGE(strlen(search), (const byte *)search);
        printf("('%s': %u) ", search, match);
    }

    void find(const char * search)
    {
        StringBuffer text;
        doFind(search);
        doFind(text.clear().append(strlen(search)-1, search));
        doFind(text.clear().append(strlen(search)-1, search).append('a'));
        doFind(text.clear().append(strlen(search)-1, search).append('z'));
        doFind(text.clear().append(search).append(' '));
        doFind(text.clear().append(search).append('\t'));
        doFind(text.clear().append(search).append('z'));
        printf("\n");
    }
};


static int normalizeCompare(int x)
{
    return (x < 0) ? -1 : (x > 0) ? 1 : 0;
}
class InplaceIndexTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( InplaceIndexTest  );
        //CPPUNIT_TEST(testBytesFromFirstTiming);
        CPPUNIT_TEST(testSearching);
    CPPUNIT_TEST_SUITE_END();

    void testBytesFromFirstTiming()
    {
        for (unsigned i=0; i <= 0xff; i++)
            assertex(numExtraBytesFromFirst1(i) == numExtraBytesFromFirst2(i));
        unsigned total = 0;
        {
            CCycleTimer timer;
            for (unsigned i=0; i < 0xffffff; i++)
                total += numExtraBytesFromFirst1(i);
            printf("%llu\n", timer.elapsedNs());
        }
        {
            CCycleTimer timer;
            for (unsigned i2=0; i2 < 0xffffff; i2++)
                total += numExtraBytesFromFirst2(i2);
            printf("%llu\n", timer.elapsedNs());
        }
        printf("%u\n", total);
    }

    void testSearching()
    {
        const size32_t keyLen = 8;
        bool optimizeTrailing = false;  // remove trailing bytes that match the null row.
        const byte * nullRow = (const byte *)"                                   ";
        PartialMatchBuilder builder(keyLen, nullRow, optimizeTrailing);

        const char * entries[] = {
            "abel    ",
            "abigail ",
            "absalom ",
            "barry   ",
            "barty   ",
            "bill    ",
            "brian   ",
            "briany  ",
            "charlie ",
            "charlie ",
            "chhhhhhs",
            "georgina",
            "georgina",
            "georginb",
            "jim     ",
            "jimmy   ",
            "john    ",
        };
        unsigned numEntries = _elements_in(entries);
        for (unsigned i=0; i < numEntries; i++)
        {
            const char * entry = entries[i];
            builder.add(strlen(entry), entry);
        }

        MemoryBuffer buffer;
        builder.serialize(buffer);
        DBGLOG("Raw size = %u", keyLen * numEntries);
        DBGLOG("Serialized size = %u", buffer.length());
        builder.trace();

        TestInplaceNodeSearcher searcher(builder.getCount(), buffer.bytes(), keyLen, nullRow);

        for (unsigned i=0; i < numEntries; i++)
        {
            const char * entry = entries[i];
            StringBuffer s;
            find(entry, [&searcher,&s,numEntries](const char * search) {
                unsigned match = searcher.findGE(strlen(search), (const byte *)search);
                s.appendf("('%s': %u) ", search, match);
                if (match > 0  && !(searcher.compareValueAt(search, match-1) >= 0))
                {
                    s.append("<");
                    //assertex(searcher.compareValueAt(search, match-1) >= 0);
                }
                if (match < numEntries && !(searcher.compareValueAt(search, match) <= 0))
                {
                    s.append(">");
                    //assertex(searcher.compareValueAt(search, match) <= 0);
                }
            });
            DBGLOG("%s", s.str());
        }

        for (unsigned i=0; i < numEntries; i++)
        {
            char result[256] = {0};
            const char * entry = entries[i];

            if (!searcher.getValueAt(i, result))
                printf("%u: getValue() failed\n", i);
            else if (!streq(entry, result))
                printf("%u: '%s', '%s'\n", i, entry, result);

            auto callback = [numEntries, entries, &searcher](const char * search)
            {
                for (unsigned j= 0; j < numEntries; j++)
                {
                    int expected = normalizeCompare(strcmp(search, entries[j]));
                    int actual = normalizeCompare(searcher.compareValueAt(search, j));
                    if (expected != actual)
                        printf("compareValueAt('%s', %u)=%d, expected %d\n", search, j, actual, expected);
                }
            };
            find(entry, callback);
        }

        exit(0);
    }

    void find(const char * search, std::function<void(const char *)> callback)
    {
        callback(search);

        unsigned searchLen = strlen(search);
        unsigned trimLen = rtlTrimStrLen(searchLen, search);
        StringBuffer text;
        text.clear().append(search).setCharAt(trimLen-1, ' '); callback(text);
        text.clear().append(search).setCharAt(trimLen-1, 'a'); callback(text);
        text.clear().append(search).setCharAt(trimLen-1, 'z'); callback(text);
        if (searchLen != trimLen)
        {
            text.clear().append(search).setCharAt(trimLen, '\t'); callback(text);
            text.clear().append(search).setCharAt(trimLen, 'a'); callback(text);
            text.clear().append(search).setCharAt(trimLen, 'z'); callback(text);
        }
    }

};

CPPUNIT_TEST_SUITE_REGISTRATION( InplaceIndexTest );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( InplaceIndexTest, "InplaceIndexTest" );

#endif


#if 0
//Old code:
int InplaceNodeSearcher::compareValueAt(const char * search, unsigned int compareIndex) const
{
    unsigned resultPrev = 0;
    unsigned resultNext = count;
    const byte * finger = nodeData;
    unsigned offset = 0;

    for (;;)
    {
        //An alternating sequence of match, opt blocks
        const unsigned numBytes = readPacked32(finger);
        for (unsigned i=0; i < numBytes; i++)
        {
            const byte nextSearch = search[i];
            const byte nextFinger = finger[i];
            if (nextFinger > nextSearch)
            {
                //This entry is larger than the search value => we have a match
                return -1;
            }
            else if (nextFinger < nextSearch)
            {
                //This entry (and all children) are less than the search value
                return +1;
            }
        }
        search += numBytes;
        offset += numBytes;
        if (resultNext == resultPrev+1)
            break;

        finger += numBytes;

        //Now an opt block
        const unsigned numOptions = readPacked32(finger) + 2;
        byte sizeInfo = *finger++;
        //Top two bits are currently spare - it may make sense to move before count and use them for repetition
        byte bytesPerCount = (sizeInfo >> 3) & 7; // could pack other information in....
        byte bytesPerOffset = (sizeInfo & 7);

        //MORE: Duplicates can occur on the last byte of the key - if so we have a match
        if (bytesPerOffset == 0)
            break;

        const byte * counts = finger + numOptions; // counts follow the data
        const byte nextSearch = search[0];
        for (unsigned i=0; i < numOptions; i++)
        {
            const byte nextFinger = finger[i];
            if (nextFinger > nextSearch)
            {
                //This entry is greater than search => item(i) is the correct entry
                if (i == 0)
                    return -1;

                unsigned delta;
                if (bytesPerCount == 0)
                    delta = i;
                else
                    delta = readBytesEntry32(counts, i-1, bytesPerCount) + 1;
                unsigned matchIndex = resultPrev + delta;
                return (compareIndex >= matchIndex) ? -1 : +1;
            }
            else if (nextFinger < nextSearch)
            {
                //The entry is < search => keep looping
            }
            else
            {
                if (bytesPerCount == 0)
                {
                    resultPrev += i;
                    resultNext = resultPrev+1;
                }
                else
                {
                    //Exact match.  Reduce the range of the match counts using the running counts
                    //stored for each of the options, and continue matching.
                    resultNext = resultPrev + readBytesEntry32(counts, i, bytesPerCount)+1;
                    if (i > 0)
                        resultPrev += readBytesEntry32(counts, i-1, bytesPerCount)+1;
                }

                //If the compareIndex is < the lower bound for the match index the search value must be higher
                if (compareIndex < resultPrev)
                    return +1;

                //If the compareIndex is >= the upper bound for the match index the search value must be lower
                if (compareIndex >= resultNext)
                    return -1;

                const byte * offsets = counts + numOptions * bytesPerCount;
                const byte * next = offsets + (numOptions-1) * bytesPerOffset;
                finger = next;
                if (i > 0)
                    finger += readBytesEntry32(offsets, i-1, bytesPerOffset);
                search++;
                offset++;
                //Use a goto because we can't continue the outer loop from an inner loop
                goto nextTree;
            }
        }
        //Search is > all elements
        return +1;

    nextTree:
        ;
    }

    //compare with the null value
    for (unsigned i=offset; i < keyLen; i++)
    {
        const byte nextSearch = search[i-offset];
        const byte nextFinger = nullRow[i];
        if (nextFinger > nextSearch)
            return -1;
        else if (nextFinger < nextSearch)
            return +1;
    }

    return 0;
}

inline unsigned reverseBytes(unsigned value) { return __builtin_bswap32(value); }

//Find the first row that is >= the search row
unsigned InplaceNodeSearcher::findGE(const unsigned len, const byte * search) const
{
    unsigned resultPrev = 0;
    unsigned resultNext = count;
    const byte * finger = nodeData;
    unsigned offset = 0;

    for (;;)
    {
        //An alternating sequence of match, opt blocks
        const unsigned numBytes = readPacked32(finger);
        unsigned i=0;
#if 0
        if (unlikely(numBytes >= 4))
        {
            do
            {
                //Technically undefined behaviour because the data was not originally
                //defined as unsigned values, access will be misaligned.
                const unsigned nextSearch = *(const unsigned *)(search + i);
                const unsigned nextFinger = *(const unsigned *)(finger + i);
                if (nextSearch != nextFinger)
                {
                    const unsigned revNextSearch = reverseBytes(nextSearch);
                    const unsigned revNextFinger = reverseBytes(nextFinger);
                    if (revNextFinger > revNextSearch)
                        return resultPrev;
                    else
                        return resultNext;
                }
                i += 4;
            } while (i + 4 <= numBytes);
        }
#endif

        for (; i < numBytes; i++)
        {
            const byte nextSearch = search[i];
            const byte nextFinger = finger[i];
            if (nextFinger > nextSearch)
            {
                //This entry is larger than the search value => we have a match
                return resultPrev;
            }
            else if (nextFinger < nextSearch)
            {
                //This entry (and all children) are less than the search value
                //=> the next entry is the match
                return resultNext;
            }
        }
        search += numBytes;
        offset += numBytes;
        if (resultNext == resultPrev+1)
            break;

        finger += numBytes;

        //Now an opt block
        const unsigned numOptions = readPacked32(finger) + 2;
        byte sizeInfo = *finger++;
        //Top two bits are currently spare - it may make sense to move before count and use them for repetition
        byte bytesPerCount = (sizeInfo >> 3) & 7; // could pack other information in....
        byte bytesPerOffset = (sizeInfo & 7);

        //MORE: Duplicates can occur on the last byte of the key - if so we have a match
        if (bytesPerOffset == 0)
        {
            dbgassertex(resultNext == resultPrev+numOptions);
            break;
        }

        const byte * counts = finger + numOptions; // counts follow the data
        const byte nextSearch = search[0];
        for (unsigned i=0; i < numOptions; i++)
        {
            const byte nextFinger = finger[i];
            if (nextFinger > nextSearch)
            {
                //This entry is greater than search => this is the correct entry
                if (bytesPerCount == 0)
                    return resultPrev + i;
                if (i == 0)
                    return resultPrev;
                return resultPrev + readBytesEntry32(counts, i-1, bytesPerCount) + 1;
            }
            else if (nextFinger < nextSearch)
            {
                //The entry is < search => keep looping
            }
            else
            {
                if (bytesPerCount == 0)
                {
                    resultPrev += i;
                    resultNext = resultPrev+1;
                }
                else
                {
                    //Exact match.  Reduce the range of the match counts using the running counts
                    //stored for each of the options, and continue matching.
                    resultNext = resultPrev + readBytesEntry32(counts, i, bytesPerCount)+1;
                    if (i > 0)
                        resultPrev += readBytesEntry32(counts, i-1, bytesPerCount)+1;
                }

                const byte * offsets = counts + numOptions * bytesPerCount;
                const byte * next = offsets + (numOptions-1) * bytesPerOffset;
                finger = next;
                if (i > 0)
                    finger += readBytesEntry32(offsets, i-1, bytesPerOffset);
                search++;
                offset++;
                //Use a goto because we can't continue the outer loop from an inner loop
                goto nextTree;
            }
        }
        //Did not match any => next value matches
        return resultNext;

    nextTree:
        ;
    }

    //compare with the null value
    for (unsigned i=offset; i < keyLen; i++)
    {
        const byte nextSearch = search[i-offset];
        const byte nextFinger = nullRow[i];
        if (nextFinger > nextSearch)
            return resultPrev;
        else if (nextFinger < nextSearch)
            return resultNext;
    }
    return resultPrev;
}

bool InplaceNodeSearcher::getValueAt(unsigned int searchIndex, char *key) const
{
    if (searchIndex >= count)
        return false;

    unsigned resultPrev = 0;
    unsigned resultNext = count;
    const byte * finger = nodeData;
    unsigned offset = 0;

    for (;;)
    {
        //An alternating sequence of match, opt blocks
        const unsigned numBytes = readPacked32(finger);
        for (unsigned i=0; i < numBytes; i++)
            key[offset+i] = finger[i];
        offset += numBytes;
        if (resultNext == resultPrev+1)
            break;

        finger += numBytes;

        //Now an opt block
        const unsigned numOptions = readPacked32(finger) + 2;
        byte sizeInfo = *finger++;
        //Top two bits are currently spare - it may make sense to move before count and use them for repetition
        byte bytesPerCount = (sizeInfo >> 3) & 7; // could pack other information in....
        byte bytesPerOffset = (sizeInfo & 7);

        //MORE: Duplicates can occur after the last byte of the key - bytesPerOffset is set to 0 if this occurs
        if (bytesPerOffset == 0)
            break;

        const byte * counts = finger + numOptions; // counts follow the data
        unsigned option = 0;
        unsigned countPrev = 0;
        unsigned countNext = 1;
        if (bytesPerCount == 0)
        {
            option = searchIndex - resultPrev;
            countPrev = option;
            countNext = option+1;
        }
        else
        {
            countPrev = 0;
            for (unsigned i=0; i < numOptions; i++)
            {
                countNext = readBytesEntry32(counts, i, bytesPerCount)+1;
                if (searchIndex < resultPrev + countNext)
                {
                    option = i;
                    break;
                }
                countPrev = countNext;
            }
        }
        key[offset++] = finger[option];

        resultNext = resultPrev + countNext;
        resultPrev = resultPrev + countPrev;

        const byte * offsets = counts + numOptions * bytesPerCount;
        const byte * next = offsets + (numOptions-1) * bytesPerOffset;
        finger = next;
        if (option > 0)
            finger += readBytesEntry32(offsets, option-1, bytesPerOffset);
    }

    //Finally pad with any values from the nullRow.
    for (unsigned i=offset; i < keyLen; i++)
        key[i] = nullRow[i];
    return true;
}

#endif