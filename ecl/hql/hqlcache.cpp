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
#include "hqlutil.hpp"
#include "hqlerrors.hpp"
#include "junicode.hpp"
#include "hqlplugins.hpp"

//---------------------------------------------------------------------------------------------------------------------

class EclCachedDefinition : public CInterfaceOf<IEclCachedDefinition>
{
public:
    EclCachedDefinition(IEclCachedDefinitionCollection * _collection, IEclSource * _definition)
    : collection(_collection), definition(_definition) {}

    virtual bool isUpToDate() const override;
    virtual IEclSource * queryOriginal() const override { return definition; }

protected:
    virtual bool calcUpToDate() const;

protected:
    mutable bool cachedUpToDate = false;
    mutable bool upToDate = false;
    IEclCachedDefinitionCollection * collection = nullptr;
    Linked<IEclSource> definition;
};

bool EclCachedDefinition::isUpToDate() const
{
    //MORE: Thread safety?
    if (!cachedUpToDate)
    {
        cachedUpToDate = true;
        upToDate = calcUpToDate();
    }
    return upToDate;
}

bool EclCachedDefinition::calcUpToDate() const
{
    if (!definition)
        return false;
    IFileContents * contents = definition->queryFileContents();
    if (!contents)
        return false;

    //If the cached information is younger than the original ecl (or if the original no longer exists) then not valid
    timestamp_type originalTs = contents->getTimeStamp();
    if ((originalTs == 0) || (getTimeStamp() < originalTs))
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


//---------------------------------------------------------------------------------------------------------------------

class EclXmlCachedDefinition : public EclCachedDefinition
{
public:
    EclXmlCachedDefinition(IEclCachedDefinitionCollection * _collection, IEclSource * _definition, IPropertyTree * _root)
    : EclCachedDefinition(_collection, _definition), root(_root) {}

    virtual timestamp_type getTimeStamp() const override;
    virtual IFileContents * querySimplifiedEcl() const override;
    virtual void queryDependencies(StringArray & values) const override;
    virtual bool hasKnownDependents() const override
    {
        return !root->getPropBool("@isMacro");
    }

protected:
    virtual bool calcUpToDate() const override
    {
        if (!root)
            return false;
        return EclCachedDefinition::calcUpToDate();
    }


    const char * queryName() const { return root->queryProp("@name"); }

private:
    Linked<IPropertyTree> root;
};

timestamp_type EclXmlCachedDefinition::getTimeStamp() const
{
    if (!root)
        return 0;
    return root->getPropInt64("@ts");
}

IFileContents * EclXmlCachedDefinition::querySimplifiedEcl() const
{
    return nullptr;
}

void EclXmlCachedDefinition::queryDependencies(StringArray & values) const
{
    if (!root)
        return;

    StringBuffer fullname;
    Owned<IPropertyTreeIterator> iter = root->getElements("Attr/Depend");
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

class EclFileCachedDefinition : public EclXmlCachedDefinition
{
public:
    EclFileCachedDefinition(IEclCachedDefinitionCollection * _collection, IEclSource * _definition, IPropertyTree * _root, IFile * _file)
    : EclXmlCachedDefinition(_collection, _definition, _root), file(_file)
    {
    }

    virtual timestamp_type getTimeStamp() const override;

private:
    Linked<IFile> file;
};

timestamp_type EclFileCachedDefinition::getTimeStamp() const
{
    if (!file)
        return 0;
    return ::getTimeStamp(file);
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
    return new EclXmlCachedDefinition(this, definition, resolved);
}

IEclCachedDefinitionCollection * createEclXmlCachedDefinitionCollection(IEclRepository * repository, IPropertyTree * root)
{
    return new EclXmlCachedDefinitionCollection(repository, root);
}

//---------------------------------------------------------------------------------------------------------------------

class EclFileCachedDefinitionCollection : public EclCachedDefinitionCollection
{
public:
    EclFileCachedDefinitionCollection(IEclRepository * _repository, const char * _root)
    : EclCachedDefinitionCollection(_repository), root(_root)
    {
        makeAbsolutePath(root, false);
        addPathSepChar(root);
    }

    virtual IEclCachedDefinition * createDefinition(const char * lowerPath) override;

private:
    StringBuffer root;
};


IEclCachedDefinition * EclFileCachedDefinitionCollection::createDefinition(const char * lowerPath)
{
    StringBuffer filename(root);
    convertSelectsToPath(filename, lowerPath);
    filename.append(".cache");

    Owned<IFile> file = createIFile(filename);
    Owned<IPropertyTree> root;
    if (file->exists())
    {
        try
        {
            root.setown(createPTree(*file));
        }
        catch (IException * e)
        {
            DBGLOG(e);
            e->Release();
        }
    }

    Owned<IEclSource> definition = repository->getSource(lowerPath);
    return new EclFileCachedDefinition(this, definition, root, file);
}


extern HQL_API IEclCachedDefinitionCollection * createEclFileCachedDefinitionCollection(IEclRepository * repository, const char * root)
{
    return new EclFileCachedDefinitionCollection(repository, root);
}


//---------------------------------------------------------------------------------------------------------------------

void convertSelectsToPath(StringBuffer & filename, const char * eclPath)
{
    for(;;)
    {
        const char * dot = strchr(eclPath, '.');
        if (!dot)
            break;
        filename.appendLower(dot-eclPath, eclPath);
        addPathSepChar(filename);
        eclPath = dot + 1;
    }
    filename.appendLower(strlen(eclPath), eclPath);
}

//---------------------------------------------------------------------------------------------------------------------

IHqlExpression * createSimplifiedDefinition(ITypeInfo * type)
{
#if 0 //Testing!
    try
    {
        //MORE: For records should use
        //OwnedHqlExpr actualRecord = getUnadornedRecordOrField(actual->queryRecord());
        return createNullExpr(type);
    }
    catch (IException * e)
    {
        e->Release();
    }
#endif

    return nullptr;
}

IHqlExpression * createSimplifiedDefinition(IHqlExpression * expr)
{
    if (expr->isFunction())
    {
        if (expr->getOperator() != no_funcdef)
            return nullptr;
        OwnedHqlExpr newBody = createSimplifiedDefinition(expr->queryChild(0));
        if (newBody)
            return replaceChild(expr, 0, newBody);
        return nullptr;
    }

    ITypeInfo * type = expr->queryType();
    if (!type)
        return nullptr;

    switch (type->getTypeCode())
    {
    case type_scope: // These may be possible - if the scope is not a forward scope or derived from another scope
        return nullptr;
    case type_pattern:
    case type_rule:
    case type_token:
        //Possible, but the default testing code doesn't work
        return nullptr;
    }

    OwnedHqlExpr simple = createSimplifiedDefinition(type);
    if (simple)
        return expr->cloneAllAnnotations(simple);

    return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------

const char * splitFullname(StringBuffer & module, const char * fullname)
{
    const char * dot = strrchr(fullname, '.');
    if (dot)
    {
        module.append(dot-fullname, fullname);
        return dot+1;
    }
    else
        return fullname;
}

void getFileContentText(StringBuffer & result, IFileContents * contents)
{
    unsigned len = contents->length();
    const char * text = contents->getText();
    if ((len >= 3) && (memcmp(text, UTF8_BOM, 3) == 0))
    {
        len -= 3;
        text += 3;
    }
    result.append(len, text);
}

void setDefinitionText(IPropertyTree * target, const char * prop, IFileContents * contents, bool checkDirty)
{
    StringBuffer sillyTempBuffer;
    getFileContentText(sillyTempBuffer, contents);  // We can't rely on IFileContents->getText() being null terminated..
    target->setProp(prop, sillyTempBuffer);

    ISourcePath * sourcePath = contents->querySourcePath();
    target->setProp("@sourcePath", str(sourcePath));
    if (checkDirty && contents->isDirty())
    {
        target->setPropBool("@dirty", true);
    }

    timestamp_type ts = contents->getTimeStamp();
    if (ts)
        target->setPropInt64("@ts", ts);
}

//---------------------------------------------------------------------------------------------------------------------


class ArchiveCreator
{
public:
    ArchiveCreator(IEclCachedDefinitionCollection * _collection) : collection(_collection)
    {
        archive.setown(createAttributeArchive());
    }
    ArchiveCreator(IEclCachedDefinitionCollection * _collection, IPropertyTree * _archive) : collection(_collection), archive(_archive)
    {
    }

    void processDependency(const char * name);
    IPropertyTree * getArchive() { return archive.getClear(); }

protected:
    void createArchiveItem(const char * fullName, IEclSource * original);

protected:
    Linked<IPropertyTree> archive;
    IEclCachedDefinitionCollection * collection;
};


void ArchiveCreator::processDependency(const char * fullName)
{
    if (queryArchiveEntry(archive, fullName))
        return;

    Owned<IEclCachedDefinition> definition = collection->getDefinition(fullName);
    IEclSource * original = definition->queryOriginal();
    if (!original)
        throwError1(HQLERR_CacheMissingOriginal, fullName);

    createArchiveItem(fullName, original);

    StringArray dependencies;
    definition->queryDependencies(dependencies);
    ForEachItemIn(i, dependencies)
        processDependency(dependencies.item(i));
}

void ArchiveCreator::createArchiveItem(const char * fullName, IEclSource * original)
{
    if (original->queryType() == ESTdefinition)
    {
        StringBuffer moduleName;
        const char * attrName = splitFullname(moduleName, fullName);

        IPropertyTree * module = queryEnsureArchiveModule(archive, moduleName, nullptr);
        assertex(!queryArchiveAttribute(module, attrName));
        IPropertyTree * attr = createArchiveAttribute(module, attrName);
        setDefinitionText(attr, "", original->queryFileContents(), false);
    }
    else
    {
        Owned<IProperties> properties = original->getProperties();
        IPropertyTree * module = queryEnsureArchiveModule(archive, fullName, nullptr);
        IFileContents * contents = original->queryFileContents();
        setDefinitionText(module, "Text", contents, false);

        StringBuffer s;
        unsigned flagsToSave = (properties->getPropInt(str(flagsAtom), 0) & PLUGIN_SAVEMASK);
        if (flagsToSave)
            module->setPropInt("@flags", flagsToSave);
        properties->getProp(str(pluginAtom), s.clear());
        if (s.length())
        {
            module->setProp("@fullname", s.str());

            StringBuffer pluginName(s.str());
            getFileNameOnly(pluginName, false);
            module->setProp("@plugin", pluginName.str());
        }
        properties->getProp(str(versionAtom), s.clear());
        if (s.length())
            module->setProp("@version", s.str());
        /*
        if (original->queryType() == ESTplugin)
        {
            //properties->setProp(str(flagsAtom), extraFlags);
            //properties->setProp(str(versionAtom), version.get());
            module->setProp("plugin", contents->queryFile()->queryFilename());
        }
        */
    }
}


IPropertyTree * createArchiveFromCache(IEclCachedDefinitionCollection * collection, const char * root)
{
    ArchiveCreator creator(collection);
    creator.processDependency(root);
    return creator.getArchive();
}

extern HQL_API void updateArchiveFromCache(IEclCachedDefinitionCollection * collection, const char * root, IPropertyTree * archive)
{
    ArchiveCreator creator(collection, archive);
    creator.processDependency(root);
}
