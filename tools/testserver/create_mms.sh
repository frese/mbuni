source config.sh

echo "Creating sample mms"
pushd dist
java -cp .:../jars/MMSLibrary.jar SampleMMSCreation
popd

