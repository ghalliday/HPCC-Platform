r1 := RECORD
    UNSIGNED i;
END;
r := RECORD
    UNSIGNED id;
    UNSIGNED cnt;
    DATASET(r1, COUNT(SELF.cnt)) ds;
END;
ROW(TRANSFORM(r, SELF := []));
