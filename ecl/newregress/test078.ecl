r1 := RECORD
    UNSIGNED i;
END;
r2 := RECORD
    UNSIGNED i;
    r1 nested;
END;
r := RECORD
    UNSIGNED id;
    r2 nested;
END;
DATASET([TRANSFORM(r, SELF := [])]);
