#!/bin/bash
#
#

# if [ $# -lt 1 ]; then
#     echo "usage: $0 partition_name [ testbed ]"
#     echo "  where testbed is one of local|b4|pre|p1"
#     exit 1
# fi

# default DCM is local dummy
DCM_APPLICATION="dcm@DcmApplication"
#DCM_APPLICATION="DCM@HLTSV_DCMTest"

# 
# Default parameters
# 
INCLUDES="-I daq/sw/repository.data.xml -I daq/schema/df.schema.xml -I daq/segments/setup.data.xml -I daq/sw/common-templates.data.xml -I daq/schema/hltsv.schema.xml -I daq/schema/dcm.schema.xml"
PARTITION=$1
if [ -z "${PARTITION}"]; then
    PARTITION=HLT
fi
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
    p1|"")
        INCLUDES="${INCLUDES} -I daq/hw/hosts.data.xml -I daq/sw/det-tdaq-repository.data.xml"

        # NUM_SEGMENTS=50
        DATA_NETWORKS='"10.147.0.0/255.255.0.0" , "10.150.0.0/255.255.0.0"'

        LOGROOT="/logs"

        # HLTSV_HOST=pc-tdq-dc-01.cern.ch
	HLTSV_HOST=pc-tdq-sfo-05.cern.ch
        MULTICAST="224.100.1.1/10.147.0.0"

	count=1
	for rack in 03 04 $(seq -w 6 13) $(seq -w 23 26) 38 42 $(seq -w 58 69)
	do
	  x=(pc-tdq-xpu-${rack}00{1..9}.cern.ch@Computer)
	  y=(pc-tdq-xpu-${rack}0{10..30}.cern.ch@Computer)
	  SEGMENTS[${count}]=$(echo ${x[*]} ${y[*]} | sed 's; ; , ;g')
	  SEGMENT_RACK[${count}]=${rack}
	  SEGMENT_CONTROL[${count}]=pc-tdq-xpu-${rack}031.cern.ch@Computer
	  count=$(expr ${count} + 1)
	done

	for rack in $(seq -w 0 13)
	do
	  hosts=""
	  first=$(expr ${rack} '*' 32 + 1)
	  last=$(expr ${first} + 30)
	  for index in $(seq ${first} ${last})
	  do
	    h=$(printf "%04d" ${index})
	    hosts="${hosts} pc-tdq-ef-${h}.cern.ch@Computer"
	  done
	  SEGMENTS[${count}]=$(echo $hosts | sed 's; ; , ;g')
	  SEGMENT_RACK[${count}]=$(expr ${rack} + 70)
	  h=$(printf "%04d" $(expr ${last} + 1))
	  SEGMENT_CONTROL[${count}]=pc-tdq-ef-${h}.cern.ch@Computer
	  count=$(expr ${count} + 1)
	done

	NUM_SEGMENTS=$(expr ${count} - 1)
	echo "NUM_SEGMENTS=${NUM_SEGMENTS}"
        ;;
    local)
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

# SEGMENTS[1]=$(echo ${SEGMENTS[1]} | sed 's;pc-tdq-xpu-040010.cern.ch@Computer , ;;g')



pm_set.py -n ${INCLUDES} ${PARTITION}.data.xml <<EOF

# monsvc for HLTSV

  HLTSV_IS@ISPublishingParameters
  HLTSV_IS@ISPublishingParameters.ISServer = "${TDAQ_IS_SERVER=DF}"
  HLTSV_IS@ISPublishingParameters.PublishInterval = 2

  HLTSV_ISRule@ConfigurationRule
  HLTSV_ISRule@ConfigurationRule.Parameters = HLTSV_IS@ISPublishingParameters

  HLTSV_OH@OHPublishingParameters
  HLTSV_OH@OHPublishingParameters.OHServer = "${TDAQ_OH_SERVER=Histogramming}"
  HLTSV_OH@OHPublishingParameters.ROOTProvider = "HLTSV"
  HLTSV_OH@OHPublishingParameters.PublishInterval = 20

  HLTSV_OHRule@ConfigurationRule
  HLTSV_OHRule@ConfigurationRule.Parameters = HLTSV_OH@OHPublishingParameters

  HLTSV_Rules@ConfigurationRuleBundle
  HLTSV_Rules@ConfigurationRuleBundle.Rules = [ HLTSV_ISRule@ConfigurationRule , HLTSV_OHRule@ConfigurationRule ]

