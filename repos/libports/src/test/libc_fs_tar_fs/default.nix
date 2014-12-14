{ linkComponent, singleton, compileCC, libc, nixpkgs }:

linkComponent rec {
  name = "test-libc_fs_tar_fs";
  libs = [ libc ];
  objects = [(compileCC {
    src = ./main.cc;
    inherit libs;
  })];

  # TODO postLink is handy here, but extraFiles would be nice.
  inherit (nixpkgs) gnutar;
  postLink =
    ''
      mkdir tar; cd tar
      mkdir -p testdir/testdir
      echo -n "a single line of text" > testdir/testdir/test.tst
      mkdir -p testdir/a
      mkdir -p testdir/c
      ln -sf /a testdir/c/d
      ln -sf /c testdir/e
      echo -n "a single line of text" > testdir/a/b
      $gnutar/bin/tar cfv $out/libc_fs_tar_fs.tar .
    '';
}
