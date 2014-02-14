r1 := RECORD
    UNSIGNED i;
    UNSIGNED v1;
END;
r := RECORD
    r1;
    UNSIGNED v2;
END;
ROW(TRANSFORM(r, SELF := []));
