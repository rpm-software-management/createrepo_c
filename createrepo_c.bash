# bash completion for createrepo and friends

_cr_compress_type()
{
    COMPREPLY=( $( compgen -W "bz2 gz xz" -- "$2" ) )
}

_cr_checksum_type()
{
    COMPREPLY=( $( compgen -W "md5 sha sha1 sha224 sha256 sha384 sha512" -- "$2" ) )
}

_cr_createrepo()
{
    COMPREPLY=()

    case $3 in
        -V|--version|-h|--help)
            return 0
            ;;
        --update-md-path|-o|--outputdir|--oldpackagedirs)
            COMPREPLY=( $( compgen -d -- "$2" ) )
            return 0
            ;;
        -g|--groupfile)
            COMPREPLY=( $( compgen -f -o plusdirs -X '!*.xml' -- "$2" ) )
            return 0
            ;;
        -s|--checksum)
            _cr_checksum_type "$1" "$2"
            return 0
            ;;
        -i|--pkglist|--read-pkgs-list)
            COMPREPLY=( $( compgen -f -o plusdirs -- "$2" ) )
            return 0
            ;;
        -n|--includepkg)
            COMPREPLY=( $( compgen -f -o plusdirs -X '!*.rpm' -- "$2" ) )
            return 0
            ;;
#        --retain-old-md)
#            COMPREPLY=( $( compgen -W '0 1 2 3 4 5 6 7 8 9' -- "$2" ) )
#            return 0
#            ;;
#        --num-deltas|--max-delta-rpm-size)
#            COMPREPLY=( $( compgen -W '1 2 3 4 5 6 7 8 9' -- "$2" ) )
#            return 0
#            ;;
        --workers)
            local min=2 max=$( getconf _NPROCESSORS_ONLN 2>/dev/null )
            [[ -z $max || $max -lt $min ]] && max=$min
            COMPREPLY=( $( compgen -W "{1..$max}" -- "$2" ) )
            return 0
            ;;
        --compress-type)
            _cr_compress_type "$1" "$2"
            return 0
            ;;
    esac

    if [[ $2 == -* ]] ; then
        COMPREPLY=( $( compgen -W '--help --version --quiet --verbose
            --excludes --basedir --baseurl --groupfile --checksum
            --pretty --database --no-database --update --update-md-path
            --skip-stat --pkglist --includepkg --outputdir
            --skip-symlinks --changelog-limit --unique-md-filenames
            --simple-md-filenames --retain-old-md --distro --content --repo
            --revision --read-pkgs-list --workers --xz
            --compress-type --keep-all-metadata --compatibility
            --retain-old-md-by-age --cachedir --local-sqlite
            --cut-dirs --location-prefix
            --deltas --oldpackagedirs
            --num-deltas --max-delta-rpm-size --recycle-pkglist' -- "$2" ) )
    else
        COMPREPLY=( $( compgen -d -- "$2" ) )
    fi
} &&
complete -F _cr_createrepo -o filenames createrepo_c

_cr_mergerepo()
{
    COMPREPLY=()

    case $3 in
        --version|-h|--help)
            return 0
            ;;
        -g|--groupfile|--blocked)
            COMPREPLY=( $( compgen -f -o plusdirs -- "$2" ) )
            return 0
            ;;
        -r|--repo|-o|--outputdir|--noarch-repo)
            COMPREPLY=( $( compgen -d -- "$2" ) )
            return 0
            ;;
        --compress-type)
            _cr_compress_type "" "$2"
            return 0
            ;;
        --method)
            COMPREPLY=( $( compgen -W "repo ts nvr" -- "$2" ) )
            return 0
            ;;
    esac

    if [[ $2 == -* ]] ; then
        COMPREPLY=( $( compgen -W '--version --help --repo --archlist --database
            --no-database --verbose --outputdir --nogroups --noupdateinfo
            --compress-type --method --all --noarch-repo --unique-md-filenames
            --simple-md-filenames --omit-baseurl --koji --groupfile
            --blocked' -- "$2" ) )
    else
        COMPREPLY=( $( compgen -d -- "$2" ) )
    fi
} &&
complete -F _cr_mergerepo -o filenames mergerepo_c

_cr_modifyrepo()
{
    COMPREPLY=()

    case $3 in
        --version|-h|--help)
            return 0
            ;;
        -f|--batchfile)
            COMPREPLY=( $( compgen -f -o plusdirs -- "$2" ) )
            return 0
            ;;
        --compress-type)
            _cr_compress_type "" "$2"
            return 0
            ;;
        -s|--checksum)
            _cr_checksum_type "$1" "$2"
            return 0
            ;;
    esac

    if [[ $2 == -* ]] ; then
        COMPREPLY=( $( compgen -W '--version --help --mdtype --remove
            --compress --no-compress --compress-type --checksum
            --unique-md-filenames --simple-md-filenames
            --verbose --batchfile --new-name' -- "$2" ) )
    else
        COMPREPLY=( $( compgen -f -- "$2" ) )
    fi
} &&
complete -F _cr_modifyrepo -o filenames modifyrepo_c

_cr_sqliterepo()
{
    COMPREPLY=()

    case $3 in
        -h|--help|-V|--version)
            return 0
            ;;
        --compress-type)
            _cr_compress_type "" "$2"
            return 0
            ;;
        -s|--checksum)
            _cr_checksum_type "$1" "$2"
            return 0
            ;;
    esac

    if [[ $2 == -* ]] ; then
        COMPREPLY=( $( compgen -W '--help --version --quiet --verbose
            --force --keep-old --xz --compress-type --checksum
            --local-sqlite ' -- "$2" ) )
    else
        COMPREPLY=( $( compgen -f -- "$2" ) )
    fi
} &&
complete -F _cr_sqliterepo -o filenames sqliterepo_c

# Local variables:
# mode: shell-script
# sh-basic-offset: 4
# sh-indent-comment: t
# indent-tabs-mode: nil
# End:
# ex: ts=4 sw=4 et filetype=sh
