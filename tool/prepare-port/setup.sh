# Run the named hook, either by calling the function with that name or
# by evaluating the variable with that name.  This allows convenient
# setting of hooks both from Nix expressions (as attributes /
# environment variables) and from shell scripts (as functions).
runHook() {
    local hookName="$1"
    case "$(type -t $hookName)" in
        (function|alias|builtin) $hookName;;
        (file) source $hookName;;
        (keyword) :;;
        (*) eval "${!hookName}";;
    esac
}


exitHandler() {
    exitCode=$?
    set +e

    closeNest

    if [ -n "$showBuildStats" ]; then
        times > "$NIX_BUILD_TOP/.times"
        local -a times=($(cat "$NIX_BUILD_TOP/.times"))
        # Print the following statistics:
        # - user time for the shell
        # - system time for the shell
        # - user time for all child processes
        # - system time for all child processes
        echo "build time elapsed: " ${times[*]}
    fi

    if [ $exitCode != 0 ]; then
        runHook failureHook

        # If the builder had a non-zero exit code and
        # $succeedOnFailure is set, create the file
        # `$out/nix-support/failed' to signal failure, and exit
        # normally.  Otherwise, return the original exit code.
        if [ -n "$succeedOnFailure" ]; then
            echo "build failed with exit code $exitCode (ignored)"
            mkdir -p "$out/nix-support"
            echo -n $exitCode > "$out/nix-support/failed"
            exit 0
        fi

    else
        runHook exitHook
    fi

    exit $exitCode
}

trap "exitHandler" EXIT


nestingLevel=0

startNest() {
    nestingLevel=$(($nestingLevel + 1))
    echo -en "\033[$1p"
}

stopNest() {
    nestingLevel=$(($nestingLevel - 1))
    echo -en "\033[q"
}

header() {
    startNest "$2"
    echo "$1"
}

# Make sure that even when we exit abnormally, the original nesting
# level is properly restored.
closeNest() {
    while [ $nestingLevel -gt 0 ]; do
        stopNest
    done
}


######################################################################
# Helper functions that might be useful in setup hooks.


addToSearchPathWithCustomDelimiter() {
    local delimiter=$1
    local varName=$2
    local dir=$3
    if [ -d "$dir" ]; then
        eval export ${varName}=${!varName}${!varName:+$delimiter}${dir}
    fi
}

PATH_DELIMITER=':'

addToSearchPath() {
    addToSearchPathWithCustomDelimiter "${PATH_DELIMITER}" "$@"
}


######################################################################
# Initialisation.

set -e

# Wildcard expansions that don't match should expand to an empty list.
# This ensures that, for instance, "for i in *; do ...; done" does the
# right thing.
shopt -s nullglob


# Set up the initial path.
PATH=
for i in @initialPath@; do
    if [ "$i" = / ]; then i=; fi
    addToSearchPath PATH $i/bin
    addToSearchPath PATH $i/sbin
done

if [ "$NIX_DEBUG" = 1 ]; then
    echo "initial path: $PATH"
fi


# Allow the caller to augment buildInputs (it's not always possible to
# do this before the call to setup.sh, since the PATH is empty at that
# point; here we have a basic Unix environment).
runHook addInputsHook


# Recursively find all build inputs.
findInputs() {
    local pkg=$1
    local var=$2
    local propagatedBuildInputsFile=$3

    case ${!var} in
        *\ $pkg\ *)
            return 0
            ;;
    esac

    eval $var="'${!var} $pkg '"

    if [ -f $pkg/nix-support/setup-hook ]; then
        source $pkg/nix-support/setup-hook
    fi

    if [ -f $pkg/nix-support/$propagatedBuildInputsFile ]; then
        for i in $(cat $pkg/nix-support/$propagatedBuildInputsFile); do
            findInputs $i $var $propagatedBuildInputsFile
        done
    fi
}

crossPkgs=""
for i in $buildInputs $propagatedBuildInputs; do
    findInputs $i crossPkgs propagated-build-inputs
done

nativePkgs=""
for i in $nativeBuildInputs $propagatedNativeBuildInputs; do
    findInputs $i nativePkgs propagated-native-build-inputs
done

PATH=$_PATH${_PATH:+:}$PATH
if [ "$NIX_DEBUG" = 1 ]; then
    echo "final path: $PATH"
fi


# Set the relevant environment variables to point to the build inputs
# found above.
addToNativeEnv() {
    local pkg=$1

    if [ -d $1/bin ]; then
        addToSearchPath _PATH $1/bin
    fi

    # Run the package-specific hooks set by the setup-hook scripts.
    for i in "${envHooks[@]}"; do
        $i $pkg
    done
}

for i in $nativePkgs; do
    addToNativeEnv $i
done

