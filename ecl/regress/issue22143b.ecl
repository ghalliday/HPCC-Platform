Mod2 := MODULE, virtual
    EXPORT STRING SOME_NAME := 'foo';
    EXPORT DoSomething(VIRTUAL DATASET inFile, <?> ANY f) := FUNCTION
        res := SOME_NAME;
        RETURN res;
    END;
END;

ds0 := DATASET(['bar', 'baz'], {STRING s});
ds := NOFOLD(ds0);

OUTPUT(Mod2.DoSomething(ds, s));
