void Settings::processEnvironment()
{
    nixStore = NIX_STORE_DIR;
    nixDataDir = NIX_DATA_DIR;
    nixLogDir = NIX_LOG_DIR;
    nixStateDir = NIX_STATE_DIR;
    nixDBPath = NIX_DB_DIR;
    nixConfDir = NIX_CONF_DIR;
    #nixLibexecDir = NIX_LIBEXEC_DIR;
    #nixBinDir = canonPath(getEnv("NIX_BIN_DIR", NIX_BIN_DIR));
    #nixDaemonSocketFile = canonPath(nixStateDir + DEFAULT_SOCKET_PATH);
}