/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TabJobs.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;

import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.dnd.ByteArrayTransfer;
import org.eclipse.swt.dnd.DND;
import org.eclipse.swt.dnd.DragSource;
import org.eclipse.swt.dnd.DragSourceEvent;
import org.eclipse.swt.dnd.DragSourceListener;
import org.eclipse.swt.dnd.DropTarget;
import org.eclipse.swt.dnd.DropTargetAdapter;
import org.eclipse.swt.dnd.DropTargetEvent;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.dnd.TransferData;
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.List;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.swt.widgets.TabFolder;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.swt.widgets.TableItem;
import org.eclipse.swt.widgets.Text;
import org.eclipse.swt.widgets.Tree;
import org.eclipse.swt.widgets.TreeColumn;
import org.eclipse.swt.widgets.TreeItem;
import org.eclipse.swt.widgets.Widget;

/****************************** Classes ********************************/

/** tab jobs
 */
class TabJobs
{
  /** pattern types
   */
  enum PatternTypes
  {
    INCLUDE,
    EXCLUDE,
  };

  /** file types
   */
  enum FileTypes
  {
    FILE,
    DIRECTORY,
    LINK,
    SPECIAL,
    DEVICE,
    SOCKET
  };

  /** file tree data
   */
  class FileTreeData
  {
    String    name;
    FileTypes type;
    long      size;
    long      datetime;
    String    title;

    FileTreeData(String name, FileTypes type, long size, long datetime, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = size;
      this.datetime = datetime;
      this.title    = title;
    }

    FileTreeData(String name, FileTypes type, long datetime, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = 0;
      this.datetime = datetime;
      this.title    = title;
    }

    FileTreeData(String name, FileTypes type, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = 0;
      this.datetime = 0;
      this.title    = title;
    }

    public String toString()
    {
      return "File {"+name+", "+type+", "+size+" bytes, datetime="+datetime+", title="+title+"}";
    }
  };

  /** file data comparator
   */
  class FileTreeDataComparator implements Comparator<FileTreeData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_NAME     = 0;
    private final static int SORTMODE_TYPE     = 1;
    private final static int SORTMODE_SIZE     = 2;
    private final static int SORTMODE_DATETIME = 3;

    private int sortMode;

    /** create file data comparator
     * @param tree file tree
     */
    FileTreeDataComparator(Tree tree)
    {
      TreeColumn sortColumn = tree.getSortColumn();

      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (tree.getColumn(3) == sortColumn) sortMode = SORTMODE_DATETIME;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** compare file tree data without take care about type
     * @param fileTreeData1, fileTreeData2 file tree data to compare
     * @return -1 iff fileTreeData1 < fileTreeData2,
                0 iff fileTreeData1 = fileTreeData2, 
                1 iff fileTreeData1 > fileTreeData2
     */
    private int compareWithoutType(FileTreeData fileTreeData1, FileTreeData fileTreeData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return fileTreeData1.title.compareTo(fileTreeData2.title);
        case SORTMODE_TYPE:
          return fileTreeData1.type.compareTo(fileTreeData2.type);
        case SORTMODE_SIZE:
          if      (fileTreeData1.size < fileTreeData2.size) return -1;
          else if (fileTreeData1.size > fileTreeData2.size) return  1;
          else                                              return  0;
        case SORTMODE_DATETIME:
          if      (fileTreeData1.datetime < fileTreeData2.datetime) return -1;
          else if (fileTreeData1.datetime > fileTreeData2.datetime) return  1;
          else                                                      return  0;
        default:
          return 0;
      }
    }

    /** compare file tree data
     * @param fileTreeData1, fileTreeData2 file tree data to compare
     * @return -1 iff fileTreeData1 < fileTreeData2,
                0 iff fileTreeData1 = fileTreeData2, 
                1 iff fileTreeData1 > fileTreeData2
     */
    public int compare(FileTreeData fileTreeData1, FileTreeData fileTreeData2)
    {
//System.err.println("BARControl.java"+", "+2734+": file1="+fileTreeData1+" file=2"+fileTreeData2+" "+sortMode);
      if (fileTreeData1.type == FileTypes.DIRECTORY)
      {
        if (fileTreeData2.type == FileTypes.DIRECTORY)
        {
          return compareWithoutType(fileTreeData1,fileTreeData2);
        }
        else
        {
          return -1;
        }
      }
      else
      {
        if (fileTreeData2.type == FileTypes.DIRECTORY)
        {
          return 1;
        }
        else
        {
          return compareWithoutType(fileTreeData1,fileTreeData2);
        }
      }
    }

