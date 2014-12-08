/*
 * \author Emery Hemingway
 * \date   2014-09-19
 */

{ preparePort, fetchsvn, flex, bison, glibc, gcc }:

let
  version = "8.2.0";
  baseUrl = "http://svn.freebsd.org/base/release/${version}";

  subdirs =
    [ { name = "libc";
        dir = "lib/libc";
        sha256 = "1p4s33gl51z8alxnq4311icjcpbzl3wv1gp5p89md6ivk1biwkck";
      }
      { name = "gen";
        dir = "lib/libc/gen";
        sha256 = "019wxwwd3ha3yxvjq0qkzm71cl19q3c6g7q1skcx3zgnxn221amr";
      }
      { name = "libutil";
        dir = "lib/libutil";
        sha256 = "0zdirqwdfpzz4ykl30azbnknv89cs1p640d3m20313hqlpglwy13";
      }
      { name = "include";
        dir = "include";
        sha256 = "11xdlylf7ymk2jwh6cs9hm25b39z6vm96iwj0sain8f1c024x6i8";
      }
      { name = "sys_sys";
        dir = "sys/sys";
        sha256 = "1fy62y1hcikl6bpcabcx893dicj8lz4p97kynarbhqdbsil9b06q";
      }
      { name = "sys_rpc";
        dir = "sys/rpc";
        sha256 = "0l28zmn5bjfs6b07qhaq4qhgg1zzx1vr5ky7y1nkrd0gdm2w59jw";
      }
      { name = "sys_net";
        dir = "sys/net";
        sha256 = "0rilj0l6gclx14gbr2jzdbn0h4q2a3amd421i7w6mdi27lvqbf29";
      }
      { name = "sys_netinet";
        dir = "sys/netinet";
        sha256 = "1v8x819wljd87cdqfa86aj2qm654p8c104qvim6xrzlpwm6dqbmv";
      }
      { name = "sys_inet6";
        dir = "sys/netinet6";
        sha256 = "0aykc27qkqzkrxqjx4hwsjgk8kwd9pifisccbmai0dvik8xq0f10";
      }
      { name = "sys_bsm";
        dir = "sys/bsm";
        sha256 = "0b549xmdp4ahv90gms9pha294nvpdpalmn342w8wj0djqsmizn91";
      }
      { name = "sys_vm";
        dir = "sys/vm";
        sha256 = "0lfz39nfklkq6qscih00a2qhjsl6sj53whvmsf8hhvpz5lnbhz46";
      }
      { name = "sys_arm";
        dir = "sys/arm/include";
        sha256 = "1bb0wayn9djq2yx2rj4c862jp5p494k8qjlgizwxl652ns7xr2vk";
      }
      { name = "sys_i386";
        dir = "sys/i386/include";
        sha256 = "0i36j64nc1jbakaaqgsakis6nrmigv212z3rqradrx06qqiq016j";
      }
      { name = "sys_amd64";
        dir = "sys/amd64/include";
        sha256 = "1wb4iq59wv5ckvkm4i5bzc7h8k7dlivbg2w2s48hyh17y2cdgax9";
      }
      { name = "msun";
        dir = "lib/msun";
        sha256 = "1yajfn06b1syl8y37a2rmq61cvq8d8922mgd01rpcj7wl8xarlp9";
      }
      { name = "gdtoa";
        dir = "contrib/gdtoa";
        sha256 = "068ivpzi279gnahgggng0w10rd31psiyl8gpk3gr37pf2fzdiyw7";
      }
    ];

in
preparePort {
  name = "libc-freebsd-${version}";

  builder = ./libc-builder.sh;

  buildInputs = [ flex bison glibc gcc ];

  srcs = map
    ( {name, dir, sha256 }:
      fetchsvn {
        url = "${baseUrl}/${dir}";
        inherit name sha256;
      }
    ) subdirs;

  sourceRoot = ".";
    
  repos = map (x: x.name) subdirs;
  dirs  = map (x: x.dir ) subdirs;

  patches = "${../src/lib/libc/patches}/*.patch";
  patchFlags = "-p0 --strip 3";
}
