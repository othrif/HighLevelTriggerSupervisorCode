#!/bin/bash
#
# Create a file with various L1source configurations
#

pm_set.py -n -I daq/schema/hltsv.schema.xml l1source.data.xml <<EOF

   internal@RoIBPluginInternal
   internal@RoIBPluginInternal.Libraries = [ "libsvl1internal" ]
   internal@RoIBPluginInternal.IsMasterTrigger = True
   internal@RoIBPluginInternal.FragmentSize = 240

   preloaded@RoIBPluginPreload
   preloaded@RoIBPluginPreload.Libraries = [ "libsvl1preloaded" ]
   preloaded@RoIBPluginPreload.IsMasterTrigger = True

   filar@RoIBPluginFilar
   filar@RoIBPluginFilar.Libraries = [ "libsvl1filar" ]
   filar@RoIBPluginFilar.IsMasterTrigger = False
   filar@RoIBPluginFilar.Links = [ 0 , 1 ]

   ttc2lan@RoIBPluginTTC2LAN
   ttc2lan@RoIBPluginTTC2LAN.Libraries = [ "libsvl1ttc2lan" ]
   ttc2lan@RoIBPluginTTC2LAN.IsMasterTrigger = True
   ttc2lan@RoIBPluginTTC2LAN.Networks = [ '137.138.0.0/255.255.0.0' ]

EOF