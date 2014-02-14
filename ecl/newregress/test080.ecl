r1 := RECORD
    UNSIGNED id;
    IFBLOCK (SELF.id = 0)
        UNSIGNED v1;
    END;
    IFBLOCK (SELF.id != 0)
        UNSIGNED v2;
    END;
END;
r := RECORD
    UNSIGNED id;
    IFBLOCK (SELF.id != 0)
        r1 v1;
    END;
    IFBLOCK (SELF.id = 0)
        r1 v2;
    END;
END;
DATASET([TRANSFORM(r, SELF := [])]);
