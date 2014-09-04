nix-build \
    --no-out-link \
    specs/x86_32-linux.nix specs/x86_64-linux.nix \
    specs/x86_32-nova.nix specs/x86_64-nova.nix \
    -A base.test     -A base.run \
    -A os.test       -A os.run   \
    -A demo.test
