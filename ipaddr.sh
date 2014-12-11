#!/bin/bash
ipaddr=$(LANG=C ifconfig eth0		|
		grep "inet addr"	|
		awk '{print $2}'	|
		sed 's/addr://'		)
echo $ipaddr
./proxy -1 10080 $ipaddr
exit 0
