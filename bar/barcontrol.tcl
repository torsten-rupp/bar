#!/bin/sh
#\
exec wish "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol.tcl,v $
# $Revision: 1.6 $
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
    puts "  - version is correct,"
    puts "  - file is accessable (permission 755 or more),"
    puts "  - directory of file is included in search path of system linker."
  } \
  else \
  {
    puts "Package 'Tix' cannot be found (library libtix*, libTix* not found). Please check if"
    puts "  - 'Tix*' package is installed in '$tcl_pkgPath',"
    puts "  - '$tcl_pkgPath/Tix*' is accessable,"
    puts "  - library libtix* or libTix* exists somewhere in '$tcl_pkgPath'."
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
wm geometry . "800x600"

# ------------------------ internal constants/variables ----------------------

set config(JOB_LIST_UPDATE_TIME)    5000
set config(CURRENT_JOB_UPDATE_TIME) 1000

set server(socketHandle)  -1
set server(lastCommandId) 0

set barConfigFileName     ""
set barConfigModifiedFlag 0

set barConfig(included)               {}
set barConfig(excluded)               {}
set barConfig(storageType)            ""
set barConfig(storageFileName)        ""
set barConfig(storageLoginName)       ""
set barConfig(storageHostName)        ""
set barConfig(archivePartSizeFlag)    0
set barConfig(archivePartSize)        0
set barConfig(maxTmpSizeFlag)         0
set barConfig(maxTmpSize)             0
set barConfig(sshPort)                0
set barConfig(sshPublicKeyFileName)   ""
set barConfig(sshPrivatKeyFileName)   ""
set barConfig(compressAlgorithm)      ""
set barConfig(cryptAlgorithm)         ""
set barConfig(cryptPassword)          ""
set barConfig(skipUnreadable)         1
set barConfig(overwriteArchiveFiles)  0
set barConfig(overwriteFiles)         0

set currentJob(id)                    0
set currentJob(error)                 0
set currentJob(doneFiles)             0
set currentJob(doneBytes)             0
set currentJob(doneBytesShort)        0
set currentJob(doneBytesShortUnit)    "K"
set currentJob(totalFiles)            0
set currentJob(totalBytes)            0
set currentJob(totalBytesShort)       0
set currentJob(totalBytesShortUnit)   "K"
set currentJob(skippedFiles)          0
set currentJob(skippedBytes)          0
set currentJob(skippedBytesShort)     0
set currentJob(skippedBytesShortUnit) "K"
set currentJob(errorFiles)            0
set currentJob(errorBytes)            0
set currentJob(errorBytesShort)       0
set currentJob(errorBytesShortUnit)   "K"
set currentJob(compressionRatio)      0
set currentJob(fileName)              ""
set currentJob(fileDoneBytes)         0
set currentJob(fileTotalBytes)        0
set currentJob(storageName)           ""
set currentJob(storageDoneBytes)      0
set currentJob(storageTotalBytes)     0

set jobListTimerId    0
set currentJobTimerId 0

set fileTreeWidget     ""
set includedListWidget ""
set excludedListWidget ""

# format of data in file list
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

proc Dialog:new { title } \
{
  set id "[info cmdcount]"
  namespace eval "::dialog$id" \
  {
  }

  set handle ".dialog$id"
  catch {destroy $handle }
  toplevel $handle
  wm title $handle $title

  return $handle
}

proc Dialog:delete { handle } \
{
  set id [string range $handle 7 end]
  catch { wm destroy $handle }

  namespace delete "::dialog$id"
}

proc Dialog:show { handle } \
{
  tkwait visibility $handle
  set w [winfo width  $handle]
  set h [winfo height $handle]
  set x [expr {[winfo pointerx $handle]-$w/2}]; if {$x < 0} { set x 0 }
  set y [expr {[winfo pointery $handle]-$h/2}]; if {$y < 0} { set y 0 }
  wm geometry $handle +$x+$y
  raise $handle
  tkwait window $handle
}

proc Dialog:close { handle } \
{
  catch {destroy $handle}
}

proc Dialog:addVariable { handle name value } \
{
  set id [string range $handle 7 end]
  namespace eval "::dialog$id" \
  "
    variable $name \"$value\"
  "
}

proc Dialog:variable { handle name } \
{
  set id [string range $handle 7 end]
  return "::dialog$id\:\:$name"
}

proc Dialog:set { handle name value } \
{
  set id [string range $handle 7 end]
  eval "set ::dialog$id\:\:$name \"$value\""
}

proc Dialog:get { handle name } \
{
  set id [string range $handle 7 end]
  eval "set result \$::dialog$id\:\:$name"

  return $result
}

#***********************************************************************
# Name   : Dialog:select
# Purpose: display select dialog
# Input  : title      - title text
#          message    - message text
#          image      - image or ""
#          buttonList - list of buttons {{<text> [<key>]}...}
#          default    - default button (0..n)
# Output : -
# Return : selected button
# Notes  : -
#***********************************************************************

proc Dialog:select { title message image buttonList {default 0} } \
{
  set handle [Dialog:new $title]
  Dialog:addVariable $handle result -1

  frame $handle.message
    if {$image != ""} \
    {
      label $handle.message.image -image $image
      pack $handle.message.image -side left -fill y
    }
    message $handle.message.text -width 400 -text $message
    pack $handle.message.text -side right -fill both -expand yes -padx 2p -pady 2p
  pack $handle.message -padx 2p -pady 2p

  frame $handle.buttons
   set n 0
   foreach button $buttonList \
   {
     set text [lindex $button 0]
     set key  [lindex $button 1]

     button $handle.buttons.button$n -text $text -command "Dialog:set $handle result $n; Dialog:close $handle"
     pack $handle.buttons.button$n -side left -padx 2p
     bind $handle.buttons.button$n <Return> "$handle.buttons.button$n invoke"
     if {$key != ""} \
     {
       bind $handle.buttons.button$n <$key> "$handle.buttons.button$n invoke"
     }

     incr n
   }
  pack $handle.buttons -side bottom -padx 2p -pady 2p

  focus $handle.buttons.button$default

  Dialog:show $handle

  set result [Dialog:get $handle result]
  Dialog:delete $handle

  return $result
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

  Dialog:select "Error" $message $image {{"Ok" Escape}}
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
  Dialog:select "Info" $message "" {{"Ok" Escape}}
}

#***********************************************************************
# Name   : Dialog:confirm
# Purpose: show confirm-dialog
# Input  : message - message
#          yesText - yes text
#          noText  - no text
# Output : -
# Return : 1 for "yes", 0 for "no"
# Notes  : -
#***********************************************************************

proc Dialog:confirm { message yesText noText } \
{
  return [expr {([Dialog:select "Confirm" $message "" [list [list $yesText] [list $noText Escape]]]==0)?1:0}]
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
      $path.back.text configure -text [format "%.1f%%" [expr {$p*100}]]
      $path.back.fill.text configure -text [format "%.1f%%" [expr {$p*100}]]
      update
    }
  } 
}

