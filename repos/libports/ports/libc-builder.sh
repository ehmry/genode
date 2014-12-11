source $preparePort/setup

postUnpack() {
    local repos_=($repos)
    local dirs_=($dirs)

    for ((i = 0; i < ${#repos_[@]}; i++)); do
        repo="${repos_[$i]}"
        dir="${dirs_[$i]}"
        if [ "$repo" != "$dir" ] ; then
            mkdir -p $(dirname "$dir")
            mv $repo $dir
        fi
    done

    cd $NIX_BUILD_TOP
}


#
# Add a prefix to the following arguments
#
addPrefix() {
    local prefix=$1
    local files=""

    for (( i = 2; i <= $#; i++)); do
        files="$files $prefix${!i}"
    done
    echo -n $files
}

#
# Copy into dest the following arguments,
# rooted at $include
#
copyIncludes() {
    local to=$1; shift
    local from=$1; shift

    mkdir -p $to
    for i in $*; do
        cp $from/$i $to/
    done
}


#
# CPU-architecture-specific headers
#
# The 'common_include_*_content' functions take the CPU architecture as first
# argument.
#

common_include_libc_arch_content() {
    local content
    for i in stdarg.h float.h
    do content="$content sys/$1/include/$i"; done

    for i in arith.h _fpmath.h SYS.h gd_qnan.h
    do content="$content lib/libc/$1/$i"; done

    echo -n $content
}

common_include_libc_arch_machine_content() {
    for i in \
        _types.h endian.h _limits.h signal.h trap.h _stdint.h \
        sysarch.h ieeefp.h frame.h sigframe.h vm.h \
        cpufunc.h vmparam.h atomic.h elf.h exec.h reloc.h pmap.h \
        ucontext.h setjmp.h asm.h param.h _inttypes.h
    do echo sys/$1/include/$i; done
}

rpcgen_() {
    rpcgen -C -h -DWANT_NFS3 $1 -o $2
}


postPatch() {
    mkdir $include

    #
    # CPU-architecture-specific headers
    #

    #
    # i386-specific headers
    #
    mkdir -p $include/libc-i386/machine
    cp \
        $(common_include_libc_arch_content i386) \
        lib/msun/i387/fenv.h \
        $include/libc-i386

    cp \
        $(common_include_libc_arch_machine_content i386) \
        sys/i386/include/specialreg.h \
        sys/i386/include/npx.h \
        $include/libc-i386/machine

    #
    # AMD64-specific headers
    #
    mkdir -p $include/libc-amd64/machine
    cp \
        $(common_include_libc_arch_content amd64) \
        lib/msun/amd64/fenv.h \
        $include/libc-amd64

    cp \
        $(common_include_libc_arch_machine_content amd64) \
        sys/amd64/include/specialreg.h \
        sys/amd64/include/fpu.h \
        $include/libc-amd64/machine

    #
    # ARM-specific headers
    #
    mkdir -p $include/libc-arm/machine
    cp  \
        $(common_include_libc_arch_content arm) \
        lib/msun/arm/fenv.h \
        $include/libc-arm

    cp \
        $(common_include_libc_arch_machine_content arm) \
        $include/libc-arm/machine

    copyIncludes $include/libc-arm/machine sys/arm/include/ \
        pte.h cpuconf.h armreg.h ieee.h


    ##############################################################

    flex -P_nsyy -t lib/libc/net/nslexer.l \
        | sed -e '/YY_BUF_SIZE/s/16384/1024/' \
        > lib/libc/net/nslexer.c

    bison -d -p_nsyy lib/libc/net/nsparser.y \
        --defines=lib/libc/net/nsparser.h \
        --output=lib/libc/net/nsparser.c

    local generated_files="include/rpc/rpcb_prot.h"
    for h in \
        bootparam_prot.h nfs_prot.h nlm_prot.h rstat.h ypupdate_prot.h \
        crypt.h nis_cache.h pmap_prot.h rwall.h yp.h \
        key_prot.h nis_callback.h rex.h sm_inter.h ypxfrd.h \
        klm_prot.h nis_object.h rnusers.h spray.h \
        mount.h nis.h rquota.h yppasswd.h
    do generated_files="$generated_files include/rpcsvc/$h"; done

    for file in $generated_files; do
        rpcgen -C -h -DWANT_NFS3 ${file%h}x -o $file
    done


    include=$include/libc
    #
    # Generic headers
    #
    copyIncludes $include include \
        strings.h limits.h string.h ctype.h _ctype.h runetype.h \
        stdlib.h stdio.h signal.h unistd.h wchar.h time.h sysexits.h \
        resolv.h wctype.h locale.h langinfo.h regex.h paths.h ieeefp.h \
        inttypes.h fstab.h netdb.h ar.h memory.h res_update.h \
        netconfig.h ifaddrs.h pthread.h err.h getopt.h search.h \
        varargs.h stddef.h stdbool.h assert.h monetary.h printf.h vis.h \
        libgen.h dirent.h dlfcn.h link.h fmtmsg.h fnmatch.h fts.h ftw.h \
        db.h grp.h nsswitch.h pthread_np.h pwd.h utmp.h ttyent.h \
        stringlist.h glob.h a.out.h elf-hints.h nlist.h spawn.h \
        readpassphrase.h setjmp.h elf.h ulimit.h utime.h wordexp.h \
        complex.h


    copyIncludes $include sys/sys \
        syslog.h fcntl.h stdint.h sched.h ktrace.h termios.h \
        semaphore.h _semaphore.h errno.h

    cp lib/msun/src/math.h $include

    copyIncludes $include/rpc include/rpc \
        rpc.h xdr.h auth.h clnt_stat.h clnt.h clnt_soc.h rpc_msg.h \
        auth_unix.h auth_des.h svc.h svc_soc.h svc_auth.h pmap_clnt.h \
        pmap_prot.h rpcb_clnt.h rpcent.h des_crypt.h des.h nettype.h \
        rpcsec_gss.h raw.h rpc_com.h

    cp sys/rpc/rpcb_prot.h $include/rpc

    copyIncludes $include/rpcsvc include/rpcsvc \
        yp_prot.h ypclnt.h nis_tags.h nislib.h

    rpcgen_ include/rpcsvc/nis.x $include/rpcsvc/nis.h
    rpcgen_ include/rpcsvc/crypt.x $include/rpcsvc/crypt.h

    mkdir $include/gssapi
    cp include/gssapi/gssapi.h $include/gssapi

    copyIncludes $include/arpa include/arpa \
        inet.h ftp.h nameser.h nameser_compat.h telnet.h tftp.h

    copyIncludes $include/vm sys/vm vm_param.h vm.h pmap.h

    copyIncludes $include/net sys/net \
        if.h if_dl.h if_tun.h if_types.h radix.h route.h

    copyIncludes $include/netinet sys/netinet \
        in.h in_systm.h ip.h tcp.h

    mkdir -p $include/netinet6
    cp sys/netinet6/in6.h $include/netinet6

    mkdir -p $include/bsm
    cp sys/bsm/audit.h $include/bsm

    copyIncludes $include/sys sys/sys \
        _types.h limits.h cdefs.h _null.h types.h _pthreadtypes.h \
        syslimits.h select.h _sigset.h _timeval.h timespec.h \
        _timespec.h stat.h signal.h unistd.h time.h param.h stdint.h \
        event.h eventhandler.h disk.h errno.h poll.h queue.h mman.h \
        stddef.h sysctl.h uio.h _iovec.h ktrace.h ioctl.h ttycom.h \
        ioccom.h filio.h sockio.h wait.h file.h fcntl.h resource.h \
        disklabel.h link_elf.h endian.h mount.h ucred.h dirent.h \
        cpuset.h socket.h un.h ttydefaults.h imgact_aout.h elf32.h \
        elf64.h elf_generic.h elf_common.h nlist_aout.h ipc.h sem.h \
        exec.h _lock.h _mutex.h statvfs.h ucontext.h syslog.h times.h \
        utsname.h elf.h mtio.h

    mkdir $include/sys/rpc
    cp sys/rpc/types.h $include/sys/rpc

    rm -r include # Don't need this anymore
}


genericBuild