# 
# template application with testDCM binary
#
  DCM@HLTSV_DCMTest
  DCM@HLTSV_DCMTest.Program = testDCM@Binary
  DCM@HLTSV_DCMTest.Instances = 1
  DCM@HLTSV_DCMTest.RestartableDuringRun = True

  DCM@HLTSV_DCMTest.L2ProcessingTime = 50
  DCM@HLTSV_DCMTest.L2Acceptance = 0
  DCM@HLTSV_DCMTest.EventBuildingTime = 40
  DCM@HLTSV_DCMTest.EFProcessingTime = 4000
  DCM@HLTSV_DCMTest.NumberOfCores = 16

#
# Real DCM Application
# 

  dcm_IS@ISPublishingParameters
  dcm_IS@ISPublishingParameters.PublishInterval = 7
  dcm_IS@ISPublishingParameters.ISServer = "\${TDAQ_IS_SERVER=DF}"

  dcmISRule@ConfigurationRule
  dcmISRule@ConfigurationRule.Parameters = dcm_IS@ISPublishingParameters

  dcm_OH@OHPublishingParameters
  dcm_OH@OHPublishingParameters.PublishInterval = 7
  dcm_OH@OHPublishingParameters.OHServer = "\${TDAQ_OH_SERVER=Histogramming}"
  dcm_OH@OHPublishingParameters.ROOTProvider = "\${TDAQ_APPLICATION_NAME}"

  dcmOHRule@ConfigurationRule
  dcmOHRule@ConfigurationRule.Parameters = dcm_OH@OHPublishingParameters

  dcmRules@ConfigurationRuleBundle
  dcmRules@ConfigurationRuleBundle.Rules = [ dcmISRule@ConfigurationRule , dcmOHRule@ConfigurationRule ] 

  dcmL1Source@DcmHltsvL1Source
  dcmDatacollector@DcmRosDataCollector
  dcmOutput@DcmFileOutput  
  dcmProcessor@DcmDummyProcessor

  dcm@DcmApplication
  dcm@DcmApplication.Program            = dcm_main@Binary
  dcm@DcmApplication.ConfigurationRules = dcmRules@ConfigurationRuleBundle
  dcm@DcmApplication.Instances          = 1
  dcm@DcmApplication.RestartableDuringRun = True

  dcm@DcmApplication.sbaDirectory = '/local_L/efHeap'

  dcm@DcmApplication.l1Source           = dcmL1Source@DcmHltsvL1Source
  dcm@DcmApplication.dataCollector      = dcmDatacollector@DcmRosDataCollector
  dcm@DcmApplication.processor          = dcmProcessor@DcmDummyProcessor
  dcm@DcmApplication.output             = dcmOutput@DcmFileOutput

