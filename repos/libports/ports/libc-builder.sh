source $preparePort/setup

preUnpack() {
    mkdir -p src/lib/libc
    cd src/lib/libc
}

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
# Link into dest the following arguments,
# rooted at $out
#
linkIncludes() {
    local dest=$1
    mkdir -p $dest
    for (( i = 2; i <= $#; i++)); do
        ln -s $out/${!i} $dest
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
    do content="$content src/lib/libc/sys/$1/include/$i"; done

    for i in arith.h _fpmath.h SYS.h gd_qnan.h
    do content="$content src/lib/libc/lib/libc/$1/$i"; done

    echo -n $content
}

common_include_libc_arch_machine_content() {
    for i in \
        _types.h endian.h _limits.h signal.h trap.h _stdint.h \
        sysarch.h ieeefp.h frame.h sigframe.h vm.h \
        cpufunc.h vmparam.h atomic.h elf.h exec.h reloc.h pmap.h \
        ucontext.h setjmp.h asm.h param.h _inttypes.h
    do echo src/lib/libc/sys/$1/include/$i; done
}


postPatch() {
    #
    # Generic headers
    #

    linkIncludes \
        include/libc \
        \
        $(addPrefix src/lib/libc/include/ \
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
        complex.h) \
        \
        $(addPrefix src/lib/libc/sys/sys/ \
        syslog.h fcntl.h stdint.h sched.h ktrace.h termios.h \
        semaphore.h _semaphore.h) \
        \
        src/lib/libc/sys/sys/errno.h \
        src/lib/libc/lib/msun/src/math.h

    linkIncludes \
        include/libc/rpc \
        \
        $(addPrefix src/lib/libc/include/rpc/ \
        rpc.h xdr.h auth.h clnt_stat.h clnt.h clnt_soc.h rpc_msg.h \
        auth_unix.h auth_des.h svc.h svc_soc.h svc_auth.h pmap_clnt.h \
        pmap_prot.h rpcb_clnt.h rpcent.h des_crypt.h des.h nettype.h \
        rpcsec_gss.h raw.h rpc_com.h) \
        \
        src/lib/libc/include/rpc/rpcb_prot.h

    linkIncludes \
        include/libc/rpcsvc \
        \
        $(addPrefix src/lib/libc/include/rpcsvc/ \
        yp_prot.h nis.h ypclnt.h nis_tags.h nislib.h crypt.h)

    linkIncludes \
        include/libc/gssapi \
        src/lib/libc/include/gssapi/gssapi.h

    linkIncludes \
        include/libc/arpa \
        $(addPrefix src/lib/libc/include/arpa/ \
        inet.h ftp.h nameser.h nameser_compat.h telnet.h tftp.h)

    linkIncludes \
        include/libc/vm \
        $(addPrefix src/lib/libc/sys/vm/ \
        vm_param.h vm.h pmap.h )

    linkIncludes \
        include/libc/net \
        $(addPrefix src/lib/libc/sys/net/ \
        if.h if_dl.h if_tun.h if_types.h radix.h route.h)

    linkIncludes \
        include/libc/netinet \
        $(addPrefix src/lib/libc/sys/netinet/ \
        in.h in_systm.h ip.h tcp.h)

    linkIncludes \
        include/libc/netinet6 \
        src/lib/libc/sys/netinet6/in6.h

    linkIncludes \
        include/libc/bsm \
        src/lib/libc/sys/bsm/audit.h

    linkIncludes \
        include/libc/sys/rpc \
        src/lib/libc/sys/rpc/types.h

    linkIncludes \
        include/libc/sys \
        $(addPrefix src/lib/libc/sys/sys/ \
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
        utsname.h elf.h mtio.h)

    #
    # i386-specific headers
    #
    linkIncludes \
        include/libc-i386 \
        $(common_include_libc_arch_content i386) \
        src/lib/libc/lib/msun/i387/fenv.h

    linkIncludes \
        include/libc-i386/machine \
        $(common_include_libc_arch_machine_content i386) \
        $(addPrefix src/lib/libc/sys/i386/include/ \
        specialreg.h npx.h)

    #
    # AMD64-specific headers
    #
    linkIncludes \
        include/libc-amd64 \
        $(common_include_libc_arch_content amd64) \
        src/lib/libc/lib/msun/amd64/fenv.h

    linkIncludes \
        include/libc-amd64/machine \
        $(common_include_libc_arch_machine_content amd64) \
        $(addPrefix src/lib/libc/sys/amd64/include/ \
        specialreg.h fpu.h)

    #
    # ARM-specific headers
    #
    linkIncludes \
        include/libc-arm \
        $(common_include_libc_arch_content arm) \
        src/lib/libc/lib/msun/arm/fenv.h

    linkIncludes \
        include/libc-arm/machine \
        $(common_include_libc_arch_machine_content arm) \
        $(addPrefix src/lib/libc/sys/arm/include/ \
        pte.h cpuconf.h armreg.h ieee.h)


    ##############################################################

    flex -P_nsyy -t src/lib/libc/lib/libc/net/nslexer.l \
        | sed -e '/YY_BUF_SIZE/s/16384/1024/' \
        > src/lib/libc/lib/libc/net/nslexer.c

    bison -d -p_nsyy src/lib/libc/lib/libc/net/nsparser.y \
        --defines=src/lib/libc/lib/libc/net/nsparser.h \
        --output=src/lib/libc/lib/libc/net/nsparser.c

    local generated_files="src/lib/libc/include/rpc/rpcb_prot.h"
    for h in \
        bootparam_prot.h nfs_prot.h nlm_prot.h rstat.h ypupdate_prot.h \
        crypt.h nis_cache.h pmap_prot.h rwall.h yp.h \
        key_prot.h nis_callback.h rex.h sm_inter.h ypxfrd.h \
        klm_prot.h nis_object.h rnusers.h spray.h \
        mount.h nis.h rquota.h yppasswd.h
    do generated_files="$generated_files src/lib/libc/include/rpcsvc/$h"; done

    for file in $generated_files; do
        rpcgen -C -h -DWANT_NFS3 ${file%h}x -o $file
    done
}


genericBuild
