DataRec := RECORD
    STRING      theName;
END;

Mod2 := MODULE, virtual
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