#
# Top level gatherer
# 
  HLT-Storage@HistogramStorage
  HLT-Storage@HistogramStorage.ISServerName = 'Histogramming'

  HLT-TopGatherer-Provider@HistogramProviderConfig
  HLT-TopGatherer-Provider@HistogramProviderConfig.Storage = [ HLT-Storage@HistogramStorage ]
  HLT-TopGatherer-Provider@HistogramProviderConfig.PublishProviderName = 'HLT-Top'
  HLT-TopGatherer-Provider@HistogramProviderConfig.UpdateFrequency = 30

  HLT-TopGatherer-Receiver@HistogramReceiverConfig
  HLT-TopGatherer-Receiver@HistogramReceiverConfig.Storage = [ HLT-Storage@HistogramStorage ]
  HLT-TopGatherer-Receiver@HistogramReceiverConfig.Providers = 'HLT-Rack-.*'

  HLT-TopGathererConfiguration@GATHERERConfiguration
  HLT-TopGathererConfiguration@GATHERERConfiguration.Input = HLT-TopGatherer-Receiver@HistogramReceiverConfig
  HLT-TopGathererConfiguration@GATHERERConfiguration.Output = HLT-TopGatherer-Provider@HistogramProviderConfig

  HLT-TopGatherer@GATHERERApplication
  HLT-TopGatherer@GATHERERApplication.GATHERERConfiguration = [ HLT-TopGathererConfiguration@GATHERERConfiguration ]
  HLT-TopGatherer@GATHERERApplication.Program = Gatherer@Binary
  HLT-TopGatherer@GATHERERApplication.Parameters = "-n HLT-TopGatherer"
  HLT-TopGatherer@GATHERERApplication.RestartParameters = "-n HLT-TopGatherer"

#
# DCM segments x ${NUM_SEGMENTS}
# 

  $(for i in $(seq 1 ${NUM_SEGMENTS}) ; do 
     r=${SEGMENT_RACK[${i}]}
     
     echo DF-Rack-${r}@InfrastructureApplication
     echo DF-Rack-${r}@InfrastructureApplication.Program = is_server@Binary
     echo DF-Rack-${r}@InfrastructureApplication.Parameters = "'-t -1 -s -p \${TDAQ_PARTITION} -n DF-Rack-${r}'"
     echo DF-Rack-${r}@InfrastructureApplication.RestartParameters = "'-t -1 -s -p \${TDAQ_PARTITION} -n DF-Rack-${r}'"
     echo DF-Rack-${r}@InfrastructureApplication.SegmentProcEnvVarValue = '"appId"'
     echo DF-Rack-${r}@InfrastructureApplication.SegmentProcEnvVarName = '"TDAQ_IS_SERVER"'

     echo Histogramming-Rack-${r}@InfrastructureApplication
     echo Histogramming-Rack-${r}@InfrastructureApplication.Program = is_server@Binary
     echo Histogramming-Rack-${r}@InfrastructureApplication.Parameters = "'-t -1 -s -p \${TDAQ_PARTITION} -n Histogramming-Rack-${r}'"
     echo Histogramming-Rack-${r}@InfrastructureApplication.RestartParameters = "'-t -1 -s -p \${TDAQ_PARTITION} -n Histogramming-Rack-${r}'"
     echo Histogramming-Rack-${r}@InfrastructureApplication.SegmentProcEnvVarValue = '"appId"'
     echo Histogramming-Rack-${r}@InfrastructureApplication.SegmentProcEnvVarName = '"TDAQ_OH_SERVER"'

     echo HLT-Storage-Rack-${r}@HistogramStorage
     echo HLT-Storage-Rack-${r}@HistogramStorage.ISServerName = "'Histogramming-Rack-${r}'"

     echo HLT-Receiver-Rack-${r}@HistogramReceiverConfig
     echo HLT-Receiver-Rack-${r}@HistogramReceiverConfig.Storage = [ HLT-Storage-Rack-${r}@HistogramStorage ]

     echo HLT-Provider-Rack-${r}@HistogramProviderConfig
     echo HLT-Provider-Rack-${r}@HistogramProviderConfig.Storage = [ HLT-Storage@HistogramStorage ]
     echo HLT-Provider-Rack-${r}@HistogramProviderConfig.PublishProviderName = "'HLT-Rack-${r}'"
     echo HLT-Provider-Rack-${r}@HistogramProviderConfig.UpdateFrequency = 30

     echo HLT-GathererConfiguration-Rack-${r}@GATHERERConfiguration
     echo HLT-GathererConfiguration-Rack-${r}@GATHERERConfiguration.Input = HLT-Receiver-Rack-${r}@HistogramReceiverConfig
     echo HLT-GathererConfiguration-Rack-${r}@GATHERERConfiguration.Output = HLT-Provider-Rack-${r}@HistogramProviderConfig

     echo HLT-Gatherer-Rack-${r}@GATHERERApplication
     echo HLT-Gatherer-Rack-${r}@GATHERERApplication.GATHERERConfiguration = [ HLT-GathererConfiguration-Rack-${r}@GATHERERConfiguration ]
     echo HLT-Gatherer-Rack-${r}@GATHERERApplication.Program = Gatherer@Binary
     echo HLT-Gatherer-Rack-${r}@GATHERERApplication.Parameters = "'-n HLT-Gatherer-Rack-${r}'"
     echo HLT-Gatherer-Rack-${r}@GATHERERApplication.RestartParameters = "'-n HLT-Gatherer-Rack-${r}'"

     echo DCM-Segment-${r}@HLTSegment 
     echo DCM-Segment-${r}@HLTSegment.IsControlledBy = DefRC@RunControlTemplateApplication 
     echo DCM-Segment-${r}@HLTSegment.TemplateApplications = [ ${DCM_APPLICATION} ]
     echo DCM-Segment-${r}@HLTSegment.TemplateHosts = [ ${SEGMENTS[${i}]} ]
     echo DCM-Segment-${r}@HLTSegment.DefaultHost = ${SEGMENT_CONTROL[${i}]}
     echo DCM-Segment-${r}@HLTSegment.Resources = [ HLT-Gatherer-Rack-${r}@GATHERERApplication ]
     echo DCM-Segment-${r}@HLTSegment.Infrastructure = [ DF-Rack-${r}@InfrastructureApplication , Histogramming-Rack-${r}@InfrastructureApplication ]
    
    done)

