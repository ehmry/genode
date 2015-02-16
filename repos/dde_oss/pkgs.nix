{ spec, tool, callComponent, ... }:

let

  ports = import ./ports { inherit tool; };
  # Append 'Src' to each attribute in ports.
  ports' = builtins.listToAttrs (
    map
      (n: { name = n+"Src"; value = builtins.getAttr n ports; })
      (builtins.attrNames ports)
  );

in
{ driver.audio_out =  callComponent ports' (import ./src/drivers/audio_out); }
