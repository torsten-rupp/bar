#!/bin/sh
#\
exec wish "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol.tcl,v $
# $Revision: 1.4 $
# $Author: torsten $
# Contents: Backup ARchiver frontend
# Systems: all with TclTk+Tix
#
# ----------------------------------------------------------------------------

# ---------------------------- additional packages ---------------------------

# get program base path
if {[catch {set basePath [file dirname [file readlink $argv0]]}]} \
{
  set basePath [file dirname $argv0]
}
if {$basePath==""} { set BasePath "." }

# extend package library path
lappend auto_path $basePath
lappend auto_path tcltk-lib
lappend auto_path $env(HOME)/sources/tcl-lib
lappend auto_path $env(HOME)/sources/tcltk-lib

# load packages
if {[catch {package require Tix}]} \
{
  # check if 'tix' is in any sub-directory of package path
  set l ""
  foreach z "$auto_oldpath $tcl_pkgPath" \
  {
    set l [lindex [glob -path $z/ -type f -nocomplain -- libTix*] 0]
    if {$l!=""} { break }
    set l [lindex [glob -path $z/ -type f -nocomplain -- libtix*] 0]
    if {$l!=""} { break }
  }
  # output error info
  if {$l!=""} \
  {
    puts "ERROR: Found '$l' (permission: [file attribute $l -permission]), but it seems not"
    puts "to be usable or accessable. Please check if"
    puts " - version is correct,"
    puts " - file is accessable (permission 755 or more),"
    puts " - directory of file is included in search path of system linker."
  } \
  else \
  {
    puts "Package 'Tix' cannot be found (library libtix*, libTix* not found). Please check if"
    puts " - 'Tix*' package is installed in '$tcl_pkgPath',"
    puts " - '$tcl_pkgPath/Tix*' is accessable,"
    puts " - library libtix* or libTix* exists somewhere in '$tcl_pkgPath'."
  }
  exit 104
}
package require tools
package require mclistbox
load "[pwd]/tcl/scanx.so"

# ---------------------------- constants/variables ---------------------------

# --------------------------------- includes ---------------------------------

# -------------------------------- main window -------------------------------

# init main window
set mainWindow ""
wm title . "BAR control"
wm iconname . "BAR"
wm geometry . "600x600"

# ------------------------ internal constants/variables ----------------------

set config(JOB_LIST_UPDATE_TIME)    5000
set config(CURRENT_JOB_UPDATE_TIME) 1000

set server(socketHandle)  -1
set server(lastCommandId) 0

set barConfigFileName     ""
set barConfigModifiedFlag 0

set barConfig(storageType)             ""
set barConfig(storageLoginName)        ""
set barConfig(storageHostName)         ""
set barConfig(storageFileName)         ""
set barConfig(archivePartSizeFlag)     0
set barConfig(archivePartSize)         0
set barConfig(maxTmpDirectorySizeFlag) 0
set barConfig(maxTmpDirectorySize)     0
set barConfig(sshPassword)             ""
set barConfig(sshPort)                 0
set barConfig(compressAlgorithm)       ""
set barConfig(cryptAlgorithm)          ""
set barConfig(cryptPassword)           ""

set currentJob(id)                     0
set currentJob(doneFiles)              0
set currentJob(doneBytes)              0
set currentJob(compressionRatio)       0
set currentJob(totalFiles)             0
set currentJob(totalBytes)             0
set currentJob(fileName)               ""
set currentJob(storageName)            ""

set jobListTimerId    0
set currentJobTimerId 0

# file list data format
#  {<type> [NONE|INCLUDED|EXCLUDED] <directory open flag>}

# --------------------------------- images -----------------------------------

# images
set images(folder) [image create photo -data \
{
  R0lGODlhEAAMAKEDAAD//wAAAPD/gP///yH5BAEAAAMALAAAAAAQAAwAAAIgnINhyycvVFsB
  QtmS3rjaH1Hg141WaT5ouprt2HHcUgAAOw==
}]
set images(folderOpen) [image create photo -data \
{
  R0lGODlhEAAMAMIEAAD//wAAAP//AFtXRv///////////////yH5BAEAAAAALAAAAAAQAAwA
  AAMtCBoc+nCJKVx8gVIbm/9cMAhjSZLhCa5jpr1VNrjw5Ih0XQFZXt++F2Ox+jwSADs=
}]
set images(folderIncluded) [image create photo -data \
{
  R0lGODlhEAAMAKEDAAD//wAAAADNAP///yH5BAEKAAMALAAAAAAQAAwAAAIgnINhyycvVFsB
  QtmS3rjaH1Hg141WaT5ouprt2HHcUgAAOw==
}]
set images(folderIncludedOpen) [image create photo -data \
{
  R0lGODlhEAAMAMIEAAD//wAAAADNAFtXRv///////////////yH5BAEKAAAALAAAAAAQAAwA
  AAMtCBoc+nCJKVx8gVIbm/9cMAhjSZLhCa5jpr1VNrjw5Ih0XQFZXt++F2Ox+jwSADs=
}]
set images(folderExcluded) [image create photo -data \
{
  R0lGODlhEAAMAMIEAP8AAAD//wAAAPD/gP///////////////yH5BAEKAAEALAAAAAAQAAwA
  AAMzCBDSEjCoqMC448lJFc5VxAiVU2rMVQ1rFglY5V0orMpYfVut3rKe2m+HGsY4G4eygUwA
}]
set images(file) [image create photo -data \
{
  R0lGODlhDAAMAKEDAAD//wAAAP//8////yH5BAEAAAMALAAAAAAMAAwAAAIdXI43a+IfWHsO
  rUBpnFls3HlXKHLZR6Kh2n3JOxQAOw==
}]
set images(fileIncluded) [image create photo -data \
{
  R0lGODlhDAAMAKEDAAD//wAAAADNAP///yH5BAEKAAMALAAAAAAMAAwAAAIdXI43a+IfWHsO
  rUBpnFls3HlXKHLZR6Kh2n3JOxQAOw==
}]
set images(fileExcluded) [image create photo -data \
{
  R0lGODlhDAAMAMIEAP8AAAAAAAD/////8////////////////yH5BAEKAAIALAAAAAAMAAwA
  AAMqCBDcISqOOUOEio4rbN7K04ERMIjBVFbCSJpnm5YZKpHirSoYrNEOhyIBADs=
}]
set images(link) [image create photo -data \
{
  R0lGODlhDAAMAMIDAAD//wAAAP//8////////////////////yH5BAEKAAAALAAAAAAMAAwA
  AAMmCLG88ErIGWAcS7IXBM4a5zXh1XSVSabdtwwCO75lS9dTHnNnAyQAOw==
}]
set images(linkIncluded) [image create photo -data \
{
  R0lGODlhDAAMAKEDAAD//wAAAADNAP///yH5BAEKAAMALAAAAAAMAAwAAAIjXI43a+IfWBPH
  oRVstVjvOCUZmFEJ1ZlfikBsyU2Pa4jJUAAAOw==
}]
set images(linkExcluded) [image create photo -data \
{
  R0lGODlhDAAMAMIEAP8AAAAAAAD/////8////////////////yH5BAEKAAcALAAAAAAMAAwA
  AAMuCBDccSqOOUOEis17bPbL0Q2R9IxEGQznmkZrS5YAK4IMLMIBMQOYGmWjcjQUCQA7
}]

# -------------------------------- functions ---------------------------------

#***********************************************************************
# Name   : internalError
# Purpose: output internal error
# Input  : args - optional arguments
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc internalError { args } \
{
  error "INTERNAL ERROR: [join $args]"
}

#***********************************************************************
# Name   : Dialog:error
# Purpose: show error-dialog
# Input  : message - message
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc Dialog:error { message } \
{
  set image [image create photo -data \
  {
    R0lGODdhIAAgAPEAAAAAANnZ2f8AAP///ywAAAAAIAAgAAAClIyPBsubDw8TtFIW47KcuywB
    3Vh9mUim16m2AgChrgoj20zXyjjgli6zDIYpYqcWpAyXPKZn11kaK9LeMymUUrWjG6kqAJO8
    36pzjI2aW+QiN+cyW1NttRzuZoq7gXT4vfcDxRH4d1YyuHWoOIeYqDSldmSD1Thj8ugj+OCH
    o8OpuXnS2fU56mkK0ld3ganK2YChWgAAOw==
  }]

  catch {destroy .dialog_error}
  toplevel .dialog_error
  wm title .dialog_error "Error/Alert"

  frame .dialog_error.message
    label .dialog_error.message.image -image $image
    pack .dialog_error.message.image -side left -fill y
    message .dialog_error.message.text -width 400 -text $message
    pack .dialog_error.message.text -side right -fill both -expand yes -padx 2m -pady 2m
  pack .dialog_error.message -padx 2m -pady 2m
  button .dialog_error.ok -text " Ok " -command "destroy .dialog_error"
  pack .dialog_error.ok -side bottom -padx 2m -pady 2m
  bind .dialog_error.ok <Return> ".dialog_error.ok invoke"
  bind .dialog_error.ok <Escape> ".dialog_error.ok invoke"

  focus .dialog_error.ok
  raise .dialog_error

  catch {tkwait window .dialog_error}
}

