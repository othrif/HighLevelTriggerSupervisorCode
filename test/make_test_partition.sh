#!/bin/bash

#
# Execute this in the hltsv/test directory.
# You can move the generated file then somewhere else.
# 
REPOSITORY=$(dirname $(dirname $(pwd)))/installed

pm_part_l2.py -p test_hltsv -I daq/schema/hltsv.schema.xml
pm_set.py test_hltsv.data.xml <<EOF

  ProtoRepo@SW_Repository
  ProtoRepo@SW_Repository.Tags = [ i686-slc5-gcc43-opt@Tag ,  i686-slc5-gcc43-dbg@Tag  ]
  ProtoRepo@SW_Repository.Name = "Proto"
  ProtoRepo@SW_Repository.InstallationPath = "$REPOSITORY"
  ProtoRepo@SW_Repository.Uses = [ Online@SW_Repository ]
  ProtoRepo@SW_Repository.ISInfoDescriptionFiles = [ 'share/data/hltsv/schema/hltsv_is.schema.xml' ] 

  testDCM@Binary
  testDCM@Binary.BinaryName = 'testDCM'
  testDCM@Binary.BelongsTo =  ProtoRepo@SW_Repository

  ProtoRepo@SW_Repository.SW_Objects = [ testDCM@Binary ]

  HLTSVConf@DC_ISResourceUpdate
  HLTSVConf@DC_ISResourceUpdate.name = 'HLTSV'
  HLTSVConf@DC_ISResourceUpdate.delay = 10
  HLTSVConf@DC_ISResourceUpdate.activeOnNodes = [ "HLTSV" ]

  DCAppConf-1@DCApplicationConfig.refDC_ISResourceUpdate += [ HLTSVConf@DC_ISResourceUpdate ]

  HLTSVConfig@HLTSVConfiguration

  HLTSV@HLTSVApplication
  HLTSV@HLTSVApplication.Program = hltsv_main@Binary
  HLTSV@HLTSVApplication.ReceiveMulticast = True
  HLTSV@HLTSVApplication.Configuration = HLTSVConfig@HLTSVConfiguration
  HLTSV@HLTSVApplication.DFApplicationConfig = DCAppConf-1@DCApplicationConfig

  L2-Segment-1@Segment.Applications += [ HLTSV@HLTSVApplication ]
  L2-Segment-1@Segment.Resources -= L2SV-1@L2SVApplication

  L2Master@MasterTrigger.Controller = HLTSV@HLTSVApplication

  L2PU-1@L2PUTemplateApplication.Program = testDCM@Binary
  L2PU-1@L2PUTemplateApplication.Instances = 64

#  L2-Segment-1-1@HLTSegment.TemplateHosts = [ pc-preseries-xpu-001.cern.ch@Computer , pc-preseries-xpu-002.cern.ch@Computer , pc-preseries-xpu-003.cern.ch@Computer , pc-preseries-xpu-004.cern.ch@Computer , pc-preseries-xpu-005.cern.ch@Computer , pc-preseries-xpu-006.cern.ch@Computer , pc-preseries-xpu-007.cern.ch@Computer , pc-preseries-xpu-008.cern.ch@Computer , pc-preseries-xpu-009.cern.ch@Computer , pc-preseries-xpu-012.cern.ch@Computer , pc-preseries-xpu-013.cern.ch@Computer , pc-preseries-xpu-014.cern.ch@Computer , pc-preseries-xpu-015.cern.ch@Computer , pc-preseries-xpu-016.cern.ch@Computer , pc-preseries-xpu-017.cern.ch@Computer , pc-preseries-xpu-018.cern.ch@Computer , pc-preseries-xpu-019.cern.ch@Computer , pc-preseries-xpu-020.cern.ch@Computer , pc-preseries-xpu-021.cern.ch@Computer , pc-preseries-xpu-022.cern.ch@Computer , pc-preseries-xpu-023.cern.ch@Computer , pc-preseries-xpu-024.cern.ch@Computer , pc-preseries-xpu-025.cern.ch@Computer , pc-preseries-xpu-026.cern.ch@Computer , pc-preseries-xpu-027.cern.ch@Computer , pc-preseries-xpu-028.cern.ch@Computer , pc-preseries-xpu-029.cern.ch@Computer , pc-preseries-xpu-030.cern.ch@Computer ]

#  L2-Segment-1-1@HLTSegment.TemplateHosts = [ msu-pc3.cern.ch@Computer , msu-pc7.cern.ch@Computer , msu-pc9.cern.ch@Computer , msu-pc8.cern.ch@Computer  ]

  test_hltsv@Partition.RepositoryRoot = "${REPOSITORY}"

  test_hltsv-lvl1-counter@IS_EventsAndRates.EventCounter = "DF.HLTSV.HLTSV.LVL1Events"
  test_hltsv-lvl1-counter@IS_EventsAndRates.Rate = ""

  test_hltsv-lvl2-counter@IS_EventsAndRates.EventCounter = "DF.HLTSV.HLTSV.ProcessedEvents"
  test_hltsv-lvl2-counter@IS_EventsAndRates.Rate = "DF.HLTSV.HLTSV.Rate"

  test_hltsv@Partition.Disabled = [ ROS-Segment-1@Segment ]

#  Default-DFParameters-1@DFParameters.DefaultDataNetworks = [ '10.149.96.31/24' , '10.149.64.0/24' ]

EOF
