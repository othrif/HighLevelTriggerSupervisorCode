#!/usr/bin/env pm_set.py  
  HLT@Segment.Applications += [ TTC2LAN@RunControlApplication ]
  HLTSV@HLTSVApplication.RoIBInput = ttc2lan@RoIBPluginTTC2LAN
