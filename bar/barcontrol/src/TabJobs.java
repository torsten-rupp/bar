/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TabJobs.java,v $
* $Revision: 1.29 $
* $Author: torsten $
* Contents: jobs tab
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
// base
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
import java.util.ListIterator;

// graphics
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
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Cursor;
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
  /** entry types
   */
  enum EntryTypes
  {
    FILE,
    IMAGE
  };

  /** pattern types
   */
  enum PatternTypes
  {
    GLOB,
    REGEX,
    EXTENDED_REGEX
  };

  /** file types
   */
  enum FileTypes
  {
    FILE,
    DIRECTORY,
    LINK,
    HARDLINK,
    SPECIAL,
    UNKNOWN
  };

  enum SpecialTypes
  {
    NONE,

    CHARACTER_DEVICE,
    BLOCK_DEVICE,
    FIFO,
    SOCKET,
    OTHER
  };

  /** file tree data
   */
  class FileTreeData
  {
    String       name;
    FileTypes    fileType;
    long         size;
    long         dateTime;
    String       title;
    SpecialTypes specialType;

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param size file size [bytes]
     * @param dateTime file date/time [s]
     * @param title title to display
     * @param specialType special type
     */
    FileTreeData(String name, FileTypes fileType, long size, long dateTime, String title, SpecialTypes specialType)
    {
      this.name        = name;
      this.fileType    = fileType;
      this.size        = size;
      this.dateTime    = dateTime;
      this.title       = title;
      this.specialType = specialType;
    }

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param size file size [bytes]
     * @param dateTime file date/time [s]
     * @param title title to display
     */
    FileTreeData(String name, FileTypes fileType, long size, long dateTime, String title)
    {
      this(name,fileType,size,dateTime,title,SpecialTypes.NONE);
    }

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param dateTime file date/time [s]
     * @param title title to display
     */
    FileTreeData(String name, FileTypes fileType, long dateTime, String title)
    {
      this(name,fileType,0L,dateTime,title);
    }

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param title title to display
     */
    FileTreeData(String name, FileTypes fileType, String title)
    {
      this(name,fileType,0L,title);
    }

    /** create file tree data
     * @param name file name
     * @param specialType special type
     * @param size file size [bytes]
     * @param dateTime file date/time [s]
     * @param title title to display
     */
    FileTreeData(String name, SpecialTypes specialType, long size, long dateTime, String title)
    {
      this(name,FileTypes.SPECIAL,size,dateTime,title,specialType);
    }

    /** create file tree data
     * @param name file name
     * @param specialType special type
     * @param dateTime file date/time [s]
     * @param title title to display
     */
    FileTreeData(String name, SpecialTypes specialType, long dateTime, String title)
    {
      this(name,FileTypes.SPECIAL,0L,dateTime,title,specialType);
    }


    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "File {"+name+", "+fileType+", "+size+" bytes, dateTime="+dateTime+", title="+title+"}";
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
     * @param sortColumn column to sort
     */
    FileTreeDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (tree.getColumn(3) == sortColumn) sortMode = SORTMODE_DATETIME;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** create file data comparator
     * @param tree file tree
     */
    FileTreeDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
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
          return fileTreeData1.fileType.compareTo(fileTreeData2.fileType);
        case SORTMODE_SIZE:
          if      (fileTreeData1.size < fileTreeData2.size) return -1;
          else if (fileTreeData1.size > fileTreeData2.size) return  1;
          else                                              return  0;
        case SORTMODE_DATETIME:
          if      (fileTreeData1.dateTime < fileTreeData2.dateTime) return -1;
          else if (fileTreeData1.dateTime > fileTreeData2.dateTime) return  1;
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
      if (fileTreeData1.fileType == FileTypes.DIRECTORY)
      {
        if (fileTreeData2.fileType == FileTypes.DIRECTORY)
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
        if (fileTreeData2.fileType == FileTypes.DIRECTORY)
        {
          return 1;
        }
        else
        {
          return compareWithoutType(fileTreeData1,fileTreeData2);
        }
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "FileComparator {"+sortMode+"}";
    }
  }

  /** device tree data
   */
  class DeviceTreeData
  {
    String name;
    long   size;

    /** create device tree data
     * @param name device name
     * @param size device size [bytes]
     */
    DeviceTreeData(String name, long size)
    {
      this.name = name;
      this.size = size;
    }

    /** create device tree data
     * @param name device name
     */
    DeviceTreeData(String name)
    {
      this.name = name;
      this.size = 0;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "File {"+name+", "+size+" bytes}";
    }
  };

  /** device data comparator
   */
  class DeviceTreeDataComparator implements Comparator<DeviceTreeData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_NAME = 0;
    private final static int SORTMODE_SIZE = 1;

    private int sortMode;

    /** create device data comparator
     * @param tree device tree
     * @param sortColumn column to sort
     */
    DeviceTreeDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** create device data comparator
     * @param tree device tree
     */
    DeviceTreeDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
    }

    /** compare device tree data without take care about type
     * @param deviceTreeData1, deviceTreeData2 device tree data to compare
     * @return -1 iff deviceTreeData1 < deviceTreeData2,
                0 iff deviceTreeData1 = deviceTreeData2,
                1 iff deviceTreeData1 > deviceTreeData2
     */
    public int compare(DeviceTreeData deviceTreeData1, DeviceTreeData deviceTreeData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return deviceTreeData1.name.compareTo(deviceTreeData2.name);
        case SORTMODE_SIZE:
          if      (deviceTreeData1.size < deviceTreeData2.size) return -1;
          else if (deviceTreeData1.size > deviceTreeData2.size) return  1;
          else                                                  return  0;
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "DeviceComparator {"+sortMode+"}";
    }
  }

  /** Background thread to get directory file size of tree items.
      This thread get the number of files and total size of a
      directories and update the file-tree widget entries. Requests
      are sorted by the depth of the directory and the timeout to
      read the contents. Requests with timeout are reinserted in
      the internal sorted list with an increasing timeout. This
      make sure short running requests are processed first.
   */
  class DirectoryInfoThread extends Thread
  {
    /** directory info request structure
     */
    class DirectoryInfoRequest
    {
      String   name;
      boolean  forceFlag;
      int      depth;
      int      timeout;
      TreeItem treeItem;

      /** create directory info request
       * @param name directory name
       * @param forceFlag true to force update size
       * @param treeItem tree item
       * @param timeout timeout [ms] or -1 for no timeout
       */
      DirectoryInfoRequest(String name, boolean forceFlag, TreeItem treeItem, int timeout)
      {
        this.name      = name;
        this.forceFlag = forceFlag;
        this.depth     = StringUtils.split(name,BARServer.fileSeparator,true).length;
        this.timeout   = timeout;
        this.treeItem  = treeItem;
      }

      /** convert data to string
       * @return string
       */
      public String toString()
      {
      return "DirectoryInfoRequest {"+name+", "+forceFlag+", "+depth+", "+timeout+"}";
      }
    };

    // timeouts to get directory information
    private final int DEFAULT_TIMEOUT = 1*1000;
    private final int TIMEOUT_DETLA   = 2*1000;
    private final int MAX_TIMEOUT     = 5*1000;

    // variables
    private Display                          display;
    private LinkedList<DirectoryInfoRequest> directoryInfoRequestList;

    /** create tree item size thread
     * @param display display
     */
    DirectoryInfoThread(Display display)
    {
      this.display                  = display;
      this.directoryInfoRequestList = new LinkedList<DirectoryInfoRequest>();
      setDaemon(true);
    }

    /**
     * @param
     * @return
     */
    public void run()
    {
      try
      {
        for (;;)
        {
          // get next directory info request
          final DirectoryInfoRequest directoryInfoRequest;
          synchronized(directoryInfoRequestList)
          {
            // get next request
            while (directoryInfoRequestList.size() == 0)
            {
              try
              {
                directoryInfoRequestList.wait();
              }
              catch (InterruptedException exception)
              {
                // ignored
              }
            }
            directoryInfoRequest = directoryInfoRequestList.remove();
          }

          if (directorySizesFlag || directoryInfoRequest.forceFlag)
          {
            // check if disposed tree item
            final Object[] disposedData = new Object[]{null};
            display.syncExec(new Runnable()
            {
              public void run()
              {
                TreeItem treeItem = directoryInfoRequest.treeItem;
                disposedData[0] = (Boolean)treeItem.isDisposed();
              }
            });
            if ((Boolean)disposedData[0])
            {
              // disposed -> skip
              continue;
            }

            // get file count, size
            String[] resultErrorMessage = new String[1];
            ValueMap resultMap          = new ValueMap();
            int error = BARServer.executeCommand(StringParser.format("DIRECTORY_INFO name=%S timeout=%d",
                                                                     directoryInfoRequest.name,
                                                                     directoryInfoRequest.timeout
                                                                    ),
                                                 new TypeMap("count", long.class,
                                                             "size",  long.class,
                                                             "timeoutFlag",boolean.class
                                                            ),
                                                 resultErrorMessage,
                                                 resultMap
                                                );
            if (error != Errors.NONE)
            {
              // command execution fail or parsing error; ignore request
              continue;
            }
            final long    count       = resultMap.getLong   ("count"      );
            final long    size        = resultMap.getLong   ("size"       );
            final boolean timeoutFlag = resultMap.getBoolean("timeoutFlag");

            // update view
//Dprintf.dprintf("name=%s count=%d size=%d timeout=%s\n",directoryInfoRequest.name,count,size,timeoutFlag);
            display.syncExec(new Runnable()
            {
              public void run()
              {
                TreeItem treeItem = directoryInfoRequest.treeItem;
                if (!treeItem.isDisposed())
                {
                  FileTreeData fileTreeData = (FileTreeData)treeItem.getData();

                  fileTreeData.size = size;

//Dprintf.dprintf("update %s\n",treeItem.isDisposed());
                  treeItem.setText(2,Units.formatByteSize(size));
                  treeItem.setForeground(2,timeoutFlag?COLOR_RED:COLOR_BLACK);
                }
              }
            });

            if (timeoutFlag)
            {
              // timeout -> increase timmeout and re-insert in list if not beyond max. timeout
              if (directoryInfoRequest.timeout+TIMEOUT_DETLA <= MAX_TIMEOUT)
              {
                directoryInfoRequest.timeout += TIMEOUT_DETLA;
              }
              add(directoryInfoRequest);
            }
          }
        }
      }
      catch (Exception exception)
      {
        if (Settings.debugFlag)
        {
          BARServer.disconnect();
          System.err.println("ERROR: "+exception.getMessage());
          BARControl.printStackTrace(exception);
          System.exit(1);
        }
      }
    }

    /** add directory info request
     * @param name path name
     * @param forceFlag true to force update
     * @param treeItem tree item
     * @param timeout timeout [ms]
     */
    public void add(String name, boolean forceFlag, TreeItem treeItem, int timeout)
    {
      DirectoryInfoRequest directoryInfoRequest = new DirectoryInfoRequest(name,forceFlag,treeItem,timeout);
      add(directoryInfoRequest);
    }

    /** add directory info request
     * @param name path name
     * @param treeItem tree item
     * @param timeout timeout [ms]
     */
    public void add(String name, TreeItem treeItem, int timeout)
    {
      DirectoryInfoRequest directoryInfoRequest = new DirectoryInfoRequest(name,false,treeItem,timeout);
      add(directoryInfoRequest);
    }

    /** add directory info request with default timeout
     * @param name path name
     * @param forceFlag true to force update
     * @param treeItem tree item
     */
    public void add(String name, boolean forceFlag, TreeItem treeItem)
    {
      add(name,forceFlag,treeItem,DEFAULT_TIMEOUT);
    }

    /** add directory info request with default timeout
     * @param name path name
     * @param treeItem tree item
     */
    public void add(String name, TreeItem treeItem)
    {
      add(name,false,treeItem,DEFAULT_TIMEOUT);
    }

    /** clear all directory info requests
     * @param treeItem tree item
     */
    public void clear()
    {
      synchronized(directoryInfoRequestList)
      {
        directoryInfoRequestList.clear();
      }
    }

    // ----------------------------------------------------------------------

    /** get index of directory info request in list
     * @param directoryInfoRequest directory info request
     * @return index or 0
     */
    private int getIndex(DirectoryInfoRequest directoryInfoRequest)
    {
//Dprintf.dprintf("find index %d: %s\n",directoryInfoRequestList.size(),directoryInfoRequest);
      // find new position in list
      ListIterator<DirectoryInfoRequest> listIterator = directoryInfoRequestList.listIterator();
      boolean                            foundFlag = false;
      int                                index = 0;
      while (listIterator.hasNext() && !foundFlag)
      {
        index = listIterator.nextIndex();

        DirectoryInfoRequest nextDirectoryInfoRequest = listIterator.next();
        foundFlag = (   (directoryInfoRequest.depth > nextDirectoryInfoRequest.depth)
                     || (directoryInfoRequest.timeout < nextDirectoryInfoRequest.timeout)
                    );
      }
//Dprintf.dprintf("found index=%d\n",index);

      return index;
    }

    /** add directory info request
     * @param directoryInfoRequest directory info request
     */
    private void add(DirectoryInfoRequest directoryInfoRequest)
    {
      synchronized(directoryInfoRequestList)
      {
        int index = getIndex(directoryInfoRequest);
        directoryInfoRequestList.add(index,directoryInfoRequest);
        directoryInfoRequestList.notifyAll();
      }
    }
  }

  /** entry data
   */
  class EntryData implements Cloneable
  {
    EntryTypes entryType;
    String     pattern;

    /** create entry data
     * @param entryType entry type
     * @param pattern pattern
     */
    EntryData(EntryTypes entryType, String pattern)
    {
      this.entryType = entryType;
      this.pattern   = pattern;
    }

    /** clone entry data object
     * @return clone of object
     */
    public EntryData clone()
    {
      return new EntryData(entryType,pattern);
    }

    /** get image for entry data
     * @return image
     */
    Image getImage()
    {
      Image image = null;
      switch (entryType)
      {
        case FILE:  image = IMAGE_FILE;   break;
        case IMAGE: image = IMAGE_DEVICE; break;
      }

      return image;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Entry {"+entryType+", "+pattern+"}";
    }
  }

  /** schedule data
   */
  class ScheduleData implements Cloneable
  {
    final static int ANY = -1;
    final static int MON = 0;
    final static int TUE = 1;
    final static int WED = 2;
    final static int THU = 3;
    final static int FRI = 4;
    final static int SAT = 5;
    final static int SUN = 6;

    int     year,month,day;
    int     weekDays;
    int     hour,minute;
    String  archiveType;
    String  customText;
    boolean enabled;

    /** create schedule data
     * @param year year
     * @param month month
     * @param day day
     * @param weekDays week days
     * @param hour hour
     * @param minute minute
     * @param archiveType archive type string
     * @param customText custom text
     * @param enabled enabled state
     */
    ScheduleData(int year, int month, int day, int weekDays, int hour, int minute, String archiveType, String customText, boolean enabled)
    {
      this.year        = year;
      this.month       = month;
      this.day         = day;
      this.weekDays    = weekDays;
      this.hour        = hour;
      this.minute      = minute;
      this.archiveType = archiveType;
      this.customText  = customText;
      this.enabled     = enabled;
    }

    /** create schedule data
     */
    ScheduleData()
    {
      this(ScheduleData.ANY,ScheduleData.ANY,ScheduleData.ANY,ScheduleData.ANY,ScheduleData.ANY,ScheduleData.ANY,"*","",true);
    }

    /** create schedule data
     * @param date date string (<year>-<month>-<day>)
     * @param weekDays week days string; values separated by ','
     * @param time time string (<hour>:<minute>)
     * @param archiveType archive type string
     * @param enabled enabled state
     */
    ScheduleData(String date, String weekDays, String time, String archiveType, String customText, boolean enabled)
    {
      setDate(date);
      setWeekDays(weekDays);
      setTime(time);
      this.archiveType = getValidString(archiveType,new String[]{"*","full","incremental","differential"},"*");
      this.customText  = customText;
      this.enabled     = enabled;
    }

    /** clone schedule data object
     * @return clone of object
     */
    public ScheduleData clone()
    {
      return new ScheduleData(year,
                              month,
                              day,
                              weekDays,
                              hour,
                              minute,
                              archiveType,
                              customText,
                              enabled
                             );
    }

    /** get year value
     * @return year string
     */
    String getYear()
    {
      assert (year == ANY) || (year >= 1);

      return (year != ANY) ? Integer.toString(year) : "*";
    }

    /** get month value
     * @return month string
     */
    String getMonth()
    {
      assert (month == ANY) || ((month >= 1) && (month <= 12));

      switch (month)
      {
        case ANY: return "*";
        case 1:   return "Jan";
        case 2:   return "Feb";
        case 3:   return "Mar";
        case 4:   return "Apr";
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

    /** get day value
     * @return day string
     */
    String getDay()
    {
      assert (day == ANY) || ((day >= 1) && (day <= 31));

      return (day != ANY) ? Integer.toString(day) : "*";
    }

    /** get week days value
     * @return week days string
     */
    String getWeekDays()
    {
      assert    (weekDays == ANY)
             || ((weekDays & ~(  (1 << ScheduleData.MON)
                               | (1 << ScheduleData.TUE)
                               | (1 << ScheduleData.WED)
                               | (1 << ScheduleData.THU)
                               | (1 << ScheduleData.FRI)
                               | (1 << ScheduleData.SAT)
                               | (1 << ScheduleData.SUN)
                              )) == 0
                );

      if (weekDays == ANY)
      {
        return "*";
      }
      else
      {
        StringBuilder buffer = new StringBuilder();

        if ((weekDays & (1 << ScheduleData.MON)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Mon"); }
        if ((weekDays & (1 << ScheduleData.TUE)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Tue"); }
        if ((weekDays & (1 << ScheduleData.WED)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Wed"); }
        if ((weekDays & (1 << ScheduleData.THU)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Thu"); }
        if ((weekDays & (1 << ScheduleData.FRI)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Fri"); }
        if ((weekDays & (1 << ScheduleData.SAT)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Sat"); }
        if ((weekDays & (1 << ScheduleData.SUN)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append("Sun"); }

        return buffer.toString();
      }
    }

    /** get date value
     * @return date string
     */
    String getDate()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getYear());
      buffer.append('-');
      buffer.append(getMonth());
      buffer.append('-');
      buffer.append(getDay());

      return buffer.toString();
    }

    /** get hour value
     * @return hour string
     */
    String getHour()
    {
      assert (hour == ANY) || ((hour >= 0) && (hour <= 23));

      return (hour != ANY) ? String.format("%02d",hour) : "*";
    }

    /** get minute value
     * @return minute string
     */
    String getMinute()
    {
      assert (minute == ANY) || ((minute >= 0) && (minute <= 59));

      return (minute != ANY) ? String.format("%02d",minute) : "*";
    }

    /** get time value
     * @return time string
     */
    String getTime()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getHour());
      buffer.append(':');
      buffer.append(getMinute());

      return buffer.toString();
    }

    /** get archive type
     * @return archive type
     */
    String getArchiveType()
    {
      return archiveType;
    }

    /** set date
     * @param year year value
     * @param month month value
     * @param day day value
     */
    private void setDate(String year, String month, String day)
    {
      this.year = !year.equals("*") ? Integer.parseInt(year) : ANY;
      if      (month.equals("*")) this.month = ANY;
      else if (month.toLowerCase().equals("jan")) this.month =  1;
      else if (month.toLowerCase().equals("feb")) this.month =  2;
      else if (month.toLowerCase().equals("mar")) this.month =  3;
      else if (month.toLowerCase().equals("apr")) this.month =  4;
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
      this.day = !day.equals("*") ? Integer.parseInt(day) : ANY;
    }

    private void setDate(String date)
    {
      String[] parts = date.split("-");
      setDate(parts[0],parts[1],parts[2]);
    }

    /** set week days
     * @param weekDays week days string; values separated by ','
     */
    void setWeekDays(String weekDays)
    {
      if (weekDays.equals("*"))
      {
        this.weekDays = ScheduleData.ANY;
      }
      else
      {
        for (String name : weekDays.split(","))
        {
          if      (name.toLowerCase().equals("mon")) this.weekDays |= (1 << ScheduleData.MON);
          else if (name.toLowerCase().equals("tue")) this.weekDays |= (1 << ScheduleData.TUE);
          else if (name.toLowerCase().equals("wed")) this.weekDays |= (1 << ScheduleData.WED);
          else if (name.toLowerCase().equals("thu")) this.weekDays |= (1 << ScheduleData.THU);
          else if (name.toLowerCase().equals("fri")) this.weekDays |= (1 << ScheduleData.FRI);
          else if (name.toLowerCase().equals("sat")) this.weekDays |= (1 << ScheduleData.SAT);
          else if (name.toLowerCase().equals("sun")) this.weekDays |= (1 << ScheduleData.SUN);
        }
      }
    }

    /** set week days
     * @param monFlag true for Monday
     * @param tueFlag true for Tuesday
     * @param wedFlag true for Wednesday
     * @param thuFlag true for Thursday
     * @param friFlag true for Friday
     * @param satFlag true for Saturday
     * @param SunFlag true for Sunday
     */
    void setWeekDays(boolean monFlag,
                     boolean tueFlag,
                     boolean wedFlag,
                     boolean thuFlag,
                     boolean friFlag,
                     boolean satFlag,
                     boolean SunFlag
                    )
    {

      if (   monFlag
          && tueFlag
          && wedFlag
          && thuFlag
          && friFlag
          && satFlag
          && SunFlag
         )
      {
        this.weekDays = ScheduleData.ANY;
      }
      else
      {
        this.weekDays = 0;
        if (monFlag) this.weekDays |= (1 << ScheduleData.MON);
        if (tueFlag) this.weekDays |= (1 << ScheduleData.TUE);
        if (wedFlag) this.weekDays |= (1 << ScheduleData.WED);
        if (thuFlag) this.weekDays |= (1 << ScheduleData.THU);
        if (friFlag) this.weekDays |= (1 << ScheduleData.FRI);
        if (satFlag) this.weekDays |= (1 << ScheduleData.SAT);
        if (SunFlag) this.weekDays |= (1 << ScheduleData.SUN);
      }
    }

    /** set time
     * @param hour hour value
     * @param minute minute value
     */
    void setTime(String hour, String minute)
    {
      this.hour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.minute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
    }

    void setTime(String time)
    {
      String[] parts = time.split(":");
      setTime(parts[0],parts[1]);
    }

    /** check if week day enabled
     * @param weekDay week data
     * @return TRUE iff enabled
     */
    boolean weekDayIsEnabled(int weekDay)
    {
      assert(   (weekDay == ScheduleData.MON)
             || (weekDay == ScheduleData.TUE)
             || (weekDay == ScheduleData.WED)
             || (weekDay == ScheduleData.THU)
             || (weekDay == ScheduleData.FRI)
             || (weekDay == ScheduleData.SAT)
             || (weekDay == ScheduleData.SUN)
            );

      return (weekDays == ScheduleData.ANY) || ((weekDays & (1 << weekDay)) != 0);
    }

    /** check if enabled
     * @return TRUE iff enabled
     */
    boolean isEnabled()
    {
      return enabled;
    }

    /** convert data to string
     */
    public String toString()
    {
      return "Schedule {"+getDate()+", "+getWeekDays()+", "+getTime()+", "+archiveType+", "+enabled+"}";
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
    private final static int SORTMODE_DATE         = 0;
    private final static int SORTMODE_WEEKDAY      = 1;
    private final static int SORTMODE_TIME         = 2;
    private final static int SORTMODE_ARCHIVE_TYPE = 3;
    private final static int SORTMODE_CUSTOM_TEXT  = 4;
    private final static int SORTMODE_ENABLED      = 5;

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
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_ARCHIVE_TYPE;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_CUSTOM_TEXT;
      else if (table.getColumn(5) == sortColumn) sortMode = SORTMODE_ENABLED;
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
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_ARCHIVE_TYPE;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_CUSTOM_TEXT;
      else if (table.getColumn(5) == sortColumn) sortMode = SORTMODE_ENABLED;
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
          if      (scheduleData1.weekDays < scheduleData2.weekDays) return -1;
          else if (scheduleData1.weekDays > scheduleData2.weekDays) return  1;
          else                      return  0;
        case SORTMODE_TIME:
          String time1 = scheduleData1.hour+":"+scheduleData1.minute;
          String time2 = scheduleData2.hour+":"+scheduleData2.minute;

          return time1.compareTo(time2);
        case SORTMODE_ARCHIVE_TYPE:
          return scheduleData1.archiveType.compareTo(scheduleData2.archiveType);
        case SORTMODE_CUSTOM_TEXT:
          return scheduleData1.customText.compareTo(scheduleData2.customText);
        case SORTMODE_ENABLED:
          if      (scheduleData1.enabled && !scheduleData2.enabled) return -1;
          else if (!scheduleData1.enabled && scheduleData2.enabled) return  1;
          else                                                      return  0;
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
  private final Color  COLOR_BLACK;
  private final Color  COLOR_WHITE;
  private final Color  COLOR_RED;
  private final Color  COLOR_MODIFIED;

  // images
  private final Image  IMAGE_DIRECTORY;
  private final Image  IMAGE_DIRECTORY_INCLUDED;
  private final Image  IMAGE_DIRECTORY_EXCLUDED;
  private final Image  IMAGE_FILE;
  private final Image  IMAGE_FILE_INCLUDED;
  private final Image  IMAGE_FILE_EXCLUDED;
  private final Image  IMAGE_LINK;
  private final Image  IMAGE_LINK_INCLUDED;
  private final Image  IMAGE_LINK_EXCLUDED;
  private final Image  IMAGE_DEVICE;
  private final Image  IMAGE_DEVICE_INCLUDED;
  private final Image  IMAGE_DEVICE_EXCLUDED;
  private final Image  IMAGE_TRASHCAN;

  // date/time format
  private final SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  // cursors
  private final Cursor waitCursor;

  // global variable references
  private Shell        shell;
  private TabStatus    tabStatus;

  // widgets
  public  Composite    widgetTab;
  private TabFolder    widgetTabFolder;
  private Combo        widgetJobList;
  private Tree         widgetFileTree;
  private MenuItem     menuItemOpenClose;
  private Shell        widgetFileTreeToolTip = null;
  private Tree         widgetDeviceTree;
  private Table        widgetIncludeTable;
  private Button       widgetIncludeTableInsert,widgetIncludeTableEdit,widgetIncludeTableRemove;
  private List         widgetExcludeList;
  private Button       widgetExcludeListInsert,widgetExcludeListEdit,widgetExcludeListRemove;
  private Combo        widgetArchivePartSize;
  private List         widgetCompressExcludeList;
  private Button       widgetCompressExcludeListInsert,widgetCompressExcludeListEdit,widgetCompressExcludeListRemove;
  private Text         widgetCryptPassword1,widgetCryptPassword2;
  private Combo        widgetFTPMaxBandWidth;
  private Combo        widgetSCPSFTPMaxBandWidth;
  private Combo        widgetWebdavMaxBandWidth;
  private Table        widgetScheduleList;
  private Button       widgetScheduleListAdd,widgetScheduleListEdit,widgetScheduleListRemove;

  // BAR variables
  private WidgetVariable  archiveType             = new WidgetVariable(new String[]{"normal","full","incremental","differential"});
  private WidgetVariable  archivePartSizeFlag     = new WidgetVariable(false);
  private WidgetVariable  archivePartSize         = new WidgetVariable(0);
  private WidgetVariable  deltaCompressAlgorithm  = new WidgetVariable(new String[]{"none","xdelta1","xdelta2","xdelta3","xdelta4","xdelta5","xdelta6","xdelta7","xdelta8","xdelta9"});
  private WidgetVariable  deltaSource             = new WidgetVariable("");
  private WidgetVariable  byteCompressAlgorithm   = new WidgetVariable(new String[]{"none","zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9","bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9","lzma1","lzma2","lzma3","lzma4","lzma5","lzma6","lzma7","lzma8","lzma9"});
  private WidgetVariable  compressMinSize         = new WidgetVariable(0);
  private WidgetVariable  cryptAlgorithm          = new WidgetVariable(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256"});
  private WidgetVariable  cryptType               = new WidgetVariable(new String[]{"none","symmetric","asymmetric"});
  private WidgetVariable  cryptPublicKeyFileName  = new WidgetVariable("");
  private WidgetVariable  cryptPasswordMode       = new WidgetVariable(new String[]{"default","ask","config"});
  private WidgetVariable  cryptPassword           = new WidgetVariable("");
  private WidgetVariable  incrementalListFileName = new WidgetVariable("");
  private WidgetVariable  storageType             = new WidgetVariable(new String[]{"filesystem","ftp","scp","sftp","webdav","cd","dvd","bd","device"});
  private WidgetVariable  storageHostName         = new WidgetVariable("");
  private WidgetVariable  storageHostPort         = new WidgetVariable(0);
  private WidgetVariable  storageLoginName        = new WidgetVariable("");
  private WidgetVariable  storageLoginPassword    = new WidgetVariable("");
  private WidgetVariable  storageDeviceName       = new WidgetVariable("");
  private WidgetVariable  storageFileName         = new WidgetVariable("");
  private WidgetVariable  overwriteArchiveFiles   = new WidgetVariable(false);
  private WidgetVariable  sshPublicKeyFileName    = new WidgetVariable("");
  private WidgetVariable  sshPrivateKeyFileName   = new WidgetVariable("");
  private WidgetVariable  maxBandWidthFlag        = new WidgetVariable(false);
  private WidgetVariable  maxBandWidth            = new WidgetVariable(0);
  private WidgetVariable  volumeSize              = new WidgetVariable(0);
  private WidgetVariable  ecc                     = new WidgetVariable(false);
  private WidgetVariable  waitFirstVolume         = new WidgetVariable(false);
  private WidgetVariable  skipUnreadable          = new WidgetVariable(false);
  private WidgetVariable  rawImages               = new WidgetVariable(false);
  private WidgetVariable  overwriteFiles          = new WidgetVariable(false);

  // variables
  private DirectoryInfoThread       directoryInfoThread;
  private boolean                   directorySizesFlag     = false;
  private HashMap<String,Integer>   jobIds                 = new HashMap<String,Integer>();
  private int                       selectedJobId          = 0;
  private String                    selectedJobName        = null;
  private WidgetEvent               selectJobEvent         = new WidgetEvent();
  private HashMap<String,EntryData> includeHashMap         = new HashMap<String,EntryData>();
  private HashSet<String>           excludeHashSet         = new HashSet<String>();
  private HashSet<String>           compressExcludeHashSet = new HashSet<String>();
  private LinkedList<ScheduleData>  scheduleList           = new LinkedList<ScheduleData>();


  /** create jobs tab
   * @param parentTabFolder parent tab folder
   * @param accelerator keyboard shortcut to select tab
   */
  TabJobs(TabFolder parentTabFolder, int accelerator)
  {
    final Display display;
    Composite     tab;
    Menu          menu;
    MenuItem      menuItem;
    Group         group;
    Composite     composite,subComposite,subSubComposite;
    Label         label;
    Button        button;
    Combo         combo;
    TreeColumn    treeColumn;
    TreeItem      treeItem;
    Control       control;
    Text          text;
    TableColumn   tableColumn;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    // get colors
    COLOR_BLACK    = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
    COLOR_WHITE    = shell.getDisplay().getSystemColor(SWT.COLOR_WHITE);
    COLOR_RED      = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
    COLOR_MODIFIED = new Color(null,0xFF,0xA0,0xA0);

    // get images
    IMAGE_DIRECTORY          = Widgets.loadImage(display,"directory.png");
    IMAGE_DIRECTORY_INCLUDED = Widgets.loadImage(display,"directoryIncluded.png");
    IMAGE_DIRECTORY_EXCLUDED = Widgets.loadImage(display,"directoryExcluded.png");
    IMAGE_FILE               = Widgets.loadImage(display,"file.png");
    IMAGE_FILE_INCLUDED      = Widgets.loadImage(display,"fileIncluded.png");
    IMAGE_FILE_EXCLUDED      = Widgets.loadImage(display,"fileExcluded.png");
    IMAGE_LINK               = Widgets.loadImage(display,"link.png");
    IMAGE_LINK_INCLUDED      = Widgets.loadImage(display,"linkIncluded.png");
    IMAGE_LINK_EXCLUDED      = Widgets.loadImage(display,"linkExcluded.png");
    IMAGE_DEVICE             = Widgets.loadImage(display,"device.png");
    IMAGE_DEVICE_INCLUDED    = Widgets.loadImage(display,"deviceIncluded.png");
    IMAGE_DEVICE_EXCLUDED    = Widgets.loadImage(display,"deviceExcluded.png");
    IMAGE_TRASHCAN           = Widgets.loadImage(display,"trashcan.png");

    // get cursors
    waitCursor = new Cursor(display,SWT.CURSOR_WAIT);

    // start tree item size thread
    directoryInfoThread = new DirectoryInfoThread(display);
    directoryInfoThread.start();

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Jobs"+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // job selector
    composite = Widgets.newComposite(widgetTab);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0,0.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,"Name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobList = Widgets.newOptionMenu(composite);
      Widgets.layout(widgetJobList,0,1,TableLayoutData.WE);
      widgetJobList.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo widget = (Combo)selectionEvent.widget;
          int   index  = widget.getSelectionIndex();
          if (index >= 0)
          {
            selectedJobName = widgetJobList.getItem(index);
            selectedJobId = jobIds.get(selectedJobName);
            selectJobEvent.trigger();
            update();
          }
        }
      });
      widgetJobList.setToolTipText("Existing job entries.");

      button = Widgets.newButton(composite,"New\u2026");
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobNew();
        }
      });
      button.setToolTipText("Create new job entry.");

      button = Widgets.newButton(composite,"Copy\u2026");
      button.setEnabled(false);
      Widgets.layout(button,0,3,TableLayoutData.DEFAULT);
      Widgets.addEventListener(new WidgetEventListener(button,selectJobEvent)
      {
        public void trigger(Widget widget)
        {
          Widgets.setEnabled(widget,selectedJobId != 0);
        }
      });
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          if (selectedJobId > 0)
          {
            jobCopy();
          }
        }
      });
      button.setToolTipText("Copy an existing job entry and create a new one.");

      button = Widgets.newButton(composite,"Rename\u2026");
      button.setEnabled(false);
      Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
      Widgets.addEventListener(new WidgetEventListener(button,selectJobEvent)
      {
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,selectedJobId != 0);
        }
      });
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          if (selectedJobId > 0)
          {
            jobRename();
          }
        }
      });
      button.setToolTipText("Rename a job entry.");

      button = Widgets.newButton(composite,"Delete\u2026");
      button.setEnabled(false);
      Widgets.layout(button,0,5,TableLayoutData.DEFAULT);
      Widgets.addEventListener(new WidgetEventListener(button,selectJobEvent)
      {
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,selectedJobId != 0);
        }
      });
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          if (selectedJobId > 0)
          {
            jobDelete();
          }
        }
      });
      button.setToolTipText("Delete a job entry.");
    }

    // create sub-tabs
    widgetTabFolder = Widgets.newTabFolder(widgetTab);
    Widgets.setEnabled(widgetTabFolder,false);
    Widgets.layout(widgetTabFolder,1,0,TableLayoutData.NSWE);
    {
      tab = Widgets.addTab(widgetTabFolder,"Files");
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // file tree
        widgetFileTree = Widgets.newTree(tab,SWT.NONE);
        Widgets.layout(widgetFileTree,0,0,TableLayoutData.NSWE);
        SelectionListener fileTreeColumnSelectionListener = new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TreeColumn             treeColumn             = (TreeColumn)selectionEvent.widget;
            FileTreeDataComparator fileTreeDataComparator = new FileTreeDataComparator(widgetFileTree,treeColumn);
            synchronized(widgetFileTree)
            {
              Widgets.sortTreeColumn(widgetFileTree,treeColumn,fileTreeDataComparator);
            }
          }
        };
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Name",    SWT.LEFT, 390,true);
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn.setToolTipText("Click to sort for name.");
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Type",    SWT.LEFT, 160,true);
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn.setToolTipText("Click to sort for type.");
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Size",    SWT.RIGHT,100,true);
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn.setToolTipText("Click to sort for size.");
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Modified",SWT.LEFT, 100,true);
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn.setToolTipText("Click to sort for modified time.");

        widgetFileTree.addListener(SWT.Expand,new Listener()
        {
          public void handleEvent(final Event event)
          {
            final TreeItem treeItem = (TreeItem)event.item;
            addFileTree(treeItem);
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
        widgetFileTree.addMouseListener(new MouseListener()
        {
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
            TreeItem treeItem = widgetFileTree.getItem(new Point(mouseEvent.x,mouseEvent.y));
            if (treeItem != null)
            {
              FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
              if (fileTreeData.fileType == FileTypes.DIRECTORY)
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
          }

          public void mouseDown(final MouseEvent mouseEvent)
          {
            TreeItem treeItem = widgetFileTree.getItem(new Point(mouseEvent.x,mouseEvent.y));
            if (treeItem != null)
            {
              FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
              menuItemOpenClose.setEnabled(fileTreeData.fileType == FileTypes.DIRECTORY);
            }
          }

          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });
        widgetFileTree.addMouseTrackListener(new MouseTrackListener()
        {
          public void mouseEnter(MouseEvent mouseEvent)
          {
          }

          public void mouseExit(MouseEvent mouseEvent)
          {
          }

          public void mouseHover(MouseEvent mouseEvent)
          {
            Tree tree = (Tree)mouseEvent.widget;

            if (widgetFileTreeToolTip != null)
            {
              widgetFileTreeToolTip.dispose();
              widgetFileTreeToolTip = null;
            }

            // show if table item available and mouse is in the left side
            if (mouseEvent.x < 64)
            {
              Label       label;

              final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
              final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

              widgetFileTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
              widgetFileTreeToolTip.setBackground(COLOR_BACKGROUND);
              widgetFileTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
              Widgets.layout(widgetFileTreeToolTip,0,0,TableLayoutData.NSWE);
              widgetFileTreeToolTip.addMouseTrackListener(new MouseTrackListener()
              {
                public void mouseEnter(MouseEvent mouseEvent)
                {
                }

                public void mouseExit(MouseEvent mouseEvent)
                {
                  widgetFileTreeToolTip.dispose();
                  widgetFileTreeToolTip = null;
                }

                public void mouseHover(MouseEvent mouseEvent)
                {
                }
              });

              label = Widgets.newLabel(widgetFileTreeToolTip,"Tree representation of files, directories, links and special entries.\nDouble-click to open sub-directories, right-click to open context menu.\nNote size column: numbers in red color indicates size update is still in progress.");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,0,0,TableLayoutData.W);

              Point size = widgetFileTreeToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
              Point point = tree.toDisplay(mouseEvent.x+16,mouseEvent.y);
              widgetFileTreeToolTip.setBounds(point.x,point.y,size.x,size.y);
              widgetFileTreeToolTip.setVisible(true);
            }
          }
        });

        menu = Widgets.newPopupMenu(shell);
        {
          menuItemOpenClose = Widgets.addMenuItem(menu,"Open/Close");
          menuItemOpenClose.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              TreeItem[] treeItems = widgetFileTree.getSelection();
              if (treeItems != null)
              {
                FileTreeData fileTreeData = (FileTreeData)treeItems[0].getData();
                if (fileTreeData.fileType == FileTypes.DIRECTORY)
                {
                  Event treeEvent = new Event();
                  treeEvent.item = treeItems[0];
                  if (treeItems[0].getExpanded())
                  {
                    widgetFileTree.notifyListeners(SWT.Collapse,treeEvent);
                    treeItems[0].setExpanded(false);
                  }
                  else
                  {
                    widgetFileTree.notifyListeners(SWT.Expand,treeEvent);
                    treeItems[0].setExpanded(true);
                  }
                }
              }
            }
          });

          menuItem = Widgets.addMenuSeparator(menu);

          menuItem = Widgets.addMenuItem(menu,"Include");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                includeListAdd(new EntryData(EntryTypes.FILE,fileTreeData.name));
                excludeListRemove(fileTreeData.name);
                switch (fileTreeData.fileType)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE_INCLUDED);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY_INCLUDED); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK_INCLUDED);      break;
                  case HARDLINK:  treeItem.setImage(IMAGE_LINK_INCLUDED);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE_INCLUDED);      break;
                }
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Exclude");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                includeListRemove(fileTreeData.name);
                excludeListAdd(fileTreeData.name);
                switch (fileTreeData.fileType)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE_EXCLUDED);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY_EXCLUDED); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK_EXCLUDED);      break;
                  case HARDLINK:  treeItem.setImage(IMAGE_LINK_EXCLUDED);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE_EXCLUDED);      break;
                }
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"None");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                includeListRemove(fileTreeData.name);
                excludeListRemove(fileTreeData.name);
                switch (fileTreeData.fileType)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK);      break;
                  case HARDLINK:  treeItem.setImage(IMAGE_LINK);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE);      break;
                }
              }
            }
          });

          menuItem = Widgets.addMenuSeparator(menu);

          menuItem = Widgets.addMenuItem(menu,"Directory size");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                directoryInfoThread.add(fileTreeData.name,true,treeItem);
              }
            }
          });
        }
        widgetFileTree.setMenu(menu);
