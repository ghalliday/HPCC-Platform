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

// This header must not be included by the generated code

#ifndef rtldynfield_hpp
#define rtldynfield_hpp

#include "rtlfield.hpp"
#include <vector>
#include <memory>
#include "jlib.hpp"

//These classes support the dynamic creation of type and field information

struct ECLRTL_API RtlDynFieldInfo : public RtlFieldInfo
{
public:
    RtlDynFieldInfo(const char * _name, const char * _xpath, const RtlTypeInfo * _type)
    : RtlFieldInfo(_name, _xpath, _type, nullptr)
    {
    }
    ~RtlDynFieldInfo()
    {
        free(const_cast<char *>(name));
        free(const_cast<char *>(xpath));
    }
};


//-------------------------------------------------------------------------------------------------------------------

struct ECLRTL_API RtlDynRecordTypeInfo : public RtlRecordTypeInfo
{
    inline RtlDynRecordTypeInfo(unsigned _fieldType, unsigned _length, const RtlFieldInfo * const * _fields) : RtlRecordTypeInfo(_fieldType, _length, _fields) {}
    ~RtlDynRecordTypeInfo() { delete[] fields; }
};

//-------------------------------------------------------------------------------------------------------------------

typedef ConstPointerArrayOf<const RtlFieldInfo> RtlFieldArray;

class ECLRTL_API DynamicFieldTypeInstance : public CInterface
{
public:
    DynamicFieldTypeInstance() {}
    DynamicFieldTypeInstance(const DynamicFieldTypeInstance &) = delete; // must be deleted because of the vectors of unique_ptrs
    DynamicFieldTypeInstance & operator = (const DynamicFieldTypeInstance &) = delete;

    RtlDynFieldInfo * addField(const char * name, const char * xpath, const RtlTypeInfo * type);
    void addType(RtlTypeInfo * type);

    void expandFields(RtlFieldArray &, const RtlFieldInfo * type);

protected:
    void expandFields(RtlFieldArray &, const RtlFieldInfo * type, StringBuffer & prefix);
    void expandFields(RtlFieldArray &, const RtlTypeInfo * type, StringBuffer & prefix);

protected:
    std::vector<std::unique_ptr<RtlITypeInfo>> types;
    std::vector<std::unique_ptr<RtlDynFieldInfo>> fields;
};

#endif
