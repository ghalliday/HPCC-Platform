FROM ubuntu:bionic

#It is recommended this is combined with the apt-get commands, but the idea is that this image
#remains stable, and is only forced to be rebuilt when a significant update has occurred

RUN apt-get update
RUN apt-get install -y curl
RUN apt-get install -y wget
RUN apt-get install -y g++
RUN apt-get install -y make
RUN apt-get install -y openssl
RUN apt-get install -y libssl-dev

WORKDIR /tmp/cmake
RUN wget https://cmake.org/files/v3.16/cmake-3.16.3.tar.gz
RUN tar xzf cmake-3.16.3.tar.gz
WORKDIR /tmp/cmake/cmake-3.16.3
RUN ./bootstrap --parallel=16
RUN make -j16
RUN make install

WORKDIR /
RUN rm -r /tmp/cmake

RUN apt-get install -y bison
RUN apt-get install -y flex
RUN apt-get install -y build-essential
RUN apt-get install -y binutils-dev
RUN apt-get install -y libldap2-dev
RUN apt-get install -y libcppunit-dev
RUN apt-get install -y libicu-dev
RUN apt-get install -y libxslt1-dev
RUN apt-get install -y zlib1g-dev
RUN apt-get install -y libboost-regex-dev
RUN apt-get install -y libarchive-dev
RUN apt-get install -y python-dev
RUN apt-get install -y libv8-dev
RUN apt-get install -y default-jdk
RUN apt-get install -y libapr1-dev
RUN apt-get install -y libaprutil1-dev
RUN apt-get install -y libiberty-dev
RUN apt-get install -y libhiredis-dev
RUN apt-get install -y libtbb-dev
RUN apt-get install -y libxalan-c-dev
RUN apt-get install -y libnuma-dev
RUN apt-get install -y nodejs
RUN apt-get install -y libevent-dev
RUN apt-get install -y libatlas-base-dev
RUN apt-get install -y libblas-dev
RUN apt-get install -y python3-dev
RUN apt-get install -y default-libmysqlclient-dev
RUN apt-get install -y libsqlite3-dev
RUN apt-get install -y libmemcached-dev
RUN apt-get install -y libcurl4-openssl-dev
RUN apt-get install -y pkg-config
RUN apt-get install -y libtool
RUN apt-get install -y autotools-dev
RUN apt-get install -y automake
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y tzdata
RUN apt-get install -y r-base-dev
RUN apt-get install -y r-cran-rcpp
RUN apt-get install -y r-cran-rinside
RUN apt-get install -y r-cran-inline

WORKDIR /testing/node
RUN curl -sL https://deb.nodesource.com/setup_13.x | bash -
RUN apt-get install -y nodejs