//        widgetFileTree.setToolTipText("Tree representation of files, directories, links and special entries.\nDouble-click to open sub-directories, right-click to open context menu.\nNote size column: numbers in red color indicates size update is still in progress.");

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,1.0,1.0,0.0,0.0,0.0}));
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          button = Widgets.newButton(composite,"Include");
          Widgets.layout(button,0,0,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                includeListAdd(new EntryData(EntryTypes.FILE,fileTreeData.name));
                excludeListRemove(fileTreeData.name);
                switch (fileTreeData.fileType)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE_INCLUDED);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY_INCLUDED); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK_INCLUDED);      break;
                  case HARDLINK:  treeItem.setImage(IMAGE_LINK_INCLUDED);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE_INCLUDED);      break;
                }
              }
            }
          });
          button.setToolTipText("Include entry in archive.");

          button = Widgets.newButton(composite,"Exclude");
          Widgets.layout(button,0,1,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                includeListRemove(fileTreeData.name);
                excludeListAdd(fileTreeData.name);
                switch (fileTreeData.fileType)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE_EXCLUDED);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY_EXCLUDED); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK_EXCLUDED);      break;
                  case HARDLINK:  treeItem.setImage(IMAGE_LINK_EXCLUDED);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE_EXCLUDED);      break;
                }
              }
            }
          });
          button.setToolTipText("Exclude entry from archive.");

          button = Widgets.newButton(composite,"None");
          Widgets.layout(button,0,2,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                includeListRemove(fileTreeData.name);
                excludeListRemove(fileTreeData.name);
                switch (fileTreeData.fileType)
                {
                  case FILE:      treeItem.setImage(IMAGE_FILE);      break;
                  case DIRECTORY: treeItem.setImage(IMAGE_DIRECTORY); break;
                  case LINK:      treeItem.setImage(IMAGE_LINK);      break;
                  case HARDLINK:  treeItem.setImage(IMAGE_LINK);      break;
                  case SPECIAL:   treeItem.setImage(IMAGE_FILE);      break;
                }
              }
            }
          });
          button.setToolTipText("Do not include/exclude entry in/from archive.");

          control = Widgets.newSpacer(composite);
          Widgets.layout(control,0,3,TableLayoutData.NONE,0,0,30,0);

          button = Widgets.newButton(composite,IMAGE_DIRECTORY_INCLUDED);
          Widgets.layout(button,0,4,TableLayoutData.E,0,0,2,0);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              openIncludedDirectories();
            }
          });
          button.setToolTipText("Open all included directories.");

          button = Widgets.newCheckbox(composite,"directory size");
          Widgets.layout(button,0,5,TableLayoutData.E,0,0,2,0);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button  widget = (Button)selectionEvent.widget;
              directorySizesFlag = widget.getSelection();
            }
          });
          button.setToolTipText("Show directory sizes (sum of file sizes).");
        }
      }

      tab = Widgets.addTab(widgetTabFolder,"Images");
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // image tree
        widgetDeviceTree = Widgets.newTree(tab);
        Widgets.layout(widgetDeviceTree,0,0,TableLayoutData.NSWE);
        SelectionListener deviceTreeColumnSelectionListener = new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TreeColumn               treeColumn               = (TreeColumn)selectionEvent.widget;
            DeviceTreeDataComparator deviceTreeDataComparator = new DeviceTreeDataComparator(widgetDeviceTree,treeColumn);
            synchronized(widgetDeviceTree)
            {
              Widgets.sortTreeColumn(widgetDeviceTree,treeColumn,deviceTreeDataComparator);
            }
          }
        };
        treeColumn = Widgets.addTreeColumn(widgetDeviceTree,"Name",SWT.LEFT, 500,true);
        treeColumn.addSelectionListener(deviceTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetDeviceTree,"Size",SWT.RIGHT,100,false);
        treeColumn.addSelectionListener(deviceTreeColumnSelectionListener);

        menu = Widgets.newPopupMenu(shell);
        {
          menuItem = Widgets.addMenuItem(menu,"Include");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                includeListAdd(new EntryData(EntryTypes.IMAGE,deviceTreeData.name));
                excludeListRemove(deviceTreeData.name);
                treeItem.setImage(IMAGE_DEVICE_INCLUDED);
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Exclude");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                includeListRemove(deviceTreeData.name);
                excludeListAdd(deviceTreeData.name);
                treeItem.setImage(IMAGE_DEVICE_EXCLUDED);
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"None");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                includeListRemove(deviceTreeData.name);
                excludeListRemove(deviceTreeData.name);
                treeItem.setImage(IMAGE_DEVICE);
              }
            }
          });
        }
        widgetDeviceTree.setMenu(menu);
        widgetDeviceTree.setToolTipText("List of existing devices for image storage.\nRight-click to open context menu.");

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,1.0,1.0,0.0,0.0,0.0}));
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          button = Widgets.newButton(composite,"Include");
          Widgets.layout(button,0,0,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                includeListAdd(new EntryData(EntryTypes.IMAGE,deviceTreeData.name));
                excludeListRemove(deviceTreeData.name);
                treeItem.setImage(IMAGE_DEVICE_INCLUDED);
              }
            }
          });
          button.setToolTipText("Include selected device for image storage.");

          button = Widgets.newButton(composite,"Exclude");
          Widgets.layout(button,0,1,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                includeListRemove(deviceTreeData.name);
                excludeListAdd(deviceTreeData.name);
                treeItem.setImage(IMAGE_DEVICE_EXCLUDED);
              }
            }
          });
          button.setToolTipText("Exclude selected device from image storage.");

          button = Widgets.newButton(composite,"None");
          Widgets.layout(button,0,2,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                includeListRemove(deviceTreeData.name);
                excludeListRemove(deviceTreeData.name);
                treeItem.setImage(IMAGE_DEVICE);
              }
            }
          });
          button.setToolTipText("Remove selected device from image storage.");
        }
      }

      tab = Widgets.addTab(widgetTabFolder,"Filters");
      tab.setLayout(new TableLayout(new double[]{0.5,0.0,0.5,0.0,0.0},new double[]{0.0,1.0}));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // included table
        label = Widgets.newLabel(tab,"Included:");
        Widgets.layout(label,0,0,TableLayoutData.NS);
        widgetIncludeTable = Widgets.newTable(tab);
        widgetIncludeTable.setHeaderVisible(false);
        Widgets.addTableColumn(widgetIncludeTable,0,SWT.LEFT,20);
        Widgets.addTableColumn(widgetIncludeTable,1,SWT.LEFT,1024,true);
