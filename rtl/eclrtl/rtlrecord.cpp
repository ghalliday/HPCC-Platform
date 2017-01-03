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

#include "platform.h"
#include <math.h>
#include <stdio.h>
#include "jmisc.hpp"
#include "jlib.hpp"
#include "eclhelper.hpp"
#include "eclrtl_imp.hpp"
#include "rtlrecord.hpp"


RtlFieldOffsetCalculator::RtlFieldOffsetCalculator(RtlFieldArray & fields)
{
    //Expand the record definition to a list of types.
    numFields = fields.ordinality();
    types = new const RtlTypeInfo * [numFields];
    //MORE: Does not cope with ifblocks.
    numVarFields = 0;
    for (unsigned i=0; i < numFields; i++)
    {
        types[i] = fields.item(i)->type;
        if (!types[i]->isFixedSize())
            numVarFields++;
    }
    fixedOffsets = new size_t[numFields + 1];
    variableIndex = new unsigned[numFields + 1];
    variableFields = new size_t[numVarFields];
    variableTotals = new size_t[numVarFields+1];

    unsigned curVariable = 0;
    variableTotals[0] = 0;
    size_t totalFixed = 0;
    for (unsigned i=0;; i++)
    {
        variableIndex[i] = curVariable;
        fixedOffsets[i] = totalFixed;
        if (i == numFields)
            break;
        if (types[i]->isFixedSize())
        {
            size_t thisSize = types[i]->size(nullptr, nullptr);
            totalFixed += thisSize;
        }
        else
        {
            variableFields[curVariable] = i;
            curVariable++;
            //variableTotals filled in for a particular row
        }
    }

}

void RtlFieldOffsetCalculator::setRow(const void * _row)
{
    const byte * row = static_cast<const byte *>(_row);
    size_t totalVarSize = 0;
    for (unsigned i = 0; i < numVarFields; i++)
    {
        unsigned fieldIndex = variableFields[i];
        size_t offset = fixedOffsets[fieldIndex] + totalVarSize;
        size_t fieldSize = types[fieldIndex]->size(row + offset, row);
        totalVarSize += fieldSize;
        variableTotals[i] = totalVarSize;
    }
}

//---------------------------------------------------------------------------------------------------------------------
