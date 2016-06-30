/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2016 HPCC Systems®.

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

#ifndef rtlrecord_hpp
#define rtlrecord_hpp

#include "rtlfield.hpp"
#include "rtldynfield.hpp"

//These classe provides a relatively efficient way to access fields within a variable length record structure.
// Probably convert to an interface with various concrete implementations for varing degrees of complexity
struct ECLRTL_API RtlFieldOffsetCalculator
{
public:
    RtlFieldOffsetCalculator(RtlFieldArray & fields);

    void setRow(const void * row);

    size_t getOffset(unsigned field) const
    {
        return fixedOffsets[field] + variableTotals[variableIndex[field]];
    }

    size_t getRecordSize() const
    {
        return fixedOffsets[numFields] + variableTotals[numVarFields];
    }

    virtual size32_t getFixedSize() const
    {
        return  numVarFields ? 0 : fixedOffsets[numFields];
    }

    virtual size32_t getMinRecordSize() const
    {
        return fixedOffsets[numFields] + numVarFields * sizeof(size32_t);
    }

protected:
    size_t * fixedOffsets;        // fixed portion of the field offsets + 1 extra
    unsigned * variableIndex;       // which variable offset should be added to the fixed
    size_t * variableFields;      // map variable field to real field id.
    size_t * variableTotals;       // [0 + 1 entry for each variable size field ]
    unsigned numFields;
    unsigned numVarFields;
    const RtlTypeInfo * * types;
};

class ECLRTL_API RtlRecordSize : CInterfaceOf<IRecordSize>
{
    RtlRecordSize(RtlFieldArray & fields) : offsetCalculator(fields) {}

    virtual size32_t getRecordSize(const void *rec)
    {
        assertex(rec);
        offsetCalculator.setRow(rec);
        return offsetCalculator.getRecordSize();
    }

    virtual size32_t getFixedSize()
    {
        return offsetCalculator.getFixedSize();
    }
    // returns 0 for variable row size
    virtual size32_t getMinRecordSize() const
    {
        return offsetCalculator.getMinRecordSize();
    }

protected:
    RtlFieldOffsetCalculator offsetCalculator;
};

#endif