// automatic column width calculation?
//widgetIncludeTable.setLayout(new TableLayout(new double[]{0.5,0.0,0.5,0.0,0.0},new double[]{0.0,1.0}));
        Widgets.layout(widgetIncludeTable,0,1,TableLayoutData.NSWE);
        widgetIncludeTable.addMouseListener(new MouseListener()
        {
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
            includeListEdit();
          }
          public void mouseDown(final MouseEvent mouseEvent)
          {
          }
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });
        widgetIncludeTable.addKeyListener(new KeyListener()
        {
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          public void keyReleased(KeyEvent keyEvent)
          {
            if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
            {
              Widgets.invoke(widgetIncludeTableInsert);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.CR))
            {
              Widgets.invoke(widgetIncludeTableEdit);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
            {
              Widgets.invoke(widgetIncludeTableRemove);
            }
          }
        });
        widgetIncludeTable.setToolTipText("List of included entries.");

        menu = Widgets.newPopupMenu(shell);
        {
          menuItem = Widgets.addMenuItem(menu,"Add\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                includeListAdd();
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Edit\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                includeListEdit();
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Clone\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                includeListClone();
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Remove\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              if (selectedJobId > 0)
              {
                includeListRemove();
              }
            }
          });
        }
        widgetIncludeTable.setMenu(menu);
        widgetIncludeTable.setToolTipText("List with include patterns, right-click for context menu.");

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,1,TableLayoutData.W);
        {
          widgetIncludeTableInsert = Widgets.newButton(composite,"Add\u2026");
          Widgets.layout(widgetIncludeTableInsert,0,0,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetIncludeTableInsert.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                includeListAdd();
              }
            }
          });
          widgetIncludeTableInsert.setToolTipText("Add entry to included list.");

          widgetIncludeTableEdit = Widgets.newButton(composite,"Edit\u2026");
          Widgets.layout(widgetIncludeTableEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetIncludeTableEdit.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                includeListEdit();
              }
            }
          });
          widgetIncludeTableEdit.setToolTipText("Edit entry in included list.");

          widgetIncludeTableRemove = Widgets.newButton(composite,"Clone\u2026");
          Widgets.layout(widgetIncludeTableRemove,0,2,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetIncludeTableRemove.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                includeListClone();
              }
            }
          });
          widgetIncludeTableRemove.setToolTipText("Clone entry in included list.");

          button = Widgets.newButton(composite,"Remove\u2026");
          Widgets.layout(button,0,3,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              if (selectedJobId > 0)
              {
                includeListRemove();
              }
            }
          });
          button.setToolTipText("Remove entry from included list.");
        }

        // excluded list
        label = Widgets.newLabel(tab,"Excluded:");
        Widgets.layout(label,2,0,TableLayoutData.NS);
        widgetExcludeList = Widgets.newList(tab);
        Widgets.layout(widgetExcludeList,2,1,TableLayoutData.NSWE);
        widgetExcludeList.addMouseListener(new MouseListener()
        {
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
            excludeListEdit();
          }
          public void mouseDown(final MouseEvent mouseEvent)
          {
          }
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });
        widgetExcludeList.addKeyListener(new KeyListener()
        {
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          public void keyReleased(KeyEvent keyEvent)
          {
            if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
            {
              Widgets.invoke(widgetExcludeListInsert);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.CR))
            {
              Widgets.invoke(widgetExcludeListEdit);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
            {
              Widgets.invoke(widgetExcludeListRemove);
            }
          }
        });
        widgetExcludeList.setToolTipText("List with exclude patterns, right-click for context menu.");

        menu = Widgets.newPopupMenu(shell);
        {
          menuItem = Widgets.addMenuItem(menu,"Add\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                excludeListAdd();
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Edit\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                excludeListEdit();
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Clone\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                excludeListClone();
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Remove\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                excludeListRemove();
              }
            }
          });
        }
        widgetExcludeList.setMenu(menu);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,3,1,TableLayoutData.W);
        {
          widgetExcludeListInsert = Widgets.newButton(composite,"Add\u2026");
          Widgets.layout(widgetExcludeListInsert,0,0,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetExcludeListInsert.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                excludeListAdd();
              }
            }
          });
          widgetExcludeListInsert.setToolTipText("Add entry to excluded list.");

          widgetExcludeListEdit = Widgets.newButton(composite,"Edit\u2026");
          Widgets.layout(widgetExcludeListEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetExcludeListEdit.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                excludeListEdit();
              }
            }
          });
          widgetExcludeListEdit.setToolTipText("Edit entry in excluded list.");

          button = Widgets.newButton(composite,"Clone\u2026");
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                excludeListClone();
              }
            }
          });
          button.setToolTipText("Clone entry in excluded list.");

          widgetExcludeListRemove = Widgets.newButton(composite,"Remove\u2026");
          Widgets.layout(widgetExcludeListRemove,0,3,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetExcludeListRemove.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                excludeListRemove();
              }
            }
          });
          widgetExcludeListRemove.setToolTipText("Remove entry from excluded list.");
        }

        // options
        label = Widgets.newLabel(tab,"Options:");
        Widgets.layout(label,4,0,TableLayoutData.N);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,4,1,TableLayoutData.WE);
        {
          button = Widgets.newCheckbox(composite,"skip unreadable entries");
          Widgets.layout(button,0,0,TableLayoutData.NW);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button  widget      = (Button)selectionEvent.widget;
              boolean checkedFlag = widget.getSelection();
              skipUnreadable.set(checkedFlag);
              BARServer.setOption(selectedJobId,"skip-unreadable",checkedFlag);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,skipUnreadable));
          button.setToolTipText("If enabled skip not readable entries (write information to log file).\nIf disabled stop job with an error.");

          button = Widgets.newCheckbox(composite,"raw images");
          Widgets.layout(button,1,0,TableLayoutData.NW);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button  widget      = (Button)selectionEvent.widget;
              boolean checkedFlag = widget.getSelection();
              rawImages.set(checkedFlag);
              BARServer.setOption(selectedJobId,"raw-images",checkedFlag);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,rawImages));
          button.setToolTipText("If enabled store all data of a device into an image.\nIf disabled try to detect file system and only store used blocks to image.");
        }
      }

      tab = Widgets.addTab(widgetTabFolder,"Storage");
      tab.setLayout(new TableLayout(new double[]{0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0},new double[]{0.0,1.0}));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // part size
        label = Widgets.newLabel(tab,"Part size:");
        Widgets.layout(label,0,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,0,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,"unlimited");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              boolean changedFlag = archivePartSizeFlag.set(false);
              archivePartSize.set(0);
              BARServer.setOption(selectedJobId,"archive-part-size",0);

              if (   changedFlag
                  && (   storageType.equals("cd")
                      || storageType.equals("dvd")
                      || storageType.equals("bd")
                     )
                 )
              {
                Dialogs.warning(shell,"When writing to a CD/DVD/BD without splitting enabled\nthe resulting archive file may not fit on medium.");
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
          {
            public void modified(Control control, WidgetVariable archivePartSizeFlag)
            {
              ((Button)control).setSelection(!archivePartSizeFlag.getBoolean());
            }
          });
          button.setToolTipText("Create storage files with an unlimited size. Do not split storage files.");

          button = Widgets.newRadio(composite,"limit to");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archivePartSizeFlag.set(true);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
          {
            public void modified(Control control, WidgetVariable archivePartSizeFlag)
            {
              ((Button)control).setSelection(archivePartSizeFlag.getBoolean());
            }
          });
          button.setToolTipText("Limit size of storage files to specified value.");

          widgetArchivePartSize = Widgets.newCombo(composite);
          widgetArchivePartSize.setItems(new String[]{"32M","64M","128M","140M","256M","280M","512M","600M","1G","2G","4G","8G","10G","20G"});
          widgetArchivePartSize.setData("showedErrorDialog",false);
          Widgets.layout(widgetArchivePartSize,0,2,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(widgetArchivePartSize,archivePartSizeFlag)
          {
            public void modified(Control control, WidgetVariable archivePartSizeFlag)
            {
              Widgets.setEnabled(control,archivePartSizeFlag.getBoolean());
            }
          });
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
              widget.setData("showedErrorDialog",false);
            }
          });
          widgetArchivePartSize.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();
              try
              {
                long n = Units.parseByteSize(string);
                archivePartSize.set(n);
                BARServer.setOption(selectedJobId,"archive-part-size",n);
                widget.setText(Units.formatByteSize(n));
                widget.setBackground(COLOR_WHITE);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                  widget.forceFocus();
                }
              }
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();
              try
              {
                long  n = Units.parseByteSize(string);
                archivePartSize.set(n);
                BARServer.setOption(selectedJobId,"archive-part-size",n);
                widget.setText(Units.formatByteSize(n));
                widget.setBackground(COLOR_WHITE);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                  widget.forceFocus();
                }
              }
            }
          });
          widgetArchivePartSize.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
              Combo widget = (Combo)focusEvent.widget;
              widget.setData("showedErrorDialog",false);
            }
            public void focusLost(FocusEvent focusEvent)
            {
              Combo  widget = (Combo)focusEvent.widget;
              String string = widget.getText();
              try
              {
                long n = Units.parseByteSize(string);
                archivePartSize.set(n);
                BARServer.setOption(selectedJobId,"archive-part-size",n);
                widget.setText(Units.formatByteSize(n));
                widget.setBackground(COLOR_WHITE);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                  widget.forceFocus();
                }
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetArchivePartSize,archivePartSize)
          {
            public String getString(WidgetVariable variable)
            {
              return Units.formatByteSize(variable.getLong());
            }
          });
          widgetArchivePartSize.setToolTipText("Size limit for one storage file part.");

          label = Widgets.newLabel(composite,"bytes");
          Widgets.layout(label,0,3,TableLayoutData.W);
        }

        // compress
        label = Widgets.newLabel(tab,"Compress:");
        Widgets.layout(label,1,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,0.0));
        Widgets.layout(composite,1,1,TableLayoutData.W);
        {
          label = Widgets.newLabel(composite,"Delta:");
          Widgets.layout(label,0,0,TableLayoutData.NONE);

          combo = Widgets.newOptionMenu(composite);
          combo.setItems(new String[]{"none","xdelta1","xdelta2","xdelta3","xdelta4","xdelta5","xdelta6","xdelta7","xdelta8","xdelta9"});
          Widgets.layout(combo,0,1,TableLayoutData.W);
          combo.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();
              deltaCompressAlgorithm.set(string);
              BARServer.setOption(selectedJobId,"compress-algorithm",deltaCompressAlgorithm.toString()+"+"+byteCompressAlgorithm.toString());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(combo,deltaCompressAlgorithm));
          combo.setToolTipText("Delta compression method to use.");

          label = Widgets.newLabel(composite,"Byte:");
          Widgets.layout(label,0,2,TableLayoutData.NONE);

          combo = Widgets.newOptionMenu(composite);
          combo.setItems(new String[]{"none","zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9","bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9","lzma1","lzma2","lzma3","lzma4","lzma5","lzma6","lzma7","lzma8","lzma9"});
          Widgets.layout(combo,0,3,TableLayoutData.W);
          combo.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();
              byteCompressAlgorithm.set(string);
              BARServer.setOption(selectedJobId,"compress-algorithm",deltaCompressAlgorithm.toString()+"+"+byteCompressAlgorithm.toString());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(combo,byteCompressAlgorithm));
          combo.setToolTipText("Byte compression method to use.");
        }

        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0,0.0}));
        Widgets.layout(composite,2,1,TableLayoutData.WE);
        {
          label = Widgets.newLabel(composite,"Source:");
          Widgets.layout(label,0,0,TableLayoutData.NONE);

          text = Widgets.newText(composite);
          Widgets.layout(text,0,1,TableLayoutData.WE);
          Widgets.addModifyListener(new WidgetModifyListener(text,deltaCompressAlgorithm)
          {
            public void modified(Control control, WidgetVariable byteCompressAlgorithm)
            {
              Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none"));
            }
          });
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              String s = widget.getText();
              if (deltaSource.getString().equals(s)) color = COLOR_WHITE;
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();
              deltaSource.set(string);
              BARServer.setOption(selectedJobId,"delta-source",string);
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
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
              String string = widget.getText();
              deltaSource.set(string);
              BARServer.setOption(selectedJobId,"delta-source",string);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,deltaSource));
          text.setToolTipText("Name of source to use for delta-compression.");

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
          Widgets.addModifyListener(new WidgetModifyListener(button,deltaCompressAlgorithm)
          {
            public void modified(Control control, WidgetVariable byteCompressAlgorithm)
            {
              Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none"));
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget   = (Button)selectionEvent.widget;
              String fileName = Dialogs.fileSave(shell,
                                                 "Select source file",
                                                 deltaSource.getString(),
                                                 new String[]{"BAR files","*.bar",
                                                              "All files","*",
                                                             }
                                                );
              if (fileName != null)
              {
                deltaSource.set(fileName);
              }
            }
          });
        }

        label = Widgets.newLabel(tab,"Compress exclude:");
        Widgets.layout(label,3,0,TableLayoutData.NW);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(composite,3,1,TableLayoutData.NSWE);
        {
          // compress exclude list
          widgetCompressExcludeList = Widgets.newList(composite);
          Widgets.layout(widgetCompressExcludeList,0,0,TableLayoutData.NSWE);
          widgetCompressExcludeList.addMouseListener(new MouseListener()
          {
            public void mouseDoubleClick(final MouseEvent mouseEvent)
            {
              compressExcludeListEdit();
            }
            public void mouseDown(final MouseEvent mouseEvent)
            {
            }
            public void mouseUp(final MouseEvent mouseEvent)
            {
            }
          });
          widgetCompressExcludeList.addKeyListener(new KeyListener()
          {
            public void keyPressed(KeyEvent keyEvent)
            {
            }
            public void keyReleased(KeyEvent keyEvent)
            {
              if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
              {
                Widgets.invoke(widgetCompressExcludeListInsert);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.CR))
              {
                Widgets.invoke(widgetCompressExcludeListEdit);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
              {
                Widgets.invoke(widgetCompressExcludeListRemove);
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetCompressExcludeList,new WidgetVariable[]{deltaCompressAlgorithm,byteCompressAlgorithm})
          {
            public void modified(Control control, WidgetVariable[] compressAlgorithms)
            {
              Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none") || !byteCompressAlgorithm.equals("none"));
            }
          });
          menu = Widgets.newPopupMenu(shell);
          {
            menuItem = Widgets.addMenuItem(menu,"Add\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                MenuItem widget = (MenuItem)selectionEvent.widget;

                compressExcludeListAdd();
              }
            });

            menuItem = Widgets.addMenuItem(menu,"Add most used compressed file suffixes");
            menuItem.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                final String[] COMPRESSED_PATTERNS = new String[]
                {
                  "*.gz",
                  "*.tgz",
                  "*.bz",
                  "*.bz2",
                  "*.gzip",
                  "*.lzma",
                  "*.zip",
                  "*.rar",
                  "*.7z",
                };

                MenuItem widget = (MenuItem)selectionEvent.widget;

                compressExcludeListAdd(COMPRESSED_PATTERNS);
              }
            });

            menuItem = Widgets.addMenuItem(menu,"Add most used multi-media file suffixes");
            menuItem.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                final String[] MULTIMEDIA_PATTERNS = new String[]
                {
                  "*.jpg",
                  "*.jpeg",
                  "*.mkv",
                  "*.mp3",
                  "*.mp4",
                  "*.mpeg",
                  "*.avi",
                  "*.wma",
                  "*.wmv",
                  "*.flv",
                  "*.3gp",
                };

                MenuItem widget = (MenuItem)selectionEvent.widget;

                compressExcludeListAdd(MULTIMEDIA_PATTERNS);
              }
            });

            menuItem = Widgets.addMenuItem(menu,"Add most used package file suffixes");
            menuItem.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                final String[] PACKAGE_PATTERNS = new String[]
                {
                  "*.rpm",
                  "*.deb",
                  "*.pkg",
                };

                MenuItem widget = (MenuItem)selectionEvent.widget;

                compressExcludeListAdd(PACKAGE_PATTERNS);
              }
            });

            menuItem = Widgets.addMenuItem(menu,"Remove\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobId > 0)
                {
                  compressExcludeListRemove();
                }
              }
            });
          }
          widgetCompressExcludeList.setMenu(menu);
          widgetCompressExcludeList.setToolTipText("List with compress exclude patterns. Entries which match to one of these patterns will not be compressed.\nRight-click for context menu.");

          // buttons
          subComposite = Widgets.newComposite(composite,SWT.NONE,4);
          Widgets.layout(subComposite,1,0,TableLayoutData.W);
          {
            widgetCompressExcludeListInsert = Widgets.newButton(subComposite,"Add\u2026");
            Widgets.layout(widgetCompressExcludeListInsert,0,0,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
            Widgets.addModifyListener(new WidgetModifyListener(widgetCompressExcludeListInsert,new WidgetVariable[]{deltaCompressAlgorithm,byteCompressAlgorithm})
            {
              public void modified(Control control, WidgetVariable byteCompressAlgorithm)
              {
                Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none") || !byteCompressAlgorithm.equals("none"));
              }
            });
            widgetCompressExcludeListInsert.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobId > 0)
                {
                  compressExcludeListAdd();
                }
              }
            });
            widgetCompressExcludeListInsert.setToolTipText("Add entry to compress exclude list.");

            widgetCompressExcludeListEdit = Widgets.newButton(subComposite,"Edit\u2026");
            Widgets.layout(widgetCompressExcludeListEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
            Widgets.addModifyListener(new WidgetModifyListener(widgetCompressExcludeListEdit,new WidgetVariable[]{deltaCompressAlgorithm,byteCompressAlgorithm})
            {
              public void modified(Control control, WidgetVariable byteCompressAlgorithm)
              {
                Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none") || !byteCompressAlgorithm.equals("none"));
              }
            });
            widgetCompressExcludeListEdit.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobId > 0)
                {
Dprintf.dprintf("");
                  compressExcludeListEdit();
                }
              }
            });
            widgetCompressExcludeListEdit.setToolTipText("Edit entry in compress exclude list.");

            widgetCompressExcludeListRemove = Widgets.newButton(subComposite,"Remove\u2026");
            Widgets.layout(widgetCompressExcludeListRemove,0,2,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
            Widgets.addModifyListener(new WidgetModifyListener(widgetCompressExcludeListRemove,new WidgetVariable[]{deltaCompressAlgorithm,byteCompressAlgorithm})
            {
              public void modified(Control control, WidgetVariable byteCompressAlgorithm)
              {
                Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none") || !byteCompressAlgorithm.equals("none"));
              }
            });
            widgetCompressExcludeListRemove.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobId > 0)
                {
                  compressExcludeListRemove();
                }
              }
            });
            widgetCompressExcludeListRemove.setToolTipText("Remove entry from compress exclude list.");
          }
        }

        // crypt
        label = Widgets.newLabel(tab,"Crypt:");
        Widgets.layout(label,4,0,TableLayoutData.NW);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,4,1,TableLayoutData.WE);
        {
          combo = Widgets.newOptionMenu(composite);
          combo.setItems(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256"});
          Widgets.layout(combo,0,0,TableLayoutData.W);
          combo.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();
              cryptAlgorithm.set(string);
              BARServer.setOption(selectedJobId,"crypt-algorithm",string);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(combo,cryptAlgorithm));
          combo.setToolTipText("Encryption method to use.");
        }

        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,0.0,0.0,0.0,1.0,0.0}));
        Widgets.layout(composite,5,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,"symmetric");
          button.setSelection(true);
          Widgets.layout(button,0,0,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptAlgorithm)
          {
            public void modified(Control control, WidgetVariable cryptAlgorithm)
            {
              Widgets.setEnabled(control,!cryptAlgorithm.equals("none"));
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              cryptType.set("symmetric");
              BARServer.setOption(selectedJobId,"crypt-type","symmetric");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptType)
          {
            public void modified(Control control, WidgetVariable cryptType)
            {
              ((Button)control).setSelection(cryptType.equals("none") || cryptType.equals("symmetric"));
            }
          });
          button.setToolTipText("Use symmetric encryption with pass-phrase.");

          button = Widgets.newRadio(composite,"asymmetric");
          button.setSelection(false);
          Widgets.layout(button,0,1,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptAlgorithm)
          {
            public void modified(Control control, WidgetVariable cryptAlgorithm)
            {
              Widgets.setEnabled(control,!cryptAlgorithm.equals("none"));
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              cryptType.set("asymmetric");
              BARServer.setOption(selectedJobId,"crypt-type","asymmetric");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptType)
          {
            public void modified(Control control, WidgetVariable cryptType)
            {
              ((Button)control).setSelection(cryptType.equals("asymmetric"));
            }
          });
          button.setToolTipText("Use asymmetric hyprid-encryption with pass-phrase and public/private key.");

          control = Widgets.newSpacer(composite);
          Widgets.layout(control,0,2,TableLayoutData.NONE,0,0,5,0);

          label = Widgets.newLabel(composite,"Public key:");
          Widgets.layout(label,0,3,TableLayoutData.W);
          text = Widgets.newText(composite);
          Widgets.layout(text,0,4,TableLayoutData.WE);
          Widgets.addModifyListener(new WidgetModifyListener(text,new WidgetVariable[]{cryptAlgorithm,cryptType})
          {
            public void modified(Control control, WidgetVariable cryptAlgorithm)
            {
            }
            public void modified(Control control, WidgetVariable variables[])
            {
              Widgets.setEnabled(control,
                                    !variables[0].equals("none")
                                 && variables[1].equals("asymmetric")
                                );
            }
          });
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              String s = widget.getText();
              if (cryptPublicKeyFileName.getString().equals(s)) color = COLOR_WHITE;
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();
              cryptPublicKeyFileName.set(string);
              BARServer.setOption(selectedJobId,"crypt-public-key",string);
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
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
              cryptPublicKeyFileName.set(string);
              BARServer.setOption(selectedJobId,"crypt-public-key",string);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,cryptPublicKeyFileName));
          text.setToolTipText("Public key file used for asymmetric encryption.");

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          Widgets.layout(button,0,5,TableLayoutData.DEFAULT);
          Widgets.addModifyListener(new WidgetModifyListener(button,new WidgetVariable[]{cryptAlgorithm,cryptType})
          {
            public void modified(Control control, WidgetVariable cryptAlgorithm)
            {
            }
            public void modified(Control control, WidgetVariable variables[])
            {
              Widgets.setEnabled(control,
                                    !variables[0].equals("none")
                                 && variables[1].equals("asymmetric")
                                );
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget   = (Button)selectionEvent.widget;
              String fileName = Dialogs.fileOpen(shell,
                                                 "Select public key file",
                                                 cryptPublicKeyFileName.getString(),
                                                 new String[]{"Public key","*.public",
                                                              "All files","*",
                                                             }
                                                );
              if (fileName != null)
              {
                cryptPublicKeyFileName.set(fileName);
                BARServer.setOption(selectedJobId,"crypt-public-key",fileName);
              }
            }
          });
        }

        // crypt password
        label = Widgets.newLabel(tab,"Crypt password:");
        Widgets.layout(label,6,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,0.0,0.0,1.0,0.0,1.0}));
        Widgets.layout(composite,6,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,"default");
          Widgets.layout(button,0,0,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,new WidgetVariable[]{cryptAlgorithm,cryptType})
          {
            public void modified(Control control, WidgetVariable variables[])
            {
              Widgets.setEnabled(control,
                                    !variables[0].equals("none")
                                 && (variables[1].equals("none") || variables[1].equals("symmetric"))
                                );
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              cryptPasswordMode.set("default");
              BARServer.setOption(selectedJobId,"crypt-password-mode","default");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptPasswordMode)
          {
            public void modified(Control control, WidgetVariable cryptPasswordMode)
            {
              ((Button)control).setSelection(cryptPasswordMode.equals("default"));
            }
          });
          button.setToolTipText("Use default password from configuration file for encryption.");

          button = Widgets.newRadio(composite,"ask");
          Widgets.layout(button,0,1,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,new WidgetVariable[]{cryptAlgorithm,cryptType})
          {
            public void modified(Control control, WidgetVariable variables[])
            {
              Widgets.setEnabled(control,
                                    !variables[0].equals("none")
                                 && (variables[1].equals("none") || variables[1].equals("symmetric"))
                                );
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              cryptPasswordMode.set("ask");
              BARServer.setOption(selectedJobId,"crypt-password-mode","ask");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptPasswordMode)
          {
            public void modified(Control control, WidgetVariable cryptPasswordMode)
            {
              ((Button)control).setSelection(cryptPasswordMode.equals("ask"));
            }
          });
          button.setToolTipText("Input password for encryption.");

          button = Widgets.newRadio(composite,"this");
          Widgets.layout(button,0,2,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,new WidgetVariable[]{cryptAlgorithm,cryptType})
          {
            public void modified(Control control, WidgetVariable variables[])
            {
              Widgets.setEnabled(control,
                                    !variables[0].equals("none")
                                 && (variables[1].equals("none") || variables[1].equals("symmetric"))
                                );
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              cryptPasswordMode.set("config");
              BARServer.setOption(selectedJobId,"crypt-password-mode","config");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptPasswordMode)
          {
            public void modified(Control control, WidgetVariable cryptPasswordMode)
            {
              ((Button)control).setSelection(cryptPasswordMode.equals("config"));
            }
          });
          button.setToolTipText("Use specified password for encryption.");

          widgetCryptPassword1 = Widgets.newPassword(composite);
          Widgets.layout(widgetCryptPassword1,0,3,TableLayoutData.WE);
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword1,new WidgetVariable[]{cryptAlgorithm,cryptType,cryptPasswordMode})
          {
            public void modified(Control control, WidgetVariable variables[])
            {
              Widgets.setEnabled(control,
                                    !variables[0].equals("none")
                                 && (variables[1].equals("none") || variables[1].equals("symmetric"))
                                 && variables[2].equals("config")
                                );
            }
          });
          widgetCryptPassword1.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              String s = widget.getText();
              if (cryptPassword.getString().equals(s)) color = COLOR_WHITE;
              widget.setBackground(color);
            }
          });
          widgetCryptPassword1.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              String string1 = widgetCryptPassword1.getText();
              String string2 = widgetCryptPassword2.getText();
              if (string1.equals(string2))
              {
                cryptPassword.set(string1);
                BARServer.setOption(selectedJobId,"crypt-password",string1);
              }
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          widgetCryptPassword1.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              String string1 = widgetCryptPassword1.getText();
              String string2 = widgetCryptPassword2.getText();
              if (string1.equals(string2))
              {
                cryptPassword.set(string1);
                BARServer.setOption(selectedJobId,"crypt-password",string1);
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword1,cryptPassword));
          widgetCryptPassword1.setToolTipText("Password used for encryption.");

          label = Widgets.newLabel(composite,"Repeat:");
          Widgets.layout(label,0,4,TableLayoutData.W);

          widgetCryptPassword2 = Widgets.newPassword(composite);
          Widgets.layout(widgetCryptPassword2,0,5,TableLayoutData.WE);
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword2,new WidgetVariable[]{cryptAlgorithm,cryptType,cryptPasswordMode})
          {
            public void modified(Control control, WidgetVariable[] variables)
            {
              Widgets.setEnabled(control,
                                    !variables[0].equals("none")
                                 && (variables[1].equals("none") || variables[1].equals("symmetric"))
                                 && variables[2].equals("config")
                                );
            }
          });
          widgetCryptPassword2.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              String s = widget.getText();
              if (cryptPassword.getString().equals(s)) color = COLOR_WHITE;
              widget.setBackground(color);
            }
          });
          widgetCryptPassword2.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              String string1 = widgetCryptPassword1.getText();
              String string2 = widgetCryptPassword2.getText();
              if (string1.equals(string2))
              {
                cryptPassword.set(string1);
                BARServer.setOption(selectedJobId,"crypt-password",string1);
              }
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          widgetCryptPassword2.addFocusListener(new FocusListener()
          {
            public void focusGained(FocusEvent focusEvent)
            {
            }
            public void focusLost(FocusEvent focusEvent)
            {
              String string1 = widgetCryptPassword1.getText();
              String string2 = widgetCryptPassword2.getText();
              if (string1.equals(string2))
              {
                cryptPassword.set(string1);
                BARServer.setOption(selectedJobId,"crypt-password",string1);
              }
              else
              {
                Dialogs.error(shell,"Crypt passwords are not equal!");
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword2,cryptPassword));
          widgetCryptPassword1.setToolTipText("Password used for encryption.");
        }

        // archive type
        label = Widgets.newLabel(tab,"Mode:");
        Widgets.layout(label,7,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,0.0,0.0,0.0,0.0,1.0,0.0}));
        Widgets.layout(composite,7,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,"normal");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archiveType.set("normal");
              BARServer.setOption(selectedJobId,"archive-type","normal");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archiveType)
          {
            public void modified(Control control, WidgetVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("normal"));
            }
          });
          button.setToolTipText("Normal mode: do not create incremental data files.");

          button = Widgets.newRadio(composite,"full");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archiveType.set("full");
              BARServer.setOption(selectedJobId,"archive-type","full");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archiveType)
          {
            public void modified(Control control, WidgetVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("full"));
            }
          });
          button.setToolTipText("Full mode: store all entries and create incremental data files.");

          button = Widgets.newRadio(composite,"incremental");
          Widgets.layout(button,0,2,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archiveType.set("incremental");
              BARServer.setOption(selectedJobId,"archive-type","incremental");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archiveType)
          {
            public void modified(Control control, WidgetVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("incremental"));
            }
          });
          button.setToolTipText("Incremental mode: store only modified entries since last full or incremental storage.");

          button = Widgets.newRadio(composite,"differential");
          Widgets.layout(button,0,3,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              archiveType.set("differential");
              BARServer.setOption(selectedJobId,"archive-type","differential");
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archiveType)
          {
            public void modified(Control control, WidgetVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("differential"));
            }
          });
          button.setToolTipText("Differential mode: store only modified entries since last full storage.");
        }

        // file name
        label = Widgets.newLabel(tab,"File name:");
        Widgets.layout(label,8,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(composite,8,1,TableLayoutData.WE);
        {
          text = Widgets.newText(composite);
          Widgets.layout(text,0,0,TableLayoutData.WE);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              String s = widget.getText();
              if (storageFileName.getString().equals(s)) color = COLOR_WHITE;
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text widget = (Text)selectionEvent.widget;
              storageFileName.set(widget.getText());
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
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
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,storageFileName));
          text.setToolTipText("Name of storage files to create. Several macros are supported. Click on button to the right to open storage file name editor.");

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId != 0)
              {
                storageFileNameEdit();
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            }
          });
        }

        // incremental file name
        label = Widgets.newLabel(tab,"Incremental file name:");
        Widgets.layout(label,9,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(composite,9,1,TableLayoutData.WE);
        {
          text = Widgets.newText(composite);
          Widgets.layout(text,0,0,TableLayoutData.WE);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              Color  color  = COLOR_MODIFIED;
              String string = widget.getText();
              if (incrementalListFileName.getString().equals(string)) color = COLOR_WHITE;
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
              BARServer.setOption(selectedJobId,"incremental-list-file",string);
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
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
              BARServer.setOption(selectedJobId,"incremental-list-file",string);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,incrementalListFileName));
          text.setToolTipText("Name of incremental data file. If no file name is given a name is derived automatically from the storage file name.");

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
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
          });
        }

        // destination
        label = Widgets.newLabel(tab,"Destination:");
        Widgets.layout(label,10,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,10,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,"file system");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("filesystem");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("filesystem"));
            }
          });
          button.setToolTipText("Store created storage files into file system destination.");

          button = Widgets.newRadio(composite,"ftp");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("ftp");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("ftp"));
            }
          });
          button.setToolTipText("Store created storage files on FTP server.");

          button = Widgets.newRadio(composite,"scp");
          Widgets.layout(button,0,2,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("scp");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("scp"));
            }
          });
          button.setToolTipText("Store created storage files on SSH server via SCP protocol.");

          button = Widgets.newRadio(composite,"sftp");
          Widgets.layout(button,0,3,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("sftp");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("sftp"));
            }
          });
          button.setToolTipText("Store created storage files on SSH server via SFTP protocol.");

          button = Widgets.newRadio(composite,"webdav");
          Widgets.layout(button,0,4,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("webdav");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("webdav"));
            }
          });
          button.setToolTipText("Store created storage files on Webdav server.");

          button = Widgets.newRadio(composite,"CD");
          Widgets.layout(button,0,5,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              boolean changedFlag = storageType.set("cd");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());

              if (   changedFlag
                  && !archivePartSizeFlag.getBoolean()
                 )
              {
                Dialogs.warning(shell,"When writing to a CD without splitting enabled\nthe resulting archive file may not fit on medium.");
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (volumeSize.getLong() <= 0)
                 )
              {
                Dialogs.warning(shell,"When writing to a CD without setting medium size\nthe resulting archive file may not fit on medium.");
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (archivePartSize.getLong() > 0)
                  && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                  && ecc.getBoolean()
                 )
              {
                Dialogs.warning(shell,"When writing to a CD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 250M, medium 500M\n- part size 140M, medium 560M\n");
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("cd"));
            }
          });
          button.setToolTipText("Store created storage files on CD.");

          button = Widgets.newRadio(composite,"DVD");
          Widgets.layout(button,0,6,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              boolean changedFlag = storageType.set("dvd");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());

              if (   changedFlag
                  && !archivePartSizeFlag.getBoolean()
                 )
              {
                Dialogs.warning(shell,"When writing to a DVD without splitting enabled\nthe resulting archive file may not fit on medium.");
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (volumeSize.getLong() <= 0)
                 )
              {
                Dialogs.warning(shell,"When writing to a DVD without setting medium size\nthe resulting archive file may not fit on medium.");
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (archivePartSize.getLong() > 0)
                  && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                  && ecc.getBoolean()
                 )
              {
                Dialogs.warning(shell,"When writing to a DVD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G");
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("dvd"));
            }
          });
          button.setToolTipText("Store created storage files on DVD.");

          button = Widgets.newRadio(composite,"BD");
          Widgets.layout(button,0,7,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              boolean changedFlag = storageType.set("bd");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());

              if (   changedFlag
                  && !archivePartSizeFlag.getBoolean()
                 )
              {
                Dialogs.warning(shell,"When writing to a BD without splitting enabled\nthe resulting archive file may not fit on medium.");
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (volumeSize.getLong() <= 0)
                 )
              {
                Dialogs.warning(shell,"When writing to a BD without setting medium size\nthe resulting archive file may not fit on medium.");
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (archivePartSize.getLong() > 0)
                  && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                  && ecc.getBoolean()
                 )
              {
                Dialogs.warning(shell,"When writing to a BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 1G, medium 20G\n- part size 5G, medium 20G\n");
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("bd"));
            }
          });
          button.setToolTipText("Store created storage files on BD.");

          button = Widgets.newRadio(composite,"device");
          Widgets.layout(button,0,8,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              storageType.set("device");
              BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            public void modified(Control control, WidgetVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("device"));
            }
          });
          button.setToolTipText("Store created storage files on device.");
        }

        // destination file system
        composite = Widgets.newComposite(tab,SWT.BORDER);
        composite.setLayout(new TableLayout(0.0,new double[]{1.0}));
        Widgets.layout(composite,11,1,TableLayoutData.WE|TableLayoutData.N);
        Widgets.addModifyListener(new WidgetModifyListener(composite,storageType)
        {
          public void modified(Control control, WidgetVariable variable)
          {
            Widgets.setVisible(control,variable.equals("filesystem"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          button = Widgets.newCheckbox(composite,"overwrite archive files");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button  widget      = (Button)selectionEvent.widget;
              boolean checkedFlag = widget.getSelection();
              overwriteArchiveFiles.set(checkedFlag);
              BARServer.setOption(selectedJobId,"overwrite-archive-files",checkedFlag);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,overwriteArchiveFiles));
          button.setToolTipText("If enabled overwrite existing archive files. If disabled do not overwrite existing files and stop with an error.");
        }

        // destination ftp
        composite = Widgets.newComposite(tab,SWT.BORDER);
        composite.setLayout(new TableLayout(0.0,1.0));
        Widgets.layout(composite,11,1,TableLayoutData.WE|TableLayoutData.N);
        Widgets.addModifyListener(new WidgetModifyListener(composite,storageType)
        {
          public void modified(Control control, WidgetVariable variable)
          {
            Widgets.setVisible(control,variable.equals("ftp"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          composite = Widgets.newComposite(composite,SWT.NONE);
          composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0,0.0,1.0,0.0,1.0}));
          Widgets.layout(composite,0,0,TableLayoutData.WE);
          {
            label = Widgets.newLabel(composite,"User:");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(composite);
            Widgets.layout(text,0,1,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageLoginName.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginName.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageLoginName));
            text.setToolTipText("FTP server user login name. Leave it empty to use the default name from the configuration file.");

            label = Widgets.newLabel(composite,"Host:");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(composite);
            Widgets.layout(text,0,3,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageHostName.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageHostName.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageHostName));
            text.setToolTipText("FTP server name.");

            label = Widgets.newLabel(composite,"Password:");
            Widgets.layout(label,0,4,TableLayoutData.W);

            text = Widgets.newPassword(composite);
            Widgets.layout(text,0,5,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageLoginPassword.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginPassword.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageLoginPassword));
            text.setToolTipText("FTP server login password. Leave it empty to use the default password from the configuration file.");
          }

/*
          label = Widgets.newLabel(composite,"Max. band width:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          composite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(composite,1,1,TableLayoutData.WE);
          {
            button = Widgets.newRadio(composite,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setOption(selectedJobId,"max-band-width",0);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            button = Widgets.newRadio(composite,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                archivePartSizeFlag.set(true);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
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
        composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));
        Widgets.layout(composite,11,1,TableLayoutData.WE|TableLayoutData.N);
        Widgets.addModifyListener(new WidgetModifyListener(composite,storageType)
        {
          public void modified(Control control, WidgetVariable variable)
          {
            Widgets.setVisible(control,variable.equals("scp") || variable.equals("sftp"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Server");
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE|SWT.BORDER);
          subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0,1.0,0.0,1.0}));
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            label = Widgets.newLabel(subComposite,"Login:");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,1,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageLoginName.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginName.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageLoginName));
            text.setToolTipText("SSH login name. Leave it empty to use the default login name from the configuration file.");

            label = Widgets.newLabel(subComposite,"Host:");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,3,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageHostName.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageHostName.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageHostName));
            text.setToolTipText("SSH login host name.");

            label = Widgets.newLabel(subComposite,"Port:");
            Widgets.layout(label,0,4,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            text.setData("showedErrorDialog",false);
            Widgets.layout(text,0,5,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                try
                {
                  long n = !string.equals("") ? Long.parseLong(string) : 0;
                  if (storageHostPort.getLong() == n) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
                widget.setData("showedErrorDialog",false);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text   widget = (Text)selectionEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = !string.equals("") ? Long.parseLong(string) : 0;
                  if ((n >= 0) && (n <= 65535))
                  {
                    storageHostPort.set(n);
                    BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
                  }
                  else
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,"'"+n+"' is out of range!\n\nEnter a number between 0 and 65535.");
                      widget.forceFocus();
                    }
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid port number!\n\nEnter a number between 0 and 65535.");
                    widget.forceFocus();
                  }
                }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
                Text widget = (Text)focusEvent.widget;
                widget.setData("showedErrorDialog",false);
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Text   widget = (Text)focusEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = !string.equals("") ? Long.parseLong(string) : 0;
                  if ((n >= 0) && (n <= 65535))
                  {
                    storageHostPort.set(n);
                    BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
                  }
                  else
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,"'"+n+"' is out of range!\n\nEnter a number between 0 and 65535.");
                      widget.forceFocus();
                    }
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid port number!\n\nEnter a number between 0 and 65535.");
                    widget.forceFocus();
                  }
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageHostPort));
            text.setToolTipText("SSH login port number. Set to 0 to use default port number from configuration file.");
          }

          label = Widgets.newLabel(composite,"SSH public key:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,1,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (sshPublicKeyFileName.getString().equals(string)) color = COLOR_WHITE;
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
                BARServer.setOption(selectedJobId,"ssh-public-key",string);
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"ssh-public-key",string);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPublicKeyFileName));
            text.setToolTipText("SSH public key file name. Leave it empty to use the default key file from the configuration file.");

            button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
            Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget   = (Button)selectionEvent.widget;
                String fileName = Dialogs.fileOpen(shell,
                                                   "Select SSH public key file",
                                                   incrementalListFileName.getString(),
                                                   new String[]{"Public key files","*.pub",
                                                                "All files","*",
                                                               }
                                                  );
                if (fileName != null)
                {
                  sshPublicKeyFileName.set(fileName);
                }
              }
            });
          }

          label = Widgets.newLabel(composite,"SSH private key:");
          Widgets.layout(label,2,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,2,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (sshPrivateKeyFileName.getString().equals(string)) color = COLOR_WHITE;
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
                BARServer.setOption(selectedJobId,"ssh-private-key",string);
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"ssh-private-key",string);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPrivateKeyFileName));
            text.setToolTipText("SSH private key file name. Leave it empty to use the default key file from the configuration file.");

            button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
            Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget   = (Button)selectionEvent.widget;
                String fileName = Dialogs.fileOpen(shell,
                                                   "Select SSH private key file",
                                                   incrementalListFileName.getString(),
                                                   new String[]{"All files","*",
                                                               }
                                                  );
                if (fileName != null)
                {
                  sshPrivateKeyFileName.set(fileName);
                }
              }
            });
          }

/*
          label = Widgets.newLabel(composite,"Max. band width:");
          Widgets.layout(label,3,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE);
          {
            button = Widgets.newRadio(subComposite,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setOption(selectedJobId,"max-band-width",0);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetSCPSFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            button = Widgets.newRadio(subComposite,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setOption(selectedJobId,"max-band-width",0);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
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

        // destination Webdav
        composite = Widgets.newComposite(tab,SWT.BORDER);
        composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));
        Widgets.layout(composite,11,1,TableLayoutData.WE|TableLayoutData.N);
        Widgets.addModifyListener(new WidgetModifyListener(composite,storageType)
        {
          public void modified(Control control, WidgetVariable variable)
          {
            Widgets.setVisible(control,variable.equals("webdav"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Server");
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE|SWT.BORDER);
          subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0,1.0,0.0,1.0}));
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            label = Widgets.newLabel(subComposite,"Login:");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,1,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageLoginName.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginName.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageLoginName));
            text.setToolTipText("SSH login name. Leave it empty to use the default login name from the configuration file.");

            label = Widgets.newLabel(subComposite,"Host:");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,3,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageHostName.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageHostName.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageHostName));
            text.setToolTipText("SSH login host name.");

            label = Widgets.newLabel(subComposite,"Port:");
            Widgets.layout(label,0,4,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            text.setData("showedErrorDialog",false);
            Widgets.layout(text,0,5,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                try
                {
                  long n = !string.equals("") ? Long.parseLong(string) : 0;
                  if (storageHostPort.getLong() == n) color = COLOR_WHITE;
                }
                catch (NumberFormatException exception)
                {
                }
                widget.setBackground(color);
                widget.setData("showedErrorDialog",false);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text   widget = (Text)selectionEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = !string.equals("") ? Long.parseLong(string) : 0;
                  if ((n >= 0) && (n <= 65535))
                  {
                    storageHostPort.set(n);
                    BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
                  }
                  else
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,"'"+n+"' is out of range!\n\nEnter a number between 0 and 65535.");
                      widget.forceFocus();
                    }
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid port number!\n\nEnter a number between 0 and 65535.");
                    widget.forceFocus();
                  }
                }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
                Text widget = (Text)focusEvent.widget;
                widget.setData("showedErrorDialog",false);
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Text   widget = (Text)focusEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = !string.equals("") ? Long.parseLong(string) : 0;
                  if ((n >= 0) && (n <= 65535))
                  {
                    storageHostPort.set(n);
                    BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
                  }
                  else
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,"'"+n+"' is out of range!\n\nEnter a number between 0 and 65535.");
                      widget.forceFocus();
                    }
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid port number!\n\nEnter a number between 0 and 65535.");
                    widget.forceFocus();
                  }
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageHostPort));
            text.setToolTipText("SSH login port number. Set to 0 to use default port number from configuration file.");
          }

          label = Widgets.newLabel(composite,"SSH public key:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,1,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (sshPublicKeyFileName.getString().equals(string)) color = COLOR_WHITE;
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
                BARServer.setOption(selectedJobId,"ssh-public-key",string);
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"ssh-public-key",string);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPublicKeyFileName));
            text.setToolTipText("SSH public key file name. Leave it empty to use the default key file from the configuration file.");

            button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
            Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget   = (Button)selectionEvent.widget;
                String fileName = Dialogs.fileOpen(shell,
                                                   "Select SSH public key file",
                                                   incrementalListFileName.getString(),
                                                   new String[]{"Public key files","*.pub",
                                                                "All files","*",
                                                               }
                                                  );
                if (fileName != null)
                {
                  sshPublicKeyFileName.set(fileName);
                }
              }
            });
          }

          label = Widgets.newLabel(composite,"SSH private key:");
          Widgets.layout(label,2,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,2,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (sshPrivateKeyFileName.getString().equals(string)) color = COLOR_WHITE;
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
                BARServer.setOption(selectedJobId,"ssh-private-key",string);
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"ssh-private-key",string);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPrivateKeyFileName));
            text.setToolTipText("SSH private key file name. Leave it empty to use the default key file from the configuration file.");

            button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
            Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget   = (Button)selectionEvent.widget;
                String fileName = Dialogs.fileOpen(shell,
                                                   "Select SSH private key file",
                                                   incrementalListFileName.getString(),
                                                   new String[]{"All files","*",
                                                               }
                                                  );
                if (fileName != null)
                {
                  sshPrivateKeyFileName.set(fileName);
                }
              }
            });
          }

/*
          label = Widgets.newLabel(composite,"Max. band width:");
          Widgets.layout(label,3,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE);
          {
            button = Widgets.newRadio(subComposite,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setOption(selectedJobId,"max-band-width",0);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetWebdavMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            button = Widgets.newRadio(subComposite,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setOption(selectedJobId,"max-band-width",0);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetWebdavMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            widgetWebdavMaxBandWidth = Widgets.newCombo(subComposite,null);
            widgetWebdavMaxBandWidth.setItems(new String[]{"32K","64K","128K","256K","512K"});
            Widgets.layout(widgetWebdavMaxBandWidth,0,2,TableLayoutData.W);
          }
*/
        }

        // destination cd/dvd/bd
        composite = Widgets.newComposite(tab,SWT.BORDER);
        composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));
        Widgets.layout(composite,11,1,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(composite,storageType)
        {
          public void modified(Control control, WidgetVariable variable)
          {
            Widgets.setVisible(control,variable.equals("cd") || variable.equals("dvd") || variable.equals("bd"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Device:");
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageDeviceName.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageDeviceName.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageDeviceName));
            text.setToolTipText("Device name. Leave it empty to use system default device name.");

            button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
            Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget   = (Button)selectionEvent.widget;
                String fileName = Dialogs.fileOpen(shell,
                                                   "Select device name",
                                                   incrementalListFileName.getString(),
                                                   new String[]{"All files","*",
                                                               }
                                                  );
                if (fileName != null)
                {
                  storageDeviceName.set(fileName);
                }
              }
            });
          }

          label = Widgets.newLabel(composite,"Size:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,1,1,TableLayoutData.WE);
          {
            combo = Widgets.newCombo(subComposite);
            combo.setItems(new String[]{"630M","700M","2G","3G","3.6G","4G","7.2G","8G"});
            combo.setData("showedErrorDialog",false);
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
                widget.setData("showedErrorDialog",false);
              }
            });
            combo.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = Units.parseByteSize(string);
                  boolean changedFlag = volumeSize.set(n);
                  BARServer.setOption(selectedJobId,"volume-size",n);

                  if (   changedFlag
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                      && ecc.getBoolean()
                     )
                  {
                    Dialogs.warning(shell,"When writing to a CD/DVD/BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 140M, medium 560M\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G");
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                    widget.forceFocus();
                  }
                }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = widget.getText();
                try
                {
                  long  n = Units.parseByteSize(string);
                  boolean changedFlag = volumeSize.set(n);
                  BARServer.setOption(selectedJobId,"volume-size",n);

                  if (   changedFlag
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                      && ecc.getBoolean()
                     )
                  {
                    Dialogs.warning(shell,"When writing to a CD/DVD/BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 140M, medium 560M\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G");
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                    widget.forceFocus();
                  }
                }
              }
            });
            combo.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
                Combo widget = (Combo)focusEvent.widget;
                widget.setData("showedErrorDialog",false);
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Combo  widget = (Combo)focusEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = Units.parseByteSize(string);
                  boolean changedFlag = volumeSize.set(n);
                  BARServer.setOption(selectedJobId,"volume-size",n);

                  if (   changedFlag
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                      && ecc.getBoolean()
                     )
                  {
                    Dialogs.warning(shell,"When writing to a CD/DVD/BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 140M, medium 560M\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G");
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                    widget.forceFocus();
                  }
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(combo,volumeSize)
            {
              public String getString(WidgetVariable variable)
              {
                return Units.formatByteSize(variable.getLong());
              }
            });
            combo.setToolTipText("Size of medium. You may specify a smaller value than the real physical size when you have enabled error-correction codes.");

            label = Widgets.newLabel(subComposite,"bytes");
            Widgets.layout(label,0,1,TableLayoutData.W);
          }

          label = Widgets.newLabel(composite,"Options:");
          Widgets.layout(label,3,0,TableLayoutData.NW);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE);
          {
            button = Widgets.newCheckbox(subComposite,"add error-correction codes");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();
                boolean changedFlag = ecc.set(checkedFlag);
                BARServer.setOption(selectedJobId,"ecc",checkedFlag);

                if (   changedFlag
                    && archivePartSizeFlag.getBoolean()
                    && (archivePartSize.getLong() > 0)
                    && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                    && ecc.getBoolean()
                   )
                {
                  Dialogs.warning(shell,"When writing to a CD/DVD/BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 140M, medium 560M\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G");
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,ecc));
            button.setToolTipText("Add error-correction codes to CD/DVD/BD image (require dvdisaster tool).");

            button = Widgets.newCheckbox(subComposite,"wait for first volume");
            Widgets.layout(button,1,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();
                waitFirstVolume.set(checkedFlag);
                BARServer.setOption(selectedJobId,"wait-first-volume",checkedFlag);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,waitFirstVolume));
            button.setToolTipText("Wait until first volume is loaded.");
          }
        }

        // destination device
        composite = Widgets.newComposite(tab,SWT.BORDER);
        composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));
        Widgets.layout(composite,11,1,TableLayoutData.WE|TableLayoutData.N);
        Widgets.addModifyListener(new WidgetModifyListener(composite,storageType)
        {
          public void modified(Control control, WidgetVariable variable)
          {
            Widgets.setVisible(control,variable.equals("device"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Device:");
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageDeviceName.getString().equals(string)) color = COLOR_WHITE;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageDeviceName.set(widget.getText());
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
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
                BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageDeviceName));
            text.setToolTipText("Device name.");

            button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
            Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget   = (Button)selectionEvent.widget;
                String fileName = Dialogs.fileOpen(shell,
                                                   "Select device name",
                                                   incrementalListFileName.getString(),
                                                   new String[]{"All files","*",
                                                               }
                                                  );
                if (fileName != null)
                {
                  storageDeviceName.set(fileName);
                }
              }
            });
          }

          label = Widgets.newLabel(composite,"Size:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,1,1,TableLayoutData.WE);
          {
            combo = Widgets.newCombo(subComposite);
            combo.setItems(new String[]{"2G","3G","3.6G","4G"});
            combo.setData("showedErrorDialog",false);
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
                widget.setData("showedErrorDialog",false);
              }
            });
            combo.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = Units.parseByteSize(string);
                  volumeSize.set(n);
                  BARServer.setOption(selectedJobId,"volume-size",n);
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                    widget.forceFocus();
                  }
                }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = widget.getText();
                try
                {
                  long  n = Units.parseByteSize(string);
                  volumeSize.set(n);
                  BARServer.setOption(selectedJobId,"volume-size",n);
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                    widget.forceFocus();
                  }
                }
              }
            });
            text.addFocusListener(new FocusListener()
            {
              public void focusGained(FocusEvent focusEvent)
              {
                Combo widget = (Combo)focusEvent.widget;
                widget.setData("showedErrorDialog",false);
              }
              public void focusLost(FocusEvent focusEvent)
              {
                Combo  widget = (Combo)focusEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = Units.parseByteSize(string);
                  volumeSize.set(n);
                  BARServer.setOption(selectedJobId,"volume-size",n);
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,"'"+string+"' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB.");
                    widget.forceFocus();
                  }
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(combo,volumeSize)
            {
              public String getString(WidgetVariable variable)
              {
                return Units.formatByteSize(variable.getLong());
              }
            });
            combo.setToolTipText("Size of medium for device.");

            label = Widgets.newLabel(subComposite,"bytes");
            Widgets.layout(label,0,1,TableLayoutData.W);
          }
        }
      }

      tab = Widgets.addTab(widgetTabFolder,"Schedule");
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // list
        widgetScheduleList = Widgets.newTable(tab,SWT.NONE);
        Widgets.layout(widgetScheduleList,0,0,TableLayoutData.NSWE);
        widgetScheduleList.addMouseListener(new MouseListener()
        {
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
            scheduleEdit();
          }
          public void mouseDown(final MouseEvent mouseEvent)
          {
          }
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });
        widgetScheduleList.addKeyListener(new KeyListener()
        {
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          public void keyReleased(KeyEvent keyEvent)
          {
            if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
            {
              Widgets.invoke(widgetScheduleListAdd);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.CR))
            {
              Widgets.invoke(widgetScheduleListEdit);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
            {
              Widgets.invoke(widgetScheduleListRemove);
            }
          }
        });
        SelectionListener scheduleListColumnSelectionListener = new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn            tableColumn = (TableColumn)selectionEvent.widget;
            ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleList,tableColumn);
            synchronized(scheduleList)
            {
              Widgets.sortTableColumn(widgetScheduleList,tableColumn,scheduleDataComparator);
            }
          }
        };
        tableColumn = Widgets.addTableColumn(widgetScheduleList,0,"Date",        SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,1,"Week day",    SWT.LEFT,250,true );
        synchronized(scheduleList)
        {
          Widgets.sortTableColumn(widgetScheduleList,tableColumn,new ScheduleDataComparator(widgetScheduleList,tableColumn));
        }
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,2,"Time",        SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,3,"Archive type",SWT.LEFT, 80,true );
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,4,"Custom text", SWT.LEFT, 90,true );
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,5,"Enabled",     SWT.LEFT, 60,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);

        menu = Widgets.newPopupMenu(shell);
        {
          menuItem = Widgets.addMenuItem(menu,"Add\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              scheduleNew();
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Edit\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              scheduleEdit();
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Clone\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              scheduleClone();
            }
          });

          menuItem = Widgets.addMenuItem(menu,"Remove\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;
              scheduleDelete();
            }
          });
        }
        widgetScheduleList.setMenu(menu);
        widgetScheduleList.setToolTipText("List with schedule entries.\nDouble-click to edit entry, right-click to open context menu.");

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          widgetScheduleListAdd = Widgets.newButton(composite,"Add\u2026");
          Widgets.layout(widgetScheduleListAdd,0,0,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetScheduleListAdd.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              scheduleNew();
            }
          });
          widgetScheduleListAdd.setToolTipText("Add new schedule entry.");

          widgetScheduleListEdit = Widgets.newButton(composite,"Edit\u2026");
          Widgets.layout(widgetScheduleListEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetScheduleListEdit.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              scheduleEdit();
            }
          });
          widgetScheduleListEdit.setToolTipText("Edit schedule entry.");

          button = Widgets.newButton(composite,"Clone\u2026");
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              scheduleClone();
            }
          });
          button.setToolTipText("Clone schedule entry.");

          widgetScheduleListRemove = Widgets.newButton(composite,"Remove\u2026");
          Widgets.layout(widgetScheduleListRemove,0,3,TableLayoutData.DEFAULT,0,0,0,0,90,SWT.DEFAULT);
          widgetScheduleListRemove.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              scheduleDelete();
            }
          });
          widgetScheduleListRemove.setToolTipText("Remove schedule entry.");
        }
      }
    }
    Widgets.addEventListener(new WidgetEventListener(widgetTabFolder,selectJobEvent)
    {
      public void trigger(Control control)
      {
        Widgets.setEnabled(control,selectedJobId != 0);
      }
    });

    // add root devices
    addDirectoryRootDevices();
    addDevicesList();

    // update data
    updateJobList();
  }

  /** select job by name
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
        selectJobEvent.trigger();

        update();
      }
    }
  }

  //-----------------------------------------------------------------------

  void setTabStatus(TabStatus tabStatus)
  {
    this.tabStatus = tabStatus;
  }

  /** get archive name
   * @return archive name
   *   ftp://<login name>:<login password>@<host name>[:<host port>]/<file name>
   *   scp://<login name>@<host name>:<host port>/<file name>
   *   sftp://<login name>@<host name>:<host port>/<file name>
   *   webdav://<login name>@<host name>/<file name>
   *   cd://<device name>/<file name>
   *   dvd://<device name>/<file name>
   *   bd://<device name>/<file name>
   *   device://<device name>/<file name>
   *   file://<file name>
   */
  private String getArchiveName()
  {
    ArchiveNameParts archiveNameParts = new ArchiveNameParts(StorageTypes.parse(storageType.getString()),
                                                             storageLoginName.getString(),
                                                             storageLoginPassword.getString(),
                                                             storageHostName.getString(),
                                                             (int)storageHostPort.getLong(),
                                                             storageDeviceName.getString(),
                                                             storageFileName.getString()
                                                            );

    return archiveNameParts.getName();
  }

  /** parse archive name
   * @param name archive name string
   *   ftp://<login name>:<login password>@<host name>[:<host port>]/<file name>
   *   scp://<login name>@<host name>:<host port>/<file name>
   *   sftp://<login name>@<host name>:<host port>/<file name>
   *   webdav://<login name>@<host name>/<file name>
   *   cd://<device name>/<file name>
   *   dvd://<device name>/<file name>
   *   bd://<device name>/<file name>
   *   device://<device name>/<file name>
   *   file://<file name>
   *   <file name>
   */
  private void parseArchiveName(String name)
  {
    ArchiveNameParts archiveNameParts = new ArchiveNameParts(name);

    storageType.set         (archiveNameParts.type.toString());
    storageLoginName.set    (archiveNameParts.loginName      );
    storageLoginPassword.set(archiveNameParts.loginPassword  );
    storageHostName.set     (archiveNameParts.hostName       );
    storageHostPort.set     (archiveNameParts.hostPort       );
    storageDeviceName.set   (archiveNameParts.deviceName     );
    storageFileName.set     (archiveNameParts.fileName       );
  }

  //-----------------------------------------------------------------------

  /** clear job data
   */
  private void clearJobData()
  {
    Widgets.removeAllTableEntries(widgetIncludeTable);
    Widgets.removeAllListEntries(widgetExcludeList);
    Widgets.removeAllListEntries(widgetCompressExcludeList);
    Widgets.removeAllTableEntries(widgetScheduleList);
  }

  /** update selected job data
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
      parseArchiveName(BARServer.getStringOption(selectedJobId,"archive-name"));
      archiveType.set(BARServer.getStringOption(selectedJobId,"archive-type"));
      archivePartSize.set(Units.parseByteSize(BARServer.getStringOption(selectedJobId,"archive-part-size"),0));
      archivePartSizeFlag.set(archivePartSize.getLong() > 0);

      String[] compressAlgorithms = StringUtils.split(BARServer.getStringOption(selectedJobId,"compress-algorithm"),"+");
      deltaCompressAlgorithm.set((compressAlgorithms.length >= 1) ? compressAlgorithms[0] : "");
      byteCompressAlgorithm.set((compressAlgorithms.length >= 2) ? compressAlgorithms[1] : "");
      cryptAlgorithm.set(BARServer.getStringOption(selectedJobId,"crypt-algorithm"));
      cryptType.set(BARServer.getStringOption(selectedJobId,"crypt-type"));
      cryptPublicKeyFileName.set(BARServer.getStringOption(selectedJobId,"crypt-public-key"));
      cryptPasswordMode.set(BARServer.getStringOption(selectedJobId,"crypt-password-mode"));
      cryptPassword.set(BARServer.getStringOption(selectedJobId,"crypt-password"));
      incrementalListFileName.set(BARServer.getStringOption(selectedJobId,"incremental-list-file"));
      overwriteArchiveFiles.set(BARServer.getBooleanOption(selectedJobId,"overwrite-archive-files"));
      sshPublicKeyFileName.set(BARServer.getStringOption(selectedJobId,"ssh-public-key"));
      sshPrivateKeyFileName.set(BARServer.getStringOption(selectedJobId,"ssh-private-key"));
/* NYI ???
      maxBandWidth.set(Units.parseByteSize(BARServer.getStringOption(jobId,"max-band-width")));
      maxBandWidthFlag.set(maxBandWidth.getLongOption() > 0);
*/
      volumeSize.set(Units.parseByteSize(BARServer.getStringOption(selectedJobId,"volume-size"),0));
      ecc.set(BARServer.getBooleanOption(selectedJobId,"ecc"));
      waitFirstVolume.set(BARServer.getBooleanOption(selectedJobId,"wait-first-volume"));
      skipUnreadable.set(BARServer.getBooleanOption(selectedJobId,"skip-unreadable"));
      overwriteFiles.set(BARServer.getBooleanOption(selectedJobId,"overwrite-files"));

      updateFileTreeImages();
      updateDeviceImages();
      updateIncludeList();
      updateExcludeList();
      updateCompressExcludeList();
      updateScheduleList();
    }
  }

  /** find index for insert of job in sorted job list
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
    String[]            resultErrorMessage = new String[1];
    ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("JOB_LIST"),
                                         new TypeMap("jobId",int.class,
                                                     "name", String.class
                                                    ),
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

// NYI: how to update job listin status tab?
    // update job list
    synchronized(widgetJobList)
    {
      jobIds.clear();
      widgetJobList.removeAll();
      for (ValueMap resultMap : resultMapList)
      {
        // get data
        int    jobId = resultMap.getInt   ("jobId");
        String name  = resultMap.getString("name" );

// TODO deleted jobs?
        int index = findJobListIndex(name);
        widgetJobList.add(name,index);
        jobIds.put(name,jobId);
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

    final Shell dialog = Dialogs.openModal(shell,"New job",300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetJobName;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,"Name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobName = Widgets.newText(composite);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetAdd = Widgets.newButton(composite,"Add");
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          widget.getShell().close();
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
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget  = (Button)selectionEvent.widget;
        String jobName = widgetJobName.getText();
        if (!jobName.equals(""))
        {
          try
          {
            String[] errorMessage = new String[1];
            String[] result = new String[1];
            int error = BARServer.executeCommand(StringParser.format("JOB_NEW name=%S",jobName),
                                                 errorMessage
                                                );
            if (error == Errors.NONE)
            {
              updateJobList();
              selectJob(jobName);
            }
            else
            {
              Dialogs.error(shell,"Cannot create new job:\n\n"+result[0]);
              widgetJobName.forceFocus();
              return;
            }
          }
          catch (CommunicationError error)
          {
            Dialogs.error(shell,"Cannot create new job:\n\n"+error.getMessage());
            widgetJobName.forceFocus();
            return;
          }
        }
        widget.getShell().close();
      }
    });

    Dialogs.run(dialog);
  }

  /** clone job
   */
  private void jobCopy()
  {
    Composite composite;
    Label     label;
    Button    button;

    assert selectedJobName != null;
    assert selectedJobId != 0;

    final Shell dialog = Dialogs.openModal(shell,"Clone job",300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetJobName;
    final Button widgetCopy;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,"Name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobName = Widgets.newText(composite);
      widgetJobName.setText(selectedJobName);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetCopy = Widgets.newButton(composite,"Clone");
      Widgets.layout(widgetCopy,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          widget.getShell().close();
        }
      });
    }

    // add selection listeners
    widgetJobName.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetCopy.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetCopy.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget  = (Button)selectionEvent.widget;
        String jobName = widgetJobName.getText();
        if (!jobName.equals(""))
        {
          try
          {
            String[] errorMessage = new String[1];
            int error = BARServer.executeCommand(StringParser.format("JOB_COPY jobId=%d newName=%S",
                                                                     selectedJobId,
                                                                     jobName
                                                                    ),
                                                 errorMessage
                                                );
            if (error != Errors.NONE)
            {
              updateJobList();
              selectJob(jobName);
            }
            else
            {
              Dialogs.error(shell,"Cannot copy job:\n\n"+errorMessage[0]);
            }
          }
          catch (CommunicationError error)
          {
            Dialogs.error(shell,"Cannot copy job:\n\n"+error.getMessage());
          }
        }
        widget.getShell().close();
      }
    });

    Widgets.setFocus(widgetJobName);
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

    final Shell dialog = Dialogs.openModal(shell,"Rename job",300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetNewJobName;
    final Button widgetRename;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,"Old name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      label = Widgets.newLabel(composite,selectedJobName);
      Widgets.layout(label,0,1,TableLayoutData.W);

      label = Widgets.newLabel(composite,"New name:");
      Widgets.layout(label,1,0,TableLayoutData.W);

      widgetNewJobName = Widgets.newText(composite);
      widgetNewJobName.setText(selectedJobName);
      Widgets.layout(widgetNewJobName,1,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetRename = Widgets.newButton(composite,"Rename");
      Widgets.layout(widgetRename,0,0,TableLayoutData.W);

      button = Widgets.newButton(composite,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          widget.getShell().close();
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
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget     = (Button)selectionEvent.widget;
        String newJobName = widgetNewJobName.getText();
        if (!newJobName.equals(""))
        {
          try
          {
            String[] errorMessage = new String[1];
            int error = BARServer.executeCommand(StringParser.format("JOB_RENAME jobId=%d newName=%S",
                                                                     selectedJobId,
                                                                     newJobName
                                                                    ),
                                                 errorMessage
                                                );
            if (error == Errors.NONE)
            {
              updateJobList();
              selectJob(newJobName);
            }
            else
            {
              Dialogs.error(shell,"Cannot rename job:\n\n"+errorMessage[0]);
            }
          }
          catch (CommunicationError error)
          {
            Dialogs.error(shell,"Cannot rename job:\n\n"+error.getMessage());
          }
        }
        widget.getShell().close();
      }
    });

    Widgets.setFocus(widgetNewJobName);
    Dialogs.run(dialog);
  }

  /** delete selected job
   */
  private void jobDelete()
  {
    assert selectedJobName != null;
    assert selectedJobId != 0;

    if (Dialogs.confirm(shell,"Delete job '"+selectedJobName+"'?"))
    {
      try
      {
        String[] result = new String[1];
        int error = BARServer.executeCommand(StringParser.format("JOB_DELETE jobId=%d",selectedJobId));
        if (error == Errors.NONE)
        {
          updateJobList();
          selectJobEvent.trigger();
          clear();
        }
        else
        {
          Dialogs.error(shell,"Cannot delete job:\n\n"+result[0]);
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,"Cannot delete job:\n\n"+error.getMessage());
      }
    }
  }

  //-----------------------------------------------------------------------

  /** add directory root devices
   */
  private void addDirectoryRootDevices()
  {
    TreeItem treeItem = Widgets.addTreeItem(widgetFileTree,new FileTreeData("/",FileTypes.DIRECTORY,"/"),true);
    treeItem.setText("/");
    treeItem.setImage(IMAGE_DIRECTORY);
  }

  /** clear file tree, close all sub-directories
   */
  private void clearFileTree()
  {
    // close all directories
    for (TreeItem treeItem : widgetFileTree.getItems())
    {
      treeItem.removeAll();
      new TreeItem(treeItem,SWT.NONE);
    }

    // clear directory info requests
    directoryInfoThread.clear();
  }

  /** find tree item
   * @param name name of tree item
   * @return tree item or null if not found
   */
  private TreeItem findTreeItem(TreeItem treeItems[], String name)
  {
    for (TreeItem treeItem : treeItems)
    {
      FileTreeData fileTreeData = (FileTreeData)treeItem.getData();

      if ((fileTreeData != null) && fileTreeData.name.equals(name))
      {
        return treeItem;
      }
    }

    return null;
  }

  /** open all included sub-directories
   */
  private void openIncludedDirectories()
  {
    // open all included directories
    for (TableItem tableItem : widgetIncludeTable.getItems())
    {
      EntryData entryData = (EntryData)tableItem.getData();

      TreeItem[] treeItems = widgetFileTree.getItems();

      StringBuilder buffer = new StringBuilder();
      for (String part : StringUtils.split(entryData.pattern,BARServer.fileSeparator,true))
      {
        // expand name
        if ((buffer.length() == 0) || (buffer.charAt(buffer.length()-1) != BARServer.fileSeparator)) buffer.append(BARServer.fileSeparator);
        buffer.append(part);

        TreeItem treeItem = findTreeItem(treeItems,buffer.toString());
        if (treeItem != null)
        {
          // open sub-directory
          if (!treeItem.getExpanded())
          {
//Dprintf.dprintf("open %s\n",treeItem);
            Event treeEvent = new Event();
            treeEvent.item = treeItem;
            widgetFileTree.notifyListeners(SWT.Expand,treeEvent);
            treeItem.setExpanded(true);
          }

          // get sub-directory items
          treeItems = treeItem.getItems();
        }
        else
        {
          break;
        }
      }
    }
  }

  /** find index for insert of tree item in sorted list of tree items
   * @param treeItem tree item
   * @param fileTreeData data of tree item
   * @return index in tree item
   */
  private int findFilesTreeIndex(TreeItem treeItem, FileTreeData fileTreeData)
  {
    TreeItem               subTreeItems[]         = treeItem.getItems();
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
  private void addFileTree(TreeItem treeItem)
  {
    FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
    TreeItem     subTreeItem;

    shell.setCursor(waitCursor);

    treeItem.removeAll();

    String[]            resultErrorMessage = new String[1];
    ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("FILE_LIST storageDirectory=%S",
                                                             "file://"+fileTreeData.name
                                                            ),
                                                            new TypeMap("fileType",FileTypes.class,
                                                                        "name",String.class,
                                                                        "size",long.class,
                                                                        "dateTime",long.class,
                                                                        "noBackupFlag",boolean.class,
                                                                        "specialType",SpecialTypes.class
                                                                       ),
                                                            resultErrorMessage,
                                                            resultMapList
                                                    );
    if (error == Errors.NONE)
    {
      for (ValueMap resultMap : resultMapList)
      {
        FileTypes fileType = resultMap.getEnum("fileType");
        switch (fileType)
        {
          case FILE:
            {
              String  name         = resultMap.getString ("name"              );
              long    size         = resultMap.getLong   ("size"              );
              long    dateTime     = resultMap.getLong   ("dateTime"          );
              boolean noBackupFlag = resultMap.getBoolean("noBackupFlag",false);

              // create file tree data
              fileTreeData = new FileTreeData(name,FileTypes.FILE,size,dateTime,new File(name).getName());

              // add entry
              Image image;
              if      (includeHashMap.containsKey(name))
                image = IMAGE_FILE_INCLUDED;
              else if (excludeHashSet.contains(name))
                image = IMAGE_FILE_EXCLUDED;
              else
                image = IMAGE_FILE;

              subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
              subTreeItem.setText(0,fileTreeData.title);
              subTreeItem.setText(1,"FILE");
              subTreeItem.setText(2,Units.formatByteSize(size));
              subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
              subTreeItem.setImage(image);
            }
            break;
          case DIRECTORY:
            {
              String  name         = resultMap.getString ("name"              );
              long    dateTime     = resultMap.getLong   ("dateTime"          );
              boolean noBackupFlag = resultMap.getBoolean("noBackupFlag",false);

              // create file tree data
              fileTreeData = new FileTreeData(name,FileTypes.DIRECTORY,dateTime,new File(name).getName());

              // add entry
              Image   image;
              if      (includeHashMap.containsKey(name))
                image = IMAGE_DIRECTORY_INCLUDED;
              else if (excludeHashSet.contains(name))
                image = IMAGE_DIRECTORY_EXCLUDED;
              else
                image = IMAGE_DIRECTORY;

              subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,true);
              subTreeItem.setText(0,fileTreeData.title);
              subTreeItem.setText(1,"DIR");
              subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
              subTreeItem.setImage(image);

              // request directory info
              directoryInfoThread.add(name,subTreeItem);
            }
            break;
          case LINK:
            {
              String  name         = resultMap.getString ("name"              );
              long    dateTime     = resultMap.getLong   ("dateTime"          );
              boolean noBackupFlag = resultMap.getBoolean("noBackupFlag",false);

              // create file tree data
              fileTreeData = new FileTreeData(name,FileTypes.LINK,dateTime,new File(name).getName());

              // add entry
              Image image;
              if      (includeHashMap.containsKey(name))
                image = IMAGE_LINK_INCLUDED;
              else if (excludeHashSet.contains(name))
                image = IMAGE_LINK_EXCLUDED;
              else
                image = IMAGE_LINK;

              subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
              subTreeItem.setText(0,fileTreeData.title);
              subTreeItem.setText(1,"LINK");
              subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
              subTreeItem.setImage(image);
            }
            break;
          case HARDLINK:
            {
              String  name         = resultMap.getString ("name"              );
              long    size         = resultMap.getLong   ("size"              );
              long    dateTime     = resultMap.getLong   ("dateTime"          );
              boolean noBackupFlag = resultMap.getBoolean("noBackupFlag",false);

              // create file tree data
              fileTreeData = new FileTreeData(name,FileTypes.HARDLINK,size,dateTime,new File(name).getName());

              // add entry
              Image image;
              if      (includeHashMap.containsKey(name))
                image = IMAGE_FILE_INCLUDED;
              else if (excludeHashSet.contains(name))
                image = IMAGE_FILE_EXCLUDED;
              else
                image = IMAGE_FILE;

              subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
              subTreeItem.setText(0,fileTreeData.title);
              subTreeItem.setText(1,"HARDLINK");
              subTreeItem.setText(2,Units.formatByteSize(size));
              subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
              subTreeItem.setImage(image);
            }
            break;
          case SPECIAL:
            {
              String  name     = resultMap.getString("name"    );
              long    size     = resultMap.getLong  ("size"    );
              long    dateTime = resultMap.getLong  ("dateTime");

              SpecialTypes specialType = resultMap.getEnum("specialType");
              switch (specialType)
              {
                case CHARACTER_DEVICE:
                  // create file tree data
                  fileTreeData = new FileTreeData(name,SpecialTypes.CHARACTER_DEVICE,dateTime,name);

                  // add entry
                  subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
                  subTreeItem.setText(0,fileTreeData.title);
                  subTreeItem.setText(1,"CHARACTER DEVICE");
                  subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
                  break;
                case BLOCK_DEVICE:
                  // create file tree data
                  fileTreeData = new FileTreeData(name,SpecialTypes.BLOCK_DEVICE,size,dateTime,name);

                  // add entry
                  subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
                  subTreeItem.setText(0,fileTreeData.title);
                  subTreeItem.setText(1,"BLOCK DEVICE");
                  if (size >= 0) subTreeItem.setText(2,Units.formatByteSize(size));
                  subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
                  break;
                case FIFO:
                  // create file tree data
                  fileTreeData = new FileTreeData(name,SpecialTypes.FIFO,dateTime,name);

                  // add entry
                  subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
                  subTreeItem.setText(0,fileTreeData.title);
                  subTreeItem.setText(1,"FIFO");
                  subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
                  break;
                case SOCKET:
                  // create file tree data
                  fileTreeData = new FileTreeData(name,SpecialTypes.SOCKET,dateTime,name);

                  // add entry
                  subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
                  subTreeItem.setText(0,fileTreeData.title);
                  subTreeItem.setText(1,"SOCKET");
                  subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
                  break;
                case OTHER:
                  // create file tree data
                  fileTreeData = new FileTreeData(name,SpecialTypes.OTHER,dateTime,name);

                  // add entry
                  subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
                  subTreeItem.setText(0,fileTreeData.title);
                  subTreeItem.setText(1,"SPECIAL");
                  subTreeItem.setText(3,simpleDateFormat.format(new Date(dateTime*1000)));
                  break;
              }
            }
            break;
        }
      }
    }
    else
    {
//Dprintf.dprintf("fileTreeData.name=%s fileListResult=%s errorCode=%d\n",fileTreeData.name,fileListResult,errorCode);
       Dialogs.error(shell,"Cannot get file list (error: "+resultErrorMessage[0]+")");
    }

    shell.setCursor(null);
  }

  /** update file tree item images
   * @param treeItem tree item to update
   */
  private void updateFileTreeImages(TreeItem treeItem)
  {
    if (treeItem.getExpanded())
    {
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        updateFileTreeImages(subTreeItem);
      }
    }
    else
    {
      FileTreeData fileTreeData = (FileTreeData)treeItem.getData();

      Image image = null;
      if      (includeHashMap.containsKey(fileTreeData.name) && !excludeHashSet.contains(fileTreeData.name))
      {
        switch (fileTreeData.fileType)
        {
          case FILE:      image = IMAGE_FILE_INCLUDED;      break;
          case DIRECTORY: image = IMAGE_DIRECTORY_INCLUDED; break;
          case LINK:      image = IMAGE_LINK_INCLUDED;      break;
          case HARDLINK:  image = IMAGE_LINK_INCLUDED;      break;
          case SPECIAL:   image = IMAGE_FILE_INCLUDED;      break;
        }
      }
      else if (excludeHashSet.contains(fileTreeData.name))
      {
        switch (fileTreeData.fileType)
        {
          case FILE:      image = IMAGE_FILE_EXCLUDED;      break;
          case DIRECTORY: image = IMAGE_DIRECTORY_EXCLUDED; break;
          case LINK:      image = IMAGE_LINK_EXCLUDED;      break;
          case HARDLINK:  image = IMAGE_LINK_EXCLUDED;      break;
          case SPECIAL:   image = IMAGE_FILE_EXCLUDED;      break;
        }
      }
      else
      {
        switch (fileTreeData.fileType)
        {
          case FILE:      image = IMAGE_FILE;      break;
          case DIRECTORY: image = IMAGE_DIRECTORY; break;
          case LINK:      image = IMAGE_LINK;      break;
          case HARDLINK:  image = IMAGE_LINK;      break;
          case SPECIAL:
            switch (fileTreeData.specialType)
            {
              case CHARACTER_DEVICE: image = IMAGE_FILE;      break;
              case BLOCK_DEVICE:     image = IMAGE_FILE;      break;
              case FIFO:             image = IMAGE_FILE;      break;
              case SOCKET:           image = IMAGE_FILE;      break;
              case OTHER:            image = IMAGE_FILE;      break;
            }
            break;
        }
      }
      treeItem.setImage(image);
    }
  }

  /** update all file tree item images
   */
  private void updateFileTreeImages()
  {
    for (TreeItem treeItem : widgetFileTree.getItems())
    {
      updateFileTreeImages(treeItem);
    }
  }

  //-----------------------------------------------------------------------

  /** add devices to list
   */
  private void addDevicesList()
  {
    String[]            resultErrorMessage = new String[1];
    ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("DEVICE_LIST"),
                                         new TypeMap("name",String.class,
                                                     "size",long.class,
                                                     "mountedFlag",Boolean.class
                                                    ),
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error == Errors.NONE)
    {
      for (ValueMap resultMap : resultMapList)
      {
        long    size        = resultMap.getLong   ("size"       );
        boolean mountedFlag = resultMap.getBoolean("mountedFlag");
        String  name        = resultMap.getString ("name"       );

        // create device data
        DeviceTreeData deviceTreeData = new DeviceTreeData(name,size);

        TreeItem treeItem = Widgets.addTreeItem(widgetDeviceTree,findDeviceIndex(widgetDeviceTree,deviceTreeData),deviceTreeData,false);
        treeItem.setText(0,name);
        treeItem.setText(1,Units.formatByteSize(size));
        treeItem.setImage(IMAGE_DEVICE);
      }
    }
    else
    {
      Dialogs.error(shell,"Cannot get device list:\n\n"+resultErrorMessage[0]);
    }
  }

  /** find index for insert of tree item in sorted list of tree items
   * @param treeItem tree item
   * @param fileTreeData data of tree item
   * @return index in tree item
   */
  private int findDeviceIndex(Tree tree, DeviceTreeData deviceTreeData)
  {
    TreeItem                 treeItems[]              = tree.getItems();
    DeviceTreeDataComparator deviceTreeDataComparator = new DeviceTreeDataComparator(widgetDeviceTree);

    int index = 0;
    while (   (index < treeItems.length)
           && (deviceTreeDataComparator.compare(deviceTreeData,(DeviceTreeData)treeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update images in device tree
   */
  private void updateDeviceImages()
  {
    for (TreeItem treeItem : widgetDeviceTree.getItems())
    {
      DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();

      Image image;
      if      (includeHashMap.containsKey(deviceTreeData.name) && !excludeHashSet.contains(deviceTreeData.name))
        image = IMAGE_DEVICE_INCLUDED;
      else if (excludeHashSet.contains(deviceTreeData.name))
        image = IMAGE_DEVICE;
      else
        image = IMAGE_DEVICE;
      treeItem.setImage(image);
    }
  }

  //-----------------------------------------------------------------------

  /** find index for insert of entry in sorted table
   * @param table table
   * @param pattern pattern to insert
   * @return index in table
   */
  private int findTableIndex(Table table, String pattern)
  {
    TableItem tableItems[] = table.getItems();

    int index = 0;
    while (   (index < tableItems.length)
           && (pattern.compareTo(((EntryData)tableItems[index].getData()).pattern) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of pattern in sorted pattern list
   * @param list list
   * @param pattern pattern to insert
   * @return index in list
   */
  private int findListIndex(List list, String pattern)
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

  //-----------------------------------------------------------------------

  /** update include list
   */
  private void updateIncludeList()
  {
    assert selectedJobId != 0;

    String[]            resultErrorMessage = new String[1];
    ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("INCLUDE_LIST jobId=%d",
                                                             selectedJobId
                                                            ),
                                         new TypeMap("entryType",  EntryTypes.class,
                                                     "patternType",PatternTypes.class,
                                                     "pattern",    String.class
                                                    ),
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    includeHashMap.clear();
    Widgets.removeAllTableEntries(widgetIncludeTable);
    for (ValueMap resultMap : resultMapList)
    {
      // get data
      EntryTypes   entryType   = resultMap.getEnum  ("entryType"  );
      PatternTypes patternType = resultMap.getEnum  ("patternType");
      String       pattern     = resultMap.getString("pattern"    );

      if (!pattern.equals(""))
      {
        EntryData entryData = new EntryData(entryType,pattern);

/*
        // add entry
        Image image;
        if      (includeHashMap.containsKey(name))
          image = IMAGE_DEVICE_INCLUDED;
        else if (excludeHashSet.contains(name))
          image = IMAGE_FILE_EXCLUDED;
        else
          image = IMAGE_DEVICE;
Dprintf.dprintf("name=%s %s",name,includeHashMap.containsKey(name));
*/

        includeHashMap.put(pattern,entryData);
        Widgets.insertTableEntry(widgetIncludeTable,
                                 findTableIndex(widgetIncludeTable,pattern),
                                 (Object)entryData,
                                 entryData.getImage(),
                                 entryData.pattern
                                );
      }
    }
  }

  /** update exclude list
   */
  private void updateExcludeList()
  {
    assert selectedJobId != 0;

    String[]            resultErrorMessage = new String[1];
    ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("EXCLUDE_LIST jobId=%d",
                                                             selectedJobId
                                                            ),
                                         new TypeMap("patternType",PatternTypes.class,
                                                     "pattern",    String.class
                                                    ),
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    excludeHashSet.clear();
    Widgets.removeAllListEntries(widgetExcludeList);

    for (ValueMap resultMap : resultMapList)
    {
      // get data
      PatternTypes patternType = resultMap.getEnum  ("patternType");
      String       pattern     = resultMap.getString("pattern"    );

      if (!pattern.equals(""))
      {
        excludeHashSet.add(pattern);
        widgetExcludeList.add(pattern,findListIndex(widgetExcludeList,pattern));
      }
    }
  }

  /** update compress exclude list
   */
  private void updateCompressExcludeList()
  {
    assert selectedJobId != 0;

    String[]            resultErrorMessage  = new String[1];
    ArrayList<ValueMap> resultMapList       = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST jobId=%d",
                                                             selectedJobId
                                                            ),
                                                            new TypeMap("patternType",PatternTypes.class,
                                                                        "pattern",    String.class
                                                                       ),
                                                            resultErrorMessage,
                                                            resultMapList
                                                           );
    if (error != Errors.NONE)
    {
      return;
    }

    compressExcludeHashSet.clear();
    Widgets.removeAllListEntries(widgetCompressExcludeList);

    for (ValueMap resultMap : resultMapList)
    {
      // get data
      PatternTypes patternType = resultMap.getEnum  ("patternType");
      String       pattern     = resultMap.getString("pattern"    );

      if (!pattern.equals(""))
      {
         compressExcludeHashSet.add(pattern);
         Widgets.insertListEntry(widgetCompressExcludeList,
                                 findListIndex(widgetCompressExcludeList,pattern),
                                 pattern,
                                 pattern
                                );
      }
    }
  }

  //-----------------------------------------------------------------------

  /** edit include entry
   * @param entryData entry data
   * @param title dialog title
   * @param buttonText add button text
   */
  private boolean includeEdit(final EntryData entryData, String title, String buttonText)
  {
    Composite composite,subComposite;
    Label     label;
    Button    button;

    assert selectedJobId != 0;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetPattern;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,"Pattern:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetPattern = Widgets.newText(subComposite);
        if (entryData.pattern != null) widgetPattern.setText(entryData.pattern);
        Widgets.layout(widgetPattern,0,0,TableLayoutData.WE);
        widgetPattern.setToolTipText("Include pattern. Use * and ? as wildcards.");

        button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
        Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String pathName = Dialogs.directory(shell,
                                                "Select path",
                                                widgetPattern.getText()
                                               );
            if (pathName != null)
            {
              widgetPattern.setText(pathName.trim());
            }
          }
        });
      }

      label = Widgets.newLabel(composite,"Type:");
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,0.0));
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        button = Widgets.newRadio(subComposite,"file");
        button.setSelection(entryData.entryType == EntryTypes.FILE);
        Widgets.layout(button,0,0,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            entryData.entryType = EntryTypes.FILE;
          }
        });
        button = Widgets.newRadio(subComposite,"image");
        button.setSelection(entryData.entryType == EntryTypes.IMAGE);
        Widgets.layout(button,0,1,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            entryData.entryType = EntryTypes.IMAGE;
          }
        });
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetAdd = Widgets.newButton(composite,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);
      widgetAdd.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          entryData.pattern = widgetPattern.getText().trim();
          Dialogs.close(dialog,true);
        }
      });

      button = Widgets.newButton(composite,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
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

    return (Boolean)Dialogs.run(dialog,false) && !entryData.pattern.equals("");
  }

  /** add include entry
   * @param entryData entry data
   */
  private void includeListAdd(EntryData entryData)
  {
    final String[] PATTERN_MAP_FROM = new String[]{"\n","\r","\\"};
    final String[] PATTERN_MAP_TO   = new String[]{"\\n","\\r","\\\\"};

    assert selectedJobId != 0;

    // update include list
    String[] resultErrorMessage = new String[1];
    int error = BARServer.executeCommand(StringParser.format("INCLUDE_LIST_ADD jobId=%d entryType=%s patternType=%s pattern=%'S",
                                                             selectedJobId,
                                                             entryData.entryType.toString(),
                                                             "GLOB",
                                                             entryData.pattern
                                                            ),
                                         resultErrorMessage
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,"Cannot add include entry:\n\n"+resultErrorMessage[0]);
      return;
    }

    // update hash map
    includeHashMap.put(entryData.pattern,entryData);

    // update table widget
    Widgets.insertTableEntry(widgetIncludeTable,
                             findTableIndex(widgetIncludeTable,entryData.pattern),
                             (Object)entryData,
                             entryData.getImage(),
                             entryData.pattern
                            );

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove include entry
   * @param patterns patterns to remove from include/exclude list
   */
  private void includeListRemove(String[] patterns)
  {
    final String[] PATTERN_MAP_FROM = new String[]{"\n","\r","\\"};
    final String[] PATTERN_MAP_TO   = new String[]{"\\n","\\r","\\\\"};

    assert selectedJobId != 0;

    // remove patterns from hash map
    for (String pattern : patterns)
    {
      includeHashMap.remove(pattern);
    }

    // update include list
    String[] resultErrorMessage = new String[1];
//TODO return value?
    BARServer.executeCommand(StringParser.format("INCLUDE_LIST_CLEAR jobId=%d",selectedJobId),resultErrorMessage);
    for (EntryData entryData : includeHashMap.values())
    {
      BARServer.executeCommand(StringParser.format("INCLUDE_LIST_ADD jobId=%d entryType=%s patternType=%s pattern=%'S",
                                                   selectedJobId,
                                                   entryData.entryType.toString(),
                                                   "GLOB",
                                                   entryData.pattern
                                                  ),
                               resultErrorMessage
                              );
    }

    // update table widget
    Widgets.removeAllTableEntries(widgetIncludeTable);
    for (EntryData entryData : includeHashMap.values())
    {
      Widgets.insertTableEntry(widgetIncludeTable,
                               findTableIndex(widgetIncludeTable,entryData.pattern),
                               (Object)entryData,
                               entryData.getImage(),
                               entryData.pattern
                              );
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove include entry
   * @param pattern pattern to remove from include/exclude list
   */
  private void includeListRemove(String pattern)
  {
    includeListRemove(new String[]{pattern});
  }

  /** add new include entry
   */
  private void includeListAdd()
  {
    assert selectedJobId != 0;

    EntryData entryData = new EntryData(EntryTypes.FILE,"");
    if (includeEdit(entryData,"Add new include pattern","Add"))
    {
      includeListAdd(entryData);
    }
  }

  /** edit include entry
   */
  private void includeListEdit()
  {
    assert selectedJobId != 0;

    TableItem[] tableItems = widgetIncludeTable.getSelection();
    if (tableItems.length > 0)
    {
      EntryData oldEntryData = (EntryData)tableItems[0].getData();
      EntryData newEntryData = oldEntryData.clone();

      if (includeEdit(newEntryData,"Edit include pattern","Save"))
      {
        // update include list
        includeListRemove(new String[]{oldEntryData.pattern,newEntryData.pattern});
        includeListAdd(newEntryData);

        // update file tree/device images
        updateFileTreeImages();
        updateDeviceImages();
      }
    }
  }

  /** clone include entry
   */
  private void includeListClone()
  {
    assert selectedJobId != 0;

    TableItem[] tableItems = widgetIncludeTable.getSelection();
    if (tableItems.length > 0)
    {
      EntryData entryData = ((EntryData)tableItems[0].getData()).clone();

      if (includeEdit(entryData,"Clone include pattern","Add"))
      {
        // update include list
        includeListRemove(entryData.pattern);
        includeListAdd(entryData);

        // update file tree/device images
        updateFileTreeImages();
        updateDeviceImages();
      }
    }
  }

  /** remove selected include pattern
   */
  private void includeListRemove()
  {
    assert selectedJobId != 0;

    ArrayList<EntryData> entryDataList = new ArrayList<EntryData>();
    for (TableItem tableItem : widgetIncludeTable.getSelection())
    {
      entryDataList.add((EntryData)tableItem.getData());
    }
    if (entryDataList.size() > 0)
    {
      if ((entryDataList.size() == 1) || Dialogs.confirm(shell,"Remove "+entryDataList.size()+" include patterns?"))
      {
        for (EntryData entryData : entryDataList)
        {
          includeListRemove(entryData.pattern);
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** edit exclude pattern
   * @param pattern pattern
   * @param title dialog title
   * @param buttonText add button text
   */
  private boolean excludeEdit(final String[] pattern, String title, String buttonText)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert selectedJobId != 0;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetPattern;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,"Pattern:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPattern = Widgets.newText(composite);
      if (pattern[0] != null) widgetPattern.setText(pattern[0]);
      Widgets.layout(widgetPattern,0,1,TableLayoutData.WE);
      widgetPattern.setToolTipText("Exclude pattern. Use * and ? as wildcards.");

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String pathName = Dialogs.directory(shell,
                                              "Select path",
                                              widgetPattern.getText()
                                             );
          if (pathName != null)
          {
            widgetPattern.setText(pathName.trim());
          }
        }
      });
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetAdd = Widgets.newButton(composite,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);
      widgetAdd.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          pattern[0] = widgetPattern.getText().trim();
          Dialogs.close(dialog,true);
        }
      });

      button = Widgets.newButton(composite,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
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

    return (Boolean)Dialogs.run(dialog,false) && !pattern[0].equals("");
  }

  /** add exclude pattern
   * @param pattern pattern to add to included/exclude list
   */
  private void excludeListAdd(String pattern)
  {
    final String[] PATTERN_MAP_FROM = new String[]{"\n","\r","\\"};
    final String[] PATTERN_MAP_TO   = new String[]{"\\n","\\r","\\\\"};

    assert selectedJobId != 0;

    // update exclude list
    String[] resultErrorMessage = new String[1];
    int error = BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_ADD jobId=%d patternType=%s pattern=%'S",
                                                             selectedJobId,
                                                             "GLOB",
                                                             pattern
                                                            ),
                                         resultErrorMessage
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,"Cannot add exclude entry:\n\n"+resultErrorMessage[0]);
      return;
    }

    // update hash map
    excludeHashSet.add(pattern);

