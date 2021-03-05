#The following are required
SUBSCRIPTION=us-hpccplatform-dev
USER=ghalliday

#The following are optional
#RG=
#LOCATION=
#SIZE=Standard_D2a_v4
#VM=

### Check parameters
function error
{
    echo "$1" 1>&2
    exit 1
}

function getip
{
    az vm list-ip-addresses --name buildVM --resource-group ghallidayrg --query [0].virtualMachine.network.publicIpAddresses[0].ipAddress -o tsv
}

AZUSER=`az account show --query user.name -o tsv`

[[ -z ${AZUSER} ]] && error "Use az login before running this script"
[[ -z ${SUBSCRIPTION} ]] && error "SUBSCRIPTION not specified"
[[ -z ${USER} ]] && error "USER not specified (ideally the github user id)"

### Apply defaults

RG=${RG:-${USER}rg}
LOCATION=${LOCATION:-eastus}
SIZE=${SIZE:-Standard_D4a_v4}
VM=${VM:-buildVM}

# vm creation
#az login
echo ------ Tracing commands for user ${AZUSER} ----
echo az account set --subscription ${SUBSCRIPTION}
echo time az group create --name ${RG} --location ${LOCATION}
echo time az vm create  --resource-group ${RG} --name ${VM} --image UbuntuLTS --admin-username ${USER} --generate-ssh-keys --size ${SIZE}

# commands to run on the virtual machine to set it up
echo "use the following command to connect to the azure vm:"
echo "ssh ${USER}@$(getip)"
echo
echo sudo apt-get update
echo sudo apt-get install docker git docker.io
echo sudo usermod -aG docker ${USER}
echo newgrp docker
echo docker login -u <docker-user-id>

echo mkdir dev
echo cd dev
echo git clone https://github.com/${USER}/HPCC-Platform.git
echo cd HPCC-Platform
echo git remote add upstream https://github.com/hpcc-systems/HPCC-Platform.git
echo git fetch upstream

#Not really needed
echo #git submodule update --recursive --init

#echo sudo apt-get install cmake bison flex build-essential binutils-dev libldap2-dev libcppunit-dev libicu-dev libxslt1-dev zlib1g-dev libboost-regex-dev libarchive-dev python-dev libv8-dev default-jdk libapr1-dev libaprutil1-dev libiberty-dev libhiredis-dev libtbb-dev libxalan-c-dev libnuma-dev nodejs libevent-dev libatlas-base-dev libblas-dev python3-dev default-libmysqlclient-dev libsqlite3-dev r-base-dev r-cran-rcpp r-cran-rinside r-cran-inline libmemcached-dev libcurl4-openssl-dev pkg-config libtool autotools-dev automake libssl-dev

# Ready to do a new build

echo "----------- start a build -----------"
echo time az vm start --resource-group ${RG} --name ${VM}
echo
echo "echo <DOCKER_PASSWORD> | docker login -u \${DOCKER_USERNAME} --password-stdin"

# After completing a new build

echo
#I am fairly sure deallocate implies stop, and seems to be quicker than executing stop 1st
#echo time az vm stop --resource-group ${RG} --name ${VM}
echo time az vm deallocate --resource-group ${RG} --name ${VM}


# Checking the status of the vm
echo
echo az vm get-instance-view --name ${VM} --resource-group ${RG} --query instanceView.statuses[1] --output table
echo az vm list-ip-addresses --name buildVM --resource-group ghallidayrg

# -- closedown --
echo
echo az vm delete --resource-group ghallidayrg --name ${VM} --yes
echo az group delete --name ${RG} --yes
