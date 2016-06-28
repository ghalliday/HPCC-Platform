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
#include "jliball.hpp"
#include "hql.hpp"
#include "eclrtl.hpp"

#include "platform.h"
#include "jlib.hpp"
#include "hqlfield.hpp"

#include "hqlexpr.hpp"
#include "hqlattr.hpp"
#include "hqlutil.hpp"

class FieldInfoCreator
{
public:
    RtlFieldInfo * getField(IHqlExpression * expr);
    RtlTypeInfo * getType(ITypeInfo * type, unsigned typeFlags);

protected:
    unsigned getFieldList(PointerArrayOf<RtlFieldInfo> & fields, IHqlExpression * record, IHqlExpression * rowRecord);

protected:
    DynamicFieldTypeInstance & instance;
    bool expandRows;
};

RtlFieldInfo * FieldInfoCreator::getField(IHqlExpression * expr)
{
    return nullptr;
}

RtlTypeInfo * FieldInfoCreator::getType(ITypeInfo * type, unsigned typeFlags)
{
    type_t tc = type->getTypeCode();
    unsigned fieldType = typeFlags;
    if (tc == type_alien)
    {
        ITypeInfo * physicalType = queryAlienType(type)->queryPhysicalType();
        if (physicalType->getSize() != UNKNOWN_LENGTH)
        {
            //Don't use the generated class for xml generation since it will generate physical rather than logical
            fieldType |= (RFTMalien|RFTMinvalidxml);
            type = physicalType;
            tc = type->getTypeCode();
        }
        else
        {
            fieldType |= RFTMunknownsize;
            //can't work out the size of the field - to keep it as unknown for the moment.
            //until the alien field type is supported
        }
    }
    fieldType |= tc;
    unsigned length = type->getSize();
    if (length == UNKNOWN_LENGTH)
    {
        fieldType |= RFTMunknownsize;
        length = 0;
    }

    RtlTypeInfo * childType = nullptr;
    RtlTypeInfo * info = nullptr;
    switch (tc)
    {
    case type_boolean:
        info = new RtlBoolTypeInfo(fieldType, length);
        break;
    case type_real:
        info = new RtlRealTypeInfo(fieldType, length);
        break;
    case type_date:
    case type_enumerated:
    case type_int:
        if (!type->isSigned())
            fieldType |= RFTMunsigned;
        info = new RtlIntTypeInfo(fieldType, length);
        break;
    case type_swapint:
        if (!type->isSigned())
            fieldType |= RFTMunsigned;
        info = new RtlSwapIntTypeInfo(fieldType, length);
        break;
    case type_packedint:
        if (!type->isSigned())
            fieldType |= RFTMunsigned;
        info = new RtlPackedIntTypeInfo(fieldType, length);
        break;
    case type_decimal:
        if (!type->isSigned())
            fieldType |= RFTMunsigned;
        length = type->getDigits() | (type->getPrecision() << 16);
        info = new RtlDecimalTypeInfo(fieldType, length);
        break;
    case type_char:
        info = new RtlCharTypeInfo(fieldType, length);
        break;
    case type_data:
        info = new RtlDataTypeInfo(fieldType, length);
        break;
    case type_qstring:
        length = type->getStringLen();
        info = new RtlQStringTypeInfo(fieldType, length);
        break;
    case type_varstring:
        if (type->queryCharset() && type->queryCharset()->queryName()==ebcdicAtom)
            fieldType |= RFTMebcdic;
        length = type->getStringLen();
        info = new RtlVarStringTypeInfo(fieldType, length);
        break;
    case type_string:
        if (type->queryCharset() && type->queryCharset()->queryName()==ebcdicAtom)
            fieldType |= RFTMebcdic;
        info = new RtlStringTypeInfo(fieldType, length);
        break;
    case type_bitfield:
    {
        return nullptr;
        /*
        unsigned size = type->getSize();
        unsigned bitsize = type->getBitSize();
        unsigned offset = (unsigned)getIntValue(queryAttributeChild(type, bitfieldOffsetAtom, 0),-1);
        bool isLastBitfield = (queryAttribute(type, isLastBitfieldAtom) != NULL);
        if (isLastBitfield)
            fieldType |= RFTMislastbitfield;
        if (!type->isSigned())
            fieldType |= RFTMunsigned;
        length = size | (bitsize << 8) | (offset << 16);
        info = new RtlBitfieldTypeInfo(fieldType, length);
        break;
        */
    }
    case type_record:
    {
        IHqlExpression * record = ::queryRecord(type);
        PointerArrayOf<RtlFieldInfo> childFields;
        unsigned childType = getFieldList(childFields, record, record);
        if (childType & RFTMcontainsunknown)
            return nullptr;

        fieldType |= (childType & (RFTMcontainsunknown|RFTMinvalidxml|RFTMhasxmlattr));
        //          fieldType |= (childType & RFTMcontainsifblock);
        length = getMinRecordSize(record);
        if (isVariableSizeRecord(record))
            fieldType |= RFTMunknownsize;
        unsigned numFields = childFields.ordinality();
        const RtlFieldInfo * * fields = new const RtlFieldInfo *[numFields + 1];
        ForEachItemIn(i, childFields)
            fields[i] = childFields.item(i);
        fields[numFields] = nullptr;
        info = new RtlRecordTypeInfo(fieldType, length, fields);
        break;
    }
    case type_row:
    {
        childType = getType(::queryRecordType(type), 0);
        if (!childType)
            return nullptr;
        fieldType |= (childType->fieldType & (RFTMcontainsunknown|RFTMinvalidxml|RFTMhasxmlattr));
        if (hasLinkCountedModifier(type))
            fieldType |= RFTMlinkcounted;
        info = new RtlRowTypeInfo(fieldType, length, childType);
        break;
    }
    case type_table:
    case type_groupedtable:
    {
        childType = getType(::queryRecordType(type), 0);
        if (!childType)
            return nullptr;
        fieldType |= (childType->fieldType & (RFTMcontainsunknown|RFTMinvalidxml|RFTMhasxmlattr));
        if (hasLinkCountedModifier(type))
            fieldType |= RFTMlinkcounted;
        info = new RtlDatasetTypeInfo(fieldType, length, childType);
        break;
    }
    case type_dictionary:
    {
        return nullptr;
        childType = getType(::queryRecordType(type), 0);
        if (!childType)
            return nullptr;
        fieldType |= (childType->fieldType & (RFTMcontainsunknown|RFTMinvalidxml|RFTMhasxmlattr));
        if (hasLinkCountedModifier(type))
            fieldType |= RFTMlinkcounted;
        IHThorHashLookupInfo * hasher = nullptr; // more
        info = new RtlDictionaryTypeInfo(fieldType, length, childType, hasher);
        break;
    }
    case type_set:
        childType = getType(type->queryChildType(), 0);
        if (!childType)
            return nullptr;
        info = new RtlSetTypeInfo(fieldType, length, childType);
        break;
    case type_unicode:
        length = type->getStringLen();
        info = new RtlUnicodeTypeInfo(fieldType, length, str(type->queryLocale()));
        break;
    case type_varunicode:
        length = type->getStringLen();
        info = new RtlVarUnicodeTypeInfo(fieldType, length, str(type->queryLocale()));
        break;
    case type_utf8:
        length = type->getStringLen();
        info = new RtlUtf8TypeInfo(fieldType, length, str(type->queryLocale()));
        break;
    case type_blob:
    case type_pointer:
    case type_class:
    case type_array:
    case type_void:
    case type_alien:
    case type_none:
    case type_any:
    case type_pattern:
    case type_rule:
    case type_token:
    case type_feature:
    case type_event:
    case type_null:
    case type_scope:
    case type_transform:
    default:
        return nullptr;
        fieldType |= (RFTMcontainsunknown|RFTMinvalidxml);
        info = new RtlUnimplementedTypeInfo(fieldType, length);
        break;
    }

    instance.addType(info);
    return info;
}


unsigned FieldInfoCreator::getFieldList(PointerArrayOf<RtlFieldInfo> & fields, IHqlExpression * record, IHqlExpression * rowRecord)
{
    unsigned fieldType = 0;
    ForEachChild(i, record)
    {
        IHqlExpression * cur = record->queryChild(i);
        StringBuffer next;
        unsigned childType = 0;
        switch (cur->getOperator())
        {
        case no_field:
        case no_ifblock:
        {
            RtlFieldInfo * field = getField(cur);
            if (!field)
                return RFTMcontainsunknown;
            fields.append(field);
            childType = field->type->fieldType;
            break;
        }
        case no_record:
            childType = getFieldList(fields, cur, rowRecord);
            break;
        }
        fieldType |= (childType & (RFTMcontainsunknown|RFTMinvalidxml|RFTMhasxmlattr));
    }
    return fieldType;
}

RtlFieldInfo * createRtlFieldInfo(DynamicFieldTypeInstance & instance, IHqlExpression * record, bool expandRows)
{
    return nullptr;
}