    public String toString()
    {
      return "FileComparator {"+sortMode+"}";
    }
  }

  /** schedule data
   */
  class ScheduleData
  {
    final static int ANY = -1;

    int     year,month,day;
    String  weekDay;
    int     hour,minute;
    boolean enabled;
    String  type;

    ScheduleData()
    {
      this.year    = ScheduleData.ANY;
      this.month   = ScheduleData.ANY;
      this.day     = ScheduleData.ANY;
      this.weekDay = "*";
      this.hour    = ScheduleData.ANY;
      this.minute  = ScheduleData.ANY;
      this.enabled = false;
      this.type    = "*";
    }

    ScheduleData(String year, String month, String day, String weekDay, String hour, String minute, boolean enabled, String type)
    {
      setDate(year,month,day);
      this.weekDay = getValidString(weekDay,new String[]{"*","Mon","Tue","Wed","Thu","Fri","Sat","Sun"},"*");
      setTime(hour,minute);
      this.enabled = enabled;
      this.type    = getValidString(type,new String[]{"*","full","incremental"},"*");;
    }

    String getYear()
    {
      assert (year == ANY) || (year >= 1);

      return (year != ANY)?Integer.toString(year):"*";
    }

    String getMonth()
    {
      assert (month == ANY) || ((month >= 1) && (month <= 12));

      switch (month)
      {
        case ANY: return "*";
        case 1:   return "Jan";
        case 2:   return "Feb";
        case 3:   return "Mar";
        case 4:   return "Arp";
        case 5:   return "May";
        case 6:   return "Jun";
        case 7:   return "Jul";
        case 8:   return "Aug";
        case 9:   return "Sep";
        case 10:  return "Oct";
        case 11:  return "Nov";
        case 12:  return "Dec";
        default:  return "*";
      }
    }

    String getDay()
    {
      assert (day == ANY) || ((day >= 1) && (day <= 31));

      return (day != ANY)?Integer.toString(day):"*";
    }

    String getDate()
    {
      StringBuffer date = new StringBuffer();

      date.append(getYear());
      date.append('-');
      date.append(getMonth());
      date.append('-');
      date.append(getDay());

      return date.toString();
    }

    String getHour()
    {
      assert (hour   == ANY) || ((hour   >= 0) && (hour   <= 23));

      return (hour != ANY)?Integer.toString(hour):"*";
    }

    String getMinute()
    {
      assert (minute == ANY) || ((minute >= 0) && (minute <= 59));

      return (minute != ANY)?Integer.toString(minute):"*";
    }

    String getTime()
    {
      StringBuffer time = new StringBuffer();

      time.append(getHour());
      time.append(':');
      time.append(getMinute());

      return time.toString();
    }

    String getEnabled()
    {
      return enabled?"yes":"no";
    }

    void setDate(String year, String month, String day)
    {
      this.year = !year.equals("*")?Integer.parseInt(year):ANY;
      if      (month.equals("*")) this.month = ANY;
      else if (month.toLowerCase().equals("jan")) this.month =  1;
      else if (month.toLowerCase().equals("feb")) this.month =  2;
      else if (month.toLowerCase().equals("mar")) this.month =  3;
      else if (month.toLowerCase().equals("arp")) this.month =  4;
      else if (month.toLowerCase().equals("may")) this.month =  5;
      else if (month.toLowerCase().equals("jun")) this.month =  6;
      else if (month.toLowerCase().equals("jul")) this.month =  7;
      else if (month.toLowerCase().equals("aug")) this.month =  8;
      else if (month.toLowerCase().equals("sep")) this.month =  9;
      else if (month.toLowerCase().equals("oct")) this.month = 10;
      else if (month.toLowerCase().equals("nov")) this.month = 11;
      else if (month.toLowerCase().equals("dec")) this.month = 12;
      else
      {
        try
        {
          this.month = Integer.parseInt(month);
        }
        catch (NumberFormatException exception)
        {
          this.month = ANY;
        }
      }
      this.day = !day.equals("*")?Integer.parseInt(day):ANY;
    }

    void setTime(String hour, String minute)
    {
      this.hour   = !hour.equals  ("*")?Integer.parseInt(hour  ):ANY;
      this.minute = !minute.equals("*")?Integer.parseInt(minute):ANY;
    }

    public String toString()
    {
      return "File {"+getDate()+", "+weekDay+", "+getTime()+", "+enabled+", "+type+"}";
    }

    /** 
     * @param 
     * @return 
     */
    private String getValidString(String string, String[] validStrings, String defaultString)
    {
      for (String validString : validStrings)
      {
        if (validString.equals(string)) return validString;
      }

      return defaultString;
    }
  };

  /** schedule data comparator
   */
  class ScheduleDataComparator implements Comparator<ScheduleData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_DATE    = 0;
    private final static int SORTMODE_WEEKDAY = 1;
    private final static int SORTMODE_TIME    = 2;
    private final static int SORTMODE_ENABLED = 3;
    private final static int SORTMODE_TYPE    = 4;

    private int sortMode;

    private final String[] weekDays = new String[]{"mon","tue","wed","thu","fri","sat","sun"};

    /** create schedule data comparator
     * @param table schedule table
     * @param sortColumn sorting column
     */
    ScheduleDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_WEEKDAY;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_ENABLED;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_TYPE;
      else                                       sortMode = SORTMODE_DATE;
    }

    /** create schedule data comparator
     * @param table schedule table
     */
    ScheduleDataComparator(Table table)
    {
      TableColumn sortColumn = table.getSortColumn();

      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_WEEKDAY;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_ENABLED;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_TYPE;
      else                                       sortMode = SORTMODE_DATE;
    }
    /** compare schedule data
     * @param scheduleData1, scheduleData2 file tree data to compare
     * @return -1 iff scheduleData1 < scheduleData2,
                0 iff scheduleData1 = scheduleData2, 
                1 iff scheduleData1 > scheduleData2
     */
    public int compare(ScheduleData scheduleData1, ScheduleData scheduleData2)
    {
      switch (sortMode)
      {
        case SORTMODE_DATE:
          String date1 = scheduleData1.year+"-"+scheduleData1.month+"-"+scheduleData1.day;
          String date2 = scheduleData2.year+"-"+scheduleData2.month+"-"+scheduleData2.day;

          return date1.compareTo(date2);
        case SORTMODE_WEEKDAY:
          int index1 = indexOfWeekDay(scheduleData1.weekDay.toLowerCase());
          int index2 = indexOfWeekDay(scheduleData2.weekDay.toLowerCase());

          if      (index1 < index2) return -1;
          else if (index1 > index2) return  1;
          else                      return  0;
        case SORTMODE_TIME:
          String time1 = scheduleData1.hour+":"+scheduleData1.minute;
          String time2 = scheduleData2.hour+":"+scheduleData2.minute;

          return time1.compareTo(time2);
        case SORTMODE_ENABLED:
          if      (scheduleData1.enabled && !scheduleData2.enabled) return -1;
          else if (!scheduleData1.enabled && scheduleData2.enabled) return  1;
          else                                                      return  0;
        case SORTMODE_TYPE:
          return scheduleData1.type.compareTo(scheduleData2.type);
        default:
          return 0;
      }
    }

    /** get index of week day
     * @param weekDay week day
     * @return index
     */
    private int indexOfWeekDay(String weekDay)
    {
      int index = 0;
      while ((index < weekDays.length) && !weekDays[index].equals(weekDay))
      {
        index++;
      }

      return index;
    }
  }

  // colors
  private final Color COLOR_WHITE;
  private final Color COLOR_MODIFIED;

  // images
  private final Image IMAGE_DIRECTORY;
  private final Image IMAGE_DIRECTORY_INCLUDED;
  private final Image IMAGE_DIRECTORY_EXCLUDED;
  private final Image IMAGE_FILE;
  private final Image IMAGE_FILE_INCLUDED;
  private final Image IMAGE_FILE_EXCLUDED;
  private final Image IMAGE_LINK;
  private final Image IMAGE_LINK_INCLUDED;
  private final Image IMAGE_LINK_EXCLUDED;

  // global variable references
  Shell       shell;
  TabStatus   tabStatus;

  // widgets
  Composite   widgetTab;
  TabFolder   widgetTabFolder;
  Combo       widgetJobList;
  Tree        widgetFileTree;
  List        widgetIncludedPatterns;
  List        widgetExcludedPatterns;
  Combo       widgetArchivePartSize;
  Text        widgetCryptPublicKeyFileName;
  Button      widgetCryptPublicKeyFileNameSelect;
  Combo       widgetFTPMaxBandWidth;
  Combo       widgetSCPSFTPMaxBandWidth;
  Table       widgetScheduleList;

  // BAR variables
  BARVariable skipUnreadable          = new BARVariable(false);
  BARVariable overwriteFiles          = new BARVariable(false);

  BARVariable archiveType             = new BARVariable(new String[]{"normal","full","incremental"});
  BARVariable archivePartSizeFlag     = new BARVariable(false);
  BARVariable archivePartSize         = new BARVariable(0);
  BARVariable compressAlgorithm       = new BARVariable(new String[]{"none","zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9","bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9"});
  BARVariable cryptAlgorithm          = new BARVariable(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256"});
  BARVariable cryptType               = new BARVariable(new String[]{"","symmetric","asymmetric"});
  BARVariable cryptPublicKeyFileName  = new BARVariable("");
  BARVariable incrementalListFileName = new BARVariable("");
  BARVariable storageType             = new BARVariable(new String[]{"filesystem","ftp","scp","sftp","dvd","device"});
  BARVariable storageFileName         = new BARVariable("");
  BARVariable storageHostName         = new BARVariable("");
  BARVariable storageHostPort         = new BARVariable(0);
  BARVariable storageLoginName        = new BARVariable("");
  BARVariable storageLoginPassword    = new BARVariable("");
  BARVariable storageDeviceName       = new BARVariable("");
  BARVariable overwriteArchiveFiles   = new BARVariable(false);
  BARVariable sshPublicKeyFileName    = new BARVariable("");
  BARVariable sshPrivateKeyFileName   = new BARVariable("");
  BARVariable maxBandWidthFlag        = new BARVariable(false);
  BARVariable maxBandWidth            = new BARVariable(0);
  BARVariable volumeSize              = new BARVariable(0);
  BARVariable ecc                     = new BARVariable(false);

  // variables
  private     HashMap<String,Integer>  jobIds           = new HashMap<String,Integer>();
  private     String                   selectedJobName  = null;
  private     int                      selectedJobId    = 0;
  private     HashSet<String>          includedPatterns = new HashSet<String>();
  private     HashSet<String>          excludedPatterns = new HashSet<String>();
  private     LinkedList<ScheduleData> scheduleList     = new LinkedList<ScheduleData>();

  private     SimpleDateFormat         simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  TabJobs(TabFolder parentTabFolder, int accelerator)
  {
    Display     display;
    Composite   tab;
    Group       group;
    Composite   composite,subComposite;
    Label       label;
    Button      button;
    Combo       combo;
    TreeColumn  treeColumn;
    TreeItem    treeItem;
    Text        text;
    Spinner     spinner;
    TableColumn tableColumn;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    // get colors
    COLOR_WHITE    = shell.getDisplay().getSystemColor(SWT.COLOR_WHITE);
    COLOR_MODIFIED = new Color(null,0xFF,0xA0,0xA0);

    // get images
    IMAGE_DIRECTORY          = Widgets.loadImage(display,"directory.gif");
    IMAGE_DIRECTORY_INCLUDED = Widgets.loadImage(display,"directoryIncluded.gif");
    IMAGE_DIRECTORY_EXCLUDED = Widgets.loadImage(display,"directoryExcluded.gif");
    IMAGE_FILE               = Widgets.loadImage(display,"file.gif");
    IMAGE_FILE_INCLUDED      = Widgets.loadImage(display,"fileIncluded.gif");
    IMAGE_FILE_EXCLUDED      = Widgets.loadImage(display,"fileExcluded.gif");
    IMAGE_LINK               = Widgets.loadImage(display,"link.gif");
    IMAGE_LINK_INCLUDED      = Widgets.loadImage(display,"linkIncluded.gif");
    IMAGE_LINK_EXCLUDED      = Widgets.loadImage(display,"linkExcluded.gif");

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Jobs"+((accelerator != 0)?" ("+Widgets.acceleratorToText(accelerator)+")":""));
    widgetTab.setLayout(new TableLayout(new double[]{0,1,0},
                                        null,
                                        2
                                       )
                       );
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);

    // job selector
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      label = Widgets.newLabel(composite,"Name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobList = Widgets.newOptionMenu(composite,null);
      Widgets.layout(widgetJobList,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      widgetJobList.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo widget = (Combo)selectionEvent.widget;
          int   index  = widget.getSelectionIndex();
          if (index >= 0)
          {
            selectedJobName = widgetJobList.getItem(index);
            selectedJobId   = jobIds.get(selectedJobName);
            Widgets.setEnabled(widgetTabFolder,true);
            update();
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,null,"New");
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobNew();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,null,"Rename");
      Widgets.layout(button,0,3,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          if (selectedJobId > 0)
          {
            jobRename();
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,null,"Delete");
      Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          if (selectedJobId > 0)
          {
            jobDelete();
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // sub-tabs
    widgetTabFolder = Widgets.newTabFolder(widgetTab);
    Widgets.layout(widgetTabFolder,1,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
    {
      tab = Widgets.addTab(widgetTabFolder,"Files");
      Widgets.layout(tab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
      {
        // file tree
        widgetFileTree = Widgets.newTree(tab,null);
        Widgets.layout(widgetFileTree,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
        SelectionListener filesTreeColumnSelectionListener = new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TreeColumn             treeColumn = (TreeColumn)selectionEvent.widget;
            FileTreeDataComparator fileTreeDataComparator = new FileTreeDataComparator(widgetFileTree);
            synchronized(widgetFileTree)
            {
              Widgets.sortTreeColumn(widgetFileTree,treeColumn,fileTreeDataComparator);
            }
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        };
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Name",    SWT.LEFT, 500,true);
        treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Type",    SWT.LEFT,  50,false);
        treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Size",    SWT.RIGHT,100,false);
        treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Modified",SWT.LEFT, 100,false);
        treeColumn.addSelectionListener(filesTreeColumnSelectionListener);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newButton(composite,null,"*");
          Widgets.layout(button,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                patternDelete(PatternTypes.INCLUDE,fileTreeData.name);
                patternDelete(PatternTypes.EXCLUDE,fileTreeData.name);
                switch (fileTreeData.type)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE);      break;
                  case DEVICE:    treeItem.setImage(IMAGE_FILE);      break;
                  case SOCKET:    treeItem.setImage(IMAGE_FILE);      break;
                }
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,null,"+");
          Widgets.layout(button,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                patternNew(PatternTypes.INCLUDE,fileTreeData.name);
                patternDelete(PatternTypes.EXCLUDE,fileTreeData.name);
                switch (fileTreeData.type)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE_INCLUDED);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY_INCLUDED); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK_INCLUDED);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE_INCLUDED);      break;
                  case DEVICE:    treeItem.setImage(IMAGE_FILE_INCLUDED);      break;
                  case SOCKET:    treeItem.setImage(IMAGE_FILE_INCLUDED);      break;
                }
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,null,"-");
          Widgets.layout(button,0,2,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                patternDelete(PatternTypes.INCLUDE,fileTreeData.name);
                patternNew(PatternTypes.EXCLUDE,fileTreeData.name);
                switch (fileTreeData.type)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE_EXCLUDED);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY_EXCLUDED); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK_EXCLUDED);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE_EXCLUDED);      break;
                  case DEVICE:    treeItem.setImage(IMAGE_FILE_EXCLUDED);      break;
                  case SOCKET:    treeItem.setImage(IMAGE_FILE_EXCLUDED);      break;
                }
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }
      }

      tab = Widgets.addTab(widgetTabFolder,"Filters");
      Widgets.layout(tab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
      {
        // included list
        label = Widgets.newLabel(tab,"Included:");
        Widgets.layout(label,0,0,TableLayoutData.NS);
        widgetIncludedPatterns = Widgets.newList(tab,null);
        Widgets.layout(widgetIncludedPatterns,0,1,TableLayoutData.NSWE|TableLayoutData.EXPAND);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                patternNew(PatternTypes.INCLUDE);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                patternDelete(PatternTypes.INCLUDE);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // excluded list
        label = Widgets.newLabel(tab,"Excluded:");
        Widgets.layout(label,2,0,TableLayoutData.NS);
        widgetExcludedPatterns = Widgets.newList(tab,null);
        Widgets.layout(widgetExcludedPatterns,2,1,TableLayoutData.NSWE|TableLayoutData.EXPAND);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                patternNew(PatternTypes.EXCLUDE);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                patternDelete(PatternTypes.EXCLUDE);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // options
        label = Widgets.newLabel(tab,"Options:");
        Widgets.layout(label,4,0,TableLayoutData.NS);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,4,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newCheckbox(composite,null,"skip unreadable files");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button  widget      = (Button)selectionEvent.widget;
              boolean checkedFlag = widget.getSelection();
              skipUnreadable.set(checkedFlag);
              BARServer.set(selectedJobId,"skip-unreadable",checkedFlag);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,skipUnreadable));
        }
      }

      tab = Widgets.addTab(widgetTabFolder,"Storage");
      Widgets.layout(tab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
      {
        // part size
        label = Widgets.newLabel(tab,"Part size:");
        Widgets.layout(label,0,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newRadio(composite,null,"unlimited");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archivePartSizeFlag.set(false);
              archivePartSize.set(0);
              BARServer.set(selectedJobId,"archive-part-size",0);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
          {
            public void modified(Control control, BARVariable archivePartSizeFlag)
            {
              ((Button)control).setSelection(!archivePartSizeFlag.getBoolean());
              widgetArchivePartSize.setEnabled(!archivePartSizeFlag.getBoolean());
            }
          });

          button = Widgets.newRadio(composite,null,"limit to");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archivePartSizeFlag.set(true);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
          {
            public void modified(Control control, BARVariable archivePartSizeFlag)
            {
              ((Button)control).setSelection(archivePartSizeFlag.getBoolean());
              widgetArchivePartSize.setEnabled(archivePartSizeFlag.getBoolean());
            }
          });

          widgetArchivePartSize = Widgets.newCombo(composite,null);
          widgetArchivePartSize.setItems(new String[]{"32M","64M","128M","256M","512M","1G","2G"});
          Widgets.layout(widgetArchivePartSize,0,2,TableLayoutData.W);
          widgetArchivePartSize.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Combo widget = (Combo)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              try
              {
                long n = Units.parseByteSize(widget.getText());
                if (archivePartSize.getLong() == n) color = COLOR_WHITE;
              }
              catch (NumberFormatException exception)
              {
              }
              widget.setBackground(color);
            }
          });
          widgetArchivePartSize.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String s      = widget.getText();
              try
              {
                long n = Units.parseByteSize(s);
                archivePartSize.set(n);
                BARServer.set(selectedJobId,"archive-part-size",n);
              }
              catch (NumberFormatException exception)
              {
                Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
              }
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo widget = (Combo)selectionEvent.widget;
              long  n      = Units.parseByteSize(widget.getText());
              archivePartSize.set(n);
              BARServer.set(selectedJobId,"archive-part-size",n);
            }
          });
          widgetArchivePartSize.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Combo  widget = (Combo)focusEvent.widget;
              String s      = widget.getText();
              try
              {
                long n = Units.parseByteSize(s);
                archivePartSize.set(n);
                BARServer.set(selectedJobId,"archive-part-size",n);
              }
              catch (NumberFormatException exception)
              {
                Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
                widget.forceFocus();
              }
            }
          });
          Widgets.addModifyListener(new WidgetListener(widgetArchivePartSize,archivePartSize)
          {
            public String getString(BARVariable variable)
            {
              return Units.formatByteSize(variable.getLong());
            }
          });
        }

        // compress
        label = Widgets.newLabel(tab,"Compress:");
        Widgets.layout(label,1,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          combo = Widgets.newOptionMenu(composite,null);
          combo.setItems(new String[]{"none","zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9","bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9"});
          Widgets.layout(combo,0,0,TableLayoutData.W);
          combo.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String s      = widget.getText();
              compressAlgorithm.set(s);
              BARServer.set(selectedJobId,"compress-algorithm",s);
            }
          });
          Widgets.addModifyListener(new WidgetListener(combo,compressAlgorithm));
        }

        // crypt
        label = Widgets.newLabel(tab,"Crypt:");
        Widgets.layout(label,2,0,TableLayoutData.NW);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,2,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          combo = Widgets.newOptionMenu(composite,null);
          combo.setItems(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256"});
          Widgets.layout(combo,0,0,TableLayoutData.W);
          combo.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String s      = widget.getText();
              cryptAlgorithm.set(s);
              BARServer.set(selectedJobId,"crypt-algorithm",s);
            }
          });
          Widgets.addModifyListener(new WidgetListener(combo,cryptAlgorithm));
        }

        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newRadio(composite,null,"symmetric");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              Widgets.setEnabled(widgetCryptPublicKeyFileName,false);
              Widgets.setEnabled(widgetCryptPublicKeyFileNameSelect,false);
              cryptType.set("symmetric");
              BARServer.set(selectedJobId,"crypt-type","symmetric");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,cryptType)
          {
            public void modified(Control control, BARVariable cryptType)
            {
              boolean asymmetricFlag = cryptType.equals("asymmetric");
              Widgets.setEnabled(widgetCryptPublicKeyFileName,asymmetricFlag);
              Widgets.setEnabled(widgetCryptPublicKeyFileNameSelect,asymmetricFlag);
              ((Button)control).setSelection(cryptType.equals("symmetric"));
            }
          });

          button = Widgets.newRadio(composite,null,"asymmetric");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              Widgets.setEnabled(widgetCryptPublicKeyFileName,true);
              Widgets.setEnabled(widgetCryptPublicKeyFileNameSelect,true);
              cryptType.set("asymmetric");
              BARServer.set(selectedJobId,"crypt-type","asymmetric");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,cryptType)
          {
            public void modified(Control control, BARVariable cryptType)
            {
              boolean asymmetricFlag = cryptType.equals("asymmetric");
              Widgets.setEnabled(widgetCryptPublicKeyFileName,asymmetricFlag);
              Widgets.setEnabled(widgetCryptPublicKeyFileNameSelect,asymmetricFlag);
              ((Button)control).setSelection(cryptType.equals("asymmetric"));
            }
          });

          label = Widgets.newLabel(composite,"Public key:");
          Widgets.layout(label,0,2,TableLayoutData.W);
          widgetCryptPublicKeyFileName = Widgets.newText(composite,null);
          Widgets.layout(widgetCryptPublicKeyFileName,0,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          widgetCryptPublicKeyFileName.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              try
              {
                String s = widget.getText();
                if (cryptPublicKeyFileName.getString().equals(s)) color = COLOR_WHITE;
              }
              catch (NumberFormatException exception)
              {
              }
              widget.setBackground(color);
            }
          });
          widgetCryptPublicKeyFileName.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();
              cryptPublicKeyFileName.set(string);
              BARServer.set(selectedJobId,"crypt-public-key",string);
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          widgetCryptPublicKeyFileName.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();
              cryptPublicKeyFileName.set(string);
              BARServer.set(selectedJobId,"crypt-public-key",string);
            }
          });
          Widgets.addModifyListener(new WidgetListener(widgetCryptPublicKeyFileName,cryptPublicKeyFileName));

          widgetCryptPublicKeyFileNameSelect = Widgets.newButton(composite,null,IMAGE_DIRECTORY);
          Widgets.layout(widgetCryptPublicKeyFileNameSelect,0,4,TableLayoutData.DEFAULT);
          widgetCryptPublicKeyFileNameSelect.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget   = (Button)selectionEvent.widget;
              String fileName = Dialogs.fileSave(shell,
                                                 "Select public key file",
                                                 cryptPublicKeyFileName.getString(),
                                                 new String[]{"Public key","*.public",
                                                              "All files","*",
                                                             }
                                                );
              if (fileName != null)
              {
                cryptPublicKeyFileName.set(fileName);
                BARServer.set(selectedJobId,"crypt-public-key",fileName);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // archive type
        label = Widgets.newLabel(tab,"Mode:");
        Widgets.layout(label,4,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,4,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newRadio(composite,null,"normal");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archiveType.set("normal");
              BARServer.set(selectedJobId,"archive-type","normal");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archiveType)
          {
            public void modified(Control control, BARVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("normal"));
            }
          });

          button = Widgets.newRadio(composite,null,"full");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archiveType.set("full");
              BARServer.set(selectedJobId,"archive-type","full");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archiveType)
          {
            public void modified(Control control, BARVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("full"));
            }
          });

          button = Widgets.newRadio(composite,null,"incremental");
          Widgets.layout(button,0,2,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archiveType.set("incremental");
              BARServer.set(selectedJobId,"archive-type","incremental");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archiveType)
          {
            public void modified(Control control, BARVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("incremental"));
            }
          });

          text = Widgets.newText(composite,null);
          Widgets.layout(text,0,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              try
              {
                String s = widget.getText();
                if (incrementalListFileName.getString().equals(s)) color = COLOR_WHITE;
              }
              catch (NumberFormatException exception)
              {
              }
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();
              incrementalListFileName.set(string);
              BARServer.set(selectedJobId,"incremental-list-file",string);
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          text.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();
              incrementalListFileName.set(string);
              BARServer.set(selectedJobId,"incremental-list-file",string);
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,incrementalListFileName));

          button = Widgets.newButton(composite,null,IMAGE_DIRECTORY);
          Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget   = (Button)selectionEvent.widget;
              String fileName = Dialogs.fileSave(shell,
                                                 "Select incremental file",
                                                 incrementalListFileName.getString(),
                                                 new String[]{"BAR incremental data","*.bid",
                                                              "All files","*",
                                                             }
                                                );
              if (fileName != null)
              {
                incrementalListFileName.set(fileName);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // file name
        label = Widgets.newLabel(tab,"File name:");
        Widgets.layout(label,5,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,5,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          text = Widgets.newText(composite,null);
          Widgets.layout(text,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              try
              {
                String s = widget.getText();
                if (storageFileName.getString().equals(s)) color = COLOR_WHITE;
              }
              catch (NumberFormatException exception)
              {
              }
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text widget = (Text)selectionEvent.widget;
              storageFileName.set(widget.getText());
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          text.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;
              storageFileName.set(widget.getText());
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,storageFileName));

          button = Widgets.newButton(composite,null,IMAGE_DIRECTORY);
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId != 0)
              {
                storageFileNameEdit();
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // destination
        label = Widgets.newLabel(tab,"Destination:");
        Widgets.layout(label,6,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,6,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newRadio(composite,null,"File system");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("filesystem");
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("filesystem"));
            }
          });

          button = Widgets.newRadio(composite,null,"ftp");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("ftp");
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("ftp"));
            }
          });

          button = Widgets.newRadio(composite,null,"scp");
          Widgets.layout(button,0,2,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("scp");
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("scp"));
            }
          });

          button = Widgets.newRadio(composite,null,"sftp");
          Widgets.layout(button,0,3,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("sftp");
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("sftp"));
            }
          });

          button = Widgets.newRadio(composite,null,"DVD");
          Widgets.layout(button,0,4,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("dvd");
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("dvd"));
            }
          });

          button = Widgets.newRadio(composite,null,"Device");
          Widgets.layout(button,0,5,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("device");
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("device"));
            }
          });
        }

        // destination file system
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,7,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("filesystem"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          button = Widgets.newCheckbox(composite,null,"overwrite archive files");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button  widget      = (Button)selectionEvent.widget;
              boolean checkedFlag = widget.getSelection();
              overwriteArchiveFiles.set(checkedFlag);
              BARServer.set(selectedJobId,"overwrite-archive-files",checkedFlag);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,overwriteArchiveFiles));
        }

        // destination ftp
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,7,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("ftp"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          composite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            label = Widgets.newLabel(composite,"User:");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(composite,null);
            Widgets.layout(text,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text  widget = (Text)modifyEvent.widget;
                Color color  = COLOR_MODIFIED;
                try
                {
                  String s = widget.getText();
                  if (storageLoginName.getString().equals(s)) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginName.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Text widget = (Text)focusEvent.widget;
                storageLoginName.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageLoginName));

            label = Widgets.newLabel(composite,"Host:");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(composite,null);
            Widgets.layout(text,0,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text  widget = (Text)modifyEvent.widget;
                Color color  = COLOR_MODIFIED;
                try
                {
                  String s = widget.getText();
                  if (storageHostName.getString().equals(s)) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageHostName.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Text widget = (Text)focusEvent.widget;
                storageHostName.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageHostName));

            label = Widgets.newLabel(composite,"Password:");
            Widgets.layout(label,0,4,TableLayoutData.W);

            text = Widgets.newPassword(composite,null);
            Widgets.layout(text,0,5,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text  widget = (Text)modifyEvent.widget;
                Color color  = COLOR_MODIFIED;
                try
                {
                  String s = widget.getText();
                  if (storageLoginPassword.getString().equals(s)) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginPassword.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Text widget = (Text)focusEvent.widget;
                storageLoginPassword.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageLoginPassword));
          }