// TODO
//.    widgetExcludeList.add(pattern,findListIndex(widgetExcludeList,pattern));
    Widgets.insertListEntry(widgetExcludeList,
                            findListIndex(widgetExcludeList,pattern),
                            pattern,
                            pattern
                           );

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove exclude pattern
   * @param patterns pattern to remove from exclude list
   */
  private void excludeListRemove(String[] patterns)
  {
    final String[] PATTERN_MAP_FROM = new String[]{"\n","\r","\\"};
    final String[] PATTERN_MAP_TO   = new String[]{"\\n","\\r","\\\\"};

    assert selectedJobId != 0;

    // remove patterns from hash set
    for (String pattern : patterns)
    {
      excludeHashSet.remove(pattern);
    }

    // update exclude list
    String[] resultErrorMessage = new String[1];
//TODO return value?
    BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_CLEAR jobId=%d",selectedJobId),resultErrorMessage);
    for (String pattern : excludeHashSet)
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_ADD jobId=%d patternType=%s pattern=%'S",
                                                   selectedJobId,
                                                   "GLOB",
                                                   pattern
                                                  ),
                                resultErrorMessage
                               );
    }

    // update list widget
    Widgets.removeAllListEntries(widgetExcludeList);
    for (String pattern : excludeHashSet)
    {
      Widgets.insertListEntry(widgetExcludeList,
                              findListIndex(widgetExcludeList,pattern),
                              pattern,
                              pattern
                             );
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove exclude pattern
   * @param pattern pattern to remove from exclude list
   */
  private void excludeListRemove(String pattern)
  {
    excludeListRemove(new String[]{pattern});
  }

  /** add new exclude pattern
   */
  private void excludeListAdd()
  {
    assert selectedJobId != 0;

    String[] pattern = new String[1];
    if (excludeEdit(pattern,"Add new exclude pattern","Add"))
    {
      excludeListAdd(pattern[0]);
    }
  }

  /** edit exclude pattern
   */
  private void excludeListEdit()
  {
    assert selectedJobId != 0;

    String[] patterns = widgetExcludeList.getSelection();
    if (patterns.length > 0)
    {
      String   oldPattern = patterns[0];
      String[] newPattern = new String[]{new String(oldPattern)};
      if (excludeEdit(newPattern,"Edit exclude pattern","Save"))
      {
        // update exclude list
        excludeListRemove(new String[]{oldPattern,newPattern[0]});
        excludeListAdd(newPattern[0]);

        // update file tree/device images
        updateFileTreeImages();
        updateDeviceImages();
      }
    }
  }

  /** clone exclude pattern
   */
  private void excludeListClone()
  {
    assert selectedJobId != 0;

    String[] patterns = widgetExcludeList.getSelection();
    if (patterns.length > 0)
    {
      String[] pattern = new String[]{new String(patterns[0])};
      if (excludeEdit(pattern,"Clone exclude pattern","Add"))
      {
        // update exclude list
        excludeListRemove(new String[]{pattern[0]});
        excludeListAdd(pattern[0]);

        // update file tree/device images
        updateFileTreeImages();
        updateDeviceImages();
      }
    }
  }

  /** remove selected exclude pattern
   */
  private void excludeListRemove()
  {
    assert selectedJobId != 0;

    String[] patterns = widgetExcludeList.getSelection();
    if (patterns.length > 0)
    {
      if ((patterns.length == 1) || Dialogs.confirm(shell,"Remove "+patterns.length+" exclude patterns?"))
      {
        excludeListRemove(patterns);
      }
    }
  }

  //-----------------------------------------------------------------------

  /** edit compress exclude pattern
   * @param pattern pattern
   * @param title dialog title
   * @param buttonText add button text
   */
  private boolean compressExcludeEdit(final String[] pattern, String title, String buttonText)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert selectedJobId != 0;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetPattern;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,"Pattern:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPattern = Widgets.newText(composite);
      if (pattern[0] != null) widgetPattern.setText(pattern[0]);
      Widgets.layout(widgetPattern,0,1,TableLayoutData.WE);
      widgetPattern.setToolTipText("Compress exclude pattern. Use * and ? as wildcards.");

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String pathName = Dialogs.directory(shell,
                                              "Select path",
                                              widgetPattern.getText()
                                             );
          if (pathName != null)
          {
            widgetPattern.setText(pathName);
          }
        }
      });
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetAdd = Widgets.newButton(composite,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
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
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        pattern[0] = widgetPattern.getText();
        Dialogs.close(dialog,true);
      }
    });

    return (Boolean)Dialogs.run(dialog,false) && !pattern[0].equals("");
  }

  /** add compress exclude pattern
   * @param pattern pattern to add to compress exclude list
   */
  private void compressExcludeListAdd(String pattern)
  {
    assert selectedJobId != 0;

    if (!compressExcludeHashSet.contains(pattern))
    {
      String[] resultErrorMesage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobId=%d patternType=%s pattern=%'S",
                                                               selectedJobId,
                                                               "GLOB",
                                                               pattern
                                                              ),
                                           resultErrorMesage
                                          );
      if (error != Errors.NONE)
      {
        Dialogs.error(shell,"Cannot add compress exclude entry:\n\n"+resultErrorMesage[0]);
        return;
      }

      compressExcludeHashSet.add(pattern);
