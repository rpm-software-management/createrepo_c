# bash completion for createrepo and friends

_cr_compress_type()
{
    COMPREPLY=( $( compgen -W "bz2 gz xz" -- "$2" ) )
}

_cr_createrepo()
{
    COMPREPLY=()

    case $3 in
        -V|--version|-h|--help|-u|--baseurl|--content|--repo|\
        -x|--excludes|--changelog-limit|\
        -q|--quiet|-v|--verbose|--skip-stat)
            return 0
            ;;
        --update-md-path|-o|--outputdir)
            COMPREPLY=( $( compgen -d -- "$2" ) )
            return 0
            ;;
        -g|--groupfile)
            COMPREPLY=( $( compgen -f -o plusdirs -X '!*.xml' -- "$2" ) )
            return 0
            ;;
        -s|--sumtype)
            COMPREPLY=( $( compgen -W 'md5 sha1 sha256' -- "$2" ) )
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
#        --num-deltas)
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
        COMPREPLY=( $( compgen -W '--version --help --quiet --verbose
            --excludes --baseurl --groupfile --checksum
            --no-database --update --update-md-path --database
            --skip-stat --pkglist --includepkg --outputdir
            --skip-symlinks --changelog-limit --unique-md-filenames
            --simple-md-filenames --workers --xz --compress-type' -- "$2" ) )
    else
        COMPREPLY=( $( compgen -d -- "$2" ) )
    fi
} &&
complete -F _cr_createrepo -o filenames createrepo_c

_cr_mergerepo()
{
    COMPREPLY=()

    case $3 in
        --version|-h|--help|-a|--archlist)
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

    COMPREPLY=( $( compgen -W '--version --help --repo --archlist --no-database
        --outputdir --nogroups --noupdateinfo --compress-type --verbose --method
        --all --noarch-repo --database' -- "$2" ) )
} &&
complete -F _cr_mergerepo -o filenames mergerepo_c

# Local variables:
# mode: shell-script
# sh-basic-offset: 4
# sh-indent-comment: t
# indent-tabs-mode: nil
# End:
# ex: ts=4 sw=4 et filetype=sh