/*
          label = Widgets.newLabel(composite,"Max. band width:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          composite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(composite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            button = Widgets.newRadio(composite,null,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.set(selectedJobId,"max-band-width",0);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, BARVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            button = Widgets.newRadio(composite,null,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                archivePartSizeFlag.set(true);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, BARVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(maxBandWidthFlag.getBoolean());
                widgetFTPMaxBandWidth.setEnabled(maxBandWidthFlag.getBoolean());
              }
            });

            widgetFTPMaxBandWidth = Widgets.newCombo(composite,null);
            widgetFTPMaxBandWidth.setItems(new String[]{"32K","64K","128K","256K","512K"});
            Widgets.layout(widgetFTPMaxBandWidth,0,2,TableLayoutData.W);
          }
*/
        }

        // destination scp/sftp
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,7,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("scp") || variable.equals("sftp"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Server");
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            label = Widgets.newLabel(subComposite,"Login:");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(subComposite,null);
            Widgets.layout(text,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text  widget = (Text)modifyEvent.widget;
                Color color  = COLOR_MODIFIED;
                try
                {
                  String s = widget.getText();
                  if (storageLoginName.getString().equals(s)) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginName.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Text widget = (Text)focusEvent.widget;
                storageLoginName.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageLoginName));

            label = Widgets.newLabel(subComposite,"Host:");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(subComposite,null);
            Widgets.layout(text,0,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String s      = widget.getText();
                if (storageHostName.getString().equals(s)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageHostName.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Text widget = (Text)focusEvent.widget;
                storageHostName.set(widget.getText());
                BARServer.set(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageHostName));

            label = Widgets.newLabel(subComposite,"Port:");
            Widgets.layout(label,0,4,TableLayoutData.W);

            text = Widgets.newText(subComposite,null);
            Widgets.layout(text,0,5,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String s      = widget.getText();
                try
                {
                  long n = !s.equals("")?Long.parseLong(widget.getText()):0;
                  if (storageHostPort.getLong() == n) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text   widget = (Text)selectionEvent.widget;
                String s      = widget.getText();
                try
                {
                  long n = !s.equals("")?Long.parseLong(widget.getText()):0;
                  if ((n >= 0) && (n <= 65535))
                  {
                    storageHostPort.set(n);
                    BARServer.set(selectedJobId,"archive-name",getArchiveName());
                  }
                  else
                  {
                    Dialogs.error(shell,"'"+n+"' is out of range!\n\nEnter a number between 0 and 65535.");
                    widget.forceFocus();
                  }
                }
                catch (NumberFormatException exception)
                {
                  Dialogs.error(shell,"'"+s+"' is not valid port number!\n\nEnter a number between 0 and 65535.");
                  widget.forceFocus();
                }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
throw new Error("NYI");
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Text   widget = (Text)focusEvent.widget;
                String s      = widget.getText();
                try
                {
                  long n = !s.equals("")?Long.parseLong(widget.getText()):0;
                  if ((n >= 0) && (n <= 65535))
                  {
                    storageHostPort.set(n);
                    BARServer.set(selectedJobId,"archive-name",getArchiveName());
                  }
                  else
                  {
                    Dialogs.error(shell,"'"+n+"' is out of range!\n\nEnter a number between 0 and 65535.");
                    widget.forceFocus();
                  }
                }
                catch (NumberFormatException exception)
                {
                  Dialogs.error(shell,"'"+s+"' is not valid port number!\n\nEnter a number between 0 and 65535.");
                  widget.forceFocus();
                }
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageHostPort));
          }

          label = Widgets.newLabel(composite,"SSH public key:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          text = Widgets.newText(composite,null);
          Widgets.layout(text,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              try
              {
                String s = widget.getText();
                if (sshPublicKeyFileName.getString().equals(s)) color = COLOR_WHITE;
              }
              catch (NumberFormatException exception)
              {
              }
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();
              sshPublicKeyFileName.set(string);
              BARServer.set(selectedJobId,"ssh-public-key",string);
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          text.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();
              sshPublicKeyFileName.set(string);
              BARServer.set(selectedJobId,"ssh-public-key",string);
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,sshPublicKeyFileName));

          label = Widgets.newLabel(composite,"SSH private key:");
          Widgets.layout(label,2,0,TableLayoutData.W);
          text = Widgets.newText(composite,null);
          Widgets.layout(text,2,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              try
              {
                String s = widget.getText();
                if (sshPrivateKeyFileName.getString().equals(s)) color = COLOR_WHITE;
              }
              catch (NumberFormatException exception)
              {
              }
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();
              sshPrivateKeyFileName.set(string);
              BARServer.set(selectedJobId,"ssh-private-key",string);
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          text.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();
              sshPrivateKeyFileName.set(string);
              BARServer.set(selectedJobId,"ssh-private-key",string);
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,sshPrivateKeyFileName));

