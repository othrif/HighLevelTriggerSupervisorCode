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
ROS_HOSTS="${DEFAULT_HOST}"

LOGROOT="/tmp"

# default is TCP multicast
MULTICAST=""

case "$2" in
    b4)
        NUM_SEGMENTS=4
        DATA_NETWORKS='"10.193.64.0/255.255.254.0", "10.193.128.0/255.255.254.0"'

        LOGROOT="/logs"

        HLTSV_HOST=pc-tbed-r3-01.cern.ch
        MULTICAST="224.100.1.1/10.193.64.0"

        ROS_HOSTS="pc-tbed-r3-02.cern.ch pc-tbed-r3-03.cern.ch"

        x=(pc-tbed-r3-0{4..9}.cern.ch@Computer)
        SEGMENTS[1]=$(echo ${x[*]} | sed 's; ; , ;g')

        x=(pc-tbed-r3-{10..20}.cern.ch@Computer)
        SEGMENTS[2]=$(echo ${x[*]} | sed 's; ; , ;g')

        x=(pc-tbed-r3-{21..30}.cern.ch@Computer)
        SEGMENTS[3]=$(echo ${x[*]} | sed 's; ; , ;g')

        x=(pc-tbed-r3-{31..40}.cern.ch@Computer)
        SEGMENTS[4]=$(echo ${x[*]} | sed 's; ; , ;g')

        INCLUDES="${INCLUDES} -I daq/hw/hosts.data.xml"
        ;;
    b4-slc5)
        NUM_SEGMENTS=2
        DATA_NETWORKS='"10.193.64.0/255.255.254.0", "10.193.128.0/255.255.254.0"'

        LOGROOT="/logs"

        HLTSV_HOST=pc-tbed-r3-01.cern.ch
        MULTICAST="224.100.1.1/10.193.64.0"

        ROS_HOSTS="pc-tbed-r3-02.cern.ch pc-tbed-r3-03.cern.ch"

        x=(pc-tbed-r3-0{4..9}.cern.ch@Computer)
        SEGMENTS[1]=$(echo ${x[*]} | sed 's; ; , ;g')

        x=(pc-tbed-r3-{10..20}.cern.ch@Computer)
        SEGMENTS[2]=$(echo ${x[*]} | sed 's; ; , ;g')

        INCLUDES="${INCLUDES} -I daq/hw/hosts.data.xml"
        ;;
    b4-slc6)
        NUM_SEGMENTS=2
        DATA_NETWORKS='"10.193.64.0/255.255.254.0", "10.193.128.0/255.255.254.0"'

        LOGROOT="/logs"

        HLTSV_HOST=pc-tbed-r3-21.cern.ch
        MULTICAST="224.100.1.1/10.193.64.0"

        ROS_HOSTS="pc-tbed-r3-22.cern.ch pc-tbed-r3-23.cern.ch"

        x=(pc-tbed-r3-{24..32}.cern.ch@Computer)
        SEGMENTS[1]=$(echo ${x[*]} | sed 's; ; , ;g')

        x=(pc-tbed-r3-{33..40}.cern.ch@Computer)
        SEGMENTS[2]=$(echo ${x[*]} | sed 's; ; , ;g')

        INCLUDES="${INCLUDES} -I daq/hw/hosts.data.xml"
        ;;        
    pre)
        NUM_SEGMENTS=1
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
        DATA_NETWORKS='"137.138.0.0/255.255.0.0", "188.184.2.64/255.255.255.192"'
        SEGMENTS[1]="${DEFAULT_HOST}@Computer"
	#MULTICAST="225.0.1.1/137.138.0.0"
        
        if [ ! -f farm.data.xml ]; then
            pm_farm.py --add=${DEFAULT_HOST} farm.data.xml
        fi
        INCLUDES="${INCLUDES} -I farm.data.xml"
        ;;
esac

pm_set.py -n ${INCLUDES} ${PARTITION}.data.xml <<EOF

#
# repository 
#
  ProtoRepo@SW_Repository
  ProtoRepo@SW_Repository.Tags = [ x86_64-slc6-gcc47-opt@Tag ,  x86_64-slc6-gcc47-dbg@Tag , x86_64-slc5-gcc47-opt@Tag ,  x86_64-slc5-gcc47-dbg@Tag  ]
  ProtoRepo@SW_Repository.Name = "HLTSVTestProto"
  ProtoRepo@SW_Repository.InstallationPath = "$REPOSITORY"
  ProtoRepo@SW_Repository.Uses = [ Online@SW_Repository ]
  ProtoRepo@SW_Repository.ISInfoDescriptionFiles = [ 'share/data/hltsv/schema/hltsv_is.schema.xml' ] 


# hack if binary in release did no build
#hltsv_main@Binary
#hltsv_main@Binary.BinaryName = 'hltsv_main'
#hltsv_main@Binary.BelongsTo  = ProtoRepo@SW_Repository
#ProtoRepo@SW_Repository.SW_Objects += [ hltsv_main@Binary ]


#
# HLTSV configuration: use defaults for everything
#
  HLTSVConfig@HLTSVConfiguration

  HLTSV_Rules@ConfigurationRuleBundle

