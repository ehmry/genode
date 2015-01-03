{ spec, tool, pkgs }:

with tool;

let
  difference = x: y: builtins.filter (e: !builtins.elem e x) y;
  intersect  = x: y: builtins.filter (e:  builtins.elem e x) y;
in
rec {

  provides = node:
    builtins.concatLists (map provides node.components or []) ++
    node.runtime.provides or [];

  requires = node:
    let
      above = builtins.concatLists (map requires node.components or []);
      below = difference (provides node) (above ++ requires.node or []);
      #maskedAbove = intersect above node.maskAbove or [];
      #maskedBelow = intersect below node.maskBelow or [];
    in
    #if maskedAbove != []
    #then throw "${node.name} masked its children from ${maskedAbove}" else
    #if maskedBelow != []
    #then throw "${node.name} masked its parent from ${maskedBelow}"   else
    below ++ node.runtime.requires or [];

  startChild = child:
    { name = child.name;
      resources = child.runtime.resources or { RAM = "1M"; };
      provides  = child.runtime.provides  or [];
    };


  mkInitConfig =
    { name ? "init", components }:
    let
      self =
        { inherit name components;
          runtime.requires =
            [ "CAP" "CPU" "IO_MEM" "LOG" "PD" "RAM" "RM" "ROM" "SIGNAL" ];
        };
    in
    derivation {
      name = name+".xml";
      system = builtins.currentSystem;
      builder = "${nixpkgs.bash}/bin/sh";
      args =
        [ "-c"
          ''
            echo $xml | \
            ${nixpkgs.libxslt}/bin/xsltproc $stylesheet - | \
            ${nixpkgs.gnused}/bin/sed '/<?/d' > $out
          ''
        ];

      stylesheet = ./nix-conversion.xsl;

      xml = builtins.toXML
        { parent-provides = requires self;
          children = map startChild components;
        };
    };

  test =
    let
      args = { inherit tool pkgs; };
      platform =
        if spec.isLinux then import ../../repos/base-linux/tool/test args else
        if spec.isNOVA  then import ../../repos/base-nova/tool/test  args else
        abort "no test framework for ${spec.system}";
    in
    import ./test (args // { inherit mkInitConfig; } // platform);
}
