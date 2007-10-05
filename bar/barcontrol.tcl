#!/bin/sh
#\
exec wish "$0" "$@"

# ----------------------------------------------------------------------------
#
# $Source: /home/torsten/cvs/bar/barcontrol.tcl,v $
# $Revision: 1.2 $
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

set server(socketHandle)  -1
set server(lastCommandId) 0

set configFilename ""

set barConfig(storageType)                    "FILESYSTEM"
set barConfig(storagePartSizeFlag)            0
set barConfig(storagePartSize)                0
set barConfig(storageMaxTmpDirectorySizeFlag) 0
set barConfig(storageMaxTmpDirectorySize)     0
set barConfig(storageFileName)                ""
set barConfig(storageLoginName)               ""
set barConfig(storageLoginPassword)           ""
set barConfig(storageHostName)                ""
set barConfig(storageSSHPort)                 22
set barConfig(compressAlgorithm)              "bzip9"
set barConfig(cryptAlgorithm)                 "none"
set barConfig(cryptPassword)                  ""

set currentJob(id)                            0
set currentJob(doneFiles)                     0
set currentJob(doneBytes)                      0
set currentJob(compressionRatio)              0
set currentJob(totalFiles)                    0
set currentJob(totalBytes)                     0
set currentJob(fileName)                      ""
set currentJob(storageName)                   ""

# --------------------------------- images -----------------------------------

set images(folder) [image create photo -data \
{
  R0lGODlhEAAMAKEAAAD//wAAAPD/gAAAACH5BAEAAAAALAAAAAAQAAwAAAIghINhyycvVFsB
  QtmS3rjaH1Hg141WaT5ouprt2HHcUgAAOw==
}]
set images(folder_open) [image create photo -data \
{
  R0lGODlhEAAMAKEAAAD//wAAAP//AFtXRiH5BAEAAAAALAAAAAAQAAwAAAIrhINhyycvVFsB
  QtmS3jgM8YUg2I3c+VXWGlWDyj6KB8cRUNXxrK/IcdosCgA7
}]
set images(folder_excluded) [image create photo -data \
{
  R0lGODdhEAAMAKEAAP8AAAD//wAAAPD/gCwAAAAAEAAMAAACMQQiKcFhbcCYY7kHDa2xIREp
  oYVM0XBWjUBF2kSypkvFs5TaKarF+036tTAXhTFBLAAAOw==
}]
set images(file) [image create photo -data \
{
  R0lGODlhDAAMAKEAAAD//wAAAP//8wAAACH5BAEAAAAALAAAAAAMAAwAAAIdRI4Ha+IfWHsO
  rUBpnFls3HlXKHLZR6Kh2n3JCxQAOw==
}]
set images(file_excluded) [image create photo -data \
{
  R0lGODdhDAAMAKEAAP8AAAAAAAD/////8ywAAAAADAAMAAACKASCGWI9/0JjBo4pZL1mZd4A
  gxc8YSR8oDimZViRDujNpkGxFqwoRgEAOw==
}]
set images(link) [image create photo -data \
{
  R0lGODdhDAAMAKEAAAD//wAAAP//8////ywAAAAADAAMAAACJUSOB2viH1gbx6EVBK0W69RN
  SRaJYJltxyCg3xqmcPzULTYmQAEAOw==
}]
set images(link_excluded) [image create photo -data \
{
  R0lGODdhDAAMAMIAAP8AAAAAAAD/////8////wAAAAAAAAAAACwAAAAADAAMAAADLggQ3CEq
  jjlDhIrNK2z2i9ANkfSMRBkM55pGa0uWACuCDCzCATEDmBplo3I0FAkAOw==
}]

# -------------------------------- functions ---------------------------------

proc Dialog:error { message } \
{
  puts "ERROR: $message\n"
}

proc Dialog:ok { message } \
{
  puts "OK: $message\n"
}

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
      place configure $path.back.fill -relwidth [lindex $args 1]
      update
    }
  } 
}

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