#
# HLTSV application
#
  HLTSV@HLTSVApplication
  HLTSV@HLTSVApplication.Program              = hltsv_main@Binary
  HLTSV@HLTSVApplication.RestartableDuringRun = True
  HLTSV@HLTSVApplication.RunsOn               = ${HLTSV_HOST:-${DEFAULT_HOST}}@Computer
  HLTSV@HLTSVApplication.Configuration        = HLTSVConfig@HLTSVConfiguration

  HLTSV@HLTSVApplication.ConfigurationRules   =  HLTSV_Rules@ConfigurationRuleBundle

# 
# the testDCM and testROS binary, if not available in release
#
#  testDCM@Binary
#  testDCM@Binary.BinaryName = 'testDCM'
#  testDCM@Binary.BelongsTo =  ProtoRepo@SW_Repository

#  testROS@Binary
#  testROS@Binary.BinaryName = 'testROS'
#  testROS@Binary.BelongsTo =  ProtoRepo@SW_Repository

#  ProtoRepo@SW_Repository.SW_Objects = [ testDCM@Binary , testROS@Binary ]

# 
# template application with testDCM binary
#
  DCM@HLTSV_DCMTest
  DCM@HLTSV_DCMTest.Program = testDCM@Binary
  DCM@HLTSV_DCMTest.Instances = 0
  DCM@HLTSV_DCMTest.RestartableDuringRun = True

  DCM@HLTSV_DCMTest.L2ProcessingTime = 40
  DCM@HLTSV_DCMTest.L2Acceptance = 5
  DCM@HLTSV_DCMTest.EventBuildingTime = 40
  DCM@HLTSV_DCMTest.EFProcessingTime = 4000

#
# DCM segments x ${NUM_SEGMENTS}
# 
  $(for i in $(seq 1 ${NUM_SEGMENTS}) ; do 
     echo DCM-Segment-${i}@HLTSegment 
     echo DCM-Segment-${i}@HLTSegment.IsControlledBy = DefRC@RunControlTemplateApplication 
     echo DCM-Segment-${i}@HLTSegment.TemplateApplications = [ DCM@HLTSV_DCMTest ]
     echo DCM-Segment-${i}@HLTSegment.TemplateHosts = [ ${SEGMENTS[${i}]} ]
    done)

#
# ROS segment
#
  ROS@Segment
  ROS@Segment.IsControlledBy = DefRC@RunControlTemplateApplication


#
# application with testROS binary
# 
  
  $(count=1; for ros in ${ROS_HOSTS:-DEFAULT_HOST}; do
    echo ROS-${count}@RunControlApplication
    echo ROS-${count}@RunControlApplication.Program = testROS@Binary
    echo ROS-${count}@RunControlApplication.RunsOn = ${ros}@Computer
    echo ROS@Segment.Applications += [ ROS-${count}@RunControlApplication ]
    count=$(expr ${count} + 1)
  done)

#
# Main HLT segment
# 
  HLT@Segment
  HLT@Segment.IsControlledBy = DefRC@RunControlTemplateApplication
  HLT@Segment.Resources += [ HLTSV@HLTSVApplication ]


#
# Data flow parameters.
# Set network parameters here.
# 
  Dataflow@DFParameters
  Dataflow@DFParameters.DefaultDataNetworks = [ ${DATA_NETWORKS} ]
  Dataflow@DFParameters.MulticastAddress = "${MULTICAST}"

# The partition itself

  ${PARTITION}@Partition
  ${PARTITION}@Partition.OnlineInfrastructure = setup@OnlineSegment 
  ${PARTITION}@Partition.DefaultTags = [ x86_64-slc6-gcc47-opt@Tag , x86_64-slc6-gcc47-dbg@Tag , x86_64-slc5-gcc47-opt@Tag ,  x86_64-slc5-gcc47-dbg@Tag ]
  ${PARTITION}@Partition.DataFlowParameters = Dataflow@DFParameters
  ${PARTITION}@Partition.Parameters = [ CommonParameters@VariableSet ]
  ${PARTITION}@Partition.DefaultHost = ${DEFAULT_HOST}@Computer
  ${PARTITION}@Partition.RepositoryRoot = "${REPOSITORY}"
  ${PARTITION}@Partition.LogRoot = "${LOGROOT}"

# add segments
  ${PARTITION}@Partition.Segments = [ HLT@Segment ]

  $(for i in $(seq 1 ${NUM_SEGMENTS}) ; do 
     echo ${PARTITION}@Partition.Segments += [ DCM-Segment-${i}@HLTSegment ]
    done)

  ${PARTITION}@Partition.Segments += [ ROS@Segment ]

# event counter
  lvl1-counter@IS_EventsAndRates
  lvl1-counter@IS_EventsAndRates.EventCounter = "DF.HLTSV.Events.LVL1Events"
  lvl1-counter@IS_EventsAndRates.Rate = ""

  lvl2-counter@IS_EventsAndRates
  lvl2-counter@IS_EventsAndRates.EventCounter = "DF.HLTSV.Events.ProcessedEvents"
  lvl2-counter@IS_EventsAndRates.Rate = "DF.HLTSV.Events.Rate"

  counters@IS_InformationSources
  counters@IS_InformationSources.LVL1 = lvl1-counter@IS_EventsAndRates
  counters@IS_InformationSources.LVL2 = lvl2-counter@IS_EventsAndRates

  ${PARTITION}@Partition.IS_InformationSource = counters@IS_InformationSources

#  ${PARTITION}@Partition.Uses += [ ProtoRepo@SW_Repository ]

EOF
