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
  private final Image  IMAGE_TOGGLE_MARK;
  private final Image  IMAGE_EDIT;

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
  private WidgetVariable  byteCompressAlgorithm   = new WidgetVariable(new String[]{"none",
                                                                                    "zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9",
                                                                                    "bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9",
                                                                                    "lzma1","lzma2","lzma3","lzma4","lzma5","lzma6","lzma7","lzma8","lzma9",
                                                                                    "lzo1","lzo2","lzo3","lzo4","lzo5",
                                                                                    "lz4-0","lz4-1","lz4-2","lz4-3","lz4-4","lz4-5","lz4-6","lz4-7","lz4-8","lz4-9"
                                                                                   }
                                                                      );
  private WidgetVariable  compressMinSize         = new WidgetVariable(0);
  private WidgetVariable  cryptAlgorithm          = new WidgetVariable(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256"});
  private WidgetVariable  cryptType               = new WidgetVariable(new String[]{"none","symmetric","asymmetric"});
  private WidgetVariable  cryptPublicKeyFileName  = new WidgetVariable("");
  private WidgetVariable  cryptPasswordMode       = new WidgetVariable(new String[]{"default","ask","config"});
  private WidgetVariable  cryptPassword           = new WidgetVariable("");
  private WidgetVariable  incrementalListFileName = new WidgetVariable("");
  private WidgetVariable  storageType             = new WidgetVariable(new String[]{"filesystem",
                                                                                    "ftp",
                                                                                    "scp",
                                                                                    "sftp",
                                                                                    "webdav",
                                                                                    "cd",
                                                                                    "dvd",
                                                                                    "bd",
                                                                                    "device"}
                                                                                   );
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
  private WidgetVariable  preCommand              = new WidgetVariable("");
  private WidgetVariable  postCommand             = new WidgetVariable("");

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
    IMAGE_TOGGLE_MARK        = Widgets.loadImage(display,"togglemark.png");
    IMAGE_EDIT               = Widgets.loadImage(display,"edit.png");

    // get cursors
    waitCursor = new Cursor(display,SWT.CURSOR_WAIT);

    // start tree item size thread
    directoryInfoThread = new DirectoryInfoThread(display);
    directoryInfoThread.start();

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Jobs")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // job selector
    composite = Widgets.newComposite(widgetTab);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0,0.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobList = Widgets.newOptionMenu(composite);
      widgetJobList.setToolTipText(BARControl.tr("Existing job entries."));
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

      button = Widgets.newButton(composite,BARControl.tr("New")+"\u2026");
      button.setToolTipText(BARControl.tr("Create new job entry."));
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

      button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
      button.setToolTipText(BARControl.tr("Clone an existing job entry."));
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
            jobClone();
          }
        }
      });

      button = Widgets.newButton(composite,BARControl.tr("Rename")+"\u2026");
      button.setToolTipText(BARControl.tr("Rename a job entry."));
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

      button = Widgets.newButton(composite,BARControl.tr("Delete")+"\u2026");
      button.setToolTipText(BARControl.tr("Delete a job entry."));
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
    }

    // create sub-tabs
    widgetTabFolder = Widgets.newTabFolder(widgetTab);
    Widgets.setEnabled(widgetTabFolder,false);
    Widgets.layout(widgetTabFolder,1,0,TableLayoutData.NSWE);
    {
      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Files"));
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // file tree
        widgetFileTree = Widgets.newTree(tab,SWT.MULTI);
//        widgetFileTree.setToolTipText(BARControl.tr("Tree representation of files, directories, links and special entries.\nDouble-click to open sub-directories, right-click to open context menu.\nNote size column: numbers in red color indicates size update is still in progress."));
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
        treeColumn.setToolTipText(BARControl.tr("Click to sort for name."));
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Type",    SWT.LEFT, 160,true);
        treeColumn.setToolTipText(BARControl.tr("Click to sort for type."));
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Size",    SWT.RIGHT,100,true);
        treeColumn.setToolTipText(BARControl.tr("Click to sort for size."));
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Modified",SWT.LEFT, 100,true);
        treeColumn.setToolTipText(BARControl.tr("Click to sort for modified time."));
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);

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

              label = Widgets.newLabel(widgetFileTreeToolTip,BARControl.tr("Tree representation of files, directories, links and special entries.\nDouble-click to open sub-directories, right-click to open context menu.\nNote size column: numbers in red color indicates size update is still in progress."));
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

          Widgets.addMenuSeparator(menu);

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

          Widgets.addMenuSeparator(menu);

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

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,1.0,1.0,0.0,0.0,0.0}));
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          button = Widgets.newButton(composite,BARControl.tr("Include"));
          button.setToolTipText(BARControl.tr("Include entry in archive."));
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

          button = Widgets.newButton(composite,BARControl.tr("Exclude"));
          button.setToolTipText(BARControl.tr("Exclude entry from archive."));
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

          button = Widgets.newButton(composite,BARControl.tr("None"));
          button.setToolTipText(BARControl.tr("Do not include/exclude entry in/from archive."));
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

          control = Widgets.newSpacer(composite);
          Widgets.layout(control,0,3,TableLayoutData.NONE,0,0,30,0);

          button = Widgets.newButton(composite,IMAGE_DIRECTORY_INCLUDED);
          button.setToolTipText(BARControl.tr("Open all included directories."));
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

          button = Widgets.newCheckbox(composite,BARControl.tr("directory size"));
          button.setToolTipText(BARControl.tr("Show directory sizes (sum of file sizes)."));
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
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Images"));
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // image tree
        widgetDeviceTree = Widgets.newTree(tab,SWT.MULTI);
        widgetDeviceTree.setToolTipText(BARControl.tr("List of existing devices for image storage.\nRight-click to open context menu."));
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

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,1.0,1.0,0.0,0.0,0.0}));
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          button = Widgets.newButton(composite,BARControl.tr("Include"));
          button.setToolTipText(BARControl.tr("Include selected device for image storage."));
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

          button = Widgets.newButton(composite,BARControl.tr("Exclude"));
          button.setToolTipText(BARControl.tr("Exclude selected device from image storage."));
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

          button = Widgets.newButton(composite,BARControl.tr("None"));
          button.setToolTipText(BARControl.tr("Remove selected device from image storage."));
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
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Filters"));
      tab.setLayout(new TableLayout(new double[]{0.5,0.0,0.5,0.0,0.0},new double[]{0.0,1.0}));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // included table
        label = Widgets.newLabel(tab,BARControl.tr("Included")+":");
        Widgets.layout(label,0,0,TableLayoutData.NS);
        widgetIncludeTable = Widgets.newTable(tab);
        widgetIncludeTable.setToolTipText(BARControl.tr("List with include patterns, right-click for context menu."));
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

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,1,TableLayoutData.W);
        {
          widgetIncludeTableInsert = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
          widgetIncludeTableInsert.setToolTipText(BARControl.tr("Add entry to included list."));
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

          widgetIncludeTableEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"u2026");
          widgetIncludeTableEdit.setToolTipText(BARControl.tr("Edit entry in included list."));
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

          widgetIncludeTableRemove = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
          widgetIncludeTableRemove.setToolTipText(BARControl.tr("Clone entry in included list."));
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

          button = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
          button.setToolTipText(BARControl.tr("Remove entry from included list."));
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
        }

        // excluded list
        label = Widgets.newLabel(tab,BARControl.tr("Excluded")+":");
        Widgets.layout(label,2,0,TableLayoutData.NS);
        widgetExcludeList = Widgets.newList(tab);
        widgetExcludeList.setToolTipText(BARControl.tr("List with exclude patterns, right-click for context menu."));
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
          widgetExcludeListInsert = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
          widgetExcludeListInsert.setToolTipText(BARControl.tr("Add entry to excluded list."));
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

          widgetExcludeListEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
          widgetExcludeListEdit.setToolTipText(BARControl.tr("Edit entry in excluded list."));
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

          button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
          button.setToolTipText(BARControl.tr("Clone entry in excluded list."));
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

          widgetExcludeListRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
          widgetExcludeListRemove.setToolTipText(BARControl.tr("Remove entry from excluded list."));
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
        }

        // options
        label = Widgets.newLabel(tab,BARControl.tr("Options")+":");
        Widgets.layout(label,4,0,TableLayoutData.N);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,4,1,TableLayoutData.WE);
        {
          button = Widgets.newCheckbox(composite,BARControl.tr("skip unreadable entries"));
          button.setToolTipText(BARControl.tr("If enabled skip not readable entries (write information to log file).\nIf disabled stop job with an error."));
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

          button = Widgets.newCheckbox(composite,BARControl.tr("raw images"));
          button.setToolTipText(BARControl.tr("If enabled store all data of a device into an image.\nIf disabled try to detect file system and only store used blocks to image."));
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
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Storage"));
      tab.setLayout(new TableLayout(new double[]{0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0},new double[]{0.0,1.0}));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // part size
        label = Widgets.newLabel(tab,BARControl.tr("Part size")+":");
        Widgets.layout(label,0,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,0,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,BARControl.tr("unlimited"));
          button.setToolTipText(BARControl.tr("Create storage files with an unlimited size. Do not split storage files."));
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
                Dialogs.warning(shell,BARControl.tr("When writing to a CD/DVD/BD without splitting enabled\nthe resulting archive file may not fit on medium."));
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

          button = Widgets.newRadio(composite,BARControl.tr("limit to"));
          button.setToolTipText(BARControl.tr("Limit size of storage files to specified value."));
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

          widgetArchivePartSize = Widgets.newCombo(composite);
          widgetArchivePartSize.setToolTipText(BARControl.tr("Size limit for one storage file part."));
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
                if (archivePartSize.getLong() == n) color = null;
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
                widget.setBackground(null);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,String.format(BARControl.tr("'%s' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB."),string));
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
                widget.setBackground(null);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,String.format(BARControl.tr("'%s' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB."),string));
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
                widget.setBackground(null);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,String.format(BARControl.tr("'%s' is not valid size!\n\nEnter a number in the format 'n' or 'n.m'. Optional units are KB, MB or GB."),string));
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

          label = Widgets.newLabel(composite,BARControl.tr("bytes"));
          Widgets.layout(label,0,3,TableLayoutData.W);
        }

        // compress
        label = Widgets.newLabel(tab,BARControl.tr("Compress")+":");
        Widgets.layout(label,1,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,0.0));
        Widgets.layout(composite,1,1,TableLayoutData.W);
        {
          label = Widgets.newLabel(composite,BARControl.tr("Delta")+":");
          Widgets.layout(label,0,0,TableLayoutData.NONE);

          combo = Widgets.newOptionMenu(composite);
          combo.setToolTipText(BARControl.tr("Delta compression method to use."));
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

          label = Widgets.newLabel(composite,BARControl.tr("Byte")+":");
          Widgets.layout(label,0,2,TableLayoutData.NONE);

          combo = Widgets.newOptionMenu(composite);
          combo.setToolTipText(BARControl.tr("Byte compression method to use."));
          combo.setItems(new String[]{"none",
                                      "zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9",
                                      "bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9",
                                      "lzma1","lzma2","lzma3","lzma4","lzma5","lzma6","lzma7","lzma8","lzma9",
                                      "lzo1","lzo2","lzo3","lzo4","lzo5",
                                      "lz4-0","lz4-1","lz4-2","lz4-3","lz4-4","lz4-5","lz4-6","lz4-7","lz4-8","lz4-9"
                                     }
                        );
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
        }

        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0,0.0}));
        Widgets.layout(composite,2,1,TableLayoutData.WE);
        {
          label = Widgets.newLabel(composite,BARControl.tr("Source")+":");
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
              if (deltaSource.getString().equals(s)) color = null;
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
              widget.setBackground(null);
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
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,deltaSource));

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

        label = Widgets.newLabel(tab,BARControl.tr("Compress exclude")+":");
        Widgets.layout(label,3,0,TableLayoutData.NW);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(composite,3,1,TableLayoutData.NSWE);
        {
          // compress exclude list
          widgetCompressExcludeList = Widgets.newList(composite);
          widgetCompressExcludeList.setToolTipText(BARControl.tr("List with compress exclude patterns. Entries which match to one of these patterns will not be compressed.\nRight-click for context menu."));
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
                  "*.lzo",
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

          // buttons
          subComposite = Widgets.newComposite(composite,SWT.NONE,4);
          Widgets.layout(subComposite,1,0,TableLayoutData.W);
          {
            widgetCompressExcludeListInsert = Widgets.newButton(subComposite,BARControl.tr("Add")+"\u2026");
            widgetCompressExcludeListInsert.setToolTipText(BARControl.tr("Add entry to compress exclude list."));
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

            widgetCompressExcludeListEdit = Widgets.newButton(subComposite,BARControl.tr("Edit")+"\u2026");
            widgetCompressExcludeListEdit.setToolTipText(BARControl.tr("Edit entry in compress exclude list."));
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
                  compressExcludeListEdit();
                }
              }
            });

            widgetCompressExcludeListRemove = Widgets.newButton(subComposite,BARControl.tr("Remove")+"\u2026");
            widgetCompressExcludeListRemove.setToolTipText(BARControl.tr("Remove entry from compress exclude list."));
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
          }
        }

        // crypt
        label = Widgets.newLabel(tab,BARControl.tr("Crypt")+":");
        Widgets.layout(label,4,0,TableLayoutData.NW);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,4,1,TableLayoutData.WE);
        {
          combo = Widgets.newOptionMenu(composite);
          combo.setToolTipText(BARControl.tr("Encryption method to use."));
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
        }

        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,0.0,0.0,0.0,1.0,0.0}));
        Widgets.layout(composite,5,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,BARControl.tr("symmetric"));
          button.setToolTipText(BARControl.tr("Use symmetric encryption with pass-phrase."));
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

          button = Widgets.newRadio(composite,BARControl.tr("asymmetric"));
          button.setToolTipText(BARControl.tr("Use asymmetric hyprid-encryption with pass-phrase and public/private key."));
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

          control = Widgets.newSpacer(composite);
          Widgets.layout(control,0,2,TableLayoutData.NONE,0,0,5,0);

          label = Widgets.newLabel(composite,BARControl.tr("Public key")+":");
          Widgets.layout(label,0,3,TableLayoutData.W);
          text = Widgets.newText(composite);
          text.setToolTipText(BARControl.tr("Public key file used for asymmetric encryption."));
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
              if (cryptPublicKeyFileName.getString().equals(s)) color = null;
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
              widget.setBackground(null);
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
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,cryptPublicKeyFileName));

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
        label = Widgets.newLabel(tab,BARControl.tr("Crypt password")+":");
        Widgets.layout(label,6,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,0.0,0.0,1.0,0.0,1.0}));
        Widgets.layout(composite,6,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,BARControl.tr("default"));
          button.setToolTipText(BARControl.tr("Use default password from configuration file for encryption."));
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

          button = Widgets.newRadio(composite,BARControl.tr("ask"));
          button.setToolTipText(BARControl.tr("Input password for encryption."));
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

          button = Widgets.newRadio(composite,BARControl.tr("this"));
          button.setToolTipText(BARControl.tr("Use specified password for encryption."));
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

          widgetCryptPassword1 = Widgets.newPassword(composite);
          widgetCryptPassword1.setToolTipText(BARControl.tr("Password used for encryption."));
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
              if (cryptPassword.getString().equals(s)) color = null;
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
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
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
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword1,cryptPassword));

          label = Widgets.newLabel(composite,BARControl.tr("Repeat")+":");
          Widgets.layout(label,0,4,TableLayoutData.W);

          widgetCryptPassword2 = Widgets.newPassword(composite);
          widgetCryptPassword1.setToolTipText(BARControl.tr("Password used for encryption."));
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
              if (cryptPassword.getString().equals(s)) color = null;
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
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
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
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
              }
              else
              {
                Dialogs.error(shell,"Crypt passwords are not equal!");
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword2,cryptPassword));
        }

        // archive type
        label = Widgets.newLabel(tab,BARControl.tr("Mode")+":");
        Widgets.layout(label,7,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,0.0,0.0,0.0,0.0,1.0,0.0}));
        Widgets.layout(composite,7,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,BARControl.tr("normal"));
          button.setToolTipText(BARControl.tr("Normal mode: do not create incremental data files."));
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

          button = Widgets.newRadio(composite,BARControl.tr("full"));
          button.setToolTipText(BARControl.tr("Full mode: store all entries and create incremental data files."));
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

          button = Widgets.newRadio(composite,BARControl.tr("incremental"));
          button.setToolTipText(BARControl.tr("Incremental mode: store only modified entries since last full or incremental storage."));
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

          button = Widgets.newRadio(composite,BARControl.tr("differential"));
          button.setToolTipText(BARControl.tr("Differential mode: store only modified entries since last full storage."));
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
        }

        // file name
        label = Widgets.newLabel(tab,BARControl.tr("File name")+":");
        Widgets.layout(label,8,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(composite,8,1,TableLayoutData.WE);
        {
          text = Widgets.newText(composite);
          text.setToolTipText(BARControl.tr("Name of storage files to create. Several macros are supported. Click on button to the right to open storage file name editor."));
          Widgets.layout(text,0,0,TableLayoutData.WE);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text  widget = (Text)modifyEvent.widget;
              Color color  = COLOR_MODIFIED;
              String s = widget.getText();
              if (storageFileName.getString().equals(s)) color = null;
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
              widget.setBackground(null);
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
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,storageFileName));

          button = Widgets.newButton(composite,IMAGE_EDIT);
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

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
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
                String fileName = Dialogs.fileSave(shell,
                                                   "Select storage file name",
                                                   storageFileName.getString(),
                                                   new String[]{"BAR files","*.bar",
                                                                "All files","*",
                                                               }
                                                  );
                if (fileName != null)
                {
                  storageFileName.set(fileName);
                  BARServer.setOption(selectedJobId,"archive-name",getArchiveName());
                }
              }
            }
          });
        }

        // incremental file name
        label = Widgets.newLabel(tab,BARControl.tr("Incremental file name")+":");
        Widgets.layout(label,9,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(composite,9,1,TableLayoutData.WE);
        {
          text = Widgets.newText(composite);
          text.setToolTipText(BARControl.tr("Name of incremental data file. If no file name is given a name is derived automatically from the storage file name."));
          Widgets.layout(text,0,0,TableLayoutData.WE);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              Color  color  = COLOR_MODIFIED;
              String string = widget.getText();
              if (incrementalListFileName.getString().equals(string)) color = null;
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
              widget.setBackground(null);
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
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,incrementalListFileName));

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
        label = Widgets.newLabel(tab,BARControl.tr("Destination")+":");
        Widgets.layout(label,10,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,10,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,BARControl.tr("file system"));
          button.setToolTipText(BARControl.tr("Store created storage files into file system destination."));
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

          button = Widgets.newRadio(composite,BARControl.tr("ftp"));
          button.setToolTipText(BARControl.tr("Store created storage files on FTP server."));
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

          button = Widgets.newRadio(composite,BARControl.tr("scp"));
          button.setToolTipText(BARControl.tr("Store created storage files on SSH server via SCP protocol."));
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

          button = Widgets.newRadio(composite,BARControl.tr("sftp"));
          button.setToolTipText(BARControl.tr("Store created storage files on SSH server via SFTP protocol."));
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

          button = Widgets.newRadio(composite,BARControl.tr("webdav"));
          button.setToolTipText(BARControl.tr("Store created storage files on Webdav server."));
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

          button = Widgets.newRadio(composite,BARControl.tr("CD"));
          button.setToolTipText(BARControl.tr("Store created storage files on CD."));
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

          button = Widgets.newRadio(composite,BARControl.tr("DVD"));
          button.setToolTipText(BARControl.tr("Store created storage files on DVD."));
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

          button = Widgets.newRadio(composite,BARControl.tr("BD"));
          button.setToolTipText(BARControl.tr("Store created storage files on BD."));
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

          button = Widgets.newRadio(composite,BARControl.tr("device"));
          button.setToolTipText(BARControl.tr("Store created storage files on device."));
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
          button = Widgets.newCheckbox(composite,BARControl.tr("overwrite archive files"));
          button.setToolTipText(BARControl.tr("If enabled overwrite existing archive files. If disabled do not overwrite existing files and stop with an error."));
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
            label = Widgets.newLabel(composite,BARControl.tr("User")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(composite);
            text.setToolTipText(BARControl.tr("FTP server user login name. Leave it empty to use the default name from the configuration file."));
            Widgets.layout(text,0,1,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageLoginName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageLoginName));

            label = Widgets.newLabel(composite,BARControl.tr("Host")+":");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(composite);
            text.setToolTipText(BARControl.tr("FTP server name."));
            Widgets.layout(text,0,3,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageHostName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageHostName));

            label = Widgets.newLabel(composite,BARControl.tr("Password")+":");
            Widgets.layout(label,0,4,TableLayoutData.W);

            text = Widgets.newPassword(composite);
            text.setToolTipText(BARControl.tr("FTP server login password. Leave it empty to use the default password from the configuration file."));
            Widgets.layout(text,0,5,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageLoginPassword.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageLoginPassword));
          }

