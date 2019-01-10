#onwarning (1028, ignore);

r := { unsigned i };

ds := DATASET(100, transform(r, self.i := COUNTER), distributed);

g1 := group(ds, 0, ALL);    // Should move all the records onto a single node.
agg1 := TABLE(g1, { val := SUM(GROUP, i); });
output(agg1);

agg2 := TABLE(g1, { val := SUM(GROUP, i); }, 0, few);
output(agg2);
