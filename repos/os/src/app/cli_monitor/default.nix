{ spec, linkComponent, compileCC, base, cli_monitor, config }:

linkComponent {
  name = "cli_monitor";
  libs = [ base cli_monitor config ];
  objects = compileCC {
    src = ./main.cc;
    includes =
      [ ../cli_monitor ] ++
      ( if spec.isx86 then [ ./x86 ] else
        throw "cli_monitor not expressed for ${spec.system}"
      );
  };
}