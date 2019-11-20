addGenodePkgConfigPath () {
    if test -e "$1/.genode" && test -d "$1/lib/pkgconfig"; then
        export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$1/lib/pkgconfig"
    fi
}

addEnvHooks "$targetOffset" addGenodePkgConfigPath
