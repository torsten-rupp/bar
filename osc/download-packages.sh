#!/bin/sh

# ----------------------------------------------------------------------------
#
# $Source: sh.template $
# $Revision: 1.1 $
# $Author: $
# Contents:
# Systems: Unix
#
# ----------------------------------------------------------------------------

# --------------------------------- constants --------------------------------
# get program name
PROGRAM_NAME=`basename $0`

# shell commands/tools
AWK="awk"
BASENAME="basename"
BC="bc"
BZIP2="bzip2"
CAT="cat"
CD="cd"
CHMOD="chmod"
CMD="cmd.exe"
CP="cp"
CUT="cut"
CVS="cvs"
CWD="pwd"
DATE="date"
DIFF="diff"
DIRNAME="dirname"
DU="du"
ECHO="echo"
ECHO_NO_LF="echo"
EXPR="expr"
FILE="file"
FIND="find"
GREP="grep"
HEAD="head"
INSTALL_DIRECTORY="install -d"
INSTALL_FILE="install"
LN="ln"
LS="ls"
MAIL="mail"
MAKE="make"
MD5SUM="md5sum"
MKDIR="mkdir"
MKTEMP="mktemp"
MV="mv"
MYSQL="mysql"
PRINTF="printf --"
RMF="rm -f"
RMRF="rm -rf"
SED="sed"
SH="sh"
SORT="sort"
TAIL="tail"
TAR="tar"
TIMEOUT="timeout"
TOUCH="touch"
TR="tr"
WC="wc"
WGET="wget"
WHICH="which"
XARGS="xargs"

# exit codes
EXITCODE_OK=0
EXITCODE_FAILURE=1

EXITCODE_INVALID_ARGUMENT=5

EXITCODE_INTERNAL_ERROR=126

EXITCODE_UNKNOWN=127

# OSC
OSC_URL="http://download.opensuse.org"
OSC_PATH="repositories/home:/torsten20:/BAR"

# --------------------------- environment variables --------------------------

# --------------------------------- variables --------------------------------

# flags
listFlag=0
htmlFlag=0
simulateFlag=0
quietFlag=0
debugFlag=0

# ---------------------------------- functions -------------------------------

#***********************************************************************
# Name       : print, println
# Purpose    : print text to stdout
# Input      : text - text
# Return     : -
# Notes      : -
#***********************************************************************

print()
{
  local text="$@"

  if test $quietFlag -ne 1; then
    $ECHO -n "$text"
  fi
}
println()
{
  local text="$@"

  if test $quietFlag -ne 1; then
    $ECHO "$text"
  fi
}

#***********************************************************************
# Name       : printStderr
# Purpose    : print text on stderr (with LF)
# Input      : -
# Return     : -
# Notes      : -
#***********************************************************************

printStderr()
{
  local text="$@"

  $ECHO >&2 "$text"
}

#***********************************************************************
# Name       : printError
# Purpose    : print error text with prefix "ERROR:" (with LF)
# Input      : -
# Return     : -
# Notes      : -
#***********************************************************************

printError()
{
  local text="$@"

  $ECHO >&2 "ERROR: $text"
}

#***********************************************************************
# Name       : printWarning
# Purpose    : print warning text with prefix "warning:" (with LF)
# Input      : -
# Return     : -
# Notes      : -
#***********************************************************************

printWarning()
{
  local text="$@"

  $ECHO >&2 "warning: $text"
}

#***********************************************************************
# Name       : catStdout, catStderr
# Purpose    : output file on stdout, stderr
# Input      : fileName - file name
# Return     : -
# Notes      : -
#***********************************************************************

catStdout()
{
  local fileName="$1"; shift

  $CAT "$fileName"
}
catStderr()
{
  local fileName="$1"; shift

  $CAT 1>&2 "$fileName"
}

#***********************************************************************
# Name       : internalError
# Purpose    : print internal error text with prefix "INTERNAL ERROR:" and stop
# Input      : -
# Return     : -
# Notes      : -
#***********************************************************************

internalError()
{
  local text="$@"

  $ECHO >&2 "INTERNAL ERROR: $text"
  exit $EXITCODE_INTERNAL_ERROR
}

#***********************************************************************
# Name       : countLines
# Purpose    : count number of lines in file
# Input      : fileName - file name
# Return     : number of lines in file
# Notes      : -
#***********************************************************************

