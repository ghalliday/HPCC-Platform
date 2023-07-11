#!/bin/bash

for i in {1,2,3,4,5,6,8,10,12,15,20,25,30,35,40,45,50,60}; do
  ~/hpccrel/opt/HPCCSystems/bin/ecl publish roxie ecl/stresstext.ecl --job-name stresstext_$i -DnumJoins=$i
done
