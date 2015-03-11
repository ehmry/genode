{ config, lib, pkgs, ... }:

with lib;

let
  runtimeLibs = components:
    let component = builtins.head components; in
    if components == [] then [] else
    (component.runtime.libs or []) ++
    runtimeLibs (builtins.tail components);

  cfgs = map
    (name:
      let
        scenario = builtins.getAttr name config.boot.genode.scenarios;
        genodePkgs = import ./pkgs.nix { inherit (scenario) system; };
        kernel = genodePkgs.kernel;
        componentModules = scenario.romModules genodePkgs;
        romModules = componentModules ++ (runtimeLibs componentModules);

        fileSet = import (pkgs.stdenv.mkDerivation {
          name = "${name}-boot-files";
          prefix = name;
          inherit (scenario) config;
          builder = builtins.toFile "boot-files-builder.sh"
            ''
              source $stdenv/setup
              format="\"$prefix/%f\"=\"%p\";"
              config="\"$prefix/config\"=\"$config\";"
              echo {$config$(find $romModules -type f -printf $format)} > $out
            '';
          romModules = [ genodePkgs.init genodePkgs.libs.ld ] ++ romModules;
        }) // (builtins.listToAttrs (map
	  (fn: { name = "${name}/${fn}"; value = builtins.getAttr fn scenario.extraFiles; })
          (builtins.attrNames scenario.extraFiles)
	));

        moduleLines = map
          (file: "module @bootRoot@${file} ${baseNameOf file}\n")
          (attrNames fileSet);
      in
      { grubEntry =
          ''
            menuentry "Genode - ${name}" {
              multiboot @bootRoot@${name}/${kernel.image} ${toString kernel.args}
	      module @bootRoot@${name}/core core
            ${toString moduleLines}
            }
          '';
        files =
          { "${name}/${kernel.image}" = "${kernel}/${kernel.image}";
            "${name}/core" = "${genodePkgs.core}/core";
          } // fileSet;
      }
    )
    (builtins.attrNames config.boot.genode.scenarios);

  mergeFiles = cfgs:
    let cfg = builtins.head cfgs; in
    if cfgs == [] then {} else
    cfg.files // (mergeFiles (builtins.tail cfgs));
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
      { extraEntries = toString (map (cfg: cfg.grubEntry) cfgs);
        extraFiles = mergeFiles cfgs;
        extraPrepareConfig = toString (map (subdir: "mkdir -p /boot/"+subdir) (builtins.attrNames config.boot.genode.scenarios));
      };
  };
}
