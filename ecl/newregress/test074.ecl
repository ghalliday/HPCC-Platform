r1 := RECORD
    UNSIGNED i;
END;
r := RECORD
    UNSIGNED id;
    DATASET(r1) ds;
END;
ROW(TRANSFORM(r, SELF := []));
