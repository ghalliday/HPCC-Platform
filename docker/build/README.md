| Command     | Builds         | Descrption
| ----------- | -------------- | ----------
| buildbase   | hpccdev:base   | create a base image with an initial checkout from guthub
| buildbranch | hpccdev:latest | build debug system based on a branch/repo
| buildrun    | hpccrun:base   | install the debug build and copy the local environment file into the image
| builddali   | hpccrun:dali   | run congifgen, create directories, and container to run dali

| -- should probably have a builddelta for quick changes to the branch --

gosh <container> - run the bash shell within an image
godali - run the dali container
goesp - run esp in a container