countLines()
{
  local fileName="$@"

  $WC -l $fileName | $AWK '{print $1 }'
}

#***********************************************************************
# Name       : catErrorlog
# Purpose    : output shortend error-log (either complete log if not
#              longer than MAX_ERROR_LINES or MAX_ERROR_LINES_HEAD
#              from the start and MAX_ERROR_LINES_TAIL from end of the
#              file)
# Input      : fileName - log file name
# Return     : -
# Notes      : -
#***********************************************************************

catErrorlog()
{
  local fileName="$1"; shift

  if test `countLines $fileName` -gt $MAX_ERROR_LINES; then
    $HEAD -$MAX_ERROR_LINES_HEAD $fileName
    $ECHO "[...]"
    $TAIL -$MAX_ERROR_LINES_TAIL $fileName
  else
    $TAIL -$MAX_ERROR_LINES $fileName
  fi
}

#***********************************************************************
# Name       : downloadPackage
# Purpose    : download package
# Input      : path                - path
#              pattern             - pattern
#              destinationFileName - destination file name
# Return     : -
# Notes      : -
#***********************************************************************

downloadPackage()
{
  local path="$1"; shift
  local pattern="$1"; shift
  local destinationFileName="$1"; shift

  fileName=`$WGET -q -O - "$path"|$GREP -E -e \"$pattern\"|$HEAD -1|$SED "s/.*\($pattern\).*/\1/g"`
#  $ECHO $fileName
  $WGET "$path/$fileName" -O $destinationFileName
  if test $? -ne 0; then
    exit 1
  fi
}

