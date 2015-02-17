{ config, lib, pkgs, ... }:

with lib;

let
  runtimeLibs = components:
    let component = builtins.head components; in
    if components == [] then [] else
    (component.runtime.libs or []) ++
    runtimeLibs (builtins.tail components);

  scenarios = map
    (name:
      let
        scenario = builtins.getAttr name config.boot.genode.scenarios;
        genodePkgs = import ./pkgs.nix { system = scenario.system; };
        kernel = genodePkgs.kernel;
        componentModules = scenario.romModules genodePkgs;
        romModules = componentModules ++ (runtimeLibs componentModules);
        moduleLines = map
		  (name: " module @bootRoot@${name} ${name}\n")
          ( (map (m: m.name) romModules) ++
            (builtins.attrNames scenario.extraFiles or {})
          );
      in
      { grubEntry =
          ''
            menuentry "Genode - ${name}" {
              multiboot @bootRoot@${kernel.name} ${toString kernel.args}
              module @bootRoot@core core
              module @bootRoot@config config
              module @bootRoot@init init
            ${toString moduleLines}
            }
          '';

        grubFileList =
          [ { name = "config"; value = scenario.config; } ] ++
          (map
            (m: { name = m.name; value = "${m}/${m.name}"; })
            (romModules ++ [ genodePkgs.kernel genodePkgs.core genodePkgs.init ])
          );
      }
    )
    (builtins.attrNames config.boot.genode.scenarios);

  extraFiles = scenarios:
    let scenario = builtins.head scenarios; in
    if scenarios == [] then {} else
    (scenario.extraFiles or {}) // (extraFiles (builtins.tail scenarios));
in
{
  options = {
    boot.genode = {

      scenarios = mkOption {
        type = types.attrsOf (types.submodule (
          { options, ... }:
          { options =
            { system = mkOption {
                type = types.str;
                example = "x86_64-nova";
              };
              config = mkOption { type = types.path; };
              romModules = mkOption { };
              extraFiles = mkOption {
                type = types.attrsOf types.path;
              };
            };
          }
        ));
      };

    };
  };

  config = {

    boot.loader.grub =
      #if config.boot.loader.grub.version != 2 then throw "Genode is only supported with GRUB 2" else
      { extraEntries = toString (map (s: s.grubEntry) scenarios);
        extraFiles =
          (extraFiles scenarios)
          //
          (builtins.listToAttrs (builtins.concatLists (map (s: s.grubFileList) scenarios)));
      };
  };
}