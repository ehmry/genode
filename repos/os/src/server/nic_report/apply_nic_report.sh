#
# Configures an interface on a modern Linux system
# from a file containing the XML structure of a Nic
# state ROM.
#

function xpath {
	echo "${1}" | xmllint -xpath "${2}" - 2> /dev/null
}

function find_iface {
	for iface in /sys/class/net/*
	do
		read MAC < ${iface}/address
		if [ "${MAC}" == "${1}" ]
		then
			echo $(basename ${iface})
			return;
		fi
	done
}

function ip_address_add {
	local IFACE=$1
	local XML=$2

	local    ADDR=$(xpath "$XML" 'string(/ipv4/@addr)')
	local NETMASK=$(xpath "$XML" 'string(/ipv4/@netmask)')
	local GATEWAY=$(xpath "$XML" 'string(/ipv4/@GATEWAY)')

	ip address add $ADDR peer $NETMASK dev $IFACE

	if [ -n "$GATEWAY" ]; then
		echo setting gateway address not implemented, see $FUNCNAME at line :$LINENO
	fi
}

# strip any trailing zeros off the input file
REPORT=$(tr -d '\0' < $@)

MAC_ADDR=$(xpath "${REPORT}" 'string(/nic/@mac_addr)')
IFACE=$(find_iface ${MAC_ADDR})

if IPV4_XML=$(xpath "$REPORT" '/nic/ipv4'); then
	ip_address_add "$IFACE" "$IPV4_XML"
fi

if IPV6_XML=$(xpath "$REPORT" '/nic/ip6'); then
	ip_address_add "$IFACE" "$IPV6_XML"
fi
