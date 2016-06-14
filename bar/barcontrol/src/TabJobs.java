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
import java.util.Collection;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.ListIterator;

// graphics
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
    boolean      noBackup;                 // true iff .nobackup exists in directory
    boolean      noDump;                   // true iff no-dump attribute is set

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
    FileTreeData(String name, FileTypes fileType, long size, long dateTime, String title, SpecialTypes specialType, boolean noBackup, boolean noDump)
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
    FileTreeData(String name, FileTypes fileType, long size, long dateTime, String title, boolean noBackup, boolean noDump)
    {
      this(name,fileType,size,dateTime,title,SpecialTypes.NONE,noBackup,noDump);
    }

    /** create file tree data
     * @param name file name
     * @param fileType file type
     * @param dateTime file date/time [s]
     * @param title title to display
     * @param noBackup true iff .nobackup exists (directory)
     * @param noDump true iff no-dump attribute is set
     */
    FileTreeData(String name, FileTypes fileType, long dateTime, String title, boolean noBackup, boolean noDump)
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
    FileTreeData(String name, FileTypes fileType, String title, boolean noBackup, boolean noDump)
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
    FileTreeData(String name, SpecialTypes specialType, long size, long dateTime, String title, boolean noBackup, boolean noDump)
    {
      this(name,FileTypes.SPECIAL,size,dateTime,title,specialType,noBackup,noDump);
    }

    /** create file tree data
     * @param name file name
     * @param specialType special type
     * @param dateTime file date/time [s]
     * @param title title to display
     * @param noBackup true iff .nobackup exists (directory)
     * @param noDump true iff no-dump attribute is set
     */
    FileTreeData(String name, SpecialTypes specialType, long dateTime, String title, boolean noBackup, boolean noDump)
    {
      this(name,FileTypes.SPECIAL,0L,dateTime,title,specialType,noBackup,noDump);
    }

    /** get image for entry data
     * @return image
     */
    Image getImage()
    {
      Image image = null;
      if      (includeHashMap.containsKey(name) && !excludeHashSet.contains(name))
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
      else if (excludeHashSet.contains(name) || noBackup || noDump )
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
              case CHARACTER_DEVICE: image = IMAGE_FILE;      break;
              case BLOCK_DEVICE:     image = IMAGE_FILE;      break;
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
      if (fileType == FileTypes.DIRECTORY) setNoBackup(name,false);
      setNoDump(name,false);

      noBackup = false;
      noDump   = false;
    }

    public void excludeByList()
    {
      includeListRemove(name);
      excludeListAdd(name);
      if (fileType == FileTypes.DIRECTORY) setNoBackup(name,false);
      setNoDump(name,false);

      noBackup = false;
      noDump   = false;
    }

    public void excludeByNoBackup()
    {
      includeListRemove(name);
      excludeListRemove(name);
      if (fileType == FileTypes.DIRECTORY) setNoBackup(name,true);
      setNoDump(name,false);

      noBackup = true;
      noDump   = false;
    }

    public void excludeByNoDump()
    {
      includeListRemove(name);
      excludeListRemove(name);
      if (fileType == FileTypes.DIRECTORY) setNoBackup(name,false);
      setNoDump(name,true);

      noBackup = false;
      noDump   = true;
    }

    public void none()
    {
      includeListRemove(name);
      excludeListRemove(name);
      if (fileType == FileTypes.DIRECTORY) setNoBackup(name,false);
      setNoDump(name,false);

      noBackup = false;
      noDump   = false;
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

    private SortModes sortMode;

    /** create file data comparator
     * @param tree file tree
     * @param sortColumn column to sort
     */
    FileTreeDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SortModes.NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SortModes.TYPE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SortModes.SIZE;
      else if (tree.getColumn(3) == sortColumn) sortMode = SortModes.DATETIME;
      else                                      sortMode = SortModes.NAME;
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
      if      (includeHashMap.containsKey(name) && !excludeHashSet.contains(name))
        image = IMAGE_DEVICE_INCLUDED;
      else if (excludeHashSet.contains(name))
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
      return "Device {"+name+", "+size+" bytes}";
    }
  };

  /** device data comparator
   */
  static class DeviceTreeDataComparator implements Comparator<DeviceTreeData>
  {
    // tree sort modes
    enum SortModes
    {
      NAME,
      SIZE
    };

    private SortModes sortMode;

    /** create device data comparator
     * @param tree device tree
     * @param sortColumn column to sort
     */
    DeviceTreeDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SortModes.NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SortModes.SIZE;
      else                                      sortMode = SortModes.NAME;
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
        case NAME:
          return deviceTreeData1.name.compareTo(deviceTreeData2.name);
        case SIZE:
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
            String[] resultErrorMessage = new String[1];
            ValueMap resultMap          = new ValueMap();
            int error = BARServer.executeCommand(StringParser.format("DIRECTORY_INFO name=%S timeout=%d",
                                                                     directoryInfoRequest.name,
                                                                     directoryInfoRequest.timeout
                                                                    ),
                                                 0,  // debugLevel
                                                 resultErrorMessage,
                                                 resultMap
                                                );
            if (error != Errors.NONE)
            {
              // command execution fail or parsing error; ignore request
              continue;
            }
            final long    count    = resultMap.getLong   ("count"   );
            final long    size     = resultMap.getLong   ("size"    );
            final boolean timedOut = resultMap.getBoolean("timedOut");

            // update view
//Dprintf.dprintf("name=%s count=%d size=%d timedOut=%s\n",directoryInfoRequest.name,count,size,timedOut);
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

//Dprintf.dprintf("update %s\n",treeItem.isDisposed());
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
      catch (Exception exception)
      {
        if (Settings.debugLevel > 0)
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

  /** mount data
   */
  class MountData implements Cloneable
  {
    int     id;
    String  name;
    boolean alwaysUnmount;

    /** create mount entry
     * @param id unique id
     * @param name mount name
     * @param alwaysUnmount TRUE to always unmount
     */
    MountData(int id, String name, boolean alwaysUnmount)
    {
      this.id            = id;
      this.name          = name;
      this.alwaysUnmount = alwaysUnmount;
    }

    /** create mount entry
     * @param name mount name
     * @param alwaysUnmount TRUE to always unmount
     */
    MountData(String name, boolean alwaysUnmount)
    {
      this(0,name,alwaysUnmount);
    }

    /** clone mount entry
     * @return clone of object
     */
    public MountData clone()
    {
      return new MountData(name,alwaysUnmount);
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Mount {"+id+", "+name+", "+alwaysUnmount+"}";
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

    String  uuid;
    int     year,month,day;
    int     weekDays;
    int     hour,minute;
//TODO: enum?
    String  archiveType;
    int     interval;
    String  customText;
    int     minKeep,maxKeep;
    int     maxAge;
    boolean noStorage;
    boolean enabled;
    long    lastExecutedDateTime;
    long    totalEntities,totalEntryCount,totalEntrySize;

    /** create schedule data
     * @param uuid schedule UUID
     * @param year year
     * @param month month
     * @param day day
     * @param weekDays week days
     * @param hour hour
     * @param minute minute
     * @param archiveType archive type string
     * @param interval continuous interval [min]
     * @param customText custom text
     * @param minKeep min. number of archives to keep
     * @param maxKeep max. number of archives to keep
     * @param maxAge max. age to keep archives [days]
     * @param noStorage true to skip storage
     * @param enabled true iff enabled
     * @param lastExecutedDateTime date/time of last execution
     * @param totalEntities total number of existing entities for schedule
     * @param totalEntryCount total number of existing entries for schedule
     * @param totalEntrySize total size of existing entries for schedule [bytes]
     */
    ScheduleData(String  uuid,
                 int     year,
                 int     month,
                 int     day,
                 int     weekDays,
                 int     hour,
                 int     minute,
                 String  archiveType,
                 int     interval,
                 String  customText,
                 int     minKeep,
                 int     maxKeep,
                 int     maxAge,
                 boolean noStorage,
                 boolean enabled,
                 long    lastExecutedDateTime,
                 long    totalEntities,
                 long    totalEntryCount,
                 long    totalEntrySize
                )
    {
      this.uuid                 = uuid;
      this.year                 = year;
      this.month                = month;
      this.day                  = day;
      this.weekDays             = weekDays;
      this.hour                 = hour;
      this.minute               = minute;
      this.archiveType          = getValidString(archiveType,new String[]{"normal","full","incremental","differential","continuous"},"normal");
      this.interval             = interval;
      this.customText           = customText;
      this.minKeep              = minKeep;
      this.maxKeep              = maxKeep;
      this.maxAge               = maxAge;
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
      this(null,ScheduleData.ANY,ScheduleData.ANY,ScheduleData.ANY,ScheduleData.ANY,ScheduleData.ANY,ScheduleData.ANY,"normal",0,"",0,0,0,false,true,0,0,0,0);
    }

    /** create schedule data
     * @param date date string (<year>-<month>-<day>)
     * @param weekDays week days string; values separated by ','
     * @param time time string (<hour>:<minute>)
     * @param archiveType archive type string
     * @param interval continuous interval [min]
     * @param customText custom text
     * @param noStorage true to skip storage
     * @param enabled true iff enabled
     * @param lastExecutedDateTime date/time of last execution
     * @param totalEntities total number of existing entities for schedule
     * @param totalEntryCount total number of existing entries for schedule
     * @param totalEntrySize total size of existing entries for schedule [bytes]
     */
    ScheduleData(String  uuid,
                 String  date,
                 String  weekDays,
                 String  time,
                 String  archiveType,
                 int     interval,
                 String  customText,
                 int     minKeep,
                 int     maxKeep,
                 int     maxAge,
                 boolean noStorage,
                 boolean enabled,
                 long    lastExecutedDateTime,
                 long    totalEntities,
                 long    totalEntryCount,
                 long    totalEntrySize
                )
    {
      this.uuid                 = uuid;
      setDate(date);
      setWeekDays(weekDays);
      setTime(time);
      this.archiveType          = getValidString(archiveType,new String[]{"normal","full","incremental","differential","continuous"},"normal");
      this.interval             = interval;
      this.customText           = customText;
      this.minKeep              = minKeep;
      this.maxKeep              = maxKeep;
      this.maxAge               = maxAge;
      this.noStorage            = noStorage;
      this.enabled              = enabled;
      this.lastExecutedDateTime = lastExecutedDateTime;
      this.totalEntities        = totalEntities;
      this.totalEntryCount      = totalEntryCount;
      this.totalEntrySize       = totalEntrySize;
    }

    /** clone schedule data object
     * @return clone of object
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
                              customText,
                              minKeep,
                              maxKeep,
                              maxAge,
                              noStorage,
                              enabled,
                              lastExecutedDateTime,
                              totalEntities,
                              totalEntryCount,
                              totalEntrySize
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

    /** set date
     * @param date date string
     */
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

    /** set time
     * @param time time string
     */
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

    /** get max. number of archives to keep
     * @return max. number of archives to keep
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

    /** get max. age to keep archives
     * @return number of days to keep archives
     */
    void getMaxAge(int maxAge)
    {
      this.maxAge = maxAge;
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

    /** convert data to string
     */
    public String toString()
    {
      return "Schedule {"+uuid+", "+getDate()+", "+getWeekDays()+", "+getTime()+", "+archiveType+", "+noStorage+", "+enabled+"}";
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
  };

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
      CUSTOM_TEXT,
      ENABLED
    };

    private SortModes sortMode;

    private final String[] weekDays = new String[]{"mon","tue","wed","thu","fri","sat","sun"};

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
      else if (table.getColumn(4) == sortColumn) sortMode = SortModes.CUSTOM_TEXT;
      else if (table.getColumn(5) == sortColumn) sortMode = SortModes.ENABLED;
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
      else if (table.getColumn(4) == sortColumn) sortMode = SortModes.CUSTOM_TEXT;
      else if (table.getColumn(5) == sortColumn) sortMode = SortModes.ENABLED;
      else                                       sortMode = SortModes.DATE;
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
        case DATE:
          String date1 = scheduleData1.year+"-"+scheduleData1.month+"-"+scheduleData1.day;
          String date2 = scheduleData2.year+"-"+scheduleData2.month+"-"+scheduleData2.day;

          return date1.compareTo(date2);
        case WEEKDAY:
          if      (scheduleData1.weekDays < scheduleData2.weekDays) return -1;
          else if (scheduleData1.weekDays > scheduleData2.weekDays) return  1;
          else                      return  0;
        case TIME:
          String time1 = scheduleData1.hour+":"+scheduleData1.minute;
          String time2 = scheduleData2.hour+":"+scheduleData2.minute;

          return time1.compareTo(time2);
        case ARCHIVE_TYPE:
          return scheduleData1.archiveType.compareTo(scheduleData2.archiveType);
        case CUSTOM_TEXT:
          return scheduleData1.customText.compareTo(scheduleData2.customText);
        case ENABLED:
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
  private final Color  COLOR_INFO_FORGROUND;
  private final Color  COLOR_INFO_BACKGROUND;

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

  // global variable references
  private Shell        shell;
  private Display      display;
  private TabStatus    tabStatus;

  // widgets
  public  Composite    widgetTab;
  private TabFolder    widgetTabFolder;
  private Combo        widgetJobList;
  private Tree         widgetFileTree;
  private Shell        widgetFileTreeToolTip = null;
  private MenuItem     menuItemOpenClose;
  private MenuItem     menuItemInclude;
  private MenuItem     menuItemExcludeByList;
  private MenuItem     menuItemExcludeByNoBackup;
  private MenuItem     menuItemExcludeByNoDump;
  private MenuItem     menuItemNone;
  private Button       widgetInclude;
  private Button       widgetExclude;
  private Button       widgetNone;
  private Tree         widgetDeviceTree;
  private Table        widgetMountTable;
  private Button       widgetMountTableInsert,widgetMountTableEdit,widgetMountTableRemove;
  private Table        widgetIncludeTable;
  private Button       widgetIncludeTableInsert,widgetIncludeTableEdit,widgetIncludeTableRemove;
  private List         widgetExcludeList;
  private Button       widgetExcludeListInsert,widgetExcludeListEdit,widgetExcludeListRemove;
  private Button       widgetArchivePartSizeLimited;
  private Combo        widgetArchivePartSize;
  private List         widgetCompressExcludeList;
  private Button       widgetCompressExcludeListInsert,widgetCompressExcludeListEdit,widgetCompressExcludeListRemove;
  private Text         widgetCryptPassword1,widgetCryptPassword2;
  private Combo        widgetFTPMaxBandWidth;
  private Combo        widgetSCPSFTPMaxBandWidth;
  private Combo        widgetWebdavMaxBandWidth;
  private Table        widgetScheduleTable;
  private Shell        widgetScheduleTableToolTip = null;
  private Button       widgetScheduleTableAdd,widgetScheduleTableEdit,widgetScheduleTableRemove;

  // BAR variables
  private WidgetVariable  remoteHostName          = new WidgetVariable<String> ("remote-host-name","");
  private WidgetVariable  remoteHostPort          = new WidgetVariable<Long>   ("remote-host-port",0);
  private WidgetVariable  remoteHostForceSSL      = new WidgetVariable<Boolean>("remote-host-force-ssl",false);
  private WidgetVariable  includeFileCommand      = new WidgetVariable<String> ("include-file-command","");
  private WidgetVariable  includeImageCommand     = new WidgetVariable<String> ("include-image-command","");
  private WidgetVariable  excludeCommand          = new WidgetVariable<String> ("exclude-command","");
  private WidgetVariable  archiveType             = new WidgetVariable<String> ("archive-type",new String[]{"normal","full","incremental","differential","continuous"},"normal");
  private WidgetVariable  archivePartSizeFlag     = new WidgetVariable<Boolean>(false);
  private WidgetVariable  archivePartSize         = new WidgetVariable<Long>   ("archive-part-size",0);
  private WidgetVariable  deltaCompressAlgorithm  = new WidgetVariable<String> ("delta-compress-algorithm",
                                                                                new String[]{"none",
                                                                                             "xdelta1","xdelta2","xdelta3","xdelta4","xdelta5","xdelta6","xdelta7","xdelta8","xdelta9"
                                                                                            },
                                                                                "none"
                                                                               );
  private WidgetVariable  deltaSource             = new WidgetVariable<String> ("delta-source","");
  private WidgetVariable  byteCompressAlgorithm   = new WidgetVariable<String> ("byte-compress-algorithm",
                                                                                new String[]{"none",
                                                                                             "zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9",
                                                                                             "bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9",
                                                                                             "lzma1","lzma2","lzma3","lzma4","lzma5","lzma6","lzma7","lzma8","lzma9",
                                                                                             "lzo1","lzo2","lzo3","lzo4","lzo5",
                                                                                             "lz4-0","lz4-1","lz4-2","lz4-3","lz4-4","lz4-5","lz4-6","lz4-7","lz4-8","lz4-9","lz4-10","lz4-11","lz4-12","lz4-13","lz4-14","lz4-15","lz4-16"
                                                                                            },
                                                                                "none"
                                                                               );
  private WidgetVariable  compressMinSize         = new WidgetVariable<Long>   ("compress-min-size",0);
  private WidgetVariable  cryptAlgorithm          = new WidgetVariable<String> ("crypt-algorithm",
                                                                                new String[]{"none",
                                                                                             "3DES",
                                                                                             "CAST5",
                                                                                             "BLOWFISH",
                                                                                             "AES128","AES192","AES256",
                                                                                             "TWOFISH128","TWOFISH256",
                                                                                             "SERPENT128","SERPENT192","SERPENT256",
                                                                                             "CAMELLIA128","CAMELLIA192","CAMELLIA256"
                                                                                            },
                                                                                "none"
                                                                               );
  private WidgetVariable  cryptType               = new WidgetVariable<String> ("crypt-type",new String[]{"none","symmetric","asymmetric"},"none");
  private WidgetVariable  cryptPublicKeyFileName  = new WidgetVariable<String> ("crypt-public-key","");
  private WidgetVariable  cryptPasswordMode       = new WidgetVariable<String> ("crypt-password-mode",new String[]{"default","ask","config"},"default");
  private WidgetVariable  cryptPassword           = new WidgetVariable<String> ("crypt-password","");
  private WidgetVariable  incrementalListFileName = new WidgetVariable<String> ("incremental-list-file","");
  private WidgetVariable  storageType             = new WidgetVariable<String> ("storage-type",
                                                                                new String[]{"filesystem",
                                                                                             "ftp",
                                                                                             "scp",
                                                                                             "sftp",
                                                                                             "webdav",
                                                                                             "cd",
                                                                                             "dvd",
                                                                                             "bd",
                                                                                             "device"
                                                                                            },
                                                                                "filesystem"
                                                                               );
  private WidgetVariable  storageHostName         = new WidgetVariable<String> ("");
  private WidgetVariable  storageHostPort         = new WidgetVariable<Long>   ("",0);
  private WidgetVariable  storageLoginName        = new WidgetVariable<String> ("","");
  private WidgetVariable  storageLoginPassword    = new WidgetVariable<String> ("","");
  private WidgetVariable  storageDeviceName       = new WidgetVariable<String> ("","");
  private WidgetVariable  storageFileName         = new WidgetVariable<String> ("","");
  private WidgetVariable  maxStorageSize          = new WidgetVariable<Long>   ("max-storage-size",0);
  private WidgetVariable  archiveFileMode         = new WidgetVariable<String> ("archive-file-mode",new String[]{"stop","overwrite","append"},"stop");
  private WidgetVariable  sshPublicKeyFileName    = new WidgetVariable<String> ("ssh-public-key","");
  private WidgetVariable  sshPrivateKeyFileName   = new WidgetVariable<String> ("ssh-private-key","");
  private WidgetVariable  maxBandWidthFlag        = new WidgetVariable<Boolean>(false);
  private WidgetVariable  maxBandWidth            = new WidgetVariable<Long>   ("max-band-width",0);
  private WidgetVariable  volumeSize              = new WidgetVariable<Long>   ("volume-size",0);
  private WidgetVariable  ecc                     = new WidgetVariable<Boolean>("ecc",false);
  private WidgetVariable  waitFirstVolume         = new WidgetVariable<Boolean>("wait-first-volume",false);
  private WidgetVariable  skipUnreadable          = new WidgetVariable<Boolean>("skip-unreadable",false);
  private WidgetVariable  rawImages               = new WidgetVariable<Boolean>("raw-images",false);
  private WidgetVariable  overwriteFiles          = new WidgetVariable<Boolean>("overwrite-files",false);
  private WidgetVariable  preCommand              = new WidgetVariable<String> ("pre-command","");
  private WidgetVariable  postCommand             = new WidgetVariable<String> ("post-command","");
  private WidgetVariable  comment                 = new WidgetVariable<String> ("comment","");

  // variables
  private DirectoryInfoThread          directoryInfoThread;
  private boolean                      directorySizesFlag     = false;
  private JobData                      selectedJobData        = null;
  private WidgetEvent                  selectJobEvent         = new WidgetEvent();
  private HashMap<String,EntryData>    includeHashMap         = new HashMap<String,EntryData>();
  private HashSet<String>              excludeHashSet         = new HashSet<String>();
  private HashSet<String>              sourceHashSet          = new HashSet<String>();
  private HashSet<String>              compressExcludeHashSet = new HashSet<String>();
  private HashMap<String,ScheduleData> scheduleDataMap        = new HashMap<String,ScheduleData>();

  /** create jobs tab
   * @param parentTabFolder parent tab folder
   * @param accelerator keyboard shortcut to select tab
   */
  TabJobs(TabFolder parentTabFolder, int accelerator)
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
    TreeColumn  treeColumn;
    TreeItem    treeItem;
    Control     control;
    Text        text;
    StyledText  styledText;
    TableColumn tableColumn;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    // get colors
    COLOR_BLACK           = display.getSystemColor(SWT.COLOR_BLACK);
    COLOR_WHITE           = display.getSystemColor(SWT.COLOR_WHITE);
    COLOR_RED             = display.getSystemColor(SWT.COLOR_RED);
    COLOR_MODIFIED        = new Color(null,0xFF,0xA0,0xA0);
    COLOR_INFO_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
    COLOR_INFO_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

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
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Jobs")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{0.0,0.0,1.0,0.0},1.0,2));
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
            if (tabStatus != null) tabStatus.setSelectedJob(selectedJobData);
            update();

            selectJobEvent.trigger();
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

      button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
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

      button = Widgets.newButton(composite,BARControl.tr("Rename")+"\u2026");
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

    composite = Widgets.newComposite(widgetTab);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0,0.0}));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Host")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      text = Widgets.newText(composite);
      text.setToolTipText(BARControl.tr("Host to run job. Leave empty to run on local host."));
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

          if (remoteHostName.getString().equals(string)) color = null;
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

          remoteHostName.set(string);
          BARServer.setJobOption(selectedJobData.uuid,remoteHostName);
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
          Text   widget = (Text)focusEvent.widget;
          String string = widget.getText().trim();

          remoteHostName.set(string);
          BARServer.setJobOption(selectedJobData.uuid,remoteHostName);
          widget.setBackground(null);
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(text,remoteHostName));

      label = Widgets.newLabel(composite,BARControl.tr("Port")+":");
      Widgets.layout(label,0,2,TableLayoutData.W);

      spinner = Widgets.newSpinner(composite);
      spinner.setToolTipText(BARControl.tr("Port number. Set to 0 to use default port number from configuration file."));
      spinner.setMinimum(0);
      spinner.setMaximum(65535);
      spinner.setEnabled(false);
      Widgets.layout(spinner,0,3,TableLayoutData.W,0,0,0,0,70,SWT.DEFAULT);
      Widgets.addEventListener(new WidgetEventListener(spinner,selectJobEvent)
      {
        @Override
        public void trigger(Control control)
        {
          Widgets.setEnabled(control,selectedJobData != null);
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

          if (remoteHostPort.getLong() == n) color = null;
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

          remoteHostPort.set(n);
          BARServer.setJobOption(selectedJobData.uuid,remoteHostPort);
          widget.setBackground(null);
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Spinner widget = (Spinner)selectionEvent.widget;
          int     n      = widget.getSelection();

          remoteHostPort.set(n);
          BARServer.setJobOption(selectedJobData.uuid,remoteHostPort);
          widget.setBackground(null);
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

          remoteHostPort.set(n);
          BARServer.setJobOption(selectedJobData.uuid,remoteHostPort);
          widget.setBackground(null);
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(spinner,remoteHostPort));

      button = Widgets.newCheckbox(composite,BARControl.tr("SSL"));
      button.setToolTipText(BARControl.tr("Enable to force SSL connection."));
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
          Button widget = (Button)selectionEvent.widget;
          remoteHostForceSSL.set(widget.getSelection());
          BARServer.setJobOption(selectedJobData.uuid,remoteHostForceSSL);
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(button,remoteHostForceSSL));
    }

    // create sub-tabs
    widgetTabFolder = Widgets.newTabFolder(widgetTab);
    Widgets.setEnabled(widgetTabFolder,false);
    Widgets.layout(widgetTabFolder,2,0,TableLayoutData.NSWE);
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
            final TreeItem treeItem = (TreeItem)event.item;
            updateFileTree(treeItem);
          }
        });
        widgetFileTree.addListener(SWT.Collapse,new Listener()
        {
          @Override
          public void handleEvent(final Event event)
          {
            final TreeItem treeItem = (TreeItem)event.item;
            treeItem.removeAll();
            new TreeItem(treeItem,SWT.NONE);
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
              if      (includeHashMap.containsKey(fileTreeData.name) && !excludeHashSet.contains(fileTreeData.name))
                isIncluded = true;
              else if (fileTreeData.noBackup)
                isExcludedByNoBackup = true;
              else if (fileTreeData.noDump)
                isExcludedByNoDump = true;
              else if (excludeHashSet.contains(fileTreeData.name))
                isExcludedByList = true;
              else
                isNone     = true;

              menuItemOpenClose.setEnabled(fileTreeData.fileType == FileTypes.DIRECTORY);
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

            // show if table item available and mouse is in the right side
            if (mouseEvent.x > tree.getBounds().width/2)
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
                @Override
                public void mouseEnter(MouseEvent mouseEvent)
                {
                }
                @Override
                public void mouseExit(MouseEvent mouseEvent)
                {
                  widgetFileTreeToolTip.dispose();
                  widgetFileTreeToolTip = null;
                }
                @Override
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

          menuItemInclude = Widgets.addMenuRadio(menu,BARControl.tr("Include"));
          menuItemInclude.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

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

          menuItemExcludeByList = Widgets.addMenuRadio(menu,BARControl.tr("Exclude by list"));
          menuItemExcludeByList.addSelectionListener(new SelectionListener()
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

          menuItemExcludeByNoBackup = Widgets.addMenuRadio(menu,BARControl.tr("Exclude by .nobackup"));
          menuItemExcludeByNoBackup.addSelectionListener(new SelectionListener()
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
          });

          menuItemExcludeByNoDump = Widgets.addMenuRadio(menu,BARControl.tr("Exclude by no dump"));
          menuItemExcludeByNoDump.addSelectionListener(new SelectionListener()
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
          });

          menuItemNone = Widgets.addMenuRadio(menu,BARControl.tr("None"));
          menuItemNone.addSelectionListener(new SelectionListener()
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

          Widgets.addMenuSeparator(menu);

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add mount")+"\u2026");
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

              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                mountListAdd(fileTreeData.name);
              }
            }
          });

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove mount"));
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

              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                mountListRemove(fileTreeData.name);
              }
            }
          });

          Widgets.addMenuSeparator(menu);

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
              MenuItem widget = (MenuItem)selectionEvent.widget;

              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                directoryInfoThread.add(fileTreeData.name,true,treeItem);
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
              Button widget = (Button)selectionEvent.widget;
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
              Button widget = (Button)selectionEvent.widget;
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
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
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

              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                deviceTreeData.include();
                treeItem.setImage(IMAGE_DEVICE_INCLUDED);
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
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                deviceTreeData.exclude();
                treeItem.setImage(IMAGE_DEVICE_EXCLUDED);
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                deviceTreeData.include();
                treeItem.setImage(IMAGE_DEVICE_INCLUDED);
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
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                deviceTreeData.exclude();
                treeItem.setImage(IMAGE_DEVICE_EXCLUDED);
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
              for (TreeItem treeItem : widgetDeviceTree.getSelection())
              {
                DeviceTreeData deviceTreeData = (DeviceTreeData)treeItem.getData();
                deviceTreeData.none();
                includeListRemove(deviceTreeData.name);
                excludeListRemove(deviceTreeData.name);
                treeItem.setImage(IMAGE_DEVICE);
              }
            }
          });
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Filters && Mounts"));
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
          Widgets.addTableColumn(widgetIncludeTable,0,SWT.LEFT,20);
          Widgets.addTableColumn(widgetIncludeTable,1,SWT.LEFT,1024,true);
