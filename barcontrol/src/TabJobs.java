/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
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
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.ListIterator;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

// graphics
import org.eclipse.swt.widgets.TabItem;
import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.custom.StyledText;
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
import org.eclipse.swt.events.MenuListener;
import org.eclipse.swt.events.MenuEvent;
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
public class TabJobs
{
  /** status update thread
   */
  class UpdateThread extends Thread
  {
    final int SLEEP_TIME = 30;

    /** initialize status update thread
     */
    UpdateThread()
    {
      setName("BARControl Update Job");
      setDaemon(true);
    }

    /** run status update thread
     */
    public void run()
    {
      try
      {
        for (;;)
        {
          // update
          try
          {
            // update job
            update();
          }
          catch (org.eclipse.swt.SWTException exception)
          {
            // ignore SWT exceptions
            if (Settings.debugLevel > 2)
            {
              BARControl.printStackTrace(exception);
              System.exit(ExitCodes.FAIL);
            }
          }

          // sleep a short time
          try { Thread.sleep(SLEEP_TIME*1000); } catch (InterruptedException exception) { /* ignored */ };
        }
      }
      catch (Throwable throwable)
      {
        if (Settings.debugLevel > 0)
        {
          BARServer.disconnect();
          BARControl.internalError(throwable);
        }
      }
    }
  }

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

  /** file tree data
   */
  class FileTreeData
  {
    String                 name;
    BARServer.FileTypes    fileType;
    long                   size;
    long                   dateTime;
    String                 title;
    BARServer.SpecialTypes specialType;
    boolean                noBackup;       // true iff .nobackup exists in directory
    boolean                noDump;         // true iff no-dump attribute is set

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param size file size [bytes]
     * @param dateTime file date/time [s]
     * @param title title to display
     * @param specialType special type
     * @param noBackup true iff .nobackup exists (directory)
     * @param noDump true iff no-dump attribute is set
     */
    FileTreeData(String name, BARServer.FileTypes fileType, long size, long dateTime, String title, BARServer.SpecialTypes specialType, boolean noBackup, boolean noDump)
    {
      this.name        = name;
      this.fileType    = fileType;
      this.size        = size;
      this.dateTime    = dateTime;
      this.title       = title;
      this.specialType = specialType;
      this.noBackup    = noBackup;
      this.noDump      = noDump;
    }

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param size file size [bytes]
     * @param dateTime file date/time [s]
     * @param title title to display
     * @param noBackup true iff .nobackup exists (directory)
     * @param noDump true iff no-dump attribute is set
     */
    FileTreeData(String name, BARServer.FileTypes fileType, long size, long dateTime, String title, boolean noBackup, boolean noDump)
    {
      this(name,fileType,size,dateTime,title,BARServer.SpecialTypes.NONE,noBackup,noDump);
    }

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param dateTime file date/time [s]
     * @param title title to display
     * @param noBackup true iff .nobackup exists (directory)
     * @param noDump true iff no-dump attribute is set
     */
    FileTreeData(String name, BARServer.FileTypes fileType, long dateTime, String title, boolean noBackup, boolean noDump)
    {
      this(name,fileType,0L,dateTime,title,noBackup,noDump);
    }

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param title title to display
     * @param noBackup true iff .nobackup exists (directory)
     * @param noDump true iff no-dump attribute is set
     */
    FileTreeData(String name, BARServer.FileTypes fileType, String title, boolean noBackup, boolean noDump)
    {
      this(name,fileType,0L,title,noBackup,noDump);
    }

    /** create file tree data
     * @param name file name
     * @param specialType special type
     * @param size file size [bytes]
     * @param dateTime file date/time [s]
     * @param title title to display
     * @param noBackup true iff .nobackup exists (directory)
     * @param noDump true iff no-dump attribute is set
     */
    FileTreeData(String name, BARServer.SpecialTypes specialType, long size, long dateTime, String title, boolean noBackup, boolean noDump)
    {
      this(name,BARServer.FileTypes.SPECIAL,size,dateTime,title,specialType,noBackup,noDump);
    }

    /** create file tree data
     * @param name file name
     * @param specialType special type
     * @param dateTime file date/time [s]
     * @param title title to display
     * @param noBackup true iff .nobackup exists (directory)
     * @param noDump true iff no-dump attribute is set
     */
    FileTreeData(String name, BARServer.SpecialTypes specialType, long dateTime, String title, boolean noBackup, boolean noDump)
    {
      this(name,BARServer.FileTypes.SPECIAL,0L,dateTime,title,specialType,noBackup,noDump);
    }

    /** get image for entry data
     * @return image
     */
    Image getImage()
    {
      Image image = null;
      if      (isIncluded(name) && !isExcluded(name))
      {
        switch (fileType)
        {
          case FILE:      image = IMAGE_FILE_INCLUDED;      break;
          case DIRECTORY: image = IMAGE_DIRECTORY_INCLUDED; break;
          case LINK:      image = IMAGE_LINK_INCLUDED;      break;
          case HARDLINK:  image = IMAGE_LINK_INCLUDED;      break;
          case SPECIAL:   image = IMAGE_FILE_INCLUDED;      break;
        }
      }
      else if (isExcluded(name) || noBackup || noDump )
      {
        switch (fileType)
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
        switch (fileType)
        {
          case FILE:      image = IMAGE_FILE;      break;
          case DIRECTORY: image = IMAGE_DIRECTORY; break;
          case LINK:      image = IMAGE_LINK;      break;
          case HARDLINK:  image = IMAGE_LINK;      break;
          case SPECIAL:
            switch (specialType)
            {
              case DEVICE_CHARACTER: image = IMAGE_FILE;      break;
              case DEVICE_BLOCK:     image = IMAGE_FILE;      break;
              case FIFO:             image = IMAGE_FILE;      break;
              case SOCKET:           image = IMAGE_FILE;      break;
              case OTHER:            image = IMAGE_FILE;      break;
            }
            break;
        }
      }

      return image;
    }

    public void include()
    {
      includeListAdd(new EntryData(EntryTypes.FILE,name));
      excludeListRemove(name);
      if (fileType == BARServer.FileTypes.DIRECTORY) setNoBackup(name,false);
      setNoDump(name,false);

      noBackup = false;
      noDump   = false;
    }

    public void excludeByList()
    {
      includeListRemove(name);
      excludeListAdd(name);
      if (fileType == BARServer.FileTypes.DIRECTORY) setNoBackup(name,false);
      setNoDump(name,false);

      noBackup = false;
      noDump   = false;
    }

    public void excludeByNoBackup()
    {
      includeListRemove(name);
      excludeListRemove(name);
      if (fileType == BARServer.FileTypes.DIRECTORY) setNoBackup(name,true);
      setNoDump(name,false);

      noBackup = true;
      noDump   = false;
    }

    public void excludeByNoDump()
    {
      includeListRemove(name);
      excludeListRemove(name);
      if (fileType == BARServer.FileTypes.DIRECTORY) setNoBackup(name,false);
      setNoDump(name,true);

      noBackup = false;
      noDump   = true;
    }

    public void none()
    {
      includeListRemove(name);
      excludeListRemove(name);
      if (fileType == BARServer.FileTypes.DIRECTORY) setNoBackup(name,false);
      setNoDump(name,false);

      noBackup = false;
      noDump   = false;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "FileTreeData {"+name+", "+fileType+", "+size+" bytes, dateTime="+dateTime+", title="+title+"}";
    }
  }

  /** file data comparator
   */
  static class FileTreeDataComparator implements Comparator<FileTreeData>
  {
    // sort modes
    enum SortModes
    {
      NAME,
      TYPE,
      SIZE,
      DATETIME
    };

    final static SortModes[] SORT_MODE_SEQUENCE_NAME     = new SortModes[]{SortModes.NAME, SortModes.TYPE, SortModes.SIZE, SortModes.DATETIME};
    final static SortModes[] SORT_MODE_SEQUENCE_TYPE     = new SortModes[]{SortModes.TYPE, SortModes.NAME, SortModes.SIZE, SortModes.DATETIME};
    final static SortModes[] SORT_MODE_SEQUENCE_SIZE     = new SortModes[]{SortModes.SIZE, SortModes.NAME, SortModes.TYPE, SortModes.DATETIME};
    final static SortModes[] SORT_MODE_SEQUENCE_DATETIME = new SortModes[]{SortModes.DATETIME, SortModes.NAME, SortModes.TYPE, SortModes.SIZE};

    private SortModes[] sortModeSequence;

    /** create file data comparator
     * @param tree file tree
     * @param sortColumn column to sort
     */
    FileTreeDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortModeSequence = SORT_MODE_SEQUENCE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortModeSequence = SORT_MODE_SEQUENCE_TYPE;
      else if (tree.getColumn(2) == sortColumn) sortModeSequence = SORT_MODE_SEQUENCE_SIZE;
      else if (tree.getColumn(3) == sortColumn) sortModeSequence = SORT_MODE_SEQUENCE_DATETIME;
      else                                      sortModeSequence = SORT_MODE_SEQUENCE_NAME;
    }

    /** create file data comparator
     * @param tree file tree
     */
    FileTreeDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
    }

    /** compare file tree data
     * @param fileTreeData1, fileTreeData2 file tree data to compare
     * @return -1 iff fileTreeData1 < fileTreeData2,
                0 iff fileTreeData1 = fileTreeData2,
                1 iff fileTreeData1 > fileTreeData2
     */
    @Override
    public int compare(FileTreeData fileTreeData1, FileTreeData fileTreeData2)
    {
      int result = 0;

      int i = 0;
      while ((i < sortModeSequence.length) && (result == 0))
      {
        if (sortModeSequence[i] == SortModes.NAME)
        {
          // directories first, then files
          if (fileTreeData1.fileType == BARServer.FileTypes.DIRECTORY)
          {
            if (fileTreeData2.fileType == BARServer.FileTypes.DIRECTORY)
            {
              result = compareWithoutType(sortModeSequence[i],fileTreeData1,fileTreeData2);
            }
            else
            {
              result = -1;
            }
          }
          else
          {
            if (fileTreeData2.fileType == BARServer.FileTypes.DIRECTORY)
            {
              result = 1;
            }
            else
            {
              result = compareWithoutType(sortModeSequence[i],fileTreeData1,fileTreeData2);
            }
          }
        }
        else
        {
          // sort directories/files mixed
          result = compareWithoutType(sortModeSequence[i],fileTreeData1,fileTreeData2);
        }
        i++;
      }
      return result;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "FileTreeDataComparator {"+sortModeSequence+"}";
    }

    /** compare file tree data without take care about type
     * @param sortMode sort mode
     * @param fileTreeData1, fileTreeData2 file tree data to compare
     * @return -1 iff fileTreeData1 < fileTreeData2,
                0 iff fileTreeData1 = fileTreeData2,
                1 iff fileTreeData1 > fileTreeData2
     */
    private int compareWithoutType(SortModes sortMode, FileTreeData fileTreeData1, FileTreeData fileTreeData2)
    {
      switch (sortMode)
      {
        case NAME:
          return fileTreeData1.title.compareTo(fileTreeData2.title);
        case TYPE:
          return fileTreeData1.fileType.compareTo(fileTreeData2.fileType);
        case SIZE:
          if      (fileTreeData1.size < fileTreeData2.size) return -1;
          else if (fileTreeData1.size > fileTreeData2.size) return  1;
          else                                              return  0;
        case DATETIME:
          if      (fileTreeData1.dateTime < fileTreeData2.dateTime) return -1;
          else if (fileTreeData1.dateTime > fileTreeData2.dateTime) return  1;
          else                                                      return  0;
        default:
          return 0;
      }
    }
  }

  /** device data
   */
  class DeviceData
  {
    String name;
    long   size;

    /** create device data
     * @param name device name
     * @param size device size [bytes]
     */
    DeviceData(String name, long size)
    {
      this.name = name;
      this.size = size;
    }

    /** create device data
     * @param name device name
     */
    DeviceData(String name)
    {
      this.name = name;
      this.size = 0;
    }

    /** insert in include list, remove from exclude list
     */
    public void include()
    {
      includeListAdd(new EntryData(EntryTypes.IMAGE,name));
      excludeListRemove(name);
    }

    /** insert in exclude list, remove from include list
     */
    public void exclude()
    {
      includeListRemove(name);
      excludeListAdd(name);
    }

    /** remove from include list, remove from exclude list
     */
    public void none()
    {
      includeListRemove(name);
      excludeListRemove(name);
    }

    /** get image for entry data
     * @return image
     */
    Image getImage()
    {
      Image image = null;
      if      (isIncluded(name) && !isExcluded(name))
        image = IMAGE_DEVICE_INCLUDED;
      else if (isExcluded(name))
        image = IMAGE_DEVICE;
      else
        image = IMAGE_DEVICE;

      return image;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "DeviceData {"+name+", "+size+" bytes}";
    }
  }

  /** device data comparator
   */
  static class DeviceDataComparator implements Comparator<DeviceData>
  {
    // tree sort modes
    enum SortModes
    {
      NAME,
      SIZE
    };

    private SortModes sortMode;

    /** create device data comparator
     * @param table device table
     * @param sortColumn column to sort
     */
    DeviceDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SortModes.NAME;
      else if (table.getColumn(1) == sortColumn) sortMode = SortModes.SIZE;
      else                                       sortMode = SortModes.NAME;
    }

    /** create device data comparator
     * @param tree device tree
     */
    DeviceDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare device tree data without take care about type
     * @param deviceData1, deviceData2 device tree data to compare
     * @return -1 iff deviceData1 < deviceData2,
                0 iff deviceData1 = deviceData2,
                1 iff deviceData1 > deviceData2
     */
    @Override
    public int compare(DeviceData deviceData1, DeviceData deviceData2)
    {
      switch (sortMode)
      {
        case NAME:
          return deviceData1.name.compareTo(deviceData2.name);
        case SIZE:
          if      (deviceData1.size < deviceData2.size) return -1;
          else if (deviceData1.size > deviceData2.size) return  1;
          else                                          return  0;
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "DeviceDataComparator {"+sortMode+"}";
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
      String   jobUUID;
      String   name;
      boolean  forceFlag;
      int      depth;
      int      timeout;
      TreeItem treeItem;

      /** create directory info request
       * @param jobUUID job UUID or null
       * @param name directory name
       * @param forceFlag true to force update size
       * @param treeItem tree item
       * @param timeout timeout [ms] or -1 for no timeout
       */
      DirectoryInfoRequest(String jobUUID, String name, boolean forceFlag, TreeItem treeItem, int timeout)
      {
        this.jobUUID   = jobUUID;
        this.name      = name;
        this.forceFlag = forceFlag;
        this.depth     = StringUtils.splitArray(name,BARServer.filePathSeparator,true).length;
        this.timeout   = timeout;
        this.treeItem  = treeItem;
      }

      /** convert data to string
       * @return string
       */
      public String toString()
      {
      return "DirectoryInfoRequest {"+jobUUID+", "+name+", "+forceFlag+", "+depth+", "+timeout+"}";
      }
    }

    // timeouts to get directory information [ms]
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
      setName("BARControl Directory Info");
    }

    /** run method
     */
    @Override
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
              @Override
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
            final long    count;
            final long    size;
            final boolean timedOut;
            try
            {
              ValueMap valueMap = new ValueMap();
              BARServer.executeCommand(StringParser.format("DIRECTORY_INFO jobUUID=%s name=%'S timeout=%d",
                                                           (directoryInfoRequest.jobUUID != null) ? directoryInfoRequest.jobUUID : "",
                                                           directoryInfoRequest.name,
                                                           directoryInfoRequest.timeout
                                                          ),
                                       0,  // debugLevel
                                       valueMap
                                      );
              count    = valueMap.getLong   ("count"   );
              size     = valueMap.getLong   ("size"    );
              timedOut = valueMap.getBoolean("timedOut");
            }
            catch (Exception exception)
            {
              // command execution fail or parsing error; ignore request
              continue;
            }

            // update view
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                TreeItem treeItem = directoryInfoRequest.treeItem;
                if (!treeItem.isDisposed())
                {
                  FileTreeData fileTreeData = (FileTreeData)treeItem.getData();

                  fileTreeData.size = size;

                  treeItem.setText(2,Units.formatByteSize(size));
                  treeItem.setForeground(2,timedOut ? COLOR_RED : COLOR_BLACK);
                }
              }
            });

            if (timedOut)
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
      catch (Throwable throwable)
      {
        BARControl.logThrowable(throwable);
        if (Settings.debugLevel > 0)
        {
          BARServer.disconnect();
          System.exit(ExitCodes.FAIL);
        }
      }
    }

    /** add directory info request
     * @param jobUUID job UUID or null
     * @param name path name
     * @param forceFlag true to force update
     * @param treeItem tree item
     * @param timeout timeout [ms]
     */
    public void add(String jobUUID, String name, boolean forceFlag, TreeItem treeItem, int timeout)
    {
      DirectoryInfoRequest directoryInfoRequest = new DirectoryInfoRequest(jobUUID,name,forceFlag,treeItem,timeout);
      add(directoryInfoRequest);
    }

    /** add directory info request
     * @param jobUUID job UUID or null
     * @param name path name
     * @param treeItem tree item
     * @param timeout timeout [ms]
     */
    public void add(String jobUUID, String name, TreeItem treeItem, int timeout)
    {
      DirectoryInfoRequest directoryInfoRequest = new DirectoryInfoRequest(jobUUID,name,false,treeItem,timeout);
      add(directoryInfoRequest);
    }

    /** add directory info request with default timeout
     * @param jobUUID job UUID or null
     * @param name path name
     * @param forceFlag true to force update
     * @param treeItem tree item
     */
    public void add(String jobUUID, String name, boolean forceFlag, TreeItem treeItem)
    {
      add(jobUUID,name,forceFlag,treeItem,DEFAULT_TIMEOUT);
    }

    /** add directory info request with default timeout
     * @param jobUUID job UUID or null
     * @param name path name
     * @param treeItem tree item
     */
    public void add(String jobUUID, String name, TreeItem treeItem)
    {
      add(jobUUID,name,false,treeItem,DEFAULT_TIMEOUT);
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

    /** create entry data
     * @param entryType entry type
     */
    EntryData(EntryTypes entryType)
    {
      this(entryType,(String)null);
    }

    /** clone entry data object
     * @return cloned object
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

  /** entry data comparator
   */
  static class EntryDataComparator implements Comparator<EntryData>
  {
    /** create entry data comparator
     * @param table entry table
     */
    EntryDataComparator(Table table)
    {
    }

    /** compare entry data
     * @param entryData1, entryData2 tree data to compare
     * @return -1 iff entryData1 < entryData2,
                0 iff entryData1 = entryData2,
                1 iff entryData1 > entryData2
     */
    @Override
    public int compare(EntryData entryData1, EntryData entryData2)
    {
      return entryData1.pattern.compareTo(entryData2.pattern);
    }
  }

  /** pattern data
   */
  class PatternData
  {
    PatternTypes type;
    String       pattern;

    /** create patter data
     * @param type pattern type
     * @param pattern pattern
     */
    PatternData(PatternTypes type, String pattern)
    {
      this.type    = type;
      this.pattern = pattern;
    }
  }

  /** mount data
   */
  class MountData implements Cloneable, Comparable<MountData>
  {
    int     id;
    String  name;
    String  device;

    /** create mount data
     * @param id unique id
     * @param name mount name
     * @param device device name (or null)
     */
    MountData(int id, String name, String device)
    {
      this.id     = id;
      this.name   = name;
      this.device = device;
    }

    /** create mount data
     * @param name mount name
     * @param device device name (or null)
     */
    MountData(String name, String device)
    {
      this(0,name,device);
    }

    /** create mount data
     * @param name mount name
     */
    MountData(String name)
    {
      this(0,name,(String)null);
    }

    /** clone mount data
     * @return cloned object
     */
    public MountData clone()
    {
      return new MountData(name,device);
    }

    /** compare with other mount data
     * @return -1/0+1 iff lower/equals/greater
     */
    @Override
    public int compareTo(MountData other)
    {
      if      (id < other.id) return -1;
      else if (id > other.id) return  1;
      else                    return  0;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "MountData {"+id+", "+name+", "+device+"}";
    }
  }

  /** mount data comparator
   */
  static class MountDataComparator implements Comparator<MountData>
  {
    /** create mount data comparator
     * @param table mount table
     */
    MountDataComparator(Table table)
    {
    }

    /** compare mount data
     * @param mountData1, mountData2 tree data to compare
     * @return -1 iff mountData1 < mountData2,
                0 iff mountData1 = mountData2,
                1 iff mountData1 > mountData2
     */
    @Override
    public int compare(MountData mountData1, MountData mountData2)
    {
      return mountData1.name.compareTo(mountData2.name);
    }
  }

  /** schedule data
   */
  class ScheduleData implements Cloneable, Comparable<ScheduleData>
  {
    final static int NONE = 0;
    final static int ANY  = -1;
    final static int MON  = 0;
    final static int TUE  = 1;
    final static int WED  = 2;
    final static int THU  = 3;
    final static int FRI  = 4;
    final static int SAT  = 5;
    final static int SUN  = 6;

    String       uuid;
    int          year,month,day;
    int          weekDays;
    int          hour,minute;
    ArchiveTypes archiveType;
    int          interval;
    int          beginHour,beginMinute;
    int          endHour,endMinute;
    String       customText;
    boolean      testCreatedArchives;
    boolean      noStorage;
    boolean      enabled;

    long         lastExecutedDateTime;
    long         totalEntities,totalEntryCount,totalEntrySize;

    /** create schedule data
     * @param uuid schedule UUID
     * @param year year
     * @param month month [1..12]
     * @param day day [1..31]
     * @param weekDays week days
     * @param hour hour [0..23]
     * @param minute minute [0..59]
     * @param archiveType archive type string
     * @param interval continuous interval [min]
     * @param customText custom text
     * @param testCreatedArchives true to test created archives
     * @param noStorage true to skip storage
     * @param enabled true iff enabled
     * @param lastExecutedDateTime date/time of last execution
     * @param totalEntities total number of existing entities for schedule
     * @param totalEntryCount total number of existing entries for schedule
     * @param totalEntrySize total size of existing entries for schedule [bytes]
     */
    ScheduleData(String       uuid,
                 int          year,
                 int          month,
                 int          day,
                 int          weekDays,
                 int          hour,
                 int          minute,
                 ArchiveTypes archiveType,
                 int          interval,
                 int          beginHour,
                 int          beginMinute,
                 int          endHour,
                 int          endMinute,
                 String       customText,
                 boolean      testCreatedArchives,
                 boolean      noStorage,
                 boolean      enabled,
                 long         lastExecutedDateTime,
                 long         totalEntities,
                 long         totalEntryCount,
                 long         totalEntrySize
                )
    {
      this.uuid                 = uuid;
      this.year                 = year;
      this.month                = month;
      this.day                  = day;
      this.weekDays             = weekDays;
      this.hour                 = hour;
      this.minute               = minute;
      this.archiveType          = archiveType;
      this.interval             = interval;
      this.beginHour            = beginHour;
      this.beginMinute          = beginMinute;
      this.endHour              = endHour;
      this.endMinute            = endMinute;
      this.customText           = customText;
      this.testCreatedArchives  = testCreatedArchives;
      this.noStorage            = noStorage;
      this.enabled              = enabled;
      this.lastExecutedDateTime = lastExecutedDateTime;
      this.totalEntities        = totalEntities;
      this.totalEntryCount      = totalEntryCount;
      this.totalEntrySize       = totalEntrySize;
    }

    /** create schedule data
     */
    ScheduleData()
    {
      this(null,
           ScheduleData.ANY,
           ScheduleData.ANY,
           ScheduleData.ANY,
           ScheduleData.ANY,
           ScheduleData.ANY,
           ScheduleData.ANY,
           ArchiveTypes.NORMAL,
           0,
           ScheduleData.ANY,
           ScheduleData.ANY,
           ScheduleData.ANY,
           ScheduleData.ANY,
           "",
           false,
           false,
           true,
           0,
           0,
           0,
           0
          );
    }

    /** create schedule data
     * @param uuid schedule UUID
     * @param date date string (<year>-<month>-<day>)
     * @param weekDays week days string; values separated by ','
     * @param time time string (<hour>:<minute>)
     * @param archiveType archive type string
     * @param interval continuous interval [min]
     * @param beginTime,endTime continous begin/end time
     * @param customText custom text
     * @param testCreatedArchives true to test created archives
     * @param noStorage true to skip storage
     * @param enabled true iff enabled
     * @param lastExecutedDateTime date/time of last execution
     * @param totalEntities total number of existing entities for schedule
     * @param totalEntryCount total number of existing entries for schedule
     * @param totalEntrySize total size of existing entries for schedule [bytes]
     */
    ScheduleData(String       uuid,
                 String       date,
                 String       weekDays,
                 String       time,
                 ArchiveTypes archiveType,
                 int          interval,
                 String       beginTime,
                 String       endTime,
                 String       customText,
                 boolean      testCreatedArchives,
                 boolean      noStorage,
                 boolean      enabled,
                 long         lastExecutedDateTime,
                 long         totalEntities,
                 long         totalEntryCount,
                 long         totalEntrySize
                )
    {
      this.uuid                 = uuid;
      setDate(date);
      setWeekDays(weekDays);
      setTime(time);
      this.archiveType          = archiveType;
      this.interval             = interval;
      setBeginTime(beginTime);
      setEndTime(endTime);
      this.customText           = customText;
      this.testCreatedArchives  = testCreatedArchives;
      this.noStorage            = noStorage;
      this.enabled              = enabled;
      this.lastExecutedDateTime = lastExecutedDateTime;
      this.totalEntities        = totalEntities;
      this.totalEntryCount      = totalEntryCount;
      this.totalEntrySize       = totalEntrySize;
    }

    /** create schedule data
     * @param uuid schedule UUID
     * @param date date string (<year>-<month>-<day>)
     * @param weekDays week days string; values separated by ','
     * @param time time string (<hour>:<minute>)
     * @param archiveType archive type string
     */
    ScheduleData(String       uuid,
                 String       date,
                 String       weekDays,
                 String       time,
                 ArchiveTypes archiveType
                )
    {
      this(uuid,
           date,
           weekDays,
           time,
           archiveType,
           0,
           "",
           "",
           "",
           false,
           false,
           true,
           0,
           0,
           0,
           0
          );
    }

    /** clone schedule data
     * @return cloned object
     */
    public ScheduleData clone()
    {
      return new ScheduleData(uuid,
                              year,
                              month,
                              day,
                              weekDays,
                              hour,
                              minute,
                              archiveType,
                              interval,
                              beginHour,
                              beginMinute,
                              endHour,
                              endMinute,
                              customText,
                              testCreatedArchives,
                              noStorage,
                              enabled,
                              lastExecutedDateTime,
                              totalEntities,
                              totalEntryCount,
                              totalEntrySize
                             );
    }

    /** compare with other schedule data
     * @return -1/0+1 iff lower/equals/greater
     */
    @Override
    public int compareTo(ScheduleData other)
    {
      return uuid.compareTo(other.uuid);
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

    /** set date
     * @param year year value
     * @param month month value
     * @param day day value
     */
    private void setDate(String year, String month, String day)
    {
      this.year  = !year.equals ("*") ? Integer.parseInt(year ) : ANY;
      this.month = !month.equals("*") ? Integer.parseInt(month) : ANY;
      this.day   = !day.equals  ("*") ? Integer.parseInt(day  ) : ANY;
      assert (this.year == ANY) || (this.year >= 0) : this.hour;
      assert (this.month == ANY) || ((this.month >= 1) && (this.month <= 12)) : this.hour;
      assert (this.day == ANY) || ((this.day >= 1) && (this.day <= 31)) : this.hour;
    }

    /** set date
     * @param date date string
     */
    private void setDate(String date)
    {
      String[] parts = date.split("-");
      setDate(parts[0],parts[1],parts[2]);
    }

    /** get year value
     * @return year string
     */
    String getYear()
    {
      assert (year == ANY) || (year >= 1) : year;

      return (year != ANY) ? String.format("%04d",year) : "*";
    }

    /** get month value
     * @return month string
     */
    String getMonth()
    {
      assert (month == ANY) || ((month >= 1) && (month <= 12)) : month;

      return (month != ANY) ? String.format("%02d",month) : "*";
    }

    /** get day value
     * @return day string
     */
    String getDay()
    {
      assert (day == ANY) || ((day >= 1) && (day <= 31)) : day;

      return (day != ANY) ? String.format("%02d",day) : "*";
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
                ) : weekDays;

      if (weekDays == ANY)
      {
        return "*";
      }
      else
      {
        StringBuilder buffer = new StringBuilder();

        if ((weekDays & (1 << ScheduleData.MON)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Mon")); }
        if ((weekDays & (1 << ScheduleData.TUE)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Tue")); }
        if ((weekDays & (1 << ScheduleData.WED)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Wed")); }
        if ((weekDays & (1 << ScheduleData.THU)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Thu")); }
        if ((weekDays & (1 << ScheduleData.FRI)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Fri")); }
        if ((weekDays & (1 << ScheduleData.SAT)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Sat")); }
        if ((weekDays & (1 << ScheduleData.SUN)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Sun")); }

        return buffer.toString();
      }
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
        this.weekDays = ScheduleData.NONE;
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
     * @param sunFlag true for Sunday
     */
    void setWeekDays(boolean monFlag,
                     boolean tueFlag,
                     boolean wedFlag,
                     boolean thuFlag,
                     boolean friFlag,
                     boolean satFlag,
                     boolean sunFlag
                    )
    {

      if (   monFlag
          && tueFlag
          && wedFlag
          && thuFlag
          && friFlag
          && satFlag
          && sunFlag
         )
      {
        this.weekDays = ScheduleData.ANY;
      }
      else
      {
        this.weekDays = ScheduleData.NONE;
        if (monFlag) this.weekDays |= (1 << ScheduleData.MON);
        if (tueFlag) this.weekDays |= (1 << ScheduleData.TUE);
        if (wedFlag) this.weekDays |= (1 << ScheduleData.WED);
        if (thuFlag) this.weekDays |= (1 << ScheduleData.THU);
        if (friFlag) this.weekDays |= (1 << ScheduleData.FRI);
        if (satFlag) this.weekDays |= (1 << ScheduleData.SAT);
        if (sunFlag) this.weekDays |= (1 << ScheduleData.SUN);
      }
    }

    /** get begin hour value
     * @return begin hour string
     */
    String getBeginHour()
    {
      assert (beginHour == ANY) || ((beginHour >= 0) && (beginHour <= 23)) : beginHour;

      return (beginHour != ANY) ? String.format("%02d",beginHour) : "*";
    }

    /** get hour value
     * @return hour string
     */
    String getHour()
    {
      assert (hour == ANY) || ((hour >= 0) && (hour <= 23)) : hour;

      return (hour != ANY) ? String.format("%02d",hour) : "*";
    }

    /** get minute value
     * @return minute string
     */
    String getMinute()
    {
      assert (minute == ANY) || ((minute >= 0) && (minute <= 59)) : minute;

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

    /** set time
     * @param hour hour value
     * @param minute minute value
     */
    void setTime(String hour, String minute)
    {
      this.hour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.minute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
      assert (this.hour == ANY) || ((this.hour >= 0) && (this.hour <= 23)) : this.hour;
      assert (this.minute == ANY) || ((this.minute >= 0) && (this.minute <= 59)) : this.minute;
    }

    /** set time
     * @param time time string
     */
    void setTime(String time)
    {
      String[] parts = time.split(":");
      setTime(parts[0],parts[1]);
    }

    /** get archive type
     * @return archive type
     */
    ArchiveTypes getArchiveType()
    {
      return archiveType;
    }

    /** get begin minute value
     * @return begin minute string
     */
    String getBeginMinute()
    {
      assert (endMinute == ANY) || ((endMinute >= 0) && (endMinute <= 59)) : endMinute;

      return (endMinute != ANY) ? String.format("%02d",endMinute) : "*";
    }

    /** get begin time value
     * @return begin time string
     */
    String getBeginTime()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getBeginHour());
      buffer.append(':');
      buffer.append(getBeginMinute());

      return buffer.toString();
    }

    /** set begin time
     * @param hour hour value
     * @param minute minute value
     */
    void setBeginTime(String hour, String minute)
    {
      this.beginHour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.beginMinute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
      assert (beginHour == ANY) || ((beginHour >= 0) && (beginHour <= 23)) : beginHour;
      assert (beginMinute == ANY) || ((beginMinute >= 0) && (beginMinute <= 59)) : beginMinute;
    }

    /** set beginn time
     * @param time time string
     */
    void setBeginTime(String time)
    {
      String[] parts = time.split(":");
      setBeginTime(parts[0],parts[1]);
    }

    /** get end hour value
     * @return end hour string
     */
    String getEndHour()
    {
      assert (endHour == ANY) || ((endHour >= 0) && (endHour <= 23)) : endHour;

      return (endHour != ANY) ? String.format("%02d",endHour) : "*";
    }

    /** get end minute value
     * @return end minute string
     */
    String getEndMinute()
    {
      assert (endMinute == ANY) || ((endMinute >= 0) && (endMinute <= 59)) : endMinute;

      return (endMinute != ANY) ? String.format("%02d",endMinute) : "*";
    }

    /** get end time value
     * @return end time string
     */
    String getEndTime()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getEndHour());
      buffer.append(':');
      buffer.append(getEndMinute());

      return buffer.toString();
    }

    /** set end time
     * @param hour hour value
     * @param minute minute value
     */
    void setEndTime(String hour, String minute)
    {
      this.endHour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.endMinute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
      assert (endHour == ANY) || ((endHour >= 0) && (endHour <= 23)) : endHour;
      assert (endMinute == ANY) || ((endMinute >= 0) && (endMinute <= 59)) : endMinute;
    }

    /** set end time
     * @param time time string
     */
    void setEndTime(String time)
    {
      String[] parts = time.split(":");
      setEndTime(parts[0],parts[1]);
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

    /** check if no-storage option set
     * @return TRUE iff no-storage option is set
    */
    boolean isNoStorage()
    {
      return noStorage;
    }

    /** check if enabled
     * @return TRUE iff enabled
     */
    boolean isEnabled()
    {
      return enabled;
    }

    /** convert week days to string
     * @return week days string
     */
    String weekDaysToString()
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
                ) : weekDays;

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

    /** convert data to string
     */
    public String toString()
    {
      return "ScheduleData {"+uuid+", "+getDate()+", "+getWeekDays()+", "+getTime()+", "+archiveType+", "+noStorage+", "+enabled+"}";
    }

    /** get valid string
     * @param string string
     * @param validStrings valid strings
     * @param defaultString default string
     * @return valid string or default string
     */
    private String getValidString(String string, String[] validStrings, String defaultString)
    {
      for (String validString : validStrings)
      {
        if (validString.equals(string)) return validString;
      }

      return defaultString;
    }
  }

  /** schedule data comparator
   */
  static class ScheduleDataComparator implements Comparator<ScheduleData>
  {
    // sort modes
    enum SortModes
    {
      DATE,
      WEEKDAY,
      TIME,
      ARCHIVE_TYPE,
      BEGIN_TIME,
      END_TIME,
      CUSTOM_TEXT,
      TEST_CREATED_ARCHIVES,
      NO_STORAGE,
      ENABLED
    };

    private SortModes sortMode;

    /** create schedule data comparator
     * @param table schedule table
     * @param sortColumn sorting column
     */
    ScheduleDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SortModes.DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SortModes.WEEKDAY;
      else if (table.getColumn(2) == sortColumn) sortMode = SortModes.TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SortModes.ARCHIVE_TYPE;
      else if (table.getColumn(4) == sortColumn) sortMode = SortModes.BEGIN_TIME;
      else if (table.getColumn(5) == sortColumn) sortMode = SortModes.END_TIME;
      else if (table.getColumn(6) == sortColumn) sortMode = SortModes.CUSTOM_TEXT;
      else if (table.getColumn(7) == sortColumn) sortMode = SortModes.ENABLED;
      else                                       sortMode = SortModes.DATE;
    }

    /** create schedule data comparator
     * @param table schedule table
     */
    ScheduleDataComparator(Table table)
    {
      TableColumn sortColumn = table.getSortColumn();

      if      (table.getColumn(0) == sortColumn) sortMode = SortModes.DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SortModes.WEEKDAY;
      else if (table.getColumn(2) == sortColumn) sortMode = SortModes.TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SortModes.ARCHIVE_TYPE;
      else if (table.getColumn(4) == sortColumn) sortMode = SortModes.BEGIN_TIME;
      else if (table.getColumn(5) == sortColumn) sortMode = SortModes.END_TIME;
      else if (table.getColumn(6) == sortColumn) sortMode = SortModes.CUSTOM_TEXT;
      else if (table.getColumn(7) == sortColumn) sortMode = SortModes.ENABLED;
      else                                       sortMode = SortModes.DATE;
    }

    /** compare schedule data
     * @param scheduleData1, scheduleData2 data to compare
     * @return -1 iff scheduleData1 < scheduleData2,
                0 iff scheduleData1 = scheduleData2,
                1 iff scheduleData1 > scheduleData2
     */
    @Override
    public int compare(ScheduleData scheduleData1, ScheduleData scheduleData2)
    {
      EnumSet<SortModes> sortModeSet = EnumSet.allOf(SortModes.class);

      // primary sort key
      int result = compare(sortMode,scheduleData1,scheduleData2);
      sortModeSet.remove(sortMode);

      // other sort keys
      for (SortModes nextSortMode : sortModeSet)
      {
        if (result == 0)
        {
          result = compare(nextSortMode,scheduleData1,scheduleData2);
        }
        if (result != 0)
        {
          break;
        }
      }

      return result;
    }

    /** get index of week day
     * @param weekDay week day
     * @return index
     */
    private int indexOfWeekDay(String weekDay)
    {
      final String[] WEEK_DAYS = new String[]{"mon","tue","wed","thu","fri","sat","sun"};

      int index = 0;
      while ((index < WEEK_DAYS.length) && !WEEK_DAYS[index].equals(weekDay))
      {
        index++;
      }

      return index;
    }

    /** compare schedule data
     * @param sortMode sort mode
     * @param scheduleData1, scheduleData2 data to compare
     * @return -1 iff scheduleData1 < scheduleData2,
                0 iff scheduleData1 = scheduleData2,
                1 iff scheduleData1 > scheduleData2
     */
    private int compare(SortModes sortMode, ScheduleData scheduleData1, ScheduleData scheduleData2)
    {
      int result = 0;

      switch (sortMode)
      {
        case DATE:
          String date1 = scheduleData1.getDate();
          String date2 = scheduleData2.getDate();

          result = date1.compareTo(date2);
          break;
        case WEEKDAY:
          if      (scheduleData1.weekDays < scheduleData2.weekDays) result = -1;
          else if (scheduleData1.weekDays > scheduleData2.weekDays) result =  1;
          else                                                      result =  0;
          break;
        case TIME:
          String time1 = scheduleData1.getTime();
          String time2 = scheduleData2.getTime();

          result = time1.compareTo(time2);
          break;
        case ARCHIVE_TYPE:
          result = scheduleData1.archiveType.compareTo(scheduleData2.archiveType);
          break;
        case BEGIN_TIME:
          String beginTime1 = scheduleData1.getBeginTime();
          String beginTime2 = scheduleData2.getBeginTime();

          result = beginTime1.compareTo(beginTime2);
          break;
        case END_TIME:
          String endTime1 = scheduleData1.getEndTime();
          String endTime2 = scheduleData2.getEndTime();

          result = endTime1.compareTo(endTime2);
          break;
        case CUSTOM_TEXT:
          result = scheduleData1.customText.compareTo(scheduleData2.customText);
          break;
        case ENABLED:
          if      (scheduleData1.enabled && !scheduleData2.enabled) result = -1;
          else if (!scheduleData1.enabled && scheduleData2.enabled) result =  1;
          else                                                      result =  0;
          break;
        default:
          break;
      }

      return result;
    }
  }

  /** persistence data
   */
  class PersistenceData implements Cloneable, Comparable
  {
    int          id;
    ArchiveTypes archiveType;
    int          minKeep,maxKeep;
    int          maxAge;
    URIParts     moveTo;

    /** create persistence data
     * @param id id or 0
     * @param archiveType archive type string
     * @param minKeep min. number of archives to keep
     * @param maxKeep max. number of archives to keep
     * @param maxAge max. age to keep archives [days]
     * @param moveTo move-to URI or "
     */
    PersistenceData(int          id,
                    ArchiveTypes archiveType,
                    int          minKeep,
                    int          maxKeep,
                    int          maxAge,
                    URIParts     moveTo
                   )
    {
      this.id          = id;
      this.archiveType = archiveType;
      this.minKeep     = minKeep;
      this.maxKeep     = maxKeep;
      this.maxAge      = maxAge;
      this.moveTo      = moveTo;
    }

    /** create persistence data
     * @param id id or 0
     * @param archiveType archive type string
     * @param minKeep min. number of archives to keep
     * @param maxKeep max. number of archives to keep
     * @param maxAge max. age to keep archives [days]
     * @param moveTo move-to URI or ""
     */
    PersistenceData(int          id,
                    ArchiveTypes archiveType,
                    int          minKeep,
                    int          maxKeep,
                    int          maxAge,
                    String       moveTo
                   )
    {
      this(id,archiveType,minKeep,maxKeep,maxAge,new URIParts(moveTo));
    }

    /** create persistence data
     * @param archiveType archive type string
     * @param minKeep min. number of archives to keep
     * @param maxKeep max. number of archives to keep
     * @param maxAge max. age to keep archives [days]
     * @param moveTo move-to URI
     */
    PersistenceData(ArchiveTypes archiveType,
                    int          minKeep,
                    int          maxKeep,
                    int          maxAge,
                    URIParts     moveTo
                   )
    {
      this(0,archiveType,minKeep,maxKeep,maxAge,moveTo);
    }

    /** create persistence data
     * @param archiveType archive type string
     * @param minKeep min. number of archives to keep
     * @param maxKeep max. number of archives to keep
     * @param maxAge max. age to keep archives [days]
     * @param moveTo move-to URI or ""
     */
    PersistenceData(ArchiveTypes archiveType,
                    int          minKeep,
                    int          maxKeep,
                    int          maxAge,
                    String       moveTo
                   )
    {
      this(archiveType,minKeep,maxKeep,maxAge,new URIParts(moveTo));
    }

    /** create persistence data
     */
    PersistenceData()
    {
      this(ArchiveTypes.NORMAL,0,Keep.ALL,Age.FOREVER,"");
    }

    /** clone persistence data object
     * @return cloned object
     */
    public PersistenceData clone()
    {
      return new PersistenceData(archiveType,
                                 minKeep,
                                 maxKeep,
                                 maxAge,
                                 (URIParts)moveTo.clone()
                                );
    }

    /** get archive type
     * @return archive type
     */
    ArchiveTypes getArchiveType()
    {
      return archiveType;
    }

    /** get min. number of archives to keep
     * @return min. number of archives to keep
     */
    int getMinKeep()
    {
      return minKeep;
    }

    /** set min. number of archives to keep
     * @param minKeep min. number of archives to keep
     */
    void setMinKeep(int minKeep)
    {
      this.minKeep = minKeep;
    }

    /** get max. number of archives to keep
     * @return max. number of archives to keep
     */
    int getMaxKeep()
    {
      return minKeep;
    }

    /** set max. number of archives to keep
     * @param maxKeep max. number of archives to keep
     */
    void setMaxKeep(int maxKeep)
    {
      this.maxKeep = maxKeep;
    }

    /** get max. age to keep archives
     * @return number of days to keep archives
     */
    int getMaxAge()
    {
      return maxAge;
    }

    /** set max. age to keep archives
     * @param maxAge max. number of days to keep archives
     */
    void setMaxAge(int maxAge)
    {
      this.maxAge = maxAge;
    }

    /** get move-to URI
     * @return move-to URI
     */
    URIParts getMoveTo()
    {
      return moveTo;
    }

    /** set move-to URI
     * @param moveTo move-to URI
     */
    void setMoveTo(URIParts moveTo)
    {
      this.moveTo = moveTo;
    }

    /** check if index data equals
     * @param object index data
     * @return true iff equals
     */
    @Override
    public boolean equals(Object object)
    {
      PersistenceData persistenceData = (PersistenceData)object;

      return (persistenceData != null) && (id == persistenceData.id);
    }

    /** compare index data
     * @param object index data
     * @return -1/0/1 if less/equals/greater
     */
    @Override
    public int compareTo(Object object)
    {
      PersistenceData persistenceData = (PersistenceData)object;
      int             result;

      result = archiveType.compareTo(persistenceData.archiveType);
      if (result == 0)
      {
        if      ((maxAge != Age.FOREVER) && (persistenceData.maxAge != Age.FOREVER))
        {
          if      (maxAge < persistenceData.maxAge) return -1;
          else if (maxAge > persistenceData.maxAge) return  1;
          else                                      return  0;
        }
        else if (maxAge                 != Age.FOREVER) return -1;
        else if (persistenceData.maxAge != Age.FOREVER) return  1;
      }

      return result;
    }

    /** convert data to string
     */
    public String toString()
    {
      return "PersistenceData { archiveType="+archiveType+", minKeep="+minKeep+", maxKeep="+maxKeep+", maxAge="+maxAge+"}";
    }
  }

  /** persistence data comparator
   */
  static class PersistenceDataComparator implements Comparator<PersistenceData>
  {
    // sort modes
    enum SortModes
    {
      ARCHIVE_TYPE,
      MIN_KEEP,
      MAX_KEEP,
      MAX_AGE
    };

    private SortModes sortMode;

    /** create persistence data comparator
     * @param tree persistence tree
     * @param sortColumn sorting column
     */
    PersistenceDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SortModes.ARCHIVE_TYPE;
      else if (tree.getColumn(1) == sortColumn) sortMode = SortModes.MIN_KEEP;
      else if (tree.getColumn(2) == sortColumn) sortMode = SortModes.MAX_KEEP;
      else if (tree.getColumn(3) == sortColumn) sortMode = SortModes.MAX_AGE;
      else                                      sortMode = SortModes.MAX_AGE;
    }

    /** create persistence data comparator
     * @param tree persistence tree
     */
    PersistenceDataComparator(Tree tree)
    {
      TreeColumn sortColumn = tree.getSortColumn();

      if      (tree.getColumn(0) == sortColumn) sortMode = SortModes.ARCHIVE_TYPE;
      else if (tree.getColumn(1) == sortColumn) sortMode = SortModes.MIN_KEEP;
      else if (tree.getColumn(2) == sortColumn) sortMode = SortModes.MAX_KEEP;
      else if (tree.getColumn(3) == sortColumn) sortMode = SortModes.MAX_AGE;
      else                                      sortMode = SortModes.MAX_AGE;
    }

    /** create persistence data comparator
     * @param sortMode sort mode
     */
    PersistenceDataComparator(SortModes sortMode)
    {
      this.sortMode = sortMode;
    }

    /** compare persistence data
     * @param persistenceData1, persistenceData2 data to compare
     * @return -1 iff persistenceData1 < persistenceData2,
                0 iff persistenceData1 = persistenceData2,
                1 iff persistenceData1 > persistenceData2
     */
    @Override
    public int compare(PersistenceData persistenceData1, PersistenceData persistenceData2)
    {
      EnumSet<SortModes> sortModeSet = EnumSet.allOf(SortModes.class);

      // primary sort key
      int result = compare(sortMode,persistenceData1,persistenceData2);
      sortModeSet.remove(sortMode);

      // other sort keys
      for (SortModes nextSortMode : sortModeSet)
      {
        if (result == 0)
        {
          result = compare(nextSortMode,persistenceData1,persistenceData2);
        }
        if (result != 0)
        {
          break;
        }
      }

      return result;
    }

    /** compare persistence data
     * @param sortMode sort mode
     * @param persistenceData1, persistenceData2 data to compare
     * @return -1 iff persistenceData1 < persistenceData2,
                0 iff persistenceData1 = persistenceData2,
                1 iff persistenceData1 > persistenceData2
     */
    private int compare(SortModes sortMode, PersistenceData persistenceData1, PersistenceData persistenceData2)
    {
      int result = 0;

      switch (sortMode)
      {
        case ARCHIVE_TYPE:
          result = persistenceData1.archiveType.compareTo(persistenceData2.archiveType);
          break;
        case MIN_KEEP:
          if      ((persistenceData1.minKeep != Keep.ALL) && (persistenceData2.minKeep != Keep.ALL))
          {
            if      (persistenceData1.minKeep < persistenceData2.minKeep) result = -1;
            else if (persistenceData1.minKeep > persistenceData2.minKeep) result =  1;
          }
          else if (persistenceData1.minKeep != Keep.ALL) result = 1;
          else if (persistenceData2.minKeep != Keep.ALL) result = -1;
          break;
        case MAX_KEEP:
          if      ((persistenceData1.maxKeep != Keep.ALL) && (persistenceData2.maxKeep != Keep.ALL))
          {
            if      (persistenceData1.maxKeep < persistenceData2.maxKeep) result = -1;
            else if (persistenceData1.maxKeep > persistenceData2.maxKeep) result =  1;
          }
          else if (persistenceData1.maxKeep != Keep.ALL) result =  1;
          else if (persistenceData2.maxKeep != Keep.ALL) result = -1;
          break;
        case MAX_AGE:
          if      ((persistenceData1.maxAge != Age.FOREVER) && (persistenceData2.maxAge != Age.FOREVER))
          {
            if      (persistenceData1.maxAge < persistenceData2.maxAge) result = -1;
            else if (persistenceData1.maxAge > persistenceData2.maxAge) result =  1;
          }
          else if (persistenceData1.maxAge != Age.FOREVER) result = -1;
          else if (persistenceData2.maxAge != Age.FOREVER) result =  1;
          break;
      }

      return result;
    }
  }

  class EntityIndexData implements Comparable<EntityIndexData>
  {
    public long    id;
    public String  scheduleUUID;
    public long    createdDateTime;
    public long    totalSize;
    public long    totalEntryCount;
    public long    totalEntrySize;
    public boolean inTransit;

    /** create entity index data index
     * @param indexId index id
     * @param scheduleUUID schedule UUID
     * @param createdDateTime create date/time (timestamp)
     * @param totalSize total size of enity [byte]
     * @param totalEntryCount total number of entries
     * @param totalEntrySize total sum of size of entries
     * @param inTransit true if in-transit to next persistence periode
     */
    EntityIndexData(long         indexId,
                    String       scheduleUUID,
                    long         createdDateTime,
                    long         totalSize,
                    long         totalEntryCount,
                    long         totalEntrySize,
                    boolean      inTransit
                   )
    {
      assert (indexId & 0x0000000F) == 2 : indexId;

      this.id              = indexId;
      this.scheduleUUID    = scheduleUUID;
      this.createdDateTime = createdDateTime;
      this.totalSize       = totalSize;
      this.totalEntryCount = totalEntryCount;
      this.totalEntrySize  = totalEntrySize;
      this.inTransit       = inTransit;
    }

    /** compare index data
     * @param object index data
     * @return -1/0/1 if less/equals/greater
     */
    @Override
    public int compareTo(EntityIndexData entityIndexData)
    {
      int result;

      if (id == entityIndexData.id)
      {
        result = 0;
      }
      else
      {
        if      (createdDateTime > entityIndexData.createdDateTime) result = -1;
        else if (createdDateTime < entityIndexData.createdDateTime) result =  1;
        else                                                        result =  0;
      }

      return result;
    }

    /** get name
     * @return name
     */
    public String getName()
    {
      return archiveType.toString();
    }

    /** get date/time
     * @return date/time [s]
     */
    public long getDateTime()
    {
      return createdDateTime;
    }

    /** get total size of entity
     * @return total size [bytes]
     */
    public long getTotalSize()
    {
      return totalSize;
    }

    /** get total number of entries
     * @return entries
     */
    public long getTotalEntryCount()
    {
      return totalEntryCount;
    }

    /** get total size of entries
     * @return size of entries [bytes]
     */
    public long getTotalEntrySize()
    {
      return totalEntrySize;
    }

    /** check if index data equals
     * @param object index data
     * @return true iff equals
     */
    @Override
    public boolean equals(Object object)
    {
      EntityIndexData entityIndexData = (EntityIndexData)object;
      int             result;

      return (entityIndexData != null) && (id == entityIndexData.id);
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      return "EntityIndexData {id="+id+", createdDateTime="+createdDateTime+"}";
    }
  }

  // max. size of medium data with ECC [%]
  private final double MAX_VOLUME_SIZE_ECC = 0.8;

  // colors
  private final Color  COLOR_BLACK;
  private final Color  COLOR_WHITE;
  private final Color  COLOR_RED;
  private final Color  COLOR_MODIFIED;
  private final Color  COLOR_INFO_FOREGROUND;
  private final Color  COLOR_INFO_BACKGROUND;
  private final Color  COLOR_DISABLED_BACKGROUND;

  private final Color  COLOR_BACKGROUND_ODD;
  private final Color  COLOR_BACKGROUND_EVEN;
  private final Color  COLOR_EXPIRED;
  private final Color  COLOR_IN_TRANSIT;

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
  private final SimpleDateFormat SIMPLE_DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  // global variable references
  private Shell                        shell;
  private Display                      display;
  private UpdateThread                 updateThread;
  private TabStatus                    tabStatus;

  // widgets
  public  Composite                    widgetTab;
  private TabFolder                    widgetTabFolder;
  private Combo                        widgetJobList;
  private Tree                         widgetFileTree;
  private Shell                        widgetFileTreeToolTip = null;
  private MenuItem                     menuItemOpenClose;
  private MenuItem                     menuItemInclude;
  private MenuItem                     menuItemExcludeByList;
  private MenuItem                     menuItemExcludeByNoBackup;
  private MenuItem                     menuItemExcludeByNoDump;
  private MenuItem                     menuItemNone;
  private Button                       widgetInclude;
  private Button                       widgetExclude;
  private Button                       widgetNone;
  private Table                        widgetDeviceTable;
  private Table                        widgetMountTable;
  private Button                       widgetMountTableAdd,widgetMountTableEdit,widgetMountTableRemove;
  private Table                        widgetIncludeTable;
  private Button                       widgetIncludeTableAdd,widgetIncludeTableEdit,widgetIncludeTableRemove;
  private List                         widgetExcludeList;
  private Button                       widgetExcludeListAdd,widgetExcludeListEdit,widgetExcludeListRemove;
  private Button                       widgetArchivePartSizeLimited;
  private Combo                        widgetArchivePartSize;
  private List                         widgetCompressExcludeList;
  private Button                       widgetCompressExcludeListInsert,widgetCompressExcludeListEdit,widgetCompressExcludeListRemove;
  private Combo[]                      widgetCryptAlgorithms = new Combo[4];
  private Text                         widgetCryptPassword1,widgetCryptPassword2;
  private Combo                        widgetFTPMaxBandWidth;
  private Combo                        widgetSCPSFTPMaxBandWidth;
  private Combo                        widgetWebdavMaxBandWidth;
  private Table                        widgetScheduleTable;
  private Shell                        widgetScheduleTableToolTip = null;
  private Button                       widgetScheduleTableAdd,widgetScheduleTableEdit,widgetScheduleTableRemove;
  private Tree                         widgetPersistenceTree;
  private Shell                        widgetPersistenceTreeToolTip = null;
  private Button                       widgetPersistenceTreeAdd,widgetPersistenceTreeEdit,widgetPersistenceTreeRemove;

  // BAR variables
  private WidgetVariable               slaveHostName             = new WidgetVariable<String>            ("slave-host-name","");
  private WidgetVariable               slaveHostPort             = new WidgetVariable<Integer>           ("slave-host-port",0);
// TODO: string -> BARServer.TLSModes
  private WidgetVariable               slaveTLSMode              = new WidgetVariable<String>            ("slave-tls-mode","none");
  private WidgetVariable               includeFileCommand        = new WidgetVariable<String>            ("include-file-command","");
  private WidgetVariable               includeImageCommand       = new WidgetVariable<String>            ("include-image-command","");
  private WidgetVariable               excludeCommand            = new WidgetVariable<String>            ("exclude-command","");
  private WidgetVariable               archiveName               = new WidgetVariable<String>            ("archive-name","");
  private WidgetVariable               archiveType               = new WidgetVariable<String>            ("archive-type",new String[]{"normal","full","incremental","differential","continuous"},"normal");
  private WidgetVariable               archivePartSizeFlag       = new WidgetVariable<Boolean>           (false);
  private WidgetVariable               archivePartSize           = new WidgetVariable<Long>              ("archive-part-size",0L);
  private WidgetVariable               deltaCompressAlgorithm    = new WidgetVariable<String>            ("delta-compress-algorithm",
                                                                                                          new String[]{"none",
                                                                                                                       "xdelta1","xdelta2","xdelta3","xdelta4","xdelta5","xdelta6","xdelta7","xdelta8","xdelta9"
                                                                                                                      },
                                                                                                          "none"
                                                                                                         );
  private WidgetVariable               deltaSource               = new WidgetVariable<String>            ("delta-source","");
  private WidgetVariable               byteCompressAlgorithmType = new WidgetVariable<String>            (new String[]{"none","zip","bzip","lzma","lzo","lz4-","zstd",},
                                                                                                          "none"
                                                                                                         );
  private WidgetVariable               byteCompressAlgorithm     = new WidgetVariable<String>            (new String[]{"none",
                                                                                                                       "zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9",
                                                                                                                       "bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9",
                                                                                                                       "lzma1","lzma2","lzma3","lzma4","lzma5","lzma6","lzma7","lzma8","lzma9",
                                                                                                                       "lzo1","lzo2","lzo3","lzo4","lzo5",
                                                                                                                       "lz4-0","lz4-1","lz4-2","lz4-3","lz4-4","lz4-5","lz4-6","lz4-7","lz4-8","lz4-9","lz4-10","lz4-11","lz4-12","lz4-13","lz4-14","lz4-15","lz4-16",
                                                                                                                       "zstd0", "zstd1", "zstd2", "zstd3", "zstd4", "zstd5", "zstd6", "zstd7", "zstd8", "zstd9", "zstd10", "zstd11", "zstd12", "zstd13", "zstd14", "zstd15", "zstd16", "zstd17", "zstd18", "zstd19"
                                                                                                                      },
                                                                                                          "none"
                                                                                                         );
  private WidgetVariable               compressMinSize           = new WidgetVariable<Long>              ("compress-min-size",0L);
  private WidgetVariable               cryptAlgorithm            = new WidgetVariable<String>            ("crypt-algorithm",
                                                                                                          new String[]{"none",
                                                                                                                       "3DES",
                                                                                                                       "CAST5",
                                                                                                                       "BLOWFISH",
                                                                                                                       "AES128",
                                                                                                                       "AES192",
                                                                                                                       "AES256",
                                                                                                                       "TWOFISH128",
                                                                                                                       "TWOFISH256",
                                                                                                                       "SERPENT128",
                                                                                                                       "SERPENT192",
                                                                                                                       "SERPENT256",
                                                                                                                       "CAMELLIA128",
                                                                                                                       "CAMELLIA192",
                                                                                                                       "CAMELLIA256"
                                                                                                                      },
                                                                                                          "none"
                                                                                                         );
  private WidgetVariable               cryptType                 = new WidgetVariable<String>            ("crypt-type",new String[]{"none","symmetric","asymmetric"},"none");
  private WidgetVariable               cryptPublicKeyFileName    = new WidgetVariable<String>            ("crypt-public-key","");
  private WidgetVariable               cryptPasswordMode         = new WidgetVariable<String>            ("crypt-password-mode",new String[]{"default","ask","config"},"default");
  private WidgetVariable               cryptPassword             = new WidgetVariable<String>            ("crypt-password","");
  private WidgetVariable               incrementalListFileName   = new WidgetVariable<String>            ("incremental-list-file","");
  private WidgetVariable               storageOnMasterFlag       = new WidgetVariable<Boolean>           ("storage-on-master",true);
  private WidgetVariable               storageType               = new WidgetVariable<Enum>              ("storage-type",
                                                                                                          new StorageTypes[]{StorageTypes.FILESYSTEM,
                                                                                                                             StorageTypes.FTP,
                                                                                                                             StorageTypes.SCP,
                                                                                                                             StorageTypes.SFTP,
                                                                                                                             StorageTypes.WEBDAV,
                                                                                                                             StorageTypes.WEBDAVS,
                                                                                                                             StorageTypes.SMB,
                                                                                                                             StorageTypes.CD,
                                                                                                                             StorageTypes.DVD,
                                                                                                                             StorageTypes.BD,
                                                                                                                             StorageTypes.DEVICE
                                                                                                                          },
                                                                                                          StorageTypes.FILESYSTEM
                                                                                                         );
  private WidgetVariable               storageHostName           = new WidgetVariable<String>            ("");
  private WidgetVariable               storageHostPort           = new WidgetVariable<Integer>           ("",0);
  private WidgetVariable               storageLoginName          = new WidgetVariable<String>            ("","");
  private WidgetVariable               storageLoginPassword      = new WidgetVariable<String>            ("","");
  private WidgetVariable               storageShareName          = new WidgetVariable<String>            ("","");
  private WidgetVariable               storageDeviceName         = new WidgetVariable<String>            ("","");
  private WidgetVariable               storageFileName           = new WidgetVariable<String>            ("","");
  private WidgetVariable               archiveFileMode           = new WidgetVariable<String>            ("archive-file-mode",
                                                                                                          new String[]
                                                                                                          {
                                                                                                            "stop",
                                                                                                            "rename",
                                                                                                            "append",
                                                                                                            "overwrite"
                                                                                                          },
                                                                                                          "stop"
                                                                                                         );
  private WidgetVariable               sshPublicKeyFileName      = new WidgetVariable<String>            ("ssh-public-key","");
  private WidgetVariable               sshPrivateKeyFileName     = new WidgetVariable<String>            ("ssh-private-key","");
  private WidgetVariable               maxBandWidthFlag          = new WidgetVariable<Boolean>           (false);
  private WidgetVariable               maxBandWidth              = new WidgetVariable<Long>              ("max-band-width",0L);
  private WidgetVariable               volumeSize                = new WidgetVariable<String>            ("volume-size","");
  private WidgetVariable               ecc                       = new WidgetVariable<Boolean>           ("ecc",false);
  private WidgetVariable               blank                     = new WidgetVariable<Boolean>           ("blank",false);
  private WidgetVariable               waitFirstVolume           = new WidgetVariable<Boolean>           ("wait-first-volume",false);
  private WidgetVariable               skipUnreadable            = new WidgetVariable<Boolean>           ("skip-unreadable",false);
  private WidgetVariable               noStopOnOwnerError        = new WidgetVariable<Boolean>           ("no-stop-on-owner-error",false);
  private WidgetVariable               noStopOnAttributeError    = new WidgetVariable<Boolean>           ("no-stop-on-attribute-error",false);
  private WidgetVariable               rawImages                 = new WidgetVariable<Boolean>           ("raw-images",false);
  private WidgetVariable               overwriteFiles            = new WidgetVariable<Boolean>           ("overwrite-files",false);
  private WidgetVariable               preCommand                = new WidgetVariable<String>            ("pre-command","");
  private WidgetVariable               postCommand               = new WidgetVariable<String>            ("post-command","");
  private WidgetVariable               slavePreCommand           = new WidgetVariable<String>            ("slave-pre-command","");
  private WidgetVariable               slavePostCommand          = new WidgetVariable<String>            ("slave-post-command","");
  private WidgetVariable               maxStorageSize            = new WidgetVariable<String>            ("max-storage-size","");
  private WidgetVariable               comment                   = new WidgetVariable<String>            ("comment","");

  // variables
  private DirectoryInfoThread          directoryInfoThread;
  private boolean                      directorySizesFlag        = false;
  private JobData                      selectedJobData           = null;
  private WidgetEvent                  selectJobEvent            = new WidgetEvent();
  private HashMap<String,EntryData>    includeHashMap            = new HashMap<String,EntryData>();
  private HashSet<String>              excludeHashSet            = new HashSet<String>();
  private HashSet<String>              sourceHashSet             = new HashSet<String>();
  private HashSet<String>              compressExcludeHashSet    = new HashSet<String>();
  private HashMap<String,ScheduleData> scheduleDataMap           = new HashMap<String,ScheduleData>();

  /** create jobs tab
   * @param parentTabFolder parent tab folder
   * @param accelerator keyboard shortcut to select tab
   */
  TabJobs(final TabFolder parentTabFolder, int accelerator)
  {
    TabFolder   tabFolder;
    Composite   tab,subTab;
    Menu        menu;
    MenuItem    menuItem;
    Group       group;
    Composite   composite,subComposite,subSubComposite;
    Label       label;
    Button      button;
    Combo       combo;
    Spinner     spinner;
    TreeItem    treeItem;
    TreeColumn  treeColumn;
    TableColumn tableColumn;
    Control     control;
    Text        text;
    StyledText  styledText;

    final Combo widgetByteCompressAlgorithmType;
    final Combo widgetByteCompressAlgorithmLevel;

    // get shell, display
    shell   = parentTabFolder.getShell();
    display = shell.getDisplay();

    // get colors
    COLOR_BLACK               = display.getSystemColor(SWT.COLOR_BLACK);
    COLOR_WHITE               = display.getSystemColor(SWT.COLOR_WHITE);
    COLOR_RED                 = display.getSystemColor(SWT.COLOR_RED);
    COLOR_MODIFIED            = new Color(null,0xFF,0xA0,0xA0);
    COLOR_INFO_FOREGROUND     = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
    COLOR_INFO_BACKGROUND     = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);
    COLOR_DISABLED_BACKGROUND = display.getSystemColor(SWT.COLOR_WIDGET_BACKGROUND);

    COLOR_BACKGROUND_ODD      = new Color(display,0xF8,0xF7,0xF6);
    COLOR_BACKGROUND_EVEN     = display.getSystemColor(SWT.COLOR_WHITE);
    COLOR_EXPIRED             = new Color(null,0xFF,0xA0,0xA0);
    COLOR_IN_TRANSIT          = new Color(null,0xA0,0xFF,0xA0);

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

    // start tree item size thread
    directoryInfoThread = new DirectoryInfoThread(display);
    directoryInfoThread.start();

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Jobs")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""),!BARServer.isSlave());
    widgetTab.setLayout(new TableLayout(new double[]{0.0,0.0,1.0,0.0},1.0,2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // job selector
    composite = Widgets.newComposite(widgetTab);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0,0.0,0.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobList = Widgets.newOptionMenu(composite);
      widgetJobList.setToolTipText(BARControl.tr("Existing job entries."));
      Widgets.setOptionMenuItems(widgetJobList,new Object[]{});
      Widgets.layout(widgetJobList,0,1,TableLayoutData.WE);
      widgetJobList.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo widget = (Combo)selectionEvent.widget;
          int   index  = widget.getSelectionIndex();
          if (index >= 0)
          {
            selectedJobData = Widgets.getSelectedOptionMenuItem(widgetJobList,null);
            Widgets.notify(shell,BARControl.USER_EVENT_SELECT_JOB,selectedJobData.uuid);
          }
        }
      });

      button = Widgets.newButton(composite,BARControl.tr("New")+"\u2026");
      button.setToolTipText(BARControl.tr("Create new job entry."));
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobNew();
        }
      });

      button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
      button.setToolTipText(BARControl.tr("Clone an existing job entry."));
      button.setEnabled(false);
      Widgets.layout(button,0,3,TableLayoutData.DEFAULT);
      Widgets.addEventListener(new WidgetEventListener(button,selectJobEvent)
      {
        @Override
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,selectedJobData != null);
        }
      });
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          if (selectedJobData != null)
          {
            jobClone();
          }
        }
      });

      button = Widgets.newButton(composite,BARControl.tr("Rename")+"\u2026",Settings.hasNormalRole());
      button.setToolTipText(BARControl.tr("Rename a job entry."));
      button.setEnabled(false);
      Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
      Widgets.addEventListener(new WidgetEventListener(button,selectJobEvent)
      {
        @Override
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,selectedJobData != null);
        }
      });
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          if (selectedJobData != null)
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
        @Override
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,selectedJobData != null);
        }
      });
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          if (selectedJobData != null)
          {
            jobDelete();
          }
        }
      });
    }

    composite = Widgets.newComposite(widgetTab,Settings.hasExpertRole());
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0,0.0}));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Slave")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      text = Widgets.newText(composite);
      text.setToolTipText(BARControl.tr("Hostname of slave to run job. Leave empty to run on host where BAR is executed."));
      text.setEnabled(false);
      Widgets.layout(text,0,1,TableLayoutData.WE);
      Widgets.addEventListener(new WidgetEventListener(text,selectJobEvent)
      {
        @Override
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,selectedJobData != null);
        }
      });
      text.addModifyListener(new ModifyListener()
      {
        @Override
        public void modifyText(ModifyEvent modifyEvent)
        {
          Text   widget = (Text)modifyEvent.widget;
          String string = widget.getText().trim();
          Color  color  = COLOR_MODIFIED;

          if (slaveHostName.getString().equals(string)) color = null;
          widget.setBackground(color);
        }
      });
      text.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text   widget = (Text)selectionEvent.widget;
          String string = widget.getText().trim();

          try
          {
            slaveHostName.set(string);
            BARServer.setJobOption(selectedJobData.uuid,slaveHostName);
            widget.setBackground(null);
          }
          catch (Exception exception)
          {
            // ignored
          }
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      text.addFocusListener(new FocusListener()
      {
        @Override
        public void focusGained(FocusEvent focusEvent)
        {
        }
        @Override
        public void focusLost(FocusEvent focusEvent)
        {
          Text   widget = (Text)focusEvent.widget;
          String string = widget.getText().trim();

          try
          {
            slaveHostName.set(string);
            BARServer.setJobOption(selectedJobData.uuid,slaveHostName);
            widget.setBackground(null);
          }
          catch (Exception exception)
          {
            // ignored
          }
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(text,slaveHostName));

      label = Widgets.newLabel(composite,BARControl.tr("Port")+":");
      Widgets.layout(label,0,2,TableLayoutData.W);

      spinner = Widgets.newSpinner(composite);
      spinner.setToolTipText(BARControl.tr("Port number. Set to 0 to use default port number from configuration file."));
      spinner.setMinimum(0);
      spinner.setMaximum(65535);
      spinner.setEnabled(false);
      Widgets.layout(spinner,0,3,TableLayoutData.W,0,0,0,0,80,SWT.DEFAULT);
      Widgets.addEventListener(new WidgetEventListener(spinner,selectJobEvent)
      {
        @Override
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,(selectedJobData != null) && !slaveHostName.getString().isEmpty());
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(spinner,slaveHostName)
      {
        @Override
        public void modified(Control control, WidgetVariable slaveHostName)
        {
          Widgets.setEnabled(control,!slaveHostName.getString().isEmpty());
        }
      });
      spinner.addModifyListener(new ModifyListener()
      {
        @Override
        public void modifyText(ModifyEvent modifyEvent)
        {
          Spinner widget = (Spinner)modifyEvent.widget;
          int     n      = widget.getSelection();
          Color   color  = COLOR_MODIFIED;

          if (slaveHostPort.getInteger() == n) color = null;
          widget.setBackground(color);
          widget.setData("showedErrorDialog",false);
        }
      });
      spinner.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Spinner widget = (Spinner)selectionEvent.widget;
          int     n      = widget.getSelection();

          try
          {
            slaveHostPort.set(n);
            BARServer.setJobOption(selectedJobData.uuid,slaveHostPort);
            widget.setBackground(null);
          }
          catch (Exception exception)
          {
            // ignored
          }
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Spinner widget = (Spinner)selectionEvent.widget;
          int     n      = widget.getSelection();

          try
          {
            slaveHostPort.set(n);
            BARServer.setJobOption(selectedJobData.uuid,slaveHostPort);
            widget.setBackground(null);
          }
          catch (Exception exception)
          {
            // ignored
          }
        }
      });
      spinner.addFocusListener(new FocusListener()
      {
        @Override
        public void focusGained(FocusEvent focusEvent)
        {
          Spinner widget = (Spinner)focusEvent.widget;
          widget.setData("showedErrorDialog",false);
        }
        @Override
        public void focusLost(FocusEvent focusEvent)
        {
          Spinner widget = (Spinner)focusEvent.widget;
          int     n      = widget.getSelection();

          try
          {
            slaveHostPort.set(n);
            BARServer.setJobOption(selectedJobData.uuid,slaveHostPort);
            widget.setBackground(null);
          }
          catch (Exception exception)
          {
            // ignored
          }
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(spinner,slaveHostPort));

      label = new Label(composite,SWT.LEFT);
      label.setText(BARControl.tr("TLS"));
      label.setLayoutData(new TableLayoutData(0,4,TableLayoutData.W));

      combo = Widgets.newOptionMenu(composite);
      Widgets.setOptionMenuItems(combo,new Object[]{BARControl.tr("none" ),BARServer.TLSModes.NONE,
                                                    BARControl.tr("try"  ),BARServer.TLSModes.TRY,
                                                    BARControl.tr("force"),BARServer.TLSModes.FORCE
                                                   }
                                );
      Widgets.setSelectedOptionMenuItem(combo,BARServer.TLSModes.FORCE);
      Widgets.layout(combo,0,5,TableLayoutData.W);
      Widgets.addEventListener(new WidgetEventListener(combo,selectJobEvent)
      {
        @Override
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,(selectedJobData != null) && !slaveHostName.getString().isEmpty());
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(combo,slaveHostName)
      {
        @Override
        public void modified(Control control, WidgetVariable slaveHostName)
        {
          Widgets.setEnabled(control,!slaveHostName.getString().isEmpty());
        }
      });
      combo.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo              widget = (Combo)selectionEvent.widget;
          BARServer.TLSModes tlsMode = Widgets.getSelectedComboItem(widget,BARServer.TLSModes.NONE);

          try
          {
            slaveTLSMode.set(tlsMode.toString());
            BARServer.setJobOption(selectedJobData.uuid,slaveTLSMode);
          }
          catch (Exception exception)
          {
            // ignored
          }
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(combo,slaveTLSMode));
    }

    // create sub-tabs
    widgetTabFolder = Widgets.newTabFolder(widgetTab);
    Widgets.setEnabled(widgetTabFolder,false);
    Widgets.layout(widgetTabFolder,2,0,TableLayoutData.NSWE);
    {
      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Entries"));
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // file tree
        widgetFileTree = Widgets.newTree(tab,SWT.MULTI);
        Widgets.layout(widgetFileTree,0,0,TableLayoutData.NSWE);
        SelectionListener fileTreeColumnSelectionListener = new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
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
        treeColumn.setToolTipText(BARControl.tr("Click to sort by name."));
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Type",    SWT.LEFT, 160,true);
        treeColumn.setToolTipText(BARControl.tr("Click to sort by type."));
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Size",    SWT.RIGHT,100,true);
        treeColumn.setToolTipText(BARControl.tr("Click to sort by size."));
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Modified",SWT.LEFT, 100,true);
        treeColumn.setToolTipText(BARControl.tr("Click to sort by modification time."));
        treeColumn.addSelectionListener(fileTreeColumnSelectionListener);

        widgetFileTree.addListener(SWT.Expand,new Listener()
        {
          @Override
          public void handleEvent(final Event event)
          {
            TreeItem treeItem = (TreeItem)event.item;
            updateFileTree(treeItem);
          }
        });
        widgetFileTree.addListener(SWT.Collapse,new Listener()
        {
          @Override
          public void handleEvent(final Event event)
          {
            TreeItem treeItem = (TreeItem)event.item;

            if (!treeItem.isDisposed())
            {
              treeItem.removeAll();
              new TreeItem(treeItem,SWT.NONE);
            }
          }
        });
        widgetFileTree.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TreeItem treeItem = (TreeItem)selectionEvent.item;

            if (treeItem != null)
            {
              FileTreeData fileTreeData = (FileTreeData)treeItem.getData();

              boolean isIncluded           = false;
              boolean isExcludedByList     = false;
              boolean isExcludedByNoDump   = false;
              boolean isExcludedByNoBackup = false;
              boolean isNone               = false;
              if      (isIncluded(fileTreeData.name) && !isExcluded(fileTreeData.name))
                isIncluded = true;
              else if (fileTreeData.noBackup)
                isExcludedByNoBackup = true;
              else if (fileTreeData.noDump)
                isExcludedByNoDump = true;
              else if (excludeHashSet.contains(fileTreeData.name))
                isExcludedByList = true;
              else
                isNone     = true;

              menuItemOpenClose.setEnabled(fileTreeData.fileType == BARServer.FileTypes.DIRECTORY);
              menuItemInclude.setSelection(isIncluded);
              menuItemExcludeByList.setSelection(isExcludedByList);
              menuItemExcludeByNoBackup.setSelection(isExcludedByNoBackup);
              menuItemExcludeByNoDump.setSelection(isExcludedByNoDump);
              menuItemNone.setSelection(isNone);

              widgetInclude.setEnabled(!isIncluded);
              widgetExclude.setEnabled(!isExcludedByList && !isExcludedByNoBackup && !isExcludedByNoDump);
              widgetNone.setEnabled(!isNone);
            }
          }
        });
        widgetFileTree.addMouseListener(new MouseListener()
        {
          @Override
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
            TreeItem treeItem = widgetFileTree.getItem(new Point(mouseEvent.x,mouseEvent.y));
            if (treeItem != null)
            {
              FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
              if (fileTreeData.fileType == BARServer.FileTypes.DIRECTORY)
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
          @Override
          public void mouseDown(final MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });
        widgetFileTree.addMouseTrackListener(new MouseTrackListener()
        {
          @Override
          public void mouseEnter(MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseExit(MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseHover(MouseEvent mouseEvent)
          {
            Tree tree = (Tree)mouseEvent.widget;

            if (widgetFileTreeToolTip != null)
            {
              widgetFileTreeToolTip.dispose();
              widgetFileTreeToolTip = null;
            }

            // show if mouse is in the right side
            if (mouseEvent.x > tree.getBounds().width/2)
            {
              Label label;

              final Color COLOR_FOREGROUND = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
              final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

              widgetFileTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
              widgetFileTreeToolTip.setBackground(COLOR_BACKGROUND);
              widgetFileTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
              Widgets.layout(widgetFileTreeToolTip,0,0,TableLayoutData.NSWE);
              label = Widgets.newLabel(widgetFileTreeToolTip,BARControl.tr("Tree representation of files, directories, links and special entries.\nDouble-click to open sub-directories, right-click to open context menu.\nNote size column: numbers in red color indicates size update is still in progress."));
              label.setForeground(COLOR_FOREGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,0,0,TableLayoutData.W);

              Point point = display.getCursorLocation();
              if (point.x > 16) point.x -= 16;
              if (point.y > 16) point.y -= 16;

              Widgets.showToolTip(widgetFileTreeToolTip,point.x,point.y);
            }
          }
        });
        widgetFileTree.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
            if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
            {
              TreeItem treeItem =  widgetFileTree.getSelection()[0];
              if (treeItem != null)
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                if (fileTreeData.fileType == BARServer.FileTypes.DIRECTORY)
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
          }
        });

        menu = Widgets.newPopupMenu(shell);
        {
          menuItemOpenClose = Widgets.addMenuItem(menu,BARControl.tr("Open/Close"));
          menuItemOpenClose.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              TreeItem[] treeItems = widgetFileTree.getSelection();
              if (treeItems != null)
              {
                FileTreeData fileTreeData = (FileTreeData)treeItems[0].getData();
                if (fileTreeData.fileType == BARServer.FileTypes.DIRECTORY)
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

          Widgets.addMenuItemSeparator(menu);

          menuItemInclude = Widgets.addMenuItemRadio(menu,BARControl.tr("Include"));
          menuItemInclude.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem menuItem = (MenuItem)selectionEvent.widget;
              if (menuItem.getSelection())
              {
                for (TreeItem treeItem : widgetFileTree.getSelection())
                {
                  FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                  fileTreeData.include();
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
            }
          });

          menuItemExcludeByList = Widgets.addMenuItemRadio(menu,BARControl.tr("Exclude"));
          menuItemExcludeByList.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem menuItem = (MenuItem)selectionEvent.widget;
              if (menuItem.getSelection())
              {
                for (TreeItem treeItem : widgetFileTree.getSelection())
                {
                  FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                  fileTreeData.excludeByList();
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
            }
          });

          menuItemExcludeByNoBackup = Widgets.addMenuItemRadio(menu,BARControl.tr("Exclude by .nobackup"),Settings.hasExpertRole());
          menuItemExcludeByNoBackup.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem menuItem = (MenuItem)selectionEvent.widget;
              if (menuItem.getSelection())
              {
                for (TreeItem treeItem : widgetFileTree.getSelection())
                {
                  FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                  fileTreeData.excludeByNoBackup();
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
            }
          });

          menuItemExcludeByNoDump = Widgets.addMenuItemRadio(menu,BARControl.tr("Exclude by no dump"),Settings.hasExpertRole());
          menuItemExcludeByNoDump.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem menuItem = (MenuItem)selectionEvent.widget;
              if (menuItem.getSelection())
              {
                for (TreeItem treeItem : widgetFileTree.getSelection())
                {
                  FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                  fileTreeData.excludeByNoDump();
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
            }
          });

          menuItemNone = Widgets.addMenuItemRadio(menu,BARControl.tr("None"));
          menuItemNone.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem menuItem = (MenuItem)selectionEvent.widget;
              if (menuItem.getSelection())
              {
                for (TreeItem treeItem : widgetFileTree.getSelection())
                {
                  FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                  fileTreeData.none();
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
            }
          });

          Widgets.addMenuItemSeparator(menu);

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add mount")+"\u2026",Settings.hasExpertRole());
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                mountListAdd(fileTreeData.name);
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove mount"),Settings.hasExpertRole());
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                mountListRemove(fileTreeData.name);
              }
            }
          });

          Widgets.addMenuItemSeparator(menu,Settings.hasExpertRole());

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Directory/File size"));
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                directoryInfoThread.add(selectedJobData.uuid,fileTreeData.name,true,treeItem);
              }
            }
          });
        }
        menu.addMenuListener(new MenuListener()
        {
          @Override
          public void menuShown(MenuEvent menuEvent)
          {
            if (widgetFileTreeToolTip != null)
            {
              widgetFileTreeToolTip.dispose();
              widgetFileTreeToolTip = null;
            }
          }
          @Override
          public void menuHidden(MenuEvent menuEvent)
          {
          }
        });
        widgetFileTree.setMenu(menu);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,1.0,1.0,0.0,0.0,0.0}));
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          widgetInclude = Widgets.newButton(composite,BARControl.tr("Include"));
          widgetInclude.setToolTipText(BARControl.tr("Include entry in archive."));
          Widgets.layout(widgetInclude,0,0,TableLayoutData.WE);
          widgetInclude.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                fileTreeData.include();
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

          widgetExclude = Widgets.newButton(composite,BARControl.tr("Exclude"));
          widgetExclude.setToolTipText(BARControl.tr("Exclude entry from archive."));
          Widgets.layout(widgetExclude,0,1,TableLayoutData.WE);
          widgetExclude.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                fileTreeData.excludeByList();
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

          widgetNone = Widgets.newButton(composite,BARControl.tr("None"));
          widgetNone.setToolTipText(BARControl.tr("Do not include/exclude entry in/from archive."));
          Widgets.layout(widgetNone,0,2,TableLayoutData.WE);
          widgetNone.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                fileTreeData.none();
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              openAllIncludedDirectories();
            }
          });

          button = Widgets.newCheckbox(composite,BARControl.tr("directory size"));
          button.setToolTipText(BARControl.tr("Show directory sizes (sum of file sizes)."));
          Widgets.layout(button,0,5,TableLayoutData.E,0,0,2,0);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button  widget = (Button)selectionEvent.widget;
              directorySizesFlag = widget.getSelection();
            }
          });
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Images"),Settings.hasExpertRole());
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // image tree
        widgetDeviceTable = Widgets.newTable(tab,SWT.MULTI);
        widgetDeviceTable.setToolTipText(BARControl.tr("List of existing devices for image storage.\nRight-click to open context menu."));
        Widgets.layout(widgetDeviceTable,0,0,TableLayoutData.NSWE);
        SelectionListener deviceTableColumnSelectionListener = new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn          tableColumn          = (TableColumn)selectionEvent.widget;
            DeviceDataComparator deviceDataComparator = new DeviceDataComparator(widgetDeviceTable,tableColumn);
            synchronized(widgetDeviceTable)
            {
              Widgets.sortTableColumn(widgetDeviceTable,tableColumn,deviceDataComparator);
            }
          }
        };
        tableColumn = Widgets.addTableColumn(widgetDeviceTable,0,"Name",SWT.LEFT, 500,true);
        tableColumn.addSelectionListener(deviceTableColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetDeviceTable,1,"Size",SWT.RIGHT,100,false);
        tableColumn.addSelectionListener(deviceTableColumnSelectionListener);

        menu = Widgets.newPopupMenu(shell);
        {
          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Include"));
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              for (TableItem tableItem : widgetDeviceTable.getSelection())
              {
                DeviceData deviceData = (DeviceData)tableItem.getData();
                deviceData.include();
                tableItem.setImage(IMAGE_DEVICE_INCLUDED);
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Exclude"));
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TableItem tableItem : widgetDeviceTable.getSelection())
              {
                DeviceData deviceData = (DeviceData)tableItem.getData();
                deviceData.exclude();
                tableItem.setImage(IMAGE_DEVICE_EXCLUDED);
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("None"));
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TableItem tableItem : widgetDeviceTable.getSelection())
              {
                DeviceData deviceData = (DeviceData)tableItem.getData();
                includeListRemove(deviceData.name);
                excludeListRemove(deviceData.name);
                tableItem.setImage(IMAGE_DEVICE);
              }
            }
          });
        }
        widgetDeviceTable.setMenu(menu);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,2);
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,1.0,1.0,0.0,0.0,0.0}));
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          button = Widgets.newButton(composite,BARControl.tr("Include"));
          button.setToolTipText(BARControl.tr("Include selected device for image storage."));
          Widgets.layout(button,0,0,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TableItem tableItem : widgetDeviceTable.getSelection())
              {
                DeviceData deviceData = (DeviceData)tableItem.getData();
                deviceData.include();
                tableItem.setImage(IMAGE_DEVICE_INCLUDED);
              }
            }
          });

          button = Widgets.newButton(composite,BARControl.tr("Exclude"));
          button.setToolTipText(BARControl.tr("Exclude selected device from image storage."));
          Widgets.layout(button,0,1,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TableItem tableItem : widgetDeviceTable.getSelection())
              {
                DeviceData deviceData = (DeviceData)tableItem.getData();
                deviceData.exclude();
                tableItem.setImage(IMAGE_DEVICE_EXCLUDED);
              }
            }
          });

          button = Widgets.newButton(composite,BARControl.tr("None"));
          button.setToolTipText(BARControl.tr("Remove selected device from image storage."));
          Widgets.layout(button,0,2,TableLayoutData.WE);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              for (TableItem tableItem : widgetDeviceTable.getSelection())
              {
                DeviceData deviceData = (DeviceData)tableItem.getData();
                deviceData.none();
                includeListRemove(deviceData.name);
                excludeListRemove(deviceData.name);
                tableItem.setImage(IMAGE_DEVICE);
              }
            }
          });
        }
      }

      tab = Widgets.addTab(widgetTabFolder,Settings.hasNormalRole() ? BARControl.tr("Filters && Mounts") : BARControl.tr("Filters"));
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        tabFolder = Widgets.newTabFolder(tab);
        Widgets.layout(tabFolder,0,0,TableLayoutData.NSWE);

        // included tab
        subTab = Widgets.addTab(tabFolder,BARControl.tr("Included"));
        subTab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
        {
          widgetIncludeTable = Widgets.newTable(subTab);
          widgetIncludeTable.setToolTipText(BARControl.tr("List of include patterns, right-click for context menu."));
          widgetIncludeTable.setHeaderVisible(false);
          widgetIncludeTable.setLayout(new TableLayout(null,1.0));
          Widgets.addTableColumn(widgetIncludeTable,0,SWT.LEFT,0,true);
          Widgets.layout(widgetIncludeTable,0,0,TableLayoutData.NSWE);
          widgetIncludeTable.addMouseListener(new MouseListener()
          {
            @Override
            public void mouseDoubleClick(final MouseEvent mouseEvent)
            {
              includeListEdit();
            }
            @Override
            public void mouseDown(final MouseEvent mouseEvent)
            {
            }
            @Override
            public void mouseUp(final MouseEvent mouseEvent)
            {
            }
          });
          widgetIncludeTable.addKeyListener(new KeyListener()
          {
            @Override
            public void keyPressed(KeyEvent keyEvent)
            {
            }
            @Override
            public void keyReleased(KeyEvent keyEvent)
            {
              if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
              {
                Widgets.invoke(widgetIncludeTableAdd);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
              {
                Widgets.invoke(widgetIncludeTableRemove);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
              {
                Widgets.invoke(widgetIncludeTableEdit);
              }
            }
          });

          menu = Widgets.newPopupMenu(shell);
          {
            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                includeListAdd();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Edit")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                includeListEdit();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                includeListClone();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                includeListRemove();
              }
            });
          }
          widgetIncludeTable.setMenu(menu);

          // buttons
          composite = Widgets.newComposite(subTab,SWT.NONE,2);
          Widgets.layout(composite,1,0,TableLayoutData.WE);
          {
            widgetIncludeTableAdd = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
            widgetIncludeTableAdd.setToolTipText(BARControl.tr("Add entry to included list."));
            Widgets.layout(widgetIncludeTableAdd,0,0,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetIncludeTableAdd.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                includeListAdd();
              }
            });

            widgetIncludeTableEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
            widgetIncludeTableEdit.setToolTipText(BARControl.tr("Edit entry in included list."));
            Widgets.layout(widgetIncludeTableEdit,0,1,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetIncludeTableEdit.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                includeListEdit();
              }
            });

            button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
            button.setToolTipText(BARControl.tr("Clone entry in included list."));
            Widgets.layout(button,0,2,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                includeListClone();
              }
            });

            widgetIncludeTableRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
            widgetIncludeTableRemove.setToolTipText(BARControl.tr("Remove entry from included list."));
            Widgets.layout(widgetIncludeTableRemove,0,3,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetIncludeTableRemove.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                includeListRemove();
              }
            });
          }
        }

        // excluded tab
        subTab = Widgets.addTab(tabFolder,BARControl.tr("Excluded"));
        subTab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
        {
          widgetExcludeList = Widgets.newList(subTab);
          widgetExcludeList.setToolTipText(BARControl.tr("List of exclude patterns, right-click for context menu."));
          Widgets.layout(widgetExcludeList,0,0,TableLayoutData.NSWE);
          widgetExcludeList.addMouseListener(new MouseListener()
          {
            @Override
            public void mouseDoubleClick(final MouseEvent mouseEvent)
            {
              excludeListEdit();
            }
            @Override
            public void mouseDown(final MouseEvent mouseEvent)
            {
            }
            @Override
            public void mouseUp(final MouseEvent mouseEvent)
            {
            }
          });
          widgetExcludeList.addKeyListener(new KeyListener()
          {
            @Override
            public void keyPressed(KeyEvent keyEvent)
            {
            }
            @Override
            public void keyReleased(KeyEvent keyEvent)
            {
              if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
              {
                Widgets.invoke(widgetExcludeListAdd);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
              {
                Widgets.invoke(widgetExcludeListRemove);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
              {
                Widgets.invoke(widgetExcludeListEdit);
              }
            }
          });

          menu = Widgets.newPopupMenu(shell);
          {
            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                excludeListAdd();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Edit")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                excludeListEdit();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                excludeListClone();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                excludeListRemove();
              }
            });
          }
          widgetExcludeList.setMenu(menu);

          // buttons
          composite = Widgets.newComposite(subTab,SWT.NONE,2);
          Widgets.layout(composite,1,0,TableLayoutData.WE);
          {
            widgetExcludeListAdd = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
            widgetExcludeListAdd.setToolTipText(BARControl.tr("Add entry to excluded list."));
            Widgets.layout(widgetExcludeListAdd,0,0,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetExcludeListAdd.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                excludeListAdd();
              }
            });

            widgetExcludeListEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
            widgetExcludeListEdit.setToolTipText(BARControl.tr("Edit entry in excluded list."));
            Widgets.layout(widgetExcludeListEdit,0,1,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetExcludeListEdit.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                excludeListEdit();
              }
            });

            button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
            button.setToolTipText(BARControl.tr("Clone entry in excluded list."));
            Widgets.layout(button,0,2,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                excludeListClone();
              }
            });

            widgetExcludeListRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
            widgetExcludeListRemove.setToolTipText(BARControl.tr("Remove entry from excluded list."));
            Widgets.layout(widgetExcludeListRemove,0,3,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetExcludeListRemove.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                excludeListRemove();
              }
            });
          }
        }

        // include command tab
        subTab = Widgets.addTab(tabFolder,BARControl.tr("Include command"),Settings.hasExpertRole());
        subTab.setLayout(new TableLayout(1.0,1.0));
        Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
        {
          subComposite = Widgets.newComposite(subTab);
          subComposite.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0));
          Widgets.layout(subComposite,0,0,TableLayoutData.NSWE);
          {
            label = Widgets.newLabel(subComposite,BARControl.tr("Entries")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            styledText = Widgets.newStyledText(subComposite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
            styledText.setToolTipText(BARControl.tr("Command or script to execute to get a list of entries to include."));
            Widgets.layout(styledText,1,0,TableLayoutData.NSWE);
            styledText.addModifyListener(new ModifyListener()
            {
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                StyledText widget = (StyledText)modifyEvent.widget;
                String     string = widget.getText();
                Color      color  = COLOR_MODIFIED;

                if (includeFileCommand.equals(string.replace(widget.getLineDelimiter(),"\n"))) color = null;
                widget.setBackground(color);
              }
            });
            styledText.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                StyledText widget = (StyledText)selectionEvent.widget;
                String     text   = widget.getText();

                try
                {
                  includeFileCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                  BARServer.setJobOption(selectedJobData.uuid,includeFileCommand);
                  widget.setBackground(null);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
              }
            });
            styledText.addFocusListener(new FocusListener()
            {
              @Override
              public void focusGained(FocusEvent focusEvent)
              {
              }
              @Override
              public void focusLost(FocusEvent focusEvent)
              {
                StyledText widget = (StyledText)focusEvent.widget;
                String     text   = widget.getText();

                try
                {
                  includeFileCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                  BARServer.setJobOption(selectedJobData.uuid,includeFileCommand);
                  widget.setBackground(null);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(styledText,includeFileCommand));

            // buttons
            button = Widgets.newButton(subComposite,BARControl.tr("Test")+"\u2026");
            button.setToolTipText(BARControl.tr("Test script."));
            Widgets.layout(button,2,0,TableLayoutData.E,0,0,2,2);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                testScript("include-file-command",includeFileCommand.getString());
              }
            });
          }

          subComposite = Widgets.newComposite(subTab);
          subComposite.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0));
          Widgets.layout(subComposite,1,0,TableLayoutData.NSWE);
          {
            label = Widgets.newLabel(subComposite,BARControl.tr("Images")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            styledText = Widgets.newStyledText(subComposite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
            styledText.setToolTipText(BARControl.tr("Command or script to execute to get a list of images to include."));
            Widgets.layout(styledText,1,0,TableLayoutData.NSWE);
            styledText.addModifyListener(new ModifyListener()
            {
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                StyledText widget = (StyledText)modifyEvent.widget;
                String     string = widget.getText();
                Color      color  = COLOR_MODIFIED;

                if (includeImageCommand.equals(string.replace(widget.getLineDelimiter(),"\n"))) color = null;
                widget.setBackground(color);
              }
            });
            styledText.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                StyledText widget = (StyledText)selectionEvent.widget;
                String     text   = widget.getText();

                try
                {
                  includeImageCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                  BARServer.setJobOption(selectedJobData.uuid,includeImageCommand);
                  widget.setBackground(null);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
              }
            });
            styledText.addFocusListener(new FocusListener()
            {
              @Override
              public void focusGained(FocusEvent focusEvent)
              {
              }
              @Override
              public void focusLost(FocusEvent focusEvent)
              {
                StyledText widget = (StyledText)focusEvent.widget;
                String     text   = widget.getText();

                try
                {
                  includeImageCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                  BARServer.setJobOption(selectedJobData.uuid,includeImageCommand);
                  widget.setBackground(null);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(styledText,includeImageCommand));

            // buttons
            button = Widgets.newButton(subComposite,BARControl.tr("Test")+"\u2026");
            button.setToolTipText(BARControl.tr("Test script."));
            Widgets.layout(button,2,0,TableLayoutData.E,0,0,2,2);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                testScript("include-image-command",includeImageCommand.getString());
              }
            });
          }
        }

        // excluded command tab
        subTab = Widgets.addTab(tabFolder,BARControl.tr("Exclude command"),Settings.hasExpertRole());
        subTab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
        {
          styledText = Widgets.newStyledText(subTab,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
          styledText.setToolTipText(BARControl.tr("Command or script to execute to get a list of entries/images to exclude."));
          Widgets.layout(styledText,0,0,TableLayoutData.NSWE);
          styledText.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              StyledText widget = (StyledText)modifyEvent.widget;
              String     text   = widget.getText();
              Color      color  = COLOR_MODIFIED;

              if (excludeCommand.equals(text.replace(widget.getLineDelimiter(),"\n"))) color = null;
              widget.setBackground(color);
            }
          });
          styledText.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              StyledText widget = (StyledText)selectionEvent.widget;
              String     text   = widget.getText();

              try
              {
                excludeCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                BARServer.setJobOption(selectedJobData.uuid,excludeCommand);
                widget.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          styledText.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              StyledText widget = (StyledText)focusEvent.widget;
              String     text   = widget.getText();

              try
              {
                excludeCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                BARServer.setJobOption(selectedJobData.uuid,excludeCommand);
                widget.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(styledText,excludeCommand));

          // buttons
          button = Widgets.newButton(subTab,BARControl.tr("Test")+"\u2026");
          button.setToolTipText(BARControl.tr("Test script."));
          Widgets.layout(button,1,0,TableLayoutData.E,0,0,2,2);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              testScript("exclude-command",excludeCommand.getString());
            }
          });
        }

        // mount tab
        subTab = Widgets.addTab(tabFolder,BARControl.tr("Mounts"),Settings.hasNormalRole());
        subTab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
        {
          Listener mountListColumnResizeListener = new Listener()
          {
            public void handleEvent(Event event)
            {
              Settings.mountTableColumns = new Settings.ColumnWidth(Widgets.getTableColumnWidth(widgetMountTable));
            }
          };

          widgetMountTable = Widgets.newTable(subTab);
          widgetMountTable.setToolTipText(BARControl.tr("List of devices to mount, right-click for context menu."));
          Widgets.layout(widgetMountTable,0,0,TableLayoutData.NSWE);
          tableColumn = Widgets.addTableColumn(widgetMountTable,0,BARControl.tr("Name"),  SWT.LEFT,600,true);
          tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
          tableColumn.addListener(SWT.Resize,mountListColumnResizeListener);
          tableColumn = Widgets.addTableColumn(widgetMountTable,1,BARControl.tr("Device"),SWT.LEFT,100,true);
          tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
          tableColumn.addListener(SWT.Resize,mountListColumnResizeListener);
          Widgets.setTableColumnWidth(widgetMountTable,Settings.mountTableColumns.width);

          widgetMountTable.addMouseListener(new MouseListener()
          {
            @Override
            public void mouseDoubleClick(final MouseEvent mouseEvent)
            {
              mountListEdit();
            }
            @Override
            public void mouseDown(final MouseEvent mouseEvent)
            {
            }
            @Override
            public void mouseUp(final MouseEvent mouseEvent)
            {
            }
          });
          widgetMountTable.addKeyListener(new KeyListener()
          {
            @Override
            public void keyPressed(KeyEvent keyEvent)
            {
            }
            @Override
            public void keyReleased(KeyEvent keyEvent)
            {
              if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
              {
                Widgets.invoke(widgetMountTableAdd);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
              {
                Widgets.invoke(widgetMountTableRemove);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
              {
                Widgets.invoke(widgetMountTableEdit);
              }
            }
          });

          menu = Widgets.newPopupMenu(shell);
          {
            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                mountListAdd();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Edit")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                mountListEdit();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                mountListClone();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                mountListRemove();
              }
            });
          }
          widgetMountTable.setMenu(menu);

          // buttons
          composite = Widgets.newComposite(subTab,SWT.NONE,2);
          Widgets.layout(composite,1,0,TableLayoutData.WE);
          {
            widgetMountTableAdd = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
            widgetMountTableAdd.setToolTipText(BARControl.tr("Add entry to mount list."));
            Widgets.layout(widgetMountTableAdd,0,0,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetMountTableAdd.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                mountListAdd();
              }
            });

            widgetMountTableEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
            widgetMountTableEdit.setToolTipText(BARControl.tr("Edit entry in mount list."));
            Widgets.layout(widgetMountTableEdit,0,1,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetMountTableEdit.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                mountListEdit();
              }
            });

            button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
            button.setToolTipText(BARControl.tr("Clone entry in mount list."));
            Widgets.layout(button,0,2,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                mountListClone();
              }
            });

            widgetMountTableRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
            widgetMountTableRemove.setToolTipText(BARControl.tr("Remove entry from mount list."));
            Widgets.layout(widgetMountTableRemove,0,3,TableLayoutData.W,0,0,0,0,110,SWT.DEFAULT);
            widgetMountTableRemove.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                mountListRemove();
              }
            });
          }
        }

        // options
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0}));
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          label = Widgets.newLabel(composite,BARControl.tr("Options")+":");
          Widgets.layout(label,0,0,TableLayoutData.NW);

          subComposite = Widgets.newComposite(composite);
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            button = Widgets.newCheckbox(subComposite,BARControl.tr("skip unreadable entries"));
            button.setToolTipText(BARControl.tr("If enabled then skip not readable entries (write information to log file).\nIf disabled stop job with an error."));
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                try
                {
                  skipUnreadable.set(checkedFlag);
                  BARServer.setJobOption(selectedJobData.uuid,skipUnreadable);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,skipUnreadable));

            button = Widgets.newCheckbox(subComposite,BARControl.tr("no stop on owner error"));
            button.setToolTipText(BARControl.tr("If enabled then do not stop if the owner of an entry cannot be set."));
            Widgets.layout(button,1,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                try
                {
                  noStopOnOwnerError.set(checkedFlag);
                  BARServer.setJobOption(selectedJobData.uuid,noStopOnOwnerError);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,noStopOnOwnerError));

            button = Widgets.newCheckbox(subComposite,BARControl.tr("no stop on attribute error"));
            button.setToolTipText(BARControl.tr("If enabled then do not stop if there is an attribute error when reading a file."));
            Widgets.layout(button,2,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                try
                {
                  noStopOnAttributeError.set(checkedFlag);
                  BARServer.setJobOption(selectedJobData.uuid,noStopOnAttributeError);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,noStopOnAttributeError));

            button = Widgets.newCheckbox(subComposite,BARControl.tr("raw images"),Settings.hasExpertRole());
            button.setToolTipText(BARControl.tr("If enabled then store all data of a device into an image.\nIf disabled try to detect file system and only store used blocks to image."));
            Widgets.layout(button,3,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                try
                {
                  rawImages.set(checkedFlag);
                  BARServer.setJobOption(selectedJobData.uuid,rawImages);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,rawImages));
          }
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Storage"));
      tab.setLayout(new TableLayout(new double[]{0.0,0.0,0.0,Settings.hasNormalRole() ? 1.0 : 0.0,0.0,0.0,0.0,0.0,0.0},new double[]{0.0,1.0}));
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                boolean changedFlag = archivePartSizeFlag.set(false);
                archivePartSize.set(0L);
                BARServer.setJobOption(selectedJobData.uuid,archivePartSize);

                if (   changedFlag
                    && (   (storageType.getEnum() == StorageTypes.CD )
                        || (storageType.getEnum() == StorageTypes.DVD)
                        || (storageType.getEnum() == StorageTypes.BD )
                       )
                   )
                {
                  Dialogs.warning(shell,
                                  BARControl.tr("When writing to a CD/DVD/BD without splitting enabled\nthe resulting archive file may not fit on medium.")
                                 );
                }
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
          {
            @Override
            public void modified(Control control, WidgetVariable archivePartSizeFlag)
            {
              ((Button)control).setSelection(!archivePartSizeFlag.getBoolean());
            }
          });

          widgetArchivePartSizeLimited = Widgets.newRadio(composite,BARControl.tr("limit to"));
          widgetArchivePartSizeLimited.setToolTipText(BARControl.tr("Limit size of storage files to specified value."));
          Widgets.layout(widgetArchivePartSizeLimited,0,1,TableLayoutData.W);
          widgetArchivePartSizeLimited.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              archivePartSizeFlag.set(true);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetArchivePartSizeLimited,archivePartSizeFlag)
          {
            @Override
            public void modified(Control control, WidgetVariable archivePartSizeFlag)
            {
              ((Button)control).setSelection(archivePartSizeFlag.getBoolean());
            }
          });

          widgetArchivePartSize = Widgets.newCombo(composite);
          widgetArchivePartSize.setToolTipText(BARControl.tr("Size limit for one storage file part."));
          widgetArchivePartSize.setItems(new String[]{"32M","64M","128M","140M","215M","240M","250M","256M","260M","280M","425M","470M","512M","620M","660M","800M","850M","1G","1800M","2G","4G","5G","6.4G","8G","10G","20G"});
          widgetArchivePartSize.setData("showedErrorDialog",false);
          Widgets.layout(widgetArchivePartSize,0,2,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(widgetArchivePartSize,archivePartSizeFlag)
          {
            @Override
            public void modified(Control control, WidgetVariable archivePartSizeFlag)
            {
              Widgets.setEnabled(control,archivePartSizeFlag.getBoolean());
            }
          });
          widgetArchivePartSize.addModifyListener(new ModifyListener()
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();
              try
              {
                long n = Units.parseByteSize(string);
                archivePartSize.set(n);
                BARServer.setJobOption(selectedJobData.uuid,archivePartSize);
                widget.setText(Units.formatByteSize(n));
                widget.setBackground(null);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                  widget.forceFocus();
                }
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();
              try
              {
                long n = Units.parseByteSize(string);
                archivePartSize.set(n);
                BARServer.setJobOption(selectedJobData.uuid,archivePartSize);
                widget.setText(Units.formatByteSize(n));
                widget.setBackground(null);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                  widget.forceFocus();
                }
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          widgetArchivePartSize.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
              Combo widget = (Combo)focusEvent.widget;
              widget.setData("showedErrorDialog",false);
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Combo  widget = (Combo)focusEvent.widget;
              String string = widget.getText();
              try
              {
                long n = Units.parseByteSize(string);
                archivePartSize.set(n);
                BARServer.setJobOption(selectedJobData.uuid,archivePartSize);
                widget.setText(Units.formatByteSize(n));
                widget.setBackground(null);
              }
              catch (NumberFormatException exception)
              {
                if (!(Boolean)widget.getData("showedErrorDialog"))
                {
                  widget.setData("showedErrorDialog",true);
                  Dialogs.error(shell,BARControl.tr("''{0}'' is not a valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                  widget.forceFocus();
                }
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetArchivePartSize,archivePartSize)
          {
            @Override
            public String getString(WidgetVariable variable)
            {
              return Units.formatByteSize(variable.getLong());
            }
          });

          label = Widgets.newLabel(composite,BARControl.tr("bytes"));
          Widgets.layout(label,0,3,TableLayoutData.W);
        }
        composite.addMouseListener(new MouseListener()
        {
          @Override
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseDown(final MouseEvent mouseEvent)
          {
            Rectangle bounds = widgetArchivePartSize.getBounds();

            if (bounds.contains(mouseEvent.x,mouseEvent.y))
            {
              archivePartSizeFlag.set(true);
              widgetArchivePartSize.setListVisible(true);
            }
          }
          @Override
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });

        // compress
        label = Widgets.newLabel(tab,BARControl.tr("Compress")+":");
        Widgets.layout(label,1,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(0.0,0.0));
        Widgets.layout(composite,1,1,TableLayoutData.W);
        {
          // xdelta
          subComposite = Widgets.newComposite(composite,Settings.hasExpertRole());
          subComposite.setLayout(new TableLayout(0.0,0.0));
          Widgets.layout(subComposite,0,0,TableLayoutData.NONE);
          {
            label = Widgets.newLabel(subComposite,BARControl.tr("Delta")+":");
            Widgets.layout(label,0,0,TableLayoutData.NONE);

            combo = Widgets.newOptionMenu(subComposite);
            combo.setToolTipText(BARControl.tr("Delta compression method to use."));
            Widgets.setComboItems(combo,
                                  new String[]{"",       "none",
                                               "xdelta1","xdelta1",
                                               "xdelta2","xdelta2",
                                               "xdelta3","xdelta3",
                                               "xdelta4","xdelta4",
                                               "xdelta5","xdelta5",
                                               "xdelta6","xdelta6",
                                               "xdelta7","xdelta7",
                                               "xdelta8","xdelta8",
                                               "xdelta9","xdelta9"
                                              }
                                 );
            Widgets.layout(combo,0,1,TableLayoutData.W);
            combo.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = Widgets.getSelectedComboItem(widget);

                try
                {
                  deltaCompressAlgorithm.set(string);
                  BARServer.setJobOption(selectedJobData.uuid,"compress-algorithm",deltaCompressAlgorithm.getString()+"+"+byteCompressAlgorithm.getString());
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(combo,deltaCompressAlgorithm));
          }

          // byte
          label = Widgets.newLabel(composite,BARControl.tr("Byte")+":");
          Widgets.layout(label,0,1,TableLayoutData.NONE);

          widgetByteCompressAlgorithmType = Widgets.newOptionMenu(composite);
          widgetByteCompressAlgorithmType.setToolTipText(BARControl.tr("Byte compression method to use."));
          Widgets.setComboItems(widgetByteCompressAlgorithmType,new String[]{" ",   "none",
                                                                             "zip", "zip",
                                                                             "bzip","bzip",
                                                                             "lzma","lzma",
                                                                             "lzo", "lzo",
                                                                             "lz4", "lz4",
                                                                             "zstd","zstd"
                                                                            }
          );
          Widgets.layout(widgetByteCompressAlgorithmType,0,2,TableLayoutData.W);
          widgetByteCompressAlgorithmType.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = Widgets.getSelectedComboItem(widget);

              try
              {
                if      (string.equals("none")) byteCompressAlgorithm.set("none" );
                else if (string.equals("zip" )) byteCompressAlgorithm.set("zip0" );
                else if (string.equals("bzip")) byteCompressAlgorithm.set("bzip1");
                else if (string.equals("lzma")) byteCompressAlgorithm.set("lzma1");
                else if (string.equals("lzo" )) byteCompressAlgorithm.set("lzo1" );
                else if (string.equals("lz4" )) byteCompressAlgorithm.set("lz4-0");
                else if (string.equals("zstd")) byteCompressAlgorithm.set("zstd0");
                BARServer.setJobOption(selectedJobData.uuid,"compress-algorithm",deltaCompressAlgorithm.getString()+"+"+byteCompressAlgorithm.getString());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetByteCompressAlgorithmType,byteCompressAlgorithmType));

          widgetByteCompressAlgorithmLevel = Widgets.newOptionMenu(composite);
          widgetByteCompressAlgorithmLevel.setEnabled(true);
          widgetByteCompressAlgorithmLevel.setToolTipText(BARControl.tr("Byte compression level to use."));
          Widgets.setComboItems(widgetByteCompressAlgorithmLevel,new String[]{" ","none"});
          Widgets.layout(widgetByteCompressAlgorithmLevel,0,3,TableLayoutData.W);
          widgetByteCompressAlgorithmLevel.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = Widgets.getSelectedComboItem(widget);

              try
              {
                byteCompressAlgorithm.set(string);
                BARServer.setJobOption(selectedJobData.uuid,"compress-algorithm",deltaCompressAlgorithm.getString()+"+"+byteCompressAlgorithm.getString());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          widgetByteCompressAlgorithmType.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Combo  widget = (Combo)modifyEvent.widget;
              String string = Widgets.getSelectedComboItem(widget);

              String  byteCompressAlgorithms[];
              boolean byteCompressAlgrithmLevelEnabledFlag;
              if      (string.equals("none"))
              {
                byteCompressAlgorithms = new String[]{" ",     "none",
                                                     };
                byteCompressAlgrithmLevelEnabledFlag = false;
              }
              else if (string.startsWith("zip"))
              {
                byteCompressAlgorithms = new String[]{"0",  "zip0",
                                                      "1",  "zip1",
                                                      "2",  "zip2",
                                                      "3",  "zip3",
                                                      "4",  "zip4",
                                                      "5",  "zip5",
                                                      "6",  "zip6",
                                                      "7",  "zip7",
                                                      "8",  "zip8",
                                                      "9",  "zip9",
                                                     };
                byteCompressAlgrithmLevelEnabledFlag = true;
              }
              else if (string.startsWith("bzip"))
              {
                byteCompressAlgorithms = new String[]{"1", "bzip1",
                                                      "2", "bzip2",
                                                      "3", "bzip3",
                                                      "4", "bzip4",
                                                      "5", "bzip5",
                                                      "6", "bzip6",
                                                      "7", "bzip7",
                                                      "8", "bzip8",
                                                      "9", "bzip9",
                                                     };
                byteCompressAlgrithmLevelEnabledFlag = true;
              }
              else if (string.startsWith("lzma"))
              {
                byteCompressAlgorithms = new String[]{"1", "lzma1",
                                                      "2", "lzma2",
                                                      "3", "lzma3",
                                                      "4", "lzma4",
                                                      "5", "lzma5",
                                                      "6", "lzma6",
                                                      "7", "lzma7",
                                                      "8", "lzma8",
                                                      "9", "lzma9",
                                                     };
                byteCompressAlgrithmLevelEnabledFlag = true;
              }
              else if (string.startsWith("lzo"))
              {
                byteCompressAlgorithms = new String[]{"1",  "lzo1",
                                                      "2",  "lzo2",
                                                      "3",  "lzo3",
                                                      "4",  "lzo4",
                                                      "5",  "lzo5",
                                                     };
                byteCompressAlgrithmLevelEnabledFlag = true;
              }
              else if (string.startsWith("lz4"))
              {
                byteCompressAlgorithms = new String[]{"0", "lz4-0",
                                                      "1", "lz4-1",
                                                      "2", "lz4-2",
                                                      "3", "lz4-3",
                                                      "4", "lz4-4",
                                                      "5", "lz4-5",
                                                      "6", "lz4-6",
                                                      "7", "lz4-7",
                                                      "8", "lz4-8",
                                                      "9", "lz4-9",
                                                      "10","lz4-10",
                                                      "11","lz4-11",
                                                      "12","lz4-12",
                                                      "13","lz4-13",
                                                      "14","lz4-14",
                                                      "15","lz4-15",
                                                      "16","lz4-16",
                                                     };
                byteCompressAlgrithmLevelEnabledFlag = true;
              }
              else if (string.startsWith("zstd"))
              {
                byteCompressAlgorithms = new String[]{"0", "zstd0",
                                                      "1", "zstd1",
                                                      "2", "zstd2",
                                                      "3", "zstd3",
                                                      "4", "zstd4",
                                                      "5", "zstd5",
                                                      "6", "zstd6",
                                                      "7", "zstd7",
                                                      "8", "zstd8",
                                                      "9", "zstd9",
                                                      "10","zstd10",
                                                      "11","zstd11",
                                                      "12","zstd12",
                                                      "13","zstd13",
                                                      "14","zstd14",
                                                      "15","zstd15",
                                                      "16","zstd16",
                                                      "17","zstd17",
                                                      "18","zstd18",
                                                      "19","zstd19"
                                                     };
                byteCompressAlgrithmLevelEnabledFlag = true;
              }
              else
              {
                byteCompressAlgorithms = new String[]{" ",     "none",
                                                     };
                byteCompressAlgrithmLevelEnabledFlag = false;
              }

              widgetByteCompressAlgorithmLevel.setEnabled(byteCompressAlgrithmLevelEnabledFlag);
              Widgets.setComboItems(widgetByteCompressAlgorithmLevel,byteCompressAlgorithms);
              Widgets.setSelectedComboItem(widgetByteCompressAlgorithmLevel,0);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetByteCompressAlgorithmLevel,byteCompressAlgorithm));
        }

        // xdelta source
        composite = Widgets.newComposite(tab,Settings.hasExpertRole());
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0,0.0}));
        Widgets.layout(composite,2,1,TableLayoutData.WE);
        {
          label = Widgets.newLabel(composite,BARControl.tr("Source")+":");
          Widgets.layout(label,0,0,TableLayoutData.NONE);

//TODO: list?
          text = Widgets.newText(composite);
          Widgets.layout(text,0,1,TableLayoutData.WE);
          Widgets.addModifyListener(new WidgetModifyListener(text,deltaCompressAlgorithm)
          {
            @Override
            public void modified(Control control, WidgetVariable byteCompressAlgorithm)
            {
              Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none"));
            }
          });
          text.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();
              Color  color  = COLOR_MODIFIED;

              if (deltaSource.getString().equals(string)) color = null;
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();
              deltaSource.set(string);
              sourceListRemoveAll();
              sourceListAdd(string);
              widget.setBackground(null);
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          text.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;
              String string = widget.getText();
              deltaSource.set(string);
              sourceListRemoveAll();
              sourceListAdd(string);
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,deltaSource));

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
          Widgets.addModifyListener(new WidgetModifyListener(button,deltaCompressAlgorithm)
          {
            @Override
            public void modified(Control control, WidgetVariable byteCompressAlgorithm)
            {
              Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none"));
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              String fileName;

              fileName = Dialogs.file(shell,
                                      Dialogs.FileDialogTypes.SAVE,
                                      BARControl.tr("Select source file"),
                                      deltaSource.getString(),
                                      new String[]{BARControl.tr("BAR files"),"*.bar",
                                                   BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                        ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                        : BARControl.listDirectory
                                     );
              if (fileName != null)
              {
                deltaSource.set(fileName);
                sourceListRemoveAll();
                sourceListAdd(fileName);
              }
            }
          });
        }

        // compress exclude
        label = Widgets.newLabel(tab,BARControl.tr("Compress exclude")+":",Settings.hasNormalRole());
        Widgets.layout(label,3,0,TableLayoutData.NW);
        composite = Widgets.newComposite(tab,Settings.hasNormalRole());
        composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(composite,3,1,TableLayoutData.NSWE);
        {
          // compress exclude list
          widgetCompressExcludeList = Widgets.newList(composite);
          widgetCompressExcludeList.setToolTipText(BARControl.tr("List of compress exclude patterns. Entries which match to one of these patterns will not be compressed.\nRight-click for context menu."));
          Widgets.layout(widgetCompressExcludeList,0,0,TableLayoutData.NSWE);
          widgetCompressExcludeList.addMouseListener(new MouseListener()
          {
            @Override
            public void mouseDoubleClick(final MouseEvent mouseEvent)
            {
              compressExcludeListEdit();
            }
            @Override
            public void mouseDown(final MouseEvent mouseEvent)
            {
            }
            @Override
            public void mouseUp(final MouseEvent mouseEvent)
            {
            }
          });
          widgetCompressExcludeList.addKeyListener(new KeyListener()
          {
            @Override
            public void keyPressed(KeyEvent keyEvent)
            {
            }
            @Override
            public void keyReleased(KeyEvent keyEvent)
            {
              if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
              {
                Widgets.invoke(widgetCompressExcludeListInsert);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
              {
                Widgets.invoke(widgetCompressExcludeListRemove);
              }
              else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
              {
                Widgets.invoke(widgetCompressExcludeListEdit);
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetCompressExcludeList,deltaCompressAlgorithm,byteCompressAlgorithm)
          {
            @Override
            public void modified(Control control, WidgetVariable[] compressAlgorithms)
            {
              Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none") || !byteCompressAlgorithm.equals("none"));
            }
          });
          menu = Widgets.newPopupMenu(shell);
          {
            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                compressExcludeListAdd();
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add most used compressed file suffixes"));
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
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

                compressExcludeListAdd(COMPRESSED_PATTERNS);
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add most used multi-media file suffixes"));
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                final String[] MULTIMEDIA_PATTERNS = new String[]
                {
                  "*.jpg",
                  "*.jpeg",
                  "*.mkv",
                  "*.mp3",
                  "*.mp4",
                  "*.mpg",
                  "*.mpeg",
                  "*.avi",
                  "*.wma",
                  "*.wmv",
                  "*.flv",
                  "*.3gp",
                };

                compressExcludeListAdd(MULTIMEDIA_PATTERNS);
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add most used package file suffixes"));
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                final String[] PACKAGE_PATTERNS = new String[]
                {
                  "*.rpm",
                  "*.deb",
                  "*.pkg",
                };

                compressExcludeListAdd(PACKAGE_PATTERNS);
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove")+"\u2026");
            menuItem.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                compressExcludeListRemove();
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
            Widgets.layout(widgetCompressExcludeListInsert,0,0,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            Widgets.addModifyListener(new WidgetModifyListener(widgetCompressExcludeListInsert,deltaCompressAlgorithm,byteCompressAlgorithm)
            {
              @Override
              public void modified(Control control, WidgetVariable byteCompressAlgorithm)
              {
                Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none") || !byteCompressAlgorithm.equals("none"));
              }
            });
            widgetCompressExcludeListInsert.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                compressExcludeListAdd();
              }
            });

            widgetCompressExcludeListEdit = Widgets.newButton(subComposite,BARControl.tr("Edit")+"\u2026");
            widgetCompressExcludeListEdit.setToolTipText(BARControl.tr("Edit entry in compress exclude list."));
            Widgets.layout(widgetCompressExcludeListEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            Widgets.addModifyListener(new WidgetModifyListener(widgetCompressExcludeListEdit,deltaCompressAlgorithm,byteCompressAlgorithm)
            {
              @Override
              public void modified(Control control, WidgetVariable byteCompressAlgorithm)
              {
                Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none") || !byteCompressAlgorithm.equals("none"));
              }
            });
            widgetCompressExcludeListEdit.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                compressExcludeListEdit();
              }
            });

            widgetCompressExcludeListRemove = Widgets.newButton(subComposite,BARControl.tr("Remove")+"\u2026");
            widgetCompressExcludeListRemove.setToolTipText(BARControl.tr("Remove entry from compress exclude list."));
            Widgets.layout(widgetCompressExcludeListRemove,0,2,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            Widgets.addModifyListener(new WidgetModifyListener(widgetCompressExcludeListRemove,deltaCompressAlgorithm,byteCompressAlgorithm)
            {
              @Override
              public void modified(Control control, WidgetVariable byteCompressAlgorithm)
              {
                Widgets.setEnabled(control,!deltaCompressAlgorithm.equals("none") || !byteCompressAlgorithm.equals("none"));
              }
            });
            widgetCompressExcludeListRemove.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                compressExcludeListRemove();
              }
            });
          }
        }

        // crypt
        label = Widgets.newLabel(tab,BARControl.tr("Crypt")+":");
        Widgets.layout(label,4,0,Settings.hasExpertRole() ? TableLayoutData.NW : TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        Widgets.layout(composite,4,1,TableLayoutData.WE);
        {
//TODO: multi crypt
//          for (int i = 0; i < 4; i++)
          for (int i = 0; i < 1; i++)
          {
            widgetCryptAlgorithms[i] = Widgets.newOptionMenu(composite);
            widgetCryptAlgorithms[i].setToolTipText(BARControl.tr("Encryption methods to use."));
            Widgets.setComboItems(widgetCryptAlgorithms[i],
                                  new String[]{"",           "none",
                                               "3DES",       "3DES",
                                               "CAST5",      "CAST5",
                                               "BLOWFISH",   "BLOWFISH",
                                               "AES128",     "AES128",
                                               "AES192",     "AES192",
                                               "AES256",     "AES256",
                                               "TWOFISH128", "TWOFISH128",
                                               "TWOFISH256", "TWOFISH256",
                                               "SERPENT128", "SERPENT128",
                                               "SERPENT192", "SERPENT192",
                                               "SERPENT256", "SERPENT256",
                                               "CAMELLIA128","CAMELLIA128",
                                               "CAMELLIA192","CAMELLIA192",
                                               "CAMELLIA256","CAMELLIA256"
                                              }
                                 );
            Widgets.layout(widgetCryptAlgorithms[i],0,i,TableLayoutData.W);
            widgetCryptAlgorithms[i].addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                try
                {
                  StringBuilder buffer = new StringBuilder();
//TODO: multi crypt
//                  for (int i = 0; i < 4; i++)
                  for (int i = 0; i < 1; i++)
                  {
                    String string = Widgets.getSelectedComboItem(widgetCryptAlgorithms[i]);

                    if (buffer.length() > 0) buffer.append('+');
                    buffer.append(string);
                  }
                  cryptAlgorithm.set(buffer.toString());
                  BARServer.setJobOption(selectedJobData.uuid,cryptAlgorithm);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            });
          }
//TODO: multi crypt
//          for (int i = 0; i < 4; i++)
          for (int i = 0; i < 1; i++)
          {
            Widgets.addModifyListener(new WidgetModifyListener(widgetCryptAlgorithms[i],cryptAlgorithm)
            {
              @Override
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
              {
                String[] s = StringUtils.splitArray(cryptAlgorithm.getString(),"+");

                int i = 0;
//TODO: multi crypt
//                while (i < 4)
                while (i < 1)
                {
                  if (i < s.length)
                  {
                    Widgets.setSelectedComboItem(widgetCryptAlgorithms[i],s[i]);
                  }
                  else
                  {
                    Widgets.setSelectedComboItem(widgetCryptAlgorithms[i],"none");
                  }
                  i++;
                }
              }
            });
          }
        }

        composite = Widgets.newComposite(tab,Settings.hasExpertRole());
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,0.0,0.0,0.0,1.0,0.0}));
        Widgets.layout(composite,5,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,BARControl.tr("symmetric"));
          button.setToolTipText(BARControl.tr("Use symmetric encryption with pass-phrase."));
          button.setSelection(true);
          Widgets.layout(button,0,0,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptAlgorithm)
          {
            @Override
            public void modified(Control control, WidgetVariable cryptAlgorithm)
            {
              Widgets.setEnabled(control,!cryptAlgorithm.equals("none"));
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                cryptType.set("symmetric");
                BARServer.setJobOption(selectedJobData.uuid,cryptType);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptType)
          {
            @Override
            public void modified(Control control, WidgetVariable cryptType)
            {
              ((Button)control).setSelection(cryptType.equals("none") || cryptType.equals("symmetric"));
            }
          });

          button = Widgets.newRadio(composite,BARControl.tr("asymmetric"));
          button.setToolTipText(BARControl.tr("Use asymmetric hybrid-encryption with pass-phrase and public/private key."));
          button.setSelection(false);
          Widgets.layout(button,0,1,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptAlgorithm)
          {
            @Override
            public void modified(Control control, WidgetVariable cryptAlgorithm)
            {
              Widgets.setEnabled(control,!cryptAlgorithm.equals("none"));
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                cryptType.set("asymmetric");
                BARServer.setJobOption(selectedJobData.uuid,cryptType);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptType)
          {
            @Override
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
          Widgets.addModifyListener(new WidgetModifyListener(text,cryptAlgorithm,cryptType)
          {
            @Override
            public void modified(Text text, WidgetVariable cryptAlgorithm)
            {
            }
            @Override
            public void modified(Text text, WidgetVariable variables[])
            {
              Widgets.setEnabled(text,
                                    !variables[0].equals("none")
                                 && variables[1].equals("asymmetric")
                                );
            }
          });
          text.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();
              Color  color  = COLOR_MODIFIED;

              if (cryptPublicKeyFileName.getString().equals(string)) color = null;
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();

              try
              {
                cryptPublicKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,cryptPublicKeyFileName);
                widget.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          text.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();

              try
              {
                cryptPublicKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,cryptPublicKeyFileName);
                widget.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,cryptPublicKeyFileName));

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,5,TableLayoutData.DEFAULT);
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptAlgorithm,cryptType)
          {
            @Override
            public void modified(Control control, WidgetVariable cryptAlgorithm)
            {
            }
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              String fileName;

              fileName = Dialogs.file(shell,
                                      Dialogs.FileDialogTypes.OPEN,
                                      BARControl.tr("Select public key file"),
                                      cryptPublicKeyFileName.getString(),
                                      new String[]{BARControl.tr("Public key"),"*.public",
                                                   BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                        ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                        : BARControl.listDirectory
                                     );
              if (fileName != null)
              {
                try
                {
                  cryptPublicKeyFileName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,cryptPublicKeyFileName);
                }
                catch (Exception exception)
                {
                  // ignored
                }
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
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptAlgorithm,cryptType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                cryptPasswordMode.set("default");
                BARServer.setJobOption(selectedJobData.uuid,cryptPasswordMode);

                cryptPassword.set("");
                BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptPasswordMode)
          {
            @Override
            public void modified(Control control, WidgetVariable cryptPasswordMode)
            {
              ((Button)control).setSelection(cryptPasswordMode.equals("default"));
            }
          });

          button = Widgets.newRadio(composite,BARControl.tr("ask"));
          button.setToolTipText(BARControl.tr("Input password for encryption."));
          Widgets.layout(button,0,1,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptAlgorithm,cryptType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                cryptPasswordMode.set("ask");
                BARServer.setJobOption(selectedJobData.uuid,cryptPasswordMode);

                cryptPassword.set("");
                BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptPasswordMode)
          {
            @Override
            public void modified(Control control, WidgetVariable cryptPasswordMode)
            {
              ((Button)control).setSelection(cryptPasswordMode.equals("ask"));
            }
          });

          button = Widgets.newRadio(composite,BARControl.tr("this"));
          button.setToolTipText(BARControl.tr("Use specified password for encryption."));
          Widgets.layout(button,0,2,TableLayoutData.W);
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptAlgorithm,cryptType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                cryptPasswordMode.set("config");
                BARServer.setJobOption(selectedJobData.uuid,cryptPasswordMode);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,cryptPasswordMode)
          {
            @Override
            public void modified(Control control, WidgetVariable cryptPasswordMode)
            {
              ((Button)control).setSelection(cryptPasswordMode.equals("config"));
            }
          });

          widgetCryptPassword1 = Widgets.newPassword(composite);
          widgetCryptPassword1.setToolTipText(BARControl.tr("Password used for encryption."));
          Widgets.layout(widgetCryptPassword1,0,3,TableLayoutData.WE);
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword1,cryptAlgorithm,cryptType,cryptPasswordMode)
          {
            @Override
            public void modified(Text text, WidgetVariable variables[])
            {
              boolean enabledFlag =    !variables[0].equals("none")
                                    && (variables[1].equals("none") || variables[1].equals("symmetric"))
                                    && variables[2].equals("config");
              if (!enabledFlag) text.setText("");
              Widgets.setEnabled(text,enabledFlag);
            }
          });
          widgetCryptPassword1.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();
              Color  color  = COLOR_MODIFIED;

              if (cryptPassword.getString().equals(string)) color = null;
              widget.setBackground(color);
            }
          });
          widgetCryptPassword1.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              String string1 = widgetCryptPassword1.getText();
              String string2 = widgetCryptPassword2.getText();
              if (string1.equals(string2))
              {
                try
                {
                  cryptPassword.set(string1);
                  BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                  widgetCryptPassword1.setBackground(null);
                  widgetCryptPassword2.setBackground(null);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          widgetCryptPassword1.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              String string1 = widgetCryptPassword1.getText();
              String string2 = widgetCryptPassword2.getText();
              if (string1.equals(string2))
              {
                try
                {
                  cryptPassword.set(string1);
                  BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                  widgetCryptPassword1.setBackground(null);
                  widgetCryptPassword2.setBackground(null);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword1,cryptPassword)
          {
            @Override
            public void modified(Text text, WidgetVariable[] variables)
            {
              super.modified(text,variables);

              boolean enabledFlag =    !cryptAlgorithm.getString().equals("none")
                                    && (cryptType.getString().equals("none") || cryptType.getString().equals("symmetric"))
                                    && cryptPasswordMode.getString().equals("config");
              if (!enabledFlag) text.setText("");
            }
          });

          label = Widgets.newLabel(composite,BARControl.tr("Repeat")+":");
          Widgets.layout(label,0,4,TableLayoutData.W);

          widgetCryptPassword2 = Widgets.newPassword(composite);
          widgetCryptPassword1.setToolTipText(BARControl.tr("Password used for encryption."));
          Widgets.layout(widgetCryptPassword2,0,5,TableLayoutData.WE);
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword2,cryptAlgorithm,cryptType,cryptPasswordMode)
          {
            @Override
            public void modified(Text text, WidgetVariable variables[])
            {
              boolean enabledFlag =    !variables[0].equals("none")
                                    && (variables[1].equals("none") || variables[1].equals("symmetric"))
                                    && variables[2].equals("config");
              if (!enabledFlag) text.setText("");
              Widgets.setEnabled(text,enabledFlag);
            }
          });
          widgetCryptPassword2.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();
              Color  color  = COLOR_MODIFIED;

              if (cryptPassword.getString().equals(string)) color = null;
              widget.setBackground(color);
            }
          });
          widgetCryptPassword2.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              String string1 = widgetCryptPassword1.getText();
              String string2 = widgetCryptPassword2.getText();
              if (string1.equals(string2))
              {
                try
                {
                  cryptPassword.set(string1);
                  BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                  widgetCryptPassword1.setBackground(null);
                  widgetCryptPassword2.setBackground(null);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          widgetCryptPassword2.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              String string1 = widgetCryptPassword1.getText();
              String string2 = widgetCryptPassword2.getText();
              if (string1.equals(string2))
              {
                try
                {
                  cryptPassword.set(string1);
                  BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                  widgetCryptPassword1.setBackground(null);
                  widgetCryptPassword2.setBackground(null);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
              else
              {
                Dialogs.error(shell,BARControl.tr("Crypt passwords are not equal!"));
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword2,cryptPassword)
          {
            @Override
            public void modified(Text text, WidgetVariable[] variables)
            {
              super.modified(text,variables);

              boolean enabledFlag =    !cryptAlgorithm.getString().equals("none")
                                    && (cryptType.getString().equals("none") || cryptType.getString().equals("symmetric"))
                                    && cryptPasswordMode.getString().equals("config");
              if (!enabledFlag) text.setText("");
            }
          });
        }

        // archive type
        label = Widgets.newLabel(tab,BARControl.tr("Mode")+":",Settings.hasNormalRole());
        Widgets.layout(label,7,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,Settings.hasNormalRole());
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,0.0,0.0,0.0,0.0,1.0,0.0}));
        Widgets.layout(composite,7,1,TableLayoutData.WE);
        {
          button = Widgets.newRadio(composite,BARControl.tr("normal"));
          button.setToolTipText(BARControl.tr("Normal mode: do not create incremental data files."));
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                archiveType.set("normal");
                BARServer.setJobOption(selectedJobData.uuid,archiveType);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archiveType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                archiveType.set("full");
                BARServer.setJobOption(selectedJobData.uuid,archiveType);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archiveType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                archiveType.set("incremental");
                BARServer.setJobOption(selectedJobData.uuid,archiveType);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archiveType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              try
              {
                archiveType.set("differential");
                BARServer.setJobOption(selectedJobData.uuid,archiveType);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,archiveType)
          {
            @Override
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
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();
              Color  color  = COLOR_MODIFIED;

              if (storageFileName.getString().equals(string)) color = null;
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text widget = (Text)selectionEvent.widget;

              try
              {
                storageFileName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                widget.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          text.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Text widget = (Text)focusEvent.widget;

              try
              {
                storageFileName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                widget.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,storageFileName));
          Widgets.addModifyListener(new WidgetModifyListener(text,storageFileName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
// TODO: ignore?
                // ignored
              }
            }
          });

          button = Widgets.newButton(composite,IMAGE_EDIT);
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              if (selectedJobData != null)
              {
                try
                {
                  String fileName = fileNameEdit(storageFileName.getString());
                  if (fileName != null)
                  {
                    storageFileName.set(fileName);
                    BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                  }
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            }
          });

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              if (selectedJobData != null)
              {
                String fileName;

                fileName = Dialogs.file(shell,
                                        Dialogs.FileDialogTypes.SAVE,
                                        BARControl.tr("Select storage file name"),
                                        storageFileName.getString(),
                                        new String[]{BARControl.tr("BAR files"),"*.bar",
                                                     BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*",
                                        ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                          ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                          : BARControl.listDirectory
                                       );
                if (fileName != null)
                {
                  try
                  {
                    storageFileName.set(fileName);
                    BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
              }
            }
          });
        }

        // incremental file name
        label = Widgets.newLabel(tab,BARControl.tr("Incremental file name")+":",Settings.hasExpertRole());
        Widgets.layout(label,9,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,Settings.hasExpertRole());
        composite.setLayout(new TableLayout(1.0,new double[]{1.0,0.0}));
        Widgets.layout(composite,9,1,TableLayoutData.WE);
        {
          text = Widgets.newText(composite);
          text.setToolTipText(BARControl.tr("Name of incremental data file. If no file name is given a name is derived automatically from the storage file name."));
          Widgets.layout(text,0,0,TableLayoutData.WE);
          text.addModifyListener(new ModifyListener()
          {
            @Override
            public void modifyText(ModifyEvent modifyEvent)
            {
              Text   widget = (Text)modifyEvent.widget;
              String string = widget.getText();
              Color  color  = COLOR_MODIFIED;

              if (incrementalListFileName.getString().equals(string)) color = null;
              widget.setBackground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
              Text   widget = (Text)selectionEvent.widget;
              String string = widget.getText();

              try
              {
                incrementalListFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,incrementalListFileName);
                widget.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
            }
          });
          text.addFocusListener(new FocusListener()
          {
            @Override
            public void focusGained(FocusEvent focusEvent)
            {
            }
            @Override
            public void focusLost(FocusEvent focusEvent)
            {
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();

              try
              {
                incrementalListFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,incrementalListFileName);
                widget.setBackground(null);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,incrementalListFileName));

          button = Widgets.newButton(composite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              String fileName;

              fileName = Dialogs.file(shell,
                                      Dialogs.FileDialogTypes.SAVE,
                                      BARControl.tr("Select incremental file"),
                                      incrementalListFileName.getString(),
                                      new String[]{BARControl.tr("BAR incremental data"),"*.bid",
                                                   BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                        ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                        : BARControl.listDirectory
                                     );
              if (fileName != null)
              {
                try
                {
                  incrementalListFileName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,incrementalListFileName);
                }
                catch (Exception exception)
                {
                  // ignored
                }
              }
            }
          });
        }

        // destination
        label = Widgets.newLabel(tab,BARControl.tr("Destination")+":");
        Widgets.layout(label,10,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0}));
        Widgets.layout(composite,10,1,TableLayoutData.W);
        {
          combo = Widgets.newOptionMenu(composite);
          combo.setToolTipText(BARControl.tr("Storage destination type:\n"+
                                             "  into file system\n"+
                                             "  on FTP server\n"+
// TODO:implemente/remove?                                             "  on SSH server with scp (secure copy)\n"+
                                             "  on SSH server with sftp (secure FTP)\n"+
                                             "  on WebDAV server\n"+
                                             "  on WebDAV secure server\n"+
                                             "  on SMB/CIFS server\n"+
                                             "  on CD\n"+
                                             "  on DVD\n"+
                                             "  on BD\n"+
                                             "  on generic device\n"
                                            )
                              );
          Widgets.setComboItems(combo,
                                new Object[]{BARControl.tr("file system"),StorageTypes.FILESYSTEM,
                                                           "ftp",         StorageTypes.FTP,
// TODO:implemente/remove?                                                           "scp",         StorageTypes.SCP,
                                                           "sftp",        StorageTypes.SFTP,
                                                           "webdav",      StorageTypes.WEBDAV,
                                                           "webdavs",     StorageTypes.WEBDAVS,
                                                           "smb",         StorageTypes.SMB,
                                                           "CD",          StorageTypes.CD,
                                                           "DVD",         StorageTypes.DVD,
                                                           "BD",          StorageTypes.BD,
                                             BARControl.tr("device"),     StorageTypes.DEVICE
                                            }
                          );
          Widgets.layout(combo,0,0,TableLayoutData.W);
          combo.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo        widget = (Combo)selectionEvent.widget;
              StorageTypes type   = Widgets.getSelectedComboItem(widget);

              try
              {
                switch (type)
                {
                  case CD:
                {
                  if (   archivePartSizeFlag.getBoolean()
                      && (Units.parseByteSize(volumeSize.getString()) <= 0)
                     )
                  {
                    Dialogs.warning(shell,BARControl.tr("When writing to a CD without setting medium size\nthe resulting archive file may not fit on medium."));
                  }

                  long size = (long)((double)Units.parseByteSize(volumeSize.getString())*MAX_VOLUME_SIZE_ECC);
                  if (   ecc.getBoolean()
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((size%archivePartSize.getLong()) > 0)
                      && ((double)(size%archivePartSize.getLong()) < (double)archivePartSize.getLong()*0.5)
                     )
                  {
                    Dialogs.warning(shell,
                                    BARControl.tr("When writing to a CD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes (~20%).\n"+
                                                  "\n"+
                                                  "Good settings may be:\n"+
                                                  "- part size 215M, size 430M, medium 540\n"+
                                                  "- part size 260M, size 520M, medium 650\n"+
                                                  "- part size 280M, size 560M, medium 700\n"+
                                                  "- part size 250M, size 650M, medium 800\n"+
                                                  "- part size 240M, size 720M, medium 900\n"
                                                 )
                                   );
                  }
                }
                  case DVD:
                {
                  if (   archivePartSizeFlag.getBoolean()
                      && (Units.parseByteSize(volumeSize.getString()) <= 0)
                     )
                  {
                    Dialogs.warning(shell,BARControl.tr("When writing to a DVD without setting medium size\n"+
                                                        "the resulting archive file may not fit on medium."
                                                       )
                                   );
                  }

                  long size = (long)((double)Units.parseByteSize(volumeSize.getString())*MAX_VOLUME_SIZE_ECC);
                  if (   ecc.getBoolean()
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((size%archivePartSize.getLong()) > 0)
                      && ((double)(size%archivePartSize.getLong()) < (double)archivePartSize.getLong()*0.5)
                     )
                  {
                    Dialogs.warning(shell,
                                    BARControl.tr("When writing to a DVD with error-correction codes enabled\n"+
                                                  "some free space should be available on medium for error-correction codes (~20%).\n"+
                                                  "\n"+
                                                  "Good settings may be:\n"+
                                                  "- part size 470M, size 3.7G,\tmedium 4.7G\n"+
                                                  "- part size 425M, size 6.8G,\tmedium 8.5G\n"+
                                                  "- part size 470M, size 7.52G,\tmedium 9.4G\n"+
                                                  "- part size 660M, size 10.56G,\tmedium 13.2G\n"+
                                                  "- part size 850M, size 13.6G,\tmedium 17G"
                                                 )
                                   );
                  }
                }
                  case BD:
                {
                  if (   archivePartSizeFlag.getBoolean()
                      && (Units.parseByteSize(volumeSize.getString()) <= 0)
                     )
                  {
                    Dialogs.warning(shell,BARControl.tr("When writing to a BD without setting medium size\n"+
                                                        "the resulting archive file may not fit on medium."
                                                       )
                                   );
                  }

                  long size = (long)((double)Units.parseByteSize(volumeSize.getString())*MAX_VOLUME_SIZE_ECC);
                  if (   ecc.getBoolean()
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((size%archivePartSize.getLong()) > 0)
                      && ((double)(size%archivePartSize.getLong()) < (double)archivePartSize.getLong()*0.5)
                     )
                  {
                    Dialogs.warning(shell,
                                    BARControl.tr("When writing to a BD with error-correction codes enabled\n"+
                                                  "some free space should be available on medium for error-correction codes (~20%).\n"+
                                                  "\n"+
                                                  "Good settings may be:\n"+
                                                  "- part size 1G,\t\tsize 20G,\t\tmedium 25G\n"+
                                                  "- part size 2G,\t\tsize 40G,\t\tmedium 50G\n"+
                                                  "- part size 5G,\t\tsize 80G,\t\tmedium 100G\n"+
                                                  "- part size 6.4G,\tsize 102.4G,\tmedium 128G\n"
                                                 )
                                   );
                  }
                }
                }

                storageType.set(type);
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(combo,storageType));

          button = Widgets.newCheckbox(composite,BARControl.tr("on master"));
          button.setToolTipText(BARControl.tr("Enable for storage through master."));
          button.setEnabled(false);
          Widgets.layout(button,0,10,TableLayoutData.DEFAULT);
          Widgets.addEventListener(new WidgetEventListener(button,selectJobEvent)
          {
            @Override
            public void trigger(Control control)
            {
              Widgets.setEnabled(control,(selectedJobData != null) && !slaveHostName.getString().isEmpty());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,slaveHostName)
          {
            @Override
            public void modified(Control control, WidgetVariable slaveHostName)
            {
              Widgets.setEnabled(control,!slaveHostName.getString().isEmpty());
            }
          });
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              try
              {
                storageOnMasterFlag.set(widget.getSelection());
                BARServer.setJobOption(selectedJobData.uuid,storageOnMasterFlag);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageOnMasterFlag));
        }

        // destination file system
        BARWidgets.File widgetFile = new BARWidgets.File(tab,
                                                         EnumSet.allOf(BARWidgets.File.WidgetTypes.class),
                                                         maxStorageSize,
                                                         archiveFileMode
                                                        );
        Widgets.layout(widgetFile,11,1,TableLayoutData.WE|TableLayoutData.N);
        {
          Widgets.addModifyListener(new WidgetModifyListener(widgetFile.maxStorageSize,maxStorageSize)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,maxStorageSize);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }

            @Override
            public String getString(WidgetVariable variable)
            {
              return (variable.getLong() > 0) ? Units.formatByteSize(variable.getLong()) : "";
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetFile.archiveFileMode,archiveFileMode)
          {
            @Override
            public void modified(Combo combo, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetFile,storageType)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              Widgets.setVisible(control,variable.getEnum() == StorageTypes.FILESYSTEM);
            }
          });
        }

        // destination ftp
        BARWidgets.FTP widgetFTP = new BARWidgets.FTP(tab,
                                                      storageHostName,
                                                      storageLoginName,
                                                      storageLoginPassword,
                                                      archiveFileMode
                                                     );
        Widgets.layout(widgetFTP,11,1,TableLayoutData.WE|TableLayoutData.N);
        {
          Widgets.addModifyListener(new WidgetModifyListener(widgetFTP.hostName,storageHostName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetFTP.loginName,storageLoginName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetFTP.loginPassword,storageLoginPassword)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetFTP.archiveFileMode,archiveFileMode)
          {
            @Override
            public void modified(Combo combo, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetFTP,storageType)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              Widgets.setVisible(control,variable.getEnum() == StorageTypes.FTP);
            }
          });
        }

        // destination scp/sftp
        BARWidgets.SFTP widgetSFTP = new BARWidgets.SFTP(tab,
                                                         storageHostName,
                                                         storageHostPort,
                                                         storageLoginName,
                                                         storageLoginPassword,
                                                         sshPublicKeyFileName,
                                                         sshPrivateKeyFileName,
                                                         archiveFileMode
                                                        );
        Widgets.layout(widgetSFTP,11,1,TableLayoutData.WE|TableLayoutData.N);
        {
          Widgets.addModifyListener(new WidgetModifyListener(widgetSFTP.hostName,storageHostName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSFTP.hostPort,storageHostPort)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSFTP.loginName,storageLoginName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSFTP.loginPassword,storageLoginPassword)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSFTP.publicKey,sshPublicKeyFileName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSFTP.privateKey,sshPrivateKeyFileName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,sshPrivateKeyFileName);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSFTP.archiveFileMode,archiveFileMode)
          {
            @Override
            public void modified(Combo combo, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSFTP,storageType)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              Widgets.setVisible(control,
                                    (variable.getEnum() == StorageTypes.SCP)
                                 || (variable.getEnum() == StorageTypes.SFTP)
                                );
            }
          });
        }

        // destination WebDAV
        BARWidgets.WebDAV widgetWebDAV = new BARWidgets.WebDAV(tab,
                                                               storageHostName,
                                                               storageHostPort,
                                                               storageLoginName,
                                                               storageLoginPassword,
                                                               sshPublicKeyFileName,
                                                               sshPrivateKeyFileName,
                                                               archiveFileMode
                                                              );
        Widgets.layout(widgetWebDAV,11,1,TableLayoutData.WE|TableLayoutData.N);
        {
          Widgets.addModifyListener(new WidgetModifyListener(widgetWebDAV.hostName,storageHostName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetWebDAV.hostPort,storageHostPort)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetWebDAV.loginName,storageLoginName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetWebDAV.loginPassword,storageLoginPassword)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetWebDAV.publicKey,sshPublicKeyFileName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetWebDAV.privateKey,sshPrivateKeyFileName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,sshPrivateKeyFileName);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetWebDAV.archiveFileMode,archiveFileMode)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetWebDAV,storageType)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              Widgets.setVisible(control,
                                    (variable.getEnum() == StorageTypes.WEBDAV)
                                 || (variable.getEnum() == StorageTypes.WEBDAVS)
                                );
            }
          });
        }

        // destination SMB
        BARWidgets.SMB widgetSMB = new BARWidgets.SMB(tab,
                                                      storageHostName,
                                                      storageLoginName,
                                                      storageLoginPassword,
                                                      storageShareName,
                                                      archiveFileMode
                                                     );
        Widgets.layout(widgetSMB,11,1,TableLayoutData.WE|TableLayoutData.N);
        {
          Widgets.addModifyListener(new WidgetModifyListener(widgetSMB.hostName,storageHostName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSMB.loginName,storageLoginName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSMB.loginPassword,storageLoginPassword)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSMB.shareName,storageShareName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSMB.archiveFileMode,archiveFileMode)
          {
            @Override
            public void modified(Combo combo, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetSMB,storageType)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              Widgets.setVisible(control,variable.getEnum() == StorageTypes.SMB);
            }
          });
        }

        // destination cd/dvd/bd
        BARWidgets.Optical widgetOptical = new BARWidgets.Optical(tab,
                                                                  storageDeviceName,
                                                                  volumeSize,
                                                                  ecc,
                                                                  blank,
                                                                  waitFirstVolume,
                                                                  archivePartSizeFlag,
                                                                  archivePartSize
                                                                 );
        Widgets.layout(widgetOptical,11,1,TableLayoutData.WE|TableLayoutData.N);
        {
          Widgets.addModifyListener(new WidgetModifyListener(widgetOptical.deviceName,storageDeviceName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetOptical.volumeSize,volumeSize)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,volumeSize);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetOptical.ecc,ecc)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,ecc);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetOptical.blank,blank)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,blank);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetOptical.waitFirstVolume,waitFirstVolume)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,waitFirstVolume);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetOptical,storageType)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              Widgets.setVisible(control,   (variable.getEnum() == StorageTypes.CD )
                                         || (variable.getEnum() == StorageTypes.DVD)
                                         || (variable.getEnum() == StorageTypes.BD )
                                );
            }
          });
        }

        // destination device
        BARWidgets.Device widgetDevice = new BARWidgets.Device(tab,
                                                               storageDeviceName,
                                                               volumeSize
                                                              );
        Widgets.layout(widgetDevice,11,1,TableLayoutData.WE|TableLayoutData.N);
        {
          Widgets.addModifyListener(new WidgetModifyListener(widgetDevice.deviceName,storageDeviceName)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetDevice.volumeSize,volumeSize)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              try
              {
                BARServer.setJobOption(selectedJobData.uuid,volumeSize);
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(widgetDevice,storageType)
          {
            @Override
            public void modified(Control control, WidgetVariable variable)
            {
              Widgets.setVisible(control,variable.getEnum() == StorageTypes.DEVICE);
            }
          });
        }
      }
      Widgets.addModifyListener(new WidgetModifyListener(archiveName)
      {
        @Override
        public void parse(String value)
        {
          URIParts uriParts = new URIParts(value);

          storageType.set         (uriParts.type         );
          storageLoginName.set    (uriParts.loginName    );
          storageLoginPassword.set(uriParts.loginPassword);
          storageHostName.set     (uriParts.hostName     );
          storageHostPort.set     (uriParts.hostPort     );
          storageShareName.set    (uriParts.shareName    );
          storageDeviceName.set   (uriParts.deviceName   );
          storageFileName.set     (uriParts.fileName     );
        }
      });

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Scripts"),Settings.hasExpertRole());
      tab.setLayout(new TableLayout(1.0,1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        tabFolder = Widgets.newTabFolder(tab);
        Widgets.layout(tabFolder,0,0,TableLayoutData.NSWE);
        {
          subTab = Widgets.addTab(tabFolder,BARControl.tr("Local"),Settings.hasExpertRole());
          subTab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0,1.0},1.0));
          Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
          {
            // pre-script
            label = Widgets.newLabel(subTab,BARControl.tr("Pre-script")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            composite = Widgets.newComposite(subTab,SWT.NONE,4);
            composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
            Widgets.layout(composite,1,0,TableLayoutData.NSWE);
            {
              styledText = Widgets.newStyledText(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
              styledText.setToolTipText(BARControl.tr("Command or script to execute before start of job.\n\nMacros:\n\n%name - job name\n%archive - archive name\n%type - archive type\n%file - archive file name\n%directory - archive directory\n\nAdditional time macros are available."));
              Widgets.layout(styledText,0,0,TableLayoutData.NSWE);
              styledText.addModifyListener(new ModifyListener()
              {
                @Override
                public void modifyText(ModifyEvent modifyEvent)
                {
                  StyledText widget = (StyledText)modifyEvent.widget;
                  String     string = widget.getText();
                  Color      color  = COLOR_MODIFIED;

                  if (preCommand.equals(string.replace(widget.getLineDelimiter(),"\n"))) color = null;
                  widget.setBackground(color);
                }
              });
              styledText.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                  StyledText widget = (StyledText)selectionEvent.widget;
                  String     text   = widget.getText();

                  try
                  {
                    preCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                    BARServer.setJobOption(selectedJobData.uuid,preCommand);
                    widget.setBackground(null);
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                }
              });
              styledText.addFocusListener(new FocusListener()
              {
                @Override
                public void focusGained(FocusEvent focusEvent)
                {
                }
                @Override
                public void focusLost(FocusEvent focusEvent)
                {
                  StyledText widget = (StyledText)focusEvent.widget;
                  String     text   = widget.getText();

                  try
                  {
                    preCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                    BARServer.setJobOption(selectedJobData.uuid,preCommand);
                    widget.setBackground(null);
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
              });
              Widgets.addModifyListener(new WidgetModifyListener(styledText,preCommand));

              button = Widgets.newButton(composite,BARControl.tr("Test")+"\u2026");
              button.setToolTipText(BARControl.tr("Test script."));
              Widgets.layout(button,1,0,TableLayoutData.E,0,0,2,2);
              button.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                  testScript("pre-command",preCommand.getString());
                }
              });
            }

            // post-script
            label = Widgets.newLabel(subTab,BARControl.tr("Post-script")+":");
            Widgets.layout(label,2,0,TableLayoutData.W);

            composite = Widgets.newComposite(subTab,SWT.NONE,4);
            composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
            Widgets.layout(composite,3,0,TableLayoutData.NSWE);
            {
              styledText = Widgets.newStyledText(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
              styledText.setToolTipText(BARControl.tr("Command or script to execute after termination of job.\n\nMacros:\n\n%name - job name\n%archive - archive name\n%type - archive type\n%T - archive type short\n%file - archive file name\n%directory - archive directory\n%error - error code\n%message - message\n%nextSchedule - next schedule [s]\nnextScheduleDateTime - next schedule date/time [s]\n\nAdditional time macros are available."));
              Widgets.layout(styledText,0,0,TableLayoutData.NSWE);
              styledText.addModifyListener(new ModifyListener()
              {
                @Override
                public void modifyText(ModifyEvent modifyEvent)
                {
                  StyledText widget = (StyledText)modifyEvent.widget;
                  String     string = widget.getText();
                  Color      color  = COLOR_MODIFIED;

                  if (postCommand.equals(string.replace(widget.getLineDelimiter(),"\n"))) color = null;
                  widget.setBackground(color);
                }
              });
              styledText.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                  StyledText widget = (StyledText)selectionEvent.widget;
                  String     text   = widget.getText();

                  try
                  {
                    postCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                    BARServer.setJobOption(selectedJobData.uuid,postCommand);
                    widget.setBackground(null);
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                }
              });
              styledText.addFocusListener(new FocusListener()
              {
                @Override
                public void focusGained(FocusEvent focusEvent)
                {
                }
                @Override
                public void focusLost(FocusEvent focusEvent)
                {
                  StyledText widget = (StyledText)focusEvent.widget;
                  String     text   = widget.getText();

                  try
                  {
                    postCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                    BARServer.setJobOption(selectedJobData.uuid,postCommand);
                    widget.setBackground(null);
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
              });
              Widgets.addModifyListener(new WidgetModifyListener(styledText,postCommand));

              button = Widgets.newButton(composite,BARControl.tr("Test")+"\u2026");
              button.setToolTipText(BARControl.tr("Test script."));
              Widgets.layout(button,1,0,TableLayoutData.E,0,0,2,2);
              button.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                  testScript("post-command",postCommand.getString());
                }
              });
            }
          }

          subTab = Widgets.addTab(tabFolder,BARControl.tr("Slave"),Settings.hasExpertRole());
          subTab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0,1.0},1.0));
          Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
          {
            // pre-script
            label = Widgets.newLabel(subTab,BARControl.tr("Pre-script")+":");
            Widgets.layout(label,0,0,TableLayoutData.W);

            composite = Widgets.newComposite(subTab,SWT.NONE,4);
            composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
            Widgets.layout(composite,1,0,TableLayoutData.NSWE);
            {
              styledText = Widgets.newStyledText(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
              styledText.setToolTipText(BARControl.tr("Command or script to execute before start of job on slave.\n\nMacros:\n\n%name - job name\n%archive - archive name\n%type - archive type\n%file - archive file name\n%directory - archive directory\n\nAdditional time macros are available."));
              Widgets.setEnabled(styledText,false);
              Widgets.layout(styledText,0,0,TableLayoutData.NSWE);
              styledText.addModifyListener(new ModifyListener()
              {
                @Override
                public void modifyText(ModifyEvent modifyEvent)
                {
                  StyledText widget = (StyledText)modifyEvent.widget;
                  String     string = widget.getText();
                  Color      color  = COLOR_MODIFIED;

                  if (slavePreCommand.equals(string.replace(widget.getLineDelimiter(),"\n"))) color = null;
                  widget.setBackground(color);
                }
              });
              styledText.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                  StyledText widget = (StyledText)selectionEvent.widget;
                  String     text   = widget.getText();

                  try
                  {
                    slavePreCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                    BARServer.setJobOption(selectedJobData.uuid,slavePreCommand);
                    widget.setBackground(null);
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                }
              });
              styledText.addFocusListener(new FocusListener()
              {
                @Override
                public void focusGained(FocusEvent focusEvent)
                {
                }
                @Override
                public void focusLost(FocusEvent focusEvent)
                {
                  StyledText widget = (StyledText)focusEvent.widget;
                  String     text   = widget.getText();

                  try
                  {
                    slavePreCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                    BARServer.setJobOption(selectedJobData.uuid,slavePreCommand);
                    widget.setBackground(null);
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
              });
              Widgets.addModifyListener(new WidgetModifyListener(styledText,slavePreCommand));
              Widgets.addModifyListener(new WidgetModifyListener(styledText,slaveHostName)
              {
                @Override
                public void modified(Control control, WidgetVariable slaveHostName)
                {
                  if (!slaveHostName.getString().isEmpty())
                  {
                    Widgets.setEnabled(control,true);
                    control.setBackground(null);
                  }
                  else
                  {
                    Widgets.setEnabled(control,false);
                    control.setBackground(COLOR_DISABLED_BACKGROUND);
                  }
                }
              });

              button = Widgets.newButton(composite,BARControl.tr("Test")+"\u2026");
              button.setToolTipText(BARControl.tr("Test script."));
              Widgets.setEnabled(button,false);
              Widgets.layout(button,1,0,TableLayoutData.E,0,0,2,2);
              button.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                  testScript("pre-command",slavePreCommand.getString());
                }
              });
              Widgets.addModifyListener(new WidgetModifyListener(button,slaveHostName)
              {
                @Override
                public void modified(Control control, WidgetVariable slaveHostName)
                {
                  Widgets.setEnabled(control,!slaveHostName.getString().isEmpty());
                }
              });
            }

            // post-script
            label = Widgets.newLabel(subTab,BARControl.tr("Post-script")+":");
            Widgets.layout(label,2,0,TableLayoutData.W);

            composite = Widgets.newComposite(subTab,SWT.NONE,4);
            composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
            Widgets.layout(composite,3,0,TableLayoutData.NSWE);
            {
              styledText = Widgets.newStyledText(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
              styledText.setToolTipText(BARControl.tr("Command or script to execute after termination of job on slave.\n\nMacros:\n\n%name - job name\n%archive - archive name\n%type - archive type\n%T - archive type short\n%file - archive file name\n%directory - archive directory\n%error - error code\n%message - message\n%nextSchedule - next schedule [s]\n%nextScheduleDateTime - next schedule date/time [s]\nAdditional time macros are available."));
              Widgets.setEnabled(styledText,false);
              Widgets.layout(styledText,0,0,TableLayoutData.NSWE);
              styledText.addModifyListener(new ModifyListener()
              {
                @Override
                public void modifyText(ModifyEvent modifyEvent)
                {
                  StyledText widget = (StyledText)modifyEvent.widget;
                  String     string = widget.getText();
                  Color      color  = COLOR_MODIFIED;

                  if (slavePostCommand.equals(string.replace(widget.getLineDelimiter(),"\n"))) color = null;
                  widget.setBackground(color);
                }
              });
              styledText.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                  StyledText widget = (StyledText)selectionEvent.widget;
                  String     text   = widget.getText();

                  try
                  {
                    slavePostCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                    BARServer.setJobOption(selectedJobData.uuid,slavePostCommand);
                    widget.setBackground(null);
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                }
              });
              styledText.addFocusListener(new FocusListener()
              {
                @Override
                public void focusGained(FocusEvent focusEvent)
                {
                }
                @Override
                public void focusLost(FocusEvent focusEvent)
                {
                  StyledText widget = (StyledText)focusEvent.widget;
                  String     text   = widget.getText();

                  try
                  {
                    slavePostCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                    BARServer.setJobOption(selectedJobData.uuid,slavePostCommand);
                    widget.setBackground(null);
                  }
                  catch (Exception exception)
                  {
                    // ignored
                  }
                }
              });
              Widgets.addModifyListener(new WidgetModifyListener(styledText,slavePostCommand));
              Widgets.addModifyListener(new WidgetModifyListener(styledText,slaveHostName)
              {
                @Override
                public void modified(Control control, WidgetVariable slaveHostName)
                {
                  if (!slaveHostName.getString().isEmpty())
                  {
                    Widgets.setEnabled(control,true);
                    control.setBackground(null);
                  }
                  else
                  {
                    Widgets.setEnabled(control,false);
                    control.setBackground(COLOR_DISABLED_BACKGROUND);
                  }
                }
              });

              button = Widgets.newButton(composite,BARControl.tr("Test")+"\u2026");
              button.setToolTipText(BARControl.tr("Test script."));
              Widgets.layout(button,1,0,TableLayoutData.E,0,0,2,2);
              button.addSelectionListener(new SelectionListener()
              {
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                  testScript("post-command",slavePostCommand.getString());
                }
              });
              Widgets.addModifyListener(new WidgetModifyListener(button,slaveHostName)
              {
                @Override
                public void modified(Control control, WidgetVariable slaveHostName)
                {
                  Widgets.setEnabled(control,!slaveHostName.getString().isEmpty());
                }
              });
            }
          }
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Schedule"));
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // schedule table
        widgetScheduleTable = Widgets.newTable(tab,SWT.CHECK);
        Widgets.layout(widgetScheduleTable,0,0,TableLayoutData.NSWE);
        SelectionListener scheduleTableColumnSelectionListener = new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn            tableColumn            = (TableColumn)selectionEvent.widget;
            ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleTable,tableColumn);
            Widgets.sortTableColumn(widgetScheduleTable,tableColumn,scheduleDataComparator);
          }
        };
        Listener scheduleListColumnResizeListener = new Listener()
        {
          public void handleEvent(Event event)
          {
            Settings.scheduleTableColumns = new Settings.ColumnWidth(Widgets.getTableColumnWidth(widgetScheduleTable));
          }
        };
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,0,BARControl.tr("Date"),        SWT.LEFT,120,false);
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn.addListener(SWT.Resize,scheduleListColumnResizeListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,1,BARControl.tr("Week days"),   SWT.LEFT,250,true );
        Widgets.sortTableColumn(widgetScheduleTable,tableColumn,new ScheduleDataComparator(widgetScheduleTable,tableColumn));
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn.addListener(SWT.Resize,scheduleListColumnResizeListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,2,BARControl.tr("Time"),        SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn.addListener(SWT.Resize,scheduleListColumnResizeListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,3,BARControl.tr("Archive type"),SWT.LEFT,100,true );
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn.addListener(SWT.Resize,scheduleListColumnResizeListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,4,BARControl.tr("Begin"),       SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn.addListener(SWT.Resize,scheduleListColumnResizeListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,5,BARControl.tr("End"),         SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn.addListener(SWT.Resize,scheduleListColumnResizeListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,6,BARControl.tr("Custom text"), SWT.LEFT, 90,true );
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn.addListener(SWT.Resize,scheduleListColumnResizeListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,7,BARControl.tr("Test"),        SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn.addListener(SWT.Resize,scheduleListColumnResizeListener);
        Widgets.setTableColumnWidth(widgetScheduleTable,Settings.scheduleTableColumns.width);

        widgetScheduleTable.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Table table = (Table)selectionEvent.widget;

            int index = table.getSelectionIndex();
            if (index >= 0)
            {
              TableItem    tableItem    = table.getItem(index);
              ScheduleData scheduleData = (ScheduleData)tableItem.getData();

              try
              {
                if (scheduleData.enabled != tableItem.getChecked())
                {
                  scheduleData.enabled = tableItem.getChecked();
                  BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"enabled",scheduleData.enabled);
                }
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          }
        });
        widgetScheduleTable.addMouseListener(new MouseListener()
        {
          @Override
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
            scheduleEditEntry();
          }
          @Override
          public void mouseDown(final MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });
        widgetScheduleTable.addMouseTrackListener(new MouseTrackListener()
        {
          @Override
          public void mouseEnter(MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseExit(MouseEvent mouseEvent)
          {
            if (widgetScheduleTableToolTip != null)
            {
              widgetScheduleTableToolTip.dispose();
              widgetScheduleTableToolTip = null;
            }
          }
          @Override
          public void mouseHover(MouseEvent mouseEvent)
          {
            Table     table     = (Table)mouseEvent.widget;
            TableItem tableItem = table.getItem(new Point(mouseEvent.x,mouseEvent.y));

            if (widgetScheduleTableToolTip != null)
            {
              widgetScheduleTableToolTip.dispose();
              widgetScheduleTableToolTip = null;
            }

            // show if tree item available and mouse is in the right side
            if ((tableItem != null) && (mouseEvent.x > table.getBounds().width/2))
            {
              ScheduleData scheduleData = (ScheduleData)tableItem.getData();
              Label        label;

              widgetScheduleTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
              widgetScheduleTableToolTip.setBackground(COLOR_INFO_BACKGROUND);
              widgetScheduleTableToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
              Widgets.layout(widgetScheduleTableToolTip,0,0,TableLayoutData.NSWE);
              widgetScheduleTableToolTip.addMouseTrackListener(new MouseTrackListener()
              {
                @Override
                public void mouseEnter(MouseEvent mouseEvent)
                {
                }
                @Override
                public void mouseExit(MouseEvent mouseEvent)
                {
                  widgetScheduleTableToolTip.dispose();
                  widgetScheduleTableToolTip = null;
                }
                @Override
                public void mouseHover(MouseEvent mouseEvent)
                {
                }
              });

              label = Widgets.newLabel(widgetScheduleTableToolTip,BARControl.tr("Last created")+":");
              label.setForeground(COLOR_INFO_FOREGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,0,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetScheduleTableToolTip,(scheduleData.lastExecutedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(scheduleData.lastExecutedDateTime*1000L)) : "-");
              label.setForeground(COLOR_INFO_FOREGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,0,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetScheduleTableToolTip,BARControl.tr("Total entities")+":");
              label.setForeground(COLOR_INFO_FOREGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,1,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetScheduleTableToolTip,String.format("%d",scheduleData.totalEntities));
              label.setForeground(COLOR_INFO_FOREGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,1,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetScheduleTableToolTip,BARControl.tr("Total entries")+":");
              label.setForeground(COLOR_INFO_FOREGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,2,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetScheduleTableToolTip,String.format("%d",scheduleData.totalEntryCount));
              label.setForeground(COLOR_INFO_FOREGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,2,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetScheduleTableToolTip,BARControl.tr("Total size")+":");
              label.setForeground(COLOR_INFO_FOREGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,3,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetScheduleTableToolTip,String.format(BARControl.tr("{0} {0,choice,0#bytes|1#byte|1<bytes} ({1})",scheduleData.totalEntrySize,Units.formatByteSize(scheduleData.totalEntrySize))));
              label.setForeground(COLOR_INFO_FOREGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,3,1,TableLayoutData.WE);

              Point size = widgetScheduleTableToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
              Point point = widgetScheduleTable.toDisplay(mouseEvent.x+16,mouseEvent.y);
              widgetScheduleTableToolTip.setBounds(point.x,point.y,size.x,size.y);
              widgetScheduleTableToolTip.setVisible(true);
            }
          }
        });
        widgetScheduleTable.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
            if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
            {
              Widgets.invoke(widgetScheduleTableAdd);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
            {
              Widgets.invoke(widgetScheduleTableRemove);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
            {
              Widgets.invoke(widgetScheduleTableEdit);
            }
          }
        });

        menu = Widgets.newPopupMenu(shell);
        {
          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add")+"\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleAddEntry();
            }
          });

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Edit")+"\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleEditEntry();
            }
          });

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleCloneEntry();
            }
          });

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove")+"\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleRemoveEntry();
            }
          });

          Widgets.addMenuItemSeparator(menu,Settings.hasExpertRole());

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Trigger now"),Settings.hasExpertRole());
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleTriggerEntry();
            }
          });
        }
        widgetScheduleTable.setMenu(menu);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          widgetScheduleTableAdd = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
          widgetScheduleTableAdd.setToolTipText(BARControl.tr("Add new schedule entry."));
          Widgets.layout(widgetScheduleTableAdd,0,0,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
          widgetScheduleTableAdd.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleAddEntry();
            }
          });

          widgetScheduleTableEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
          widgetScheduleTableEdit.setToolTipText(BARControl.tr("Edit schedule entry."));
          Widgets.layout(widgetScheduleTableEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
          widgetScheduleTableEdit.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleEditEntry();
            }
          });

          button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
          button.setToolTipText(BARControl.tr("Clone schedule entry."));
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleCloneEntry();
            }
          });

          widgetScheduleTableRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
          widgetScheduleTableRemove.setToolTipText(BARControl.tr("Remove schedule entry."));
          Widgets.layout(widgetScheduleTableRemove,0,3,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
          widgetScheduleTableRemove.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleRemoveEntry();
            }
          });
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Persistence"),Settings.hasExpertRole());
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // persistence tree
        widgetPersistenceTree = Widgets.newTree(tab);
        widgetPersistenceTree.setLayout(new TableLayout(1.0,new double[]{1.0,0.0,0.0,0.0,0.0,0.0}));
        Widgets.layout(widgetPersistenceTree,0,0,TableLayoutData.NSWE);
        Widgets.addTreeColumn(widgetPersistenceTree,BARControl.tr("Archive type"),SWT.LEFT, 100,true );
        Widgets.addTreeColumn(widgetPersistenceTree,BARControl.tr("min. keep"   ),SWT.RIGHT, 90,true );
        Widgets.addTreeColumn(widgetPersistenceTree,BARControl.tr("max. keep"   ),SWT.RIGHT, 90,true );
        Widgets.addTreeColumn(widgetPersistenceTree,BARControl.tr("max. age"    ),SWT.RIGHT, 90,true );
        Widgets.addTreeColumn(widgetPersistenceTree,BARControl.tr("Created"     ),SWT.LEFT, 140,true );
        Widgets.addTreeColumn(widgetPersistenceTree,BARControl.tr("Age [days]"  ),SWT.RIGHT, 90,true );
        Widgets.addTreeColumn(widgetPersistenceTree,BARControl.tr("Total size"  ),SWT.RIGHT,120,true );
        Widgets.setTreeColumnWidth(widgetPersistenceTree,Settings.persistenceTreeColumns.width);

        widgetPersistenceTree.addListener(SWT.EraseItem, new Listener()
        {
          public void handleEvent(Event event)
          {
            TreeItem  treeItem   = (TreeItem)event.item;

            Rectangle clientArea = widgetPersistenceTree.getBounds();
            Rectangle bounds     = event.getBounds();
            GC        gc         = event.gc;

            // enable alpha blending if possible
            gc.setAdvanced(true);
            if (gc.getAdvanced()) gc.setAlpha(127);

            // draw background
            if      (treeItem.getData() instanceof PersistenceData)
            {
              // nothing to do
            }
            else if (treeItem.getData() instanceof EntityIndexData)
            {
              EntityIndexData entityIndexData = (EntityIndexData)treeItem.getData();

              if (entityIndexData.inTransit)
              {
                Color background = gc.getBackground();
                gc.setBackground(COLOR_IN_TRANSIT);
                gc.fillRectangle(bounds.x,bounds.y,clientArea.width,bounds.height);
                gc.setBackground(background);
              }
            }

            event.detail &= ~SWT.BACKGROUND;
          }
        });
/*
TODO: implement delete entity
        widgetPersistenceTree.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Tree tree = (Tree)selectionEvent.widget;

            TreeItem treeItems[] = tree.getSelection();
            if (treeItems.length >= 0)
            {
            }
          }
        });

*/
        widgetPersistenceTree.addMouseListener(new MouseListener()
        {
          @Override
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
            persistenceListEdit();
          }
          @Override
          public void mouseDown(final MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });
        widgetPersistenceTree.addMouseTrackListener(new MouseTrackListener()
        {
          @Override
          public void mouseEnter(MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseExit(MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseHover(MouseEvent mouseEvent)
          {
            Tree     tree     = (Tree)mouseEvent.widget;
            TreeItem treeItem = tree.getItem(new Point(mouseEvent.x,mouseEvent.y));

            if (widgetPersistenceTreeToolTip != null)
            {
              widgetPersistenceTreeToolTip.dispose();
              widgetPersistenceTreeToolTip = null;
            }

            // show tooltip if tree item available and mouse is in the right side
            if ((treeItem != null) && (mouseEvent.x > tree.getBounds().width/2))
            {
              Point point = display.getCursorLocation();
              if (point.x > 16) point.x -= 16;
              if (point.y > 16) point.y -= 16;

              if      (treeItem.getData() instanceof PersistenceData)
              {
                // nothing to do
              }
              else if (treeItem.getData() instanceof EntityIndexData)
              {
// TODO: hide tooltop correct?
                showEntityIndexToolTip((EntityIndexData)treeItem.getData(),point.x,point.y);
              }
            }
          }
        });
        widgetPersistenceTree.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
            if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
            {
              Widgets.invoke(widgetPersistenceTreeAdd);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
            {
              Widgets.invoke(widgetPersistenceTreeRemove);
            }
            else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
            {
              Widgets.invoke(widgetPersistenceTreeEdit);
            }
          }
        });

        menu = Widgets.newPopupMenu(shell);
        menu.addListener(SWT.Show, new Listener()
        {
          public void handleEvent(Event event)
          {
            Menu     menu = (Menu)event.widget;
            MenuItem menuItem;

            // get tree item
            Point p = widgetPersistenceTree.toControl(Display.getCurrent().getCursorLocation());
            final TreeItem treeItem = widgetPersistenceTree.getItem(p);

            Widgets.removeAllMenuItems(menu);
            if (treeItem != null)
            {
              if      (treeItem.getData() instanceof PersistenceData)
              {
                // persistence context menu
                final PersistenceData persistenceData = (PersistenceData)treeItem.getData();

                menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add")+"\u2026");
                menuItem.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    persistenceListAdd();
                  }
                });

                menuItem = Widgets.addMenuItem(menu,BARControl.tr("Edit")+"\u2026");
                menuItem.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    persistenceListEdit();
                  }
                });

                menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
                menuItem.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    persistenceListClone();
                  }
                });

                menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove")+"\u2026");
                menuItem.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    persistenceListRemove(persistenceData);
                  }
                });
              }
              else if (treeItem.getData() instanceof EntityIndexData)
              {
                // entity context menu
                final EntityIndexData entityIndexData = (EntityIndexData)treeItem.getData();

                menuItem = Widgets.addMenuItem(menu,BARControl.tr("Refresh")+"\u2026");
                menuItem.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    refreshEntityIndex(entityIndexData);
                  }
                });

                menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove from index")+"\u2026");
                menuItem.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    if (Dialogs.confirm(shell,
                                        BARControl.tr("Remove entity from index with {0} {0,choice,0#entries|1#entry|1<entries}?",
                                                      entityIndexData.totalEntryCount
                                                     )
                                       )
                      )
                    {
                      removeEntityIndex(entityIndexData);
                      Widgets.removeTreeItem(widgetPersistenceTree,entityIndexData);
                    }
                  }
                });

                menuItem = Widgets.addMenuItem(menu,BARControl.tr("Delete")+"\u2026");
                menuItem.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    if (Dialogs.confirm(shell,
                                        BARControl.tr("Delete entity with {0} {0,choice,0#entries|1#entry|1<entries}, {1} ({2} {2,choice,0#bytes|1#byte|1<bytes})?",
                                                      entityIndexData.totalEntryCount,
                                                      Units.formatByteSize(entityIndexData.totalEntrySize),
                                                      entityIndexData.totalEntrySize
                                                     )
                                       )
                      )
                    {
                      deleteEntity(entityIndexData);
                      Widgets.removeTreeItem(treeItem);
                    }
                  }
                });

                Widgets.addMenuItemSeparator(menu);

                menuItem = Widgets.addMenuItem(menu,BARControl.tr("Info")+"\u2026");
                menuItem.addSelectionListener(new SelectionListener()
                {
                  @Override
                  public void widgetDefaultSelected(SelectionEvent selectionEvent)
                  {
                  }
                  @Override
                  public void widgetSelected(SelectionEvent selectionEvent)
                  {
                    if (widgetPersistenceTreeToolTip != null)
                    {
                      widgetPersistenceTreeToolTip.dispose();
                      widgetPersistenceTreeToolTip = null;
                    }

                    if (treeItem != null)
                    {
                      Point point = display.getCursorLocation();
                      if (point.x > 16) point.x -= 16;
                      if (point.y > 16) point.y -= 16;

                      showEntityIndexToolTip(entityIndexData,point.x,point.y);
                    }
                  }
                });
              }
            }
          }
        });
        widgetPersistenceTree.setMenu(menu);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        Widgets.layout(composite,1,0,TableLayoutData.WE);
        {
          widgetPersistenceTreeAdd = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
          widgetPersistenceTreeAdd.setToolTipText(BARControl.tr("Add new persistence entry."));
          Widgets.layout(widgetPersistenceTreeAdd,0,0,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
          widgetPersistenceTreeAdd.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              persistenceListAdd();
            }
          });

          widgetPersistenceTreeEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
          widgetPersistenceTreeEdit.setToolTipText(BARControl.tr("Edit persistence entry."));
          Widgets.layout(widgetPersistenceTreeEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
          widgetPersistenceTreeEdit.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              persistenceListEdit();
            }
          });

          button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026",Settings.hasNormalRole());
          button.setToolTipText(BARControl.tr("Clone persistence entry."));
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              persistenceListClone();
            }
          });

          widgetPersistenceTreeRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
          widgetPersistenceTreeRemove.setToolTipText(BARControl.tr("Remove persistence entry."));
          Widgets.layout(widgetPersistenceTreeRemove,0,3,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
          widgetPersistenceTreeRemove.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              persistenceListRemove();
            }
          });
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Comment"),Settings.hasNormalRole());
      tab.setLayout(new TableLayout(1.0,1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        styledText = Widgets.newStyledText(tab,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
        styledText.setToolTipText(BARControl.tr("Free text comment for job."));
        Widgets.layout(styledText,0,0,TableLayoutData.NSWE);
        styledText.addModifyListener(new ModifyListener()
        {
          @Override
          public void modifyText(ModifyEvent modifyEvent)
          {
            StyledText widget = (StyledText)modifyEvent.widget;
            String     string = widget.getText();
            Color      color  = COLOR_MODIFIED;

            if (comment.equals(string.replace(widget.getLineDelimiter(),"\n"))) color = null;
            widget.setBackground(color);
          }
        });
        styledText.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            StyledText widget = (StyledText)selectionEvent.widget;
            String     text   = widget.getText();

            try
            {
              comment.set(text.replace(widget.getLineDelimiter(),"\n"));
              BARServer.setJobOption(selectedJobData.uuid,comment);
              widget.setBackground(null);
            }
            catch (Exception exception)
            {
              // ignored
            }
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
        });
        styledText.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
            StyledText widget = (StyledText)focusEvent.widget;
            String     text   = widget.getText();

            try
            {
              comment.set(text.replace(widget.getLineDelimiter(),"\n"));
              BARServer.setJobOption(selectedJobData.uuid,comment);
              widget.setBackground(null);
            }
            catch (Exception exception)
            {
              // ignored
            }
          }
        });
        Widgets.addModifyListener(new WidgetModifyListener(styledText,comment));
      }
    }
    Widgets.addEventListener(new WidgetEventListener(widgetTabFolder,selectJobEvent)
    {
      @Override
      public void trigger(Control control)
      {
        Widgets.setEnabled(control,selectedJobData != null);
      }
    });

    // listeners
    shell.addListener(BARControl.USER_EVENT_SELECT_SERVER,new Listener()
    {
      public void handleEvent(Event event)
      {
        clearSelectedJob();
      }
    });
    shell.addListener(BARControl.USER_EVENT_NEW_JOB,new Listener()
    {
      public void handleEvent(Event event)
      {
        assert(event.text != null);
      }
    });
    shell.addListener(BARControl.USER_EVENT_UPDATE_JOB,new Listener()
    {
      public void handleEvent(Event event)
      {
        update();
      }
    });
    shell.addListener(BARControl.USER_EVENT_DELETE_JOB,new Listener()
    {
      public void handleEvent(Event event)
      {
        assert(event.text != null);

        clearSelectedJob();
      }
    });
    shell.addListener(BARControl.USER_EVENT_SELECT_JOB,new Listener()
    {
      public void handleEvent(Event event)
      {
        assert(event.text != null);

        if (!widgetJobList.isDisposed())
        {
          for (JobData jobData : (JobData[])Widgets.getOptionMenuItems(widgetJobList,JobData.class))
          {
            if (jobData.uuid.equals(event.text))
            {
              setSelectedJob(jobData);
              break;
            }
          }
        }
      }
    });

    // create update thread
    updateThread = new UpdateThread();
  }

  /** set tab status reference
   * @param tabStatus tab status object
   */
  public void setTabStatus(TabStatus tabStatus)
  {
    this.tabStatus = tabStatus;
  }

  /** start update job data
   */
  public void startUpdate()
  {
    updateThread.start();
  }

  /** update job list
   * @param jobData_ job data to update
   */
  public void updateJobList(final Collection<JobData> jobData_)
  {
    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        if (!widgetJobList.isDisposed())
        {
          // get option menu data
          HashSet<JobData> removeJobDataSet = new HashSet<JobData>();
          for (JobData jobData : (JobData[])Widgets.getOptionMenuItems(widgetJobList,JobData.class))
          {
            removeJobDataSet.add(jobData);
          }

          // update job list
          final Comparator<JobData> jobDataComparator = new Comparator<JobData>()
          {
            /** compare job data
             * @param jobData1, jobData2 file tree data to compare
             * @return -1 iff jobData2.name < jobData1.name,
                        0 iff jobData2.name = jobData1.name,
                        1 iff jobData2.name > jobData1.name
             */
            @Override
            public int compare(JobData jobData1, JobData jobData2)
            {
              return jobData2.name.compareTo(jobData1.name);
            }
          };

          synchronized(widgetJobList)
          {
            for (JobData jobData : jobData_)
            {
              if (Widgets.updateInsertOptionMenuItem(widgetJobList,
                                                     jobDataComparator,
                                                     jobData,
                                                     jobData.name
                                                    )
                 )
              {
                removeJobDataSet.remove(jobData);
              }
            }
          }

          // remove delete option menu items
          for (JobData jobData : removeJobDataSet)
          {
            Widgets.removeOptionMenuItem(widgetJobList,jobData);
          }

          // select job
          Widgets.setSelectedOptionMenuItem(widgetJobList,selectedJobData);
        }
      }
    });
  }

  /** create new job
   * @return true iff new job created
   */
  public boolean jobNew()
  {
    /** dialog data
     */
    class Data
    {
      String jobName;

      Data()
      {
        this.jobName = "";
      }
    };

    Composite composite;
    Label     label;
    Button    button;

    final Data data = new Data();

    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("New job"),300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetJobName;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobName = Widgets.newText(composite);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetAdd = Widgets.newButton(composite,BARControl.tr("Add"));
      widgetAdd.setEnabled(false);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetJobName.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text   widget = (Text)modifyEvent.widget;
        String string = widget.getText().trim();

        widgetAdd.setEnabled(!string.isEmpty());
      }
    });
    widgetJobName.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetAdd.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        data.jobName  = widgetJobName.getText();
        Dialogs.close(dialog,true);
      }
    });

    if ((Boolean)Dialogs.run(dialog,false))
    {
      if (!data.jobName.isEmpty())
      {
        try
        {
          ValueMap valueMap = new ValueMap();
          BARServer.executeCommand(StringParser.format("JOB_NEW name=%S",data.jobName),
                                   0,  // debugLevel
                                   valueMap
                                  );

          String newJobUUID = valueMap.getString("jobUUID");
          Widgets.notify(shell,BARControl.USER_EVENT_NEW_JOB,newJobUUID);
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,BARControl.tr("Cannot create new job:\n\n{0}",exception.getMessage()));
          BARControl.logThrowable(exception);
          return false;
        }
        catch (CommunicationError error)
        {
          Dialogs.error(shell,BARControl.tr("Cannot create new job:\n\n{0}",error.getMessage()));
          return false;
        }
      }
      return true;
    }
    else
    {
      return false;
    }
  }

  /** clone job
   * @param jobData job data
   * @return true iff job cloned
   */
  public boolean jobClone(final JobData jobData)
  {
    /** dialog data
     */
    class Data
    {
      String jobName;

      Data()
      {
        this.jobName = "";
      }
    };

    Composite composite;
    Label     label;
    Button    button;

    assert jobData != null;

    final Data data = new Data();

    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Clone job"),300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetJobName;
    final Button widgetClone;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobName = Widgets.newText(composite);
      widgetJobName.setText(jobData.name);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetClone = Widgets.newButton(composite,BARControl.tr("Clone"));
      widgetClone.setEnabled(false);
      Widgets.layout(widgetClone,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetJobName.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text   widget = (Text)modifyEvent.widget;
        String name   = widget.getText();

        widgetClone.setEnabled(!name.isEmpty() && !name.equals(jobData.name));
      }
    });
    widgetJobName.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetClone.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetClone.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        data.jobName  = widgetJobName.getText();
        Dialogs.close(dialog,true);
      }
    });

    Widgets.setFocus(widgetJobName);
    if ((Boolean)Dialogs.run(dialog,false))
    {
      if (!data.jobName.isEmpty())
      {
        try
        {
          ValueMap valueMap = new ValueMap();
          BARServer.executeCommand(StringParser.format("JOB_CLONE jobUUID=%s name=%S",
                                                       jobData.uuid,
                                                       data.jobName
                                                      ),
                                   0,  // debugLevel
                                   valueMap
                                  );
          String newJobUUID = valueMap.getString("jobUUID");
          Widgets.notify(shell,BARControl.USER_EVENT_NEW_JOB,newJobUUID);
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,
                        BARControl.tr("Cannot clone job ''{0}'':\n\n{1}",
                                      jobData.name.replaceAll("&","&&"),
                                      exception.getMessage()
                                     )
                       );
          BARControl.logThrowable(exception);
          return false;
        }
        catch (CommunicationError error)
        {
          Dialogs.error(shell,BARControl.tr("Cannot clone job ''{0}'':\n\n{1}",jobData.name.replaceAll("&","&&"),error.getMessage()));
          return false;
        }
      }
      return true;
    }
    else
    {
      return false;
    }
  }

  /** rename job
   * @param jobData job data
   * @return true iff new job renamed
   */
  public boolean jobRename(final JobData jobData)
  {
    /** dialog data
     */
    class Data
    {
      String jobName;

      Data()
      {
        this.jobName = "";
      }
    };

    Composite composite;
    Label     label;
    Button    button;

    assert jobData != null;

    final Data data = new Data();

    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Rename job"),300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetNewJobName;
    final Button widgetRename;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Old name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      label = Widgets.newLabel(composite,jobData.name.replaceAll("&","&&"));
      Widgets.layout(label,0,1,TableLayoutData.W);

      label = Widgets.newLabel(composite,BARControl.tr("New name")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);

      widgetNewJobName = Widgets.newText(composite);
      widgetNewJobName.setText(jobData.name);
      Widgets.layout(widgetNewJobName,1,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetRename = Widgets.newButton(composite,BARControl.tr("Rename"));
      widgetRename.setEnabled(false);
      Widgets.layout(widgetRename,0,0,TableLayoutData.W);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetNewJobName.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text   widget = (Text)modifyEvent.widget;
        String name   = widget.getText();

        widgetRename.setEnabled(!name.isEmpty() && !name.equals(jobData.name));
      }
    });
    widgetNewJobName.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetRename.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetRename.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        data.jobName  = widgetNewJobName.getText();
        Dialogs.close(dialog,true);
      }
    });

    Widgets.setFocus(widgetNewJobName);
    if ((Boolean)Dialogs.run(dialog,false))
    {
      if (!data.jobName.isEmpty())
      {
        try
        {
          BARServer.executeCommand(StringParser.format("JOB_RENAME jobUUID=%s newName=%S",
                                                       jobData.uuid,
                                                       data.jobName
                                                      ),
                                   0  // debugLevel
                                  );
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,
                        BARControl.tr("Cannot rename job ''{0}'':\n\n{1}",
                                      jobData.name.replaceAll("&","&&"),
                                      exception.getMessage()
                                     )
                       );
          BARControl.logThrowable(exception);
          return false;
        }
        catch (CommunicationError error)
        {
          Dialogs.error(shell,
                        BARControl.tr("Cannot rename job ''{0}'':\n\n{1}",
                                      jobData.name.replaceAll("&","&&"),
                                      error.getMessage()
                                     )
                       );
          return false;
        }
      }

      Widgets.notify(shell,BARControl.USER_EVENT_UPDATE_JOB,jobData.uuid);

      return true;
    }
    else
    {
      return false;
    }
  }

  /** delete job
   * @param jobData job data
   * @return true iff job deleted
   */
  public boolean jobDelete(final JobData jobData)
  {
    assert jobData != null;

    if (Dialogs.confirm(shell,BARControl.tr("Delete job ''{0}''?",jobData.name.replaceAll("&","&&"))))
    {
      try
      {
        BARServer.executeCommand(StringParser.format("JOB_DELETE jobUUID=%s",jobData.uuid),0);
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,BARControl.tr("Cannot delete job ''{0}'':\n\n{1}",jobData.name.replaceAll("&","&&"),exception.getMessage()));
        BARControl.logThrowable(exception);
        return false;
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Cannot delete job ''{0}'':\n\n{1}",jobData.name.replaceAll("&","&&"),error.getMessage()));
        return false;
      }

      Widgets.notify(shell,BARControl.USER_EVENT_DELETE_JOB,jobData.uuid);

      return true;
    }
    else
    {
      return false;
    }
  }

  //-----------------------------------------------------------------------

  /** get selected job data
   */
  private void getJobData()
  {
    // disable tab
    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        Widgets.setEnabled(widgetTabFolder,false);
    }});

    if (selectedJobData != null)
    {
      {
// TODO: required? values are read only
        BARServer.lockSet();
      }
      try
      {
        // get job data
        BARServer.getJobOption(selectedJobData.uuid,slaveHostName);
        BARServer.getJobOption(selectedJobData.uuid,slaveHostPort);
        BARServer.getJobOption(selectedJobData.uuid,slaveTLSMode);
        BARServer.getJobOption(selectedJobData.uuid,includeFileCommand);
        BARServer.getJobOption(selectedJobData.uuid,includeImageCommand);
        BARServer.getJobOption(selectedJobData.uuid,excludeCommand);
        BARServer.getJobOption(selectedJobData.uuid,archiveName);
        BARServer.getJobOption(selectedJobData.uuid,archiveType);
//TODO: use widgetVariable+getJobOption
        archivePartSize.set(Units.parseByteSize(BARServer.getStringJobOption(selectedJobData.uuid,"archive-part-size"),0));
        archivePartSizeFlag.set(archivePartSize.getLong() > 0);

        String compressAlgorithms[] = parseCompressAlgorithm(BARServer.getStringJobOption(selectedJobData.uuid,"compress-algorithm"));
//Dprintf.dprintf("compressAlgorithms=%s:%s:%s",compressAlgorithms[0],compressAlgorithms[1],compressAlgorithms[2]);
        deltaCompressAlgorithm.set(compressAlgorithms[0]);
        byteCompressAlgorithmType.set(compressAlgorithms[1]);
        byteCompressAlgorithm.set(compressAlgorithms[2]);
        cryptAlgorithm.set(BARServer.getStringJobOption(selectedJobData.uuid,"crypt-algorithm"));
        cryptType.set(BARServer.getStringJobOption(selectedJobData.uuid,"crypt-type"));
        BARServer.getJobOption(selectedJobData.uuid,cryptPublicKeyFileName);
        cryptPasswordMode.set(BARServer.getStringJobOption(selectedJobData.uuid,"crypt-password-mode"));
        BARServer.getJobOption(selectedJobData.uuid,cryptPassword);
        BARServer.getJobOption(selectedJobData.uuid,incrementalListFileName);
        archiveFileMode.set(BARServer.getStringJobOption(selectedJobData.uuid,"archive-file-mode"));
// TODO:
//        BARServer.getStringJobOption(jobData.uuid,archiveFileMode);
        BARServer.getJobOption(selectedJobData.uuid,storageOnMasterFlag);
        BARServer.getJobOption(selectedJobData.uuid,sshPublicKeyFileName);
        BARServer.getJobOption(selectedJobData.uuid,sshPrivateKeyFileName);
/* NYI ???
        maxBandWidth.set(Units.parseByteSize(BARServer.getStringJobOption(selectedJobData.uuid,"max-band-width")));
        maxBandWidthFlag.set(maxBandWidth.getLongOption() > 0);
*/
        BARServer.getJobOption(selectedJobData.uuid,volumeSize);
        BARServer.getJobOption(selectedJobData.uuid,ecc);
        BARServer.getJobOption(selectedJobData.uuid,blank);
        BARServer.getJobOption(selectedJobData.uuid,waitFirstVolume);
        BARServer.getJobOption(selectedJobData.uuid,skipUnreadable);
        BARServer.getJobOption(selectedJobData.uuid,noStopOnOwnerError);
        BARServer.getJobOption(selectedJobData.uuid,noStopOnAttributeError);
        BARServer.getJobOption(selectedJobData.uuid,rawImages);
        BARServer.getJobOption(selectedJobData.uuid,overwriteFiles);
        BARServer.getJobOption(selectedJobData.uuid,preCommand);
        BARServer.getJobOption(selectedJobData.uuid,postCommand);
        BARServer.getJobOption(selectedJobData.uuid,slavePreCommand);
        BARServer.getJobOption(selectedJobData.uuid,slavePostCommand);
        BARServer.getJobOption(selectedJobData.uuid,maxStorageSize);
        BARServer.getJobOption(selectedJobData.uuid,comment);

        // update trees/tables
        updateIncludeList(selectedJobData);
        updateExcludeList(selectedJobData);
        updateMountList(selectedJobData);
        updateSourceList(selectedJobData);
        updateCompressExcludeList(selectedJobData);
        updateScheduleTable(selectedJobData);
        updatePersistenceTree(selectedJobData);

        // update images
        updateFileTreeImages();
        updateDeviceImages();
      }
      catch (Exception exception)
      {
        // ignored
      }
      finally
      {
        BARServer.unlockSet();
      }
    }
    else
    {
      display.syncExec(new Runnable()
      {
        @Override
        public void run()
        {
          clearSelectedJob();
        }
      });
    }

    // enable tab
    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        Widgets.setEnabled(widgetTabFolder,true);
      }
    });
  }

  /** update selected job data (read only data)
   */
  private void update()
  {
    if (selectedJobData != null)
    {
      try
      {
        // update root/device lists
        updateRootList();
        updateDevicesList();

        // update persistence tree
        updatePersistenceTree(selectedJobData);
      }
      catch (Throwable throwable)
      {
        // internal error
        if (Settings.debugLevel > 0)
        {
          BARServer.disconnect();
          BARControl.internalError(throwable);
        }
      }
    }
  }

  /** check if name is included
   * @param name
   * @return true iff included
  */
  private boolean isIncluded(String name)
  {
    for (String pattern : includeHashMap.keySet())
    {
      if (name.matches(StringUtils.globToRegex(pattern)))
      {
        return true;
      }
    }

    return false;
  }

  /** check if name is excluded
   * @param name
   * @return true iff excluded
  */
  private boolean isExcluded(String name)
  {
    for (String pattern : excludeHashSet)
    {
      if (name.matches(StringUtils.globToRegex(pattern)))
      {
        return true;
      }
    }

    return false;
  }

  /** parse compress algorithm string
   * @return [deltaCompressAlgorithm,byteCompressAlgorithmType,byteCompressAlgorithm]
   */
  private String[] parseCompressAlgorithm(String string)
  {
    final String COMPRESS_ALGORITHM_PATTERN = "^(none|zip|bzip|lzma|lzo|lz4|zstd)(-{0,1}[0-9]+){0,1}$";

    try
    {
      String[] compressAlgorithms = StringUtils.splitArray(string,"+");

      Matcher matcher;
      String  byteCompressAlgorithmType;
      if ((matcher = Pattern.compile(COMPRESS_ALGORITHM_PATTERN).matcher(compressAlgorithms[1])).matches())
      {
        byteCompressAlgorithmType  = matcher.group(1);
      }
      else
      {
        byteCompressAlgorithmType  = "";
      }

      if      (compressAlgorithms.length >= 2)
      {
        return new String[]{compressAlgorithms[0],byteCompressAlgorithmType,compressAlgorithms[1]};
      }
      else if (compressAlgorithms.length >= 1)
      {
        return new String[]{"",byteCompressAlgorithmType,compressAlgorithms[0]};
      }
      else
      {
        return new String[]{"","",""};
      }
    }
    catch (PatternSyntaxException e)
    {
      return new String[]{"","",""};
    }
  }

  /** get archive name
   * @return archive name
   *   ftp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   scp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   sftp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   webdav://[[<login name>[:<login password>]@<host name>[:<host port>]/]<file name>
   *   webdavs://[[<login name>[:<login password>]@<host name>[:<host port>]/]<file name>
   *   smb://[[<login name>[:<login password>]@<host name>/]<file name>
   *   cd://[<device name>:]<file name>
   *   dvd://[<device name>:]<file name>
   *   bd://[<device name>:]<file name>
   *   device://[<device name>:]<file name>
   *   file://<file name>
   */
  private String getArchiveName()
  {
    URIParts uriParts = new URIParts((StorageTypes)storageType.getEnum(),
                                     storageHostName.getString(),
                                     storageHostPort.getInteger(),
                                     storageLoginName.getString(),
                                     storageLoginPassword.getString(),
                                     storageShareName.getString(),
                                     storageDeviceName.getString(),
                                     storageFileName.getString()
                                    );

    return uriParts.getURI();
  }

  //-----------------------------------------------------------------------

  /** set selected job
   * @param jobData job data
   */
  private void setSelectedJob(JobData jobData)
  {
    selectedJobData = jobData;

    if (selectedJobData != null)
    {
      Widgets.setSelectedOptionMenuItem(widgetJobList,selectedJobData);
    }
    else
    {
      Widgets.clearSelectedOptionMenuItem(widgetJobList);
    }

    closeAllFileTree();
    clearIncludeList();
    clearExcludeList();
    clearSourceList();
    clearCompressExcludeList();
    clearScheduleTable();
    clearPersistenceTable();

    // update root/device lists
    updateRootList();
    updateDevicesList();

    // set selected job
    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        selectJobEvent.trigger();
      }
    });

    // get job data
    getJobData();
  }

  /** clear selected
   */
  private void clearSelectedJob()
  {
    selectedJobData = null;

    closeAllFileTree();
    clearIncludeList();
    clearExcludeList();
    clearSourceList();
    clearCompressExcludeList();
    clearScheduleTable();
    clearPersistenceTable();

    selectJobEvent.trigger();
  }

  //-----------------------------------------------------------------------

  /** clone selected job
   */
  private void jobClone()
  {
    assert selectedJobData != null;

    jobClone(selectedJobData);
  }

  /** rename selected job
   */
  private void jobRename()
  {
    assert selectedJobData != null;

    if (jobRename(selectedJobData))
    {
// TODO:
//Dprintf.dprintf("xxxxxxxxxxxxxxxxxx");
    }
  }

  /** delete selected job
   */
  private void jobDelete()
  {
    assert selectedJobData != null;

    if (jobDelete(selectedJobData))
    {
      selectedJobData = null;
      clearSelectedJob();
    }
  }

  //-----------------------------------------------------------------------

  /** update root list
   */
  private void updateRootList()
  {
    // get root names
    final ArrayList<String> rootNameList = new ArrayList<String>();
    try
    {
      BARServer.executeCommand(StringParser.format("ROOT_LIST jobUUID=%s allMounts=no",
                                                   (selectedJobData != null) ? selectedJobData.uuid : ""
                                                  ),
                               1,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   String name = valueMap.getString("name");

                                   rootNameList.add(name);
                                 }
                               }
                              );
    }
    catch (BARException exception)
    {
      if (!shell.isDisposed())
      {
        Dialogs.warning(shell,
                        (exception.getCode() == BARException.SLAVE_DISCONNECTED)
                          ? Dialogs.booleanFieldUpdater(Settings.class,"showSlaveDisconnected")
                          : (BooleanFieldUpdater)null,
                        BARControl.tr("Cannot get root list (error: {0})",exception.getMessage())
                       );
      }
    }
    catch (IOException exception)
    {
      if (!shell.isDisposed())
      {
        Dialogs.warning(shell,BARControl.tr("Cannot get root list (error: {0})",exception.getMessage()));
      }
    }
    catch (Exception exception)
    {
       Dialogs.error(shell,BARControl.tr("Cannot get root list (error: {0})",exception.getMessage()));
       BARControl.logThrowable(exception);
    }

    if (!widgetFileTree.isDisposed())
    {
      // insert entry, request directory info
      display.syncExec(new Runnable()
      {
        @Override
        public void run()
        {
          final FileTreeDataComparator fileTreeDataComparator = new FileTreeDataComparator(widgetFileTree);

          HashSet<TreeItem> removeTreeItems = Widgets.getTreeItems(widgetFileTree);

          // update/insert/remove tree items
          for (final String rootName : rootNameList)
          {
            final Image image;
            if      (isIncluded(rootName) && !isExcluded(rootName))
              image = IMAGE_DIRECTORY_INCLUDED;
            else if (excludeHashSet.contains(rootName))
              image = IMAGE_DIRECTORY_EXCLUDED;
            else
              image = IMAGE_DIRECTORY;

            TreeItem treeItem = Widgets.updateInsertTreeItem(widgetFileTree,
                                                             fileTreeDataComparator,
                                                             new FileTreeData(rootName,BARServer.FileTypes.DIRECTORY,rootName,false,false),
                                                             image,
                                                             Widgets.TREE_ITEM_FLAG_FOLDER,
                                                             rootName
                                                            );
            removeTreeItems.remove(treeItem);
          }

          Widgets.removeTreeItems(removeTreeItems);
        }
      });
    }
  }

  /** close all sub-directories in file tree
   */
  private void closeAllFileTree()
  {
    // close all directories and remove sub-directory items (all except root)
    if (!widgetFileTree.isDisposed())
    {
      for (TreeItem treeItem : widgetFileTree.getItems())
      {
        treeItem.removeAll();
        new TreeItem(treeItem,SWT.NONE);
      }
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
  private void openAllIncludedDirectories()
  {
    // open all included directories
    for (TableItem tableItem : widgetIncludeTable.getItems())
    {
      EntryData entryData = (EntryData)tableItem.getData();

      TreeItem[] treeItems = widgetFileTree.getItems();

      StringBuilder buffer = new StringBuilder();
      for (String part : StringUtils.splitArray(entryData.pattern,BARServer.filePathSeparator,true))
      {
        // expand name
        if (   (buffer.length() == 0)
            || (buffer.charAt(buffer.length()-1) != BARServer.filePathSeparator))
        {
          buffer.append(BARServer.filePathSeparator);
        }
        buffer.append(part);

        TreeItem treeItem = findTreeItem(treeItems,buffer.toString());
        if (treeItem != null)
        {
          FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
          if (fileTreeData.fileType == BARServer.FileTypes.DIRECTORY)
          {
            // open sub-directory
            if (!treeItem.getExpanded())
            {
              Event treeEvent = new Event();
              treeEvent.item = treeItem;
              widgetFileTree.notifyListeners(SWT.Expand,treeEvent);
              treeItem.setExpanded(true);
            }

            // get sub-directory items
            treeItems = treeItem.getItems();
          }
        }
        else
        {
          break;
        }
      }
    }
  }

  /** update file list of tree item
   * @param treeItem tree item to update
   */
  private void updateFileTree(final TreeItem treeItem)
  {
    {
      BARControl.waitCursor();
    }
    try
    {
      final ArrayList<FileTreeData> fileTreeDataList = new ArrayList<FileTreeData>();
      BARServer.executeCommand(StringParser.format("FILE_LIST jobUUID=%s directory=%'S",
                                                   (selectedJobData.uuid != null) ? selectedJobData.uuid : "",
                                                   ((FileTreeData)treeItem.getData()).name
                                                  ),
                               0,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   BARServer.FileTypes fileType = valueMap.getEnum("fileType",BARServer.FileTypes.class);
                                   switch (fileType)
                                   {
                                     case FILE:
                                       {
                                         final String  name       = valueMap.getString ("name"         );
                                         final long    size       = valueMap.getLong   ("size",0L      );
                                         final long    dateTime   = valueMap.getLong   ("dateTime",0L  );
                                         final boolean noDumpFlag = valueMap.getBoolean("noDump", false);

                                         fileTreeDataList.add(new FileTreeData(name,
                                                                               BARServer.FileTypes.FILE,
                                                                               size,
                                                                               dateTime,
                                                                               new File(name).getName(),
                                                                               false,noDumpFlag
                                                                              )
                                                             );
                                       }
                                       break;
                                     case DIRECTORY:
                                       {
                                         final String  name         = valueMap.getString ("name"          );
                                         final long    dateTime     = valueMap.getLong   ("dateTime",0L   );
                                         final boolean noBackupFlag = valueMap.getBoolean("noBackup",false);
                                         final boolean noDumpFlag   = valueMap.getBoolean("noDump",  false);

                                         fileTreeDataList.add(new FileTreeData(name,
                                                                               BARServer.FileTypes.DIRECTORY,
                                                                               dateTime,
                                                                               new File(name).getName(),
                                                                               noBackupFlag,noDumpFlag
                                                                              )
                                                             );
                                       }
                                       break;
                                     case LINK:
                                       {
                                         final String  name       = valueMap.getString ("name"          );
                                         final long    dateTime   = valueMap.getLong   ("dateTime",0L   );
                                         final boolean noDumpFlag = valueMap.getBoolean("noDump",  false);

                                         fileTreeDataList.add(new FileTreeData(name,
                                                                               BARServer.FileTypes.LINK,
                                                                               dateTime,
                                                                               new File(name).getName(),
                                                                               false,
                                                                               noDumpFlag
                                                                              )
                                                             );
                                       }
                                       break;
                                     case HARDLINK:
                                       {
                                         final String  name       = valueMap.getString ("name"         );
                                         final long    size       = valueMap.getLong   ("size"         );
                                         final long    dateTime   = valueMap.getLong   ("dateTime",0L  );
                                         final boolean noDumpFlag = valueMap.getBoolean("noDump", false);

                                         fileTreeDataList.add(new FileTreeData(name,
                                                                               BARServer.FileTypes.HARDLINK,
                                                                               size,
                                                                               dateTime,
                                                                               new File(name).getName(),
                                                                               false,noDumpFlag
                                                                              )
                                                             );
                                       }
                                       break;
                                     case SPECIAL:
                                       {
                                         final String                 name         = valueMap.getString ("name"                                    );
                                         final long                   size         = valueMap.getLong   ("size",       0L                          );
                                         final long                   dateTime     = valueMap.getLong   ("dateTime",   0L                          );
                                         final boolean                noBackupFlag = valueMap.getBoolean("noBackup",   false                       );
                                         final boolean                noDumpFlag   = valueMap.getBoolean("noDump",     false                       );
                                         final BARServer.SpecialTypes specialType  = valueMap.getEnum   ("specialType",BARServer.SpecialTypes.class);

                                         switch (specialType)
                                         {
                                           case DEVICE_CHARACTER:
                                             fileTreeDataList.add(new FileTreeData(name,
                                                                                   BARServer.SpecialTypes.DEVICE_CHARACTER,
                                                                                   dateTime,
                                                                                   name,
                                                                                   false,
                                                                                   noDumpFlag
                                                                                  )
                                                                 );
                                             break;
                                           case DEVICE_BLOCK:
                                             fileTreeDataList.add(new FileTreeData(name,
                                                                                   BARServer.SpecialTypes.DEVICE_BLOCK,
                                                                                   size,
                                                                                   dateTime,
                                                                                   name,
                                                                                   false,
                                                                                   noDumpFlag
                                                                                  )
                                                                 );
                                             break;
                                           case FIFO:
                                             fileTreeDataList.add(new FileTreeData(name,
                                                                                   BARServer.SpecialTypes.FIFO,
                                                                                   dateTime,
                                                                                   name,
                                                                                   false,
                                                                                   noDumpFlag
                                                                                  )
                                                                 );
                                             break;
                                           case SOCKET:
                                             fileTreeDataList.add(new FileTreeData(name,
                                                                                   BARServer.SpecialTypes.SOCKET,
                                                                                   dateTime,
                                                                                   name,
                                                                                   false,
                                                                                   noDumpFlag
                                                                                  )
                                                                 );
                                             break;
                                           case OTHER:
                                             fileTreeDataList.add(new FileTreeData(name,
                                                                                   BARServer.SpecialTypes.OTHER,
                                                                                   dateTime,
                                                                                   name,
                                                                                   false,
                                                                                   noDumpFlag
                                                                                  )
                                                                 );
                                             break;
                                         }
                                       }
                                       break;
                                   }
                                 }
                               }
                              );

      if (!treeItem.isDisposed())
      {
        final FileTreeDataComparator fileTreeDataComparator = new FileTreeDataComparator(widgetFileTree);

        // if not expanded clear place-holder item
        if (!treeItem.getExpanded())
        {
          treeItem.removeAll();
        }

        final HashSet<TreeItem> removeTreeItems = Widgets.getSubTreeItems(treeItem);

        // update/insert/remove tree items
        for (final FileTreeData fileTreeData : fileTreeDataList)
        {
          switch (fileTreeData.fileType)
          {
            case FILE:
              {
                final Image image;
                if      (isIncluded(fileTreeData.name) && !isExcluded(fileTreeData.name))
                  image = IMAGE_FILE_INCLUDED;
                else if (isExcluded(fileTreeData.name) || fileTreeData.noDump)
                  image = IMAGE_FILE_EXCLUDED;
                else
                  image = IMAGE_FILE;

                // insert entry
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                        fileTreeDataComparator,
                                                                        fileTreeData,
                                                                        image,
                                                                        Widgets.TREE_ITEM_FLAG_NONE,
                                                                        fileTreeData.title,
                                                                        "FILE",
                                                                        Units.formatByteSize(fileTreeData.size),
                                                                        SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                       );

                    removeTreeItems.remove(subTreeItem);
                  }
                });
              }
              break;
            case DIRECTORY:
              {
                final Image image;
                if      (isIncluded(fileTreeData.name) && !isExcluded(fileTreeData.name))
                  image = IMAGE_DIRECTORY_INCLUDED;
                else if (isExcluded(fileTreeData.name) || fileTreeData.noBackup || fileTreeData.noDump)
                  image = IMAGE_DIRECTORY_EXCLUDED;
                else
                  image = IMAGE_DIRECTORY;

                // insert entry, request directory info
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                        fileTreeDataComparator,
                                                                        fileTreeData,
                                                                        image,
                                                                        Widgets.TREE_ITEM_FLAG_FOLDER,
                                                                        fileTreeData.title,
                                                                        "DIR",
                                                                        null,
                                                                        SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                       );
                    removeTreeItems.remove(subTreeItem);

                    directoryInfoThread.add(selectedJobData.uuid,fileTreeData.name,subTreeItem);
                  }
                });
              }
              break;
            case LINK:
              {
                final Image image;
                if      (isIncluded(fileTreeData.name) && !isExcluded(fileTreeData.name))
                  image = IMAGE_LINK_INCLUDED;
                else if (isExcluded(fileTreeData.name) || fileTreeData.noDump)
                  image = IMAGE_LINK_EXCLUDED;
                else
                  image = IMAGE_LINK;

                // insert entry
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                        fileTreeDataComparator,
                                                                        fileTreeData,
                                                                        image,
                                                                        Widgets.TREE_ITEM_FLAG_NONE,
                                                                        fileTreeData.title,
                                                                        "LINK",
                                                                        null,
                                                                        SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                       );
                    removeTreeItems.remove(subTreeItem);
                  }
                });
              }
              break;
            case HARDLINK:
              {
                final Image image;
                if      (isIncluded(fileTreeData.name) && !isExcluded(fileTreeData.name))
                  image = IMAGE_FILE_INCLUDED;
                else if (isExcluded(fileTreeData.name) || fileTreeData.noDump)
                  image = IMAGE_FILE_EXCLUDED;
                else
                  image = IMAGE_FILE;

                // insert entry
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                        fileTreeDataComparator,
                                                                        fileTreeData,
                                                                        image,
                                                                        Widgets.TREE_ITEM_FLAG_NONE,
                                                                        fileTreeData.title,
                                                                        "HARDLINK",
                                                                        Units.formatByteSize(fileTreeData.size),
                                                                        SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                       );
                    removeTreeItems.remove(subTreeItem);
                  }
                });
              }
              break;
            case SPECIAL:
              {
                final Image image;
                if      (isIncluded(fileTreeData.name) && !isExcluded(fileTreeData.name))
                  image = IMAGE_FILE_INCLUDED;
                else if (isExcluded(fileTreeData.name) || fileTreeData.noDump)
                  image = IMAGE_FILE_EXCLUDED;
                else
                  image = IMAGE_FILE;

                switch (fileTreeData.specialType)
                {
                  case DEVICE_CHARACTER:
                    display.syncExec(new Runnable()
                    {
                      @Override
                      public void run()
                      {
                        TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                            fileTreeDataComparator,
                                                                            fileTreeData,
                                                                            image,
                                                                            Widgets.TREE_ITEM_FLAG_NONE,
                                                                            fileTreeData.title,
                                                                            "CHARACTER DEVICE",
                                                                            SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                           );
                        removeTreeItems.remove(subTreeItem);
                      }
                    });
                    break;
                  case DEVICE_BLOCK:
                    display.syncExec(new Runnable()
                    {
                      @Override
                      public void run()
                      {
                        TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                            fileTreeDataComparator,
                                                                            fileTreeData,
                                                                            image,
                                                                            Widgets.TREE_ITEM_FLAG_NONE,
                                                                            fileTreeData.title,
                                                                            "BLOCK DEVICE",
                                                                            (fileTreeData.size > 0) ? Units.formatByteSize(fileTreeData.size) : null,
                                                                            SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                           );
                      }
                    });
                    break;
                  case FIFO:
                    display.syncExec(new Runnable()
                    {
                      @Override
                      public void run()
                      {
                        TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                            fileTreeDataComparator,
                                                                            fileTreeData,
                                                                            image,
                                                                            Widgets.TREE_ITEM_FLAG_NONE,
                                                                            fileTreeData.title,
                                                                            "FIFO",
                                                                            null,
                                                                            SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                           );
                        removeTreeItems.remove(subTreeItem);
                      }
                    });
                    break;
                  case SOCKET:
                    display.syncExec(new Runnable()
                    {
                      @Override
                      public void run()
                      {
                        TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                            fileTreeDataComparator,
                                                                            fileTreeData,
                                                                            image,
                                                                            Widgets.TREE_ITEM_FLAG_NONE,
                                                                            fileTreeData.title,
                                                                            "SOCKET",
                                                                            SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                           );
                        removeTreeItems.remove(subTreeItem);
                      }
                    });
                    break;
                  case OTHER:
                    display.syncExec(new Runnable()
                    {
                      @Override
                      public void run()
                      {
                        TreeItem subTreeItem = Widgets.updateInsertTreeItem(treeItem,
                                                                            fileTreeDataComparator,
                                                                            fileTreeData,
                                                                            image,
                                                                            Widgets.TREE_ITEM_FLAG_NONE,
                                                                            fileTreeData.title,
                                                                            "SPECIAL",
                                                                            SIMPLE_DATE_FORMAT.format(new Date(fileTreeData.dateTime*1000))
                                                                           );
                        removeTreeItems.remove(subTreeItem);
                      }
                    });
                    break;
                }
              }
              break;
          }
        }

        Widgets.removeTreeItems(removeTreeItems);
      }
    }
    catch (BARException exception)
    {
      if (!shell.isDisposed())
      {
        Dialogs.warning(shell,
                        (exception.getCode() == BARException.SLAVE_DISCONNECTED)
                          ? Dialogs.booleanFieldUpdater(Settings.class,"showSlaveDisconnected")
                          : (BooleanFieldUpdater)null,
                        BARControl.tr("Cannot get file list (error: {0})",exception.getMessage())
                       );
      }
    }
    catch (IOException exception)
    {
      if (!shell.isDisposed())
      {
        Dialogs.warning(shell,BARControl.tr("Cannot get file list (error: {0})",exception.getMessage()));
      }
    }
    catch (Exception exception)
    {
       Dialogs.error(shell,BARControl.tr("Cannot get file list (error: {0})",exception.getMessage()));
       BARControl.logThrowable(exception);
    }
    finally
    {
      BARControl.resetCursor();
    }
  }

  /** update file tree item images
   * @param treeItem tree item to update
   */
  private void updateFileTreeImages(TreeItem treeItem)
  {
    FileTreeData fileTreeData = (FileTreeData)treeItem.getData();

    Image image = null;
    if      (isIncluded(fileTreeData.name) && !isExcluded(fileTreeData.name))
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
    else if (isExcluded(fileTreeData.name) || fileTreeData.noBackup || fileTreeData.noDump )
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
            case DEVICE_CHARACTER: image = IMAGE_FILE;      break;
            case DEVICE_BLOCK:     image = IMAGE_FILE;      break;
            case FIFO:             image = IMAGE_FILE;      break;
            case SOCKET:           image = IMAGE_FILE;      break;
            case OTHER:            image = IMAGE_FILE;      break;
          }
          break;
      }
    }
    treeItem.setImage(image);

    // update sub-items
    if (treeItem.getExpanded())
    {
      for (TreeItem subTreeItem : treeItem.getItems())
      {
        updateFileTreeImages(subTreeItem);
      }
    }
  }

  /** update all file tree item images
   */
  private void updateFileTreeImages()
  {
    if (!widgetFileTree.isDisposed())
    {
      for (TreeItem treeItem : widgetFileTree.getItems())
      {
        updateFileTreeImages(treeItem);
      }
    }
  }

  //-----------------------------------------------------------------------

  /** update devices to list
   */
  private void updateDevicesList()
  {
    // get device list
    final ArrayList<DeviceData> deviceDataList = new ArrayList<DeviceData>();
    try
    {
      if (!widgetDeviceTable.isDisposed())
      {
        BARServer.executeCommand(StringParser.format("DEVICE_LIST jobUUID=%s",
                                                     (selectedJobData != null) ? selectedJobData.uuid : ""
                                                    ),
                                 1,  // debugLevel
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     String  name    = valueMap.getString ("name"   );
                                     long    size    = valueMap.getLong   ("size",0L);
// TODO: use
                                     boolean mounted = valueMap.getBoolean("mounted");

                                     deviceDataList.add(new DeviceData(name,size));
                                   }
                                 }
                                );
      }
    }
    catch (BARException exception)
    {
      if (!shell.isDisposed())
      {
        Dialogs.warning(shell,
                        (exception.getCode() == BARException.SLAVE_DISCONNECTED)
                          ? Dialogs.booleanFieldUpdater(Settings.class,"showSlaveDisconnected")
                          : (BooleanFieldUpdater)null,
                        BARControl.tr("Cannot get device list (error: {0})",exception.getMessage())
                       );
      }
    }
    catch (IOException exception)
    {
      if (!shell.isDisposed())
      {
        Dialogs.warning(shell,BARControl.tr("Cannot get device list (error: {0})",exception.getMessage()));
      }
    }
    catch (Exception exception)
    {
       Dialogs.error(shell,BARControl.tr("Cannot get device list (error: {0})",exception.getMessage()));
       BARControl.logThrowable(exception);
    }

    // update table
    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        if (!widgetDeviceTable.isDisposed())
        {
          final DeviceDataComparator deviceDataComparator = new DeviceDataComparator(widgetDeviceTable);

          HashSet<TableItem> removeTableItems = Widgets.getAllTableItems(widgetDeviceTable);

          for (DeviceData deviceData : deviceDataList)
          {
            TableItem tableItem = Widgets.updateInsertTableItem(widgetDeviceTable,
                                                                deviceDataComparator,
                                                                deviceData,
                                                                IMAGE_DEVICE,
                                                                deviceData.name,
                                                                Units.formatByteSize(deviceData.size)
                                                               );
            removeTableItems.remove(tableItem);
          }

          Widgets.removeTableItems(removeTableItems);
        }
      }
    });
  }

  /** update images in device tree
   */
  private void updateDeviceImages()
  {
    if (!widgetDeviceTable.isDisposed())
    {
      for (TableItem tableItem : widgetDeviceTable.getItems())
      {
        DeviceData deviceData = (DeviceData)tableItem.getData();

        Image image;
        if      (isIncluded(deviceData.name) && !isExcluded(deviceData.name))
        {
          image = IMAGE_DEVICE_INCLUDED;
        }
        else if (isExcluded(deviceData.name))
        {
          image = IMAGE_DEVICE;
        }
        else
        {
          image = IMAGE_DEVICE;
        }
        tableItem.setImage(image);
      }
    }
  }

  //-----------------------------------------------------------------------

  /** clear include list
   */
  private void clearIncludeList()
  {
    if (!widgetIncludeTable.isDisposed())
    {
      Widgets.removeAllTableItems(widgetIncludeTable);
    }
  }

  /** update include list
   * @param jobData job data
   */
  private void updateIncludeList(JobData jobData)
  {
    final EntryDataComparator entryDataComparator = new EntryDataComparator(widgetIncludeTable);

    synchronized(includeHashMap)
    {
      includeHashMap.clear();
      try
      {
        BARServer.executeCommand(StringParser.format("INCLUDE_LIST jobUUID=%s",
                                                     jobData.uuid
                                                    ),
                                 0,  // debugLevel
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     final EntryTypes   entryType   = valueMap.getEnum  ("entryType",  EntryTypes.class  );
                                     final PatternTypes patternType = valueMap.getEnum  ("patternType",PatternTypes.class);
                                     final String       pattern     = valueMap.getString("pattern"                       );

                                     if (!pattern.isEmpty())
                                     {
                                       includeHashMap.put(pattern,new EntryData(entryType,pattern));
                                     }
                                   }
                                 }
                                );
      }
      catch (Exception exception)
      {
        // ignored
      }

      display.syncExec(new Runnable()
      {
        @Override
        public void run()
        {
          if (!widgetIncludeTable.isDisposed())
          {
            Widgets.removeAllTableItems(widgetIncludeTable);
            for (EntryData entryData : includeHashMap.values())
            {
              Widgets.insertTableItem(widgetIncludeTable,
                                      entryDataComparator,
                                      entryData,
                                      entryData.getImage(),
                                      entryData.pattern
                                     );
            }
          }
        }
      });
    }
  }

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

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetPattern;
    final Button widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
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
        button.setToolTipText(BARControl.tr("Select remote path. CTRL+click to select local path."));
        Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String pathName;

            pathName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.ENTRY,
                                    BARControl.tr("Select entry"),
                                    widgetPattern.getText(),
                                    new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                },
                                    "*",
                                    ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                      ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                      : BARControl.listDirectory
                                   );
            if (pathName != null)
            {
              widgetPattern.setText(pathName.trim());
            }
          }
        });
      }

      label = Widgets.newLabel(composite,BARControl.tr("Type")+":",Settings.hasExpertRole());
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,Settings.hasExpertRole());
      subComposite.setLayout(new TableLayout(0.0,0.0));
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        button = Widgets.newRadio(subComposite,BARControl.tr("file"));
        button.setSelection(entryData.entryType == EntryTypes.FILE);
        Widgets.layout(button,0,0,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            entryData.entryType = EntryTypes.FILE;
          }
        });
        button = Widgets.newRadio(subComposite,BARControl.tr("image"));
        button.setSelection(entryData.entryType == EntryTypes.IMAGE);
        Widgets.layout(button,0,1,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            entryData.entryType = EntryTypes.IMAGE;
          }
        });
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetSave = Widgets.newButton(composite,buttonText);
      widgetSave.setEnabled(false);
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      widgetSave.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          entryData.pattern = widgetPattern.getText().trim();
          Dialogs.close(dialog,true);
        }
      });

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetPattern.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text   widget  = (Text)modifyEvent.widget;
        String pattern = widget.getText().trim();

        widgetSave.setEnabled(!pattern.isEmpty());
      }
    });
    widgetPattern.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetSave.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });

    return (Boolean)Dialogs.run(dialog,false) && !entryData.pattern.isEmpty();
  }

  /** add include entry
   * @param entryData entry data
   */
  private void includeListAdd(EntryData entryData)
  {
    final EntryDataComparator entryDataComparator = new EntryDataComparator(widgetIncludeTable);

    assert selectedJobData != null;

    // update include list
    try
    {
      BARServer.executeCommand(StringParser.format("INCLUDE_LIST_ADD jobUUID=%s entryType=%s patternType=%s pattern=%'S",
                                                   selectedJobData.uuid,
                                                   entryData.entryType.toString(),
                                                   "GLOB",
                                                   entryData.pattern
                                                  ),
                               0  // debugLevel
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot add include entry:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
    }

    // update hash map
    synchronized(includeHashMap)
    {
      includeHashMap.put(entryData.pattern,entryData);

      // update/insert table widget
      Widgets.updateInsertTableItem(widgetIncludeTable,
                                    entryDataComparator,
                                    entryData,
                                    entryData.getImage(),
                                    entryData.pattern
                                   );

    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove include patterns
   * @param patterns patterns to remove from include/exclude list
   */
  private void includeListRemove(String[] patterns)
  {
    final EntryDataComparator entryDataComparator = new EntryDataComparator(widgetIncludeTable);

    assert selectedJobData != null;

    synchronized(includeHashMap)
    {
      // remove patterns from hash map
      for (String pattern : patterns)
      {
        includeHashMap.remove(pattern);
      }

      // update include list
      try
      {
        BARServer.executeCommand(StringParser.format("INCLUDE_LIST_CLEAR jobUUID=%s",
                                                     selectedJobData.uuid
                                                    ),
                                 0  // debugLevel
                                );


        for (EntryData entryData : includeHashMap.values())
        {
          BARServer.executeCommand(StringParser.format("INCLUDE_LIST_ADD jobUUID=%s entryType=%s patternType=%s pattern=%'S",
                                                       selectedJobData.uuid,
                                                       entryData.entryType.toString(),
                                                       "GLOB",
                                                       entryData.pattern
                                                      ),
                                   0  // debugLevel
                                  );
        }
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot remove include entry:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }

      display.syncExec(new Runnable()
      {
        @Override
        public void run()
        {
          if (!widgetIncludeTable.isDisposed())
          {
            Widgets.removeAllTableItems(widgetIncludeTable);
            for (EntryData entryData : includeHashMap.values())
            {
              Widgets.insertTableItem(widgetIncludeTable,
                                      entryDataComparator,
                                      entryData,
                                      entryData.getImage(),
                                      entryData.pattern
                                     );
            }
          }
        }
      });
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove include pattern
   * @param pattern pattern to remove from include list
   */
  private void includeListRemove(String pattern)
  {
    includeListRemove(new String[]{pattern});
  }

  /** add new include entry
   */
  private void includeListAdd()
  {
    if (selectedJobData != null)
    {
      EntryData entryData = new EntryData(EntryTypes.FILE);
      if (includeEdit(entryData,BARControl.tr("Add new include pattern"),BARControl.tr("Add")))
      {
        includeListAdd(entryData);
      }
    }
  }

  /** edit currently selected include entry
   */
  private void includeListEdit()
  {
    if (selectedJobData != null)
    {
      TableItem[] tableItems = widgetIncludeTable.getSelection();
      if (tableItems.length > 0)
      {
        EntryData oldEntryData = (EntryData)tableItems[0].getData();
        EntryData newEntryData = oldEntryData.clone();

        if (includeEdit(newEntryData,BARControl.tr("Edit include pattern"),BARControl.tr("Save")))
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
  }

  /** clone currently selected include entry
   */
  private void includeListClone()
  {
    if (selectedJobData != null)
    {
      TableItem[] tableItems = widgetIncludeTable.getSelection();
      if (tableItems.length > 0)
      {
        EntryData entryData = ((EntryData)tableItems[0].getData()).clone();

        if (includeEdit(entryData,BARControl.tr("Clone include pattern"),BARControl.tr("Add")))
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
  }

  /** remove currently selected include entry
   */
  private void includeListRemove()
  {
    if (selectedJobData != null)
    {
      ArrayList<EntryData> entryDataList = new ArrayList<EntryData>();
      for (TableItem tableItem : widgetIncludeTable.getSelection())
      {
        entryDataList.add((EntryData)tableItem.getData());
      }
      if (entryDataList.size() > 0)
      {
        if ((entryDataList.size() == 1) || Dialogs.confirm(shell,BARControl.tr("Remove {0} include {0,choice,0#patterns|1#pattern|1<patterns}?",entryDataList.size())))
        {
          for (EntryData entryData : entryDataList)
          {
            includeListRemove(entryData.pattern);
          }
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** clear exclude list
   */
  private void clearExcludeList()
  {
    if (!widgetExcludeList.isDisposed())
    {
      Widgets.removeAllListItems(widgetExcludeList);
    }
  }

  /** update exclude list
   * @param jobData job data
   */
  private void updateExcludeList(JobData jobData)
  {
    excludeHashSet.clear();

    final ArrayList<PatternData> patternDataList = new ArrayList<PatternData>();
    try
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_LIST jobUUID=%s",
                                                   jobData.uuid
                                                  ),
                               0,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   final PatternTypes patternType = valueMap.getEnum  ("patternType",PatternTypes.class);
                                   final String       pattern     = valueMap.getString("pattern"                       );

                                   if (!pattern.isEmpty())
                                   {
                                     patternDataList.add(new PatternData(patternType,pattern));
                                     excludeHashSet.add(pattern);
                                   }
                                 }
                               }
                              );
    }
    catch (Exception exception)
    {
      // ignored
    }

    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        if (!widgetExcludeList.isDisposed())
        {
          Widgets.removeAllListItems(widgetExcludeList);
          for (PatternData patternData : patternDataList)
          {
            Widgets.insertListItem(widgetExcludeList,
                                   Widgets.getListItemIndex(widgetExcludeList,String.CASE_INSENSITIVE_ORDER,patternData.pattern),
// TODO: patternData instead string
                                   (Object)patternData.pattern,
                                   patternData.pattern
                                  );
          }
        }
      }
    });
  }

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

    assert selectedJobData != null;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetPattern;
    final Button widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Pattern")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPattern = Widgets.newText(composite);
      widgetPattern.setToolTipText(BARControl.tr("Exclude pattern. Use * and ? as wildcards."));
      if (pattern[0] != null) widgetPattern.setText(pattern[0]);
      Widgets.layout(widgetPattern,0,1,TableLayoutData.WE);

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
      button.setToolTipText(BARControl.tr("Select remote path. CTRL+click to select local path."));
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String pathName;

          pathName = Dialogs.file(shell,
                                  Dialogs.FileDialogTypes.ENTRY,
                                  BARControl.tr("Select entry"),
                                  widgetPattern.getText(),
                                  new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                              },
                                  "*",
                                  ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                    ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                    : BARControl.listDirectory
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
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetSave = Widgets.newButton(composite,buttonText);
      widgetSave.setEnabled((pattern[0] != null) && !pattern[0].isEmpty());
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      widgetSave.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          pattern[0] = widgetPattern.getText().trim();
          Dialogs.close(dialog,true);
        }
      });

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetPattern.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text   widget  = (Text)modifyEvent.widget;
        String pattern = widget.getText().trim();

        widgetSave.setEnabled(!pattern.isEmpty());
      }
    });
    widgetPattern.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetSave.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
// TODO:
throw new Error("NYI");
      }
    });

    return (Boolean)Dialogs.run(dialog,false) && !pattern[0].isEmpty();
  }

  /** add exclude pattern
   * @param pattern pattern to add to included/exclude list
   */
  private void excludeListAdd(String pattern)
  {
    assert selectedJobData != null;

    // update exclude list
    try
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                   selectedJobData.uuid,
                                                   "GLOB",
                                                   pattern
                                                  ),
                               0  // debugLevel
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot add exclude entry:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
    }

    // update hash map
    excludeHashSet.add(pattern);

    // update list
    Widgets.updateInsertListItem(widgetExcludeList,
                                 String.CASE_INSENSITIVE_ORDER,
                                 pattern,
                                 pattern
                                );

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove exclude patterns
   * @param patterns pattern to remove from exclude list
   */
  private void excludeListRemove(String[] patterns)
  {
    assert selectedJobData != null;

    // remove patterns from hash set
    for (String pattern : patterns)
    {
      excludeHashSet.remove(pattern);
    }

    // update exclude list
    try
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_CLEAR jobUUID=%s",
                                                   selectedJobData.uuid
                                                  ),
                               0  // debugLevel
                              );
      Widgets.removeAllListItems(widgetExcludeList);

      for (String pattern : excludeHashSet)
      {
        BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                     selectedJobData.uuid,
                                                     "GLOB",
                                                     pattern
                                                    ),
                                  0  // debugLevel
                                 );
        Widgets.insertListItem(widgetExcludeList,
                               Widgets.getListItemIndex(widgetExcludeList,String.CASE_INSENSITIVE_ORDER,pattern),
                               (Object)pattern,
                               pattern
                              );
      }
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot remove exclude entry:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
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
    if (selectedJobData != null)
    {
      String[] pattern = new String[1];
      if (excludeEdit(pattern,BARControl.tr("Add new exclude pattern"),BARControl.tr("Add")))
      {
        excludeListAdd(pattern[0]);
      }
    }
  }

  /** edit currently selected exclude pattern
   */
  private void excludeListEdit()
  {
    if (selectedJobData != null)
    {
      String[] patterns = widgetExcludeList.getSelection();
      if (patterns.length > 0)
      {
        String   oldPattern = patterns[0];
        String[] newPattern = new String[]{new String(oldPattern)};
        if (excludeEdit(newPattern,BARControl.tr("Edit exclude pattern"),BARControl.tr("Save")))
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
  }

  /** clone currently selected exclude pattern
   */
  private void excludeListClone()
  {
    if (selectedJobData != null)
    {
      String[] patterns = widgetExcludeList.getSelection();
      if (patterns.length > 0)
      {
        String[] pattern = new String[]{new String(patterns[0])};
        if (excludeEdit(pattern,BARControl.tr("Clone exclude pattern"),BARControl.tr("Add")))
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
  }

  /** remove currently selected exclude pattern
   */
  private void excludeListRemove()
  {
    if (selectedJobData != null)
    {
      String[] patterns = widgetExcludeList.getSelection();
      if (patterns.length > 0)
      {
        if ((patterns.length == 1) || Dialogs.confirm(shell,BARControl.tr("Remove {0} exclude {0,choice,0#patterns|1#pattern|1<patterns}?",patterns.length)))
        {
          excludeListRemove(patterns);
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** clear compress exclude list
   */
  private void clearCompressExcludeList()
  {
    if (!widgetCompressExcludeList.isDisposed())
    {
      Widgets.removeAllListItems(widgetCompressExcludeList);
    }
  }

  /** update compress exclude list
   * @param jobData job data
   */
  private void updateCompressExcludeList(JobData jobData)
  {
    compressExcludeHashSet.clear();

    final ArrayList<PatternData> patternDataList = new ArrayList<PatternData>();
    try
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST jobUUID=%s",
                                                   jobData.uuid
                                                  ),
                               0,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   final PatternTypes patternType = valueMap.getEnum  ("patternType",PatternTypes.class);
                                   final String       pattern     = valueMap.getString("pattern"                       );

                                   if (!pattern.isEmpty())
                                   {
                                     patternDataList.add(new PatternData(patternType,pattern));
                                     compressExcludeHashSet.add(pattern);
                                   }
                                 }
                               }
                              );
    }
    catch (Exception exception)
    {
      // ignored
    }

    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        if (!widgetCompressExcludeList.isDisposed())
        {
          Widgets.removeAllListItems(widgetCompressExcludeList);
          for (PatternData patternData : patternDataList)
          {
            Widgets.insertListItem(widgetCompressExcludeList,
                                   Widgets.getListItemIndex(widgetCompressExcludeList,String.CASE_INSENSITIVE_ORDER,patternData.pattern),
// TODO: patterData instead of string?
                                   (Object)patternData.pattern,
                                   patternData.pattern
                                  );
          }
        }
      }
    });
  }

  //-----------------------------------------------------------------------

  /** update mount list
   * @param jobData job data
   */
  private void updateMountList(JobData jobData)
  {
    final ArrayList<MountData> mountDataList = new ArrayList<MountData>();
    try
    {
      BARServer.executeCommand(StringParser.format("MOUNT_LIST jobUUID=%s",
                                                   jobData.uuid
                                                  ),
                               0,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   int    id     = valueMap.getInt   ("id"         );
                                   String name   = valueMap.getString("name"       );
                                   String device = valueMap.getString("device",null);

                                   if (!name.isEmpty())
                                   {
                                     mountDataList.add(new MountData(id,name,device));
                                   }
                                 }
                               }
                              );
    }
    catch (Exception exception)
    {
      // ignored
    }

    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        if (!widgetMountTable.isDisposed())
        {
          final MountDataComparator mountDataComparator = new MountDataComparator(widgetMountTable);

          HashSet<TableItem> removeTableItems = Widgets.getAllTableItems(widgetMountTable);

          for (MountData mountData : mountDataList)
          {
            TableItem tableItem = Widgets.updateInsertTableItem(widgetMountTable,
                                                                mountDataComparator,
                                                                mountData,
                                                                mountData.name,
                                                                (mountData.device != null) ? mountData.device : ""
                                                               );
            removeTableItems.remove(tableItem);
          }

          Widgets.removeTableItems(removeTableItems);
        }
      }
    });
  }

  /** edit mount data
   * @param mountData mount data
   * @param title dialog title
   * @param buttonText add button text
   */
  private boolean mountEdit(final MountData mountData, String title, String buttonText)
  {
    Composite composite,subComposite;
    Label     label;
    Button    button;

    assert selectedJobData != null;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetName;
    final Text   widgetDevice;
    final Button widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetName = Widgets.newText(subComposite);
        widgetName.setToolTipText(BARControl.tr("Mount name."));
        if (mountData.name != null) widgetName.setText(mountData.name);
        Widgets.layout(widgetName,0,0,TableLayoutData.WE);

        button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
        button.setToolTipText(BARControl.tr("Select remote path. CTRL+click to select local path."));
        Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String pathName;

            pathName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.DIRECTORY,
                                    BARControl.tr("Select name"),
                                    widgetName.getText(),
                                    ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                      ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                      : BARControl.listDirectory
                                   );
            if (pathName != null)
            {
              widgetName.setText(pathName.trim());
            }
          }
        });
      }

      label = Widgets.newLabel(composite,BARControl.tr("Device")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        widgetDevice = Widgets.newText(subComposite);
        widgetDevice.setToolTipText(BARControl.tr("Mount device (optional)."));
        if (mountData.device != null) widgetDevice.setText(mountData.device);
        Widgets.layout(widgetDevice,0,0,TableLayoutData.WE);

        button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
        button.setToolTipText(BARControl.tr("Select remote path. CTRL+click to select local path."));
        Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            String pathName;

            pathName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.OPEN,
                                    BARControl.tr("Select device"),
                                    widgetDevice.getText(),
                                    ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                      ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                      : BARControl.listDirectory
                                   );
            if (pathName != null)
            {
              widgetDevice.setText(pathName.trim());
            }
          }
        });
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetSave = Widgets.newButton(composite,buttonText);
      widgetSave.setEnabled((mountData.name != null) && !mountData.name.isEmpty());
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      widgetSave.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          mountData.name   = widgetName.getText().trim();
          mountData.device = widgetDevice.getText().trim();
          Dialogs.close(dialog,true);
        }
      });

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetName.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text   widget = (Text)modifyEvent.widget;
        String name   = widget.getText().trim();

        widgetSave.setEnabled(!name.isEmpty());
      }
    });
    widgetName.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetSave.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
// TODO:
throw new Error("NYI");
      }
    });

    return (Boolean)Dialogs.run(dialog,false) && !mountData.name.isEmpty();
  }

  /** add mount data
   * @param mountData mount data
   */
  private void mountListAdd(MountData mountData)
  {
    final MountDataComparator mountDataComparator = new MountDataComparator(widgetMountTable);

    assert selectedJobData != null;

    // add to mount list
    try
    {
      ValueMap valueMap = new ValueMap();
      BARServer.executeCommand(StringParser.format("MOUNT_LIST_ADD jobUUID=%s name=%'S device=%'S",
                                                   selectedJobData.uuid,
                                                   mountData.name,
                                                   mountData.device
                                                  ),
                               0,  // debugLevel
                               valueMap
                              );
      mountData.id = valueMap.getInt("id");
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot add mount data:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
    }

    // insert into table
    Widgets.updateInsertTableItem(widgetMountTable,
                                  mountDataComparator,
                                  mountData,
                                  mountData.name,
                                  (mountData.device != null) ? mountData.device : ""
                                 );

    // remove duplicate names
    TableItem tableItems[] = widgetMountTable.getItems();
    for (TableItem tableItem : tableItems)
    {
      MountData otherMountData = (MountData)tableItem.getData();
      if ((otherMountData != mountData) && otherMountData.name.equals(mountData.name))
      {
        Widgets.removeTableItem(widgetMountTable,otherMountData);
      }
    }
  }

  /** update mount data
   * @param mountData mount data
   */
  private void mountListUpdate(MountData mountData)
  {
    final MountDataComparator mountDataComparator = new MountDataComparator(widgetMountTable);

    assert selectedJobData != null;

    // update mount data
    try
    {
      BARServer.executeCommand(StringParser.format("MOUNT_LIST_UPDATE jobUUID=%s id=%d name=%'S device=%'S",
                                                   selectedJobData.uuid,
                                                   mountData.id,
                                                   mountData.name,
                                                   mountData.device
                                                  ),
                               0  // debugLevel
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot update mount data:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
    }

    // update table item
    Widgets.updateInsertTableItem(widgetMountTable,
                                  mountDataComparator,
                                  mountData,
                                  mountData.name,
                                  (mountData.device != null) ? mountData.device : ""
                                 );

    // remove duplicate names
    TableItem tableItems[] = widgetMountTable.getItems();
    for (TableItem tableItem : tableItems)
    {
      MountData otherMountData = (MountData)tableItem.getData();
      if ((otherMountData != mountData) && otherMountData.name.equals(mountData.name))
      {
        Widgets.removeTableItem(widgetMountTable,otherMountData);
      }
    }
  }

  /** remove mount data
   * @param mountData mount data
   */
  private void mountListRemove(MountData mountData)
  {
    assert selectedJobData != null;

    // remove from mount list
    try
    {
      BARServer.executeCommand(StringParser.format("MOUNT_LIST_REMOVE jobUUID=%s id=%d",
                                                   selectedJobData.uuid,
                                                   mountData.id
                                                  ),
                               0  // debugLevel
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot remove mount data:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
    }

    // remove from table
    Widgets.removeTableItem(widgetMountTable,
                            mountData
                           );
  }

  /** remove mount entries by names
   * @param names names to remove from mount list
   */
  private void mountListRemove(String[] names)
  {
    assert selectedJobData != null;

    TableItem tableItems[] = widgetMountTable.getItems();
    for (TableItem tableItem : tableItems)
    {
      MountData mountData = (MountData)tableItem.getData();
      if (StringUtils.indexOf(names,mountData.name) >= 0)
      {
        mountListRemove(mountData);
      }
    }
  }

  /** remove mount data by name
   * @param name mount name to remove from mount list
   */
  private void mountListRemove(String name)
  {
    mountListRemove(new String[]{name});
  }

  /** add new mount data
   * @param name name
   */
  private void mountListAdd(String name)
  {
    assert selectedJobData != null;

    MountData mountData = new MountData(name);
    if (mountEdit(mountData,BARControl.tr("Add new mount"),BARControl.tr("Add")))
    {
      mountListAdd(mountData);
    }
  }

  /** add new mount data
   */
  private void mountListAdd()
  {
    if (selectedJobData != null)
    {
      mountListAdd("");
    }
  }

  /** edit currently selected mount data
   */
  private void mountListEdit()
  {
    if (selectedJobData != null)
    {
      TableItem[] tableItems = widgetMountTable.getSelection();
      if (tableItems.length > 0)
      {
        MountData mountData = (MountData)tableItems[0].getData();

        if (mountEdit(mountData,BARControl.tr("Edit mount"),BARControl.tr("Save")))
        {
          mountListUpdate(mountData);
        }
      }
    }
  }

  /** clone currently selected mount data
   */
  private void mountListClone()
  {
    if (selectedJobData != null)
    {
      TableItem[] tableItems = widgetMountTable.getSelection();
      if (tableItems.length > 0)
      {
        MountData cloneMountData = (MountData)tableItems[0].getData();

        if (mountEdit(cloneMountData,BARControl.tr("Clone mount"),BARControl.tr("Add")))
        {
          mountListAdd(cloneMountData);
        }
      }
    }
  }

  /** remove currently selected mount entries
   */
  private void mountListRemove()
  {
    if (selectedJobData != null)
    {
      TableItem[] tableItems = widgetMountTable.getSelection();
      if (tableItems.length > 0)
      {
        if ((tableItems.length == 1) || Dialogs.confirm(shell,BARControl.tr("Remove {0} {0,choice,0#mounts|1#mount|1<mounts}?",tableItems.length)))
        {
          for (TableItem tableItem : tableItems)
          {
            mountListRemove((MountData)tableItem.getData());
          }
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** clear source list
   * Note: currently not a list. Show only first source pattern.
   */
  private void clearSourceList()
  {
    deltaSource.set("");
  }

  /** update source list
   * @param jobData job data
   * Note: currently not a list. Show only first source pattern.
   */
  private void updateSourceList(JobData jobData)
  {
    sourceHashSet.clear();
    deltaSource.set("");

//TODO: use list
    try
    {
      BARServer.executeCommand(StringParser.format("SOURCE_LIST jobUUID=%s",
                                                   jobData.uuid
                                                  ),
                               0,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   PatternTypes patternType = valueMap.getEnum  ("patternType",PatternTypes.class);
                                   String       pattern     = valueMap.getString("pattern"                       );

                                   if (!pattern.isEmpty())
                                   {
                                     sourceHashSet.add(pattern);
                                     deltaSource.set(pattern);
                                   }
                                 }
                               }
                              );
    }
    catch (Exception exception)
    {
      // ignored
    }
  }

  //-----------------------------------------------------------------------

  /** set/clear .nobackup file for directory
   * @param name directory name
   * @param enabled true to set/false to remove .nobackup
   * @return true iff set
   */
  private boolean setNoBackup(String name, boolean enabled)
  {
    assert selectedJobData != null;

    // update exclude list
    try
    {
      BARServer.executeCommand(StringParser.format("%s jobUUID=%s attribute=%s name=%'S",
                                                   enabled ? "FILE_ATTRIBUTE_SET" : "FILE_ATTRIBUTE_CLEAR",
                                                   selectedJobData.uuid,
                                                   "NOBACKUP",
                                                   name
                                                  ),
                               0  // debugLevel
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot set/remove .nobackup for {0}:\n\n{1}",
                                  name,
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return false;
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();

    return true;
  }

  /** set/clear no-dump attribute for file
   * @param name directory name
   * @param enabled true to set/clear no-dump attribute
   * @return true iff set
   */
 private boolean setNoDump(String name, boolean enabled)
  {
    assert selectedJobData != null;

    // update exclude list
    try
    {
      BARServer.executeCommand(StringParser.format("%s jobUUID=%s attribute=%s name=%'S",
                                                   enabled ? "FILE_ATTRIBUTE_SET" : "FILE_ATTRIBUTE_CLEAR",
                                                   selectedJobData.uuid,
                                                   "NODUMP",
                                                   name
                                                  ),
                               0  // debugLevel
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot set/clear no-dump attribute for {0}:\n\n{1}",
                                  name,
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return false;
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();

    return true;
  }

  //-----------------------------------------------------------------------

  /** add source entry
   * @param pattern patterns to add to source list
   */
  private void sourceListAdd(String[] patterns)
  {
    assert selectedJobData != null;

    try
    {
      for (String pattern : patterns)
      {
        if (!sourceHashSet.contains(pattern))
        {
          BARServer.executeCommand(StringParser.format("SOURCE_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                       selectedJobData.uuid,
                                                       "GLOB",
                                                       pattern
                                                      ),
                                   0  // debugLevel
                                  );
          sourceHashSet.add(pattern);
        }
      }
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot add source pattern:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
    }
  }

  /** add source pattern
   * @param pattern pattern to add to source list
   */
  private void sourceListAdd(String pattern)
  {
    sourceListAdd(new String[]{pattern});
  }

  /** remove source list patterns
   * @param patterns patterns to remove from source list
   */
  private void sourceListRemove(String[] patterns)
  {
    assert selectedJobData != null;

    // remove patterns from hash map
    for (String pattern : patterns)
    {
      sourceHashSet.remove(pattern);
    }

    // update source list
    try
    {
      BARServer.executeCommand(StringParser.format("SOURCE_LIST_CLEAR jobUUID=%s",
                                                   selectedJobData.uuid
                                                  ),
                               0  // debugLevel
                              );

      for (String pattern : sourceHashSet)
      {
        BARServer.executeCommand(StringParser.format("SOURCE_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                     selectedJobData.uuid,
                                                     "GLOB",
                                                     pattern
                                                    ),
                                 0  // debugLevel
                                );
      }
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot remove source pattern:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
    }
  }

  /** remove source pattern
   * @param pattern pattern to remove from source list
   */
  private void sourceListRemove(String pattern)
  {
    sourceListRemove(new String[]{pattern});
  }

  /** remove all source patterns
   */
  private void sourceListRemoveAll()
  {
    sourceHashSet.clear();

    try
    {
      BARServer.executeCommand(StringParser.format("SOURCE_LIST_CLEAR jobUUID=%s",
                                                   selectedJobData.uuid
                                                  ),
                               0  // debugLevel\
                              );
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,
                    BARControl.tr("Cannot clear source patterns:\n\n{0}",
                                  exception.getMessage()
                                 )
                   );
      BARControl.logThrowable(exception);
      return;
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

    assert selectedJobData != null;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetPattern;
    final Button widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Pattern")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPattern = Widgets.newText(composite);
      widgetPattern.setToolTipText(BARControl.tr("Compress exclude pattern. Use * and ? as wildcards."));
      if (pattern[0] != null) widgetPattern.setText(pattern[0]);
      Widgets.layout(widgetPattern,0,1,TableLayoutData.WE);

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
      button.setToolTipText(BARControl.tr("Select remote path. CTRL+click to select local path."));
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String pathName;

          pathName = Dialogs.file(shell,
                                  Dialogs.FileDialogTypes.OPEN,
                                  BARControl.tr("Select entry"),
                                  widgetPattern.getText(),
                                  new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                              },
                                  "*",
                                  ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                    ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                    : BARControl.listDirectory
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
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetSave = Widgets.newButton(composite,buttonText);
      widgetSave.setEnabled((pattern[0] != null) && !pattern[0].isEmpty());
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetPattern.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text   widget  = (Text)modifyEvent.widget;
        String pattern = widget.getText().trim();

        widgetSave.setEnabled(!pattern.isEmpty());
      }
    });
    widgetPattern.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetSave.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
// TODO:
throw new Error("NYI");
      }
    });
    widgetSave.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        pattern[0] = widgetPattern.getText();
        Dialogs.close(dialog,true);
      }
    });

    return (Boolean)Dialogs.run(dialog,false) && !pattern[0].isEmpty();
  }

  /** add compress exclude pattern
   * @param pattern pattern to add to compress exclude list
   */
  private void compressExcludeListAdd(String pattern)
  {
    assert selectedJobData != null;

    if (!compressExcludeHashSet.contains(pattern))
    {
      try
      {
        BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                     selectedJobData.uuid,
                                                     "GLOB",
                                                     pattern
                                                    ),
                                 0  // debugLevel
                                );
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot add compress exclude entry:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }

      compressExcludeHashSet.add(pattern);
      Widgets.insertListItem(widgetCompressExcludeList,
                             Widgets.getListItemIndex(widgetCompressExcludeList,String.CASE_INSENSITIVE_ORDER,pattern),
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
    if (selectedJobData != null)
    {
      try
      {
        for (String pattern : patterns)
        {
          if (!compressExcludeHashSet.contains(pattern))
          {
            BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                         selectedJobData.uuid,
                                                         "GLOB",
                                                         pattern
                                                        ),
                                     0  // debugLevel
                                    );
          }

          compressExcludeHashSet.add(pattern);
          Widgets.insertListItem(widgetCompressExcludeList,
                                 Widgets.getListItemIndex(widgetCompressExcludeList,String.CASE_INSENSITIVE_ORDER,pattern),
                                 (Object)pattern,
                                 pattern
                                );
        }
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot add compress exclude entry:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }

      // update file tree/device images
      updateFileTreeImages();
      updateDeviceImages();
    }
  }

  /** add new compress exclude pattern
   */
  private void compressExcludeListAdd()
  {
    if (selectedJobData != null)
    {
      String[] pattern = new String[1];
      if (compressExcludeEdit(pattern,BARControl.tr("Add new compress exclude pattern"),BARControl.tr("Add")))
      {
        compressExcludeListAdd(pattern[0]);
      }
    }
  }

  /** edit compress exclude entry
   */
  private void compressExcludeListEdit()
  {
    if (selectedJobData != null)
    {
      String[] patterns = widgetCompressExcludeList.getSelection();
      if (patterns.length > 0)
      {
        String   oldPattern = patterns[0];
        String[] newPattern = new String[]{new String(oldPattern)};

        if (compressExcludeEdit(newPattern,BARControl.tr("Edit compress exclude pattern"),BARControl.tr("Save")))
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
  }

  /** remove compress exclude patterns
   * @param pattern pattern to remove from include/exclude list
   */
  private void compressExcludeListRemove(String[] patterns)
  {
    assert selectedJobData != null;

    for (String pattern : patterns)
    {
      compressExcludeHashSet.remove(pattern);
    }

    try
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%s",
                                                   selectedJobData.uuid
                                                  ),
                               0  // debugLevel
                              );
      Widgets.removeAllListItems(widgetCompressExcludeList);

      for (String pattern : compressExcludeHashSet)
      {
        BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                     selectedJobData.uuid,
                                                     "GLOB",
                                                     pattern
                                                    ),
                                 0  // debugLevel
                                );
        Widgets.insertListItem(widgetCompressExcludeList,
                               Widgets.getListItemIndex(widgetCompressExcludeList,String.CASE_INSENSITIVE_ORDER,pattern),
                               (Object)pattern,
                               pattern
                              );
      }
    }
    catch (Exception exception)
    {
        Dialogs.error(shell,
                      BARControl.tr("Cannot remove compress exclude entry:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
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
    if (selectedJobData != null)
    {
      String[] patterns = widgetCompressExcludeList.getSelection();
      if (patterns.length > 0)
      {
        if (Dialogs.confirm(shell,BARControl.tr("Remove {0} selected compress exclude {0,choice,0#patterns|1#pattern|1<patterns}?",patterns.length)))
        {
          compressExcludeListRemove(patterns);
        }
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

    /** create name part
     * @param ch character
     */
    StorageNamePart(char ch)
    {
      this.string = String.valueOf(ch);
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
      return "StorageNamePart {string="+string+", "+bounds+"}";
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
      Composite  composite,subComposite;
      Label      label;
      Control    control;
      Button     button;
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
          @Override
          public void mouseEnter(MouseEvent mouseEvent)
          {
          }
          @Override
          public void mouseExit(MouseEvent mouseEvent)
          {
            clearHighlight();
          }
          @Override
          public void mouseHover(MouseEvent mouseEvent)
          {
          }
        });
        // Note: needed, because MouseTrackListener.hover() has a delay
        widgetFileName.addMouseMoveListener(new MouseMoveListener()
        {
          @Override
          public void mouseMove(MouseEvent mouseEvent)
          {
            Point point = new Point(mouseEvent.x,mouseEvent.y);
            setHighlight(point);
          }
        });
        widgetFileName.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
            if (   (highlightedNamePart != null)
                && (highlightedNamePart.string != null)
                && (Widgets.isAccelerator(keyEvent,SWT.DEL) || Widgets.isAccelerator(keyEvent,SWT.BS))
               )
            {
              remPart(highlightedNamePart);
            }
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
          }
        });
        dragSource = new DragSource(widgetFileName,DND.DROP_MOVE);
        dragSource.setTransfer(new Transfer[]{StorageNamePartTransfer.getInstance()});
        dragSource.addDragListener(new DragSourceListener()
        {
          @Override
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
          @Override
          public void dragSetData(DragSourceEvent dragSourceEvent)
          {
            dragSourceEvent.data = selectedNamePart;
          }
          @Override
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
          @Override
          public void dragLeave(DropTargetEvent dropTargetEvent)
          {
            clearHighlight();
          }
          @Override
          public void dragOver(DropTargetEvent dropTargetEvent)
          {
            Point point = display.map(null,widgetFileName,dropTargetEvent.x,dropTargetEvent.y);
            setHighlight(point);
          }
          @Override
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
                  addParts(index,((StorageNamePart)dropTargetEvent.data).string);
                }
                else if (dropTargetEvent.data instanceof String)
                {
                  addParts(index,(String)dropTargetEvent.data);
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
          @Override
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
          @Override
          public void dragLeave(DropTargetEvent dropTargetEvent)
          {
          }
          @Override
          public void dragOver(DropTargetEvent dropTargetEvent)
          {
          }
          @Override
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
        addDragAndDrop(composite,"-","'-'",                                                                             0,0);
        addDragAndDrop(composite,"_","'_'",                                                                             1,0);
        addDragAndDrop(composite,BARServer.filePathSeparator,BARServer.filePathSeparator,                               2,0);
        addDragAndDrop(composite,".bar","'.bar'",                                                                       3,0);
        subComposite = Widgets.newComposite(composite,SWT.NONE);
        subComposite.setToolTipText(BARControl.tr("Use drag&drop to add name parts."));
        subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
        {
          widgetText = Widgets.newText(subComposite);
          Widgets.layout(widgetText,0,0,TableLayoutData.WE);

          button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
          button.setToolTipText(BARControl.tr("Select remote file. CTRL+click to select local file."));
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              String fileName;

              fileName = Dialogs.file(shell,
                                      Dialogs.FileDialogTypes.ENTRY,
                                      BARControl.tr("Select source file"),
                                      widgetText.getText(),
                                      new String[]{BARControl.tr("BAR files"),"*.bar",
                                                   BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                        ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                        : BARControl.listDirectory
                                     );
              if (fileName != null)
              {
                widgetText.setText(fileName);
              }
            }
          });
        }
        addDragAndDrop(composite,"Text",subComposite,widgetText,                                                             4,0);

        addDragAndDrop(composite,"#",         BARControl.tr("part number 1 digit"),                                          6,0);
        addDragAndDrop(composite,"##",        BARControl.tr("part number 2 digits"),                                         7,0);
        addDragAndDrop(composite,"###",       BARControl.tr("part number 3 digits"),                                         8,0);
        addDragAndDrop(composite,"####",      BARControl.tr("part number 4 digits"),                                         9,0);

        addDragAndDrop(composite,"%type",     BARControl.tr("archive type: full\n,incremental\n,differential\n,continuous"),11,0);
        addDragAndDrop(composite,"%T",        BARControl.tr("archive type short: F, I, D, C"),                              12,0);
        addDragAndDrop(composite,"%uuid",     BARControl.tr("universally unique identifier"),                               13,0);
        addDragAndDrop(composite,"%text",     BARControl.tr("schedule custom text"),                                        14,0);

        // column 2
        addDragAndDrop(composite,"%d",        BARControl.tr("day 01..31"),                                                   0,1);
        addDragAndDrop(composite,"%j",        BARControl.tr("day of year 001..366"),                                         1,1);
        addDragAndDrop(composite,"%m",        BARControl.tr("month 01..12"),                                                 2,1);
        addDragAndDrop(composite,"%b",        BARControl.tr("month name"),                                                   3,1);
        addDragAndDrop(composite,"%B",        BARControl.tr("full month name"),                                              4,1);
        addDragAndDrop(composite,"%H",        BARControl.tr("hour 00..23"),                                                  5,1);
        addDragAndDrop(composite,"%I",        BARControl.tr("hour 00..12"),                                                  6,1);
        addDragAndDrop(composite,"%M",        BARControl.tr("minute 00..59"),                                                7,1);
        addDragAndDrop(composite,"%S",        BARControl.tr("seconds 00..59"),                                               8,1);
        addDragAndDrop(composite,"%p",        BARControl.tr("'AM' or 'PM'"),                                                 9,1);
        addDragAndDrop(composite,"%P",        BARControl.tr("'am' or 'pm'"),                                                10,1);
        addDragAndDrop(composite,"%a",        BARControl.tr("week day name"),                                               11,1);
        addDragAndDrop(composite,"%A",        BARControl.tr("full week day name"),                                          12,1);
        addDragAndDrop(composite,"%u",        BARControl.tr("day of week 1..7"),                                            13,1);
        addDragAndDrop(composite,"%w",        BARControl.tr("day of week 0..6"),                                            14,1);
        addDragAndDrop(composite,"%U",        BARControl.tr("week number 00..53 starting with Sunday"),                     15,1);
        addDragAndDrop(composite,"%U2",       BARControl.tr("week number 1 or 2 starting with Sunday"),                     16,1);
        addDragAndDrop(composite,"%U4",       BARControl.tr("week number 1, 2, 3, 4 starting with Sunday"),                 17,1);
        addDragAndDrop(composite,"%W",        BARControl.tr("week number 00..53 starting with Monday"),                     18,1);
        addDragAndDrop(composite,"%W2",       BARControl.tr("week number 1 or 2 starting with Monday"),                     19,1);
        addDragAndDrop(composite,"%W4",       BARControl.tr("week number 1, 2, 3, 4 starting with Monday"),                 20,1);
        addDragAndDrop(composite,"%C",        BARControl.tr("century two digits"),                                          21,1);
        addDragAndDrop(composite,"%y",        BARControl.tr("year two digits"),                                             22,1);
        addDragAndDrop(composite,"%Y",        BARControl.tr("year four digits"),                                            23,1);
        addDragAndDrop(composite,"%s",        BARControl.tr("seconds since 1.1.1970 00:00"),                                24,1);
        addDragAndDrop(composite,"%Z",        BARControl.tr("time-zone abbreviation"),                                      25,1);

        // column 3
        addDragAndDrop(composite,"%%",        "%",                                                                           0,2);
        addDragAndDrop(composite,"%#",        "#",                                                                           1,2);
        addDragAndDrop(composite,"%:",        ":",                                                                           2,2);

        addDragAndDrop(composite,"%Y-%m-%d",  BARControl.tr("Date YYYY-MM-DD"),                                              4,2);
        addDragAndDrop(composite,"%H%:%M%:%S",BARControl.tr("Time hh:mm:ss"),                                                5,2);
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
        StringBuilder buffer = new StringBuilder();
        int i = 0;
        while (i < fileName.length())
        {
          switch (fileName.charAt(i))
          {
            case '%':
              // add variable part
              buffer = new StringBuilder();
              buffer.append('%'); i++;
              if (i < fileName.length())
              {
                if      (fileName.charAt(i) == '%')
                {
                  buffer.append('%'); i++;
                }
                else if (fileName.charAt(i) == ':')
                {
                  buffer.append(':'); i++;
                }
                else
                {
                  while (   (i < fileName.length())
                         && Character.isLetterOrDigit(fileName.charAt(i))
                        )
                  {
                    buffer.append(fileName.charAt(i)); i++;
                  }
                }
              }
              storageNamePartList.add(new StorageNamePart(buffer.toString()));
              storageNamePartList.add(new StorageNamePart(null));
              break;
            case '#':
              // add number part
              buffer = new StringBuilder();
              while ((i < fileName.length()) && (fileName.charAt(i) == '#'))
              {
                buffer.append(fileName.charAt(i)); i++;
              }
              storageNamePartList.add(new StorageNamePart(buffer.toString()));
              storageNamePartList.add(new StorageNamePart(null));
              break;
            case '/':
            case '-':
              // separating character
              storageNamePartList.add(new StorageNamePart(fileName.charAt(i)));
              storageNamePartList.add(new StorageNamePart(null));
              i++;
              break;
            case '.':
              // extension
              buffer = new StringBuilder();
              buffer.append('.'); i++;
              while (   (i < fileName.length())
                     && (fileName.charAt(i) != '%')
                     && (fileName.charAt(i) != '#')
                     && (fileName.charAt(i) != '/')
                    )
              {
                buffer.append(fileName.charAt(i)); i++;
              }
              storageNamePartList.add(new StorageNamePart(buffer.toString()));
              storageNamePartList.add(new StorageNamePart(null));
              break;
            default:
              // text part
              buffer = new StringBuilder();
              while (   (i < fileName.length())
                     && (fileName.charAt(i) != '%')
                     && (fileName.charAt(i) != '#')
                     && (fileName.charAt(i) != '/')
                     && (fileName.charAt(i) != '-')
                     && (fileName.charAt(i) != '.')
                    )
              {
                buffer.append(fileName.charAt(i)); i++;
              }
              storageNamePartList.add(new StorageNamePart(buffer.toString()));
              storageNamePartList.add(new StorageNamePart(null));
              break;
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

    /** add drag-and-drop part
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
        @Override
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
        }
        @Override
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          Control control = ((DragSource)dragSourceEvent.widget).getControl();
          dragSourceEvent.data = (String)control.getData();
        }
        @Override
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
        }
      });

      label = Widgets.newLabel(composite,description,SWT.LEFT);
      Widgets.layout(label,row,column*2+1,TableLayoutData.WE);
    }

    /** add drag-and-drop part
     * @param composite composite to add into
     * @param text text to show
     * @param description of part
     * @param row,column row/column
     */
    private void addDragAndDrop(Composite composite, char text, char description, int row, int column)
    {
      addDragAndDrop(composite,Character.toString(text),Character.toString(description),row,column);
    }

    /** add drag-and-drop part
     * @param composite composite to add into
     * @param text text to show
     * @param control control to add
     * @param dragControl drag control
     * @param row,column row/column
     */
    private void addDragAndDrop(Composite composite, String text, Control control, Control dragControl, int row, int column)
    {
      Label label;

      label = Widgets.newLabel(composite,text,SWT.LEFT|SWT.BORDER);
      label.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_GRAY));
      label.setData(dragControl);
      Widgets.layout(label,row,column*2+0,TableLayoutData.W);

      DragSource dragSource = new DragSource(label,DND.DROP_MOVE|DND.DROP_COPY);
      dragSource.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dragSource.addDragListener(new DragSourceListener()
      {
        @Override
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
        @Override
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
        @Override
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
    private void addParts(int index, String string)
    {
      boolean redrawFlag = false;

      // split into parts
      ArrayList<String> parts = new ArrayList<String>();
      StringBuilder buffer;
      int i = 0;
      while (i < string.length())
      {
        switch (string.charAt(i))
        {
          case '%':
            // add variable part
            buffer = new StringBuilder();
            buffer.append('%'); i++;
            if (i < string.length())
            {
              if      (string.charAt(i) == '%')
              {
                buffer.append('%'); i++;
              }
              else if (string.charAt(i) == ':')
              {
                buffer.append(':'); i++;
              }
              else
              {
                while (   (i < string.length())
                       && Character.isLetterOrDigit(string.charAt(i))
                      )
                {
                  buffer.append(string.charAt(i)); i++;
                }
              }
            }
            parts.add(buffer.toString());
            break;
          case '#':
            // add number part
            buffer = new StringBuilder();
            while ((i < string.length()) && (string.charAt(i) == '#'))
            {
              buffer.append(string.charAt(i)); i++;
            }
            parts.add(buffer.toString());
            break;
          default:
            // add text
            buffer = new StringBuilder();
            while (   (i < string.length())
               && (string.charAt(i) != '%')
               && (string.charAt(i) != '#')
              )
            {
              buffer.append(string.charAt(i)); i++;
            }
            parts.add(buffer.toString());
        }
      }

      // insert parts
      synchronized(storageNamePartList)
      {
        if (index < storageNamePartList.size())
        {
          if (storageNamePartList.get(index).string != null)
          {
            // replace
            if (parts.size() > 0)
            {
              storageNamePartList.get(index).string = parts.get(0);
            }
            for (int j = 1; j < parts.size(); j++)
            {
              storageNamePartList.add(index+1,new StorageNamePart(null));
              storageNamePartList.add(index+2,new StorageNamePart(parts.get(j)));
              index += 2;
            }
          }
          else
          {
            // insert
            for (String part : parts)
            {
              storageNamePartList.add(index+1,new StorageNamePart(part));
              storageNamePartList.add(index+2,new StorageNamePart(null));
              index += 2;
            }
          }
        }
        else
        {
          // add
          for (String part : parts)
          {
            storageNamePartList.add(new StorageNamePart(part));
            storageNamePartList.add(new StorageNamePart(null));
          }
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
            else if (storageNamePart.string.equals("%S"))
              buffer.append("34");
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
            else if (storageNamePart.string.equals("%s"))
              buffer.append("1198598100");
            else if (storageNamePart.string.equals("%Z"))
              buffer.append("JST");
            else if (storageNamePart.string.equals("%%"))
              buffer.append("%");
            else if (storageNamePart.string.equals("%#"))
              buffer.append("#");
            else if (storageNamePart.string.equals("%:"))
              buffer.append(":");
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

  /** edit file name with macros
   * @oaram fileName file name to edit
   * @return new file name or null
   */
  private String fileNameEdit(String fileName)
  {
    Composite composite;
    Label     label;
    Button    button;
    Composite subComposite;

    assert selectedJobData != null;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Edit storage file name"),900,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final StorageFileNameEditor storageFileNameEditor;
    final Button                widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    storageFileNameEditor = new StorageFileNameEditor(composite,fileName);

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,2,0,TableLayoutData.WE,0,0,2);
    {
      widgetSave = Widgets.newButton(composite,BARControl.tr("Save"));
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetSave.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Dialogs.close(dialog,storageFileNameEditor.getFileName());
      }
    });

    return (String)Dialogs.run(dialog);
  }

  //-----------------------------------------------------------------------

  /** test script on server
   * @param name script name/type
   * @param script script to test
   */
  private void testScript(String name, String script)
  {
    final BusyDialog busyDialog = new BusyDialog(shell,
                                                 BARControl.tr("Test script results"),
                                                 500,300,
                                                 BusyDialog.LIST|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE
                                                );

    String[] errorMessage = new String[1];
    ValueMap valueMap     = new ValueMap();
    Command command = BARServer.runCommand(StringParser.format("TEST_SCRIPT name=%'S script=%'S",
                                                               name,
                                                               script
                                                              ),
                                           0
                                          );
    while (   !command.endOfData()
           && command.getNextResult(errorMessage,
                                    valueMap,
                                    Command.TIMEOUT
                                   ) == BARException.NONE
          )
    {
      try
      {
        String line = valueMap.getString("line");
        busyDialog.updateList(line);
      }
      catch (IllegalArgumentException exception)
      {
        // ignored
        BARControl.logThrowable(exception);
      }
    }
    busyDialog.done();
  }

  //-----------------------------------------------------------------------

  /** get table item for schedule by UUID
   * @param scheduleUUID schedule UUID
   * @return table item or null if not found
   */
  private TableItem getScheduleTableItemByUUID(String scheduleUUID)
  {
    for (TableItem tableItem : widgetScheduleTable.getItems())
    {
      if (((ScheduleData)tableItem.getData()).uuid.equals(scheduleUUID))
      {
        return tableItem;
      }
    }

    return null;
  }

  /** clear schedule table
   */
  private void clearScheduleTable()
  {
    synchronized(scheduleDataMap)
    {
      scheduleDataMap.clear();
      Widgets.removeAllTableItems(widgetScheduleTable);
    }
  }

  /** update schedule table
   * @param jobData job data
   */
  private void updateScheduleTable(JobData jobData)
  {
    try
    {
      // get schedule list
      try
      {
        final HashMap<String,ScheduleData> newScheduleDataMap = new HashMap<String,ScheduleData>();
        BARServer.executeCommand(StringParser.format("SCHEDULE_LIST jobUUID=%s",
                                                     jobData.uuid
                                                    ),
                                 1,  // debugLevel
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     // get data
                                     String       jobName              = valueMap.getString ("jobName"                       );
                                     String       jobUUID              = valueMap.getString ("jobUUID"                       );
                                     String       scheduleUUID         = valueMap.getString ("scheduleUUID"                  );
                                     String       date                 = valueMap.getString ("date"                          );
                                     String       weekDays             = valueMap.getString ("weekDays"                      );
                                     String       time                 = valueMap.getString ("time"                          );
                                     ArchiveTypes archiveType          = valueMap.getEnum   ("archiveType",ArchiveTypes.class);
                                     int          interval             = valueMap.getInt    ("interval"                      );
                                     String       customText           = valueMap.getString ("customText"                    );
                                     String       beginTime            = valueMap.getString ("beginTime"                     );
                                     String       endTime              = valueMap.getString ("endTime"                       );
                                     boolean      testCreatedArchives  = valueMap.getBoolean("testCreatedArchives"           );
                                     boolean      noStorage            = valueMap.getBoolean("noStorage"                     );
                                     boolean      enabled              = valueMap.getBoolean("enabled"                       );
                                     long         lastExecutedDateTime = valueMap.getLong   ("lastExecutedDateTime"          );
                                     long         nextExecutedDateTime = valueMap.getLong   ("nextExecutedDateTime"          );
// TODO: needed?
                                     long         totalEntities        = valueMap.getLong   ("totalEntities"                 );
                                     long         totalEntryCount      = valueMap.getLong   ("totalEntryCount"               );
                                     long         totalEntrySize       = valueMap.getLong   ("totalEntrySize"                );

                                     ScheduleData scheduleData = scheduleDataMap.get(scheduleUUID);
                                     if (scheduleData != null)
                                     {
                                       scheduleData.setDate(date);
                                       scheduleData.setWeekDays(weekDays);
                                       scheduleData.setTime(time);
                                       scheduleData.archiveType          = archiveType;
                                       scheduleData.interval             = interval;
                                       scheduleData.setBeginTime(beginTime);
                                       scheduleData.setEndTime(endTime);
                                       scheduleData.customText           = customText;
                                       scheduleData.testCreatedArchives  = testCreatedArchives;
                                       scheduleData.noStorage            = noStorage;
                                       scheduleData.enabled              = enabled;
                                       scheduleData.lastExecutedDateTime = lastExecutedDateTime;
                                       scheduleData.totalEntities        = totalEntities;
                                       scheduleData.totalEntryCount      = totalEntryCount;
                                       scheduleData.totalEntrySize       = totalEntrySize;
                                     }
                                     else
                                     {
                                       scheduleData = new ScheduleData(scheduleUUID,
                                                                       date,
                                                                       weekDays,
                                                                       time,
                                                                       archiveType,
                                                                       interval,
                                                                       beginTime,
                                                                       endTime,
                                                                       customText,
                                                                       testCreatedArchives,
                                                                       noStorage,
                                                                       enabled,
                                                                       lastExecutedDateTime,
                                                                       totalEntities,
                                                                       totalEntryCount,
                                                                       totalEntrySize
                                                                      );
                                     }
                                     newScheduleDataMap.put(scheduleUUID,scheduleData);
                                   }
                                 }
                                );
        scheduleDataMap = newScheduleDataMap;
      }
      catch (Exception exception)
      {
        return;
      }

      // update schedule table
      display.syncExec(new Runnable()
      {
        @Override
        public void run()
        {
          if (!widgetScheduleTable.isDisposed())
          {
            synchronized(scheduleDataMap)
            {
              final ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleTable);

              HashSet<TableItem> removeTableItemSet = Widgets.getAllTableItems(widgetScheduleTable);

              for (ScheduleData scheduleData : scheduleDataMap.values())
              {
/*
                // find table item
                TableItem tableItem = Widgets.getTableItem(widgetScheduleTable,scheduleData);

                // update/create table item
                if (tableItem != null)
                {
                  Widgets.updateTableItem(tableItem,
                                          scheduleData,
                                          scheduleData.getDate(),
                                          scheduleData.getWeekDays(),
                                          scheduleData.getTime(),
                                          scheduleData.archiveType.getText(),
                                          scheduleData.getBeginTime(),
                                          scheduleData.getEndTime()
                                          scheduleData.customText,
                                          scheduleData.testCreatedArchives ? BARControl.tr("yes") : BARControl.tr("no")
                                         );
                  tableItem.setChecked(scheduleData.enabled);

                  // keep table item
                  removeTableItemSet.remove(tableItem);
                }
                else
                {
                  // insert new item
                  tableItem = Widgets.insertTableItem(widgetScheduleTable,
                                                      scheduleDataComparator,
                                                      scheduleData,
                                                      scheduleData.getDate(),
                                                      scheduleData.getWeekDays(),
                                                      scheduleData.getTime(),
                                                      scheduleData.archiveType.toString(),
                                                      scheduleData.getBeginTime(),
                                                      scheduleData.getEndTime()
                                                      scheduleData.customText,
                                                      scheduleData.testCreatedArchives ? BARControl.tr("yes") : BARControl.tr("no")
                                                     );
                  tableItem.setChecked(scheduleData.enabled);
                  tableItem.setData(scheduleData);
                }
*/
                TableItem tableItem = Widgets.updateInsertTableItem(widgetScheduleTable,
                                                                    scheduleDataComparator,
                                                                    scheduleData,
                                                                    scheduleData.getDate(),
                                                                    scheduleData.getWeekDays(),
                                                                    scheduleData.getTime(),
                                                                    scheduleData.archiveType.toString(),
                                                                    scheduleData.getBeginTime(),
                                                                    scheduleData.getEndTime(),
                                                                    scheduleData.customText,
                                                                    scheduleData.testCreatedArchives ? BARControl.tr("yes") : BARControl.tr("no")
                                                                   );
                tableItem.setChecked(scheduleData.enabled);

                removeTableItemSet.remove(tableItem);
              }

              Widgets.removeTableItems(removeTableItemSet);
            }
          }
        }
      });
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,BARControl.tr("Cannot get schedule list (error: {0})",error.getMessage()));
      return;
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
    Composite composite,subComposite;
    Label     label;
    Button    button;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Button   widgetTypeDefault,widgetTypeNormal,widgetTypeFull,widgetTypeIncremental,widgetTypeDifferential,widgetTypeContinuous;
    final Combo    widgetYear,widgetMonth,widgetDay;
    final Button[] widgetWeekDays = new Button[7];
    final Combo    widgetHour,widgetMinute;
    final Combo    widgetInterval;
    final Combo    widgetBeginHour,widgetBeginMinute,widgetEndHour,widgetEndMinute;
    final Text     widgetCustomText;
    final Button   widgetTestCreatedArchives;
    final Button   widgetNoStorage;
    final Button   widgetEnabled;
    final Button   widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Type")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetTypeNormal = Widgets.newRadio(subComposite,BARControl.tr("normal"),Settings.hasNormalRole());
        widgetTypeNormal.setToolTipText(BARControl.tr("Execute job as normal backup (no incremental data)."));
        Widgets.layout(widgetTypeNormal,0,0,TableLayoutData.W);
        widgetTypeNormal.setSelection(scheduleData.archiveType == ArchiveTypes.NORMAL);

        widgetTypeFull = Widgets.newRadio(subComposite,BARControl.tr("full"));
        widgetTypeFull.setToolTipText(BARControl.tr("Execute job as full backup."));
        Widgets.layout(widgetTypeFull,0,1,TableLayoutData.W);
        widgetTypeFull.setSelection(scheduleData.archiveType == ArchiveTypes.FULL);

        widgetTypeIncremental = Widgets.newRadio(subComposite,BARControl.tr("incremental"));
        widgetTypeIncremental.setToolTipText(BARControl.tr("Execute job as incremental backup."));
        Widgets.layout(widgetTypeIncremental,0,2,TableLayoutData.W);
        widgetTypeIncremental.setSelection(scheduleData.archiveType == ArchiveTypes.INCREMENTAL);

        widgetTypeDifferential = Widgets.newRadio(subComposite,BARControl.tr("differential"),Settings.hasExpertRole());
        widgetTypeDifferential.setToolTipText(BARControl.tr("Execute job as differential backup."));
        Widgets.layout(widgetTypeDifferential,0,3,TableLayoutData.W);
        widgetTypeDifferential.setSelection(scheduleData.archiveType == ArchiveTypes.DIFFERENTIAL);

        widgetTypeContinuous = Widgets.newRadio(subComposite,BARControl.tr("continuous"),Settings.hasExpertRole());
        widgetTypeContinuous.setToolTipText(BARControl.tr("Execute job as continuous backup."));
        Widgets.layout(widgetTypeContinuous,0,4,TableLayoutData.W);
        widgetTypeContinuous.setSelection(scheduleData.archiveType == ArchiveTypes.CONTINUOUS);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Date")+":",Settings.hasNormalRole());
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE,Settings.hasNormalRole());
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        widgetYear = Widgets.newOptionMenu(subComposite);
        widgetYear.setToolTipText(BARControl.tr("Year to execute job. Leave to '*' for each year."));
        widgetYear.setItems(new String[]{"*","2008","2009","2010","2011","2012","2013","2014","2015","2016","2017","2018","2019","2020","2021","2022","2023","2024","2025"});
        widgetYear.setText(scheduleData.getYear()); if (widgetYear.getText().isEmpty()) widgetYear.setText("*");
        if (widgetYear.getText().isEmpty()) widgetYear.setText("*");
        Widgets.layout(widgetYear,0,0,TableLayoutData.W);

        widgetMonth = Widgets.newOptionMenu(subComposite);
        widgetMonth.setToolTipText(BARControl.tr("Month to execute job. Leave to '*' for each month."));
        widgetMonth.setItems(new String[]{"*","01","02","03","04","05","06","07","08","09","10","11","12"});
        widgetMonth.setText(scheduleData.getMonth()); if (widgetMonth.getText().isEmpty()) widgetMonth.setText("*");
        Widgets.layout(widgetMonth,0,1,TableLayoutData.W);

        widgetDay = Widgets.newOptionMenu(subComposite);
        widgetDay.setToolTipText(BARControl.tr("Day to execute job. Leave to '*' for each day."));
        widgetDay.setItems(new String[]{"*","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31"});
        widgetDay.setText(scheduleData.getDay()); if (widgetDay.getText().isEmpty()) widgetDay.setText("*");
        Widgets.layout(widgetDay,0,2,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Week days")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,2,1,TableLayoutData.WE);
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
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
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
      Widgets.layout(label,3,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,3,1,TableLayoutData.WE);
      {
        widgetHour = Widgets.newOptionMenu(subComposite);
        widgetHour.setEnabled(scheduleData.archiveType != ArchiveTypes.CONTINUOUS);
        widgetHour.setToolTipText(BARControl.tr("Hour to execute job. Leave to '*' for every hour."));
        widgetHour.setItems(new String[]{"*","00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetHour.setText(scheduleData.getHour()); if (widgetHour.getText().isEmpty()) widgetHour.setText("*");
        Widgets.layout(widgetHour,0,0,TableLayoutData.W);

        widgetMinute = Widgets.newOptionMenu(subComposite);
        widgetMinute.setEnabled(scheduleData.archiveType != ArchiveTypes.CONTINUOUS);
        widgetMinute.setToolTipText(BARControl.tr("Minute to execute job. Leave to '*' for every minute."));
        widgetMinute.setItems(new String[]{"*","00","05","10","15","20","30","35","40","45","50","55"});
        widgetMinute.setText(scheduleData.getMinute()); if (widgetMinute.getText().isEmpty()) widgetMinute.setText("*");
        Widgets.layout(widgetMinute,0,1,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Interval")+":",Settings.hasExpertRole());
      Widgets.layout(label,4,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE,Settings.hasExpertRole());
      Widgets.layout(subComposite,4,1,TableLayoutData.WE);
      {
        widgetInterval = Widgets.newOptionMenu(subComposite);
        widgetInterval.setEnabled(scheduleData.archiveType == ArchiveTypes.CONTINUOUS);
        widgetInterval.setToolTipText(BARControl.tr("Interval time for continuous storage."));
        Widgets.setOptionMenuItems(widgetInterval,new Object[]{"",                        0,
                                                               BARControl.tr("1 min"),    1,
                                                               BARControl.tr("5 min"),    5,
                                                               BARControl.tr("10 min"),  10,
                                                               BARControl.tr("30 min"),  30,
                                                               BARControl.tr("1 h"),   1*60,
                                                               BARControl.tr("2 h"),   3*60,
                                                               BARControl.tr("4 h"),   4*60,
                                                               BARControl.tr("8 h"),   8*60
                                                              }
                                  );
        Widgets.setSelectedOptionMenuItem(widgetInterval,new Integer(scheduleData.interval));
        Widgets.layout(widgetInterval,0,0,TableLayoutData.W);

        label = Widgets.newLabel(subComposite,BARControl.tr("Active time")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        widgetBeginHour = Widgets.newOptionMenu(subComposite);
        widgetBeginHour.setEnabled(scheduleData.archiveType == ArchiveTypes.CONTINUOUS);
        widgetBeginHour.setToolTipText(BARControl.tr("Begin hour where continuous storage is active."));
        widgetBeginHour.setItems(new String[]{"*","00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetBeginHour.setText(scheduleData.getBeginHour()); if (widgetBeginHour.getText().isEmpty()) widgetBeginHour.setText("*");
        Widgets.layout(widgetBeginHour,0,2,TableLayoutData.W);

        widgetBeginMinute = Widgets.newOptionMenu(subComposite);
        widgetBeginMinute.setEnabled(scheduleData.archiveType == ArchiveTypes.CONTINUOUS);
        widgetBeginMinute.setToolTipText(BARControl.tr("Begin minute where continuous storage is active."));
        widgetBeginMinute.setItems(new String[]{"*","00","05","10","15","20","30","35","40","45","50","55"});
        widgetBeginMinute.setText(scheduleData.getBeginMinute()); if (widgetBeginMinute.getText().isEmpty()) widgetBeginMinute.setText("*");
        Widgets.layout(widgetBeginMinute,0,3,TableLayoutData.W);

        label = Widgets.newLabel(subComposite,"..");
        Widgets.layout(label,0,4,TableLayoutData.W);

        widgetEndHour = Widgets.newOptionMenu(subComposite);
        widgetEndHour.setEnabled(scheduleData.archiveType == ArchiveTypes.CONTINUOUS);
        widgetEndHour.setToolTipText(BARControl.tr("End hour where continuous storage is active."));
        widgetEndHour.setItems(new String[]{"*","00","01","02","03","04","05","06","07","08","09","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetEndHour.setText(scheduleData.getEndHour()); if (widgetEndHour.getText().isEmpty()) widgetEndHour.setText("*");
        Widgets.layout(widgetEndHour,0,5,TableLayoutData.W);

        widgetEndMinute = Widgets.newOptionMenu(subComposite);
        widgetEndMinute.setEnabled(scheduleData.archiveType == ArchiveTypes.CONTINUOUS);
        widgetEndMinute.setToolTipText(BARControl.tr("End minute where continuous storage is active."));
        widgetEndMinute.setItems(new String[]{"*","00","05","10","15","20","30","35","40","45","50","55"});
        widgetEndMinute.setText(scheduleData.getEndMinute()); if (widgetEndMinute.getText().isEmpty()) widgetEndMinute.setText("*");
        Widgets.layout(widgetEndMinute,0,6,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Custom text")+":",Settings.hasExpertRole());
      Widgets.layout(label,5,0,TableLayoutData.W);

      widgetCustomText = Widgets.newText(composite,Settings.hasExpertRole());
      widgetCustomText.setToolTipText(BARControl.tr("Custom text")+".");
      widgetCustomText.setText(scheduleData.customText);
      Widgets.layout(widgetCustomText,5,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Options")+":");
      Widgets.layout(label,6,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,6,1,TableLayoutData.WE);
      {
        widgetTestCreatedArchives = Widgets.newCheckbox(subComposite,BARControl.tr("test created archives"));
        widgetTestCreatedArchives.setToolTipText(BARControl.tr("Test created archives."));
        Widgets.layout(widgetTestCreatedArchives,0,0,TableLayoutData.W);
        widgetTestCreatedArchives.setSelection(scheduleData.testCreatedArchives);

        widgetNoStorage = Widgets.newCheckbox(subComposite,BARControl.tr("do no create storages"),Settings.hasExpertRole());
        widgetNoStorage.setToolTipText(BARControl.tr("Do not create storage files. Only update incremental data."));
        Widgets.layout(widgetNoStorage,0,1,TableLayoutData.W);
        widgetNoStorage.setSelection(scheduleData.noStorage);

        widgetEnabled = Widgets.newCheckbox(subComposite,BARControl.tr("enabled"));
        Widgets.layout(widgetEnabled,0,2,TableLayoutData.W);
        widgetEnabled.setSelection(scheduleData.enabled);
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetSave = Widgets.newButton(composite,buttonText);
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetTypeContinuous.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;

        widgetHour.setEnabled(!widget.getSelection());
        widgetMinute.setEnabled(!widget.getSelection());

        widgetInterval.setEnabled(widget.getSelection());
        widgetBeginHour.setEnabled(widget.getSelection());
        widgetBeginMinute.setEnabled(widget.getSelection());
        widgetEndHour.setEnabled(widget.getSelection());
        widgetEndMinute.setEnabled(widget.getSelection());
      }
    });
    widgetSave.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        if      (widgetTypeNormal.getSelection())       scheduleData.archiveType = ArchiveTypes.NORMAL;
        else if (widgetTypeFull.getSelection())         scheduleData.archiveType = ArchiveTypes.FULL;
        else if (widgetTypeIncremental.getSelection())  scheduleData.archiveType = ArchiveTypes.INCREMENTAL;
        else if (widgetTypeDifferential.getSelection()) scheduleData.archiveType = ArchiveTypes.DIFFERENTIAL;
        else if (widgetTypeContinuous.getSelection())   scheduleData.archiveType = ArchiveTypes.CONTINUOUS;
        else                                            scheduleData.archiveType = ArchiveTypes.NORMAL;
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
        scheduleData.interval            = (Integer)Widgets.getSelectedOptionMenuItem(widgetInterval,0);
        scheduleData.setBeginTime(widgetBeginHour.getText(),widgetBeginMinute.getText());
        scheduleData.setEndTime(widgetEndHour.getText(),widgetEndMinute.getText());
        scheduleData.customText          = widgetCustomText.getText();
        scheduleData.testCreatedArchives = widgetTestCreatedArchives.getSelection();
        scheduleData.noStorage           = widgetNoStorage.getSelection();
        scheduleData.enabled             = widgetEnabled.getSelection();

        if ((scheduleData.archiveType != ArchiveTypes.CONTINUOUS) && (scheduleData.minute == ScheduleData.ANY))
        {
          if (!Dialogs.confirm(dialog,BARControl.tr("No specific time set. Really execute job every minute?")))
          {
            return;
          }
        }
        if (scheduleData.weekDays == ScheduleData.NONE)
        {
          Dialogs.error(dialog,BARControl.tr("No weekdays specified!"));
          return;
        }
        if ((scheduleData.day != ScheduleData.ANY) && (scheduleData.weekDays != ScheduleData.ANY))
        {
          if (!Dialogs.confirm(dialog,
                               BARControl.tr("The job may not be triggered if the specified day is not in the set of specified weekdays.\nReally keep this setting?")
                              )
             )
          {
            return;
          }
        }

        Dialogs.close(dialog,true);
      }
    });

    return (Boolean)Dialogs.run(dialog,false);
  }

  /** add schedule data
   */
  private void scheduleAddEntry()
  {
    if (selectedJobData != null)
    {
      ScheduleData scheduleData = new ScheduleData();
      if (scheduleEdit(scheduleData,BARControl.tr("New schedule"),BARControl.tr("Add")))
      {
        try
        {
          ValueMap valueMap = new ValueMap();
          BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_ADD jobUUID=%s date=%s weekDays=%s time=%s archiveType=%s interval=%d beginTime=%s endTime=%s customText=%S testCreatedArchives=%y noStorage=%y enabled=%y",
                                                       selectedJobData.uuid,
                                                       scheduleData.getDate(),
                                                       scheduleData.weekDaysToString(),
                                                       scheduleData.getTime(),
                                                       scheduleData.archiveType.toString(),
                                                       scheduleData.interval,
                                                       scheduleData.getBeginTime(),
                                                       scheduleData.getEndTime(),
                                                       scheduleData.customText,
                                                       scheduleData.testCreatedArchives,
                                                       scheduleData.noStorage,
                                                       scheduleData.enabled
                                                      ),
                                   0,  // debugLevel
                                   valueMap
                                  );
          scheduleData.uuid = valueMap.getString("scheduleUUID");
          scheduleDataMap.put(scheduleData.uuid,scheduleData);
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,
                        BARControl.tr("Cannot create new schedule:\n\n{0}",
                                      exception.getMessage()
                                     )
                       );
          BARControl.logThrowable(exception);
          return;
        }

        // update table
        TableItem tableItem = Widgets.updateInsertTableItem(widgetScheduleTable,
                                                            new ScheduleDataComparator(widgetScheduleTable),
                                                            scheduleData,
                                                            scheduleData.getDate(),
                                                            scheduleData.getWeekDays(),
                                                            scheduleData.getTime(),
                                                            scheduleData.archiveType.toString(),
                                                            scheduleData.getBeginTime(),
                                                            scheduleData.getEndTime(),
                                                            scheduleData.customText,
                                                            scheduleData.testCreatedArchives ? BARControl.tr("yes") : BARControl.tr("no")
                                                           );
        tableItem.setChecked(scheduleData.enabled);
        tableItem.setData(scheduleData);

        Widgets.notify(shell,BARControl.USER_EVENT_UPDATE_JOB,selectedJobData.uuid);
      }
    }
  }

  /** edit schedule data
   */
  private void scheduleEditEntry()
  {
    if (selectedJobData != null)
    {
      int index = widgetScheduleTable.getSelectionIndex();
      if (index >= 0)
      {
        TableItem    tableItem    = widgetScheduleTable.getItem(index);
        ScheduleData scheduleData = (ScheduleData)tableItem.getData();

        if (scheduleEdit(scheduleData,BARControl.tr("Edit schedule"),BARControl.tr("Save")))
        {
          try
          {
// TODO: change to SCHEDULE_LIST_UPDATE?
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"date",scheduleData.getDate());
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"weekdays",scheduleData.weekDaysToString());
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"time",scheduleData.getTime());
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"archive-type",scheduleData.archiveType.toString());
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"interval",scheduleData.interval);
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"begin",scheduleData.getBeginTime());
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"end",scheduleData.getEndTime());
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"text",scheduleData.customText);
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"test-created-archives",scheduleData.testCreatedArchives);
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"no-storage",scheduleData.noStorage);
            BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"enabled",scheduleData.enabled);
          }
          catch (Exception exception)
          {
            Dialogs.error(shell,
                          BARControl.tr("Cannot update schedule entry:\n\n{0}",
                                        exception.getMessage()
                                       )
                         );
            BARControl.logThrowable(exception);
            return;
          }

          // update table
          Widgets.updateTableItem(tableItem,
                                  scheduleData,
                                  scheduleData.getDate(),
                                  scheduleData.getWeekDays(),
                                  scheduleData.getTime(),
                                  scheduleData.archiveType.getText(),
                                  scheduleData.getBeginTime(),
                                  scheduleData.getEndTime(),
                                  scheduleData.customText,
                                  scheduleData.testCreatedArchives ? BARControl.tr("yes") : BARControl.tr("no")
                                 );
          tableItem.setChecked(scheduleData.enabled);

          Widgets.notify(shell,BARControl.USER_EVENT_UPDATE_JOB,selectedJobData.uuid);
        }
      }
    }
  }

  /** clone a schedule data
   */
  private void scheduleCloneEntry()
  {
    if (selectedJobData != null)
    {
      int index = widgetScheduleTable.getSelectionIndex();
      if (index >= 0)
      {
        TableItem          tableItem    = widgetScheduleTable.getItem(index);
        final ScheduleData scheduleData = (ScheduleData)tableItem.getData();

        ScheduleData newScheduleData = scheduleData.clone();
        if (scheduleEdit(newScheduleData,BARControl.tr("Clone schedule"),BARControl.tr("Add")))
        {
          try
          {
            ValueMap valueMap = new ValueMap();
            BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_ADD jobUUID=%s date=%s weekDays=%s time=%s archiveType=%s beginTime=%s endTime=%s customText=%S testCreatedArchives=%y noStorage=%y enabled=%y",
                                                         selectedJobData.uuid,
                                                         newScheduleData.getDate(),
                                                         newScheduleData.getWeekDays(),
                                                         newScheduleData.getTime(),
                                                         newScheduleData.archiveType.toString(),
                                                         scheduleData.getBeginTime(),
                                                         scheduleData.getEndTime(),
                                                         newScheduleData.customText,
                                                         newScheduleData.testCreatedArchives,
                                                         newScheduleData.noStorage,
                                                         newScheduleData.enabled
                                                        ),
                                                  0,  // debugLevel
                                                  valueMap
                                                 );
            scheduleData.uuid = valueMap.getString("scheduleUUID");
            scheduleDataMap.put(scheduleData.uuid,newScheduleData);
          }
          catch (Exception exception)
          {
            Dialogs.error(shell,
                          BARControl.tr("Cannot clone new schedule:\n\n{0}",
                                        exception.getMessage()
                                       )
                         );
            BARControl.logThrowable(exception);
            return;
          }

          // update table
          TableItem newTableItem = Widgets.updateInsertTableItem(widgetScheduleTable,
                                                                 new ScheduleDataComparator(widgetScheduleTable),
                                                                 newScheduleData,
                                                                 newScheduleData.getDate(),
                                                                 newScheduleData.getWeekDays(),
                                                                 newScheduleData.getTime(),
                                                                 newScheduleData.archiveType.toString(),
                                                                 newScheduleData.getBeginTime(),
                                                                 newScheduleData.getEndTime(),
                                                                 newScheduleData.customText,
                                                                 newScheduleData.testCreatedArchives ? BARControl.tr("yes") : BARControl.tr("no")
                                                                );
          newTableItem.setChecked(newScheduleData.enabled);
          newTableItem.setData(newScheduleData);

          Widgets.notify(shell,BARControl.USER_EVENT_UPDATE_JOB,selectedJobData.uuid);
        }
      }
    }
  }

  /** delete schedule data
   */
  private void scheduleRemoveEntry()
  {
    if (selectedJobData != null)
    {
      TableItem[] tableItems = widgetScheduleTable.getSelection();
      if (tableItems.length > 0)
      {
        if (Dialogs.confirm(shell,BARControl.tr("Delete {0} selected schedule {0,choice,0#entries|1#entry|1<entries}?",tableItems.length)))
        {
          try
          {
            for (TableItem tableItem : tableItems)
            {
              ScheduleData scheduleData = (ScheduleData)tableItem.getData();

              BARServer.executeCommand(StringParser.format("SCHEDULE_LIST_REMOVE jobUUID=%s scheduleUUID=%s",
                                                            selectedJobData.uuid,
                                                            scheduleData.uuid
                                                           ),
                                       0  // debugLevel
                                      );

              scheduleDataMap.remove(scheduleData.uuid);
              tableItem.dispose();
            }
          }
          catch (Exception exception)
          {
            Dialogs.error(shell,BARControl.tr("Cannot delete schedule:\n\n{0}",exception.getMessage()));
            BARControl.logThrowable(exception);
            return;
          }
        }
      }
    }
  }

  /** trigger schedule data
   */
  private void scheduleTriggerEntry()
  {
    if (selectedJobData != null)
    {
      int index = widgetScheduleTable.getSelectionIndex();
      if (index >= 0)
      {
        TableItem    tableItem    = widgetScheduleTable.getItem(index);
        ScheduleData scheduleData = (ScheduleData)tableItem.getData();

        try
        {
          BARServer.executeCommand(StringParser.format("SCHEDULE_TRIGGER jobUUID=%s scheduleUUID=%s",
                                                       selectedJobData.uuid,
                                                       scheduleData.uuid
                                                      ),
                                   0  // debugLevel
                                  );
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,
                        BARControl.tr("Cannot trigger schedule of job ''{0}'':\n\n{1}",
                                      selectedJobData.name.replaceAll("&","&&"),
                                      exception.getMessage()
                                     )
                       );
          BARControl.logThrowable(exception);
          return;
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** clear persistence table
   */
  private void clearPersistenceTable()
  {
    Widgets.removeAllTreeItems(widgetPersistenceTree);
  }

  /** update persistence table
   * @param jobData job data
   */
  private void updatePersistenceTree(JobData jobData)
  {
    final HashMap<EntityIndexData,Integer> entityIndexDataMap   = new HashMap<EntityIndexData,Integer>();
    final ArrayList<PersistenceData>       persistenceDataArray = new ArrayList<PersistenceData>();

    try
    {
      BARServer.executeCommand(StringParser.format("PERSISTENCE_LIST jobUUID=%s",
                                                   jobData.uuid
                                                  ),
                               1,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   int  persistenceId = valueMap.getInt    ("persistenceId",0);
                                   long entityId      = valueMap.getLong   ("entityId",0L    );

                                   if      (entityId != 0L)
                                   {
                                     ArchiveTypes archiveType     = valueMap.getEnum   ("archiveType",  ArchiveTypes.class,ArchiveTypes.UNKNOWN);
                                     long         createdDateTime = valueMap.getLong   ("createdDateTime"                                      );
                                     long         totalSize       = valueMap.getLong   ("size"                                                 );
                                     long         totalEntryCount = valueMap.getLong   ("totalEntryCount"                                      );
                                     long         totalEntrySize  = valueMap.getLong   ("totalEntrySize"                                       );
                                     boolean      inTransit       = valueMap.getBoolean("inTransit"                                            );

                                     EntityIndexData entityIndexData = new EntityIndexData(entityId,
                                                                                           "", // scheuduleUUID
                                                                                           createdDateTime,
                                                                                           totalSize,
                                                                                           totalEntryCount,
                                                                                           totalEntrySize,
                                                                                           inTransit
                                                                                          );
                                     entityIndexDataMap.put(entityIndexData,persistenceId);
                                   }
                                   else if (persistenceId != 0L)
                                   {
                                     ArchiveTypes archiveType     = valueMap.getEnum   ("archiveType",  ArchiveTypes.class,ArchiveTypes.UNKNOWN);
                                     int          minKeep         = (!valueMap.getString("minKeep","*").equals("*"))
                                                                      ? valueMap.getInt("minKeep")
                                                                      : Keep.ALL;
                                     int          maxKeep         = (!valueMap.getString("maxKeep","*").equals("*"))
                                                                      ? valueMap.getInt("maxKeep")
                                                                      : Keep.ALL;
                                     int          maxAge          = (!valueMap.getString("maxAge","*").equals("*"))
                                                                      ? valueMap.getInt("maxAge")
                                                                      : Age.FOREVER;
                                     String       moveTo          = valueMap.getString ("moveTo"                                               );

                                     PersistenceData persistenceData = new PersistenceData(persistenceId,
                                                                                           archiveType,
                                                                                           minKeep,
                                                                                           maxKeep,
                                                                                           maxAge,
                                                                                           moveTo
                                                                                          );

                                     persistenceDataArray.add(persistenceData);
                                   }
                                 }
                               }
                             );
    }
    catch (Exception exception)
    {
      // ignored
      if (Settings.debugLevel > 0)
      {
        BARControl.printStackTrace(exception);
      }
    }

    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        if (!widgetPersistenceTree.isDisposed())
        {
          final PersistenceDataComparator persistenceDataComparator = new PersistenceDataComparator(PersistenceDataComparator.SortModes.MAX_AGE);

          HashSet<TreeItem> removeTreeItems = Widgets.getAllTreeItems(widgetPersistenceTree);

          // add persistence nodes
          final HashMap<Integer,TreeItem> persistenceTreeItemMap = new HashMap<Integer,TreeItem>();
          for (PersistenceData persistenceData : persistenceDataArray)
          {
            TreeItem treeItem = Widgets.updateInsertTreeItem(widgetPersistenceTree,
                                                             persistenceDataComparator,
                                                             persistenceData,
                                                             Widgets.TREE_ITEM_FLAG_FOLDER,
                                                             persistenceData.archiveType.getText(),
                                                             Keep.format(persistenceData.minKeep),
                                                             Keep.format(persistenceData.maxKeep),
                                                             Age.format(persistenceData.maxAge)
                                                            );
            persistenceTreeItemMap.put(persistenceData.id,treeItem);

            removeTreeItems.remove(treeItem);
          }

          // update/insert/remove persitences nodes
          for (EntityIndexData entityIndexData : entityIndexDataMap.keySet())
          {
            TreeItem persistenceTreeItem = persistenceTreeItemMap.get(entityIndexDataMap.get(entityIndexData));
            if (persistenceTreeItem != null)
            {
              TreeItem treeItem = Widgets.updateInsertTreeItem(persistenceTreeItem,
                                                               entityIndexData,
                                                               Widgets.TREE_ITEM_FLAG_OPEN,
                                                               "",
                                                               "",
                                                               "",
                                                               "",
                                                               SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.createdDateTime*1000)),
                                                               (int)((System.currentTimeMillis()/1000-entityIndexData.createdDateTime)/(24*60*60)),
                                                               Units.formatByteSize(entityIndexData.totalSize)
                                                              );
              if (entityIndexData.inTransit) Widgets.setTreeItemColor(treeItem,COLOR_IN_TRANSIT);

              removeTreeItems.remove(treeItem);
            }
          }

          Widgets.removeTreeItems(removeTreeItems);
        }
      }
    });
  }

  /** edit persistence data
   * @param persistenceData persistence data
   * @param title title text
   * @param buttonText button text
   * @return true if edit OK, false otherwise
   */
  private boolean persistenceEdit(final PersistenceData persistenceData, String title, String buttonText)
  {
    Composite composite;
    Label     label;
    Button    button;
    Composite subComposite,subSubComposite,subSubSubComposite;
    Text      text;
    Combo     combo;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Button            widgetTypeDefault,widgetTypeNormal,widgetTypeFull,widgetTypeIncremental,widgetTypeDifferential,widgetTypeContinuous;
    final Combo             widgetMinKeep,widgetMaxKeep;
    final Combo             widgetMaxAge;
    final Text              widgetMoveToDirectory;
    final Combo             widgetMoveToType;
    final BARWidgets.File   widgetMoveToFile;
    final BARWidgets.FTP    widgetMoveToFTP;
    final BARWidgets.SFTP   widgetMoveToSFTP;
    final BARWidgets.WebDAV widgetMoveToWebDAV;
    final BARWidgets.SMB    widgetMoveToSMB;

    final Button   widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(new double[]{0.0,0.0,0.0,0.0,1.0},new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Type")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetTypeNormal = Widgets.newRadio(subComposite,BARControl.tr("normal"),Settings.hasNormalRole());
        widgetTypeNormal.setToolTipText(BARControl.tr("Execute job as normal backup (no incremental data)."));
        widgetTypeNormal.setSelection(persistenceData.archiveType == ArchiveTypes.NORMAL);
        Widgets.layout(widgetTypeNormal,0,0,TableLayoutData.W);

        widgetTypeFull = Widgets.newRadio(subComposite,BARControl.tr("full"));
        widgetTypeFull.setToolTipText(BARControl.tr("Execute job as full backup."));
        widgetTypeFull.setSelection(persistenceData.archiveType == ArchiveTypes.FULL);
        Widgets.layout(widgetTypeFull,0,1,TableLayoutData.W);

        widgetTypeIncremental = Widgets.newRadio(subComposite,BARControl.tr("incremental"));
        widgetTypeIncremental.setToolTipText(BARControl.tr("Execute job as incremental backup."));
        widgetTypeIncremental.setSelection(persistenceData.archiveType == ArchiveTypes.INCREMENTAL);
        Widgets.layout(widgetTypeIncremental,0,2,TableLayoutData.W);

        widgetTypeDifferential = Widgets.newRadio(subComposite,BARControl.tr("differential"),Settings.hasExpertRole());
        widgetTypeDifferential.setToolTipText(BARControl.tr("Execute job as differential backup."));
        widgetTypeDifferential.setSelection(persistenceData.archiveType == ArchiveTypes.DIFFERENTIAL);
        Widgets.layout(widgetTypeDifferential,0,3,TableLayoutData.W);

        widgetTypeContinuous = Widgets.newRadio(subComposite,BARControl.tr("continuous"),Settings.hasExpertRole());
        widgetTypeContinuous.setToolTipText(BARControl.tr("Execute job as continuous backup."));
        widgetTypeContinuous.setSelection(persistenceData.archiveType == ArchiveTypes.CONTINUOUS);
        Widgets.layout(widgetTypeContinuous,0,4,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Keep")+":",Settings.hasNormalRole());
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        label = Widgets.newLabel(subComposite,BARControl.tr("min")+".:",Settings.hasExpertRole());
        Widgets.layout(label,0,0,TableLayoutData.W);

        widgetMinKeep = Widgets.newOptionMenu(subComposite,Settings.hasExpertRole());
        widgetMinKeep.setToolTipText(BARControl.tr("Min. number of archives to keep."));
        Widgets.setOptionMenuItems(widgetMinKeep,new Object[]{BARControl.tr("unlimited"),Keep.ALL,
                                                              "0",0,
                                                              "1",1,
                                                              "2",2,
                                                              "3",3,
                                                              "4",4,
                                                              "5",5,
                                                              "6",6,
                                                              "7",7,
                                                              "8",8,
                                                              "9",9,
                                                              "10",10
                                                             }
                                  );
        Widgets.setSelectedOptionMenuItem(widgetMinKeep,new Integer(persistenceData.minKeep));
        Widgets.layout(widgetMinKeep,0,1,TableLayoutData.W);

        label = Widgets.newLabel(subComposite,BARControl.tr("max")+".:",Settings.hasNormalRole());
        Widgets.layout(label,0,2,TableLayoutData.W);

        widgetMaxKeep = Widgets.newOptionMenu(subComposite,Settings.hasNormalRole());
        widgetMaxKeep.setToolTipText(BARControl.tr("Max. number of archives to keep."));
        Widgets.setOptionMenuItems(widgetMaxKeep,new Object[]{BARControl.tr("unlimited"),Keep.ALL,
                                                              "1",1,
                                                              "2",2,
                                                              "3",3,
                                                              "4",4,
                                                              "5",5,
                                                              "6",6,
                                                              "7",7,
                                                              "8",8,
                                                              "9",9,
                                                              "10",10
                                                             }
                                  );
        Widgets.setSelectedOptionMenuItem(widgetMaxKeep,new Integer(persistenceData.maxKeep));
        Widgets.layout(widgetMaxKeep,0,3,TableLayoutData.W);

        label = Widgets.newLabel(subComposite,BARControl.tr("age")+":",Settings.hasExpertRole());
        Widgets.layout(label,0,4,TableLayoutData.W);

        widgetMaxAge = Widgets.newOptionMenu(subComposite,Settings.hasExpertRole());
        widgetMaxAge.setToolTipText(BARControl.tr("Max. age of archives to keep."));
        Widgets.setOptionMenuItems(widgetMaxAge,new Object[]{BARControl.tr("forever"),Age.FOREVER,
                                                             BARControl.tr("1 day"),1,
                                                             BARControl.tr("2 days"),2,
                                                             BARControl.tr("3 days"),3,
                                                             BARControl.tr("4 days"),4,
                                                             BARControl.tr("5 days"),5,
                                                             BARControl.tr("6 days"),6,
                                                             BARControl.tr("1 week"),7,
                                                             BARControl.tr("2 weeks"),14,
                                                             BARControl.tr("3 weeks"),21,
                                                             BARControl.tr("1 month"),1*30,
                                                             BARControl.tr("2 months"),2*30,
                                                             BARControl.tr("3 months"),3*30,
                                                             BARControl.tr("6 months"),6*30,
                                                             BARControl.tr("1 year"),1*365,
                                                             BARControl.tr("18 months"),548,
                                                             BARControl.tr("2 years"),2*365,
                                                             BARControl.tr("3 years"),3*365,
                                                             BARControl.tr("4 years"),4*365,
                                                             BARControl.tr("5 years"),5*365
                                                            }
                                  );
        Widgets.setSelectedOptionMenuItem(widgetMaxAge,new Integer(persistenceData.maxAge));
        Widgets.layout(widgetMaxAge,0,5,TableLayoutData.W);
      }

      // move to directory
      label = Widgets.newLabel(composite,BARControl.tr("Move to")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0}));
      Widgets.layout(subComposite,2,1,TableLayoutData.WE);
      {
        widgetMoveToDirectory = Widgets.newText(subComposite);
        widgetMoveToDirectory.setToolTipText(BARControl.tr("Name of directory to move storages to. Several macros are supported. Click on button to the right to open storage file name editor."));
        widgetMoveToDirectory.setText(persistenceData.moveTo.fileName);
        Widgets.layout(widgetMoveToDirectory,0,0,TableLayoutData.WE);

        button = Widgets.newButton(subComposite,IMAGE_EDIT);
        Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            if (selectedJobData != null)
            {
              try
              {
                String fileName = fileNameEdit(widgetMoveToDirectory.getText());
                if (fileName != null)
                {
                  widgetMoveToDirectory.setText(fileName);
                }
              }
              catch (Exception exception)
              {
                // ignored
              }
            }
          }
        });

        button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
        button.setToolTipText(BARControl.tr("Select remote directory. CTRL+click to select local directory."));
        Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            if (selectedJobData != null)
            {
              String directory = Dialogs.file(shell,
                                              Dialogs.FileDialogTypes.DIRECTORY,
                                              BARControl.tr("Select storage directory"),
                                              widgetMoveToDirectory.getText(),
                                              ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                                ? BARServer.remoteListDirectory(selectedJobData.uuid)
                                                : BARControl.listDirectory
                                             );
              if (directory != null)
              {
                widgetMoveToDirectory.setText(directory);
              }
            }
          }
        });
      }

      // destination
      label = Widgets.newLabel(composite,BARControl.tr("Destination")+":");
      Widgets.layout(label,3,0,TableLayoutData.W);
      subComposite = Widgets.newComposite(composite);
      Widgets.layout(subComposite,3,1,TableLayoutData.WE);
      {
        widgetMoveToType = Widgets.newOptionMenu(subComposite);
        widgetMoveToType.setToolTipText(BARControl.tr("Storage destination type:\n"+
                                           "  into file system\n"+
                                           "  on FTP server\n"+
                                           "  on SSH server with scp (secure copy)\n"+
                                           "  on SSH server with sftp (secure FTP)\n"+
                                           "  on WebDAV server\n"+
                                           "  on WebDAV secure server\n"+
                                           "  on SMB/CIFS server\n"
                                          )
                            );
        Widgets.setComboItems(widgetMoveToType,
                              new Object[]{BARControl.tr("file system"),StorageTypes.FILESYSTEM,
                                                         "ftp",         StorageTypes.FTP,
                                                         "scp",         StorageTypes.SCP,
                                                         "sftp",        StorageTypes.SFTP,
                                                         "webdav",      StorageTypes.WEBDAV,
                                                         "webdavs",     StorageTypes.WEBDAVS,
                                                         "smb",         StorageTypes.SMB
                                          }
                        );
        Widgets.setSelectedComboItem(widgetMoveToType,persistenceData.moveTo.type);
        Widgets.layout(widgetMoveToType,0,0,TableLayoutData.W);
      }

      // destination file system
      widgetMoveToFile = new BARWidgets.File(composite);
      Widgets.layout(widgetMoveToFile,4,1,TableLayoutData.WE|TableLayoutData.N);
      {
      }
      Widgets.addModifyListener(new WidgetModifyListener(widgetMoveToFile,widgetMoveToType)
      {
        @Override
        public void modified(Control control, Combo combo)
        {
          Widgets.setVisible(control,Widgets.getSelectedComboItem(combo) == StorageTypes.FILESYSTEM);
          dialog.pack();
        }
      });

      // destination ftp
      widgetMoveToFTP = new BARWidgets.FTP(composite);
      Widgets.layout(widgetMoveToFTP,4,1,TableLayoutData.WE|TableLayoutData.N);
      {
        widgetMoveToFTP.hostName.setText(persistenceData.moveTo.hostName);
        widgetMoveToFTP.loginName.setText(persistenceData.moveTo.loginName);
        widgetMoveToFTP.loginPassword.setText(persistenceData.moveTo.loginPassword);
      }
      Widgets.addModifyListener(new WidgetModifyListener(widgetMoveToFTP,widgetMoveToType)
      {
        @Override
        public void modified(Control control, Combo combo)
        {
          Widgets.setVisible(control,Widgets.getSelectedComboItem(combo) == StorageTypes.FTP);
          dialog.pack();
        }
      });

      // destination scp/sftp
      widgetMoveToSFTP = new BARWidgets.SFTP(composite);
      Widgets.layout(widgetMoveToSFTP,4,1,TableLayoutData.WE|TableLayoutData.N);
      {
        widgetMoveToSFTP.hostName.setText(persistenceData.moveTo.hostName);
        widgetMoveToSFTP.hostPort.setSelection(persistenceData.moveTo.hostPort);
        widgetMoveToSFTP.loginName.setText(persistenceData.moveTo.loginName);
        widgetMoveToSFTP.loginPassword.setText(persistenceData.moveTo.loginPassword);
      }
      Widgets.addModifyListener(new WidgetModifyListener(widgetMoveToSFTP,widgetMoveToType)
      {
        @Override
        public void modified(Control control, Combo combo)
        {
          Widgets.setVisible(control,
                                Widgets.getSelectedComboItem(combo) == StorageTypes.SCP
                             || Widgets.getSelectedComboItem(combo) == StorageTypes.SFTP
                            );
          dialog.pack();
        }
      });

      // destination WebDAV
      widgetMoveToWebDAV = new BARWidgets.WebDAV(composite);
      Widgets.layout(widgetMoveToWebDAV,4,1,TableLayoutData.WE|TableLayoutData.N);
      {
        widgetMoveToSFTP.hostName.setText(persistenceData.moveTo.hostName);
        widgetMoveToSFTP.hostPort.setSelection(persistenceData.moveTo.hostPort);
        widgetMoveToSFTP.loginName.setText(persistenceData.moveTo.loginName);
        widgetMoveToSFTP.loginPassword.setText(persistenceData.moveTo.loginPassword);
      }
      Widgets.addModifyListener(new WidgetModifyListener(widgetMoveToWebDAV,widgetMoveToType)
      {
        @Override
        public void modified(Control control, Combo combo)
        {
          Widgets.setVisible(control,
                                Widgets.getSelectedComboItem(combo) == StorageTypes.WEBDAV
                             || Widgets.getSelectedComboItem(combo) == StorageTypes.WEBDAVS
                            );
          dialog.pack();
        }
      });

      // destination SMB
      widgetMoveToSMB = new BARWidgets.SMB(composite);
      Widgets.layout(widgetMoveToSMB,5,1,TableLayoutData.WE|TableLayoutData.N);
      {
        widgetMoveToSMB.hostName.setText(persistenceData.moveTo.hostName);
        widgetMoveToSMB.loginName.setText(persistenceData.moveTo.loginName);
        widgetMoveToSMB.loginPassword.setText(persistenceData.moveTo.loginPassword);
      }
      Widgets.addModifyListener(new WidgetModifyListener(widgetMoveToSMB,widgetMoveToType)
      {
        @Override
        public void modified(Control control, Combo combo)
        {
          Widgets.setVisible(control,Widgets.getSelectedComboItem(combo) == StorageTypes.SMB);
          dialog.pack();
        }
      });
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetSave = Widgets.newButton(composite,buttonText);
      Widgets.layout(widgetSave,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
/*
    widgetPattern.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetSave.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
*/
    widgetSave.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        if      (widgetTypeNormal.getSelection())       persistenceData.archiveType = ArchiveTypes.NORMAL;
        else if (widgetTypeFull.getSelection())         persistenceData.archiveType = ArchiveTypes.FULL;
        else if (widgetTypeIncremental.getSelection())  persistenceData.archiveType = ArchiveTypes.INCREMENTAL;
        else if (widgetTypeDifferential.getSelection()) persistenceData.archiveType = ArchiveTypes.DIFFERENTIAL;
        else if (widgetTypeContinuous.getSelection())   persistenceData.archiveType = ArchiveTypes.CONTINUOUS;
        else                                            persistenceData.archiveType = ArchiveTypes.NORMAL;
        persistenceData.minKeep = (Integer)Widgets.getSelectedOptionMenuItem(widgetMinKeep,0);
        persistenceData.maxKeep = (Integer)Widgets.getSelectedOptionMenuItem(widgetMaxKeep,0);
        if (persistenceData.maxKeep == 0)
        {
          Dialogs.error(dialog,BARControl.tr("No max. keep set!"));
          return;
        }
        persistenceData.maxAge  = (Integer)Widgets.getSelectedOptionMenuItem(widgetMaxAge,0);
        if (persistenceData.maxAge == 0)
        {
          Dialogs.error(dialog,BARControl.tr("No max. age set!"));
          return;
        }

        persistenceData.moveTo.type     = Widgets.getSelectedComboItem(widgetMoveToType,StorageTypes.FILESYSTEM);
        persistenceData.moveTo.fileName = widgetMoveToDirectory.getText();
        switch (Widgets.getSelectedComboItem(widgetMoveToType,StorageTypes.FILESYSTEM))
        {
          case FILESYSTEM:
            break;
          case FTP:
            persistenceData.moveTo.hostName      = widgetMoveToFTP.hostName.getText();
            persistenceData.moveTo.loginName     = widgetMoveToFTP.loginName.getText();
            persistenceData.moveTo.loginPassword = widgetMoveToFTP.loginPassword.getText();
            break;
          case SCP:
          case SFTP:
            persistenceData.moveTo.hostName      = widgetMoveToSFTP.hostName.getText();
            persistenceData.moveTo.hostPort      = widgetMoveToSFTP.hostPort.getSelection();
            persistenceData.moveTo.loginName     = widgetMoveToSFTP.loginName.getText();
            persistenceData.moveTo.loginPassword = widgetMoveToSFTP.loginPassword.getText();
            break;
          case WEBDAV:
          case WEBDAVS:
            persistenceData.moveTo.hostName      = widgetMoveToWebDAV.hostName.getText();
            persistenceData.moveTo.hostPort      = widgetMoveToWebDAV.hostPort.getSelection();
            persistenceData.moveTo.loginName     = widgetMoveToWebDAV.loginName.getText();
            persistenceData.moveTo.loginPassword = widgetMoveToWebDAV.loginPassword.getText();
            break;
          case SMB:
            persistenceData.moveTo.hostName      = widgetMoveToSMB.hostName.getText();
            persistenceData.moveTo.loginName     = widgetMoveToSMB.loginName.getText();
            persistenceData.moveTo.loginPassword = widgetMoveToSMB.loginPassword.getText();
            break;
        }

        Dialogs.close(dialog,true);
      }
    });

    return (Boolean)Dialogs.run(dialog,false);
  }

  /** add persistence entry
   * @param persistenceData persistence data
   */
  private void persistenceListAdd(PersistenceData persistenceData)
  {
    if (selectedJobData != null)
    {
      // add to persistence list
      try
      {
        ValueMap valueMap = new ValueMap();
        BARServer.executeCommand(StringParser.format("PERSISTENCE_LIST_ADD jobUUID=%s archiveType=%s minKeep=%s maxKeep=%s maxAge=%s moveTo=%'s",
                                                     selectedJobData.uuid,
                                                     persistenceData.archiveType.toString(),
                                                     (persistenceData.minKeep != Keep.ALL) ? persistenceData.minKeep : "*",
                                                     (persistenceData.maxKeep != Keep.ALL) ? persistenceData.maxKeep : "*",
                                                     (persistenceData.maxAge != Age.FOREVER) ? persistenceData.maxAge : "*",
                                                     persistenceData.moveTo.getURI()
                                                    ),
                                 0,  // debugLevel
                                 valueMap
                                );
        persistenceData.id = valueMap.getInt("id");
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot add persistence entry:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }

      updatePersistenceTree(selectedJobData);
    }
  }

  /** update persistence entry
   * @param persistenceData persistence data
   */
  private void persistenceListUpdate(PersistenceData persistenceData)
  {
    if (selectedJobData != null)
    {
      // update persistence list
      try
      {
        BARServer.executeCommand(StringParser.format("PERSISTENCE_LIST_UPDATE jobUUID=%s id=%d archiveType=%s minKeep=%s maxKeep=%s maxAge=%s moveTo=%'s",
                                                     selectedJobData.uuid,
                                                     persistenceData.id,
                                                     persistenceData.archiveType.toString(),
                                                     (persistenceData.minKeep != Keep.ALL) ? persistenceData.minKeep : "*",
                                                     (persistenceData.maxKeep != Keep.ALL) ? persistenceData.maxKeep : "*",
                                                     (persistenceData.maxAge != Age.FOREVER) ? persistenceData.maxAge : "*",
                                                     persistenceData.moveTo.getURI()
                                                    ),
                                 0  // debugLevel
                                );
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot update persistence data:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }

      updatePersistenceTree(selectedJobData);
    }
  }

  /** remove persistence entry
   * @param persistenceData persistence data
   */
  private void persistenceListRemove(PersistenceData persistenceData)
  {
    if (selectedJobData != null)
    {
      // remove from persistence list
      try
      {
        BARServer.executeCommand(StringParser.format("PERSISTENCE_LIST_REMOVE jobUUID=%s id=%d",
                                                     selectedJobData.uuid,
                                                     persistenceData.id
                                                    ),
                                 0  // debugLevel
                                );
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot remove persistence data:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }

      updatePersistenceTree(selectedJobData);
    }
  }

  /** add new persistence entry
   */
  private void persistenceListAdd()
  {
    if (selectedJobData != null)
    {
      PersistenceData persistenceData = new PersistenceData();
      if (persistenceEdit(persistenceData,BARControl.tr("Add new persistence"),BARControl.tr("Add")))
      {
        persistenceListAdd(persistenceData);
      }
    }
  }

  /** edit currently selected persistence entry
   */
  private void persistenceListEdit()
  {
    if (selectedJobData != null)
    {
      TreeItem[] treeItems = widgetPersistenceTree.getSelection();
      if (treeItems.length > 0)
      {
        if (treeItems[0].getData() instanceof PersistenceData)
        {
          PersistenceData persistenceData = (PersistenceData)treeItems[0].getData();

          if (persistenceEdit(persistenceData,BARControl.tr("Edit persistence"),BARControl.tr("Save")))
          {
            persistenceListUpdate(persistenceData);
          }
        }
      }
    }
  }

  /** clone currently selected persistence entry
   */
  private void persistenceListClone()
  {
    if (selectedJobData != null)
    {
      TreeItem[] treeItems = widgetPersistenceTree.getSelection();
      if (treeItems.length > 0)
      {
        PersistenceData persistenceData      = (PersistenceData)treeItems[0].getData();
        PersistenceData clonePersistenceData = (PersistenceData)persistenceData.clone();

        if (persistenceEdit(clonePersistenceData,BARControl.tr("Clone persistence"),BARControl.tr("Add")))
        {
          persistenceListAdd(clonePersistenceData);
        }
      }
    }
  }

  /** remove currently selected persistence entries
   */
  private void persistenceListRemove()
  {
    if (selectedJobData != null)
    {
      TreeItem[] treeItems = widgetPersistenceTree.getSelection();
      if (treeItems.length > 0)
      {
        if ((treeItems.length == 1) || Dialogs.confirm(shell,BARControl.tr("Remove {0} persistence {0,choice,0#entries|1#entry|1<entries}?",treeItems.length)))
        {
          for (TreeItem treeItem : treeItems)
          {
            if      (treeItem.getData() instanceof PersistenceData)
            {
              PersistenceData persistenceData = (PersistenceData)treeItem.getData();
              persistenceListRemove(persistenceData);
            }
            else if (treeItem.getData() instanceof EntityIndexData)
            {
              EntityIndexData entityIndexData = (EntityIndexData)treeItem.getData();
              deleteEntity(entityIndexData);
              Widgets.removeTreeItem(widgetPersistenceTree,entityIndexData);
            }
          }
        }
      }
    }
  }

  /** show entity index tool tip
   * @param entityIndexData entity index data
   * @param x,y positions
   */
  private void showEntityIndexToolTip(EntityIndexData entityIndexData, int x, int y)
  {
    int       row;
    Label     label;
    Separator separator;

    if (widgetPersistenceTreeToolTip != null)
    {
      widgetPersistenceTreeToolTip.dispose();
    }

    if (entityIndexData != null)
    {
      widgetPersistenceTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetPersistenceTreeToolTip.setBackground(COLOR_INFO_BACKGROUND);
      widgetPersistenceTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetPersistenceTreeToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      if (Settings.debugLevel > 0)
      {
        assert (entityIndexData.id & 0x0000000F) == 2 : entityIndexData;

        label = Widgets.newLabel(widgetPersistenceTreeToolTip,BARControl.tr("Entity id")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetPersistenceTreeToolTip,Long.toString(entityIndexData.id >> 4));
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(widgetPersistenceTreeToolTip,BARControl.tr("Schedule UUID")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetPersistenceTreeToolTip,entityIndexData.scheduleUUID);
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetPersistenceTreeToolTip,BARControl.tr("Created")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetPersistenceTreeToolTip,(entityIndexData.createdDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.createdDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetPersistenceTreeToolTip,BARControl.tr("Total size")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetPersistenceTreeToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(entityIndexData.getTotalSize()),entityIndexData.getTotalSize())));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetPersistenceTreeToolTip,BARControl.tr("Total entries")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetPersistenceTreeToolTip,BARControl.tr("{0}",entityIndexData.getTotalEntryCount()));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetPersistenceTreeToolTip,BARControl.tr("Total entries size")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetPersistenceTreeToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(entityIndexData.getTotalEntrySize()),entityIndexData.getTotalEntrySize())));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

/*
      label = Widgets.newLabel(widgetPersistenceTreeToolTip,BARControl.tr("Expire at")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetPersistenceTreeToolTip,(entityIndexData.expireDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.expireDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;
*/

      Point size = widgetPersistenceTreeToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
      widgetPersistenceTreeToolTip.setBounds(x,y,size.x,size.y);
      widgetPersistenceTreeToolTip.setVisible(true);

      shell.addMouseTrackListener(new MouseTrackListener()
      {
        @Override
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }

        @Override
        public void mouseExit(MouseEvent mouseEvent)
        {
          if (widgetPersistenceTreeToolTip != null)
          {
            // check if inside widget
            Point point = shell.toDisplay(new Point(mouseEvent.x,mouseEvent.y));
            if (widgetPersistenceTreeToolTip.getBounds().contains(point))
            {
              return;
            }

            // check if inside sub-widget
            for (Control control : widgetPersistenceTreeToolTip.getChildren())
            {
              if (control.getBounds().contains(point))
              {
                return;
              }
            }

            // close tooltip
            widgetPersistenceTreeToolTip.dispose();
            widgetPersistenceTreeToolTip = null;
          }
        }

        @Override
        public void mouseHover(MouseEvent mouseEvent)
        {
        }
      });
    }
  }

  /** refresh entity index data
   * @param entityIndexData entity index data
   */
  private void refreshEntityIndex(EntityIndexData entityIndexData)
  {
    if (selectedJobData != null)
    {
      // remove from persistence list
      try
      {
        BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=* entityId=%lld",
                                                     entityIndexData.id
                                                    ),
                                 0  // debugLevel
                                );
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot refresh entity index data:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }
    }
  }

  /** remove entity index data
   * @param entityIndexData entity index data
   */
  private void removeEntityIndex(EntityIndexData entityIndexData)
  {
    if (selectedJobData != null)
    {
      // remove from persistence list
      {
        BARControl.waitCursor();
      }
      try
      {
        BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* entityId=%lld",
                                                     entityIndexData.id
                                                    ),
                                  0  // debugLevel
                                 );
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot remove entity index data:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }
      finally
      {
        BARControl.resetCursor();
      }
    }
  }

  /** delete entity
   * @param entityIndexData entity index data
   */
  private void deleteEntity(EntityIndexData entityIndexData)
  {
    if (selectedJobData != null)
    {
      // remove from persistence list
      {
        BARControl.waitCursor();
      }
      try
      {
        BARServer.executeCommand(StringParser.format("STORAGE_DELETE entityId=%lld",
                                                     entityIndexData.id
                                                    ),
                                 0  // debugLevel
                                );
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot delete entity:\n\n{0}",
                                    exception.getMessage()
                                   )
                     );
        BARControl.logThrowable(exception);
        return;
      }
      finally
      {
        BARControl.resetCursor();
      }
    }
  }
}

/* end of file */