download()
{
  local version="$1"; shift

  downloadPackage "$OSC_URL/$OSC_PATH/CentOS_CentOS-6/i686"   'bar-.*\.rpm'     bar-$version-centos6_i686.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/CentOS_CentOS-6/x86_64" 'bar-.*\.rpm'     bar-$version-centos6_x86_64.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/CentOS_7/x86_64"        'bar-.*\.rpm'     bar-$version-centos7_x86_64.rpm

  downloadPackage "$OSC_URL/$OSC_PATH/Debian_7.0/i386"        'bar_.*\.deb'     bar-$version-debian7_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/Debian_7.0/i386"        'bar-gui_.*\.deb' bar-gui-$version-debian7_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/Debian_7.0/amd64"       'bar_.*\.deb'     bar-$version-debian7_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/Debian_7.0/amd64"       'bar-gui_.*\.deb' bar-gui-$version-debian7_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/Debian_8.0/i386"        'bar_.*\.deb'     bar-$version-debian8_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/Debian_8.0/i386"        'bar-gui_.*\.deb' bar-gui-$version-debian8_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/Debian_8.0/amd64"       'bar_.*\.deb'     bar-$version-debian8_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/Debian_8.0/amd64"       'bar-gui_.*\.deb' bar-gui-$version-debian8_amd64.deb

  downloadPackage "$OSC_URL/$OSC_PATH/Fedora_20/i686"         'bar-.*\.rpm'     bar-$version-fedora20_i686.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/Fedora_20/x86_64"       'bar-.*\.rpm'     bar-$version-fedora20_x86_64.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/Fedora_21/i686"         'bar-.*\.rpm'     bar-$version-fedora21_i686.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/Fedora_21/x86_64"       'bar-.*\.rpm'     bar-$version-fedora21_x86_64.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/Fedora_22/i686"         'bar-.*\.rpm'     bar-$version-fedora22_i686.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/Fedora_22/x86_64"       'bar-.*\.rpm'     bar-$version-fedora22_x86_64.rpm
//  downloadPackage "$OSC_URL/$OSC_PATH/Fedora_23/i686"         'bar-.*\.rpm'     bar-$version-fedora23_i686.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/Fedora_23/x86_64"       'bar-.*\.rpm'     bar-$version-fedora23_x86_64.rpm

  downloadPackage "$OSC_URL/$OSC_PATH/RedHat_RHEL-6/i686"     'bar-.*\.rpm'     bar-$version-redhat6_i686.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/RedHat_RHEL-6/x86_64"   'bar-.*\.rpm'     bar-$version-redhat6_x86_64.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/RHEL_7/x86_64"   'bar-.*\.rpm'     bar-$version-redhat7_x86_64.rpm

  downloadPackage "$OSC_URL/$OSC_PATH/SLE_11_SP4/i586"        'bar-.*\.rpm'     bar-$version-sle11_i586.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/SLE_11_SP4/x86_64"      'bar-.*\.rpm'     bar-$version-sle11_x86_64.rpm
//  downloadPackage "$OSC_URL/$OSC_PATH/SLE_12/i586"            'bar-.*\.rpm'     bar-$version-sle12_i586.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/SLE_12/x86_64"          'bar-.*\.rpm'     bar-$version-sle12_x86_64.rpm
//  downloadPackage "$OSC_URL/$OSC_PATH/SLE_12_SP1/i586"        'bar-.*\.rpm'     bar-$version-sle12_1_i586.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/SLE_12_SP1/x86_64"      'bar-.*\.rpm'     bar-$version-sle12_1_x86_64.rpm

  downloadPackage "$OSC_URL/$OSC_PATH/openSUSE_13.1/i586"     'bar-.*\.rpm'     bar-$version-opensuse13.1_i586.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/openSUSE_13.1/x86_64"   'bar-.*\.rpm'     bar-$version-opensuse13.1_x86_64.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/openSUSE_13.2/i586"     'bar-.*\.rpm'     bar-$version-opensuse13.2_i586.rpm
  downloadPackage "$OSC_URL/$OSC_PATH/openSUSE_13.2/x86_64"   'bar-.*\.rpm'     bar-$version-opensuse13.2_x86_64.rpm

  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_12.04/i386"     'bar_.*\.deb'     bar-$version-ubuntu12.04_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_12.04/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu12.04_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_12.04/amd64"    'bar_.*\.deb'     bar-$version-ubuntu12.04_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_12.04/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu12.04_amd64.deb
#  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_13.04/i386"     'bar_.*\.deb'     bar-$version-ubuntu13.04_i386.deb
#  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_13.04/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu13.04_i386.deb
#  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_13.04/amd64"    'bar_.*\.deb'     bar-$version-ubuntu13.04_amd64.deb
#  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_13.04/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu13.04_amd64.deb
#  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_13.10/i386"     'bar_.*\.deb'     bar-$version-ubuntu13.10_i386.deb
#  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_13.10/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu13.10_i386.deb
#  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_13.10/amd64"    'bar_.*\.deb'     bar-$version-ubuntu13.10_amd64.deb
#  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_13.10/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu13.10_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_14.04/i386"     'bar_.*\.deb'     bar-$version-ubuntu14.04_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_14.04/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu14.04_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_14.04/amd64"    'bar_.*\.deb'     bar-$version-ubuntu14.04_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_14.04/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu14.04_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_14.10/i386"     'bar_.*\.deb'     bar-$version-ubuntu14.10_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_14.10/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu14.10_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_14.10/amd64"    'bar_.*\.deb'     bar-$version-ubuntu14.10_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_14.10/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu14.10_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_15.10/i386"     'bar_.*\.deb'     bar-$version-ubuntu15.10_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_15.10/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu15.10_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_15.10/amd64"    'bar_.*\.deb'     bar-$version-ubuntu15.10_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_15.10/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu15.10_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_16.04/i386"     'bar_.*\.deb'     bar-$version-ubuntu16.04_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_16.04/i386"     'bar-gui_.*\.deb' bar-gui-$version-ubuntu16.04_i386.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_16.04/amd64"    'bar_.*\.deb'     bar-$version-ubuntu16.04_amd64.deb
  downloadPackage "$OSC_URL/$OSC_PATH/xUbuntu_16.04/amd64"    'bar-gui_.*\.deb' bar-gui-$version-ubuntu16.04_amd64.deb
}

list()
{
  local version="$1"; shift

  $MD5SUM -b bar-*$version*
}

htmlCodeEntry()
{
  local title="$1"; shift
  local type="$1"; shift
  local fileName="$1"; shift

  local s="";
  case "$type" in
    CMD) s=" (command line tools)"; ;;
    GUI) s=" (GUI)"; ;;
  esac

  $CAT <<EOT
        </TR>
          <TD>$title</TD>
          <TD>$version$s</TD>
          <TD><A HREF="$fileName">$fileName</A></TD>
          <TD>`$MD5SUM -b $fileName|$CUT -d " " -f 1`</TD>
        </TR>
EOT
}

