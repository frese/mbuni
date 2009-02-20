sendmms_url="http://localhost:10001/?username=tester&password=foobar&from=1202&to=004530235242&mmsc=nokia"

sendmms_eaif_host="localhost"
sendmms_eaif_port="8787"
sendmms_eaif_user="user"
sendmms_eaif_pass="pass"
#sendmms_eaif_url="http://$sendmmsm_eaif_user:$sendmm_eaif_pass@$sendmms_eaif_host:$sendmms_eaif_port/?user=$sendmms_eaif_user&pass=$sendmms_eaif_pass"
sendmms_eaif_url="http://$sendmmsm_eaif_user:$sendmm_eaif_pass@$sendmms_eaif_host:$sendmms_eaif_port/"

terminating_log="$(pwd)/log/terminating.log"
terminating_async_mode="false"
terminating_port="7000"
