source config.sh

echo "sending to $sendmms_eaif_url"
pushd dist
java -cp .:../jars/MMSLibrary.jar OriginatingApp "$sendmms_eaif_url"
popd

