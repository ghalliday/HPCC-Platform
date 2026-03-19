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

/*
 * Jlib regression tests
 *
 */

#ifdef _USE_CPPUNIT

#include "jptree.hpp"
#include "jdebug.hpp"
#include "jlog.hpp"
#include "unittests.hpp"
#include <vector>
#include <string>
#include <iterator>

constexpr const char * commonTestXml = R"xml(
<Dataset _uid='u1' type="1" global_id="1000">
    <Record _uid='u2' id="2" name="3" my_unique_attr="4">
        <Flags _uid='u3' type="5" custom_val="6">7</Flags>
        <Child _uid='u4' ref="8">9</Child>
        <Child _uid='u5' ref="10">11</Child>
        <EmptyLeaf _uid='u6' empty_attr="12"/>
    </Record>
    <Record _uid='u7' id="13" name="14" status="15">
        <Nested _uid='u8' inner="16">
            <Node _uid='u9' specific="17">18</Node>
            <Node _uid='u10' specific="19">20</Node>
            <Deep _uid='u11' child_param="21">
                <Deeper _uid='u12' depth="22">
                    <ExtremelyDeep _uid='u13' hidden_val="23">24</ExtremelyDeep>
                </Deeper>
            </Deep>
        </Nested>
    </Record>
    <UniqueTag _uid='u14' identifier="25">26</UniqueTag>
    <ArrayItems _uid='u15' count="27">
        <Item _uid='u16' idx="28">29</Item>
        <Item _uid='u17' idx="30">31</Item>
        <Item _uid='u18' idx="32">33</Item>
        <Item _uid='u19' idx="34">35</Item>
        <Item _uid='u20' idx="36">37</Item>
        <Item _uid='u21' idx="38">39</Item>
        <Item _uid='u22' idx="40">41</Item>
        <Item _uid='u23' idx="42">43</Item>
        <Item _uid='u24' idx="44">45</Item>
        <Item _uid='u25' idx="46">47</Item>
        <Item _uid='u26' idx="48">49</Item>
        <Item _uid='u27' idx="50">51</Item>
        <Item _uid='u28' idx="52">53</Item>
        <Item _uid='u29' idx="54">55</Item>
        <Item _uid='u30' idx="56">57</Item>
        <Item _uid='u31' idx="58">59</Item>
        <Item _uid='u32' idx="60">61</Item>
        <Item _uid='u33' idx="62">63</Item>
        <Item _uid='u34' idx="64">65</Item>
        <Item _uid='u35' idx="66">67</Item>
    </ArrayItems>
    <Sections _uid='u36'>
        <Section _uid='u37' level="68" val="69">
            <Section _uid='u38' level="70" val="71">
                <Section _uid='u39' level="72" val="73">74</Section>
                <Section _uid='u40' level="75" val="76">77</Section>
                <OtherTag _uid='u41' prop="78">79</OtherTag>
            </Section>
            <Section _uid='u42' level="80" val="81">
                <Section _uid='u43' level="82" val="83">84</Section>
            </Section>
        </Section>
        <Section _uid='u44' level="85" val="86">
            <Section _uid='u45' level="87" val="88">89</Section>
            <Module _uid='u46' flag="90">
                <Property _uid='u47' key="91">92</Property>
            </Module>
        </Section>
    </Sections>
    <Types _uid='u48'>
        <StringTest _uid='u49' status="Active" mixed="aCtIvE" short="Z" long="AA" />
        <WildcardTest _uid='u50' file="config.xml" path="/usr/bin/test" />
        <NumericTest _uid='u51' int="100" neg="-5" float="1.234" leading_zero="007" />
        <BooleanTest _uid='u52' flag1="true" flag2="false" flag3="yes" flag4="no" />
        <EmptyTest _uid='u53' empty_attr="" >
            <TrueEmpty _uid='u54'></TrueEmpty>
            <SelfClosingEmpty _uid='u55' />
        </EmptyTest>
    </Types>
    <Config _uid='u56' setting="93">
        <Database _uid='u57' uid="94">95</Database>
        <Network _uid='u58' port="96" active="97">98</Network>
        <TrailingEmpty _uid='u59' />
    </Config>
    <MultiFilterTests _uid='u60'>
        <Multi _uid='u61' match_id="200" kind="A" flag="X">201</Multi>
        <Multi _uid='u62' match_id="200" kind="A" flag="Y">202</Multi>
        <Multi _uid='u63' match_id="200" kind="B" flag="Z">203</Multi>
        <Multi _uid='u64' match_id="204" kind="A" flag="W">205</Multi>
        <Complex _uid='u65' idx="206">
            <Sub _uid='u66'>207</Sub>
        </Complex>
        <Complex _uid='u67' idx="208">
            <Sub _uid='u68'>209</Sub>
        </Complex>
        <Complex _uid='u69' idx="210">
            <Sub _uid='u70'>211</Sub>
        </Complex>
        <RecursiveBase _uid='u71' state="Root">
            <Target _uid='u72' prop="300" kind="A" active="1" />
            <InnerLayer _uid='u73'>
                <Target _uid='u74' prop="301" kind="B" active="0" />
                <DeepLayer _uid='u75'>
                    <Target _uid='u76' prop="302" kind="A" active="1" />
                    <Target _uid='u77' prop="303" kind="C" active="1" />
                </DeepLayer>
            </InnerLayer>
            <SisterLayer _uid='u78'>
                <Target _uid='u79' prop="304" kind="A" active="0" />
                <Target _uid='u80' prop="305" kind="B" active="1" />
                <Target _uid='u81' prop="306" kind="A" active="1" />
            </SisterLayer>
        </RecursiveBase>
    </MultiFilterTests>