addToCrossEnv() {
    local pkg=$1

    # Some programs put important build scripts (freetype-config and similar)
    # into their crossDrv bin path. Intentionally these should go after
    # the nativePkgs in PATH.
    if [ -d $1/bin ]; then
        addToSearchPath _PATH $1/bin
    fi

    # Run the package-specific hooks set by the setup-hook scripts.
    for i in "${crossEnvHooks[@]}"; do
        $i $pkg
    done
}

for i in $crossPkgs; do
    addToCrossEnv $i
done


# Set the prefix.  This is generally $out, but it can be overriden,
# for instance if we just want to perform a test build/install to a
# temporary location and write a build report to $out.
if [ -z "$prefix" ]; then
    prefix="$out";
fi


PATH=$_PATH${_PATH:+:}$PATH
if [ "$NIX_DEBUG" = 1 ]; then
    echo "final path: $PATH"
fi


# Utility function: return the base name of the given path, with the
# prefix `HASH-' removed, if present.
stripHash() {
    strippedName=$(basename $1);
    if echo "$strippedName" | grep -q '^[a-z0-9]\{32\}-'; then
        strippedName=$(echo "$strippedName" | cut -c34-)
    fi
}


unpackFile() {
    curSrc="$1"
    local cmd

    header "unpacking source archive $curSrc" 3

    case "$curSrc" in
        *.tar.xz | *.tar.lzma)
            # Don't rely on tar knowing about .xz.
            xz -d < $curSrc | tar xf - $tarFlags
            ;;
        *.tar | *.tar.* | *.tgz | *.tbz2)
            # GNU tar can automatically select the decompression method
            # (info "(tar) gzip").
            tar -xf $curSrc $tarFlags
            ;;
        *.zip)
            unzip -qq $curSrc
            ;;
        *)
            if [ -d "$curSrc" ]; then
                stripHash $curSrc
                cp -prd --no-preserve=timestamps $curSrc $strippedName
            else
                if [ -z "$unpackCmd" ]; then
                    echo "source archive $curSrc has unknown type"
                    exit 1
                fi
                runHook unpackCmd
            fi
            ;;
    esac

    stopNest
}


unpackPhase() {
    #mkdir -p $out/$name ; cd $out/$name

    runHook preUnpack

    if [ -z "$srcs" ]; then
        if [ -z "$src" ]; then
            echo 'variable $src or $srcs should point to the source'
            exit 1
        fi
        srcs="$src"
    fi

    # To determine the source directory created by unpacking the
    # source archives, we record the contents of the current
    # directory, then look below which directory got added.  Yeah,
    # it's rather hacky.
    local dirsBefore=""
    for i in *; do
        if [ -d "$i" ]; then
            dirsBefore="$dirsBefore $i "
        fi
    done

    # Unpack all source archives.
    for a in $srcs; do
        unpackFile $a
    done

    runHook postUnpack
}

unpackPhase() {
    runHook preUnpack

    if [ -z "$srcs" ]; then
        if [ -z "$src" ]; then
            echo 'variable $src or $srcs should point to the source'
            exit 1
        fi
        srcs="$src"
    fi

    # To determine the source directory created by unpacking the
    # source archives, we record the contents of the current
    # directory, then look below which directory got added.  Yeah,
    # it's rather hacky.
    local dirsBefore=""
    for i in *; do
        if [ -d "$i" ]; then
            dirsBefore="$dirsBefore $i "
        fi
    done

    # Unpack all source archives.
    for i in $srcs; do
        unpackFile $i
    done

    # By default, add write permission to the sources.  This is often
    # necessary when sources have been copied from other store
    # locations.
    if [ "$dontMakeSourcesWritable" != 1 ]; then
        chmod -R u+w ./
    fi

    runHook postUnpack
}


patchPhase() {
    runHook prePatch

    for i in $patches; do
        header "applying patch $i" 3
        local uncompress=cat
        case $i in
            *.gz)
                uncompress="gzip -d"
                ;;
            *.bz2)
                uncompress="bzip2 -d"
                ;;
            *.xz)
                uncompress="xz -d"
                ;;
            *.lzma)
                uncompress="lzma -d"
                ;;
        esac
        # "2>&1" is a hack to make patch fail if the decompressor fails (nonexistent patch, etc.)
        $uncompress < $i 2>&1 | patch ${patchFlags:--p1}
        stopNest
    done

    runHook postPatch
}


installPhase() {
    runHook preInstall

    mkdir -p "$prefix"
    cp -r ./* "$prefix"

    runHook postInstall
}


genericBuild() {
    header "preparing $out"

    if [ -n "$buildCommand" ]; then
        eval "$buildCommand"
        return
    fi

    if [ -z "$phases" ]; then
        phases="unpackPhase patchPhase installPhase"
    fi

    for curPhase in $phases; do
        # Evaluate the variable named $curPhase if it exists, otherwise the
        # function named $curPhase.
        eval "${!curPhase:-$curPhase}"

        if [ "$curPhase" = unpackPhase ]; then
            cd "${sourceRoot:-.}"
        fi

        stopNest
    done

    stopNest
}