proc addEnableTrace { name conditionValue widget } \
{
  addEnableDisableTrace $name $conditionValue "$widget configure -state normal" "$widget configure -state disabled"
}
proc addDisableTrace { name conditionValue widget } \
{
  addEnableDisableTrace $name $conditionValue "$widget configure -state disabled" "$widget configure -state normal"
}

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
# Name   : Server:SendCommand
# Purpose: send command to server
# Input  : command - command
#          args    - arguments for command
# Output : -
# Return : command id
# Notes  : -
#***********************************************************************

proc Server:sendCommand { command args } \
{
  global server

  incr server(lastCommandId)

  set arguments [join $args]

  puts $server(socketHandle) "$command $server(lastCommandId) $arguments"; flush $server(socketHandle)

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

  gets $server(socketHandle) line
#puts "read: $line"

  set completeFlag 0
  set errorCode    -1
  set result       ""
  regexp {(\d+)\s+(\d+)\s+(\d+)\s+(.*)} $line * id completeFlag errorCode result
#puts "$completeFlag $errorCode $result"

  return [expr {!$completeFlag}]
}

# ----------------------------------------------------------------------

#***********************************************************************
# Name       : itemPathToFileName
# Purpose    : convert item path to file name
# Input      : widget   - widget
#              itemPath - item path
# Output     : -
# Return     : file name
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc itemPathToFileName { widget itemPath } \
 {
  set separator [lindex [$widget configure -separator] 4]
#puts "$itemPath -> #[string range $itemPath [string length $Separator] end]#"

  return [string map [list $separator "/"] $itemPath]
 }

#***********************************************************************
# Name       : fileNameToItemPath
# Purpose    : convert file name to item path
# Input      : widget   - widget
#              fileName - file name
# Output     : -
# Return     : item path
# Side-Effect: unknown
# Notes      : -
#***********************************************************************

