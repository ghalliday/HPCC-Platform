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

/*
 * digisign regression tests
 *
 */

#ifdef _USE_CPPUNIT

#include "unittests.hpp"
#include "digisign.hpp"

/* ============================================================= */

class DigiSignUnitTest : public CppUnit::TestFixture
{
public:
    CPPUNIT_TEST_SUITE(DigiSignUnitTest);
        CPPUNIT_TEST(testSimple);
    CPPUNIT_TEST_SUITE_END();

protected:

    void asyncDigiSignUnitText()
    {
        class casyncfor: public CAsyncFor
        {
        public:
            casyncfor()
            {
            }
            void Do(unsigned idx)
            {
                VStringBuffer text("I am here %d", idx);
                StringBuffer sig;
                bool ok = digitalSignatureManagerInstance()->digiSign(text, sig);
                if (!ok)
                    printf("Asynchronous digiSign() test %d failed!\n", idx);
                ASSERT(ok);

                ok = digitalSignatureManagerInstance()->digiVerify(text, sig);
                if (!ok)
                    printf("Asynchronous digiVerify() test %d failed!\n", idx);
                ASSERT(ok);
            }
        } afor;

        printf("Executing 1000 asynchronous digisign/digiverify operations\n");
        afor.For(1000,20,true,true);
    }


    void testSimple()
    {
        Owned<IException> exception;
        CppUnit::Exception *cppunitException;
        const char * text1 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const char * text2 = "~`!@#$%^&*()_-+=0123456789{[}]:;\"'<,>.?/'";
        const char * text3 = "W20180301-154415;ECLUsername";
        StringBuffer sig1;
        StringBuffer sig2;
        StringBuffer sig3;

        try
        {
            printf("\nExecuting digiSign() unit tests\n");

            printf("digiSign() test 1\n");
            StringBuffer txt(text1);
            bool ok = digitalSignatureManagerInstance()->digiSign(text1, sig1.clear());
            ASSERT(ok);
            ASSERT(0 == strcmp(text1, txt.str()));//source string should be unchanged
            ASSERT(!sig1.isEmpty());//signature should be populated

            StringBuffer sig(sig1);
            ok = digitalSignatureManagerInstance()->digiVerify(text1, sig1);
            ASSERT(ok);
            ASSERT(0 == strcmp(text1, txt.str()));//source string should be unchanged
            ASSERT(0 == strcmp(sig.str(), sig1.str()));//signature should be unchanged

            printf("digiSign() test 2\n");
            ok = digitalSignatureManagerInstance()->digiVerify(text1, sig1);
            ASSERT(ok);
            ok = digitalSignatureManagerInstance()->digiVerify(text1, sig1);
            ASSERT(ok);

            printf("digiSign() test 3\n");
            ok = digitalSignatureManagerInstance()->digiSign(text2, sig2.clear());
            ASSERT(ok);
            ok = digitalSignatureManagerInstance()->digiVerify(text2, sig2);
            ASSERT(ok);
            ok = digitalSignatureManagerInstance()->digiSign(text2, sig2.clear());
            ASSERT(ok);
            ok = digitalSignatureManagerInstance()->digiVerify(text2, sig2);
            ASSERT(ok);

            printf("digiSign() test 4\n");
            ok = digitalSignatureManagerInstance()->digiVerify(text1, sig1);
            ASSERT(ok);

            printf("digiSign() test 5\n");
            ok = digitalSignatureManagerInstance()->digiVerify(text2, sig2);
            ASSERT(ok);

            printf("digiSign() test 6\n");
            ok = digitalSignatureManagerInstance()->digiVerify(text1, sig2);
            ASSERT(!ok);//should fail

            printf("digiSign() test 7\n");
            ok = digitalSignatureManagerInstance()->digiVerify(text2, sig1);
            ASSERT(!ok);//should fail

            printf("digiSign() test 8\n");
            ok = digitalSignatureManagerInstance()->digiSign(text3, sig3.clear());
            ASSERT(ok);

            printf("digiSign() test 9\n");
            ok = digitalSignatureManagerInstance()->digiVerify(text3, sig1);
            ASSERT(!ok);//should fail
            ok = digitalSignatureManagerInstance()->digiVerify(text3, sig2);
            ASSERT(!ok);//should fail
            ok = digitalSignatureManagerInstance()->digiVerify(text3, sig3);
            ASSERT(ok);

            //Perform
            printf("digiSign() loop test\n");
            unsigned now = msTick();
            for (int x=0; x<1000; x++)
            {
                digitalSignatureManagerInstance()->digiSign(text3, sig3.clear());
            }
            unsigned taken = msTick() - now;
            printf("digiSign() 1000 iterations took %d MS\n", taken);

            printf("digiVerify() loop test\n");
            now = msTick();
            for (int x=0; x<1000; x++)
            {
                digitalSignatureManagerInstance()->digiVerify(text3, sig3);
            }
            taken = msTick() - now;
            printf("digiverify 1000 iterations took %d MS\n", taken);

            printf("digiVerify() asynchronous test\n");
            asyncDigiSignUnitText();
        }

        catch (IException *e)
        {
            StringBuffer err;
            e->errorMessage(err);
            printf("Digisign IException thrown:%s\n", err.str());
            exception.setown(e);
        }
        catch (CppUnit::Exception &e)
        {
            printf("Digisign CppUnit::Exception thrown\n");
            cppunitException = e.clone();
        }
        printf("Completed executing digiSign() unit tests\n");
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( DigiSignUnitTest );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( DigiSignUnitTest, "DigiSignUnitTest" );

#endif // _USE_CPPUNIT
