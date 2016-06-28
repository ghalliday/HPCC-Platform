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
#include "rtldynfield.hpp"
#include "../../common/deftype/deftype.hpp"     // MORE: These constants should really move from this header

RtlDynFieldInfo * DynamicFieldTypeInstance::addField(const char * name, const char * xpath, const RtlTypeInfo * type)
{
    RtlDynFieldInfo * field = new RtlDynFieldInfo(name, xpath, type);
    fields.emplace_back(field);
    return field;
}

void DynamicFieldTypeInstance::addType(RtlTypeInfo * type)
{
    types.emplace_back(type);
}


void DynamicFieldTypeInstance::expandFields(RtlFieldArray & target, const RtlFieldInfo * record)
{
    StringBuffer prefix;
    return expandFields(target, record, prefix);
}



void DynamicFieldTypeInstance::expandFields(RtlFieldArray & target, const RtlFieldInfo * field, StringBuffer & prefix)
{
    switch (field->type->fieldType)
    {
    case type_row:
    {
        StringLengthPreserver preserveLength(prefix);
        if (field->name && *field->name)
            prefix.append(field->name).append(".");
        expandFields(target, field->type->queryChildType(), prefix);
        break;
    }
    case type_record:
        expandFields(target, field->type, prefix);
        break;
    default:
        if (prefix.length() != 0)
        {
            StringLengthPreserver preserveLength(prefix);
            if (field->name && *field->name)
                prefix.append(field->name).append(".");
            expandFields(target, field->type, prefix);
        }
        else
            target.append(field);
        break;
    }
}

void DynamicFieldTypeInstance::expandFields(RtlFieldArray & target, const RtlTypeInfo * record, StringBuffer & prefix)
{
    assertex(record->fieldType == type_record);
    const RtlFieldInfo * const * fields = record->queryFields();
    for (; *fields; fields++)
        expandFields(target, *fields, prefix);
}

DynamicFieldTypeInstance * createDynamicTypeInfo()
{
    return new DynamicFieldTypeInstance;
}

