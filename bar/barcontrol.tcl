#!/bin/sh
#\
exec tclsh "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol.tcl,v $
# $Revision: 1.26 $
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

set TIMEDATE_FORMAT "%Y-%m-%d %H:%M:%S"

# --------------------------------- includes ---------------------------------

# ------------------------ internal constants/variables ----------------------

# configuration
set barControlConfig(serverHostName)       "localhost"
set barControlConfig(serverPort)           $DEFAULT_PORT
set barControlConfig(serverPassword)       ""
set barControlConfig(serverTLSPort)        $DEFAULT_TLS_PORT
set barControlConfig(serverCAFileName)     "$env(HOME)/.bar/bar-ca.pem"
set barControlConfig(statusListUpdateTime) 5000
set barControlConfig(statusUpdateTime)     1000

# global settings
set configFileName  ""
set listFlag        0
set startFlag       0
set fullFlag        0
set incrementalFlag 0
set abortId         0
set pauseFlag       0
set continueFlag    0
set quitFlag        0
set debugFlag       0
set guiMode         0

set passwordObfuscator [format "%c%c%c%c%c%c%c%c" [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}] [expr {int(rand()*256)}]]

# command id counter
set lastCommandId    0

# modify traces on/off (on iff 0)
set modifyTrace 0

# server variables
set server(socketHandle)   -1
set server(authorizedFlag) 0

# BAR configuration
set barConfigFileName     ""
set barConfigModifiedFlag 0

set barConfig(name)                            ""
set barConfig(included)                        {}
set barConfig(excluded)                        {}
set barConfig(schedule)                        {}
set barConfig(storageType)                     ""
set barConfig(storageFileName)                 ""
set barConfig(storageLoginName)                ""
set barConfig(storageHostName)                 ""
set barConfig(storageDeviceName)               ""
set barConfig(archivePartSizeFlag)             0
set barConfig(archivePartSize)                 0
set barConfig(maxTmpSizeFlag)                  0
set barConfig(maxTmpSize)                      0
set barConfig(archiveType)                     "normal"
set barConfig(incrementalListFileName)         ""
set barConfig(maxBandWidthFlag)                0
set barConfig(maxBandWidth)                    0
set barConfig(sshPort)                         0
set barConfig(sshPublicKeyFileName)            ""
set barConfig(sshPrivateKeyFileName)           ""
set barConfig(compressAlgorithm)               ""
set barConfig(cryptAlgorithm)                  "none"
set barConfig(cryptPasswordMode)               "default"
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

set status "ok"

set jobStatus(id)                              0
set jobStatus(name)                            ""
set jobStatus(state)                           ""
set jobStatus(error)                           0
set jobStatus(doneFiles)                       0
set jobStatus(doneBytes)                       0
set jobStatus(doneBytesShort)                  0
set jobStatus(doneBytesShortUnit)              "KBytes"
set jobStatus(totalFiles)                      0
set jobStatus(totalBytes)                      0
set jobStatus(totalBytesShort)                 0
set jobStatus(totalBytesShortUnit)             "KBytes"
set jobStatus(skippedFiles)                    0
set jobStatus(skippedBytes)                    0
set jobStatus(skippedBytesShort)               0
set jobStatus(skippedBytesShortUnit)           "KBytes"
set jobStatus(errorFiles)                      0
set jobStatus(errorBytes)                      0
set jobStatus(errorBytesShort)                 0
set jobStatus(errorBytesShortUnit)             "KBytes"
set jobStatus(filesPerSecond)                  0
set jobStatus(bytesPerSecond)                  0
set jobStatus(bytesPerSecondShort)             0
set jobStatus(bytesPerSecondShortUnit)         "KBytes/s"
set jobStatus(archiveBytes)                    0
set jobStatus(archiveBytesShort)               0
set jobStatus(archiveBytesShortUnit)           "KBytes"
set jobStatus(storageBytesPerSecond)           0
set jobStatus(storageBytesPerSecondShort)      0
set jobStatus(storageBytesPerSecondShortUnit)  "KBytes/s"
set jobStatus(compressionRatio)                0  
set jobStatus(fileName)                        "" 
set jobStatus(fileDoneBytes)                   0  
set jobStatus(fileTotalBytes)                  0  
set jobStatus(storageName)                     "" 
set jobStatus(storageDoneBytes)                0  
set jobStatus(storageTotalBytes)               0  
set jobStatus(storageTotalBytesShort)          0
set jobStatus(storageTotalBytesShortUnit)      "KBytes"
set jobStatus(volumeNumber)                    0
set jobStatus(volumeProgress)                  0.0
set jobStatus(requestedVolumeNumber)           0
set jobStatus(requestedVolumeNumberDialogFlag) 0
set jobStatus(message)                         ""

set selectedJob(id)                        0
set selectedJob(storageType)               ""
set selectedJob(storageFileName)           ""
set selectedJob(storageLoginName)          ""
set selectedJob(storageHostName)           ""
set selectedJob(storageDeviceName)         ""
set selectedJob(archiveType)               "normal"
set selectedJob(archivePartSizeFlag)       0
set selectedJob(archivePartSize)           0
#set selectedJob(destinationDirectoryName)  ""
#set selectedJob(destinationStripCount)     0
#set selectedJob(maxTmpSizeFlag)            0
#set selectedJob(maxTmpSize)                0
set selectedJob(incrementalListFileName)   ""
set selectedJob(compressAlgorithm)         ""
set selectedJob(cryptAlgorithm)            "none"
set selectedJob(cryptPasswordMode)         "default"
set selectedJob(cryptPassword)             ""
set selectedJob(cryptPasswordVerify)       ""
set selectedJob(sshPort)                   0
set selectedJob(sshPublicKeyFileName)      ""
set selectedJob(sshPrivateKeyFileName)     ""
#set selectedJob(maxBandWidthFlag)          0
#set selectedJob(maxBandWidth)              0
set selectedJob(volumeSize)                0
set selectedJob(skipUnreadableFlag)        1
#set selectedJob(skipNotWritableFlag)       1
set selectedJob(overwriteArchiveFilesFlag) 0
set selectedJob(overwriteFilesFlag)        0
set selectedJob(errorCorrectionCodesFlag)  1
set selectedJob(included)                  {}
set selectedJob(excluded)                  {}
set selectedJob(schedule)                  {}

# misc.
set statuListTimerId 0
set statusTimerId    0

set filesTreeWidget    ""
set includedListWidget ""
set excludedListWidget ""
set scheduleListWidget ""

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
    if {$::modifyTrace == 0} \
    {
      eval $action
    }
  }

  eval $action

  foreach name $nameList \
  {
    trace variable $name w "modifyTraceHandler {$action}"
  }
}

proc disableModifyTrace {} \
{
  incr ::modifyTrace 1
}

