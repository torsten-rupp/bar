#!/bin/sh
#\
exec tclsh "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol.tcl,v $
# $Revision: 1.22 $
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
lappend auto_path /usr/local/lib
lappend auto_path $basePath
lappend auto_path tcltk-lib
lappend auto_path $env(HOME)/sources/tcl-lib
lappend auto_path $env(HOME)/sources/tcltk-lib

# load packages
catch {package require tls}
package require scanx
if {[info exists tk_version]} \
{
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
  package require mclistbox
}

# ---------------------------- constants/variables ---------------------------

set DEFAULT_PORT     38523
set DEFAULT_TLS_PORT 38524

# --------------------------------- includes ---------------------------------

# ------------------------ internal constants/variables ----------------------

# configuration
set barControlConfig(serverHostName)       "localhost"
set barControlConfig(serverPort)           $DEFAULT_PORT
set barControlConfig(serverPassword)       ""
set barControlConfig(serverTLSPort)        $DEFAULT_TLS_PORT
set barControlConfig(serverCAFileName)     "$env(HOME)/.bar/bar-ca.pem"
set barControlConfig(jobListUpdateTime)    5000
set barControlConfig(currentJobUpdateTime) 1000

# global settings
set configFileName  ""
set listFlag        0
set startFlag       0
set fullFlag        0
set incrementalFlag 0
set abortId         0
set quitFlag        0
set guiMode         0

set passwordObfuscator [format "%c%c%c%c%c%c%c%c" [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}]]

# command id counter
set lastCommandId    0

# server variables
set server(socketHandle)   -1
set server(authorizedFlag) 0

# BAR configuration
set barConfigFileName     ""
set barConfigModifiedFlag 0

set barConfig(name)                            ""
set barConfig(included)                        {}
set barConfig(excluded)                        {}
set barConfig(storageType)                     ""
set barConfig(storageFileName)                 ""
set barConfig(storageLoginName)                ""
set barConfig(storageHostName)                 ""
set barConfig(storageDeviceName)               ""
set barConfig(archivePartSizeFlag)             0
set barConfig(archivePartSize)                 0
set barConfig(maxTmpSizeFlag)                  0
set barConfig(maxTmpSize)                      0
set barConfig(storageMode)                     "NORMAL"
set barConfig(incrementalListFileName)         ""
set barConfig(maxBandWidthFlag)                0
set barConfig(maxBandWidth)                    0
set barConfig(sshPort)                         0
set barConfig(sshPublicKeyFileName)            ""
set barConfig(sshPrivatKeyFileName)            ""
set barConfig(compressAlgorithm)               ""
set barConfig(cryptAlgorithm)                  ""
set barConfig(cryptPasswordMode)               "DEFAULT"
set barConfig(cryptPassword)                   ""
set barConfig(cryptPasswordVerify)             ""
set barConfig(destinationDirectoryName)        ""
set barConfig(destinationStripCount)           0
set barConfig(volumeSize)                      0
set barConfig(skipUnreadableFlag)              1
set barConfig(skipNotWritableFlag)             1
set barConfig(overwriteArchiveFilesFlag)       0
set barConfig(overwriteFilesFlag)              0
set barConfig(errorCorrectionCodesFlag)        1

set currentJob(id)                             0
set currentJob(error)                          0
set currentJob(doneFiles)                      0
set currentJob(doneBytes)                      0
set currentJob(doneBytesShort)                 0
set currentJob(doneBytesShortUnit)             "KBytes"
set currentJob(totalFiles)                     0
set currentJob(totalBytes)                     0
set currentJob(totalBytesShort)                0
set currentJob(totalBytesShortUnit)            "KBytes"
set currentJob(skippedFiles)                   0
set currentJob(skippedBytes)                   0
set currentJob(skippedBytesShort)              0
set currentJob(skippedBytesShortUnit)          "KBytes"
set currentJob(errorFiles)                     0
set currentJob(errorBytes)                     0
set currentJob(errorBytesShort)                0
set currentJob(errorBytesShortUnit)            "KBytes"
set currentJob(filesPerSecond)                 0
set currentJob(bytesPerSecond)                 0
set currentJob(bytesPerSecondShort)            0
set currentJob(bytesPerSecondShortUnit)        "KBytes/s"
set currentJob(archiveBytes)                   0
set currentJob(archiveBytesShort)              0
set currentJob(archiveBytesShortUnit)          "KBytes"
set currentJob(storageBytesPerSecond)          0
set currentJob(storageBytesPerSecondShort)     0
set currentJob(storageBytesPerSecondShortUnit) "KBytes/s"
set currentJob(compressionRatio)               0  
set currentJob(fileName)                       "" 
set currentJob(fileDoneBytes)                  0  
set currentJob(fileTotalBytes)                 0  
set currentJob(storageName)                    "" 
set currentJob(storageDoneBytes)               0  
set currentJob(storageTotalBytes)              0  
set currentJob(storageTotalBytesShort)         0
set currentJob(storageTotalBytesShortUnit)     "KBytes"
set currentJob(volumeNumber)                   0
set currentJob(volumeProgress)                 0.0
set currentJob(requestedVolumeNumber)          0
set currentJob(requestedVolumeNumberDialogFlag) 0
set currentJob(message)                        ""

# misc.
set jobListTimerId    0
set currentJobTimerId 0

set backupFilesTreeWidget    ""
set backupIncludedListWidget ""
set backupExcludedListWidget ""

set restoreFilesTreeWidget ""
set restoreIncludedListWidget ""
set restoreExcludedListWidget ""

# format of data in file list
#  {<type> [NONE|INCLUDED|EXCLUDED] <directory open flag>}

if {[info exists tk_version]} \
{
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
}

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
# Name   : Dialog:...
# Purpose: basic dialog functions
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc Dialog:new { } \
{
  set id "[info cmdcount]"
  namespace eval "::dialog$id" {}

  set handle ".dialog$id"
  catch {destroy $handle }

  return $handle
}

proc Dialog:window { title } \
{
  set handle [Dialog:new]

  toplevel $handle
  wm title $handle $title

  return $handle
}

proc Dialog:delete { handle } \
{
  set id [string range $handle 7 end]
  catch { destroy $handle }
  update

  namespace delete "::dialog$id"
}

proc Dialog:show { handle } \
{
  set xBorderDistance  0
  set yBorderDistance 80

  tkwait visibility $handle
  set w [winfo width  $handle]
  set h [winfo height $handle]
  set screenWidth  [winfo screenwidth  .]
  set screenHeight [winfo screenheight .]
  set x [expr {[winfo pointerx $handle]-$w/2}]; if {$x < $xBorderDistance} { set x $xBorderDistance }; if {[expr {$x+$w}] > [expr {$screenWidth -$xBorderDistance}]} { set x [expr {$screenWidth -$w-$xBorderDistance}] }
  set y [expr {[winfo pointery $handle]-$h/2}]; if {$y < $yBorderDistance} { set y $yBorderDistance }; if {[expr {$y+$h}] > [expr {$screenHeight-$yBorderDistance}]} { set y [expr {$screenHeight-$h-$yBorderDistance}] }
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
    variable $name \"[string map {\" \\\"} $value]\"
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
  eval "set ::dialog$id\:\:$name \"[string map {\" \\\"} $value]\""
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
  set handle [Dialog:window $title]
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

  Dialog:select "Error" $message $image {{"OK" Escape}}
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
  Dialog:select "Info" $message "" {{"OK" Escape}}
}

#***********************************************************************
# Name   : Dialog:query
# Purpose: show query-dialog
# Input  : title   - title text
#          message - message
#          yesText - yes text
#          noText  - no text
# Output : -
# Return : 1 for "yes", 0 for "no"
# Notes  : -
#***********************************************************************

proc Dialog:query { title message yesText noText } \
{
  return [expr {([Dialog:select $title $message "" [list [list $yesText] [list $noText Escape]]]==0)?1:0}]
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
  return [Dialog:query "Confirm" $message $yesText $noText]
}

#***********************************************************************
# Name   : Dialog:password
# Purpose: password dialog
# Input  : text       - text to display
#          verifyFlag - 1 for verify password input, 0 otherwise
# Output : -
# Return : password or ""
# Notes  : -
#***********************************************************************

proc Dialog:password { text verifyFlag } \
{
  set handle [Dialog:window "Enter password"]
  Dialog:addVariable $handle result   -1
  Dialog:addVariable $handle password ""

  frame $handle.password
    label $handle.password.title -text "$text:"
    grid $handle.password.title -row 0 -column 0 -sticky "w"
    if {$verifyFlag} \
    {
      entry $handle.password.data -textvariable [Dialog:variable $handle password] -bg white -show "*" -validate key -validatecommand \
        "
         if {\$[Dialog:variable $handle passwordVerify] == \"%P\"} \
         {
           $handle.buttons.ok configure -state normal
         } \
         else \
         {
           $handle.buttons.ok configure -state disabled
         }
         return 1
        "
      bind $handle.password.data <Return> "focus $handle.password.dataVerify"
    } \
    else \
    {
      entry $handle.password.data -textvariable [Dialog:variable $handle password] -bg white -show "*" -validate key -validatecommand \
        "
         if {\[string length %P\] > 0} \
         {
           $handle.buttons.ok configure -state normal
         } \
         else \
         {
           $handle.buttons.ok configure -state disabled
         }
         return 1
        "
      bind $handle.password.data <Return> "focus $handle.buttons.ok"
    }
    grid $handle.password.data  -row 0 -column 1 -sticky "we" -padx 2p -pady 2p

    if {$verifyFlag} \
    {
      label $handle.password.titleVerify -text "Verify:"
      grid $handle.password.titleVerify -row 1 -column 0 -sticky "w"
      entry $handle.password.dataVerify -textvariable [Dialog:variable $handle passwordVerify] -bg white -show "*" -validate key -validatecommand \
       "
        if {\$[Dialog:variable $handle password] == \"%P\"} \
        {
          $handle.buttons.ok configure -state normal
        } \
        else \
        {
          $handle.buttons.ok configure -state disabled
        }
        return 1
       "
      grid $handle.password.dataVerify -row 1 -column 1 -sticky "we" -padx 2p -pady 2p
      bind $handle.password.dataVerify <Return> "focus $handle.buttons.ok"
    }

    grid columnconfigure $handle.password { 1 } -weight 1
  pack $handle.password -padx 2p -pady 2p -fill x

  frame $handle.buttons
    button $handle.buttons.ok -text "OK" -state disabled -command "Dialog:set $handle result 1; Dialog:close $handle"
    pack $handle.buttons.ok -side left -padx 2p
    bind $handle.buttons.ok <Return> "$handle.buttons.ok invoke"
    button $handle.buttons.cancel -text "Cancel" -command "Dialog:set $handle result 0; Dialog:close $handle"
    pack $handle.buttons.cancel -side right -padx 2p
    bind $handle.buttons.cancel <Return> "$handle.buttons.cancel invoke"
  pack $handle.buttons -side bottom -fill x -padx 2p -pady 2p

  bind $handle <Escape> "$handle.buttons.cancel invoke"

  focus $handle.password.data

  Dialog:show $handle

  set result   [Dialog:get $handle result  ]
  set password [Dialog:get $handle password]
  Dialog:delete $handle
  if {$result != 1} { return "" }

  return $password
}

#***********************************************************************
# Name   : Dialog:progressbar
# Purpose: progress bar
# Input  : path - widget path
#          args - optional arguments
# Output : -
# Return : -
# Notes  : optional
#            <path> update <value>
#***********************************************************************

proc Dialog:progressbar { path args } \
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