#
# HLTSV application
#

  HLTSVConfig@HLTSVConfiguration

  HLTSV@HLTSVApplication
  HLTSV@HLTSVApplication.Program              = hltsv_main@Binary
  HLTSV@HLTSVApplication.RestartableDuringRun = True
  HLTSV@HLTSVApplication.RunsOn               = ${HLTSV_HOST:-${DEFAULT_HOST}}@Computer
  HLTSV@HLTSVApplication.ConfigurationRules   =  HLTSV_Rules@ConfigurationRuleBundle
  HLTSV@HLTSVApplication.Configuration        = HLTSVConfig@HLTSVConfiguration

  mergeDCM@Application
  mergeDCM@Application.Program = dcmMerge@Script
  mergeDCM@Application.InitTimeout = 0
  mergeDCM@Application.StartAt = 'SOR'
  mergeDCM@Application.StopAt = 'EOR'
  mergeDCM@Application.RestartableDuringRun = True
  mergeDCM@Application.IfDies = 'Ignore'
  mergeDCM@Application.IfFailed = 'Ignore'
  mergeDCM@Application.RunsOn = pc-tdq-onl-80.cern.ch@Computer
  
#
# Main HLT segment
# 
  HLT@Segment
  HLT@Segment.IsControlledBy = DefRC@RunControlTemplateApplication
  HLT@Segment.Resources += [ HLTSV@HLTSVApplication , HLT-TopGatherer@GATHERERApplication ]
  HLT@Segment.Applications = [ mergeDCM@Application ]
  HLT@Segment.DefaultHost = pc-tdq-onl-80.cern.ch@Computer

  $(for i in $(seq 1 ${NUM_SEGMENTS}) ; do 
     echo HLT@Segment.Segments += [ DCM-Segment-${SEGMENT_RACK[${i}]}@HLTSegment ]
    done)

# more hacks

  TDAQ_DCM_CONNECT_SLEEP@Variable
  TDAQ_DCM_CONNECT_SLEEP@Variable.Name  = 'TDAQ_DCM_CONNECT_SLEEP'
  TDAQ_DCM_CONNECT_SLEEP@Variable.Value = '30'

  HLT@Segment.ProcessEnvironment = [ TDAQ_DCM_CONNECT_SLEEP@Variable ]


EOF
