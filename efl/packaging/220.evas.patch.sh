#!/bin/sh
#-------------------------------------
# evas for FOTA
#-------------------------------------

echo "evas binary shader removal for FOTA upgrade"
# cache all remove
/bin/rm -rf /opt/home/app/.cache/evas_gl_common_caches/*.eet
/bin/rm -rf /opt/home/root/.cache/evas_gl_common_caches/*.eet