#***********************************************************************
# Name   : Dialog:ok
# Purpose: show ok-dialog
# Input  : message - message
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc Dialog:ok { message } \
{
  puts "OK: $message\n"
}

#***********************************************************************
# Name   : progressbar
# Purpose: progress bar
# Input  : path - widget path
#          args - optional arguments
# Output : -
# Return : -
# Notes  : optional
#            <path> update <value>
#***********************************************************************

proc progressbar { path args } \
{
  if {[llength $args] == 0} \
  {
    frame $path -height 18 -relief sunken -borderwidth 1
      frame $path.back -background white -borderwidth 0
        label $path.back.text -foreground black -background white -text "0%"
        place $path.back.text -relx 0.5 -rely 0.5 -anchor center
        frame $path.back.fill -background lightblue
          label $path.back.fill.text -foreground white -background lightblue -text "0%"
          place $path.back.fill.text -x 0 -rely 0.5 -anchor center
        place $path.back.fill -x 0 -y 0 -relwidth 0.0 -relheight 1.0
      place $path.back -x 0 -y 0 -relwidth 1.0 -relheight 1.0
      bind $path.back <Configure> " place conf $path.back.fill.text -x \[expr {int(%w/2)}\] "
  } \
  else \
  {
    if {[lindex $args 0] == "update"} \
    {
      set p [lindex $args 1]
      place configure $path.back.fill -relwidth $p
      $path.back.text configure -text "[expr {int($p*100)}]%"
      $path.back.fill.text configure -text "[expr {int($p*100)}]%"
      update
    }
  } 
}

#***********************************************************************
# Name   : addEnableDisableTrace
# Purpose: add enable/disable trace
# Input  : name           - variable name
#          conditionValue - condition value
#          action1        - action if condition is true
#          action2        - action if condition is false
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addEnableDisableTrace { name conditionValue action1 action2 } \
{
  proc enableDisableTraceHandler { conditionValue action1 action2 name1 name2 op } \
  {
#puts "$conditionValue: $name1 $name2"
    # get value
    if {$name2!=""} \
    {
      eval "set value \$::$name1\($name2\)"
    } \
    else \
    {
      eval "set value \$::$name1"
    }

    if {[eval {expr {$value==$conditionValue}}]} { eval $action1 } else { eval $action2 }
  }

  eval "set value \$$name"
  if {$value==$conditionValue} { eval $action1 } else { eval $action2 }

  trace variable $name w "enableDisableTraceHandler $conditionValue {$action1} {$action2}"
}

#***********************************************************************
# Name   : addEnableTrace/addDisableTrace
# Purpose: add enable trace
# Input  : name           - variable name
#          conditionValue - condition value
#          widget         - widget to enable/disable
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addEnableTrace { name conditionValue widget } \
{
  addEnableDisableTrace $name $conditionValue "$widget configure -state normal" "$widget configure -state disabled"
}
proc addDisableTrace { name conditionValue widget } \
{
  addEnableDisableTrace $name $conditionValue "$widget configure -state disabled" "$widget configure -state normal"
}