/*
          label = Widgets.newLabel(composite,BARControl.tr("Max. band width")+":");
          Widgets.layout(label,1,0,TableLayoutData.W);
          composite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(composite,1,1,TableLayoutData.WE);
          {
            button = Widgets.newRadio(composite,BARControl.tr("unlimited"));
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

            button = Widgets.newRadio(composite,BARControl.tr("limit to"));
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
          label = Widgets.newLabel(composite,BARControl.tr("Server"));
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE|SWT.BORDER);
          subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0,1.0,0.0,1.0}));
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            label = Widgets.newLabel(subComposite,BARControl.tr("Login")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH login name. Leave it empty to use the default login name from the configuration file."));
            Widgets.layout(text,0,1,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageLoginName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageLoginName));

            label = Widgets.newLabel(subComposite,BARControl.tr("Host")+":");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH login host name."));
            Widgets.layout(text,0,3,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageHostName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageHostName));

            label = Widgets.newLabel(subComposite,BARControl.tr("Port")+":");
            Widgets.layout(label,0,4,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH login port number. Set to 0 to use default port number from configuration file."));
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
                  if (storageHostPort.getLong() == n) color = null;
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
                    widget.setBackground(null);
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
                    widget.setBackground(null);
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
          }

          label = Widgets.newLabel(composite,BARControl.tr("SSH public key")+":");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,1,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH public key file name. Leave it empty to use the default key file from the configuration file."));
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (sshPublicKeyFileName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPublicKeyFileName));

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

          label = Widgets.newLabel(composite,BARControl.tr("SSH private key")+":");
          Widgets.layout(label,2,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,2,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH private key file name. Leave it empty to use the default key file from the configuration file."));
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (sshPrivateKeyFileName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPrivateKeyFileName));

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
          label = Widgets.newLabel(composite,BARControl.tr("Max. band width")+":");
          Widgets.layout(label,3,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE);
          {
            button = Widgets.newRadio(subComposite,BARControl.tr("unlimited"));
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

            button = Widgets.newRadio(subComposite,BARControl.tr("limit to"));
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
          label = Widgets.newLabel(composite,BARControl.tr("Server"));
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE|SWT.BORDER);
          subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0,1.0,0.0,1.0}));
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            label = Widgets.newLabel(subComposite,BARControl.tr("Login")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH login name. Leave it empty to use the default login name from the configuration file."));
            Widgets.layout(text,0,1,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageLoginName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageLoginName));

            label = Widgets.newLabel(subComposite,BARControl.tr("Host")+":");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH login host name."));
            Widgets.layout(text,0,3,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageHostName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageHostName));

            label = Widgets.newLabel(subComposite,BARControl.tr("Port")+":");
            Widgets.layout(label,0,4,TableLayoutData.W);

            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH login port number. Set to 0 to use default port number from configuration file."));
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
                  if (storageHostPort.getLong() == n) color = null;
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
                    widget.setBackground(null);
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
                    widget.setBackground(null);
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
          }

          label = Widgets.newLabel(composite,BARControl.tr("SSH public key")+":");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,1,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH public key file name. Leave it empty to use the default key file from the configuration file."));
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (sshPublicKeyFileName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPublicKeyFileName));

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

          label = Widgets.newLabel(composite,BARControl.tr("SSH private key")+":");
          Widgets.layout(label,2,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,2,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("SSH private key file name. Leave it empty to use the default key file from the configuration file."));
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (sshPrivateKeyFileName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPrivateKeyFileName));

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
          label = Widgets.newLabel(composite,BARControl.tr("Max. band width")+":");
          Widgets.layout(label,3,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE);
          {
            button = Widgets.newRadio(subComposite,BARControl.tr("unlimited"));
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

            button = Widgets.newRadio(subComposite,BARControl.tr("limit to"));
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
          label = Widgets.newLabel(composite,BARControl.tr("Device")+":");
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("Device name. Leave it empty to use system default device name."));
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageDeviceName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageDeviceName));

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

          label = Widgets.newLabel(composite,BARControl.tr("Size")+":");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,1,1,TableLayoutData.WE);
          {
            combo = Widgets.newCombo(subComposite);
            combo.setToolTipText(BARControl.tr("Size of medium. You may specify a smaller value than the real physical size to leave some free space for error-correction codes."));
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
                  if (volumeSize.getLong() == n) color = null;
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
                  widget.setBackground(null);

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
                  widget.setBackground(null);

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
                  widget.setBackground(null);

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

            label = Widgets.newLabel(subComposite,BARControl.tr("bytes"));
            Widgets.layout(label,0,1,TableLayoutData.W);
          }

          label = Widgets.newLabel(composite,BARControl.tr("Options")+":");
          Widgets.layout(label,3,0,TableLayoutData.NW);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE);
          {
            button = Widgets.newCheckbox(subComposite,BARControl.tr("add error-correction codes"));
            button.setToolTipText(BARControl.tr("Add error-correction codes to CD/DVD/BD image (require dvdisaster tool)."));
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

            button = Widgets.newCheckbox(subComposite,BARControl.tr("wait for first volume"));
            button.setToolTipText(BARControl.tr("Wait until first volume is loaded."));
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
          label = Widgets.newLabel(composite,BARControl.tr("Device")+":");
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          subComposite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            text = Widgets.newText(subComposite);
            text.setToolTipText(BARControl.tr("Device name."));
            Widgets.layout(text,0,0,TableLayoutData.WE);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                Color  color  = COLOR_MODIFIED;
                String string = widget.getText();
                if (storageDeviceName.getString().equals(string)) color = null;
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
                widget.setBackground(null);
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
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageDeviceName));

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

          label = Widgets.newLabel(composite,BARControl.tr("Size")+":");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,1,1,TableLayoutData.WE);
          {
            combo = Widgets.newCombo(subComposite);
            combo.setToolTipText(BARControl.tr("Size of medium for device."));
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
                  if (volumeSize.getLong() == n) color = null;
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
                  widget.setBackground(null);
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
                  widget.setBackground(null);
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
                  widget.setBackground(null);
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

            label = Widgets.newLabel(subComposite,BARControl.tr("bytes"));
            Widgets.layout(label,0,1,TableLayoutData.W);
          }
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Scripts"));
      tab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0,1.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // pre-script
        label = Widgets.newLabel(tab,BARControl.tr("Pre-script")+":");
        Widgets.layout(label,0,0,TableLayoutData.W);
        text = Widgets.newText(tab,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
        text.setToolTipText(BARControl.tr("Command or script to execute before start of a job."));
        Widgets.layout(text,1,0,TableLayoutData.NSWE);
        text.addModifyListener(new ModifyListener()
        {
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            Color  color  = COLOR_MODIFIED;
            String string = widget.getText();
            if (preCommand.equals(string)) color = null;
            widget.setBackground(color);
          }
        });
        text.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            String text = widget.getText();
            BARServer.setOption(selectedJobId,"pre-command",text);
            widget.setBackground(null);
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
            String text = widget.getText();
            BARServer.setOption(selectedJobId,"pre-command",text);
            widget.setBackground(null);
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(text,preCommand));

        // post-script
        label = Widgets.newLabel(tab,BARControl.tr("Post-script")+":");
        Widgets.layout(label,2,0,TableLayoutData.W);
        text = Widgets.newText(tab,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
        text.setToolTipText(BARControl.tr("Command or script to execute after termination of a job."));
        Widgets.layout(text,3,0,TableLayoutData.NSWE);
        text.addModifyListener(new ModifyListener()
        {
          public void modifyText(ModifyEvent modifyEvent)
          {
            Text   widget = (Text)modifyEvent.widget;
            Color  color  = COLOR_MODIFIED;
            String string = widget.getText();
            if (postCommand.equals(string)) color = null;
            widget.setBackground(color);
          }
        });
        text.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            String text = widget.getText();
            BARServer.setOption(selectedJobId,"post-command",text);
            widget.setBackground(null);
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
            String text = widget.getText();
            BARServer.setOption(selectedJobId,"post-command",text);
            widget.setBackground(null);
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(text,postCommand));
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Schedule"));
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // list
        widgetScheduleList = Widgets.newTable(tab);
        widgetScheduleList.setToolTipText(BARControl.tr("List with schedule entries.\nDouble-click to edit entry, right-click to open context menu."));
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
        tableColumn = Widgets.addTableColumn(widgetScheduleList,0,BARControl.tr("Date"),        SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,1,BARControl.tr("Week day"),    SWT.LEFT,250,true );
        synchronized(scheduleList)
        {
          Widgets.sortTableColumn(widgetScheduleList,tableColumn,new ScheduleDataComparator(widgetScheduleList,tableColumn));
        }
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,2,BARControl.tr("Time"),        SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,3,BARControl.tr("Archive type"),SWT.LEFT, 80,true );
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,4,BARControl.tr("Custom text"), SWT.LEFT, 90,true );
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,5,BARControl.tr("Enabled"),     SWT.LEFT, 60,false);
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

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          widgetScheduleListAdd = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
          widgetScheduleListAdd.setToolTipText(BARControl.tr("Add new schedule entry."));
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

          widgetScheduleListEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
          widgetScheduleListEdit.setToolTipText(BARControl.tr("Edit schedule entry."));
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

          button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
          button.setToolTipText(BARControl.tr("Clone schedule entry."));
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

          widgetScheduleListRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
          widgetScheduleListRemove.setToolTipText(BARControl.tr("Remove schedule entry."));
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

  /** set tab status reference
   * @param tabStatus tab statut object
   */
  void setTabStatus(TabStatus tabStatus)
  {
    this.tabStatus = tabStatus;
  }

  /** select job by name
   * @param name job name
   */
  public void selectJob(String name)
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

  /** create new job
   * @return true iff new job created
   */
  public boolean jobNew()
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
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobName = Widgets.newText(composite);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetAdd = Widgets.newButton(composite,BARControl.tr("Add"));
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
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
        Dialogs.close(dialog,true);
      }
    });

    return (Boolean)Dialogs.run(dialog,false);
  }

  /** clone job
   * @param jobId job id
   * @param jobName job name
   * @return true iff job cloned
   */
  public boolean jobClone(final int jobId, final String jobName)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert jobId != 0;
    assert jobName != null;

    final Shell dialog = Dialogs.openModal(shell,"Clone job",300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetJobName;
    final Button widgetClone;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobName = Widgets.newText(composite);
      widgetJobName.setText(jobName);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetClone = Widgets.newButton(composite,BARControl.tr("Clone"));
      widgetClone.setEnabled(false);
      Widgets.layout(widgetClone,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
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
    widgetJobName.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text  widget = (Text)modifyEvent.widget;
        widgetClone.setEnabled(!widget.getText().equals(jobName));
      }
    });
    widgetJobName.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetClone.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetClone.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget     = (Button)selectionEvent.widget;
        String newJobName = widgetJobName.getText();
        if (!newJobName.equals(""))
        {
          try
          {
            String[] errorMessage = new String[1];
            int error = BARServer.executeCommand(StringParser.format("JOB_CLONE jobId=%d name=%S",
                                                                     jobId,
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
              Dialogs.error(shell,"Cannot clone job:\n\n"+errorMessage[0]);
            }
          }
          catch (CommunicationError error)
          {
            Dialogs.error(shell,"Cannot clone job:\n\n"+error.getMessage());
          }
        }
        Dialogs.close(dialog,true);
      }
    });

    Widgets.setFocus(widgetJobName);
    return (Boolean)Dialogs.run(dialog,false);
  }

  /** rename job
   * @param jobId job id
   * @param jobName job name
   * @return true iff new job renamed
   */
  public boolean jobRename(final int jobId, final String jobName)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert jobId != 0;
    assert jobName != null;

    final Shell dialog = Dialogs.openModal(shell,"Rename job",300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetNewJobName;
    final Button widgetRename;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Old name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      label = Widgets.newLabel(composite,jobName);
      Widgets.layout(label,0,1,TableLayoutData.W);

      label = Widgets.newLabel(composite,BARControl.tr("New name")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);

      widgetNewJobName = Widgets.newText(composite);
      widgetNewJobName.setText(jobName);
      Widgets.layout(widgetNewJobName,1,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetRename = Widgets.newButton(composite,BARControl.tr("Rename"));
      widgetRename.setEnabled(false);
      Widgets.layout(widgetRename,0,0,TableLayoutData.W);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E);
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
    widgetNewJobName.addModifyListener(new ModifyListener()
    {
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text  widget = (Text)modifyEvent.widget;
        widgetRename.setEnabled(!widget.getText().equals(jobName));
      }
    });
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
                                                                     jobId,
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
        Dialogs.close(dialog,true);
      }
    });

    Widgets.setFocus(widgetNewJobName);
    return (Boolean)Dialogs.run(dialog,false);
  }

  /** delete job
   * @param jobId job id
   * @param jobName job name
   * @return true iff job deleted
   */
  public boolean jobDelete(int jobId, String jobName)
  {
    assert jobId != 0;
    assert jobName != null;

    if (Dialogs.confirm(shell,"Delete job '"+jobName+"'?"))
    {
      try
      {
        String[] result = new String[1];
        int error = BARServer.executeCommand(StringParser.format("JOB_DELETE jobId=%d",jobId));
        if (error == Errors.NONE)
        {
          updateJobList();
          selectJobEvent.trigger();
          clear();
        }
        else
        {
          Dialogs.error(shell,"Cannot delete job:\n\n"+result[0]);
          return false;
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,"Cannot delete job:\n\n"+error.getMessage());
        return false;
      }
    }

    return true;
  }

  //-----------------------------------------------------------------------

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
      preCommand.set(BARServer.getStringOption(selectedJobId,"pre-command"));
      postCommand.set(BARServer.getStringOption(selectedJobId,"post-command"));

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
        try
        {
          // get data
          int    jobId = resultMap.getInt   ("jobId");
          String name  = resultMap.getString("name" );

// TODO deleted jobs?
          int index = findJobListIndex(name);
          widgetJobList.add(name,index);
          jobIds.put(name,jobId);
        }
        catch (IllegalArgumentException exception)
        {
          if (Settings.debugFlag)
          {
            System.err.println("ERROR: "+exception.getMessage());
          }
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** clone selected job
   */
  private void jobClone()
  {
    assert selectedJobId != 0;
    assert selectedJobName != null;

    jobClone(selectedJobId,selectedJobName);
  }

  /** rename selected job
   */
  private void jobRename()
  {
    assert selectedJobId != 0;
    assert selectedJobName != null;

    jobRename(selectedJobId,selectedJobName);
  }

  /** delete selected job
   */
  private void jobDelete()
  {
    assert selectedJobId != 0;
    assert selectedJobName != null;

    jobDelete(selectedJobId,selectedJobName);
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
                                                            resultErrorMessage,
                                                            resultMapList
                                                    );
    if (error == Errors.NONE)
    {
      for (ValueMap resultMap : resultMapList)
      {
        try
        {
          FileTypes fileType = resultMap.getEnum("fileType",FileTypes.class);
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
                long    size     = resultMap.getLong  ("size", 0L);
                long    dateTime = resultMap.getLong  ("dateTime");

                SpecialTypes specialType = resultMap.getEnum("specialType",SpecialTypes.class);
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
        catch (IllegalArgumentException exception)
        {
          if (Settings.debugFlag)
          {
            System.err.println("ERROR: "+exception.getMessage());
          }
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
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error == Errors.NONE)
    {
      for (ValueMap resultMap : resultMapList)
      {
        try
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
        catch (IllegalArgumentException exception)
        {
          if (Settings.debugFlag)
          {
            System.err.println("ERROR: "+exception.getMessage());
          }
        }
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
      try
      {
        // get data
        EntryTypes   entryType   = resultMap.getEnum  ("entryType",  EntryTypes.class  );
        PatternTypes patternType = resultMap.getEnum  ("patternType",PatternTypes.class);
        String       pattern     = resultMap.getString("pattern"                       );

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
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugFlag)
        {
          System.err.println("ERROR: "+exception.getMessage());
        }
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
      try
      {
        // get data
        PatternTypes patternType = resultMap.getEnum  ("patternType",PatternTypes.class);
        String       pattern     = resultMap.getString("pattern"                       );

        if (!pattern.equals(""))
        {
          excludeHashSet.add(pattern);
          Widgets.insertListEntry(widgetExcludeList,
                            findListIndex(widgetExcludeList,pattern),
                            (Object)pattern,
                            pattern
                           );
        }
      }
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugFlag)
        {
          System.err.println("ERROR: "+exception.getMessage());
        }
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
      try
      {
        // get data
        PatternTypes patternType = resultMap.getEnum  ("patternType",PatternTypes.class);
        String       pattern     = resultMap.getString("pattern"                       );

        if (!pattern.equals(""))
        {
           compressExcludeHashSet.add(pattern);
           Widgets.insertListEntry(widgetCompressExcludeList,
                                   findListIndex(widgetCompressExcludeList,pattern),
                                   (Object)pattern,
                                   pattern
                                  );
        }
      }
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugFlag)
        {
          System.err.println("ERROR: "+exception.getMessage());
        }
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
      label = Widgets.newLabel(composite,BARControl.tr("Pattern")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetPattern = Widgets.newText(subComposite);
        widgetPattern.setToolTipText(BARControl.tr("Include pattern. Use * and ? as wildcards."));
        if (entryData.pattern != null) widgetPattern.setText(entryData.pattern);
        Widgets.layout(widgetPattern,0,0,TableLayoutData.WE);

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

      label = Widgets.newLabel(composite,BARControl.tr("Type")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,0.0));
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        button = Widgets.newRadio(subComposite,BARControl.tr("file"));
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
        button = Widgets.newRadio(subComposite,BARControl.tr("image"));
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

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
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
      label = Widgets.newLabel(composite,BARControl.tr("Pattern")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPattern = Widgets.newText(composite);
      widgetPattern.setToolTipText(BARControl.tr("Exclude pattern. Use * and ? as wildcards."));
      if (pattern[0] != null) widgetPattern.setText(pattern[0]);
      Widgets.layout(widgetPattern,0,1,TableLayoutData.WE);

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

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
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

    // update list
    Widgets.insertListEntry(widgetExcludeList,
                            findListIndex(widgetExcludeList,pattern),
                            (Object)pattern,
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
                              (Object)pattern,
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
      label = Widgets.newLabel(composite,BARControl.tr("Pattern")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPattern = Widgets.newText(composite);
      widgetPattern.setToolTipText(BARControl.tr("Compress exclude pattern. Use * and ? as wildcards."));
      if (pattern[0] != null) widgetPattern.setText(pattern[0]);
      Widgets.layout(widgetPattern,0,1,TableLayoutData.WE);

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

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
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
                              (Object)pattern,
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
                                (Object)pattern,
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
                              (Object)pattern,
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


    String[] patterns = widgetCompressExcludeList.getSelection();
    if (patterns.length > 0)
    {
      if (Dialogs.confirm(shell,"Remove "+patterns.length+" selected compress exclude patterns?"))
      {
        compressExcludeListRemove(patterns);
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
        label = Widgets.newLabel(composite,BARControl.tr("File name")+":");
        Widgets.layout(label,0,0,TableLayoutData.W);

        widgetFileName = Widgets.newCanvas(composite,SWT.BORDER);
        widgetFileName.setToolTipText(BARControl.tr("Drag to trashcan icon to the right to remove name part."));
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
        control.setToolTipText(BARControl.tr("Use drag&drop to remove name parts."));
        Widgets.layout(control,0,2,TableLayoutData.NONE);
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

        label = Widgets.newLabel(composite,BARControl.tr("Example")+":");
        Widgets.layout(label,1,0,TableLayoutData.W);

        widgetExample = Widgets.newView(composite);
        Widgets.layout(widgetExample,1,1,TableLayoutData.WE,0,2);
      }

      composite = Widgets.newComposite(parentComposite,SWT.NONE);
      composite.setToolTipText(BARControl.tr("Use drag&drop to add name parts."));
      composite.setLayout(new TableLayout(0.0,new double[]{0.0,0.4,0.0,0.4,0.0,0.2}));
      Widgets.layout(composite,1,0,TableLayoutData.NSWE);
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
        addDragAndDrop(composite,"%T","archive type short: F, I, D",                   10,0);
        addDragAndDrop(composite,"%last","'-last' if last archive part",               11,0);
        addDragAndDrop(composite,"%uuid","universally unique identifier",              12,0);
        addDragAndDrop(composite,"%text","schedule custom text",                       13,0);

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
        addDragAndDrop(composite,"%U","week number 00..53",                            14,1);
        addDragAndDrop(composite,"%U2","week number 1 or 2",                           15,1);
        addDragAndDrop(composite,"%U4","week number 1, 2, 3, 4",                       16,1);
        addDragAndDrop(composite,"%W","week number 00..53",                            17,1);
        addDragAndDrop(composite,"%W2","week number 1 or 2",                           18,1);
        addDragAndDrop(composite,"%W4","week number 1, 2, 3, 4",                       19,1);
        addDragAndDrop(composite,"%C","century two digits",                            20,1);
        addDragAndDrop(composite,"%y","year two digits",                               21,1);
        addDragAndDrop(composite,"%Y","year four digits",                              22,1);
        addDragAndDrop(composite,"%S","seconds since 1.1.1970 00:00",                  23,1);
        addDragAndDrop(composite,"%Z","time-zone abbreviation",                        24,1);

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
            else if (storageNamePart.string.equals("%T"))
              buffer.append("F");
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
            else if (storageNamePart.string.equals("%U2"))
              buffer.append("1");
            else if (storageNamePart.string.equals("%U4"))
              buffer.append("3");
            else if (storageNamePart.string.equals("%W"))
              buffer.append("51");
            else if (storageNamePart.string.equals("%W2"))
              buffer.append("1");
            else if (storageNamePart.string.equals("%W4"))
              buffer.append("3");
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
      widgetSave = Widgets.newButton(composite,BARControl.tr("Save"));
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
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
        try
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
        catch (IllegalArgumentException exception)
        {
          if (Settings.debugFlag)
          {
            System.err.println("ERROR: "+exception.getMessage());
          }
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
      label = Widgets.newLabel(composite,BARControl.tr("Date")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetYear = Widgets.newOptionMenu(subComposite);
        widgetYear.setToolTipText(BARControl.tr("Year to execute job. Leave to '*' for each year."));
        widgetYear.setItems(new String[]{"*","2008","2009","2010","2011","2012","2013","2014","2015","2016","2017","2018","2019","2020"});
        widgetYear.setText(scheduleData.getYear()); if (widgetYear.getText().equals("")) widgetYear.setText("*");
        if (widgetYear.getText().equals("")) widgetYear.setText("*");
        Widgets.layout(widgetYear,0,0,TableLayoutData.W);

        widgetMonth = Widgets.newOptionMenu(subComposite);
        widgetMonth.setToolTipText(BARControl.tr("Month to execute job. Leave to '*' for each month."));
        widgetMonth.setItems(new String[]{"*","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"});
        widgetMonth.setText(scheduleData.getMonth()); if (widgetMonth.getText().equals("")) widgetMonth.setText("*");
        Widgets.layout(widgetMonth,0,1,TableLayoutData.W);

        widgetDay = Widgets.newOptionMenu(subComposite);
        widgetDay.setToolTipText(BARControl.tr("Day to execute job. Leave to '*' for each day."));
        widgetDay.setItems(new String[]{"*","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31"});
        widgetDay.setText(scheduleData.getDay()); if (widgetDay.getText().equals("")) widgetDay.setText("*");
        Widgets.layout(widgetDay,0,2,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Week days")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        widgetWeekDays[ScheduleData.MON] = Widgets.newCheckbox(subComposite,BARControl.tr("Mon"));
        widgetWeekDays[ScheduleData.MON].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[ScheduleData.MON],0,0,TableLayoutData.W);
        widgetWeekDays[ScheduleData.MON].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.MON));

        widgetWeekDays[ScheduleData.TUE] = Widgets.newCheckbox(subComposite,BARControl.tr("Tue"));
        widgetWeekDays[ScheduleData.TUE].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[ScheduleData.TUE],0,1,TableLayoutData.W);
        widgetWeekDays[ScheduleData.TUE].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.TUE));

        widgetWeekDays[ScheduleData.WED] = Widgets.newCheckbox(subComposite,BARControl.tr("Wed"));
        widgetWeekDays[ScheduleData.WED].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[ScheduleData.WED],0,2,TableLayoutData.W);
        widgetWeekDays[ScheduleData.WED].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.WED));

        widgetWeekDays[ScheduleData.THU] = Widgets.newCheckbox(subComposite,BARControl.tr("Thu"));
        widgetWeekDays[ScheduleData.THU].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[ScheduleData.THU],0,3,TableLayoutData.W);
        widgetWeekDays[ScheduleData.THU].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.THU));

        widgetWeekDays[ScheduleData.FRI] = Widgets.newCheckbox(subComposite,BARControl.tr("Fri"));
        widgetWeekDays[ScheduleData.FRI].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[ScheduleData.FRI],0,4,TableLayoutData.W);
        widgetWeekDays[ScheduleData.FRI].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.FRI));

        widgetWeekDays[ScheduleData.SAT] = Widgets.newCheckbox(subComposite,BARControl.tr("Sat"));
        widgetWeekDays[ScheduleData.SAT].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[ScheduleData.SAT],0,5,TableLayoutData.W);
        widgetWeekDays[ScheduleData.SAT].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.SAT));

        widgetWeekDays[ScheduleData.SUN] = Widgets.newCheckbox(subComposite,BARControl.tr("Sun"));
        widgetWeekDays[ScheduleData.SUN].setToolTipText(BARControl.tr("Week days to execute job."));
        Widgets.layout(widgetWeekDays[ScheduleData.SUN],0,6,TableLayoutData.W);
        widgetWeekDays[ScheduleData.SUN].setSelection(scheduleData.weekDayIsEnabled(ScheduleData.SUN));

        button = Widgets.newButton(subComposite,IMAGE_TOGGLE_MARK);
        button.setToolTipText(BARControl.tr("Toggle week days set."));
        Widgets.layout(button,0,7,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            for (Button button : widgetWeekDays)
            {
              button.setSelection(!button.getSelection());
            }
          }
        });
      }

      label = Widgets.newLabel(composite,BARControl.tr("Time")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,2,1,TableLayoutData.WE);
      {
        widgetHour = Widgets.newOptionMenu(subComposite);
        widgetHour.setToolTipText(BARControl.tr("Hour to execute job. Leave to '*' for every hour."));
        widgetHour.setItems(new String[]{"*","00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetHour.setText(scheduleData.getHour()); if (widgetHour.getText().equals("")) widgetHour.setText("*");
        Widgets.layout(widgetHour,0,0,TableLayoutData.W);

        widgetMinute = Widgets.newOptionMenu(subComposite);
        widgetMinute.setToolTipText(BARControl.tr("Minute to execute job. Leave to '*' for every minute."));
        widgetMinute.setItems(new String[]{"*","00","05","10","15","20","30","35","40","45","50","55"});
        widgetMinute.setText(scheduleData.getMinute()); if (widgetMinute.getText().equals("")) widgetMinute.setText("*");
        Widgets.layout(widgetMinute,0,1,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Type")+":");
      Widgets.layout(label,3,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,3,1,TableLayoutData.WE);
      {
        widgetTypeDefault = Widgets.newRadio(subComposite,BARControl.tr("*"));
        widgetTypeDefault.setToolTipText(BARControl.tr("Execute job with default type."));
        Widgets.layout(widgetTypeDefault,0,0,TableLayoutData.W);
        widgetTypeDefault.setSelection(scheduleData.archiveType.equals("*"));

        widgetTypeNormal = Widgets.newRadio(subComposite,BARControl.tr("normal"));
        widgetTypeNormal.setToolTipText(BARControl.tr("Execute job as normal backup (no incremental data)."));
        Widgets.layout(widgetTypeNormal,0,1,TableLayoutData.W);
        widgetTypeNormal.setSelection(scheduleData.archiveType.equals("normal"));

        widgetTypeFull = Widgets.newRadio(subComposite,BARControl.tr("full"));
        widgetTypeFull.setToolTipText(BARControl.tr("Execute job as full backup."));
        Widgets.layout(widgetTypeFull,0,2,TableLayoutData.W);
        widgetTypeFull.setSelection(scheduleData.archiveType.equals("full"));

        widgetTypeIncremental = Widgets.newRadio(subComposite,BARControl.tr("incremental"));
        widgetTypeIncremental.setToolTipText(BARControl.tr("Execute job as incremental backup."));
        Widgets.layout(widgetTypeIncremental,0,3,TableLayoutData.W);
        widgetTypeIncremental.setSelection(scheduleData.archiveType.equals("incremental"));

        widgetTypeDifferential = Widgets.newRadio(subComposite,BARControl.tr("differential"));
        widgetTypeDifferential.setToolTipText(BARControl.tr("Execute job as differential backup."));
        Widgets.layout(widgetTypeDifferential,0,4,TableLayoutData.W);
        widgetTypeDifferential.setSelection(scheduleData.archiveType.equals("differential"));
      }

      label = Widgets.newLabel(composite,BARControl.tr("Custom text")+":");
      Widgets.layout(label,4,0,TableLayoutData.W);

      widgetCustomText = Widgets.newText(composite);
      widgetCustomText.setToolTipText(BARControl.tr("Custom text."));
      widgetCustomText.setText(scheduleData.customText);
      Widgets.layout(widgetCustomText,4,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Options")+":");
      Widgets.layout(label,5,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,5,1,TableLayoutData.WE);
      {
        widgetEnabled = Widgets.newCheckbox(subComposite,BARControl.tr("enabled"));
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

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
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

    TableItem[] tableItems = widgetScheduleList.getSelection();
    if (tableItems.length > 0)
    {
      if (Dialogs.confirm(shell,"Delete "+tableItems.length+" selected schedule entries?"))
      {
        for (TableItem tableItem : tableItems)
        {
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