/*
          label = Widgets.newLabel(composite,"Max. band width:");
          Widgets.layout(label,3,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            button = Widgets.newRadio(subComposite,null,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.set(selectedJobId,"max-band-width",0);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, BARVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetSCPSFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            button = Widgets.newRadio(subComposite,null,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.set(selectedJobId,"max-band-width",0);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, BARVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetSCPSFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            widgetSCPSFTPMaxBandWidth = Widgets.newCombo(subComposite,null);
            widgetSCPSFTPMaxBandWidth.setItems(new String[]{"32K","64K","128K","256K","512K"});
            Widgets.layout(widgetSCPSFTPMaxBandWidth,0,2,TableLayoutData.W);
          }
*/
        }

        // destination dvd
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,7,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("dvd"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Device:");
          Widgets.layout(label,0,0,TableLayoutData.W);
          text = Widgets.newText(composite,null);
          Widgets.layout(text,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              try
              {
                String s = widget.getText();
                if (storageDeviceName.getString().equals(s)) color = COLOR_WHITE;
              }
              catch (NumberFormatException exception)
              {
              }
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text widget = (Text)selectionEvent.widget;
              storageDeviceName.set(widget.getText());
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          text.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;
              storageDeviceName.set(widget.getText());
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,storageDeviceName));

          label = Widgets.newLabel(composite,"Size:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            combo = Widgets.newCombo(subComposite,null);
            combo.setItems(new String[]{"2G","3G","3.6G","4G"});
            Widgets.layout(combo,0,0,TableLayoutData.W);
            combo.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Combo widget = (Combo)modifyEvent.widget;
                Color color  = COLOR_MODIFIED;
                try
                {
                  long n = Units.parseByteSize(widget.getText());
                  if (volumeSize.getLong() == n) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
              }
            });
            combo.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String s      = widget.getText();
                try
                {
                  long n = Units.parseByteSize(s);
                  volumeSize.set(n);
                  BARServer.set(selectedJobId,"volume-size",n);
                }
                catch (NumberFormatException exception)
                {
                  Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
                }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Combo widget = (Combo)selectionEvent.widget;
                long  n      = Units.parseByteSize(widget.getText());
                volumeSize.set(n);
                BARServer.set(selectedJobId,"volume-size",n);
              }
            });
            combo.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Combo  widget = (Combo)focusEvent.widget;
                String s      = widget.getText();
                try
                {
                  long n = Units.parseByteSize(s);
                  volumeSize.set(n);
                  BARServer.set(selectedJobId,"volume-size",n);
                }
                catch (NumberFormatException exception)
                {
                  Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
                  widget.forceFocus();
                }
              }
            });
            Widgets.addModifyListener(new WidgetListener(combo,volumeSize)
            {
              public String getString(BARVariable variable)
              {
                return Units.formatByteSize(variable.getLong());
              }
            });

            label = Widgets.newLabel(subComposite,"bytes");
            Widgets.layout(label,0,1,TableLayoutData.W);
          }

          label = Widgets.newLabel(composite,"Options:");
          Widgets.layout(label,3,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            button = Widgets.newCheckbox(subComposite,null,"add error-correction codes");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();
                ecc.set(checkedFlag);
                BARServer.set(selectedJobId,"ecc",checkedFlag);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,ecc));
          }
        }

        // destination device 
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,7,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("device"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Device:");
          Widgets.layout(label,0,0,TableLayoutData.W);
          text = Widgets.newText(composite,null);
          Widgets.layout(text,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              try
              {
                String s = widget.getText();
                if (storageDeviceName.getString().equals(s)) color = COLOR_WHITE;
              }
              catch (NumberFormatException exception)
              {
              }
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text widget = (Text)selectionEvent.widget;
              storageDeviceName.set(widget.getText());
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          text.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;
              storageDeviceName.set(widget.getText());
              BARServer.set(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,storageDeviceName));

          label = Widgets.newLabel(composite,"Size:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            combo = Widgets.newCombo(subComposite,null);
            combo.setItems(new String[]{"2G","3G","3.6G","4G"});
            Widgets.layout(combo,0,0,TableLayoutData.W);
            combo.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Combo widget = (Combo)modifyEvent.widget;
                Color color  = COLOR_MODIFIED;
                try
                {
                  long n = Units.parseByteSize(widget.getText());
                  if (volumeSize.getLong() == n) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
              }
            });
            combo.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String s      = widget.getText();
                try
                {
                  long n = Units.parseByteSize(s);
                  volumeSize.set(n);
                  BARServer.set(selectedJobId,"volume-size",n);
                }
                catch (NumberFormatException exception)
                {
                  Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
                }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Combo widget = (Combo)selectionEvent.widget;
                long  n      = Units.parseByteSize(widget.getText());
                volumeSize.set(n);
                BARServer.set(selectedJobId,"volume-size",n);
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Combo  widget = (Combo)focusEvent.widget;
                String s      = widget.getText();
                try
                {
                  long n = Units.parseByteSize(s);
                  volumeSize.set(n);
                  BARServer.set(selectedJobId,"volume-size",n);
                }
                catch (NumberFormatException exception)
                {
                  Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
                  widget.forceFocus();
                }
              }
            });
            Widgets.addModifyListener(new WidgetListener(combo,volumeSize)
            {
              public String getString(BARVariable variable)
              {
                return Units.formatByteSize(variable.getLong());
              }
            });

            label = Widgets.newLabel(subComposite,"bytes");
            Widgets.layout(label,0,1,TableLayoutData.W);
          }
        }
      }

      tab = Widgets.addTab(widgetTabFolder,"Schedule");
      Widgets.layout(tab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
      {
        // list
        widgetScheduleList = Widgets.newTable(tab,this);
        Widgets.layout(widgetScheduleList,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
        widgetScheduleList.addListener(SWT.MouseDoubleClick,new Listener()
        {
          public void handleEvent(final Event event)
          {
            scheduleEdit();
          }
        });
        SelectionListener scheduleListColumnSelectionListener = new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn            tableColumn = (TableColumn)selectionEvent.widget;
            ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleList,tableColumn);
            synchronized(scheduleList)
            {
              Widgets.sortTableColumn(widgetScheduleList,tableColumn,scheduleDataComparator);
            }
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        };
        tableColumn = Widgets.addTableColumn(widgetScheduleList,0,"Date",     SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,1,"Week day", SWT.LEFT,100,true );
        synchronized(scheduleList)
        {
          Widgets.sortTableColumn(widgetScheduleList,tableColumn,new ScheduleDataComparator(widgetScheduleList,tableColumn));
        }
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,2,"Time",     SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,3,"Enabled",  SWT.LEFT,  0,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,4,"Type",     SWT.LEFT,  0,true );
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              scheduleNew();
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          button = Widgets.newButton(composite,null,"Edit");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              scheduleEdit();
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              scheduleDelete();
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }
      }
    }
    Widgets.setEnabled(widgetTabFolder,false);

    // add root devices
    addRootDevices();

    // update data
    updateJobList();
  }

  /** select job
   * @param name job name
   */
  void selectJob(String name)
  {
    synchronized(widgetJobList)
    {
      int index = 0;
      while (   (index < widgetJobList.getItemCount())
             && !name.equals(widgetJobList.getItem(index))
            )
      {
        index++;
      }
      if (index < widgetJobList.getItemCount())
      {
        selectedJobName = name;
        selectedJobId   = jobIds.get(name);
        widgetJobList.select(index);
        Widgets.setEnabled(widgetTabFolder,true);
        update();
      }
    }
  }

  //-----------------------------------------------------------------------

  void setTabStatus(TabStatus tabStatus)
  {
    this.tabStatus = tabStatus;
  }

  /** add root devices
   */
  private void addRootDevices()
  {
    TreeItem treeItem = Widgets.addTreeItem(widgetFileTree,new FileTreeData("/",FileTypes.DIRECTORY,"/"),true);
    treeItem.setText("/");
    treeItem.setImage(IMAGE_DIRECTORY);
    widgetFileTree.addListener(SWT.Expand,new Listener()
    {
      public void handleEvent(final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        updateFileList(treeItem);
      }
    });
    widgetFileTree.addListener(SWT.Collapse,new Listener()
    {
      public void handleEvent(final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        treeItem.removeAll();
        new TreeItem(treeItem,SWT.NONE);
      }
    });
    widgetFileTree.addListener(SWT.MouseDoubleClick,new Listener()
    {
      public void handleEvent(final Event event)
      {
        TreeItem treeItem = widgetFileTree.getItem(new Point(event.x,event.y));
        if (treeItem != null)
        {
          Event treeEvent = new Event();
          treeEvent.item = treeItem;
          if (treeItem.getExpanded())
          {
            widgetFileTree.notifyListeners(SWT.Collapse,treeEvent);
            treeItem.setExpanded(false);
          }
          else
          {
            widgetFileTree.notifyListeners(SWT.Expand,treeEvent);
            treeItem.setExpanded(true);
          }
        }
      }
    });
  }

  /** get archive name
   * @return archive name
   *   ftp://<name>@<host>/<file name>
   *   scp://<name>@<host>:<port>/<file name>
   *   sftp://<name>@<host>:<port>/<file name>
   *   dvd://<device>/<file name>
   *   device://<device>/<file name>
   *   <file name>
   */
  private String getArchiveName()
  {
    StringBuffer archiveName = new StringBuffer();

    if      (storageType.equals("ftp"))
    {
      archiveName.append("ftp:");
      if (!storageLoginName.equals("") || !storageHostName.equals(""))
      {
        archiveName.append("//");
        if (!storageLoginName.equals("") || !storageLoginPassword.equals(""))
        {
          if (!storageLoginName.equals("")) archiveName.append(storageLoginName);
          if (!storageLoginPassword.equals("")) { archiveName.append(':'); archiveName.append(storageLoginPassword); }
          archiveName.append('@');
        }
        if (!storageHostName.equals("")) { archiveName.append(storageHostName); }
        archiveName.append('/');
      }
    }
    else if (storageType.equals("scp"))
    {
      archiveName.append("scp:");
      if (!storageLoginName.equals("") || !storageHostName.equals(""))
      {
        archiveName.append("//");
        if (!storageLoginName.equals("")) { archiveName.append(storageLoginName); archiveName.append('@'); }
        if (!storageHostName.equals("")) { archiveName.append(storageHostName); }
        if (storageHostPort.getLong() > 0) { archiveName.append(':'); archiveName.append(storageHostPort.getLong()); }
        archiveName.append('/');
      }
    }
    else if (storageType.equals("sftp"))
    {
      archiveName.append("sftp:");
      if (!storageLoginName.equals("") || !storageHostName.equals(""))
      {
        archiveName.append("//");
        if (!storageLoginName.equals("")) { archiveName.append(storageLoginName); archiveName.append('@'); }
        if (!storageHostName.equals("")) { archiveName.append(storageHostName); }
        if (storageHostPort.getLong() > 0) { archiveName.append(':'); archiveName.append(storageHostPort.getLong()); }
        archiveName.append('/');
      }
    }
    else if (storageType.equals("dvd"))
    {
      archiveName.append("dvd:");
      if (!storageDeviceName.equals(""))
      {
        archiveName.append("//");
        if (!storageHostName.equals("")) { archiveName.append(storageDeviceName); }
        archiveName.append('/');
      }
    }
    else if (storageType.equals("device"))
    {
      archiveName.append("device:");
      if (!storageDeviceName.equals(""))
      {
        archiveName.append("//");
        if (!storageHostName.equals("")) { archiveName.append(storageDeviceName); }
        archiveName.append('/');
      }
    }
    archiveName.append(storageFileName);

    return archiveName.toString();
  }

  /** parse archive name
   * @param archiveName archive name
   *   ftp://<name>@<host>/<file name>
   *   scp://<name>@<host>:<port>/<file name>
   *   sftp://<name>@<host>:<port>/<file name>
   *   dvd://<device>/<file name>
   *   device://<device>/<file name>
   *   <file name>
   */
  private void parseArchiveName(String archiveName)
  {
    storageType.set      ("filesystem");
    storageLoginName.set ("");
    storageHostName.set  ("");
    storageDeviceName.set("");
    storageFileName.set  (archiveName);

    if       (archiveName.startsWith("ftp:"))
    {
      // ftp
      storageType.set("ftp");

      String specifier = archiveName.substring(4);
      if (specifier.startsWith("//"))
      {
        Object[] data = new Object[2];

        int index = 2;
        if      (StringParser.parse(specifier.substring(index),"%s:%s@",data,StringParser.QUOTE_CHARS))
        {
          storageLoginName.set((String)data[0]);
          storageLoginPassword.set((String)data[1]);
          index = specifier.indexOf('@')+1;
        }
        else if (StringParser.parse(specifier.substring(index),"%s@",data,StringParser.QUOTE_CHARS))
        {
          storageLoginName.set((String)data[0]);
          index = specifier.indexOf('@')+1;
        }
        if (StringParser.parse(specifier.substring(index),"%s/%s",data,StringParser.QUOTE_CHARS))
        {
          storageHostName.set((String)data[0]);
          storageFileName.set((String)data[1]);
        }
        else
        {
          storageFileName.set(specifier);
        }
      }
      else
      {
        storageFileName.set(specifier);
      }
    }
    else if (archiveName.startsWith("scp:"))
    {
      // scp
      storageType.set("scp");

      String specifier = archiveName.substring(4);
      if (specifier.startsWith("//"))
      {
        Object[] data = new Object[3];

        int index = 2;
        if (StringParser.parse(specifier.substring(index),"%s@",data,StringParser.QUOTE_CHARS))
        {
          storageLoginName.set((String)data[0]);
          index = specifier.indexOf('@')+1;
        }
        if      (StringParser.parse(specifier.substring(index),"%s:%d/%s",data,StringParser.QUOTE_CHARS))
        {
          storageHostName.set((String)data[0]);
          storageHostPort.set((Integer)data[1]);
          storageFileName.set((String)data[2]);
        }
        else if (StringParser.parse(specifier.substring(2),"%s/%s",data,StringParser.QUOTE_CHARS))
        {
          storageHostName.set((String)data[0]);
          storageHostPort.set(0);
          storageFileName.set((String)data[1]);
        }
        else
        {
        storageFileName.set(specifier);
        }
      }
      else
      {
        storageFileName.set(specifier);
      }
    }
    else if (archiveName.startsWith("sftp:"))
    {
      // sftp
      storageType.set("sftp");

      String specifier = archiveName.substring(5);
      if (specifier.startsWith("//"))
      {
        Object[] data = new Object[3];

        int index = 2;
        if (StringParser.parse(specifier.substring(index),"%s@",data,StringParser.QUOTE_CHARS))
        {
          storageLoginName.set((String)data[0]);
          index = specifier.indexOf('@')+1;
        }
        if      (StringParser.parse(specifier.substring(index),"%s:%d/%s",data,StringParser.QUOTE_CHARS))
        {
          storageHostName.set((String)data[0]);
          storageHostPort.set((Integer)data[1]);
          storageFileName.set((String)data[2]);
        }
        else if (StringParser.parse(specifier.substring(2),"%s/%s",data,StringParser.QUOTE_CHARS))
        {
          storageHostName.set((String)data[0]);
          storageHostPort.set(0);
          storageFileName.set((String)data[1]);
        }
        else
        {
        storageFileName.set(specifier);
        }
      }
      else
      {
        storageFileName.set(specifier);
      }
    }
    else if (archiveName.startsWith("dvd:"))
    {
      // dvd
      storageType.set("dvd");

      String specifier = archiveName.substring(4);
      if (specifier.startsWith("//"))
      {
        Object[] data = new Object[2];
        if (StringParser.parse(archiveName,"%s/%s",data,StringParser.QUOTE_CHARS))
        {
          storageDeviceName.set((String)data[0]);
          storageFileName.set((String)data[1]);
        }
        else
        {
        storageFileName.set(specifier);
        }
      }
      else
      {
        storageFileName.set(specifier);
      }
    }
    else if (archiveName.startsWith("device:"))
    {
      // dvd
      storageType.set("device");

      String specifier = archiveName.substring(7);
      if (specifier.startsWith("//"))
      {
        Object[] data = new Object[2];
        if (StringParser.parse(archiveName,"%s/%s",data,StringParser.QUOTE_CHARS))
        {
          storageDeviceName.set((String)data[0]);
          storageFileName.set((String)data[1]);
        }
        else
        {
        storageFileName.set(specifier);
        }
      }
      else
      {
        storageFileName.set(specifier);
      }
    }
  }

  //-----------------------------------------------------------------------

  /** find index for insert of tree item in sort list of tree items
   * @param treeItem tree item
   * @param name name of tree item to insert
   * @param data data of tree item to insert
   * @return index in tree item
   */
  private int findFilesTreeIndex(TreeItem treeItem, FileTreeData fileTreeData)
  {
    TreeItem               subTreeItems[] = treeItem.getItems();
    FileTreeDataComparator fileTreeDataComparator = new FileTreeDataComparator(widgetFileTree);

    int index = 0;
    while (   (index < subTreeItems.length)
           && (fileTreeDataComparator.compare(fileTreeData,(FileTreeData)subTreeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update file list of tree item
   * @param treeItem tree item to update
   */
  private void updateFileList(TreeItem treeItem)
  {
    FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
    TreeItem     subTreeItem;

    ArrayList<String> result = new ArrayList<String>();
    BARServer.executeCommand("FILE_LIST "+StringParser.escape(fileTreeData.name),result);

    treeItem.removeAll();
    for (String line : result)
    {
//System.err.println("BARControl.java"+", "+1733+": "+line);
      Object data[] = new Object[10];
      if      (StringParser.parse(line,"FILE %ld %ld %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             size
             date/time
             name
        */
        long   size     = (Long  )data[0];
        long   datetime = (Long  )data[1];
        String name     = (String)data[2];

        fileTreeData = new FileTreeData(name,FileTypes.FILE,size,datetime,new File(name).getName());

        Image image;
        if      (includedPatterns.contains(name))
          image = IMAGE_FILE_INCLUDED;
        else if (excludedPatterns.contains(name))
          image = IMAGE_FILE_EXCLUDED;
        else
          image = IMAGE_FILE;

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"FILE");
        subTreeItem.setText(2,Long.toString(size));
        subTreeItem.setText(3,simpleDateFormat.format(new Date(datetime*1000)));
        subTreeItem.setImage(image);
      }
      else if (StringParser.parse(line,"DIRECTORY %ld %ld %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             size
             date/time
             name
        */
        long   size     = (Long  )data[0];
        long   datetime = (Long  )data[1];
        String name     = (String)data[2];

        fileTreeData = new FileTreeData(name,FileTypes.DIRECTORY,new File(name).getName());

        Image image;
        if      (includedPatterns.contains(name))
          image = IMAGE_DIRECTORY_INCLUDED;
        else if (excludedPatterns.contains(name))
          image = IMAGE_DIRECTORY_EXCLUDED;
        else
          image = IMAGE_DIRECTORY;

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,true);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"DIR");
        subTreeItem.setText(3,simpleDateFormat.format(new Date(datetime*1000)));
        subTreeItem.setImage(image);
      }
      else if (StringParser.parse(line,"LINK %ld %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             date/time
             name
        */
        long   datetime = (Long  )data[0];
        String name     = (String)data[1];

        fileTreeData = new FileTreeData(name,FileTypes.LINK,0,datetime,new File(name).getName());

        Image image;
        if      (includedPatterns.contains(name))
          image = IMAGE_LINK_INCLUDED;
        else if (excludedPatterns.contains(name))
          image = IMAGE_LINK_EXCLUDED;
        else
          image = IMAGE_LINK;


        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"LINK");
        subTreeItem.setText(3,simpleDateFormat.format(new Date(datetime*1000)));
        subTreeItem.setImage(image);
      }
      else if (StringParser.parse(line,"SPECIAL %ld %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             date/time
             name
        */
        long   datetime = (Long  )data[0];
        String name     = (String)data[1];

        fileTreeData = new FileTreeData(name,FileTypes.SPECIAL,0,datetime,name);

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"SPECIAL");
        subTreeItem.setText(3,simpleDateFormat.format(new Date(datetime*1000)));
      }
      else if (StringParser.parse(line,"DEVICE %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             name
        */
        String name = (String)data[0];

        fileTreeData = new FileTreeData(name,FileTypes.DEVICE,name);

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"DEVICE");
      }
      else if (StringParser.parse(line,"SOCKET %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             name
        */
        String name = (String)data[0];

        fileTreeData = new FileTreeData(name,FileTypes.SOCKET,name);

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"SOCKET");
      }
    }
  }

  //-----------------------------------------------------------------------

  /** find index for insert of job in sort list job list
   * @param jobs jobs
   * @param name name to insert
   * @return index in list
   */
  private int findJobListIndex(String name)
  {
    String names[] = widgetJobList.getItems();

    int index = 0;
    while (   (index < names.length)
           && (names[index].compareTo(name) < 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update job list
   */
  private void updateJobList()
  {
    // get job list
    ArrayList<String> result = new ArrayList<String>();
    BARServer.executeCommand("JOB_LIST",result);

    // update job list
    synchronized(widgetJobList)
    {
      jobIds.clear();
      widgetJobList.removeAll();
      for (String line : result)
      {
        Object data[] = new Object[10];
        /* format:
           <id>
           <name>
           <state>
           <type>
           <archivePartSize>
           <compressAlgorithm>
           <cryptAlgorithm>
           <cryptType>
           <lastExecutedDateTime>
           <estimatedRestTime>
        */
  //System.err.println("BARControl.java"+", "+1357+": "+line);
        if (StringParser.parse(line,"%d %S %S %s %d %S %S %S %ld %ld",data,StringParser.QUOTE_CHARS))
        {
  //System.err.println("BARControl.java"+", "+747+": "+data[0]+"--"+data[5]+"--"+data[6]);
          // get data
          int    id   = (Integer)data[0];
          String name = (String )data[1];

          int index = findJobListIndex(name);
          widgetJobList.add(name,index);
          jobIds.put(name,id);
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** add a new job
   */
  private void jobNew()
  {
    Composite composite;
    Label     label;
    Button    button;

    final Shell  dialog = Dialogs.open(shell,"New job",300,SWT.DEFAULT);

    // create widgets
    final Text   widgetJobName;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4);
    {
      label = Widgets.newLabel(composite,"Name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobName = Widgets.newText(composite,null);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4);
    {
      widgetAdd = Widgets.newButton(composite,null,"Add");
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          widget.getShell().close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetJobName.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetAdd.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget  = (Button)selectionEvent.widget;
        String jobName = widgetJobName.getText();
        if (!jobName.equals(""))
        {
          try
          {
            BARServer.executeCommand("JOB_NEW "+StringParser.escape(jobName));
            updateJobList();
          }
          catch (CommunicationError error)
          {
            Dialogs.error(shell,"Cannot create new job:\n\n"+error.getMessage());
          }
        }
        widget.getShell().close();
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    Dialogs.run(dialog);
  }

  /** rename selected job
   */
  private void jobRename()
  {
    Composite composite;
    Label     label;
    Button    button;

    assert selectedJobName != null;
    assert selectedJobId != 0;

    final Shell  dialog = Dialogs.open(shell,"Rename job",300,SWT.DEFAULT);

    // create widgets
    final Text   widgetNewJobName;
    final Button widgetRename;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4);
    {
      label = Widgets.newLabel(composite,"Old name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      label = Widgets.newLabel(composite,selectedJobName);
      Widgets.layout(label,0,1,TableLayoutData.W);

      label = Widgets.newLabel(composite,"New name:");
      Widgets.layout(label,1,0,TableLayoutData.W);

      widgetNewJobName = Widgets.newText(composite,null);
      Widgets.layout(widgetNewJobName,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4);
    {
      widgetRename = Widgets.newButton(composite,null,"Rename");
      Widgets.layout(widgetRename,0,0,TableLayoutData.W);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          widget.getShell().close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetNewJobName.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetRename.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetRename.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget     = (Button)selectionEvent.widget;
        String newJobName = widgetNewJobName.getText();
        if (!newJobName.equals(""))
        {
          BARServer.executeCommand("JOB_RENAME "+selectedJobId+" "+StringParser.escape(newJobName));
          updateJobList();
        }
        widget.getShell().close();
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    Dialogs.run(dialog);
  }

  /** delete selected job
   */
  private void jobDelete()
  {
    assert selectedJobName != null;
    assert selectedJobId != 0;

    if (Dialogs.confirm(shell,"Delete job","Delete job '"+selectedJobName+"'?"))
    {
      BARServer.executeCommand("JOB_DELETE "+selectedJobId);
      updateJobList();
      Widgets.setEnabled(widgetTabFolder,false);
      clear();
    }
  }

  //-----------------------------------------------------------------------

  /** find index for insert of name in sort list job list
   * @param list list
   * @param pattern pattern to insert
   * @return index in list
   */
  private int findPatternsIndex(List list, String pattern)
  {
    String patterns[] = list.getItems();

    int index = 0;
    while (   (index < patterns.length)
           && (pattern.compareTo(patterns[index]) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update pattern list
   * @param patternType pattern type
   */
  private void updatePatternList(PatternTypes patternType)
  {
    assert selectedJobId != 0;

    ArrayList<String> result = new ArrayList<String>();
    switch (patternType)
    {
      case INCLUDE:
        BARServer.executeCommand("INCLUDE_PATTERNS_LIST "+selectedJobId,result);
        break;
      case EXCLUDE:
        BARServer.executeCommand("EXCLUDE_PATTERNS_LIST "+selectedJobId,result);
        break;
    }

    switch (patternType)
    {
      case INCLUDE:
        includedPatterns.clear();
        widgetIncludedPatterns.removeAll();
        break;
      case EXCLUDE:
        excludedPatterns.clear();
        widgetExcludedPatterns.removeAll();
        break;
    }

    for (String line : result)
    {
      Object[] data = new Object[2];
      if (StringParser.parse(line,"%s %S",data,StringParser.QUOTE_CHARS))
      {
        // get data
        String type    = (String)data[0];
        String pattern = (String)data[1];

        if (!pattern.equals(""))
        {
          switch (patternType)
          {
            case INCLUDE:
              includedPatterns.add(pattern);
              widgetIncludedPatterns.add(pattern,findPatternsIndex(widgetIncludedPatterns,pattern));
              break;
            case EXCLUDE:
              excludedPatterns.add(pattern);
              widgetExcludedPatterns.add(pattern,findPatternsIndex(widgetExcludedPatterns,pattern));
              break;
          }
        }
      }
    }
  }

  /** add new include/exclude pattern
   * @param patternType pattern type
   */
  private boolean patternEdit(final PatternTypes patternType, final String pattern[], String title, String buttonText)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert selectedJobId != 0;

    // create dialog
    final Shell  dialog = Dialogs.open(shell,title,300,70);

    // create widgets
    final Text   widgetPattern;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4);
    {
      label = Widgets.newLabel(composite,"Pattern:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPattern = Widgets.newText(composite,null);
      Widgets.layout(widgetPattern,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4);
    {
      widgetAdd = Widgets.newButton(composite,null,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetPattern.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetAdd.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        pattern[0] = widgetPattern.getText();
        Dialogs.close(dialog,true);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    return (Boolean)Dialogs.run(dialog);
  }

  /** add new include/exclude pattern
   * @param patternType pattern type
   * @param pattern pattern to add to included/exclude list
   */
  private void patternNew(PatternTypes patternType, String pattern)
  {
    assert selectedJobId != 0;

    switch (patternType)
    {
      case INCLUDE:
        {
          BARServer.executeCommand("INCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(pattern));

          includedPatterns.add(pattern);
          widgetIncludedPatterns.add(pattern,findPatternsIndex(widgetIncludedPatterns,pattern));
        }
        break;
      case EXCLUDE:
        {
          BARServer.executeCommand("EXCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(pattern));

          excludedPatterns.add(pattern);
          widgetExcludedPatterns.add(pattern,findPatternsIndex(widgetIncludedPatterns,pattern));
        }
        break;
    }
  }

  /** add new include/exclude pattern
   * @param patternType pattern type
   * @param pattern pattern to add to included/exclude list
   */
  private void patternNew(PatternTypes patternType)
  {
    assert selectedJobId != 0;

    String title = null;
    switch (patternType)
    {
      case INCLUDE:
        title = "New include pattern";
        break;
      case EXCLUDE:
        title = "New exclude pattern";
        break;
    }
    String[] pattern = new String[1];
    if (patternEdit(patternType,pattern,title,"Add"))
    {
      patternNew(patternType,pattern[0]);
    }
  }

  /** delete include/exclude pattern
   * @param patternType pattern type
   * @param pattern pattern to remove from include/exclude list
   */
  private void patternDelete(PatternTypes patternType, String pattern)
  {
    assert selectedJobId != 0;

    switch (patternType)
    {
      case INCLUDE:
        {
          includedPatterns.remove(pattern);

          BARServer.executeCommand("INCLUDE_PATTERNS_CLEAR "+selectedJobId);
          widgetIncludedPatterns.removeAll();
          for (String s : includedPatterns)
          {
            BARServer.executeCommand("INCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(s));
            widgetIncludedPatterns.add(s,findPatternsIndex(widgetIncludedPatterns,s));
          }
        }
        break;
      case EXCLUDE:
        {
          excludedPatterns.remove(pattern);

          BARServer.executeCommand("EXCLUDE_PATTERNS_CLEAR "+selectedJobId);
          widgetExcludedPatterns.removeAll();
          for (String s : excludedPatterns)
          {
            BARServer.executeCommand("EXCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(s));
            widgetExcludedPatterns.add(s,findPatternsIndex(widgetExcludedPatterns,s));
          }
        }
        break;
    }
  }

  /** delete selected include/exclude pattern
   * @param patternType pattern type
   */
  private void patternDelete(PatternTypes patternType)
  {
    assert selectedJobId != 0;

    int index;
    String pattern = null;
    switch (patternType)
    {
      case INCLUDE:
        index = widgetIncludedPatterns.getSelectionIndex();
        if (index >= 0) pattern = widgetIncludedPatterns.getItem(index);
        break;
      case EXCLUDE:
        index = widgetExcludedPatterns.getSelectionIndex();
        if (index >= 0) pattern = widgetExcludedPatterns.getItem(index);
        break;
    }

    if (pattern != null)
    {
      patternDelete(patternType,pattern);
    }
  }

  //-----------------------------------------------------------------------

  /** storage name part data
   */
  class StorageNamePart implements Serializable
  {
    String    string;
    Rectangle bounds;

    /** create name part
     * @param string string or null
     */
    StorageNamePart(String string)
    {
      this.string = string;
      this.bounds = new Rectangle(0,0,0,0);
    }

    /** write storage name part object to object stream
     * Note: must be implented because Java serializaion API cannot write
     *       inner classes without writing outer classes, too!
     * @param out stream
     */
    private void writeObject(java.io.ObjectOutputStream out)
      throws IOException
    {
      out.writeObject(string);
      out.writeObject(bounds);
    }

    /** read storage name part object from object stream
     * Note: must be implented because Java serializaion API cannot read
     *       inner classes without reading outer classes, too!
     * @param in stream
     * @return 
     */
    private void readObject(java.io.ObjectInputStream in)
      throws IOException, ClassNotFoundException
    {
      string = (String)in.readObject();
      bounds = (Rectangle)in.readObject();
    }

    public String toString()
    {
      return "Part {string="+string+", "+bounds+"}";
    }
  }

  /** storage name part transfer class (required for drag&drop)
   */
  static class StorageNamePartTransfer extends ByteArrayTransfer
  {
    private static final String NAME = "StorageNamePart";
    private static final int    ID   = registerType(NAME);

    private static StorageNamePartTransfer instance = new StorageNamePartTransfer();

    public static StorageNamePartTransfer getInstance()
    {
      return instance;
    }

    public void javaToNative(Object object, TransferData transferData)
    {
      if (!validate(object) || !isSupportedType(transferData))
      {
        DND.error(DND.ERROR_INVALID_DATA);
      }

      StorageNamePart storageNamePart = (StorageNamePart)object;
      try
      {
        // write data to a byte array and then ask super to convert to pMedium
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        ObjectOutputStream outputStream = new ObjectOutputStream(byteArrayOutputStream);
        outputStream.writeObject(storageNamePart);
        byte[] buffer = byteArrayOutputStream.toByteArray();
        outputStream.close();

        // call super to convert to pMedium
        super.javaToNative(buffer,transferData);
      }
      catch (IOException exception)
      {
        // do nothing
      }
   }

   public Object nativeToJava (TransferData transferData)
   {
     if (isSupportedType(transferData))
     {
       byte[] buffer = (byte[])super.nativeToJava(transferData);
       if (buffer == null) return null;

       StorageNamePart storageNamePart = null;
       try
       {
         ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream (buffer);
         ObjectInputStream inputStream = new ObjectInputStream(byteArrayInputStream);
         storageNamePart = (StorageNamePart)inputStream.readObject();
         inputStream.close ();
       }
       catch (java.lang.ClassNotFoundException exception)
       {
         return null;
       }
       catch (IOException exception)
       {
         return null;
       }

       return storageNamePart;
     }

     return null;
    }

    protected String[] getTypeNames()
    {
      return new String[]{NAME};
    }

    protected int[] getTypeIds()
    {
      return new int[]{ID};
    }

    protected boolean validate(Object object)
    {
      return (object != null && (object instanceof StorageNamePart));
    }
  }

  /** storage name editor
   */
  class StorageFileNameEditor
  {
    // global variables
    final Display display;

    // colors
    final Color   textForegroundColor;
    final Color   textBackgroundColor;
    final Color   textHighlightColor;
    final Color   separatorForegroundColor;
    final Color   separatorBackgroundColor;
    final Color   separatorHighlightColor;

    // widgets
    final Canvas  widgetFileName;
    final Label   widgetExample;
    final Text    widgetText;

    // variables
    LinkedList<StorageNamePart> storageNamePartList = new LinkedList<StorageNamePart>();
    StorageNamePart             selectedNamePart    = null;
    StorageNamePart             highlightedNamePart = null;

    /** create name part editor
     * @param parentComposite parent composite
     */
    StorageFileNameEditor(Composite parentComposite, String fileName)
    {
      Composite  composite;
      Label      label;
      DragSource dragSource;
      DropTarget dropTarget;

      display = parentComposite.getDisplay();

      textForegroundColor      = display.getSystemColor(SWT.COLOR_BLACK);
      textBackgroundColor      = display.getSystemColor(SWT.COLOR_GRAY);
      textHighlightColor       = new Color(null,0xFA,0x0A,0x0A);
      separatorForegroundColor = textForegroundColor;
      separatorBackgroundColor = new Color(null,0xAD,0xD8,0xE6);
      separatorHighlightColor  = textHighlightColor;

      composite = Widgets.newComposite(parentComposite,SWT.NONE);
      Widgets.layout(composite,0,0,TableLayoutData.WE);
      {
        label = Widgets.newLabel(composite,"File name:");
        Widgets.layout(label,0,0,TableLayoutData.WE);

        widgetFileName = Widgets.newCanvas(composite,SWT.BORDER);
        widgetFileName.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_WHITE));
        Widgets.layout(widgetFileName,0,1,TableLayoutData.WE|TableLayoutData.EXPAND,0,0,SWT.DEFAULT,24);
        widgetFileName.addMouseTrackListener(new MouseTrackListener()
        {
          public void mouseEnter(MouseEvent mouseEvent)
          {
          }
          public void mouseExit(MouseEvent mouseEvent)
          {
            clearHighlight();
          }
          public void mouseHover(MouseEvent mouseEvent)
          {
          }
        });
        // Note: needed, because MouseTrackListener.hover() has a delay
        widgetFileName.addMouseMoveListener(new MouseMoveListener()
        {
          public void mouseMove(MouseEvent mouseEvent)
          {
            Point point = new Point(mouseEvent.x,mouseEvent.y);
            setHighlight(point);
          }
        });
        widgetFileName.addKeyListener(new KeyListener()
        {
          public void keyPressed(KeyEvent keyEvent)
          {
//System.err.println("BARControl.java"+", "+6268+": "+selectedNamePart+" "+highlightedNamePart);
            if ((highlightedNamePart != null) && (highlightedNamePart.string != null) && ((keyEvent.keyCode == SWT.DEL) || (keyEvent.keyCode == SWT.BS)))
            {
              remPart(highlightedNamePart);
            }
          }
          public void keyReleased(KeyEvent keyEvent)
          {
          }
        });
        dragSource = new DragSource(widgetFileName,DND.DROP_MOVE);
        dragSource.setTransfer(new Transfer[]{StorageNamePartTransfer.getInstance()});
        dragSource.addDragListener(new DragSourceListener()
        {
          public void dragStart(DragSourceEvent dragSourceEvent)
          {
            Point point = new Point(dragSourceEvent.x,dragSourceEvent.y);
            StorageNamePart storageNamePart = getPart(point);
            if ((storageNamePart != null) && (storageNamePart.string != null))
            {
              selectedNamePart = storageNamePart;
            }
            else
            {
              dragSourceEvent.doit = false;
            }
          }
          public void dragSetData(DragSourceEvent dragSourceEvent)
          {
            dragSourceEvent.data = selectedNamePart;
          }
          public void dragFinished(DragSourceEvent dragSourceEvent)
          {
//System.err.println("BARControl.java"+", "+6261+": "+dragSourceEvent.data+" "+dragSourceEvent.doit+" "+dragSourceEvent.detail+" "+selectedNamePart);
            if (dragSourceEvent.detail == DND.DROP_MOVE)
            {
              remPart(selectedNamePart);
            }
            selectedNamePart = null;
            widgetFileName.redraw();
          }
        });
        dropTarget = new DropTarget(widgetFileName,DND.DROP_MOVE|DND.DROP_COPY);
        dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance(),StorageNamePartTransfer.getInstance()});
	dropTarget.addDropListener(new DropTargetAdapter()
        {
          public void dragLeave(DropTargetEvent dropTargetEvent)
          {
            clearHighlight();
          }
          public void dragOver(DropTargetEvent dropTargetEvent)
          {
            Point point = display.map(null,widgetFileName,dropTargetEvent.x,dropTargetEvent.y);
            setHighlight(point);
          }
          public void drop(DropTargetEvent dropTargetEvent)
          {
            if (dropTargetEvent.data != null)
            {
              Point point = display.map(null,widgetFileName,dropTargetEvent.x,dropTargetEvent.y);
              synchronized(storageNamePartList)
              {
                // find part to replace
                int index = 0;
                while ((index < storageNamePartList.size()) && !storageNamePartList.get(index).bounds.contains(point))
                {
                  index++;
                }

                // replace/insert part
                addPart(index,(String)dropTargetEvent.data);
              }
            }
            else
            {
              dropTargetEvent.detail = DND.DROP_NONE;
            }
          }
	});
        widgetFileName.addPaintListener(new PaintListener()
        {
          public void paintControl(PaintEvent paintEvent)
          {
            redraw(paintEvent);
          }
        }); 

        label = Widgets.newLabel(composite,Widgets.loadImage(display,"trashcan.gif"),SWT.BORDER);
        Widgets.layout(label,0,2,TableLayoutData.DEFAULT);
        dropTarget = new DropTarget(label,DND.DROP_MOVE);
        dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance(),StorageNamePartTransfer.getInstance()});
	dropTarget.addDropListener(new DropTargetAdapter()
        {
          public void dragLeave(DropTargetEvent dropTargetEvent)
          {
          }
          public void dragOver(DropTargetEvent dropTargetEvent)
          {
//System.err.println("BARControl.java"+", "+6353+": "+dropTargetEvent);
          }
          public void drop(DropTargetEvent dropTargetEvent)
          {
//System.err.println("BARControl.java"+", "+6350+": "+dropTargetEvent.data+" "+dropTargetEvent.currentDataType);
            if (dropTargetEvent.data != null)
            {
              if      (dropTargetEvent.data instanceof String)
              {
                // ignored
              }
              else if (dropTargetEvent.data instanceof StorageNamePart)
              {
                // OK
              }
              else
              {
                dropTargetEvent.detail = DND.DROP_NONE;
              }
            }
            else
            {
              dropTargetEvent.detail = DND.DROP_NONE;
            }
          }
	});

        label = Widgets.newLabel(composite,"Example:");
        Widgets.layout(label,1,0,TableLayoutData.W);

        widgetExample = Widgets.newView(composite);
        Widgets.layout(widgetExample,1,1,TableLayoutData.WE,0,2);
      }

      composite = Widgets.newComposite(parentComposite,SWT.NONE);
      Widgets.layout(composite,1,0,TableLayoutData.NSWE);
      {
        // column 1
        addDragAndDrop(composite,"-","text '-'",                          0, 0);
        addDragAndDrop(composite,".bar","text '.bar'",                    1, 0);
        widgetText = Widgets.newText(composite,null);
        addDragAndDrop(composite,"Text",widgetText,                       2, 0);

        addDragAndDrop(composite,"#","part number 1 digit",              4, 0);
        addDragAndDrop(composite,"##","part number 2 digits",            5, 0);
        addDragAndDrop(composite,"###","part number 3 digits",           6, 0);
        addDragAndDrop(composite,"####","part number 4 digits",          7, 0);

        addDragAndDrop(composite,"%type","archive type: full,incremental",9, 0);
        addDragAndDrop(composite,"%last","'-last' if last archive part",  10,0);

        // column 2
        addDragAndDrop(composite,"%d","day 01..31",                  0, 1);
        addDragAndDrop(composite,"%j","day of year 001..366",        1, 1);
        addDragAndDrop(composite,"%m","month 01..12",                2, 1);
        addDragAndDrop(composite,"%b","month name",                  3, 1);
        addDragAndDrop(composite,"%B","full month name",             4, 1);
        addDragAndDrop(composite,"%H","hour 00..23",                 5, 1);
        addDragAndDrop(composite,"%I","hour 00..12",                 6, 1);
        addDragAndDrop(composite,"%M","minute 00..59",               7, 1);
        addDragAndDrop(composite,"%p","'AM' or 'PM'",                8, 1);
        addDragAndDrop(composite,"%P","'am' or 'pm'",                9, 1);
        addDragAndDrop(composite,"%a","week day name",               10,1);
        addDragAndDrop(composite,"%A","full week day name",          11,1);
        addDragAndDrop(composite,"%u","day of week 1..7",            12,1);
        addDragAndDrop(composite,"%w","day of week 0..6",            13,1);
        addDragAndDrop(composite,"%U","week number 1..52",           14,1);
        addDragAndDrop(composite,"%C","century two digits",          15,1);
        addDragAndDrop(composite,"%Y","year four digits",            16,1);
        addDragAndDrop(composite,"%S","seconds since 1.1.1970 00:00",17,1);
        addDragAndDrop(composite,"%Z","time-zone abbreviation",      18,1);

        // column 3
        addDragAndDrop(composite,"%%","%",                           0, 2);
        addDragAndDrop(composite,"%#","#",                           1, 2);
      }

      // set name
      setFileName(fileName);
    }

    /** set file name, parse parts
     * @param file name
     */
    void setFileName(String fileName)
    {
      synchronized(storageNamePartList)
      {
        // clear existing list
        storageNamePartList.clear();

        // parse file name
        storageNamePartList.add(new StorageNamePart(null));
        int z = 0;
        while (z < fileName.length())
        {
          StringBuffer part;

          // get next text part
          part = new StringBuffer();
          while (   (z < fileName.length())
                 && (fileName.charAt(z) != '%')
                 && (fileName.charAt(z) != '#')
                )
          {
            part.append(fileName.charAt(z)); z++;
          }
          storageNamePartList.add(new StorageNamePart(part.toString()));
          storageNamePartList.add(new StorageNamePart(null));

          if (z < fileName.length())
          {
            switch (fileName.charAt(z))
            {
              case '%':
                // add variable part
                part = new StringBuffer();
                part.append('%'); z++;
                if ((z < fileName.length()) && (fileName.charAt(z) == '%'))
                {
                  part.append('%'); z++;
                }
                else
                {
                  while ((z < fileName.length()) && (Character.isLetterOrDigit(fileName.charAt(z))))
                  {
                    part.append(fileName.charAt(z)); z++;
                  }
                }
                storageNamePartList.add(new StorageNamePart(part.toString()));
                storageNamePartList.add(new StorageNamePart(null));
                break;
              case '#':
                // add number part
                part = new StringBuffer();
                while ((z < fileName.length()) && (fileName.charAt(z) == '#'))
                {
                  part.append(fileName.charAt(z)); z++;
                }
                storageNamePartList.add(new StorageNamePart(part.toString()));
                storageNamePartList.add(new StorageNamePart(null));
                break;
            }
          }
        }
      }

      // redraw
      widgetFileName.redraw();
      updateExample();
    }

    /** get file name
     * @return file name
     */
    String getFileName()
    {
      StringBuffer fileName = new StringBuffer();
      for (StorageNamePart storageNamePart : storageNamePartList)
      {
        if (storageNamePart.string != null)
        {
          fileName.append(storageNamePart.string);
        }
      }

      return fileName.toString();
    }

    //-----------------------------------------------------------------------

    /** add part
     * @param composite composite to add into
     * @param text text to show
     * @param description of part
     * @param row,column row/column
     */
    private void addDragAndDrop(Composite composite, String text, String description, int row, int column)
    {
      Label label;

      label = Widgets.newLabel(composite,text,SWT.LEFT|SWT.BORDER);
      label.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_GRAY));
      label.setData(text);
      Widgets.layout(label,row,column*2+0,TableLayoutData.W);
      DragSource dragSource = new DragSource(label,DND.DROP_MOVE|DND.DROP_COPY);
      dragSource.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dragSource.addDragListener(new DragSourceListener()
      {
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
        }
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          Control control = ((DragSource)dragSourceEvent.widget).getControl();
          dragSourceEvent.data = (String)control.getData();
        }
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
        }
      });

      label = Widgets.newLabel(composite,description,SWT.LEFT|SWT.BORDER);
      Widgets.layout(label,row,column*2+1,TableLayoutData.W);
    }

    /** add part
     * @param composite composite to add into
     * @param text text to show
     * @param control control to add
     * @param row,column row/column
     */
    private void addDragAndDrop(Composite composite, String text, Control control, int row, int column)
    {
      Label label;

      label = Widgets.newLabel(composite,text,SWT.LEFT|SWT.BORDER);
      label.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_GRAY));
      label.setData(control);
      Widgets.layout(label,row,column*2+0,TableLayoutData.W);
      DragSource dragSource = new DragSource(label,DND.DROP_MOVE|DND.DROP_COPY);
      dragSource.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dragSource.addDragListener(new DragSourceListener()
      {
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
          Control control = ((DragSource)dragSourceEvent.widget).getControl();
          Widget widget = (Widget)control.getData();
          if (widget instanceof Text)
          {
            String text = ((Text)widget).getText();
            if ((text == null) || (text.length() == 0)) dragSourceEvent.doit = false;
          }
        }
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          Control control = ((DragSource)dragSourceEvent.widget).getControl();
          Widget widget = (Widget)control.getData();
          if (widget instanceof Text)
          {
            dragSourceEvent.data = ((Text)widget).getText();
            if (dragSourceEvent.data.equals("")) dragSourceEvent.doit = false;
          }
        }
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
        }
      });

      Widgets.layout(control,row,column*2+1,TableLayoutData.WE);
    }

    /** add part
     * @param index index to add/insert part
     * @param string part to add
     */
    private void addPart(int index, String string)
    {
      boolean redrawFlag = false;

      synchronized(storageNamePartList)
      {
        if (index < storageNamePartList.size())
        {
          if (storageNamePartList.get(index).string != null)
          {
            // replace
            storageNamePartList.get(index).string = string;
          }
          else
          {
            // insert
            storageNamePartList.add(index+1,new StorageNamePart(string));
            storageNamePartList.add(index+2,new StorageNamePart(null));
          }
        }
        else
        {
          // add
          storageNamePartList.add(new StorageNamePart(string));
          storageNamePartList.add(new StorageNamePart(null));
        }
        redrawFlag = true;
      }

      if (redrawFlag)
      {
        widgetFileName.redraw();
        updateExample();
      }
    }

    /** remove part
     * @param storageNamePart storage name part to remove
     */
    private void remPart(StorageNamePart storageNamePart)
    {
      boolean redrawFlag = false;

      synchronized(storageNamePartList)
      {
        // find part to delete
        int index = 0;
        while ((index < storageNamePartList.size()) && (storageNamePartList.get(index) != storageNamePart))
        {
          index++;
        }

        // delete part and separator
        if (index < storageNamePartList.size())
        {
          storageNamePartList.remove(index);
          if ((index < storageNamePartList.size()) && (storageNamePartList.get(index).string == null))
          {
            storageNamePartList.remove(index);
          }
          redrawFlag = true;
        }
      }

      if (redrawFlag)
      {
        widgetFileName.redraw();
        updateExample();
      }
    }

    /** redraw part widget content
     * @param paintEvent paint event
     */
    private void redraw(PaintEvent paintEvent)
    {
      GC        gc         = paintEvent.gc;
      Rectangle clientArea = widgetFileName.getClientArea();
      Color     color;

      int x = 0;
      synchronized(storageNamePartList)
      {
        for (StorageNamePart storageNamePart : storageNamePartList)
        {
          if (storageNamePart.string != null)
          {
            Point size = Widgets.getTextSize(widgetFileName,storageNamePart.string);
            if   ((storageNamePart == highlightedNamePart) || (storageNamePart == selectedNamePart)) color = textHighlightColor;
            else                                                                                     color = textBackgroundColor;
            gc.setBackground(color);
            gc.setForeground(textForegroundColor);
            gc.drawString(storageNamePart.string,x,0);
            storageNamePart.bounds = new Rectangle(x,0,size.x,clientArea.height);
            x += size.x;
          }
          else
          {
            if      (storageNamePart == highlightedNamePart) color = separatorHighlightColor;
            else                                             color = separatorBackgroundColor;
            gc.setBackground(color);
            gc.fillRectangle(x,0,8,clientArea.height-1);
            gc.setForeground(separatorForegroundColor);
            gc.drawRectangle(x,0,8,clientArea.height-1);
            storageNamePart.bounds = new Rectangle(x,0,8,clientArea.height);
            x += 8+1;
          }
        }
      }
    }

    /** update example line
     */
    private void updateExample()
    {
      StringBuffer exampleName = new StringBuffer();

      synchronized(storageNamePartList)
      {
        for (StorageNamePart storageNamePart : storageNamePartList)
        {
          if (storageNamePart.string != null)
          {
            if      (storageNamePart.string.startsWith("#"))
            {
              int z = 0;
              while ((z < storageNamePart.string.length()) && (storageNamePart.string.charAt(z) == '#'))
              {
                exampleName.append("1234567890".charAt(z%10));
                z++;
              }
            }
            else if (storageNamePart.string.equals("%type"))
              exampleName.append("full");
            else if (storageNamePart.string.equals("%last"))
              exampleName.append("-last");
            else if (storageNamePart.string.equals("%d"))
              exampleName.append("24");
            else if (storageNamePart.string.equals("%j"))
              exampleName.append("354");
            else if (storageNamePart.string.equals("%m"))
              exampleName.append("12");
            else if (storageNamePart.string.equals("%b"))
              exampleName.append("Dec");
            else if (storageNamePart.string.equals("%B"))
              exampleName.append("December");
            else if (storageNamePart.string.equals("%H"))
              exampleName.append("23");
            else if (storageNamePart.string.equals("%I"))
              exampleName.append("11");
            else if (storageNamePart.string.equals("%M"))
              exampleName.append("55");
            else if (storageNamePart.string.equals("%p"))
              exampleName.append("PM");
            else if (storageNamePart.string.equals("%P"))
              exampleName.append("pm");
            else if (storageNamePart.string.equals("%a"))
              exampleName.append("Mon");
            else if (storageNamePart.string.equals("%A"))
              exampleName.append("Monday");
            else if (storageNamePart.string.equals("%u"))
              exampleName.append("1");
            else if (storageNamePart.string.equals("%w"))
              exampleName.append("0");
            else if (storageNamePart.string.equals("%U"))
              exampleName.append("51");
            else if (storageNamePart.string.equals("%C"))
              exampleName.append("07");
            else if (storageNamePart.string.equals("%Y"))
              exampleName.append("2007");
            else if (storageNamePart.string.equals("%S"))
              exampleName.append("1198598100");
            else if (storageNamePart.string.equals("%Z"))
              exampleName.append("JST");
            else if (storageNamePart.string.equals("%%"))
              exampleName.append("%");
            else if (storageNamePart.string.equals("%#"))
              exampleName.append("#");
            else
              exampleName.append(storageNamePart.string);
          }
        }
      }
      widgetExample.setText(exampleName.toString());
    }

  /** find part at location x,y
   * @param point location
   * @return part or null
   */
    private StorageNamePart getPart(Point point)
    {
      synchronized(storageNamePartList)
      {
        for (StorageNamePart storageNamePart : storageNamePartList)
        {
          if (storageNamePart.bounds.contains(point))
          {
            return storageNamePart;
          }
        }
      }

      return null;
    }

    /** clear part highlighting
     */
    private void clearHighlight()
    {
      if (highlightedNamePart != null)
      {
        highlightedNamePart = null;
        widgetFileName.redraw();
      }
    }

    /** set highlighting of part
     * @param point mouse position
     */
    private void setHighlight(Point point)
    {
      boolean redrawFlag = false;

      synchronized(storageNamePartList)
      {
        // find part to highlight
        StorageNamePart storageNamePart = getPart(point);

        // clear previous highlighting
        if ((highlightedNamePart != null) && (storageNamePart != highlightedNamePart))
        {
          highlightedNamePart = null;
          redrawFlag = true;
        }

        // highlight part
        if (storageNamePart != null)
        {
          highlightedNamePart = storageNamePart;
          redrawFlag = true;
        }
      }

      if (redrawFlag) widgetFileName.redraw();
    }
  };

  /** edit storage file name
   */
  //  private void storageFileNameEdit()
  void storageFileNameEdit()
  {
    Composite composite;
    Label     label;
    Button    button;
    Composite subComposite;

    assert selectedJobId != 0;

    // create dialog
    final Shell dialog = Dialogs.open(shell,
                                      "Edit storage file name",
                                      SWT.DEFAULT,SWT.DEFAULT,
                                      new double[]{0,1,0},null
                                     );

    // create widgets
    final StorageFileNameEditor storageFileNameEditor;
    final Button                widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    storageFileNameEditor = new StorageFileNameEditor(composite,storageFileName.getString());

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,2,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4);
    {
      widgetSave = Widgets.newButton(composite,null,"Save");
      Widgets.layout(widgetSave,0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetSave.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        storageFileName.set(storageFileNameEditor.getFileName());
        Dialogs.close(dialog,true);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    Dialogs.run(dialog);
  }

  //-----------------------------------------------------------------------

  /** find index for insert of schedule data in sorted schedule table
   * @param scheduleData schedule data
   * @return index in schedule table
   */
  private int findScheduleListIndex(ScheduleData scheduleData)
  {
    TableItem              tableItems[] = widgetScheduleList.getItems();
    ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleList);

    int index = 0;
    while (   (index < tableItems.length)
           && (scheduleDataComparator.compare(scheduleData,(ScheduleData)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** clear schedule list
   */
  private void clearScheduleList()
  {
    synchronized(scheduleList)
    {
      scheduleList.clear();
      widgetScheduleList.removeAll();
    }
  }

  /** update schedule list
   */
  private void updateScheduleList()
  {
    // get schedule list
    ArrayList<String> result = new ArrayList<String>();
    BARServer.executeCommand("SCHEDULE_LIST "+selectedJobId,result);

    // update schedule list
    synchronized(scheduleList)
    {
      scheduleList.clear();
      widgetScheduleList.removeAll();
      for (String line : result)
      {
        Object data[] = new Object[8];
        /* format:
           <date y-m-d>
           <weekDay>
           <time h:m>
           <enabled>
           <type>
        */
//System.err.println("BARControl.java"+", "+1357+": "+line);
        if (StringParser.parse(line,"%S-%S-%S %S %S:%S %y %S",data,StringParser.QUOTE_CHARS))
        {
//System.err.println("BARControl.java"+", "+747+": "+data[0]+"--"+data[5]+"--"+data[6]);
          // get data
          String  year    = (String)data[0];
          String  month   = (String)data[1];
          String  day     = (String)data[2];
          String  weekDay = (String)data[3];
          String  hour    = (String)data[4];
          String  minute  = (String)data[5];
          boolean enabled = (Boolean)data[6];
          String  type    = (String)data[7];

          ScheduleData scheduleData = new ScheduleData(year,month,day,weekDay,hour,minute,enabled,type);

          scheduleList.add(scheduleData);
          TableItem tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
          tableItem.setData(scheduleData);
          tableItem.setText(0,scheduleData.getDate());
          tableItem.setText(1,scheduleData.weekDay);
          tableItem.setText(2,scheduleData.getTime());
          tableItem.setText(3,scheduleData.getEnabled());
          tableItem.setText(4,scheduleData.type);
        }
      }
    }
  }

  /** edit schedule data
   * @param scheduleData schedule data
   * @param title title text
   * @param buttonText button text
   * @return true if edit OK, false otherwise
   */
  private boolean scheduleEdit(final ScheduleData scheduleData, String title, String buttonText)
  {
    Composite composite;
    Label     label;
    Button    button;
    Composite subComposite;

    assert selectedJobId != 0;

    // create dialog
    final Shell dialog = Dialogs.open(shell,title,300,70);

    // create widgets
    final Combo  widgetYear,widgetMonth,widgetDay,widgetWeekDay;
    final Combo  widgetHour,widgetMinute;
    final Button widgetTypeDefault,widgetTypeNormal,widgetTypeFull,widgetTypeIncremental,widgetEnabled;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4,4);
    {
      label = Widgets.newLabel(composite,"Date:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetYear = Widgets.newOptionMenu(subComposite,null);
        widgetYear.setItems(new String[]{"*","2008","2009","2010","2011","2012","2013","2014","2015"});
        widgetYear.setText(scheduleData.getYear()); if (widgetYear.getText().equals("")) widgetYear.setText("*");
        if (widgetYear.getText().equals("")) widgetYear.setText("*");
        Widgets.layout(widgetYear,0,0,TableLayoutData.W);

        widgetMonth = Widgets.newOptionMenu(subComposite,null);
        widgetMonth.setItems(new String[]{"*","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"});
        widgetMonth.setText(scheduleData.getMonth()); if (widgetMonth.getText().equals("")) widgetMonth.setText("*");
        Widgets.layout(widgetMonth,0,1,TableLayoutData.W);

        widgetDay = Widgets.newOptionMenu(subComposite,null);
        widgetDay.setItems(new String[]{"*","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31"});
        widgetDay.setText(scheduleData.getDay()); if (widgetDay.getText().equals("")) widgetDay.setText("*");
        Widgets.layout(widgetDay,0,2,TableLayoutData.W);

        widgetWeekDay = Widgets.newOptionMenu(subComposite,null);
        widgetWeekDay.setItems(new String[]{"*","Mon","Tue","Wed","Thu","Fri","Sat","Sun"});
        widgetWeekDay.setText(scheduleData.weekDay); if (widgetWeekDay.getText().equals("")) widgetWeekDay.setText("*");
        Widgets.layout(widgetWeekDay,0,3,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,"Time:");
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        widgetHour = Widgets.newOptionMenu(subComposite,null);
        widgetHour.setItems(new String[]{"*","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetHour.setText(scheduleData.getHour()); if (widgetHour.getText().equals("")) widgetHour.setText("*");
        Widgets.layout(widgetHour,0,0,TableLayoutData.W);

        widgetMinute = Widgets.newOptionMenu(subComposite,null);
        widgetMinute.setItems(new String[]{"*","0","5","10","15","20","30","35","40","45","50","55"});
        widgetMinute.setText(scheduleData.getMinute()); if (widgetMinute.getText().equals("")) widgetMinute.setText("*");
        Widgets.layout(widgetMinute,0,1,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,"Type:");
      Widgets.layout(label,2,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,2,1,TableLayoutData.WE);
      {
        widgetTypeDefault = Widgets.newRadio(subComposite,null,"*");
        Widgets.layout(widgetTypeDefault,0,0,TableLayoutData.W);
        widgetTypeDefault.setSelection(scheduleData.type.equals("*"));

        widgetTypeNormal = Widgets.newRadio(subComposite,null,"normal");
        Widgets.layout(widgetTypeNormal,0,1,TableLayoutData.W);
        widgetTypeNormal.setSelection(scheduleData.type.equals("normal"));

        widgetTypeFull = Widgets.newRadio(subComposite,null,"full");
        Widgets.layout(widgetTypeFull,0,2,TableLayoutData.W);
        widgetTypeFull.setSelection(scheduleData.type.equals("full"));

        widgetTypeIncremental = Widgets.newRadio(subComposite,null,"incremental");
        Widgets.layout(widgetTypeIncremental,0,3,TableLayoutData.W);
        widgetTypeIncremental.setSelection(scheduleData.type.equals("incremental"));
      }

      label = Widgets.newLabel(composite,"Options:");
      Widgets.layout(label,3,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,3,1,TableLayoutData.WE);
      {
        widgetEnabled = Widgets.newCheckbox(subComposite,null,"enabled");
        Widgets.layout(widgetEnabled,0,0,TableLayoutData.W);
        widgetEnabled.setSelection(scheduleData.enabled);
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,0,4,4);
    {
      widgetAdd = Widgets.newButton(composite,null,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
/*
    widgetPattern.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetAdd.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
*/
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        scheduleData.setDate(widgetYear.getText(),widgetMonth.getText(),widgetDay.getText());
        scheduleData.weekDay = widgetWeekDay.getText();
        scheduleData.setTime(widgetHour.getText(),widgetMinute.getText());
        scheduleData.enabled = widgetEnabled.getSelection();
        if      (widgetTypeNormal.getSelection())      scheduleData.type = "normal";
        else if (widgetTypeFull.getSelection())        scheduleData.type = "full";
        else if (widgetTypeIncremental.getSelection()) scheduleData.type = "incremental";
        else                                           scheduleData.type = "*";

        Dialogs.close(dialog,true);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    return (Boolean)Dialogs.run(dialog);
  }

  /** create new schedule entry
   */
  private void scheduleNew()
  {
    assert selectedJobId != 0;

    ScheduleData scheduleData = new ScheduleData();
    if (scheduleEdit(scheduleData,"New schedule","Add"))
    {
      BARServer.executeCommand("SCHEDULE_ADD "+selectedJobId+" "+scheduleData.getDate()+" "+scheduleData.weekDay+" "+scheduleData.getTime()+" "+scheduleData.getEnabled()+" "+scheduleData.type);

      scheduleList.add(scheduleData);
      TableItem tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
      tableItem.setData(scheduleData);
      tableItem.setText(0,scheduleData.getDate());
      tableItem.setText(1,scheduleData.weekDay);
      tableItem.setText(2,scheduleData.getTime());
      tableItem.setText(3,scheduleData.getEnabled());
      tableItem.setText(3,scheduleData.type);   
    }
  }

  /** edit schedule entry
   */
  private void scheduleEdit()
  {
    assert selectedJobId != 0;

    int index = widgetScheduleList.getSelectionIndex();
    if (index >= 0)
    {
      TableItem tableItem = widgetScheduleList.getItem(index);

      ScheduleData scheduleData = (ScheduleData)tableItem.getData();
      if (scheduleEdit(scheduleData,"Edit schedule","Save"))
      {
        BARServer.executeCommand("SCHEDULE_CLEAR "+selectedJobId);
        for (ScheduleData data : scheduleList)
        {
          BARServer.executeCommand("SCHEDULE_ADD "+selectedJobId+" "+data.getDate()+" "+data.weekDay+" "+data.getTime()+" "+data.getEnabled()+" "+data.type);
        }
        
        tableItem.dispose();
        tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
        tableItem.setData(scheduleData);
        tableItem.setText(0,scheduleData.getDate());
        tableItem.setText(1,scheduleData.weekDay);
        tableItem.setText(2,scheduleData.getTime());
        tableItem.setText(3,scheduleData.getEnabled());   
        tableItem.setText(4,scheduleData.type);   
      }
    }
  }

  /** delete schedule entry
   */
  private void scheduleDelete()
  {
    assert selectedJobId != 0;

    int index = widgetScheduleList.getSelectionIndex();
    if (index >= 0)
    {
      TableItem tableItem = widgetScheduleList.getItem(index);

      ScheduleData scheduleData = (ScheduleData)tableItem.getData();

      scheduleList.remove(scheduleData);

      BARServer.executeCommand("SCHEDULE_CLEAR "+selectedJobId);
      for (ScheduleData data : scheduleList)
      {
        BARServer.executeCommand("SCHEDULE_ADD "+selectedJobId+" "+data.getDate()+" "+data.weekDay+" "+data.getTime()+" "+data.getEnabled()+" "+data.type);
      }

      tableItem.dispose();
    }
  }

  //-----------------------------------------------------------------------

  private void clearJobData()
  {
// NYI: rest?
    widgetIncludedPatterns.removeAll();
    widgetExcludedPatterns.removeAll();
  }

  /** update job data
   * @param name name of job
   */
  private void updateJobData()
  {
    ArrayList<String> result = new ArrayList<String>();
    Object[]          data;

    // clear
    clearJobData();

    if (selectedJobId > 0)
    {
      // get job data
      skipUnreadable.set(BARServer.getBoolean(selectedJobId,"skip-unreadable"));
      overwriteFiles.set(BARServer.getBoolean(selectedJobId,"overwrite-files"));

      parseArchiveName(BARServer.getString(selectedJobId,"archive-name"));
      archiveType.set(BARServer.getString(selectedJobId,"archive-type"));
      archivePartSize.set(Units.parseByteSize(BARServer.getString(selectedJobId,"archive-part-size")));
      archivePartSizeFlag.set(archivePartSize.getLong() > 0);
      compressAlgorithm.set(BARServer.getString(selectedJobId,"compress-algorithm"));
      cryptAlgorithm.set(BARServer.getString(selectedJobId,"crypt-algorithm"));
      cryptType.set(BARServer.getString(selectedJobId,"crypt-type"));
      cryptPublicKeyFileName.set(BARServer.getString(selectedJobId,"crypt-public-key"));
      incrementalListFileName.set(BARServer.getString(selectedJobId,"incremental-list-file"));
      overwriteArchiveFiles.set(BARServer.getBoolean(selectedJobId,"overwrite-archive-files"));
      sshPublicKeyFileName.set(BARServer.getString(selectedJobId,"ssh-public-key"));
      sshPrivateKeyFileName.set(BARServer.getString(selectedJobId,"ssh-private-key"));
/* NYI ???
      maxBandWidth.set(Units.parseByteSize(BARServer.getString(jobId,"max-band-width")));
      maxBandWidthFlag.set(maxBandWidth.getLong() > 0);
*/
      volumeSize.set(Units.parseByteSize(BARServer.getString(selectedJobId,"volume-size")));
      ecc.set(BARServer.getBoolean(selectedJobId,"ecc"));

      updatePatternList(PatternTypes.INCLUDE);
      updatePatternList(PatternTypes.EXCLUDE);
    }
  }

  /** clear all data
   */
  private void clear()
  {
    clearJobData();
    clearScheduleList();
  }

  /** update all data
   */
  private void update()
  {
    updateJobData();
    updateScheduleList();
  }
}

/* end of file */
