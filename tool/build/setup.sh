# Run the named hook, either by calling the function with that name or
# by evaluating the variable with that name.  This allows convenient
# setting of hooks both from Nix expressions (as attributes /
# environment variables) and from shell scripts (as functions).
runHook() {
    local hookName="$1"
    if [ "${!hookName}" ]; then
        eval "${!hookName}"
        return 
    fi

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


# Execute the pre-hook.
export SHELL=@shell@
if [ -z "$shell" ]; then export shell=@shell@; fi
runHook preHook


# Check that the pre-hook initialised SHELL.
if [ -z "$SHELL" ]; then echo "SHELL not set"; exit 1; fi

# Hack: run gcc's setup hook.
envHooks=()
crossEnvHooks=()
if [ -f @toolchain@/nix-support/setup-hook ]; then
    source @toolchain@/nix-support/setup-hook
fi


# Ensure that the given directories exists.
ensureDir() {
    local dir
    for dir in "$@"; do
        if ! [ -x "$dir" ]; then mkdir -p "$dir"; fi
    done
}



# Add the output as an rpath.
if [ "$NIX_NO_SELF_RPATH" != 1 ]; then
    export NIX_LDFLAGS="-rpath $out/lib $NIX_LDFLAGS"
    if [ -n "$NIX_LIB64_IN_SELF_RPATH" ]; then
        export NIX_LDFLAGS="-rpath $out/lib64 $NIX_LDFLAGS"
    fi
    if [ -n "$NIX_LIB32_IN_SELF_RPATH" ]; then
        export NIX_LDFLAGS="-rpath $out/lib32 $NIX_LDFLAGS"
    fi
fi


# Set the TZ (timezone) environment variable, otherwise commands like
# `date' will complain (e.g., `Tue Mar 9 10:01:47 Local time zone must
# be set--see zic manual page 2004').
export TZ=UTC


PATH=$_PATH${_PATH:+:}$PATH
if [ "$NIX_DEBUG" = 1 ]; then
    echo "final path: $PATH"
fi


# Make GNU Make produce nested output.
export NIX_INDENT_MAKE=1


# Normalize the NIX_BUILD_CORES variable. The value might be 0, which
# means that we're supposed to try and auto-detect the number of
# available CPU cores at run-time.

if [ -z "${NIX_BUILD_CORES:-}" ]; then
  NIX_BUILD_CORES="1"
elif [ "$NIX_BUILD_CORES" -le 0 ]; then
  NIX_BUILD_CORES=$(nproc 2>/dev/null || true)
  if expr >/dev/null 2>&1 "$NIX_BUILD_CORES" : "^[0-9][0-9]*$"; then
    :
  else
    NIX_BUILD_CORES="1"
  fi
fi
export NIX_BUILD_CORES


######################################################################
# Misc. helper functions.


stripDirs() {
    local dirs="$1"
    local stripFlags="$2"
    local dirsNew=

    for d in ${dirs}; do
        if [ -d "$prefix/$d" ]; then
            dirsNew="${dirsNew} $prefix/$d "
        fi
    done
    dirs=${dirsNew}

    if [ -n "${dirs}" ]; then
        header "stripping (with flags $stripFlags) in $dirs"
        find $dirs -type f -print0 | xargs -0 ${xargsFlags:--r} strip $commonStripFlags $stripFlags || true
        stopNest
    fi
}

######################################################################
# Textual substitution functions.


substitute() {
    local input="$1"
    local output="$2"

    local -a params=("$@")

    local n p pattern replacement varName content

    # a slightly hacky way to keep newline at the end
    content="$(cat $input; echo -n X)"
    content="${content%X}"

    for ((n = 2; n < ${#params[*]}; n += 1)); do
        p=${params[$n]}

        if [ "$p" = --replace ]; then
            pattern="${params[$((n + 1))]}"
            replacement="${params[$((n + 2))]}"
            n=$((n + 2))
        fi

        if [ "$p" = --subst-var ]; then
            varName="${params[$((n + 1))]}"
            pattern="@$varName@"
            replacement="${!varName}"
            n=$((n + 1))
        fi

        if [ "$p" = --subst-var-by ]; then
            pattern="@${params[$((n + 1))]}@"
            replacement="${params[$((n + 2))]}"
            n=$((n + 2))
        fi

        content="${content//"$pattern"/$replacement}"
    done

    # !!! This doesn't work properly if $content is "-n".
    echo -n "$content" > "$output".tmp
    if [ -x "$output" ]; then chmod +x "$output".tmp; fi
    mv -f "$output".tmp "$output"
}


substituteInPlace() {
    local fileName="$1"
    shift
    substitute "$fileName" "$fileName" "$@"
}


substituteAll() {
    local input="$1"
    local output="$2"

    # Select all environment variables that start with a lowercase character.
    for envVar in $(env | sed "s/^[^a-z].*//" | sed "s/^\([^=]*\)=.*/\1/"); do
        if [ "$NIX_DEBUG" = "1" ]; then
            echo "$envVar -> ${!envVar}"
        fi
        args="$args --subst-var $envVar"
    done

    substitute "$input" "$output" $args
}


substituteAllInPlace() {
    local fileName="$1"
    shift
    substituteAll "$fileName" "$fileName" "$@"
}


######################################################################
# What follows is the generic builder.


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


# This function is useful for debugging broken Nix builds.  It dumps
# all environment variables to a file `env-vars' in the build
# directory.  If the build fails and the `-K' option is used, you can
# then go to the build directory and source in `env-vars' to reproduce
# the environment used for building.
dumpVars() {
    if [ "$noDumpEnvVars" != 1 ]; then
        export > "$NIX_BUILD_TOP/env-vars"
    fi
}


# Utility function: return the base name of the given path, with the
# prefix `HASH-' removed, if present.
stripHash() {
    strippedName=$(basename $1);
    if echo "$strippedName" | grep -q '^[a-z0-9]\{32\}-'; then
        strippedName=$(echo "$strippedName" | cut -c34-)
    fi
    echo $strippedName
}

compile() {
    local main=$1

    echo -e "    COMPILE  $@"

    local base="${main%.*}"
    local object="${base}.o"
    base="${base//./_}"

    local file_opt_var="ccOpt_$(basename $base)"
    local ccFlags="$ccFlags ${!file_opt_var}"

    test "$prefix" && cd $prefix

    case $main in
        *.c | *.s )
            VERBOSE $cc $ccFlags $includeOpts \
                -c $main -o $object
            ;;

        *.cc | *.cpp )
            VERBOSE $cxx $ccFlags $cxxFlags \
                $includeOpts -c $main -o $object
            ;;

        *.S )
            VERBOSE $cc $ccFlags -D__ASSEMBLY__ \
                $includeOpts -c $main -o $object
            ;;
        * )
            echo Unhandled source file $main
            exit 1
            ;;
    esac
    objects="$objects $object"
}