proc enableModifyTrace {} \
{
  if {$::modifyTrace <= 0} \
  {
    internalError "invalid 'modifyTrace' state"
  }
  incr ::modifyTrace -1
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
  puts "         --pause                      - pause jobs"
  puts "         --continue                   - continue jobs"
  puts "         --quit                       - quit"
  puts "         --debug                      - enable debug mode"
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
# Name   : unescapeString
# Purpose: unescape string: 's' -> s with unescaping
# Input  : s - string
# Output : -
# Return : escaped string
# Notes  : -
#***********************************************************************

proc unescapeString { s } \
{
  if {[regexp {^'(.*)'$} $s * t]} \
  {
    return $t;
  } \
  else \
  {
    return $s;
  }
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
# Name   : BackupServer:sendCommand
# Purpose: send command to server
# Input  : command - command
#          args    - arguments for command
# Output : -
# Return : command id or 0 on error
# Notes  : -
#***********************************************************************

proc BackupServer:sendCommand { command } \
{
  global server lastCommandId

  incr lastCommandId

  if {[catch {puts $server(socketHandle) "$lastCommandId $command"; flush $server(socketHandle)}]} \
  {
    return 0
  }
#puts "sent [clock clicks]: $command $lastCommandId"

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
# Output : _globalErrorCode - error code (will only be set if 0)
#          _globalErrorText - error text (will only be set if _errorCode is 0)
#          _result          - result (optional)
#          script           - script to execute on success (optional)
# Return : 1 if command executed, 0 otherwise
# Notes  : -
#***********************************************************************

proc BackupServer:executeCommand { _globalErrorCode _globalErrorText command {_result ""} {script {}} } \
{
  upvar $_globalErrorCode globalErrorCode
  upvar $_globalErrorText globalErrorText

  if {$globalErrorCode==0} \
  {
#puts "execute command: $command"
    set commandId [BackupServer:sendCommand $command]
    if {$commandId == 0} \
    {
      return 0
    }
    if {![BackupServer:readResult $commandId completeFlag errorCode result]} \
    {
      return 0
    }
#puts "execute result: error=$errorCode $result"

    if {$errorCode == 0} \
    {
#puts "_result=$_result"
      if {$_result != ""} \
      {
         if {[regexp {^::.*} $_result]} \
         {
           set $_result $result
         } \
         else \
         {
           upvar $_result tmp; set tmp $result
         }
      }
      eval "set result \"$result\"; $script"
      return 1
    } \
    else \
    {
      set globalErrorCode $errorCode
      set globalErrorText $result
      return 0
    }
  } \
  else \
  {
    return 0
  }
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
  set errorText ""
  BackupServer:executeCommand errorCode errorText "AUTHORIZE $s"
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
# Name   : BackupServer:get
# Purpose: get config value
# Input  : id       - job id
#          name     - name
#          variable - variable
#          script   - script to execute on success
# Output : 1 if command executed, 0 otherwise
# Notes  : -
#***********************************************************************

proc BackupServer:get { id name variable {script {}} } \
{
  set errorCode 0
  set errorText ""
  return [BackupServer:executeCommand errorCode errorText "OPTION_GET $id $name" $variable $script]
}

#***********************************************************************
# Name   : BackupServer:set
# Purpose: set config value
# Input  : id    - job id
#          name  - name
#          value - value
# Output : 1 if command executed, 0 otherwise
# Notes  : -
#***********************************************************************

proc BackupServer:set { id name value } \
{
  set errorCode 0
  set errorText ""
  return [BackupServer:executeCommand errorCode errorText "OPTION_SET $id $name $value"]
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
  set includedFlag 0
  foreach pattern $::selectedJob(included) \
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
  set excludedFlag 0
  foreach pattern $::selectedJob(excluded) \
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
  set included     {}
  set includedFlag 0
  set excluded     {}
  set excludedFlag 0
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
    set index [lsearch -sorted -exact $::selectedJob(excluded) $fileName]; if {$index >= 0} { set excluded [lreplace $::selectedJob(excluded) $index $index]; set excludedFlag 1 }
    set included [lsort -uniq [concat $::selectedJob(included) [list $fileName]]]; set includedFlag 1
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
    set index [lsearch -sorted -exact $::selectedJob(included) $fileName]; if {$index >= 0} { set included [lreplace $::selectedJob(included) $index $index]; set includedFlag 1 }
    set excluded [lsort -uniq [concat $::selectedJob(excluded) [list $fileName]]]; set excludedFlag 1
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
    set index [lsearch -sorted -exact $::selectedJob(included) $fileName]; if {$index >= 0} { set included [lreplace $::selectedJob(included) $index $index]; set includedFlag 1 }
    set index [lsearch -sorted -exact $::selectedJob(excluded) $fileName]; if {$index >= 0} { set excluded [lreplace $::selectedJob(excluded) $index $index]; set excludedFlag 1 }
  }
  $widget item configure $itemPath 0 -image $image

  # update data
  lset data 1 $state
  $widget entryconfigure $itemPath -data $data

  # store
  if {$includedFlag} \
  {
    set errorCode 0
    set errorText ""
    BackupServer:executeCommand errorCode errorText "INCLUDE_PATTERNS_CLEAR $::selectedJob(id)"
    foreach pattern $included \
    {
      BackupServer:executeCommand errorCode errorText "INCLUDE_PATTERNS_ADD $::selectedJob(id) GLOB [escapeString $pattern]"
    }
    if {$errorCode != 0} \
    {
      return
    }
    set ::selectedJob(included) [lsort -uniq $included]
  }
  if {$excludedFlag} \
  {
    set errorCode 0
    set errorText ""
    BackupServer:executeCommand errorCode errorText "EXCLUDE_PATTERNS_CLEAR $::selectedJob(id)"
    foreach pattern $excluded \
    {
      BackupServer:executeCommand errorCode errorText "EXCLUDE_PATTERNS_ADD $::selectedJob(id) GLOB [escapeString $pattern]"
    }
    if {$errorCode != 0} \
    {
      return
    }
    set ::selectedJob(excluded) [lsort -uniq $excluded]
  }
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
# Name   : clearBackupFilesTree
# Purpose: clear backup file tree
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc clearBackupFilesTree { } \
{
  global images

  foreach itemPath [$::filesTreeWidget info children ""] \
  {
    $::filesTreeWidget delete offsprings $itemPath
    $::filesTreeWidget item configure $itemPath 0 -image $images(folder)
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
  catch {$::filesTreeWidget delete entry $deviceName}

  set n 0
  set l [$::filesTreeWidget info children ""]
  while {($n < [llength $l]) && (([$::filesTreeWidget info data [lindex $l $n]] != {}) || ($deviceName>[lindex $l $n]))} \
  {
    incr n
  }

  set style [tixDisplayStyle imagetext -refwindow $::filesTreeWidget]

  $::filesTreeWidget add $deviceName -at $n -itemtype imagetext -text $deviceName -image [tix getimage folder] -style $style -data [list "DIRECTORY" "NONE" 0]
  $::filesTreeWidget item create $deviceName 1 -itemtype imagetext -style $style
  $::filesTreeWidget item create $deviceName 2 -itemtype imagetext -style $style
  $::filesTreeWidget item create $deviceName 3 -itemtype imagetext -style $style
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

proc addBackupEntry { fileName fileType fileSize fileDateTime } \
{
  global barConfig images

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
  set itemPath       [fileNameToItemPath $::filesTreeWidget "" $fileName       ]
  set parentItemPath [fileNameToItemPath $::filesTreeWidget "" $parentDirectory]
#puts "f=$fileName"
#puts "i=$itemPath"
#puts "p=$parentItemPath"

  catch {$::filesTreeWidget delete entry $itemPath}

  # create parent entry if it does not exists
  if {($parentItemPath != "") && ![$::filesTreeWidget info exists $parentItemPath]} \
  {
    addBackupEntry [file dirname $fileName] "DIRECTORY" 0 0
  }

  # get excluded flag of entry
  set includedFlag [checkIncluded $fileName]
  set excludedFlag [checkExcluded $fileName]

  # get styles
  set styleImage     [tixDisplayStyle imagetext -refwindow $::filesTreeWidget -anchor w]
  set styleTextLeft  [tixDisplayStyle text      -refwindow $::filesTreeWidget -anchor w]
  set styleTextRight [tixDisplayStyle text      -refwindow $::filesTreeWidget -anchor e]

   if     {$fileType=="FILE"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$::filesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$::filesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     if     {$includedFlag} { set image $images(fileIncluded) } \
     elseif {$excludedFlag} { set image $images(fileExcluded) } \
     else                   { set image $images(file)         }
     $::filesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "FILE" "NONE" 0]
     $::filesTreeWidget item create $itemPath 1 -itemtype text -style $styleTextLeft  -text "FILE"
     $::filesTreeWidget item create $itemPath 2 -itemtype text -style $styleTextRight -text $fileSize
     $::filesTreeWidget item create $itemPath 3 -itemtype text -style $styleTextLeft  -text [clock format $fileDateTime -format $::TIMEDATE_FORMAT]
   } \
   elseif {$fileType=="DIRECTORY"} \
   {
#puts "add directory $fileName"
     # find insert position (sort)
     set n 0
     set l [$::filesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && ([lindex [$::filesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") && ($itemPath > [lindex $l $n])} \
     {
       incr n
     }

     # add directory item
     if     {$includedFlag} { set image $images(folderIncluded) } \
     elseif {$excludedFlag} { set image $images(folderExcluded) } \
     else                   { set image $images(folder)         }
     $::filesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "DIRECTORY" "NONE" 0]
     $::filesTreeWidget item create $itemPath 1 -itemtype text -style $styleTextLeft
     $::filesTreeWidget item create $itemPath 2 -itemtype text -style $styleTextLeft
     $::filesTreeWidget item create $itemPath 3 -itemtype text -style $styleTextLeft
   } \
   elseif {$fileType=="LINK"} \
   {
#puts "add link $fileName"
     set n 0
     set l [$::filesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$::filesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add link item
     if     {$includedFlag} { set image $images(linkIncluded) } \
     elseif {$excludedFlag} { set image $images(linkExcluded) } \
     else                   { set image $images(link)         }
     $::filesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "LINK" "NONE" 0]
     $::filesTreeWidget item create $itemPath 1 -itemtype text -style $styleTextLeft -text "LINK"
     $::filesTreeWidget item create $itemPath 2 -itemtype text -style $styleTextLeft
     $::filesTreeWidget item create $itemPath 3 -itemtype text -style $styleTextLeft -text [clock format $fileDateTime -format $::TIMEDATE_FORMAT]
   } \
   elseif {$fileType=="DEVICE"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$::filesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$::filesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     if     {$includedFlag} { set image $images(fileIncluded) } \
     elseif {$excludedFlag} { set image $images(fileExcluded) } \
     else                   { set image $images(file)         }
     $::filesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "FILE" "NONE" 0]
     $::filesTreeWidget item create $itemPath 1 -itemtype text -text "DEVICE"  -style $styleTextLeft
     $::filesTreeWidget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $::filesTreeWidget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
   } \
   elseif {$fileType=="SOCKET"} \
   {
#puts "add file $fileName $itemPath - $parentItemPath -- [file tail $fileName]"
     # find insert position (sort)
     set n 0
     set l [$::filesTreeWidget info children $parentItemPath]
     while {($n < [llength $l]) && (([lindex [$::filesTreeWidget info data [lindex $l $n]] 0] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     # add file item
     if     {$includedFlag} { set image $images(fileIncluded) } \
     elseif {$excludedFlag} { set image $images(fileExcluded) } \
     else                   { set image $images(file)         }
     $::filesTreeWidget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $image -style $styleImage -data [list "FILE" "NONE" 0]
     $::filesTreeWidget item create $itemPath 1 -itemtype text -text "SOCKET"  -style $styleTextLeft
     $::filesTreeWidget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $::filesTreeWidget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
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
  global images

#puts $itemPath
  # get directory name
  set directoryName [itemPathToFileName $::filesTreeWidget $itemPath 0]

  # check if existing, add if not exists
  if {![$::filesTreeWidget info exists $itemPath]} \
  {
    addBackupEntry $directoryName "DIRECTORY" 0 0
  }
#puts [$::filesTreeWidget info exists $itemPath]
#puts [$::filesTreeWidget info data $itemPath]

  # check if parent exist and is open, open if needed
  set parentItemPath [$::filesTreeWidget info parent $itemPath]
#  set data [$::filesTreeWidget info data $parentItemPath]
#puts "$parentItemPath: $data"
  if {[$::filesTreeWidget info exists $parentItemPath]} \
  {
    set data [$::filesTreeWidget info data $parentItemPath]
    if {[lindex $data 2] == 0} \
    {
      openCloseBackupDirectory $parentItemPath
    }
    set data [$::filesTreeWidget info data $parentItemPath]
    if {[lindex $data 0] == "LINK"} { return }
  }

  # get data
  set data [$::filesTreeWidget info data $itemPath]
  set type              [lindex $data 0]
  set state             [lindex $data 1]
  set directoryOpenFlag [lindex $data 2]

  if {$type == "DIRECTORY"} \
  {
    # get open/closed flag

    $::filesTreeWidget delete offsprings $itemPath
    if {!$directoryOpenFlag} \
    {
      if     {$state == "INCLUDED"} { set image $images(folderIncludedOpen) } \
      elseif {$state == "EXCLUDED"} { set image $images(folderExcludedOpen) } \
      else                          { set image $images(folderOpen)         }
      $::filesTreeWidget item configure $itemPath 0 -image $image
      update

      set fileName [itemPathToFileName $::filesTreeWidget $itemPath 0]
      set commandId [BackupServer:sendCommand "FILE_LIST [escapeString $fileName] 0"]
      while {[BackupServer:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
      {
#puts "add file $result"
        if     {[scanx $result "FILE %ld %ld %S" fileSize fileDateTime fileName] == 3} \
        {
          addBackupEntry $fileName "FILE" $fileSize $fileDateTime
        } \
        elseif {[scanx $result "DIRECTORY %ld %ld %S" directorySize directoryDateTime directoryName] == 3} \
        {
          addBackupEntry $directoryName "DIRECTORY" $directorySize $directoryDateTime
        } \
        elseif {[scanx $result "LINK %ld %S" linkDateTime linkName] == 2} \
        {
          addBackupEntry $linkName "LINK" 0 $linkDateTime
        } \
        elseif {[scanx $result "SPECIAL %ld %S" specialDateTime specialName] == 2} \
        {
          addBackupEntry $specialName "SPECIAL" 0 $specialDateTime
        } \
        elseif {[scanx $result "DEVICE %S" deviceName] == 1} \
        {
          addBackupEntry $deviceName "DEVICE" 0 0
        } \
        elseif {[scanx $result "SOCKET %S" socketName] == 1} \
        {
          addBackupEntry $socketName "SOCKET" 0 0
        } else {
  internalError "unknown file type in openCloseBackupDirectory: $result"
}
      }

      set directoryOpenFlag 1
    } \
    else \
    {
      if     {$state == "INCLUDED"} { set image $images(folderIncluded) } \
      elseif {$state == "EXCLUDED"} { set image $images(folderExcluded) } \
      else                          { set image $images(folder)         }
      $::filesTreeWidget item configure $itemPath 0 -image $image

      set directoryOpenFlag 0
    }

    # update data
    lset data 2 $directoryOpenFlag
    $::filesTreeWidget entryconfigure $itemPath -data $data
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
  global images

  set itemPathList [$::filesTreeWidget info children ""]
  while {[llength $itemPathList] > 0} \
  {
    set fileName [lindex $itemPathList 0]; set itemPathList [lreplace $itemPathList 0 0]
    set itemPath [fileNameToItemPath $::filesTreeWidget "" $fileName]

    set data [$::filesTreeWidget info data $itemPath]
    set type              [lindex $data 0]
    set state             [lindex $data 1]
    set directoryOpenFlag [lindex $data 2]

    # add sub-directories to update
    if {($type == "DIRECTORY") && ($state != "EXCLUDED") && $directoryOpenFlag} \
    {
      foreach z [$::filesTreeWidget info children $itemPath] \
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
    elseif {$includedFlag} \
    {
      set state "INCLUDED"
    } \
    else \
    {
      set state "NONE"
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
          $::filesTreeWidget delete offsprings $itemPath
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
    $::filesTreeWidget item configure [fileNameToItemPath $::filesTreeWidget "" $fileName] 0 -image $image

    # update data
    lset data 1 $state
    $::filesTreeWidget entryconfigure $itemPath -data $data
  }
}

if {0} {
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
  global restoreFilesTreeWidget images

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
# Name   : closeRestoreDirectory
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
      set errorText ""
      BackupServer:executeCommand errorCode errorText "CLEAR"
#      BackupServer:executeCommand errorCode errorText "ADD_INCLUDE_PATTERN REGEX ^$directoryName/\[^/\]+"
      set commandId [BackupServer:sendCommand "ARCHIVE_LIST [escapeString$archiveName]"]
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
return
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
      "FxILESYSTEM" \
      {
        set fileName $barConfig(storageFileName)
        set itemPath $barConfig(storageFileName)
        $restoreFilesTreeWidget add $itemPath -itemtype imagetext -text $fileName -image $images(folder) -style $styleImage -data [list "DIRECTORY" "NONE" 0]
        $restoreFilesTreeWidget item create $itemPath 1 -itemtype text -style $styleTextLeft
        $restoreFilesTreeWidget item create $itemPath 2 -itemtype text -style $styleTextLeft
        $restoreFilesTreeWidget item create $itemPath 3 -itemtype text -style $styleTextLeft
      }
      "SxCP" \
      {
      }
      "SxFTP" \
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
  global images

  set itemPathList [$::filesTreeWidget info children ""]
  while {[llength $itemPathList] > 0} \
  {
    set fileName [lindex $itemPathList 0]; set itemPathList [lreplace $itemPathList 0 0]
    set itemPath [fileNameToItemPath $::filesTreeWidget "" $fileName]

    set data [$::filesTreeWidget info data $itemPath]
    set type              [lindex $data 0]
    set state             [lindex $data 1]
    set directoryOpenFlag [lindex $data 2]

    # add sub-directories to update
    if {($type == "DIRECTORY") && ($state != "EXCLUDED") && $directoryOpenFlag} \
    {
      foreach z [$::filesTreeWidget info children $itemPath] \
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
          $::filesTreeWidget delete offsprings $itemPath
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
    $::filesTreeWidget item configure [fileNameToItemPath $::filesTreeWidget "" $fileName] 0 -image $image

    # update data
    lset data 1 $state
    $::filesTreeWidget entryconfigure $itemPath -data $data
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
  global images

  set itemPathList [$::filesTreeWidget info children ""]
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
    $::filesTreeWidget item configure $itemPath 0 -image $image

    # update data
    lset data 1 $state
    $::filesTreeWidget entryconfigure $itemPath -data $data
  }
}
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name   : updateStatusList
# Purpose: update status list
# Input  : statusListWidget - status list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc updateStatusList { statusListWidget } \
{
  global jobListTimerId status jobStatus barControlConfig

  proc private_compareJobs { job0 job1 } \
  {
    set status0 [lindex $job0 2]
    set status1 [lindex $job1 2]
    if {($status0 == "running") && ($status1 != "running")} { return -1 }
    if {($status1 == "running") && ($status0 != "running")} { return  1 }
    if {($status0 == "waiting") && ($status1 != "waiting")} { return -1 }
    if {($status1 == "waiting") && ($status0 != "waiting")} { return  1 }
    return 0
  }

  catch {after cancel $jobListTimerId}

  # get current selection
  set selectedId 0
  if {[$statusListWidget curselection] != {}} \
  {
    set n [lindex [$statusListWidget curselection] 0]
    set selectedId [lindex [lindex [$statusListWidget get $n $n] 0] 0]
  }
  set yview [lindex [$statusListWidget yview] 0]

  # get list
  set jobList {}
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
      lastExecutedDateTime \
      estimatedRestTime
#puts "1: ok"

    set estimatedRestDays    [expr {int($estimatedRestTime/(24*60*60)        )}]
    set estimatedRestHours   [expr {int($estimatedRestTime%(24*60*60)/(60*60))}]
    set estimatedRestMinutes [expr {int($estimatedRestTime%(60*60   )/60     )}]
    set estimatedRestSeconds [expr {int($estimatedRestTime%(60)              )}]

    lappend jobList \
      [list \
        $id \
        $name \
        [expr {($status eq "pause")?"pause":$state}] \
        $type \
        [expr {($archivePartSize > 0)?[formatByteSize $archivePartSize]:"-"}] \
        $compressAlgorithm \
        $cryptAlgorithm \
        [expr {($lastExecutedDateTime > 0)?[clock format $lastExecutedDateTime -format "%Y-%m-%d %H:%M:%S"]:"-"}] \
        [format "%2d days %02d:%02d:%02d" $estimatedRestDays $estimatedRestHours $estimatedRestMinutes $estimatedRestSeconds] \
      ]
  }
  set jobList [lsort -command private_compareJobs $jobList]

  # update list
  $statusListWidget delete 0 end
  foreach job $jobList \
  {
    $statusListWidget insert end $job
  }

  # set selection
#puts "end 3"
  if {[llength $jobList] > 0} \
  {
    if {$selectedId == 0} \
    {
      foreach entry [$statusListWidget get 0 end] \
      {
        if {[lindex $entry 2] == "running"} \
        {
          set selectedId      [lindex $entry 0]
          set jobStatus(id)   [lindex $entry 0]
          set jobStatus(name) [lindex $entry 1]
          break;
        }
      }
    }
    if {$selectedId == 0} \
    {
      set entry [lindex [$statusListWidget get 0 0] 0]
      set selectedId      [lindex $entry 0]
      set jobStatus(id)   [lindex $entry 0]
      set jobStatus(name) [lindex $entry 1]
    }
  } \
  else \
  {
    set selectedId      0
    set jobStatus(id)   0
    set jobStatus(name) ""
  }

  # restore selection
  if     {$selectedId > 0} \
  {
    set n 0
    while {($n < [$statusListWidget index end]) && ($selectedId != [lindex [lindex [$statusListWidget get $n $n] 0] 0])} \
    {
      incr n
    }
    if {$n < [$statusListWidget index end]} \
    {
      $statusListWidget selection set $n
      $statusListWidget yview moveto $yview
    }
  }

  set jobListTimerId [after $barControlConfig(statusListUpdateTime) "updateStatusList $statusListWidget"]
}

#***********************************************************************
# Name   : updateStatus
# Purpose: update status data
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc updateStatus { } \
{
  global jobStatus statusTimerId barControlConfig

  catch {after cancel $statusTimerId}

  # get status
  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "STATUS" ::status

  # get job status
  if {$jobStatus(id) != 0} \
  {
#puts "start 2"
    set commandId [BackupServer:sendCommand "JOB_INFO $jobStatus(id)"]
    if {[BackupServer:readResult $commandId completeFlag errorCode result] && ($errorCode == 0)} \
    {
#puts "2: $result"
      scanx $result "%S %S %lu %lu %lu %lu %lu %lu %lu %lu %f %f %f %lu %f %S %lu %lu %S %lu %lu %d %f %d" \
        jobStatus(state) \
        jobStatus(error) \
        jobStatus(doneFiles) \
        jobStatus(doneBytes) \
        jobStatus(totalFiles) \
        jobStatus(totalBytes) \
        jobStatus(skippedFiles) \
        jobStatus(skippedBytes) \
        jobStatus(errorFiles) \
        jobStatus(errorBytes) \
        filesPerSecond \
        jobStatus(bytesPerSecond) \
        jobStatus(storageBytesPerSecond) \
        jobStatus(archiveBytes) \
        ratio \
        jobStatus(fileName) \
        jobStatus(fileDoneBytes) \
        jobStatus(fileTotalBytes) \
        jobStatus(storageName) \
        jobStatus(storageDoneBytes) \
        jobStatus(storageTotalBytes) \
        jobStatus(volumeNumber) \
        jobStatus(volumeProgress) \
        jobStatus(requestedVolumeNumber)
#puts "2: ok"
      if {$jobStatus(error) != ""} { set jobStatus(message) $jobStatus(error) }

      if     {$jobStatus(doneBytes)            > 1024*1024*1024} { set jobStatus(doneBytesShort)             [format "%.1f" [expr {double($jobStatus(doneBytes)            )/(1024*1024*1024)}]]; set jobStatus(doneBytesShortUnit)             "GBytes"   } \
      elseif {$jobStatus(doneBytes)            >      1024*1024} { set jobStatus(doneBytesShort)             [format "%.1f" [expr {double($jobStatus(doneBytes)            )/(     1024*1024)}]]; set jobStatus(doneBytesShortUnit)             "MBytes"   } \
      else                                                       { set jobStatus(doneBytesShort)             [format "%.1f" [expr {double($jobStatus(doneBytes)            )/(          1024)}]]; set jobStatus(doneBytesShortUnit)             "KBytes"   }
      if     {$jobStatus(totalBytes)           > 1024*1024*1024} { set jobStatus(totalBytesShort)            [format "%.1f" [expr {double($jobStatus(totalBytes)           )/(1024*1024*1024)}]]; set jobStatus(totalBytesShortUnit)            "GBytes"   } \
      elseif {$jobStatus(totalBytes)           >      1024*1024} { set jobStatus(totalBytesShort)            [format "%.1f" [expr {double($jobStatus(totalBytes)           )/(     1024*1024)}]]; set jobStatus(totalBytesShortUnit)            "MBytes"   } \
      else                                                       { set jobStatus(totalBytesShort)            [format "%.1f" [expr {double($jobStatus(totalBytes)           )/(          1024)}]]; set jobStatus(totalBytesShortUnit)            "KBytes"   }
      if     {$jobStatus(skippedBytes)         > 1024*1024*1024} { set jobStatus(skippedBytesShort)          [format "%.1f" [expr {double($jobStatus(skippedBytes)         )/(1024*1024*1024)}]]; set jobStatus(skippedBytesShortUnit)          "GBytes"   } \
      elseif {$jobStatus(skippedBytes)         >      1024*1024} { set jobStatus(skippedBytesShort)          [format "%.1f" [expr {double($jobStatus(skippedBytes)         )/(     1024*1024)}]]; set jobStatus(skippedBytesShortUnit)          "MBytes"   } \
      else                                                       { set jobStatus(skippedBytesShort)          [format "%.1f" [expr {double($jobStatus(skippedBytes)         )/(          1024)}]]; set jobStatus(skippedBytesShortUnit)          "KBytes"   }
      if     {$jobStatus(errorBytes)           > 1024*1024*1024} { set jobStatus(errorBytesShort)            [format "%.1f" [expr {double($jobStatus(errorBytes)           )/(1024*1024*1024)}]]; set jobStatus(errorBytesShortUnit)            "GBytes"   } \
      elseif {$jobStatus(errorBytes)           >      1024*1024} { set jobStatus(errorBytesShort)            [format "%.1f" [expr {double($jobStatus(errorBytes)           )/(     1024*1024)}]]; set jobStatus(errorBytesShortUnit)            "MBytes"   } \
      else                                                       { set jobStatus(errorBytesShort)            [format "%.1f" [expr {double($jobStatus(errorBytes)           )/(          1024)}]]; set jobStatus(errorBytesShortUnit)            "KBytes"   }
      if     {$jobStatus(bytesPerSecond)       > 1024*1024*1024} { set jobStatus(bytesPerSecondShort)        [format "%.1f" [expr {double($jobStatus(bytesPerSecond)       )/(1024*1024*1024)}]]; set jobStatus(bytesPerSecondUnit)             "GBytes/s" } \
      elseif {$jobStatus(bytesPerSecond)       >      1024*1024} { set jobStatus(bytesPerSecondShort)        [format "%.1f" [expr {double($jobStatus(bytesPerSecond)       )/(     1024*1024)}]]; set jobStatus(bytesPerSecondShortUnit)        "MBytes/s" } \
      else                                                       { set jobStatus(bytesPerSecondShort)        [format "%.1f" [expr {double($jobStatus(bytesPerSecond)       )/(          1024)}]]; set jobStatus(bytesPerSecondShortUnit)        "KBytes/s" }
      if     {$jobStatus(storageBytesPerSecond)> 1024*1024*1024} { set jobStatus(storageBytesPerSecondShort) [format "%.1f" [expr {double($jobStatus(storageBytesPerSecond))/(1024*1024*1024)}]]; set jobStatus(storageBytesPerSecondShortUnit) "GBytes/s" } \
      elseif {$jobStatus(storageBytesPerSecond)>      1024*1024} { set jobStatus(storageBytesPerSecondShort) [format "%.1f" [expr {double($jobStatus(storageBytesPerSecond))/(     1024*1024)}]]; set jobStatus(storageBytesPerSecondShortUnit) "MBytes/s" } \
      else                                                       { set jobStatus(storageBytesPerSecondShort) [format "%.1f" [expr {double($jobStatus(storageBytesPerSecond))/(          1024)}]]; set jobStatus(storageBytesPerSecondShortUnit) "KBytes/s" }
      if     {$jobStatus(archiveBytes)         > 1024*1024*1024} { set jobStatus(archiveBytesShort)          [format "%.1f" [expr {double($jobStatus(archiveBytes)         )/(1024*1024*1024)}]]; set jobStatus(archiveBytesShortUnit)          "GBytes"   } \
      elseif {$jobStatus(archiveBytes)         >      1024*1024} { set jobStatus(archiveBytesShort)          [format "%.1f" [expr {double($jobStatus(archiveBytes)         )/(     1024*1024)}]]; set jobStatus(archiveBytesShortUnit)          "MBytes"   } \
      else                                                       { set jobStatus(archiveBytesShort)          [format "%.1f" [expr {double($jobStatus(archiveBytes)         )/(          1024)}]]; set jobStatus(archiveBytesShortUnit)          "KBytes"   }
      if     {$jobStatus(storageTotalBytes)    > 1024*1024*1024} { set jobStatus(storageTotalBytesShort)     [format "%.1f" [expr {double($jobStatus(storageTotalBytes)    )/(1024*1024*1024)}]]; set jobStatus(storageTotalBytesShortUnit)     "GBytes"   } \
      elseif {$jobStatus(storageTotalBytes)    >      1024*1024} { set jobStatus(storageTotalBytesShort)     [format "%.1f" [expr {double($jobStatus(storageTotalBytes)    )/(     1024*1024)}]]; set jobStatus(storageTotalBytesShortUnit)     "MBytes"   } \
      else                                                       { set jobStatus(storageTotalBytesShort)     [format "%.1f" [expr {double($jobStatus(storageTotalBytes)    )/(          1024)}]]; set jobStatus(storageTotalBytesShortUnit)     "KBytes"   }

      set jobStatus(filesPerSecond)   [format "%.1f" $filesPerSecond]
      set jobStatus(compressionRatio) [format "%.1f" $ratio]
    }
#puts "end 2"
  } \
  else \
  {
    set jobStatus(doneFiles)                      0
    set jobStatus(doneBytes)                      0
    set jobStatus(doneBytesShort)                 0
    set jobStatus(doneBytesShortUnit)             "KBytes"
    set jobStatus(totalFiles)                     0
    set jobStatus(totalBytes)                     0
    set jobStatus(totalBytesShort)                0
    set jobStatus(totalBytesShortUnit)            "KBytes"
    set jobStatus(skippedFiles)                   0
    set jobStatus(skippedBytes)                   0
    set jobStatus(skippedBytesShort)              0
    set jobStatus(skippedBytesShortUnit)          "KBytes"
    set jobStatus(errorFiles)                     0
    set jobStatus(errorBytes)                     0
    set jobStatus(errorBytesShort)                0
    set jobStatus(errorBytesShortUnit)            "KBytes"
    set jobStatus(filesPerSecond)                 0
    set jobStatus(filesPerSecondUnit)             "files/s"
    set jobStatus(bytesPerSecond)                 0
    set jobStatus(bytesPerSecondShort)            0
    set jobStatus(bytesPerSecondShortUnit)        "KBytes/s"
    set jobStatus(storageBytesPerSecond)          0
    set jobStatus(storageBytesPerSecondShort)     0
    set jobStatus(storageBytesPerSecondShortUnit) "KBytes/s"
    set jobStatus(compressionRatio)               0
    set jobStatus(fileName)                       ""
    set jobStatus(fileDoneBytes)                  0
    set jobStatus(fileTotalBytes)                 0
    set jobStatus(storageName)                    ""
    set jobStatus(storageName)                    ""
    set jobStatus(storageDoneBytes)               0
    set jobStatus(storageTotalBytes)              0
    set jobStatus(storageTotalBytesShort)         0
    set jobStatus(storageTotalBytesUnit)          "KBytes"
    set jobStatus(volumeNumber)                   0
    set jobStatus(requestedVolumeNumber)          0
  }

  set statusTimerId [after $barControlConfig(statusUpdateTime) "updateStatus"]
}

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
  global barControlConfig

  # get selection
  set selectedJobId $::selectedJob(id)
  if {$selectedJobId == ""} { set selectedJobId 0 }

  # get jobs
#puts "start 3"
  set jobs {}
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
      lastExecutedDateTime \
      estimatedRestTime
#puts "1: ok"

    lappend jobs [list $id $name]
  }
  set jobs [lsort -index 1 $jobs]
#puts "end 3: $jobs"

  # update list
  foreach name [$jobListWidget entries] \
  {
    $jobListWidget delete $name
  }
  $jobListWidget add command 0 -label ""
  foreach job $jobs \
  {
    $jobListWidget add command [lindex $job 0] -label [lindex $job 1]
  }

  # restore selection
  set ::selectedJob(id) $selectedJobId
}

#***********************************************************************
# Name   : clearJob
# Purpose: clear selected job
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc clearJob {} \
{
  set ::selectedJob(id)                        0
  set ::selectedJob(storageType)               ""
  set ::selectedJob(storageFileName)           ""
  set ::selectedJob(storageLoginName)          ""
  set ::selectedJob(storageHostName)           ""
  set ::selectedJob(storageDeviceName)         ""
  set ::selectedJob(archivePartSizeFlag)       0
  set ::selectedJob(archivePartSize)           0
  #set ::selectedJob(maxTmpSizeFlag)            0
  #set ::selectedJob(maxTmpSize)                0
  set ::selectedJob(archiveType)               "normal"
  set ::selectedJob(incrementalListFileName)   ""
  set ::selectedJob(maxBandWidthFlag)          0
  set ::selectedJob(maxBandWidth)              0
  set ::selectedJob(sshPort)                   0
  set ::selectedJob(sshPublicKeyFileName)      ""
  set ::selectedJob(sshPrivateKeyFileName)     ""
  set ::selectedJob(compressAlgorithm)         ""
  set ::selectedJob(cryptAlgorithm)            "none"
  set ::selectedJob(cryptPasswordMode)         "default"
  set ::selectedJob(cryptPassword)             ""
  set ::selectedJob(cryptPasswordVerify)       ""
  set ::selectedJob(destinationDirectoryName)  ""
  set ::selectedJob(destinationStripCount)     0
  set ::selectedJob(volumeSize)                0
  set ::selectedJob(skipUnreadableFlag)        1
  set ::selectedJob(skipNotWritableFlag)       1
  set ::selectedJob(overwriteArchiveFilesFlag) 0
  set ::selectedJob(overwriteFilesFlag)        0
  set ::selectedJob(errorCorrectionCodesFlag)  1
  set ::selectedJob(included)                  {}
  set ::selectedJob(excluded)                  {}
  set ::selectedJob(schedule)                  {}

  clearBackupFilesTree 
  $::includedListWidget delete 0 end
  $::excludedListWidget delete 0 end
}

#***********************************************************************
# Name   : selectJob
# Purpose: select job and get settings
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc selectJob { id } \
{
#if {$::selectedJob(id)==0} { return }
#puts "selectJob $id $::selectedJob(id)"
  # clear
  clearJob

  # disabled buttons
  .jobs.list.data configure -state disabled
  .jobs.list.new configure -state disabled
  .jobs.list.delete configure -state disabled

  # settings
  BackupServer:get $id "archive-type" ::selectedJob(archiveType)
  BackupServer:get $id "archive-name" "" \
  {
    scanx $result "%S" s

    if     {[regexp {^ftp:([^@]*)@([^:]*):(.*)} $s * loginName hostName fileName]} \
    {
      set ::selectedJob(storageType)      "ftp"
      set ::selectedJob(storageLoginName) $loginName
      set ::selectedJob(storageHostName)  $hostName
      set ::selectedJob(storageFileName)  $fileName
    } \
    elseif {[regexp {^scp:([^@]*)@([^:]*):(.*)} $s * loginName hostName fileName]} \
    {
      set ::selectedJob(storageType)      "scp"
      set ::selectedJob(storageLoginName) $loginName
      set ::selectedJob(storageHostName)  $hostName
      set ::selectedJob(storageFileName)  $fileName
    } \
    elseif {[regexp {^sftp:([^@]*)@([^:]*):(.*)} $s * loginName hostName fileName]} \
    {
      set ::selectedJob(storageType)      "sftp"
      set ::selectedJob(storageLoginName) $loginName
      set ::selectedJob(storageHostName)  $hostName
      set ::selectedJob(storageFileName)  $fileName
    } \
    elseif {[regexp {^dvd:([^:]*):(.*)} $s * deviceName fileName]} \
    {
      set ::selectedJob(storageType)       "dvd"
      set ::selectedJob(storageDeviceName) $deviceName
      set ::selectedJob(storageFileName)   $fileName
    } \
    elseif {[regexp {^dvd:(.*)} $s * fileName]} \
    {
      set ::selectedJob(storageType)       "dvd"
      set ::selectedJob(storageDeviceName) ""
      set ::selectedJob(storageFileName)   $fileName
    } \
    elseif {[regexp {^([^:]*):(.*)} $s * deviceName fileName]} \
    {
      set ::selectedJob(storageType)       "device"
      set ::selectedJob(storageDeviceName) $deviceName
      set ::selectedJob(storageFileName)   $fileName
    } \
    else \
    {
      set ::selectedJob(storageType)      "filesystem"
      set ::selectedJob(storageLoginName) ""
      set ::selectedJob(storageHostName)  ""
      set ::selectedJob(storageFileName)  $s
    }
  }
  BackupServer:get $id "archive-part-size" ::selectedJob(archivePartSize) \
  {
    set ::selectedJob(archivePartSizeFlag) [expr {$::selectedJob(archivePartSize)>0}]
  }
  BackupServer:get $id "incremental-list-file" "" \
  {
    scanx $result "%S" ::selectedJob(incrementalListFileName)
  }
  BackupServer:get $id "compress-algorithm" ::selectedJob(compressAlgorithm)
  BackupServer:get $id "crypt-algorithm" ::selectedJob(cryptAlgorithm)
  BackupServer:get $id "crypt-password-mode" ::selectedJob(cryptPasswordMode)
  set ::selectedJob(cryptPassword)       ""
  set ::selectedJob(cryptPasswordVerify) ""
  BackupServer:get $id "ssh-port" ::selectedJob(sshPort)
  BackupServer:get $id "ssh-public-key" "" \
  {
    scanx $result "%S" ::selectedJob(sshPublicKeyFileName)
  }
  BackupServer:get $id "ssh-private-key" "" \
  {
    scanx $result "%S" ::selectedJob(sshPrivateKeyFileName)
  }
  BackupServer:get $id "volume-size" ::selectedJob(volumeSize)
  BackupServer:get $id "skip-unreadable" ::selectedJob(skipUnreadableFlag)
  BackupServer:get $id "overwrite-archive-files" ::selectedJob(overwriteArchiveFilesFlag)
  BackupServer:get $id "overwrite-files" ::selectedJob(overwriteFilesFlag)
  BackupServer:get $id "ecc" ::selectedJob(errorCorrectionCodesFlag)

  # include list
  set ::selectedJob(included) {}
  set commandId [BackupServer:sendCommand "INCLUDE_PATTERNS_LIST $id"]
  while {[BackupServer:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
  {
    # parse
    scanx $result "%s %S" type pattern

    # add to exclude pattern list
    lappend ::selectedJob(included) $pattern
  }
  set ::selectedJob(included) [lsort -uniq $::selectedJob(included)]

  # exclude list
  set ::selectedJob(excluded) {}
  set commandId [BackupServer:sendCommand "EXCLUDE_PATTERNS_LIST $id"]
  while {[BackupServer:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
  {
    # parse
    scanx $result "%s %S" type pattern

    # add to exclude pattern list
    lappend ::selectedJob(excluded) $pattern
  }
  set ::selectedJob(excluded) [lsort -uniq $::selectedJob(excluded)]

  # schedule list
  set ::selectedJob(schedule) {}
  set commandId [BackupServer:sendCommand "SCHEDULE_LIST $id"]
  while {[BackupServer:readResult $commandId completeFlag errorCode result] && !$completeFlag} \
  {
    # parse
    scanx $result "%S %S %S %S" date weekDay time type

    # add to schedule list
    lappend ::selectedJob(schedule) [list $date $weekDay $time $type]
  }
  set ::selectedJob(schedule) [lsort -index 0 $::selectedJob(schedule)]

  # open included/excluded directories
  foreach pattern $::selectedJob(included) \
  {
    # get item path
    set itemPath [fileNameToItemPath $::filesTreeWidget "" $pattern]

    # add directory for entry
    set directoryName [file dirname $pattern]
    set directoryItemPath [fileNameToItemPath $::filesTreeWidget "" $directoryName]
    if {![$::filesTreeWidget info exists $directoryItemPath]} \
    {
      catch {openCloseBackupDirectory $directoryItemPath}
    }
  }
  foreach pattern $::selectedJob(excluded) \
  {
    # get item path
    set itemPath [fileNameToItemPath $::filesTreeWidget "" $pattern]

    # add directory for entry
    set directoryName [file dirname $pattern]
    set directoryItemPath [fileNameToItemPath $::filesTreeWidget "" $directoryName]
    if {![$::filesTreeWidget info exists $directoryItemPath]} \
    {
      catch {openCloseBackupDirectory $directoryItemPath}
    }
  }

  # enable buttons
  .jobs.list.data configure -state normal
  .jobs.list.new configure -state normal

  # store id
  set ::selectedJob(id) $id
}

# ----------------------------------------------------------------------

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
      set barControlConfig(statusListUpdateTime) $s
    } \
    elseif {[scanx $line "current-job-update-time = %d" s] == 1} \
    {
      set barControlConfig(statusUpdateTime) $s
    } \
    else \
    {
      printWarning "Unknown line '$line' in config file '$configFileName', line $lineNb - skipped"
    }
  }

  # close file
  close $handle
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
#      saveBARConfig $barConfigFileName
puts "NYI"; exit 1;
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
  global barConfig

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
  set included [lsort -uniq [concat $::selectedJob(included) [list $pattern]]]

  # store
  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "INCLUDE_PATTERNS_CLEAR $::selectedJob(id)"
  foreach pattern $included \
  {
    BackupServer:executeCommand errorCode errorText "INCLUDE_PATTERNS_ADD $::selectedJob(id) GLOB [escapeString $pattern]"
  }
  if {$errorCode != 0} \
  {
    return
  }
  set ::selectedJob(included) $included

  # update view 
  backupUpdateFileTreeStates 
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

  set index [lsearch -sorted -exact $::selectedJob(included) $pattern]
  if {$index >= 0} \
  {
    # remove
    set included [lreplace $::selectedJob(included) $index $index]
    set errorCode 0
    set errorText ""
    BackupServer:executeCommand errorCode errorText "INCLUDE_PATTERNS_CLEAR $::selectedJob(id)"
    foreach pattern $included \
    {
      BackupServer:executeCommand errorCode errorText "INCLUDE_PATTERNS_ADD $::selectedJob(id) GLOB [escapeString $pattern]"
    }
    if {$errorCode != 0} \
    {
      return
    }
    set ::selectedJob(included) $included

    # update view 
    backupUpdateFileTreeStates 
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
  global barConfig restoreFilesTreeWidget

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
  set excluded [lsort -uniq [concat $::selectedJob(excluded) [list $pattern]]]

  # store
  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "EXCLUDE_PATTERNS_CLEAR $::selectedJob(id)"
  foreach pattern $excluded \
  {
    BackupServer:executeCommand errorCode errorText "EXCLUDE_PATTERNS_ADD $::selectedJob(id) GLOB [escapeString $pattern]"
  }
  if {$errorCode != 0} \
  {
    return
  }
  set ::selectedJob(excluded) $excluded

  # update view 
  backupUpdateFileTreeStates 
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
  global barConfig restoreFilesTreeWidget

  set index [lsearch -sorted -exact $::selectedJob(excluded) $pattern]
  if {$index >= 0} \
  {
    # remove
    set excluded [lreplace $::selectedJob(excluded) $index $index]
    set errorCode 0
    set errorText ""
    BackupServer:executeCommand errorCode errorText "EXCLUDE_PATTERNS_CLEAR $::selectedJob(id)"
    foreach pattern $excluded \
    {
      BackupServer:executeCommand errorCode errorText "EXCLUDE_PATTERNS_ADD $::selectedJob(id) GLOB [escapeString $pattern]"
    }
    if {$errorCode != 0} \
    {
      return
    }
    set ::selectedJob(excluded) $excluded

    # update view 
    backupUpdateFileTreeStates
  }
}

#***********************************************************************
# Name   : jobNewDialog
# Purpose: new job dialog
# Input  : -
# Output : -
# Return : name or "" on cancel
# Notes  : -
#***********************************************************************

proc jobNewDialog {} \
{
  # dialog
  set handle [Dialog:window "Add schedule"]
  Dialog:addVariable $handle result      -1
  Dialog:addVariable $handle name        ""

  frame $handle.name
    label $handle.name.title -text "Name:"
    grid $handle.name.title -row 0 -column 0 -sticky "w"
    entry $handle.name.data -textvariable [Dialog:variable $handle name] -bg white
    grid $handle.name.data -row 0 -column 1 -sticky "we"
    bind $handle.name.data <Return> "focus $handle.buttons.add"

    grid rowconfigure    $handle.name { 0 } -weight 1
    grid columnconfigure $handle.name { 1 } -weight 1
  grid $handle.name -row 0 -column 0 -sticky "nswe" -padx 3p -pady 3p

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

  focus $handle.name.data

  Dialog:show $handle
  set result [Dialog:get $handle result     ]
  set name   [Dialog:get $handle name       ]
  Dialog:delete $handle
  if {($result != 1)} { return "" }

  return $name
}

#***********************************************************************
# Name   : jobNew
# Purpose: create new job
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc jobNew { } \
{
  global barConfig

  # new
  set name [jobNewDialog]
  if {$name == ""} \
  {
    return
  }

  # add job
  set errorCode 0
  set errorText ""
  set result    ""
  BackupServer:executeCommand errorCode errorText "JOB_NEW $name" result
  if {$errorCode != 0} \
  {
    return
  }
  scanx $result "%d" id

  return $id
}

#***********************************************************************
# Name   : jobDelete
# Purpose: delete job
# Input  : id - job id
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc jobDelete { id } \
{
  global barConfig

  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "JOB_DELETE $id"
  if {$errorCode != 0} \
  {
    Dialog:error "Cannot delete job!\n\nError: $errorText"
    return
  }
}

#***********************************************************************
# Name   : jobStart
# Purpose: run job
# Input  : id   - job id
#          type - FULL|INCREMENTAL
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc jobStart { id type } \
{
  global guiMode

  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "JOB_START $id $type"
  if {$errorCode != 0} \
  {
    Dialog:error "Cannot start job!\n\nError: $errorText"
    return
  }
}

#***********************************************************************
# Name   : jobAbort
# Purpose: abort running job
# Input  : id - job id
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc jobAbort { id } \
{
  global guiMode

  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "JOB_ABORT $id"
  if {$errorCode != 0} \
  {
    Dialog:error "Cannot abort job!\n\nError: $errorText"
    return
  }
}

#***********************************************************************
# Name   : jobPause
# Purpose: pause running job
# Input  : id - job id
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc jobTogglePause { } \
{
  global guiMode

  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "JOB_PAUSE $id"
  if {$errorCode != 0} \
  {
    Dialog:error "Cannot pause/continue job!\n\nError: $errorText"
    return
  }
}

#***********************************************************************
# Name   : editScheduleDialog
# Purpose: edit schedule
# Input  : titleText - title text
#          okText    - ok-button text
#          _schedule - schedule to edit
# Output : _schedule - schedule
# Return : 1 if schedule edited, 0 otherwise
# Notes  : -
#***********************************************************************

proc editScheduleDialog { titleText okText _schedule } \
{
  global barConfig

  upvar $_schedule schedule

  # format number or *
  proc numberFormat {f s} \
  {
    if {[string is integer $s]} \
    {
      return [format $f $s]
    } \
    else \
    {
      return $s
    }
  }

  # dialog
  set handle [Dialog:window $titleText]
  Dialog:addVariable $handle result      -1
  Dialog:addVariable $handle year        "*"
  Dialog:addVariable $handle month       "*"
  Dialog:addVariable $handle day         "*"
  Dialog:addVariable $handle weekDay     "*"
  Dialog:addVariable $handle hour        "*"
  Dialog:addVariable $handle minute      "*"
  Dialog:addVariable $handle archiveType "*"

  frame $handle.schedule
    label $handle.schedule.date -text "Date:"
    grid $handle.schedule.date -row 0 -column 0 -sticky "w"
    tixOptionMenu $handle.schedule.year -label "" -labelside right -variable [Dialog:variable $handle year] -dynamicgeometry true  -options { entry.background white }
    $handle.schedule.year add command "*"
    for {set z [clock format [clock seconds] -format "%G"]} {$z < [expr {[clock format [clock seconds] -format "%G"]+8}]} {incr z} \
    {
      $handle.schedule.year add command $z
    }
    grid $handle.schedule.year -row 0 -column 1 -sticky "we"
    bind [$handle.schedule.year subwidget menubutton] <Return> "focus \[$handle.schedule.month subwidget menubutton\]"
    tixOptionMenu $handle.schedule.month -label "" -labelside right -variable [Dialog:variable $handle month] -dynamicgeometry true  -options { entry.background white }
    foreach z {"*" "Jan" "Feb" "Mar" "Apr" "May" "Jun" "Jul" "Aug" "Sep" "Oct" "Nov" "Dec"} \
    {
      $handle.schedule.month add command $z -label $z
    }
    grid $handle.schedule.month -row 0 -column 2 -sticky "we"
    bind [$handle.schedule.month subwidget menubutton] <Return> "focus \[$handle.schedule.day subwidget menubutton\]"
    tixOptionMenu $handle.schedule.day -label "" -labelside right -variable [Dialog:variable $handle day] -dynamicgeometry true  -options { entry.background white }
    $handle.schedule.day add command "*"
    for {set z 1} {$z <= 31} {incr z} \
    {
      $handle.schedule.day add command $z
    }
    grid $handle.schedule.day -row 0 -column 3 -sticky "we"
    bind [$handle.schedule.day subwidget menubutton] <Return> "focus \[$handle.schedule.weekday subwidget menubutton\]"
    tixOptionMenu $handle.schedule.weekday -label "" -labelside right -variable [Dialog:variable $handle weekDay] -dynamicgeometry true  -options { entry.background white }
    foreach z {"*" "Mon" "Tue" "Wed" "Thu" "Fri" "Sat" "Sun"} \
    {
      $handle.schedule.weekday add command $z
    }
    grid $handle.schedule.weekday -row 0 -column 4 -sticky "we"
    bind [$handle.schedule.weekday subwidget menubutton] <Return> "focus \[$handle.schedule.hour subwidget menubutton\]"

    label $handle.schedule.time -text "Time:"
    grid $handle.schedule.time -row 1 -column 0 -sticky "w"
    tixOptionMenu $handle.schedule.hour -label "" -labelside right -variable [Dialog:variable $handle hour] -dynamicgeometry true  -options { entry.background white }
    $handle.schedule.hour add command "*"
    for {set z 0} {$z <= 23} {incr z} \
    {
      $handle.schedule.hour add command $z -label $z
    }
    grid $handle.schedule.hour -row 1 -column 1 -sticky "we"
    bind [$handle.schedule.hour subwidget menubutton] <Return> "focus \[$handle.schedule.minute subwidget menubutton\]"
    tixOptionMenu $handle.schedule.minute -label "" -labelside right -variable [Dialog:variable $handle minute] -dynamicgeometry true  -options { entry.background white }
    $handle.schedule.minute add command "*"
    for {set z 0} {$z <= 59} {incr z 5} \
    {
      $handle.schedule.minute add command $z
    }
    grid $handle.schedule.minute -row 1 -column 2 -sticky "we"
    bind [$handle.schedule.minute subwidget menubutton] <Return> "focus $handle.schedule.archivetype.any"

    label $handle.schedule.archivetypetitle -text "Type:"
    grid $handle.schedule.archivetypetitle -row 2 -column 0 -sticky "w"
    frame $handle.schedule.archivetype
      radiobutton $handle.schedule.archivetype.any -text "*" -anchor w -variable [Dialog:variable $handle archiveType] -value "*"
      pack $handle.schedule.archivetype.any -side left
      bind $handle.schedule.archivetype.any <Return> "focus $handle.buttons.add"
      radiobutton $handle.schedule.archivetype.normal -text "normal" -anchor w -variable [Dialog:variable $handle archiveType] -value "normal"
      pack $handle.schedule.archivetype.normal -side left
      bind $handle.schedule.archivetype.normal <Return> "focus $handle.buttons.add"
      radiobutton $handle.schedule.archivetype.full -text "full" -anchor w -variable [Dialog:variable $handle archiveType] -value "full"
      pack $handle.schedule.archivetype.full -side left
      bind $handle.schedule.archivetype.full <Return> "focus $handle.buttons.add"
      radiobutton $handle.schedule.archivetype.incremental -text "incremental" -anchor w -variable [Dialog:variable $handle archiveType] -value "incremental"
      pack $handle.schedule.archivetype.incremental -side left
      bind $handle.schedule.archivetype.incremental <Return> "focus $handle.buttons.add"
    grid $handle.schedule.archivetype -row 2 -column 1 -sticky "we"

    grid rowconfigure    $handle.schedule { 0 } -weight 1
    grid columnconfigure $handle.schedule { 1 } -weight 1
  grid $handle.schedule -row 0 -column 0 -sticky "nswe" -padx 3p -pady 3p

  frame $handle.buttons
    button $handle.buttons.add -text $okText -command "event generate $handle <<Event_add>>"
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

  # set values
  set year "*"; set month "*"; set day "*"
  regexp {0*([^-]+)-0*([^-]+)-0*([^-]+)} [lindex $schedule 0] * year month day
  set hour "*"; set minute "*"
  regexp {0*([^-]+):0*([^-]+)} [lindex $schedule 2] * hour minute
  Dialog:set $handle year        $year
  Dialog:set $handle month       $month
  Dialog:set $handle day         $day
  Dialog:set $handle weekDay     [lindex $schedule 1]
  Dialog:set $handle hour        $hour
  Dialog:set $handle minute      $minute
  Dialog:set $handle archiveType [lindex $schedule 3]

  focus [$handle.schedule.year subwidget menubutton]

  Dialog:show $handle
  set result      [Dialog:get $handle result     ]
  set year        [Dialog:get $handle year       ]
  set month       [Dialog:get $handle month      ]
  set day         [Dialog:get $handle day        ]
  set weekDay     [Dialog:get $handle weekDay    ]
  set hour        [Dialog:get $handle hour       ]
  set minute      [Dialog:get $handle minute     ]
  set archiveType [Dialog:get $handle archiveType]
  Dialog:delete $handle
  if {($result != 1)} { return 0 }

  set date "[numberFormat "%04d" $year]-[numberFormat "%04d" $month]-[numberFormat "%04d" $day]"
  set time "[numberFormat "%02d" $hour]:[numberFormat "%02d" $minute]"
  set schedule [list $date $weekDay $time $archiveType]

  return 1
}

#***********************************************************************
# Name   : addSchedule
# Purpose: add new schedule
# Input  : -
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addSchedule { } \
{
  global barConfig

  # edit
  set schedule [list "*" "*" "*" "*"]
  if {![editScheduleDialog "Add schedule" "Add" schedule]} \
  {
    return
  }

  # add
  set scheduleList [concat $::selectedJob(schedule) [list $schedule]]
  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "SCHEDULE_CLEAR $::selectedJob(id)"
  foreach schedule $scheduleList \
  {
    BackupServer:executeCommand errorCode errorText "SCHEDULE_ADD $::selectedJob(id) [lindex $schedule 0] [lindex $schedule 1] [lindex $schedule 2] [lindex $schedule 3]"
  }
  if {$errorCode != 0} \
  {
    return
  }
  set ::selectedJob(schedule) $scheduleList
}

#***********************************************************************
# Name   : editSchedule
# Purpose: edit schedule
# Input  : index - index of schedule in widget list
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc editSchedule { index } \
{
  global barConfig

  # edit schedule
  set schedule [lindex $::selectedJob(schedule) $index]
  if {![editScheduleDialog "Edit schedule" "Save" schedule]} \
  {
    return
  }

  # update list
  set scheduleList [lreplace $::selectedJob(schedule) $index $index $schedule]
  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "SCHEDULE_CLEAR $::selectedJob(id)"
  foreach schedule $scheduleList \
  {
    BackupServer:executeCommand errorCode errorText "SCHEDULE_ADD $::selectedJob(id) [lindex $schedule 0] [lindex $schedule 1] [lindex $schedule 2] [lindex $schedule 3]"
  }
  if {$errorCode != 0} \
  {
    return
  }
  set ::selectedJob(schedule) $scheduleList
}

#***********************************************************************
# Name   : remSchedule
# Purpose: remove schedule widget list
# Input  : index - index of schedule in widget list
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc remSchedule { index } \
{
  global barConfig

  # remove
  set scheduleList [lreplace $::selectedJob(schedule) $index $index]
  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "SCHEDULE_CLEAR $::selectedJob(id)"
  foreach schedule $scheduleList \
  {
    BackupServer:executeCommand errorCode errorText "SCHEDULE_ADD $::selectedJob(id) [lindex $schedule 0] [lindex $schedule 1] [lindex $schedule 2] [lindex $schedule 3]"
  }
  if {$errorCode != 0} \
  {
    return
  }
  set ::selectedJob(schedule) $scheduleList
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
    {0  1 "-"     "text '-'"                       "-"}
    {0  2 ".bar"  "text '.bar'"                    ".bar"}

    {0  4 "#"     "part number 1 digit"            "1"}
    {0  5 "##"    "part number 2 digit"            "12"}
    {0  6 "###"   "part number 3 digit"            "123"}
    {0  7 "####"  "part number 4 digit"            "1234"}

    {0  9 "%type" "archive type: full,incremental" "full"}
    {0 10 "%last" "'-last' if last archive part"   "-last"}

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
      set fileNamePartList [lreplace [Dialog:get $handle fileNamePartList] $selectedIndex $selectedIndex]
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
# Name   : getArchiveName
# Purpose: get archive name
# Input  : storageType       - storage type
#          storageFileName   - storage file name
#          storageLoginName  - storage login name
#          storageHostName   - storage host name
#          storageDeviceName - storage device name
# Output : -
# Return : archive name
# Notes  : -
#***********************************************************************

proc getArchiveName { storageType storageFileName storageLoginName storageHostName storageDeviceName } \
{
  if     {$storageType == "filesystem"} \
  {
    set archiveName $storageFileName
  } \
  elseif {$storageType == "ftp"} \
  {
    set archiveName "ftp:$storageLoginName@$storageHostName:$storageFileName"
  } \
  elseif {$storageType == "scp"} \
  {
    set archiveName "scp:$storageLoginName@$storageHostName:$storageFileName"
  } \
  elseif {$storageType == "sftp"} \
  {
    set archiveName "sftp:$storageLoginName@$storageHostName:$storageFileName"
  } \
  elseif {$storageType == "dvd"} \
  {
    set archiveName "dvd:$storageDeviceName:$storageFileName"
  } \
  elseif {$storageType == "device"} \
  {
    set archiveName "$storageDeviceName:$storageFileName"
  } \
  else \
  {
    internalError "unknown storage type '$storageType'"
  }

  return $archiveName;
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
    "^--pause$" \
    {
      set pauseFlag 1
      set guiMode 0
    }
    "^--continue$" \
    {
      set continueFlag 1
      set guiMode 0
    }
    "^--quit$" \
    {
      set quitFlag 1
      set guiMode 0
    }
    "^--debug$" \
    {
      set debugFlag 1
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
        lastExecutedDateTime \
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
        [expr {($lastExecutedDateTime > 0)?[clock format $lastExecutedDateTime -format "%Y-%m-%d %H:%M:%S"]:"-"}] \
        [format "%d days %02d:%02d:%02d" $estimatedRestDays $estimatedRestHours $estimatedRestMinutes $estimatedRestSeconds] \
      ]
    }
    puts [string repeat "-" [string length $s]]
  }
  if {$startFlag} \
  {
    if {$configFileName != ""} \
    {
#      loadBARConfig $configFileName
puts "NYI"; exit 1;
      addBackupJob ""
    }
  }
  if {$abortId != 0} \
  {
    jobAbort $abortId
  }
  if {$pauseFlag} \
  {
    set errorCode 0
    set errorText ""
    BackupServer:executeCommand errorCode errorText "PAUSE"
  }
  if {$continueFlag} \
  {
    set errorCode 0
    set errorText ""
    BackupServer:executeCommand errorCode errorText "CONTINUE"
  }
  if {$quitFlag} \
  {
    # disconnect
    BackupServer:disconnect
    exit 0
  }
}

# ----------------------------------------------------------------------

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
  $mainWindow.menu.file.items add separator
#  $mainWindow.menu.file.items add command -label "Start"                            -command "event generate . <<Event_start>>"
#  $mainWindow.menu.file.items add separator
  $mainWindow.menu.file.items add command -label "Quit"       -accelerator "Ctrl-q" -command "event generate . <<Event_quit>>"
  pack $mainWindow.menu.file -side left

  if {$debugFlag} \
  {
    menubutton $mainWindow.menu.debug -text "Debug" -menu $mainWindow.menu.debug.items -underline 0
    menu $mainWindow.menu.debug.items
    $mainWindow.menu.debug.items add command -label "Memory info" -accelerator "" -command "event generate . <<Event_debugMemoryInfo>>"
    pack $mainWindow.menu.debug -side left
  }

#  menubutton $mainWindow.menu.edit -text "Edit" -menu $mainWindow.menu.edit.items -underline 0
#  menu $mainWindow.menu.edit.items
#  $mainWindow.menu.edit.items add command -label "None"    -accelerator "*" -command "event generate . <<Event_backupStateNone>>"
#  $mainWindow.menu.edit.items add command -label "Include" -accelerator "+" -command "event generate . <<Event_backupStateIncluded>>"
#  $mainWindow.menu.edit.items add command -label "Exclude" -accelerator "-" -command "event generate . <<Event_backupStateExcluded>>"
#  pack $mainWindow.menu.edit -side left
pack $mainWindow.menu -side top -fill x

# window
tixNoteBook $mainWindow.tabs
  $mainWindow.tabs add jobs    -label "Status (F1)"  -underline -1 -raisecmd { focus .status.list.data }
  $mainWindow.tabs add backup  -label "Jobs (F2)"    -underline -1
  $mainWindow.tabs add restore -label "Restore (F3)" -underline -1
pack $mainWindow.tabs -fill both -expand yes -padx 2p -pady 2p

# ----------------------------------------------------------------------

frame .status
  frame .status.list
    mclistbox::mclistbox .status.list.data \
      -height 1 \
      -fillcolumn name \
      -bg white \
      -labelanchor w \
      -selectmode single \
      -exportselection 0 \
      -xscrollcommand ".status.list.xscroll set" \
      -yscrollcommand ".status.list.yscroll set"
    # note: -visible 0 does not work; bug in mclistbox (no selection possible)
    .status.list.data column add id                   -width 0 -resizable 0
    .status.list.data column add name                 -label "Name"           -width 12
    .status.list.data column add state                -label "State"          -width 16
    .status.list.data column add type                 -label "Type"           -width 10
    .status.list.data column add archivePartSize      -label "Part size"      -width 8
    .status.list.data column add compressAlgortihm    -label "Compress"       -width 10
    .status.list.data column add cryptAlgorithm       -label "Crypt"          -width 10
    .status.list.data column add lastExecutedDateTime -label "Last executed"  -width 20
    .status.list.data column add estimatedRestTime    -label "Estimated time" -width 20
    grid .status.list.data -row 0 -column 0 -sticky "nswe"
    scrollbar .status.list.yscroll -orient vertical -command ".status.list.data yview"
    grid .status.list.yscroll -row 0 -column 1 -sticky "ns"
    scrollbar .status.list.xscroll -orient horizontal -command ".status.list.data xview"
    grid .status.list.xscroll -row 1 -column 0 -sticky "we"

    grid rowconfigure    .status.list { 0 } -weight 1
    grid columnconfigure .status.list { 0 } -weight 1
  grid .status.list -row 0 -column 0 -sticky "nswe" -padx 2p -pady 2p

  labelframe .status.selected -text "Selected"
    label .status.selected.done -text "Done:"
    grid .status.selected.done -row 0 -column 0 -sticky "w" 

    frame .status.selected.doneFiles
      entry .status.selected.doneFiles.data -width 10 -textvariable jobStatus(doneFiles) -justify right -borderwidth 0 -state readonly
      pack .status.selected.doneFiles.data -side left
      label .status.selected.doneFiles.unit -text "files" -anchor w
      pack .status.selected.doneFiles.unit -side left
    grid .status.selected.doneFiles -row 0 -column 1 -sticky "w" -padx 2p -pady 2p

    frame .status.selected.doneBytes
      entry .status.selected.doneBytes.data -width 20 -textvariable jobStatus(doneBytes) -justify right -borderwidth 0 -state readonly
      pack .status.selected.doneBytes.data -side left
      label .status.selected.doneBytes.unit -text "bytes" -anchor w
      pack .status.selected.doneBytes.unit -side left
    grid .status.selected.doneBytes -row 0 -column 2 -sticky "w" -padx 2p -pady 2p

    label .status.selected.doneSeparator1 -text "/"
    grid .status.selected.doneSeparator1 -row 0 -column 3 -sticky "w" 

    frame .status.selected.doneBytesShort
      entry .status.selected.doneBytesShort.data -width 6 -textvariable jobStatus(doneBytesShort) -justify right -borderwidth 0 -state readonly
      pack .status.selected.doneBytesShort.data -side left
      label .status.selected.doneBytesShort.unit -width 8 -textvariable jobStatus(doneBytesShortUnit) -anchor w
      pack .status.selected.doneBytesShort.unit -side left
    grid .status.selected.doneBytesShort -row 0 -column 4 -sticky "w" -padx 2p -pady 2p

    label .status.selected.stored -text "Stored:"
    grid .status.selected.stored -row 1 -column 0 -sticky "w" 

    frame .status.selected.storageTotalBytes
      entry .status.selected.storageTotalBytes.data -width 20 -textvariable jobStatus(archiveBytes) -justify right -borderwidth 0 -state readonly
      pack .status.selected.storageTotalBytes.data -side left
      label .status.selected.storageTotalBytes.postfix -text "bytes" -anchor w
      pack .status.selected.storageTotalBytes.postfix -side left
    grid .status.selected.storageTotalBytes -row 1 -column 2 -sticky "w" -padx 2p -pady 2p

    label .status.selected.doneSeparator2 -text "/"
    grid .status.selected.doneSeparator2 -row 1 -column 3 -sticky "w" 

    frame .status.selected.doneStorageTotalBytesShort
      entry .status.selected.doneStorageTotalBytesShort.data -width 6 -textvariable jobStatus(archiveBytesShort) -justify right -borderwidth 0 -state readonly
      pack .status.selected.doneStorageTotalBytesShort.data -side left
      label .status.selected.doneStorageTotalBytesShort.unit -width 8 -textvariable jobStatus(archiveBytesShortUnit) -anchor w
      pack .status.selected.doneStorageTotalBytesShort.unit -side left
    grid .status.selected.doneStorageTotalBytesShort -row 1 -column 4 -sticky "w" -padx 2p -pady 2p

    frame .status.selected.doneCompressRatio
      label .status.selected.doneCompressRatio.title -text "Ratio"
      pack .status.selected.doneCompressRatio.title -side left
      entry .status.selected.doneCompressRatio.data -width 7 -textvariable jobStatus(compressionRatio) -justify right -borderwidth 0 -state readonly
      pack .status.selected.doneCompressRatio.data -side left
      label .status.selected.doneCompressRatio.postfix -text "%" -anchor w
      pack .status.selected.doneCompressRatio.postfix -side left
    grid .status.selected.doneCompressRatio -row 1 -column 5 -sticky "w" -padx 2p -pady 2p

    label .status.selected.skipped -text "Skipped:"
    grid .status.selected.skipped -row 2 -column 0 -sticky "w" 

    frame .status.selected.skippedFiles
      entry .status.selected.skippedFiles.data -width 10 -textvariable jobStatus(skippedFiles) -justify right -borderwidth 0 -state readonly
      pack .status.selected.skippedFiles.data -side left
      label .status.selected.skippedFiles.unit -text "files" -anchor w
      pack .status.selected.skippedFiles.unit -side left
    grid .status.selected.skippedFiles -row 2 -column 1 -sticky "w" -padx 2p -pady 2p

    frame .status.selected.skippedBytes
      entry .status.selected.skippedBytes.data -width 20 -textvariable jobStatus(skippedBytes) -justify right -borderwidth 0 -state readonly
      pack .status.selected.skippedBytes.data -side left
      label .status.selected.skippedBytes.unit -text "bytes" -anchor w
      pack .status.selected.skippedBytes.unit -side left
    grid .status.selected.skippedBytes -row 2 -column 2 -sticky "w" -padx 2p -pady 2p

    label .status.selected.skippedSeparator -text "/"
    grid .status.selected.skippedSeparator -row 2 -column 3 -sticky "w" 

    frame .status.selected.skippedBytesShort
      entry .status.selected.skippedBytesShort.data -width 6 -textvariable jobStatus(skippedBytesShort) -justify right -borderwidth 0 -state readonly
      pack .status.selected.skippedBytesShort.data -side left
      label .status.selected.skippedBytesShort.unit -width 8 -textvariable jobStatus(skippedBytesShortUnit) -anchor w
      pack .status.selected.skippedBytesShort.unit -side left
    grid .status.selected.skippedBytesShort -row 2 -column 4 -sticky "w" -padx 2p -pady 2p

    label .status.selected.error -text "Errors:"
    grid .status.selected.error -row 3 -column 0 -sticky "w" 

    frame .status.selected.errorFiles
      entry .status.selected.errorFiles.data -width 10 -textvariable jobStatus(errorFiles) -justify right -borderwidth 0 -state readonly
      pack .status.selected.errorFiles.data -side left
      label .status.selected.errorFiles.unit -text "files" -anchor w
      pack .status.selected.errorFiles.unit -side left
    grid .status.selected.errorFiles -row 3 -column 1 -sticky "w" -padx 2p -pady 2p

    frame .status.selected.errorBytes
      entry .status.selected.errorBytes.data -width 20 -textvariable jobStatus(errorBytes) -justify right -borderwidth 0 -state readonly
      pack .status.selected.errorBytes.data -side left
      label .status.selected.errorBytes.unit -text "bytes"
      pack .status.selected.errorBytes.unit -side left
    grid .status.selected.errorBytes -row 3 -column 2 -sticky "w" -padx 2p -pady 2p

    label .status.selected.errorSeparator -text "/"
    grid .status.selected.errorSeparator -row 3 -column 3 -sticky "w" 

    frame .status.selected.errorBytesShort
      entry .status.selected.errorBytesShort.data -width 6 -textvariable jobStatus(errorBytesShort) -justify right -borderwidth 0 -state readonly
      pack .status.selected.errorBytesShort.data -side left
      label .status.selected.errorBytesShort.unit -width 8 -textvariable jobStatus(errorBytesShortUnit) -anchor w
      pack .status.selected.errorBytesShort.unit -side left
    grid .status.selected.errorBytesShort -row 3 -column 4 -sticky "w" -padx 2p -pady 2p

    label .status.selected.total -text "Total:"
    grid .status.selected.total -row 4 -column 0 -sticky "w" 

    frame .status.selected.totalFiles
      entry .status.selected.totalFiles.data -width 10 -textvariable jobStatus(totalFiles) -justify right -borderwidth 0 -state readonly
      pack .status.selected.totalFiles.data -side left
      label .status.selected.totalFiles.unit -text "files" -anchor w
      pack .status.selected.totalFiles.unit -side left
    grid .status.selected.totalFiles -row 4 -column 1 -sticky "w" -padx 2p -pady 2p

    frame .status.selected.totalBytes
      entry .status.selected.totalBytes.data -width 20 -textvariable jobStatus(totalBytes) -justify right -borderwidth 0 -state readonly
      pack .status.selected.totalBytes.data -side left
      label .status.selected.totalBytes.unit -text "bytes"
      pack .status.selected.totalBytes.unit -side left
    grid .status.selected.totalBytes -row 4 -column 2 -sticky "w" -padx 2p -pady 2p

    label .status.selected.totalSeparator -text "/"
    grid .status.selected.totalSeparator -row 4 -column 3 -sticky "w" 

    frame .status.selected.totalBytesShort
      entry .status.selected.totalBytesShort.data -width 6 -textvariable jobStatus(totalBytesShort) -justify right -borderwidth 0 -state readonly
      pack .status.selected.totalBytesShort.data -side left
      label .status.selected.totalBytesShort.unit -width 8 -textvariable jobStatus(totalBytesShortUnit) -anchor w
      pack .status.selected.totalBytesShort.unit -side left
    grid .status.selected.totalBytesShort -row 4 -column 4 -sticky "w" -padx 2p -pady 2p

    frame .status.selected.doneFilesPerSecond
      entry .status.selected.doneFilesPerSecond.data -width 10 -textvariable jobStatus(filesPerSecond) -justify right -borderwidth 0 -state readonly
      pack .status.selected.doneFilesPerSecond.data -side left
      label .status.selected.doneFilesPerSecond.unit -text "files/s" -anchor w
      pack .status.selected.doneFilesPerSecond.unit -side left
    grid .status.selected.doneFilesPerSecond -row 4 -column 5 -sticky "w" -padx 2p -pady 2p

    frame .status.selected.doneBytesPerSecond
      entry .status.selected.doneBytesPerSecond.data -width 10 -textvariable jobStatus(bytesPerSecondShort) -justify right -borderwidth 0 -state readonly
      pack .status.selected.doneBytesPerSecond.data -side left
      label .status.selected.doneBytesPerSecond.unit -width 8 -textvariable jobStatus(bytesPerSecondShortUnit) -anchor w
      pack .status.selected.doneBytesPerSecond.unit -side left
    grid .status.selected.doneBytesPerSecond -row 4 -column 6 -sticky "w" -padx 2p -pady 2p

    label .status.selected.currentFileNameTitle -text "File:"
    grid .status.selected.currentFileNameTitle -row 5 -column 0 -sticky "w"
    entry .status.selected.currentFileName -textvariable jobStatus(fileName) -borderwidth 0 -state readonly
    grid .status.selected.currentFileName -row 5 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    Dialog:progressbar .status.selected.filePercentage
    grid .status.selected.filePercentage -row 6 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .status.selected.storageNameTitle -text "Storage:"
    grid .status.selected.storageNameTitle -row 7 -column 0 -sticky "w"
    entry .status.selected.storageName -textvariable jobStatus(storageName) -borderwidth 0 -state readonly
    grid .status.selected.storageName -row 7 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    Dialog:progressbar .status.selected.storagePercentage
    grid .status.selected.storagePercentage -row 8 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .status.selected.storageVolumeTitle -text "Volume:"
    grid .status.selected.storageVolumeTitle -row 9 -column 0 -sticky "w"
    Dialog:progressbar .status.selected.storageVolume
    grid .status.selected.storageVolume -row 9 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .status.selected.totalFilesPercentageTitle -text "Total files:"
    grid .status.selected.totalFilesPercentageTitle -row 10 -column 0 -sticky "w"
    Dialog:progressbar .status.selected.totalFilesPercentage
    grid .status.selected.totalFilesPercentage -row 10 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .status.selected.totalBytesPercentageTitle -text "Total bytes:"
    grid .status.selected.totalBytesPercentageTitle -row 11 -column 0 -sticky "w"
    Dialog:progressbar .status.selected.totalBytesPercentage
    grid .status.selected.totalBytesPercentage -row 11 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

    label .status.selected.messageTitle -text "Message:"
    grid .status.selected.messageTitle -row 12 -column 0 -sticky "w"
    entry .status.selected.message -textvariable jobStatus(message) -borderwidth 0 -highlightthickness 0 -state readonly -font -*-*-bold-*-*-*-*-*-*-*-*-*-*-*-*
    grid .status.selected.message -row 12 -column 1 -columnspan 6 -sticky "we" -padx 2p -pady 2p

#    grid rowconfigure    .status.selected { 0 } -weight 1
    grid columnconfigure .status.selected { 4 } -weight 1
  grid .status.selected -row 1 -column 0 -sticky "we" -padx 2p -pady 2p

  frame .status.buttons
    button .status.buttons.start -text "Start" -state disabled -command "event generate . <<Event_jobStart>>"
    pack .status.buttons.start -side left -padx 2p
    button .status.buttons.abort -text "Abort" -state disabled -command "event generate . <<Event_jobAbort>>"
    pack .status.buttons.abort -side left -padx 2p
    button .status.buttons.pausecontinue -text "Pause" -width 12 -command "event generate . <<Event_jobPauseContinue>>"
    pack .status.buttons.pausecontinue -side left -padx 2p
    button .status.buttons.volume -text "Volume" -state disabled -command "event generate . <<Event_volume>>"
    pack .status.buttons.volume -side left -padx 2p
    button .status.buttons.quit -text "Quit" -command "event generate . <<Event_quit>>"
    pack .status.buttons.quit -side right -padx 2p
  grid .status.buttons -row 2 -column 0 -sticky "we" -padx 2p -pady 2p

  bind .status.list.data <ButtonRelease-1> "event generate . <<Event_selectJobStatus>>"

  grid rowconfigure    .status { 0 } -weight 1
  grid columnconfigure .status { 0 } -weight 1
pack .status -side top -fill both -expand yes -in [$mainWindow.tabs subwidget jobs]

# ----------------------------------------------------------------------

frame .jobs
  label .jobs.listTitle -text "Name:"
  grid .jobs.listTitle -row 0 -column 0 -sticky "w"
  frame .jobs.list
    tixOptionMenu .jobs.list.data -label "" -labelside right -variable selectedJob(id) -command selectJob -options { entry.background white }
    grid .jobs.list.data -row 0 -column 0 -sticky "we" -padx 2p -pady 2p
    button .jobs.list.new -text "New" -command "event generate . <<Event_jobNew>>"
    grid .jobs.list.new -row 0 -column 1 -sticky "e" -padx 2p -pady 2p
    button .jobs.list.delete -text "Delete" -command "event generate . <<Event_jobDelete>>"
    grid .jobs.list.delete -row 0 -column 2 -sticky "e" -padx 2p -pady 2p

    grid columnconfigure .jobs.list { 0 } -weight 1
  grid .jobs.list -row 0 -column 1 -sticky "we" -padx 2p -pady 2p

  tixNoteBook .jobs.tabs
    .jobs.tabs add files    -label "Files"    -underline -1 -raisecmd { focus .jobs.files.list }
    .jobs.tabs add filters  -label "Filters"  -underline -1 -raisecmd { focus .jobs.filters.included }
    .jobs.tabs add storage  -label "Storage"  -underline -1 
    .jobs.tabs add schedule -label "Schedule" -underline -1 -raisecmd { focus .jobs.schedule.list }
  #  $mainWindow.tabs add misc          -label "Misc"             -underline -1
  grid .jobs.tabs -row 1 -column 0 -columnspan 2 -sticky "nswe" -padx 2p -pady 2p

  frame .jobs.files
    tixTree .jobs.files.list -scrollbar both -options \
    {
      hlist.separator "/"
      hlist.columns 4
      hlist.header yes
      hlist.indent 16
    }
    .jobs.files.list subwidget hlist configure -selectmode extended

    .jobs.files.list subwidget hlist header create 0 -itemtype text -text "File"
    .jobs.files.list subwidget hlist header create 1 -itemtype text -text "Type"
    .jobs.files.list subwidget hlist column width 1 -char 10
    .jobs.files.list subwidget hlist header create 2 -itemtype text -text "Size"
    .jobs.files.list subwidget hlist column width 2 -char 10
    .jobs.files.list subwidget hlist header create 3 -itemtype text -text "Modified"
    .jobs.files.list subwidget hlist column width 3 -char 15
    grid .jobs.files.list -row 0 -column 0 -sticky "nswe" -padx 2p -pady 2p
    set filesTreeWidget [.jobs.files.list subwidget hlist]

    tixPopupMenu .jobs.files.list.popup -title "Command"
    .jobs.files.list.popup subwidget menu add command -label "Add include"                            -command ""
    .jobs.files.list.popup subwidget menu add command -label "Add exclude"                            -command ""
    .jobs.files.list.popup subwidget menu add command -label "Remove include"                         -command ""
    .jobs.files.list.popup subwidget menu add command -label "Remove exclude"                         -command ""
    .jobs.files.list.popup subwidget menu add command -label "Add include pattern"    -state disabled -command ""
    .jobs.files.list.popup subwidget menu add command -label "Add exclude pattern"    -state disabled -command ""
    .jobs.files.list.popup subwidget menu add command -label "Remove include pattern" -state disabled -command ""
    .jobs.files.list.popup subwidget menu add command -label "Remove exclude pattern" -state disabled -command ""

    proc backupFilesPopupHandler { widget x y } \
    {
      set fileName [$widget nearest $y]
      set extension ""
      regexp {.*(\.[^\.]+)} $fileName * extension

      .jobs.files.list.popup subwidget menu entryconfigure 0 -label "Add include '$fileName'"    -command "addIncludedPattern $fileName"
      .jobs.files.list.popup subwidget menu entryconfigure 1 -label "Add exclude '$fileName'"    -command "addExcludedPattern $fileName"
      .jobs.files.list.popup subwidget menu entryconfigure 2 -label "Remove include '$fileName'" -command "remIncludedPattern $fileName"
      .jobs.files.list.popup subwidget menu entryconfigure 3 -label "Remove exclude '$fileName'" -command "remExcludedPattern $fileName"
      if {$extension != ""} \
      {
        .jobs.files.list.popup subwidget menu entryconfigure 4 -label "Add include pattern *$extension"    -state normal -command "addIncludedPattern *$extension"
        .jobs.files.list.popup subwidget menu entryconfigure 5 -label "Add exclude pattern *$extension"    -state normal -command "addExcludedPattern *$extension"
        .jobs.files.list.popup subwidget menu entryconfigure 6 -label "Remove include pattern *$extension" -state normal -command "remIncludedPattern *$extension"
        .jobs.files.list.popup subwidget menu entryconfigure 7 -label "Remove exclude pattern *$extension" -state normal -command "remExcludedPattern *$extension"
      } \
      else \
      {
        .jobs.files.list.popup subwidget menu entryconfigure 4 -label "Add include pattern -"    -state disabled -command ""
        .jobs.files.list.popup subwidget menu entryconfigure 5 -label "Add exclude pattern -"    -state disabled -command ""
        .jobs.files.list.popup subwidget menu entryconfigure 6 -label "Remove include pattern -" -state disabled -command ""
        .jobs.files.list.popup subwidget menu entryconfigure 7 -label "Remove exclude pattern -" -state disabled -command ""
      }
      .jobs.files.list.popup post $widget $x $y
    }

    frame .jobs.files.buttons
      button .jobs.files.buttons.stateNone -text "*" -command "event generate . <<Event_backupStateNone>>"
      pack .jobs.files.buttons.stateNone -side left -fill x -expand yes
      button .jobs.files.buttons.stateIncluded -text "+" -command "event generate . <<Event_backupStateIncluded>>"
      pack .jobs.files.buttons.stateIncluded -side left -fill x -expand yes
      button .jobs.files.buttons.stateExcluded -text "-" -command "event generate . <<Event_backupStateExcluded>>"
      pack .jobs.files.buttons.stateExcluded -side left -fill x -expand yes
    grid .jobs.files.buttons -row 1 -column 0 -sticky "we" -padx 2p -pady 2p

    bind [.jobs.files.list subwidget hlist] <Button-3>    "backupFilesPopupHandler %W %x %y"
    bind [.jobs.files.list subwidget hlist] <BackSpace>   "event generate . <<Event_backupStateNone>>"
    bind [.jobs.files.list subwidget hlist] <Delete>      "event generate . <<Event_backupStateNone>>"
    bind [.jobs.files.list subwidget hlist] <plus>        "event generate . <<Event_backupStateIncluded>>"
    bind [.jobs.files.list subwidget hlist] <KP_Add>      "event generate . <<Event_backupStateIncluded>>"
    bind [.jobs.files.list subwidget hlist] <minus>       "event generate . <<Event_backupStateExcluded>>"
    bind [.jobs.files.list subwidget hlist] <KP_Subtract> "event generate . <<Event_backupStateExcluded>>"
    bind [.jobs.files.list subwidget hlist] <space>       "event generate . <<Event_backupToggleStateNoneIncludedExcluded>>"

    # fix a bug in tix: end does not use separator-char to detect last entry
    bind [.jobs.files.list subwidget hlist] <KeyPress-End> \
      "
       .jobs.files.list subwidget hlist yview moveto 1
       .jobs.files.list subwidget hlist anchor set \[lindex \[.jobs.files.list subwidget hlist info children /\] end\]
       break
      "

    # mouse-wheel events
    bind [.jobs.files.list subwidget hlist] <Button-4> \
      "
       set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
       .jobs.files.list subwidget hlist yview scroll -\$n units
      "
    bind [.jobs.files.list subwidget hlist] <Button-5> \
      "
       set n \[expr {\[string is integer \"%D\"\]?\"%D\":5}\]
       .jobs.files.list subwidget hlist yview scroll +\$n units
      "

    grid rowconfigure    .jobs.files { 0 } -weight 1
    grid columnconfigure .jobs.files { 0 } -weight 1
  pack .jobs.files -side top -fill both -expand yes -in [.jobs.tabs subwidget files]

  frame .jobs.filters
    label .jobs.filters.includedTitle -text "Included:"
    grid .jobs.filters.includedTitle -row 0 -column 0 -sticky "nw"
    tixScrolledListBox .jobs.filters.included -height 1 -scrollbar both -options { listbox.background white  }
    grid .jobs.filters.included -row 0 -column 1 -sticky "nswe" -padx 2p -pady 2p
    .jobs.filters.included subwidget listbox configure -listvariable ::selectedJob(included) -selectmode extended
    bind [.jobs.filters.included subwidget listbox] <Button-1> ".jobs.filters.includedButtons.rem configure -state normal"
    bind [.jobs.filters.included subwidget listbox] <Insert> "event generate . <<Event_backupAddIncludePattern>>"
    bind [.jobs.filters.included subwidget listbox] <Delete> "event generate . <<Event_backupRemIncludePattern>>"
    set includedListWidget [.jobs.filters.included subwidget listbox]

    frame .jobs.filters.includedButtons
      button .jobs.filters.includedButtons.add -text "Add (F5)" -command "event generate . <<Event_backupAddIncludePattern>>"
      pack .jobs.filters.includedButtons.add -side left
      button .jobs.filters.includedButtons.rem -text "Rem (F6)" -state disabled -command "event generate . <<Event_backupRemIncludePattern>>"
      pack .jobs.filters.includedButtons.rem -side left
    grid .jobs.filters.includedButtons -row 1 -column 1 -sticky "we" -padx 2p -pady 2p

    label .jobs.filters.excludedTitle -text "Excluded:"
    grid .jobs.filters.excludedTitle -row 2 -column 0 -sticky "nw"
    tixScrolledListBox .jobs.filters.excluded -height 1 -scrollbar both -options { listbox.background white }
    grid .jobs.filters.excluded -row 2 -column 1 -sticky "nswe" -padx 2p -pady 2p
    .jobs.filters.excluded subwidget listbox configure -listvariable ::selectedJob(excluded) -selectmode extended
    bind [.jobs.filters.excluded subwidget listbox] <Button-1> ".jobs.filters.excludedButtons.rem configure -state normal"
    bind [.jobs.filters.excluded subwidget listbox] <Insert> "event generate . <<Event_backupAddExcludePattern>>"
    bind [.jobs.filters.excluded subwidget listbox] <Delete> "event generate . <<Event_backupRemExcludePattern>>"
    set excludedListWidget [.jobs.filters.excluded subwidget listbox]

    frame .jobs.filters.excludedButtons
      button .jobs.filters.excludedButtons.add -text "Add (F7)" -command "event generate . <<Event_backupAddExcludePattern>>"
      pack .jobs.filters.excludedButtons.add -side left
      button .jobs.filters.excludedButtons.rem -text "Rem (F8)" -state disabled -command "event generate . <<Event_backupRemExcludePattern>>"
      pack .jobs.filters.excludedButtons.rem -side left
    grid .jobs.filters.excludedButtons -row 3 -column 1 -sticky "we" -padx 2p -pady 2p

    label .jobs.filters.optionsTitle -text "Options:"
    grid .jobs.filters.optionsTitle -row 4 -column 0 -sticky "nw" 
    checkbutton .jobs.filters.optionSkipUnreadable -text "skip unreable files" -variable ::selectedJob(skipUnreadableFlag)
    grid .jobs.filters.optionSkipUnreadable -row 4 -column 1 -sticky "nw" 
    addModifyTrace {::selectedJob(skipUnreadableFlag)} \
    {
      BackupServer:set $::selectedJob(id) "skip-unreadable" $::selectedJob(skipUnreadableFlag)
    }

    grid rowconfigure    .jobs.filters { 0 2 } -weight 1
    grid columnconfigure .jobs.filters { 1 } -weight 1
  pack .jobs.filters -side top -fill both -expand yes -in [.jobs.tabs subwidget filters]

  frame .jobs.storage
    label .jobs.storage.archivePartSizeTitle -text "Part size:"
    grid .jobs.storage.archivePartSizeTitle -row 0 -column 0 -sticky "w" 
    frame .jobs.storage.archivePartSize
      radiobutton .jobs.storage.archivePartSize.unlimited -text "unlimited" -anchor w -variable ::selectedJob(archivePartSizeFlag) -value 0 -command "set selectedJob(archivePartSize) 0"
      grid .jobs.storage.archivePartSize.unlimited -row 0 -column 1 -sticky "w" 
      radiobutton .jobs.storage.archivePartSize.limited -text "limit to" -width 8 -anchor w -variable ::selectedJob(archivePartSizeFlag) -value 1
      grid .jobs.storage.archivePartSize.limited -row 0 -column 2 -sticky "w" 
      tixComboBox .jobs.storage.archivePartSize.size -variable ::selectedJob(archivePartSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
      grid .jobs.storage.archivePartSize.size -row 0 -column 3 -sticky "w" 
      label .jobs.storage.archivePartSize.unit -text "bytes"
      grid .jobs.storage.archivePartSize.unit -row 0 -column 4 -sticky "w" 

     .jobs.storage.archivePartSize.size insert end 32M
     .jobs.storage.archivePartSize.size insert end 64M
     .jobs.storage.archivePartSize.size insert end 128M
     .jobs.storage.archivePartSize.size insert end 256M
     .jobs.storage.archivePartSize.size insert end 512M
     .jobs.storage.archivePartSize.size insert end 1G
     .jobs.storage.archivePartSize.size insert end 2G

      grid rowconfigure    .jobs.storage.archivePartSize { 0 } -weight 1
      grid columnconfigure .jobs.storage.archivePartSize { 1 } -weight 1
    grid .jobs.storage.archivePartSize -row 0 -column 1 -sticky "w" -padx 2p -pady 2p
    addEnableTrace ::selectedJob(archivePartSizeFlag) 1 .jobs.storage.archivePartSize.size
    addModifyTrace {::selectedJob(archivePartSizeFlag) ::selectedJob(archivePartSize)} \
    {
      BackupServer:set $::selectedJob(id) "archive-part-size" [expr {$::selectedJob(archivePartSizeFlag)?$::selectedJob(archivePartSize):0}]
    }

if {0} {
    label .jobs.storage.maxTmpSizeTitle -text "Max. temp. size:"
    grid .jobs.storage.maxTmpSizeTitle -row 1 -column 0 -sticky "w" 
    frame .jobs.storage.maxTmpSize
      radiobutton .jobs.storage.maxTmpSize.unlimited -text "unlimited" -anchor w -variable barConfig(maxTmpSizeFlag) -value 0
      grid .jobs.storage.maxTmpSize.unlimited -row 0 -column 1 -sticky "w" 
      radiobutton .jobs.storage.maxTmpSize.limitto -text "limit to" -width 8 -anchor w -variable barConfig(maxTmpSizeFlag) -value 1
      grid .jobs.storage.maxTmpSize.limitto -row 0 -column 2 -sticky "w" 
      tixComboBox .jobs.storage.maxTmpSize.size -variable barConfig(maxTmpSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
      grid .jobs.storage.maxTmpSize.size -row 0 -column 3 -sticky "w" 
      label .jobs.storage.maxTmpSize.unit -text "bytes"
      grid .jobs.storage.maxTmpSize.unit -row 0 -column 4 -sticky "w" 

     .jobs.storage.maxTmpSize.size insert end 32M
     .jobs.storage.maxTmpSize.size insert end 64M
     .jobs.storage.maxTmpSize.size insert end 128M
     .jobs.storage.maxTmpSize.size insert end 256M
     .jobs.storage.maxTmpSize.size insert end 512M
     .jobs.storage.maxTmpSize.size insert end 1G
     .jobs.storage.maxTmpSize.size insert end 2G
     .jobs.storage.maxTmpSize.size insert end 4G
     .jobs.storage.maxTmpSize.size insert end 8G

      grid rowconfigure    .jobs.storage.maxTmpSize { 0 } -weight 1
      grid columnconfigure .jobs.storage.maxTmpSize { 1 } -weight 1
    grid .jobs.storage.maxTmpSize -row 1 -column 1 -sticky "w" -padx 2p -pady 2p
    addEnableTrace ::barConfig(maxTmpSizeFlag) 1 .jobs.storage.maxTmpSize.size
    addModifyTrace {::selectedJob(maxTmpSizeFlag) ::selectedJob(maxTmpSize)} \
    {
      BackupServer:set $::selectedJob(id) "max-tmp-size" [expr {$::selectedJob(maxTmpSizeFlag)?$::selectedJob(maxTmpSize):0}]
    }
}

    label .jobs.storage.compressAlgorithmTitle -text "Compress:"
    grid .jobs.storage.compressAlgorithmTitle -row 2 -column 0 -sticky "w" 
    tk_optionMenu .jobs.storage.compressAlgorithm ::selectedJob(compressAlgorithm) \
      "none" "zip0" "zip1" "zip2" "zip3" "zip4" "zip5" "zip6" "zip7" "zip8" "zip9" "bzip1" "bzip2" "bzip3" "bzip4" "bzip5" "bzip6" "bzip7" "bzip8" "bzip9"
    grid .jobs.storage.compressAlgorithm -row 2 -column 1 -sticky "w" -padx 2p -pady 2p
    addModifyTrace {::selectedJob(compressAlgorithm) } \
    {
      BackupServer:set $::selectedJob(id) "compress-algorithm" $::selectedJob(compressAlgorithm)
    }

    label .jobs.storage.cryptAlgorithmTitle -text "Crypt:"
    grid .jobs.storage.cryptAlgorithmTitle -row 3 -column 0 -sticky "w" 
    tk_optionMenu .jobs.storage.cryptAlgorithm ::selectedJob(cryptAlgorithm) \
      "none" "3DES" "CAST5" "BLOWFISH" "AES128" "AES192" "AES256" "TWOFISH128" "TWOFISH256"
    grid .jobs.storage.cryptAlgorithm -row 3 -column 1 -sticky "w" -padx 2p -pady 2p
    addModifyTrace {::selectedJob(cryptAlgorithm) } \
    {
      BackupServer:set $::selectedJob(id) "crypt-algorithm" $::selectedJob(cryptAlgorithm)
    }

    label .jobs.storage.cryptPasswordTitle -text "Password:"
    grid .jobs.storage.cryptPasswordTitle -row 4 -column 0 -sticky "nw" 
    frame .jobs.storage.cryptPassword
      radiobutton .jobs.storage.cryptPassword.modeDefault -text "default" -variable ::selectedJob(cryptPasswordMode) -value "default"
      grid .jobs.storage.cryptPassword.modeDefault -row 0 -column 1 -sticky "w"
      radiobutton .jobs.storage.cryptPassword.modeAsk -text "ask" -variable ::selectedJob(cryptPasswordMode) -value "ask"
      grid .jobs.storage.cryptPassword.modeAsk -row 0 -column 2 -sticky "w"
      radiobutton .jobs.storage.cryptPassword.modeConfig -text "this" -variable ::selectedJob(cryptPasswordMode) -value "config"
      grid .jobs.storage.cryptPassword.modeConfig -row 0 -column 3 -sticky "w"
      entry .jobs.storage.cryptPassword.data1 -textvariable ::selectedJob(cryptPassword) -bg white -show "*"
      grid .jobs.storage.cryptPassword.data1 -row 0 -column 4 -sticky "we"
      entry .jobs.storage.cryptPassword.data2 -textvariable ::selectedJob(cryptPasswordVerify) -bg white -show "*"
      grid .jobs.storage.cryptPassword.data2 -row 1 -column 4 -sticky "we"

      grid columnconfigure .jobs.storage.cryptPassword { 4 } -weight 1
    grid .jobs.storage.cryptPassword -row 4 -column 1 -sticky "we" -padx 2p -pady 2p
    addEnableTrace ::selectedJob(cryptPasswordMode) "config" .jobs.storage.cryptPassword.data1
    addEnableTrace ::selectedJob(cryptPasswordMode) "config" .jobs.storage.cryptPassword.data2
    addModifyTrace {::selectedJob(cryptPasswordMode) } \
    {
      BackupServer:set $::selectedJob(id) "crypt-password-mode" $::selectedJob(cryptPasswordMode)
    }
    addModifyTrace {::selectedJob(cryptPassword) } \
    {
      BackupServer:set $::selectedJob(id) "crypt-password" $::selectedJob(cryptPassword)
    }

    label .jobs.storage.modeTitle -text "Mode:"
    grid .jobs.storage.modeTitle -row 5 -column 0 -sticky "nw" 
    frame .jobs.storage.mode
      radiobutton .jobs.storage.mode.normal -text "normal" -variable ::selectedJob(archiveType) -value "normal"
      grid .jobs.storage.mode.normal -row 0 -column 0 -sticky "w"
      radiobutton .jobs.storage.mode.full -text "full" -variable ::selectedJob(archiveType) -value "full"
      grid .jobs.storage.mode.full -row 0 -column 1 -sticky "w"
      radiobutton .jobs.storage.mode.incremental -text "incremental" -variable ::selectedJob(archiveType) -value "incremental"
      grid .jobs.storage.mode.incremental -row 0 -column 3 -sticky "w"
      addModifyTrace {::selectedJob(archiveType)} \
      {
        BackupServer:set $::selectedJob(id) "archive-type" $::selectedJob(archiveType)
      }

      frame .jobs.storage.mode.incrementalListFileName
        entry .jobs.storage.mode.incrementalListFileName.data -textvariable ::selectedJob(incrementalListFileName) -bg white
        pack .jobs.storage.mode.incrementalListFileName.data -side left -fill x -expand yes
        button .jobs.storage.mode.incrementalListFileName.select -image $images(folder) -command \
        "
          set fileName \[Dialog:fileSelector \"Incremental list file\" \$::selectedJob(incrementalListFileName) {{\"*.bid\" \"incremental list\"} {\"*\" \"all\"}}\]
          if {\$fileName != \"\"} \
          {
            set ::selectedJob(incrementalListFileName) \$fileName
          }
        "
        pack .jobs.storage.mode.incrementalListFileName.select -side left
      grid .jobs.storage.mode.incrementalListFileName -row 0 -column 4 -sticky "we"
      addModifyTrace {::selectedJob(incrementalListFileName)} \
      {
        BackupServer:set $::selectedJob(id) "incremental-list-file" $::selectedJob(incrementalListFileName)
      }

      grid columnconfigure .jobs.storage.mode { 4 } -weight 1
    grid .jobs.storage.mode -row 5 -column 1 -sticky "we" -padx 2p -pady 2p

    label .jobs.storage.fileNameTitle -text "File name:"
    grid .jobs.storage.fileNameTitle -row 6 -column 0 -sticky "w" 
    frame .jobs.storage.fileName
      entry .jobs.storage.fileName.data -textvariable ::selectedJob(storageFileName) -bg white
      grid .jobs.storage.fileName.data -row 0 -column 0 -sticky "we" 

      button .jobs.storage.fileName.edit -image $images(folder) -command \
      "
        set fileName \[editStorageFileName \$::selectedJob(storageFileName)\]
        if {\$fileName != \"\"} \
        {
          set ::selectedJob(storageFileName) \$fileName
        }
      "
      grid .jobs.storage.fileName.edit -row 0 -column 1 -sticky "we" 

      grid columnconfigure .jobs.storage.fileName { 0 } -weight 1
    grid .jobs.storage.fileName -row 6 -column 1 -sticky "we" -padx 2p -pady 2p
    addModifyTrace {::selectedJob(storageFileName)} \
    {
      if {$::selectedJob(storageType) != ""} { BackupServer:set $::selectedJob(id) "archive-name" [getArchiveName $::selectedJob(storageType) $::selectedJob(storageFileName) $::selectedJob(storageLoginName) $::selectedJob(storageHostName) $::selectedJob(storageDeviceName)] }
    }

    label .jobs.storage.destinationTitle -text "Destination:"
    grid .jobs.storage.destinationTitle -row 7 -column 0 -sticky "nw" 
    frame .jobs.storage.destination
      frame .jobs.storage.destination.type
        radiobutton .jobs.storage.destination.type.fileSystem -text "File system" -variable ::selectedJob(storageType) -value "filesystem"
        grid .jobs.storage.destination.type.fileSystem -row 0 -column 0 -sticky "w"

        radiobutton .jobs.storage.destination.type.ftp -text "ftp" -variable ::selectedJob(storageType) -value "ftp"
        grid .jobs.storage.destination.type.ftp -row 0 -column 1 -sticky "w" 

        radiobutton .jobs.storage.destination.type.ssh -text "scp" -variable ::selectedJob(storageType) -value "scp"
        grid .jobs.storage.destination.type.ssh -row 0 -column 2 -sticky "w" 

        radiobutton .jobs.storage.destination.type.sftp -text "sftp" -variable ::selectedJob(storageType) -value "sftp"
        grid .jobs.storage.destination.type.sftp -row 0 -column 3 -sticky "w" 

        radiobutton .jobs.storage.destination.type.dvd -text "DVD" -variable ::selectedJob(storageType) -value "dvd"
        grid .jobs.storage.destination.type.dvd -row 0 -column 4 -sticky "w"

        radiobutton .jobs.storage.destination.type.device -text "Device" -variable ::selectedJob(storageType) -value "device"
        grid .jobs.storage.destination.type.device -row 0 -column 5 -sticky "w"

        grid rowconfigure    .jobs.storage.destination.type { 0 } -weight 1
        grid columnconfigure .jobs.storage.destination.type { 6 } -weight 1
      grid .jobs.storage.destination.type -row 0 -column 0 -sticky "we" -padx 2p -pady 2p
      addModifyTrace {::selectedJob(storageType)} \
      {
        if {$::selectedJob(storageType) != ""} { BackupServer:set $::selectedJob(id) "archive-name" [getArchiveName $::selectedJob(storageType) $::selectedJob(storageFileName) $::selectedJob(storageLoginName) $::selectedJob(storageHostName) $::selectedJob(storageDeviceName)] }
      }

      labelframe .jobs.storage.destination.fileSystem
        label .jobs.storage.destination.fileSystem.optionsTitle -text "Options:"
        grid .jobs.storage.destination.fileSystem.optionsTitle -row 0 -column 0 -sticky "w" 
        frame .jobs.storage.destination.fileSystem.options
          checkbutton .jobs.storage.destination.fileSystem.options.overwriteArchiveFiles -text "overwrite archive files" -variable ::selectedJob(overwriteArchiveFilesFlag)
          grid .jobs.storage.destination.fileSystem.options.overwriteArchiveFiles -row 0 -column 0 -sticky "w" 
          addModifyTrace {::selectedJob(overwriteArchiveFilesFlag)} \
          {
            BackupServer:set $::selectedJob(id) "overwrite-archive-files" $::selectedJob(overwriteArchiveFilesFlag)
          }

          grid columnconfigure .jobs.storage.destination.fileSystem.options { 0 } -weight 1
        grid .jobs.storage.destination.fileSystem.options -row 0 -column 1 -sticky "we"

        grid columnconfigure .jobs.storage.destination.fileSystem { 1 } -weight 1

      labelframe .jobs.storage.destination.ftp
        label .jobs.storage.destination.ftp.loginTitle -text "Login:"
        grid .jobs.storage.destination.ftp.loginTitle -row 0 -column 0 -sticky "w" 
        frame .jobs.storage.destination.ftp.login
          entry .jobs.storage.destination.ftp.login.name -textvariable ::selectedJob(storageLoginName) -bg white
          pack .jobs.storage.destination.ftp.login.name -side left -fill x -expand yes
          addModifyTrace {::selectedJob(storageLoginName)} \
          {
            if {$::selectedJob(storageType) != ""} { BackupServer:set $::selectedJob(id) "archive-name" [getArchiveName $::selectedJob(storageType) $::selectedJob(storageFileName) $::selectedJob(storageLoginName) $::selectedJob(storageHostName) $::selectedJob(storageDeviceName)] }
          }

      #    label .jobs.storage.destination.ftp.loginPasswordTitle -text "Password:"
      #    pack .jobs.storage.destination.ftp.loginPasswordTitle -row 0 -column 2 -sticky "w" 
      #    entry .jobs.storage.destination.ftp.loginPassword -textvariable barConfig(sshPassword) -bg white -show "*"
      #    pack .jobs.storage.destination.ftp.loginPassword -row 0 -column 3 -sticky "we" 

          label .jobs.storage.destination.ftp.login.hostNameTitle -text "Host:"
          pack .jobs.storage.destination.ftp.login.hostNameTitle -side left
          entry .jobs.storage.destination.ftp.login.hostName -textvariable ::selectedJob(storageHostName) -bg white
          pack .jobs.storage.destination.ftp.login.hostName -side left -fill x -expand yes
          addModifyTrace {::selectedJob(storageHostName)} \
          {
            if {$::selectedJob(storageType) != ""} { BackupServer:set $::selectedJob(id) "archive-name" [getArchiveName $::selectedJob(storageType) $::selectedJob(storageFileName) $::selectedJob(storageLoginName) $::selectedJob(storageHostName) $::selectedJob(storageDeviceName)] }
          }
        grid .jobs.storage.destination.ftp.login -row 0 -column 1 -sticky "we"

        label .jobs.storage.destination.ftp.maxBandWidthTitle -text "Max. band width:"
        grid .jobs.storage.destination.ftp.maxBandWidthTitle -row 3 -column 0 -sticky "w" 
        frame .jobs.storage.destination.ftp.maxBandWidth
          radiobutton .jobs.storage.destination.ftp.maxBandWidth.unlimited -text "unlimited" -anchor w -variable ::selectedJob(maxBandWidthFlag) -value 0
          grid .jobs.storage.destination.ftp.maxBandWidth.unlimited -row 0 -column 1 -sticky "w" 
          radiobutton .jobs.storage.destination.ftp.maxBandWidth.limitto -text "limit to" -width 8 -anchor w -variable ::selectedJob(maxBandWidthFlag) -value 1
          grid .jobs.storage.destination.ftp.maxBandWidth.limitto -row 0 -column 2 -sticky "w" 
          tixComboBox .jobs.storage.destination.ftp.maxBandWidth.size -variable ::selectedJob(maxBandWidth) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
          grid .jobs.storage.destination.ftp.maxBandWidth.size -row 0 -column 3 -sticky "w" 
          label .jobs.storage.destination.ftp.maxBandWidth.unit -text "bits/s"
          grid .jobs.storage.destination.ftp.maxBandWidth.unit -row 0 -column 4 -sticky "w" 

         .jobs.storage.destination.ftp.maxBandWidth.size insert end 64K
         .jobs.storage.destination.ftp.maxBandWidth.size insert end 128K
         .jobs.storage.destination.ftp.maxBandWidth.size insert end 256K
         .jobs.storage.destination.ftp.maxBandWidth.size insert end 512K

          grid rowconfigure    .jobs.storage.destination.ftp.maxBandWidth { 0 } -weight 1
          grid columnconfigure .jobs.storage.destination.ftp.maxBandWidth { 1 } -weight 1
        grid .jobs.storage.destination.ftp.maxBandWidth -row 3 -column 1 -sticky "w" -padx 2p -pady 2p
        addEnableTrace ::barConfig(maxBandWidthFlag) 1 .jobs.storage.destination.ftp.maxBandWidth.size
        addModifyTrace {::selectedJob(maxBandWidthFlag) ::selectedJob(maxBandWidth)} \
        {
#puts "???"
#          BackupServer:set $::selectedJob(id) "ssh-private-key" $::selectedJob(sshPrivateKeyFileName)
        }

  #      grid rowconfigure    .jobs.storage.destination.ftp { } -weight 1
        grid columnconfigure .jobs.storage.destination.ftp { 1 } -weight 1

      labelframe .jobs.storage.destination.ssh
        label .jobs.storage.destination.ssh.loginTitle -text "Login:"
        grid .jobs.storage.destination.ssh.loginTitle -row 0 -column 0 -sticky "w" 
        frame .jobs.storage.destination.ssh.login
          entry .jobs.storage.destination.ssh.login.name -textvariable ::selectedJob(storageLoginName) -bg white
          pack .jobs.storage.destination.ssh.login.name -side left -fill x -expand yes
          addModifyTrace {::selectedJob(storageLoginName)} \
          {
            if {$::selectedJob(storageType) != ""} { BackupServer:set $::selectedJob(id) "archive-name" [getArchiveName $::selectedJob(storageType) $::selectedJob(storageFileName) $::selectedJob(storageLoginName) $::selectedJob(storageHostName) $::selectedJob(storageDeviceName)] }
          }

      #    label .jobs.storage.destination.ssh.loginPasswordTitle -text "Password:"
      #    pack .jobs.storage.destination.ssh.loginPasswordTitle -row 0 -column 2 -sticky "w" 
      #    entry .jobs.storage.destination.ssh.loginPassword -textvariable barConfig(sshPassword) -bg white -show "*"
      #    pack .jobs.storage.destination.ssh.loginPassword -row 0 -column 3 -sticky "we" 

          label .jobs.storage.destination.ssh.login.hostNameTitle -text "Host:"
          pack .jobs.storage.destination.ssh.login.hostNameTitle -side left
          entry .jobs.storage.destination.ssh.login.hostName -textvariable ::selectedJob(storageHostName) -bg white
          pack .jobs.storage.destination.ssh.login.hostName -side left -fill x -expand yes
          addModifyTrace {::selectedJob(storageHostName)} \
          {
            if {$::selectedJob(storageType) != ""} { BackupServer:set $::selectedJob(id) "archive-name" [getArchiveName $::selectedJob(storageType) $::selectedJob(storageFileName) $::selectedJob(storageLoginName) $::selectedJob(storageHostName) $::selectedJob(storageDeviceName)] }
          }

          label .jobs.storage.destination.ssh.login.sshPortTitle -text "SSH port:"
          pack .jobs.storage.destination.ssh.login.sshPortTitle -side left
          tixControl .jobs.storage.destination.ssh.login.sshPort -variable ::selectedJob(sshPort) -label "" -labelside right -integer true -min 0 -max 65535 -options { entry.background white }
          pack .jobs.storage.destination.ssh.login.sshPort -side left -fill x -expand yes
          addModifyTrace {::selectedJob(sshPort)} \
          {
            BackupServer:set $::selectedJob(id) "ssh-port" $::selectedJob(sshPort)
          }
        grid .jobs.storage.destination.ssh.login -row 0 -column 1 -sticky "we"

        label .jobs.storage.destination.ssh.sshPublicKeyFileNameTitle -text "SSH public key:"
        grid .jobs.storage.destination.ssh.sshPublicKeyFileNameTitle -row 1 -column 0 -sticky "w" 
        frame .jobs.storage.destination.ssh.sshPublicKeyFileName
          entry .jobs.storage.destination.ssh.sshPublicKeyFileName.data -textvariable ::selectedJob(sshPublicKeyFileName) -bg white
          pack .jobs.storage.destination.ssh.sshPublicKeyFileName.data -side left -fill x -expand yes
          button .jobs.storage.destination.ssh.sshPublicKeyFileName.select -image $images(folder) -command \
          "
            set fileName \[Dialog:fileSelector \"Select SSH public key file\" \$::selectedJob(sshPublicKeyFileName) {{\"*.pub\" \"Public key\"} {\"*\" \"all\"}}\]
            if {\$fileName != \"\"} \
            {
              set ::selectedJob(sshPublicKeyFileName) \$fileName
            }
          "
          pack .jobs.storage.destination.ssh.sshPublicKeyFileName.select -side left
        grid .jobs.storage.destination.ssh.sshPublicKeyFileName -row 1 -column 1 -sticky "we"
        addModifyTrace {::selectedJob(sshPublicKeyFileName)} \
        {
          BackupServer:set $::selectedJob(id) "ssh-public-key" $::selectedJob(sshPublicKeyFileName)
        }

        label .jobs.storage.destination.ssh.sshPrivateKeyFileNameTitle -text "SSH privat key:"
        grid .jobs.storage.destination.ssh.sshPrivateKeyFileNameTitle -row 2 -column 0 -sticky "w" 
        frame .jobs.storage.destination.ssh.sshPrivateKeyFileName
          entry .jobs.storage.destination.ssh.sshPrivateKeyFileName.data -textvariable ::selectedJob(sshPrivateKeyFileName) -bg white
          pack .jobs.storage.destination.ssh.sshPrivateKeyFileName.data -side left -fill x -expand yes
          button .jobs.storage.destination.ssh.sshPrivateKeyFileName.select -image $images(folder) -command \
          "
            set fileName \[Dialog:fileSelector \"Select SSH privat key file\" \$::selectedJob(sshPrivateKeyFileName) {{\"*\" \"all\"}}\]
            if {\$fileName != \"\"} \
            {
              set ::selectedJob(sshPrivateKeyFileName) \$fileName
            }
          "
          pack .jobs.storage.destination.ssh.sshPrivateKeyFileName.select -side left
        grid .jobs.storage.destination.ssh.sshPrivateKeyFileName -row 2 -column 1 -sticky "we"
        addModifyTrace {::selectedJob(sshPrivateKeyFileName)} \
        {
          BackupServer:set $::selectedJob(id) "ssh-private-key" $::selectedJob(sshPrivateKeyFileName)
        }

        label .jobs.storage.destination.ssh.maxBandWidthTitle -text "Max. band width:"
        grid .jobs.storage.destination.ssh.maxBandWidthTitle -row 3 -column 0 -sticky "w" 
        frame .jobs.storage.destination.ssh.maxBandWidth
          radiobutton .jobs.storage.destination.ssh.maxBandWidth.unlimited -text "unlimited" -anchor w -variable ::selectedJob(maxBandWidthFlag) -value 0
          grid .jobs.storage.destination.ssh.maxBandWidth.unlimited -row 0 -column 1 -sticky "w" 
          radiobutton .jobs.storage.destination.ssh.maxBandWidth.limitto -text "limit to" -width 8 -anchor w -variable ::selectedJob(maxBandWidthFlag) -value 1
          grid .jobs.storage.destination.ssh.maxBandWidth.limitto -row 0 -column 2 -sticky "w" 
          tixComboBox .jobs.storage.destination.ssh.maxBandWidth.size -variable ::selectedJob(maxBandWidth) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
          grid .jobs.storage.destination.ssh.maxBandWidth.size -row 0 -column 3 -sticky "w" 
          label .jobs.storage.destination.ssh.maxBandWidth.unit -text "bits/s"
          grid .jobs.storage.destination.ssh.maxBandWidth.unit -row 0 -column 4 -sticky "w" 

         .jobs.storage.destination.ssh.maxBandWidth.size insert end 64K
         .jobs.storage.destination.ssh.maxBandWidth.size insert end 128K
         .jobs.storage.destination.ssh.maxBandWidth.size insert end 256K
         .jobs.storage.destination.ssh.maxBandWidth.size insert end 512K

          grid rowconfigure    .jobs.storage.destination.ssh.maxBandWidth { 0 } -weight 1
          grid columnconfigure .jobs.storage.destination.ssh.maxBandWidth { 1 } -weight 1
        grid .jobs.storage.destination.ssh.maxBandWidth -row 3 -column 1 -sticky "w" -padx 2p -pady 2p
        addEnableTrace ::barConfig(maxBandWidthFlag) 1 .jobs.storage.destination.ssh.maxBandWidth.size
        addModifyTrace {::selectedJob(maxBandWidthFlag) ::selectedJob(maxBandWidth)} \
        {
#puts "???"
#          BackupServer:set $::selectedJob(id) "ssh-private-key" $::selectedJob(sshPrivateKeyFileName)
        }

  #      grid rowconfigure    .jobs.storage.destination.ssh { } -weight 1
        grid columnconfigure .jobs.storage.destination.ssh { 1 } -weight 1

      labelframe .jobs.storage.destination.dvd
        label .jobs.storage.destination.dvd.deviceNameTitle -text "DVD device:"
        grid .jobs.storage.destination.dvd.deviceNameTitle -row 0 -column 0 -sticky "w" 
        entry .jobs.storage.destination.dvd.deviceName -textvariable ::selectedJob(storageDeviceName) -bg white
        grid .jobs.storage.destination.dvd.deviceName -row 0 -column 1 -sticky "we" 
        addModifyTrace {::selectedJob(storageDeviceName)} \
        {
          if {$::selectedJob(storageType) != ""} { BackupServer:set $::selectedJob(id) "archive-name" [getArchiveName $::selectedJob(storageType) $::selectedJob(storageFileName) $::selectedJob(storageLoginName) $::selectedJob(storageHostName) $::selectedJob(storageDeviceName)] }
        }

        label .jobs.storage.destination.dvd.volumeSizeTitle -text "Size:"
        grid .jobs.storage.destination.dvd.volumeSizeTitle -row 1 -column 0 -sticky "w"
        frame .jobs.storage.destination.dvd.volumeSize
          tixComboBox .jobs.storage.destination.dvd.volumeSize.size -variable ::selectedJob(volumeSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
          pack .jobs.storage.destination.dvd.volumeSize.size -side left
          label .jobs.storage.destination.dvd.volumeSize.unit -text "bytes"
          pack .jobs.storage.destination.dvd.volumeSize.unit -side left
        grid .jobs.storage.destination.dvd.volumeSize -row 1 -column 1 -sticky "w"
        addModifyTrace {::selectedJob(volumeSize)} \
        {
          BackupServer:set $::selectedJob(id) "volume-size" $::selectedJob(volumeSize)
        }

        label .jobs.storage.destination.dvd.optionsTitle -text "Options:"
        grid .jobs.storage.destination.dvd.optionsTitle -row 2 -column 0 -sticky "w"
        frame .jobs.storage.destination.dvd.options
          checkbutton .jobs.storage.destination.dvd.options.ecc -text "add error-correction codes" -variable ::selectedJob(errorCorrectionCodesFlag)
          grid .jobs.storage.destination.dvd.options.ecc -row 0 -column 0 -sticky "w" 
          addModifyTrace {::selectedJob(errorCorrectionCodesFlag)} \
          {
            BackupServer:set $::selectedJob(id) "ecc" $::selectedJob(errorCorrectionCodesFlag)
          }

          grid columnconfigure .jobs.storage.destination.dvd.options { 0 } -weight 1
        grid .jobs.storage.destination.dvd.options -row 2 -column 1 -sticky "we"

       .jobs.storage.destination.dvd.volumeSize.size insert end 2G
       .jobs.storage.destination.dvd.volumeSize.size insert end 3G
       .jobs.storage.destination.dvd.volumeSize.size insert end 3.6G
       .jobs.storage.destination.dvd.volumeSize.size insert end 4G

        grid rowconfigure    .jobs.storage.destination.dvd { 4 } -weight 1
        grid columnconfigure .jobs.storage.destination.dvd { 1 } -weight 1

      labelframe .jobs.storage.destination.device
        label .jobs.storage.destination.device.nameTitle -text "Device name:"
        grid .jobs.storage.destination.device.nameTitle -row 0 -column 0 -sticky "w" 
        entry .jobs.storage.destination.device.name -textvariable ::selectedJob(storageDeviceName) -bg white
        grid .jobs.storage.destination.device.name -row 0 -column 1 -sticky "we" 
        addModifyTrace {::selectedJob(storageDeviceName)} \
        {
          if {$::selectedJob(storageType) != ""} { BackupServer:set $::selectedJob(id) "archive-name" [getArchiveName $::selectedJob(storageType) $::selectedJob(storageFileName) $::selectedJob(storageLoginName) $::selectedJob(storageHostName) $::selectedJob(storageDeviceName)] }
        }

        label .jobs.storage.destination.device.volumeSizeTitle -text "Size:"
        grid .jobs.storage.destination.device.volumeSizeTitle -row 1 -column 0 -sticky "w"
        frame .jobs.storage.destination.device.volumeSize
          tixComboBox .jobs.storage.destination.device.volumeSize.size -variable ::selectedJob(volumeSize) -label "" -labelside right -editable true -options { entry.width 6 entry.background white entry.justify right }
          pack .jobs.storage.destination.device.volumeSize.size -side left
          label .jobs.storage.destination.device.volumeSize.unit -text "bytes"
          pack .jobs.storage.destination.device.volumeSize.unit -side left
        grid .jobs.storage.destination.device.volumeSize -row 1 -column 1 -sticky "w"
        addModifyTrace {::selectedJob(volumeSize)} \
        {
          BackupServer:set $::selectedJob(id) "volume-size" $::selectedJob(volumeSize)
        }

       .jobs.storage.destination.device.volumeSize.size insert end 2G
       .jobs.storage.destination.device.volumeSize.size insert end 3G
       .jobs.storage.destination.device.volumeSize.size insert end 3.6G
       .jobs.storage.destination.device.volumeSize.size insert end 4G

        grid rowconfigure    .jobs.storage.destination.device { 3 } -weight 1
        grid columnconfigure .jobs.storage.destination.device { 1 } -weight 1

      grid rowconfigure    .jobs.storage.destination { 0 } -weight 1
      grid columnconfigure .jobs.storage.destination { 0 } -weight 1
    grid .jobs.storage.destination -row 7 -column 1 -sticky "we"
    addModifyTrace {::selectedJob(storageType)} \
    {
      if {$::selectedJob(storageType)=="filesystem"} \
      {
        grid .jobs.storage.destination.fileSystem -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
      } \
      else \
      {
        grid forget .jobs.storage.destination.fileSystem
      }

      if {$::selectedJob(storageType) == "ftp"} \
      {
        grid .jobs.storage.destination.ftp -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
      } \
      else \
      {
        grid forget .jobs.storage.destination.ftp
      }

      if {($::selectedJob(storageType) == "scp") || ($::selectedJob(storageType) == "sftp")} \
      {
        grid .jobs.storage.destination.ssh -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
      } \
      else \
      {
        grid forget .jobs.storage.destination.ssh
      }

      if {$::selectedJob(storageType)=="dvd"} \
      {
        grid .jobs.storage.destination.dvd -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p
      } \
      else \
      {
        grid forget .jobs.storage.destination.dvd
      }

      if {$::selectedJob(storageType)=="device"} \
      {
        grid .jobs.storage.destination.device -row 7 -column 0 -sticky "nswe" -padx 2p -pady 2p
      } \
      else \
      {
        grid forget .jobs.storage.destination.device
      }
    }

    grid rowconfigure    .jobs.storage { 8 } -weight 1
    grid columnconfigure .jobs.storage { 1 } -weight 1
  pack .jobs.storage -side top -fill both -expand yes -in [.jobs.tabs subwidget storage]

  frame .jobs.schedule
    mclistbox::mclistbox .jobs.schedule.list \
      -height 1 \
      -fillcolumn type \
      -bg white \
      -labelanchor w \
      -selectmode single \
      -exportselection 0 \
      -xscrollcommand ".jobs.schedule.xscroll set" \
      -yscrollcommand ".jobs.schedule.yscroll set"
    .jobs.schedule.list column add date    -label "Date"     -width 20
    .jobs.schedule.list column add weekday -label "Week day" -width 14
    .jobs.schedule.list column add time    -label "Time"     -width 20
    .jobs.schedule.list column add type    -label "Type"     -width 20
    grid .jobs.schedule.list -row 0 -column 0 -sticky "nswe"
    scrollbar .jobs.schedule.yscroll -orient vertical -command ".jobs.schedule.list yview"
    grid .jobs.schedule.yscroll -row 0 -column 1 -sticky "ns"
    scrollbar .jobs.schedule.xscroll -orient horizontal -command ".jobs.schedule.list xview"
    grid .jobs.schedule.xscroll -row 1 -column 0 -sticky "we"
    bind .jobs.schedule.list <Button-1> ".jobs.schedule.buttons.edit configure -state normal; .jobs.schedule.buttons.rem configure -state normal"
    bind .jobs.schedule.list <Insert> "event generate . <<Event_backupAddSchedule>>"
    bind .jobs.schedule.list <Double-Button-1> "event generate . <<Event_backupEditSchedule>>"
    bind .jobs.schedule.list <Delete> "event generate . <<Event_backupRemSchedule>>"
    set scheduleListWidget .jobs.schedule.list
    addModifyTrace {::selectedJob(schedule)} \
    {
      .jobs.schedule.list delete 0 end
      foreach schedule $::selectedJob(schedule) \
      {
        .jobs.schedule.list insert end $schedule
      }
    }

    frame .jobs.schedule.buttons
      button .jobs.schedule.buttons.add -text "Add" -command "event generate . <<Event_backupAddSchedule>>"
      pack .jobs.schedule.buttons.add -side left
      button .jobs.schedule.buttons.edit -text "Edit" -state disabled -command "event generate . <<Event_backupEditSchedule>>"
      pack .jobs.schedule.buttons.edit -side left
      button .jobs.schedule.buttons.rem -text "Rem" -state disabled -command "event generate . <<Event_backupRemSchedule>>"
      pack .jobs.schedule.buttons.rem -side left
    grid .jobs.schedule.buttons -row 2 -column 0 -sticky "we" -padx 2p -pady 2p

#    label .jobs.schedule.optionsTitle -text "Options:"
#    grid .jobs.schedule.optionsTitle -row 4 -column 0 -sticky "nw" 
#    checkbutton .jobs.schedule.optionSkipUnreadable -text "skip unreable files" -variable ::selectedJob(skipUnreadableFlag)
#    grid .jobs.schedule.optionSkipUnreadable -row 4 -column 1 -sticky "nw" 
#    addModifyTrace {::selectedJob(skipUnreadableFlag)} \
#    {
#      BackupServer:set $::selectedJob(id) "skip-unreadable" $::selectedJob(skipUnreadableFlag)
#    }

    grid rowconfigure    .jobs.schedule { 0 } -weight 1
    grid columnconfigure .jobs.schedule { 0 } -weight 1
  pack .jobs.schedule -side top -fill both -expand yes -in [.jobs.tabs subwidget schedule]

  grid rowconfigure    .jobs { 1 } -weight 1
  grid columnconfigure .jobs { 1 } -weight 1
pack .jobs -side top -fill both -expand yes -in [$mainWindow.tabs subwidget backup]

update

# ----------------------------------------------------------------------

$::filesTreeWidget configure -command "openCloseBackupDirectory"
#$restoreFilesTreeWidget configure -command "openCloseRestoreDirectory"

addModifyTrace {::status} \
  "
    global status

    if {\$status == \"pause\"} \
    {
      .status.buttons.pausecontinue configure -text \"Continue\"
    } \
    else \
    {
      .status.buttons.pausecontinue configure -text \"Pause\"
    }
  "
addModifyTrace {::jobStatus(id) ::jobStatus(state) ::jobStatus(name)} \
  "
    global jobStatus

    if {\$jobStatus(id) != 0} \
    {
       .status.selected configure -text \"Selected '\$jobStatus(name)'\"
       if {(\$jobStatus(state) != \"waiting\") && (\$jobStatus(state) != \"running\")} \
       {
         .status.buttons.start configure -state normal
       } \
       else \
       {
         .status.buttons.start configure -state disabled
       }
       if {(\$jobStatus(state) == \"waiting\") || (\$jobStatus(state) == \"running\")} \
       {
         .status.buttons.abort configure -state normal
       } \
       else \
       {
         .status.buttons.abort configure -state disabled
       }
    } \
    else \
    {
       .status.selected configure -text \"Selected\"
       .status.buttons.start configure -state disabled
       .status.buttons.abort configure -state disabled
    }
  "
addModifyTrace ::jobStatus(fileDoneBytes) \
  "
    global jobStatus

    if {\$jobStatus(fileTotalBytes) > 0} \
    {
      set p \[expr {double(\$jobStatus(fileDoneBytes))/\$jobStatus(fileTotalBytes)}]
      Dialog:progressbar .status.selected.filePercentage update \$p
    }
  "
addModifyTrace ::jobStatus(fileTotalBytes) \
  "
    global jobStatus

    if {\$jobStatus(fileTotalBytes) > 0} \
    {
      set p \[expr {double(\$jobStatus(fileDoneBytes))/\$jobStatus(fileTotalBytes)}]
      Dialog:progressbar .status.selected.filePercentage update \$p
    }
  "
addModifyTrace ::jobStatus(storageDoneBytes) \
  "
    global jobStatus

    if {\$jobStatus(storageTotalBytes) > 0} \
    {
      set p \[expr {double(\$jobStatus(storageDoneBytes))/\$jobStatus(storageTotalBytes)}]
      Dialog:progressbar .status.selected.storagePercentage update \$p
    }
  "
addModifyTrace ::jobStatus(doneBytes) \
  "
    global jobStatus

    if {\$jobStatus(totalBytes) > 0} \
    {
      set p \[expr {double(\$jobStatus(doneBytes))/\$jobStatus(totalBytes)}]
      Dialog:progressbar .status.selected.totalBytesPercentage update \$p
    }
  "
addModifyTrace ::jobStatus(doneFiles) \
  "
    global jobStatus

    if {\$jobStatus(totalFiles) > 0} \
    {
      set p \[expr {double(\$jobStatus(doneFiles))/\$jobStatus(totalFiles)}]
      Dialog:progressbar .status.selected.totalFilesPercentage update \$p
    }
  "
addModifyTrace ::jobStatus(storageTotalBytes) \
  "
    global jobStatus

    if {\$jobStatus(storageTotalBytes) > 0} \
    {
      set p \[expr {double(\$jobStatus(storageDoneBytes))/\$jobStatus(storageTotalBytes)}]
      Dialog:progressbar .status.selected.storagePercentage update \$p
    }
  "
addModifyTrace ::jobStatus(volumeProgress) \
  "
    global jobStatus

    Dialog:progressbar .status.selected.storageVolume update \$::jobStatus(volumeProgress)
  "
addModifyTrace ::jobStatus(totalFiles) \
  "
    global jobStatus

    if {\$jobStatus(totalFiles) > 0} \
    {
      set p \[expr {double(\$jobStatus(doneFiles))/\$jobStatus(totalFiles)}]
      Dialog:progressbar .status.selected.totalFilesPercentage update \$p
    }
  "
addModifyTrace ::jobStatus(totalBytes) \
  "
    global jobStatus

    if {\$jobStatus(totalBytes) > 0} \
    {
      set p \[expr {double(\$jobStatus(doneBytes))/\$jobStatus(totalBytes)}]
      Dialog:progressbar .status.selected.totalBytesPercentage update \$p
    }
  "

addModifyTrace {::selectedJob(id)} \
{
  # note tab disble does not work => remove/add whole tab-set
  if {$::selectedJob(id) != 0} \
  {
     grid .jobs.tabs -row 1 -column 0 -columnspan 2 -sticky "nswe" -padx 2p -pady 2p
     .jobs.list.delete configure -state normal
  } \
  else \
  {
     grid remove .jobs.tabs
     .jobs.list.delete configure -state disabled
  }
}

addModifyTrace {::jobStatus(requestedVolumeNumber)} \
  {
    if {($::jobStatus(volumeNumber) != $::jobStatus(requestedVolumeNumber)) && ($::jobStatus(requestedVolumeNumber) > 0)} \
    {
if {0} {
      switch [Dialog:select "Request" "Please insert volume #$::jobStatus(requestedVolumeNumber) into drive." "" [list [list "Continue"] [list "Abort job"] [list "Cancel" Escape]]]
      {
        0 \
        {
          set errorCode 0
          set errorText ""
          BackupServer:executeCommand errorCode errorText "VOLUME $::jobStatus(id) $::jobStatus(requestedVolumeNumber)"
        }
        1 \
        {
          set errorCode 0
          set errorText ""
          BackupServer:executeCommand errorCode errorText "ABORT_JOB $::jobStatus(id)"
        }
        2 \
        {
        }
      }
}
      .status.buttons.volume configure -state normal
      set ::jobStatus(message) "Please insert volume #$::jobStatus(requestedVolumeNumber) into drive."
    } \
    else \
    {
      .status.buttons.volume configure -state disabled
      set ::jobStatus(message) ""
    }
  }

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
  }
}

bind . <<Event_quit>> \
{
  quit
}

bind . <<Event_jobNew>> \
{
  set id [jobNew]
  if {$id != ""} \
  {
    updateJobList .jobs.list.data
    set selectedJob(id) $id
  }
}

bind . <<Event_jobDelete>> \
{
  if {$selectedJob(id) != 0} \
  {
    if {[Dialog:confirm "Delete job?" "Delete" "Cancel"]} \
    {
      jobDelete $selectedJob(id)
      clearJob
      updateJobList .jobs.list.data
    }
  }
 }

bind . <<Event_selectJobStatus>> \
{
  set n [.status.list.data curselection]
  if {$n != {}} \
  {
    set entry [lindex [.status.list.data get $n $n] 0]
    set jobStatus(id)    [lindex $entry 0]
    set jobStatus(name)  [lindex $entry 1]
    set jobStatus(state) [lindex $entry 2]
  }
}

bind . <<Event_jobStart>> \
{
  set n [.status.list.data curselection]
  if {$n != {}} \
  {
    set entry [lindex [.status.list.data get $n $n] 0]
    switch [Dialog:select "Start job" "Start job '[lindex $entry 1]'?" "" {"Full" "Incremental" "Cancel"}] \
    {
      0 \
      {
        set id [lindex $entry 0]
        jobStart $id "full"
        updateStatusList .status.list.data
      }
      1 \
      {
        set id [lindex $entry 0]
        jobStart $id "incremental"
        updateStatusList .status.list.data
      }
      2 \
      {
      }
    }
  }
}

bind . <<Event_jobAbort>> \
{
  set n [.status.list.data curselection]
  if {$n != {}} \
  {
    set entry [lindex [.status.list.data get $n $n] 0]
    if {[Dialog:confirm "Really abort job '[lindex $entry 1]'?" "Abort job" "Cancel"]} \
    {
      set id [lindex $entry 0]
      jobAbort $id
      updateStatusList .status.list.data
    }
  }
}

bind . <<Event_jobPauseContinue>> \
{
  set errorCode 0
  set errorText ""
  if {$status eq "pause"} \
  {
    BackupServer:executeCommand errorCode errorText "CONTINUE"
  } \
  else \
  {
    BackupServer:executeCommand errorCode errorText "PAUSE"
  }
}

bind . <<Event_backupStateNone>> \
{
  foreach itemPath [$::filesTreeWidget info selection] \
  {
    setEntryState $::filesTreeWidget $itemPath 0 "NONE"
  }
}

bind . <<Event_backupStateIncluded>> \
{
  foreach itemPath [$::filesTreeWidget info selection] \
  {
    setEntryState $::filesTreeWidget $itemPath 0 "INCLUDED"
  }
}

bind . <<Event_backupStateExcluded>> \
{
  foreach itemPath [$::filesTreeWidget info selection] \
  {
    setEntryState $::filesTreeWidget $itemPath 0 "EXCLUDED"
  }
}

bind . <<Event_backupToggleStateNoneIncludedExcluded>> \
{
  foreach itemPath [$::filesTreeWidget info selection] \
  {
    toggleEntryIncludedExcluded $::filesTreeWidget $itemPath 0
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
  foreach index [$::includedListWidget curselection] \
  {
    lappend patternList [$::includedListWidget get $index]
  }
  foreach pattern $patternList \
  {
    remIncludedPattern $pattern
  }
  $::includedListWidget selection clear 0 end
  .jobs.filters.includedButtons.rem configure -state disabled
}

bind . <<Event_backupAddExcludePattern>> \
{
  addExcludedPattern ""
}

bind . <<Event_backupRemExcludePattern>> \
{
  set patternList {}
  foreach index [$excludedListWidget curselection] \
  {
    lappend patternList [$excludedListWidget get $index]
  }
  foreach pattern $patternList \
  {
    remExcludedPattern $pattern
  }
  $excludedListWidget selection clear 0 end
  .jobs.filters.excludedButtons.rem configure -state disabled
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

bind . <<Event_backupAddSchedule>> \
{
  addSchedule
}

bind . <<Event_backupEditSchedule>> \
{
  set index [lindex [$scheduleListWidget curselection] 0]
  if {$index != ""} \
  {
    editSchedule $index
  }
}

bind . <<Event_backupRemSchedule>> \
{
  foreach index [$scheduleListWidget curselection] \
  {
    remSchedule $index
  }
  $scheduleListWidget selection clear 0 end
  .jobs.schedule.buttons.edit configure -state disabled
  .jobs.schedule.buttons.rem configure -state disabled
}

bind . <<Event_volume>> \
{
  set errorCode 0
  set errorText ""
  BackupServer:executeCommand errorCode errorText "VOLUME $::jobStatus(id) $::jobStatus(requestedVolumeNumber)"
}

if {$debugFlag} \
{
  bind . <<Event_debugMemoryInfo>> \
  {
    set errorCode 0
    set errorText ""
    BackupServer:executeCommand errorCode errorText "DEBUG_MEMORY_INFO"
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

if {1} {
updateStatusList .status.list.data
updateStatus
}
updateJobList .jobs.list.data

# read devices
#set commandId [BackupServer:sendCommand "DEVICE_LIST"]
#while {[BackupServer:readResult $commandId completeFlag errorCode result]} \
#{
#  addBackupDevice $result
#}
addBackupDevice "/"

# load config if given
if {$configFileName != ""} \
{
  if {[file exists $configFileName]} \
  {
#    loadBARConfig $configFileName
puts "NYI"; exit 1;
    if {$startFlag} \
    {
      addBackupJob .status.list.data
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

#selectJob 1
#addSchedule
#Dialog:password "xxx" 0
#editStorageFileName "test-%type-%a-###.bar"
#Dialog:fileSelector "x" "/tmp/test.bnid" {}

set xx 1

# end of file
