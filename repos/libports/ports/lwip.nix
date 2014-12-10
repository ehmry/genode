{ genodeEnv, preparePort, fetchgit, fetchurl }:

let
  windowScalingPatch =  fetchurl {
    name = "window_scaling.patch";
    url = https://savannah.nongnu.org/patch/download.php?file_id=28026;
    sha256 = "0vz4932w1i6ns1ibm998qyzzd9pbwg5l4jp8wci848080lpamp5q";
  };
in
preparePort {
  name = "lwip";
  outputs = [ "source" "include" ];

  src = fetchgit {
    url = git://git.savannah.nongnu.org/lwip.git;
    rev = "fe63f36656bd66b4051bdfab93e351a584337d7c";
    sha256 = "0n6gby3bvnvfba2f50di0qr0bj9i1ijnl5dscpbr0dp4lz6d1mr6";
  };

  prePatch = "cat ${windowScalingPatch} | patch -p1";

  patches =
    [ ../src/lib/lwip/errno.patch
      ../src/lib/lwip/libc_select_notify.patch
      ../src/lib/lwip/sockets_c_errno.patch
      ../src/lib/lwip/sol_socket_definition.patch
    ];

  patchFlags = "-p3";

  installPhase =
    ''
      cp -r src $source
      mkdir -p $include/lwip $include/netif

      cp \
          src/include/lwip/*.h \
          src/include/ipv4/lwip/*.h \
          src/include/ipv6/lwip/*.h \
          $include/lwip

      cp src/include/netif/*.h $include/netif
    '';
}