htmlCode()
{
  local version="$1"; shift

  $CAT <<EOT
      <TABLE BORDER=1>
        <TR>
          <TH>System</TH>
          <TH>Version</TH>
          <TH>Package</TH>
          <TH>md5sum</TH>
        </TR>
EOT

  htmlCodeEntry "CentOS 6, 32bit"        "" bar-$version-centos6_i686.rpm
  htmlCodeEntry "CentOS 6, 64bit x86_64" "" bar-$version-centos6_x86_64.rpm
  htmlCodeEntry "CentOS 7, 64bit x86_64" "" bar-$version-centos7_x86_64.rpm

  htmlCodeEntry "Debian 7, 32bit"        CMD bar-$version-debian7_i386.deb
  htmlCodeEntry "Debian 7, 32bit"        GUI bar-gui-$version-debian7_i386.deb
  htmlCodeEntry "Debian 7, 64bit x86_64" CMD bar-$version-debian7_amd64.deb
  htmlCodeEntry "Debian 7, 64bit x86_64" GUI bar-gui-$version-debian7_amd64.deb
  htmlCodeEntry "Debian 8, 32bit"        CMD bar-$version-debian8_i386.deb
  htmlCodeEntry "Debian 8, 32bit"        GUI bar-gui-$version-debian8_i386.deb
  htmlCodeEntry "Debian 8, 64bit x86_64" CMD bar-$version-debian8_amd64.deb
  htmlCodeEntry "Debian 8, 64bit x86_64" GUI bar-gui-$version-debian8_amd64.deb

  htmlCodeEntry "Fedora 20, 32bit"        "" bar-$version-fedora20_i686.rpm
  htmlCodeEntry "Fedora 20, 64bit x86_64" "" bar-$version-fedora20_x86_64.rpm
  htmlCodeEntry "Fedora 21, 32bit"        "" bar-$version-fedora21_i686.rpm
  htmlCodeEntry "Fedora 21, 64bit x86_64" "" bar-$version-fedora21_x86_64.rpm
  htmlCodeEntry "Fedora 22, 32bit"        "" bar-$version-fedora22_i686.rpm
  htmlCodeEntry "Fedora 22, 64bit x86_64" "" bar-$version-fedora22_x86_64.rpm
#  htmlCodeEntry "Fedora 23, 32bit"        "" bar-$version-fedora23_i686.rpm
  htmlCodeEntry "Fedora 23, 64bit x86_64" "" bar-$version-fedora23_x86_64.rpm

  htmlCodeEntry "RedHat 6, 32bit"        "" bar-$version-redhat6_i686.rpm
  htmlCodeEntry "RedHat 6, 64bit x86_64" "" bar-$version-redhat6_x86_64.rpm
  htmlCodeEntry "RedHat 7, 64bit x86_64" "" bar-$version-redhat7_x86_64.rpm

  htmlCodeEntry "SLE 11 SP4, 32bit"        "" bar-$version-sle11_i586.rpm
  htmlCodeEntry "SLE 11 SP4, 64bit x86_64" "" bar-$version-sle11_x86_64.rpm
#  htmlCodeEntry "SLE 12, 32bit"            "" bar-$version-sle12_i586.rpm
  htmlCodeEntry "SLE 12, 64bit x86_64"     "" bar-$version-sle12_x86_64.rpm
#  htmlCodeEntry "SLE 12 SP1, 32bit"        "" bar-$version-sle12_1_i586.rpm
  htmlCodeEntry "SLE 12 SP1, 64bit x86_64" "" bar-$version-sle12_1_x86_64.rpm

  htmlCodeEntry "openSuSE 13.1, 32bit"        "" bar-$version-opensuse13.1_i586.rpm
  htmlCodeEntry "openSuSE 13.1, 64bit x86_64" "" bar-$version-opensuse13.1_x86_64.rpm
  htmlCodeEntry "openSuSE 13.2, 32bit"        "" bar-$version-opensuse13.2_i586.rpm
  htmlCodeEntry "openSuSE 13.2, 64bit x86_64" "" bar-$version-opensuse13.2_x86_64.rpm

  htmlCodeEntry "Ubuntu 12.04, 32bit"        CMD bar-$version-ubuntu12.04_i386.deb
  htmlCodeEntry "Ubuntu 12.04, 32bit"        GUI bar-gui-$version-ubuntu12.04_i386.deb
  htmlCodeEntry "Ubuntu 12.04, 64bit x86_64" CMD bar-$version-ubuntu12.04_amd64.deb
  htmlCodeEntry "Ubuntu 12.04, 64bit x86_64" GUI bar-gui-$version-ubuntu12.04_amd64.deb
  htmlCodeEntry "Ubuntu 14.04, 32bit"        CMD bar-$version-ubuntu14.04_i386.deb
  htmlCodeEntry "Ubuntu 14.04, 32bit"        GUI bar-gui-$version-ubuntu14.04_i386.deb
  htmlCodeEntry "Ubuntu 14.04, 64bit x86_64" CMD bar-$version-ubuntu14.04_amd64.deb
  htmlCodeEntry "Ubuntu 14.04, 64bit x86_64" GUI bar-gui-$version-ubuntu14.04_amd64.deb
  htmlCodeEntry "Ubuntu 14.10, 32bit"        CMD bar-$version-ubuntu14.10_i386.deb
  htmlCodeEntry "Ubuntu 14.10, 32bit"        GUI bar-gui-$version-ubuntu14.10_i386.deb
  htmlCodeEntry "Ubuntu 14.10, 64bit x86_64" CMD bar-$version-ubuntu14.10_amd64.deb
  htmlCodeEntry "Ubuntu 14.10, 64bit x86_64" GUI bar-gui-$version-ubuntu14.10_amd64.deb
  htmlCodeEntry "Ubuntu 15.10, 32bit"        CMD bar-$version-ubuntu15.10_i386.deb
  htmlCodeEntry "Ubuntu 15.10, 32bit"        GUI bar-gui-$version-ubuntu15.10_i386.deb
  htmlCodeEntry "Ubuntu 15.10, 64bit x86_64" CMD bar-$version-ubuntu15.10_amd64.deb
  htmlCodeEntry "Ubuntu 15.10, 64bit x86_64" GUI bar-gui-$version-ubuntu15.10_amd64.deb
  htmlCodeEntry "Ubuntu 16.04, 32bit"        CMD bar-$version-ubuntu16.04_i386.deb
  htmlCodeEntry "Ubuntu 16.04, 32bit"        GUI bar-gui-$version-ubuntu16.04_i386.deb
  htmlCodeEntry "Ubuntu 16.04, 64bit x86_64" CMD bar-$version-ubuntu16.04_amd64.deb
  htmlCodeEntry "Ubuntu 16.04, 64bit x86_64" GUI bar-gui-$version-ubuntu16.04_amd64.deb

  $CAT <<EOT
      </TABLE>
EOT
}

