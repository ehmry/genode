#
# lwIP TCP/IP library
#
# The library implementes TCP and UDP as well as DNS and DHCP.
#

{ linkSharedLibrary, compileCRepo, compileCC, addPrefix, fromDir, lwipSrc
, alarm, libc, timed_semaphore }:

let
  includes =
    [ ./include
    ];
  externalIncludes =
    [ lwipSrc.include
      ../../../include/lwip
    ];

  headers =
    [ ../../../include/lwip/arch/cc.h ] ++
    (addPrefix "${lwipSrc.include}/lwip/"
      [ "debug.h" "opt.h" "arch.h" "sys.h" ]);
in
linkSharedLibrary rec {
  name = "lwip";
  libs = [ alarm libc timed_semaphore ];

  objects = map
    (src: compileCC {
      inherit src libs includes externalIncludes headers; })
    [ # Genode platform files
      ./platform/nic.cc ./platform/printf.cc ./platform/sys_arch.cc
    ];

  externalObjects = compileCRepo {
    inherit libs includes externalIncludes headers;
    sources = fromDir lwipSrc (
      ( # Core files
        addPrefix "core/"
          [ "init.c" "mem.c" "memp.c" "netif.c" "pbuf.c" "stats.c"
            "udp.c" "raw.c" "sys.c" "tcp.c" "tcp_in.c" "tcp_out.c"
            "dhcp.c" "dns.c" "timers.c" "def.c" "inet_chksum.c"
          ]
      ) ++
      ( # IPv4 files
        addPrefix "core/ipv4/"
          [ "icmp.c" "igmp.c" "ip4_addr.c" "ip4.c" "ip_frag.c" ]
      ) ++
      ( # API files
        addPrefix "api/"
          [ "err.c" "api_lib.c" "api_msg.c" "netbuf.c" "netdb.c"
            "netifapi.c" "sockets.c" "tcpip.c"
          ]
      )++
      [ "netif/etharp.c" ] # Network interface files
    );

    extraFlags = [ "-DERRNO" ];
  };

  propagate = { inherit includes externalIncludes; };
}
