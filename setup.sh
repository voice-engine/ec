#!/bin/sh


DIR=/var/tmp/ec
CAPTURE=${DIR}/capture.pipe
PLAYBACK=${DIR}/playback.pipe

mkdir -p /var/tmp/ec
test -p ${CAPTURE}  || ( rm -rf ${CAPTURE}  && mkfifo -m 0664 ${CAPTURE}  )
test -p ${PLAYBACK} || ( rm -rf ${PLAYBACK} && mkfifo -m 0662 ${PLAYBACK} )