# ----------------------------------------------------------------------------

# print usage-help
printUsage()
{
  $CAT << EOT
Usage: $PROGRAM_NAME [<option>...] [--] <version>

Options:
         -h|--help      - print this help
         -l             - output list with MD5 sums
         --html         - output HTML code
EOT
}

# ------------------------------------ main ----------------------------------

# get arguments
version=""
n=0
while test $# != 0; do
  case $1 in
    -h | --help)
      printUsage
      exit $EXITCODE_OK
      ;;
    -l=* | --list=*)
      listFlag=`$ECHO "$1" | $SED 's/^[^=]*=\(.*\)$/\1/g'`
      shift
      ;;
    -l | --list)
      listFlag=1
      shift
      ;;
    --html=*)
      htmlFlag=`$ECHO "$1" | $SED 's/^[^=]*=\(.*\)$/\1/g'`
      shift
      ;;
    --html)
      htmlFlag=1
      shift
      ;;
    --simulate=*)
      simulateFlag=`$ECHO "$1" | $SED 's/^[^=]*=\(.*\)$/\1/g'`
      shift
      ;;
    --simulate)
      simulateFlag=1
      shift
      ;;
    --quiet=*)
      quietFlag=`$ECHO "$1" | sed 's/^[^=]*=\(.*\)$/\1/g'`
      shift
      ;;
    --quiet)
      quietFlag=1
      shift
      ;;
    --)
      shift
      break
      ;;
    -*)
      printError "unknown option '$1'"
      printUsage
      exit $EXITCODE_INVALID_ARGUMENT
      ;;
    *)
      version=$1
      shift
      ;;
  esac
done
while test $# != 0; do
  version=$1
  shift
done

# check arguments
if test -z "$version"; then
  $ECHO >&2 "ERROR: no version specified!"
  printUsage
  exit $EXITCODE_INVALID_ARGUMENT
fi

if   test $listFlag -eq 1; then
  list $version
elif test $htmlFlag -eq 1; then
  htmlCode $version
else
  download $version
  list $version
fi

exit 0