Dprintf.dprintf("%d",findListIndex(widgetCompressExcludeList,pattern));
      Widgets.insertListEntry(widgetCompressExcludeList,
                              findListIndex(widgetCompressExcludeList,pattern),
                              pattern,
                              pattern
                             );
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** add compress exclude patterns
   * @param patterns patterns to add to compress exclude list
   */
  private void compressExcludeListAdd(String[] patterns)
  {
    assert selectedJobId != 0;

    for (String pattern : patterns)
    {
      if (!compressExcludeHashSet.contains(pattern))
      {
        String[] resultErrorMessage = new String[1];
        int error = BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobId=%d patternType=%s pattern=%'S",
                                                                 selectedJobId,
                                                                 "GLOB",
                                                                 pattern
                                                                ),
                                             resultErrorMessage
                                            );
        if (error != Errors.NONE)
        {
          Dialogs.error(shell,"Cannot add compress exclude entry:\n\n"+resultErrorMessage[0]);
          return;
        }

        compressExcludeHashSet.add(pattern);
        Widgets.insertListEntry(widgetCompressExcludeList,
                                findListIndex(widgetCompressExcludeList,pattern),
                                pattern,
                                pattern
                               );
      }
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** add new compress exclude pattern
   */
  private void compressExcludeListAdd()
  {
    assert selectedJobId != 0;

    String[] pattern = new String[1];
    if (compressExcludeEdit(pattern,"Add new compress exclude pattern","Add"))
    {
      compressExcludeListAdd(pattern[0]);
    }
  }

  /** edit compress exclude entry
   */
  private void compressExcludeListEdit()
  {
    assert selectedJobId != 0;

    String[] patterns = widgetCompressExcludeList.getSelection();
    if (patterns.length > 0)
    {
      String   oldPattern = patterns[0];
      String[] newPattern = new String[]{new String(oldPattern)};

      if (compressExcludeEdit(newPattern,"Edit compress exclude pattern","Save"))
      {
        // update include list
        compressExcludeListRemove(new String[]{oldPattern,newPattern[0]});
        compressExcludeListAdd(newPattern[0]);

        // update file tree/device images
        updateFileTreeImages();
        updateDeviceImages();
      }
    }
  }

  /** remove compress exclude patterns
   * @param pattern pattern to remove from include/exclude list
   */
  private void compressExcludeListRemove(String[] patterns)
  {
    final String[] PATTERN_MAP_FROM = new String[]{"\n","\r","\\"};
    final String[] PATTERN_MAP_TO   = new String[]{"\\n","\\r","\\\\"};

    assert selectedJobId != 0;

    for (String pattern : patterns)
    {
      compressExcludeHashSet.remove(pattern);
    }

    String[] resultErrorMessage = new String[1];
//TODO return value?
    BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_CLEAR jobId=%d",selectedJobId),resultErrorMessage);
    Widgets.removeAllListEntries(widgetCompressExcludeList);
    for (String pattern : compressExcludeHashSet)
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobId=%d patternType=%s pattern=%'S",
                                                   selectedJobId,
                                                   "GLOB",
                                                   pattern
                                                  ),
                               resultErrorMessage
                              );
      Widgets.insertListEntry(widgetCompressExcludeList,
                              findListIndex(widgetCompressExcludeList,pattern),
                              pattern,
                              pattern
                             );
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove compress exclude pattern
   * @param pattern pattern to remove from compress exclude list
   */
  private void compressExcludeListRemove(String pattern)
  {
    compressExcludeListRemove(new String[]{pattern});
  }

  /** remove selected compress exclude pattern
   */
  private void compressExcludeListRemove()
  {
    assert selectedJobId != 0;

    int index = widgetCompressExcludeList.getSelectionIndex();
    if (index >= 0)
    {
      String pattern = widgetCompressExcludeList.getItem(index);

      if (Dialogs.confirm(shell,"Remove compress exclude pattern '"+pattern+"'?"))
      {
        compressExcludeListRemove(pattern);
      }
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

    /** get storage name part transfer instance
     * @return storage transfer instance
     */
    public static StorageNamePartTransfer getInstance()
    {
      return instance;
    }

    /** convert Java object to native data
     * @param object object to convert
     * @param transferData transfer data
     */
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

  /** get native data from transfer and convert to object
   * @param transferData transfer data
   * @return object
   */
   public Object nativeToJava(TransferData transferData)
   {
     if (isSupportedType(transferData))
     {
       byte[] buffer = (byte[])super.nativeToJava(transferData);
       if (buffer == null) return null;

       StorageNamePart storageNamePart = null;
       try
       {
         ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream(buffer);
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

    /** get type names
     * @return names
     */
    protected String[] getTypeNames()
    {
      return new String[]{NAME};
    }

    /** get ids
     * @return ids
     */
    protected int[] getTypeIds()
    {
      return new int[]{ID};
    }

    /** validate data
     * @return true iff data OK, false otherwise
     */
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
      Control    control;
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
      composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0}));
      Widgets.layout(composite,0,0,TableLayoutData.WE);
      {
        label = Widgets.newLabel(composite,"File name:");
        Widgets.layout(label,0,0,TableLayoutData.W);

        widgetFileName = Widgets.newCanvas(composite,SWT.BORDER);
        widgetFileName.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_WHITE));
        Widgets.layout(widgetFileName,0,1,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,Widgets.getTextHeight(widgetFileName)+5);
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
            if ((highlightedNamePart != null) && (highlightedNamePart.string != null) && ((keyEvent.keyCode == SWT.DEL) || (keyEvent.keyCode == SWT.BS)))
            {
              remPart(highlightedNamePart);
            }
          }
          public void keyReleased(KeyEvent keyEvent)
          {
          }
        });
        widgetFileName.setToolTipText("Drag to trashcan icon to the right to remove name part.");
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
                if      (dropTargetEvent.data instanceof StorageNamePart)
                {
                  addPart(index,((StorageNamePart)dropTargetEvent.data).string);
                }
                else if (dropTargetEvent.data instanceof String)
                {
                  addPart(index,(String)dropTargetEvent.data);
                }
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

        control = Widgets.newImage(composite,IMAGE_TRASHCAN,SWT.BORDER);
        Widgets.layout(control,0,2,TableLayoutData.NONE);
        control.setToolTipText("Use drag&drop to remove name parts.");
        dropTarget = new DropTarget(control,DND.DROP_MOVE);
        dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance(),StorageNamePartTransfer.getInstance()});
        dropTarget.addDropListener(new DropTargetAdapter()
        {
          public void dragLeave(DropTargetEvent dropTargetEvent)
          {
          }
          public void dragOver(DropTargetEvent dropTargetEvent)
          {
          }
          public void drop(DropTargetEvent dropTargetEvent)
          {
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
      composite.setLayout(new TableLayout(0.0,new double[]{0.0,0.4,0.0,0.4,0.0,0.2}));
      Widgets.layout(composite,1,0,TableLayoutData.NSWE);
      composite.setToolTipText("Use drag&drop to add name parts.");
      {
        // column 1
        addDragAndDrop(composite,"-","text '-'",                                       0, 0);
        addDragAndDrop(composite,".bar","text '.bar'",                                 1, 0);
        widgetText = Widgets.newText(composite);
        addDragAndDrop(composite,"Text",widgetText,                                    2, 0);

        addDragAndDrop(composite,"#","part number 1 digit",                            4, 0);
        addDragAndDrop(composite,"##","part number 2 digits",                          5, 0);
        addDragAndDrop(composite,"###","part number 3 digits",                         6, 0);
        addDragAndDrop(composite,"####","part number 4 digits",                        7, 0);

        addDragAndDrop(composite,"%type","archive type: full,incremental,differential",9, 0);
        addDragAndDrop(composite,"%last","'-last' if last archive part",               10,0);
        addDragAndDrop(composite,"%uuid","universally unique identifier",              11,0);
        addDragAndDrop(composite,"%text","schedule custom text",                       12,0);

        // column 2
        addDragAndDrop(composite,"%d","day 01..31",                                    0, 1);
        addDragAndDrop(composite,"%j","day of year 001..366",                          1, 1);
        addDragAndDrop(composite,"%m","month 01..12",                                  2, 1);
        addDragAndDrop(composite,"%b","month name",                                    3, 1);
        addDragAndDrop(composite,"%B","full month name",                               4, 1);
        addDragAndDrop(composite,"%H","hour 00..23",                                   5, 1);
        addDragAndDrop(composite,"%I","hour 00..12",                                   6, 1);
        addDragAndDrop(composite,"%M","minute 00..59",                                 7, 1);
        addDragAndDrop(composite,"%p","'AM' or 'PM'",                                  8, 1);
        addDragAndDrop(composite,"%P","'am' or 'pm'",                                  9, 1);
        addDragAndDrop(composite,"%a","week day name",                                 10,1);
        addDragAndDrop(composite,"%A","full week day name",                            11,1);
        addDragAndDrop(composite,"%u","day of week 1..7",                              12,1);
        addDragAndDrop(composite,"%w","day of week 0..6",                              13,1);
        addDragAndDrop(composite,"%U","week number 1..52",                             14,1);
        addDragAndDrop(composite,"%C","century two digits",                            15,1);
        addDragAndDrop(composite,"%y","year two digits",                               16,1);
        addDragAndDrop(composite,"%Y","year four digits",                              17,1);
        addDragAndDrop(composite,"%S","seconds since 1.1.1970 00:00",                  18,1);
        addDragAndDrop(composite,"%Z","time-zone abbreviation",                        19,1);

        // column 3
        addDragAndDrop(composite,"%%","%",                                             0, 2);
        addDragAndDrop(composite,"%#","#",                                             1, 2);
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
          StringBuilder buffer;

          // get next text part
          buffer = new StringBuilder();
          while (   (z < fileName.length())
                 && (fileName.charAt(z) != '%')
                 && (fileName.charAt(z) != '#')
                )
          {
            buffer.append(fileName.charAt(z)); z++;
          }
          storageNamePartList.add(new StorageNamePart(buffer.toString()));
          storageNamePartList.add(new StorageNamePart(null));

          if (z < fileName.length())
          {
            switch (fileName.charAt(z))
            {
              case '%':
                // add variable part
                buffer = new StringBuilder();
                buffer.append('%'); z++;
                if ((z < fileName.length()) && (fileName.charAt(z) == '%'))
                {
                  buffer.append('%'); z++;
                }
                else
                {
                  while ((z < fileName.length()) && (Character.isLetterOrDigit(fileName.charAt(z))))
                  {
                    buffer.append(fileName.charAt(z)); z++;
                  }
                }
                storageNamePartList.add(new StorageNamePart(buffer.toString()));
                storageNamePartList.add(new StorageNamePart(null));
                break;
              case '#':
                // add number part
                buffer = new StringBuilder();
                while ((z < fileName.length()) && (fileName.charAt(z) == '#'))
                {
                  buffer.append(fileName.charAt(z)); z++;
                }
                storageNamePartList.add(new StorageNamePart(buffer.toString()));
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
      StringBuilder buffer = new StringBuilder();
      for (StorageNamePart storageNamePart : storageNamePartList)
      {
        if (storageNamePart.string != null)
        {
          buffer.append(storageNamePart.string);
        }
      }

      return buffer.toString();
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

      label = Widgets.newLabel(composite,description,SWT.LEFT);
      Widgets.layout(label,row,column*2+1,TableLayoutData.WE);
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
      StringBuilder buffer = new StringBuilder();

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
                buffer.append("1234567890".charAt(z%10));
                z++;
              }
            }
            else if (storageNamePart.string.equals("%type"))
              buffer.append("full");
            else if (storageNamePart.string.equals("%last"))
              buffer.append("-last");
            else if (storageNamePart.string.equals("%uuid"))
              buffer.append("9f4aebd5-40a5-4056-8cf1-8be316638685");
            else if (storageNamePart.string.equals("%text"))
              buffer.append("foo");
            else if (storageNamePart.string.equals("%d"))
              buffer.append("24");
            else if (storageNamePart.string.equals("%j"))
              buffer.append("354");
            else if (storageNamePart.string.equals("%m"))
              buffer.append("12");
            else if (storageNamePart.string.equals("%b"))
              buffer.append("Dec");
            else if (storageNamePart.string.equals("%B"))
              buffer.append("December");
            else if (storageNamePart.string.equals("%H"))
              buffer.append("23");
            else if (storageNamePart.string.equals("%I"))
              buffer.append("11");
            else if (storageNamePart.string.equals("%M"))
              buffer.append("55");
            else if (storageNamePart.string.equals("%p"))
              buffer.append("PM");
            else if (storageNamePart.string.equals("%P"))
              buffer.append("pm");
            else if (storageNamePart.string.equals("%a"))
              buffer.append("Mon");
            else if (storageNamePart.string.equals("%A"))
              buffer.append("Monday");
            else if (storageNamePart.string.equals("%u"))
              buffer.append("1");
            else if (storageNamePart.string.equals("%w"))
              buffer.append("0");
            else if (storageNamePart.string.equals("%U"))
              buffer.append("51");
            else if (storageNamePart.string.equals("%C"))
              buffer.append("20");
            else if (storageNamePart.string.equals("%y"))
              buffer.append("07");
            else if (storageNamePart.string.equals("%Y"))
              buffer.append("2007");
            else if (storageNamePart.string.equals("%S"))
              buffer.append("1198598100");
            else if (storageNamePart.string.equals("%Z"))
              buffer.append("JST");
            else if (storageNamePart.string.equals("%%"))
              buffer.append("%");
            else if (storageNamePart.string.equals("%#"))
              buffer.append("#");
            else
              buffer.append(storageNamePart.string);
          }
        }
      }
      widgetExample.setText(buffer.toString());
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
  private void storageFileNameEdit()
  {
    Composite composite;
    Label     label;
    Button    button;
    Composite subComposite;

    assert selectedJobId != 0;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,"Edit storage file name",700,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final StorageFileNameEditor storageFileNameEditor;
    final Button                widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    storageFileNameEditor = new StorageFileNameEditor(composite,storageFileName.getString());

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,2,0,TableLayoutData.WE);
    {
      widgetSave = Widgets.newButton(composite,"Save");
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetSave.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        storageFileName.set(storageFileNameEditor.getFileName());
        Dialogs.close(dialog,true);
      }
    });

    Dialogs.run(dialog);
  }

  //-----------------------------------------------------------------------

  /** find index for insert of schedule data in sorted schedule list
   * @param scheduleData schedule data
   * @return index in schedule table
   */
  private int findScheduleListIndex(ScheduleData scheduleData)
  {
    TableItem              tableItems[]           = widgetScheduleList.getItems();
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
    String[]            resultErrorMessage  = new String[1];
    ArrayList<ValueMap> resultMapList       = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("SCHEDULE_LIST jobId=%d",selectedJobId),
                                         new TypeMap("date",       String.class,
                                                     "weekDays",   String.class,
                                                     "time",       String.class,
                                                     "archiveType",String.class,
                                                     "customText", String.class,
                                                     "enabledFlag",Boolean.class
                                                    ),
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    // update schedule list
    synchronized(scheduleList)
    {
      scheduleList.clear();
      widgetScheduleList.removeAll();
      for (ValueMap resultMap : resultMapList)
      {
        // get data
        String  date        = resultMap.getString ("date"       );
        String  weekDays    = resultMap.getString ("weekDays"   );
        String  time        = resultMap.getString ("time"       );
        String  archiveType = resultMap.getString ("archiveType");
        String  customText  = resultMap.getString ("customText" );
        boolean enabled     = resultMap.getBoolean("enabledFlag");

        ScheduleData scheduleData = new ScheduleData(date,weekDays,time,archiveType,customText,enabled);

        scheduleList.add(scheduleData);
        TableItem tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
        tableItem.setData(scheduleData);
        tableItem.setText(0,scheduleData.getDate());
        tableItem.setText(1,scheduleData.getWeekDays());
        tableItem.setText(2,scheduleData.getTime());
        tableItem.setText(3,scheduleData.archiveType);
        tableItem.setText(4,scheduleData.customText);
        tableItem.setText(5,scheduleData.enabled ? "yes" : "no");
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
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Combo    widgetYear,widgetMonth,widgetDay;
    final Button[] widgetWeekDays = new Button[7];
    final Combo    widgetHour,widgetMinute;
    final Button   widgetTypeDefault,widgetTypeNormal,widgetTypeFull,widgetTypeIncremental,widgetTypeDifferential;
    final Text     widgetCustomText;
    final Button   widgetEnabled;
    final Button   widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,"Date:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetYear = Widgets.newOptionMenu(subComposite);
        widgetYear.setItems(new String[]{"*","2008","2009","2010","2011","2012","2013","2014","2015"});
        widgetYear.setText(scheduleData.getYear()); if (widgetYear.getText().equals("")) widgetYear.setText("*");
        if (widgetYear.getText().equals("")) widgetYear.setText("*");
        Widgets.layout(widgetYear,0,0,TableLayoutData.W);
        widgetYear.setToolTipText("Year to execute job. Leave to '*' for each year.");

        widgetMonth = Widgets.newOptionMenu(subComposite);
        widgetMonth.setItems(new String[]{"*","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"});
        widgetMonth.setText(scheduleData.getMonth()); if (widgetMonth.getText().equals("")) widgetMonth.setText("*");
        Widgets.layout(widgetMonth,0,1,TableLayoutData.W);
        widgetMonth.setToolTipText("Month to execute job. Leave to '*' for each month.");

        widgetDay = Widgets.newOptionMenu(subComposite);
        widgetDay.setItems(new String[]{"*","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31"});
        widgetDay.setText(scheduleData.getDay()); if (widgetDay.getText().equals("")) widgetDay.setText("*");
        Widgets.layout(widgetDay,0,2,TableLayoutData.W);
        widgetDay.setToolTipText("Day to execute job. Leave to '*' for each day.");
      }

      label = Widgets.newLabel(composite,"Week days:");
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        widgetWeekDays[ScheduleData.MON] = Widgets.newCheckbox(subComposite,"Mon");
        Widgets.layout(widgetWeekDays[ScheduleData.MON],0,0,TableLayoutData.W);
        widgetWeekDays[ScheduleData.MON].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.MON));
        widgetWeekDays[ScheduleData.MON].setToolTipText("Week days to execute job.");

        widgetWeekDays[ScheduleData.TUE] = Widgets.newCheckbox(subComposite,"Tue");
        Widgets.layout(widgetWeekDays[ScheduleData.TUE],0,1,TableLayoutData.W);
        widgetWeekDays[ScheduleData.TUE].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.TUE));
        widgetWeekDays[ScheduleData.TUE].setToolTipText("Week days to execute job.");

        widgetWeekDays[ScheduleData.WED] = Widgets.newCheckbox(subComposite,"Wed");
        Widgets.layout(widgetWeekDays[ScheduleData.WED],0,2,TableLayoutData.W);
        widgetWeekDays[ScheduleData.WED].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.WED));
        widgetWeekDays[ScheduleData.WED].setToolTipText("Week days to execute job.");

        widgetWeekDays[ScheduleData.THU] = Widgets.newCheckbox(subComposite,"Thu");
        Widgets.layout(widgetWeekDays[ScheduleData.THU],0,3,TableLayoutData.W);
        widgetWeekDays[ScheduleData.THU].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.THU));
        widgetWeekDays[ScheduleData.THU].setToolTipText("Week days to execute job.");

        widgetWeekDays[ScheduleData.FRI] = Widgets.newCheckbox(subComposite,"Fri");
        Widgets.layout(widgetWeekDays[ScheduleData.FRI],0,4,TableLayoutData.W);
        widgetWeekDays[ScheduleData.FRI].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.FRI));
        widgetWeekDays[ScheduleData.FRI].setToolTipText("Week days to execute job.");

        widgetWeekDays[ScheduleData.SAT] = Widgets.newCheckbox(subComposite,"Sat");
        Widgets.layout(widgetWeekDays[ScheduleData.SAT],0,5,TableLayoutData.W);
        widgetWeekDays[ScheduleData.SAT].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.SAT));
        widgetWeekDays[ScheduleData.SAT].setToolTipText("Week days to execute job.");

        widgetWeekDays[ScheduleData.SUN] = Widgets.newCheckbox(subComposite,"Sun");
        Widgets.layout(widgetWeekDays[ScheduleData.SUN],0,6,TableLayoutData.W);
        widgetWeekDays[ScheduleData.SUN].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.SUN));
        widgetWeekDays[ScheduleData.SUN].setToolTipText("Week days to execute job.");
      }

      label = Widgets.newLabel(composite,"Time:");
      Widgets.layout(label,2,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,2,1,TableLayoutData.WE);
      {
        widgetHour = Widgets.newOptionMenu(subComposite);
        widgetHour.setItems(new String[]{"*","00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetHour.setText(scheduleData.getHour()); if (widgetHour.getText().equals("")) widgetHour.setText("*");
        Widgets.layout(widgetHour,0,0,TableLayoutData.W);
        widgetHour.setToolTipText("Hour to execute job. Leave to '*' for every hour.");

        widgetMinute = Widgets.newOptionMenu(subComposite);
        widgetMinute.setItems(new String[]{"*","00","05","10","15","20","30","35","40","45","50","55"});
        widgetMinute.setText(scheduleData.getMinute()); if (widgetMinute.getText().equals("")) widgetMinute.setText("*");
        Widgets.layout(widgetMinute,0,1,TableLayoutData.W);
        widgetMinute.setToolTipText("Minute to execute job. Leave to '*' for every minute.");
      }

      label = Widgets.newLabel(composite,"Type:");
      Widgets.layout(label,3,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,3,1,TableLayoutData.WE);
      {
        widgetTypeDefault = Widgets.newRadio(subComposite,"*");
        Widgets.layout(widgetTypeDefault,0,0,TableLayoutData.W);
        widgetTypeDefault.setSelection(scheduleData.archiveType.equals("*"));
        widgetTypeDefault.setToolTipText("Execute job with default type.");

        widgetTypeNormal = Widgets.newRadio(subComposite,"normal");
        Widgets.layout(widgetTypeNormal,0,1,TableLayoutData.W);
        widgetTypeNormal.setSelection(scheduleData.archiveType.equals("normal"));
        widgetTypeNormal.setToolTipText("Execute job as normal backup (no incremental data).");

        widgetTypeFull = Widgets.newRadio(subComposite,"full");
        Widgets.layout(widgetTypeFull,0,2,TableLayoutData.W);
        widgetTypeFull.setSelection(scheduleData.archiveType.equals("full"));
        widgetTypeFull.setToolTipText("Execute job as full backup.");

        widgetTypeIncremental = Widgets.newRadio(subComposite,"incremental");
        Widgets.layout(widgetTypeIncremental,0,3,TableLayoutData.W);
        widgetTypeIncremental.setSelection(scheduleData.archiveType.equals("incremental"));
        widgetTypeIncremental.setToolTipText("Execute job as incremental backup.");

        widgetTypeDifferential = Widgets.newRadio(subComposite,"differential");
        Widgets.layout(widgetTypeDifferential,0,4,TableLayoutData.W);
        widgetTypeDifferential.setSelection(scheduleData.archiveType.equals("differential"));
        widgetTypeDifferential.setToolTipText("Execute job as differential backup.");
      }

      label = Widgets.newLabel(composite,"Custom text:");
      Widgets.layout(label,4,0,TableLayoutData.W);

      widgetCustomText = Widgets.newText(composite);
      widgetCustomText.setText(scheduleData.customText);
      Widgets.layout(widgetCustomText,4,1,TableLayoutData.WE);
      widgetCustomText.setToolTipText("Custom text.");

      label = Widgets.newLabel(composite,"Options:");
      Widgets.layout(label,5,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,5,1,TableLayoutData.WE);
      {
        widgetEnabled = Widgets.newCheckbox(subComposite,"enabled");
        Widgets.layout(widgetEnabled,0,0,TableLayoutData.W);
        widgetEnabled.setSelection(scheduleData.enabled);
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      widgetAdd = Widgets.newButton(composite,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
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
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        scheduleData.setDate(widgetYear.getText(),widgetMonth.getText(),widgetDay.getText());
        scheduleData.setWeekDays(widgetWeekDays[ScheduleData.MON].getSelection(),
                                 widgetWeekDays[ScheduleData.TUE].getSelection(),
                                 widgetWeekDays[ScheduleData.WED].getSelection(),
                                 widgetWeekDays[ScheduleData.THU].getSelection(),
                                 widgetWeekDays[ScheduleData.FRI].getSelection(),
                                 widgetWeekDays[ScheduleData.SAT].getSelection(),
                                 widgetWeekDays[ScheduleData.SUN].getSelection()
                                );
        scheduleData.setTime(widgetHour.getText(),widgetMinute.getText());
        if      (widgetTypeNormal.getSelection())       scheduleData.archiveType = "normal";
        else if (widgetTypeFull.getSelection())         scheduleData.archiveType = "full";
        else if (widgetTypeIncremental.getSelection())  scheduleData.archiveType = "incremental";
        else if (widgetTypeDifferential.getSelection()) scheduleData.archiveType = "differential";
        else                                            scheduleData.archiveType = "*";
        scheduleData.customText = widgetCustomText.getText();
        scheduleData.enabled    = widgetEnabled.getSelection();

        Dialogs.close(dialog,true);
      }
    });

    return (Boolean)Dialogs.run(dialog,false);
  }

  /** create new schedule entry
   */
  private void scheduleNew()
  {
    assert selectedJobId != 0;

    ScheduleData scheduleData = new ScheduleData();
    if (scheduleEdit(scheduleData,"New schedule","Add"))
    {
    String[] resultErrorMessage = new String[1];
//TODO return value?
      BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_ADD jobId=%d date=%s weekDays=%s time=%s archiveType=%s customText=%S enabledFlag=%y",
                                                   selectedJobId,
                                                   scheduleData.getDate(),
                                                   scheduleData.getWeekDays(),
                                                   scheduleData.getTime(),
                                                   scheduleData.archiveType,
                                                   scheduleData.customText,
                                                   scheduleData.enabled
                                                  ),
                               resultErrorMessage
                              );

      scheduleList.add(scheduleData);
      TableItem tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
      tableItem.setData(scheduleData);
      tableItem.setText(0,scheduleData.getDate());
      tableItem.setText(1,scheduleData.getWeekDays());
      tableItem.setText(2,scheduleData.getTime());
      tableItem.setText(3,scheduleData.archiveType);
      tableItem.setText(4,scheduleData.customText);
      tableItem.setText(5,scheduleData.enabled ? "yes" : "no");
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
      TableItem tableItem       = widgetScheduleList.getItem(index);
      ScheduleData scheduleData = (ScheduleData)tableItem.getData();

      if (scheduleEdit(scheduleData,"Edit schedule","Save"))
      {
        tableItem.dispose();
        tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
        tableItem.setData(scheduleData);
        tableItem.setText(0,scheduleData.getDate());
        tableItem.setText(1,scheduleData.getWeekDays());
        tableItem.setText(2,scheduleData.getTime());
        tableItem.setText(3,scheduleData.archiveType);
        tableItem.setText(4,scheduleData.customText);
        tableItem.setText(5,scheduleData.enabled ? "yes" : "no");

    String[] resultErrorMessage = new String[1];
//TODO return value?
        BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_CLEAR jobId=%d",selectedJobId),resultErrorMessage);
        for (ScheduleData scheduleData_ : scheduleList)
        {
          BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_ADD jobId=%d date=%s weekDays=%s time=%s archiveType=%s customText=%S enabledFlag=%y",
                                                       selectedJobId,
                                                       scheduleData_.getDate(),
                                                       scheduleData_.getWeekDays(),
                                                       scheduleData_.getTime(),
                                                       scheduleData_.archiveType,
                                                       scheduleData_.customText,
                                                       scheduleData_.enabled
                                                      ),
                                   resultErrorMessage
                                  );
        }
      }
    }
  }

  /** clone a schedule entry
   */
  private void scheduleClone()
  {
    assert selectedJobId != 0;

    int index = widgetScheduleList.getSelectionIndex();
    if (index >= 0)
    {
      TableItem    tableItem    = widgetScheduleList.getItem(index);
      ScheduleData scheduleData = (ScheduleData)tableItem.getData();

      ScheduleData newScheduleData = scheduleData.clone();
      if (scheduleEdit(newScheduleData,"New schedule","Add"))
      {
        scheduleList.add(newScheduleData);

        tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(newScheduleData));
        tableItem.setData(newScheduleData);
        tableItem.setText(0,newScheduleData.getDate());
        tableItem.setText(1,newScheduleData.getWeekDays());
        tableItem.setText(2,newScheduleData.getTime());
        tableItem.setText(3,newScheduleData.archiveType);
        tableItem.setText(4,newScheduleData.customText);
        tableItem.setText(5,newScheduleData.enabled ? "yes" : "no");

// TODO result
        String[] resultErrorMessage = new String[1];
        BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_ADD jobId=%d date=%s weekDays=%s time=%s archiveType=%s customText=%S enabledFlag=%y",
                                                     selectedJobId,
                                                     newScheduleData.getDate(),
                                                     newScheduleData.getWeekDays(),
                                                     newScheduleData.getTime(),
                                                     newScheduleData.archiveType,
                                                     newScheduleData.customText,
                                                     newScheduleData.enabled
                                                    ),
                                 resultErrorMessage
                                );
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
      if (Dialogs.confirm(shell,"Delete schedule?"))
      {
        TableItem    tableItem    = widgetScheduleList.getItem(index);
        ScheduleData scheduleData = (ScheduleData)tableItem.getData();

        scheduleList.remove(scheduleData);

        tableItem.dispose();

// TODO result?
        String[] resultErrorMessage = new String[1];
        BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_CLEAR jobId=%d",selectedJobId),resultErrorMessage);
        for (ScheduleData scheduleData_ : scheduleList)
        {
          BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_ADD jobId=%d date=%s weekDays=%s time=%s archiveType=%s customText=%S enabledFlag=%y",
                                                       selectedJobId,
                                                       scheduleData_.getDate(),
                                                       scheduleData_.getWeekDays(),
                                                       scheduleData_.getTime(),
                                                       scheduleData_.archiveType,
                                                       scheduleData_.customText,
                                                       scheduleData_.enabled
                                                      ),
                                   resultErrorMessage
                                  );
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** clear all data
   */
  private void clear()
  {
    clearJobData();
    clearFileTree();
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
