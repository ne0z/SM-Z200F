#!/bin/sh

echo "gstreamer-registry script start"

#delete old cache file
rm -f /opt/home/app/.cache/gstreamer-1.0/registry.bin
rm -f /opt/home/system/.cache/gstreamer-1.0/registry.bin

echo "gstreamer-registry end"
