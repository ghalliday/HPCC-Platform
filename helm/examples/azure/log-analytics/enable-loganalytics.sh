#!/bin/bash
WORK_DIR=$(dirname $0)
source ${WORK_DIR}/env-loganalytics

if [[ -z "$LOGANALYTICS_WORKSPACE_NAME" ]]
then
   echo "Missing LOGANALYTICS_WORKSPACE_NAME environment variable - this should be set to the desired name of the new Azure LogAnalytics workspace"
   exit 1
fi

if [[ -z "$LOGANALYTICS_RESOURCE_GROUP" ]]
then
   echo "Missing LOGANALYTICS_RESOURCE_GROUP environment variable - this should be set to the Azure resource group associated with the target AKS cluster"
   exit 1
fi

if [[ -z "$AKS_CLUSTER_NAME" ]]
then
   echo "Missing AKS_CLUSTER_NAME environment variable - this should be set to the AKS cluster to be monitored via newly created LogAnalytics workspace"
   exit 1
fi

if [[ -z "$AKS_RESOURCE_GROUP" ]]
then
   echo "Missing AKS_RESOURCE_GROUP environment variable - this should be set to the AKS cluster Resource group"
   exit 1
fi

if [[ -n "$AZURE_SUBSCRIPTION" ]]
then
   echo "Setting subscription to '$AZURE_SUBSCRIPTION'..."
   az account set --subscription $AZURE_SUBSCRIPTION
fi

tid=$(az account show | awk '/tenantId/ {print $2;}' | tr -d '"' | tr -d ',')
if [[ -z "$tid" ]]
then
   echo "Could not determine Azure Subscription Tenant ID!"
else
   echo "Tenant ID: ${tid}"
fi

echo "Confirming workspace resource group exists..."
wsrgexist=$(az group exists -n $LOGANALYTICS_RESOURCE_GROUP)
if [[ $? -ne 0 ]] || [[ $wsrgexist == false* ]]
then
   echo "creating $LOGANALYTICS_RESOURCE_GROUP resource group..."
   if [[ -z "$AKS_RESOURCE_LOCATION" ]]
   then
     echo "Missing AKS_RESOURCE_LOCATION environment variable - this should be set to the desired Azure resource location"
     exit 1
   fi

   azrgcreate=$(az group create -n $LOGANALYTICS_RESOURCE_GROUP -l $AKS_RESOURCE_LOCATION --tags $TAGS | awk '/id/ {print $2;}' | tr -d '"' | tr -d ',')
   if [[ $? -ne 0 ]]
   then
     echo "Could not create desired Resource Group: $azrgcreate"
     exit 1
   fi

   echo "Resource group id: $azrgcreate"
else
   echo "Resource group $LOGANALYTICS_RESOURCE_GROUP exists!"
fi

echo "Creating workspace..."
wsid=$(az monitor log-analytics workspace create -g $LOGANALYTICS_RESOURCE_GROUP -n $LOGANALYTICS_WORKSPACE_NAME --query-access Enabled --tags $TAGS | awk '/id/ {print $2;}' | tr -d '"' | tr -d ',')
if [[ $? -ne 0 ]] || [[ -z "$wsid" ]]
then
  echo "Could not create target log-analytics workspace '${LOGANALYTICS_RESOURCE_GROUP}'!"
  exit 1
else
  echo "Success, workspace id: ${wsid}"
fi

echo "Fetching workspace customerId..."
wscid=$(az monitor log-analytics workspace show -g $LOGANALYTICS_RESOURCE_GROUP -n $LOGANALYTICS_WORKSPACE_NAME | awk '/customerId/ {print $2;}' | tr -d '"' | tr -d ',')
if [[ $? -ne 0 ]] || [[ -z "$wscid" ]]
then
  echo "Could not fetch target log-analytics workspace customerId for $LOGANALYTICS_WORKSPACE_NAME in group ${LOGANALYTICS_RESOURCE_GROUP}!"
else
  echo "Success, workspace customerId: ${wscid}"
fi

echo "Enabling workspace on target AKS cluster '$AKS_CLUSTER_NAME'..."

if $ENABLE_CONTAINER_LOG_V2 ; then DATA_COLLECTION_SETTINGS="--workspace-resource-id $wsid --data-collection-settings dataCollectionSettings.json"; fi

echo "aks enable-addons -g $AKS_RESOURCE_GROUP -n $AKS_CLUSTER_NAME -a monitoring $DATA_COLLECTION_SETTINGS"
az aks enable-addons -g $AKS_RESOURCE_GROUP -n $AKS_CLUSTER_NAME -a monitoring --workspace-resource-id $wsid
if [[ $? -ne 0 ]]
then
  echo "Could not enable monitoring on ${AKS_RESOURCE_GROUP}/${AKS_CLUSTER_NAME} targeting workspace '${wsid}'!"
  exit 1
else
  echo "Success, workspace id: '$wsid' enabled on AKS $AKS_CLUSTER_NAME"
fi

if $ENABLE_CONTAINER_LOG_V2 ; then
 echo "Setting ContainerLogV2 schema via container-azm-ms-agentconfig" 
 kubectl apply -f ${WORK_DIR}/container-azm-ms-agentconfig.yaml;
fi