//????
// automatic column width calculation?
//widgetIncludeTable.setLayout(new TableLayout(new double[]{0.5,0.0,0.5,0.0,0.0},new double[]{0.0,1.0}));
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
                Widgets.invoke(widgetIncludeTableInsert);
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
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  includeListAdd();
                }
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
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  includeListEdit();
                }
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026");
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
                if (selectedJobData != null)
                {
                  includeListClone();
                }
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
                if (selectedJobData != null)
                {
                  includeListRemove();
                }
              }
            });
          }
          widgetIncludeTable.setMenu(menu);

          // buttons
          composite = Widgets.newComposite(subTab,SWT.NONE,4);
          Widgets.layout(composite,1,0,TableLayoutData.W);
          {
            widgetIncludeTableInsert = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
            widgetIncludeTableInsert.setToolTipText(BARControl.tr("Add entry to included list."));
            Widgets.layout(widgetIncludeTableInsert,0,0,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetIncludeTableInsert.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  includeListAdd();
                }
              }
            });

            widgetIncludeTableEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
            widgetIncludeTableEdit.setToolTipText(BARControl.tr("Edit entry in included list."));
            Widgets.layout(widgetIncludeTableEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetIncludeTableEdit.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  includeListEdit();
                }
              }
            });

            button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
            button.setToolTipText(BARControl.tr("Clone entry in included list."));
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
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  includeListClone();
                }
              }
            });

            widgetIncludeTableRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
            widgetIncludeTableRemove.setToolTipText(BARControl.tr("Remove entry from included list."));
            Widgets.layout(widgetIncludeTableRemove,0,3,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetIncludeTableRemove.addSelectionListener(new SelectionListener()
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
                  includeListRemove();
                }
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
                Widgets.invoke(widgetExcludeListInsert);
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
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  excludeListAdd();
                }
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
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  excludeListEdit();
                }
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026");
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
                if (selectedJobData != null)
                {
                  excludeListClone();
                }
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
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  excludeListRemove();
                }
              }
            });
          }
          widgetExcludeList.setMenu(menu);

          // buttons
          composite = Widgets.newComposite(subTab,SWT.NONE,4);
          Widgets.layout(composite,1,0,TableLayoutData.W);
          {
            widgetExcludeListInsert = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
            widgetExcludeListInsert.setToolTipText(BARControl.tr("Add entry to excluded list."));
            Widgets.layout(widgetExcludeListInsert,0,0,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetExcludeListInsert.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  excludeListAdd();
                }
              }
            });

            widgetExcludeListEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
            widgetExcludeListEdit.setToolTipText(BARControl.tr("Edit entry in excluded list."));
            Widgets.layout(widgetExcludeListEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetExcludeListEdit.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  excludeListEdit();
                }
              }
            });

            button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
            button.setToolTipText(BARControl.tr("Clone entry in excluded list."));
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
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  excludeListClone();
                }
              }
            });

            widgetExcludeListRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
            widgetExcludeListRemove.setToolTipText(BARControl.tr("Remove entry from excluded list."));
            Widgets.layout(widgetExcludeListRemove,0,3,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetExcludeListRemove.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  excludeListRemove();
                }
              }
            });
          }
        }

        // include command tab
        subTab = Widgets.addTab(tabFolder,BARControl.tr("Include command"));
