#!/bin/bash
REPOSITORY=$(dirname $(dirname $(pwd)))/installed


case $(basename $(dirname $TDAQ_INST_PATH)) in
    nightly)
        export DCAPPCONF=DCAppConf-1
        ;;
    tdaq-03-00-01)
        export DCAPPCONF=DCApplicationConfig-1
        ;;
esac

#FARM=
FARM="-I farm.data.xml"
#FARM="-I daq/hw/hosts-preseries.data.xml"

pm_part_l2.py -p test_hltsv $FARM
pm_set.py test_hltsv.data.xml <<EOF

  ProtoRepo@SW_Repository
  ProtoRepo@SW_Repository.Tags = [ i686-slc5-gcc43-opt@Tag ,  i686-slc5-gcc43-dbg@Tag  ]
  ProtoRepo@SW_Repository.Name = "Proto"
  ProtoRepo@SW_Repository.InstallationPath = "$REPOSITORY"
  ProtoRepo@SW_Repository.Uses = [ Online@SW_Repository ]
  ProtoRepo@SW_Repository.ISInfoDescriptionFiles = [ 'share/data/hltsv_is.schema.xml' ] 

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

#  L2SV-1@L2SVApplication.RunsOn = pc-preseries-xpu-031.cern.ch@Computer
  L2SV-1@L2SVApplication.RunsOn = msu-pc6.cern.ch@Computer

  L2PU-1@L2PUTemplateApplication.Program = testDCM@Binary
  L2PU-1@L2PUTemplateApplication.Instances = 128

  NUM_ASSIGN_THREADS@Variable
  NUM_ASSIGN_THREADS@Variable.Name = 'NUM_ASSIGN_THREADS'
  NUM_ASSIGN_THREADS@Variable.Value = 6

  L2SV-1@L2SVApplication.ProcessEnvironment += [ NUM_ASSIGN_THREADS@Variable ]

#  L2-Segment-1-1@HLTSegment.TemplateHosts = [ pc-preseries-xpu-001.cern.ch@Computer , pc-preseries-xpu-002.cern.ch@Computer , pc-preseries-xpu-003.cern.ch@Computer , pc-preseries-xpu-004.cern.ch@Computer , pc-preseries-xpu-005.cern.ch@Computer , pc-preseries-xpu-006.cern.ch@Computer , pc-preseries-xpu-007.cern.ch@Computer , pc-preseries-xpu-008.cern.ch@Computer , pc-preseries-xpu-009.cern.ch@Computer , pc-preseries-xpu-012.cern.ch@Computer , pc-preseries-xpu-013.cern.ch@Computer , pc-preseries-xpu-014.cern.ch@Computer , pc-preseries-xpu-015.cern.ch@Computer , pc-preseries-xpu-016.cern.ch@Computer , pc-preseries-xpu-017.cern.ch@Computer , pc-preseries-xpu-018.cern.ch@Computer , pc-preseries-xpu-019.cern.ch@Computer , pc-preseries-xpu-020.cern.ch@Computer , pc-preseries-xpu-021.cern.ch@Computer , pc-preseries-xpu-022.cern.ch@Computer , pc-preseries-xpu-023.cern.ch@Computer , pc-preseries-xpu-024.cern.ch@Computer , pc-preseries-xpu-025.cern.ch@Computer , pc-preseries-xpu-026.cern.ch@Computer , pc-preseries-xpu-027.cern.ch@Computer , pc-preseries-xpu-028.cern.ch@Computer , pc-preseries-xpu-029.cern.ch@Computer , pc-preseries-xpu-030.cern.ch@Computer ]

  L2-Segment-1-1@HLTSegment.TemplateHosts = [ msu-pc3.cern.ch@Computer , msu-pc7.cern.ch@Computer , msu-pc9.cern.ch@Computer , msu-pc8.cern.ch@Computer  ]

  test_hltsv@Partition.RepositoryRoot = "${REPOSITORY}"

  test_hltsv-lvl1-counter@IS_EventsAndRates.EventCounter = "DF.L2SV-1.HLTSV.LVL1Events"
  test_hltsv-lvl1-counter@IS_EventsAndRates.Rate = ""

  test_hltsv-lvl2-counter@IS_EventsAndRates.EventCounter = "DF.L2SV-1.HLTSV.ProcessedEvents"
  test_hltsv-lvl2-counter@IS_EventsAndRates.Rate = "DF.L2SV-1.HLTSV.Rate"

#  Default-DFParameters-1@DFParameters.DefaultDataNetworks = [ '10.149.96.31/24' , '10.149.64.0/24' ]

EOF
