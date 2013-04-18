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
        DATA_NETWORKS='"137.138.0.0/255.255.0.0"'
        SEGMENTS[1]="${DEFAULT_HOST}@Computer"
        
        if [ ! -f farm.data.xml ]; then
            pm_farm.py --add=${DEFAULT_HOST} farm.data.xml
        fi
        INCLUDES="${INCLUDES} -I farm.data.xml"
        ;;
esac

pm_set.py -n ${INCLUDES} ${PARTITION}.data.xml <<EOF

#
# HLTSV configuration: use defaults for everything
#
  HLTSVConfig@HLTSVConfiguration

#
# HLTSV application
#
  HLTSV@HLTSVApplication
  HLTSV@HLTSVApplication.Program              = hltsv_main@Binary
  HLTSV@HLTSVApplication.RestartableDuringRun = True
  HLTSV@HLTSVApplication.RunsOn               = ${DEFAULT_HOST}@Computer
  HLTSV@HLTSVApplication.Configuration        = HLTSVConfig@HLTSVConfiguration

#
# Dummy DCM application
# 

#
# repository just for the testDCM application
#
  ProtoRepo@SW_Repository
  ProtoRepo@SW_Repository.Tags = [ x86_64-slc5-gcc47-opt@Tag ,  x86_64-slc5-gcc47-dbg@Tag  ]
  ProtoRepo@SW_Repository.Name = "HLTSVTestProto"
  ProtoRepo@SW_Repository.InstallationPath = "$REPOSITORY"
  ProtoRepo@SW_Repository.Uses = [ Online@SW_Repository ]
  ProtoRepo@SW_Repository.ISInfoDescriptionFiles = [ 'share/data/hltsv/schema/hltsv_is.schema.xml' ] 


# 
# the testDCM and testROS binary
#
  testDCM@Binary
  testDCM@Binary.BinaryName = 'testDCM'
  testDCM@Binary.BelongsTo =  ProtoRepo@SW_Repository

  testROS@Binary
  testROS@Binary.BinaryName = 'testROS'
  testROS@Binary.BelongsTo =  ProtoRepo@SW_Repository

  ProtoRepo@SW_Repository.SW_Objects = [ testDCM@Binary , testROS@Binary ]

#
# Configuration via environment variables for DCM
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
# template application with testDCM binary
#
  DCM@RunControlTemplateApplication
  DCM@RunControlTemplateApplication.Program = testDCM@Binary
  DCM@RunControlTemplateApplication.Instances = 2
  DCM@RunControlTemplateApplication.ProcessEnvironment = [ DCMVariables@VariableSet ]

#
# DCM segments x ${NUM_SEGMENTS}
# 
  $(for i in $(seq 1 ${NUM_SEGMENTS}) ; do 
     echo DCM-Segment-${i}@HLTSegment 
     echo DCM-Segment-${i}@HLTSegment.IsControlledBy = DefRC@RunControlTemplateApplication 
     echo DCM-Segment-${i}@HLTSegment.TemplateApplications = [ DCM@TemplateApplication ]
     echo DCM-Segment-${i}@HLTSegment.TemplateHosts = [ ${SEGMENTS[${i}]} ]
    done)

#
# application with testROS binary
# 

  ROS-1@RunControlApplication
  ROS-1@RunControlApplication.Program = testROS@Binary

#
# Main HLT segment
# 
  HLT@Segment
  HLT@Segment.IsControlledBy = DefRC@RunControlTemplateApplication
  HLT@Segment.Resources += [ HLTSV@HLTSVApplication ]

#
# ROS segment
#
  ROS@Segment
  ROS@Segment.IsControlledBy = DefRC@RunControlTemplateApplication
  ROS@Segment.Applications += [ ROS-1@RunControlApplication ]

#
# Data flow parameters.
# Set network parameters here.
# 
  Dataflow@DFParameters
  Dataflow@DFParameters.DefaultDataNetworks = [ ${DATA_NETWORKS} ]

# The partition itself

  ${PARTITION}@Partition
  ${PARTITION}@Partition.OnlineInfrastructure = setup@OnlineSegment 
  ${PARTITION}@Partition.DefaultTags = [ x86_64-slc5-gcc47-opt@Tag ,  x86_64-slc5-gcc47-dbg@Tag , x86_64-slc6-gcc47-opt@Tag , x86_64-slc6-gcc47-dbg@Tag ]
  ${PARTITION}@Partition.DataFlowParameters = Dataflow@DFParameters
  ${PARTITION}@Partition.Parameters = [ CommonParameters@VariableSet ]
  ${PARTITION}@Partition.DefaultHost = ${DEFAULT_HOST}@Computer
  ${PARTITION}@Partition.RepositoryRoot = "${REPOSITORY}"

# add segments
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
