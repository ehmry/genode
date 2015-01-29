{
  kernel = "linux";

  #
  # Startup code to be used when building a program and linker script that is
  # specific for Linux. We also reserve the thread-context area via a segment in
  # the program under Linux to prevent clashes with vdso.
  #
  ldTextAddr = "0x01000000";
  ldScriptsStatic = [ ../repos/base/src/platform/genode.ld ../repos/base-linux/src/platform/context_area.nostdlib.ld ];

  pic = true;
}
