#!/bin/bash
REPOSITORY=$(dirname $(dirname $(pwd)))/installed

# make a copy...
cp  /det/tdaq/db/tdaq-03-00-01/daq/segments/L2/L2-Segment-1.data.xml .
# cp  L2-Segment-1.data.xml.orig L2-Segment-1.data.xml 

export DCAPPCONF=L2-DCApplicationConfig-1

# case $(basename $(dirname $TDAQ_INST_PATH)) in
#     nightly)
#         export DCAPPCONF=DCAppConf-1
#         ;;
#     tdaq-03-00-01)
#         export DCAPPCONF=DCApplicationConfig-1
#         ;;
# esac

./pm_set.py -I daq/hw/hosts-dc.data.xml L2-Segment-1.data.xml <<EOF

  ProtoRepo@SW_Repository
  ProtoRepo@SW_Repository.Tags = [ i686-slc5-gcc43-opt@Tag ,  i686-slc5-gcc43-dbg@Tag  ]
  ProtoRepo@SW_Repository.Name = "Proto"
  ProtoRepo@SW_Repository.InstallationPath = "$REPOSITORY"
  ProtoRepo@SW_Repository.Uses = [ Online@SW_Repository ]
  ProtoRepo@SW_Repository.ISInfoDescriptionFiles = [ 'hltsv/schema/hltsv_is.schema.xml' ] 

  hltsv_main@Binary
  hltsv_main@Binary.BinaryName = 'hltsv_main'
  hltsv_main@Binary.BelongsTo =  ProtoRepo@SW_Repository

  testDCM@Binary
  testDCM@Binary.BinaryName = 'testDCM'
  testDCM@Binary.BelongsTo =  ProtoRepo@SW_Repository

  HLTSVConf@DC_ISResourceUpdate
  HLTSVConf@DC_ISResourceUpdate.name = 'HLTSV'
  HLTSVConf@DC_ISResourceUpdate.delay = 10
  HLTSVConf@DC_ISResourceUpdate.activeOnNodes = [ "L2SV" ]

  ${DCAPPCONF}@DCApplicationConfig.refDC_ISResourceUpdate += [ HLTSVConf@DC_ISResourceUpdate ]

  L2SV-1@L2SVApplication.Program = hltsv_main@Binary
  L2SV-1@L2SVApplication.ReceiveMulticast = True

  L2SV-1@L2SVApplication.RunsOn = pc-tdq-dc-01.cern.ch@Computer

  L2PU-1@L2PUTemplateApplication.Program = testDCM@Binary
  L2PU-1@L2PUTemplateApplication.Instances = 1

  NUM_ASSIGN_THREADS@Variable
  NUM_ASSIGN_THREADS@Variable.Name = 'NUM_ASSIGN_THREADS'
  NUM_ASSIGN_THREADS@Variable.Value = 8

  L2SV-1@L2SVApplication.ProcessEnvironment += [ NUM_ASSIGN_THREADS@Variable ]

  L2SV-Segment-2@Segment.Applications -= L2SV-2@L2SVApplication
  L2SV-Segment-3@Segment.Applications -= L2SV-3@L2SVApplication
  L2SV-Segment-4@Segment.Applications -= L2SV-4@L2SVApplication
  L2SV-Segment-5@Segment.Applications -= L2SV-5@L2SVApplication

  hltsv-lvl1-counter@IS_EventsAndRates
  hltsv-lvl1-counter@IS_EventsAndRates.EventCounter = "DF.L2SV-1.HLTSV.LVL1Events"
  hltsv-lvl1-counter@IS_EventsAndRates.Rate = ""

  hltsv-lvl2-counter@IS_EventsAndRates
  hltsv-lvl2-counter@IS_EventsAndRates.EventCounter = "DF.L2SV-1.HLTSV.ProcessedEvents"
  hltsv-lvl2-counter@IS_EventsAndRates.Rate = "DF.L2SV-1.HLTSV.Rate"

EOF
