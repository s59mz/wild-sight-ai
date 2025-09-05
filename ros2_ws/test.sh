#!/bin/bash

gst-launch-1.0 videotestsrc pattern=smpte ! videoconvert ! \
  video/x-raw,format=BGRA,width=1920,height=1080 ! \
  kmssink driver-name=xlnx plane-id=40 sync=false
