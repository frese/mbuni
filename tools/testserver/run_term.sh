source config.sh

pushd dist
while [ 1 ]; do
	sleep 1
	date >> $terminating_log
	if [ $terminating_async_mode="true" ]; then
		ASYNC="A"
        else
		ASYNC=""
	fi
	java -cp .:../jars/MMSLibrary.jar TerminatingApp $terminating_port $ASYNC | tee -a $terminating_log
done
popd
