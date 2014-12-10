#
# lwIP TCP/IP library
#
# The library implementes TCP and UDP as well as DNS and DHCP.
#

{ linkSharedLibrary, compileCRepo, compileCC, lwipSrc
, alarm, libc, timed_semaphore }:

let
  systemIncludes =
    [ ./include
      ../../../include/lwip
      lwipSrc.include
    ];
in
linkSharedLibrary rec {
  name = "lwip";
  libs = [ alarm libc timed_semaphore ];

  objects = map (src: compileCC { inherit src libs systemIncludes; })
    [ # Genode platform files
      ./platform/nic.cc ./platform/printf.cc ./platform/sys_arch.cc
    ];

  externalObjects = compileCRepo {
    sourceRoot = lwipSrc;
    inherit libs systemIncludes;
    sources =
      ( # Core files
        map (fn: "core/"+fn) [
          "init.c" "mem.c" "memp.c" "netif.c" "pbuf.c" "stats.c"
          "udp.c" "raw.c" "sys.c" "tcp.c" "tcp_in.c" "tcp_out.c"
          "dhcp.c" "dns.c" "timers.c" "def.c" "inet_chksum.c"
        ]
      ) ++
      ( # IPv4 files
        map (fn: "core/ipv4/"+fn)
          [ "icmp.c" "igmp.c" "ip4_addr.c" "ip4.c" "ip_frag.c" ]
      ) ++
      ( # API files
        map (fn: "api/"+fn) [
          "err.c" "api_lib.c" "api_msg.c" "netbuf.c" "netdb.c"
          "netifapi.c" "sockets.c" "tcpip.c"
        ]
      )++
      [ "netif/etharp.c" ]; # Network interface files

    extraFlags = [ "-DERRNO" ];
  };

}
