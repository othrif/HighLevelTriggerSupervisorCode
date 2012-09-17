#!/bin/bash
#
#

if [ $# -lt 1 ]; then
    echo "usage: $0 partition_name [ testbed ]"
    echo "  where testbed is one of local|b4|pre|p1"
    exit 1
fi

INCLUDES="-I daq/sw/repository.data.xml -I daq/schema/df.schema.xml -I daq/segments/setup.data.xml -I daq/sw/common-templates.data.xml -I daq/schema/hltsv.schema.xml"
PARTITION=$1
REPOSITORY=$(dirname $(dirname $(pwd)))/installed
DEFAULT_HOST=$(hostname)

case "$2" in
    b4)
        NUM_SEGMENTS=4
        DATA_NETWORKS=
        SEGMENTS[1]=(pc-preseries-xpu-050{01..31}.cern.ch@Computer)
        SEGMENTS[2]=(pc-preseries-xpu-060{01..31}.cern.ch@Computer)
        SEGMENTS[3]=(pc-preseries-xpu-070{01..31}.cern.ch@Computer)
        SEGMENTS[4]=(pc-preseries-xpu-080{01..31}.cern.ch@Computer)
        INCLUDES="${INCLUDES} -I daq/hw/hosts-bld4.data.xml"
        ;;
    pre)
        NUM_SEGMENTS=3
        DATA_NETWORKS=
        SEGMENTS[1]=(pc-preseries-xpu-910{01..31}.cern.ch@Computer)
        SEGMENTS[2]=(pc-preseries-xpu-940{01..31}.cern.ch@Computer)
        SEGMENTS[3]=(pc-preseries-xpu-950{01..31}.cern.ch@Computer)
        INCLUDES="${INCLUDES} -I daq/hw/hosts-preseries.data.xml"
        ;;
    p1)
        NUM_SEGMENTS=30
        DATA_NETWORKS=
        INCLUDES="${INCLUDES} -I daq/hw/hosts.data.xml"
        ;;
    *)
        NUM_SEGMENTS=1
        DATA_NETWORKS=
        SEGMENTS[1]="${DEFAULT_HOST}@Computer"
        
        if [ ! -f farm.data.xml ]; then
            pm_farm.py --add=${DEFAULT_HOST} farm.data.xml
        fi
        INCLUDES="${INCLUDES} -I farm.data.xml"
        ;;
esac

./pm_set.py -n ${INCLUDES} ${PARTITION}.data.xml <<EOF

#
# IS counter configuration: publish everything
#
  HLTSV_Resource@DC_ISResourceUpdate
  HLTSV_Resource@DC_ISResourceUpdate.activeOnNodes = [ 'HLTSV' ]
  HLTSV_Resource@DC_ISResourceUpdate.name = 'HLTSV'
  HLTSV_Resource@DC_ISResourceUpdate.delay = 10
  HLTSV_Resource@DC_ISResourceUpdate.isServer = 'DF'

  HLTSV_Histograms@DC_HistogramTypeUpdate

#
# Global IS counter config: everything goes to DF IS server
#
  HLTSVAppConfig@DCApplicationConfig
  HLTSVAppConfig@DCApplicationConfig.ISDefaultServer = 'DF'
  HLTSVAppConfig@DCApplicationConfig.refDC_ISResourceUpdate = [ HLTSV_Resource@DC_ISResourceUpdate ]
  HLTSVAppConfig@DCApplicationConfig.refDC_HistogramTypeUpdate = [ HLTSV_Histograms@DC_HistogramTypeUpdate ]

#
# HLTSV configuration: use defaults for everything
#
  HLTSVConfig@HLTSVConfiguration

#
# HLTSV application
#
  HLTSV@HLTSVApplication
  HLTSV@HLTSVApplication.Configuration       = HLTSVConfig@HLTSVConfiguration
  HLTSV@HLTSVApplication.Program             = hltsv_main@Binary
  HLTSV@HLTSVApplication.ReceiveMulticast    = True
  HLTSV@HLTSVApplication.DFApplicationConfig = HLTSVAppConfig@DCApplicationConfig
  HLTSV@HLTSVApplication.RestartableDuringRun = True

#
# Dummy DCM application
# 

#
# repository just for the testDCM application
#
  ProtoRepo@SW_Repository
  ProtoRepo@SW_Repository.Tags = [ i686-slc5-gcc43-opt@Tag ,  i686-slc5-gcc43-dbg@Tag  ]
  ProtoRepo@SW_Repository.Name = "Proto"
  ProtoRepo@SW_Repository.InstallationPath = "$REPOSITORY"
  ProtoRepo@SW_Repository.Uses = [ Online@SW_Repository ]
  ProtoRepo@SW_Repository.ISInfoDescriptionFiles = [ 'share/data/hltsv/schema/hltsv_is.schema.xml' ] 

# 
# the testDCM binary
#
  testDCM@Binary
  testDCM@Binary.BinaryName = 'testDCM'
  testDCM@Binary.BelongsTo =  ProtoRepo@SW_Repository

  ProtoRepo@SW_Repository.SW_Objects = [ testDCM@Binary ]