#***********************************************************************
# Name   : Dialog:fileSelector
# Purpose: file selector
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc Dialog:fileSelector { title fileName fileTypesList } \
{
  proc Dialog:fileSelector:selectFile { handle fileName } \
  {
    Dialog:set $handle result   1
    Dialog:set $handle fileName $fileName
    Dialog:close $handle
  }

  set handle [Dialog:new]
  Dialog:addVariable $handle result   -1
  Dialog:addVariable $handle fileName ""

  tixExFileSelectDialog $handle -title "xx"
  $handle configure -title $title -command "Dialog:fileSelector:selectFile $handle"
  $handle subwidget fsbox configure -filetypes $fileTypesList -pattern [file tail $fileName]
  if {[file dirname $fileName]!=""} { $handle subwidget fsbox configure -dir [file dirname $fileName] }
  $handle popup

  # fix missing scroll-wheel in directory list widget
  bind $handle <Escape> "destroy $handle"
  bind [[[$handle subwidget fsbox].lf.pane subwidget 1].dirlist subwidget hlist] <Button-4> \
    "
      set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
      [[$handle subwidget fsbox].lf.pane subwidget 1].dirlist subwidget hlist yview scroll -\$n units
    "
  bind [[[$handle subwidget fsbox].lf.pane subwidget 1].dirlist subwidget hlist] <Button-5> \
    "
       set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
       [[$handle subwidget fsbox].lf.pane subwidget 1].dirlist subwidget hlist yview scroll +\$n units
    "

  Dialog:show $handle

  set result   [Dialog:get $handle result  ]
  set fileName [Dialog:get $handle fileName]
  Dialog:delete $handle
  if {$result != 1} { return "" }

  return $fileName
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

proc addEnableDisableTrace { name conditionValueList action1 action2 } \
{
  proc enableDisableTraceHandler { conditionValueList action1 action2 name1 name2 op } \
  {
    # get value
    if {$name2!=""} \
    {
      eval "set value \$::$name1\($name2\)"
    } \
    else \
    {
      eval "set value \$::$name1"
    }
#puts "$conditionValueList: $name1 $name2: $value"

    set flag 0
    foreach conditionValue $conditionValueList \
    {
      if {[eval {expr {$value==$conditionValue}}]} { set flag 1 }
    }
    if {$flag} { eval $action1 } else { eval $action2 }
  }

  eval "set value \$$name"
  foreach conditionValue $conditionValueList \
  {
    if {$value==$conditionValue} { eval $action1 } else { eval $action2 }
  }

  trace variable $name w "enableDisableTraceHandler {$conditionValueList} {$action1} {$action2}"
}

#***********************************************************************
# Name   : addEnableTrace/addDisableTrace
# Purpose: add enable trace
# Input  : name               - variable name
#          conditionValueList - condition value
#          widget             - widget to enable/disable
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addEnableTrace { name conditionValueList widget } \
{
  addEnableDisableTrace $name $conditionValueList "$widget configure -state normal" "$widget configure -state disabled"
}
proc addDisableTrace { name conditionValue widget } \
{
  addEnableDisableTrace $name $conditionValueList "$widget configure -state disabled" "$widget configure -state normal"
}

#***********************************************************************
# Name   : addModifyTrace
# Purpose: add modify trace
# Input  : nameList - variable name list
#          action   - action to execute when variable is modified
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addModifyTrace { nameList action } \
{
  proc modifyTraceHandler { action name1 name2 op } \
  {
    eval $action
  }

  eval $action

  foreach name $nameList \
  {
    trace variable $name w "modifyTraceHandler {$action}"
  }
}

#***********************************************************************
# Name   : addFocusTrace
# Purpose: add focus trace
# Input  : widgetList - widget list
#          inAction   - action to execute when focus in
#          outAction  - action to execute when focus out
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addFocusTrace { widgetList inAction outAction } \
{
  proc focusTraceHandler { action name1 name2 op } \
  {
    eval $action
  }

  foreach widget $widgetList \
  {
    bind $widget <FocusIn>  $inAction
    bind $widget <FocusOut> $outAction
  }
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : printError
# Purpose: print error message to console
# Input  : args - optional arguments
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc printError { args } \
{
  puts stderr "ERROR: [join $args]"
}

#***********************************************************************
# Name   : printWarning
# Purpose: print warning message to console
# Input  : args - optional arguments
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc printWarning { args } \
{
  puts stderr "Warning: [join $args]"
}


#***********************************************************************
# Name   : printUsage
# Purpose: print program usage
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc printUsage { } \
{
  global argv0

  puts "Usage: $argv0 \[<options>\] <config name>"
  puts ""
  puts "Options: -h|--host=<server name>      - server host name"
  puts "         --port=<server port>         - server port number"
  puts "         -p|--tls-port=<server port>  - server TLS (SSL) port number"
  puts "         --list                       - list jobs and quit"
  puts "         --start                      - add new job"
  puts "         --full                       - create full archive (overwrite settings in config)"
  puts "         --incremental                - create incremental archives (overwrite settings in config)"
  puts "         --abort=<id>                 - abort running job"
  puts "         --quit                       - quit"
  puts "         --password=<password>        - server password (use with care!)"
  puts "         --help                       - print this help"
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
# Name   : stringToBoolean
# Purpose: convert string to boolean value
# Input  : s - string
# Output : -
# Return : 1 or 0 (default 0)
# Notes  : -
#***********************************************************************

proc booleanToString { n } \
{
  return [expr {$n?"yes":"no"}]
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

#***********************************************************************
# Name       : getTmpFileName
# Purpose    : path - path to create temporary file
# Input      : -
# Output     : -
# Return     : filename
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc getTmpFileName { {path "/tmp"} } \
{
  set n [clock clicks]
  while {[file exists [file join $path [format "barcontrol-%08x" $n]]]} \
  {
    incr n
  }
  set tmpFileName [file join $path [format "barcontrol-%08x" $n]]

  if {[catch {set handle [open $tmpFileName "w"]}]} \
  {
    return ""
  }
  close $handle

  return $tmpFileName
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : stringToBytes
# Purpose: convert string into bytes
# Input  : s - string
# Output : -
# Return : list with bytes
# Notes  : -
#***********************************************************************

proc stringToBytes { s } \
{
  set bytes {}
  foreach z [split $s {}] \
  {
    scan $z "%c" n
    lappend bytes $n
  }

  return $bytes
}

#***********************************************************************
# Name   : obfuscate
# Purpose: obfuscate character
# Input  : ch         - character
#          z          - index
#          obfuscator - obfuscator value
# Output : -
# Return : obfuscated character
# Notes  : -
#***********************************************************************

proc obfuscate { ch z $obfuscator } \
{
  if {$passwordObfuscator != ""} \
  {
    set passwordObfuscatorBytes [stringToBytes $passwordObfuscator]
    scan $ch "%c" n0
    set n1 [expr {([llength $passwordObfuscatorBytes]>0)?[lindex $passwordObfuscatorBytes [expr {$z%[llength $passwordObfuscatorBytes]}]]:0}]
    set ch [format "%c" [expr {$n^$n1}]]
  }

  return $ch
}

#***********************************************************************
# Name   : obfuscatePassword
# Purpose: obfuscate password
# Input  : password            - password
#          $passwordObfuscator - password obfuscator
# Output : -
# Return : obfuscated password
# Notes  : -
#***********************************************************************

proc obfuscatePassword { password passwordObfuscator } \
{
  if {$passwordObfuscator != ""} \
  {
    set passwordObfuscatorBytes [stringToBytes $passwordObfuscator]
    set passwordBytes           [stringToBytes $password          ]
    set password ""
    for {set z 0} {$z < [llength $passwordBytes]} {incr z} \
    {
      set n0 [expr {([llength $passwordBytes          ]>0)?[lindex $passwordBytes           [expr {$z%[llength $passwordBytes          ]}]]:0}]
      set n1 [expr {([llength $passwordObfuscatorBytes]>0)?[lindex $passwordObfuscatorBytes [expr {$z%[llength $passwordObfuscatorBytes]}]]:0}]
#puts "$n0 $n1: [expr {$n0^$n1}]-[format "%c" [expr {$n0^$n1}]]"
      append password [format "%c" [expr {$n0^$n1}]]
    }
#puts $password
  }

  return $password
}

#***********************************************************************
# Name   : getPassword
# Purpose: input password
# Input  : title         - title text
#          verifyFlag    - 1 for verify password input, 0 otherwise
#          obfuscateFlag - 1 for obfuscate password, 0 otherwise
# Output : -
# Return : obfuscated password or ""
# Notes  : -
#***********************************************************************

proc getPassword { title verifyFlag obfuscateFlag } \
{
  global guiMode

  if {!$guiMode} \
  {
    if {[info exists env(SSH_ASKPASS)]} \
    {
      set password [exec sh -c $env(SSH_ASKPASS)]
    } \
    else \
    {
      puts -nonewline stdout "$title: "; flush stdout
      set password [exec sh -c "read -s password; echo \$password; unset password"]
      puts ""
      if {$verifyFlag} \
      {
        puts -nonewline stdout "Verify password: "; flush stdout
        set passwordVerify [exec sh -c "read -s password; echo \$password; unset password"]
        puts ""
      }
    }
    if {$password != $passwordVerify} { set password "" }
  } \
  else \
  {
    set password [Dialog:password "$title" $verifyFlag]
  }
  if {$password == ""} { return "" }

  if {$obfuscateFlag} \
  {
    set password [obfuscatePassword $password]
  }

  return $password
}

#***********************************************************************
# Name   : memorySizeToBytes
# Purpose: convert memory size into bytes
# Input  : s - memory size string <n>(G|M|K)
# Output : -
# Return : bytes
# Notes  : -
#***********************************************************************

proc memorySizeToBytes { s } \
{
  if     {[regexp {(\d+)G} $s * t]} \
  {
    set n [expr {int($t)*1024*1024*1024}]
  } \
  elseif {[regexp {(\d+)M} $s * t]} \
  {
    set n [expr {int($t)*1024*1024}]
  } \
  elseif {[regexp {(\d+)K} $s * t]} \
  {
    set n [expr {int($t)*1024}]
  } \
  elseif {[string is integer $s]} \
  {
    set n [expr {int($s)}]
  } \
  else \
  {
    set n 0
  }

  return $n
}

#***********************************************************************
# Name   : BackupServer:connect
# Purpose: connect to server
# Input  : hostname - host name
#          port     - port number
#          password - obfuscated password
#          tlsFlag  - 1 for TLS connection, 0 for plain connection
# Output : -
# Return : 1 if connected, 0 on error
# Notes  : -
#***********************************************************************

proc BackupServer:connect { hostname port password tlsFlag } \
{
  global server passwordObfuscator

  if {$tlsFlag} \
  {
    if {[catch {set server(socketHandle) [tls::socket $hostname $port]}]} \
    {
      return 0
    }
    tls::handshake $server(socketHandle)
  } \
  else \
  {
    if {[catch {set server(socketHandle) [socket $hostname $port]}]} \
    {
      return 0
    }
  }
  fconfigure $server(socketHandle) -buffering line -blocking 1 -translation lf

  # get session id
  gets $server(socketHandle) line
  if {[scanx $line "SESSION %s" sessionId] != 1} \
  {
    close $server(socketHandle) 
    set server(socketHandle) -1
    return
  }
#puts "sessionid=$sessionId"

  # authorize
  set passwordObfuscatorBytes [stringToBytes $passwordObfuscator]
  set passwordBytes           [stringToBytes $password          ]
#puts $passwordBytes
  set s ""
  set z 0
  foreach {h l} [split $sessionId {}] \
  {
#puts "hl $h $l"
    set n0 [expr {([llength $passwordBytes          ]>0)?[lindex $passwordBytes           [expr {$z%[llength $passwordBytes          ]}]]:0}]
    set n1 [expr {([llength $passwordObfuscatorBytes]>0)?[lindex $passwordObfuscatorBytes [expr {$z%[llength $passwordObfuscatorBytes]}]]:0}]
    scan "0x$h$l" "%x" n2
#puts "n $n0 $n1 $n2 [expr {$n0^$n1}] [expr {$n0^$n1^$n2}]"

    append s [format "%02x" [expr {$n0^$n1^$n2}]]
#puts $s

    incr z
  }
  set errorCode 0
  BackupServer:executeCommand errorCode errorText "AUTHORIZE" $s
  if {$errorCode != 0} \
  {
    close $server(socketHandle) 
    set server(socketHandle) -1
    return 0
  }

  return 1
}

#***********************************************************************
# Name   : BackupServer:disconnect
# Purpose: disconnect from server
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc BackupServer:disconnect {} \
{
  global server

  if {$server(socketHandle) != -1} \
  {
    catch {close $server(socketHandle)}
    set server(socketHandle) -1
  }
}

#***********************************************************************
# Name   : BackupServer:sendCommand
# Purpose: send command to server
# Input  : command - command
#          args    - arguments for command
# Output : -
# Return : command id or 0 on error
# Notes  : -
#***********************************************************************

proc BackupServer:sendCommand { command args } \
{
  global server lastCommandId

  incr lastCommandId

  set arguments [join $args]

  if {[catch {puts $server(socketHandle) "$lastCommandId $command $arguments"; flush $server(socketHandle)}]} \
  {
    return 0
  }
#puts "sent [clock clicks]: $command $lastCommandId $arguments"

  return $lastCommandId
}

#***********************************************************************
# Name   : BackupServer:readResult
# Purpose: read result from server
# Input  : commandId - command id
# Output : _errorCode - error code
#          _result    - result data
# Return : 1 if result read, 0 for end of data
# Notes  : -
#***********************************************************************

proc BackupServer:readResult { commandId _completeFlag _errorCode _result } \
{
  global server

  upvar $_completeFlag completeFlag
  upvar $_errorCode    errorCode
  upvar $_result       result

  set id -1
  while {($id != $commandId) && ($id != 0)} \
  {
     if {$server(socketHandle) == -1} \
     {
       return 0
     }
     if {[eof $server(socketHandle)]} \
     {
       catch {close $server(socketHandle)}
       set server(socketHandle) -1
       return 0
     }
#puts "read [clock clicks] [eof $server(socketHandle)]"
    while {[fblocked $server(socketHandle)]} \
    {
puts "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX blocked"
after 100
}
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
# Name   : BackupServer:executeCommand
# Purpose: execute command
# Input  : command - command
#          args    - arguments for command
# Output : _errorCode - error code (will only be set if 0)
#          _errorText - error text (will only be set if _errorCode is 0)
# Return : 1 if command executed, 0 otherwise
# Notes  : -
#***********************************************************************

proc BackupServer:executeCommand { _errorCode _errorText command args } \
{
  upvar $_errorCode errorCode
  upvar $_errorText errorText

#puts "start 1"
  set commandId [BackupServer:sendCommand $command [join $args]]
  if {$commandId == 0} \
  {
    return 0
  }
  if {![BackupServer:readResult $commandId completeFlag localErrorCode result]} \
  {
    return 0
  }
#puts "end 1"
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

proc Restore:open {} \
{
  set restoreHandle [open "| /home/torsten/projects/bar/bar --batch" RDWR]
puts [eof $restoreHandle]
puts $restoreHandle "VERSION"; flush $restoreHandle
gets $restoreHandle line
puts "line=$line"
gets $restoreHandle line
puts "line=$line"
exit 1

  return $restoreHandle
}

proc Restore:close { restoreHandle } \
{
puts $restoreHandle
  close $restoreHandle
}

proc Restore:sendCommand { restoreHandle command args } \
{
  global server lastCommandId

  incr lastCommandId

  set arguments [join $args]

  if {[catch {puts $restoreHandle "$command $lastCommandId $arguments"; flush $restoreHandle}]} \
  {
    return 0
  }
#puts "sent [clock clicks]: $command $lastCommandId $arguments"

  return $lastCommandId
}
  

proc Restore:readResult { restoreHandle commandId _completeFlag _errorCode _result } \
{
  global server

  upvar $_completeFlag completeFlag
  upvar $_errorCode    errorCode
  upvar $_result       result

  set id -1
  while {($id != $commandId) && ($id != 0)} \
  {
     if {[eof $restoreHandle]} { puts "obs?"; return 0 }
    gets $restoreHandle line
#puts "received [clock clicks] [eof $server(socketHandle)]: $line"

    set completeFlag 0
    set errorCode    -1
    set result       ""
    regexp {(\d+)\s+(\d+)\s+(\d+)\s+(.*)} $line * id completeFlag errorCode result
  }
#puts "$completeFlag $errorCode $result"

  return 1
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
  global jobListTimerId currentJob barControlConfig

  catch {after cancel $jobListTimerId}

  # get current selection
  set selectedId 0
  if {[$jobListWidget curselection] != {}} \
  {
    set n [lindex [$jobListWidget curselection] 0]
    set selectedId [lindex [lindex [$jobListWidget get $n $n] 0] 0]
  }
  set yview [lindex [$jobListWidget yview] 0]

  # update list
  $jobListWidget delete 0 end
#puts "start 3"
  set commandId [BackupServer:sendCommand "JOB_LIST"]
  while {[BackupServer:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
  {
#puts "1: $result"
    scanx $result "%d %S %S %s %d %S %S %d %d" \
      id \
      name \
      state \
      type \
      archivePartSize \
      compressAlgorithm \
      cryptAlgorithm \
      startTime \
      estimatedRestTime
#puts "1: ok"

    set estimatedRestDays    [expr {int($estimatedRestTime/(24*60*60)        )}]
    set estimatedRestHours   [expr {int($estimatedRestTime%(24*60*60)/(60*60))}]
    set estimatedRestMinutes [expr {int($estimatedRestTime%(60*60   )/60     )}]
    set estimatedRestSeconds [expr {int($estimatedRestTime%(60)              )}]

    $jobListWidget insert end [list \
      $id \
      $name \
      $state \
      $type \
      [expr {($archivePartSize > 0)?[formatByteSize $archivePartSize]:"-"}] \
      $compressAlgorithm \
      $cryptAlgorithm \
      [expr {($startTime > 0)?[clock format $startTime -format "%Y-%m-%d %H:%M:%S"]:"-"}] \
      [format "%2d days %02d:%02d:%02d" $estimatedRestDays $estimatedRestHours $estimatedRestMinutes $estimatedRestSeconds] \
    ]
  }
#puts "end 3"

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
  $jobListWidget yview moveto $yview

  set jobListTimerId [after $barControlConfig(jobListUpdateTime) "updateJobList $jobListWidget"]
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
  global currentJob currentJobTimerId barControlConfig

  catch {after cancel $currentJobTimerId}

  if {$currentJob(id) != 0} \
  {
#puts "start 2"
    set commandId [BackupServer:sendCommand "JOB_INFO" $currentJob(id)]
    if {[BackupServer:readResult $commandId completeFlag errorCode result] && ($errorCode == 0)} \
    {
#puts "2: $result"
      scanx $result "%S %S %lu %lu %lu %lu %lu %lu %lu %lu %f %f %f %lu %f %S %lu %lu %S %lu %lu %d %f %d" \
        state \
        currentJob(error) \
        currentJob(doneFiles) \
        currentJob(doneBytes) \
        currentJob(totalFiles) \
        currentJob(totalBytes) \
        currentJob(skippedFiles) \
        currentJob(skippedBytes) \
        currentJob(errorFiles) \
        currentJob(errorBytes) \
        filesPerSecond \
        currentJob(bytesPerSecond) \
        currentJob(storageBytesPerSecond) \
        currentJob(archiveBytes) \
        ratio \
        currentJob(fileName) \
        currentJob(fileDoneBytes) \
        currentJob(fileTotalBytes) \
        currentJob(storageName) \
        currentJob(storageDoneBytes) \
        currentJob(storageTotalBytes) \
        currentJob(volumeNumber) \
        currentJob(volumeProgress) \
        currentJob(requestedVolumeNumber)
#puts "2: ok"
      if {$currentJob(error) != ""} { set currentJob(message) $currentJob(error) }

      if     {$currentJob(doneBytes)            > 1024*1024*1024} { set currentJob(doneBytesShort)             [format "%.1f" [expr {double($currentJob(doneBytes)            )/(1024*1024*1024)}]]; set currentJob(doneBytesShortUnit)             "GBytes"   } \
      elseif {$currentJob(doneBytes)            >      1024*1024} { set currentJob(doneBytesShort)             [format "%.1f" [expr {double($currentJob(doneBytes)            )/(     1024*1024)}]]; set currentJob(doneBytesShortUnit)             "MBytes"   } \
      else                                                        { set currentJob(doneBytesShort)             [format "%.1f" [expr {double($currentJob(doneBytes)            )/(          1024)}]]; set currentJob(doneBytesShortUnit)             "KBytes"   }
      if     {$currentJob(totalBytes)           > 1024*1024*1024} { set currentJob(totalBytesShort)            [format "%.1f" [expr {double($currentJob(totalBytes)           )/(1024*1024*1024)}]]; set currentJob(totalBytesShortUnit)            "GBytes"   } \
      elseif {$currentJob(totalBytes)           >      1024*1024} { set currentJob(totalBytesShort)            [format "%.1f" [expr {double($currentJob(totalBytes)           )/(     1024*1024)}]]; set currentJob(totalBytesShortUnit)            "MBytes"   } \
      else                                                        { set currentJob(totalBytesShort)            [format "%.1f" [expr {double($currentJob(totalBytes)           )/(          1024)}]]; set currentJob(totalBytesShortUnit)            "KBytes"   }
      if     {$currentJob(skippedBytes)         > 1024*1024*1024} { set currentJob(skippedBytesShort)          [format "%.1f" [expr {double($currentJob(skippedBytes)         )/(1024*1024*1024)}]]; set currentJob(skippedBytesShortUnit)          "GBytes"   } \
      elseif {$currentJob(skippedBytes)         >      1024*1024} { set currentJob(skippedBytesShort)          [format "%.1f" [expr {double($currentJob(skippedBytes)         )/(     1024*1024)}]]; set currentJob(skippedBytesShortUnit)          "MBytes"   } \
      else                                                        { set currentJob(skippedBytesShort)          [format "%.1f" [expr {double($currentJob(skippedBytes)         )/(          1024)}]]; set currentJob(skippedBytesShortUnit)          "KBytes"   }
      if     {$currentJob(errorBytes)           > 1024*1024*1024} { set currentJob(errorBytesShort)            [format "%.1f" [expr {double($currentJob(errorBytes)           )/(1024*1024*1024)}]]; set currentJob(errorBytesShortUnit)            "GBytes"   } \
      elseif {$currentJob(errorBytes)           >      1024*1024} { set currentJob(errorBytesShort)            [format "%.1f" [expr {double($currentJob(errorBytes)           )/(     1024*1024)}]]; set currentJob(errorBytesShortUnit)            "MBytes"   } \
      else                                                        { set currentJob(errorBytesShort)            [format "%.1f" [expr {double($currentJob(errorBytes)           )/(          1024)}]]; set currentJob(errorBytesShortUnit)            "KBytes"   }
      if     {$currentJob(bytesPerSecond)       > 1024*1024*1024} { set currentJob(bytesPerSecondShort)        [format "%.1f" [expr {double($currentJob(bytesPerSecond)       )/(1024*1024*1024)}]]; set currentJob(bytesPerSecondUnit)             "GBytes/s" } \
      elseif {$currentJob(bytesPerSecond)       >      1024*1024} { set currentJob(bytesPerSecondShort)        [format "%.1f" [expr {double($currentJob(bytesPerSecond)       )/(     1024*1024)}]]; set currentJob(bytesPerSecondShortUnit)        "MBytes/s" } \
      else                                                        { set currentJob(bytesPerSecondShort)        [format "%.1f" [expr {double($currentJob(bytesPerSecond)       )/(          1024)}]]; set currentJob(bytesPerSecondShortUnit)        "KBytes/s" }
      if     {$currentJob(storageBytesPerSecond)> 1024*1024*1024} { set currentJob(storageBytesPerSecondShort) [format "%.1f" [expr {double($currentJob(storageBytesPerSecond))/(1024*1024*1024)}]]; set currentJob(storageBytesPerSecondShortUnit) "GBytes/s" } \
      elseif {$currentJob(storageBytesPerSecond)>      1024*1024} { set currentJob(storageBytesPerSecondShort) [format "%.1f" [expr {double($currentJob(storageBytesPerSecond))/(     1024*1024)}]]; set currentJob(storageBytesPerSecondShortUnit) "MBytes/s" } \
      else                                                        { set currentJob(storageBytesPerSecondShort) [format "%.1f" [expr {double($currentJob(storageBytesPerSecond))/(          1024)}]]; set currentJob(storageBytesPerSecondShortUnit) "KBytes/s" }
      if     {$currentJob(archiveBytes)         > 1024*1024*1024} { set currentJob(archiveBytesShort)          [format "%.1f" [expr {double($currentJob(archiveBytes)         )/(1024*1024*1024)}]]; set currentJob(archiveBytesShortUnit)          "GBytes"   } \
      elseif {$currentJob(archiveBytes)         >      1024*1024} { set currentJob(archiveBytesShort)          [format "%.1f" [expr {double($currentJob(archiveBytes)         )/(     1024*1024)}]]; set currentJob(archiveBytesShortUnit)          "MBytes"   } \
      else                                                        { set currentJob(archiveBytesShort)          [format "%.1f" [expr {double($currentJob(archiveBytes)         )/(          1024)}]]; set currentJob(archiveBytesShortUnit)          "KBytes"   }
      if     {$currentJob(storageTotalBytes)    > 1024*1024*1024} { set currentJob(storageTotalBytesShort)     [format "%.1f" [expr {double($currentJob(storageTotalBytes)    )/(1024*1024*1024)}]]; set currentJob(storageTotalBytesShortUnit)     "GBytes"   } \
      elseif {$currentJob(storageTotalBytes)    >      1024*1024} { set currentJob(storageTotalBytesShort)     [format "%.1f" [expr {double($currentJob(storageTotalBytes)    )/(     1024*1024)}]]; set currentJob(storageTotalBytesShortUnit)     "MBytes"   } \
      else                                                        { set currentJob(storageTotalBytesShort)     [format "%.1f" [expr {double($currentJob(storageTotalBytes)    )/(          1024)}]]; set currentJob(storageTotalBytesShortUnit)     "KBytes"   }

      set currentJob(filesPerSecond)   [format "%.1f" $filesPerSecond]
      set currentJob(compressionRatio) [format "%.1f" $ratio]
    }
#puts "end 2"
  } \
  else \
  {
    set currentJob(doneFiles)                      0
    set currentJob(doneBytes)                      0
    set currentJob(doneBytesShort)                 0
    set currentJob(doneBytesShortUnit)             "KBytes"
    set currentJob(totalFiles)                     0
    set currentJob(totalBytes)                     0
    set currentJob(totalBytesShort)                0
    set currentJob(totalBytesShortUnit)            "KBytes"
    set currentJob(skippedFiles)                   0
    set currentJob(skippedBytes)                   0
    set currentJob(skippedBytesShort)              0
    set currentJob(skippedBytesShortUnit)          "KBytes"
    set currentJob(errorFiles)                     0
    set currentJob(errorBytes)                     0
    set currentJob(errorBytesShort)                0
    set currentJob(errorBytesShortUnit)            "KBytes"
    set currentJob(filesPerSecond)                 0
    set currentJob(filesPerSecondUnit)             "files/s"
    set currentJob(bytesPerSecond)                 0
    set currentJob(bytesPerSecondShort)            0
    set currentJob(bytesPerSecondShortUnit)        "KBytes/s"
    set currentJob(storageBytesPerSecond)          0
    set currentJob(storageBytesPerSecondShort)     0
    set currentJob(storageBytesPerSecondShortUnit) "KBytes/s"
    set currentJob(compressionRatio)               0
    set currentJob(fileName)                       ""
    set currentJob(fileDoneBytes)                  0
    set currentJob(fileTotalBytes)                 0
    set currentJob(storageName)                    ""
    set currentJob(storageName)                    ""
    set currentJob(storageDoneBytes)               0
    set currentJob(storageTotalBytes)              0
    set currentJob(storageTotalBytesShort)         0
    set currentJob(storageTotalBytesUnit)          "KBytes"
    set currentJob(volumeNumber)                   0
    set currentJob(requestedVolumeNumber)          0
  }

  set currentJobTimerId [after $barControlConfig(currentJobUpdateTime) "updateCurrentJob"]
}

#***********************************************************************
# Name       : itemPathToFileName
# Purpose    : convert item path to file name
# Input      : widget     - tree-widget
#              itemPath   - item path
#              prefixFlag - 1 if item have a prefix, 0 otherwise
# Output     : -
# Return     : file name
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc itemPathToFileName { widget itemPath prefixFlag } \
{
  set separator [lindex [$widget configure -separator] 4]
#puts "$itemPath -> #[string range $itemPath [string length $separator] end]#"

  if {$prefixFlag} \
  {
    set i [string first $separator $itemPath]
    if {$i >= 0} \
    {
      set fileName [string map [list $separator "/"] [string range $itemPath [expr {$i+1}] end]]
    } \
    else \
    {
      set fileName ""
    }
  } \
  else \
  {
    set fileName [string map [list $separator "/"] $itemPath]
  }

  return $fileName
}

#***********************************************************************
# Name       : itemPathToPrefix
# Purpose    : convert item path to file name
# Input      : widget   - tree-widget
#              itemPath - item path
# Output     : -
# Return     : file name
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc itemPathToPrefix { widget itemPath } \
{
  set separator [lindex [$widget configure -separator] 4]
#puts "$itemPath -> #[string range $itemPath [string length $Separator] end]#"

  set i [string first $separator $itemPath]
  if {$i > 0} \
  {
    return [string range $itemPath 0 [expr {$i-1}]]
  } \
  else \
  {
    return $itemPath
  }
}

#***********************************************************************
# Name       : fileNameToItemPath
# Purpose    : convert file name to item path
# Input      : widget   - tree-widget 
#              prefix   - prefix or ""
#              fileName - file name
# Output     : -
# Return     : item path
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc fileNameToItemPath { widget prefix fileName } \
 {
  # get separator
  set separator "/"
  catch {set separator [lindex [$widget configure -separator] 4]}

  # get path name
  if {($fileName != "") && ($fileName != ".") && ($fileName != "/")} \
  {
    if {$prefix != ""} \
    {
      set itemPath "$prefix$separator[string map [list "/" $separator] $fileName]"
    } \
    else \
    {
      set itemPath [string map [list "/" $separator] $fileName]
    }
  } \
  else \
  {
    if {$prefix != ""} \
    {
      set itemPath $prefix
    } \
    else \
    {
      set itemPath $separator
    }
  }

  return $itemPath
 }

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : checkIncluded
# Purpose: check if file is in included-pattern-list
# Input  : fileName - file name
# Output : -
# Return : 1 if file is in included-pattern-list, 0 otherwise
# Notes  : -
#***********************************************************************

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

#***********************************************************************
# Name   : checkExcluded
# Purpose: check if file is in excluded-pattern-list
# Input  : fileName - file name
# Output : -
# Return : 1 if file is in excluded-pattern-list, 0 otherwise
# Notes  : -
#***********************************************************************

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
# Name   : setEntryState
# Purpose: set entry state
# Input  : itemPath - item path
#          state    - NONE, INCLUDED, EXCLUDED
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc setEntryState { widget itemPath prefixFlag state } \
{
  global barConfig images

  # get file name
  set fileName [itemPathToFileName $widget $itemPath $prefixFlag]

  # get data
  set data [$widget info data $itemPath]
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
        $widget delete offsprings $itemPath
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
  $widget item configure $itemPath 0 -image $image

  # update data
  lset data 1 $state
  $widget entryconfigure $itemPath -data $data

  setConfigModify
}

#***********************************************************************
# Name   : toggleEntryIncludedExcluded
# Purpose: toggle entry state: NONE, INCLUDED, EXCLUDED
# Input  : widget     - files tree widget
#          itemPath   - item path
#          prefixFlag - 1 if files have a prefix, 0 otherwise
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc toggleEntryIncludedExcluded { widget itemPath prefixFlag } \
{
  # get data
  set data [$widget info data $itemPath]
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

  setEntryState $widget $itemPath $prefixFlag $state
}


#***********************************************************************
# Name   : clearFileList
# Purpose: clear file list
# Input  : widget - files tree-widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc clearFileList { widget } \
{
  global images

  foreach itemPath [$widget info children ""] \
  {
    $widget delete offsprings $itemPath
    $widget item configure $itemPath 0 -image $images(folder)
  }
}

#***********************************************************************
# Name   : addBackupDevice
# Purpose: add a device to backup-tree-widget
# Input  : deviceName - file name/directory name
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addBackupDevice { deviceName } \
{
  global backupFilesTreeWidget

  catch {$backupFilesTreeWidget delete entry $deviceName}

  set n 0
  set l [$backupFilesTreeWidget info children ""]
  while {($n < [llength $l]) && (([$backupFilesTreeWidget info data [lindex $l $n]] != {}) || ($deviceName>[lindex $l $n]))} \
  {
    incr n
  }

  set style [tixDisplayStyle imagetext -refwindow $backupFilesTreeWidget]

  $backupFilesTreeWidget add $deviceName -at $n -itemtype imagetext -text $deviceName -image [tix getimage folder] -style $style -data [list "DIRECTORY" "NONE" 0]
  $backupFilesTreeWidget item create $deviceName 1 -itemtype imagetext -style $style
  $backupFilesTreeWidget item create $deviceName 2 -itemtype imagetext -style $style
  $backupFilesTreeWidget item create $deviceName 3 -itemtype imagetext -style $style
}

#***********************************************************************
# Name   : addBackupEntry
# Purpose: add a file/directory/link entry to tree-widget
# Input  : fileName - file name/directory name
#          fileType - FILE|DIRECTORY|LINK
#          fileSize - file size
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addBackupEntry { fileName fileType fileSize } \
{
  global backupFilesTreeWidget barConfig images

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
  set itemPath       [fileNameToItemPath $backupFilesTreeWidget "" $fileName       ]
  set parentItemPath [fileNameToItemPath $backupFilesTreeWidget "" $parentDirectory]
#puts "f=$fileName"
#puts "i=$itemPath"
#puts "p=$parentItemPath"

  catch {$backupFilesTreeWidget delete entry $itemPath}

  # create parent entry if it does not exists
  if {($parentItemPath != "") && ![$backupFilesTreeWidget info exists $parentItemPath]} \
  {
    addBackupEntry [file dirname $fileName] "DIRECTORY" 0
  }

  # get excluded flag of entry
  set excludedFlag [checkExcluded $fileName]

  # get styles
  set styleImage     [tixDisplayStyle imagetext -refwindow $backupFilesTreeWidget -anchor w]
  set styleTextLeft  [tixDisplayStyle text      -refwindow $backupFilesTreeWidget -anchor w]
  set styleTextRight [tixDisplayStyle text      -refwindow $backupFilesTreeWidget -anchor e]

   if     {$fileType=="FILE"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$backupFilesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$backupFilesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     if {!$excludedFlag} { set image $images(file) } else { set image $images(fileExcluded) }
     $backupFilesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "FILE" "NONE" 0]
     $backupFilesTreeWidget item create $itemPath 1 -itemtype text -text "FILE"    -style $styleTextLeft
     $backupFilesTreeWidget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $backupFilesTreeWidget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
   } \
   elseif {$fileType=="DIRECTORY"} \
   {
#puts "add directory $fileName"
     # find insert position (sort)
     set n 0
     set l [$backupFilesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && ([lindex [$backupFilesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") && ($itemPath > [lindex $l $n])} \
     {
       incr n
     }

     # add directory item
     if {!$excludedFlag} { set image $images(folder) } else { set image $images(folderExcluded) }
     $backupFilesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "DIRECTORY" "NONE" 0]
     $backupFilesTreeWidget item create $itemPath 1 -itemtype text -style $styleTextLeft
     $backupFilesTreeWidget item create $itemPath 2 -itemtype text -style $styleTextLeft
     $backupFilesTreeWidget item create $itemPath 3 -itemtype text -style $styleTextLeft
   } \
   elseif {$fileType=="LINK"} \
   {
#puts "add link $fileName"
     set n 0
     set l [$backupFilesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$backupFilesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add link item
     if {!$excludedFlag} { set image $images(link) } else { set image $images(linkExcluded) }
     $backupFilesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "LINK" "NONE" 0]
     $backupFilesTreeWidget item create $itemPath 1 -itemtype text -text "LINK" -style $styleTextLeft
     $backupFilesTreeWidget item create $itemPath 2 -itemtype text              -style $styleTextLeft
     $backupFilesTreeWidget item create $itemPath 3 -itemtype text              -style $styleTextLeft
   } \
   elseif {$fileType=="DEVICE"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$backupFilesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$backupFilesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     if {!$excludedFlag} { set image $images(file) } else { set image $images(fileExcluded) }
     $backupFilesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "FILE" "NONE" 0]
     $backupFilesTreeWidget item create $itemPath 1 -itemtype text -text "DEVICE"  -style $styleTextLeft
     $backupFilesTreeWidget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $backupFilesTreeWidget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
   } \
   elseif {$fileType=="SOCKET"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$backupFilesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$backupFilesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     if {!$excludedFlag} { set image $images(file) } else { set image $images(fileExcluded) }
     $backupFilesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "FILE" "NONE" 0]
     $backupFilesTreeWidget item create $itemPath 1 -itemtype text -text "SOCKET"  -style $styleTextLeft
     $backupFilesTreeWidget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $backupFilesTreeWidget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
   } \
}

#***********************************************************************
# Name   : openCloseBackupDirectory
# Purpose: open/close backup directory
# Input  : itemPath - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc openCloseBackupDirectory { itemPath } \
{
  global backupFilesTreeWidget backupIncludedListWidget backupExcludedListWidget images

#puts $itemPath
  # get directory name
  set directoryName [itemPathToFileName $backupFilesTreeWidget $itemPath 0]

  # check if existing, add if not exists
  if {![$backupFilesTreeWidget info exists $itemPath]} \
  {
    addBackupEntry $directoryName "DIRECTORY" 0
  }
#puts [$backupFilesTreeWidget info exists $itemPath]
#puts [$backupFilesTreeWidget info data $itemPath]

  # check if parent exist and is open, open if needed
  set parentItemPath [$backupFilesTreeWidget info parent $itemPath]
#  set data [$backupFilesTreeWidget info data $parentItemPath]
#puts "$parentItemPath: $data"
  if {[$backupFilesTreeWidget info exists $parentItemPath]} \
  {
    set data [$backupFilesTreeWidget info data $parentItemPath]
    if {[lindex $data 2] == 0} \
    {
      openCloseBackupDirectory $parentItemPath
    }
    set data [$backupFilesTreeWidget info data $parentItemPath]
    if {[lindex $data 0] == "LINK"} { return }
  }

  # get data
  set data [$backupFilesTreeWidget info data $itemPath]
  set type              [lindex $data 0]
  set state             [lindex $data 1]
  set directoryOpenFlag [lindex $data 2]

  if {$type == "DIRECTORY"} \
  {
    # get open/closed flag

    $backupFilesTreeWidget delete offsprings $itemPath
    if {!$directoryOpenFlag} \
    {
      if     {$state == "INCLUDED"} { set image $images(folderIncludedOpen) } \
      elseif {$state == "EXCLUDED"} { set image $images(folderExcludedOpen) } \
      else                          { set image $images(folderOpen)         }
      $backupFilesTreeWidget item configure $itemPath 0 -image $image
      update

      set fileName [itemPathToFileName $backupFilesTreeWidget $itemPath 0]
      set commandId [BackupServer:sendCommand "FILE_LIST" $fileName 0]
      while {[BackupServer:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
      {
#puts "add file $result"
        if     {[scanx $result "FILE %d %S" fileSize fileName] == 2} \
        {
          addBackupEntry $fileName "FILE" $fileSize
        } \
        elseif {[scanx $result "DIRECTORY %ld %S" totalSize directoryName] == 2} \
        {
          addBackupEntry $directoryName "DIRECTORY" $totalSize
        } \
        elseif {[scanx $result "LINK %S" linkName] == 1} \
        {
          addBackupEntry $linkName "LINK" 0
        } \
        elseif {[scanx $result "DEVICE %S" deviceName] == 1} \
        {
          addBackupEntry $deviceName "DEVICE" 0
        } \
        elseif {[scanx $result "SOCKET %S" socketName] == 1} \
        {
          addBackupEntry $socketName "SOCKET" 0
        } else {
  internalError "unknown file type in openclosedirectory: $result"
}
      }

      set directoryOpenFlag 1
    } \
    else \
    {
      if     {$state == "INCLUDED"} { set image $images(folderIncluded) } \
      elseif {$state == "EXCLUDED"} { set image $images(folderExcluded) } \
      else                          { set image $images(folder)         }
      $backupFilesTreeWidget item configure $itemPath 0 -image $image

      set directoryOpenFlag 0
    }

    # update data
    lset data 2 $directoryOpenFlag
    $backupFilesTreeWidget entryconfigure $itemPath -data $data
  }
}

#***********************************************************************
# Name   : backupUpdateFileTreeStates
# Purpose: update file tree states depending on include/exclude patterns
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc backupUpdateFileTreeStates { } \
{
  global backupFilesTreeWidget barConfig images

  set itemPathList [$backupFilesTreeWidget info children ""]
  while {[llength $itemPathList] > 0} \
  {
    set fileName [lindex $itemPathList 0]; set itemPathList [lreplace $itemPathList 0 0]
    set itemPath [fileNameToItemPath $backupFilesTreeWidget "" $fileName]

    set data [$backupFilesTreeWidget info data $itemPath]
    set type              [lindex $data 0]
    set state             [lindex $data 1]
    set directoryOpenFlag [lindex $data 2]

    # add sub-directories to update
    if {($type == "DIRECTORY") && ($state != "EXCLUDED") && $directoryOpenFlag} \
    {
      foreach z [$backupFilesTreeWidget info children $itemPath] \
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
    elseif {($state == "INCLUDED") && !$includedFlag} \
    {
      if {$excludedFlag} \
      {
        set state "EXCLUDED"
      } \
      else \
      {
        set state "NONE"
      }
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
          $backupFilesTreeWidget delete offsprings $itemPath
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
    $backupFilesTreeWidget item configure [fileNameToItemPath $backupFilesTreeWidget "" $fileName] 0 -image $image

    # update data
    lset data 1 $state
    $backupFilesTreeWidget entryconfigure $itemPath -data $data
  }
}

#***********************************************************************
# Name   : addRestoreEntry
# Purpose: add a file/directory/link entry to tree-widget
# Input  : fileName - file name/directory name
#          fileType - FILE|DIRECTORY|LINK
#          fileSize - file size
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addRestoreEntry { archiveName fileName fileType fileSize directoryOpenFlag } \
{
  global restoreFilesTreeWidget barConfig images

if {[info level]>15} { error "x" }

  # get parent directory
  if {[file tail $fileName] != $fileName} \
  {
    set parentDirectory [file dirname $fileName]
  } \
  else \
  {
    set parentDirectory ""
  }

  # get item path, parent item path
  set itemPath       [fileNameToItemPath $restoreFilesTreeWidget $archiveName $fileName       ]
  set parentItemPath [fileNameToItemPath $restoreFilesTreeWidget $archiveName $parentDirectory]
#puts "a=$archiveName"
#puts "f=$fileName"
#puts "D=$parentDirectory"
#puts "i=$itemPath"
#puts "p=$parentItemPath"
#puts ""

  catch {$restoreFilesTreeWidget delete entry $itemPath}

  # create parent entry if it does not exists
  if {($parentItemPath != "") && ![$restoreFilesTreeWidget info exists $parentItemPath]} \
  {
    addRestoreEntry $archiveName [file dirname $fileName] "DIRECTORY" 0 1
  }

  # get excluded flag of entry
  set excludedFlag [checkExcluded $fileName]

  # get styles
  set styleImage     [tixDisplayStyle imagetext -refwindow $restoreFilesTreeWidget -anchor w]
  set styleTextLeft  [tixDisplayStyle text      -refwindow $restoreFilesTreeWidget -anchor w]
  set styleTextRight [tixDisplayStyle text      -refwindow $restoreFilesTreeWidget -anchor e]

   if     {$fileType=="FILE"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$restoreFilesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$restoreFilesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     if {!$excludedFlag} { set image $images(file) } else { set image $images(fileExcluded) }
     $restoreFilesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "FILE" "NONE" 0]
     $restoreFilesTreeWidget item create $itemPath 1 -itemtype text -text "FILE"    -style $styleTextLeft
     $restoreFilesTreeWidget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $restoreFilesTreeWidget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
   } \
   elseif {$fileType=="DIRECTORY"} \
   {
#puts "add directory $fileName"
     # find insert position (sort)
     set n 0
     set l [$restoreFilesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && ([lindex [$restoreFilesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") && ($itemPath > [lindex $l $n])} \
     {
       incr n
     }

     # add directory item
     if {$directoryOpenFlag} \
     {
       if {!$excludedFlag} { set image $images(folderOpen) } else { set image $images(folderExcluded) }
     } \
     else \
     {
       if {!$excludedFlag} { set image $images(folder) } else { set image $images(folderExcluded) }
     }
     $restoreFilesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "DIRECTORY" "NONE" $directoryOpenFlag]
     $restoreFilesTreeWidget item create $itemPath 1 -itemtype text -style $styleTextLeft
     $restoreFilesTreeWidget item create $itemPath 2 -itemtype text -style $styleTextLeft
     $restoreFilesTreeWidget item create $itemPath 3 -itemtype text -style $styleTextLeft
   } \
   elseif {$fileType=="LINK"} \
   {
#puts "add link $fileName"
     set n 0
     set l [$restoreFilesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$restoreFilesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add link item
     if {!$excludedFlag} { set image $images(link) } else { set image $images(linkExcluded) }
     $restoreFilesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "LINK" "NONE" 0]
     $restoreFilesTreeWidget item create $itemPath 1 -itemtype text -text "LINK" -style $styleTextLeft
     $restoreFilesTreeWidget item create $itemPath 2 -itemtype text              -style $styleTextLeft
     $restoreFilesTreeWidget item create $itemPath 3 -itemtype text              -style $styleTextLeft
   }
}

#***********************************************************************
# Name   : openCloseBackupDirectory
# Purpose: open/close backup directory
# Input  : itemPath - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc closeRestoreDirectory { itemPath } \
{
  global restoreFilesTreeWidget restoreIncludedListWidget restoreExcludedListWidget images

  # get directory name
  set directoryName [itemPathToFileName $restoreFilesTreeWidget $itemPath 0]

  # get data
  set data [$restoreFilesTreeWidget info data $itemPath]
  set type              [lindex $data 0]
  set state             [lindex $data 1]
  set directoryOpenFlag [lindex $data 2]

  # close
  $restoreFilesTreeWidget delete offsprings $itemPath
  if     {$state == "INCLUDED"} { set image $images(folderIncluded) } \
  elseif {$state == "EXCLUDED"} { set image $images(folderExcluded) } \
  else                          { set image $images(folder)         }
  $restoreFilesTreeWidget item configure $itemPath 0 -image $image

  # update data
  lset data 2 0
  $restoreFilesTreeWidget entryconfigure $itemPath -data $data
}

#***********************************************************************
# Name   : openCloseRestoreDirectory
# Purpose: open/close restore directory
# Input  : itemPath - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc openCloseRestoreDirectory { itemPath } \
{
  global restoreFilesTreeWidget restoreIncludedListWidget restoreExcludedListWidget images

  # get archive name

  # get directory name
  set directoryName [itemPathToFileName $restoreFilesTreeWidget $itemPath 1]
  set archiveName   [itemPathToPrefix   $restoreFilesTreeWidget $itemPath]
puts "item=$itemPath dir=$directoryName archiveName=$archiveName"

  # check if existing, add if not exists
  if {![$restoreFilesTreeWidget info exists $itemPath]} \
  {
    addRestoreEntry $archiveName $directoryName "DIRECTORY" 0 0
  }

  # check if parent exist and is open, open if needed
  set parentItemPath [$restoreFilesTreeWidget info parent $itemPath]
  if {[$restoreFilesTreeWidget info exists $parentItemPath]} \
  {
    set data [$restoreFilesTreeWidget info data $parentItemPath]
    if {[lindex $data 2] == 0} \
    {
      openCloseRestoreDirectory $parentItemPath
    }
  }

  # get data
  set data [$restoreFilesTreeWidget info data $itemPath]
  set type              [lindex $data 0]
  set state             [lindex $data 1]
  set directoryOpenFlag [lindex $data 2]

  if {$type == "DIRECTORY"} \
  {
    # get open/closed flag

    $restoreFilesTreeWidget delete offsprings $itemPath
    if {!$directoryOpenFlag} \
    {
      if     {$state == "INCLUDED"} { set image $images(folderIncludedOpen) } \
      elseif {$state == "EXCLUDED"} { set image $images(folderExcludedOpen) } \
      else                          { set image $images(folderOpen)         }
      $restoreFilesTreeWidget item configure $itemPath 0 -image $image
      update

      set errorCode 0
      BackupServer:executeCommand errorCode errorText "CLEAR"
#      BackupServer:executeCommand errorCode errorText "ADD_INCLUDE_PATTERN" "REGEX" "^$directoryName/\[^/\]+"
      set commandId [BackupServer:sendCommand "ARCHIVE_LIST" $archiveName]
      while {[BackupServer:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
      {
#puts "add file #$result#"
        if     {[scanx $result "FILE %ld %ld %ld %ld %d %d %S" fileSize archiveSize fragmentOffset fragmentSize compressAlgorithm cryptAlgorithm fileName] > 0} \
        {
          addRestoreEntry $archiveName $fileName "FILE" $fileSize 0
        } \
        elseif {[scanx $result "DIRECTORY %d %S" cryptAlgorithm directoryName] > 0} \
        {
          addRestoreEntry $archiveName $directoryName "DIRECTORY" 0 1
        } \
        elseif {[scanx $result "LINK %d %S %S " cryptAlgorithm linkName fileName] > 0} \
        {
          addRestoreEntry $archiveName $linkName "LINK" 0 0
        } else {
  internalError "unknown file type in openclosedirectory: $result"
}
      }

      set directoryOpenFlag 1
    } \
    else \
    {
      if     {$state == "INCLUDED"} { set image $images(folderIncluded) } \
      elseif {$state == "EXCLUDED"} { set image $images(folderExcluded) } \
      else                          { set image $images(folder)         }
      $restoreFilesTreeWidget item configure $itemPath 0 -image $image

      set directoryOpenFlag 0
    }

    # update data
    lset data 2 $directoryOpenFlag
    $restoreFilesTreeWidget entryconfigure $itemPath -data $data
  }
}

proc newRestoreArchive { } \
{
  global restoreFilesTreeWidget barConfig images

  # delete old of exists
  foreach itemPath [$restoreFilesTreeWidget info children ""] \
  {
    $restoreFilesTreeWidget delete all $itemPath
  }

  if {$barConfig(storageFileName) != ""} \
  {
    set styleImage    [tixDisplayStyle imagetext -refwindow $restoreFilesTreeWidget -anchor w]
    set styleTextLeft [tixDisplayStyle text      -refwindow $restoreFilesTreeWidget -anchor w]

    # add new
    switch $barConfig(storageType) \
    {
      "FILESYSTEM" \
      {
        set fileName $barConfig(storageFileName)
        set itemPath $barConfig(storageFileName)
        $restoreFilesTreeWidget add $itemPath -itemtype imagetext -text $fileName -image $images(folder) -style $styleImage -data [list "DIRECTORY" "NONE" 0]
        $restoreFilesTreeWidget item create $itemPath 1 -itemtype text -style $styleTextLeft
        $restoreFilesTreeWidget item create $itemPath 2 -itemtype text -style $styleTextLeft
        $restoreFilesTreeWidget item create $itemPath 3 -itemtype text -style $styleTextLeft
      }
      "SCP" \
      {
      }
      "SFTP" \
      {
      }
      default \
      {
      }
    }
  }
}

#***********************************************************************
# Name   : restoreUpdateFileTreeStates
# Purpose: update file tree states depending on include/exclude patterns
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc restoreUpdateFileTreeStates { } \
{
  global backupFilesTreeWidget barConfig images

  set itemPathList [$backupFilesTreeWidget info children ""]
  while {[llength $itemPathList] > 0} \
  {
    set fileName [lindex $itemPathList 0]; set itemPathList [lreplace $itemPathList 0 0]
    set itemPath [fileNameToItemPath $backupFilesTreeWidget "" $fileName]

    set data [$backupFilesTreeWidget info data $itemPath]
    set type              [lindex $data 0]
    set state             [lindex $data 1]
    set directoryOpenFlag [lindex $data 2]

    # add sub-directories to update
    if {($type == "DIRECTORY") && ($state != "EXCLUDED") && $directoryOpenFlag} \
    {
      foreach z [$backupFilesTreeWidget info children $itemPath] \
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
    elseif {($state == "INCLUDED") && !$includedFlag} \
    {
      if {$excludedFlag} \
      {
        set state "EXCLUDED"
      } \
      else \
      {
        set state "NONE"
      }
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
          $backupFilesTreeWidget delete offsprings $itemPath
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
    $backupFilesTreeWidget item configure [fileNameToItemPath $backupFilesTreeWidget "" $fileName] 0 -image $image

    # update data
    lset data 1 $state
    $backupFilesTreeWidget entryconfigure $itemPath -data $data
  }
}

#***********************************************************************
# Name   : restoreUpdateFileTreeStates
# Purpose: update file tree states depending on include/exclude patterns
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc updateFileTreeStates { widget prefixFlag } \
{
  global  barConfig images

  set itemPathList [$widget info children ""]
  while {[llength $itemPathList] > 0} \
  {
    set itemPath [lindex $itemPathList 0]; set itemPathList [lreplace $itemPathList 0 0]
    set fileName [itemPathToFileName $widget $itemPath $prefixFlag]

    set data [$widget info data $itemPath]
    set type              [lindex $data 0]
    set state             [lindex $data 1]
    set directoryOpenFlag [lindex $data 2]

    # add sub-directories to update
    if {($type == "DIRECTORY") && ($state != "EXCLUDED") && $directoryOpenFlag} \
    {
      foreach z [$widget info children $itemPath] \
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
    elseif {($state == "INCLUDED") && !$includedFlag} \
    {
      if {$excludedFlag} \
      {
        set state "EXCLUDED"
      } \
      else \
      {
        set state "NONE"
      }
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
          $widget delete offsprings $itemPath
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
    $widget item configure $itemPath 0 -image $image

    # update data
    lset data 1 $state
    $widget entryconfigure $itemPath -data $data
  }
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
  global barConfigFileName guiMode barConfigModifiedFlag

  if {$guiMode} \
  {  
    wm title . "$barConfigFileName"
  }
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
# Name   : resetBARConfig
# Purpose: reset bar config to default values
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc resetBARConfig {} \
{
  global barConfig

  set barConfig(storageType)               "FILESYSTEM"
  set barConfig(storageHostName)           ""
  set barConfig(storageLoginName)          ""
  set barConfig(storageFileName)           ""
  set barConfig(archivePartSizeFlag)       0
  set barConfig(archivePartSize)           0
  set barConfig(maxTmpSizeFlag)            0
  set barConfig(maxTmpSize)                0
  set barConfig(storageMode)               "NORMAL"
  set barConfig(incrementalListFileName)   ""
  set barConfig(maxBandWidthFlag)          0
  set barConfig(maxBandWidth)              0
  set barConfig(sshPassword)               ""
  set barConfig(sshPort)                   0
  set barConfig(compressAlgorithm)         "bzip9"
  set barConfig(cryptAlgorithm)            "none"
  set barConfig(cryptPasswordMode)         "DEFAULT"
  set barConfig(cryptPassword)             ""
  set barConfig(cryptPasswordVerify)       ""
  clearConfigModify
}

#***********************************************************************
# Name   : loadBARControlConfig
# Purpose: load BAR control config from file
# Input  : configFileName - config file name or ""
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc loadBARControlConfig { configFileName } \
{
  global barControlConfig passwordObfuscator errorCode

  if {($configFileName == "") || ![file exists $configFileName]} \
  {
    return;
  }

  # check access rights of file
  if {![string match {???00} [file attributes $configFileName -permissions]]} \
  {
    printWarning "Invalid permissions of config file '$configFileName' - skipped"
    return
  }

  # open file
  if {[catch {set handle [open $configFileName "r"]}]} \
  {
    printWarning "Cannot open config file '$configFileName' (error [lindex $errorCode 2])"
    return;
  }

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
    if     {[scanx $line "server = %s" s] == 1} \
    {
      set barControlConfig(serverHostName) $s
    } \
    elseif {[scanx $line "server-password = %S" s] == 1} \
    {
      set barControlConfig(serverPassword) [obfuscatePassword $s $passwordObfuscator]
    } \
    elseif {[scanx $line "server-port = %d" s] == 1} \
    {
      set barControlConfig(serverPort) $s
    } \
    elseif {[scanx $line "server-tls-port = %d" s] == 1} \
    {
      set barControlConfig(serverTLSPort) $s
    } \
    elseif {[scanx $line "server-ca-file-name = %S" s] == 1} \
    {
      set barControlConfig(serverCAFileName) $s
    } \
    elseif {[scanx $line "job-list-update-time = %d" s] == 1} \
    {
      set barControlConfig(jobListUpdateTime) $s
    } \
    elseif {[scanx $line "current-job-update-time = %d" s] == 1} \
    {
      set barControlConfig(currentJobUpdateTime) $s
    } \
    else \
    {
      printWarning "Unknown line '$line' in config file '$configFileName', line $lineNb - skipped"
    }
  }

  # close file
  close $handle
}

#***********************************************************************
# Name   : loadBARConfig
# Purpose: load BAR config from file
# Input  : configFileName - config file name or ""
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc loadBARConfig { configFileName } \
{
  global backupFilesTreeWidget backupIncludedListWidget backupExcludedListWidget
  global restoreFilesTreeWidget restoreIncludedListWidget restoreExcludedListWidget
  global tk_strictMotif barConfigFileName barConfigModifiedFlag barConfig guiMode errorCode

  # get file name
  if {$configFileName == ""} \
  {
    set configFileName [Dialog:fileSelector "Load configuration" "" {{"*.cfg" "BAR config"} {"*" "all"}}]
    if {$configFileName == ""} { return }
  }

  # open file
  if {[catch {set handle [open $configFileName "r"]}]} \
  {
    Dialog:error "Cannot open file '$configFileName' (error: [lindex $errorCode 2])"
    return;
  }

  # reset variables
  resetBARConfig
  if {$guiMode} \
  {
    clearFileList $backupFilesTreeWidget
    $backupIncludedListWidget delete 0 end
    $backupExcludedListWidget delete 0 end

    clearFileList $restoreFilesTreeWidget
    $restoreIncludedListWidget delete 0 end
    $restoreExcludedListWidget delete 0 end
  }

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
    if {[scanx $line "name = %S" s] == 1} \
    {
      # name = <name>
      set barConfig(name) $s
      continue
    }
    if {[scanx $line "archive-filename = %S" s] == 1} \
    {
      # archive-filename = <file name>
      if     {[regexp {^scp:([^@]*)@([^:]*):(.*)} $s * loginName hostName fileName]} \
      {
        set barConfig(storageType)      "SCP"
        set barConfig(storageLoginName) $loginName
        set barConfig(storageHostName)  $hostName
        set barConfig(storageFileName)  $fileName
      } \
      elseif {[regexp {^sftp:([^@]*)@([^:]*):(.*)} $s * loginName hostName fileName]} \
      {
        set barConfig(storageType)      "SFTP"
        set barConfig(storageLoginName) $loginName
        set barConfig(storageHostName)  $hostName
        set barConfig(storageFileName)  $fileName
      } \
      elseif {[regexp {^dvd:([^:]*):(.*)} $s * deviceName fileName]} \
      {
        set barConfig(storageType)       "DVD"
        set barConfig(storageDeviceName) $deviceName
        set barConfig(storageFileName)   $fileName
      } \
      elseif {[regexp {^dvd:(.*)} $s * fileName]} \
      {
        set barConfig(storageType)       "DVD"
        set barConfig(storageDeviceName) ""
        set barConfig(storageFileName)   $fileName
      } \
      elseif {[regexp {^([^:]*):(.*)} $s * deviceName fileName]} \
      {
        set barConfig(storageType)       "DEVICE"
        set barConfig(storageDeviceName) $deviceName
        set barConfig(storageFileName)   $fileName
      } \
      else \
      {
        set barConfig(storageType)      "FILESYSTEM"
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
    if {[scanx $line "volume-size = %s" s] == 1} \
    {
      # volume-size = <size>
      set barConfig(volumeSize) $s
      continue
    }
    if {[scanx $line "max-tmp-size = %s" s] == 1} \
    {
      # max-tmp-size = <size>
      set barConfig(maxTmpSizeFlag) 1
      set barConfig(maxTmpSize)     $s
      continue
    }
    if {[scanx $line "full = %S" s] == 1} \
    {
      # incremental = [yes|no]
      if {[stringToBoolean $s]} \
      {
        set barConfig(storageMode) "FULL"
      }
      continue
    }
    if {[scanx $line "incremental = %S" s] == 1} \
    {
      # incremental = [yes|no]
      if {[stringToBoolean $s]} \
      {
        set barConfig(storageMode) "INCREMENTAL"
      }
      continue
    }
    if {[scanx $line "incremental-list-file = %S" s] == 1} \
    {
      # incremental-list-file = <file name>
      set barConfig(incrementalListFileName) $s
      continue
    }
    if {[scanx $line "max-band-width = %s" s] == 1} \
    {
      # max-band-width = <bits/s>
      set barConfig(maxBandWidthFlag) 1
      set barConfig(maxBandWidth)     $s
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
    if {[scanx $line "crypt-password-mode = %S" s] == 1} \
    {
      # crypt-password-mode = none|default|ask|config
      if     {$s == "default"} { set barConfig(cryptPasswordMode) "DEFAULT" } \
      elseif {$s == "ask"    } { set barConfig(cryptPasswordMode) "ASK"     } \
      elseif {$s == "config" } { set barConfig(cryptPasswordMode) "CONFIG"  } \
      else                     { set barConfig(cryptPasswordMode) "NONE"    }
      continue
    }
    if {[scanx $line "crypt-password = %S" s] == 1} \
    {
      # crypt-password = <password>
      set barConfig(cryptPassword)       $s
      set barConfig(cryptPasswordVerify) $s
      continue
    }
    if {[scanx $line "include = %S" s] == 1} \
    {
      # include = <filename|pattern>
      set pattern $s

      # add to include pattern list
      lappend barConfig(included) $pattern; set barConfig(included) [lsort -uniq $barConfig(included)]

      if {$guiMode} \
      {
        set fileName $pattern
        set itemPath [fileNameToItemPath $backupFilesTreeWidget "" $fileName]

        # add directory for entry
        if {![$backupFilesTreeWidget info exists $itemPath]} \
        {
          set directoryName [file dirname $fileName]
          set directoryItemPath [fileNameToItemPath $backupFilesTreeWidget "" $directoryName]
          catch {openCloseBackupDirectory $directoryItemPath}
        }

        # set state of entry to "included"
        if {[$backupFilesTreeWidget info exists $itemPath]} \
        {
          setEntryState $backupFilesTreeWidget $itemPath 0 "INCLUDED"
        }
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
      set barConfig(skipUnreadableFlag) [stringToBoolean $s]
      continue
    }
    if {[scanx $line "overwrite-archive-files = %s" s] == 1} \
    {
      # overwrite-archive-files = [yes|no]
      set barConfig(overwriteArchiveFilesFlag) [stringToBoolean $s]
      continue
    }
    if {[scanx $line "overwrite-files = %s" s] == 1} \
    {
      # overwrite-archive-files = [yes|no]
      set barConfig(overwriteFilesFlag) [stringToBoolean $s]
      continue
    }
    if {[scanx $line "ecc = %s" s] == 1} \
    {
      # ecc = [yes|no]
      set barConfig(errorCorrectionCodesFlag) [stringToBoolean $s]
      continue
    }
puts "Config $configFileName: unknown '$line'"
  }

  # close file
  close $handle

  if {$guiMode} \
  {
    updateFileTreeStates $backupFilesTreeWidget 0
    updateFileTreeStates $restoreFilesTreeWidget 1
  }

  set barConfigFileName $configFileName
  clearConfigModify
}

#***********************************************************************
# Name   : saveBARConfig
# Purpose: saveBAR config into file
# Input  : configFileName - config file name or ""
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc saveBARConfig { configFileName } \
{
  global backupIncludedListWidget backupExcludedListWidget tk_strictMotif barConfigFileName barConfig errorCode

  # get file name
  if {$configFileName == ""} \
  {
    set fileName $barConfigFileName
  }
  if {$configFileName == ""} \
  {
    set configFileName [Dialog:fileSelector "Save configuration" "" {{"*.cfg" "BAR config"} {"*" "all"}}]
    if {$configFileName == ""} { return }
  }

  # open temporay file
  set tmpFileName [getTmpFileName]
  if {[catch {set handle [open $tmpFileName "w"]}]} \
  {
    Dialog:error "Cannot open file '$tmpFileName' (error: [lindex $errorCode 2])"
    return;
  }

  # write file
  puts $handle "name = [escapeString $barConfig(name)]"
  switch $barConfig(storageType) \
  {
    "FILESYSTEM" \
    {
      puts $handle "archive-filename = [escapeString $barConfig(storageFileName)]"
    }
    "SCP" \
    {
      puts $handle "archive-filename = [escapeString scp:$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)]"
    }
    "SFTP" \
    {
      puts $handle "archive-filename = [escapeString sftp:$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)]"
    }
    "DVD" \
    {
      if {$barConfig(storageDeviceName) != ""} \
      {
        puts $handle "archive-filename = [escapeString dvd:$barConfig(storageDeviceName):$barConfig(storageFileName)]"
      } \
      else \
      {
        puts $handle "archive-filename = [escapeString dvd:$barConfig(storageFileName)]"
      }
    }
    "DEVICE" \
    {
      puts $handle "archive-filename = [escapeString $barConfig(storageDeviceName):$barConfig(storageFileName)]"
    }
    default \
    {
      internalError "unknown storage type '$barConfig(storageType)'"
    }
  }
  if {$barConfig(archivePartSizeFlag)} \
  {
    puts $handle "archive-part-size = $barConfig(archivePartSize)"
  }
  if {$barConfig(maxTmpSizeFlag)} \
  {
    puts $handle "max-tmp-size = $barConfig(maxTmpSize)"
  }
  switch $barConfig(storageMode) \
  {
    "FULL"        { puts $handle "full = yes" }
    "INCREMENTAL" { puts $handle "incremental = yes" }
  }
  puts $handle "incremental-list-file = $barConfig(incrementalListFileName)"
  if {$barConfig(maxBandWidthFlag)} \
  {
    puts $handle "max-band-width = $barConfig(maxBandWidth)"
  }
  puts $handle "ssh-port = $barConfig(sshPort)"
  puts $handle "ssh-public-key = $barConfig(sshPublicKeyFileName)"
  puts $handle "ssh-privat-key = $barConfig(sshPrivatKeyFileName)"
  puts $handle "compress-algorithm = [escapeString $barConfig(compressAlgorithm)]"
  puts $handle "crypt-algorithm = [escapeString $barConfig(cryptAlgorithm)]"
  switch $barConfig(cryptPasswordMode) \
  {
    "DEFAULT" { puts $handle "crypt-password-mode = default" }
    "ASK"     { puts $handle "crypt-password-mode = ask"     }
    "CONFIG"  { puts $handle "crypt-password-mode = config"  }
  }
  puts $handle "crypt-password = [escapeString $barConfig(cryptPassword)]"
  puts $handle "volume-size = $barConfig(volumeSize)"
  foreach pattern [$backupIncludedListWidget get 0 end] \
  {
    puts $handle "include = [escapeString $pattern]"
  }
  foreach pattern [$backupExcludedListWidget get 0 end] \
  {
    puts $handle "exclude = [escapeString $pattern]"
  }
  puts $handle "skip-unreadable = [booleanToString $barConfig(skipUnreadableFlag)]"
  puts $handle "overwrite-archive-files = [booleanToString $barConfig(overwriteArchiveFilesFlag)]"
  puts $handle "overwrite-files = [booleanToString $barConfig(overwriteFilesFlag)]"
  puts $handle "ecc = [booleanToString $barConfig(errorCorrectionCodesFlag)]"

  # close file
  close $handle

  # make backup file, rename temporary file
  catch {file rename $configFileName "$configFileName~"}
  if {[catch {file copy -force $tmpFileName $configFileName}]} \
  {
    Dialog:error "Cannot store file '$configFileName' (error: [lindex $errorCode 2])"
    catch {file rename "$configFileName~" $configFileName}
    catch {file delete $tmpFileName}
    return
  }
  catch {file delete $tmpFileName}

  # reset modify flag
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
      saveBARConfig $barConfigFileName
    }
  }

  BackupServer:disconnect

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
  global barConfig backupFilesTreeWidget restoreFilesTreeWidget

  if {$pattern == ""} \
  {
    # dialog
    set handle [Dialog:window "Add included pattern"]
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

    grid rowconfigure $handle    { 0 } -weight 1
    grid columnconfigure $handle { 0 } -weight 1

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
#  backupUpdateFileTreeStates
  updateFileTreeStates $backupFilesTreeWidget 0
  updateFileTreeStates $restoreFilesTreeWidget 1
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
  global barConfig backupFilesTreeWidget restoreFilesTreeWidget

  set index [lsearch -sorted -exact $barConfig(included) $pattern]
  if {$index >= 0} \
  {
    set barConfig(included) [lreplace $barConfig(included) $index $index]
#    backupUpdateFileTreeStates
    updateFileTreeStates $backupFilesTreeWidget 0
    updateFileTreeStates $restoreFilesTreeWidget 1
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
  global barConfig backupFilesTreeWidget restoreFilesTreeWidget

  if {$pattern == ""} \
  {
    # dialog
    set handle [Dialog:window "Add excluded pattern"]
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

    grid rowconfigure $handle    { 0 } -weight 1
    grid columnconfigure $handle { 0 } -weight 1

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
#  backupUpdateFileTreeStates
  updateFileTreeStates $backupFilesTreeWidget 0
  updateFileTreeStates $restoreFilesTreeWidget 1
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
  global barConfig backupFilesTreeWidget restoreFilesTreeWidget

  set index [lsearch -sorted -exact $barConfig(excluded) $pattern]
  if {$index >= 0} \
  {
    set barConfig(excluded) [lreplace $barConfig(excluded) $index $index]
#    backupUpdateFileTreeStates
    updateFileTreeStates $backupFilesTreeWidget 0
    updateFileTreeStates $restoreFilesTreeWidget 1
    setConfigModify
  }
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : editStorageFileName
# Purpose: edit storage file name
# Input  : fileName - file name
# Output : -
# Return : file name or ""
# Notes  : -
#***********************************************************************

proc editStorageFileName { fileName } \
{
  set font "-*-helvetica-*-r-*-*-18-*-*-*-*-*-*-*"

  set partList \
  {
    {0  1 "#"     "part number 1 digit"            "1"}
    {0  2 "##"    "part number 2 digit"            "12"}
    {0  3 "###"   "part number 3 digit"            "123"}
    {0  4 "####"  "part number 4 digit"            "1234"}

    {0  6 "%type" "archive type: full,incremental" "full"}
    {0  7 "%last" "'-last' if last archive part"   "-last"}

    {1  1 "%d"    "day 01..31"                     "24"}
    {1  2 "%j"    "day of year 001..366"           "354"}
    {1  3 "%m"    "month 01..12"                   "12"}
    {1  4 "%b"    "month name"                     "Dec"}
    {1  5 "%B"    "full month name"                "December"}
    {1  6 "%H"    "hour 00..23"                    "11"}
    {1  7 "%I"    "hour 00..12"                    "23"}
    {1  8 "%M"    "minute 00..59"                  "55"}
    {1  9 "%p"    "'AM' or 'PM'"                   "PM"}
    {1 10 "%P"    "'am' or 'pm'"                   "pm"}
    {1 11 "%a"    "week day name"                  "Mon"}
    {1 12 "%A"    "full week day name"             "Monday"}
    {1 13 "%u"    "day of week 1..7"               "1"}
    {1 14 "%w"    "day of week 0..6"               "1"}
    {1 15 "%U"    "week number 1..52"              "51"}
    {1 16 "%C"    "century two digit"              "07"}
    {1 17 "%Y"    "year four digit"                "2007"}
    {1 18 "%S"    "seconds since 1.1.1970 00:00"   "1198508100"}
    {1 19 "%Z"    "time-zone abbreviation"         "JSP"}

    {2  1 "%%"    "%"                              "%"}
  }

  # add part to part list
  proc addPart { handle widget column row part description } \
  {
    label $handle.parts.p$row$column -text $part -borderwidth 1 -background grey -relief raised
    grid $handle.parts.p$row$column -row $row -column [expr {$column*2+0}] -sticky "w"
    label $handle.parts.d$row$column -text $description
    grid $handle.parts.d$row$column -row $row -column [expr {$column*2+1}] -sticky "w"
    bind $handle.parts.p$row$column <Button-1> \
    "
      $handle.parts.p$row$column configure -background green
      set selectedId \[Dialog:get $handle selectedId\]
      if {\$selectedId != \"\"} { $widget itemconfigure \$selectedId -outline \"\" }
      Dialog:set $handle selectedIndex \"\"
      Dialog:set $handle selectedId    \"\"
      Dialog:set $handle newPart       [string map {% %%} $part]
      Dialog:set $handle newPartWidget $handle.parts.p$row$column
    "
  }

  # update file name part list
  proc updateList { handle widget fileNamePartList } \
  {
    set partList [Dialog:get $handle partList]
    set font     [Dialog:get $handle font]

    proc expandPart { partList part } \
    {
      foreach z $partList \
      {
        if {[lindex $z 2]==$part} { return [lindex $z 4] }
      }
      return $part
    }

    $widget delete all
    Dialog:set $handle example ""
    set i 0
    set x 1
    foreach part $fileNamePartList \
    {
      if {$x>1} \
      {
        set separatorId [$widget create rectangle $x 2 [expr {$x+6}] 20 -fill lightblue -outline black -tags [list -$i]]
        incr x 8
      }
      set textId [$widget create text $x 10 -text $part -anchor w -fill black -font $font]
      set textWidth [font measure [$widget itemcget $textId -font] $part]
      set rectangleId [$widget create rectangle $x 1 [expr {$x+$textWidth-1}] 19 -fill "" -outline "" -tags [list $i]]
      $widget raise $textId

      $widget bind $textId <Button-1> \
      "
        set selectedId \[Dialog:get $handle selectedId\]
        if {\$selectedId != \"\"} { $widget itemconfigure \$selectedId -outline \"\" }
        $widget itemconfigure $rectangleId -outline red
        Dialog:set $handle selectedIndex $i
        Dialog:set $handle selectedId    $rectangleId
      "

      incr x [expr {$textWidth+1}]
      incr i

      set example [Dialog:get $handle example]
      append example [expandPart $partList $part]
      Dialog:set $handle example $example
    }
    set separatorId [$widget create rectangle $x 2 [expr {$x+6}] 20 -fill lightblue -outline black -tags [list -$i]]

    Dialog:set $handle selectedIndex ""
    Dialog:set $handle selectedId    ""
  }

  set handle [Dialog:window "Edit storage file name"]
  Dialog:addVariable $handle result           -1
  Dialog:addVariable $handle partList         $partList
  Dialog:addVariable $handle font             $font
  Dialog:addVariable $handle fileNamePartList {}
  Dialog:addVariable $handle newPart          ""
  Dialog:addVariable $handle newPartWidget    ""
  Dialog:addVariable $handle selectedIndex    ""
  Dialog:addVariable $handle selectedId       ""

  frame $handle.edit
    label $handle.edit.dataTitle -text "File name:"
    grid $handle.edit.dataTitle -row 0 -column 0 -sticky "w"
    canvas $handle.edit.data -width 400 -height 20
    grid $handle.edit.data -row 0 -column 1 -sticky "we" -padx 2p -pady 2p

    label $handle.edit.exampleTitle -text "Example:"
    grid $handle.edit.exampleTitle -row 1 -column 0 -sticky "w"
    entry $handle.edit.example -textvariable [Dialog:variable $handle example] -state readonly
    grid $handle.edit.example -row 1 -column 1 -sticky "we"

    grid columnconfigure $handle.edit { 1 } -weight 1
  pack $handle.edit -fill x -padx 2p -pady 2p

  frame $handle.parts
    label $handle.parts.textTitle -text "Text" -borderwidth 1 -background grey -relief raised
    grid $handle.parts.textTitle -row 0 -column 0 -sticky "w"
    entry $handle.parts.text -background white
    grid $handle.parts.text -row 0 -column 1 -columnspan 5 -sticky "we"
    bind $handle.parts.textTitle <Button-1> \
    "
      $handle.parts.textTitle configure -background green
      Dialog:set $handle newPart       \[string map {%% %%%%} \[$handle.parts.text get\]\]
      Dialog:set $handle newPartWidget $handle.parts.textTitle
    "

    foreach part $partList \
    {
      addPart \
        $handle \
        $handle.edit.data \
        [lindex $part 0] \
        [lindex $part 1] \
        [lindex $part 2] \
        [lindex $part 3]
    }
    grid columnconfigure $handle.parts { 6 } -weight 1
  pack $handle.parts -fill both -expand yes -padx 2p -pady 2p

  frame $handle.buttons
    button $handle.buttons.ok -text "OK" -command "Dialog:set $handle result 1; Dialog:close $handle"
    pack $handle.buttons.ok -side left -padx 2p
    bind $handle.buttons.ok <Return> "$handle.buttons.ok invoke"
    button $handle.buttons.cancel -text "Cancel" -command "Dialog:set $handle result 0; Dialog:close $handle"
    pack $handle.buttons.cancel -side right -padx 2p
    bind $handle.buttons.cancel <Return> "$handle.buttons.cancel invoke"
  pack $handle.buttons -side bottom -fill x -padx 2p -pady 2p

  bind $handle <Escape> "$handle.buttons.cancel invoke"
  bind $handle <BackSpace> "event generate $handle <<Event_delete>>"
  bind $handle <Delete> "event generate $handle <<Event_delete>>"

  proc editStorageFileName:motion { handle widget x y } \
  {
    set newPart [Dialog:get $handle newPart]
    if {$newPart != ""} \
    {
      if {[winfo containing $x $y]==$widget} \
      {
        set wx [expr {[$widget canvasx $x]-[winfo rootx $widget]}]
        set wy [expr {[$widget canvasx $y]-[winfo rooty $widget]}]
        set selectedId [Dialog:get $handle selectedId]
        set ids [$widget find overlapping $wx $wy $wx $wy]
        foreach id $ids \
        {
          if {[$widget type $id]=="rectangle"} \
          {
            if {($selectedId!="") && ("$id"!="$selectedId")} \
            {
              $widget itemconfigure $selectedId -fill ""
            }
            $widget itemconfigure $id -fill green
            Dialog:set $handle selectedId $id
          }
        }
      } \
      else \
      {
        set selectedId [Dialog:get $handle selectedId]
        if {$selectedId!=""} \
        {
          $widget itemconfigure $selectedId -fill ""
        }
        Dialog:set $handle selectedId ""
      }
    }
  }
  proc editStorageFileName:drop { handle widget x y } \
  {
    set newPart       [Dialog:get $handle newPart]
    set newPartWidget [Dialog:get $handle newPartWidget]
    set selectedId    [Dialog:get $handle selectedId]
    if {$newPart != ""} \
    {
      if {$selectedId != ""} \
      {
        set fileNamePartList [Dialog:get $handle fileNamePartList]
#puts "fileNamePartList1=$fileNamePartList"

        set index [$widget gettags $selectedId]
        if {$index >= 0} \
        {
          set fileNamePartList [lreplace $fileNamePartList $index $index $newPart]
        } \
        else \
        {
          set fileNamePartList [linsert $fileNamePartList [expr {abs($index)}] $newPart]
        }
        Dialog:set $handle fileNamePartList $fileNamePartList
        updateList $handle $handle.edit.data $fileNamePartList

        Dialog:set $handle selectedIndex ""
        Dialog:set $handle selectedId    ""
      }
      Dialog:set $handle newPart    ""
    }
    if {$newPartWidget!=""} \
    {
      $newPartWidget configure -background grey
      Dialog:set $handle newPartWidget ""
    }
  }
  proc editStorageFileName:delete { handle widget } \
  {
    set selectedIndex [Dialog:get $handle selectedIndex]
    if {$selectedIndex!=""} \
    {
puts "selectedIndex=$selectedIndex v=[Dialog:get $handle fileNamePartList]"
      set fileNamePartList [lreplace [Dialog:get $handle fileNamePartList] $selectedIndex $selectedIndex]
puts "n=$fileNamePartList"
      Dialog:set $handle fileNamePartList $fileNamePartList
      updateList $handle $handle.edit.data $fileNamePartList
    }
    Dialog:set $handle newPart       ""
    Dialog:set $handle newPartWidget ""
    Dialog:set $handle selectedIndex ""
    Dialog:set $handle selectedId    ""
  }
  bind $handle <B1-Motion>       "catch {editStorageFileName:motion $handle $handle.edit.data %X %Y}"
  bind $handle <ButtonRelease-1> "catch {editStorageFileName:drop   $handle $handle.edit.data %X %Y}"
  bind $handle <<Event_delete>>  "puts 1; catch {editStorageFileName:delete $handle $handle.edit.data}"

  focus $handle.parts.text

  # parse file name
  set fileNamePartList {}
  set s $fileName
  while {[regexp {([^%#]*)(%\w+|#+)(.*)} $s * prefix part postfix]} \
  {
    if {$prefix != ""} { lappend fileNamePartList $prefix }
    lappend fileNamePartList $part
    set s $postfix
  }
  if {$s != ""} { lappend fileNamePartList $s }
  Dialog:set $handle fileNamePartList $fileNamePartList
  updateList $handle $handle.edit.data $fileNamePartList

  Dialog:show $handle

  set result           [Dialog:get $handle result          ]
  set fileNamePartList [Dialog:get $handle fileNamePartList]
  Dialog:delete $handle
  if {$result != 1} { return "" }

  return [join $fileNamePartList ""]
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : addBackupJob
# Purpose: add new backup job
# Input  : jobListWidget - job list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addBackupJob { jobListWidget } \
{
  global backupIncludedListWidget backupExcludedListWidget barConfig currentJob fullFlag incrementalFlag guiMode

  set errorCode 0
  set errorText ""

  # clear settings
  BackupServer:executeCommand errorCode errorText "CLEAR"

  # add included directories/files
  foreach pattern $barConfig(included) \
  {
    BackupServer:executeCommand errorCode errorText "ADD_INCLUDE_PATTERN" "GLOB" [escapeString $pattern]
  }

  # add excluded directories/files
  foreach pattern $barConfig(excluded) \
  {
    BackupServer:executeCommand errorCode errorText "ADD_EXCLUDE_PATTERN" "GLOB" [escapeString $pattern]
  }

  # set other parameters
  BackupServer:executeCommand errorCode errorText "SET" "archive-part-size"       $barConfig(archivePartSize)
  if       {$fullFlag} \
  {
     BackupServer:executeCommand errorCode errorText "SET" "archive-type" "full"
  } elseif {$incrementalFlag} \
  {
     BackupServer:executeCommand errorCode errorText "SET" "archive-type" "incremental"
  } \
  else \
  {
    switch $barConfig(storageMode) \
    {
      "FULL"        { BackupServer:executeCommand errorCode errorText "SET" "archive-type" "full"        }
      "INCREMENTAL" { BackupServer:executeCommand errorCode errorText "SET" "archive-type" "incremental" }
    }
  }
  if {$barConfig(incrementalListFileName) != ""} \
  {
    BackupServer:executeCommand errorCode errorText "SET" "incremental-list-file" $barConfig(incrementalListFileName)
  }
  BackupServer:executeCommand errorCode errorText "SET" "max-tmp-size"            $barConfig(maxTmpSize)
  BackupServer:executeCommand errorCode errorText "SET" "max-band-width"          $barConfig(maxBandWidth)
  BackupServer:executeCommand errorCode errorText "SET" "ssh-port"                $barConfig(sshPort)
  BackupServer:executeCommand errorCode errorText "SET" "compress-algorithm"      $barConfig(compressAlgorithm)
  BackupServer:executeCommand errorCode errorText "SET" "crypt-algorithm"         $barConfig(cryptAlgorithm)
  switch $barConfig(cryptPasswordMode) \
  {
    "DEFAULT" \
    {
    }
    "ASK" \
    {
      set password [getPassword "Crypt password" 1 0]
      if {$password == ""} \
      {
        return
      }
      BackupServer:executeCommand errorCode errorText "SET" "crypt-password" $password
    }
    "CONFIG" \
    {
      if {$barConfig(cryptPassword) != $barConfig(cryptPasswordVerify)} \
      {
        if {$guiMode} \
        {
          Dialog:error "Crypt passwords are not equal!"
        } \
        else \
        {
          puts stderr "Crypt passwords are not equal!"
        }
        return
      }
      if {$barConfig(cryptPassword) == ""} \
      {
        if {$guiMode} \
        {
          Dialog:error "No crypt passwords given!"
        } \
        else \
        {
          puts stderr "No crypt passwords given!"
        }
        return
      }
      BackupServer:executeCommand errorCode errorText "SET" "crypt-password" $barConfig(cryptPassword)
    }
  } 
  BackupServer:executeCommand errorCode errorText "SET" "volume-size"             $barConfig(volumeSize)
  BackupServer:executeCommand errorCode errorText "SET" "skip-unreadable"         $barConfig(skipUnreadableFlag)
  BackupServer:executeCommand errorCode errorText "SET" "overwrite-archive-files" $barConfig(overwriteArchiveFilesFlag)
  BackupServer:executeCommand errorCode errorText "SET" "overwrite-files"         $barConfig(overwriteFilesFlag)
  BackupServer:executeCommand errorCode errorText "SET" "ecc"                     $barConfig(errorCorrectionCodesFlag)

  # add jobs archive file name
  if     {$barConfig(storageType) == "FILESYSTEM"} \
  {
    set archiveFileName $barConfig(storageFileName)
  } \
  elseif {$barConfig(storageType) == "SCP"} \
  {
    set archiveFileName "scp:$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)"
  } \
  elseif {$barConfig(storageType) == "SFTP"} \
  {
    set archiveFileName "sftp:$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)"
  } \
  elseif {$barConfig(storageType) == "DVD"} \
  {
    set archiveFileName "dvd:$barConfig(storageDeviceName):$barConfig(storageFileName)"
  } \
  elseif {$barConfig(storageType) == "DEVICE"} \
  {
    set archiveFileName "$barConfig(storageDeviceName):$barConfig(storageFileName)"
  } \
  else \
  {
    internalError "unknown storage type '$barConfig(storageType)'"
  }
  if {$errorCode == 0} \
  {
    if {![BackupServer:executeCommand errorCode errorText "ADD_JOB" "BACKUP" [escapeString $barConfig(name)] [escapeString $archiveFileName]]} \
    {
      if {$guiMode} \
      {
        Dialog:error "Error adding new job: $errorText"
      } \
      else \
      {
        puts stderr "Error adding new job: $errorText"
      }
      return
    }
  } \
  else \
  {
    if {$guiMode} \
    {
      Dialog:error "Error adding new job: $errorText"
    } \
    else \
    {
      puts stderr "Error adding new job: $errorText"
    }
    return
  }

  if {$guiMode} \
  {
    updateJobList $jobListWidget
  }
}

#***********************************************************************
# Name   : addBackupJob
# Purpose: add new backup job
# Input  : jobListWidget - job list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addRestoreJob { jobListWidget } \
{
  global backupIncludedListWidget backupExcludedListWidget barConfig currentJob guiMode

  set errorCode 0

  # clear settings
  BackupServer:executeCommand errorCode errorText "CLEAR"

  # add included directories/files
  foreach pattern $barConfig(included) \
  {
    BackupServer:executeCommand errorCode errorText "ADD_INCLUDE_PATTERN" "GLOB" [escapeString $pattern]
  }

  # add excluded directories/files
  foreach pattern $barConfig(excluded) \
  {
    BackupServer:executeCommand errorCode errorText "ADD_EXCLUDE_PATTERN" "GLOB" [escapeString $pattern]
  }

  # set other parameters
  BackupServer:executeCommand errorCode errorText "SET" "max-band-width"          $barConfig(maxBandWidth)
  BackupServer:executeCommand errorCode errorText "SET" "ssh-port"                $barConfig(sshPort)
  BackupServer:executeCommand errorCode errorText "SET" "overwrite-files"         $barConfig(overwriteFilesFlag)
  BackupServer:executeCommand errorCode errorText "SET" "destination-directory"   $barConfig(destinationDirectoryName)
  BackupServer:executeCommand errorCode errorText "SET" "destination-strip-count" $barConfig(destinationStripCount)

  # add jobs archive file name
  if     {$barConfig(storageType) == "FILESYSTEM"} \
  {
    set archiveFileName $barConfig(storageFileName)
  } \
  elseif {$barConfig(storageType) == "SCP"} \
  {
    set archiveFileName "scp:$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)"
  } \
  elseif {$barConfig(storageType) == "SFTP"} \
  {
    set archiveFileName "sftp:$barConfig(storageLoginName)@$barConfig(storageHostName):$barConfig(storageFileName)"
  } \
  elseif {$barConfig(storageType) == "DEVICE"} \
  {
    set archiveFileName "$barConfig(storageDeviceName):$barConfig(storageFileName)"
  } \
  else \
  {
    internalError "unknown storage type '$barConfig(storageType)'"
  }
  if {$errorCode == 0} \
  {
    if {![BackupServer:executeCommand errorCode errorText "ADD_JOB" "RESTORE" [escapeString $barConfig(name)] [escapeString $archiveFileName]]} \
    {
      Dialog:error "Error adding new job: $errorText"
    }
  } \
  else \
  {
    Dialog:error "Error adding new job: $errorText"
  }

  if {$guiMode} \
  {
    updateJobList $jobListWidget
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
  set errorCode 0
  BackupServer:executeCommand errorCode errorText "REM_JOB" $id

  updateJobList $jobListWidget
}

#***********************************************************************
# Name   : abortJob
# Purpose: abort running job
# Input  : jobListWidget - job list widget
#          id            - job id
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc abortJob { jobListWidget id } \
{
  global guiMode

  set errorCode 0
  BackupServer:executeCommand errorCode errorText "ABORT_JOB" $id

  if {$guiMode} \
  {
    updateJobList $jobListWidget
  }
}

# ----------------------------- main program  -------------------------------

# read barcontrol config
loadBARControlConfig "$env(HOME)/.bar/barcontrol.cfg"

# parse command line arguments
set guiMode 1
set z 0
while {$z<[llength $argv]} \
{
  switch -regexp -- [lindex $argv $z] \
  {
    "^--help$" \
    {
      printUsage
      exit 1
    }
    "^-h=" - \
    "^--host=" \
    {
      set s [string range [lindex $argv $z] [expr {[string first "=" [lindex $argv $z]]+1}] end]
      set hostName $s
    }

    "^-h$" - \
    "^--host$" \
    {
      incr z
      if {$z >= [llength $argv]} \
      {
        printError "No argument given for '[llength $argv]'. Expected host name."
        exit 1
      }
      set barControlConfig(serverHostName) [lindex $argv $z]
    }
    "^-p=" - \
    "^--port=" \
    {
      set s [string range [lindex $argv $z] [expr {[string first "=" [lindex $argv $z]]+1}] end]
      if {![string is integer $s]} \
      {
        printError "No a port number!"
        exit 1
      }
      set barControlConfig(serverPort) $s
    }
    "^-p$" - \
    "^--port$" \
    {
      incr z
      if {$z >= [llength $argv]} \
      {
        printError "No argument given for '[llength $argv]'. Expected port number."
        exit 1
      }
      if {![string is integer [lindex $argv $z]]} \
      {
        printError "No a port number!"
        exit 1
      }
      set barControlConfig(serverPort) $s
    }
    "^--tls-port=" - \
    "^--ssl-port=" \
    {
      set s [string range [lindex $argv $z] [expr {[string first "=" [lindex $argv $z]]+1}] end]
      if {![string is integer $s]} \
      {
        printError "No a port number!"
        exit 1
      }
      set barControlConfig(serverTLSPort) $s
    }
    "^--tls-port$" - \
    "^--ssl-port$" \
    {
      incr z
      if {$z >= [llength $argv]} \
      {
        printError "No argument given for '[llength $argv]'. Expected port number."
        exit 1
      }
      if {![string is integer [lindex $argv $z]]} \
      {
        printError "No a port number!"
        exit 1
      }
      set barControlConfig(serverTLSPort) [lindex $argv $z]
    }
    "^--list$" \
    {
      set listFlag 1
      set guiMode 0
    }
    "^--start$" \
    {
      set startFlag 1
    }
    "^--full$" \
    {
      set fullFlag 1
    }
    "^--incremental$" \
    {
      set incrementalFlag 1
    }
    "^--abort$" \
    {
      incr z
      if {$z >= [llength $argv]} \
      {
        printError "No argument given for '[llength $argv]'. Expected id."
        exit 1
      }
      set abortId [lindex $argv $z]
      set guiMode 0
    }
    "^--quit$" \
    {
      set quitFlag 1
      set guiMode 0
    }
    "^--password=" \
    {
      set s [string range [lindex $argv $z] [expr {[string first "=" [lindex $argv $z]]+1}] end]
      set password $s
    }
    "^--password$" \
    {
      incr z
      if {$z >= [llength $argv]} \
      {
        printError "No argument given for '[llength $argv]'. Expected password."
        exit 1
      }
      set password [obfuscatePassword [lindex $argv $z] $passwordObfuscator]
    }
    "^--$" \
    {
      break
    }
    "^-" \
    {
      printError "Unknown option '[lindex $argv $z]'!"
      exit 1
    }
    default \
    {
      set configFileName [lindex $argv $z]
    }
  }
  incr z
}
while {$z<[llength $argv]} \
{
  set configFileName [lindex $argv $z]
  incr z
}

if {![info exists tk_version] && !$guiMode} \
{
  # get password
  if {$barControlConfig(serverPassword) == ""} \
  {
    set barControlConfig(serverPassword) [getPassword "Server password" 0 1]
    if {$barControlConfig(serverPassword) == ""} \
    {
      printError "No password given"
      exit 1
    }
  }

  # connect to server
  if     {($barControlConfig(serverTLSPort) != 0) && ![catch {tls::init -version}]} \
  {
    if {[catch {tls::init -cafile $barControlConfig(serverCAFileName)}]} \
    {
      printError "Cannot initialise TLS/SSL system"
      exit 1
    }
    if {![BackupServer:connect $barControlConfig(serverHostName) $barControlConfig(serverTLSPort) $barControlConfig(serverPassword) 1]} \
    {
      printError "Cannot connect to TLS/SSL server '$barControlConfig(serverHostName):$barControlConfig(serverTLSPort)'!"
      exit 1
    }
  } \
  elseif {$barControlConfig(serverPort) != 0} \
  {
    if {![BackupServer:connect $barControlConfig(serverHostName) $barControlConfig(serverPort) $barControlConfig(serverPassword) 0]} \
    {
      printError "Cannot connect to server '$barControlConfig(serverHostName):$barControlConfig(serverPort)'!"
      exit 1
    }
  } \
  else  \
  {
    printError "Cannot connect to server '$barControlConfig(serverHostName)'!"
    exit 1
  }

  # non-GUI commands
  if {$listFlag} \
  {
    set formatString "%3s %-20s %-20s %10s %-10s %-10s %-20s %-20s"

    set s [format $formatString "Id" "Name" "State" "Part size" "Compress" "Crypt" "Started" "Estimate time"]
    puts $s
    puts [string repeat "-" [string length $s]]
    set commandId [BackupServer:sendCommand "JOB_LIST"]
    while {[BackupServer:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
    {
      scanx $result "%d %S %S %d %S %S %d %d" \
        id \
        name \
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

      puts [format $formatString \
        $id \
        $name \
        $state \
        [formatByteSize $archivePartSize] \
        $compressAlgorithm \
        $cryptAlgorithm \
        [expr {($startTime > 0)?[clock format $startTime -format "%Y-%m-%d %H:%M:%S"]:"-"}] \
        [format "%d days %02d:%02d:%02d" $estimatedRestDays $estimatedRestHours $estimatedRestMinutes $estimatedRestSeconds] \
      ]
    }
    puts [string repeat "-" [string length $s]]
  }
  if {$startFlag} \
  {
    if {$configFileName != ""} \
    {
      loadBARConfig $configFileName
      addBackupJob ""
    }
  }
  if {$abortId != 0} \
  {
    abortJob "" $abortId
  }
  if {$quitFlag} \
  {
    # disconnect
    BackupServer:disconnect
    exit 0
  }
}

# run in GUI mode
if {![info exists tk_version]} \
{
  if {[info exists env(DISPLAY)] && ($env(DISPLAY) != "")} \
  {
    if {[catch {[eval "exec wish $argv0 -- [join $argv] >@stdout 2>@stderr"]}]} \
    {
      exit 1
    }
  } \
  else \
  {
    if {[catch {[eval "exec wish $argv0 -display :0.0 -- [join $argv] >@stdout 2>@stderr"]}]} \
    {
      exit 1
    }
  }
  exit 0
}

# get password
if {$barControlConfig(serverPassword) == ""} \
{
  wm state . withdrawn
  set barControlConfig(serverPassword) [getPassword "Server password" 0 1]
  if {$barControlConfig(serverPassword) == ""} \
  {
    printError "No password given"
    exit 1
  }
}

# init main window
set mainWindow ""
wm title . "BAR control"
wm iconname . "BAR"
wm geometry . "800x600"
wm protocol . WM_DELETE_WINDOW quit
wm state . normal

# menu
frame $mainWindow.menu -relief raised -bd 2
  menubutton $mainWindow.menu.file -text "Program" -menu $mainWindow.menu.file.items -underline 0
  menu $mainWindow.menu.file.items
  $mainWindow.menu.file.items add command -label "New..."     -accelerator "Ctrl-n" -command "event generate . <<Event_new>>"
  $mainWindow.menu.file.items add command -label "Load..."    -accelerator "Ctrl-o" -command "event generate . <<Event_load>>"
  $mainWindow.menu.file.items add command -label "Save"       -accelerator "Ctrl-s" -command "event generate . <<Event_save>>"
  $mainWindow.menu.file.items add command -label "Save as..."                       -command "event generate . <<Event_saveAs>>"
  $mainWindow.menu.file.items add separator
#  $mainWindow.menu.file.items add command -label "Start"                            -command "event generate . <<Event_start>>"
#  $mainWindow.menu.file.items add separator
  $mainWindow.menu.file.items add command -label "Quit"       -accelerator "Ctrl-q" -command "event generate . <<Event_quit>>"
  pack $mainWindow.menu.file -side left

#  menubutton $mainWindow.menu.edit -text "Edit" -menu $mainWindow.menu.edit.items -underline 0
#  menu $mainWindow.menu.edit.items
#  $mainWindow.menu.edit.items add command -label "None"    -accelerator "*" -command "event generate . <<Event_backupStateNone>>"
#  $mainWindow.menu.edit.items add command -label "Include" -accelerator "+" -command "event generate . <<Event_backupStateIncluded>>"
#  $mainWindow.menu.edit.items add command -label "Exclude" -accelerator "-" -command "event generate . <<Event_backupStateExcluded>>"
#  pack $mainWindow.menu.edit -side left
pack $mainWindow.menu -side top -fill x

# window
tixNoteBook $mainWindow.tabs
  $mainWindow.tabs add jobs    -label "Jobs (F1)"    -underline -1 -raisecmd { focus .jobs.list.data }
  $mainWindow.tabs add backup  -label "Backup (F2)"  -underline -1
  $mainWindow.tabs add restore -label "Restore (F3)" -underline -1
pack $mainWindow.tabs -fill both -expand yes -padx 2p -pady 2p

# ----------------------------------------------------------------------

frame .jobs
  labelframe .jobs.selected -text "Selected"
    label .jobs.selected.done -text "Done:"
    grid .jobs.selected.done -row 0 -column 0 -sticky "w" 

    frame .jobs.selected.doneFiles
      entry .jobs.selected.doneFiles.data -width 10 -textvariable currentJob(doneFiles) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.doneFiles.data -side left
      label .jobs.selected.doneFiles.unit -text "files" -anchor w
      pack .jobs.selected.doneFiles.unit -side left
    grid .jobs.selected.doneFiles -row 0 -column 1 -sticky "w" -padx 2p -pady 2p

    frame .jobs.selected.doneBytes
      entry .jobs.selected.doneBytes.data -width 20 -textvariable currentJob(doneBytes) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.doneBytes.data -side left
      label .jobs.selected.doneBytes.unit -text "bytes" -anchor w
      pack .jobs.selected.doneBytes.unit -side left
    grid .jobs.selected.doneBytes -row 0 -column 2 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.doneSeparator1 -text "/"
    grid .jobs.selected.doneSeparator1 -row 0 -column 3 -sticky "w" 

    frame .jobs.selected.doneBytesShort
      entry .jobs.selected.doneBytesShort.data -width 6 -textvariable currentJob(doneBytesShort) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.doneBytesShort.data -side left
      label .jobs.selected.doneBytesShort.unit -width 8 -textvariable currentJob(doneBytesShortUnit) -anchor w
      pack .jobs.selected.doneBytesShort.unit -side left
    grid .jobs.selected.doneBytesShort -row 0 -column 4 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.stored -text "Stored:"
    grid .jobs.selected.stored -row 1 -column 0 -sticky "w" 

    frame .jobs.selected.storageTotalBytes
      entry .jobs.selected.storageTotalBytes.data -width 20 -textvariable currentJob(archiveBytes) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.storageTotalBytes.data -side left
      label .jobs.selected.storageTotalBytes.postfix -text "bytes" -anchor w
      pack .jobs.selected.storageTotalBytes.postfix -side left
    grid .jobs.selected.storageTotalBytes -row 1 -column 2 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.doneSeparator2 -text "/"
    grid .jobs.selected.doneSeparator2 -row 1 -column 3 -sticky "w" 

    frame .jobs.selected.doneStorageTotalBytesShort
      entry .jobs.selected.doneStorageTotalBytesShort.data -width 6 -textvariable currentJob(archiveBytesShort) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.doneStorageTotalBytesShort.data -side left
      label .jobs.selected.doneStorageTotalBytesShort.unit -width 8 -textvariable currentJob(archiveBytesShortUnit) -anchor w
      pack .jobs.selected.doneStorageTotalBytesShort.unit -side left
    grid .jobs.selected.doneStorageTotalBytesShort -row 1 -column 4 -sticky "w" -padx 2p -pady 2p

    frame .jobs.selected.doneCompressRatio
      label .jobs.selected.doneCompressRatio.title -text "Ratio"
      pack .jobs.selected.doneCompressRatio.title -side left
      entry .jobs.selected.doneCompressRatio.data -width 7 -textvariable currentJob(compressionRatio) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.doneCompressRatio.data -side left
      label .jobs.selected.doneCompressRatio.postfix -text "%" -anchor w
      pack .jobs.selected.doneCompressRatio.postfix -side left
    grid .jobs.selected.doneCompressRatio -row 1 -column 5 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.skipped -text "Skipped:"
    grid .jobs.selected.skipped -row 2 -column 0 -sticky "w" 

    frame .jobs.selected.skippedFiles
      entry .jobs.selected.skippedFiles.data -width 10 -textvariable currentJob(skippedFiles) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.skippedFiles.data -side left
      label .jobs.selected.skippedFiles.unit -text "files" -anchor w
      pack .jobs.selected.skippedFiles.unit -side left
    grid .jobs.selected.skippedFiles -row 2 -column 1 -sticky "w" -padx 2p -pady 2p

    frame .jobs.selected.skippedBytes
      entry .jobs.selected.skippedBytes.data -width 20 -textvariable currentJob(skippedBytes) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.skippedBytes.data -side left
      label .jobs.selected.skippedBytes.unit -text "bytes" -anchor w
      pack .jobs.selected.skippedBytes.unit -side left
    grid .jobs.selected.skippedBytes -row 2 -column 2 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.skippedSeparator -text "/"
    grid .jobs.selected.skippedSeparator -row 2 -column 3 -sticky "w" 

    frame .jobs.selected.skippedBytesShort
      entry .jobs.selected.skippedBytesShort.data -width 6 -textvariable currentJob(skippedBytesShort) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.skippedBytesShort.data -side left
      label .jobs.selected.skippedBytesShort.unit -width 8 -textvariable currentJob(skippedBytesShortUnit) -anchor w
      pack .jobs.selected.skippedBytesShort.unit -side left
    grid .jobs.selected.skippedBytesShort -row 2 -column 4 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.error -text "Errors:"
    grid .jobs.selected.error -row 3 -column 0 -sticky "w" 

    frame .jobs.selected.errorFiles
      entry .jobs.selected.errorFiles.data -width 10 -textvariable currentJob(errorFiles) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.errorFiles.data -side left
      label .jobs.selected.errorFiles.unit -text "files" -anchor w
      pack .jobs.selected.errorFiles.unit -side left
    grid .jobs.selected.errorFiles -row 3 -column 1 -sticky "w" -padx 2p -pady 2p

    frame .jobs.selected.errorBytes
      entry .jobs.selected.errorBytes.data -width 20 -textvariable currentJob(errorBytes) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.errorBytes.data -side left
      label .jobs.selected.errorBytes.unit -text "bytes"
      pack .jobs.selected.errorBytes.unit -side left
    grid .jobs.selected.errorBytes -row 3 -column 2 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.errorSeparator -text "/"
    grid .jobs.selected.errorSeparator -row 3 -column 3 -sticky "w" 

    frame .jobs.selected.errorBytesShort
      entry .jobs.selected.errorBytesShort.data -width 6 -textvariable currentJob(errorBytesShort) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.errorBytesShort.data -side left
      label .jobs.selected.errorBytesShort.unit -width 8 -textvariable currentJob(errorBytesShortUnit) -anchor w
      pack .jobs.selected.errorBytesShort.unit -side left
    grid .jobs.selected.errorBytesShort -row 3 -column 4 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.total -text "Total:"
    grid .jobs.selected.total -row 4 -column 0 -sticky "w" 

    frame .jobs.selected.totalFiles
      entry .jobs.selected.totalFiles.data -width 10 -textvariable currentJob(totalFiles) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.totalFiles.data -side left
      label .jobs.selected.totalFiles.unit -text "files" -anchor w
      pack .jobs.selected.totalFiles.unit -side left
    grid .jobs.selected.totalFiles -row 4 -column 1 -sticky "w" -padx 2p -pady 2p

    frame .jobs.selected.totalBytes
      entry .jobs.selected.totalBytes.data -width 20 -textvariable currentJob(totalBytes) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.totalBytes.data -side left
      label .jobs.selected.totalBytes.unit -text "bytes"
      pack .jobs.selected.totalBytes.unit -side left
    grid .jobs.selected.totalBytes -row 4 -column 2 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.totalSeparator -text "/"
    grid .jobs.selected.totalSeparator -row 4 -column 3 -sticky "w" 

    frame .jobs.selected.totalBytesShort
      entry .jobs.selected.totalBytesShort.data -width 6 -textvariable currentJob(totalBytesShort) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.totalBytesShort.data -side left
      label .jobs.selected.totalBytesShort.unit -width 8 -textvariable currentJob(totalBytesShortUnit) -anchor w
      pack .jobs.selected.totalBytesShort.unit -side left
    grid .jobs.selected.totalBytesShort -row 4 -column 4 -sticky "w" -padx 2p -pady 2p

    frame .jobs.selected.doneFilesPerSecond
      entry .jobs.selected.doneFilesPerSecond.data -width 10 -textvariable currentJob(filesPerSecond) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.doneFilesPerSecond.data -side left
      label .jobs.selected.doneFilesPerSecond.unit -text "files/s" -anchor w
      pack .jobs.selected.doneFilesPerSecond.unit -side left
    grid .jobs.selected.doneFilesPerSecond -row 4 -column 5 -sticky "w" -padx 2p -pady 2p

    frame .jobs.selected.doneBytesPerSecond
      entry .jobs.selected.doneBytesPerSecond.data -width 10 -textvariable currentJob(bytesPerSecondShort) -justify right -borderwidth 0 -state readonly
      pack .jobs.selected.doneBytesPerSecond.data -side left
      label .jobs.selected.doneBytesPerSecond.unit -width 8 -textvariable currentJob(bytesPerSecondShortUnit) -anchor w
      pack .jobs.selected.doneBytesPerSecond.unit -side left
    grid .jobs.selected.doneBytesPerSecond -row 4 -column 6 -sticky "w" -padx 2p -pady 2p

    label .jobs.selected.currentFileNameTitle -text "File:"
    grid .jobs.selected.currentFileNameTitle -row 5 -column 0 -sticky "w"
    entry .jobs.selected.currentFileName -textvariable currentJob(fileName) -borderwidth 0 -state readonly
    grid .jobs.selected.currentFileName -row 5 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    Dialog:progressbar .jobs.selected.filePercentage
    grid .jobs.selected.filePercentage -row 6 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.storageNameTitle -text "Storage:"
    grid .jobs.selected.storageNameTitle -row 7 -column 0 -sticky "w"
    entry .jobs.selected.storageName -textvariable currentJob(storageName) -borderwidth 0 -state readonly
    grid .jobs.selected.storageName -row 7 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    Dialog:progressbar .jobs.selected.storagePercentage
    grid .jobs.selected.storagePercentage -row 8 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.storageVolumeTitle -text "Volume:"
    grid .jobs.selected.storageVolumeTitle -row 9 -column 0 -sticky "w"
    Dialog:progressbar .jobs.selected.storageVolume
    grid .jobs.selected.storageVolume -row 9 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.totalFilesPercentageTitle -text "Total files:"
    grid .jobs.selected.totalFilesPercentageTitle -row 10 -column 0 -sticky "w"
    Dialog:progressbar .jobs.selected.totalFilesPercentage
    grid .jobs.selected.totalFilesPercentage -row 10 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.totalBytesPercentageTitle -text "Total bytes:"
    grid .jobs.selected.totalBytesPercentageTitle -row 11 -column 0 -sticky "w"
    Dialog:progressbar .jobs.selected.totalBytesPercentage
    grid .jobs.selected.totalBytesPercentage -row 11 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .jobs.selected.messageTitle -text "Message:"
    grid .jobs.selected.messageTitle -row 12 -column 0 -sticky "w"
    entry .jobs.selected.message -textvariable currentJob(message) -borderwidth 0 -highlightthickness 0 -state readonly -font -*-*-bold-*-*-*-*-*-*-*-*-*-*-*-*
    grid .jobs.selected.message -row 12 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

#    grid rowconfigure    .jobs.selected { 0 } -weight 1
    grid columnconfigure .jobs.selected { 4 } -weight 1
  grid .jobs.selected -row 0 -column 0 -sticky "we" -padx 2p -pady 2p

  frame .jobs.list
    mclistbox::mclistbox .jobs.list.data -height 1 -bg white -labelanchor w -selectmode single -xscrollcommand ".jobs.list.xscroll set" -yscrollcommand ".jobs.list.yscroll set"
    .jobs.list.data column add id                -label "Nb."            -width 5
    .jobs.list.data column add name              -label "Name"           -width 12
    .jobs.list.data column add state             -label "State"          -width 16
    .jobs.list.data column add type              -label "Type"           -width 10
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
    button .jobs.buttons.abort -text "Abort" -state disabled -command "event generate . <<Event_abortJob>>"
    pack .jobs.buttons.abort -side left -padx 2p
    button .jobs.buttons.rem -text "Rem (Del)" -state disabled -command "event generate . <<Event_remJob>>"
    pack .jobs.buttons.rem -side left -padx 2p
    button .jobs.buttons.volume -text "Volume" -state disabled -command "event generate . <<Event_volume>>"
    pack .jobs.buttons.volume -side left -padx 2p
    button .jobs.buttons.quit -text "Quit" -command "event generate . <<Event_quit>>"
    pack .jobs.buttons.quit -side right -padx 2p
  grid .jobs.buttons -row 2 -column 0 -sticky "we" -padx 2p -pady 2p

  bind .jobs.list.data <ButtonRelease-1>    "event generate . <<Event_selectJob>>"
  bind .jobs.list.data <KeyPress-Delete>    "event generate . <<Event_remJob>>"
  bind .jobs.list.data <KeyPress-KP_Delete> "event generate . <<Event_remJob>>"

  grid rowconfigure    .jobs { 1 } -weight 1
  grid columnconfigure .jobs { 0 } -weight 1
pack .jobs -side top -fill both -expand yes -in [$mainWindow.tabs subwidget jobs]

# ----------------------------------------------------------------------

frame .backup
  label .backup.nameTitle -text "Name:"
  grid .backup.nameTitle -row 0 -column 0 -sticky "w"
  entry .backup.name -textvariable barConfig(name) -bg white
  grid .backup.name -row 0 -column 1 -sticky "we" -padx 2p -pady 2p

  tixNoteBook .backup.tabs
    .backup.tabs add files   -label "Files"   -underline -1 -raisecmd { focus .backup.files.list }
    .backup.tabs add filters -label "Filters" -underline -1 -raisecmd { focus .backup.filters.included }
    .backup.tabs add storage -label "Storage" -underline -1
  #  $mainWindow.tabs add misc          -label "Misc"             -underline -1
  grid .backup.tabs -row 1 -column 0 -columnspan 2 -sticky "nswe" -padx 2p -pady 2p

  frame .backup.files
    tixTree .backup.files.list -scrollbar both -options \
    {
      hlist.separator "/"
      hlist.columns 4
      hlist.header yes
      hlist.indent 16
    }
    .backup.files.list subwidget hlist configure -selectmode extended

    .backup.files.list subwidget hlist header create 0 -itemtype text -text "File"
    .backup.files.list subwidget hlist header create 1 -itemtype text -text "Type"
    .backup.files.list subwidget hlist column width 1 -char 10
    .backup.files.list subwidget hlist header create 2 -itemtype text -text "Size"
    .backup.files.list subwidget hlist column width 2 -char 10
    .backup.files.list subwidget hlist header create 3 -itemtype text -text "Modified"
    .backup.files.list subwidget hlist column width 3 -char 15
    grid .backup.files.list -row 0 -column 0 -sticky "nswe" -padx 2p -pady 2p
    set backupFilesTreeWidget [.backup.files.list subwidget hlist]

    tixPopupMenu .backup.files.list.popup -title "Command"
    .backup.files.list.popup subwidget menu add command -label "Add include"                            -command ""
    .backup.files.list.popup subwidget menu add command -label "Add exclude"                            -command ""
    .backup.files.list.popup subwidget menu add command -label "Remove include"                         -command ""
    .backup.files.list.popup subwidget menu add command -label "Remove exclude"                         -command ""
    .backup.files.list.popup subwidget menu add command -label "Add include pattern"    -state disabled -command ""
    .backup.files.list.popup subwidget menu add command -label "Add exclude pattern"    -state disabled -command ""
    .backup.files.list.popup subwidget menu add command -label "Remove include pattern" -state disabled -command ""
    .backup.files.list.popup subwidget menu add command -label "Remove exclude pattern" -state disabled -command ""

    proc backupFilesPopupHandler { widget x y } \
    {
      set fileName [$widget nearest $y]
      set extension ""
      regexp {.*(\.[^\.]+)} $fileName * extension

      .backup.files.list.popup subwidget menu entryconfigure 0 -label "Add include '$fileName'"    -command "backupAddIncludedPattern $fileName"
      .backup.files.list.popup subwidget menu entryconfigure 1 -label "Add exclude '$fileName'"    -command "backupAddExcludedPattern $fileName"
      .backup.files.list.popup subwidget menu entryconfigure 2 -label "Remove include '$fileName'" -command "backupRemIncludedPattern $fileName"
      .backup.files.list.popup subwidget menu entryconfigure 3 -label "Remove exclude '$fileName'" -command "backupRemExcludedPattern $fileName"
      if {$extension != ""} \
      {
        .backup.files.list.popup subwidget menu entryconfigure 4 -label "Add include pattern *$extension"    -state normal -command "backupAddIncludedPattern *$extension"
        .backup.files.list.popup subwidget menu entryconfigure 5 -label "Add exclude pattern *$extension"    -state normal -command "backupAddExcludedPattern *$extension"
        .backup.files.list.popup subwidget menu entryconfigure 6 -label "Remove include pattern *$extension" -state normal -command "backupRemIncludedPattern *$extension"
        .backup.files.list.popup subwidget menu entryconfigure 7 -label "Remove exclude pattern *$extension" -state normal -command "backupRemExcludedPattern *$extension"
      } \
      else \
      {
        .backup.files.list.popup subwidget menu entryconfigure 4 -label "Add include pattern -"    -state disabled -command ""
        .backup.files.list.popup subwidget menu entryconfigure 5 -label "Add exclude pattern -"    -state disabled -command ""
        .backup.files.list.popup subwidget menu entryconfigure 6 -label "Remove include pattern -" -state disabled -command ""
        .backup.files.list.popup subwidget menu entryconfigure 7 -label "Remove exclude pattern -" -state disabled -command ""
      }
      .backup.files.list.popup post $widget $x $y
    }

    frame .backup.files.buttons
      button .backup.files.buttons.stateNone -text "*" -command "event generate . <<Event_backupStateNone>>"
      pack .backup.files.buttons.stateNone -side left -fill x -expand yes
      button .backup.files.buttons.stateIncluded -text "+" -command "event generate . <<Event_backupStateIncluded>>"
      pack .backup.files.buttons.stateIncluded -side left -fill x -expand yes
      button .backup.files.buttons.stateExcluded -text "-" -command "event generate . <<Event_backupStateExcluded>>"
      pack .backup.files.buttons.stateExcluded -side left -fill x -expand yes
    grid .backup.files.buttons -row 1 -column 0 -sticky "we" -padx 2p -pady 2p

    bind [.backup.files.list subwidget hlist] <Button-3>    "backupFilesPopupHandler %W %x %y"
    bind [.backup.files.list subwidget hlist] <BackSpace>   "event generate . <<Event_backupStateNone>>"
    bind [.backup.files.list subwidget hlist] <Delete>      "event generate . <<Event_backupStateNone>>"
    bind [.backup.files.list subwidget hlist] <plus>        "event generate . <<Event_backupStateIncluded>>"
    bind [.backup.files.list subwidget hlist] <KP_Add>      "event generate . <<Event_backupStateIncluded>>"
    bind [.backup.files.list subwidget hlist] <minus>       "event generate . <<Event_backupStateExcluded>>"
    bind [.backup.files.list subwidget hlist] <KP_Subtract> "event generate . <<Event_backupStateExcluded>>"
    bind [.backup.files.list subwidget hlist] <space>       "event generate . <<Event_backupToggleStateNoneIncludedExcluded>>"

    # fix a bug in tix: end does not use separator-char to detect last entry
    bind [.backup.files.list subwidget hlist] <KeyPress-End> \
      "
       .backup.files.list subwidget hlist yview moveto 1
       .backup.files.list subwidget hlist anchor set \[lindex \[.backup.files.list subwidget hlist info children /\] end\]
       break
      "

    # mouse-wheel events
    bind [.backup.files.list subwidget hlist] <Button-4> \
      "
       set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
       .backup.files.list subwidget hlist yview scroll -\$n units
      "
    bind [.backup.files.list subwidget hlist] <Button-5> \
      "
       set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
       .backup.files.list subwidget hlist yview scroll +\$n units
      "

    grid rowconfigure    .backup.files { 0 } -weight 1
    grid columnconfigure .backup.files { 0 } -weight 1
  pack .backup.files -side top -fill both -expand yes -in [.backup.tabs subwidget files]

  frame .backup.filters
    label .backup.filters.includedTitle -text "Included:"
    grid .backup.filters.includedTitle -row 0 -column 0 -sticky "nw"
    tixScrolledListBox .backup.filters.included -height 1 -scrollbar both -options { listbox.background white  }
    grid .backup.filters.included -row 0 -column 1 -sticky "nswe" -padx 2p -pady 2p
    .backup.filters.included subwidget listbox configure -listvariable barConfig(included) -selectmode extended
    set backupIncludedListWidget [.backup.filters.included subwidget listbox]

    bind [.backup.filters.included subwidget listbox] <Button-1> ".backup.filters.includedButtons.rem configure -state normal"

    frame .backup.filters.includedButtons
      button .backup.filters.includedButtons.add -text "Add (F5)" -command "event generate . <<Event_backupAddIncludePattern>>"
      pack .backup.filters.includedButtons.add -side left
      button .backup.filters.includedButtons.rem -text "Rem (F6)" -state disabled -command "event generate . <<Event_backupRemIncludePattern>>"
      pack .backup.filters.includedButtons.rem -side left
    grid .backup.filters.includedButtons -row 1 -column 1 -sticky "we" -padx 2p -pady 2p

    bind [.backup.filters.included subwidget listbox] <Insert> "event generate . <<Event_backupAddIncludePattern>>"
    bind [.backup.filters.included subwidget listbox] <Delete> "event generate . <<Event_backupRemIncludePattern>>"

    label .backup.filters.excludedTitle -text "Excluded:"
    grid .backup.filters.excludedTitle -row 2 -column 0 -sticky "nw"
    tixScrolledListBox .backup.filters.excluded -height 1 -scrollbar both -options { listbox.background white }
    grid .backup.filters.excluded -row 2 -column 1 -sticky "nswe" -padx 2p -pady 2p
    .backup.filters.excluded subwidget listbox configure -listvariable barConfig(excluded) -selectmode extended
    set backupExcludedListWidget [.backup.filters.excluded subwidget listbox]

    bind [.backup.filters.excluded subwidget listbox] <Button-1> ".backup.filters.excludedButtons.rem configure -state normal"

    frame .backup.filters.excludedButtons
      button .backup.filters.excludedButtons.add -text "Add (F7)" -command "event generate . <<Event_backupAddExcludePattern>>"
      pack .backup.filters.excludedButtons.add -side left
      button .backup.filters.excludedButtons.rem -text "Rem (F8)" -state disabled -command "event generate . <<Event_backupRemExcludePattern>>"
      pack .backup.filters.excludedButtons.rem -side left
    grid .backup.filters.excludedButtons -row 3 -column 1 -sticky "we" -padx 2p -pady 2p

    bind [.backup.filters.excluded subwidget listbox] <Insert> "event generate . <<Event_backupAddExcludePattern>>"
    bind [.backup.filters.excluded subwidget listbox] <Delete> "event generate . <<Event_backupRemExcludePattern>>"

    label .backup.filters.optionsTitle -text "Options:"
    grid .backup.filters.optionsTitle -row 4 -column 0 -sticky "nw" 
    checkbutton .backup.filters.optionSkipUnreadable -text "skip unreable files" -variable barConfig(skipUnreadableFlag)
    grid .backup.filters.optionSkipUnreadable -row 4 -column 1 -sticky "nw" 

    grid rowconfigure    .backup.filters { 0 2 } -weight 1
    grid columnconfigure .backup.filters { 1 } -weight 1
  pack .backup.filters -side top -fill both -expand yes -in [.backup.tabs subwidget filters]

  frame .backup.storage
    label .backup.storage.archivePartSizeTitle -text "Part size:"
    grid .backup.storage.archivePartSizeTitle -row 0 -column 0 -sticky "w" 
    frame .backup.storage.split
      radiobutton .backup.storage.split.unlimited -text "unlimited" -anchor w -variable barConfig(archivePartSizeFlag) -value 0
      grid .backup.storage.split.unlimited -row 0 -column 1 -sticky "w" 
      radiobutton .backup.storage.split.size -text "split in" -width 8 -anchor w -variable barConfig(archivePartSizeFlag) -value 1
      grid .backup.storage.split.size -row 0 -column 2 -sticky "w" 
      tixComboBox .backup.storage.split.archivePartSize -variable barConfig(archivePartSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
      grid .backup.storage.split.archivePartSize -row 0 -column 3 -sticky "w" 
      label .backup.storage.split.unit -text "bytes"
      grid .backup.storage.split.unit -row 0 -column 4 -sticky "w" 

     .backup.storage.split.archivePartSize insert end 32M
     .backup.storage.split.archivePartSize insert end 64M
     .backup.storage.split.archivePartSize insert end 128M
     .backup.storage.split.archivePartSize insert end 256M
     .backup.storage.split.archivePartSize insert end 512M
     .backup.storage.split.archivePartSize insert end 1G
     .backup.storage.split.archivePartSize insert end 2G

      grid rowconfigure    .backup.storage.split { 0 } -weight 1
      grid columnconfigure .backup.storage.split { 1 } -weight 1
    grid .backup.storage.split -row 0 -column 1 -sticky "w" -padx 2p -pady 2p
    addEnableTrace ::barConfig(archivePartSizeFlag) 1 .backup.storage.split.archivePartSize

    label .backup.storage.maxTmpSizeTitle -text "Max. temp. size:"
    grid .backup.storage.maxTmpSizeTitle -row 1 -column 0 -sticky "w" 
    frame .backup.storage.maxTmpSize
      radiobutton .backup.storage.maxTmpSize.unlimited -text "unlimited" -anchor w -variable barConfig(maxTmpSizeFlag) -value 0
      grid .backup.storage.maxTmpSize.unlimited -row 0 -column 1 -sticky "w" 
      radiobutton .backup.storage.maxTmpSize.limitto -text "limit to" -width 8 -anchor w -variable barConfig(maxTmpSizeFlag) -value 1
      grid .backup.storage.maxTmpSize.limitto -row 0 -column 2 -sticky "w" 
      tixComboBox .backup.storage.maxTmpSize.size -variable barConfig(maxTmpSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
      grid .backup.storage.maxTmpSize.size -row 0 -column 3 -sticky "w" 
      label .backup.storage.maxTmpSize.unit -text "bytes"
      grid .backup.storage.maxTmpSize.unit -row 0 -column 4 -sticky "w" 

     .backup.storage.maxTmpSize.size insert end 32M
     .backup.storage.maxTmpSize.size insert end 64M
     .backup.storage.maxTmpSize.size insert end 128M
     .backup.storage.maxTmpSize.size insert end 256M
     .backup.storage.maxTmpSize.size insert end 512M
     .backup.storage.maxTmpSize.size insert end 1G
     .backup.storage.maxTmpSize.size insert end 2G
     .backup.storage.maxTmpSize.size insert end 4G
     .backup.storage.maxTmpSize.size insert end 8G

      grid rowconfigure    .backup.storage.maxTmpSize { 0 } -weight 1
      grid columnconfigure .backup.storage.maxTmpSize { 1 } -weight 1
    grid .backup.storage.maxTmpSize -row 1 -column 1 -sticky "w" -padx 2p -pady 2p
    addEnableTrace ::barConfig(maxTmpSizeFlag) 1 .backup.storage.maxTmpSize.size

    label .backup.storage.compressAlgorithmTitle -text "Compress:"
    grid .backup.storage.compressAlgorithmTitle -row 2 -column 0 -sticky "w" 
    tk_optionMenu .backup.storage.compressAlgorithm barConfig(compressAlgorithm) \
      "none" "zip0" "zip1" "zip2" "zip3" "zip4" "zip5" "zip6" "zip7" "zip8" "zip9" "bzip1" "bzip2" "bzip3" "bzip4" "bzip5" "bzip6" "bzip7" "bzip8" "bzip9"
    grid .backup.storage.compressAlgorithm -row 2 -column 1 -sticky "w" -padx 2p -pady 2p

    label .backup.storage.cryptAlgorithmTitle -text "Crypt:"
    grid .backup.storage.cryptAlgorithmTitle -row 3 -column 0 -sticky "w" 
    tk_optionMenu .backup.storage.cryptAlgorithm barConfig(cryptAlgorithm) \
      "none" "3DES" "CAST5" "BLOWFISH" "AES128" "AES192" "AES256" "TWOFISH128" "TWOFISH256"
    grid .backup.storage.cryptAlgorithm -row 3 -column 1 -sticky "w" -padx 2p -pady 2p

    label .backup.storage.cryptPasswordTitle -text "Password:"
    grid .backup.storage.cryptPasswordTitle -row 4 -column 0 -sticky "nw" 
    frame .backup.storage.cryptPassword
      radiobutton .backup.storage.cryptPassword.modeDefault -text "default" -variable barConfig(cryptPasswordMode) -value "DEFAULT"
      grid .backup.storage.cryptPassword.modeDefault -row 0 -column 1 -sticky "w"
      radiobutton .backup.storage.cryptPassword.modeAsk -text "ask" -variable barConfig(cryptPasswordMode) -value "ASK"
      grid .backup.storage.cryptPassword.modeAsk -row 0 -column 2 -sticky "w"
      radiobutton .backup.storage.cryptPassword.modeConfig -text "this" -variable barConfig(cryptPasswordMode) -value "CONFIG"
      grid .backup.storage.cryptPassword.modeConfig -row 0 -column 3 -sticky "w"
      entry .backup.storage.cryptPassword.data1 -textvariable barConfig(cryptPassword) -bg white -show "*"
      grid .backup.storage.cryptPassword.data1 -row 0 -column 4 -sticky "we"
      entry .backup.storage.cryptPassword.data2 -textvariable barConfig(cryptPasswordVerify) -bg white -show "*"
      grid .backup.storage.cryptPassword.data2 -row 1 -column 4 -sticky "we"

      grid columnconfigure .backup.storage.cryptPassword { 4 } -weight 1
    grid .backup.storage.cryptPassword -row 4 -column 1 -sticky "we" -padx 2p -pady 2p
    addEnableTrace ::barConfig(cryptPasswordMode) "CONFIG" .backup.storage.cryptPassword.data1
    addEnableTrace ::barConfig(cryptPasswordMode) "CONFIG" .backup.storage.cryptPassword.data2

    label .backup.storage.modeTitle -text "Mode:"
    grid .backup.storage.modeTitle -row 5 -column 0 -sticky "nw" 
    frame .backup.storage.mode
      radiobutton .backup.storage.mode.normal -text "normal" -variable barConfig(storageMode) -value "NORMAL"
      grid .backup.storage.mode.normal -row 0 -column 0 -sticky "w"
      radiobutton .backup.storage.mode.full -text "full" -variable barConfig(storageMode) -value "FULL"
      grid .backup.storage.mode.full -row 0 -column 1 -sticky "w"
      radiobutton .backup.storage.mode.incremental -text "incremental" -variable barConfig(storageMode) -value "INCREMENTAL"
      grid .backup.storage.mode.incremental -row 0 -column 3 -sticky "w"
      entry .backup.storage.mode.incrementalListFileName -textvariable barConfig(incrementalListFileName) -bg white
      grid .backup.storage.mode.incrementalListFileName -row 0 -column 4 -sticky "we"
      button .backup.storage.mode.select -image $images(folder) -command \
      "
        set fileName \[Dialog:fileSelector \"Incremental list file\" \$barConfig(incrementalListFileName) {{\"*.bid\" \"incremental list\"} {\"*\" \"all\"}}\]
        if {\$fileName != \"\"} \
        {
          set barConfig(incrementalListFileName) \$fileName
        }
      "
      grid .backup.storage.mode.select -row 0 -column 5 -sticky "we"

      grid columnconfigure .backup.storage.mode { 4 } -weight 1
    grid .backup.storage.mode -row 5 -column 1 -sticky "we" -padx 2p -pady 2p
    addModifyTrace {::barConfig(storageMode)} \
    {
      if {   ($::barConfig(storageMode) == "FULL")
          || ($::barConfig(storageMode) == "INCREMENTAL")
         } \
      {
        .backup.storage.mode.incrementalListFileName configure -state normal
      } \
      else \
      {
        .backup.storage.mode.incrementalListFileName configure -state disabled
      }
    }

    label .backup.storage.fileNameTitle -text "File name:"
    grid .backup.storage.fileNameTitle -row 6 -column 0 -sticky "w" 
    frame .backup.storage.fileName
      entry .backup.storage.fileName.data -textvariable barConfig(storageFileName) -bg white
      grid .backup.storage.fileName.data -row 0 -column 0 -sticky "we" 

      button .backup.storage.fileName.edit -image $images(folder) -command \
      "
        set fileName \[editStorageFileName \$barConfig(storageFileName)\]
        if {\$fileName != \"\"} \
        {
          set barConfig(storageFileName) \$fileName
        }
      "
      grid .backup.storage.fileName.edit -row 0 -column 1 -sticky "we" 

      grid columnconfigure .backup.storage.fileName { 0 } -weight 1
    grid .backup.storage.fileName -row 6 -column 1 -sticky "we" -padx 2p -pady 2p

    label .backup.storage.destinationTitle -text "Destination:"
    grid .backup.storage.destinationTitle -row 7 -column 0 -sticky "nw" 
    frame .backup.storage.destination
      frame .backup.storage.destination.type
        radiobutton .backup.storage.destination.type.fileSystem -text "File system" -variable barConfig(storageType) -value "FILESYSTEM"
        grid .backup.storage.destination.type.fileSystem -row 0 -column 0 -sticky "w"

        radiobutton .backup.storage.destination.type.ssh -text "scp" -variable barConfig(storageType) -value "SCP"
        grid .backup.storage.destination.type.ssh -row 0 -column 1 -sticky "w" 

        radiobutton .backup.storage.destination.type.sftp -text "sftp" -variable barConfig(storageType) -value "SFTP"
        grid .backup.storage.destination.type.sftp -row 0 -column 2 -sticky "w" 

        radiobutton .backup.storage.destination.type.dvd -text "DVD" -variable barConfig(storageType) -value "DVD"
        grid .backup.storage.destination.type.dvd -row 0 -column 3 -sticky "w"

        radiobutton .backup.storage.destination.type.device -text "Device" -variable barConfig(storageType) -value "DEVICE"
        grid .backup.storage.destination.type.device -row 0 -column 4 -sticky "w"

        grid rowconfigure    .backup.storage.destination.type { 0 } -weight 1
        grid columnconfigure .backup.storage.destination.type { 5 } -weight 1
      grid .backup.storage.destination.type -row 0 -column 0 -sticky "we" -padx 2p -pady 2p

      labelframe .backup.storage.destination.fileSystem
        label .backup.storage.destination.fileSystem.optionsTitle -text "Options:"
        grid .backup.storage.destination.fileSystem.optionsTitle -row 0 -column 0 -sticky "w" 
        frame .backup.storage.destination.fileSystem.options
          checkbutton .backup.storage.destination.fileSystem.options.overwriteArchiveFiles -text "overwrite archive files" -variable barConfig(overwriteArchiveFilesFlag)
          grid .backup.storage.destination.fileSystem.options.overwriteArchiveFiles -row 0 -column 0 -sticky "w" 

          grid columnconfigure .backup.storage.destination.fileSystem.options { 0 } -weight 1
        grid .backup.storage.destination.fileSystem.options -row 0 -column 1 -sticky "we"

        grid columnconfigure .backup.storage.destination.fileSystem { 1 } -weight 1
      addModifyTrace {::barConfig(storageType)} \
      {
        if {$::barConfig(storageType)=="FILESYSTEM"} \
        {
          grid .backup.storage.destination.fileSystem -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
        } \
        else \
        {
          grid forget .backup.storage.destination.fileSystem
        }
      }

      labelframe .backup.storage.destination.ssh
        label .backup.storage.destination.ssh.loginTitle -text "Login:"
        grid .backup.storage.destination.ssh.loginTitle -row 0 -column 0 -sticky "w" 
        frame .backup.storage.destination.ssh.login
          entry .backup.storage.destination.ssh.login.name -textvariable barConfig(storageLoginName) -bg white
          pack .backup.storage.destination.ssh.login.name -side left -fill x -expand yes

      #    label .backup.storage.destination.ssh.loginPasswordTitle -text "Password:"
      #    pack .backup.storage.destination.ssh.loginPasswordTitle -row 0 -column 2 -sticky "w" 
      #    entry .backup.storage.destination.ssh.loginPassword -textvariable barConfig(sshPassword) -bg white -show "*"
      #    pack .backup.storage.destination.ssh.loginPassword -row 0 -column 3 -sticky "we" 

          label .backup.storage.destination.ssh.login.hostNameTitle -text "Host:"
          pack .backup.storage.destination.ssh.login.hostNameTitle -side left
          entry .backup.storage.destination.ssh.login.hostName -textvariable barConfig(storageHostName) -bg white
          pack .backup.storage.destination.ssh.login.hostName -side left -fill x -expand yes

          label .backup.storage.destination.ssh.login.sshPortTitle -text "SSH port:"
          pack .backup.storage.destination.ssh.login.sshPortTitle -side left
          tixControl .backup.storage.destination.ssh.login.sshPort -variable barConfig(sshPort) -label "" -labelside right -integer true -min 0 -max 65535 -options { entry.background white }
          pack .backup.storage.destination.ssh.login.sshPort -side left -fill x -expand yes
        grid .backup.storage.destination.ssh.login -row 0 -column 1 -sticky "we"

        label .backup.storage.destination.ssh.sshPublicKeyFileNameTitle -text "SSH public key:"
        grid .backup.storage.destination.ssh.sshPublicKeyFileNameTitle -row 1 -column 0 -sticky "w" 
        frame .backup.storage.destination.ssh.sshPublicKeyFileName
          entry .backup.storage.destination.ssh.sshPublicKeyFileName.data -textvariable barConfig(sshPublicKeyFileName) -bg white
          pack .backup.storage.destination.ssh.sshPublicKeyFileName.data -side left -fill x -expand yes
          button .backup.storage.destination.ssh.sshPublicKeyFileName.select -image $images(folder) -command \
          "
            set fileName \[Dialog:fileSelector \"Select SSH public key file\" \$barConfig(sshPublicKeyFileName) {{\"*.pub\" \"Public key\"} {\"*\" \"all\"}}\]
            if {\$fileName != \"\"} \
            {
              set barConfig(sshPublicKeyFileName) \$fileName
            }
          "
          pack .backup.storage.destination.ssh.sshPublicKeyFileName.select -side left
        grid .backup.storage.destination.ssh.sshPublicKeyFileName -row 1 -column 1 -sticky "we"

        label .backup.storage.destination.ssh.sshPrivatKeyFileNameTitle -text "SSH privat key:"
        grid .backup.storage.destination.ssh.sshPrivatKeyFileNameTitle -row 2 -column 0 -sticky "w" 
        frame .backup.storage.destination.ssh.sshPrivatKeyFileName
          entry .backup.storage.destination.ssh.sshPrivatKeyFileName.data -textvariable barConfig(sshPrivatKeyFileName) -bg white
          pack .backup.storage.destination.ssh.sshPrivatKeyFileName.data -side left -fill x -expand yes
          button .backup.storage.destination.ssh.sshPrivatKeyFileName.select -image $images(folder) -command \
          "
            set fileName \[Dialog:fileSelector \"Select SSH privat key file\" \$barConfig(sshPrivatKeyFileName) {{\"*\" \"all\"}}\]
            if {\$fileName != \"\"} \
            {
              set barConfig(sshPrivatKeyFileName) \$fileName
            }
          "
          pack .backup.storage.destination.ssh.sshPrivatKeyFileName.select -side left
        grid .backup.storage.destination.ssh.sshPrivatKeyFileName -row 2 -column 1 -sticky "we"

        label .backup.storage.destination.ssh.maxBandWidthTitle -text "Max. band width:"
        grid .backup.storage.destination.ssh.maxBandWidthTitle -row 3 -column 0 -sticky "w" 
        frame .backup.storage.destination.ssh.maxBandWidth
          radiobutton .backup.storage.destination.ssh.maxBandWidth.unlimited -text "unlimited" -anchor w -variable barConfig(maxBandWidthFlag) -value 0
          grid .backup.storage.destination.ssh.maxBandWidth.unlimited -row 0 -column 1 -sticky "w" 
          radiobutton .backup.storage.destination.ssh.maxBandWidth.limitto -text "limit to" -width 8 -anchor w -variable barConfig(maxBandWidthFlag) -value 1
          grid .backup.storage.destination.ssh.maxBandWidth.limitto -row 0 -column 2 -sticky "w" 
          tixComboBox .backup.storage.destination.ssh.maxBandWidth.size -variable barConfig(maxBandWidth) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
          grid .backup.storage.destination.ssh.maxBandWidth.size -row 0 -column 3 -sticky "w" 
          label .backup.storage.destination.ssh.maxBandWidth.unit -text "bits/s"
          grid .backup.storage.destination.ssh.maxBandWidth.unit -row 0 -column 4 -sticky "w" 

         .backup.storage.destination.ssh.maxBandWidth.size insert end 64K
         .backup.storage.destination.ssh.maxBandWidth.size insert end 128K
         .backup.storage.destination.ssh.maxBandWidth.size insert end 256K
         .backup.storage.destination.ssh.maxBandWidth.size insert end 512K

          grid rowconfigure    .backup.storage.destination.ssh.maxBandWidth { 0 } -weight 1
          grid columnconfigure .backup.storage.destination.ssh.maxBandWidth { 1 } -weight 1
        grid .backup.storage.destination.ssh.maxBandWidth -row 3 -column 1 -sticky "w" -padx 2p -pady 2p
        addEnableTrace ::barConfig(maxBandWidthFlag) 1 .backup.storage.destination.ssh.maxBandWidth.size

  #      grid rowconfigure    .backup.storage.destination.ssh { } -weight 1
        grid columnconfigure .backup.storage.destination.ssh { 1 } -weight 1
      addModifyTrace {::barConfig(storageType)} \
      {
        if {($::barConfig(storageType) == "SCP") || ($::barConfig(storageType) == "SFTP")} \
        {
          grid .backup.storage.destination.ssh -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
        } \
        else \
        {
          grid forget .backup.storage.destination.ssh
        }
      }

      labelframe .backup.storage.destination.dvd
        label .backup.storage.destination.dvd.deviceNameTitle -text "DVD device:"
        grid .backup.storage.destination.dvd.deviceNameTitle -row 0 -column 0 -sticky "w" 
        entry .backup.storage.destination.dvd.deviceName -textvariable barConfig(storageDeviceName) -bg white
        grid .backup.storage.destination.dvd.deviceName -row 0 -column 1 -sticky "we" 

        label .backup.storage.destination.dvd.volumeSizeTitle -text "Size:"
        grid .backup.storage.destination.dvd.volumeSizeTitle -row 1 -column 0 -sticky "w"
        frame .backup.storage.destination.dvd.volumeSize
          tixComboBox .backup.storage.destination.dvd.volumeSize.size -variable barConfig(volumeSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
          pack .backup.storage.destination.dvd.volumeSize.size -side left
          label .backup.storage.destination.dvd.volumeSize.unit -text "bytes"
          pack .backup.storage.destination.dvd.volumeSize.unit -side left
        grid .backup.storage.destination.dvd.volumeSize -row 1 -column 1 -sticky "w"

        label .backup.storage.destination.dvd.optionsTitle -text "Options:"
        grid .backup.storage.destination.dvd.optionsTitle -row 2 -column 0 -sticky "w"
        frame .backup.storage.destination.dvd.options
          checkbutton .backup.storage.destination.dvd.options.ecc -text "add error-correction codes" -variable barConfig(errorCorrectionCodesFlag)
          grid .backup.storage.destination.dvd.options.ecc -row 0 -column 0 -sticky "w" 

          grid columnconfigure .backup.storage.destination.dvd.options { 0 } -weight 1
        grid .backup.storage.destination.dvd.options -row 2 -column 1 -sticky "we"

       .backup.storage.destination.dvd.volumeSize.size insert end 2G
       .backup.storage.destination.dvd.volumeSize.size insert end 3G
       .backup.storage.destination.dvd.volumeSize.size insert end 3.6G
       .backup.storage.destination.dvd.volumeSize.size insert end 4G

        grid rowconfigure    .backup.storage.destination.dvd { 4 } -weight 1
        grid columnconfigure .backup.storage.destination.dvd { 1 } -weight 1
      addModifyTrace {::barConfig(storageType)} \
      {
        if {$::barConfig(storageType)=="DVD"} \
        {
          grid .backup.storage.destination.dvd -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
        } \
        else \
        {
          grid forget .backup.storage.destination.dvd
        }
      }

      labelframe .backup.storage.destination.device
        label .backup.storage.destination.device.nameTitle -text "Device name:"
        grid .backup.storage.destination.device.nameTitle -row 0 -column 0 -sticky "w" 
        entry .backup.storage.destination.device.name -textvariable barConfig(storageDeviceName) -bg white
        grid .backup.storage.destination.device.name -row 0 -column 1 -sticky "we" 

        label .backup.storage.destination.device.volumeSizeTitle -text "Size:"
        grid .backup.storage.destination.device.volumeSizeTitle -row 1 -column 0 -sticky "w"
        frame .backup.storage.destination.device.volumeSize
          tixComboBox .backup.storage.destination.device.volumeSize.size -variable barConfig(volumeSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
          pack .backup.storage.destination.device.volumeSize.size -side left
          label .backup.storage.destination.device.volumeSize.unit -text "bytes"
          pack .backup.storage.destination.device.volumeSize.unit -side left
        grid .backup.storage.destination.device.volumeSize -row 1 -column 1 -sticky "w"

       .backup.storage.destination.device.volumeSize.size insert end 2G
       .backup.storage.destination.device.volumeSize.size insert end 3G
       .backup.storage.destination.device.volumeSize.size insert end 3.6G
       .backup.storage.destination.device.volumeSize.size insert end 4G

        grid rowconfigure    .backup.storage.destination.device { 3 } -weight 1
        grid columnconfigure .backup.storage.destination.device { 1 } -weight 1
      addModifyTrace {::barConfig(storageType)} \
      {
        if {$::barConfig(storageType)=="DEVICE"} \
        {
          grid .backup.storage.destination.device -row 7 -column 0 -sticky "nswe" -padx 2p -pady 2p
        } \
        else \
        {
          grid forget .backup.storage.destination.device
        }
      }

      grid rowconfigure    .backup.storage.destination { 0 } -weight 1
      grid columnconfigure .backup.storage.destination { 0 } -weight 1
    grid .backup.storage.destination -row 7 -column 1 -sticky "we"

    grid rowconfigure    .backup.storage { 8 } -weight 1
    grid columnconfigure .backup.storage { 1 } -weight 1
  pack .backup.storage -side top -fill both -expand yes -in [.backup.tabs subwidget storage]

  frame .backup.buttons
    button .backup.buttons.addBackupJob -text "Start backup" -command "event generate . <<Event_addBackupJob>>"
    pack .backup.buttons.addBackupJob -side left -padx 2p
    button .backup.buttons.quit -text "Quit" -command "event generate . <<Event_quit>>"
    pack .backup.buttons.quit -side right -padx 2p
  grid .backup.buttons -row 2 -column 0 -columnspan 2 -sticky "we" -padx 2p -pady 2p

  grid rowconfigure    .backup { 1 } -weight 1
  grid columnconfigure .backup { 1 } -weight 1
pack .backup -side top -fill both -expand yes -in [$mainWindow.tabs subwidget backup]

# ----------------------------------------------------------------------

frame .restore
  label .restore.nameTitle -text "Name:"
  grid .restore.nameTitle -row 0 -column 0 -sticky "w"
  entry .restore.name -textvariable barConfig(name) -bg white
  grid .restore.name -row 0 -column 1 -sticky "we" -padx 2p -pady 2p

  tixNoteBook .restore.tabs
    .restore.tabs add storage -label "Storage" -underline -1
    .restore.tabs add files   -label "Files"   -underline -1 -raisecmd { focus .restore.files.list }
    .restore.tabs add filters -label "Filters" -underline -1
  pack .restore.tabs -side top -fill both -expand yes
  grid .restore.tabs -row 1 -column 0 -columnspan 2 -sticky "nswe" -padx 2p -pady 2p

  frame .restore.storage
    label .restore.storage.cryptPasswordTitle -text "Password:"
    grid .restore.storage.cryptPasswordTitle -row 0 -column 0 -sticky "nw" 
    entry .restore.storage.cryptPassword -textvariable barConfig(cryptPassword) -bg white -show "*"
    grid .restore.storage.cryptPassword -row 0 -column 1 -sticky "w" -padx 2p -pady 2p

    label .restore.storage.sourceTitle -text "Source:"
    grid .restore.storage.sourceTitle -row 2 -column 0 -sticky "nw" 
    frame .restore.storage.source
      frame .restore.storage.source.type
        radiobutton .restore.storage.source.type.fileSystem -text "File system" -variable barConfig(storageType) -value "FILESYSTEM"
        grid .restore.storage.source.type.fileSystem -row 0 -column 0 -sticky "nw" 


        radiobutton .restore.storage.source.type.ssh -text "ssh" -variable barConfig(storageType) -value "SSH"
        grid .restore.storage.source.type.ssh -row 0 -column 1 -sticky "w" 

        radiobutton .restore.storage.source.type.scp -text "scp" -variable barConfig(storageType) -value "SCP"
        grid .restore.storage.source.type.scp -row 0 -column 2 -sticky "w" 

        radiobutton .restore.storage.source.type.sftp -text "sftp" -variable barConfig(storageType) -value "SFTP"
        grid .restore.storage.source.type.sftp -row 0 -column 3 -sticky "w" 

        radiobutton .restore.storage.source.type.device -text "Device" -variable barConfig(storageType) -value "DEVICE"
        grid .restore.storage.source.type.device -row 0 -column 4 -sticky "nw" 

        grid rowconfigure    .restore.storage.source.type { 0 } -weight 1
        grid columnconfigure .restore.storage.source.type { 5 } -weight 1
      grid .restore.storage.source.type -row 0 -column 0 -sticky "we" -padx 2p -pady 2p

      labelframe .restore.storage.source.fileSystem
        label .restore.storage.source.fileSystem.fileNameTitle -text "File name:"
        grid .restore.storage.source.fileSystem.fileNameTitle -row 0 -column 0 -sticky "w" 
        entry .restore.storage.source.fileSystem.fileName -textvariable barConfig(storageFileName) -bg white
        grid .restore.storage.source.fileSystem.fileName -row 0 -column 1 -sticky "we"
        button .restore.storage.source.fileSystem.selectDirectory -image $images(folder) -command \
        "
          set fileName \[Dialog:fileSelector \"Select archive\" \$barConfig(storageFileName) {{\"*.bar\" \"BAR archive\"} {\"*\" \"all\"}}\]
          if {\$fileName != \"\"} \
          {
            set barConfig(storageFileName) \$fileName
            newRestoreArchive
          }
        "
        grid .restore.storage.source.fileSystem.selectDirectory -row 0 -column 2

        grid rowconfigure    .restore.storage.source.fileSystem { 0 } -weight 1
        grid columnconfigure .restore.storage.source.fileSystem { 1 } -weight 1
      addModifyTrace {::barConfig(storageType)} \
      {
        if {$::barConfig(storageType) == "FILESYSTEM"} \
        {
          grid .restore.storage.source.fileSystem -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
        } \
        else \
        {
          grid forget .restore.storage.source.fileSystem
        }
      }

      labelframe .restore.storage.source.ssh
        label .restore.storage.source.ssh.fileNameTitle -text "Directory name:"
        grid .restore.storage.source.ssh.fileNameTitle -row 0 -column 0 -sticky "w" 
        entry .restore.storage.source.ssh.fileName -textvariable barConfig(storageFileName) -bg white
        grid .restore.storage.source.ssh.fileName -row 0 -column 1 -columnspan 5 -sticky "we" 
if {0} {
        button .restore.storage.source.ssh.selectDirectory -image $images(folder) -command \
        "
          set old_tk_strictMotif \$tk_strictMotif
          set tk_strictMotif 0
          set fileName \[tk_chooseDirectory -title \"Select directory\" -initialdir \"\" -mustexist 1\]
          set tk_strictMotif \$old_tk_strictMotif
          if {\$fileName != \"\"} \
          {
            set barConfig(storagefileName) \$fileName
            newRestoreArchive
          }
        "
        grid .restore.storage.source.ssh.selectDirectory -row 0 -column 5
}

        label .restore.storage.source.ssh.loginTitle -text "Login:"
        grid .restore.storage.source.ssh.loginTitle -row 1 -column 0 -sticky "w" 
        frame .restore.storage.source.ssh.login
          entry .restore.storage.source.ssh.login.name -textvariable barConfig(storageLoginName) -bg white
          pack .restore.storage.source.ssh.login.name -side left -fill x -expand yes

      #    label .restore.storage.source.ssh.login.passwordTitle -text "Password:"
      #    grid .restore.storage.source.ssh.login.passwordTitle -row 0 -column 2 -sticky "w" 
      #    entry .restore.storage.source.ssh.login.password -textvariable barConfig(sshPassword) -bg white -show "*"
      #    grid .restore.storage.source.ssh.login.password -row 0 -column 3 -sticky "we" 

          label .restore.storage.source.ssh.login.hostNameTitle -text "Host:"
          pack .restore.storage.source.ssh.login.hostNameTitle -side left
          entry .restore.storage.source.ssh.login.hostName -textvariable barConfig(storageHostName) -bg white
          pack .restore.storage.source.ssh.login.hostName -side left -fill x -expand yes

          label .restore.storage.source.ssh.login.sshPortTitle -text "SSH port:"
          pack .restore.storage.source.ssh.login.sshPortTitle -side left
          tixControl .restore.storage.source.ssh.login.sshPort -variable barConfig(sshPort) -label "" -labelside right -integer true -min 0 -max 65535 -options { entry.background white }
          pack .restore.storage.source.ssh.login.sshPort -side left -fill x -expand yes
        grid .restore.storage.source.ssh.login -row 1 -column 1 -sticky "we"

        label .restore.storage.source.ssh.sshPublicKeyFileNameTitle -text "SSH public key:"
        grid .restore.storage.source.ssh.sshPublicKeyFileNameTitle -row 2 -column 0 -sticky "w" 
        frame .restore.storage.source.ssh.sshPublicKeyFileName
          entry .restore.storage.source.ssh.sshPublicKeyFileName.data -textvariable barConfig(sshPublicKeyFileName) -bg white
          pack .restore.storage.source.ssh.sshPublicKeyFileName.data -side left -fill x -expand yes
          button .restore.storage.source.ssh.sshPublicKeyFileName.select -image $images(folder) -command \
          "
            set fileName \[Dialog:fileSelector \"Select SSH public key file\" \$barConfig(sshPublicKeyFileName) {{\"*.pub\" \"Public key\"} {\"*\" \"all\"}}\]
            if {\$fileName != \"\"} \
            {
              set barConfig(sshPublicKeyFileName) \$fileName
            }
          "
          pack .restore.storage.source.ssh.sshPublicKeyFileName.select -side left
        grid .restore.storage.source.ssh.sshPublicKeyFileName -row 2 -column 1 -sticky "we"

        label .restore.storage.source.ssh.sshPrivatKeyFileNameTitle -text "SSH privat key:"
        grid .restore.storage.source.ssh.sshPrivatKeyFileNameTitle -row 3 -column 0 -sticky "w" 
        frame .restore.storage.source.ssh.sshPrivatKeyFileName
          entry .restore.storage.source.ssh.sshPrivatKeyFileName.data -textvariable barConfig(sshPrivatKeyFileName) -bg white
          pack .restore.storage.source.ssh.sshPrivatKeyFileName.data -side left -fill x -expand yes
          button .restore.storage.source.ssh.sshPrivatKeyFileName.select -image $images(folder) -command \
          "
            set fileName \[Dialog:fileSelector \"Select SSH privat key file\" \$barConfig(sshPrivatKeyFileName) {{\"*\" \"all\"}}\]
            if {\$fileName != \"\"} \
            {
              set barConfig(sshPrivatKeyFileName) \$fileName
            }
          "
          pack .restore.storage.source.ssh.sshPrivatKeyFileName.select -side left
        grid .restore.storage.source.ssh.sshPrivatKeyFileName -row 3 -column 1 -sticky "we"

  #      grid rowconfigure    .restore.storage.source.ssh { } -weight 1
        grid columnconfigure .restore.storage.source.ssh { 1 } -weight 1
      addModifyTrace {::barConfig(storageType)} \
      {
        if {($::barConfig(storageType) == "SSH") || ($::barConfig(storageType) == "SCP") || ($::barConfig(storageType) == "SFTP")} \
        {
          grid .restore.storage.source.ssh -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
        } \
        else \
        {
          grid forget .restore.storage.source.ssh
        }
      }

      labelframe .restore.storage.source.device
        label .restore.storage.source.device.nameTitle -text "Device name:"
        grid .restore.storage.source.device.nameTitle -row 0 -column 0 -sticky "w" 
        entry .restore.storage.source.device.name -textvariable barConfig(storageDeviceName) -bg white
        grid .restore.storage.source.device.name -row 0 -column 1 -sticky "we" 

        label .restore.storage.source.device.fileNameTitle -text "File name:"
        grid .restore.storage.source.device.fileNameTitle -row 1 -column 0 -sticky "w" 
        entry .restore.storage.source.device.fileName -textvariable barConfig(storageFileName) -bg white
        grid .restore.storage.source.device.fileName -row 1 -column 1 -sticky "we" 
        button .restore.storage.source.device.selectDirectory -image $images(folder) -command \
        "
          set fileName \[Dialog:fileSelector \"Select archive\" \$barConfig(storagefileName) {{\"*.bar\" \"BAR archive\"} {\"*\" \"all\"}}\]
          if {\$fileName != \"\"} \
          {
            set barConfig(storagefileName) \$fileName
            newRestoreArchive
          }
         "
        grid .restore.storage.source.device.selectDirectory -row 0 -column 2

        grid rowconfigure    .restore.storage.source.device { 2 } -weight 1
        grid columnconfigure .restore.storage.source.device { 1 } -weight 1
      addModifyTrace {::barConfig(storageType)} \
      {
        if {$::barConfig(storageType) == "DEVICE"} \
        {
          grid .restore.storage.source.device -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
        } \
        else \
        {
          grid forget .restore.storage.source.device
        }
      }

      grid rowconfigure    .restore.storage.source { 0 } -weight 1
      grid columnconfigure .restore.storage.source { 0 } -weight 1
    grid .restore.storage.source -row 2 -column 1 -sticky "we"

    grid rowconfigure    .restore.storage { 3 } -weight 1
    grid columnconfigure .restore.storage { 1 } -weight 1
  pack .restore.storage -side top -fill both -expand yes -in [.restore.tabs subwidget storage]

  frame .restore.files
    tixTree .restore.files.list -scrollbar both -options \
    {
      hlist.separator "|"
      hlist.columns 4
      hlist.header yes
      hlist.indent 16
    }
    .restore.files.list subwidget hlist configure -selectmode extended

    .restore.files.list subwidget hlist header create 0 -itemtype text -text "File"
    .restore.files.list subwidget hlist header create 1 -itemtype text -text "Type"
    .restore.files.list subwidget hlist column width 1 -char 10
    .restore.files.list subwidget hlist header create 2 -itemtype text -text "Size"
    .restore.files.list subwidget hlist column width 2 -char 10
    .restore.files.list subwidget hlist header create 3 -itemtype text -text "Modified"
    .restore.files.list subwidget hlist column width 3 -char 15
    grid .restore.files.list -row 0 -column 0 -sticky "nswe" -padx 2p -pady 2p
    set restoreFilesTreeWidget [.restore.files.list subwidget hlist]

    frame .restore.files.buttons
      button .restore.files.buttons.stateNone -text "*" -command "event generate . <<Event_restoreStateNone>>"
      pack .restore.files.buttons.stateNone -side left -fill x -expand yes
      button .restore.files.buttons.stateIncluded -text "+" -command "event generate . <<Event_restoreStateIncluded>>"
      pack .restore.files.buttons.stateIncluded -side left -fill x -expand yes
      button .restore.files.buttons.stateExcluded -text "-" -command "event generate . <<Event_restoreStateExcluded>>"
      pack .restore.files.buttons.stateExcluded -side left -fill x -expand yes
    grid .restore.files.buttons -row 1 -column 0 -sticky "we" -padx 2p -pady 2p

    frame .restore.files.destination
      label .restore.files.destination.directoryNameTitle -text "Destination directory:"
      grid .restore.files.destination.directoryNameTitle -row 0 -column 0 -sticky "w" 
      entry .restore.files.destination.directoryName -textvariable barConfig(destinationDirectoryName) -bg white
      grid .restore.files.destination.directoryName -row 0 -column 1 -sticky "we"
      button .restore.files.destination.selectDirectory -image $images(folder) -command \
      "
        set old_tk_strictMotif \$tk_strictMotif
        set tk_strictMotif 0
        set directoryName \[tk_choosePath -title \"Select directory\" -initialdir \"\" -parent .]
        set tk_strictMotif \$old_tk_strictMotif
        if {\$directoryName != \"\"} \
        {
          set barConfig(destinationDirectoryName) \$directoryName
        }
      "
      grid .restore.files.destination.selectDirectory -row 0 -column 2

      label .restore.files.destination.directoryStripCountTitle -text "Directory strip count:"
      grid .restore.files.destination.directoryStripCountTitle -row 1 -column 0 -sticky "w" 
      tixControl .restore.files.destination.directoryStripCount -width 5 -variable barConfig(destinationStripCount) -label "" -labelside right -integer true -min 0 -options { entry.background white }
      grid .restore.files.destination.directoryStripCount -row 1 -column 1 -sticky "w" 

      grid rowconfigure    .restore.files.destination { 0 } -weight 1
      grid columnconfigure .restore.files.destination { 1 } -weight 1
    grid .restore.files.destination -row 2 -column 0 -sticky "we" -padx 2p -pady 2p 

    tixPopupMenu .restore.files.list.popup -title "Command"
    .restore.files.list.popup subwidget menu add command -label "Add include"                            -command ""
    .restore.files.list.popup subwidget menu add command -label "Add exclude"                            -command ""
    .restore.files.list.popup subwidget menu add command -label "Remove include"                         -command ""
    .restore.files.list.popup subwidget menu add command -label "Remove exclude"                         -command ""
    .restore.files.list.popup subwidget menu add command -label "Add include pattern"    -state disabled -command ""
    .restore.files.list.popup subwidget menu add command -label "Add exclude pattern"    -state disabled -command ""
    .restore.files.list.popup subwidget menu add command -label "Remove include pattern" -state disabled -command ""
    .restore.files.list.popup subwidget menu add command -label "Remove exclude pattern" -state disabled -command ""

    proc restoreFilesPopupHandler { widget x y } \
    {
      set fileName [$widget nearest $y]
      set extension ""
      regexp {.*(\.[^\.]+)} $fileName * extension

      .restore.files.list.popup subwidget menu entryconfigure 0 -label "Add include '$fileName'"    -command "restoreAddIncludedPattern $fileName"
      .restore.files.list.popup subwidget menu entryconfigure 1 -label "Add exclude '$fileName'"    -command "restoreAddExcludedPattern $fileName"
      .restore.files.list.popup subwidget menu entryconfigure 2 -label "Remove include '$fileName'" -command "restoreRemIncludedPattern $fileName"
      .restore.files.list.popup subwidget menu entryconfigure 3 -label "Remove exclude '$fileName'" -command "restoreRemExcludedPattern $fileName"
      if {$extension != ""} \
      {
        .restore.files.list.popup subwidget menu entryconfigure 4 -label "Add include pattern *$extension"    -state normal -command "restoreAddIncludedPattern *$extension"
        .restore.files.list.popup subwidget menu entryconfigure 5 -label "Add exclude pattern *$extension"    -state normal -command "restoreAddExcludedPattern *$extension"
        .restore.files.list.popup subwidget menu entryconfigure 6 -label "Remove include pattern *$extension" -state normal -command "restoreRemIncludedPattern *$extension"
        .restore.files.list.popup subwidget menu entryconfigure 7 -label "Remove exclude pattern *$extension" -state normal -command "restoreRemExcludedPattern *$extension"
      } \
      else \
      {
        .restore.files.list.popup subwidget menu entryconfigure 4 -label "Add include pattern -"    -state disabled -command ""
        .restore.files.list.popup subwidget menu entryconfigure 5 -label "Add exclude pattern -"    -state disabled -command ""
        .restore.files.list.popup subwidget menu entryconfigure 6 -label "Remove include pattern -" -state disabled -command ""
        .restore.files.list.popup subwidget menu entryconfigure 7 -label "Remove exclude pattern -" -state disabled -command ""
      }
      .restore.files.list.popup post $widget $x $y
    }

    bind [.restore.files.list subwidget hlist] <Button-3>    "restoreFilesPopupHandler %W %x %y"
    bind [.restore.files.list subwidget hlist] <BackSpace>   "event generate . <<Event_restoreStateNone>>"
    bind [.restore.files.list subwidget hlist] <Delete>      "event generate . <<Event_restoreStateNone>>"
    bind [.restore.files.list subwidget hlist] <plus>        "event generate . <<Event_restoreStateIncluded>>"
    bind [.restore.files.list subwidget hlist] <KP_Add>      "event generate . <<Event_restoreStateIncluded>>"
    bind [.restore.files.list subwidget hlist] <minus>       "event generate . <<Event_restoreStateExcluded>>"
    bind [.restore.files.list subwidget hlist] <KP_Subtract> "event generate . <<Event_restoreStateExcluded>>"
    bind [.restore.files.list subwidget hlist] <space>       "event generate . <<Event_restoreToggleStateNoneIncludedExcluded>>"

    # fix a bug in tix: end does not use separator-char to detect last entry
    bind [.restore.files.list subwidget hlist] <KeyPress-End> \
      "
       .restore.files.list subwidget hlist yview moveto 1
       .restore.files.list subwidget hlist anchor set \[lindex \[.restore.files.list subwidget hlist info children /\] end\]
       break
      "

    # mouse-wheel events
    bind [.restore.files.list subwidget hlist] <Button-4> \
      "
       set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
       .restore.files.list subwidget hlist yview scroll -\$n units
      "
    bind [.restore.files.list subwidget hlist] <Button-5> \
      "
       set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
       .restore.files.list subwidget hlist yview scroll +\$n units
      "

    grid rowconfigure    .restore.files { 0 } -weight 1
    grid columnconfigure .restore.files { 0 } -weight 1
  pack .restore.files -side top -fill both -expand yes -in [.restore.tabs subwidget files]

  frame .restore.filters
    label .restore.filters.includedTitle -text "Included:"
    grid .restore.filters.includedTitle -row 0 -column 0 -sticky "nw"
    tixScrolledListBox .restore.filters.included -height 1 -scrollbar both -options { listbox.background white  }
    grid .restore.filters.included -row 0 -column 1 -sticky "nswe" -padx 2p -pady 2p
    .restore.filters.included subwidget listbox configure -listvariable barConfig(included) -selectmode extended
    set restoreIncludedListWidget [.restore.filters.included subwidget listbox]

    bind [.restore.filters.included subwidget listbox] <Button-1> ".restore.filters.includedButtons.rem configure -state normal"

    frame .restore.filters.includedButtons
      button .restore.filters.includedButtons.add -text "Add (F5)" -command "event generate . <<Event_restoreAddIncludePattern>>"
      pack .restore.filters.includedButtons.add -side left
      button .restore.filters.includedButtons.rem -text "Rem (F6)" -state disabled -command "event generate . <<Event_restoreRemIncludePattern>>"
      pack .restore.filters.includedButtons.rem -side left
    grid .restore.filters.includedButtons -row 1 -column 1 -sticky "we" -padx 2p -pady 2p

    bind [.restore.filters.included subwidget listbox] <Insert> "event generate . <<Event_restoreAddIncludePattern>>"
    bind [.restore.filters.included subwidget listbox] <Delete> "event generate . <<Event_restoreRemIncludePattern>>"

    label .restore.filters.excludedTitle -text "Excluded:"
    grid .restore.filters.excludedTitle -row 2 -column 0 -sticky "nw"
    tixScrolledListBox .restore.filters.excluded -height 1 -scrollbar both -options { listbox.background white }
    grid .restore.filters.excluded -row 2 -column 1 -sticky "nswe" -padx 2p -pady 2p
    .restore.filters.excluded subwidget listbox configure -listvariable barConfig(excluded) -selectmode extended
    set restoreExcludedListWidget [.restore.filters.excluded subwidget listbox]

    bind [.restore.filters.excluded subwidget listbox] <Button-1> ".restore.filters.excludedButtons.rem configure -state normal"

    frame .restore.filters.excludedButtons
      button .restore.filters.excludedButtons.add -text "Add (F7)" -command "event generate . <<Event_restoreAddExcludePattern>>"
      pack .restore.filters.excludedButtons.add -side left
      button .restore.filters.excludedButtons.rem -text "Rem (F8)" -state disabled -command "event generate . <<Event_restoreRemExcludePattern>>"
      pack .restore.filters.excludedButtons.rem -side left
    grid .restore.filters.excludedButtons -row 3 -column 1 -sticky "we" -padx 2p -pady 2p

    bind [.restore.filters.excluded subwidget listbox] <Insert> "event generate . <<Event_restoreAddExcludePattern>>"
    bind [.restore.filters.excluded subwidget listbox] <Delete> "event generate . <<Event_restoreRemExcludePattern>>"

    label .restore.filters.optionsTitle -text "Options:"
    grid .restore.filters.optionsTitle -row 4 -column 0 -sticky "nw" 
    checkbutton .restore.filters.optionSkipUnreadable -text "skip not writable files" -variable barConfig(skipNotWritableFlag)
    grid .restore.filters.optionSkipUnreadable -row 4 -column 1 -sticky "nw" 

    grid rowconfigure    .restore.filters { 0 2 } -weight 1
    grid columnconfigure .restore.filters { 1 } -weight 1
  pack .restore.filters -side top -fill both -expand yes -in [.restore.tabs subwidget filters]

  frame .restore.buttons
    button .restore.buttons.addRestoreJob -text "Start restore" -command "event generate . <<Event_addRestoreJob>>"
    pack .restore.buttons.addRestoreJob -side left -padx 2p
    button .restore.buttons.quit -text "Quit" -command "event generate . <<Event_quit>>"
    pack .restore.buttons.quit -side right -padx 2p
  grid .restore.buttons -row 2 -column 0 -columnspan 2 -sticky "we" -padx 2p -pady 2p

  grid rowconfigure    .restore { 1 } -weight 1
  grid columnconfigure .restore { 1 } -weight 1
pack .restore -side top -fill both -expand yes -in [$mainWindow.tabs subwidget restore]

update

# ----------------------------------------------------------------------

$backupFilesTreeWidget configure -command "openCloseBackupDirectory"
$restoreFilesTreeWidget configure -command "openCloseRestoreDirectory"

addModifyTrace ::currentJob(id) \
  "
    if {\$::currentJob(id) != 0} \
    {
       .jobs.selected configure -text \"Selected #\$::currentJob(id)\"
       .jobs.buttons.abort configure -state normal
       .jobs.buttons.rem configure -state normal
    } \
    else \
    {
       .jobs.selected configure -text \"Selected\"
       .jobs.buttons.abort configure -state disabled
       .jobs.buttons.rem configure -state disabled
    }
  "
addModifyTrace ::currentJob(fileDoneBytes) \
  "
    global currentJob

    if {\$currentJob(fileTotalBytes) > 0} \
    {
      set p \[expr {double(\$currentJob(fileDoneBytes))/\$currentJob(fileTotalBytes)}]
      Dialog:progressbar .jobs.selected.filePercentage update \$p
    }
  "
addModifyTrace ::currentJob(fileTotalBytes) \
  "
    global currentJob

    if {\$currentJob(fileTotalBytes) > 0} \
    {
      set p \[expr {double(\$currentJob(fileDoneBytes))/\$currentJob(fileTotalBytes)}]
      Dialog:progressbar .jobs.selected.filePercentage update \$p
    }
  "
addModifyTrace ::currentJob(storageDoneBytes) \
  "
    global currentJob

    if {\$currentJob(storageTotalBytes) > 0} \
    {
      set p \[expr {double(\$currentJob(storageDoneBytes))/\$currentJob(storageTotalBytes)}]
      Dialog:progressbar .jobs.selected.storagePercentage update \$p
    }
  "
addModifyTrace ::currentJob(doneBytes) \
  "
    global currentJob

    if {\$currentJob(totalBytes) > 0} \
    {
      set p \[expr {double(\$currentJob(doneBytes))/\$currentJob(totalBytes)}]
      Dialog:progressbar .jobs.selected.totalBytesPercentage update \$p
    }
  "
addModifyTrace ::currentJob(doneFiles) \
  "
    global currentJob

    if {\$currentJob(totalFiles) > 0} \
    {
      set p \[expr {double(\$currentJob(doneFiles))/\$currentJob(totalFiles)}]
      Dialog:progressbar .jobs.selected.totalFilesPercentage update \$p
    }
  "
addModifyTrace ::currentJob(storageTotalBytes) \
  "
    global currentJob

    if {\$currentJob(storageTotalBytes) > 0} \
    {
      set p \[expr {double(\$currentJob(storageDoneBytes))/\$currentJob(storageTotalBytes)}]
      Dialog:progressbar .jobs.selected.storagePercentage update \$p
    }
  "
addModifyTrace ::currentJob(volumeProgress) \
  "
    global currentJob

    Dialog:progressbar .jobs.selected.storageVolume update \$::currentJob(volumeProgress)
  "
addModifyTrace {::barConfig(name) ::barConfig(included)} \
  "
    if {(\$::barConfig(name) != \"\") && (\$::barConfig(included) != {})} \
    {
      .backup.buttons.addBackupJob configure -state normal
      .restore.buttons.addRestoreJob configure -state normal
    } \
    else \
    {
      .backup.buttons.addBackupJob configure -state disabled
      .restore.buttons.addRestoreJob configure -state disabled
    }
  "
addModifyTrace ::currentJob(totalFiles) \
  "
    global currentJob

    if {\$currentJob(totalFiles) > 0} \
    {
      set p \[expr {double(\$currentJob(doneFiles))/\$currentJob(totalFiles)}]
      Dialog:progressbar .jobs.selected.totalFilesPercentage update \$p
    }
  "
addModifyTrace ::currentJob(totalBytes) \
  "
    global currentJob

    if {\$currentJob(totalBytes) > 0} \
    {
      set p \[expr {double(\$currentJob(doneBytes))/\$currentJob(totalBytes)}]
      Dialog:progressbar .jobs.selected.totalBytesPercentage update \$p
    }
  "

addModifyTrace {::barConfig(storageType) ::barConfig(storageFileName)} \
  "
    newRestoreArchive
  "
addFocusTrace {.restore.storage.source.fileSystem.fileName} \
  "" \
  "
    newRestoreArchive
  "
addModifyTrace {::currentJob(requestedVolumeNumber)} \
  {
    if {($::currentJob(volumeNumber) != $::currentJob(requestedVolumeNumber)) && ($::currentJob(requestedVolumeNumber) > 0)} \
    {
if {0} {
      switch [Dialog:select "Request" "Please insert volume #$::currentJob(requestedVolumeNumber) into drive." "" [list [list "Continue"] [list "Abort job"] [list "Cancel" Escape]]]
      {
        0 \
        {
          set errorCode 0
          BackupServer:executeCommand errorCode errorText "VOLUME" $::currentJob(id) $::currentJob(requestedVolumeNumber)
        }
        1 \
        {
          set errorCode 0
          BackupServer:executeCommand errorCode errorText "ABORT_JOB" $::currentJob(id)
        }
        2 \
        {
        }
      }
}
      .jobs.buttons.volume configure -state normal
      set ::currentJob(message) "Please insert volume #$::currentJob(requestedVolumeNumber) into drive."
    } \
    else \
    {
      .jobs.buttons.volume configure -state disabled
      set ::currentJob(message) ""
    }
  }

bind . <Control-o> "event generate . <<Event_load>>"
bind . <Control-s> "event generate . <<Event_save>>"
bind . <Control-q> "event generate . <<Event_quit>>"

bind . <F1> "$mainWindow.tabs raise jobs"
bind . <F2> "$mainWindow.tabs raise backup"
bind . <F3> "$mainWindow.tabs raise restore"
bind . <F5> "event generate . <<Event_backupAddIncludePattern>>"
bind . <F6> "event generate . <<Event_backupRemIncludePattern>>"
bind . <F7> "event generate . <<Event_backupAddExcludePattern>>"
bind . <F8> "event generate . <<Event_backupRemExcludePattern>>"

bind . <<Event_new>> \
{
  if {[Dialog:confirm "New configuration?" "New" "Cancel"]} \
  {
    barConfigFileName
    resetBARConfig
    clearConfigModify
  }
}

bind . <<Event_load>> \
{
  loadBARConfig ""
}

bind . <<Event_save>> \
{
  saveBARConfig $barConfigFileName
}

bind . <<Event_saveAs>> \
{
  saveBARConfig ""
}

bind . <<Event_quit>> \
{
  quit
}

bind . <<Event_backupStateNone>> \
{
  foreach itemPath [$backupFilesTreeWidget info selection] \
  {
    setEntryState $backupFilesTreeWidget $itemPath 0 "NONE"
  }
}

bind . <<Event_backupStateIncluded>> \
{
  foreach itemPath [$backupFilesTreeWidget info selection] \
  {
    setEntryState $backupFilesTreeWidget $itemPath 0 "INCLUDED"
  }
}

bind . <<Event_backupStateExcluded>> \
{
  foreach itemPath [$backupFilesTreeWidget info selection] \
  {
    setEntryState $backupFilesTreeWidget $itemPath 0 "EXCLUDED"
  }
}

bind . <<Event_backupToggleStateNoneIncludedExcluded>> \
{
  foreach itemPath [$backupFilesTreeWidget info selection] \
  {
    toggleEntryIncludedExcluded $backupFilesTreeWidget $itemPath 0
  }
}

bind . <<Event_restoreStateNone>> \
{
  foreach itemPath [$restoreFilesTreeWidget info selection] \
  {
    setEntryState $restoreFilesTreeWidget $itemPath 1 "NONE"
  }
}

bind . <<Event_restoreStateIncluded>> \
{
  foreach itemPath [$restoreFilesTreeWidget info selection] \
  {
    setEntryState $restoreFilesTreeWidget $itemPath 1 "INCLUDED"
  }
}

bind . <<Event_restoreStateExcluded>> \
{
  foreach itemPath [$restoreFilesTreeWidget info selection] \
  {
    setEntryState $restoreFilesTreeWidget $itemPath 1 "EXCLUDED"
  }
}

bind . <<Event_restoreToggleStateNoneIncludedExcluded>> \
{
  foreach itemPath [$restoreFilesTreeWidget info selection] \
  {
    toggleEntryIncludedExcluded $restoreFilesTreeWidget $itemPath 1
  }
}

bind . <<Event_backupAddIncludePattern>> \
{
  addIncludedPattern ""
}

bind . <<Event_backupRemIncludePattern>> \
{
  set patternList {}
  foreach index [$backupIncludedListWidget curselection] \
  {
    lappend patternList [$backupIncludedListWidget get $index]
  }
  foreach pattern $patternList \
  {
    remIncludedPattern $pattern
  }
  $backupIncludedListWidget selection clear 0 end
  .backup.filters.includedButtons.rem configure -state disabled
}

bind . <<Event_backupAddExcludePattern>> \
{
  addExcludedPattern ""
}

bind . <<Event_backupRemExcludePattern>> \
{
puts 11323
  set patternList {}
  foreach index [$backupExcludedListWidget curselection] \
  {
    lappend patternList [$backupExcludedListWidget get $index]
  }
puts $patternList
  foreach pattern $patternList \
  {
    remExcludedPattern $pattern
  }
  $backupExcludedListWidget selection clear 0 end
  .backup.filters.excludedButtons.rem configure -state disabled
}

bind . <<Event_restoreAddIncludePattern>> \
{
  addIncludedPattern ""
}

bind . <<Event_restoreRemIncludePattern>> \
{
  set patternList {}
  foreach index [$restoreIncludedListWidget curselection] \
  {
    lappend patternList [$restoreIncludedListWidget get $index]
  }
  foreach pattern $patternList \
  {
    remIncludedPattern $pattern
  }
  $restoreIncludedListWidget selection clear 0 end
  .restore.filters.includedButtons.rem configure -state disabled
}

bind . <<Event_restoreAddExcludePattern>> \
{
  addExcludedPattern ""
}

bind . <<Event_restoreRemExcludePattern>> \
{
  set patternList {}
  foreach index [$restoreExcludedListWidget curselection] \
  {
    lappend patternList [$restoreExcludedListWidget get $index]
  }
  foreach pattern $patternList \
  {
    remExcludedPattern $pattern
  }
  $restoreExcludedListWidget selection clear 0 end
  .restore.filters.excludedButtons.rem configure -state disabled
}

bind . <<Event_selectJob>> \
{
  set n [.jobs.list.data curselection]
  if {$n != {}} \
  {
    set currentJob(id) [lindex [lindex [.jobs.list.data get $n $n] 0] 0]
  }
}

bind . <<Event_addBackupJob>> \
{
  if {[Dialog:confirm "Start this backup?" "Start backup" "Cancel"]} \
  {
    addBackupJob .jobs.list.data
  }
}

bind . <<Event_addRestoreJob>> \
{
  if {[Dialog:confirm "Start this restore?" "Start restore" "Cancel"]} \
  {
    addRestoreJob .jobs.list.data
  }
}

bind . <<Event_remJob>> \
{
  set n [.jobs.list.data curselection]
  if {$n != {}} \
  {
    set currentJob(id) 0
    set id [lindex [lindex [.jobs.list.data get $n $n] 0] 0]
    remJob .jobs.list.data $id
  }
}

bind . <<Event_abortJob>> \
{
  set n [.jobs.list.data curselection]
  if {$n != {}} \
  {
    if {[Dialog:confirm "Really abort job?" "Abort job" "Cancel"]} \
    {
      set id [lindex [lindex [.jobs.list.data get $n $n] 0] 0]
      abortJob .jobs.list.data $id
    }
  }
}

bind . <<Event_volume>> \
{
  set errorCode 0
  BackupServer:executeCommand errorCode errorText "VOLUME" $::currentJob(id) $::currentJob(requestedVolumeNumber)
}

# config modify trace
trace add variable barConfig write "setConfigModify"

# connect to server
if     {($barControlConfig(serverTLSPort) != 0) && ![catch {tls::init -version}]} \
{
  if {[catch {tls::init -cafile $barControlConfig(serverCAFileName)}]} \
  {
    printError "Cannot initialise TLS/SSL system"
    exit 1
  }
  if {![BackupServer:connect $barControlConfig(serverHostName) $barControlConfig(serverTLSPort) $barControlConfig(serverPassword) 1]} \
  {
    printError "Cannot connect to TLS/SSL server '$barControlConfig(serverHostName):$barControlConfig(serverTLSPort)'!"
    exit 1
  }
} \
elseif {$barControlConfig(serverPort) != 0} \
{
  if {![BackupServer:connect $barControlConfig(serverHostName) $barControlConfig(serverPort) $barControlConfig(serverPassword) 0]} \
  {
    printError "Cannot connect to server '$barControlConfig(serverHostName):$barControlConfig(serverPort)'!"
    exit 1
  }
} \
else  \
{
  printError "Cannot connect to server '$barControlConfig(serverHostName)'!"
  exit 1
}

updateJobList .jobs.list.data
updateCurrentJob

# read devices
#set commandId [BackupServer:sendCommand "DEVICE_LIST"]
#while {[BackupServer:readResult $commandId completeFlag errorCode result]} \
#{
#  addBackupDevice $result
#}
addBackupDevice "/"

# load config if given
resetBARConfig
if {$configFileName != ""} \
{
  if {[file exists $configFileName]} \
  {
    loadBARConfig $configFileName
    if {$startFlag} \
    {
      addBackupJob .jobs.list.data
    }
  } \
  else \
  {
    if {[Dialog:query "Confirmation" "Configuration file '$configFileName' does not exists - create it?" "Create" "Cancel"]} \
    {
      set barConfigFileName $configFileName
      set barConfig(name) [file tail $configFileName]
    }
  }
}

#Dialog:password "xxx" 0
#editStorageFileName "test-%type-%a-###.bar"
#Dialog:fileSelector "x" "/tmp/test.bnid" {}

# end of file