proc fileNameToItemPath { widget fileName } \
 {
  # get separator
  set separator "/"
  catch {set separator [lindex [$widget configure -separator] 4]}

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

#***********************************************************************
# Name   : addDevice
# Purpose: add a device to tree-widget
# Input  : Widget     - widget
#          deviceName - file name/directory name
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addDevice { widget deviceName } \
{
  catch {$widget delete entry $deviceName}

  set n 0
  set l [$widget info children ""]
  while {($n < [llength $l]) && (([$widget info data [lindex $l $n]] != {}) || ($deviceName>[lindex $l $n]))} \
  {
    incr n
  }

  set style [tixDisplayStyle imagetext -refwindow $widget]

  $widget add $deviceName -at $n -itemtype imagetext -text $deviceName -image [tix getimage folder] -style $style -data [list "DIRECTORY" 0 0]
  $widget item create $deviceName 1 -itemtype imagetext -style $style
  $widget item create $deviceName 2 -itemtype imagetext -style $style
  $widget item create $deviceName 3 -itemtype imagetext -style $style
}

#***********************************************************************
# Name   : addEntry
# Purpose: add a file/directory/link entry to tree-widget
# Input  : Widget   - widget
#          fileName - file name/directory name
#          fileType - file status
#          fileSize - file size
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addEntry { widget fileName fileType fileSize } \
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
  set itemPath       [fileNameToItemPath $widget $fileName       ]
  set parentItemPath [fileNameToItemPath $widget $parentDirectory]
#puts "f=$fileName"
#puts "i=$itemPath"
#puts "p=$parentItemPath"

  catch {$widget delete entry $itemPath}

  # create parent entry if not exists
  if {($parentItemPath != "") && ![$widget info exists $parentItemPath]} \
  {
    addEntry $widget [file dirname $fileName] "DIRECTORY" 0
  }

  set styleImage     [tixDisplayStyle imagetext -refwindow $widget -anchor w]
  set styleTextLeft  [tixDisplayStyle text      -refwindow $widget -anchor w]
  set styleTextRight [tixDisplayStyle text      -refwindow $widget -anchor e]

   if     {$fileType=="FILE"} \
   {
     # add file item
#puts "add file $fileName $itemPath - $parentItemPath - $SortedFlag -- [file tail $fileName]"
     set n 0
     set l [$widget info children $parentItemPath]
     while {($n < [llength $l]) && (([$widget info data [lindex $l $n]] == "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     $widget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $images(file) -style $styleImage -data [list "FILE" 0]
     $widget item create $itemPath 1 -itemtype text -text "FILE"    -style $styleTextLeft
     $widget item create $itemPath 2 -itemtype text -text $fileSize -style $styleTextRight
     $widget item create $itemPath 3 -itemtype text -text 0         -style $styleTextLeft
   } \
   elseif {$fileType=="DIRECTORY"} \
   {
#puts "add directory $fileName"
     # add directory item
     set n 0
     set l [$widget info children $parentItemPath]
     while {($n < [llength $l]) && (([$widget info data [lindex $l $n]] != "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }


     $widget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $images(folder) -style $styleImage -data [list "DIRECTORY" 0 0]
     $widget item create $itemPath 1 -itemtype text -style $styleTextLeft
     $widget item create $itemPath 2 -itemtype text -style $styleTextLeft
     $widget item create $itemPath 3 -itemtype text -style $styleTextLeft
   } \
   elseif {$fileType=="LINK"} \
   {
     # add link item
#puts "add link $fileName $RealFilename"
     set n 0
     set l [$widget info children $parentItemPath]
     while {($n < [llength $l]) && (([$widget info data [lindex $l $n]] != "DIRECTORY") || ($itemPath > [lindex $l $n]))} \
     {
       incr n
     }

     set style [tixDisplayStyle imagetext -refwindow $widget]

     $widget add $itemPath -at $n -itemtype imagetext -text [file tail $fileName] -image $images(link) -style $styleImage -data [list "LINK" 0]
     $widget item create $itemPath 1 -itemtype text -text "LINK" -style $styleTextLeft
     $widget item create $itemPath 2 -itemtype text              -style $styleTextLeft
     $widget item create $itemPath 3 -itemtype text              -style $styleTextLeft
   }
}

#***********************************************************************
# Name   : openCloseDirectory
# Purpose: open/close directory
# Input  : widget   - tree widget
#          itemPath - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc openCloseDirectory { widget itemPath } \
{
  global images

  # get data
  set data [$widget info data $itemPath]

  if {[lindex $data 0] == "DIRECTORY"} \
  {
    # get open/closed flag
    set directoryOpenFlag [lindex $data 2]

    $widget delete offsprings $itemPath
    if {!$directoryOpenFlag} \
    {
      $widget item configure $itemPath 0 -image $images(folder_open)

      set fileName [itemPathToFileName $widget $itemPath]
      set commandId [Server:sendCommand "FILE_LIST" $fileName 0]
      while {[Server:readResult $commandId errorCode result]} \
      {
        if     {[regexp {^FILE\s(\d+)\s+(.*)} $result * fileSize fileName]} \
        {
          addEntry $widget $fileName "FILE" $fileSize
        } \
        elseif {[regexp {^DIRECTORY\s+(\d+)\s+(.*)} $result * totalSize directoryName]} \
        {
          addEntry $widget $directoryName "DIRECTORY" $totalSize
        } \
        elseif {[regexp {^LINK\s+(.*)} $result * linkName]} \
        {
          addEntry $widget $linkName "LINK" 0
        }
      }

      set directoryOpenFlag 1
    } \
    else \
    {
      $widget item configure $itemPath 0 -image $images(folder)

      set directoryOpenFlag 0
    }

    # update data
    set data [lreplace $data 2 2 $directoryOpenFlag]
    $widget entryconfigure $itemPath -data $data
  }
}

#***********************************************************************
# Name   : openCloseDirectory
# Purpose: include/exclude entry
# Input  : widget      - tree widget
#          itemPath    - item path
#          excludeFlag - 1 for exclude, 0 for include
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc includeExcludeEntry { widget itemPath excludeFlag } \
{
  global images

  # get data
  set data [$widget info data $itemPath]

  # get type, exclude flag
  set type [lindex $data 0]

  if {!$excludeFlag} \
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
        set image $images(folder_open)
      } \
      else \
      {
        set image $images(folder)
      }
    } \
    elseif {$type == "LINK"} \
    {
      set image $images(link)
    }
  } \
  else \
  {
    if     {$type == "FILE"} \
    {
      set image $images(file_excluded)
    } \
    elseif {$type == "DIRECTORY"} \
    {
      set image $images(folder_excluded)
      if {[lindex $data 2]} \
      {
        $widget delete offsprings $itemPath
        set data [lreplace $data 2 2 0]
      }
    } \
    elseif {$type == "LINK"} \
    {
      set image $images(link_excluded)
    }
  }
  $widget item configure $itemPath 0 -image $image

  # update data
  set data [lreplace $data 1 1 $excludeFlag]
  $widget entryconfigure $itemPath -data $data
}

#***********************************************************************
# Name   : toggleIncludeExcludeEntry
# Purpose: toggle include/exclude entry
# Input  : widget      - tree widget
#          itemPath    - item path
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc toggleIncludeExcludeEntry { widget itemPath } \
{
  # get data
  set data [$widget info data $itemPath]

  # get exclude flag
  set excludeFlag [lindex $data 1]

  includeExcludeEntry $widget $itemPath [expr {!$excludeFlag}]
}

#***********************************************************************
# Name   : addExcludePattern
# Purpose: add exclude pattern
# Input  : widget - exclude pattern list widget
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc addExcludePattern { widget } \
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
  $widget insert end $addExcludeDialog(pattern)
}

#***********************************************************************
# Name   : remExcludePattern
# Purpose: remove exclude pattern from widget list
# Input  : widget - exclude pattern list widget
#          index  - index
# Output : -
# Return : -
# Notes  : -
#***********************************************************************

proc remExcludePattern { widget index } \
{
  $widget delete $index
}

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
  $mainWindow.menu.edit.items add command -label "Include" -accelerator "+" -command "event generate . <<Event_include>>"
  $mainWindow.menu.edit.items add command -label "Exclude" -accelerator "-" -command "event generate . <<Event_exclude>>"
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
  labelframe .jobs.current -text "Current"
    label .jobs.current.idTitle -text "Id:"
    grid .jobs.current.idTitle -row 0 -column 0 -sticky "w"
    entry .jobs.current.id -width 5 -textvariable currentJob(id) -state readonly
    grid .jobs.current.id -row 0 -column 1 -sticky "w" -padx 2p -pady 2p

    label .jobs.current.doneTitle -text "Done:"
    grid .jobs.current.doneTitle -row 0 -column 2 -sticky "w" 
    frame .jobs.current.done
      entry .jobs.current.done.files -width 10 -textvariable currentJob(doneFiles) -justify right -state readonly
      grid .jobs.current.done.files -row 0 -column 0 -sticky "w" 
      label .jobs.current.done.filesPostfix -text "files"
      grid .jobs.current.done.filesPostfix -row 0 -column 1 -sticky "w" 

      entry .jobs.current.done.bytes -width 20 -textvariable currentJob(doneBytes) -justify right -state readonly
      grid .jobs.current.done.bytes -row 0 -column 2 -sticky "w" 
      label .jobs.current.done.bytesPostfix -text "bytes"
      grid .jobs.current.done.bytesPostfix -row 0 -column 3 -sticky "w" 

      label .jobs.current.done.compressRatioTitle -text "Ratio"
      grid .jobs.current.done.compressRatioTitle -row 0 -column 4 -sticky "w" 
      entry .jobs.current.done.compressRatio -width 5 -textvariable currentJob(compressionRatio) -justify right -state readonly
      grid .jobs.current.done.compressRatio -row 0 -column 5 -sticky "w" 
      label .jobs.current.done.compressRatioPostfix -text "%"
      grid .jobs.current.done.compressRatioPostfix -row 0 -column 6 -sticky "w" 

      grid rowconfigure    .jobs.current.done { 0 } -weight 1
      grid columnconfigure .jobs.current.done { 7 } -weight 1
    grid .jobs.current.done -row 0 -column 3 -sticky "we" -padx 2p -pady 2p

    label .jobs.current.totalTitle -text "Total:"
    grid .jobs.current.totalTitle -row 1 -column 2 -sticky "w" 
    frame .jobs.current.total
      entry .jobs.current.total.files -width 10 -textvariable currentJob(totalFiles) -justify right -state readonly
      grid .jobs.current.total.files -row 0 -column 0 -sticky "w" 
      label .jobs.current.total.filesPostfix -text "files"
      grid .jobs.current.total.filesPostfix -row 0 -column 1 -sticky "w" 

      entry .jobs.current.total.bytes -width 20 -textvariable currentJob(totalBytes) -justify right -state readonly
      grid .jobs.current.total.bytes -row 0 -column 2 -sticky "w" 
      label .jobs.current.total.bytesPostfix -text "bytes"
      grid .jobs.current.total.bytesPostfix -row 0 -column 3 -sticky "w" 

      grid rowconfigure    .jobs.current.total { 0 } -weight 1
      grid columnconfigure .jobs.current.total { 4 } -weight 1
    grid .jobs.current.total -row 1 -column 3 -sticky "we" -padx 2p -pady 2p

    label .jobs.current.currentFileNameTitle -text "File:"
    grid .jobs.current.currentFileNameTitle -row 2 -column 0 -sticky "w"
    entry .jobs.current.currentFileName -textvariable currentJob(fileName) -state readonly
#entry .jobs.current.done.percentageContainer.x
#pack .jobs.current.done.percentageContainer.x -fill x -expand yes
    grid .jobs.current.currentFileName -row 2 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p

    label .jobs.current.currentStorageTitle -text "Storage:"
    grid .jobs.current.currentStorageTitle -row 3 -column 0 -sticky "w"
    entry .jobs.current.currentStorage -textvariable currentJob(storageName) -state readonly
#entry .jobs.current.done.percentageContainer.x
#pack .jobs.current.done.percentageContainer.x -fill x -expand yes
    grid .jobs.current.currentStorage -row 3 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p

    label .jobs.current.filesPercentageTitle -text "Files:"
    grid .jobs.current.filesPercentageTitle -row 4 -column 0 -sticky "w"
    progressbar .jobs.current.filesPercentage
    grid .jobs.current.filesPercentage -row 4 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p
    addModifyTrace ::currentJob(doneFiles) \
      "
        global currentJob

        if {\$currentJob(totalFiles) > 0} \
        {
          set p \[expr {double(\$currentJob(doneFiles))/\$currentJob(totalFiles)}]
          progressbar .jobs.current.filesPercentage update \$p
        }
      "

    label .jobs.current.bytesPercentageTitle -text "Bytes:"
    grid .jobs.current.bytesPercentageTitle -row 5 -column 0 -sticky "w"
    progressbar .jobs.current.bytesPercentage
    grid .jobs.current.bytesPercentage -row 5 -column 1 -columnspan 4 -sticky "we" -padx 2p -pady 2p

#    grid rowconfigure    .jobs.current { 0 } -weight 1
    grid columnconfigure .jobs.current { 4 } -weight 1
  grid .jobs.current -row 0 -column 0 -sticky "we" -padx 2p -pady 2p

  tixScrolledListBox .jobs.list -scrollbar both -options { listbox.background white }
  grid .jobs.list -row 1 -column 0 -sticky "nswe" -padx 2p -pady 2p

  frame .jobs.buttons
    button .jobs.buttons.rem -text "Rem (Del)" -command "event generate . <<Event_remJob>>"
    pack .jobs.buttons.rem -side left
  grid .jobs.buttons -row 2 -column 0 -sticky "we" -padx 2p -pady 2p

  bind [.jobs.list subwidget listbox] <KeyPress-Delete>    "event generate . <<Event_remJob>>"
  bind [.jobs.list subwidget listbox] <KeyPress-KP_Delete> "event generate . <<Event_remJob>>"

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
    button .files.buttons.include -text "+" -command "event generate . <<Event_include>>"
    pack .files.buttons.include -side left -fill x -expand yes
    button .files.buttons.exclude -text "-" -command "event generate . <<Event_exclude>>"
    pack .files.buttons.exclude -side left -fill x -expand yes
  grid .files.buttons -row 1 -column 0 -sticky "we" -padx 2p -pady 2p

  bind [.files.list subwidget hlist] <KeyPress-plus>        "event generate . <<Event_include>>"
  bind [.files.list subwidget hlist] <KeyPress-KP_Add>      "event generate . <<Event_include>>"
  bind [.files.list subwidget hlist] <KeyPress-minus>       "event generate . <<Event_exclude>>"
  bind [.files.list subwidget hlist] <KeyPress-KP_Subtract> "event generate . <<Event_exclude>>"
  bind [.files.list subwidget hlist] <KeyPress-space>       "event generate . <<Event_toggleIncludeExclude>>"

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
  label .storage.partSizeTitle -text "Part size:"
  grid .storage.partSizeTitle -row 0 -column 0 -sticky "w" 
  frame .storage.split
    radiobutton .storage.split.unlimted -text "unlimted" -width 6 -anchor w -variable barConfig(storagePartSizeFlag) -value 0
    grid .storage.split.unlimted -row 0 -column 1 -sticky "w" 
    radiobutton .storage.split.size -text "split in" -width 6 -anchor w -variable barConfig(storagePartSizeFlag) -value 1
    grid .storage.split.size -row 0 -column 2 -sticky "w" 
    tixComboBox .storage.split.partSize -variable barConfig(partSize) -label "" -labelside right -editable true -options { entry.background white }
    grid .storage.split.partSize -row 0 -column 3 -sticky "w" 

   .storage.split.partSize insert end 128m
   .storage.split.partSize insert end 256m
   .storage.split.partSize insert end 512m
   .storage.split.partSize insert end 1g

    grid rowconfigure    .storage.split { 0 } -weight 1
    grid columnconfigure .storage.split { 1 } -weight 1
  grid .storage.split -row 0 -column 1 -sticky "w" -padx 2p -pady 2p
  addEnableTrace ::barConfig(storagePartSizeFlag) 1 .storage.split.partSize

  label .storage.maxTmpDirectorySizeTitle -text "Max. temp. size:"
  grid .storage.maxTmpDirectorySizeTitle -row 1 -column 0 -sticky "w" 
  frame .storage.maxTmpDirectorySize
    radiobutton .storage.maxTmpDirectorySize.unlimted -text "unlimted" -width 6 -anchor w -variable barConfig(storageMaxTmpDirectorySizeFlag) -value 0
    grid .storage.maxTmpDirectorySize.unlimted -row 0 -column 1 -sticky "w" 
    radiobutton .storage.maxTmpDirectorySize.limitto -text "limit to" -width 6 -anchor w -variable barConfig(storageMaxTmpDirectorySizeFlag) -value 1
    grid .storage.maxTmpDirectorySize.limitto -row 0 -column 2 -sticky "w" 
    tixComboBox .storage.maxTmpDirectorySize.size -variable barConfig(storageMaxTmpDirectorySize) -label "" -labelside right -editable true -options { entry.background white }
    grid .storage.maxTmpDirectorySize.size -row 0 -column 3 -sticky "w" 

   .storage.maxTmpDirectorySize.size insert end 128m
   .storage.maxTmpDirectorySize.size insert end 256m
   .storage.maxTmpDirectorySize.size insert end 512m
   .storage.maxTmpDirectorySize.size insert end 1g
   .storage.maxTmpDirectorySize.size insert end 2g
   .storage.maxTmpDirectorySize.size insert end 4g
   .storage.maxTmpDirectorySize.size insert end 8g

    grid rowconfigure    .storage.maxTmpDirectorySize { 0 } -weight 1
    grid columnconfigure .storage.maxTmpDirectorySize { 1 } -weight 1
  grid .storage.maxTmpDirectorySize -row 1 -column 1 -sticky "w" -padx 2p -pady 2p
  addEnableTrace ::barConfig(storageMaxTmpDirectorySizeFlag) 1 .storage.maxTmpDirectorySize.size

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
    entry .storage.scp.loginPassword -textvariable barConfig(storageLoginPassword) -bg white -show "*" -state disabled
    grid .storage.scp.loginPassword -row 0 -column 3 -sticky "we" 

    label .storage.scp.hostNameTitle -text "Host:" -state disabled
    grid .storage.scp.hostNameTitle -row 2 -column 0 -sticky "w" 
    entry .storage.scp.hostName -textvariable barConfig(storageHostName) -bg white -state disabled
    grid .storage.scp.hostName -row 2 -column 1 -sticky "we" 

    label .storage.scp.sshPortTitle -text "SSH port:" -state disabled
    grid .storage.scp.sshPortTitle -row 2 -column 2 -sticky "w" 
    tixControl .storage.scp.sshPort -variable barConfig(storageSSHPort) -label "" -labelside right -integer true -min 1 -max 65535 -options { entry.background white } -state disabled
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
  button $mainWindow.buttons.start -text "Start" -command "event generate . <<Event_start>>"
  pack $mainWindow.buttons.start -side left -padx 2p

  button $mainWindow.buttons.quit -text "Quit" -command "event generate . <<Event_quit>>"
  pack $mainWindow.buttons.quit -side right
pack $mainWindow.buttons -fill x -padx 2p -pady 2p

bind . <<Event_load>> \
{
}

bind . <<Event_save>> \
{
}

bind . <<Event_saveAs>> \
{
}

bind . <<Event_quit>> \
{
  quit
}

bind . <<Event_include>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    includeExcludeEntry [.files.list subwidget hlist] $itemPath 0
  }
}

