{ linkStaticLibrary, compileLibc }:

linkStaticLibrary {
  name = "libc-net";
  externalObjects = compileLibc {

    # needed for name6.c, contains res_private.h
    localIncludes = [ "lib/libc/resolv" ];
    systemIncludes = [ "sys/rpc" "sys" ];

    sources = map (fn: "lib/libc/net/"+fn)
      [ # needed for compiling getservbyname() and getservbyport()
        "getservent.c" "nsdispatch.c" "nsparser.c" "nslexer.c"

        # needed for getaddrinfo()
        "getaddrinfo.c"

        # needed for getnameinfo()
        "getnameinfo.c" "name6.c"

        # needed for gethostbyname()
        "gethostnamadr.c" "gethostbydns.c" "gethostbyht.c" "map_v4v6.c"

        # needed for getprotobyname()
        "getprotoent.c" "getprotoname.c"

        # defines in6addr_any
        "vars.c"

        # b64_ntop
        "base64.c"

        "rcmd.c" "rcmdsh.c"
      ];
  };

}
