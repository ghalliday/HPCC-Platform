DataRec := RECORD
    STRING      theName;
END;

Mod1 := MODULE, VIRTUAL
    EXPORT STRING SOME_NAME;
    EXPORT DATASET(DataRec) DoSomething(VIRTUAL DATASET inFile, <?> ANY f);
END;

Mod2 := MODULE(Mod1)
    EXPORT STRING SOME_NAME := 'foo';
    EXPORT DATASET(DataRec) DoSomething(VIRTUAL DATASET inFile, <?> ANY f) := FUNCTION
        res := PROJECT
            (
                inFile,
                TRANSFORM
                    (
                        DataRec,
                        SELF.theName := SOME_NAME + '.' + (STRING)LEFT.<f>
                    )
            );
        RETURN res;
    END;
END;

ds0 := DATASET(['bar', 'baz'], {STRING s});
ds := NOFOLD(ds0);

OUTPUT(Mod2.DoSomething(ds, s));