findLibs() {
    local pkg=$1
    local var=$2

    lib=$(echo $pkg/*.a; echo $pkg/*.so)
    case ${!var} in
        *\ $lib\ *)
            return 0
            ;;
    esac

    eval $var="'${!var} $lib '"

    if [ -f $pkg/nix-support/propagated-libraries ]; then
        for i in $(cat $pkg/nix-support/propagated-libraries); do
            findLibs $i $var
        done
    fi
}

gatherSource() {
    srcSrc=$1
    srcDst=$2
    
    bn=$(basename $srcDst)
    var="local_includes_${bn/./_}"
    localIncludes=(${!var})

    #localIncludes=($localIncludes)

    # Determine how many `..' levels appear in the header file
    # references. E.g., if there is some reference 
    # `../../foo.h', then we have to insert two extra levels
    # in the directory structure, so that `a.c' is stored at
    # `dotdot/dotdot/a.c', and a reference from it to
    # `../../foo.h' resolves to
    # `dotdot/dotdot/../../foo.h' == `foo.h'.
    local n=0
    maxDepth=0
    for ((n = 0; n < ${#localIncludes[*]}; n += 2)); do
        target=${localIncludes[$((n + 1))]}

        # Split the target name into path components using some
        # IFS magic.
        savedIFS="$IFS"
        IFS=/
        components=($target)
        depth=0
        for ((m = 0; m < ${#components[*]}; m++)); do
            c=${components[m]}
            if test "$c" = ".."; then
                depth=$((depth + 1))
            fi
        done
        IFS="$savedIFS"

        if test $depth -gt $maxDepth; then
            maxDepth=$depth;
        fi
    done

    # Create the extra levels in the directory hierarchy.
    local prefix=
    for ((n = 0; n < maxDepth; n++)); do
        prefix="dotdot/$prefix"
    done

    local dir="$(dirname $srcDst)/"

    # Create symlinks to the header files.
    for ((n = 0; n < ${#localIncludes[*]}; n += 2)); do
        incSrc=${localIncludes[n]}
        incDst=${localIncludes[$((n + 1))]}
        
        # Create missing directories.  We use IFS magic to split the path
        # into path components.
        savedIFS="$IFS"
        IFS=/
        components=($prefix$dir$incDst)
        fullPath=(.)
        for ((m = 0; m < ${#components[*]} - 1; m++)); do
            fullPath=("${fullPath[@]}" ${components[m]})
            if ! test -d "${fullPath[*]}"; then
                mkdir "${fullPath[*]}"
            fi
        done
        IFS="$savedIFS"
    
        ln -sf $incSrc $prefix$dir$incDst
    done

    # Create a symlink to the source file.
    # Create missing directories.  We use IFS magic to split the path
    # into path components.
    savedIFS="$IFS"
    IFS=/
    components=($prefix$srcDst)
    fullPath=(.)
    for ((m = 0; m < ${#components[*]} - 1; m++)); do
        fullPath=("${fullPath[@]}" ${components[m]})
        if ! test -d "${fullPath[*]}"; then
            mkdir "${fullPath[*]}"
        fi
    done
    IFS="$savedIFS"
    
    if ! test "$(readlink $prefix$srcDst)" = $srcSrc; then
        ln -s $srcSrc $prefix$srcDst
    fi
}

# turn into an array
sources=($sources)

gatherPhase() {
    runHook preGather

    local n
    for ((n = 0; n < ${#sources[*]}; n += 2)); do
        gatherSource "${sources[n]}" "${sources[$((n + 1))]}"
    done

    runHook postGather
}

ccFlags="$ccDef $ccOpt $ccOLevel $ccWarn"
cxxFlags="$ccCxxOpt"

compilePhase() {
    runHook preCompile
    
    includeOpts="-I."
    for i in $systemIncludes $nativeIncludePaths
    do includeOpts="$includeOpts -I$i"
    done

    for ((n = 1; n < ${#sources[*]}; n += 2))
    do compile ${sources[$n]}
    done

    runHook postCompile
}

genericBuild() {
    if [ -n "$buildCommand" ]; then
        eval "$buildCommand"
        return
    fi

    if [ -z "$phases" ]; then
        phases="gatherPhase compilePhase mergePhase fixupPhase"
    fi

    for curPhase in $phases; do
        if [ -n "$tracePhases" ]; then
            echo
            echo "@ phase-started $out $curPhase"
        fi

        dumpVars

        # Evaluate the variable named $curPhase if it exists, otherwise the
        # function named $curPhase.
        eval "${!curPhase:-$curPhase}"

        if [ -n "$tracePhases" ]; then
            echo
            echo "@ phase-succeeded $out $curPhase"
        fi

        stopNest
    done

    stopNest
}

BRIGHT_COL="\033[01;33m"
DARK_COL="\033[00;33m"
DEFAULT_COL="\033[0m"

MSG_LINK() {
    echo -e "    LINK     $@"
}
MSG_COMP() {
    echo -e "    COMPILE  $@"
}
MSG_BUILD() {
    echo -e "    BUILD    $@"
}
MSG_MERGE() {
    echo -e "    MERGE    $@"
}
MSG_CONVERT() {
    echo -e "    CONVERT  $@"
}
MSG_CONFIG() {
    echo -e "    CONFIG   $@"
}
MSG_CLEAN() {
    echo -e "  CLEAN $@"
}
MSG_ASSEM() {
    echo -e "    ASSEMBLE $@"
}
MSG_INST() {
    echo -e "    INSTALL  $@"
}
MSG_PRG() {
    echo -e "${BRIGHT_COL}  Program ${DEFAULT_COL}$@"
}
MSG_LIB() {
    echo -e "${DARK_COL}  Library ${DEFAULT_COL}$@"
}

VERBOSE() {
    if [ -n "$verbose" ]; then
        echo $@
    fi
    eval $@
}


dumpVars
