#!/bin/bash

TTSTEXT=$1
TTSFILE=./ttsfile.mp3
CONFFILE=../conf/init.json
LANGSFILE=./listoflangs.json
#TTSLANG=de-AU
TTSLANG=pl
TTSURL="https://texttospeech.googleapis.com/v1/text:synthesize?key="
LISTURL="https://texttospeech.googleapis.com/v1/voices?key="

function _cleanup()
{
	if [ -f $TTSFILE ]; then
		rm $TTSFILE
	fi
}

trap _cleanup EXIT

KEY=$(cat $CONFFILE|jq -r .tts.key)

if [ -z "$TTSTEXT" ]; then
	echo "Generating list of languages to file: $LANGSFILE"
	echo -e "List with samples available under:\nhttps://cloud.google.com/text-to-speech/docs/voices" 
	wget -q -O $LANGSFILE "${LISTURL}${KEY}"
	exit 1
fi


#curl -s -H "Content-Type: application/json; charset=utf-8" -d "{
#'input':{'text':'$TTSTEXT'},
#'voice':{'languageCode':'pl-PL','name':'pl-PL-Standard-A','ssmlGender':'FEMALE'},
#'audioConfig':{'audioEncoding':'MP3'}
#}" "${TTSURL}${KEY}" | jq --raw-output '.audioContent' | base64 --decode > $TTSFILE

#wget -q --header 'Content-Type:application/json' --post-data="{'input':{'text':'$TTSTEXT'},'voice':{'languageCode':'pl-PL','name':'pl-PL-Standard-A','ssmlGender':'FEMALE'},'audioConfig':{'audioEncoding':'MP3'}}" -O- "${TTSURL}${KEY}" > test.file

wget -q --header 'Content-Type:application/json' --post-data="{'input':{'text':'$TTSTEXT'},'voice':{'languageCode':'pl-PL','name':'pl-PL-Standard-E','ssmlGender':'FEMALE'},'audioConfig':{'audioEncoding':'MP3'}}" -O- "${TTSURL}${KEY}" | jq --raw-output '.audioContent' | base64 --decode > $TTSFILE 
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