proc popupMenu { widget menuItemList } \
{
  tixPopupMenu $widget.popup -title "Command"
  foreach menuItem $menuItemList \
  {
    $widget.popup subwidget menu add command -label [lindex $menuItem 0] -command "event generate $widget <<[lindex $menuItem 1]>>"
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

  set barConfig(storageType)         "FILESYSTEM"
  set barConfig(storageHostName)     ""
  set barConfig(storageLoginName)    ""
  set barConfig(storageFileName)     ""
  set barConfig(archivePartSizeFlag) 0
  set barConfig(archivePartSize)     0
  set barConfig(maxTmpSizeFlag)      0
  set barConfig(maxTmpSize)          0
  set barConfig(sshPassword)         ""
  set barConfig(sshPort)             22
  set barConfig(compressAlgorithm)   "bzip9"
  set barConfig(cryptAlgorithm)      "none"
  set barConfig(cryptPassword)       ""
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
# Name   : stringToBoolean
# Purpose: convert string to boolean value
# Input  : s - string
# Output : -
# Return : 1 or 0 (default 0)
# Notes  : -
#***********************************************************************

proc stringToBoolean { s } \
{
  return [string is true $s]
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

proc Server:readResult { commandId _completeFlag _errorCode _result } \
{
  global server

  upvar $_completeFlag completeFlag
  upvar $_errorCode    errorCode
  upvar $_result       result

  set id -1
  while {($id != $commandId) && ($id != 0)} \
  {
     if {[eof $server(socketHandle)]} { return 0 }
    gets $server(socketHandle) line
#puts "received [clock clicks] [eof $server(socketHandle)]: $line"

    set completeFlag 0
    set errorCode    -1
    set result       ""
    regexp {(\d+)\s+(\d+)\s+(\d+)\s+(.*)} $line * id completeFlag errorCode result
  }
#puts "$completeFlag $errorCode $result"

  return 1
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
    return 0
  }
  if {![Server:readResult $commandId completeFlag localErrorCode result]} \
  {
    return 0
  }
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
  global jobListTimerId config currentJob

  catch {after cancel $jobListTimerId}

  # get current selection
  set selectedId 0
  if {[$jobListWidget curselection] != {}} \
  {
    set n [lindex [$jobListWidget curselection] 0]
    set selectedId [lindex [lindex [$jobListWidget get $n $n] 0] 0]
  }

  # update list
  $jobListWidget delete 0 end
  set commandId [Server:sendCommand "JOB_LIST"]
  while {[Server:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
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
  if     {$selectedId > 0} \
  {
    set n 0
    while {($n < [$jobListWidget index end]) && ($selectedId != [lindex [lindex [$jobListWidget get $n $n] 0] 0])} \
    {
      incr n
    }
    if {$n < [$jobListWidget index end]} \
    {
      $jobListWidget selection set $n
    }
  } \
  elseif {$currentJob(id) == 0} \
  {
    set id [lindex [lindex [$jobListWidget get 0 0] 0] 0]
    if {$id != ""} \
    {
      set currentJob(id) $id
      $jobListWidget selection set 0 0 
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
    if {[Server:readResult $commandId completeFlag errorCode result]} \
    {
      scanx $result "%s %lu %lu %lu %lu %lu %lu %lu %lu %f %S %lu %lu %S %lu %lu" \
        state \
        currentJob(doneFiles) \
        currentJob(doneBytes) \
        currentJob(totalFiles) \
        currentJob(totalBytes) \
        currentJob(skippedFiles) \
        currentJob(skippedBytes) \
        currentJob(errorFiles) \
        currentJob(errorBytes) \
        currentJob(compressionRatio) \
        currentJob(fileName) \
        currentJob(fileDoneBytes) \
        currentJob(fileTotalBytes) \
        currentJob(storageName) \
        currentJob(storageDoneBytes) \
        currentJob(storageTotalBytes)

      if     {$currentJob(doneBytes)    > 1024*1024*1024} { set currentJob(doneBytesShort)    [format "%.1f" [expr {double($currentJob(doneBytes))/(1024*1024*1024)   }]]; set currentJob(doneBytesShortUnit)    "G" } \
      elseif {$currentJob(doneBytes)    >      1024*1024} { set currentJob(doneBytesShort)    [format "%.1f" [expr {double($currentJob(doneBytes))/(     1024*1024)   }]]; set currentJob(doneBytesShortUnit)    "M" } \
      else                                                { set currentJob(doneBytesShort)    [format "%.1f" [expr {double($currentJob(doneBytes))/(          1024)   }]]; set currentJob(doneBytesShortUnit)    "K" }
      if     {$currentJob(totalBytes)   > 1024*1024*1024} { set currentJob(totalBytesShort)   [format "%.1f" [expr {double($currentJob(totalBytes))/(1024*1024*1024)  }]]; set currentJob(totalBytesShortUnit)   "G" } \
      elseif {$currentJob(totalBytes)   >      1024*1024} { set currentJob(totalBytesShort)   [format "%.1f" [expr {double($currentJob(totalBytes))/(     1024*1024)  }]]; set currentJob(totalBytesShortUnit)   "M" } \
      else                                                { set currentJob(totalBytesShort)   [format "%.1f" [expr {double($currentJob(totalBytes))/(          1024)  }]]; set currentJob(totalBytesShortUnit)   "K" }
      if     {$currentJob(skippedBytes) > 1024*1024*1024} { set currentJob(skippedBytesShort) [format "%.1f" [expr {double($currentJob(skippedBytes))/(1024*1024*1024)}]]; set currentJob(skippedBytesShortUnit) "G" } \
      elseif {$currentJob(skippedBytes) >      1024*1024} { set currentJob(skippedBytesShort) [format "%.1f" [expr {double($currentJob(skippedBytes))/(     1024*1024)}]]; set currentJob(skippedBytesShortUnit) "M" } \
      else                                                { set currentJob(skippedBytesShort) [format "%.1f" [expr {double($currentJob(skippedBytes))/(          1024)}]]; set currentJob(skippedBytesShortUnit) "K" }
      if     {$currentJob(errorBytes)   > 1024*1024*1024} { set currentJob(errorBytesShort)   [format "%.1f" [expr {double($currentJob(errorBytes))/(1024*1024*1024)  }]]; set currentJob(errorBytesShortUnit)   "G" } \
      elseif {$currentJob(errorBytes)   >      1024*1024} { set currentJob(errorBytesShort)   [format "%.1f" [expr {double($currentJob(errorBytes))/(     1024*1024)  }]]; set currentJob(errorBytesShortUnit)   "M" } \
      else                                                { set currentJob(errorBytesShort)   [format "%.1f" [expr {double($currentJob(errorBytes))/(          1024)  }]]; set currentJob(errorBytesShortUnit)   "K" }
    }
  } \
  else \
  {
    set currentJob(doneFiles)             0
    set currentJob(doneBytes)             0
    set currentJob(doneBytesShort)        0
    set currentJob(doneBytesShortUnit)    "K"
    set currentJob(totalFiles)            0
    set currentJob(totalBytes)            0
    set currentJob(totalBytesShort)       0
    set currentJob(totalBytesShortUnit)   "K"
    set currentJob(skippedFiles)          0
    set currentJob(skippedBytes)          0
    set currentJob(skippedBytesShort)     0
    set currentJob(skippedBytesShortUnit) "K"
    set currentJob(errorFiles)            0
    set currentJob(errorBytes)            0
    set currentJob(errorBytesShort)       0
    set currentJob(errorBytesShortUnit)   "K"
    set currentJob(compressionRatio)      0
    set currentJob(fileName)              ""
    set currentJob(fileDoneBytes)         ""
    set currentJob(fileTotalBytes)        ""
    set currentJob(storageName)           ""
    set currentJob(storageName)           ""
    set currentJob(storageDoneBytes)      ""
    set currentJob(storageTotalBytes)     ""
  }

  set currentJobTimerId [after $config(CURRENT_JOB_UPDATE_TIME) "updateCurrentJob"]
}

#***********************************************************************
# Name       : itemPathToFileName
# Purpose    : convert item path to file name
# Input      : itemPath - item path
# Output     : -
# Return     : file name
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc itemPathToFileName { itemPath } \
 {
  global fileTreeWidget

  set separator [lindex [$fileTreeWidget configure -separator] 4]
#puts "$itemPath -> #[string range $itemPath [string length $Separator] end]#"

  return [string map [list $separator "/"] $itemPath]
 }

#***********************************************************************
# Name       : fileNameToItemPath
# Purpose    : convert file name to item path
# Input      : fileName - file name
# Output     : -
# Return     : item path
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc fileNameToItemPath { fileName } \
 {
  global fileTreeWidget

  # get separator
  set separator "/"
  catch {set separator [lindex [$fileTreeWidget configure -separator] 4]}

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

proc checkIncluded { fileName } \
{
  global barConfig

  set includedFlag 0
  foreach pattern $barConfig(included) \
  {
    if {($fileName == $pattern) || [string match $pattern $fileName]} \
    {
      set includedFlag 1
      break
    }
  }

  return $includedFlag
}

proc checkExcluded { fileName } \
{
  global barConfig

  set excludedFlag 0
  foreach pattern $barConfig(excluded) \
  {
    if {($fileName == $pattern) || [string match $pattern $fileName]} \
    {
      set excludedFlag 1
      break
    }
  }

  return $excludedFlag
}

#***********************************************************************
# Name   : clearFileList
# Purpose: clear file list
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc clearFileList { } \
{
  global fileTreeWidget images

  foreach itemPath [$fileTreeWidget info children ""] \
  {
    $fileTreeWidget delete offsprings $itemPath
    $fileTreeWidget item configure $itemPath 0 -image $images(folder)
  }
}

#***********************************************************************
# Name   : addDevice
# Purpose: add a device to tree-widget
# Input  : deviceName - file name/directory name
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addDevice { deviceName } \
{
  global fileTreeWidget

  catch {$fileTreeWidget delete entry $deviceName}

  set n 0
  set l [$fileTreeWidget info children ""]
  while {($n < [llength $l]) && (([$fileTreeWidget info data [lindex $l $n]] != {}) || ($deviceName>[lindex $l $n]))} \
  {
    incr n
  }

  set style [tixDisplayStyle imagetext -refwindow $fileTreeWidget]

  $fileTreeWidget add $deviceName -at $n -itemtype imagetext -text $deviceName -image [tix getimage folder] -style $style -data [list "DIRECTORY" "NONE" 0]
  $fileTreeWidget item create $deviceName 1 -itemtype imagetext -style $style
  $fileTreeWidget item create $deviceName 2 -itemtype imagetext -style $style
  $fileTreeWidget item create $deviceName 3 -itemtype imagetext -style $style
}

#***********************************************************************
# Name   : addEntry
# Purpose: add a file/directory/link entry to tree-widget
# Input  : fileName - file name/directory name
#          fileType - FILE|DIRECTORY|LINK
#          fileSize - file size
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addEntry { fileName fileType fileSize } \
{
  global fileTreeWidget barConfig images

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
  set itemPath       [fileNameToItemPath $fileName       ]
  set parentItemPath [fileNameToItemPath $parentDirectory]
#puts "f=$fileName"
#puts "i=$itemPath"
#puts "p=$parentItemPath"

  catch {$fileTreeWidget delete entry $itemPath}

  # create parent entry if it does not exists
  if {($parentItemPath != "") && ![$fileTreeWidget info exists $parentItemPath]} \
  {
    addEntry [file dirname $fileName] "DIRECTORY" 0
  }

  # get excluded flag of entry
  set excludedFlag [checkExcluded $fileName]

  # get styles
  set styleImage     [tixDisplayStyle imagetext -refwindow $fileTreeWidget -anchor w]
  set styleTextLeft  [tixDisplayStyle text      -refwindow $fileTreeWidget -anchor w]
  set styleTextRight [tixDisplayStyle text      -refwindow $fileTreeWidget -anchor e]

   if     {$fileType=="FILE"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath - $SortedFlag -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$fileTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$fileTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     if {!$excludedFlag} { set image $images(file) } else { set image $images(fileExcluded) }
     $fileTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "FILE" "NONE" 0]
     $fileTreeWidget item create $itemPath 1 -itemtype text -text "FILE"    -style $styleTextLeft
     $fileTreeWidget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $fileTreeWidget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
   } \
   elseif {$fileType=="DIRECTORY"} \
   {
#puts "add directory $fileName"
     # find insert position (sort)
     set n 0
     set l [$fileTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && ([lindex [$fileTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") && ($itemPath > [lindex $l $n])} \
     {
       incr n
     }

     # add directory item
     if {!$excludedFlag} { set image $images(folder) } else { set image $images(folderExcluded) }
     $fileTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "DIRECTORY" "NONE" 0]
     $fileTreeWidget item create $itemPath 1 -itemtype text -style $styleTextLeft
     $fileTreeWidget item create $itemPath 2 -itemtype text -style $styleTextLeft
     $fileTreeWidget item create $itemPath 3 -itemtype text -style $styleTextLeft
   } \
   elseif {$fileType=="LINK"} \
   {
#puts "add link $fileName $RealFilename"
     set n 0
     set l [$fileTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$fileTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add link item
     if {!$excludedFlag} { set image $images(link) } else { set image $images(linkExcluded) }
     $fileTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "LINK" "NONE" 0]
     $fileTreeWidget item create $itemPath 1 -itemtype text -text "LINK" -style $styleTextLeft
     $fileTreeWidget item create $itemPath 2 -itemtype text              -style $styleTextLeft
     $fileTreeWidget item create $itemPath 3 -itemtype text              -style $styleTextLeft
   }
}

#***********************************************************************
# Name   : openCloseDirectory
# Purpose: open/close directory
# Input  : itemPath - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc openCloseDirectory { itemPath } \
{
  global fileTreeWidget includedListWidget excludedListWidget images

  # get directory name
  set directoryName [itemPathToFileName $itemPath]

  # check if existing, add if not exists
  if {![$fileTreeWidget info exists $itemPath]} \
  {
    addEntry $directoryName "DIRECTORY" 0
  }

  # check if parent exist and is open, open it if needed
  set parentItemPath [$fileTreeWidget info parent $itemPath]
  if {[$fileTreeWidget info exists $parentItemPath]} \
  {
    set data [$fileTreeWidget info data $parentItemPath]
    if {[lindex $data 2] == 0} \
    {
      openCloseDirectory $parentItemPath
    }
  }

  # get data
  set data [$fileTreeWidget info data $itemPath]
  set type              [lindex $data 0]
  set directoryOpenFlag [lindex $data 2]

  if {$type == "DIRECTORY"} \
  {
    # get open/closed flag

    $fileTreeWidget delete offsprings $itemPath
    if {!$directoryOpenFlag} \
    {
      $fileTreeWidget item configure $itemPath 0 -image $images(folderOpen)

      set fileName [itemPathToFileName $itemPath]
      set commandId [Server:sendCommand "FILE_LIST" $fileName 0]
      while {[Server:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
      {
#puts "add file $result"
        if     {[scanx $result "FILE %d %S" fileSize fileName] == 2} \
        {
          addEntry $fileName "FILE" $fileSize
        } \
        elseif {[scanx $result "DIRECTORY %ld %S" totalSize directoryName] == 2} \
        {
          addEntry $directoryName "DIRECTORY" $totalSize
        } \
        elseif {[scanx $result "LINK %S" linkName] == 1} \
        {
          addEntry $linkName "LINK" 0
        } else {
  internalError "unknown file type in openclosedirectory"
}
      }

      set directoryOpenFlag 1
    } \
    else \
    {
      $fileTreeWidget item configure $itemPath 0 -image $images(folder)

      set directoryOpenFlag 0
    }

    # update data
    lset data 2 $directoryOpenFlag
    $fileTreeWidget entryconfigure $itemPath -data $data
  }
}

#***********************************************************************
# Name   : updateFileTreeStates
# Purpose: update file tree states depending on include/exclude patterns
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc updateFileTreeStates { } \
{
  global fileTreeWidget barConfig images

  set excludedList {}
  set itemPathList [$fileTreeWidget info children ""]
  while {[llength $itemPathList] > 0} \
  {
    set fileName [lindex $itemPathList 0]; set itemPathList [lreplace $itemPathList 0 0]
    set itemPath [fileNameToItemPath $fileName]

    set data [$fileTreeWidget info data $itemPath]
    set type              [lindex $data 0]
    set state             [lindex $data 1]
    set directoryOpenFlag [lindex $data 2]

    # add sub-directories to update
    if {($type == "DIRECTORY") && ($state != "EXCLUDED") && $directoryOpenFlag} \
    {
      foreach z [$fileTreeWidget info children $itemPath] \
      {
        lappend itemPathList $z
      }
    }

    # get excluded flag of entry
    set includedFlag [checkIncluded $fileName]
    set excludedFlag [checkExcluded $fileName]

    # detect new state
    if     {$excludedFlag} \
    {
      set state "EXCLUDED"
    } \
    elseif {($state == "EXCLUDED") && !$excludedFlag} \
    {
      if {$includedFlag} \
      {
        set state "INCLUDED"
      } \
      else \
      {
        set state "NONE"
      }
    }
#puts "update $fileName $includedFlag $excludedFlag: $state"

    # update image and state
    if     {$state == "INCLUDED"} \
    {
      if     {$type == "FILE"} \
      {
        set image $images(fileIncluded)
      } \
      elseif {$type == "DIRECTORY"} \
      {
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
        set image $images(linkIncluded)
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
        if {$directoryOpenFlag} \
        {
          $fileTreeWidget delete offsprings $itemPath
          lset data 2 0
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
    $fileTreeWidget item configure [fileNameToItemPath $fileName] 0 -image $image

    # update data
    lset data 1 $state
    $fileTreeWidget entryconfigure $itemPath -data $data
  }

  return $excludedList
}

#***********************************************************************
# Name   : setEntryState
# Purpose: set entry state
# Input  : itemPath - item path
#          state    - NONE, INCLUDED, EXCLUDED
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc setEntryState { itemPath state } \
{
  global fileTreeWidget barConfig images

  # get file name
  set fileName [itemPathToFileName $itemPath]

  # get data
  set data [$fileTreeWidget info data $itemPath]
  set type              [lindex $data 0]
  set directoryOpenFlag [lindex $data 2]
#puts "$itemPath: $state"
#puts $data

  # get type, exclude flag
  if     {$state == "INCLUDED"} \
  {
    if     {$type == "FILE"} \
    {
      set image $images(fileIncluded)
    } \
    elseif {$type == "DIRECTORY"} \
    {
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
      set image $images(linkIncluded)
    }
    set index [lsearch -sorted -exact $barConfig(excluded) $fileName]; if {$index >= 0} { set barConfig(excluded) [lreplace $barConfig(excluded) $index $index] }
    lappend barConfig(included) $fileName; set barConfig(included) [lsort -uniq $barConfig(included)]
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
      if {$directoryOpenFlag} \
      {
        $fileTreeWidget delete offsprings $itemPath
        lset data 2 0
      }
    } \
    elseif {$type == "LINK"} \
    {
      set image $images(linkExcluded)
    }
    set index [lsearch -sorted -exact $barConfig(included) $fileName]; if {$index >= 0} { set barConfig(included) [lreplace $barConfig(included) $index $index] }
    lappend barConfig(excluded) $fileName; set barConfig(excluded) [lsort -uniq $barConfig(excluded)]
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
    set index [lsearch -sorted -exact $barConfig(included) $fileName]; if {$index >= 0} { set barConfig(included) [lreplace $barConfig(included) $index $index] }
    set index [lsearch -sorted -exact $barConfig(excluded) $fileName]; if {$index >= 0} { set barConfig(excluded) [lreplace $barConfig(excluded) $index $index] }
  }
  $fileTreeWidget item configure $itemPath 0 -image $image

  # update data
  lset data 1 $state
  $fileTreeWidget entryconfigure $itemPath -data $data

  setConfigModify
}

#***********************************************************************
# Name   : toggleEntryIncludedExcluded
# Purpose: toggle entry state: NONE, INCLUDED, EXCLUDED
# Input  : itemPath - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc toggleEntryIncludedExcluded { itemPath } \
{
  global fileTreeWidget

  # get data
  set data [$fileTreeWidget info data $itemPath]
  set type  [lindex $data 0]
  set state [lindex $data 1]

  # set new state
  if {$type == "DIRECTORY"} \
  {
    if     {$state == "NONE"    } { set state "INCLUDED" } \
    elseif {$state == "INCLUDED"} { set state "EXCLUDED" } \
    else                          { set state "NONE"     }
  } \
  else \
  {
    if     {$state == "NONE"} { set state "EXCLUDED" } \
    else                      { set state "NONE"     }
  }

  setEntryState $itemPath $state
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
# Input  : configFileName - config file name or ""
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc loadConfig { configFileName } \
{
  global fileTreeWidget includedListWidget excludedListWidget tk_strictMotif barConfigFileName barConfigModifiedFlag barConfig errorCode

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
  clearFileList
  $includedListWidget delete 0 end
  $excludedListWidget delete 0 end

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
    if {[scanx $line "archive-filename = %S" s] == 1} \
    {
      # archive-filename = <file name>
      if {[regexp {^scp:([^@]*)@([^:]*):(.*)} $s * loginName hostName fileName]} \
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
    if {[scanx $line "archive-part-size = %s" s] == 1} \
    {
      # archive-part-size = <size>
      set barConfig(archivePartSizeFlag) 1
      set barConfig(archivePartSize)     $s
      continue
    }
    if {[scanx $line "max-tmp-size = %s" s] == 1} \
    {
      # max-tmp-size = <size>
      set barConfig(maxTmpSizeFlag) 1
      set barConfig(maxTmpSize)     $s
      continue
    }
    if {[scanx $line "ssh-port = %d" n] == 1} \
    {
      # ssh-port = <port>
      set barConfig(sshPort) $n
      continue
    }
    if {[scanx $line "ssh-public-key = %S" s] == 1} \
    {
      # ssh-public-key = <file name>
      set barConfig(sshPublicKeyFileName) $s
      continue
    }
    if {[scanx $line "ssh-privat-key = %S" s] == 1} \
    {
      # ssh-privat-key = <file name>
      set barConfig(sshPrivatKeyFileName) $s
      continue
    }
    if {[scanx $line "compress-algorithm = %S" s] == 1} \
    {
      # compress-algorithm = <algortihm>
      set barConfig(compressAlgorithm) $s
      continue
    }
    if {[scanx $line "crypt-algorithm = %S" s] == 1} \
    {
      # crypt-algorithm = <algorithm>
      set barConfig(cryptAlgorithm) $s
      continue
    }
    if {[scanx $line "include = %S" s] == 1} \
    {
      # include = <filename|pattern>
      set pattern $s

      # add to include pattern list
      lappend barConfig(included) $pattern; set barConfig(included) [lsort -uniq $barConfig(included)]

      set fileName $pattern
      set itemPath [fileNameToItemPath $fileName]

      # add directory for entry
      if {![$fileTreeWidget info exists $itemPath]} \
      {
        set directoryName [file dirname $fileName]
        set directoryItemPath [fileNameToItemPath $directoryName]
        openCloseDirectory $directoryItemPath
      }

      # set state of entry to "included"
      if {[$fileTreeWidget info exists $itemPath]} \
      {
        setEntryState $itemPath "INCLUDED"
      }
      continue
    }
    if {[scanx $line "exclude = %S" s] == 1} \
    {
      # exclude = <filename|pattern>
      set pattern $s

      # add to exclude pattern list
      lappend barConfig(excluded) $pattern; set barConfig(excluded) [lsort -uniq $barConfig(excluded)]
      continue
    }
    if {[scanx $line "skip-unreadable = %s" s] == 1} \
    {
      # skip-unreadable = [yes|no]
      set barConfig(skipUnreadable) [stringToBoolean $s]
      continue
    }
    if {[scanx $line "overwrite-archive-files = %s" s] == 1} \
    {
      # overwrite-archive-files = [yes|no]
      set barConfig(overWriteArchiveFiles) [stringToBoolean $s]
      continue
    }
    if {[scanx $line "overwrite-files = %s" s] == 1} \
    {
      # overwrite-archive-files = [yes|no]
      set barConfig(overwriteFiles) [stringToBoolean $s]
      continue
    }
puts "unknown $line"
  }

  # close file
  close $handle

  updateFileTreeStates

  set barConfigFileName $configFileName
  clearConfigModify
}

#***********************************************************************
# Name   : saveConfig
# Purpose: saveBAR config into file
# Input  : configFileName - config file name or ""
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc saveConfig { configFileName } \
{
  global includedListWidget excludedListWidget tk_strictMotif barConfigFileName barConfig errorInfo

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
    puts $handle "archive-filename = [escapeString scp:$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)]"
  } \
  else \
  {
    internalError "unknown storage type '$barConfig(storageType)'"
  }
  if {$barConfig(archivePartSizeFlag)} \
  {
    puts $handle "archive-part-size = $barConfig(archivePartSize)"
  }
  if {$barConfig(maxTmpSizeFlag)} \
  {
    puts $handle "max-tmp-size = $barConfig(maxTmpSize)"
  }
  puts $handle "ssh-port = $barConfig(sshPort)"
  puts $handle "ssh-public-key = $barConfig(sshPublicKeyFileName)"
  puts $handle "ssh-privat-key = $barConfig(sshPrivatKeyFileName)"
  puts $handle "compress-algorithm = [escapeString $barConfig(compressAlgorithm)]"
  puts $handle "crypt-algorithm = [escapeString $barConfig(cryptAlgorithm)]"
  foreach pattern [$includedListWidget get 0 end] \
  {
    puts $handle "include = [escapeString $pattern]"
  }
  foreach pattern [$excludedListWidget get 0 end] \
  {
    puts $handle "exclude = [escapeString $pattern]"
  }
  puts $handle "skip-unreadable = $barConfig(skipUnreadable)"
  puts $handle "overwrite-archive-files = $barConfig(overwriteArchiveFiles)"
  puts $handle "overwrite-files = $barConfig(overwriteFiles)"

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

proc quit { } \
{
  global server barConfigModifiedFlag barConfigFileName

  if {$barConfigModifiedFlag} \
  {
    if {[Dialog:confirm "Configuration not saved. Save?" "Save" "Do not save"]} \
    {
      saveConfig $barConfigFileName
    }
  }

  Server:disconnect

  destroy .
}

#***********************************************************************
# Name   : addIncludedPattern
# Purpose: add included pattern
# Input  : pattern - pattern or "" for dialog
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addIncludedPattern { pattern } \
{
  global barConfig

  if {$pattern == ""} \
  {
    # dialog
    set handle [Dialog:new "Add included pattern"]
    Dialog:addVariable $handle result  -1
    Dialog:addVariable $handle pattern ""

    frame $handle.pattern
      label $handle.pattern.title -text "Pattern:"
      grid $handle.pattern.title -row 2 -column 0 -sticky "w"
      entry $handle.pattern.data -width 30 -bg white -textvariable [Dialog:variable $handle pattern]
      grid $handle.pattern.data -row 2 -column 1 -sticky "we"
      bind $handle.pattern.data <Return> "focus $handle.buttons.add"

      grid rowconfigure    $handle.pattern { 0 } -weight 1
      grid columnconfigure $handle.pattern { 1 } -weight 1
    grid $handle.pattern -row 0 -column 0 -sticky "nswe" -padx 3p -pady 3p

    frame $handle.buttons
      button $handle.buttons.add -text "Add" -command "event generate $handle <<Event_add>>"
      pack $handle.buttons.add -side left -padx 2p -pady 2p
      bind $handle.buttons.add <Return> "$handle.buttons.add invoke"
      button $handle.buttons.cancel -text "Cancel" -command "event generate $handle <<Event_cancel>>"
      pack $handle.buttons.cancel -side right -padx 2p -pady 2p
      bind $handle.buttons.cancel <Return> "$handle.buttons.cancel invoke"
    grid $handle.buttons -row 1 -column 0 -sticky "we"

    grid rowconfigure $handle    0 -weight 1
    grid columnconfigure $handle 0 -weight 1

    # bindings
    bind $handle <KeyPress-Escape> "$handle.buttons.cancel invoke"

    bind $handle <<Event_add>> \
     "
      Dialog:set $handle result 1
      Dialog:close $handle
     "
    bind $handle <<Event_cancel>> \
     "
      Dialog:set $handle result 0
      Dialog:close $handle
     "

    focus $handle.pattern.data

    Dialog:show $handle
    set result  [Dialog:get $handle result]
    set pattern [Dialog:get $handle pattern]
    Dialog:delete $handle
    if {($result != 1) || ($pattern == "")} { return }
  }

  # add
  lappend barConfig(included) $pattern; set barConfig(included) [lsort -uniq $barConfig(included)]
  updateFileTreeStates
  setConfigModify
}

#***********************************************************************
# Name   : remIncludedPattern
# Purpose: remove included pattern from widget list
# Input  : pattern - pattern
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc remIncludedPattern { pattern } \
{
  global barConfig

  set index [lsearch -sorted -exact $barConfig(included) $pattern]
  if {$index >= 0} \
  {
    set barConfig(included) [lreplace $barConfig(included) $index $index]
    updateFileTreeStates
    setConfigModify
  }
}

#***********************************************************************
# Name   : addExcludedPattern
# Purpose: add excluded pattern
# Input  : pattern - pattern or "" for dialog
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addExcludedPattern { pattern } \
{
  global barConfig

  if {$pattern == ""} \
  {
    # dialog
    set handle [Dialog:new "Add excluded pattern"]
    Dialog:addVariable $handle result  -1
    Dialog:addVariable $handle pattern ""

    frame $handle.pattern
      label $handle.pattern.title -text "Pattern:"
      grid $handle.pattern.title -row 2 -column 0 -sticky "w"
      entry $handle.pattern.data -width 30 -bg white -textvariable [Dialog:variable $handle pattern]
      grid $handle.pattern.data -row 2 -column 1 -sticky "we"
      bind $handle.pattern.data <Return> "focus $handle.buttons.add"

      grid rowconfigure    $handle.pattern { 0 } -weight 1
      grid columnconfigure $handle.pattern { 1 } -weight 1
    grid $handle.pattern -row 0 -column 0 -sticky "nswe" -padx 3p -pady 3p

    frame $handle.buttons
      button $handle.buttons.add -text "Add" -command "event generate $handle <<Event_add>>"
      pack $handle.buttons.add -side left -padx 2p -pady 2p
      bind $handle.buttons.add <Return> "$handle.buttons.add invoke"
      button $handle.buttons.cancel -text "Cancel" -command "event generate $handle <<Event_cancel>>"
      pack $handle.buttons.cancel -side right -padx 2p -pady 2p
      bind $handle.buttons.cancel <Return> "$handle.buttons.cancel invoke"
    grid $handle.buttons -row 1 -column 0 -sticky "we"

    grid rowconfigure $handle    0 -weight 1
    grid columnconfigure $handle 0 -weight 1

    # bindings
    bind $handle <KeyPress-Escape> "$handle.buttons.cancel invoke"

    bind $handle <<Event_add>> \
     "
      Dialog:set $handle result 1
      Dialog:close $handle
     "
    bind $handle <<Event_cancel>> \
     "
      Dialog:set $handle result 0
      Dialog:close $handle
     "

    focus $handle.pattern.data

    Dialog:show $handle
    set result  [Dialog:get $handle result]
    set pattern [Dialog:get $handle pattern]
    Dialog:delete $handle
    if {($result != 1) || ($pattern == "")} { return }
  }

  # add
  lappend barConfig(excluded) $pattern; set barConfig(excluded) [lsort -uniq $barConfig(excluded)]
  updateFileTreeStates
  setConfigModify
}

#***********************************************************************
# Name   : remExcludedPattern
# Purpose: remove excluded pattern from widget list
# Input  : pattern - pattern
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc remExcludedPattern { pattern } \
{
  global barConfig

  set index [lsearch -sorted -exact $barConfig(excluded) $pattern]
  if {$index >= 0} \
  {
    set barConfig(excluded) [lreplace $barConfig(excluded) $index $index]
    updateFileTreeStates
    setConfigModify
  }
}

#***********************************************************************
# Name   : addJob
# Purpose: add new job
# Input  : jobListWidget - job list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addJob { jobListWidget } \
{
  global includedListWidget excludedListWidget barConfig currentJob

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
    set archiveFileName "scp:$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)"
  } \
  else \
  {
    internalError "unknown storage type '$barConfig(storageType)'"
  }
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "archive-file" $archiveFileName

  # add included directories/files
  foreach pattern [$includedListWidget get 0 end] \
  {
    Server:executeCommand errorCode errorText "ADD_INCLUDE_PATTERN" [escapeString $pattern]
  }

  # add excluded directories/files
  foreach pattern [$excludedListWidget get 0 end] \
  {
    Server:executeCommand errorCode errorText "ADD_EXCLUDE_PATTERN" [escapeString $pattern]
  }

  # set other parameters
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "archive-part-size"       $barConfig(archivePartSize)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "max-tmp-size"            $barConfig(maxTmpSize)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "ssh-port"                $barConfig(sshPort)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "compress-algorithm"      $barConfig(compressAlgorithm)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "crypt-algorithm"         $barConfig(cryptAlgorithm)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "skip-unreadable"         $barConfig(skipUnreadable)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "overwrite-archive-files" $barConfig(overwriteArchiveFiles)
  Server:executeCommand errorCode errorText "SET_CONFIG_VALUE" "overwrite-files"         $barConfig(overwriteFiles)

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
  $mainWindow.menu.file.items add command -label "Load..."    -accelerator "Ctrl-o" -command "event generate . <<Event_load>>"
  $mainWindow.menu.file.items add command -label "Save"       -accelerator "Ctrl-s" -command "event generate . <<Event_save>>"
  $mainWindow.menu.file.items add command -label "Save as..."                       -command "event generate . <<Event_saveAs>>"
  $mainWindow.menu.file.items add separator
  $mainWindow.menu.file.items add command -label "Start"                            -command "event generate . <<Event_start>>"
  $mainWindow.menu.file.items add separator
  $mainWindow.menu.file.items add command -label "Quit"       -accelerator "Ctrl-q" -command "event generate . <<Event_quit>>"
  pack $mainWindow.menu.file -side left

  menubutton $mainWindow.menu.edit -text "Edit" -menu $mainWindow.menu.edit.items -underline 0
  menu $mainWindow.menu.edit.items
  $mainWindow.menu.edit.items add command -label "None"    -accelerator "*" -command "event generate . <<Event_stateNone>>"
  $mainWindow.menu.edit.items add command -label "Include" -accelerator "+" -command "event generate . <<Event_stateIncluded>>"
  $mainWindow.menu.edit.items add command -label "Exclude" -accelerator "-" -command "event generate . <<Event_stateExcluded>>"
  pack $mainWindow.menu.edit -side left
pack $mainWindow.menu -side top -fill x

# window
tixNoteBook $mainWindow.tabs
  $mainWindow.tabs add jobs          -label "Jobs"             -underline -1 -raisecmd { focus .jobs.list }
  $mainWindow.tabs add files         -label "Files"            -underline -1 -raisecmd { focus .files.list }
  $mainWindow.tabs add filters       -label "Filters"          -underline -1 -raisecmd { focus .filters.included }
  $mainWindow.tabs add storage       -label "Storage"          -underline -1
  $mainWindow.tabs add compressCrypt -label "Compress & crypt" -underline -1
#  $mainWindow.tabs add misc          -label "Misc"             -underline -1
pack $mainWindow.tabs -fill both -expand yes -padx 2p -pady 2p

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

      label .jobs.selected.done.separator -text "/"
      grid .jobs.selected.done.separator -row 0 -column 4 -sticky "w" 

      entry .jobs.selected.done.bytesShort -width 6 -textvariable currentJob(doneBytesShort) -justify right -border 0 -state readonly
      grid .jobs.selected.done.bytesShort -row 0 -column 5 -sticky "w" 
      label .jobs.selected.done.bytesShortUnit -width 3 -textvariable currentJob(doneBytesShortUnit)
      grid .jobs.selected.done.bytesShortUnit -row 0 -column 6 -sticky "w"

      label .jobs.selected.done.compressRatioTitle -text "Ratio"
      grid .jobs.selected.done.compressRatioTitle -row 0 -column 7 -sticky "w" 
      entry .jobs.selected.done.compressRatio -width 5 -textvariable currentJob(compressionRatio) -justify right -border 0 -state readonly
      grid .jobs.selected.done.compressRatio -row 0 -column 8 -sticky "w" 
      label .jobs.selected.done.compressRatioPostfix -text "%"
      grid .jobs.selected.done.compressRatioPostfix -row 0 -column 9 -sticky "w" 

      grid rowconfigure    .jobs.selected.done { 0 } -weight 1
      grid columnconfigure .jobs.selected.done { 10 } -weight 1
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

      label .jobs.selected.total.separator -text "/"
      grid .jobs.selected.total.separator -row 0 -column 4 -sticky "w" 

      entry .jobs.selected.total.bytesShort -width 6 -textvariable currentJob(totalBytesShort) -justify right -border 0 -state readonly
      grid .jobs.selected.total.bytesShort -row 0 -column 5 -sticky "w" 
      label .jobs.selected.total.bytesShortUnit -width 3 -textvariable currentJob(totalBytesShortUnit)
      grid .jobs.selected.total.bytesShortUnit -row 0 -column 6 -sticky "w" 

      grid rowconfigure    .jobs.selected.total { 0 } -weight 1
      grid columnconfigure .jobs.selected.total { 7 } -weight 1
    grid .jobs.selected.total -row 1 -column 3 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.currentFileNameTitle -text "File:"
    grid .jobs.selected.currentFileNameTitle -row 2 -column 0 -sticky "w"
    entry .jobs.selected.currentFileName -textvariable currentJob(fileName) -border 0 -state readonly
#entry .jobs.selected.done.percentageContainer.x
#pack .jobs.selected.done.percentageContainer.x -fill x -expand yes
    grid .jobs.selected.currentFileName -row 2 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p

    progressbar .jobs.selected.filePercentage
    grid .jobs.selected.filePercentage -row 3 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p
    addModifyTrace ::currentJob(fileDoneBytes) \
      "
        global currentJob

        if {\$currentJob(fileTotalBytes) > 0} \
        {
          set p \[expr {double(\$currentJob(fileDoneBytes))/\$currentJob(fileTotalBytes)}]
          progressbar .jobs.selected.filePercentage update \$p
        }
      "
    addModifyTrace ::currentJob(fileTotalBytes) \
      "
        global currentJob

        if {\$currentJob(fileTotalBytes) > 0} \
        {
          set p \[expr {double(\$currentJob(fileDoneBytes))/\$currentJob(fileTotalBytes)}]
          progressbar .jobs.selected.filePercentage update \$p
        }
      "

    label .jobs.selected.storageNameTitle -text "Storage:"
    grid .jobs.selected.storageNameTitle -row 4 -column 0 -sticky "w"
    entry .jobs.selected.storageName -textvariable currentJob(storageName) -border 0 -state readonly
    grid .jobs.selected.storageName -row 4 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p

    progressbar .jobs.selected.storagePercentage
    grid .jobs.selected.storagePercentage -row 5 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p
    addModifyTrace ::currentJob(storageDoneBytes) \
      "
        global currentJob

        if {\$currentJob(storageTotalBytes) > 0} \
        {
          set p \[expr {double(\$currentJob(storageDoneBytes))/\$currentJob(storageTotalBytes)}]
          progressbar .jobs.selected.storagePercentage update \$p
        }
      "
    addModifyTrace ::currentJob(storageTotalBytes) \
      "
        global currentJob

        if {\$currentJob(storageTotalBytes) > 0} \
        {
          set p \[expr {double(\$currentJob(storageDoneBytes))/\$currentJob(storageTotalBytes)}]
          progressbar .jobs.selected.storagePercentage update \$p
        }
      "

    label .jobs.selected.totalFilesPercentageTitle -text "Total files:"
    grid .jobs.selected.totalFilesPercentageTitle -row 6 -column 0 -sticky "w"
    progressbar .jobs.selected.totalFilesPercentage
    grid .jobs.selected.totalFilesPercentage -row 6 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p
    addModifyTrace ::currentJob(doneFiles) \
      "
        global currentJob

        if {\$currentJob(totalFiles) > 0} \
        {
          set p \[expr {double(\$currentJob(doneFiles))/\$currentJob(totalFiles)}]
          progressbar .jobs.selected.totalFilesPercentage update \$p
        }
      "
    addModifyTrace ::currentJob(totalFiles) \
      "
        global currentJob

        if {\$currentJob(totalFiles) > 0} \
        {
          set p \[expr {double(\$currentJob(doneFiles))/\$currentJob(totalFiles)}]
          progressbar .jobs.selected.totalFilesPercentage update \$p
        }
      "

    label .jobs.selected.totalBytesPercentageTitle -text "Total bytes:"
    grid .jobs.selected.totalBytesPercentageTitle -row 7 -column 0 -sticky "w"
    progressbar .jobs.selected.totalBytesPercentage
    grid .jobs.selected.totalBytesPercentage -row 7 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p
    addModifyTrace ::currentJob(doneBytes) \
      "
        global currentJob

        if {\$currentJob(totalBytes) > 0} \
        {
          set p \[expr {double(\$currentJob(doneBytes))/\$currentJob(totalBytes)}]
          progressbar .jobs.selected.totalBytesPercentage update \$p
        }
      "
    addModifyTrace ::currentJob(totalBytes) \
      "
        global currentJob

        if {\$currentJob(totalBytes) > 0} \
        {
          set p \[expr {double(\$currentJob(doneBytes))/\$currentJob(totalBytes)}]
          progressbar .jobs.selected.totalBytesPercentage update \$p
        }
      "

#    grid rowconfigure    .jobs.selected { 0 } -weight 1
    grid columnconfigure .jobs.selected { 4 } -weight 1
  grid .jobs.selected -row 0 -column 0 -sticky "we" -padx 2p -pady 2p

  frame .jobs.list
    mclistbox::mclistbox .jobs.list.data -height 1 -bg white -labelanchor w -selectmode single -xscrollcommand ".jobs.list.xscroll set" -yscrollcommand ".jobs.list.yscroll set"
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

  bind .jobs.list.data <ButtonRelease-1>    "event generate . <<Event_selectJob>>"
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

  .files.list subwidget hlist header create 0 -itemtype text -text "File"
  .files.list subwidget hlist header create 1 -itemtype text -text "Type"
  .files.list subwidget hlist column width 1 -char 10
  .files.list subwidget hlist header create 2 -itemtype text -text "Size"
  .files.list subwidget hlist column width 2 -char 10
  .files.list subwidget hlist header create 3 -itemtype text -text "Modified"
  .files.list subwidget hlist column width 3 -char 15
  grid .files.list -row 0 -column 0 -sticky "nswe" -padx 2p -pady 2p
  set fileTreeWidget [.files.list subwidget hlist]

  tixPopupMenu .files.list.popup -title "Command"
  .files.list.popup subwidget menu add command -label "Add include"                            -command ""
  .files.list.popup subwidget menu add command -label "Add exclude"                            -command ""
  .files.list.popup subwidget menu add command -label "Remove include"                         -command ""
  .files.list.popup subwidget menu add command -label "Remove exclude"                         -command ""
  .files.list.popup subwidget menu add command -label "Add include pattern"    -state disabled -command ""
  .files.list.popup subwidget menu add command -label "Add exclude pattern"    -state disabled -command ""
  .files.list.popup subwidget menu add command -label "Remove include pattern" -state disabled -command ""
  .files.list.popup subwidget menu add command -label "Remove exclude pattern" -state disabled -command ""

  proc filesPopupHandler { widget x y } \
  {
    set fileName [$widget nearest $y]
    set extension ""
    regexp {.*(\.[^\.]+)} $fileName * extension

    .files.list.popup subwidget menu entryconfigure 0 -label "Add include '$fileName'"    -command "addIncludedPattern $fileName"
    .files.list.popup subwidget menu entryconfigure 1 -label "Add exclude '$fileName'"    -command "addExcludedPattern $fileName"
    .files.list.popup subwidget menu entryconfigure 2 -label "Remove include '$fileName'" -command "remIncludedPattern $fileName"
    .files.list.popup subwidget menu entryconfigure 3 -label "Remove exclude '$fileName'" -command "remExcludedPattern $fileName"
    if {$extension != ""} \
    {
      .files.list.popup subwidget menu entryconfigure 4 -label "Add include pattern *$extension"    -state normal -command "addIncludedPattern *$extension"
      .files.list.popup subwidget menu entryconfigure 5 -label "Add exclude pattern *$extension"    -state normal -command "addExcludedPattern *$extension"
      .files.list.popup subwidget menu entryconfigure 6 -label "Remove include pattern *$extension" -state normal -command "remIncludedPattern *$extension"
      .files.list.popup subwidget menu entryconfigure 7 -label "Remove exclude pattern *$extension" -state normal -command "remExcludedPattern *$extension"
    } \
    else \
    {
      .files.list.popup subwidget menu entryconfigure 4 -label "Add include pattern -"    -state disabled -command ""
      .files.list.popup subwidget menu entryconfigure 5 -label "Add exclude pattern -"    -state disabled -command ""
      .files.list.popup subwidget menu entryconfigure 6 -label "Remove include pattern -" -state disabled -command ""
      .files.list.popup subwidget menu entryconfigure 7 -label "Remove exclude pattern -" -state disabled -command ""
    }
    .files.list.popup post $widget $x $y
  }

  frame .files.buttons
    button .files.buttons.stateNone -text "*" -command "event generate . <<Event_stateNone>>"
    pack .files.buttons.stateNone -side left -fill x -expand yes
    button .files.buttons.stateIncluded -text "+" -command "event generate . <<Event_stateIncluded>>"
    pack .files.buttons.stateIncluded -side left -fill x -expand yes
    button .files.buttons.stateExcluded -text "-" -command "event generate . <<Event_stateExcluded>>"
    pack .files.buttons.stateExcluded -side left -fill x -expand yes
  grid .files.buttons -row 1 -column 0 -sticky "we" -padx 2p -pady 2p

  bind [.files.list subwidget hlist] <Button-3>    "filesPopupHandler %W %x %y"
  bind [.files.list subwidget hlist] <BackSpace>   "event generate . <<Event_stateNone>>"
  bind [.files.list subwidget hlist] <Delete>      "event generate . <<Event_stateNone>>"
  bind [.files.list subwidget hlist] <plus>        "event generate . <<Event_stateIncluded>>"
  bind [.files.list subwidget hlist] <KP_Add>      "event generate . <<Event_stateIncluded>>"
  bind [.files.list subwidget hlist] <minus>       "event generate . <<Event_stateExcluded>>"
  bind [.files.list subwidget hlist] <KP_Subtract> "event generate . <<Event_stateExcluded>>"
  bind [.files.list subwidget hlist] <space>       "event generate . <<Event_toggleStateNoneIncludedExcluded>>"

  # fix a bug in tix: end does not use separator-char to detect last entry
  bind [.files.list subwidget hlist] <KeyPress-End> \
    "
     .files.list subwidget hlist yview moveto 1
     .files.list subwidget hlist anchor set \[lindex \[.files.list subwidget hlist info children /\] end\]
     break
    "

  # mouse-wheel events
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

frame .filters
  label .filters.includedTitle -text "Included:"
  grid .filters.includedTitle -row 0 -column 0 -sticky "nw"
  tixScrolledListBox .filters.included -height 1 -scrollbar both -options { listbox.background white  }
  grid .filters.included -row 0 -column 1 -sticky "nswe" -padx 2p -pady 2p
  .filters.included subwidget listbox configure -listvariable barConfig(included) -selectmode extended
  set includedListWidget [.filters.included subwidget listbox]

  bind [.filters.included subwidget listbox] <Button-1> ".filters.includedButtons.rem configure -state normal"

  frame .filters.includedButtons
    button .filters.includedButtons.add -text "Add (F5)" -command "event generate . <<Event_addIncludePattern>>"
    pack .filters.includedButtons.add -side left
    button .filters.includedButtons.rem -text "Rem (F6)" -state disabled -command "event generate . <<Event_remIncludePattern>>"
    pack .filters.includedButtons.rem -side left
  grid .filters.includedButtons -row 1 -column 1 -sticky "we" -padx 2p -pady 2p

  bind [.filters.included subwidget listbox] <Insert> "event generate . <<Event_addIncludePattern>>"
  bind [.filters.included subwidget listbox] <Delete> "event generate . <<Event_remIncludePattern>>"

  label .filters.excludedTitle -text "Excluded:"
  grid .filters.excludedTitle -row 2 -column 0 -sticky "nw"
  tixScrolledListBox .filters.excluded -height 1 -scrollbar both -options { listbox.background white }
  grid .filters.excluded -row 2 -column 1 -sticky "nswe" -padx 2p -pady 2p
  .filters.excluded subwidget listbox configure -listvariable barConfig(excluded) -selectmode extended
  set excludedListWidget [.filters.excluded subwidget listbox]

  bind [.filters.excluded subwidget listbox] <Button-1> ".filters.excludedButtons.rem configure -state normal"

  frame .filters.excludedButtons
    button .filters.excludedButtons.add -text "Add (F7)" -command "event generate . <<Event_addExcludePattern>>"
    pack .filters.excludedButtons.add -side left
    button .filters.excludedButtons.rem -text "Rem (F8)" -state disabled -command "event generate . <<Event_remExcludePattern>>"
    pack .filters.excludedButtons.rem -side left
  grid .filters.excludedButtons -row 3 -column 1 -sticky "we" -padx 2p -pady 2p

  bind [.filters.excluded subwidget listbox] <Insert> "event generate . <<Event_addExcludePattern>>"
  bind [.filters.excluded subwidget listbox] <Delete> "event generate . <<Event_remExcludePattern>>"

  label .filters.optionsTitle -text "Options:"
  grid .filters.optionsTitle -row 4 -column 0 -sticky "nw" 
  checkbutton .filters.optionSkipUnreadable -text "skip unreable files" -variable barConfig(skipUnreadable)
  grid .filters.optionSkipUnreadable -row 4 -column 1 -sticky "nw" 

  grid rowconfigure    .filters { 0 2 } -weight 1
  grid columnconfigure .filters { 1 } -weight 1
pack .filters -side top -fill both -expand yes -in [$mainWindow.tabs subwidget filters]

frame .storage
  label .storage.archivePartSizeTitle -text "Part size:"
  grid .storage.archivePartSizeTitle -row 0 -column 0 -sticky "w" 
  frame .storage.split
    radiobutton .storage.split.unlimited -text "unlimited" -anchor w -variable barConfig(archivePartSizeFlag) -value 0
    grid .storage.split.unlimited -row 0 -column 1 -sticky "w" 
    radiobutton .storage.split.size -text "split in" -width 8 -anchor w -variable barConfig(archivePartSizeFlag) -value 1
    grid .storage.split.size -row 0 -column 2 -sticky "w" 
    tixComboBox .storage.split.archivePartSize -variable barConfig(archivePartSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
    grid .storage.split.archivePartSize -row 0 -column 3 -sticky "w" 

   .storage.split.archivePartSize insert end 32M
   .storage.split.archivePartSize insert end 64M
   .storage.split.archivePartSize insert end 128M
   .storage.split.archivePartSize insert end 256M
   .storage.split.archivePartSize insert end 512M
   .storage.split.archivePartSize insert end 1G

    grid rowconfigure    .storage.split { 0 } -weight 1
    grid columnconfigure .storage.split { 1 } -weight 1
  grid .storage.split -row 0 -column 1 -sticky "w" -padx 2p -pady 2p
  addEnableTrace ::barConfig(archivePartSizeFlag) 1 .storage.split.archivePartSize

  label .storage.maxTmpSizeTitle -text "Max. temp. size:"
  grid .storage.maxTmpSizeTitle -row 1 -column 0 -sticky "w" 
  frame .storage.maxTmpSize
    radiobutton .storage.maxTmpSize.unlimited -text "unlimited" -anchor w -variable barConfig(maxTmpSizeFlag) -value 0
    grid .storage.maxTmpSize.unlimited -row 0 -column 1 -sticky "w" 
    radiobutton .storage.maxTmpSize.limitto -text "limit to" -width 8 -anchor w -variable barConfig(maxTmpSizeFlag) -value 1
    grid .storage.maxTmpSize.limitto -row 0 -column 2 -sticky "w" 
    tixComboBox .storage.maxTmpSize.size -variable barConfig(maxTmpSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
    grid .storage.maxTmpSize.size -row 0 -column 3 -sticky "w" 

   .storage.maxTmpSize.size insert end 32M
   .storage.maxTmpSize.size insert end 64M
   .storage.maxTmpSize.size insert end 128M
   .storage.maxTmpSize.size insert end 256M
   .storage.maxTmpSize.size insert end 512M
   .storage.maxTmpSize.size insert end 1G
   .storage.maxTmpSize.size insert end 2G
   .storage.maxTmpSize.size insert end 4G
   .storage.maxTmpSize.size insert end 8G

    grid rowconfigure    .storage.maxTmpSize { 0 } -weight 1
    grid columnconfigure .storage.maxTmpSize { 1 } -weight 1
  grid .storage.maxTmpSize -row 1 -column 1 -sticky "w" -padx 2p -pady 2p
  addEnableTrace ::barConfig(maxTmpSizeFlag) 1 .storage.maxTmpSize.size

  label .storage.optionOverwriteArchiveFilesTitle -text "Options:"
  grid .storage.optionOverwriteArchiveFilesTitle -row 2 -column 0 -sticky "w" 
  checkbutton .storage.optionOverwriteArchiveFiles -text "overwrite archive files" -variable barConfig(overwriteArchiveFiles)
  grid .storage.optionOverwriteArchiveFiles -row 2 -column 1 -sticky "w" 

  label .storage.destintationTitle -text "Destination:"
  grid .storage.destintationTitle -row 3 -column 0 -sticky "nw" 
  frame .storage.destintation
    radiobutton .storage.destintation.typeFileSystem -variable barConfig(storageType) -value "FILESYSTEM"
    grid .storage.destintation.typeFileSystem -row 0 -column 0 -sticky "nw" 
    labelframe .storage.destintation.fileSystem -text "File system"
      label .storage.destintation.fileSystem.fileNameTitle -text "File name:"
      grid .storage.destintation.fileSystem.fileNameTitle -row 0 -column 0 -sticky "w" 
      entry .storage.destintation.fileSystem.fileName -textvariable barConfig(storageFileName) -bg white
      grid .storage.destintation.fileSystem.fileName -row 0 -column 1 -sticky "we" 

      grid rowconfigure    .storage.destintation.fileSystem { 0 } -weight 1
      grid columnconfigure .storage.destintation.fileSystem { 1 } -weight 1
    grid .storage.destintation.fileSystem -row 0 -column 1 -sticky "nswe" -padx 2p -pady 2p
    addEnableTrace ::barConfig(storageType) "FILESYSTEM" .storage.destintation.fileSystem.fileNameTitle
    addEnableTrace ::barConfig(storageType) "FILESYSTEM" .storage.destintation.fileSystem.fileName

    radiobutton .storage.destintation.typeSCP -variable barConfig(storageType) -value "SCP"
    grid .storage.destintation.typeSCP -row 1 -column 0 -sticky "nw" 
    labelframe .storage.destintation.scp -text "scp"
      label .storage.destintation.scp.fileNameTitle -text "File name:"
      grid .storage.destintation.scp.fileNameTitle -row 0 -column 0 -sticky "w" 
      entry .storage.destintation.scp.fileName -textvariable barConfig(storageFileName) -bg white
      grid .storage.destintation.scp.fileName -row 0 -column 1 -columnspan 5 -sticky "we" 

      label .storage.destintation.scp.loginNameTitle -text "Login:" -state disabled
      grid .storage.destintation.scp.loginNameTitle -row 1 -column 0 -sticky "w" 
      entry .storage.destintation.scp.loginName -textvariable barConfig(storageLoginName) -bg white -state disabled
      grid .storage.destintation.scp.loginName -row 1 -column 1 -sticky "we" 

  #    label .storage.destintation.scp.loginPasswordTitle -text "Password:" -state disabled
  #    grid .storage.destintation.scp.loginPasswordTitle -row 0 -column 2 -sticky "w" 
  #    entry .storage.destintation.scp.loginPassword -textvariable barConfig(sshPassword) -bg white -show "*" -state disabled
  #    grid .storage.destintation.scp.loginPassword -row 0 -column 3 -sticky "we" 

      label .storage.destintation.scp.hostNameTitle -text "Host:" -state disabled
      grid .storage.destintation.scp.hostNameTitle -row 1 -column 2 -sticky "w" 
      entry .storage.destintation.scp.hostName -textvariable barConfig(storageHostName) -bg white -state disabled
      grid .storage.destintation.scp.hostName -row 1 -column 3 -sticky "we" 

      label .storage.destintation.scp.sshPortTitle -text "SSH port:" -state disabled
      grid .storage.destintation.scp.sshPortTitle -row 1 -column 4 -sticky "w" 
      tixControl .storage.destintation.scp.sshPort -variable barConfig(sshPort) -label "" -labelside right -integer true -min 1 -max 65535 -options { entry.background white } -state disabled
      grid .storage.destintation.scp.sshPort -row 1 -column 5 -sticky "we" 

      label .storage.destintation.scp.sshPublicKeyFileNameTitle -text "SSH public key:" -state disabled
      grid .storage.destintation.scp.sshPublicKeyFileNameTitle -row 2 -column 0 -sticky "w" 
      entry .storage.destintation.scp.sshPublicKeyFileName -textvariable barConfig(sshPublicKeyFileName) -bg white -state disabled
      grid .storage.destintation.scp.sshPublicKeyFileName -row 2 -column 1 -columnspan 5 -sticky "we" 

      label .storage.destintation.scp.sshPrivatKeyFileNameTitle -text "SSH privat key:" -state disabled
      grid .storage.destintation.scp.sshPrivatKeyFileNameTitle -row 3 -column 0 -sticky "w" 
      entry .storage.destintation.scp.sshPrivatKeyFileName -textvariable barConfig(sshPrivatKeyFileName) -bg white -state disabled
      grid .storage.destintation.scp.sshPrivatKeyFileName -row 3 -column 1 -columnspan 5 -sticky "we" 

#      grid rowconfigure    .storage.destintation.scp { } -weight 1
      grid columnconfigure .storage.destintation.scp { 1 3 5 } -weight 1
    grid .storage.destintation.scp -row 1 -column 1 -sticky "nswe" -padx 2p -pady 2p
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.fileNameTitle
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.fileName
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.loginNameTitle
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.loginName
  #  addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.loginPasswordTitle
  #  addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.loginPassword
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.hostNameTitle
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.hostName
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.sshPortTitle
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.sshPort
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.sshPublicKeyFileNameTitle
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.sshPublicKeyFileName
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.sshPrivatKeyFileNameTitle
    addEnableTrace ::barConfig(storageType) "SCP" .storage.destintation.scp.sshPrivatKeyFileName

    grid rowconfigure    .storage.destintation { 0 } -weight 1
    grid columnconfigure .storage.destintation { 1 } -weight 1
  grid .storage.destintation -row 3 -column 1 -sticky "we" -padx 2p -pady 2p

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
    "none" "3DES" "CAST5" "BLOWFISH" "AES128" "AES192" "AES256" "TWOFISH128" "TWOFISH256"
  grid .compressCrypt.cryptAlgorithm -row 1 -column 1 -sticky "w" -padx 2p -pady 2p

#  label .compressCrypt.passwordTitle -text "Password:"
#  grid .compressCrypt.passwordTitle -row 2 -column 0 -sticky "w" 
#  entry .compressCrypt.password -textvariable barConfig(cryptPassword) -bg white -show "*" -state disabled
#  grid .compressCrypt.password -row 2 -column 1 -sticky "we" -padx 2p -pady 2p
#  addDisableTrace ::barConfig(cryptAlgorithm) "none" .compressCrypt.password

  grid rowconfigure    .compressCrypt { 3 } -weight 1
  grid columnconfigure .compressCrypt { 1 } -weight 1
pack .compressCrypt -side top -fill both -expand yes -in [$mainWindow.tabs subwidget compressCrypt]

#frame .misc
#  label .misc.optionsTitle -text "Options:"
#  grid .misc.optionsTitle -row 0 -column 0 -sticky "nw" 
#  checkbutton .misc.optionSkipUnreadable -text "skip unreable files" -variable barConfig(skipUnreadable)
#  grid .misc.optionSkipUnreadable -row 0 -column 1 -sticky "nw" 
#  checkbutton .misc.optionOverwriteFiles -text "overwrite files" -variable barConfig(overwriteFiles)
#  grid .misc.optionOverwriteFiles -row 2 -column 1 -sticky "nw" 

#  grid rowconfigure    .misc { 1 } -weight 1
#  grid columnconfigure .misc { 2 } -weight 1
#pack .misc -side top -fill both -expand yes -in [$mainWindow.tabs subwidget misc]

# buttons
frame $mainWindow.buttons
  button $mainWindow.buttons.startJob -text "Start job" -command "event generate . <<Event_addJob>>"
  pack $mainWindow.buttons.startJob -side left -padx 2p

  button $mainWindow.buttons.quit -text "Quit" -command "event generate . <<Event_quit>>"
  pack $mainWindow.buttons.quit -side right
pack $mainWindow.buttons -fill x -padx 2p -pady 2p

.files.list subwidget hlist configure -command "openCloseDirectory"

bind . <Control-o> "event generate . <<Event_load>>"
bind . <Control-s> "event generate . <<Event_save>>"
bind . <Control-q> "event generate . <<Event_quit>>"

bind . <F5> "event generate . <<Event_addIncludePattern>>"
bind . <F6> "event generate . <<Event_remIncludePattern>>"
bind . <F7> "event generate . <<Event_addExcludePattern>>"
bind . <F8> "event generate . <<Event_remExcludePattern>>"

bind . <<Event_load>> \
{
  loadConfig ""
}

bind . <<Event_save>> \
{
  saveConfig $barConfigFileName
}

bind . <<Event_saveAs>> \
{
  saveConfig ""
}

bind . <<Event_quit>> \
{
  quit
}

bind . <<Event_stateMenu>> \
{
puts 222
  .files.list.popup post %W %x %y

}

bind . <<Event_stateNone>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    setEntryState $itemPath "NONE"
  }
}

bind . <<Event_stateIncluded>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    setEntryState $itemPath "INCLUDED"
  }
}

bind . <<Event_stateExcluded>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    setEntryState $itemPath "EXCLUDED"
  }
}

bind . <<Event_toggleStateNoneIncludedExcluded>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    toggleEntryIncludedExcluded $itemPath
  }
}

bind . <<Event_addIncludePattern>> \
{
  addIncludedPattern ""
}

bind . <<Event_remIncludePattern>> \
{
  set patternList {}
  foreach index [.filters.included subwidget listbox curselection] \
  {
    lappend patternList [.filters.included subwidget listbox get $index]
  }
  foreach pattern $patternList \
  {
    remIncludedPattern $pattern
  }
  .filters.included subwidget listbox selection clear 0 end
  .filters.includedButtons.rem configure -state disabled
}

bind . <<Event_addExcludePattern>> \
{
  addExcludedPattern ""
}

bind . <<Event_remExcludePattern>> \
{
  set patternList {}
  foreach index [.filters.excluded subwidget listbox curselection] \
  {
    lappend patternList [.filters.excluded subwidget listbox get $index]
  }
  foreach pattern $patternList \
  {
    remExcludedPattern $pattern
  }
  .filters.excluded subwidget listbox selection clear 0 end
  .filters.excludedButtons.rem configure -state disabled
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
  addJob .jobs.list.data
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
update

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
#while {[Server:readResult $commandId completeFlag errorCode result]} \
#{
#  addDevice $result
#}
addDevice "/"

#clearFileList [.files.list subwidget hlist]
#openCloseDirectory "/"
#openCloseDirectory "/home"
#setEntryState [.files.list subwidget hlist] "/" "INCLUDED"
#setEntryState [.files.list subwidget hlist] "/boot" "EXCLUDED"
#setEntryState [.files.list subwidget hlist] "/proc" "EXCLUDED"

.filters.excluded subwidget listbox insert end "abc"

loadConfig "test.cfg"
#saveConfig "test.cfg"

#lappend barConfig(excluded) "*.avi"
#updateFileTreeStates

# end of file
