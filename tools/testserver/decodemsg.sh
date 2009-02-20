pushd dist
java -cp .:../jars/MMSLibrary.jar DecodeMessage $1
popd
