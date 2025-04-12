#!/bin/bash

TTSTEXT=$1
TTSFILE=./ttsfile.mp3
#TTSLANG=de-AU
TTSLANG=pl
TTSURL="https://translate.google.com/translate_tts?client=tw-ob&tl=$TTSLANG&q="
#TTSURL="https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=$TTSLANG&q="

function _cleanup()
{
	if [ -f $TTSFILE ]; then
		rm $TTSFILE
	fi
}

trap _cleanup EXIT

if [ -z "$TTSTEXT" ]; then
	echo "Please insert text to be said"
	exit 1
fi

wget -q -U Mozilla -O $TTSFILE "${TTSURL}${TTSTEXT}" 
if [ $? -eq 0 ]; then
	play $TTSFILE &> /dev/null
	if [ $? -eq 0 ]; then
		echo "Text successfully translated into speech"
		exit 0
	else
		echo "[ERROR] Cannot play translated audio file!"
		exit 3
	fi
else
	echo "[ERROR] Cannot fetch translaed audio file!"
	exit 2
fi

