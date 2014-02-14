r1 := RECORD
    UNSIGNED i;
    UNSIGNED v1;
END;
r2 := RECORD
    UNSIGNED i;
    UNSIGNED v2;
END;
r := r1 OR r2;
ROW(TRANSFORM(r, SELF := []));