bind . <<Event_exclude>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    includeExcludeEntry [.files.list subwidget hlist] $itemPath 1
  }
}

bind . <<Event_toggleIncludeExclude>> \
{
  foreach itemPath [.files.list subwidget hlist info selection] \
  {
    toggleIncludeExcludeEntry [.files.list subwidget hlist] $itemPath
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

set hostname "localhost"
set port 38523
if {![Server:connect $hostname $port]} \
{
  Dialog:error "Cannot connect to server '$hostname:$port'!"
  exit 1;
}

# read devices
#set commandId [Server:sendCommand "DEVICE_LIST"]
#while {[Server:readResult $commandId errorCode result]} \
#{
#  addDevice [.files.list subwidget hlist] $result
#}
addDevice [.files.list subwidget hlist] "/"

.excludes.list subwidget listbox insert end "abc"

update
puts [.jobs.current configure -width]

set currentJob(totalFiles) 10
after 100; set currentJob(doneFiles) 1
after 100; set currentJob(doneFiles) 2
after 100; set currentJob(doneFiles) 3
after 100; set currentJob(doneFiles) 4
after 100; set currentJob(doneFiles) 5
after 100; set currentJob(doneFiles) 6
after 100; set currentJob(doneFiles) 7
after 100; set currentJob(doneFiles) 8
after 100; set currentJob(doneFiles) 9
after 100; set currentJob(doneFiles) 10

# end of file