//        subTab.setLayout(new TableLayout(1.0,1.0));
        Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
        {
          subComposite = Widgets.newComposite(subTab);
          subComposite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
          Widgets.layout(subComposite,0,0,TableLayoutData.NSWE);
          {
            styledText = Widgets.newStyledText(subComposite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
            styledText.setToolTipText(BARControl.tr("Command or script to execute to get a list of include entries."));
            Widgets.layout(styledText,0,0,TableLayoutData.NSWE);
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

                includeFileCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                BARServer.setJobOption(selectedJobData.uuid,includeFileCommand);
                widget.setBackground(null);
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

                includeFileCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                BARServer.setJobOption(selectedJobData.uuid,includeFileCommand);
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(styledText,includeFileCommand));

            // buttons
            button = Widgets.newButton(subComposite,BARControl.tr("Test")+"\u2026");
            button.setToolTipText(BARControl.tr("Test script."));
            Widgets.layout(button,1,0,TableLayoutData.E);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                testScript(includeFileCommand.getString());
              }
            });
          }

          subComposite = Widgets.newComposite(subTab);
          subComposite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
          Widgets.layout(subComposite,1,0,TableLayoutData.NSWE);
          {
            styledText = Widgets.newStyledText(subComposite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
            styledText.setToolTipText(BARControl.tr("Command or script to execute to get a list of include images."));
            Widgets.layout(styledText,0,0,TableLayoutData.NSWE);
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

                includeImageCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                BARServer.setJobOption(selectedJobData.uuid,includeImageCommand);
                widget.setBackground(null);
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

                includeImageCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
                BARServer.setJobOption(selectedJobData.uuid,includeImageCommand);
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(styledText,includeImageCommand));

            // buttons
            button = Widgets.newButton(subComposite,BARControl.tr("Test")+"\u2026");
            button.setToolTipText(BARControl.tr("Test script."));
            Widgets.layout(button,1,0,TableLayoutData.E);
            button.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                testScript(includeImageCommand.getString());
              }
            });
          }
        }

        // excluded command tab
        subTab = Widgets.addTab(tabFolder,BARControl.tr("Exclude command"));
        subTab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
        {
          styledText = Widgets.newStyledText(subTab,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
          styledText.setToolTipText(BARControl.tr("Command or script to execute to get a list of include patterns."));
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

              excludeCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
              BARServer.setJobOption(selectedJobData.uuid,excludeCommand);
              widget.setBackground(null);
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

              excludeCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
              BARServer.setJobOption(selectedJobData.uuid,excludeCommand);
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(styledText,excludeCommand));

          // buttons
          button = Widgets.newButton(subTab,BARControl.tr("Test")+"\u2026");
          button.setToolTipText(BARControl.tr("Test script."));
          Widgets.layout(button,1,0,TableLayoutData.E);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              testScript(excludeCommand.getString());
            }
          });
        }

        // mount tab
        subTab = Widgets.addTab(tabFolder,BARControl.tr("Mounts"));
        subTab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(subTab,0,0,TableLayoutData.NSWE);
        {
          widgetMountTable = Widgets.newTable(subTab);
          widgetMountTable.setToolTipText(BARControl.tr("List of devices to mount, right-click for context menu."));
//????
// automatic column width calculation?
//widgetIncludeTable.setLayout(new TableLayout(new double[]{0.5,0.0,0.5,0.0,0.0},new double[]{0.0,1.0}));
          Widgets.layout(widgetMountTable,0,0,TableLayoutData.NSWE);
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
                Widgets.invoke(widgetMountTableInsert);
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
          tableColumn = Widgets.addTableColumn(widgetMountTable,0,BARControl.tr("Name"),          SWT.LEFT,600,true );
          tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
          tableColumn = Widgets.addTableColumn(widgetMountTable,1,BARControl.tr("Always unmount"),SWT.LEFT,100,false);
          tableColumn.addSelectionListener(Widgets.DEFAULT_TABLE_SELECTION_LISTENER_STRING);
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
                Widgets.invoke(widgetExcludeListInsert);
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
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  mountListAdd();
                }
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
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  mountListEdit();
                }
              }
            });

            menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026");
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
                if (selectedJobData != null)
                {
                  mountListClone();
                }
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
                if (selectedJobData != null)
                {
                  mountListRemove();
                }
              }
            });
          }
          widgetMountTable.setMenu(menu);

          // buttons
          composite = Widgets.newComposite(subTab,SWT.NONE,4);
          Widgets.layout(composite,1,0,TableLayoutData.W);
          {
            widgetMountTableInsert = Widgets.newButton(composite,BARControl.tr("Add")+"\u2026");
            widgetMountTableInsert.setToolTipText(BARControl.tr("Add entry to mount list."));
            Widgets.layout(widgetMountTableInsert,0,0,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetMountTableInsert.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  mountListAdd();
                }
              }
            });

            widgetMountTableEdit = Widgets.newButton(composite,BARControl.tr("Edit")+"\u2026");
            widgetMountTableEdit.setToolTipText(BARControl.tr("Edit entry in mount list."));
            Widgets.layout(widgetMountTableEdit,0,1,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetMountTableEdit.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  mountListEdit();
                }
              }
            });

            button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
            button.setToolTipText(BARControl.tr("Clone entry in mount list."));
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
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  mountListClone();
                }
              }
            });

            widgetMountTableRemove = Widgets.newButton(composite,BARControl.tr("Remove")+"\u2026");
            widgetMountTableRemove.setToolTipText(BARControl.tr("Remove entry from mount list."));
            Widgets.layout(widgetMountTableRemove,0,3,TableLayoutData.DEFAULT,0,0,0,0,110,SWT.DEFAULT);
            widgetMountTableRemove.addSelectionListener(new SelectionListener()
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
                  mountListRemove();
                }
              }
            });
          }
        }

        // options
        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0}));
        Widgets.layout(composite,1,0,TableLayoutData.W);
        {
          label = Widgets.newLabel(composite,BARControl.tr("Options")+":");
          Widgets.layout(label,0,0,TableLayoutData.N);

          subComposite = Widgets.newComposite(composite);
          Widgets.layout(subComposite,0,1,TableLayoutData.WE);
          {
            button = Widgets.newCheckbox(subComposite,BARControl.tr("skip unreadable entries"));
            button.setToolTipText(BARControl.tr("If enabled then skip not readable entries (write information to log file).\nIf disabled stop job with an error."));
            Widgets.layout(button,0,0,TableLayoutData.NW);
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

                skipUnreadable.set(checkedFlag);
                BARServer.setJobOption(selectedJobData.uuid,skipUnreadable);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,skipUnreadable));

            button = Widgets.newCheckbox(subComposite,BARControl.tr("raw images"));
            button.setToolTipText(BARControl.tr("If enabled then store all data of a device into an image.\nIf disabled try to detect file system and only store used blocks to image."));
            Widgets.layout(button,1,0,TableLayoutData.NW);
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

                rawImages.set(checkedFlag);
                BARServer.setJobOption(selectedJobData.uuid,rawImages);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,rawImages));
          }
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              boolean changedFlag = archivePartSizeFlag.set(false);
              archivePartSize.set(0);
              BARServer.setJobOption(selectedJobData.uuid,archivePartSize);

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
              Button widget = (Button)selectionEvent.widget;
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
          widgetArchivePartSize.setItems(new String[]{"32M","64M","128M","140M","256M","280M","512M","600M","1G","2G","4G","8G","10G","20G"});
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
                  Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                  widget.forceFocus();
                }
              }
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();
              try
              {
                long  n = Units.parseByteSize(string);
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
                  Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                  widget.forceFocus();
                }
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
                  Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
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