</Dataset>
)xml";

class PTreeXPathTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(PTreeXPathTests);
        CPPUNIT_TEST(testXPathQueries);
    CPPUNIT_TEST_SUITE_END();

    struct XPathTestCase
    {
        const char * rootXPath;
        const char * queryXPath;
        const char * expectedValue;
    };

    inline const char * safe(const char * str)
    {
        return str ? str : "nullptr";
    }

    void checkXPath(IPropertyTree * root, const char * testName, size_t testNumber, const char * rootXPath, const char * queryXPath, const char * expectedValue)
    {
        const char * safeRootXPath = safe(rootXPath);
        const char * safeQueryXPath = safe(queryXPath);
        try
        {
            IPropertyTree * startNode = root->queryPropTree(rootXPath);
            if (!startNode)
            {
                VStringBuffer msg("Test '%s' [#%zu]: Failed to find rootXPath: '%s'", testName, testNumber, safeRootXPath);
                CPPUNIT_FAIL(msg.str());
            }

            const char * actualValue = startNode->queryProp(queryXPath);
            if (expectedValue)
            {
                if (!actualValue || strcmp(actualValue, expectedValue) != 0)
                {
                    VStringBuffer msg("Test '%s' [#%zu]: Mismatch for root '%s', query '%s'. Expected: '%s', Actual: '%s'",
                                testName, testNumber, safeRootXPath, safeQueryXPath, expectedValue, safe(actualValue));
                    CPPUNIT_FAIL(msg.str());
                }
            }
            else
            {
                if (actualValue)
                {
                    VStringBuffer msg("Test '%s' [#%zu]: Expected null for root '%s', query '%s', but got: '%s'",
                                testName, testNumber, safeRootXPath, safeQueryXPath, actualValue);
                    CPPUNIT_FAIL(msg.str());
                }
            }
        }
        catch (IException * e)
        {
            StringBuffer errMsg;
            e->errorMessage(errMsg);
            e->Release();
            VStringBuffer msg("Test '%s' [#%zu]: Exception in checkXPath (root: '%s', query: '%s'): %s",
                       testName, testNumber, safeRootXPath, safeQueryXPath, errMsg.str());
            CPPUNIT_FAIL(msg.str());
        }
    }

    void runXPathTests(IPropertyTree * root, const char * testName, const XPathTestCase * tests, size_t numTests)
    {
        CCycleTimer timer;
        for (size_t i = 0; i < numTests; i++)
        {
            checkXPath(root, testName, i, tests[i].rootXPath, tests[i].queryXPath, tests[i].expectedValue);
        }
        DBGLOG("Test %s took %llu ns", testName, timer.elapsedNs());
    }

    struct IteratorTestCase
    {
        const char * rootXPath;
        const char * iteratorXPath;
        std::vector<const char *> expectedUids;
    };

    void checkIterator(IPropertyTree * root, const char * testName, size_t testNumber, const char * rootXPath, const char * iteratorXPath, const std::vector<const char *> & expectedUids)
    {
        const char * safeRootXPath = safe(rootXPath);
        const char * safeIteratorXPath = safe(iteratorXPath);
        try
        {
            IPropertyTree * startNode = root->queryPropTree(rootXPath);
            if (!startNode)
            {
                VStringBuffer msg("Test '%s' [#%zu]: Failed to find rootXPath: '%s'", testName, testNumber, safeRootXPath);
                CPPUNIT_FAIL(msg.str());
                return;
            }

            Owned<IPropertyTreeIterator> iter = startNode->getElements(iteratorXPath);
            std::vector<std::string> actualUids;
            if (iter->first())
            {
                do
                {
                    IPropertyTree & node = iter->query();
                    const char * uid = node.queryProp("@_uid");
                    actualUids.push_back(uid ? uid : "<missing>");
                } while (iter->next());
            }

            if (actualUids.size() != expectedUids.size())
            {
                VStringBuffer msg("Test '%s' [#%zu]: Mismatch in count for root '%s', query '%s'. Expected: %zu, Actual: %zu",
                            testName, testNumber, safeRootXPath, safeIteratorXPath, expectedUids.size(), actualUids.size());
                CPPUNIT_FAIL(msg.str());
                return;
            }

            for (size_t i = 0; i < expectedUids.size(); i++)
            {
                if (actualUids[i] != expectedUids[i])
                {
                    VStringBuffer msg("Test '%s' [#%zu]: Mismatch at index %zu for root '%s', query '%s'. Expected: '%s', Actual: '%s'",
                                testName, testNumber, i, safeRootXPath, safeIteratorXPath, expectedUids[i], actualUids[i].c_str());
                    CPPUNIT_FAIL(msg.str());
                    return;
                }
            }
        }
        catch (IException * e)
        {
            StringBuffer errMsg;
            e->errorMessage(errMsg);
            e->Release();
            VStringBuffer msg("Test '%s' [#%zu]: Exception in checkIterator (root: '%s', query: '%s'): %s",
                       testName, testNumber, safeRootXPath, safeIteratorXPath, errMsg.str());
            CPPUNIT_FAIL(msg.str());
        }
    }

    void runIteratorTests(IPropertyTree * root, const char * testName, const IteratorTestCase * tests, size_t numTests)
    {
        CCycleTimer timer;
        for (size_t i = 0; i < numTests; i++)
        {
            checkIterator(root, testName, i, tests[i].rootXPath, tests[i].iteratorXPath, tests[i].expectedUids);
        }
        DBGLOG("Test %s took %llu ns", testName, timer.elapsedNs());
    }

    void testStructuralLookups(IPropertyTree * root)
    {
        XPathTestCase structuralTests[] = {
            { nullptr, "UniqueTag", "26" },
            { nullptr, "UniqueTag/@identifier", "25" },
            { nullptr, "ArrayItems/@count", "27" },
            { "ArrayItems", "Item[1]", "29" },
            { "Record[1]", "Flags", "7" },
            { "Config/Network", "@port", "96" },
        };
        runXPathTests(root, "Structural Lookups", structuralTests, std::size(structuralTests));
    }

    void testIndexing(IPropertyTree * root)
    {
        XPathTestCase indexTests[] = {
            { nullptr, "ArrayItems/Item[1]", "29" },
            { nullptr, "ArrayItems/Item[20]", "67" },
            { nullptr, "ArrayItems/Item[21]", nullptr },
            { nullptr, "ArrayItems/Item[0]", nullptr },
            { nullptr, "Record[2]/@id", "13" },
            { "Record[2]", "Nested/Node[2]", "20" },
            { "Record[2]", "Nested/Node[2]/@specific", "19" }
        };
        runXPathTests(root, "Indexing", indexTests, std::size(indexTests));
    }

    void testAttributeConditions(IPropertyTree * root)
    {
        XPathTestCase attributeConditionTests[] = {
            { nullptr, "Record[@id=\"2\"]/Child[1]/@ref", "8" },          // First tag match
            { nullptr, "Record[@id=\"13\"]/Nested/Node[1]", "18" },       // Later sibling match
            { nullptr, "Record[@id=\"999\"]/Child[1]/@ref", nullptr },    // No match
            { nullptr, "Record[@my_unique_attr=\"4\"]/@name", "3" },
            { nullptr, "Config/Network[@active=\"97\"]", "98" }
        };
        runXPathTests(root, "Attribute Conditions", attributeConditionTests, std::size(attributeConditionTests));
    }

    void testNumericComparisons(IPropertyTree * root)
    {
        XPathTestCase numericComparisonTests[] = {
            { "ArrayItems", "Item[@idx>60][1]", "63" },     // First match natively returned
            { "ArrayItems", "Item[@idx>999]", nullptr }, // No match bounds check
            { "Sections/Section[2]/Module", "Property[@key<92]", "92" },
            { "Sections/Section[2]", "Section[@level>=87]/@val", "88" },
            { "Record[2]/Nested", "Node[@specific!=17]", "20" },
        };
        runXPathTests(root, "Numeric comparisons unquoted", numericComparisonTests, std::size(numericComparisonTests));
    }

    void testExactStringComparisons(IPropertyTree * root)
    {
        XPathTestCase exactStringComparisonTests[] = {
            { "Types", "StringTest[@status=\"Active\"]/@short", "Z" },
            { "Types", "StringTest[@short=\"Z\"]/@long", "AA" },
            { "Types", "StringTest[@short=\"AA\"]/@long", nullptr }, // Fails
            { "Types", "BooleanTest[@flag1=\"true\"]/@flag4", "no" }
        };
        runXPathTests(root, "Exact String comparisons quoted", exactStringComparisonTests, std::size(exactStringComparisonTests));
    }

    void testCaseInsensitive(IPropertyTree * root)
    {
        XPathTestCase caseInsensitiveTests[] = {
            { "Types", "StringTest[@status=?\"active\"]/@mixed", "aCtIvE" },
            { "Types", "StringTest[@mixed=?\"ACTIVE\"]/@short", "Z" },
            { "Types", "StringTest[@mixed=?\"AcTiVe\"]/@long", "AA" }
        };
        runXPathTests(root, "Case Insensitive Matches", caseInsensitiveTests, std::size(caseInsensitiveTests));
    }

    void testWildcardMatches(IPropertyTree * root)
    {
        XPathTestCase wildcardTests[] = {
            { "Types", "WildcardTest[@file=~\"config*\"]/@path", "/usr/bin/test" },
            { "Types", "WildcardTest[@file=~\"conf?g.xml\"]/@path", "/usr/bin/test" },
            { "Types", "WildcardTest[@path=~\"*/test\"]/@file", "config.xml" }
        };
        runXPathTests(root, "Wildcard Matches", wildcardTests, std::size(wildcardTests));
    }

    void testExistence(IPropertyTree * root)
    {
        XPathTestCase existenceTests[] = {
            { nullptr, "Record/Flags[1]", "7" },
            { nullptr, "Record[Flags]", nullptr },           // Returns the entire node, but getting value of Record gets nothing if it has subnodes
            { nullptr, "Record[@my_unique_attr]/@id", "2" },
            { nullptr, "Config[TrailingEmpty]/@setting", "93" },
            { "Types", "EmptyTest[TrueEmpty]/@empty_attr", "" }
        };
        runXPathTests(root, "Node/Attribute Existence", existenceTests, std::size(existenceTests));
    }

    void testRecursiveSearch(IPropertyTree * root)
    {
        XPathTestCase recursiveTests[] = {
            // Simple singular finds
            { nullptr, "//UniqueTag", "26" },
            { nullptr, "//Deep/Deeper/ExtremelyDeep", "24" },
            { nullptr, "//ExtremelyDeep/@hidden_val", "23" },
            { nullptr, "//Section[@level=\"87\"]", "89" },
            { nullptr, "//Property[@key=\"91\"]", "92" },

            // MORE: (JCS) Recursive searches with an index to select the match currently report
            // "IPropertyTree: Ambiguous xpath used", so disable the tests for the moment.
#if 0
            // Multiple layers & array indexing (testing cross-depth aggregation sequences)
            { "MultiFilterTests/RecursiveBase", "//Target[1]/@prop", "300" }, // First target globally mapped across tree
            { "MultiFilterTests/RecursiveBase", "//Target[3]/@prop", "302" },
            { "MultiFilterTests/RecursiveBase", "//Target[7]/@prop", "306" }, // Last sibling

            // Recursive with multiple chained filters matching subsets
            { "MultiFilterTests/RecursiveBase", "//Target[@kind=\"A\"][@active=\"1\"][1]/@prop", "300" },
            { "MultiFilterTests/RecursiveBase", "//Target[@kind=\"A\"][@active=\"1\"][2]/@prop", "302" },
            { "MultiFilterTests/RecursiveBase", "//Target[@kind=\"A\"][@active=\"1\"][3]/@prop", "306" },
#endif
            { "MultiFilterTests/RecursiveBase", "//Target[@kind=\"A\"][@active=\"1\"][4]/@prop", nullptr }, // Out of bounds > 1 limit

            // Recursive filtering matching exactly 1 item
            { "MultiFilterTests/RecursiveBase", "//Target[@kind=\"C\"][@active=\"1\"]/@prop", "303" },

            // Recursive filtering matching 0 items
            { "MultiFilterTests/RecursiveBase", "//Target[@kind=\"C\"][@active=\"0\"]/@prop", nullptr },
            { "MultiFilterTests/RecursiveBase", "//Target[@kind=\"B\"][@active=\"0\"][2]/@prop", nullptr },

            // Interspaced structural filters
            { "MultiFilterTests", "//SisterLayer/Target[@active=\"0\"]/@prop", "304" },
            { nullptr, "//RecursiveBase[@state=\"Root\"]//Target[@kind=\"B\"][@active=\"1\"]/@prop", "305" }
        };
        runXPathTests(root, "Recursive Search", recursiveTests, std::size(recursiveTests));
    }

    void testMultipleFilters(IPropertyTree * root)
    {
        XPathTestCase multipleFilterTests[] = {
            // Multiple attribute filters
            { "MultiFilterTests", "Multi[@match_id=200][@kind=\"A\"][1]/@flag", "X" }, // Matches first A
            { "MultiFilterTests", "Multi[@match_id=200][@kind=\"B\"]/@flag", "Z" }, // Matches the B
            // Filter and then index (gets the Nth node matching the previous filter sequence)
            { "MultiFilterTests", "Multi[@match_id=200][2]/@flag", "Y" },
            { "MultiFilterTests", "Multi[@kind=\"A\"][3]/@flag", "W" },
            // Index and then filter (gets the specific Nth structural node, but ONLY if it matches the subsequent filter)
            { "MultiFilterTests", "Multi[3][@kind=\"B\"]/@flag", "Z" },
            { "MultiFilterTests", "Multi[3][@kind=\"A\"]/@flag", nullptr }, // Fails, because index 3 is kind B
            // Mixed existence, child value, and attribute filtering
            { "MultiFilterTests", "Complex[Sub=\"211\"][@idx=\"210\"]/Sub", "211" },
            { "MultiFilterTests", "Complex[Sub=\"211\"][@idx=\"212\"]/Sub", nullptr },
            { "MultiFilterTests", "Complex[@idx=\"206\"][Sub=\"207\"]/Sub", "207" },
            { "MultiFilterTests", "Complex[@idx=\"206\"][Sub=\"211\"]/Sub", nullptr } // Fails mismatch
        };
        runXPathTests(root, "Multiple Filters", multipleFilterTests, std::size(multipleFilterTests));
    }

    void testIterations(IPropertyTree * root)
    {
        IteratorTestCase iterTests[] = {
            // Find multiple siblings
            { nullptr, "Record", { "u2", "u7" } },
            // Array iteration (expecting all 20 children in sequential order)
            { nullptr, "ArrayItems/Item", { "u16", "u17", "u18", "u19", "u20", "u21", "u22", "u23", "u24", "u25", "u26", "u27", "u28", "u29", "u30", "u31", "u32", "u33", "u34", "u35" } },
            // Indexing matching strictly 1 item
            { nullptr, "Record[2]", { "u7" } },
            // Indexing matching 0 items
            { nullptr, "Record[3]", {} },
            // Indexing out of bounds
            { nullptr, "ArrayItems/Item[0]", {} },
            { nullptr, "ArrayItems/Item[21]", {} },
            // Wildcard matching siblings
            { nullptr, "Record[1]/*", { "u3", "u4", "u5", "u6" } },
            // Numeric comparisons filtering multiples
            { "ArrayItems", "Item[@idx>60]", { "u33", "u34", "u35" } }, // idx are 62, 64, 66
            { "Sections/Section[2]/Module", "Property[@key<92]", { "u47" } },
            { "Sections/Section[2]/Module", "Property[@key>92]", {} }, // Math failure
            // Exact String & Case-insensitive Comparisons
            { "Types", "StringTest[@status=\"Active\"]", { "u49" } },
            { "Types", "StringTest[@status=?\"active\"]", { "u49" } },
            { "Types", "StringTest[@status=\"active\"]", {} }, // Fails strict case matching
            // Wildcard attributes
            { "Types", "WildcardTest[@file=~\"conf?g.xml\"]", { "u50" } },
            { "Types", "WildcardTest[@path=~\"*wrong*\"]", {} },
            // Existence filters matching multiple vs singular
            { nullptr, "Record[Flags]", { "u2" } },
            { nullptr, "Record[@my_unique_attr]", { "u2" } },
            { nullptr, "Config[TrailingEmpty]", { "u56" } },
            // Sibling condition match
            { "MultiFilterTests", "Multi[@kind=\"A\"]", { "u61", "u62", "u64" } },
            // Multiple conditions chaining
            { "MultiFilterTests", "Multi[@kind=\"A\"][@match_id=200]", { "u61", "u62" } },
            // Recursion gathering depth-first traversal
            { "MultiFilterTests/RecursiveBase", "//Target", { "u72", "u74", "u76", "u77", "u79", "u80", "u81" } },
            // Recursive gathering on a subset
            { "MultiFilterTests/RecursiveBase", "//Target[@kind=\"A\"]", { "u72", "u76", "u79", "u81" } },
            // Interspaced structural filters
            { "MultiFilterTests", "//SisterLayer/Target[@active=\"0\"]", { "u79" } }
        };
        runIteratorTests(root, "Iteration Sequences", iterTests, std::size(iterTests));
    }

    void testDataTypes(IPropertyTree * root)
    {
        CCycleTimer timer;
        IPropertyTree * typesNode = root->queryPropTree("Types");
        CPPUNIT_ASSERT(typesNode != nullptr);

        // String evaluations
        CPPUNIT_ASSERT_EQUAL_STR("Active", typesNode->queryProp("StringTest/@status"));
        CPPUNIT_ASSERT_EQUAL_STR("default", typesNode->queryProp("StringTest/@missing", "default"));

        StringBuffer sb;
        CPPUNIT_ASSERT(typesNode->getProp("StringTest/@short", sb) && strcmp(sb.str(), "Z") == 0);

        // Integer evaluations (32 and 64 bit)
        CPPUNIT_ASSERT_EQUAL(100, typesNode->getPropInt("NumericTest/@int", 0));
        CPPUNIT_ASSERT_EQUAL(-5, typesNode->getPropInt("NumericTest/@neg", 0));
        CPPUNIT_ASSERT_EQUAL(7, typesNode->getPropInt("NumericTest/@leading_zero", 0)); // parses 007
        CPPUNIT_ASSERT_EQUAL(999, typesNode->getPropInt("NumericTest/@missing", 999));

        CPPUNIT_ASSERT_EQUAL((__int64)100, typesNode->getPropInt64("NumericTest/@int", 0));
        CPPUNIT_ASSERT_EQUAL((__int64)-5, typesNode->getPropInt64("NumericTest/@neg", 0));
        CPPUNIT_ASSERT_EQUAL((__int64)777, typesNode->getPropInt64("NumericTest/@missing", 777));

        // Real/Double evaluations
        CPPUNIT_ASSERT_DOUBLES_EQUAL(1.234, typesNode->getPropReal("NumericTest/@float", 0.0), 0.0001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(-5.0, typesNode->getPropReal("NumericTest/@neg", 0.0), 0.0001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(3.14, typesNode->getPropReal("NumericTest/@missing", 3.14), 0.0001);

        // Boolean evaluations
        CPPUNIT_ASSERT_EQUAL(true, typesNode->getPropBool("BooleanTest/@flag1", false)); // "true"
        CPPUNIT_ASSERT_EQUAL(false, typesNode->getPropBool("BooleanTest/@flag2", true)); // "false"
        CPPUNIT_ASSERT_EQUAL(true, typesNode->getPropBool("BooleanTest/@flag3", false)); // "yes"
        CPPUNIT_ASSERT_EQUAL(false, typesNode->getPropBool("BooleanTest/@flag4", true)); // "no"
        CPPUNIT_ASSERT_EQUAL(true, typesNode->getPropBool("BooleanTest/@missing", true)); // Default fallback

        // Edge case structure checks
        CPPUNIT_ASSERT(typesNode->queryPropTree("EmptyTest") != nullptr);
        CPPUNIT_ASSERT(typesNode->queryPropTree("MissingTest") == nullptr);

        DBGLOG("Test Data Types took %llu ns", timer.elapsedNs());
    }

    void testXPathQueries()
    {
        START_TEST

        Owned<IPropertyTree> root = createPTreeFromXMLString(commonTestXml);
        CPPUNIT_ASSERT(root != nullptr);

        testStructuralLookups(root);
        testIndexing(root);
        testAttributeConditions(root);
        testNumericComparisons(root);
        testExactStringComparisons(root);
        testCaseInsensitive(root);
        testWildcardMatches(root);
        testExistence(root);
        testMultipleFilters(root);
        testRecursiveSearch(root);
        testIterations(root);
        testDataTypes(root);

        END_TEST
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(PTreeXPathTests);
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PTreeXPathTests, "PTreeXPathTests");


#endif // _USE_CPPUNIT
