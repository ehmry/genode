/*
 * \brief  Ports packages
 * \author Emery Hemingway
 * \date   2014-09-18
 */

{ spec, tool, callComponent }:

let

  # Append 'Src' to each attribute in ports.
  ports =
    let p = import ./ports { inherit tool; }; in
    builtins.listToAttrs (map
      (n: { name = n+"Src"; value = builtins.getAttr n p; })
      (builtins.attrNames p)
  );

  importComponent = path: callComponent ports (import path);

in
{
  app =
    { dosbox = importComponent ./src/app/dosbox;
      nix =
        { # Corepkgs are Nix expressions required for any evaulation.
          corepkgs = tool.shellDerivation {
             name = "nix-corepkgs";
             inherit (tool) genodeEnv;
             inherit (ports) nixSrc;
             script = builtins.toFile "nix-corepkgs.sh"
               ''
                 source $genodeEnv/setup
                 mkdir $out
                 cp -r $nixSrc/corepkgs $out
               '';
           };
           #nix-instantiate = importComponent ./src/app/nix/nix-instantiate;
        };
      seoul  = importComponent ./src/app/seoul;
    };

  noux.minimal = importComponent ./src/noux/minimal;
  noux.net     = importComponent ./src/noux/net;

  test =
    { nix-eval = importComponent ./src/test/nix-eval;
      vmm_utils = importComponent ./src/test/vmm_utils;
    };
}
