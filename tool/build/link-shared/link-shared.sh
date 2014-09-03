source @common@/setup

dumpVars

MSG_MERGE $name

VERBOSE $ld -o $shared/$libSo -shared --eh-frame-hdr \
    $ldOpt \
    -T $ldScriptSo \
    --entry=$entryPoint \
    --whole-archive \
    --start-group \
    $usedSoFiles $staticLibsBrief $objects \
    --end-group \
    --no-whole-archive \
    $libgcc
