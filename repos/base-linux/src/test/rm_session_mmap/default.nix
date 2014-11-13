{ genodeEnv, base }:

genodeEnv.mkComponent {
  name = "test-rm_session_mmap";
  libs = [ base ];
  sources = genodeEnv.fromPath ./main.cc;
  systemIncludes =
    import ../../../../os/include { inherit genodeEnv; };
}
