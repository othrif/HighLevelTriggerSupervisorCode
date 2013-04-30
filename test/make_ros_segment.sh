#!/bin/bash

# Configuration
ROS_HOST=$(hostname)
#ROS_HOST=pc-tbed-r3-02.cern.ch

# Create a ROS segment with a single ROS machine
# and add it to your partition

if [ $# -lt 1 ]; then
     echo "Usage: $0 <partition_name> [ <ros_host> ]"
     exit 1
fi     

PARTITION=$1

if [ $# -gt 1 ]; then
     ROS_HOST=$2
fi

echo "Modifying partition $PARTITION using $ROS_HOST as host for ROS"

# create ROS segment
pm_seg_ros.py --use-ros --ros-mode emulated-dc

# fix it up
pm_set.py -I daq/schema/DFTriggerIn.schema.xml -I daq/hw/hosts.data.xml ROS-Segment-1-emulated-dc.data.xml << EOF
   DFTriggerIn@DFTriggerIn
   ROS-PIXEL_BARREL-00@ROS.Trigger =  [ DFTriggerIn@DFTriggerIn ]
   ROS-PIXEL_BARREL-00@ROS.Output = None
   ROS-PIXEL_BARREL-00@ROS.RunsOn = ${ROS_HOST}@Computer
   ROS-Segment-1-MemoryPool@MemoryPool.NumberOfPages = 1000
EOF

if [ $? == 0 ]; then
pm_set.py -I  ROS-Segment-1-emulated-dc.data.xml ${PARTITION}.data.xml << EOF
  ${PARTITION}@Partition.Segments -= ROS@Segment
  ${PARTITION}@Partition.Segments += [ ROS-Segment-1-emulated-dc@Segment ]
EOF
fi