# dummy L2PU configuration
  dcmConfig@L2PUConfiguration
 

#
# Configuration via environment variables
#

  DCM_L2_PROCESSING@Variable
  DCM_L2_PROCESSING@Variable.Name  = 'DCM_L2_PROCESSING'
  DCM_L2_PROCESSING@Variable.Value = 40

  DCM_L2_ACCEPT@Variable
  DCM_L2_ACCEPT@Variable.Name  = 'DCM_L2_ACCEPT'
  DCM_L2_ACCEPT@Variable.Value = 5

  DCM_EVENT_BUILDING@Variable
  DCM_EVENT_BUILDING@Variable.Name  = 'DCM_EVENT_BUILDING'
  DCM_EVENT_BUILDING@Variable.Value = 40

  DCM_EVENT_FILTER@Variable
  DCM_EVENT_FILTER@Variable.Name  = 'DCM_EVENT_FILTER'
  DCM_EVENT_FILTER@Variable.Value = 2

  DCMVariables@VariableSet  
  DCMVariables@VariableSet.Contains = [ DCM_L2_PROCESSING@Variable , DCM_L2_ACCEPT@Variable , DCM_EVENT_BUILDING@Variable , DCM_EVENT_FILTER@Variable ]  

# 
# L2PU application with testDCM binary
#
  DCM@L2PUTemplateApplication
  DCM@L2PUTemplateApplication.Program = testDCM@Binary
  DCM@L2PUTemplateApplication.DFApplicationConfig = HLTSVAppConfig@DCApplicationConfig
  DCM@L2PUTemplateApplication.L2PUConfiguration = dcmConfig@L2PUConfiguration
  DCM@L2PUTemplateApplication.Instances = 128
  DCM@L2PUTemplateApplication.ProcessEnvironment = [ DCMVariables@VariableSet ]

#
# DCM segments x ${NUM_SEGMENTS}
# 
  $(for i in $(seq 1 ${NUM_SEGMENTS}) ; do 
     echo DCM-Segment-${i}@HLTSegment 
     echo DCM-Segment-${i}@HLTSegment.IsControlledBy = DefRC@RunControlTemplateApplication 
     echo DCM-Segment-${i}@HLTSegment.TemplateApplications = [ DCM@L2PUTemplateApplication ]
     echo DCM-Segment-${i}@HLTSegment.TemplateHosts = [ ${SEGMENTS[${i}]} ]
    done)

#
# Main HLT segment
# 
  HLT@Segment
  HLT@Segment.IsControlledBy = DefRC@RunControlTemplateApplication
  HLT@Segment.Applications += [ HLTSV@HLTSVApplication ]

#
# ROS segment
#
  ROS@Segment
  ROS@Segment.IsControlledBy = DefRC@RunControlTemplateApplication

#
# Data flow parameters.
# Set network parameters here.
# 
  Dataflow@DFParameters
  Dataflow@DFParameters.Protocol = 'tcp'
  Dataflow@DFParameters.DefaultDataNetworks = [ ${DATA_NETWORKS} ]

  ${PARTITION}@Partition
  ${PARTITION}@Partition.OnlineInfrastructure = setup@OnlineSegment 
  ${PARTITION}@Partition.DefaultTags = [ i686-slc5-gcc43-opt@Tag ,  i686-slc5-gcc43-dbg@Tag ]
  ${PARTITION}@Partition.DataFlowParameters = Dataflow@DFParameters
  ${PARTITION}@Partition.Parameters = [ CommonParameters@VariableSet ]
  ${PARTITION}@Partition.DefaultHost = ${DEFAULT_HOST}@Computer
  ${PARTITION}@Partition.RepositoryRoot = "${REPOSITORY}"

  ${PARTITION}@Partition.Segments = [ HLT@Segment ]

  $(for i in $(seq 1 ${NUM_SEGMENTS}) ; do 
     echo ${PARTITION}@Partition.Segments += [ DCM-Segment-${i}@HLTSegment ]
    done)

  ${PARTITION}@Partition.Segments += [ ROS@Segment ]

# event counter
  lvl1-counter@IS_EventsAndRates
  lvl1-counter@IS_EventsAndRates.EventCounter = "DF.HLTSV.HLTSV.LVL1Events"
  lvl1-counter@IS_EventsAndRates.Rate = ""

  lvl2-counter@IS_EventsAndRates
  lvl2-counter@IS_EventsAndRates.EventCounter = "DF.HLTSV.HLTSV.ProcessedEvents"
  lvl2-counter@IS_EventsAndRates.Rate = "DF.HLTSV.HLTSV.Rate"

  counters@IS_InformationSources
  counters@IS_InformationSources.LVL1 = lvl1-counter@IS_EventsAndRates
  counters@IS_InformationSources.LVL2 = lvl2-counter@IS_EventsAndRates

  ${PARTITION}@Partition.IS_InformationSource = counters@IS_InformationSources
EOF