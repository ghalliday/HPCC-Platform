r1 := RECORD
    UNSIGNED i;
    UNSIGNED v1;
END;
r := RECORD
    recordof(r1);
    UNSIGNED v2;
END;
ROW(TRANSFORM(r, SELF := []));
