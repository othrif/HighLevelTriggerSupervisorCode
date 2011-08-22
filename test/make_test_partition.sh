#!/bin/bash
REPOSITORY=$(dirname $(dirname $(pwd)))/installed
pm_part_l2.py -p test_hltsv
pm_set.py test_hltsv.data.xml <<EOF

  hltsv_main@Binary
  hltsv_main@Binary.BinaryName = 'hltsv_main'
  hltsv_main@Binary.BelongsTo =  Online@SW_Repository

  testDCM@Binary
  testDCM@Binary.BinaryName = 'testDCM'
  testDCM@Binary.BelongsTo =  Online@SW_Repository

  HLTSVConf@DC_ISResourceUpdate
  HLTSVConf@DC_ISResourceUpdate.name = 'HLTSV'
  HLTSVConf@DC_ISResourceUpdate.delay = 10
  HLTSVConf@DC_ISResourceUpdate.activeOnNodes = [ "L2SV" ]

  DCAppConf-1@DCApplicationConfig.refDC_ISResourceUpdate += [ HLTSVConf@DC_ISResourceUpdate ]

  L2SV-1@L2SVApplication.Program = hltsv_main@Binary
  L2SV-1@L2SVApplication.ReceiveMulticast = True

  L2PU-1@L2PUTemplateApplication.Program = testDCM@Binary
  L2PU-1@L2PUTemplateApplication.Instances = 8

  test_hltsv@Partition.RepositoryRoot = "${REPOSITORY}"

EOF
