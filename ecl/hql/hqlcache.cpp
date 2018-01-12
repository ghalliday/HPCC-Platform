/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2018 HPCC Systems®.

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
#include "hqlcache.hpp"
#include "hqlcollect.hpp"
#include "hqlexpr.hpp"

//---------------------------------------------------------------------------------------------------------------------

class EclXmlCachedDefinition : public CInterfaceOf<IEclCachedDefinition>
{
public:
    EclXmlCachedDefinition(IEclCachedDefinitionCollection * _collection, IPropertyTree * _root, IEclSource * _definition)
    : collection(_collection), root(_root), definition(_definition) {}

    virtual timestamp_type getTimeStamp() const override;
    virtual bool isUpToDate() const override;
    virtual IFileContents * querySimplifiedEcl() const override;
    virtual void queryDependencies(StringArray & values) const override;

protected:
    bool calcUpToDate() const;
    const char * queryName() const { return root->queryProp("@name"); }

private:
    mutable bool cachedUpToDate = false;
    mutable bool upToDate = false;
    IEclCachedDefinitionCollection * collection = nullptr;
    Linked<IPropertyTree> root;
    Linked<IEclSource> definition;
};

timestamp_type EclXmlCachedDefinition::getTimeStamp() const
{
    if (!root)
        return 0;
    return root->getPropInt64("@ts");
}

bool EclXmlCachedDefinition::isUpToDate() const
{
    //MORE: Thread safety?
    if (!cachedUpToDate)
    {
        cachedUpToDate = true;
        upToDate = calcUpToDate();
    }
    return upToDate;
}

bool EclXmlCachedDefinition::calcUpToDate() const
{
    if (!root || !definition)
        return false;
    IFileContents * contents = definition->queryFileContents();
    if (!contents)
        return false;
    if (getTimeStamp() < contents->getTimeStamp())
        return false;

    StringArray dependencies;
    queryDependencies(dependencies);
    ForEachItemIn(i, dependencies)
    {
        Owned<IEclCachedDefinition> match = collection->getDefinition(dependencies.item(i));
        if (!match || !match->isUpToDate())
            return false;
    }
    return true;
}

IFileContents * EclXmlCachedDefinition::querySimplifiedEcl() const
{
    return nullptr;
}

void EclXmlCachedDefinition::queryDependencies(StringArray & values) const
{
    StringBuffer fullname;
    Owned<IPropertyTreeIterator> iter = root->getElements("Depend");
    ForEach(*iter)
    {
        const char * module = iter->query().queryProp("@module");
        const char * attr = iter->query().queryProp("@name");
        if (!isEmptyString(module))
        {
            fullname.clear().append(module).append(".").append(attr);
            values.append(fullname);
        }
        else
            values.append(attr);
    }
}

//---------------------------------------------------------------------------------------------------------------------

class EclCachedDefinitionCollection : public CInterfaceOf<IEclCachedDefinitionCollection>
{
public:
    EclCachedDefinitionCollection(IEclRepository * _repository)
    : repository(_repository){}

    virtual IEclCachedDefinition * getDefinition(const char * path) override;

protected:
    virtual IEclCachedDefinition * createDefinition(const char * lowerPath) = 0;

protected:
    Linked<IEclRepository> repository;
    MapStringToMyClass<IEclCachedDefinition> map;
};


IEclCachedDefinition * EclCachedDefinitionCollection::getDefinition(const char * path)
{
    StringBuffer lowerPath;
    lowerPath.append(path).toLowerCase();
    IEclCachedDefinition * match = map.getValue(lowerPath);
    if (match)
        return LINK(match);

    Owned<IEclCachedDefinition> cached = createDefinition(lowerPath);

    map.setValue(lowerPath, cached);
    return cached.getClear();
}

//---------------------------------------------------------------------------------------------------------------------

class EclXmlCachedDefinitionCollection : public EclCachedDefinitionCollection
{
public:
    EclXmlCachedDefinitionCollection(IEclRepository * _repository, IPropertyTree * _root)
    : EclCachedDefinitionCollection(_repository), root(_root) {}

    virtual IEclCachedDefinition * createDefinition(const char * lowerPath) override;

private:
    Linked<IPropertyTree> root;
};



IEclCachedDefinition * EclXmlCachedDefinitionCollection::createDefinition(const char * lowerPath)
{
    VStringBuffer xpath("Cache[@name='%s']", lowerPath);
    Owned<IPropertyTree> resolved = root->getBranch(xpath);
    Owned<IEclSource> definition = repository->getSource(lowerPath);
    return new EclXmlCachedDefinition(this, resolved, definition);
}

IEclCachedDefinitionCollection * createEclXmlCachedDefinitionCollection(IEclRepository * repository, IPropertyTree * root)
{
    return new EclXmlCachedDefinitionCollection(repository, root);
}

//---------------------------------------------------------------------------------------------------------------------