#***********************************************************************
# Name   : addModifyTrace
# Purpose: add modify trace
# Input  : name   - variable name
#          action - action to execute when variable is modified
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addModifyTrace { name action } \
{
  proc modifyTraceHandler { action name1 name2 op } \
  {
    eval $action
  }

  eval $action

  trace variable $name w "modifyTraceHandler {$action}"
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : resetBarConfig
# Purpose: reset bar config to default values
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc resetBarConfig {} \
{
  global barConfig

  set barConfig(storageType)             "FILESYSTEM"
  set barConfig(storageHostName)         ""
  set barConfig(storageLoginName)        ""
  set barConfig(storageFileName)         ""
  set barConfig(archivePartSizeFlag)     0
  set barConfig(archivePartSize)         0
  set barConfig(maxTmpDirectorySizeFlag) 0
  set barConfig(maxTmpDirectorySize)     0
  set barConfig(sshPassword)             ""
  set barConfig(sshPort)                 22
  set barConfig(compressAlgorithm)       "bzip9"
  set barConfig(cryptAlgorithm)          "none"
  set barConfig(cryptPassword)           ""
}

#***********************************************************************
# Name   : escapeString
# Purpose: escape string: s -> 's' with escaping of ' and \
# Input  : s - string
# Output : -
# Return : escaped string
# Notes  : -
#***********************************************************************

proc escapeString { s } \
{
  return "'[string map {"'" "\\'" "\\" "\\\\"} $s]'"
}

#***********************************************************************
# Name   : formatByteSize
# Purpose: format byte size in human readable format
# Input  : n - byte size
# Output : -
# Return : string
# Notes  : -
#***********************************************************************

proc formatByteSize { n } \
{
  if     {$n > 1024*1024*1024} { return [format "%.1fG" [expr {double($n)/(1024*1024*1024)}]] } \
  elseif {$n >      1024*1024} { return [format "%.1fM" [expr {double($n)/(     1024*1024)}]] } \
  elseif {$n >           1024} { return [format "%.1fK" [expr {double($n)/(          1024)}]] } \
  else                         { return [format "%d"    $n                                  ] }
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : Server:connect
# Purpose: connect to server
# Input  : hostname - host name
#          port     - port number
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc Server:connect { hostname port } \
{
  global server

  if {[catch {set server(socketHandle) [socket $hostname $port]}]} \
  {
    return 0
  }
  fconfigure $server(socketHandle) -buffering line -blocking 1 -translation lf

  return 1
}

#***********************************************************************
# Name   : Server:disconnect
# Purpose: disconnect from server
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc Server:disconnect {} \
{
  global server

  if {$server(socketHandle) != -1} \
  {
    catch {close $server(socketHandle)}
    set server(socketHandle) -1
  }
}

#***********************************************************************
# Name   : Server:sendCommand
# Purpose: send command to server
# Input  : command - command
#          args    - arguments for command
# Output : -
# Return : command id or 0 on error
# Notes  : -
#***********************************************************************

proc Server:sendCommand { command args } \
{
  global server

  incr server(lastCommandId)

  set arguments [join $args]

  if {[catch {puts $server(socketHandle) "$command $server(lastCommandId) $arguments"; flush $server(socketHandle)}]} \
  {
    return 0
  }
#puts "sent [clock clicks]: $command $server(lastCommandId) $arguments"

  return $server(lastCommandId)
}

#***********************************************************************
# Name   : Server:readResult
# Purpose: read result from server
# Input  : commandId - command id
# Output : _errorCode - error code
#          _result    - result data
# Return : 1 if result read, 0 for end of data
# Notes  : -
#***********************************************************************

proc Server:readResult { commandId _errorCode _result } \
{
  global server

  upvar $_errorCode errorCode
  upvar $_result    result

  set id -1
  while {($id != $commandId) && ($id != 0)} \
  {
     if {[eof $server(socketHandle)]} { return 0 }
    gets $server(socketHandle) line
puts "received [clock clicks] [eof $server(socketHandle)]: $line"

    set completeFlag 0
    set errorCode    -1
    set result       ""
    regexp {(\d+)\s+(\d+)\s+(\d+)\s+(.*)} $line * id completeFlag errorCode result
  }
#puts "$completeFlag $errorCode $result"

  return [expr {!$completeFlag}]
}

#***********************************************************************
# Name   : Server:executeCommand
# Purpose: execute command
# Input  : command - command
#          args    - arguments for command
# Output : _errorCode - error code (will only be set if 0)
#          _errorText - error text (will only be set if _errorCode is 0)
# Return : 1 if command executed, 0 otherwise
# Notes  : -
#***********************************************************************

proc Server:executeCommand { _errorCode _errorText command args } \
{
  upvar $_errorCode errorCode
  upvar $_errorText errorText

  set commandId [Server:sendCommand $command $args]
  if {$commandId == 0} \
  {
    return 0;
  }
  Server:readResult $commandId localErrorCode result
#puts "execute result: $localErrorCode '$result' [expr {$localErrorCode == 0}]"

  if {$localErrorCode == 0} \
  {
    return 1
  } \
  else \
  {
    if {$errorCode == 0} \
    {
      set errorCode $localErrorCode
      set errorText $result
    }
    return 0
  }
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : updateJobList
# Purpose: update job list
# Input  : jobListWidget - job list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc updateJobList { jobListWidget } \
{
  global jobListTimerId config

  catch {after cancel $jobListTimerId}

  # get current selection
  set currentId 0
  if {[$jobListWidget curselection] != {}} \
  {
    set n [lindex [$jobListWidget curselection] 0]
    set currentId [lindex [lindex [$jobListWidget get $n $n] 0] 0]
  }

  # update list
  $jobListWidget delete 0 end
  set commandId [Server:sendCommand "JOB_LIST"]
  while {[Server:readResult $commandId errorCode result]} \
  {
    scanx $result "%d %s %d %S %S %d %d" \
      id \
      state \
      archivePartSize \
      compressAlgorithm \
      cryptAlgorithm \
      startTime \
      estimatedRestTime

    set estimatedRestDays    [expr {int($estimatedRestTime/(24*60*60)        )}]
    set estimatedRestHours   [expr {int($estimatedRestTime%(24*60*60)/(60*60))}]
    set estimatedRestMinutes [expr {int($estimatedRestTime%(60*60   )/60     )}]
    set estimatedRestSeconds [expr {int($estimatedRestTime%(60)              )}]

    $jobListWidget insert end [list \
      $id \
      $state \
      [formatByteSize $archivePartSize] \
      $compressAlgorithm \
      $cryptAlgorithm \
      [expr {($startTime > 0)?[clock format $startTime -format "%Y-%m-%d %H:%M:%S"]:"-"}] \
      [format "%2d days %02d:%02d:%02d" $estimatedRestDays $estimatedRestHours $estimatedRestMinutes $estimatedRestSeconds] \
    ]
  }

  # restore selection
  if     {$currentId > 0} \
  {
    set n 0
    while {($n < [$jobListWidget index end]) && ($currentId != [lindex [lindex [$jobListWidget get $n $n] 0] 0])} \
    {
      incr n
    }
    if {$n < [$jobListWidget index end]} \
    {
      $jobListWidget selection set $n
    }
    
  }

  set jobListTimerId [after $config(JOB_LIST_UPDATE_TIME) "updateJobList $jobListWidget"]
}

#***********************************************************************
# Name   : updateCurrentJob
# Purpose: update current job data
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc updateCurrentJob { } \
{
  global currentJob currentJobTimerId config

  if {$currentJob(id) != 0} \
  {
    catch {after cancel $currentJobTimerId}

    set commandId [Server:sendCommand "JOB_INFO" $currentJob(id)]
    Server:readResult $commandId errorCode result
    scanx $result "%s %lu %lu %f %lu %lu %S %S" \
      state \
      currentJob(doneFiles) \
      currentJob(doneBytes) \
      currentJob(compressionRatio) \
      currentJob(totalFiles) \
      currentJob(totalBytes) \
      currentJob(fileName) \
      currentJob(storageName)
  } \
  else \
  {
    set currentJob(doneFiles)        0
    set currentJob(doneBytes)        0
    set currentJob(compressionRatio) 0
    set currentJob(totalFiles)       0
    set currentJob(totalBytes)       0
    set currentJob(fileName)         ""
    set currentJob(storageName)      ""
  }

  set currentJobTimerId [after $config(CURRENT_JOB_UPDATE_TIME) "updateCurrentJob"]
}

#***********************************************************************
# Name       : itemPathToFileName
# Purpose    : convert item path to file name
# Input      : fileListWidget - file list widget
#              itemPath       - item path
# Output     : -
# Return     : file name
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc itemPathToFileName { fileListWidget itemPath } \
 {
  set separator [lindex [$fileListWidget configure -separator] 4]
#puts "$itemPath -> #[string range $itemPath [string length $Separator] end]#"

  return [string map [list $separator "/"] $itemPath]
 }

#***********************************************************************
# Name       : fileNameToItemPath
# Purpose    : convert file name to item path
# Input      : fileListWidget - file list widget
#              fileName       - file name
# Output     : -
# Return     : item path
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc fileNameToItemPath { fileListWidget fileName } \
 {
  # get separator
  set separator "/"
  catch {set separator [lindex [$fileListWidget configure -separator] 4]}

  # get path name
   if {($fileName != ".") && ($fileName != "/")} \
    {
     return [string map [list "/" $separator] $fileName]
    } \
   else \
    {
     return "$separator"
    }
 }

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : clearFileList
# Purpose: clear file list
# Input  : fileListWidget - file list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc clearFileList { fileListWidget } \
{
  global images

  foreach itemPath [$fileListWidget info children ""] \
  {
    $fileListWidget delete offsprings $itemPath
    $fileListWidget item configure $itemPath 0 -image $images(folder)
  }
}

#***********************************************************************
# Name   : addDevice
# Purpose: add a device to tree-widget
# Input  : fileListWidget - file list widget
#          deviceName     - file name/directory name
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addDevice { fileListWidget deviceName } \
{
  catch {$fileListWidget delete entry $deviceName}

  set n 0
  set l [$fileListWidget info children ""]
  while {($n < [llength $l]) && (([$fileListWidget info data [lindex $l $n]] != {}) || ($deviceName>[lindex $l $n]))} \
  {
    incr n
  }

  set style [tixDisplayStyle imagetext -refwindow $fileListWidget]

  $fileListWidget add $deviceName -at $n -itemtype imagetext -text $deviceName -image [tix getimage folder] -style $style -data [list "DIRECTORY" "NONE" 0]
  $fileListWidget item create $deviceName 1 -itemtype imagetext -style $style
  $fileListWidget item create $deviceName 2 -itemtype imagetext -style $style
  $fileListWidget item create $deviceName 3 -itemtype imagetext -style $style
}

#***********************************************************************
# Name   : addEntry
# Purpose: add a file/directory/link entry to tree-widget
# Input  : fileListWidget - file list widget
#          fileName       - file name/directory name
#          fileType       - FILE|DIRECTORY|LINK
#          fileSize       - file size
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addEntry { fileListWidget fileName fileType fileSize } \
{
  global images

  # get parent directory
  if {[file tail $fileName] !=""} \
  {
    set parentDirectory [file dirname $fileName]
  } \
  else \
  {
    set parentDirectory ""
  }

  # get item path, parent item path
  set itemPath       [fileNameToItemPath $fileListWidget $fileName       ]
  set parentItemPath [fileNameToItemPath $fileListWidget $parentDirectory]
#puts "f=$fileName"
#puts "i=$itemPath"
#puts "p=$parentItemPath"

  catch {$fileListWidget delete entry $itemPath}

  # create parent entry if it does not exists
  if {($parentItemPath != "") && ![$fileListWidget info exists $parentItemPath]} \
  {
puts "---"
    addEntry $fileListWidget [file dirname $fileName] "DIRECTORY" 0
  }

  set styleImage     [tixDisplayStyle imagetext -refwindow $fileListWidget -anchor w]
  set styleTextLeft  [tixDisplayStyle text      -refwindow $fileListWidget -anchor w]
  set styleTextRight [tixDisplayStyle text      -refwindow $fileListWidget -anchor e]

   if     {$fileType=="FILE"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath - $SortedFlag -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$fileListWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$fileListWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     $fileListWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $images(file) -style $styleImage -data [list "FILE" "NONE" 0]
     $fileListWidget item create $itemPath 1 -itemtype text -text "FILE"    -style $styleTextLeft
     $fileListWidget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $fileListWidget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
   } \
   elseif {$fileType=="DIRECTORY"} \
   {
#puts "add directory $fileName"
     # find insert position (sort)
     set n 0
     set l [$fileListWidget info children $parentItemPath]
     while {($n < [llength $l]) && ([lindex [$fileListWidget info data [lindex $l $n]] 0] != "DIRECTORY") && ($itemPath > [lindex $l $n])} \
     {
       incr n
     }

     # add directory item
     $fileListWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $images(folder) -style $styleImage -data [list "DIRECTORY" "NONE" 0]
     $fileListWidget item create $itemPath 1 -itemtype text -style $styleTextLeft
     $fileListWidget item create $itemPath 2 -itemtype text -style $styleTextLeft
     $fileListWidget item create $itemPath 3 -itemtype text -style $styleTextLeft
   } \
   elseif {$fileType=="LINK"} \
   {
     # add link item
#puts "add link $fileName $RealFilename"
     set n 0
     set l [$fileListWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$fileListWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     set style [tixDisplayStyle imagetext -refwindow $fileListWidget]

     $fileListWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $images(link) -style $styleImage -data [list "LINK" "NONE" 0]
     $fileListWidget item create $itemPath 1 -itemtype text -text "LINK" -style $styleTextLeft
     $fileListWidget item create $itemPath 2 -itemtype text              -style $styleTextLeft
     $fileListWidget item create $itemPath 3 -itemtype text              -style $styleTextLeft
   }
puts "$itemPath: [$fileListWidget info data $itemPath]"
}

#***********************************************************************
# Name   : openCloseDirectory
# Purpose: open/close directory
# Input  : fileListWidget - file list widget
#          itemPath       - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc openCloseDirectory { fileListWidget itemPath } \
{
  global images

  # get directory name
  set directoryName [itemPathToFileName $fileListWidget $itemPath]

  # check if existing, add if not exists
  if {![$fileListWidget info exists $itemPath]} \
  {
    addEntry $fileListWidget $directoryName "DIRECTORY" 0
  }

  # check if parent exist and is open, open it if needed
  set parentItemPath [$fileListWidget info parent $itemPath]
  if {[$fileListWidget info exists $parentItemPath]} \
  {
    set data [$fileListWidget info data $parentItemPath]
    if {[lindex $data 2] == 0} \
    {
      openCloseDirectory $fileListWidget $parentItemPath
    }
  }

  # get data
  set data [$fileListWidget info data $itemPath]

  if {[lindex $data 0] == "DIRECTORY"} \
  {
    # get open/closed flag
    set directoryOpenFlag [lindex $data 2]

    $fileListWidget delete offsprings $itemPath
    if {!$directoryOpenFlag} \
    {
      $fileListWidget item configure $itemPath 0 -image $images(folderOpen)

      set fileName [itemPathToFileName $fileListWidget $itemPath]
      set commandId [Server:sendCommand "FILE_LIST" $fileName 0]
      while {[Server:readResult $commandId errorCode result]} \
      {
        if     {[scanx $result "FILE %d %S" fileSize fileName]} \
        {
          addEntry $fileListWidget $fileName "FILE" $fileSize
        } \
        elseif {[scanx $result "DIRECTORY %d %S" totalSize directoryName]} \
        {
          addEntry $fileListWidget $directoryName "DIRECTORY" $totalSize
        } \
        elseif {[scanx $result "LINK %S" linkName]} \
        {
          addEntry $fileListWidget $linkName "LINK" 0
        } else {
  internalError "xxx"
}
      }

      set directoryOpenFlag 1
    } \
    else \
    {
      $fileListWidget item configure $itemPath 0 -image $images(folder)

      set directoryOpenFlag 0
    }

    # update data
    set data [lreplace $data 2 2 $directoryOpenFlag]
    $fileListWidget entryconfigure $itemPath -data $data
  }
}

#***********************************************************************
# Name   : setEntryState
# Purpose: set entry state
# Input  : fileListWidget - file list widget
#          itemPath       - item path
#          state         - NONE, INCLUDED, EXCLUDED
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc setEntryState { fileListWidget itemPath state } \
{
  global images

  # get data
  set data [$fileListWidget info data $itemPath]
#puts "$itemPath: $state"
#puts $data

  # get type, exclude flag
  set type [lindex $data 0]

  if     {$state == "INCLUDED"} \
  {
    if     {$type == "FILE"} \
    {
      set image $images(file)
    } \
    elseif {$type == "DIRECTORY"} \
    {
      set directoryOpenFlag [lindex $data 2]
      if {$directoryOpenFlag} \
      {
        set image $images(folderIncludedOpen)
      } \
      else \
      {
        set image $images(folderIncluded)
      }
    } \
    elseif {$type == "LINK"} \
    {
      set image $images(link)
    }
  } \
  elseif {$state == "EXCLUDED"} \
  {
    if     {$type == "FILE"} \
    {
      set image $images(fileExcluded)
    } \
    elseif {$type == "DIRECTORY"} \
    {
      set image $images(folderExcluded)
      if {[lindex $data 2]} \
      {
        $fileListWidget delete offsprings $itemPath
        set data [lreplace $data 2 2 0]
      }
    } \
    elseif {$type == "LINK"} \
    {
      set image $images(linkExcluded)
    }
  } \
  else  \
  {
    if     {$type == "FILE"} \
    {
      set image $images(file)
    } \
    elseif {$type == "DIRECTORY"} \
    {
      set image $images(folder)
    } \
    elseif {$type == "LINK"} \
    {
      set image $images(link)
    }
  }
  $fileListWidget item configure $itemPath 0 -image $image

  # update data
  set data [lreplace $data 1 1 $state]
  $fileListWidget entryconfigure $itemPath -data $data

  setConfigModify
}

#***********************************************************************
# Name   : toggleEntryIncludedExcluded
# Purpose: toggle entry state: NONE, INCLUDED, EXCLUDED
# Input  : fileListWidget - file list widget
#          itemPath       - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc toggleEntryIncludedExcluded { fileListWidget itemPath } \
{
  # get data
  set data [$fileListWidget info data $itemPath]

  # get state
  set state [lindex $data 1]

  # set new state
  if     {$state == "NONE"    } { set state "INCLUDED" } \
  elseif {$state == "INCLUDED"} { set state "EXCLUDED" } \
  else                          { set state "NONE"     }

  setEntryState $fileListWidget $itemPath $state
}

#***********************************************************************
# Name   : getIncludedList
# Purpose: get list of included files/directories/links
# Input  : fileListWidget - file list widget
# Output : -
# Return : list of included files/directories/links
# Notes  : -
#***********************************************************************

proc getIncludedList { fileListWidget } \
{
  set includedList {}
  set itemPathList [$fileListWidget info children ""]
  while {[llength $itemPathList] > 0} \
  {
    set itemPath [lindex $itemPathList 0]; set itemPathList [lreplace $itemPathList 0 0]
    set fileName [itemPathToFileName $fileListWidget $itemPath]

    set data [$fileListWidget info data $itemPath]
    set type  [lindex $data 0]
    set state [lindex $data 1]

    if {($type == "DIRECTORY") && ($state != "EXCLUDED")} \
    {
      foreach z [$fileListWidget info children $itemPath] \
      {
        lappend itemPathList $z
      }
    }

    if {$state == "INCLUDED"} \
    {
      lappend includedList $fileName
    }
  }

  return $includedList
}

#***********************************************************************
# Name   : getExcludedList
# Purpose: get list of excluded files/directories/links
# Input  : fileListWidget - file list widget
# Output : -
# Return : list of excluded files/directories/links
# Notes  : -
#***********************************************************************

proc getExcludedList { fileListWidget } \
{
  set excludedList {}
  set itemPathList [$fileListWidget info children ""]
  while {[llength $itemPathList] > 0} \
  {
    set fileName [lindex $itemPathList 0]; set itemPathList [lreplace $itemPathList 0 0]
    set itemPath [fileNameToItemPath $fileListWidget $fileName]

    set data [$fileListWidget info data $itemPath]
    set type              [lindex $data 0]
    set state             [lindex $data 1]
    set directoryOpenFlag [lindex $data 2]

    if {($type == "DIRECTORY") && ($state != "EXCLUDED") && $directoryOpenFlag} \
    {
      foreach z [$fileListWidget info children $itemPath] \
      {
        lappend itemPathList $z
      }
    }

    if {$state == "EXCLUDED"} \
    {
      lappend excludedList $fileName
    }
  }

  return $excludedList
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : clearConfigModify
# Purpose: clear config modify state
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc clearConfigModify {} \
{
  global barConfigFileName barConfigModifiedFlag

  wm title . "$barConfigFileName"
  set barConfigModifiedFlag 0
}

#***********************************************************************
# Name   : setConfigModify
# Purpose: set config modify state
# Input  : args - ignored
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc setConfigModify { args } \
{
  global barConfigFileName barConfigModifiedFlag

  if {!$barConfigModifiedFlag} \
  {
    wm title . "$barConfigFileName*"
    set barConfigModifiedFlag 1
  }
}

#***********************************************************************
# Name   : loadConfig
# Purpose: load BAR config from file
# Input  : configFileName    - config file name or ""
#          fileListWidget    - file list widget
#          patternListWidget - pattern list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc loadConfig { configFileName fileListWidget patternListWidget } \
{
  global tk_strictMotif barConfigFileName barConfigModifiedFlag barConfig errorCode

  # get file name
  if {$configFileName == ""} \
  {
    set old_tk_strictMotif $tk_strictMotif
    set tk_strictMotif 0
    set configFileName [tk_getOpenFile -title "Load configuration" -initialdir "" -filetypes {{"BAR config" "*.cfg"} {"all" "*"}} -parent . -defaultextension ".cfg"]
    set tk_strictMotif $old_tk_strictMotif
    if {$configFileName == ""} { return }
  }

  # open file
  if {[catch {set handle [open $configFileName "r"]}]} \
  {
    Dialog:error "Cannot open file '$configFileName' (error: [lindex $errorCode 2])"
    return;
  }

  # reset variables
  resetBarConfig
  clearFileList $fileListWidget
  $patternListWidget delete 0 end
  
  # read file
  set lineNb 0
  while {![eof $handle]} \
  {
    # read line
    gets $handle line; incr lineNb

    # skip comments, empty lines
    if {[regexp {^\s*$} $line] || [regexp {^\s*#} $line]} \
    {
      continue;
    }

#puts "read $line"
    # parse
    if {[scanx $line "archive-filename = %S" s]} \
    {
      if {[regexp {^scp:([^@])+@([^:]+):(.*)} $s * loginName hostName fileName]} \
      {
        set barConfig(storageType)      "SCP"
        set barConfig(storageLoginName) $loginName
        set barConfig(storageHostName)  $hostName
        set barConfig(storageFileName)  $fileName
      } \
      else \
      {
        set barConfig(storageType) "FILESYSTEM"
        set barConfig(storageLoginName) ""
        set barConfig(storageHostName)  ""
        set barConfig(storageFileName)  $s
      }     
      continue
    }
    if {[scanx $line "archive-part-size = %s" s]} \
    {
      set barConfig(archivePartSizeFlag) 1
      set barConfig(archivePartSize)     $s
      continue
    }
    if {[scanx $line "max-tmp-size = %d" s]} \
    {
      set barConfig(maxTmpDirectorySizeFlag) 1
      set barConfig(maxTmpDirectorySize)     $s
      continue
    }
    if {[scanx $line "ssh-port = %d" n]} \
    {
      set barConfig(sshPort) $s
      continue
    }
    if {[scanx $line "compress-algorithm = %S" s]} \
    {
      set barConfig(compressAlgorithm) $s
      continue
    }
    if {[scanx $line "crypt-algorithm = %S" s]} \
    {
      set barConfig(cryptAlgorithm) $s
      continue
    }
    if {[scanx $line "include = %S" s]} \
    {
      set fileName $s
      set itemPath [fileNameToItemPath $fileListWidget $fileName]

      # create directory
      if {![$fileListWidget info exists $itemPath]} \
      {
        set directoryName [file dirname $fileName]
        set directoryItemPath [fileNameToItemPath $fileListWidget $directoryName]
        openCloseDirectory $fileListWidget $directoryItemPath
      }

      # set state of entry to "included"
      if {[$fileListWidget info exists $itemPath]} \
      {
        setEntryState $fileListWidget $itemPath "INCLUDED"
      }
      continue
    }
    if {[scanx $line "exclude = %S" s]} \
    {
      set fileName $s
      set itemPath [fileNameToItemPath $fileListWidget $fileName]

      # create directory
      if {![$fileListWidget info exists $itemPath]} \
      {
        set directoryName [file dirname $fileName]
        set directoryItemPath [fileNameToItemPath $fileListWidget $directoryName]
        openCloseDirectory $fileListWidget $directoryItemPath
      }

      # set state of entry to "included"
      if {[$fileListWidget info exists $itemPath]} \
      {
        setEntryState $fileListWidget $itemPath "EXCLUDED"
      }
      continue
    }
puts "unknown $line"
  }

  # close file
  close $handle

  set barConfigFileName $configFileName
  clearConfigModify
}

#***********************************************************************
# Name   : saveConfig
# Purpose: saveBAR config into file
# Input  : configFileName    - config file name or ""
#          fileListWidget    - file list widget
#          patternListWidget - pattern list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc saveConfig { configFileName fileListWidget patternListWidget } \
{
  global tk_strictMotif barConfigFileName barConfig errorInfo

  # get file name
  if {$configFileName == ""} \
  {
    set fileName $barConfigFileName
  }
  if {$configFileName == ""} \
  {
    set old_tk_strictMotif $tk_strictMotif
    set tk_strictMotif 0
    set configFileName [tk_getOpenFile -title "Load configuration" -initialdir "" -filetypes {{"BAR config" "*.cfg"} {"all" "*"}} -parent . -defaultextension ".cfg"]
    set tk_strictMotif $old_tk_strictMotif
    if {$configFileName == ""} { return }
  }

  # open file
  if {[catch {set handle [open $configFileName "w"]}]} \
  {
    Dialog:error "Cannot open file '$configFileName' (error: [lindex $errorInfo 2])"
    return;
  }

  # write file
  if     {$barConfig(storageType) == "FILESYSTEM"} \
  {
    puts $handle "archive-filename = [escapeString $barConfig(storageFileName)]"
  } \
  elseif {$barConfig(storageType) == "SCP"} \
  {
    puts $handle "archive-filename = scp:$barConfig(storageLoginName)@$barConfig(storageHostName):[escapeString $barConfig(storageFileName)]"
  }
  if {$barConfig(archivePartSizeFlag)} \
  {
    puts $handle "archive-part-size = $barConfig(archivePartSize)"
  }
  if {$barConfig(maxTmpDirectorySizeFlag)} \
  {
    puts $handle "max-tmp-size = $barConfig(maxTmpDirectorySize)"
  }
  puts $handle "compress-algorithm = [escapeString $barConfig(compressAlgorithm)]"
  puts $handle "crypt-algorithm = [escapeString $barConfig(cryptAlgorithm)]"
  foreach fileName [getIncludedList $fileListWidget] \
  {
    puts $handle "include = [escapeString $fileName]"
  }
  foreach fileName [getExcludedList $fileListWidget] \
  {
    puts $handle "exclude = [escapeString $fileName]"
  }

  # close file
  close $handle

  set barConfigFileName $configFileName
  clearConfigModify
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : quit
# Purpose: quit program
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc quit {} \
{
  global server

  Server:disconnect

  destroy .
}

#***********************************************************************
# Name   : addExcludePattern
# Purpose: add exclude pattern
# Input  : patterListWidget - pattern list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addExcludePattern { patterListWidget } \
{
  global addExcludeDialog

  # dialog
  set addExcludeDialog(handle) .dialog_addExclude
  toplevel $addExcludeDialog(handle)
  wm title $addExcludeDialog(handle) "Add exclude pattern"
  wm geometry $addExcludeDialog(handle) +[winfo pointerx .]+[winfo pointery .]
  set addExcludeDialog(result)  0
  set addExcludeDialog(pattern) ""

  frame $addExcludeDialog(handle).pattern
    label $addExcludeDialog(handle).pattern.title -text "Pattern:"
    grid $addExcludeDialog(handle).pattern.title -row 2 -column 0 -sticky "w"
    entry $addExcludeDialog(handle).pattern.data -bg white -textvariable addExcludeDialog(pattern)
    grid $addExcludeDialog(handle).pattern.data -row 2 -column 1 -sticky "we"
    bind $addExcludeDialog(handle).pattern.data <Return> "focus $addExcludeDialog(handle).buttons.add"

    grid rowconfigure    $addExcludeDialog(handle).pattern { 0 } -weight 1
    grid columnconfigure $addExcludeDialog(handle).pattern { 1 } -weight 1
  grid $addExcludeDialog(handle).pattern -row 0 -column 0 -sticky "nswe" -padx 3p -pady 3p
  
  frame $addExcludeDialog(handle).buttons
    button $addExcludeDialog(handle).buttons.add -text "Add" -command "event generate $addExcludeDialog(handle) <<Event_add>>"
    pack $addExcludeDialog(handle).buttons.add -side left -padx 2p -pady 2p
    bind $addExcludeDialog(handle).buttons.add <Return> "$addExcludeDialog(handle).buttons.add invoke"
    button $addExcludeDialog(handle).buttons.cancel -text "Cancel" -command "event generate $addExcludeDialog(handle) <<Event_cancel>>"
    pack $addExcludeDialog(handle).buttons.cancel -side right -padx 2p -pady 2p
    bind $addExcludeDialog(handle).buttons.cancel <Return> "$addExcludeDialog(handle).buttons.cancel invoke"
  grid $addExcludeDialog(handle).buttons -row 1 -column 0 -sticky "we"

  grid rowconfigure $addExcludeDialog(handle)    0 -weight 1
  grid columnconfigure $addExcludeDialog(handle) 0 -weight 1

  # bindings
  bind $addExcludeDialog(handle) <KeyPress-Escape> "$addExcludeDialog(handle).buttons.cancel invoke"

  bind $addExcludeDialog(handle) <<Event_add>> \
   "
    set addExcludeDialog(result) 1;
    destroy $addExcludeDialog(handle);
   "
  bind $addExcludeDialog(handle) <<Event_cancel>> \
   "
    set addExcludeDialog(result) 0;
    destroy $addExcludeDialog(handle);
   "

  focus $addExcludeDialog(handle).pattern.data
  catch {tkwait window $addExcludeDialog(handle)}
  if {($addExcludeDialog(result) != 1) || ($addExcludeDialog(pattern) == "")} { return }

  # add
  $patterListWidget insert end $addExcludeDialog(pattern)
}

#***********************************************************************
# Name   : remExcludePattern
# Purpose: remove exclude pattern from widget list
# Input  : patterListWidget - pattern list widget
#          index            - index
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc remExcludePattern { patternListWidget index } \
{
  $patternListWidget delete $index
}

#***********************************************************************
# Name   : addJob
# Purpose: add new job
# Input  : fileListWidget    - file list widget
#          patternListWidget - pattern list widget
#          jobListWidget     - job list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addJob { fileListWidget patternListWidget jobListWidget } \
{
  global barConfig currentJob

  set errorCode 0

  # new job
  Server:executeCommand errorCode errorText "NEW_JOB"

  # set archive file name
  if     {$barConfig(storageType) == "FILESYSTEM"} \
  {
    set archiveFileName $barConfig(storageFileName)
  } \
  elseif {$barConfig(storageType) == "SCP"} \
  {
    set archiveFileName "$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)"
  } \
  else \
  {
    internalError "unknown storage type '$barConfig(storageType)'"
  }
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "archive-file" $archiveFileName

  # add included directories/files
puts [getIncludedList $fileListWidget]
  foreach fileName [getIncludedList $fileListWidget] \
  {
    Server:executeCommand errorCode errorText "ADD_INCLUDE_PATTERN" [escapeString $fileName]
  }

  # add excluded directories/files
puts [getExcludedList $fileListWidget]
  foreach fileName [getExcludedList $fileListWidget] \
  {
    Server:executeCommand errorCode errorText "ADD_EXCLUDE_PATTERN" [escapeString $fileName]
  }

#  set commandId [Server:sendCommand "ADD_INCLUDE_PATTERN" "'t*'"]
#  Server:readResult $commandId errorCode result

  # add exclude patterns
  foreach pattern [$patternListWidget get 0 end] \
  {
    Server:executeCommand errorCode errorText "ADD_EXCLUDE_PATTERN" [escapeString $pattern]
  }

  # set other parameters
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "archive-part-size"  $barConfig(archivePartSize)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "compress-algorithm" $barConfig(compressAlgorithm)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "crypt-algorithm"    $barConfig(cryptAlgorithm)

  if {$errorCode == 0} \
  {
    if {![Server:executeCommand errorCode errorText "ADD_JOB"]} \
    {
      Dialog:error "Error adding new job: $errorText"
    }
  } \
  else \
  {
    Dialog:error "Error adding new job: $errorText"
  }

  updateJobList $jobListWidget

  # select first entry if no other selected
  if {$currentJob(id) == 0} \
  {
    set currentJob(id) [lindex [lindex [$jobListWidget get 0 0] 0] 0]
    $jobListWidget selection set 0 0 
  }
}

#***********************************************************************
# Name   : remJob
# Purpose: remove job
# Input  : jobListWidget - job list widget
#          id            - job id
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc remJob { jobListWidget id } \
{
  Server:executeCommand errorCode errorText "REM_JOB" $id

  updateJobList $jobListWidget
}

# ----------------------------- main program  -------------------------------

# menu
frame $mainWindow.menu -relief raised -bd 2
  menubutton $mainWindow.menu.file -text "Program" -menu $mainWindow.menu.file.items -underline 0
  menu $mainWindow.menu.file.items
  $mainWindow.menu.file.items add command -label "Load..." -command "event generate . <<Event_load>>"
  $mainWindow.menu.file.items add command -label "Save"    -command "event generate . <<Event_save>>"
  $mainWindow.menu.file.items add command -label "Save as" -command "event generate . <<Event_saveAs>>"
  $mainWindow.menu.file.items add separator
  $mainWindow.menu.file.items add command -label "Start"   -command "event generate . <<Event_start>>"
  $mainWindow.menu.file.items add separator
  $mainWindow.menu.file.items add command -label "Quit"    -command "event generate . <<Event_quit>>"
  pack $mainWindow.menu.file -side left

  menubutton $mainWindow.menu.edit -text "Edit" -menu $mainWindow.menu.edit.items -underline 0
  menu $mainWindow.menu.edit.items
  $mainWindow.menu.edit.items add command -label "None"    -accelerator "*" -command "event generate . <<Event_stateNone>>"
  $mainWindow.menu.edit.items add command -label "Include" -accelerator "+" -command "event generate . <<Event_stateIncluded>>"
  $mainWindow.menu.edit.items add command -label "Exclude" -accelerator "-" -command "event generate . <<Event_stateExcluded>>"
  pack $mainWindow.menu.edit -side left
pack $mainWindow.menu -side top -fill x

tixNoteBook $mainWindow.tabs
  $mainWindow.tabs add jobs          -label "Jobs"             -underline -1 -raisecmd { focus .jobs.list }
  $mainWindow.tabs add files         -label "Files"            -underline -1 -raisecmd { focus .files.list }
  $mainWindow.tabs add excludes      -label "Excludes"         -underline -1 -raisecmd { focus .excludes.list }
  $mainWindow.tabs add storage       -label "Storage"          -underline -1
  $mainWindow.tabs add compressCrypt -label "Compress & crypt" -underline -1
  $mainWindow.tabs add misc          -label "Misc"             -underline -1
pack $mainWindow.tabs -fill both -expand yes  -padx 3p -pady 3p

frame .jobs
  labelframe .jobs.selected -text "Selected"
    label .jobs.selected.idTitle -text "Id:"
    grid .jobs.selected.idTitle -row 0 -column 0 -sticky "w"
    entry .jobs.selected.id -width 5 -textvariable currentJob(id) -justify right -border 0 -state readonly
    grid .jobs.selected.id -row 0 -column 1 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.doneTitle -text "Done:"
    grid .jobs.selected.doneTitle -row 0 -column 2 -sticky "w" 
    frame .jobs.selected.done
      entry .jobs.selected.done.files -width 10 -textvariable currentJob(doneFiles) -justify right -border 0 -state readonly
      grid .jobs.selected.done.files -row 0 -column 0 -sticky "w" 
      label .jobs.selected.done.filesPostfix -text "files"
      grid .jobs.selected.done.filesPostfix -row 0 -column 1 -sticky "w" 

      entry .jobs.selected.done.bytes -width 20 -textvariable currentJob(doneBytes) -justify right -border 0 -state readonly
      grid .jobs.selected.done.bytes -row 0 -column 2 -sticky "w" 
      label .jobs.selected.done.bytesPostfix -text "bytes"
      grid .jobs.selected.done.bytesPostfix -row 0 -column 3 -sticky "w" 

      label .jobs.selected.done.compressRatioTitle -text "Ratio"
      grid .jobs.selected.done.compressRatioTitle -row 0 -column 4 -sticky "w" 
      entry .jobs.selected.done.compressRatio -width 5 -textvariable currentJob(compressionRatio) -justify right -border 0 -state readonly
      grid .jobs.selected.done.compressRatio -row 0 -column 5 -sticky "w" 
      label .jobs.selected.done.compressRatioPostfix -text "%"
      grid .jobs.selected.done.compressRatioPostfix -row 0 -column 6 -sticky "w" 

      grid rowconfigure    .jobs.selected.done { 0 } -weight 1
      grid columnconfigure .jobs.selected.done { 7 } -weight 1
    grid .jobs.selected.done -row 0 -column 3 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.totalTitle -text "Total:"
    grid .jobs.selected.totalTitle -row 1 -column 2 -sticky "w" 
    frame .jobs.selected.total
      entry .jobs.selected.total.files -width 10 -textvariable currentJob(totalFiles) -justify right -border 0 -state readonly
      grid .jobs.selected.total.files -row 0 -column 0 -sticky "w" 
      label .jobs.selected.total.filesPostfix -text "files"
      grid .jobs.selected.total.filesPostfix -row 0 -column 1 -sticky "w" 

      entry .jobs.selected.total.bytes -width 20 -textvariable currentJob(totalBytes) -justify right -border 0 -state readonly
      grid .jobs.selected.total.bytes -row 0 -column 2 -sticky "w" 
      label .jobs.selected.total.bytesPostfix -text "bytes"
      grid .jobs.selected.total.bytesPostfix -row 0 -column 3 -sticky "w" 

      grid rowconfigure    .jobs.selected.total { 0 } -weight 1
      grid columnconfigure .jobs.selected.total { 4 } -weight 1
    grid .jobs.selected.total -row 1 -column 3 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.currentFileNameTitle -text "File:"
    grid .jobs.selected.currentFileNameTitle -row 2 -column 0 -sticky "w"
    entry .jobs.selected.currentFileName -textvariable currentJob(fileName) -border 0 -state readonly
#entry .jobs.selected.done.percentageContainer.x
#pack .jobs.selected.done.percentageContainer.x -fill x -expand yes
    grid .jobs.selected.currentFileName -row 2 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.currentStorageTitle -text "Storage:"
    grid .jobs.selected.currentStorageTitle -row 3 -column 0 -sticky "w"
    entry .jobs.selected.currentStorage -textvariable currentJob(storageName) -border 0 -state readonly
#entry .jobs.selected.done.percentageContainer.x
#pack .jobs.selected.done.percentageContainer.x -fill x -expand yes
    grid .jobs.selected.currentStorage -row 3 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.filesPercentageTitle -text "Files:"
    grid .jobs.selected.filesPercentageTitle -row 4 -column 0 -sticky "w"
    progressbar .jobs.selected.filesPercentage
    grid .jobs.selected.filesPercentage -row 4 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p
    addModifyTrace ::currentJob(doneFiles) \
      "
        global currentJob

        if {\$currentJob(totalFiles) > 0} \
        {
          set p \[expr {double(\$currentJob(doneFiles))/\$currentJob(totalFiles)}]
          progressbar .jobs.selected.filesPercentage update \$p
        }
      "
    addModifyTrace ::currentJob(totalFiles) \
      "
        global currentJob

        if {\$currentJob(totalFiles) > 0} \
        {
          set p \[expr {double(\$currentJob(doneFiles))/\$currentJob(totalFiles)}]
          progressbar .jobs.selected.filesPercentage update \$p
        }
      "

    label .jobs.selected.bytesPercentageTitle -text "Bytes:"
    grid .jobs.selected.bytesPercentageTitle -row 5 -column 0 -sticky "w"
    progressbar .jobs.selected.bytesPercentage
    grid .jobs.selected.bytesPercentage -row 5 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p
    addModifyTrace ::currentJob(doneBytes) \
      "
        global currentJob

        if {\$currentJob(totalBytes) > 0} \
        {
          set p \[expr {double(\$currentJob(doneBytes))/\$currentJob(totalBytes)}]
          progressbar .jobs.selected.bytesPercentage update \$p
        }
      "
    addModifyTrace ::currentJob(totalBytes) \
      "
        global currentJob

        if {\$currentJob(totalBytes) > 0} \
        {
          set p \[expr {double(\$currentJob(doneBytes))/\$currentJob(totalBytes)}]
          progressbar .jobs.selected.bytesPercentage update \$p
        }
      "

#    grid rowconfigure    .jobs.selected { 0 } -weight 1
    grid columnconfigure .jobs.selected { 4 } -weight 1
  grid .jobs.selected -row 0 -column 0 -sticky "we" -padx 2p -pady 2p

  frame .jobs.list
    mclistbox::mclistbox .jobs.list.data -bg white -labelanchor w -selectmode single -xscrollcommand ".jobs.list.xscroll set" -yscrollcommand ".jobs.list.yscroll set"
    .jobs.list.data column add id                -label "Id"             -width 5
    .jobs.list.data column add state             -label "State"          -width 12
    .jobs.list.data column add archivePartSize   -label "Part size"      -width 8
    .jobs.list.data column add compressAlgortihm -label "Compress"       -width 10
    .jobs.list.data column add cryptAlgorithm    -label "Crypt"          -width 10
    .jobs.list.data column add startTime         -label "Started"        -width 20
    .jobs.list.data column add estimatedRestTime -label "Estimated time" -width 20
    grid .jobs.list.data -row 0 -column 0 -sticky "nswe"
    scrollbar .jobs.list.yscroll -orient vertical -command ".jobs.list.data yview"
    grid .jobs.list.yscroll -row 0 -column 1 -sticky "ns"
    scrollbar .jobs.list.xscroll -orient horizontal -command ".jobs.list.data xview"
    grid .jobs.list.xscroll -row 1 -column 0 -sticky "we"

    grid rowconfigure    .jobs.list { 0 } -weight 1
    grid columnconfigure .jobs.list { 0 } -weight 1
  grid .jobs.list -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p

  frame .jobs.buttons
    button .jobs.buttons.rem -text "Rem (Del)" -command "event generate . <<Event_remJob>>"
    pack .jobs.buttons.rem -side left
  grid .jobs.buttons -row 2 -column 0 -sticky "we" -padx 2p -pady 2p

  bind .jobs.list.data <ButtonRelease-1>           "event generate . <<Event_selectJob>>"
  bind .jobs.list.data <KeyPress-Delete>    "event generate . <<Event_remJob>>"
  bind .jobs.list.data <KeyPress-KP_Delete> "event generate . <<Event_remJob>>"

  grid rowconfigure    .jobs { 1 } -weight 1
  grid columnconfigure .jobs { 0 } -weight 1
pack .jobs -side top -fill both -expand yes -in [$mainWindow.tabs subwidget jobs]

frame .files
  tixTree .files.list -scrollbar both -options \
  {
    hlist.separator "/"
    hlist.columns 4
    hlist.header yes
    hlist.indent 16
  }
 # .files.list subwidget hlist configure -font $Config(Font,ListTitle)
  .files.list subwidget hlist configure -selectmode extended
  .files.list subwidget hlist configure -command "openCloseDirectory [.files.list subwidget hlist]"

  .files.list subwidget hlist header create 0 -itemtype text -text "File"
  .files.list subwidget hlist header create 1 -itemtype text -text "Type"
  .files.list subwidget hlist column width 1 -char 10
  .files.list subwidget hlist header create 2 -itemtype text -text "Size"
  .files.list subwidget hlist column width 2 -char 10
  .files.list subwidget hlist header create 3 -itemtype text -text "Modified"
  .files.list subwidget hlist column width 3 -char 15
  grid .files.list -row 0 -column 0 -sticky "nswe" -padx 2p -pady 2p

  frame .files.buttons
    button .files.buttons.stateNone -text "*" -command "event generate . <<Event_stateNone>>"
    pack .files.buttons.stateNone -side left -fill x -expand yes
    button .files.buttons.stateIncluded -text "+" -command "event generate . <<Event_stateIncluded>>"
    pack .files.buttons.stateIncluded -side left -fill x -expand yes
    button .files.buttons.stateExcluded -text "-" -command "event generate . <<Event_stateExcluded>>"
    pack .files.buttons.stateExcluded -side left -fill x -expand yes
  grid .files.buttons -row 1 -column 0 -sticky "we" -padx 2p -pady 2p

  bind [.files.list subwidget hlist] <BackSpace>            "event generate . <<Event_stateNone>>"
  bind [.files.list subwidget hlist] <Delete>               "event generate . <<Event_stateNone>>"
  bind [.files.list subwidget hlist] <KeyPress-plus>        "event generate . <<Event_stateIncluded>>"
  bind [.files.list subwidget hlist] <KeyPress-KP_Add>      "event generate . <<Event_stateIncluded>>"
  bind [.files.list subwidget hlist] <KeyPress-minus>       "event generate . <<Event_stateExcluded>>"
  bind [.files.list subwidget hlist] <KeyPress-KP_Subtract> "event generate . <<Event_stateExcluded>>"
  bind [.files.list subwidget hlist] <KeyPress-space>       "event generate . <<Event_toggleStateNoneIncludedExcluded>>"

  # fix a bug in tix: end does not use separator-char to detect last entry
  bind [.files.list subwidget hlist] <KeyPress-End> \
    "
     .files.list subwidget hlist yview moveto 1
     .files.list subwidget hlist anchor set \[lindex \[.files.list subwidget hlist info children /\] end\]
     break
    "
  bind [.files.list subwidget hlist] <Button-4> \
    "
     set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
     .files.list subwidget hlist yview scroll -\$n units
    "
  bind [.files.list subwidget hlist] <Button-5> \
    "
     set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
     .files.list subwidget hlist yview scroll +\$n units
    "

  grid rowconfigure    .files { 0 } -weight 1
  grid columnconfigure .files { 0 } -weight 1
pack .files -side top -fill both -expand yes -in [$mainWindow.tabs subwidget files]

frame .excludes
  tixScrolledListBox .excludes.list -scrollbar both -options { listbox.background white }
  grid .excludes.list -row 0 -column 0 -sticky "nswe" -padx 2p -pady 2p

  frame .excludes.buttons
    button .excludes.buttons.add -text "Add (Ins)" -command "event generate . <<Event_addExcludePattern>>"
    pack .excludes.buttons.add -side left
    button .excludes.buttons.rem -text "Rem (Del)" -command "event generate . <<Event_remExcludePattern>>"
    pack .excludes.buttons.rem -side left
  grid .excludes.buttons -row 1 -column 0 -sticky "we" -padx 2p -pady 2p

  bind [.excludes.list subwidget listbox] <KeyPress-Insert>    "event generate . <<Event_addExcludePattern>>"
  bind [.excludes.list subwidget listbox] <KeyPress-KP_Insert> "event generate . <<Event_addExcludePattern>>"
  bind [.excludes.list subwidget listbox] <KeyPress-Delete>    "event generate . <<Event_remExcludePattern>>"
  bind [.excludes.list subwidget listbox] <KeyPress-KP_Delete> "event generate . <<Event_remExcludePattern>>"

  grid rowconfigure    .excludes { 0 } -weight 1
  grid columnconfigure .excludes { 0 } -weight 1
pack .excludes -side top -fill both -expand yes -in [$mainWindow.tabs subwidget excludes]

frame .storage
  label .storage.archivePartSizeTitle -text "Part size:"
  grid .storage.archivePartSizeTitle -row 0 -column 0 -sticky "w" 
  frame .storage.split
    radiobutton .storage.split.unlimted -text "unlimted" -width 6 -anchor w -variable barConfig(archivePartSizeFlag) -value 0
    grid .storage.split.unlimted -row 0 -column 1 -sticky "w" 
    radiobutton .storage.split.size -text "split in" -width 6 -anchor w -variable barConfig(archivePartSizeFlag) -value 1
    grid .storage.split.size -row 0 -column 2 -sticky "w" 
    tixComboBox .storage.split.archivePartSize -variable barConfig(archivePartSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
    grid .storage.split.archivePartSize -row 0 -column 3 -sticky "w" 

   .storage.split.archivePartSize insert end 128M
   .storage.split.archivePartSize insert end 256M
   .storage.split.archivePartSize insert end 512M
   .storage.split.archivePartSize insert end 1G

    grid rowconfigure    .storage.split { 0 } -weight 1
    grid columnconfigure .storage.split { 1 } -weight 1
  grid .storage.split -row 0 -column 1 -sticky "w" -padx 2p -pady 2p
  addEnableTrace ::barConfig(archivePartSizeFlag) 1 .storage.split.archivePartSize

  label .storage.maxTmpDirectorySizeTitle -text "Max. temp. size:"
  grid .storage.maxTmpDirectorySizeTitle -row 1 -column 0 -sticky "w" 
  frame .storage.maxTmpDirectorySize
    radiobutton .storage.maxTmpDirectorySize.unlimted -text "unlimted" -width 6 -anchor w -variable barConfig(maxTmpDirectorySizeFlag) -value 0
    grid .storage.maxTmpDirectorySize.unlimted -row 0 -column 1 -sticky "w" 
    radiobutton .storage.maxTmpDirectorySize.limitto -text "limit to" -width 6 -anchor w -variable barConfig(maxTmpDirectorySizeFlag) -value 1
    grid .storage.maxTmpDirectorySize.limitto -row 0 -column 2 -sticky "w" 
    tixComboBox .storage.maxTmpDirectorySize.size -variable barConfig(maxTmpDirectorySize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
    grid .storage.maxTmpDirectorySize.size -row 0 -column 3 -sticky "w" 

   .storage.maxTmpDirectorySize.size insert end 128M
   .storage.maxTmpDirectorySize.size insert end 256M
   .storage.maxTmpDirectorySize.size insert end 512M
   .storage.maxTmpDirectorySize.size insert end 1G
   .storage.maxTmpDirectorySize.size insert end 2G
   .storage.maxTmpDirectorySize.size insert end 4G
   .storage.maxTmpDirectorySize.size insert end 8G

    grid rowconfigure    .storage.maxTmpDirectorySize { 0 } -weight 1
    grid columnconfigure .storage.maxTmpDirectorySize { 1 } -weight 1
  grid .storage.maxTmpDirectorySize -row 1 -column 1 -sticky "w" -padx 2p -pady 2p
  addEnableTrace ::barConfig(maxTmpDirectorySizeFlag) 1 .storage.maxTmpDirectorySize.size

  radiobutton .storage.typeFileSystem -variable barConfig(storageType) -value "FILESYSTEM"
  grid .storage.typeFileSystem -row 2 -column 0 -sticky "nw" 
  labelframe .storage.fileSystem -text "File system"
    label .storage.fileSystem.fileNameTitle -text "File name:"
    grid .storage.fileSystem.fileNameTitle -row 0 -column 0 -sticky "w" 
    entry .storage.fileSystem.fileName -textvariable barConfig(storageFileName) -bg white
    grid .storage.fileSystem.fileName -row 0 -column 1 -sticky "we" 

    grid rowconfigure    .storage.fileSystem { 0 } -weight 1
    grid columnconfigure .storage.fileSystem { 1 } -weight 1
  grid .storage.fileSystem -row 2 -column 1 -sticky "nswe" -padx 2p -pady 2p
  addEnableTrace ::barConfig(storageType) "FILESYSTEM" .storage.fileSystem.fileNameTitle
  addEnableTrace ::barConfig(storageType) "FILESYSTEM" .storage.fileSystem.fileName

  radiobutton .storage.typeSCP -variable barConfig(storageType) -value "SCP"
  grid .storage.typeSCP -row 3 -column 0 -sticky "nw" 
  labelframe .storage.scp -text "scp"
    label .storage.scp.loginNameTitle -text "Login:" -state disabled
    grid .storage.scp.loginNameTitle -row 0 -column 0 -sticky "w" 
    entry .storage.scp.loginName -textvariable barConfig(storageLoginName) -bg white -state disabled
    grid .storage.scp.loginName -row 0 -column 1 -sticky "we" 

    label .storage.scp.loginPasswordTitle -text "Password:" -state disabled
    grid .storage.scp.loginPasswordTitle -row 0 -column 2 -sticky "w" 
    entry .storage.scp.loginPassword -textvariable barConfig(sshPassword) -bg white -show "*" -state disabled
    grid .storage.scp.loginPassword -row 0 -column 3 -sticky "we" 

    label .storage.scp.hostNameTitle -text "Host:" -state disabled
    grid .storage.scp.hostNameTitle -row 2 -column 0 -sticky "w" 
    entry .storage.scp.hostName -textvariable barConfig(storageHostName) -bg white -state disabled
    grid .storage.scp.hostName -row 2 -column 1 -sticky "we" 

    label .storage.scp.sshPortTitle -text "SSH port:" -state disabled
    grid .storage.scp.sshPortTitle -row 2 -column 2 -sticky "w" 
    tixControl .storage.scp.sshPort -variable barConfig(sshPort) -label "" -labelside right -integer true -min 1 -max 65535 -options { entry.background white } -state disabled
    grid .storage.scp.sshPort -row 2 -column 3 -sticky "we" 

    grid rowconfigure    .storage.scp { 0 1 } -weight 1
    grid columnconfigure .storage.scp { 1 3 } -weight 1
  grid .storage.scp -row 3 -column 1 -sticky "nswe" -padx 2p -pady 2p
  addEnableTrace ::barConfig(storageType) "SCP" .storage.scp.loginNameTitle
  addEnableTrace ::barConfig(storageType) "SCP" .storage.scp.loginName
  addEnableTrace ::barConfig(storageType) "SCP" .storage.scp.loginPasswordTitle
  addEnableTrace ::barConfig(storageType) "SCP" .storage.scp.loginPassword
  addEnableTrace ::barConfig(storageType) "SCP" .storage.scp.hostNameTitle
  addEnableTrace ::barConfig(storageType) "SCP" .storage.scp.hostName
  addEnableTrace ::barConfig(storageType) "SCP" .storage.scp.sshPortTitle
  addEnableTrace ::barConfig(storageType) "SCP" .storage.scp.sshPort

  grid rowconfigure    .storage { 4 } -weight 1
  grid columnconfigure .storage { 1 } -weight 1
pack .storage -side top -fill both -expand yes -in [$mainWindow.tabs subwidget storage]

frame .compressCrypt
  label .compressCrypt.compressAlgorithmTitle -text "Compress:"
  grid .compressCrypt.compressAlgorithmTitle -row 0 -column 0 -sticky "w" 
  tk_optionMenu .compressCrypt.compressAlgorithm barConfig(compressAlgorithm) \
    "none" "zip0" "zip1" "zip2" "zip3" "zip4" "zip5" "zip6" "zip7" "zip8" "zip9" "bzip1" "bzip2" "bzip3" "bzip4" "bzip5" "bzip6" "bzip7" "bzip8" "bzip9"
  grid .compressCrypt.compressAlgorithm -row 0 -column 1 -sticky "w" -padx 2p -pady 2p

  label .compressCrypt.cryptAlgorithmTitle -text "Crypt:"
  grid .compressCrypt.cryptAlgorithmTitle -row 1 -column 0 -sticky "w" 
  tk_optionMenu .compressCrypt.cryptAlgorithm barConfig(cryptAlgorithm) \
    "none" "3des" "cast5" "blowfish" "aes128" "aes192" "aes256" "twofish128" "twofish256"
  grid .compressCrypt.cryptAlgorithm -row 1 -column 1 -sticky "w" -padx 2p -pady 2p

  label .compressCrypt.passwordTitle -text "Password:"
  grid .compressCrypt.passwordTitle -row 2 -column 0 -sticky "w" 
  entry .compressCrypt.password -textvariable barConfig(cryptPassword) -bg white -show "*" -state disabled
  grid .compressCrypt.password -row 2 -column 1 -sticky "we" -padx 2p -pady 2p
  addDisableTrace ::barConfig(cryptAlgorithm) "none" .compressCrypt.password

  grid rowconfigure    .compressCrypt { 3 } -weight 1
  grid columnconfigure .compressCrypt { 1 } -weight 1
pack .compressCrypt -side top -fill both -expand yes -in [$mainWindow.tabs subwidget compressCrypt]

# buttons
frame $mainWindow.buttons
  button $mainWindow.buttons.startJob -text "Start job" -command "event generate . <<Event_addJob>>"
  pack $mainWindow.buttons.startJob -side left -padx 2p

  button $mainWindow.buttons.quit -text "Quit" -command "event generate . <<Event_quit>>"
  pack $mainWindow.buttons.quit -side right
pack $mainWindow.buttons -fill x -padx 2p -pady 2p

bind . <<Event_load>> \
{
  loadConfig "" [.files.list subwidget hlist] [.excludes.list subwidget listbox]
}

bind . <<Event_save>> \
{
  saveConfig $barConfigFileName [.files.list subwidget hlist] [.excludes.list subwidget listbox]
}

bind . <<Event_saveAs>> \
{
  saveConfig "" [.files.list subwidget hlist] [.excludes.list subwidget listbox]
}

bind . <<Event_quit>> \
{
  quit
}

bind . <<Event_stateNone>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    setEntryState [.files.list subwidget hlist] $itemPath "NONE"
  }
}

bind . <<Event_stateIncluded>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    setEntryState [.files.list subwidget hlist] $itemPath "INCLUDED"
  }
}

bind . <<Event_stateExcluded>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    setEntryState [.files.list subwidget hlist] $itemPath "EXCLUDED"
  }
}

bind . <<Event_toggleStateNoneIncludedExcluded>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    toggleEntryIncludedExcluded [.files.list subwidget hlist] $itemPath
  }
}

bind . <<Event_addExcludePattern>> \
{
  addExcludePattern [.excludes.list subwidget listbox]
}

bind . <<Event_remExcludePattern>> \
{
  foreach index [.excludes.list subwidget listbox curselection] \
  {
    remExcludePattern [.excludes.list subwidget listbox] $index
  }
}

bind . <<Event_selectJob>> \
{
  set n [.jobs.list.data curselection]
  if {$n != {}} \
  {
    set currentJob(id) [lindex [lindex [.jobs.list.data get $n $n] 0] 0]
  }
}

bind . <<Event_addJob>> \
{
  addJob [.files.list subwidget hlist] [.excludes.list subwidget listbox] .jobs.list.data
}

bind . <<Event_remJob>> \
{
  set n [.jobs.list.data curselection]
  if {$n != {}} \
  {
    set id [lindex [lindex [.jobs.list.data get $n $n] 0] 0]
    remJob .jobs.list.data $id
  }
}

# config modify trace
trace add variable barConfig write "setConfigModify"

set hostname "localhost"
set port 38523
if {![Server:connect $hostname $port]} \
{
  Dialog:error "Cannot connect to server '$hostname:$port'!"
  exit 1
}

resetBarConfig
updateJobList .jobs.list.data
updateCurrentJob

# read devices
#set commandId [Server:sendCommand "DEVICE_LIST"]
#while {[Server:readResult $commandId errorCode result]} \
#{
#  addDevice [.files.list subwidget hlist] $result
#}
addDevice [.files.list subwidget hlist] "/"

#clearFileList [.files.list subwidget hlist]
#openCloseDirectory [.files.list subwidget hlist] "/"
#openCloseDirectory [.files.list subwidget hlist] "/home"
#setEntryState [.files.list subwidget hlist] "/" "INCLUDED"
#setEntryState [.files.list subwidget hlist] "/boot" "EXCLUDED"
#setEntryState [.files.list subwidget hlist] "/proc" "EXCLUDED"

.excludes.list subwidget listbox insert end "abc"

puts "load config "
loadConfig "test.cfg" [.files.list subwidget hlist] [.excludes.list subwidget listbox]
#saveConfig "test.cfg" [.files.list subwidget hlist] [.excludes.list subwidget listbox]

# end of file
