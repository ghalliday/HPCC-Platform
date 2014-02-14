r1 := RECORD
    UNSIGNED i;
    UNSIGNED v1;
END;
r2 := RECORD
    UNSIGNED i;
    UNSIGNED v2;
END;
r := r1 - [i];
ROW(TRANSFORM(r, SELF := []));