Dprintf.dprintf("");
            if (bounds.contains(mouseEvent.x,mouseEvent.y))
            {
Dprintf.dprintf("");
              archivePartSizeFlag.set(true);
widgetArchivePartSize.setListVisible(true);
//              widgetArchivePartSizeLimited.setSelection(true);
//                selectRestoreToEvent.trigger();
//              Widgets.setFocus(widgetArchivePartSize);
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();

              deltaCompressAlgorithm.set(string);
              BARServer.setJobOption(selectedJobData.uuid,"compress-algorithm",deltaCompressAlgorithm.getString()+"+"+byteCompressAlgorithm.getString());
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
                                      "lz4-0","lz4-1","lz4-2","lz4-3","lz4-4","lz4-5","lz4-6","lz4-7","lz4-8","lz4-9","lz4-10","lz4-11","lz4-12","lz4-13","lz4-14","lz4-15","lz4-16"
                                     }
                        );
          Widgets.layout(combo,0,3,TableLayoutData.W);
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
              String string = widget.getText();

              byteCompressAlgorithm.set(string);
              BARServer.setJobOption(selectedJobData.uuid,"compress-algorithm",deltaCompressAlgorithm.getString()+"+"+byteCompressAlgorithm.getString());
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

              if ((selectionEvent.stateMask & SWT.CTRL) == 0)
              {
                fileName = Dialogs.file(shell,
                                        Dialogs.FileDialogTypes.SAVE,
                                        BARControl.tr("Select source file"),
                                        deltaSource.getString(),
                                        new String[]{BARControl.tr("BAR files"),"*.bar",
                                                     BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*",
                                        BARServer.remoteListDirectory
                                       );
              }
              else
              {
                fileName = Dialogs.fileSave(shell,
                                            BARControl.tr("Select source file"),
                                            deltaSource.getString(),
                                            new String[]{BARControl.tr("BAR files"),"*.bar",
                                                         BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                        }
                                           );
              }
              if (fileName != null)
              {
                deltaSource.set(fileName);
                sourceListRemoveAll();
                sourceListAdd(fileName);
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
                MenuItem widget = (MenuItem)selectionEvent.widget;

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

                MenuItem widget = (MenuItem)selectionEvent.widget;

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

                MenuItem widget = (MenuItem)selectionEvent.widget;

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
                MenuItem widget = (MenuItem)selectionEvent.widget;
                if (selectedJobData != null)
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
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  compressExcludeListAdd();
                }
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
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
                {
                  compressExcludeListEdit();
                }
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
                Button widget = (Button)selectionEvent.widget;
                if (selectedJobData != null)
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
          combo.setItems(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256","SERPENT128","SERPENT192","SERPENT256","CAMELLIA128","CAMELLIA192","CAMELLIA256"});
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
              Combo  widget = (Combo)selectionEvent.widget;
              String string = widget.getText();

              cryptAlgorithm.set(string);
              BARServer.setJobOption(selectedJobData.uuid,cryptAlgorithm);
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
              cryptType.set("symmetric");
              BARServer.setJobOption(selectedJobData.uuid,cryptType);
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
              cryptType.set("asymmetric");
              BARServer.setJobOption(selectedJobData.uuid,cryptType);
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

              cryptPublicKeyFileName.set(string);
              BARServer.setJobOption(selectedJobData.uuid,cryptPublicKeyFileName);
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
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();

              cryptPublicKeyFileName.set(string);
              BARServer.setJobOption(selectedJobData.uuid,cryptPublicKeyFileName);
              widget.setBackground(null);
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

              if ((selectionEvent.stateMask & SWT.CTRL) == 0)
              {
                fileName = Dialogs.file(shell,
                                        Dialogs.FileDialogTypes.OPEN,
                                        BARControl.tr("Select public key file"),
                                        cryptPublicKeyFileName.getString(),
                                        new String[]{BARControl.tr("Public key"),"*.public",
                                                     BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*",
                                        BARServer.remoteListDirectory
                                       );
              }
              else
              {
                fileName = Dialogs.fileOpen(shell,
                                            BARControl.tr("Select public key file"),
                                            cryptPublicKeyFileName.getString(),
                                            new String[]{BARControl.tr("Public key"),"*.public",
                                                         BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                        }
                                           );
              }
              if (fileName != null)
              {
                cryptPublicKeyFileName.set(fileName);
                BARServer.setJobOption(selectedJobData.uuid,cryptPublicKeyFileName);
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
              cryptPasswordMode.set("default");
              BARServer.setJobOption(selectedJobData.uuid,cryptPasswordMode);
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
              cryptPasswordMode.set("ask");
              BARServer.setJobOption(selectedJobData.uuid,cryptPasswordMode);
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
              cryptPasswordMode.set("config");
              BARServer.setJobOption(selectedJobData.uuid,cryptPasswordMode);
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
                cryptPassword.set(string1);
                BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
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
                cryptPassword.set(string1);
                BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
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
          Widgets.addModifyListener(new WidgetModifyListener(widgetCryptPassword2,cryptAlgorithm,cryptType,cryptPasswordMode)
          {
            @Override
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
                cryptPassword.set(string1);
                BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
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
                cryptPassword.set(string1);
                BARServer.setJobOption(selectedJobData.uuid,cryptPassword);
                widgetCryptPassword1.setBackground(null);
                widgetCryptPassword2.setBackground(null);
              }
              else
              {
                Dialogs.error(shell,BARControl.tr("Crypt passwords are not equal!"));
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              archiveType.set("normal");
              BARServer.setJobOption(selectedJobData.uuid,archiveType);
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
              archiveType.set("full");
              BARServer.setJobOption(selectedJobData.uuid,archiveType);
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
              archiveType.set("incremental");
              BARServer.setJobOption(selectedJobData.uuid,archiveType);
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
              archiveType.set("differential");
              BARServer.setJobOption(selectedJobData.uuid,archiveType);
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

              storageFileName.set(widget.getText());
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

              storageFileName.set(widget.getText());
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(text,storageFileName));

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
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobData != null)
              {
                storageFileNameEdit();
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

                if ((selectionEvent.stateMask & SWT.CTRL) == 0)
                {
                  fileName = Dialogs.file(shell,
                                          Dialogs.FileDialogTypes.SAVE,
                                          BARControl.tr("Select storage file name"),
                                          storageFileName.getString(),
                                          new String[]{BARControl.tr("BAR files"),"*.bar",
                                                       BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                      },
                                          "*",
                                          BARServer.remoteListDirectory
                                         );
                }
                else
                {
                  fileName = Dialogs.fileSave(shell,
                                              BARControl.tr("Select storage file name"),
                                              storageFileName.getString(),
                                              new String[]{BARControl.tr("BAR files"),"*.bar",
                                                           BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                          }
                                             );
                }
                if (fileName != null)
                {
                  storageFileName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

              incrementalListFileName.set(string);
              BARServer.setJobOption(selectedJobData.uuid,incrementalListFileName);
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
              Text   widget = (Text)focusEvent.widget;
              String string = widget.getText();

              incrementalListFileName.set(string);
              BARServer.setJobOption(selectedJobData.uuid,incrementalListFileName);
              widget.setBackground(null);
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

              if ((selectionEvent.stateMask & SWT.CTRL) == 0)
              {
                fileName = Dialogs.file(shell,
                                        Dialogs.FileDialogTypes.SAVE,
                                        BARControl.tr("Select incremental file"),
                                        incrementalListFileName.getString(),
                                        new String[]{BARControl.tr("BAR incremental data"),"*.bid",
                                                     BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*",
                                        BARServer.remoteListDirectory
                                       );
              }
              else
              {
                fileName = Dialogs.fileSave(shell,
                                            BARControl.tr("Select incremental file"),
                                            incrementalListFileName.getString(),
                                            new String[]{BARControl.tr("BAR incremental data"),"*.bid",
                                                         BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                        }
                                           );
              }
              if (fileName != null)
              {
                incrementalListFileName.set(fileName);
                BARServer.setJobOption(selectedJobData.uuid,incrementalListFileName);
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              storageType.set("filesystem");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              storageType.set("ftp");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              storageType.set("scp");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              storageType.set("sftp");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              storageType.set("webdav");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              boolean changedFlag = storageType.set("cd");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());

              if (   changedFlag
                  && !archivePartSizeFlag.getBoolean()
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a CD without splitting enabled\nthe resulting archive file may not fit on medium."));
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (volumeSize.getLong() <= 0)
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a CD without setting medium size\nthe resulting archive file may not fit on medium."));
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (archivePartSize.getLong() > 0)
                  && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                  && ecc.getBoolean()
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a CD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 250M, medium 500M\n- part size 140M, medium 560M\n"));
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              boolean changedFlag = storageType.set("dvd");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());

              if (   changedFlag
                  && !archivePartSizeFlag.getBoolean()
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a DVD without splitting enabled\nthe resulting archive file may not fit on medium."));
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (volumeSize.getLong() <= 0)
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a DVD without setting medium size\nthe resulting archive file may not fit on medium."));
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (archivePartSize.getLong() > 0)
                  && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                  && ecc.getBoolean()
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a DVD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G"));
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              boolean changedFlag = storageType.set("bd");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());

              if (   changedFlag
                  && !archivePartSizeFlag.getBoolean()
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a BD without splitting enabled\nthe resulting archive file may not fit on medium."));
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (volumeSize.getLong() <= 0)
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a BD without setting medium size\nthe resulting archive file may not fit on medium."));
              }
              if (   changedFlag
                  && archivePartSizeFlag.getBoolean()
                  && (archivePartSize.getLong() > 0)
                  && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                  && ecc.getBoolean()
                 )
              {
                Dialogs.warning(shell,BARControl.tr("When writing to a BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 1G, medium 20G\n- part size 5G, medium 20G\n"));
              }
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              storageType.set("device");
              BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(button,storageType)
          {
            @Override
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
          @Override
          public void modified(Control control, WidgetVariable variable)
          {
            Widgets.setVisible(control,variable.equals("filesystem"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          composite = Widgets.newComposite(composite,SWT.NONE);
          composite.setLayout(new TableLayout(1.0,1.0));
          Widgets.layout(composite,0,0,TableLayoutData.WE);
          {
            subComposite = Widgets.newComposite(composite,SWT.NONE);
            subComposite.setLayout(new TableLayout(1.0,0.0));
            Widgets.layout(subComposite,0,0,TableLayoutData.WE);
            {
              label = Widgets.newLabel(subComposite,BARControl.tr("Max. storage size")+":");
              Widgets.layout(label,0,0,TableLayoutData.W);

              combo = Widgets.newCombo(subComposite);
              combo.setToolTipText(BARControl.tr("Size limit for one storage file part."));
              combo.setItems(new String[]{"32M","64M","128M","140M","256M","280M","512M","600M","1G","2G","4G","8G","64G","128G","512G","1T","2T","4T","8T"});
              combo.setData("showedErrorDialog",false);
              Widgets.layout(combo,0,1,TableLayoutData.W);
              combo.addModifyListener(new ModifyListener()
              {
                @Override
                public void modifyText(ModifyEvent modifyEvent)
                {
                  Combo widget = (Combo)modifyEvent.widget;
                  Color color  = COLOR_MODIFIED;

                  try
                  {
                    long n = Units.parseByteSize(widget.getText());
                    if (maxStorageSize.getLong() == n) color = null;
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
                @Override
                public void widgetDefaultSelected(SelectionEvent selectionEvent)
                {
                  Combo  widget = (Combo)selectionEvent.widget;
                  String string = widget.getText();
                  try
                  {
                    long n = Units.parseByteSize(string);
                    maxStorageSize.set(n);
                    BARServer.setJobOption(selectedJobData.uuid,maxStorageSize);
                    widget.setText(Units.formatByteSize(n));
                    widget.setBackground(null);
                  }
                  catch (NumberFormatException exception)
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                      widget.forceFocus();
                    }
                  }
                }
                @Override
                public void widgetSelected(SelectionEvent selectionEvent)
                {
                  Combo  widget = (Combo)selectionEvent.widget;
                  String string = widget.getText();
                  try
                  {
                    long  n = Units.parseByteSize(string);
                    maxStorageSize.set(n);
                    BARServer.setJobOption(selectedJobData.uuid,maxStorageSize);
                    widget.setText(Units.formatByteSize(n));
                    widget.setBackground(null);
                  }
                  catch (NumberFormatException exception)
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                      widget.forceFocus();
                    }
                  }
                }
              });
              combo.addFocusListener(new FocusListener()
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
                    maxStorageSize.set(n);
                    BARServer.setJobOption(selectedJobData.uuid,maxStorageSize);
                    widget.setText(Units.formatByteSize(n));
                    widget.setBackground(null);
                  }
                  catch (NumberFormatException exception)
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                      widget.forceFocus();
                    }
                  }
                }
              });
              Widgets.addModifyListener(new WidgetModifyListener(combo,maxStorageSize)
              {
                public String getString(WidgetVariable variable)
                {
                  return Units.formatByteSize(variable.getLong());
                }
              });

              label = Widgets.newLabel(subComposite,BARControl.tr("bytes"));
              Widgets.layout(label,0,2,TableLayoutData.W);
            }

            subComposite = Widgets.newComposite(composite,SWT.NONE);
            subComposite.setLayout(new TableLayout(1.0,0.0));
            Widgets.layout(subComposite,1,0,TableLayoutData.WE);
            {
              label = Widgets.newLabel(subComposite,BARControl.tr("Archive file mode:"));
              Widgets.layout(label,0,0,TableLayoutData.W);

              combo = Widgets.newOptionMenu(subComposite);
              combo.setToolTipText(BARControl.tr("If set to 'append' then append data to existing archive files.\nIf set to 'overwrite' then overwrite existing files.\nOtherwise stop with an error if archive file exists."));
              Widgets.setComboItems(combo,new Object[]{BARControl.tr("stop if exists"),"stop",
                                                       BARControl.tr("append"        ),"append",
                                                       BARControl.tr("overwrite"     ),"overwrite",
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
                  String string = Widgets.getSelectedComboItem(widget,"stop");

                  archiveFileMode.set(string);
                  BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
                }
              });
              Widgets.addModifyListener(new WidgetModifyListener(combo,archiveFileMode)
              {
                @Override
                public void modified(Widget widget, WidgetVariable variable)
                {
                  Widgets.setSelectedComboItem((Combo)widget,variable.getString());
                }
              });
            }
          }
        }

        // destination ftp
        composite = Widgets.newComposite(tab,SWT.BORDER);
        composite.setLayout(new TableLayout(0.0,1.0));
        Widgets.layout(composite,11,1,TableLayoutData.WE|TableLayoutData.N);
        Widgets.addModifyListener(new WidgetModifyListener(composite,storageType)
        {
          @Override
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageLoginName.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginName.set(widget.getText());

                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
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
                storageLoginName.set(widget.getText());

                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageHostName.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageHostName.set(widget.getText());

                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
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
                storageHostName.set(widget.getText());

                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageLoginPassword.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;
                storageLoginPassword.set(widget.getText());

                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
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
                storageLoginPassword.set(widget.getText());

                BARServer.setJobOption(selectedJobData.uuid,archiveFileMode);
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setJobOption(selectedJobData.uuid,maxBandWidth);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              @Override
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                archivePartSizeFlag.set(true);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              @Override
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(maxBandWidthFlag.getBoolean());
                widgetFTPMaxBandWidth.setEnabled(maxBandWidthFlag.getBoolean());
              }
            });

            widgetFTPMaxBandWidth = Widgets.newCombo(composite);
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
          @Override
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageLoginName.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;

                storageLoginName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

                storageLoginName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageHostName.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;

                storageHostName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

                storageHostName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

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
              @Override
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
                    BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                    widget.setBackground(null);
                  }
                  else
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,BARControl.tr("''{0}'' is out of range!\n\nEnter a number between 0 and 65535.",n));
                      widget.forceFocus();
                    }
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid port number!\n\nEnter a number between 0 and 65535.",string));
                    widget.forceFocus();
                  }
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
                Text widget = (Text)focusEvent.widget;
                widget.setData("showedErrorDialog",false);
              }
              @Override
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
                    BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                    widget.setBackground(null);
                  }
                  else
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,BARControl.tr("''{0}'' is out of range!\n\nEnter a number between 0 and 65535.",n));
                      widget.forceFocus();
                    }
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid port number!\n\nEnter a number between 0 and 65535.",string));
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (sshPublicKeyFileName.getString().equals(string)) color = null;
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
                sshPublicKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
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
                Text   widget = (Text)focusEvent.widget;
                String string = widget.getText();
                sshPublicKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPublicKeyFileName));

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

                if ((selectionEvent.stateMask & SWT.CTRL) == 0)
                {
                  fileName = Dialogs.file(shell,
                                          Dialogs.FileDialogTypes.OPEN,
                                          BARControl.tr("Select SSH public key file"),
                                          sshPublicKeyFileName.getString(),
                                          new String[]{BARControl.tr("Public key files"),"*.pub",
                                                       BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                      },
                                          "*",
                                          BARServer.remoteListDirectory
                                         );
                }
                else
                {
                  fileName = Dialogs.fileOpen(shell,
                                              BARControl.tr("Select SSH public key file"),
                                              sshPublicKeyFileName.getString(),
                                              new String[]{BARControl.tr("Public key files"),"*.pub",
                                                           BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                          }
                                             );
                }
                if (fileName != null)
                {
                  sshPublicKeyFileName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (sshPrivateKeyFileName.getString().equals(string)) color = null;
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

                sshPrivateKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
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
                Text   widget = (Text)focusEvent.widget;
                String string = widget.getText();

                sshPrivateKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPrivateKeyFileName));

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

                if ((selectionEvent.stateMask & SWT.CTRL) == 0)
                {
                  fileName = Dialogs.file(shell,
                                          Dialogs.FileDialogTypes.OPEN,
                                          BARControl.tr("Select SSH private key file"),
                                          sshPrivateKeyFileName.getString(),
                                          new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                      },
                                          "*",
                                          BARServer.remoteListDirectory
                                         );
                }
                else
                {
                  fileName = Dialogs.fileOpen(shell,
                                              BARControl.tr("Select SSH private key file"),
                                              sshPrivateKeyFileName.getString(),
                                              new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                          }
                                             );
                }
                if (fileName != null)
                {
                  sshPrivateKeyFileName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setJobOption(selectedJobData.uuid,maxBandWidth);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              @Override
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setJobOption(selectedJobData.uuid,maxBandWidth);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              @Override
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetSCPSFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            widgetSCPSFTPMaxBandWidth = Widgets.newCombo(subComposite);
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
          @Override
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageLoginName.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;

                storageLoginName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

                storageLoginName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageHostName.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;

                storageHostName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

                storageHostName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

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
              @Override
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
                    BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                    widget.setBackground(null);
                  }
                  else
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,BARControl.tr("''{0}'' is out of range!\n\nEnter a number between 0 and 65535.",n));
                      widget.forceFocus();
                    }
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid port number!\n\nEnter a number between 0 and 65535.",string));
                    widget.forceFocus();
                  }
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
                Text widget = (Text)focusEvent.widget;
                widget.setData("showedErrorDialog",false);
              }
              @Override
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
                    BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                    widget.setBackground(null);
                  }
                  else
                  {
                    if (!(Boolean)widget.getData("showedErrorDialog"))
                    {
                      widget.setData("showedErrorDialog",true);
                      Dialogs.error(shell,BARControl.tr("''{0}'' is out of range!\n\nEnter a number between 0 and 65535.",n));
                      widget.forceFocus();
                    }
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid port number!\n\nEnter a number between 0 and 65535.",string));
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (sshPublicKeyFileName.getString().equals(string)) color = null;
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

                sshPublicKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
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
                Text   widget = (Text)focusEvent.widget;
                String string = widget.getText();

                sshPublicKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPublicKeyFileName));

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

                if ((selectionEvent.stateMask & SWT.CTRL) == 0)
                {
                  fileName = Dialogs.file(shell,
                                          Dialogs.FileDialogTypes.OPEN,
                                          BARControl.tr("Select SSH public key file"),
                                          sshPublicKeyFileName.getString(),
                                          new String[]{BARControl.tr("Public key files"),"*.pub",
                                                       BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                      },
                                          "*",
                                          BARServer.remoteListDirectory
                                         );
                }
                else
                {
                  fileName = Dialogs.fileOpen(shell,
                                              BARControl.tr("Select SSH public key file"),
                                              sshPublicKeyFileName.getString(),
                                              new String[]{BARControl.tr("Public key files"),"*.pub",
                                                           BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                          }
                                             );
                }
                if (fileName != null)
                {
                  sshPublicKeyFileName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,sshPublicKeyFileName);
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (sshPrivateKeyFileName.getString().equals(string)) color = null;
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

                sshPrivateKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,sshPrivateKeyFileName);
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
                Text   widget = (Text)focusEvent.widget;
                String string = widget.getText();

                sshPrivateKeyFileName.set(string);
                BARServer.setJobOption(selectedJobData.uuid,sshPrivateKeyFileName);
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,sshPrivateKeyFileName));

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

                if ((selectionEvent.stateMask & SWT.CTRL) == 0)
                {
                  fileName = Dialogs.file(shell,
                                          Dialogs.FileDialogTypes.OPEN,
                                          BARControl.tr("Select SSH private key file"),
                                          sshPrivateKeyFileName.getString(),
                                          new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                      },
                                          "*",
                                          BARServer.remoteListDirectory
                                         );
                }
                else
                {
                  fileName = Dialogs.fileOpen(shell,
                                              BARControl.tr("Select SSH private key file"),
                                              sshPrivateKeyFileName.getString(),
                                              new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                          }
                                             );
                }
                if (fileName != null)
                {
                  sshPrivateKeyFileName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,sshPrivateKeyFileName);
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setJobOption(selectedJobData.uuid,maxBandWidth);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              @Override
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                maxBandWidthFlag.set(false);
                maxBandWidth.set(0);
                BARServer.setJobOption(selectedJobData.uuid,maxBandWidth);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,archivePartSizeFlag)
            {
              @Override
              public void modified(Control control, WidgetVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetWebdavMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            widgetWebdavMaxBandWidth = Widgets.newCombo(subComposite);
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
          @Override
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageDeviceName.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;

                storageDeviceName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

                storageDeviceName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageDeviceName));

            button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
            button.setToolTipText(BARControl.tr("Select remote device. CTRL+click to select local device."));
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

                if ((selectionEvent.stateMask & SWT.CTRL) == 0)
                {
                  fileName = Dialogs.file(shell,
                                          Dialogs.FileDialogTypes.OPEN,
                                          BARControl.tr("Select device name"),
                                          storageDeviceName.getString(),
                                          new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                      },
                                          "*",
                                          BARServer.remoteListDirectory
                                         );
                }
                else
                {
                  fileName = Dialogs.fileOpen(shell,
                                              BARControl.tr("Select device name"),
                                              storageDeviceName.getString(),
                                              new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                          }
                                             );
                }
                if (fileName != null)
                {
                  storageDeviceName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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
              @Override
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = Units.parseByteSize(string);
                  boolean changedFlag = volumeSize.set(n);
                  BARServer.setJobOption(selectedJobData.uuid,"volume-size",n);
                  widget.setBackground(null);

                  if (   changedFlag
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                      && ecc.getBoolean()
                     )
                  {
                    Dialogs.warning(shell,BARControl.tr("When writing to a CD/DVD/BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 140M, medium 560M\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G"));
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                    widget.forceFocus();
                  }
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
                  boolean changedFlag = volumeSize.set(n);
                  BARServer.setJobOption(selectedJobData.uuid,"volume-size",n);
                  widget.setBackground(null);

                  if (   changedFlag
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                      && ecc.getBoolean()
                     )
                  {
                    Dialogs.warning(shell,BARControl.tr("When writing to a CD/DVD/BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 140M, medium 560M\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G"));
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                    widget.forceFocus();
                  }
                }
              }
            });
            combo.addFocusListener(new FocusListener()
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
                  boolean changedFlag = volumeSize.set(n);
                  BARServer.setJobOption(selectedJobData.uuid,"volume-size",n);
                  widget.setBackground(null);

                  if (   changedFlag
                      && archivePartSizeFlag.getBoolean()
                      && (archivePartSize.getLong() > 0)
                      && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                      && ecc.getBoolean()
                     )
                  {
                    Dialogs.warning(shell,BARControl.tr("When writing to a CD/DVD/BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 140M, medium 560M\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G"));
                  }
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
              @Override
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button  widget      = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                boolean changedFlag = ecc.set(checkedFlag);
                BARServer.setJobOption(selectedJobData.uuid,"ecc",checkedFlag);

                if (   changedFlag
                    && archivePartSizeFlag.getBoolean()
                    && (archivePartSize.getLong() > 0)
                    && ((volumeSize.getLong()%archivePartSize.getLong()) < ((long)((double)archivePartSize.getLong()*0.1)))
                    && ecc.getBoolean()
                   )
                {
                  Dialogs.warning(shell,BARControl.tr("When writing to a CD/DVD/BD with error-correction codes enabled\nsome free space should be available on medium for error-correction codes.\n\nGood settings may be:\n- part size 140M, medium 560M\n- part size 500M, medium 3.5G\n- part size 600M, medium 3.6G"));
                }
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(button,ecc));

            button = Widgets.newCheckbox(subComposite,BARControl.tr("wait for first volume"));
            button.setToolTipText(BARControl.tr("Wait until first volume is loaded."));
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

                waitFirstVolume.set(checkedFlag);
                BARServer.setJobOption(selectedJobData.uuid,"wait-first-volume",checkedFlag);
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
          @Override
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
              @Override
              public void modifyText(ModifyEvent modifyEvent)
              {
                Text   widget = (Text)modifyEvent.widget;
                String string = widget.getText();
                Color  color  = COLOR_MODIFIED;

                if (storageDeviceName.getString().equals(string)) color = null;
                widget.setBackground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Text widget = (Text)selectionEvent.widget;

                storageDeviceName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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

                storageDeviceName.set(widget.getText());
                BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
                widget.setBackground(null);
              }
            });
            Widgets.addModifyListener(new WidgetModifyListener(text,storageDeviceName));

            button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
            button.setToolTipText(BARControl.tr("Select remote device. CTRL+click to select local device."));
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

                if ((selectionEvent.stateMask & SWT.CTRL) == 0)
                {
                  fileName = Dialogs.file(shell,
                                          Dialogs.FileDialogTypes.OPEN,
                                          BARControl.tr("Select device name"),
                                          storageDeviceName.getString(),
                                          new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                      },
                                          "*",
                                          BARServer.remoteListDirectory
                                         );
                }
                else
                {
                  fileName = Dialogs.fileOpen(shell,
                                              BARControl.tr("Select device name"),
                                              storageDeviceName.getString(),
                                              new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                          }
                                             );
                }
                if (fileName != null)
                {
                  storageDeviceName.set(fileName);
                  BARServer.setJobOption(selectedJobData.uuid,"archive-name",getArchiveName());
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
              @Override
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
              @Override
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                Combo  widget = (Combo)selectionEvent.widget;
                String string = widget.getText();
                try
                {
                  long n = Units.parseByteSize(string);
                  volumeSize.set(n);
                  BARServer.setJobOption(selectedJobData.uuid,"volume-size",n);
                  widget.setBackground(null);
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                    widget.forceFocus();
                  }
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
                  volumeSize.set(n);
                  BARServer.setJobOption(selectedJobData.uuid,"volume-size",n);
                  widget.setBackground(null);
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
                    widget.forceFocus();
                  }
                }
              }
            });
            text.addFocusListener(new FocusListener()
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
                  volumeSize.set(n);
                  BARServer.setJobOption(selectedJobData.uuid,"volume-size",n);
                  widget.setBackground(null);
                }
                catch (NumberFormatException exception)
                {
                  if (!(Boolean)widget.getData("showedErrorDialog"))
                  {
                    widget.setData("showedErrorDialog",true);
                    Dialogs.error(shell,BARControl.tr("''{0}'' is not valid size!\n\nEnter a number in the format ''n'' or ''n.m''. Optional units are KB, MB, or GB.",string));
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

        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(composite,1,0,TableLayoutData.NSWE);
        {
          styledText = Widgets.newStyledText(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
          styledText.setToolTipText(BARControl.tr("Command or script to execute before start of a job.\n\nMacros:\n\n%name - job name\n%archive - archive name\n%file - archive file name\n%directory - archive directory\n\nAdditional time macros are available."));
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

              preCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
              BARServer.setJobOption(selectedJobData.uuid,preCommand);
              widget.setBackground(null);
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

              preCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
              BARServer.setJobOption(selectedJobData.uuid,preCommand);
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(styledText,preCommand));

          button = Widgets.newButton(composite,BARControl.tr("Test")+"\u2026");
          button.setToolTipText(BARControl.tr("Test script."));
          Widgets.layout(button,1,0,TableLayoutData.E);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              testScript(preCommand.getString());
            }
          });
        }

        // post-script
        label = Widgets.newLabel(tab,BARControl.tr("Post-script")+":");
        Widgets.layout(label,2,0,TableLayoutData.W);

        composite = Widgets.newComposite(tab,SWT.NONE,4);
        composite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
        Widgets.layout(composite,3,0,TableLayoutData.NSWE);
        {
          styledText = Widgets.newStyledText(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.H_SCROLL|SWT.MULTI);
          styledText.setToolTipText(BARControl.tr("Command or script to execute after termination of a job.\n\nMacros:\n\n%name - job name\n%archive - archive name\n%file - archive file name\n%directory - archive directory\n\nAdditional time macros are available."));
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

              postCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
              BARServer.setJobOption(selectedJobData.uuid,postCommand);
              widget.setBackground(null);
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

              postCommand.set(text.replace(widget.getLineDelimiter(),"\n"));
              BARServer.setJobOption(selectedJobData.uuid,postCommand);
              widget.setBackground(null);
            }
          });
          Widgets.addModifyListener(new WidgetModifyListener(styledText,postCommand));

          button = Widgets.newButton(composite,BARControl.tr("Test")+"\u2026");
          button.setToolTipText(BARControl.tr("Test script."));
          Widgets.layout(button,1,0,TableLayoutData.E);
          button.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              testScript(postCommand.getString());
            }
          });
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Schedule"));
      tab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);
      {
        // schedule table
        widgetScheduleTable = Widgets.newTable(tab,SWT.CHECK);
        Widgets.layout(widgetScheduleTable,0,0,TableLayoutData.NSWE);
//????
// automatic column width calculation?
//widgetIncludeTable.setLayout(new TableLayout(new double[]{0.5,0.0,0.5,0.0,0.0},new double[]{0.0,1.0}));
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

              scheduleData.enabled = tableItem.getChecked();

              BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"enabled",scheduleData.enabled);
            }
          }
        });
        widgetScheduleTable.addMouseListener(new MouseListener()
        {
          @Override
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
            scheduleEdit();
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

              label = Widgets.newLabel(widgetScheduleTableToolTip,BARControl.tr("Last executed")+":");
              label.setForeground(COLOR_INFO_FORGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,0,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetScheduleTableToolTip,(scheduleData.lastExecutedDateTime > 0) ? simpleDateFormat.format(new Date(scheduleData.lastExecutedDateTime*1000L)) : "-");
              label.setForeground(COLOR_INFO_FORGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,0,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetScheduleTableToolTip,BARControl.tr("Total entities")+":");
              label.setForeground(COLOR_INFO_FORGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,1,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetScheduleTableToolTip,String.format("%d",scheduleData.totalEntities));
              label.setForeground(COLOR_INFO_FORGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,1,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetScheduleTableToolTip,BARControl.tr("Total entries")+":");
              label.setForeground(COLOR_INFO_FORGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,2,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetScheduleTableToolTip,String.format("%d",scheduleData.totalEntryCount));
              label.setForeground(COLOR_INFO_FORGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,2,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetScheduleTableToolTip,BARControl.tr("Total size")+":");
              label.setForeground(COLOR_INFO_FORGROUND);
              label.setBackground(COLOR_INFO_BACKGROUND);
              Widgets.layout(label,3,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetScheduleTableToolTip,String.format(BARControl.tr("%d bytes (%s)"),scheduleData.totalEntrySize,Units.formatByteSize(scheduleData.totalEntrySize)));
              label.setForeground(COLOR_INFO_FORGROUND);
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
        SelectionListener scheduleTableColumnSelectionListener = new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn            tableColumn = (TableColumn)selectionEvent.widget;
            ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleTable,tableColumn);
            Widgets.sortTableColumn(widgetScheduleTable,tableColumn,scheduleDataComparator);
          }
        };
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,0,BARControl.tr("Date"),        SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,1,BARControl.tr("Week days"),   SWT.LEFT,250,true );
        Widgets.sortTableColumn(widgetScheduleTable,tableColumn,new ScheduleDataComparator(widgetScheduleTable,tableColumn));
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,2,BARControl.tr("Time"),        SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,3,BARControl.tr("Archive type"),SWT.LEFT,100,true );
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleTable,4,BARControl.tr("Custom text"), SWT.LEFT, 90,true );
        tableColumn.addSelectionListener(scheduleTableColumnSelectionListener);

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
              scheduleNew();
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
              scheduleEdit();
            }
          });

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone")+"\u2026");
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleClone();
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
              MenuItem widget = (MenuItem)selectionEvent.widget;
              scheduleDelete();
            }
          });

          Widgets.addMenuSeparator(menu);

          menuItem = Widgets.addMenuItem(menu,BARControl.tr("Trigger now"));
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              scheduleTrigger();
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
              scheduleNew();
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
              scheduleEdit();
            }
          });

          button = Widgets.newButton(composite,BARControl.tr("Clone")+"\u2026");
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
              scheduleClone();
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
              scheduleDelete();
            }
          });
        }
      }

      tab = Widgets.addTab(widgetTabFolder,BARControl.tr("Comment"));
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

            comment.set(text.replace(widget.getLineDelimiter(),"\n"));
            BARServer.setJobOption(selectedJobData.uuid,comment);
            widget.setBackground(null);
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

            comment.set(text.replace(widget.getLineDelimiter(),"\n"));
            BARServer.setJobOption(selectedJobData.uuid,comment);
            widget.setBackground(null);
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

    // add root devices
    addDirectoryRootDevices();
    addDevicesList();
  }

  /** set tab status reference
   * @param tabStatus tab status object
   */
  void setTabStatus(TabStatus tabStatus)
  {
    this.tabStatus = tabStatus;
  }

  public void updateJobList(final Collection<JobData> jobData)
  {
    display.syncExec(new Runnable()
    {
      @Override
      public void run()
      {
        // get option menu items
        HashSet<JobData> removeJobDataSet = new HashSet<JobData>();
        for (JobData jobData : (JobData[])Widgets.getOptionMenuItems(widgetJobList,JobData.class))
        {
          removeJobDataSet.add(jobData);
        }

        // update job list
        synchronized(widgetJobList)
        {
          for (JobData jobData_ : jobData)
          {
            int index = Widgets.getOptionMenuIndex(widgetJobList,jobData_);
            if (index >= 0)
            {
              // update item
              Widgets.updateOptionMenuItem(widgetJobList,
                                           index,
                                           (Object)jobData_,
                                           jobData_.name
                                          );

              // keep data
              removeJobDataSet.remove(jobData_);
            }
            else
            {
              // insert new item
              Widgets.insertOptionMenuItem(widgetJobList,
                                           findJobListIndex(jobData_.name),
                                           (Object)jobData_,
                                           jobData_.name
                                          );
            }
          }
        }

        for (JobData jobData : removeJobDataSet)
        {
          Widgets.removeOptionMenuItem(widgetJobList,jobData);
        }
      }
    });
  }

  /** select job by UUID
   * @param uuid job UUID
   */
  public void selectJob(String uuid)
  {
    tabStatus.selectJob(uuid);
  }

  /** set selected job
   * @param jobData job data
   */
  public void setSelectedJob(JobData jobData)
  {
    selectedJobData = jobData;
    Widgets.setSelectedOptionMenuItem(widgetJobList,selectedJobData);
    update();
    selectJobEvent.trigger();
  }

  /** create new job
   * @return true iff new job created
   */
  public boolean jobNew()
  {
    Composite composite;
    Label     label;
    Button    button;

    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("New job"),300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

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
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
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
        Button widget  = (Button)selectionEvent.widget;
        String jobName = widgetJobName.getText();
        if (!jobName.equals(""))
        {
          try
          {
            String[] resultErrorMessage = new String[1];
            ValueMap resultMap          = new ValueMap();
            int error = BARServer.executeCommand(StringParser.format("JOB_NEW name=%S",jobName),
                                                 0,  // debugLevel
                                                 resultErrorMessage,
                                                 resultMap
                                                );
            if (error == Errors.NONE)
            {
              String newJobUUID = resultMap.getString("jobUUID");

              updateJobList();
              selectJob(newJobUUID);
            }
            else
            {
              Dialogs.error(shell,BARControl.tr("Cannot create new job:\n\n{0}",resultErrorMessage[0]));
              widgetJobName.forceFocus();
              return;
            }
          }
          catch (CommunicationError error)
          {
            Dialogs.error(shell,BARControl.tr("Cannot create new job:\n\n{0}",error.getMessage()));
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
   * @param jobData job data
   * @return true iff job cloned
   */
  public boolean jobClone(final JobData jobData)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert jobData != null;

    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Clone job"),300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

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
      widgetJobName.setText(jobData.name);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
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
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetJobName.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text  widget = (Text)modifyEvent.widget;
        widgetClone.setEnabled(!widget.getText().equals(jobData.name));
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
        Button widget     = (Button)selectionEvent.widget;
        String newJobName = widgetJobName.getText();
        if (!newJobName.equals(""))
        {
          try
          {
            String[] resultErrorMessage = new String[1];
            ValueMap resultMap          = new ValueMap();
            int error = BARServer.executeCommand(StringParser.format("JOB_CLONE jobUUID=%s name=%S",
                                                                     jobData.uuid,
                                                                     newJobName
                                                                    ),
                                                 0,  // debugLevel
                                                 resultErrorMessage,
                                                 resultMap
                                                );
            if (error != Errors.NONE)
            {
              Dialogs.error(shell,BARControl.tr("Cannot clone job ''{0}'':\n\n{1}",jobData.name,resultErrorMessage[0]));
              return;
            }

            String newJobUUID = resultMap.getString("jobUUID");
            updateJobList();
            selectJob(newJobUUID);
          }
          catch (CommunicationError error)
          {
            Dialogs.error(shell,BARControl.tr("Cannot clone job ''{0}'':\n\n{1}",jobData.name,error.getMessage()));
          }
        }
        Dialogs.close(dialog,true);
      }
    });

    Widgets.setFocus(widgetJobName);
    return (Boolean)Dialogs.run(dialog,false);
  }

  /** rename job
   * @param jobData job data
   * @return true iff new job renamed
   */
  public boolean jobRename(final JobData jobData)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert jobData != null;

    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Rename job"),300,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text   widgetNewJobName;
    final Button widgetRename;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Old name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      label = Widgets.newLabel(composite,jobData.name);
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
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
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
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetNewJobName.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text  widget = (Text)modifyEvent.widget;
        widgetRename.setEnabled(!widget.getText().equals(jobData.name));
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
        Button widget     = (Button)selectionEvent.widget;
        String newJobName = widgetNewJobName.getText();
        if (!newJobName.equals(""))
        {
          try
          {
            String[] resultErrorMessage = new String[1];
            int error = BARServer.executeCommand(StringParser.format("JOB_RENAME jobUUID=%s newName=%S",
                                                                     jobData.uuid,
                                                                     newJobName
                                                                    ),
                                                 0,  // debugLevel
                                                 resultErrorMessage
                                                );
            if (error == Errors.NONE)
            {
              updateJobList();
              selectJob(jobData.uuid);
            }
            else
            {
              Dialogs.error(shell,BARControl.tr("Cannot rename job ''{0}'':\n\n{1}",jobData.name,resultErrorMessage[0]));
            }
          }
          catch (CommunicationError error)
          {
            Dialogs.error(shell,BARControl.tr("Cannot rename job ''{0}'':\n\n{1}",jobData.name,error.getMessage()));
          }
        }
        Dialogs.close(dialog,true);
      }
    });

    Widgets.setFocus(widgetNewJobName);
    return (Boolean)Dialogs.run(dialog,false);
  }

  /** delete job
   * @param jobData job data
   * @return true iff job deleted
   */
  public boolean jobDelete(final JobData jobData)
  {
    assert jobData != null;

    if (Dialogs.confirm(shell,BARControl.tr("Delete job ''{0}''?",jobData.name)))
    {
      try
      {
        String[] result = new String[1];
        int error = BARServer.executeCommand(StringParser.format("JOB_DELETE jobUUID=%s",jobData.uuid),0);
        if (error == Errors.NONE)
        {
          updateJobList();
          clear();
          selectJobEvent.trigger();
        }
        else
        {
          Dialogs.error(shell,BARControl.tr("Cannot delete job ''{0}'':\n\n{1}",jobData.name,result[0]));
          return false;
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Cannot delete job ''{0}'':\n\n{1}",jobData.name,error.getMessage()));
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
    Widgets.removeAllTableItems(widgetIncludeTable);
    Widgets.removeAllListItems(widgetExcludeList);
    Widgets.removeAllListItems(widgetCompressExcludeList);
    Widgets.removeAllTableItems(widgetScheduleTable);
  }

  /** update selected job data
   */
  private void updateJobData()
  {
    // clear
    clearJobData();

    if (selectedJobData != null)
    {
      // get job data
      BARServer.getJobOption(selectedJobData.uuid,remoteHostName);
      BARServer.getJobOption(selectedJobData.uuid,remoteHostPort);
      BARServer.getJobOption(selectedJobData.uuid,remoteHostForceSSL);
      BARServer.getJobOption(selectedJobData.uuid,includeFileCommand);
      BARServer.getJobOption(selectedJobData.uuid,includeImageCommand);
      BARServer.getJobOption(selectedJobData.uuid,excludeCommand);
      parseArchiveName(BARServer.getStringJobOption(selectedJobData.uuid,"archive-name"));
      archiveType.set(BARServer.getStringJobOption(selectedJobData.uuid,"archive-type"));
      archivePartSize.set(Units.parseByteSize(BARServer.getStringJobOption(selectedJobData.uuid,"archive-part-size"),0));
      archivePartSizeFlag.set(archivePartSize.getLong() > 0);

      String[] compressAlgorithms = StringUtils.split(BARServer.getStringJobOption(selectedJobData.uuid,"compress-algorithm"),"+");
      deltaCompressAlgorithm.set((compressAlgorithms.length >= 1) ? compressAlgorithms[0] : "");
      byteCompressAlgorithm.set((compressAlgorithms.length >= 2) ? compressAlgorithms[1] : "");
      cryptAlgorithm.set(BARServer.getStringJobOption(selectedJobData.uuid,"crypt-algorithm"));
      cryptType.set(BARServer.getStringJobOption(selectedJobData.uuid,"crypt-type"));
      BARServer.getJobOption(selectedJobData.uuid,cryptPublicKeyFileName);
      cryptPasswordMode.set(BARServer.getStringJobOption(selectedJobData.uuid,"crypt-password-mode"));
      BARServer.getJobOption(selectedJobData.uuid,cryptPassword);
      BARServer.getJobOption(selectedJobData.uuid,incrementalListFileName);
      archiveFileMode.set(BARServer.getStringJobOption(selectedJobData.uuid,"archive-file-mode"));
      BARServer.getJobOption(selectedJobData.uuid,sshPublicKeyFileName);
      BARServer.getJobOption(selectedJobData.uuid,sshPrivateKeyFileName);
/* NYI ???
      maxBandWidth.set(Units.parseByteSize(BARServer.getStringJobOption(selectedJobData.uuid,"max-band-width")));
      maxBandWidthFlag.set(maxBandWidth.getLongOption() > 0);
*/
      volumeSize.set(Units.parseByteSize(BARServer.getStringJobOption(selectedJobData.uuid,"volume-size"),0));
      BARServer.getJobOption(selectedJobData.uuid,ecc);
      BARServer.getJobOption(selectedJobData.uuid,waitFirstVolume);
      BARServer.getJobOption(selectedJobData.uuid,skipUnreadable);
      BARServer.getJobOption(selectedJobData.uuid,rawImages);
      BARServer.getJobOption(selectedJobData.uuid,overwriteFiles);
      BARServer.getJobOption(selectedJobData.uuid,preCommand);
      BARServer.getJobOption(selectedJobData.uuid,postCommand);
      maxStorageSize.set(Units.parseByteSize(BARServer.getStringJobOption(selectedJobData.uuid,"max-storage-size"),0));
      BARServer.getJobOption(selectedJobData.uuid,comment);

      updateFileTreeImages();
      updateDeviceImages();
      updateIncludeList();
      updateExcludeList();
      updateMountList();
      updateSourceList();
      updateCompressExcludeList();
      updateScheduleTable();
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
    tabStatus.updateJobList();
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

    jobRename(selectedJobData);
  }

  /** delete selected job
   */
  private void jobDelete()
  {
    assert selectedJobData != null;

    jobDelete(selectedJobData);
  }

  //-----------------------------------------------------------------------

  /** add directory root devices
   */
  private void addDirectoryRootDevices()
  {
    TreeItem treeItem = Widgets.addTreeItem(widgetFileTree,
                                            new FileTreeData("/",FileTypes.DIRECTORY,"/",false,false),
                                            IMAGE_DIRECTORY,
                                            true,
                                            "/"//
                                           );
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
  private void openAllIncludedDirectories()
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
          FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
          if (fileTreeData.fileType == FileTypes.DIRECTORY)
          {
            // open sub-directory
            if (!treeItem.getExpanded())
            {
              Event treeEvent = new Event();
              treeEvent.item = treeItem;
              widgetFileTree.notifyListeners(SWT.Expand,treeEvent);
              treeItem.setExpanded(true);

              // get sub-directory items
              treeItems = treeItem.getItems();
            }
          }
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
  private void updateFileTree(TreeItem treeItem)
  {
    FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
    TreeItem     subTreeItem;

    {
      BARControl.waitCursor();
    }
    try
    {
      treeItem.removeAll();

      String[]            resultErrorMessage = new String[1];
      ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
      int error = BARServer.executeCommand(StringParser.format("FILE_LIST directory=%'S",
                                                               fileTreeData.name
                                                              ),
                                           0,  // debugLevel
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
                  String  name         = resultMap.getString ("name"         );
                  long    size         = resultMap.getLong   ("size"         );
                  long    dateTime     = resultMap.getLong   ("dateTime"     );
                  boolean noDumpFlag   = resultMap.getBoolean("noDump", false);

                  // create file tree data
                  fileTreeData = new FileTreeData(name,FileTypes.FILE,size,dateTime,new File(name).getName(),false,noDumpFlag);

                  // add entry
                  Image image;
                  if      (includeHashMap.containsKey(name) && !excludeHashSet.contains(name))
                    image = IMAGE_FILE_INCLUDED;
                  else if (excludeHashSet.contains(name) || noDumpFlag)
                    image = IMAGE_FILE_EXCLUDED;
                  else
                    image = IMAGE_FILE;

                  Widgets.insertTreeItem(treeItem,
                                         findFilesTreeIndex(treeItem,fileTreeData),
                                         fileTreeData,
                                         image,
                                         false,
                                         fileTreeData.title,
                                         "FILE",
                                         Units.formatByteSize(size),
                                         simpleDateFormat.format(new Date(dateTime*1000)));
                }
                break;
              case DIRECTORY:
                {
                  String  name         = resultMap.getString ("name"          );
                  long    dateTime     = resultMap.getLong   ("dateTime"      );
                  boolean noBackupFlag = resultMap.getBoolean("noBackup",false);
                  boolean noDumpFlag   = resultMap.getBoolean("noDump",  false);

                  // create file tree data
                  fileTreeData = new FileTreeData(name,FileTypes.DIRECTORY,dateTime,new File(name).getName(),noBackupFlag,noDumpFlag);

                  // add entry
                  Image   image;
                  if      (includeHashMap.containsKey(name) && !excludeHashSet.contains(name))
                    image = IMAGE_DIRECTORY_INCLUDED;
                  else if (excludeHashSet.contains(name) || noBackupFlag || noDumpFlag)
                    image = IMAGE_DIRECTORY_EXCLUDED;
                  else
                    image = IMAGE_DIRECTORY;

                  subTreeItem = Widgets.insertTreeItem(treeItem,
                                                       findFilesTreeIndex(treeItem,fileTreeData),
                                                       fileTreeData,
                                                       image,
                                                       true,
                                                       fileTreeData.title,
                                                       "DIR",
                                                       null,
                                                       simpleDateFormat.format(new Date(dateTime*1000))
                                                      );

                  // request directory info
                  directoryInfoThread.add(name,subTreeItem);
                }
                break;
              case LINK:
                {
                  String  name         = resultMap.getString ("name"    );
                  long    dateTime     = resultMap.getLong   ("dateTime");
                  boolean noDumpFlag   = resultMap.getBoolean("noDump", false);

                  // create file tree data
                  fileTreeData = new FileTreeData(name,FileTypes.LINK,dateTime,new File(name).getName(),false,noDumpFlag);

                  // add entry
                  Image image;
                  if      (includeHashMap.containsKey(name) && !excludeHashSet.contains(name))
                    image = IMAGE_LINK_INCLUDED;
                  else if (excludeHashSet.contains(name) || noDumpFlag)
                    image = IMAGE_LINK_EXCLUDED;
                  else
                    image = IMAGE_LINK;

                  Widgets.insertTreeItem(treeItem,
                                         findFilesTreeIndex(treeItem,fileTreeData),
                                         fileTreeData,
                                         image,
                                         false,
                                         fileTreeData.title,
                                         "LINK",
                                         null,
                                         simpleDateFormat.format(new Date(dateTime*1000))
                                        );
                }
                break;
              case HARDLINK:
                {
                  String  name         = resultMap.getString ("name"    );
                  long    size         = resultMap.getLong   ("size"    );
                  long    dateTime     = resultMap.getLong   ("dateTime");
                  boolean noDumpFlag   = resultMap.getBoolean("noDump", false);

                  // create file tree data
                  fileTreeData = new FileTreeData(name,FileTypes.HARDLINK,size,dateTime,new File(name).getName(),false,noDumpFlag);

                  // add entry
                  Image image;
                  if      (includeHashMap.containsKey(name) && !excludeHashSet.contains(name))
                    image = IMAGE_FILE_INCLUDED;
                  else if (excludeHashSet.contains(name) || noDumpFlag)
                    image = IMAGE_FILE_EXCLUDED;
                  else
                    image = IMAGE_FILE;

                  Widgets.insertTreeItem(treeItem,
                                         findFilesTreeIndex(treeItem,fileTreeData),
                                         fileTreeData,
                                         image,
                                         false,
                                         fileTreeData.title,
                                         "HARDLINK",
                                         Units.formatByteSize(size),
                                         simpleDateFormat.format(new Date(dateTime*1000))
                                        );
                }
                break;
              case SPECIAL:
                {
                  String  name         = resultMap.getString ("name"          );
                  long    size         = resultMap.getLong   ("size",    0L   );
                  long    dateTime     = resultMap.getLong   ("dateTime"      );
                  boolean noBackupFlag = resultMap.getBoolean("noBackup",false);
                  boolean noDumpFlag   = resultMap.getBoolean("noDump",  false);

                  SpecialTypes specialType = resultMap.getEnum("specialType",SpecialTypes.class);
                  switch (specialType)
                  {
                    case CHARACTER_DEVICE:
                      // create file tree data
                      fileTreeData = new FileTreeData(name,SpecialTypes.CHARACTER_DEVICE,dateTime,name,false,noDumpFlag);

                      // insert entry
                      Widgets.insertTreeItem(treeItem,
                                             findFilesTreeIndex(treeItem,fileTreeData),
                                             fileTreeData,
  //                                           image,
                                             false,
                                             fileTreeData.title,
                                             "CHARACTER DEVICE",
                                             simpleDateFormat.format(new Date(dateTime*1000))
                                            );
                      break;
                    case BLOCK_DEVICE:
                      // create file tree data
                      fileTreeData = new FileTreeData(name,SpecialTypes.BLOCK_DEVICE,size,dateTime,name,false,noDumpFlag);

                      // insert entry
                      Widgets.insertTreeItem(treeItem,
                                             findFilesTreeIndex(treeItem,fileTreeData),
                                             fileTreeData,
  //                                           image,
                                             false,
                                             fileTreeData.title,
                                             "BLOCK DEVICE",
                                             (size > 0) ? Units.formatByteSize(size) : null,
                                             simpleDateFormat.format(new Date(dateTime*1000))
                                            );
                      break;
                    case FIFO:
                      // create file tree data
                      fileTreeData = new FileTreeData(name,SpecialTypes.FIFO,dateTime,name,false,noDumpFlag);

                      // insert entry
                      Widgets.insertTreeItem(treeItem,
                                             findFilesTreeIndex(treeItem,fileTreeData),
                                             fileTreeData,
  //                                           image,
                                             false,
                                             fileTreeData.title,
                                             "FIFO",
                                             null,
                                             simpleDateFormat.format(new Date(dateTime*1000))
                                            );
                      break;
                    case SOCKET:
                      // create file tree data
                      fileTreeData = new FileTreeData(name,SpecialTypes.SOCKET,dateTime,name,false,noDumpFlag);

                      // insert entry
                      Widgets.insertTreeItem(treeItem,
                                             findFilesTreeIndex(treeItem,fileTreeData),
                                             fileTreeData,
  //                                           image,
                                             false,
                                             fileTreeData.title,
                                             "SOCKET",
                                             simpleDateFormat.format(new Date(dateTime*1000))
                                            );
                      break;
                    case OTHER:
                      // create file tree data
                      fileTreeData = new FileTreeData(name,SpecialTypes.OTHER,dateTime,name,false,noDumpFlag);

                      // insert entry
                      Widgets.insertTreeItem(treeItem,
                                             findFilesTreeIndex(treeItem,fileTreeData),
                                             fileTreeData,
  //                                           image,
                                             false,
                                             fileTreeData.title,
                                             "SPECIAL",
                                             simpleDateFormat.format(new Date(dateTime*1000))
                                            );
                      break;
                  }
                }
                break;
            }
          }
          catch (IllegalArgumentException exception)
          {
            if (Settings.debugLevel > 0)
            {
              System.err.println("ERROR: "+exception.getMessage());
            }
          }
        }
      }
      else
      {
  //Dprintf.dprintf("fileTreeData.name=%s fileListResult=%s errorCode=%d\n",fileTreeData.name,fileListResult,errorCode);
         Dialogs.error(shell,BARControl.tr("Cannot get file list:\n\n{0}",resultErrorMessage[0]));
      }
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
      else if (excludeHashSet.contains(fileTreeData.name) || fileTreeData.noBackup || fileTreeData.noDump )
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
                                         0,  // debugLevel
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error == Errors.NONE)
    {
      for (ValueMap resultMap : resultMapList)
      {
        try
        {
          long    size    = resultMap.getLong   ("size"   );
          boolean mounted = resultMap.getBoolean("mounted");
          String  name    = resultMap.getString ("name"   );

          // create device data
          DeviceTreeData deviceTreeData = new DeviceTreeData(name,size);

          TreeItem treeItem = Widgets.insertTreeItem(widgetDeviceTree,
                                                     findDeviceIndex(widgetDeviceTree,deviceTreeData),
                                                     deviceTreeData,
                                                     false,
                                                     name,
                                                     Units.formatByteSize(size),
                                                     IMAGE_DEVICE
                                                    );
//          treeItem.setText(0,name);
//          treeItem.setText(1,Units.formatByteSize(size));
//          treeItem.setImage(IMAGE_DEVICE);
        }
        catch (IllegalArgumentException exception)
        {
          if (Settings.debugLevel > 0)
          {
            System.err.println("ERROR: "+exception.getMessage());
          }
        }
      }
    }
    else
    {
      Dialogs.error(shell,BARControl.tr("Cannot get device list:\n\n{0}",resultErrorMessage[0]));
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
   * @param entryData entry data to insert
   * @return index in table
   */
  private int findTableIndex(Table table, EntryData entryData)
  {
    TableItem tableItems[] = table.getItems();

    int index = 0;
    while (   (index < tableItems.length)
           && (entryData.pattern.compareTo(((EntryData)tableItems[index].getData()).pattern) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of mount in sorted table
   * @param table table
   * @param mountData mount data to insert
   * @return index in table
   */
  private int findTableIndex(Table table, MountData mountData)
  {
    TableItem tableItems[] = table.getItems();

    int index = 0;
    while (   (index < tableItems.length)
           && (mountData.name.compareTo(((MountData)tableItems[index].getData()).name) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of entry in sorted table
   * @param table table
   * @param string string to insert
   * @return index in table
   */
  private int findTableIndex(Table table, String string)
  {
    TableItem tableItems[] = table.getItems();

    int index = 0;
    while (   (index < tableItems.length)
           && (string.compareTo((String)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of pattern in sorted pattern list
   * @param list list
   * @param entryData entry data to insert
   * @return index in list
   */
  private int findListIndex(List list, String string)
  {
    String strings[] = list.getItems();

    int index = 0;
    while (   (index < strings.length)
           && (string.compareTo(strings[index]) > 0)
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
    assert selectedJobData != null;

    String[]            resultErrorMessage = new String[1];
    ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("INCLUDE_LIST jobUUID=%s",
                                                             selectedJobData.uuid
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    includeHashMap.clear();
    Widgets.removeAllTableItems(widgetIncludeTable);
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

          includeHashMap.put(pattern,entryData);
          Widgets.insertTableItem(widgetIncludeTable,
                                  findTableIndex(widgetIncludeTable,entryData),
                                  (Object)entryData,
                                  entryData.getImage(),
                                  entryData.pattern
                                 );
        }
      }
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugLevel > 0)
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
    assert selectedJobData != null;

    String[]            resultErrorMessage = new String[1];
    ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("EXCLUDE_LIST jobUUID=%s",
                                                             selectedJobData.uuid
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    excludeHashSet.clear();
    Widgets.removeAllListItems(widgetExcludeList);

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
          Widgets.insertListItem(widgetExcludeList,
                                 findListIndex(widgetExcludeList,pattern),
                                 (Object)pattern,
                                 pattern
                                );
        }
      }
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugLevel > 0)
        {
          System.err.println("ERROR: "+exception.getMessage());
        }
      }
    }
  }

  /** update mount list
   */
  private void updateMountList()
  {
    assert selectedJobData != null;

    String[]            resultErrorMessage = new String[1];
    ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("MOUNT_LIST jobUUID=%s",
                                                             selectedJobData.uuid
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    Widgets.removeAllTableItems(widgetMountTable);
    for (ValueMap resultMap : resultMapList)
    {
      try
      {
        // get data
        int     id            = resultMap.getInt    ("id"           );
        String  name          = resultMap.getString ("name"         );
        boolean alwaysUnmount = resultMap.getBoolean("alwaysUnmount");

        if (!name.equals(""))
        {
          MountData mountData = new MountData(id,name,alwaysUnmount);

          Widgets.insertTableItem(widgetMountTable,
                                  findTableIndex(widgetMountTable,mountData),
                                  (Object)mountData,
                                  mountData.name,
                                  mountData.alwaysUnmount ? "\u2713" : "-"
                                 );
        }
      }
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugLevel > 0)
        {
          System.err.println("ERROR: "+exception.getMessage());
        }
      }
    }
  }

  /** update source list
   * Note: currently not a list. Show only first source pattern.
   */
  private void updateSourceList()
  {
    assert selectedJobData != null;

    String[]            resultErrorMessage  = new String[1];
    ArrayList<ValueMap> resultMapList       = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("SOURCE_LIST jobUUID=%s",
                                                             selectedJobData.uuid
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    sourceHashSet.clear();
    deltaSource.set("");

    for (ValueMap resultMap : resultMapList)
    {
      try
      {
        // get data
        PatternTypes patternType = resultMap.getEnum  ("patternType",PatternTypes.class);
        String       pattern     = resultMap.getString("pattern"                       );

        if (!pattern.equals(""))
        {
           sourceHashSet.add(pattern);
           deltaSource.set(pattern);
           break;
        }
      }
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugLevel > 0)
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
    assert selectedJobData != null;

    String[]            resultErrorMessage  = new String[1];
    ArrayList<ValueMap> resultMapList       = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST jobUUID=%s",
                                                             selectedJobData.uuid
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage,
                                         resultMapList
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    compressExcludeHashSet.clear();
    Widgets.removeAllListItems(widgetCompressExcludeList);

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
           Widgets.insertListItem(widgetCompressExcludeList,
                                  findListIndex(widgetCompressExcludeList,pattern),
                                  (Object)pattern,
                                  pattern
                                 );
        }
      }
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugLevel > 0)
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

    assert selectedJobData != null;

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

            if ((selectionEvent.stateMask & SWT.CTRL) == 0)
            {
              pathName = Dialogs.file(shell,
                                      Dialogs.FileDialogTypes.ENTRY,
                                      BARControl.tr("Select entry"),
                                      widgetPattern.getText(),
                                      new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                  },
                                      "*",
                                      BARServer.remoteListDirectory
                                     );
            }
            else
            {
              pathName = Dialogs.fileOpen(shell,
                                          BARControl.tr("Select entry"),
                                          widgetPattern.getText(),
                                          new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                      }
                                         );
            }
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
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
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
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
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
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      widgetAdd.addSelectionListener(new SelectionListener()
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

    // add selection listeners
    widgetPattern.addSelectionListener(new SelectionListener()
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

    return (Boolean)Dialogs.run(dialog,false) && !entryData.pattern.equals("");
  }

  /** add include entry
   * @param entryData entry data
   */
  private void includeListAdd(EntryData entryData)
  {
    assert selectedJobData != null;

    // update include list
    String[] resultErrorMessage = new String[1];
    int error = BARServer.executeCommand(StringParser.format("INCLUDE_LIST_ADD jobUUID=%s entryType=%s patternType=%s pattern=%'S",
                                                             selectedJobData.uuid,
                                                             entryData.entryType.toString(),
                                                             "GLOB",
                                                             entryData.pattern
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot add include entry:\n\n{0}",resultErrorMessage[0]));
      return;
    }

    // update hash map
    includeHashMap.put(entryData.pattern,entryData);

    // update table widget
    Widgets.insertTableItem(widgetIncludeTable,
                            findTableIndex(widgetIncludeTable,entryData),
                            (Object)entryData,
                            entryData.getImage(),
                            entryData.pattern
                           );

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove include patterns
   * @param patterns patterns to remove from include/exclude list
   */
  private void includeListRemove(String[] patterns)
  {
    assert selectedJobData != null;

    // remove patterns from hash map
    for (String pattern : patterns)
    {
      includeHashMap.remove(pattern);
    }

    // update include list
    String[] resultErrorMessage = new String[1];
//TODO return value?
    BARServer.executeCommand(StringParser.format("INCLUDE_LIST_CLEAR jobUUID=%s",selectedJobData.uuid),0,resultErrorMessage);
    for (EntryData entryData : includeHashMap.values())
    {
      BARServer.executeCommand(StringParser.format("INCLUDE_LIST_ADD jobUUID=%s entryType=%s patternType=%s pattern=%'S",
                                                   selectedJobData.uuid,
                                                   entryData.entryType.toString(),
                                                   "GLOB",
                                                   entryData.pattern
                                                  ),
                               0,  // debugLevel
                               resultErrorMessage
                              );
    }

    // update table widget
    Widgets.removeAllTableItems(widgetIncludeTable);
    for (EntryData entryData : includeHashMap.values())
    {
      Widgets.insertTableItem(widgetIncludeTable,
                              findTableIndex(widgetIncludeTable,entryData),
                              (Object)entryData,
                              entryData.getImage(),
                              entryData.pattern
                             );
    }

    // update file tree/device images
    updateFileTreeImages();
    updateDeviceImages();
  }

  /** remove include patterns
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
    assert selectedJobData != null;

    EntryData entryData = new EntryData(EntryTypes.FILE,"");
    if (includeEdit(entryData,"Add new include pattern","Add"))
    {
      includeListAdd(entryData);
    }
  }

  /** edit currently selected include entry
   */
  private void includeListEdit()
  {
    assert selectedJobData != null;

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

  /** clone currently selected include entry
   */
  private void includeListClone()
  {
    assert selectedJobData != null;

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

  /** remove currently selected include entry
   */
  private void includeListRemove()
  {
    assert selectedJobData != null;

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

  //-----------------------------------------------------------------------

  /** edit mount entry
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
    final Button widgetAlwaysUnmount;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,4);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Name")+":");
      Widgets.layout(label,0,0,TableLayoutData.NW);

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

            if ((selectionEvent.stateMask & SWT.CTRL) == 0)
            {
              pathName = Dialogs.file(shell,
                                      Dialogs.FileDialogTypes.DIRECTORY,
                                      BARControl.tr("Select name"),
                                      widgetName.getText(),
                                      BARServer.remoteListDirectory
                                     );
            }
            else
            {
              pathName = Dialogs.directory(shell,
                                           BARControl.tr("Select name"),
                                           widgetName.getText()
                                          );
            }
            if (pathName != null)
            {
              widgetName.setText(pathName.trim());
            }
          }
        });
      }

      widgetAlwaysUnmount = Widgets.newCheckbox(subComposite,BARControl.tr("always unmount"));
      widgetAlwaysUnmount.setSelection(mountData.alwaysUnmount);
      Widgets.layout(widgetAlwaysUnmount,1,0,TableLayoutData.W);
      widgetAlwaysUnmount.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          mountData.alwaysUnmount = widget.getSelection();
        }
      });
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE,4);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
    {
      widgetAdd = Widgets.newButton(composite,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      widgetAdd.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          mountData.name          = widgetName.getText().trim();
          mountData.alwaysUnmount = widgetAlwaysUnmount.getSelection();
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

    // add selection listeners
    widgetName.addSelectionListener(new SelectionListener()
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

    return (Boolean)Dialogs.run(dialog,false) && !mountData.name.equals("");
  }

  /** add mount entry
   * @param mountData mount data
   */
  private void mountListAdd(MountData mountData)
  {
    assert selectedJobData != null;

    // add to mount list
    String[] resultErrorMessage = new String[1];
    ValueMap resultMap          = new ValueMap();
    int error = BARServer.executeCommand(StringParser.format("MOUNT_LIST_ADD jobUUID=%s name=%'S alwaysUnmount=%y",
                                                             selectedJobData.uuid,
                                                             mountData.name,
                                                             mountData.alwaysUnmount
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage,
                                         resultMap
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot add mount entry:\n\n{0}",resultErrorMessage[0]));
      return;
    }

    // get id
    mountData.id = resultMap.getInt("id");

    // insert into table
    Widgets.insertTableItem(widgetMountTable,
                            findTableIndex(widgetMountTable,mountData),
                            (Object)mountData,
                            mountData.name,
                            mountData.alwaysUnmount ? "\u2713" : "-"
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

  /** update mount entry
   * @param entryData entry data
   */
  private void mountListUpdate(MountData mountData)
  {
    assert selectedJobData != null;

    // add to mount list
    String[] resultErrorMessage = new String[1];
    int error = BARServer.executeCommand(StringParser.format("MOUNT_LIST_UPDATE jobUUID=%s id=%d name=%'S alwaysUnmount=%y",
                                                             selectedJobData.uuid,
                                                             mountData.id,
                                                             mountData.name,
                                                             mountData.alwaysUnmount
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot update mount entry:\n\n{0}",resultErrorMessage[0]));
      return;
    }

    // update table item
    Widgets.updateTableItem(widgetMountTable,
                            (Object)mountData,
                            mountData.name,
                            mountData.alwaysUnmount ? "\u2713" : "-"
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

  /** remove mount entry
   * @param entryData entry data
   */
  private void mountListRemove(MountData mountData)
  {
    assert selectedJobData != null;

    // add to mount list
    String[] resultErrorMessage = new String[1];
    int error = BARServer.executeCommand(StringParser.format("MOUNT_LIST_REMOVE jobUUID=%s id=%d",
                                                             selectedJobData.uuid,
                                                             mountData.id
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot remove mount entry:\n\n{0}",resultErrorMessage[0]));
      return;
    }

    // remove from table
    Widgets.removeTableItem(widgetMountTable,
                            (Object)mountData
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

  /** remove mount entry by name
   * @param name mount name to remove from mount list
   */
  private void mountListRemove(String name)
  {
    mountListRemove(new String[]{name});
  }

  /** add new mount entry
   * @param name name
   */
  private void mountListAdd(String name)
  {
    assert selectedJobData != null;

    MountData mountData = new MountData(name,false);
    if (mountEdit(mountData,"Add new mount","Add"))
    {
      mountListAdd(mountData);
    }
  }

  /** add new mount entry
   */
  private void mountListAdd()
  {
    assert selectedJobData != null;

    mountListAdd("");
  }

  /** edit currently selected mount entry
   */
  private void mountListEdit()
  {
    assert selectedJobData != null;

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

  /** clone currently selected mount entry
   */
  private void mountListClone()
  {
    assert selectedJobData != null;

    TableItem[] tableItems = widgetMountTable.getSelection();
    if (tableItems.length > 0)
    {
      MountData cloneMountData = (MountData)tableItems[0].getData();

      if (mountEdit(cloneMountData,"Clone mount","Add"))
      {
        mountListAdd(cloneMountData);
      }
    }
  }

  /** remove currently selected mount entry
   */
  private void mountListRemove()
  {
    assert selectedJobData != null;

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

    assert selectedJobData != null;

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

          if ((selectionEvent.stateMask & SWT.CTRL) == 0)
          {
            pathName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.OPEN,
                                    BARControl.tr("Select entry"),
                                    widgetPattern.getText(),
                                    new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                },
                                    "*",
                                    BARServer.remoteListDirectory
                                   );
          }
          else
          {
            pathName = Dialogs.fileOpen(shell,
                                        BARControl.tr("Select entry"),
                                        widgetPattern.getText(),
                                        new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    }
                                       );
          }
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
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      widgetAdd.addSelectionListener(new SelectionListener()
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
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetPattern.addSelectionListener(new SelectionListener()
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

    return (Boolean)Dialogs.run(dialog,false) && !pattern[0].equals("");
  }

  /** add exclude pattern
   * @param pattern pattern to add to included/exclude list
   */
  private void excludeListAdd(String pattern)
  {
    assert selectedJobData != null;

    // update exclude list
    String[] resultErrorMessage = new String[1];
    int error = BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                             selectedJobData.uuid,
                                                             "GLOB",
                                                             pattern
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot add exclude entry:\n\n{0}",resultErrorMessage[0]));
      return;
    }

    // update hash map
    excludeHashSet.add(pattern);

    // update list
    Widgets.insertListItem(widgetExcludeList,
                           findListIndex(widgetExcludeList,pattern),
                           (Object)pattern,
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
    String[] resultErrorMessage = new String[1];
//TODO return value?
    BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_CLEAR jobUUID=%s",selectedJobData.uuid),0,resultErrorMessage);
    for (String pattern : excludeHashSet)
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                   selectedJobData.uuid,
                                                   "GLOB",
                                                   pattern
                                                  ),
                                0,  // debugLevel
                                resultErrorMessage
                               );
    }

    // update list widget
    Widgets.removeAllListItems(widgetExcludeList);
    for (String pattern : excludeHashSet)
    {
      Widgets.insertListItem(widgetExcludeList,
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
    assert selectedJobData != null;

    String[] pattern = new String[1];
    if (excludeEdit(pattern,"Add new exclude pattern","Add"))
    {
      excludeListAdd(pattern[0]);
    }
  }

  /** edit currently selected exclude pattern
   */
  private void excludeListEdit()
  {
    assert selectedJobData != null;

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

  /** clone currently selected exclude pattern
   */
  private void excludeListClone()
  {
    assert selectedJobData != null;

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

  /** remove currently selected exclude pattern
   */
  private void excludeListRemove()
  {
    assert selectedJobData != null;

    String[] patterns = widgetExcludeList.getSelection();
    if (patterns.length > 0)
    {
      if ((patterns.length == 1) || Dialogs.confirm(shell,BARControl.tr("Remove {0} exclude {0,choice,0#patterns|1#pattern|1<patterns}?",patterns.length)))
      {
        excludeListRemove(patterns);
      }
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
    String[] resultErrorMessage = new String[1];
    int error = BARServer.executeCommand(StringParser.format("%s jobUUID=%s attribute=%s name=%'S",
                                                             enabled ? "FILE_ATTRIBUTE_SET" : "FILE_ATTRIBUTE_CLEAR",
                                                             selectedJobData.uuid,
                                                             "NOBACKUP",
                                                             name
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot set/remove .nobackup for {0}:\n\n{1}",name,resultErrorMessage[0]));
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
    String[] resultErrorMessage = new String[1];
    int error = BARServer.executeCommand(StringParser.format("%s jobUUID=%s attribute=%s name=%'S",
                                                             enabled ? "FILE_ATTRIBUTE_SET" : "FILE_ATTRIBUTE_CLEAR",
                                                             selectedJobData.uuid,
                                                             "NODUMP",
                                                             name
                                                            ),
                                         0,  // debugLevel
                                         resultErrorMessage
                                        );
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot set/clear no-dump attribute for {0}:\n\n{1}",name,resultErrorMessage[0]));
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

    for (String pattern : patterns)
    {
      if (!sourceHashSet.contains(pattern))
      {
        String[] resultErrorMessage = new String[1];
        int error = BARServer.executeCommand(StringParser.format("SOURCE_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                                 selectedJobData.uuid,
                                                                 "GLOB",
                                                                 pattern
                                                                ),
                                             0,  // debugLevel
                                             resultErrorMessage
                                            );
        if (error != Errors.NONE)
        {
          Dialogs.error(shell,BARControl.tr("Cannot add source pattern:\n\n{0}",resultErrorMessage[0]));
          return;
        }

        sourceHashSet.add(pattern);
      }
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
    String[] resultErrorMessage = new String[1];
//TODO return value?
    BARServer.executeCommand(StringParser.format("SOURCE_LIST_CLEAR jobUUID=%s",selectedJobData.uuid),0,resultErrorMessage);
    for (String pattern : sourceHashSet)
    {
      BARServer.executeCommand(StringParser.format("SOURCE_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                   selectedJobData.uuid,
                                                   "GLOB",
                                                   pattern
                                                  ),
                               0,  // debugLevel
                               resultErrorMessage
                              );
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

//TODO return value?
    String[] resultErrorMessage = new String[1];
    BARServer.executeCommand(StringParser.format("SOURCE_LIST_CLEAR jobUUID=%s",selectedJobData.uuid),0,resultErrorMessage);
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

          if ((selectionEvent.stateMask & SWT.CTRL) == 0)
          {
            pathName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.OPEN,
                                    BARControl.tr("Select entry"),
                                    widgetPattern.getText(),
                                    new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                },
                                    "*",
                                    BARServer.remoteListDirectory
                                   );
          }
          else
          {
            pathName = Dialogs.fileOpen(shell,
                                        BARControl.tr("Select entry"),
                                        widgetPattern.getText(),
                                        new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    }
                                       );
          }
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
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetPattern.addSelectionListener(new SelectionListener()
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
    assert selectedJobData != null;

    if (!compressExcludeHashSet.contains(pattern))
    {
      String[] resultErrorMesage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                               selectedJobData.uuid,
                                                               "GLOB",
                                                               pattern
                                                              ),
                                           0,  // debugLevel
                                           resultErrorMesage
                                          );
      if (error != Errors.NONE)
      {
        Dialogs.error(shell,BARControl.tr("Cannot add compress exclude entry:\n\n{0}",resultErrorMesage[0]));
        return;
      }

      compressExcludeHashSet.add(pattern);
      Widgets.insertListItem(widgetCompressExcludeList,
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
    assert selectedJobData != null;

    for (String pattern : patterns)
    {
      if (!compressExcludeHashSet.contains(pattern))
      {
        String[] resultErrorMessage = new String[1];
        int error = BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                                 selectedJobData.uuid,
                                                                 "GLOB",
                                                                 pattern
                                                                ),
                                             0,  // debugLevel
                                             resultErrorMessage
                                            );
        if (error != Errors.NONE)
        {
          Dialogs.error(shell,BARControl.tr("Cannot add compress exclude entry:\n\n{0}",resultErrorMessage[0]));
          return;
        }

        compressExcludeHashSet.add(pattern);
        Widgets.insertListItem(widgetCompressExcludeList,
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
    assert selectedJobData != null;

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
    assert selectedJobData != null;

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

    String[] resultErrorMessage = new String[1];
//TODO return value?
    BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_CLEAR jobUUID=%s",selectedJobData.uuid),0,resultErrorMessage);
    Widgets.removeAllListItems(widgetCompressExcludeList);
    for (String pattern : compressExcludeHashSet)
    {
      BARServer.executeCommand(StringParser.format("EXCLUDE_COMPRESS_LIST_ADD jobUUID=%s patternType=%s pattern=%'S",
                                                   selectedJobData.uuid,
                                                   "GLOB",
                                                   pattern
                                                  ),
                               0,  // debugLevel
                               resultErrorMessage
                              );
      Widgets.insertListItem(widgetCompressExcludeList,
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
    assert selectedJobData != null;

    String[] patterns = widgetCompressExcludeList.getSelection();
    if (patterns.length > 0)
    {
      if (Dialogs.confirm(shell,BARControl.tr("Remove {0} selected compress exclude {0,choice,0#patterns|1#pattern|1<patterns}?",patterns.length)))
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
        addDragAndDrop(composite,"-","'-'",                                            0, 0);
        addDragAndDrop(composite,BARServer.fileSeparator,BARServer.fileSeparator,      1, 0);
        addDragAndDrop(composite,".bar","'.bar'",                                      2, 0);
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
              if ((selectionEvent.stateMask & SWT.CTRL) == 0)
              {
                fileName = Dialogs.file(shell,
                                        Dialogs.FileDialogTypes.ENTRY,
                                        BARControl.tr("Select source file"),
                                        widgetText.getText(),
                                        new String[]{BARControl.tr("BAR files"),"*.bar",
                                                     BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                    },
                                        "*",
                                        BARServer.remoteListDirectory
                                       );
              }
              else
              {
                fileName = Dialogs.fileSave(shell,
                                            BARControl.tr("Select source file"),
                                            widgetText.getText(),
                                            new String[]{BARControl.tr("BAR files"),"*.bar",
                                                         BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                        }
                                           );
              }
              if (fileName != null)
              {
                widgetText.setText(fileName);
              }
            }
          });
        }
        addDragAndDrop(composite,"Text",subComposite,widgetText,                       3, 0);

        addDragAndDrop(composite,"#","part number 1 digit",                            5, 0);
        addDragAndDrop(composite,"##","part number 2 digits",                          6, 0);
        addDragAndDrop(composite,"###","part number 3 digits",                         7, 0);
        addDragAndDrop(composite,"####","part number 4 digits",                        8, 0);

        addDragAndDrop(composite,"%type","archive type: full,incremental,differential",10,0);
        addDragAndDrop(composite,"%T","archive type short: F, I, D",                   11,0);
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

        addDragAndDrop(composite,"%Y-%m-%d","Date YYYY-MM-DD",                         3, 2);
        addDragAndDrop(composite,"%H:%M:%S","Time hh:mm:ss",                           4, 2);
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
        int z = 0;
        while (z < fileName.length())
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
            default:
              // text part
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
      int z = 0;
      while (z < string.length())
      {
        switch (string.charAt(z))
        {
          case '%':
            // add variable part
            buffer = new StringBuilder();
            buffer.append('%'); z++;
            if ((z < string.length()) && (string.charAt(z) == '%'))
            {
              buffer.append('%'); z++;
            }
            else
            {
              while ((z < string.length()) && (Character.isLetterOrDigit(string.charAt(z))))
              {
                buffer.append(string.charAt(z)); z++;
              }
            }
            parts.add(buffer.toString());
            break;
          case '#':
            // add number part
            buffer = new StringBuilder();
            while ((z < string.length()) && (string.charAt(z) == '#'))
            {
              buffer.append(string.charAt(z)); z++;
            }
            parts.add(buffer.toString());
            break;
          default:
            // add text
            buffer = new StringBuilder();
            while (   (z < string.length())
               && (string.charAt(z) != '%')
               && (string.charAt(z) != '#')
              )
            {
              buffer.append(string.charAt(z)); z++;
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
            for (int i = 1; i < parts.size(); i++)
            {
              storageNamePartList.add(index+1,new StorageNamePart(null));
              storageNamePartList.add(index+2,new StorageNamePart(parts.get(i)));
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

    assert selectedJobData != null;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Edit storage file name"),900,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

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
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add selection listeners
    widgetSave.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
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

  /** test script on server
   * @param script script to test
   */
  private void testScript(String script)
  {
    final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Test script"),500,300,BusyDialog.LIST|BusyDialog.AUTO_ANIMATE);

    String[] errorMessage = new String[1];
    ValueMap valueMap     = new ValueMap();
    Command command = BARServer.runCommand(StringParser.format("TEST_SCRIPT script=%'S",
                                                               script
                                                              ),
                                           0
                                          );
    while (   !command.endOfData()
           && command.getNextResult(errorMessage,
                                    valueMap,
                                    Command.TIMEOUT
                                   ) == Errors.NONE
          )
    {
      try
      {
        String line= valueMap.getString("line");
Dprintf.dprintf("line=%s",line);

        busyDialog.updateList(line);
      }
      catch (IllegalArgumentException exception)
      {
        if (Settings.debugLevel > 0)
        {
          System.err.println("ERROR: "+exception.getMessage());
        }
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

  /** find index for insert of schedule data in sorted schedule list
   * @param scheduleData schedule data
   * @return index in schedule table or 0
   */
  private int findScheduleItemIndex(ScheduleData scheduleData)
  {
    TableItem              tableItems[]           = widgetScheduleTable.getItems();
    ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleTable);

    int index = 0;
    while (   (index < tableItems.length)
           && (scheduleDataComparator.compare(scheduleData,(ScheduleData)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** clear schedule table
   */
  private void clearScheduleTable()
  {
    synchronized(scheduleDataMap)
    {
      scheduleDataMap.clear();
      widgetScheduleTable.removeAll();
    }
  }

  /** update schedule table
   */
  private void updateScheduleTable()
  {
    if (!widgetScheduleTable.isDisposed())
    {
      try
      {
        // get schedule list
        HashMap<String,ScheduleData> newScheduleDataMap = new HashMap<String,ScheduleData>();
        String[]            resultErrorMessage  = new String[1];
        ArrayList<ValueMap> resultMapList       = new ArrayList<ValueMap>();
        int error = BARServer.executeCommand(StringParser.format("SCHEDULE_LIST jobUUID=%s",selectedJobData.uuid),
                                             0,  // debugLevel
                                             resultErrorMessage,
                                             resultMapList
                                            );
        if (error != Errors.NONE)
        {
          return;
        }
        for (ValueMap resultMap : resultMapList)
        {
          // get data
          String  scheduleUUID         = resultMap.getString ("scheduleUUID"         );
          String  date                 = resultMap.getString ("date"                 );
          String  weekDays             = resultMap.getString ("weekDays"             );
          String  time                 = resultMap.getString ("time"                 );
          String  archiveType          = resultMap.getString ("archiveType"          );
          int     interval             = resultMap.getInt    ("interval",           0);
          String  customText           = resultMap.getString ("customText"           );
          int     minKeep              = resultMap.getInt    ("minKeep"              );
          int     maxKeep              = resultMap.getInt    ("maxKeep"              );
          int     maxAge               = resultMap.getInt    ("maxAge"               );
          boolean noStorage            = resultMap.getBoolean("noStorage"            );
          boolean enabled              = resultMap.getBoolean("enabled"              );
          long    lastExecutedDateTime = resultMap.getLong   ("lastExecutedDateTime" );
          long    totalEntities        = resultMap.getLong   ("totalEntities"        );
          long    totalEntryCount      = resultMap.getLong   ("totalEntryCount"      );
          long    totalEntrySize       = resultMap.getLong   ("totalEntrySize"       );

          ScheduleData scheduleData = scheduleDataMap.get(scheduleUUID);
          if (scheduleData != null)
          {
            scheduleData.setDate(date);
            scheduleData.setWeekDays(weekDays);
            scheduleData.setTime(time);
            scheduleData.archiveType          = archiveType;
            scheduleData.interval             = interval;
            scheduleData.customText           = customText;
            scheduleData.minKeep              = minKeep;
            scheduleData.maxKeep              = maxKeep;
            scheduleData.maxAge               = maxAge;
            scheduleData.lastExecutedDateTime = lastExecutedDateTime;
            scheduleData.totalEntities        = totalEntities;
            scheduleData.totalEntryCount      = totalEntryCount;
            scheduleData.totalEntrySize       = totalEntrySize;
            scheduleData.noStorage            = noStorage;
            scheduleData.enabled              = enabled;
          }
          else
          {
            scheduleData = new ScheduleData(scheduleUUID,
                                            date,
                                            weekDays,
                                            time,
                                            archiveType,
                                            interval,
                                            customText,
                                            minKeep,
                                            maxKeep,
                                            maxAge,
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
        scheduleDataMap = newScheduleDataMap;

        // update schedule table
        display.syncExec(new Runnable()
        {
          @Override
          public void run()
          {
            synchronized(scheduleDataMap)
            {
              // get table items
              HashSet<TableItem> removeTableItemSet = new HashSet<TableItem>();
              for (TableItem tableItem : widgetScheduleTable.getItems())
              {
                removeTableItemSet.add(tableItem);
              }

              for (ScheduleData scheduleData : scheduleDataMap.values())
              {
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
                                          scheduleData.archiveType,
                                          scheduleData.customText
                                         );
                  tableItem.setChecked(scheduleData.enabled);

                  // keep table item
                  removeTableItemSet.remove(tableItem);
                }
                else
                {
                  // insert new item
                  tableItem = Widgets.insertTableItem(widgetScheduleTable,
                                                      findScheduleItemIndex(scheduleData),
                                                      scheduleData,
                                                      scheduleData.getDate(),
                                                      scheduleData.getWeekDays(),
                                                      scheduleData.getTime(),
                                                      scheduleData.archiveType,
                                                      scheduleData.customText
                                                     );
                  tableItem.setChecked(scheduleData.enabled);
                  tableItem.setData(scheduleData);
                }
              }

              // remove not existing entries
              for (TableItem tableItem : removeTableItemSet)
              {
                Widgets.removeTableItem(widgetScheduleTable,tableItem);
              }
            }
          }
        });
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Cannot get schedule list:\n\n")+error.getMessage());
        return;
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

    assert selectedJobData != null;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,title,300,70,new double[]{1.0,0.0},1.0);

    // create widgets
    final Combo    widgetYear,widgetMonth,widgetDay;
    final Button[] widgetWeekDays = new Button[7];
    final Combo    widgetHour,widgetMinute;
    final Button   widgetTypeDefault,widgetTypeNormal,widgetTypeFull,widgetTypeIncremental,widgetTypeDifferential,widgetTypeContinuous;
    final Combo    widgetInterval;
    final Text     widgetCustomText;
    final Combo    widgetMinKeep,widgetMaxKeep;
    final Combo    widgetMaxAge;
    final Button   widgetNoStorage;
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
        widgetTypeNormal = Widgets.newRadio(subComposite,BARControl.tr("normal"));
        widgetTypeNormal.setToolTipText(BARControl.tr("Execute job as normal backup (no incremental data)."));
        Widgets.layout(widgetTypeNormal,0,0,TableLayoutData.W);
        widgetTypeNormal.setSelection(scheduleData.archiveType.equals("normal"));

        widgetTypeFull = Widgets.newRadio(subComposite,BARControl.tr("full"));
        widgetTypeFull.setToolTipText(BARControl.tr("Execute job as full backup."));
        Widgets.layout(widgetTypeFull,0,1,TableLayoutData.W);
        widgetTypeFull.setSelection(scheduleData.archiveType.equals("full"));

        widgetTypeIncremental = Widgets.newRadio(subComposite,BARControl.tr("incremental"));
        widgetTypeIncremental.setToolTipText(BARControl.tr("Execute job as incremental backup."));
        Widgets.layout(widgetTypeIncremental,0,2,TableLayoutData.W);
        widgetTypeIncremental.setSelection(scheduleData.archiveType.equals("incremental"));

        widgetTypeDifferential = Widgets.newRadio(subComposite,BARControl.tr("differential"));
        widgetTypeDifferential.setToolTipText(BARControl.tr("Execute job as differential backup."));
        Widgets.layout(widgetTypeDifferential,0,3,TableLayoutData.W);
        widgetTypeDifferential.setSelection(scheduleData.archiveType.equals("differential"));

        widgetTypeContinuous = Widgets.newRadio(subComposite,BARControl.tr("continuous"));
        widgetTypeContinuous.setToolTipText(BARControl.tr("Execute job as continuous backup."));
        Widgets.layout(widgetTypeContinuous,0,4,TableLayoutData.W);
        widgetTypeContinuous.setSelection(scheduleData.archiveType.equals("continuous"));
      }

      label = Widgets.newLabel(composite,BARControl.tr("Interval")+":");
      Widgets.layout(label,4,0,TableLayoutData.W);

      widgetInterval = Widgets.newOptionMenu(composite);
      widgetInterval.setEnabled(scheduleData.archiveType.equals("continuous"));
      widgetInterval.setToolTipText(BARControl.tr("Interval time for continuous storage."));
      Widgets.setOptionMenuItems(widgetInterval,new Object[]{"",        0,
                                                             "10min",  10,
                                                             "30min",  30,
                                                             "1h",   1*60,
                                                             "2h",   3*60,
                                                             "4h",   4*60,
                                                             "8h",   8*60
                                                            }
                                );
      Widgets.setSelectedOptionMenuItem(widgetInterval,scheduleData.interval);
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
          widgetInterval.setEnabled(widget.getSelection());
        }
      });
      Widgets.layout(widgetInterval,4,1,TableLayoutData.W);

      label = Widgets.newLabel(composite,BARControl.tr("Custom text")+":");
      Widgets.layout(label,5,0,TableLayoutData.W);

      widgetCustomText = Widgets.newText(composite);
      widgetCustomText.setToolTipText(BARControl.tr("Custom text."));
      widgetCustomText.setText(scheduleData.customText);
      Widgets.layout(widgetCustomText,5,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Keep")+":");
      Widgets.layout(label,6,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,6,1,TableLayoutData.WE);
      {
        label = Widgets.newLabel(subComposite,BARControl.tr("min.")+":");
        Widgets.layout(label,0,0,TableLayoutData.W);

        widgetMinKeep = Widgets.newOptionMenu(subComposite);
        widgetMinKeep.setToolTipText(BARControl.tr("Min. number of archives to keep."));
        Widgets.setOptionMenuItems(widgetMinKeep,new Object[]{"0",0,
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
        Widgets.setSelectedOptionMenuItem(widgetMinKeep,scheduleData.minKeep);
        Widgets.layout(widgetMinKeep,0,1,TableLayoutData.W);

        label = Widgets.newLabel(subComposite,BARControl.tr("max.")+":");
        Widgets.layout(label,0,2,TableLayoutData.W);

        widgetMaxKeep = Widgets.newOptionMenu(subComposite);
        widgetMaxKeep.setToolTipText(BARControl.tr("Max. number of archives to keep."));
        Widgets.setOptionMenuItems(widgetMaxKeep,new Object[]{"unlimited",0,
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
        Widgets.setSelectedOptionMenuItem(widgetMaxKeep,scheduleData.maxKeep);
        Widgets.layout(widgetMaxKeep,0,3,TableLayoutData.W);

        label = Widgets.newLabel(subComposite,BARControl.tr("max.")+":");
        Widgets.layout(label,0,4,TableLayoutData.W);

        widgetMaxAge = Widgets.newOptionMenu(subComposite);
        widgetMaxAge.setToolTipText(BARControl.tr("Max. age of archives to keep."));
        Widgets.setOptionMenuItems(widgetMaxAge,new Object[]{"forever",0,
                                                             "1 day",1,
                                                             "2 days",2,
                                                             "3 days",3,
                                                             "4 days",4,
                                                             "5 days",5,
                                                             "6 days",6,
                                                             "1 week",7,
                                                             "2 weeks",14,
                                                             "3 weeks",21,
                                                             "4 weeks",28,
                                                             "2 monthss",60,
                                                             "3 months",90,
                                                             "6 months",180
                                                            }
                                  );
        Widgets.setSelectedOptionMenuItem(widgetMaxAge,scheduleData.maxAge);
        Widgets.layout(widgetMaxAge,0,5,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,BARControl.tr("Options")+":");
      Widgets.layout(label,7,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,7,1,TableLayoutData.WE);
      {
        widgetNoStorage = Widgets.newCheckbox(subComposite,BARControl.tr("no storage"));
        Widgets.layout(widgetNoStorage,0,0,TableLayoutData.W);
        widgetNoStorage.setSelection(scheduleData.noStorage);

        widgetEnabled = Widgets.newCheckbox(subComposite,BARControl.tr("enabled"));
        Widgets.layout(widgetEnabled,0,1,TableLayoutData.W);
        widgetEnabled.setSelection(scheduleData.enabled);
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      widgetAdd = Widgets.newButton(composite,buttonText);
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
          Button widget = (Button)selectionEvent.widget;
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
        widgetAdd.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
*/
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
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
        else if (widgetTypeContinuous.getSelection())   scheduleData.archiveType = "continuous";
        else                                            scheduleData.archiveType = "normal";
        scheduleData.interval   = (Integer)Widgets.getSelectedOptionMenuItem(widgetInterval,0);
        scheduleData.customText = widgetCustomText.getText();
        scheduleData.minKeep    = (Integer)Widgets.getSelectedOptionMenuItem(widgetMinKeep,0);
        scheduleData.maxKeep    = (Integer)Widgets.getSelectedOptionMenuItem(widgetMaxKeep,0);
        scheduleData.maxAge     = (Integer)Widgets.getSelectedOptionMenuItem(widgetMaxAge,0);
        scheduleData.noStorage  = widgetNoStorage.getSelection();
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
    assert selectedJobData != null;

    ScheduleData scheduleData = new ScheduleData();
    if (scheduleEdit(scheduleData,BARControl.tr("New schedule"),BARControl.tr("Add")))
    {
      String[] resultErrorMessage = new String[1];
      ValueMap resultMap          = new ValueMap();
      int error = BARServer.executeCommand(StringParser.format("SCHEDULE_ADD jobUUID=%s date=%s weekDays=%s time=%s archiveType=%s interval=%d customText=%S minKeep=%d maxKeep=%d maxAge=%d noStorage=%y enabled=%y",
                                                               selectedJobData.uuid,
                                                               scheduleData.getDate(),
                                                               scheduleData.getWeekDays(),
                                                               scheduleData.getTime(),
                                                               scheduleData.archiveType,
                                                               scheduleData.interval,
                                                               scheduleData.customText,
                                                               scheduleData.minKeep,
                                                               scheduleData.maxKeep,
                                                               scheduleData.maxAge,
                                                               scheduleData.noStorage,
                                                               scheduleData.enabled
                                                              ),
                                           0,  // debugLevel
                                           resultErrorMessage,
                                           resultMap
                                          );
      if (error != Errors.NONE)
      {
        Dialogs.error(shell,BARControl.tr("Cannot create new schedule:\n\n{0}",resultErrorMessage[0]));
        return;
      }
      scheduleData.uuid = resultMap.getString("scheduleUUID");
      scheduleDataMap.put(scheduleData.uuid,scheduleData);

      TableItem tableItem = Widgets.insertTableItem(widgetScheduleTable,
                                                    findScheduleItemIndex(scheduleData),
                                                    scheduleData,
                                                    scheduleData.getDate(),
                                                    scheduleData.getWeekDays(),
                                                    scheduleData.getTime(),
                                                    scheduleData.archiveType,
                                                    scheduleData.customText
                                                   );
      tableItem.setChecked(scheduleData.enabled);
      tableItem.setData(scheduleData);
    }
  }

  /** edit schedule entry
   */
  private void scheduleEdit()
  {
    assert selectedJobData != null;

    int index = widgetScheduleTable.getSelectionIndex();
    if (index >= 0)
    {
      TableItem tableItem       = widgetScheduleTable.getItem(index);
      ScheduleData scheduleData = (ScheduleData)tableItem.getData();

      if (scheduleEdit(scheduleData,BARControl.tr("Edit schedule"),BARControl.tr("Save")))
      {
        int    error          = Errors.NONE;
        String errorMessage[] = new String[1];

        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"date",scheduleData.getDate(),          errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"weekdays",scheduleData.getWeekDays(),  errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"time",scheduleData.getTime(),          errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"archive-type",scheduleData.archiveType,errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"interval",scheduleData.interval,errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"text",scheduleData.customText,         errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"min-keep",scheduleData.minKeep,        errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"max-keep",scheduleData.maxKeep,        errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"max-age",scheduleData.maxAge,          errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"no-storage",scheduleData.noStorage,    errorMessage);
        }
        if (error == Errors.NONE)
        {
          error = BARServer.setScheduleOption(selectedJobData.uuid,scheduleData.uuid,"enabled",scheduleData.enabled,         errorMessage);
        }
        if (error != Errors.NONE)
        {
          Dialogs.error(shell,BARControl.tr("Cannot set schedule option for job ''{0}'':\n\n{1}",selectedJobData.name,errorMessage[0]));
          return;
        }

        Widgets.updateTableItem(tableItem,
                                scheduleData,
                                scheduleData.getDate(),
                                scheduleData.getWeekDays(),
                                scheduleData.getTime(),
                                scheduleData.archiveType,
                                scheduleData.customText
                               );
        tableItem.setChecked(scheduleData.enabled);
      }
    }
  }

  /** clone a schedule entry
   */
  private void scheduleClone()
  {
    assert selectedJobData != null;

    int index = widgetScheduleTable.getSelectionIndex();
    if (index >= 0)
    {
      TableItem          tableItem    = widgetScheduleTable.getItem(index);
      final ScheduleData scheduleData = (ScheduleData)tableItem.getData();

      ScheduleData newScheduleData = scheduleData.clone();
      if (scheduleEdit(newScheduleData,BARControl.tr("Clone schedule"),BARControl.tr("Add")))
      {
        String[] resultErrorMessage = new String[1];
        ValueMap resultMap          = new ValueMap();
        int error = BARServer.executeCommand(StringParser.format("SCHEDULE_ADD jobUUID=%s date=%s weekDays=%s time=%s archiveType=%s customText=%S minKeep=%d maxKeep=%d maxAge=%d noStorage=%y enabled=%y",
                                                                 selectedJobData.uuid,
                                                                 newScheduleData.getDate(),
                                                                 newScheduleData.getWeekDays(),
                                                                 newScheduleData.getTime(),
                                                                 newScheduleData.archiveType,
                                                                 newScheduleData.customText,
                                                                 newScheduleData.minKeep,
                                                                 newScheduleData.maxKeep,
                                                                 newScheduleData.maxAge,
                                                                 newScheduleData.noStorage,
                                                                 newScheduleData.enabled
                                                                ),
                                             0,  // debugLevel
                                             resultErrorMessage,
                                             resultMap
                                            );
        if (error != Errors.NONE)
        {
          Dialogs.error(shell,BARControl.tr("Cannot clone new schedule:\n\n{0}",resultErrorMessage[0]));
          return;
        }
        scheduleData.uuid = resultMap.getString("scheduleUUID");
        scheduleDataMap.put(scheduleData.uuid,newScheduleData);

        TableItem newTableItem = Widgets.insertTableItem(widgetScheduleTable,
                                                         findScheduleItemIndex(newScheduleData),
                                                         newScheduleData,
                                                         newScheduleData.getDate(),
                                                         newScheduleData.getWeekDays(),
                                                         newScheduleData.getTime(),
                                                         newScheduleData.archiveType,
                                                         newScheduleData.customText
                                                        );
        tableItem.setChecked(scheduleData.enabled);
        tableItem.setData(newScheduleData);
      }
    }
  }

  /** delete schedule entry
   */
  private void scheduleDelete()
  {
    assert selectedJobData != null;

    TableItem[] tableItems = widgetScheduleTable.getSelection();
    if (tableItems.length > 0)
    {
      if (Dialogs.confirm(shell,BARControl.tr("Delete {0} selected schedule {0,choice,0#entries|1#entry|1<entries}?",tableItems.length)))
      {
        for (TableItem tableItem : tableItems)
        {
          ScheduleData scheduleData = (ScheduleData)tableItem.getData();

          String[] resultErrorMessage = new String[1];
          ValueMap resultMap          = new ValueMap();
          int error = BARServer.executeCommand(StringParser.format("SCHEDULE_REMOVE jobUUID=%s scheduleUUID=%s",selectedJobData.uuid,scheduleData.uuid),0,resultErrorMessage);
          if (error != Errors.NONE)
          {
            Dialogs.error(shell,BARControl.tr("Cannot delete schedule:\n\n{0}",resultErrorMessage[0]));
            return;
          }

          scheduleDataMap.remove(scheduleData.uuid);
          tableItem.dispose();
        }
      }
    }
  }

  /** trigger schedule entry
   */
  private void scheduleTrigger()
  {
    assert selectedJobData != null;

    int index = widgetScheduleTable.getSelectionIndex();
    if (index >= 0)
    {
      TableItem    tableItem    = widgetScheduleTable.getItem(index);
      ScheduleData scheduleData = (ScheduleData)tableItem.getData();

      String[] resultErrorMessage = new String[1];
      ValueMap resultMap          = new ValueMap();
      int error = BARServer.executeCommand(StringParser.format("SCHEDULE_TRIGGER jobUUID=%s scheduleUUID=%s",selectedJobData.uuid,scheduleData.uuid),0,resultErrorMessage);
      if (error != Errors.NONE)
      {
        Dialogs.error(shell,BARControl.tr("Cannot trigger schedule of job ''{0}'':\n\n{1}",selectedJobData.name,resultErrorMessage[0]));
        return;
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
    clearScheduleTable();
  }

  /** update all data
   */
  private void update()
  {
    updateJobData();
    updateScheduleTable();
  }
}

/* end of file */
