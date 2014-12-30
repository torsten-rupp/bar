/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TabRestore.java,v $
* $Revision: 1.17 $
* $Author: torsten $
* Contents: restore tab
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
// base
import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Comparator;
import java.util.Date;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;

// graphics
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Cursor;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
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

/** background task
 */
abstract class BackgroundTask
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private final BusyDialog busyDialog;
  private Thread           thread;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create background task
   * @param busyDialog busy dialog
   * @param userData user data
   */
  BackgroundTask(final BusyDialog busyDialog, final Object userData)
  {
    final BackgroundTask backgroundTask = this;

    this.busyDialog = busyDialog;

    thread = new Thread(new Runnable()
    {
      public void run()
      {
        backgroundTask.run(busyDialog,userData);
      }
    });
    thread.setDaemon(true);
    thread.start();
  }

  /** create background task
   * @param busyDialog busy dialog
   */
  BackgroundTask(final BusyDialog busyDialog)
  {
    this(busyDialog,null);
  }

  /** run method
   * @param busyDialog busy dialog
   * @param userData user data
   */
  abstract public void run(BusyDialog busyDialog, Object userData);
}

/** tab restore
 */
class TabRestore
{
  /** index states
   */
  enum IndexStates
  {
    NONE,

    OK,
    CREATE,
    UPDATE_REQUESTED,
    UPDATE,
    ERROR,

    UNKNOWN;

    public static EnumSet<IndexStates> ALL()
    {
      return EnumSet.of(IndexStates.OK,IndexStates.ERROR);
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case OK:               return "ok";
        case CREATE:           return "creating";
        case UPDATE_REQUESTED: return "update requested";
        case UPDATE:           return "update";
        case ERROR:            return "error";
        default:               return "ok";
      }
    }
  };

  /** index state set
   */
  class IndexStateSet
  {
    private EnumSet<IndexStates> indexStateSet;

    /** create index state set
     * @param indexStates index states
     */
    public IndexStateSet(IndexStates... indexStates)
    {
      this.indexStateSet = EnumSet.noneOf(IndexStates.class);
      for (IndexStates indexState : indexStates)
      {
        this.indexStateSet.add(indexState);
      }
    }

    /** convert data to string
     * @param indexState index state
     */
    public void add(IndexStates indexState)
    {
      indexStateSet.add(indexState);
    }

    /** convert data to string
     * @param indexState index state
     * @return true if index state is in set
     */
    public boolean contains(IndexStates indexState)
    {
      return indexStateSet.contains(indexState);
    }

    /** convert data to name list
     * @return name list
     */
    public String nameList(String separator)
    {
      StringBuilder buffer = new StringBuilder();

      Iterator iterator = indexStateSet.iterator();
      while (iterator.hasNext())
      {
        Enum value = (Enum)iterator.next();
        if (buffer.length() > 0) buffer.append(separator);
        buffer.append(value.name());
      }

      return buffer.toString();
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return StringUtils.join(indexStateSet,",");
    }
  }

  final IndexStateSet INDEX_STATE_SET_ALL = new IndexStateSet(IndexStates.OK,
                                                              IndexStates.CREATE,
                                                              IndexStates.UPDATE_REQUESTED,
                                                              IndexStates.UPDATE,
                                                              IndexStates.ERROR
                                                             );

  /** index modes
   */
  enum IndexModes
  {
    NONE,

    MANUAL,
    AUTO,

    ALL,
    UNKNOWN;

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case MANUAL: return "manual";
        case AUTO:   return "auto";
        default:     return "manual";
      }
    }
  };

  /** index data
   */
  class IndexData
  {
    abstract class TreeItemUpdateRunnable
    {
      public abstract void update(TreeItem treeItem, IndexData indexData);
    }
    abstract class TableItemUpdateRunnable
    {
      public abstract void update(TableItem tableItem, IndexData indexData);
    }

    public String                   name;                     // name
    public long                     dateTime;                 // date/time when storage was created or last time some storage was created
    public long                     size;                     // storage size or total size [bytes]
    public String                   title;                    // title to show
    public IndexStates              indexState;               // state of index
    public String                   errorMessage;             // last error message

    private TreeItem                treeItem;                 // reference tree item or null
    private TreeItemUpdateRunnable  treeItemUpdateRunnable;
    private TableItem               tableItem;                // reference table item or null
    private TableItemUpdateRunnable tableItemUpdateRunnable;
    private boolean                 checked;                  // true iff storage entry is tagged

    /** create index data
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param size size of storage [byte]
     * @param title title to show
     * @param errorMessage error message text
     */
    IndexData(String name, long dateTime, long size, String title, String errorMessage)
    {
      this.name          = name;
      this.dateTime      = dateTime;
      this.size          = size;
      this.title         = title;
      this.indexState    = IndexStates.NONE;
      this.errorMessage  = errorMessage;
      this.treeItem      = null;
      this.tableItem     = null;
      this.checked       = false;
    }

    /** create index data
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param title title to show
     * @param lastCheckedDateTime last checked date/time (timestamp)
     */
    IndexData(String name, long dateTime, String title)
    {
      this(name,dateTime,0L,title,null);
    }

    /** create index data
     * @param name name of storage
     * @param uuid uuid
     * @param title title to show
     */
    IndexData(String name, String title)
    {
      this(name,0L,title);
    }

    /** set tree item reference
     * @param treeItem tree item
     * @param treeItemUpdateRunnable tree item update runnable
     */
    public void setTreeItem(TreeItem treeItem, TreeItemUpdateRunnable treeItemUpdateRunnable)
    {
      this.treeItem               = treeItem;
      this.treeItemUpdateRunnable = treeItemUpdateRunnable;
    }

    /** clear tree item reference
     */
    public void clearTreeItem()
    {
      this.treeItem = null;
    }

    /** set table item reference
     * @param tableItem table item
     * @param tableItemUpdateRunnable table item update runnable
     */
    public void setTableItem(TableItem tableItem, TableItemUpdateRunnable tableItemUpdateRunnable)
    {
      this.tableItem               = tableItem;
      this.tableItemUpdateRunnable = tableItemUpdateRunnable;
    }

    /** clear table item reference
     */
    public void clearTableItem()
    {
      this.tableItem = null;
    }

    /** set index state
     * @param indexState index state
     */
    public void setState(IndexStates indexState)
    {
      this.indexState = indexState;
      update();
    }

    /** update tree/table item
     */
    public void update()
    {
      if (treeItem != null)
      {
        treeItemUpdateRunnable.update(treeItem,this);
      }
      if (tableItem != null)
      {
        tableItemUpdateRunnable.update(tableItem,this);
      }
    }

    /** check if checked
     * @return true if entry is checked, false otherwise
     */
    public boolean isChecked()
    {
      return checked;
    }

    /** set checked state
     * @param checked checked state
     */
    public void setChecked(boolean checked)
    {
      this.checked = checked;
      if (treeItem != null) treeItem.setChecked(checked);
      if (tableItem != null) tableItem.setChecked(checked);
    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return title;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Index {"+name+", created="+dateTime+", size="+size+" bytes, checked="+checked+"}";
    }
  };

  /** index data comparator
   */
  class IndexDataComparator implements Comparator<IndexData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_NAME             = 0;
    private final static int SORTMODE_SIZE             = 1;
    private final static int SORTMODE_CREATED_DATETIME = 2;
    private final static int SORTMODE_STATE            = 3;

    private int sortMode;

    /** create storage data comparator
     * @param tree storage tree
     * @param sortColumn sort column
     */
    IndexDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else if (tree.getColumn(3) == sortColumn) sortMode = SORTMODE_STATE;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param table storage table
     * @param sortColumn sort column
     */
    IndexDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_STATE;
      else                                       sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param tree storage tree
     */
    IndexDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
    }

    /** create storage data comparator
     * @param tree storage tree
     */
    IndexDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare index data
     * @param indexData1, indexData2 index data to compare
     * @return -1 iff indexData1 < indexData2,
                0 iff indexData1 = indexData2,
                1 iff indexData1 > indexData2
     */
    public int compare(IndexData indexData1, IndexData indexData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return indexData1.title.compareTo(indexData2.title);
        case SORTMODE_SIZE:
          if      (indexData1.size < indexData2.size) return -1;
          else if (indexData1.size > indexData2.size) return  1;
          else                                        return  0;
        case SORTMODE_CREATED_DATETIME:
          if      (indexData1.dateTime < indexData2.dateTime) return -1;
          else if (indexData1.dateTime > indexData2.dateTime) return  1;
          else                                                return  0;
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "IndexDataComparator {"+sortMode+"}";
    }
  }

  /** UUID index data
   */
  class UUIDIndexData extends IndexData
  {
    public String uuid;                       // UUID

    /** create UUID data index
     * @param uuid uuid
     * @param name job name
     * @param lastDateTime last date/time (timestamp) when storage was created
     * @param totalSize total size of storage [byte]
     * @param lastErrorMessage last error message text
     */
    UUIDIndexData(String uuid, String name, long lastDateTime, long totalSize, String lastErrorMessage)
    {
      super(name,lastDateTime,totalSize,name,lastErrorMessage);
      this.uuid = uuid;
    }

    /** set tree item reference
     * @param treeItem tree item
     */
    public void setTreeItem(TreeItem treeItem)
    {
      final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
      {
        public void update(TreeItem treeItem, IndexData indexData)
        {
          UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

          Widgets.updateTreeItem(treeItem,
                                 (Object)uuidIndexData,
                                 uuidIndexData.name,
                                 Units.formatByteSize(uuidIndexData.size),
                                 simpleDateFormat.format(new Date(uuidIndexData.dateTime*1000)),
                                 ""
                                );
        }
      };

      setTreeItem(treeItem,treeItemUpdateRunnable);
    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return uuid;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "UUIDIndexData {"+uuid+", created="+dateTime+", size="+size+" bytes, checked="+isChecked()+"}";
    }
  }

  /** UUID data comparator
   */
  class UUIDIndexDataComparator implements Comparator<UUIDIndexData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_NAME             = 0;
    private final static int SORTMODE_SIZE             = 1;
    private final static int SORTMODE_CREATED_DATETIME = 2;

    private int sortMode;

    /** create storage data comparator
     * @param tree storage tree
     * @param sortColumn sort column
     */
    UUIDIndexDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param table storage table
     * @param sortColumn sort column
     */
    UUIDIndexDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else                                       sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param tree storage tree
     */
    UUIDIndexDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
    }

    /** create storage data comparator
     * @param tree storage tree
     */
    UUIDIndexDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare uuid index data
     * @param uuidIndexData1, uuidIndexData2 storage data to compare
     * @return -1 iff uuidIndexData1 < uuidIndexData2,
                0 iff uuidIndexData1 = uuidIndexData2,
                1 iff uuidIndexData1 > uuidIndexData2
     */
    public int compare(UUIDIndexData uuidIndexData1, UUIDIndexData uuidIndexData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return uuidIndexData1.title.compareTo(uuidIndexData2.title);
        case SORTMODE_SIZE:
          if      (uuidIndexData1.size < uuidIndexData2.size) return -1;
          else if (uuidIndexData1.size > uuidIndexData2.size) return  1;
          else                                                return  0;
        case SORTMODE_CREATED_DATETIME:
          if      (uuidIndexData1.dateTime < uuidIndexData2.dateTime) return -1;
          else if (uuidIndexData1.dateTime > uuidIndexData2.dateTime) return  1;
          else                                                        return  0;
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "UUIDIndexDataComparator {"+sortMode+"}";
    }
  }

  /** UUID data map
   */
  class UUIDIndexDataMap extends HashMap<String,UUIDIndexData>
  {
    /** remove not checked entries
     */
    public void clearNotChecked()
    {
      String[] keys = keySet().toArray(new String[0]);
      for (String key : keys)
      {
        UUIDIndexData uuidIndexData = get(key);
        if (!uuidIndexData.isChecked()) remove(key);
      }
    }

    /** get job data from map
     * @param uuid UUID
     * @return UUID data
     */
    public UUIDIndexData get(String uuid)
    {
      return super.get(uuid);
    }

    /** get UUID data from map by job name
     * @param name job name
     * @return UUID data
     */
    public UUIDIndexData getByJobName(String name)
    {
      for (UUIDIndexData uuidIndexData : values())
      {
        if (uuidIndexData.name.equals(name)) return uuidIndexData;
      }

      return null;
    }

    /** put UUID data into map
     * @param uuidIndexData UUID data
     */
    public void put(UUIDIndexData uuidIndexData)
    {
      put(uuidIndexData.uuid,uuidIndexData);
    }

    /** remove UUID data from map
     * @param uuidIndexData UUID data
     */
    public void remove(UUIDIndexData uuidIndexData)
    {
      remove(uuidIndexData.uuid);
    }
  }

  /** index modes
   */
  enum ArchiveTypes
  {
    NONE,

    NORMAL,
    FULL,
    INCREMENTAL,
    DIFFERENTAL,

    UNKNOWN;

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case NORMAL:      return "normal";
        case FULL:        return "full";
        case INCREMENTAL: return "inremental";
        case DIFFERENTAL: return "differental";
        default:          return "normal";
      }
    }
  };

  /** job index data
   */
  class JobIndexData extends IndexData
  {
    long         jobId;
    ArchiveTypes archiveType;

    /** create job data index
     * @param jobId job id
     * @param name name of storage
     * @param lastDateTime last date/time (timestamp) when storage was created
     * @param totalSize total size of storage [byte]
     * @param lastErrorMessage last error message text
     */
    JobIndexData(long jobId, ArchiveTypes archiveType, long lastDateTime, long totalSize, String lastErrorMessage)
    {
      super("",lastDateTime,totalSize,"tttt",lastErrorMessage);
      this.jobId       = jobId;
      this.archiveType = archiveType;
    }

    /** set tree item reference
     * @param treeItem tree item
     */
    public void setTreeItem(TreeItem treeItem)
    {
      final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
      {
        public void update(TreeItem treeItem, IndexData indexData)
        {
          JobIndexData jobIndexData = (JobIndexData)indexData;

          Widgets.updateTreeItem(treeItem,
                                 (Object)jobIndexData,
                                 jobIndexData.archiveType.toString(),
                                 Units.formatByteSize(jobIndexData.size),
                                 simpleDateFormat.format(new Date(jobIndexData.dateTime*1000)),
                                 ""
                                );
        }
      };

      setTreeItem(treeItem,treeItemUpdateRunnable);
    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return String.format("%d: %s",jobId,name);
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "JobIndexData {"+jobId+", type="+archiveType.toString()+", created="+dateTime+", size="+size+" bytes, checked="+isChecked()+"}";
    }
  }

  /** job data comparator
   */
  class JobIndexDataComparator implements Comparator<JobIndexData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_NAME             = 0;
    private final static int SORTMODE_SIZE             = 1;
    private final static int SORTMODE_CREATED_DATETIME = 2;

    private int sortMode;

    /** create storage data comparator
     * @param tree storage tree
     * @param sortColumn sort column
     */
    JobIndexDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param table storage table
     * @param sortColumn sort column
     */
    JobIndexDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else                                       sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param tree storage tree
     */
    JobIndexDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
    }

    /** create storage data comparator
     * @param tree storage tree
     */
    JobIndexDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare job index data
     * @param jobIndexData1, jobIndexData2 job data to compare
     * @return -1 iff jobIndexData1 < jobIndexData2,
                0 iff jobIndexData1 = jobIndexData2,
                1 iff jobIndexData1 > jobIndexData2
     */
    public int compare(JobIndexData jobIndexData1, JobIndexData jobIndexData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return jobIndexData1.title.compareTo(jobIndexData2.title);
        case SORTMODE_SIZE:
          if      (jobIndexData1.size < jobIndexData2.size) return -1;
          else if (jobIndexData1.size > jobIndexData2.size) return  1;
          else                                              return  0;
        case SORTMODE_CREATED_DATETIME:
          if      (jobIndexData1.dateTime < jobIndexData2.dateTime) return -1;
          else if (jobIndexData1.dateTime > jobIndexData2.dateTime) return  1;
          else                                                      return  0;
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "JobIndexDataComparator {"+sortMode+"}";
    }
  }

  /** job data map
   */
  class JobIndexDataMap extends HashMap<Long,JobIndexData>
  {
    /** remove not checked entries
     */
    public void clearNotChecked()
    {
      String[] keys = keySet().toArray(new String[0]);
      for (String key : keys)
      {
        JobIndexData jobIndexData = get(key);
        if (!jobIndexData.isChecked()) remove(key);
      }
    }

    /** get job data from map
     * @param jobId database id
     * @return job data
     */
    public JobIndexData get(long jobId)
    {
      return super.get(jobId);
    }

    /** get job data from map by job name
     * @param jobName job name
     * @return job data
     */
    public JobIndexData getByName(String jobName)
    {
      for (JobIndexData jobIndexData : values())
      {
        if (jobIndexData.name.equals(jobName)) return jobIndexData;
      }

      return null;
    }

    /** put job data into map
     * @param jobIndexData job data
     */
    public void put(JobIndexData jobIndexData)
    {
      put(jobIndexData.jobId,jobIndexData);
    }

    /** remove job data from map
     * @param jobIndexData job data
     */
    public void remove(JobIndexData jobIndexData)
    {
      remove(jobIndexData.jobId);
    }
  }

  /** storage index data
   */
  class StorageIndexData extends IndexData
  {
    public long        storageId;                // database storage id
    public long        jobId;                    // database job id
    public IndexModes  indexMode;                // mode of index
    public long        lastCheckedDateTime;      // last checked date/time

    /** create storage data index
     * @param storageId database storage id
     * @param jobId database job id
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param size size of storage [byte]
     * @param title title to show
     * @param indexState storage index state
     * @param indexMode storage index mode
     * @param lastCheckedDateTime last checked date/time (timestamp)
     * @param errorMessage error message text
     */
    StorageIndexData(long storageId, long jobId, String name, long dateTime, long size, String title, IndexStates indexState, IndexModes indexMode, long lastCheckedDateTime, String errorMessage)
    {
      super(name,dateTime,size,title,errorMessage);
      this.storageId           = storageId;
      this.jobId               = jobId;
      this.indexState          = indexState;
      this.indexMode           = indexMode;
      this.lastCheckedDateTime = lastCheckedDateTime;
    }

    /** create storage data
     * @param id database id
     * @param jobId database job id
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param title title to show
     * @param lastCheckedDateTime last checked date/time (timestamp)
     */
    StorageIndexData(long id, long jobId, String name, long dateTime, String title, long lastCheckedDateTime)
    {
      this(id,jobId,name,dateTime,0L,title,IndexStates.OK,IndexModes.MANUAL,lastCheckedDateTime,null);
    }

    /** create storage data
     * @param id database id
     * @param jobId database job id
     * @param name name of storage
     * @param uuid uuid
     * @param title title to show
     */
    StorageIndexData(long id, long jobId, String name, String title)
    {
      this(id,jobId,name,0L,title,0L);
    }

    /** set tree item reference
     * @param treeItem tree item
     */
    public void setTreeItem(TreeItem treeItem)
    {
      final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
      {
        public void update(TreeItem treeItem, IndexData indexData)
        {
          StorageIndexData storageIndexData = (StorageIndexData)indexData;

          Widgets.updateTreeItem(treeItem,
                                 (Object)storageIndexData,
                                 storageIndexData.name,
                                 Units.formatByteSize(storageIndexData.size),
                                 simpleDateFormat.format(new Date(storageIndexData.dateTime*1000)),
                                 storageIndexData.indexState.toString()
                                );
        }
      };

      setTreeItem(treeItem,treeItemUpdateRunnable);
    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return String.format("%d: %s",storageId,title);
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "StorageIndexData {"+jobId+", name="+name+", created="+dateTime+", size="+size+" bytes, state="+indexState+", last checked="+lastCheckedDateTime+", checked="+isChecked()+"}";
    }
  };

  /** storage index data comparator
   */
  class StorageIndexDataComparator implements Comparator<StorageIndexData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_NAME             = 0;
    private final static int SORTMODE_SIZE             = 1;
    private final static int SORTMODE_CREATED_DATETIME = 2;
    private final static int SORTMODE_STATE            = 3;

    private int sortMode;

    /** create storage data comparator
     * @param tree storage tree
     * @param sortColumn sort column
     */
    StorageIndexDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else if (tree.getColumn(3) == sortColumn) sortMode = SORTMODE_STATE;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param table storage table
     * @param sortColumn sort column
     */
    StorageIndexDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_STATE;
      else                                       sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param tree storage tree
     */
    StorageIndexDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
    }

    /** create storage data comparator
     * @param tree storage tree
     */
    StorageIndexDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare storage index data
     * @param storageIndexData1, storageIndexData2 storage data to compare
     * @return -1 iff storageIndexData1 < storageIndexData2,
                0 iff storageIndexData1 = storageIndexData2,
                1 iff storageIndexData1 > storageIndexData2
     */
    public int compare(StorageIndexData storageIndexData1, StorageIndexData storageIndexData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return storageIndexData1.title.compareTo(storageIndexData2.title);
        case SORTMODE_SIZE:
          if      (storageIndexData1.size < storageIndexData2.size) return -1;
          else if (storageIndexData1.size > storageIndexData2.size) return  1;
          else                                                      return  0;
        case SORTMODE_CREATED_DATETIME:
          if      (storageIndexData1.dateTime < storageIndexData2.dateTime) return -1;
          else if (storageIndexData1.dateTime > storageIndexData2.dateTime) return  1;
          else                                                              return  0;
        case SORTMODE_STATE:
          return storageIndexData1.indexState.compareTo(storageIndexData2.indexState);
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "StorageIndexDataComparator {"+sortMode+"}";
    }
  }

  /** storage data map
   */
  class StorageIndexDataMap extends HashMap<Long,StorageIndexData>
  {
    /** remove not checked entries
     */
    public void clearNotChecked()
    {
      String[] keys = keySet().toArray(new String[0]);
      for (String key : keys)
      {
        StorageIndexData storageIndexData = get(key);
        if (!storageIndexData.isChecked()) remove(key);
      }
    }

    /** get storage data from map
     * @param storageId database id
     * @return storage data
     */
    public StorageIndexData get(long storageId)
    {
      return super.get(storageId);
    }

    /** get storage data from map
     * @param storageName storage name
     * @return storage data
     */
    public StorageIndexData get(String storageName)
    {
      for (StorageIndexData storageIndexData : values())
      {
        if (storageIndexData.name.equals(storageName)) return storageIndexData;
      }

      return null;
    }

    /** put storage data into map
     * @param storageIndexData storage data
     */
    public void put(StorageIndexData storageIndexData)
    {
      put(storageIndexData.storageId,storageIndexData);
    }

    /** remove storage data from map
     * @param storageIndexData storage data
     */
    public void remove(StorageIndexData storageIndexData)
    {
      remove(storageIndexData.storageId);
    }
  }

  /** index data map
   */
  class IndexDataMap
  {
    private HashMap<String,UUIDIndexData>  uuidIndexDataMap;
    private HashMap<Long,JobIndexData>     jobIndexDataMap;
    private HashMap<Long,StorageIndexData> storageIndexDataMap;

    /** constructor
     */
    public IndexDataMap()
    {
      this.uuidIndexDataMap    = new HashMap<String,UUIDIndexData>();
      this.jobIndexDataMap     = new HashMap<Long,JobIndexData>();
      this.storageIndexDataMap = new HashMap<Long,StorageIndexData>();
    }

    /** remove not checked entries
     */
    public void clearNotChecked()
    {
      String[] keys = uuidIndexDataMap.keySet().toArray(new String[0]);
      for (String key : keys)
      {
        UUIDIndexData uuidIndexData = uuidIndexDataMap.get(key);
        if (!uuidIndexData.isChecked()) uuidIndexDataMap.remove(key);
      }
    }

    /** get UUID index data from map by UUID
     * @param uuid UUID
     * @return UUID index data
     */
    public UUIDIndexData getUUIDIndexData(String uuid)
    {
      return uuidIndexDataMap.get(uuid);
    }

    /** get all UUID index data from map by UUID
     * @return UUID index data collection
     */
    public Collection<UUIDIndexData> getUUIDIndexData()
    {
      return uuidIndexDataMap.values();
    }

    /** update UUID data index
     * @param uuid uuid
     * @param name job name
     * @param lastDateTime last date/time (timestamp) when storage was created
     * @param totalSize total size of storage [byte]
     * @param lastErrorMessage last error message text
     */
    synchronized public UUIDIndexData updateUUIDIndexData(String uuid, String name, long lastDateTime, long totalSize, String lastErrorMessage)
    {
      UUIDIndexData uuidIndexData = uuidIndexDataMap.get(uuid);
      if (uuidIndexData != null)
      {
        uuidIndexData.name         = name;
        uuidIndexData.dateTime     = lastDateTime;
        uuidIndexData.size         = totalSize;
        uuidIndexData.errorMessage = lastErrorMessage;
      }
      else
      {
        uuidIndexData = new UUIDIndexData(uuid,
                                          name,
                                          lastDateTime,
                                          totalSize,
                                          lastErrorMessage
                                         );
        uuidIndexDataMap.put(uuid,uuidIndexData);
      }

      return uuidIndexData;
    }

    /** get job index data from map by job id
     * @param jobId database job id
     * @return job index data
     */
    public JobIndexData getJobIndexData(long jobId)
    {
      return jobIndexDataMap.get(jobId);
    }

    /** get job index data from map by job name
     * @param name job name
     * @return UUID data
     */
    public JobIndexData getJobIndexDataByName(String name)
    {
      for (JobIndexData jobIndexData : jobIndexDataMap.values())
      {
        if (jobIndexData.name.equals(name)) return jobIndexData;
      }

      return null;
    }

    /** update job data index
     * @param jobId job id
     * @param name name of storage
     * @param lastDateTime last date/time (timestamp) when storage was created
     * @param totalSize total size of storage [byte]
     * @param lastErrorMessage last error message text
     */
    public synchronized JobIndexData updateJobIndexData(long jobId, ArchiveTypes archiveType, long lastDateTime, long totalSize, String lastErrorMessage)
    {
      JobIndexData jobIndexData = jobIndexDataMap.get(jobId);
      if (jobIndexData != null)
      {
        jobIndexData.jobId        = jobId;
        jobIndexData.archiveType  = archiveType;
        jobIndexData.dateTime     = lastDateTime;
        jobIndexData.size         = totalSize;
        jobIndexData.errorMessage = lastErrorMessage;
      }
      else
      {
        jobIndexData = new JobIndexData(jobId,
                                        archiveType,
                                        lastDateTime,
                                        totalSize,
                                        lastErrorMessage
                                       );
        jobIndexDataMap.put(jobId,jobIndexData);
      }

      return jobIndexData;
    }

    /** get storage index data from map by storage id
     * @param storageId database storage id
     * @return storage index data
     */
    public StorageIndexData getStorageIndexData(long storageId)
    {
      return storageIndexDataMap.get(storageId);
    }

    /** update storage data index
     * @param storageId database storage id
     * @param jobId database job id
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param size size of storage [byte]
     * @param title title to show
     * @param indexState storage index state
     * @param indexMode storage index mode
     * @param lastCheckedDateTime last checked date/time (timestamp)
     * @param errorMessage error message text
     */
    public synchronized StorageIndexData updateStorageIndexData(long storageId, long jobId, String name, long dateTime, long size, String title, IndexStates indexState, IndexModes indexMode, long lastCheckedDateTime, String errorMessage)
    {
      StorageIndexData storageIndexData = storageIndexDataMap.get(storageId);
      if (storageIndexData != null)
      {
        storageIndexData.jobId               = jobId;
        storageIndexData.name                = name;
        storageIndexData.dateTime            = dateTime;
        storageIndexData.size                = size;
        storageIndexData.indexState          = indexState;
        storageIndexData.indexMode           = indexMode;
        storageIndexData.lastCheckedDateTime = lastCheckedDateTime;
        storageIndexData.errorMessage        = errorMessage;
      }
      else
      {
        storageIndexData = new StorageIndexData(storageId,
                                                jobId,
                                                name,
                                                dateTime,
                                                size,
                                                new File(name).getName(),
                                                indexState,
                                                indexMode,
                                                lastCheckedDateTime,
                                                errorMessage
                                               );
        storageIndexDataMap.put(storageId,storageIndexData);
      }

      return storageIndexData;
    }

    /** remove index data from map
     * @param indexData index data
     */
    public void remove(IndexData indexData)
    {
      if      (indexData instanceof UUIDIndexData)
      {
        uuidIndexDataMap.remove((UUIDIndexData)indexData);
      }
      else if (indexData instanceof JobIndexData)
      {
        jobIndexDataMap.remove((JobIndexData)indexData);
      }
      else if (indexData instanceof StorageIndexData)
      {
        storageIndexDataMap.remove((StorageIndexData)indexData);
      }
    }
  }

  /** find index for insert of item in sorted storage data list
   * @param storageIndexData data of tree item
   * @return index in tree
   */
  private int findStorageTreeIndex(TreeItem treeItem, JobIndexData jobIndexData)
  {
    TreeItem               treeItems[]            = treeItem.getItems();
    JobIndexDataComparator jobIndexDataComparator = new JobIndexDataComparator(widgetStorageTree);

    int index = 0;
    while (   (index < treeItems.length)
           && (jobIndexDataComparator.compare(jobIndexData,(JobIndexData)treeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of item in sorted storage data list
   * @param storageIndexData data of tree item
   * @return index in table
   */
  private int findStorageListIndex(StorageIndexData storageIndexData)
  {
    TableItem                  tableItems[]          = widgetStorageTable.getItems();
    StorageIndexDataComparator storageIndexDataComparator = new StorageIndexDataComparator(widgetStorageTable);

    int index = 0;
    while (   (index < tableItems.length)
           && (storageIndexDataComparator.compare(storageIndexData,(StorageIndexData)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update storage tree/list thread
   */
  class UpdateStorageThread extends Thread
  {
    private Object        trigger                    = new Object();   // trigger update object
    private boolean       triggeredFlag              = false;
    private int           storageMaxCount            = 100;
    private String        storagePattern             = null;
    private IndexStateSet storageIndexStateSetFilter = INDEX_STATE_SET_ALL;
    private boolean       setUpdateIndicator         = false;          // true to set color/cursor at update

    /** create update storage list thread
     */
    UpdateStorageThread()
    {
      super();
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
          if (setUpdateIndicator)
          {
            // set busy cursor and foreground color to inform about update
            display.syncExec(new Runnable()
            {
              public void run()
              {
                shell.setCursor(waitCursor);
                widgetStorageTree.setForeground(COLOR_MODIFIED);
                widgetStorageTable.setForeground(COLOR_MODIFIED);
              }
            });
          }

          // update entries
          try
          {
/*
            // get current storage index data
            final HashSet<StorageIndexData> removeStorageIdSet = new HashSet<StorageIndexData>();
            if (!triggeredFlag)
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
                  {
                    UUIDIndexData uuidIndexData = (UUIDIndexData)uuidTreeItem.getData();
                    if (uuidTreeItem.getExpanded())
                    {
                      for (TreeItem jobTreeItem : uuidTreeItem.getItems())
                      {
                        JobIndexData jobIndexData = (JobIndexData)jobTreeItem.getData();
                        if (jobTreeItem.getExpanded())
                        {
                          for (TreeItem storageTreeItem : jobTreeItem.getItems())
                          {
                            removeStorageIdSet.add((StorageIndexData)storageTreeItem.getData());
                          }
                        }
                      }
                    }
                  }
                }
              });
            }
            */

            HashSet<TreeItem> uuidTreeItems = new HashSet<TreeItem>();
            if (!triggeredFlag)
            {
              updateUUIDTreeItems(uuidTreeItems);
            }

            HashSet<TreeItem> jobTreeItems = new HashSet<TreeItem>();
            if (!triggeredFlag)
            {
              updateJobTreeItems(uuidTreeItems,jobTreeItems);
            }

            if (!triggeredFlag)
            {
              updateStorageTreeItems(jobTreeItems);
            }

            if (!triggeredFlag)
            {
              updateStorageTableItems();
            }
          }
          catch (CommunicationError error)
          {
            // ignored
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

          if (setUpdateIndicator)
          {
            // reset cursor and foreground color
            display.syncExec(new Runnable()
            {
              public void run()
              {
                widgetStorageTable.setForeground(null);
                widgetStorageTree.setForeground(null);
                shell.setCursor(null);
              }
            });
          }

          // sleep a short time or get new pattern
          synchronized(trigger)
          {
            if (!triggeredFlag)
            {
              // wait for refresh request or timeout
              try { trigger.wait(30*1000); } catch (InterruptedException exception) { /* ignored */ };
            }

            // if not triggered (timeout occurred) update is done invisible (color is not set)
            if (!triggeredFlag) setUpdateIndicator = false;

            triggeredFlag = false;
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

    /** trigger an update of the storage list
     * @param storagePattern new storage pattern
     * @param storageIndexStateSetFilter new storage index state set filter
     * @param storageMaxCount new max. entries in list
     */
    public void triggerUpdate(String storagePattern, IndexStateSet storageIndexStateSetFilter, int storageMaxCount)
    {
      synchronized(trigger)
      {
        if ((this.storagePattern == null) || (storagePattern == null) || !storagePattern.equals(storagePattern))
        {
          this.storagePattern             = storagePattern;
          this.storageIndexStateSetFilter = storageIndexStateSetFilter;
          this.storageMaxCount            = storageMaxCount;
          this.setUpdateIndicator         = true;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger an update of the storage list
     * @param storagePattern new storage pattern
     */
    public void triggerUpdate(String storagePattern)
    {
      synchronized(trigger)
      {
        if ((this.storagePattern == null) || !storagePattern.equals(storagePattern))
        {
          this.storagePattern     = storagePattern;
          this.setUpdateIndicator = true;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger an update of the storage list
     * @param storageIndexStateSetFilter new storage index state set filter
     */
    public void triggerUpdate(IndexStateSet storageIndexStateSetFilter)
    {
      synchronized(trigger)
      {
        if (this.storageIndexStateSetFilter != storageIndexStateSetFilter)
        {
          this.storageIndexStateSetFilter = storageIndexStateSetFilter;
          this.setUpdateIndicator         = true;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger an update of the storage list
     * @param storageMaxCount new max. entries in list
     */
    public void triggerUpdate(int storageMaxCount)
    {
      synchronized(trigger)
      {
        if (this.storageMaxCount != storageMaxCount)
        {
          this.storageMaxCount    = storageMaxCount;
          this.setUpdateIndicator = true;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger an update of the storage list
     */
    public void triggerUpdate()
    {
      synchronized(trigger)
      {
        this.setUpdateIndicator = true;

        triggeredFlag = true;
        trigger.notify();
      }
    }

    /** update UUID tree items
     */
    private void updateUUIDTreeItems(final HashSet<TreeItem> uuidTreeItems)
    {
      final HashSet<TreeItem> removeUUIDTreeItemSet = new HashSet<TreeItem>();
      Command                 command;
      String[]                resultErrorMessage    = new String[1];
      ValueMap                resultMap             = new ValueMap();

      uuidTreeItems.clear();

      // get UUID items
      removeUUIDTreeItemSet.clear();
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TreeItem treeItem : widgetStorageTree.getItems())
          {
            assert treeItem.getData() instanceof UUIDIndexData;
            removeUUIDTreeItemSet.add(treeItem);
          }
        }
      });
      if (triggeredFlag) return;

      // update UUID list
      command = BARServer.runCommand(StringParser.format("INDEX_UUID_LIST pattern=%'S",
                                                         (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                        ),
                                     0
                                    );
      while (!command.endOfData() && !triggeredFlag)
      {
        if (command.getNextResult(resultErrorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            String uuid             = resultMap.getString("uuid"            );
            String name             = resultMap.getString("name"            );
            long   lastDateTime     = resultMap.getLong  ("lastDateTime"    );
            long   totalSize        = resultMap.getLong  ("totalSize"       );
            String lastErrorMessage = resultMap.getString("lastErrorMessage");

            // add/update index map
            final UUIDIndexData uuidIndexData = indexDataMap.updateUUIDIndexData(uuid,
                                                                                 name,
                                                                                 lastDateTime,
                                                                                 totalSize,
                                                                                 lastErrorMessage
                                                                                );

            // update/insert tree item
            display.syncExec(new Runnable()
            {
              public void run()
              {
                TreeItem uuidTreeItem = Widgets.getTreeItem(widgetStorageTree,uuidIndexData);
                if (uuidTreeItem == null)
                {
                  // insert tree item
                  uuidTreeItem = Widgets.insertTreeItem(widgetStorageTree,
                                                        findStorageTreeIndex(uuidIndexData),
                                                        (Object)uuidIndexData,
                                                        true
                                                       );
                  uuidIndexData.setTreeItem(uuidTreeItem,uuidIndexData.new TreeItemUpdateRunnable()
                  {
                    public void update(TreeItem treeItem, IndexData indexData)
                    {
                      UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

                      Widgets.updateTreeItem(treeItem,
                                             (Object)uuidIndexData,
                                             uuidIndexData.name,
                                             Units.formatByteSize(uuidIndexData.size),
                                             simpleDateFormat.format(new Date(uuidIndexData.dateTime*1000)),
                                             ""
                                            );
                    }
                  });
                }
                else
                {
                  assert uuidTreeItem.getData() instanceof UUIDIndexData;

                  // keep tree item
                  removeUUIDTreeItemSet.remove(uuidTreeItem);
                }
                if (uuidTreeItem.getExpanded())
                {
                  uuidTreeItems.add(uuidTreeItem);
                }

                // update view
                uuidIndexData.update();
              }
            });

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
      if (triggeredFlag) return;

      // remove not existing entries
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TreeItem treeItem : removeUUIDTreeItemSet)
          {
            IndexData indexData = (IndexData)treeItem.getData();
            Widgets.removeTreeItem(widgetStorageTree,treeItem);
            indexData.clearTreeItem();
          }
        }
      });
    }

    /** update job tree items
     * @param uuidTreeItem UUID tree item to update
     * @param jobTreeItems updated job tree items
     */
    private void updateJobTreeItems(final TreeItem uuidTreeItem, final HashSet<TreeItem> jobTreeItems)
    {
      final HashSet<TreeItem> removeJobTreeItemSet = new HashSet<TreeItem>();
      Command                 command;
      String[]                resultErrorMessage   = new String[1];
      ValueMap                resultMap            = new ValueMap();

      // get job items, UUID index data
      final UUIDIndexData uuidIndexData[] = new UUIDIndexData[1];
      display.syncExec(new Runnable()
      {
        public void run()
        {
          assert uuidTreeItem.getData() instanceof UUIDIndexData;

          for (TreeItem treeItem : uuidTreeItem.getItems())
          {
            assert treeItem.getData() instanceof JobIndexData;
            removeJobTreeItemSet.add(treeItem);
          }

          uuidIndexData[0] = (UUIDIndexData)uuidTreeItem.getData();
        }
      });
      if (triggeredFlag) return;

      // update job list
      command = BARServer.runCommand(StringParser.format("INDEX_JOB_LIST uuid=%'S pattern=%'S",
                                                         uuidIndexData[0].uuid,
                                                         (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                        ),
                                     0
                                    );
      while (!command.endOfData())
      {
        if (command.getNextResult(resultErrorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            long         jobId            = resultMap.getLong  ("jobId"                  );
            String       uuid             = resultMap.getString("uuid"                   );
            ArchiveTypes archiveType      = resultMap.getEnum  ("type",ArchiveTypes.class);
            long         lastDateTime     = resultMap.getLong  ("lastDateTime"           );
            long         totalSize        = resultMap.getLong  ("totalSize"              );
            String       lastErrorMessage = resultMap.getString("lastErrorMessage"       );

            // add/update job data index
            final JobIndexData jobIndexData = indexDataMap.updateJobIndexData(jobId,
                                                                              archiveType,
                                                                              lastDateTime,
                                                                              totalSize,
                                                                              lastErrorMessage
                                                                             );

            // insert/update tree item
            display.syncExec(new Runnable()
            {
              public void run()
              {
                TreeItem jobTreeItem = Widgets.getTreeItem(widgetStorageTree,jobIndexData);
                if (jobTreeItem == null)
                {
                  // insert tree item
                  jobTreeItem = Widgets.insertTreeItem(uuidTreeItem,
                                                       findStorageTreeIndex(uuidTreeItem,jobIndexData),
                                                       (Object)jobIndexData,
                                                       true
                                                      );
                  jobIndexData.setTreeItem(jobTreeItem);
                }
                else
                {
                  assert jobTreeItem.getData() instanceof JobIndexData;

                  // keep tree item
                  removeJobTreeItemSet.remove(jobTreeItem);
                }
                if (jobTreeItem.getExpanded())
                {
                  jobTreeItems.add(jobTreeItem);
                }

                // update view
                jobIndexData.update();
              }
            });
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

      // remove not existing entries
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TreeItem treeItem : removeJobTreeItemSet)
          {
            IndexData indexData = (IndexData)treeItem.getData();
            Widgets.removeTreeItem(widgetStorageTree,treeItem);
            indexData.clearTreeItem();
          }
        }
      });
    }

    /** update job tree items
     * @param uuidTreeItems UUID tree items to update
     * @param jobTreeItems updated job tree items
     */
    private void updateJobTreeItems(final HashSet<TreeItem> uuidTreeItems, final HashSet<TreeItem> jobTreeItems)
    {
      jobTreeItems.clear();

      for (final TreeItem uuidTreeItem : uuidTreeItems)
      {
        updateJobTreeItems(uuidTreeItem,jobTreeItems);
      }
    }

    /** update storage tree items
     * @param jobTreeItem job tree item to update
     */
    private void updateStorageTreeItems(final TreeItem jobTreeItem)
    {
      final HashSet<TreeItem> removeStorageTreeItemSet = new HashSet<TreeItem>();
      Command                 command;
      String[]                resultErrorMessage    = new String[1];
      ValueMap                resultMap             = new ValueMap();

      // get storage items, job index data
      final JobIndexData jobIndexData[] = new JobIndexData[1];
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TreeItem treeItem : jobTreeItem.getItems())
          {
            assert treeItem.getData() instanceof StorageIndexData;
            removeStorageTreeItemSet.add(treeItem);
          }

          jobIndexData[0] = (JobIndexData)jobTreeItem.getData();
        }
      });
      if (triggeredFlag) return;

      // update storage list
      command = BARServer.runCommand(StringParser.format("INDEX_STORAGE_LIST jobId=%d maxCount=%d indexState=%s indexMode=%s pattern=%'S",
                                                         jobIndexData[0].jobId,
                                                         -1,
                                                         "*",
                                                         "*",
                                                         (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                        ),
                                     0
                                    );
      while (!command.endOfData())
      {
        if (command.getNextResult(resultErrorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            long        storageId           = resultMap.getLong  ("storageId"                   );
            String      uuid                = resultMap.getString("uuid"                        );
            long        jobId               = resultMap.getLong  ("jobId"                       );
            String      name                = resultMap.getString("name"                        );
            long        dateTime            = resultMap.getLong  ("dateTime"                    );
            long        size                = resultMap.getLong  ("size"                        );
            IndexStates indexState          = resultMap.getEnum  ("indexState",IndexStates.class);
            IndexModes  indexMode           = resultMap.getEnum  ("indexMode",IndexModes.class  );
            long        lastCheckedDateTime = resultMap.getLong  ("lastCheckedDateTime"         );
            String      errorMessage        = resultMap.getString("errorMessage"                );

            // add/update storage data
            final StorageIndexData storageIndexData = indexDataMap.updateStorageIndexData(storageId,
                                                                                          jobId,
                                                                                          name,
                                                                                          dateTime,
                                                                                          size,
                                                                                          new File(name).getName(),
                                                                                          indexState,
                                                                                          indexMode,
                                                                                          lastCheckedDateTime,
                                                                                          errorMessage
                                                                                         );

            // insert/update tree item
            display.syncExec(new Runnable()
            {
              public void run()
              {
                TreeItem storageTreeItem = Widgets.getTreeItem(widgetStorageTree,storageIndexData);
                if (storageTreeItem == null)
                {
                  // insert tree item
                  storageTreeItem = Widgets.insertTreeItem(jobTreeItem,
                                                           findStorageTreeIndex(jobTreeItem,storageIndexData),
                                                           (Object)storageIndexData,
                                                           false
                                                          );
                  storageIndexData.setTreeItem(storageTreeItem);
                }
                else
                {
                  // keep tree item
                  removeStorageTreeItemSet.remove(storageTreeItem);
                }

                // update view
                storageIndexData.update();
              }
            });
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

      // remove not existing entries
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TreeItem treeItem : removeStorageTreeItemSet)
          {
            IndexData indexData = (IndexData)treeItem.getData();
            Widgets.removeTreeItem(widgetStorageTree,treeItem);
            indexData.clearTreeItem();
          }
        }
      });
    }

    /** update storage tree items
     * @param jobTreeItems job tree items to update
     */
    private void updateStorageTreeItems(final HashSet<TreeItem> jobTreeItems)
    {
      for (final TreeItem jobTreeItem : jobTreeItems)
      {
        updateStorageTreeItems(jobTreeItem);
      }
    }

    /** update tree items
     * @param treeItem tree item to update
     */
    private void updateTreeItems(TreeItem treeItem)
    {
      if      (treeItem.getData() instanceof UUIDIndexData)
      {
        updateStorageThread.updateJobTreeItems(treeItem,new HashSet<TreeItem>());
      }
      else if (treeItem.getData() instanceof JobIndexData)
      {
        updateStorageThread.updateStorageTreeItems(treeItem);
      }
    }

    /** update storage table items
     */
    private void updateStorageTableItems()
    {
      Command  command;
      String[] resultErrorMessage = new String[1];
      ValueMap resultMap          = new ValueMap();

      // get current storage index data
      final HashSet<TableItem> removeTableItemSet = new HashSet<TableItem>();
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TableItem tableItem : widgetStorageTable.getItems())
          {
            removeTableItemSet.add(tableItem);
          }
        }
      });
      if (triggeredFlag) return;

      // update storage table
      command = BARServer.runCommand(StringParser.format("INDEX_STORAGE_LIST jobId=* maxCount=%d indexState=%s indexMode=%s pattern=%'S",
                                                         storageMaxCount,
                                                         storageIndexStateSetFilter.nameList("|"),
                                                         "*",
                                                         (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                        ),
                                     0
                                    );
      while (!command.endOfData() && !triggeredFlag)
      {
        if (command.getNextResult(resultErrorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            long        storageId           = resultMap.getLong  ("storageId"                   );
            long        jobId               = resultMap.getLong  ("jobId"                       );
            String      uuid                = resultMap.getString("uuid"                        );
            String      name                = resultMap.getString("name"                        );
            long        dateTime            = resultMap.getLong  ("dateTime"                    );
            long        size                = resultMap.getLong  ("size"                        );
            IndexStates indexState          = resultMap.getEnum  ("indexState",IndexStates.class);
            IndexModes  indexMode           = resultMap.getEnum  ("indexMode",IndexModes.class  );
            long        lastCheckedDateTime = resultMap.getLong  ("lastCheckedDateTime"         );
            String      errorMessage        = resultMap.getString("errorMessage"                );

            // add/update to index map
            final StorageIndexData storageIndexData = indexDataMap.updateStorageIndexData(storageId,
                                                                                          jobId,
                                                                                          name,
                                                                                          dateTime,
                                                                                          size,
                                                                                          new File(name).getName(),
                                                                                          indexState,
                                                                                          indexMode,
                                                                                          lastCheckedDateTime,
                                                                                          errorMessage
                                                                                         );

            // insert/update table item
            display.syncExec(new Runnable()
            {
              public void run()
              {
                TableItem tableItem = Widgets.getTableItem(widgetStorageTable,(Object)storageIndexData);
                if (tableItem == null)
                {
                  // insert table item
                  tableItem = Widgets.insertTableItem(widgetStorageTable,
                                                      findStorageListIndex(storageIndexData),
                                                      (Object)storageIndexData
                                                     );
                  storageIndexData.setTableItem(tableItem,storageIndexData.new TableItemUpdateRunnable()
                  {
                    public void update(TableItem tableItem, IndexData indexData)
                    {
                      StorageIndexData storageIndexData = (StorageIndexData)indexData;

                      Widgets.updateTableItem(widgetStorageTable,
                                              (Object)storageIndexData,
                                              storageIndexData.name,
                                              Units.formatByteSize(storageIndexData.size),
                                              simpleDateFormat.format(new Date(storageIndexData.dateTime*1000)),
                                              storageIndexData.indexState.toString()
                                             );
                    }
                  });
                }
                else
                {
                  // keep table item
                  removeTableItemSet.remove(tableItem);
                }

                // update view
                storageIndexData.update();
              }
            });
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
      if (triggeredFlag) return;

      // remove not existing entries
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TableItem tableItem : removeTableItemSet)
          {
            IndexData indexData = (IndexData)tableItem.getData();
            Widgets.removeTableItem(widgetStorageTable,tableItem);
            indexData.clearTableItem();
          }
        }
      });
    }
  }

  /** entry types
   */
  enum EntryTypes
  {
    FILE,
    IMAGE,
    DIRECTORY,
    LINK,
    HARDLINK,
    SPECIAL,
    DEVICE,
    SOCKET
  };

  /** entry restore states
   */
  enum RestoreStates
  {
    UNKNOWN,
    RESTORED,
    ERROR
  }

  /** entry data
   */
  class EntryData
  {
    String        storageName;
    long          storageDateTime;
    String        name;
    EntryTypes    type;
    long          dateTime;
    long          size;
    boolean       checked;
    RestoreStates restoreState;

    /** create entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param name entry name
     * @param type entry type
     * @param dateTime date/time (timestamp)
     * @param size size [bytes]
     */
    EntryData(String storageName, long storageDateTime, String name, EntryTypes type, long dateTime, long size)
    {
      this.storageName     = storageName;
      this.storageDateTime = storageDateTime;
      this.name            = name;
      this.type            = type;
      this.dateTime        = dateTime;
      this.size            = size;
      this.checked         = false;
      this.restoreState    = RestoreStates.UNKNOWN;
    }

    /** create entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param name entry name
     * @param type entry type
     * @param dateTime date/time (timestamp)
     */
    EntryData(String storageName, long storageDateTime, String name, EntryTypes type, long dateTime)
    {
      this(storageName,storageDateTime,name,type,dateTime,0L);
    }

    /** set restore state of entry
     * @param restoreState restore state
     */
    public void setState(RestoreStates restoreState)
    {
      this.restoreState = restoreState;
    }

    /** check if entry is checked
     * @return true if entry is checked, false otherwise
     */
    public boolean isChecked()
    {
      return checked;
    }

    /** set checked state
     * @param checked checked state
     */
    public void setChecked(boolean checked)
    {
      this.checked = checked;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Entry {"+storageName+", "+name+", "+type+", dateTime="+dateTime+", "+size+" bytes, checked="+checked+", state="+restoreState+"}";
    }
  };

  /** entry data map
   */
  class EntryDataMap extends HashMap<String,EntryData>
  {
    /** remove not checked entries
     */
    public void clearNotChecked()
    {
      String[] keys = keySet().toArray(new String[0]);
      for (String key : keys)
      {
        EntryData entryData = get(key);
        if (!entryData.isChecked()) remove(key);
      }
    }

    /** put entry data into map
     * @param entryData entry data
     */
    public void put(EntryData entryData)
    {
      put(getHashKey(entryData),entryData);
    }

    /** get entry data
     * @param storageName storage name
     * @param name name
     * @param entryType entry type
     * @return entry data or null if not found
     */
    public EntryData get(String storageName, String name, EntryTypes entryType)
    {
      return get(getHashKey(storageName,name,entryType));
    }

    /** get hash key from data
     * @param storageName storage name
     * @param name name
     * @param entryType entry type
     * @return hash key string
     */
    private String getHashKey(String storageName, String name, EntryTypes entryType)
    {
      return storageName+name+entryType.toString();
    }

    /** get hash key from entry data
     * @param entryData
     * @return hash key string
     */
    private String getHashKey(EntryData entryData)
    {
      return getHashKey(entryData.storageName,entryData.name,entryData.type);
    }
  }

  /** entry data comparator
   */
  class EntryDataComparator implements Comparator<EntryData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_ARCHIVE = 0;
    private final static int SORTMODE_NAME    = 1;
    private final static int SORTMODE_TYPE    = 2;
    private final static int SORTMODE_SIZE    = 3;
    private final static int SORTMODE_DATE    = 4;

    private int sortMode;

    /** create entry data comparator
     * @param table entry table
     * @param sortColumn sorting column
     */
    EntryDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_ARCHIVE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_DATE;
      else                                       sortMode = SORTMODE_NAME;
    }

    /** create entry data comparator
     * @param table entry table
     */
    EntryDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare entry data
     * @param entryData1, entryData2 entry data to compare
     * @return -1 iff entryData1 < entryData2,
                0 iff entryData1 = entryData2,
                1 iff entryData1 > entryData2
     */
    public int compare(EntryData entryData1, EntryData entryData2)
    {
      switch (sortMode)
      {
        case SORTMODE_ARCHIVE:
          return entryData1.storageName.compareTo(entryData2.storageName);
        case SORTMODE_NAME:
          return entryData1.name.compareTo(entryData2.name);
        case SORTMODE_TYPE:
          return entryData1.type.compareTo(entryData2.type);
        case SORTMODE_SIZE:
          if      (entryData1.size < entryData2.size) return -1;
          else if (entryData1.size > entryData2.size) return  1;
          else                                        return  0;
        case SORTMODE_DATE:
          if      (entryData1.dateTime < entryData2.dateTime) return -1;
          else if (entryData1.dateTime > entryData2.dateTime) return  1;
          else                                                return  0;
        default:
          return 0;
      }
    }
  }

  /** update entry list thread
   */
  class UpdateEntryListThread extends Thread
  {
    private Object  trigger                = new Object();   // trigger update object
    private boolean triggeredFlag          = false;

    private boolean checkedStorageOnlyFlag = false;
    private int     entryMaxCount          = 100;
    private String  entryPattern           = null;
    private boolean newestEntriesOnlyFlag  = false;

    /** create update entry list thread
     */
    UpdateEntryListThread()
    {
      super();
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
          // set busy cursor, foreground color to inform about update
          display.syncExec(new Runnable()
          {
            public void run()
            {
              shell.setCursor(waitCursor);
              widgetEntryTable.setForeground(COLOR_MODIFIED);
            }
          });

          // update
          synchronized(entryDataMap)
          {
            // clear not checked entries
            entryDataMap.clearNotChecked();

            // get matching entries
            if (entryPattern != null)
            {
              // execute command
              Command command = BARServer.runCommand(StringParser.format("INDEX_ENTRIES_LIST entryPattern=%'S checkedStorageOnlyFlag=%y entryMaxCount=%d newestEntriesOnlyFlag=%y",
                                                                         entryPattern,
                                                                         checkedStorageOnlyFlag,
                                                                         entryMaxCount,
                                                                         newestEntriesOnlyFlag
                                                                        ),
                                                     0
                                                    );

              // read results, update/add data
              String[] resultErrorMessage = new String[1];
              ValueMap resultMap          = new ValueMap();
              while (!command.endOfData() && !triggeredFlag)
              {
                if (command.getNextResult(resultErrorMessage,
                                          resultMap,
                                          Command.TIMEOUT
                                         ) == Errors.NONE
                   )
                {
                  try
                  {
                    switch (resultMap.getEnum("entryType",EntryTypes.class))
                    {
                      case FILE:
                        {
                          String storageName     = resultMap.getString("storageName"    );
                          long   storageDateTime = resultMap.getLong  ("storageDateTime");
                          String fileName        = resultMap.getString("name"           );
                          long   dateTime        = resultMap.getLong  ("dateTime"       );
                          long   size            = resultMap.getLong  ("size"           );
                          long   fragmentOffset  = resultMap.getLong  ("fragmentOffset" );
                          long   fragmentSize    = resultMap.getLong  ("fragmentSize"   );

                          EntryData entryData = entryDataMap.get(storageName,fileName,EntryTypes.FILE);
                          if (entryData != null)
                          {
                            entryData.dateTime = dateTime;
                            entryData.size     = size;
                          }
                          else
                          {
                            entryData = new EntryData(storageName,storageDateTime,fileName,EntryTypes.FILE,dateTime,size);
                            entryDataMap.put(entryData);
                          }
                        }
                        break;
                      case IMAGE:
                        {
                          String storageName     = resultMap.getString("storageName"    );
                          long   storageDateTime = resultMap.getLong  ("storageDateTime");
                          String imageName       = resultMap.getString("name"           );
                          long   size            = resultMap.getLong  ("size"           );
                          long   blockOffset     = resultMap.getLong  ("blockOffset"    );
                          long   blockCount      = resultMap.getLong  ("blockCount"     );

                          EntryData entryData = entryDataMap.get(storageName,imageName,EntryTypes.IMAGE);
                          if (entryData != null)
                          {
                            entryData.size = size;
                          }
                          else
                          {
                            entryData = new EntryData(storageName,storageDateTime,imageName,EntryTypes.IMAGE,0L,size);
                            entryDataMap.put(entryData);
                          }
                        }
                        break;
                      case DIRECTORY:
                        {
                          String storageName     = resultMap.getString("storageName"    );
                          long   storageDateTime = resultMap.getLong  ("storageDateTime");
                          String directoryName   = resultMap.getString("name"           );
                          long   dateTime        = resultMap.getLong  ("dateTime"       );

                          EntryData entryData = entryDataMap.get(storageName,directoryName,EntryTypes.DIRECTORY);
                          if (entryData != null)
                          {
                            entryData.dateTime = dateTime;
                          }
                          else
                          {
                            entryData = new EntryData(storageName,storageDateTime,directoryName,EntryTypes.DIRECTORY,dateTime);
                            entryDataMap.put(entryData);
                          }
                        }
                        break;
                      case LINK:
                        {
                          String storageName     = resultMap.getString("storageName"    );
                          long   storageDateTime = resultMap.getLong  ("storageDateTime");
                          String linkName        = resultMap.getString("name"           );
                          String destinationName = resultMap.getString("destinationName");
                          long   dateTime        = resultMap.getLong  ("dateTime"       );

                          EntryData entryData = entryDataMap.get(storageName,linkName,EntryTypes.LINK);
                          if (entryData != null)
                          {
                            entryData.dateTime = dateTime;
                          }
                          else
                          {
                            entryData = new EntryData(storageName,storageDateTime,linkName,EntryTypes.LINK,dateTime);
                            entryDataMap.put(entryData);
                          }
                        }
                        break;
                      case HARDLINK:
                        {
                          String storageName     = resultMap.getString("storageName"    );
                          long   storageDateTime = resultMap.getLong  ("storageDateTime");
                          String fileName        = resultMap.getString("name"           );
                          long   dateTime        = resultMap.getLong  ("dateTime"       );
                          long   size            = resultMap.getLong  ("size"           );
                          long   fragmentOffset  = resultMap.getLong  ("fragmentOffset" );
                          long   fragmentSize    = resultMap.getLong  ("fragmentSize"   );

                          EntryData entryData = entryDataMap.get(storageName,fileName,EntryTypes.HARDLINK);
                          if (entryData != null)
                          {
                            entryData.dateTime = dateTime;
                          }
                          else
                          {
                            entryData = new EntryData(storageName,storageDateTime,fileName,EntryTypes.HARDLINK,dateTime);
                            entryDataMap.put(entryData);
                          }
                        }
                        break;
                      case SPECIAL:
                        {
                          String storageName     = resultMap.getString("storageName"    );
                          long   storageDateTime = resultMap.getLong  ("storageDateTime");
                          String name            = resultMap.getString("name"           );
                          long   dateTime        = resultMap.getLong  ("dateTime"       );

                          EntryData entryData = entryDataMap.get(storageName,name,EntryTypes.SPECIAL);
                          if (entryData != null)
                          {
                            entryData.dateTime = dateTime;
                          }
                          else
                          {
                            entryData = new EntryData(storageName,storageDateTime,name,EntryTypes.SPECIAL,dateTime);
                            entryDataMap.put(entryData);
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
            }
          }

          // refresh list
          display.syncExec(new Runnable()
          {
            public void run()
            {
              refreshEntryList();
            }
          });

          // reset cursor, foreground color
          display.syncExec(new Runnable()
          {
            public void run()
            {
              widgetEntryTable.setForeground(null);
              shell.setCursor(null);
            }
          });

          // get new pattern
          synchronized(trigger)
          {
            // wait for refresh request trigger
            while (!triggeredFlag)
            {
              try { trigger.wait(); } catch (InterruptedException exception) { /* ignored */ };
            }
            triggeredFlag = false;
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

    /** trigger an update of the entry list
     * @param checkedStorageOnlyFlag checked storage only or null
     * @param entryPattern new entry pattern or null
     * @param newestEntriesOnlyFlag flag for newest entries only or null
     * @param entryMaxCount max. entries in list or null
     */
    public void triggerUpdate(boolean checkedStorageOnlyFlag, String entryPattern, boolean newestEntriesOnlyFlag, int entryMaxCount)
    {
      synchronized(trigger)
      {
        this.checkedStorageOnlyFlag = checkedStorageOnlyFlag;
        this.entryPattern           = entryPattern;
        this.newestEntriesOnlyFlag  = newestEntriesOnlyFlag;
        this.entryMaxCount          = entryMaxCount;

        triggeredFlag = true;
        trigger.notify();
      }
    }
  }

  // --------------------------- constants --------------------------------
  // colors
  private final Color COLOR_MODIFIED;

  // images
  private final Image IMAGE_DIRECTORY;

  private final Image IMAGE_CLEAR;
  private final Image IMAGE_MARK_ALL;
  private final Image IMAGE_UNMARK_ALL;

  private final Image IMAGE_CONNECT0;
  private final Image IMAGE_CONNECT1;

  // date/time format
  private final SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  // cursors
  private final Cursor           waitCursor;

  // --------------------------- variables --------------------------------

  // global variable references
  private Shell               shell;
  private Display             display;

  // widgets
  public  Composite           widgetTab;
  private TabFolder           widgetTabFolder;

  private TabFolder           widgetStorageTabFolder;
  private Tree                widgetStorageTree;
  private Shell               widgetStorageTreeToolTip = null;
  private Table               widgetStorageTable;
  private Shell               widgetStorageTableToolTip = null;
  private Text                widgetStoragePattern;
  private Combo               widgetStorageStateFilter;
  private Combo               widgetStorageMaxCount;
  private WidgetEvent         checkedStorageEvent = new WidgetEvent();

  private Table               widgetEntryTable;
  private Shell               widgetEntryTableToolTip = null;
  private WidgetEvent         checkedEntryEvent = new WidgetEvent();

  private Button              widgetRestoreTo;
  private Text                widgetRestoreToDirectory;
  private Button              widgetOverwriteEntries;
  private WidgetEvent         selectRestoreToEvent = new WidgetEvent();

  private boolean             checkedStorageOnlyFlag = false;

  UpdateStorageThread         updateStorageThread;
//????
//private String              storagePattern       = null;
  private IndexStateSet       storageIndexStateSet = INDEX_STATE_SET_ALL;
  private int                 storageMaxCount      = 100;
  private IndexDataMap        indexDataMap         = new IndexDataMap();

  UpdateEntryListThread       updateEntryListThread;
  private String              entryPattern          = null;
  private boolean             newestEntriesOnlyFlag = false;
  private int                 entryMaxCount         = 100;
  private EntryDataMap        entryDataMap          = new EntryDataMap();

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create restore tab
   * @param parentTabFolder parent tab folder
   * @param accelerator keyboard shortcut to select tab
   */
  TabRestore(TabFolder parentTabFolder, int accelerator)
  {
    Group       group;
    TabFolder   tabFolder;
    Composite   tab;
    Menu        menu;
    MenuItem    menuItem;
    Composite   composite,subComposite;
    Label       label;
    Button      button;
    Combo       combo;
    TreeColumn  treeColumn;
    TreeItem    treeItem;
    Text        text;
    TableColumn tableColumn;
    Control     control;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    // get colors
    COLOR_MODIFIED = shell.getDisplay().getSystemColor(SWT.COLOR_GRAY);

    // get images
    IMAGE_DIRECTORY  = Widgets.loadImage(display,"directory.png");

    IMAGE_CLEAR      = Widgets.loadImage(display,"clear.png");
    IMAGE_MARK_ALL   = Widgets.loadImage(display,"mark.png");
    IMAGE_UNMARK_ALL = Widgets.loadImage(display,"unmark.png");

    IMAGE_CONNECT0   = Widgets.loadImage(display,"connect0.png");
    IMAGE_CONNECT1   = Widgets.loadImage(display,"connect1.png");

    // get cursors
    waitCursor = new Cursor(display,SWT.CURSOR_WAIT);

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Restore")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{0.5,0.5,0.0},new double[]{0.0,1.0},2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // connector button
    button = Widgets.newCheckbox(widgetTab,IMAGE_CONNECT0,IMAGE_CONNECT1);
    button.setToolTipText(BARControl.tr("When this connector is in state 'closed', only tagged storage archives are used for list entries."));
    Widgets.layout(button,0,0,TableLayoutData.W,2,0);
    button.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        checkedStorageOnlyFlag = widget.getSelection();
        updateEntryListThread.triggerUpdate(checkedStorageOnlyFlag,entryPattern,newestEntriesOnlyFlag,entryMaxCount);
      }
    });

    // storage tree/list
    group = Widgets.newGroup(widgetTab,BARControl.tr("Storage"));
    group.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,4));
    Widgets.layout(group,0,1,TableLayoutData.NSWE);
    {
      // fix layout
      control = Widgets.newSpacer(group);
      Widgets.layout(control,0,0,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,1);

      widgetStorageTabFolder = Widgets.newTabFolder(group);
      Widgets.layout(widgetStorageTabFolder,1,0,TableLayoutData.NSWE);

      // tree
      tab = Widgets.addTab(widgetStorageTabFolder,BARControl.tr("Jobs"));
      tab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,2));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);

      widgetStorageTree = Widgets.newTree(tab,SWT.CHECK);
      widgetStorageTree.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0}));
      Widgets.layout(widgetStorageTree,1,0,TableLayoutData.NSWE);
      SelectionListener storageTreeColumnSelectionListener = new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TreeColumn                 treeColumn            = (TreeColumn)selectionEvent.widget;
          StorageIndexDataComparator storageIndexDataComparator = new StorageIndexDataComparator(widgetStorageTree,treeColumn);
          synchronized(widgetStorageTree)
          {
            Widgets.sortTreeColumn(widgetStorageTree,treeColumn,storageIndexDataComparator);
          }
        }
      };
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,"Name",    SWT.LEFT, 450,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,"Size",    SWT.RIGHT,100,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for size."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,"Modified",SWT.LEFT, 150,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for modification date/time."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,"State",   SWT.LEFT,  60,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for state."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);

      widgetStorageTree.addListener(SWT.Expand,new Listener()
      {
        public void handleEvent(final Event event)
        {
          TreeItem treeItem = (TreeItem)event.item;
          treeItem.removeAll();
          updateStorageThread.updateTreeItems(treeItem);
          treeItem.setExpanded(true);
        }
      });
      widgetStorageTree.addListener(SWT.Collapse,new Listener()
      {
        public void handleEvent(final Event event)
        {
          final TreeItem treeItem = (TreeItem)event.item;
          treeItem.removeAll();
          new TreeItem(treeItem,SWT.NONE);
          treeItem.setExpanded(false);
        }
      });
      widgetStorageTree.addListener(SWT.MouseDoubleClick,new Listener()
      {
        public void handleEvent(final Event event)
        {
          TreeItem treeItem = widgetStorageTree.getItem(new Point(event.x,event.y));
          if (treeItem != null)
          {
            if      (   (treeItem.getData() instanceof UUIDIndexData)
                     || (treeItem.getData() instanceof JobIndexData)
                    )
            {
              // expand/collaps sub-tree
              Event treeEvent = new Event();
              treeEvent.item = treeItem;
              if (treeItem.getExpanded())
              {
                widgetStorageTree.notifyListeners(SWT.Collapse,treeEvent);
              }
              else
              {
                widgetStorageTree.notifyListeners(SWT.Expand,treeEvent);
              }
            }
            else if (treeItem.getData() instanceof StorageIndexData)
            {
              // toogle check
              StorageIndexData storageIndexData = (StorageIndexData)treeItem.getData();
              storageIndexData.setChecked(!storageIndexData.isChecked());
            }

            checkedStorageEvent.trigger();
          }
        }
      });
      widgetStorageTree.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TreeItem treeItem = (TreeItem)selectionEvent.item;
          if ((treeItem != null) && (selectionEvent.detail == SWT.CHECK))
          {
            IndexData indexData = (IndexData)treeItem.getData();

            // set checked
            indexData.setChecked(treeItem.getChecked());

            // set checked for sub-items: jobs, storage
            if      (indexData instanceof UUIDIndexData)
            {
              if (treeItem.getExpanded())
              {
                for (TreeItem jobTreeItem : treeItem.getItems())
                {
                  JobIndexData jobIndexData = (JobIndexData)jobTreeItem.getData();
                  jobIndexData.setChecked(indexData.isChecked());

                  if (jobTreeItem.getExpanded())
                  {
                    for (TreeItem storageTreeItem : jobTreeItem.getItems())
                    {
                      StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
                      storageIndexData.setChecked(indexData.isChecked());
                    }
                  }
                }
              }
            }
            else if (indexData instanceof JobIndexData)
            {
              if (treeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : treeItem.getItems())
                {
                  StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
                  storageIndexData.setChecked(indexData.isChecked());
                }
              }
            }

            // update storage list
            updateCheckedStorageList();

            // trigger update checked
            checkedStorageEvent.trigger();
            if (checkedStorageOnlyFlag)
            {
              updateEntryListThread.triggerUpdate(checkedStorageOnlyFlag,entryPattern,newestEntriesOnlyFlag,entryMaxCount);
            }
          }
        }
      });
      widgetStorageTree.addMouseTrackListener(new MouseTrackListener()
      {
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }

        public void mouseExit(MouseEvent mouseEvent)
        {
        }

        public void mouseHover(MouseEvent mouseEvent)
        {
          Tree     tree     = (Tree)mouseEvent.widget;
          TreeItem treeItem = tree.getItem(new Point(mouseEvent.x,mouseEvent.y));

          if (widgetStorageTreeToolTip != null)
          {
            widgetStorageTreeToolTip.dispose();
            widgetStorageTreeToolTip = null;
          }

          // show if tree item available and mouse is in the left side
          if ((treeItem != null) && (mouseEvent.x < 64))
          {
            if      (treeItem.getData() instanceof JobIndexData)
            {
              JobIndexData jobIndexData = (JobIndexData)treeItem.getData();
              Label        label;

              final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
              final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

              widgetStorageTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
              widgetStorageTreeToolTip.setBackground(COLOR_BACKGROUND);
              widgetStorageTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
              Widgets.layout(widgetStorageTreeToolTip,0,0,TableLayoutData.NSWE);
              widgetStorageTreeToolTip.addMouseTrackListener(new MouseTrackListener()
              {
                public void mouseEnter(MouseEvent mouseEvent)
                {
                }

                public void mouseExit(MouseEvent mouseEvent)
                {
                  widgetStorageTreeToolTip.dispose();
                  widgetStorageTreeToolTip = null;
                }

                public void mouseHover(MouseEvent mouseEvent)
                {
                }
              });

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Name")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,0,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,jobIndexData.name);
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,0,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last created")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,1,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,simpleDateFormat.format(new Date(jobIndexData.dateTime*1000)));
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,1,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total size")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,2,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("%d bytes (%s)"),jobIndexData.size,Units.formatByteSize(jobIndexData.size)));
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,2,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last error")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,3,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,jobIndexData.errorMessage);
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,3,1,TableLayoutData.WE);

              Point size = widgetStorageTreeToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
              Rectangle bounds = treeItem.getBounds(0);
              Point point = tree.toDisplay(mouseEvent.x+16,bounds.y);
              widgetStorageTreeToolTip.setBounds(point.x,point.y,size.x,size.y);
              widgetStorageTreeToolTip.setVisible(true);
            }
            else if (treeItem.getData() instanceof StorageIndexData)
            {
              StorageIndexData storageIndexData = (StorageIndexData)treeItem.getData();
              Label            label;

              final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
              final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

              widgetStorageTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
              widgetStorageTreeToolTip.setBackground(COLOR_BACKGROUND);
              widgetStorageTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
              Widgets.layout(widgetStorageTreeToolTip,0,0,TableLayoutData.NSWE);
              widgetStorageTreeToolTip.addMouseTrackListener(new MouseTrackListener()
              {
                public void mouseEnter(MouseEvent mouseEvent)
                {
                }

                public void mouseExit(MouseEvent mouseEvent)
                {
                  widgetStorageTreeToolTip.dispose();
                  widgetStorageTreeToolTip = null;
                }

                public void mouseHover(MouseEvent mouseEvent)
                {
                }
              });

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Name")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,0,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,storageIndexData.name);
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,0,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Created")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,1,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,simpleDateFormat.format(new Date(storageIndexData.dateTime*1000)));
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,1,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Size")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,2,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("%d bytes (%s)"),storageIndexData.size,Units.formatByteSize(storageIndexData.size)));
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,2,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("State")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,3,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,storageIndexData.indexState.toString());
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,3,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last checked")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,4,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,simpleDateFormat.format(new Date(storageIndexData.lastCheckedDateTime*1000)));
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,4,1,TableLayoutData.WE);

              label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Error")+":");
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,5,0,TableLayoutData.W);

              label = Widgets.newLabel(widgetStorageTreeToolTip,storageIndexData.errorMessage);
              label.setForeground(COLOR_FORGROUND);
              label.setBackground(COLOR_BACKGROUND);
              Widgets.layout(label,5,1,TableLayoutData.WE);

              Point size = widgetStorageTreeToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
              Rectangle bounds = treeItem.getBounds(0);
              Point point = tree.toDisplay(mouseEvent.x+16,bounds.y);
              widgetStorageTreeToolTip.setBounds(point.x,point.y,size.x,size.y);
              widgetStorageTreeToolTip.setVisible(true);
            }
          }
        }
      });
      widgetStorageTree.addKeyListener(new KeyListener()
      {
        public void keyPressed(KeyEvent keyEvent)
        {
        }
        public void keyReleased(KeyEvent keyEvent)
        {
          if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
          {
            refreshStorageIndex();
          }
          else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
          {
            removeStorageIndex();
          }
        }
      });

      // list
      tab = Widgets.addTab(widgetStorageTabFolder,BARControl.tr("List"));
      tab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,2));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);

      widgetStorageTable = Widgets.newTable(tab,SWT.CHECK);
      widgetStorageTable.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0}));
      Widgets.layout(widgetStorageTable,1,0,TableLayoutData.NSWE);
      SelectionListener storageListColumnSelectionListener = new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableColumn                tableColumn           = (TableColumn)selectionEvent.widget;
          StorageIndexDataComparator storageIndexDataComparator = new StorageIndexDataComparator(widgetStorageTable,tableColumn);
          synchronized(widgetStorageTable)
          {
            Widgets.sortTableColumn(widgetStorageTable,tableColumn,storageIndexDataComparator);
          }
        }
      };
      tableColumn = Widgets.addTableColumn(widgetStorageTable,0,"Name",    SWT.LEFT, 450,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,1,"Size",    SWT.RIGHT,100,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for size."));
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,2,"Modified",SWT.LEFT, 150,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for modification date/time."));
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,3,"State",   SWT.LEFT,  60,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for state."));
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      widgetStorageTable.addListener(SWT.MouseDoubleClick,new Listener()
      {
        public void handleEvent(final Event event)
        {
          TableItem tabletem = widgetStorageTable.getItem(new Point(event.x,event.y));
          if (tabletem != null)
          {
            tabletem.setChecked(!tabletem.getChecked());
            ((StorageIndexData)tabletem.getData()).setChecked(tabletem.getChecked());

            checkedStorageEvent.trigger();
          }
        }
      });
      widgetStorageTable.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableItem tabletem = (TableItem)selectionEvent.item;
          if ((tabletem != null) && (selectionEvent.detail == SWT.NONE))
          {
            // set checked
            ((StorageIndexData)tabletem.getData()).setChecked(tabletem.getChecked());

            // update storage list
            updateCheckedStorageList();

            // trigger update checked
            checkedStorageEvent.trigger();
            if (checkedStorageOnlyFlag)
            {
              updateEntryListThread.triggerUpdate(checkedStorageOnlyFlag,entryPattern,newestEntriesOnlyFlag,entryMaxCount);
            }
          }
        }
      });
      widgetStorageTable.addMouseTrackListener(new MouseTrackListener()
      {
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }

        public void mouseExit(MouseEvent mouseEvent)
        {
        }

        public void mouseHover(MouseEvent mouseEvent)
        {
          Table     table     = (Table)mouseEvent.widget;
          TableItem tableItem = table.getItem(new Point(mouseEvent.x,mouseEvent.y));

          if (widgetStorageTableToolTip != null)
          {
            widgetStorageTableToolTip.dispose();
            widgetStorageTableToolTip = null;
          }

          // show if table item available and mouse is in the left side
          if ((tableItem != null) && (mouseEvent.x < 64))
          {
            StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
            Label            label;

            final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
            final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

            widgetStorageTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
            widgetStorageTableToolTip.setBackground(COLOR_BACKGROUND);
            widgetStorageTableToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
            Widgets.layout(widgetStorageTableToolTip,0,0,TableLayoutData.NSWE);
            widgetStorageTableToolTip.addMouseTrackListener(new MouseTrackListener()
            {
              public void mouseEnter(MouseEvent mouseEvent)
              {
              }

              public void mouseExit(MouseEvent mouseEvent)
              {
                widgetStorageTableToolTip.dispose();
                widgetStorageTableToolTip = null;
              }

              public void mouseHover(MouseEvent mouseEvent)
              {
              }
            });

            label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Name")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,0,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.name);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,0,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Created")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageTableToolTip,simpleDateFormat.format(new Date(storageIndexData.dateTime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Size")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,2,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageTableToolTip,String.format(BARControl.tr("%d bytes (%s)"),storageIndexData.size,Units.formatByteSize(storageIndexData.size)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,2,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("State")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.indexState.toString());
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Last checked")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageTableToolTip,simpleDateFormat.format(new Date(storageIndexData.lastCheckedDateTime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Error")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,5,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.errorMessage);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,5,1,TableLayoutData.WE);

            Point size = widgetStorageTableToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
            Rectangle bounds = tableItem.getBounds(0);
            Point point = table.toDisplay(mouseEvent.x+16,bounds.y);
            widgetStorageTableToolTip.setBounds(point.x,point.y,size.x,size.y);
            widgetStorageTableToolTip.setVisible(true);
          }
        }
      });
      widgetStorageTable.addKeyListener(new KeyListener()
      {
        public void keyPressed(KeyEvent keyEvent)
        {
        }
        public void keyReleased(KeyEvent keyEvent)
        {
          if      (Widgets.isAccelerator(keyEvent,SWT.INSERT))
          {
            refreshStorageIndex();
          }
          else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
          {
            removeStorageIndex();
          }
        }
      });

      menu = Widgets.newPopupMenu(shell);
      {
        menuItem = Widgets.addMenuItem(menu,"Refresh index...");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            refreshStorageIndex();
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Refresh all indizes with error...");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            refreshAllWithErrorStorageIndex();
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,"Add to index...");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            addStorageIndex();
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Remove from index...");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            removeStorageIndex();
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Remove all indizes with error...");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            removeAllWithErrorStorageIndex();
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,"Mark all");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setCheckedStorage(true);
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Unmark all");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setCheckedStorage(false);
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,"Restore");
        Widgets.addEventListener(new WidgetEventListener(menuItem,checkedStorageEvent)
        {
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(isStorageChecked());
          }
        });
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            restoreArchives(getCheckedStorageNameHashSet(),
                            widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                            widgetOverwriteEntries.getSelection()
                           );
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,"Delete storage...");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            deleteStorage();
          }
        });
      }
      widgetStorageTree.setMenu(menu);
      widgetStorageTable.setMenu(menu);

      // storage list filter
      composite = Widgets.newComposite(group);
      composite.setLayout(new TableLayout(0.0,new double[]{0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(composite,2,0,TableLayoutData.WE);
      {
        button = Widgets.newButton(composite,IMAGE_MARK_ALL);
        Widgets.layout(button,0,0,TableLayoutData.W);
        Widgets.addEventListener(new WidgetEventListener(button,checkedStorageEvent)
        {
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (isStorageChecked())
            {
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText(BARControl.tr("Unmark all entries in list."));
            }
            else
            {
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText(BARControl.tr("Mark all entries in list."));
            }
          }
        });
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button button = (Button)selectionEvent.widget;
            if (isStorageChecked())
            {
              setCheckedStorage(false);
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText(BARControl.tr("Mark all entries in list."));
            }
            else
            {
              setCheckedStorage(true);
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText(BARControl.tr("Unmark all entries in list."));
            }
          }
        });

        label = Widgets.newLabel(composite,BARControl.tr("Filter")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        widgetStoragePattern = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
        widgetStoragePattern.setToolTipText(BARControl.tr("Enter filter pattern for storage list. Wildcards: * and ?."));
        widgetStoragePattern.setMessage(BARControl.tr("Enter text to filter storage list"));
        Widgets.layout(widgetStoragePattern,0,2,TableLayoutData.WE);
        widgetStoragePattern.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            setStoragePattern(widget.getText());
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            setStoragePattern(widget.getText());
          }
        });
        widgetStoragePattern.addKeyListener(new KeyListener()
        {
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          public void keyReleased(KeyEvent keyEvent)
          {
            Text widget = (Text)keyEvent.widget;
            setStoragePattern(widget.getText());
          }
        });
//???
        widgetStoragePattern.addFocusListener(new FocusListener()
        {
          public void focusGained(FocusEvent focusEvent)
          {
          }
          public void focusLost(FocusEvent focusEvent)
          {
//            Text widget = (Text)focusEvent.widget;
//            setStoragePattern(widget.getText());
          }
        });

        label = Widgets.newLabel(composite,BARControl.tr("State")+":");
        Widgets.layout(label,0,3,TableLayoutData.W);

        widgetStorageStateFilter = Widgets.newOptionMenu(composite);
        widgetStorageStateFilter.setToolTipText(BARControl.tr("Storage states filter."));
        widgetStorageStateFilter.setItems(new String[]{"*","ok","error","update","update requested","update/update requested","error/update/update requested"});
        widgetStorageStateFilter.setText("*");
        Widgets.layout(widgetStorageStateFilter,0,4,TableLayoutData.W);
        widgetStorageStateFilter.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo widget = (Combo)selectionEvent.widget;
            switch (widget.getSelectionIndex())
            {
              case 0:  storageIndexStateSet = INDEX_STATE_SET_ALL;                                                                  break;
              case 1:  storageIndexStateSet = new IndexStateSet(IndexStates.OK);                                                    break;
              case 2:  storageIndexStateSet = new IndexStateSet(IndexStates.ERROR);                                                 break;
              case 3:  storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE);                                                break;
              case 4:  storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE_REQUESTED);                                      break;
              case 5:  storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE,IndexStates.UPDATE_REQUESTED);                   break;
              case 6:  storageIndexStateSet = new IndexStateSet(IndexStates.ERROR,IndexStates.UPDATE,IndexStates.UPDATE_REQUESTED); break;
              default: storageIndexStateSet = new IndexStateSet(IndexStates.UNKNOWN);                                               break;

            }
            updateStorageThread.triggerUpdate(storageIndexStateSet);
          }
        });

        label = Widgets.newLabel(composite,BARControl.tr("Max")+":");
        Widgets.layout(label,0,5,TableLayoutData.W);

        widgetStorageMaxCount = Widgets.newOptionMenu(composite);
        widgetStorageMaxCount.setToolTipText(BARControl.tr("Max. number of entries in list."));
        widgetStorageMaxCount.setItems(new String[]{"10","50","100","500","1000"});
        widgetStorageMaxCount.setText("100");
        Widgets.layout(widgetStorageMaxCount,0,6,TableLayoutData.W);
        widgetStorageMaxCount.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo widget = (Combo)selectionEvent.widget;
            storageMaxCount = Integer.parseInt(widget.getText());
            updateStorageThread.triggerUpdate(storageMaxCount);
          }
        });

        button = Widgets.newButton(composite,BARControl.tr("Restore"));
        button.setToolTipText(BARControl.tr("Start restoring selected archives."));
        button.setEnabled(false);
        Widgets.layout(button,0,7,TableLayoutData.DEFAULT,0,0,0,0,60,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,checkedStorageEvent)
        {
          public void trigger(Control control)
          {
            control.setEnabled(isStorageChecked());
          }
        });
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            restoreArchives(getCheckedStorageNameHashSet(),
                            widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                            widgetOverwriteEntries.getSelection()
                           );
          }
        });
      }
    }

    // entries list
    group = Widgets.newGroup(widgetTab,BARControl.tr("Entries"));
    group.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,4));
    Widgets.layout(group,1,1,TableLayoutData.NSWE);
    {
      // fix layout
      control = Widgets.newSpacer(group);
      Widgets.layout(control,0,0,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,1);

      widgetEntryTable = Widgets.newTable(group,SWT.CHECK);
      widgetEntryTable.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(widgetEntryTable,1,0,TableLayoutData.NSWE);
      SelectionListener entryListColumnSelectionListener = new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableColumn         tableColumn         = (TableColumn)selectionEvent.widget;
          EntryDataComparator entryDataComparator = new EntryDataComparator(widgetEntryTable,tableColumn);
          synchronized(widgetEntryTable)
          {
            shell.setCursor(waitCursor);
            Widgets.sortTableColumn(widgetEntryTable,tableColumn,entryDataComparator);
            shell.setCursor(null);
          }
        }
      };
      tableColumn = Widgets.addTableColumn(widgetEntryTable,0,BARControl.tr("Archive"),SWT.LEFT, 200,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for archive name."));
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetEntryTable,1,BARControl.tr("Name"),   SWT.LEFT, 300,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetEntryTable,2,BARControl.tr("Type"),   SWT.LEFT,  60,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for type."));
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetEntryTable,3,BARControl.tr("Size"),   SWT.RIGHT, 60,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for size."));
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetEntryTable,4,BARControl.tr("Date"),   SWT.LEFT, 140,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for date."));
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      widgetEntryTable.addListener(SWT.MouseDoubleClick,new Listener()
      {
        public void handleEvent(final Event event)
        {
          TableItem tableItem = widgetEntryTable.getItem(new Point(event.x,event.y));
          if (tableItem != null)
          {
            tableItem.setChecked(!tableItem.getChecked());

            EntryData entryData = (EntryData)tableItem.getData();
            entryData.setChecked(tableItem.getChecked());

            checkedEntryEvent.trigger();
          }
        }
      });
      widgetEntryTable.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableItem tableItem = (TableItem)selectionEvent.item;
          if (tableItem != null)
          {
            EntryData entryData = (EntryData)tableItem.getData();
            entryData.setChecked(tableItem.getChecked());
          }

          checkedEntryEvent.trigger();
        }
      });
      widgetEntryTable.addMouseTrackListener(new MouseTrackListener()
      {
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }

        public void mouseExit(MouseEvent mouseEvent)
        {
        }

        public void mouseHover(MouseEvent mouseEvent)
        {
          Table     table     = (Table)mouseEvent.widget;
          TableItem tableItem = table.getItem(new Point(mouseEvent.x,mouseEvent.y));

          if (widgetEntryTableToolTip != null)
          {
            widgetEntryTableToolTip.dispose();
            widgetEntryTableToolTip = null;
          }

          if ((tableItem != null) && (mouseEvent.x < 64))
          {
            EntryData entryData = (EntryData)tableItem.getData();
            Label     label;
            Control   control;

            final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
            final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

            widgetEntryTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
            widgetEntryTableToolTip.setBackground(COLOR_BACKGROUND);
            widgetEntryTableToolTip.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
            Widgets.layout(widgetEntryTableToolTip,0,0,TableLayoutData.NSWE);
            widgetEntryTableToolTip.addMouseTrackListener(new MouseTrackListener()
            {
              public void mouseEnter(MouseEvent mouseEvent)
              {
              }

              public void mouseExit(MouseEvent mouseEvent)
              {
                widgetEntryTableToolTip.dispose();
                widgetEntryTableToolTip = null;
              }

              public void mouseHover(MouseEvent mouseEvent)
              {
              }
            });

            label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Storage")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,0,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryTableToolTip,entryData.storageName);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,0,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Created")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryTableToolTip,simpleDateFormat.format(new Date(entryData.storageDateTime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,1,TableLayoutData.WE);

            control = Widgets.newSpacer(widgetEntryTableToolTip);
            Widgets.layout(control,2,0,TableLayoutData.WE,0,2,0,0,SWT.DEFAULT,1,SWT.DEFAULT,1,SWT.DEFAULT,1);

            label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Type")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryTableToolTip,entryData.type.toString());
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Name")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryTableToolTip,entryData.name);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Size")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,5,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryTableToolTip,String.format(BARControl.tr("%d bytes (%s)"),entryData.size,Units.formatByteSize(entryData.size)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,5,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Date")+":");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,6,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryTableToolTip,simpleDateFormat.format(new Date(entryData.dateTime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,6,1,TableLayoutData.WE);

            Point size = widgetEntryTableToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
            Rectangle bounds = tableItem.getBounds(0);
            Point point = table.toDisplay(mouseEvent.x+16,bounds.y);
            widgetEntryTableToolTip.setBounds(point.x,point.y,size.x,size.y);
            widgetEntryTableToolTip.setVisible(true);
          }
        }
      });

      menu = Widgets.newPopupMenu(shell);
      {
        menuItem = Widgets.addMenuItem(menu,"Mark all");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setCheckedEntries(true);
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Unmark all");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setCheckedEntries(false);
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,"Restore");
        menuItem.setEnabled(false);
        Widgets.addEventListener(new WidgetEventListener(menuItem,checkedEntryEvent)
        {
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(checkEntriesChecked());
          }
        });
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            restoreEntries(getCheckedEntries(),
                           widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                           widgetOverwriteEntries.getSelection()
                          );
          }
        });
      }
      widgetEntryTable.setMenu(menu);

      // entry list filter
      composite = Widgets.newComposite(group);
      composite.setLayout(new TableLayout(null,new double[]{0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(composite,2,0,TableLayoutData.WE);
      {
        button = Widgets.newButton(composite,IMAGE_MARK_ALL);
        Widgets.layout(button,0,0,TableLayoutData.E);
        Widgets.addEventListener(new WidgetEventListener(button,checkedEntryEvent)
        {
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (checkEntriesChecked())
            {
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText(BARControl.tr("Unmark all entries in list."));
            }
            else
            {
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText(BARControl.tr("Mark all entries in list."));
            }
          }
        });
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button button = (Button)selectionEvent.widget;
            if (checkEntriesChecked())
            {
              setCheckedEntries(false);
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText(BARControl.tr("Mark all entries in list."));
            }
            else
            {
              setCheckedEntries(true);
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText(BARControl.tr("Unmark all entries in list."));
            }
          }
        });

        label = Widgets.newLabel(composite,BARControl.tr("Filter")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        text = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
        text.setToolTipText(BARControl.tr("Enter filter pattern for entry list. Wildcards: * and ?."));
        text.setMessage(BARControl.tr("Enter text to filter entry list"));
        Widgets.layout(text,0,2,TableLayoutData.WE);
        text.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text  widget = (Text)selectionEvent.widget;
            setEntryPattern(widget.getText());
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            setEntryPattern(widget.getText());
          }
        });
        text.addKeyListener(new KeyListener()
        {
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          public void keyReleased(KeyEvent keyEvent)
          {
            Text widget = (Text)keyEvent.widget;
            setEntryPattern(widget.getText());
          }
        });
//???
        text.addFocusListener(new FocusListener()
        {
          public void focusGained(FocusEvent focusEvent)
          {
          }
          public void focusLost(FocusEvent focusEvent)
          {
//            Text widget = (Text)focusEvent.widget;
//            setEntryPattern(widget.getText());
          }
        });

        button = Widgets.newCheckbox(composite,BARControl.tr("newest entries only"));
        button.setToolTipText(BARControl.tr("When this checkbox is enabled, only show newest entry instances and hide all older entry instances."));
        Widgets.layout(button,0,3,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            newestEntriesOnlyFlag = widget.getSelection();
            updateEntryListThread.triggerUpdate(checkedStorageOnlyFlag,entryPattern,newestEntriesOnlyFlag,entryMaxCount);
          }
        });

        label = Widgets.newLabel(composite,BARControl.tr("Max")+":");
        Widgets.layout(label,0,4,TableLayoutData.W);

        combo = Widgets.newOptionMenu(composite);
        combo.setToolTipText(BARControl.tr("Max. number of entries in list."));
        combo.setItems(new String[]{"10","50","100","500","1000"});
        combo.setText("100");
        Widgets.layout(combo,0,5,TableLayoutData.W);
        combo.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo widget = (Combo)selectionEvent.widget;
            entryMaxCount = Integer.parseInt(widget.getText());
            updateEntryListThread.triggerUpdate(checkedStorageOnlyFlag,entryPattern,newestEntriesOnlyFlag,entryMaxCount);
          }
        });

        button = Widgets.newButton(composite,BARControl.tr("Restore"));
        button.setToolTipText(BARControl.tr("Start restoring selected entries."));
        button.setEnabled(false);
        Widgets.layout(button,0,6,TableLayoutData.DEFAULT,0,0,0,0,60,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,checkedEntryEvent)
        {
          public void trigger(Control control)
          {
            control.setEnabled(checkEntriesChecked());
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
            restoreEntries(getCheckedEntries(),
                           widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                           widgetOverwriteEntries.getSelection()
                          );
          }
        });
      }
    }

    // destination
    group = Widgets.newGroup(widgetTab,BARControl.tr("Destination"));
    group.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0},4));
    Widgets.layout(group,2,0,TableLayoutData.WE,0,2);
    {
      // fix layout
      control = Widgets.newSpacer(group);
      Widgets.layout(control,0,0,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,1);

      widgetRestoreTo = Widgets.newCheckbox(group,BARControl.tr("to"));
      widgetRestoreTo.setToolTipText(BARControl.tr("Enable this checkbox and select a directory to restore entries to different location."));
      Widgets.layout(widgetRestoreTo,1,0,TableLayoutData.W);
      widgetRestoreTo.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button  widget      = (Button)selectionEvent.widget;
          boolean checkedFlag = widget.getSelection();
          widgetRestoreTo.setSelection(checkedFlag);
          selectRestoreToEvent.trigger();
        }
      });

      widgetRestoreToDirectory = Widgets.newText(group);
      widgetRestoreToDirectory.setEnabled(false);
      Widgets.layout(widgetRestoreToDirectory,1,1,TableLayoutData.WE);
      Widgets.addEventListener(new WidgetEventListener(widgetRestoreToDirectory,selectRestoreToEvent)
      {
        public void trigger(Control control)
        {
          control.setEnabled(widgetRestoreTo.getSelection());
        }
      });
      group.addMouseListener(new MouseListener()
      {
        public void mouseDoubleClick(final MouseEvent mouseEvent)
        {
        }

        public void mouseDown(final MouseEvent mouseEvent)
        {
          Rectangle bounds = widgetRestoreToDirectory.getBounds();

          if (bounds.contains(mouseEvent.x,mouseEvent.y))
          {
            widgetRestoreTo.setSelection(true);
            selectRestoreToEvent.trigger();
            Widgets.setFocus(widgetRestoreToDirectory);
          }
        }

        public void mouseUp(final MouseEvent mouseEvent)
        {
        }
      });

      button = Widgets.newButton(group,IMAGE_DIRECTORY);
      Widgets.layout(button,1,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          String pathName = Dialogs.directory(shell,
                                              "Select path",
                                              widgetRestoreTo.getText()
                                             );
          if (pathName != null)
          {
            widgetRestoreTo.setSelection(true);
            selectRestoreToEvent.trigger();
            widgetRestoreToDirectory.setText(pathName);
          }
        }
      });

      widgetOverwriteEntries = Widgets.newCheckbox(group,BARControl.tr("overwrite existing entries"));
      widgetOverwriteEntries.setToolTipText(BARControl.tr("Enable this checkbox when existing entries in destination should be overwritten."));
      Widgets.layout(widgetOverwriteEntries,1,3,TableLayoutData.W);
    }

    // start storage list update thread
    updateStorageThread = new UpdateStorageThread();
    updateStorageThread.start();

    // start entry list update thread
    updateEntryListThread = new UpdateEntryListThread();
    updateEntryListThread.start();
  }

  //-----------------------------------------------------------------------

  /** update list of checked storage entries
   */
  private void updateCheckedStorageList()
  {
    BARServer.executeCommand(StringParser.format("STORAGE_LIST_CLEAR"),0);
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          UUIDIndexData uuidIndexData = (UUIDIndexData)uuidTreeItem.getData();
          if (uuidIndexData.isChecked())
          {
            BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD uuid=%'S",uuidIndexData.uuid),0);
          }

          if (uuidTreeItem.getExpanded())
          {
            for (TreeItem jobTreeItem : uuidTreeItem.getItems())
            {
              JobIndexData jobIndexData = (JobIndexData)jobTreeItem.getData();
              if (jobIndexData.isChecked())
              {
                BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD jobId=%d",jobIndexData.jobId),0);
              }

              if (jobTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : jobTreeItem.getItems())
                {
                  StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
                  if (storageIndexData.isChecked())
                  {
                    BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD storageId=%d",storageIndexData.storageId),0);
                  }
                }
              }
            }
          }
        }
        break;
      case 1:
        for (TableItem tableItem : widgetStorageTable.getItems())
        {
          StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
              if (storageIndexData.isChecked())
          {
            BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD storageId=%d",storageIndexData.storageId),0);
          }
        }
        break;
    }
  }

  /** set/clear tagging of all storage entries
   * @param checked true for set checked, false for clear checked
   */
  private void setCheckedStorage(boolean checked)
  {
    // set checked
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          UUIDIndexData uuidIndexData = (UUIDIndexData)uuidTreeItem.getData();
          uuidIndexData.setChecked(checked);

          if (uuidTreeItem.getExpanded())
          {
            for (TreeItem jobTreeItem : uuidTreeItem.getItems())
            {
              JobIndexData jobIndexData = (JobIndexData)uuidTreeItem.getData();
              jobIndexData.setChecked(checked);

              if (jobTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : jobTreeItem.getItems())
                {
                  StorageIndexData storageIndexData = (StorageIndexData)uuidTreeItem.getData();
                  storageIndexData.setChecked(checked);
                }
              }
            }
          }
        }
        break;
      case 1:
        for (TableItem tableItem : widgetStorageTable.getItems())
        {
          StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
          storageIndexData.setChecked(checked);
        }
        break;
    }

    // update storage list
    updateCheckedStorageList();

    // trigger update checked
    checkedStorageEvent.trigger();
  }

  /** get checked storage names
   * @param storageNamesHashSet storage hash set to fill
   * @return checked storage name hash set
   */
  private HashSet<String> getCheckedStorageNameHashSet(HashSet<String> storageNamesHashSet)
  {
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        for (TreeItem treeItem : widgetStorageTree.getItems())
        {
          StorageIndexData storageIndexData = (StorageIndexData)treeItem.getData();
          if ((storageIndexData != null) && !treeItem.getGrayed() && treeItem.getChecked())
          {
            storageNamesHashSet.add(storageIndexData.name);
          }
        }
        break;
      case 1:
        for (TableItem tableItem : widgetStorageTable.getItems())
        {
          StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
          if ((storageIndexData != null) && !tableItem.getGrayed() && tableItem.getChecked())
          {
            storageNamesHashSet.add(storageIndexData.name);
          }
        }
        break;
    }

    return storageNamesHashSet;
  }

  /** get checked storage names
   * @return checked storage name hash set
   */
  private HashSet<String> getCheckedStorageNameHashSet()
  {
    return getCheckedStorageNameHashSet(new HashSet<String>());
  }

  /** get checked storage
   * @param storageIndexDataHashSet storage index data hash set to fill
   * @return checked storage hash set
   */
  private HashSet<IndexData> getCheckedIndexData(HashSet<IndexData> indexDataHashSet)
  {
    IndexData indexData;
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          indexData = (IndexData)uuidTreeItem.getData();
          if      (!uuidTreeItem.getGrayed() && indexData.isChecked())
          {
            indexDataHashSet.add(indexData);
          }
          else if (uuidTreeItem.getExpanded())
          {
            for (TreeItem jobTreeItem : uuidTreeItem.getItems())
            {
              indexData = (IndexData)jobTreeItem.getData();
              if      (!jobTreeItem.getGrayed() && indexData.isChecked())
              {
                indexDataHashSet.add(indexData);
              }
              else if (jobTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : jobTreeItem.getItems())
                {
                  indexData = (IndexData)storageTreeItem.getData();
                  if (!storageTreeItem.getGrayed() && indexData.isChecked())
                  {
                    indexDataHashSet.add(indexData);
                  }
                }
              }
            }
          }
        }
        break;
      case 1:
        for (TableItem tableItem : widgetStorageTable.getItems())
        {
          indexData = (StorageIndexData)tableItem.getData();
          if ((indexData != null) && !tableItem.getGrayed() && tableItem.getChecked())
          {
            indexDataHashSet.add(indexData);
          }
        }
        break;
    }

    return indexDataHashSet;
  }

  /** get checked storage
   * @return checked index data hash set
   */
  private HashSet<IndexData> getCheckedIndexData()
  {
    return getCheckedIndexData(new HashSet<IndexData>());
  }

  /** get selected storage
   * @param indexDataHashSet index data hash set to fill
   * @return selected storage hash set
   */
  private HashSet<IndexData> getSelectedIndexData(HashSet<IndexData> indexDataHashSet)
  {
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        IndexData indexData;
        for (TreeItem treeItem : widgetStorageTree.getSelection())
        {
          indexData = (IndexData)treeItem.getData();
Dprintf.dprintf("treeItem=%s: %s",treeItem,indexData);

          indexDataHashSet.add(indexData);

          if      (indexData instanceof UUIDIndexData)
          {
          }
          else if (indexData instanceof JobIndexData)
          {
          }
          else if (indexData instanceof StorageIndexData)
          {
          }
        }

/*
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          if (uuidTreeItem.getExpanded())
          {
            for (TreeItem jobTreeItem : uuidTreeItem.getItems())
            {
              if (jobTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : jobTreeItem.getItems())
                {
                  StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
                  if (!storageTreeItem.getGrayed())
                  {
                    indexDataHashSet.add(storageIndexData);
                  }
                }
              }
            }
          }
        }
*/
        break;
      case 1:
        for (TableItem tableItem : widgetStorageTable.getSelection())
        {
          StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
          if ((storageIndexData != null) && !tableItem.getGrayed())
          {
            indexDataHashSet.add(storageIndexData);
          }
        }
        break;
    }

    return indexDataHashSet;
  }

  /** get selected storage
   * @return selected index data hash set
   */
  private HashSet<IndexData> getSelectedIndexData()
  {
    return getSelectedIndexData(new HashSet<IndexData>());
  }

  /** check if some uuid/job/storage entries are checked
   * @return true iff some entry is checked
   */
  private boolean isStorageChecked()
  {
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        for (TreeItem treeItem : widgetStorageTree.getItems())
        {
          if (treeItem.getData() instanceof StorageIndexData)
          {
            IndexData indexData = (IndexData)treeItem.getData();
            if ((indexData != null) && !treeItem.getGrayed() && treeItem.getChecked())
            {
              return true;
            }
          }
        }
        break;
      case 1:
        for (TableItem tableItem : widgetStorageTable.getItems())
        {
          IndexData indexData = (IndexData)tableItem.getData();
          if ((indexData != null) && !tableItem.getGrayed() && tableItem.getChecked())
          {
            return true;
          }
        }
        break;
    }

    return false;
  }

  /** find index for insert of item in sorted storage data list
   * @param jobIndexData data of tree item
   * @return index in tree
   */
  private int findStorageTreeIndex(UUIDIndexData uuidIndexData)
  {
    TreeItem                treeItems[]             = widgetStorageTree.getItems();
    UUIDIndexDataComparator uuidIndexDataComparator = new UUIDIndexDataComparator(widgetStorageTree);

    int index = 0;
    while (   (index < treeItems.length)
           && (uuidIndexDataComparator.compare(uuidIndexData,(UUIDIndexData)treeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of item in sorted storage data list
   * @param storageIndexData data of tree item
   * @return index in tree
   */
  private int findStorageTreeIndex(TreeItem treeItem, StorageIndexData storageIndexData)
  {
    TreeItem                   treeItems[]           = treeItem.getItems();
    StorageIndexDataComparator storageIndexDataComparator = new StorageIndexDataComparator(widgetStorageTree);

    int index = 0;
    while (   (index < treeItems.length)
           && (storageIndexDataComparator.compare(storageIndexData,(StorageIndexData)treeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update storage tree item
   * @param treeItem tree item to update
   * @param storagePattern storage pattern or null
   */
  private void updateStorageTree(final TreeItem treeItem, String storagePattern)
  {
    shell.setCursor(waitCursor);

    try
    {
      String[] resultErrorMessage = new String[1];
      ValueMap resultMap          = new ValueMap();

      if      (treeItem.getData() instanceof UUIDIndexData)
      {
        // get job index data
        final HashSet<TreeItem> removeJobTreeItemSet = new HashSet<TreeItem>();
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem jobTreeItem : treeItem.getItems())
            {
              assert jobTreeItem.getData() instanceof JobIndexData;
              removeJobTreeItemSet.add(jobTreeItem);
            }
          }
        });

        // update job list
        UUIDIndexData uuidIndexData = (UUIDIndexData)treeItem.getData();
        Command command = BARServer.runCommand(StringParser.format("INDEX_JOB_LIST uuid=%'S pattern=%'S",
                                                                   uuidIndexData.uuid,
                                                                   (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                                  ),
                                               0
                                              );
        while (!command.endOfData())
        {
          if (command.getNextResult(resultErrorMessage,
                                    resultMap,
                                    Command.TIMEOUT
                                   ) == Errors.NONE
             )
          {
            try
            {
              long         jobId            = resultMap.getLong  ("jobId"                  );
              String       uuid             = resultMap.getString("uuid"                   );
              ArchiveTypes archiveType      = resultMap.getEnum  ("type",ArchiveTypes.class);
              long         lastDateTime     = resultMap.getLong  ("lastDateTime"           );
              long         totalSize        = resultMap.getLong  ("totalSize"              );
              String       lastErrorMessage = resultMap.getString("lastErrorMessage"       );

              // add/update job data index
              final JobIndexData jobIndexData = indexDataMap.updateJobIndexData(jobId,
                                                                                archiveType,
                                                                                lastDateTime,
                                                                                totalSize,
                                                                                lastErrorMessage
                                                                               );

              // insert/update tree item
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  TreeItem jobTreeItem = Widgets.getTreeItem(widgetStorageTree,jobIndexData);
                  if (jobTreeItem == null)
                  {
                    // insert tree item
                    jobTreeItem = Widgets.insertTreeItem(treeItem,
                                                         findStorageTreeIndex(treeItem,jobIndexData),
                                                         (Object)jobIndexData,
                                                         true
                                                        );
                    jobIndexData.setTreeItem(jobTreeItem);
                  }
                  else
                  {
                    assert jobTreeItem.getData() instanceof JobIndexData;

                    // keep tree item
                    removeJobTreeItemSet.remove(jobTreeItem);
                  }

                  // update view
                  jobIndexData.update();
                }
              });
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

        // remove not existing entries
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem treeItem : removeJobTreeItemSet)
            {
              IndexData indexData = (IndexData)treeItem.getData();
              Widgets.removeTreeItem(widgetStorageTree,treeItem);
              indexData.clearTreeItem();
            }
          }
        });
      }
      else if (treeItem.getData() instanceof JobIndexData)
      {
        // get job index data
        final HashSet<TreeItem> removeStorageTreeItemSet = new HashSet<TreeItem>();
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem storageTreeItem : treeItem.getItems())
            {
              assert treeItem.getData() instanceof StorageIndexData;
              removeStorageTreeItemSet.add(storageTreeItem);
            }
          }
        });


        // update storage list
        JobIndexData jobIndexData = (JobIndexData)treeItem.getData();
        Command command = BARServer.runCommand(StringParser.format("INDEX_STORAGE_LIST jobId=%d maxCount=%d indexState=%s indexMode=%s pattern=%'S",
                                                                   jobIndexData.jobId,
                                                                   -1,
                                                                   "*",
                                                                   "*",
                                                                   (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                                  ),
                                               0
                                              );
        while (!command.endOfData())
        {
          if (command.getNextResult(resultErrorMessage,
                                    resultMap,
                                    Command.TIMEOUT
                                   ) == Errors.NONE
             )
          {
            try
            {
              long        storageId           = resultMap.getLong  ("storageId"                   );
              String      uuid                = resultMap.getString("uuid"                        );
              long        jobId               = resultMap.getLong  ("jobId"                       );
              String      name                = resultMap.getString("name"                        );
              long        dateTime            = resultMap.getLong  ("dateTime"                    );
              long        size                = resultMap.getLong  ("size"                        );
              IndexStates indexState          = resultMap.getEnum  ("indexState",IndexStates.class);
              IndexModes  indexMode           = resultMap.getEnum  ("indexMode",IndexModes.class  );
              long        lastCheckedDateTime = resultMap.getLong  ("lastCheckedDateTime"         );
              String      errorMessage        = resultMap.getString("errorMessage"                );

              // add/update storage data
              final StorageIndexData storageIndexData = indexDataMap.updateStorageIndexData(storageId,
                                                                                            jobId,
                                                                                            name,
                                                                                            dateTime,
                                                                                            size,
                                                                                            new File(name).getName(),
                                                                                            indexState,
                                                                                            indexMode,
                                                                                            lastCheckedDateTime,
                                                                                            errorMessage
                                                                                           );

              // insert/update tree item
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  TreeItem storageTreeItem = Widgets.getTreeItem(widgetStorageTree,storageIndexData);
                  if (storageTreeItem == null)
                  {
                    // insert tree item
                    storageTreeItem = Widgets.insertTreeItem(treeItem,
                                                             findStorageTreeIndex(treeItem,storageIndexData),
                                                             (Object)storageIndexData,
                                                             false
                                                            );
                    storageIndexData.setTreeItem(storageTreeItem);
                  }
                  else
                  {
                    // keep tree item
                    removeStorageTreeItemSet.remove(storageTreeItem);
                  }

                  // update view
                  storageIndexData.update();
                }
              });
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

        // remove not existing entries
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem treeItem : removeStorageTreeItemSet)
            {
              IndexData indexData = (IndexData)treeItem.getData();
              Widgets.removeTreeItem(widgetStorageTree,treeItem);
              indexData.clearTreeItem();
            }
          }
        });
      }
    }
    catch (CommunicationError error)
    {
      // ignored
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

    shell.setCursor(null);
  }

  /** set storage filter pattern
   * @param string pattern string
   */
  private void setStoragePattern(String string)
  {
    string = string.trim();
    if (string.length() > 0)
    {
//      if ((storagePattern == null) || !storagePattern.equals(string))
//      {
//        storagePattern = string;
        updateStorageThread.triggerUpdate(string,storageIndexStateSet,storageMaxCount);
//      }
    }
    else
    {
//      if (storagePattern != null)
//      {
//        storagePattern = null;
        updateStorageThread.triggerUpdate(null,storageIndexStateSet,storageMaxCount);
//      }
    }
  }

  /** delete storage
   */
  private void deleteStorage()
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    getCheckedIndexData(indexDataHashSet);
    getSelectedIndexData(indexDataHashSet);
    if (!indexDataHashSet.isEmpty())
    {
      if (Dialogs.confirm(shell,"Really delete "+indexDataHashSet.size()+" storage files?"))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,"Delete storage files",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
        busyDialog.setMaximum(indexDataHashSet.size());

        new BackgroundTask(busyDialog,new Object[]{indexDataHashSet})
        {
          public void run(final BusyDialog busyDialog, Object userData)
          {
            HashSet<IndexData> indexDataHashSet = (HashSet<IndexData>)((Object[])userData)[0];

            try
            {
              boolean ignoreAllErrorsFlag = false;
              boolean abortFlag           = false;
              long    n                   = 0;
              for (IndexData indexData : indexDataHashSet)
              {
                // get index info
                final String info = indexData.getInfo();

                // update busy dialog
                busyDialog.updateText(info);

                // delete storage
                final String[] resultErrorMessage = new String[1];
                int errorCode = Errors.UNKNOWN;
                if      (indexData instanceof UUIDIndexData)
                {
                  errorCode = BARServer.executeCommand(StringParser.format("STORAGE_DELETE uuid=%'S",
                                                                           ((UUIDIndexData)indexData).uuid
                                                                          ),
                                                       0,
                                                       resultErrorMessage
                                                      );
                }
                else if (indexData instanceof JobIndexData)
                {
                  errorCode = BARServer.executeCommand(StringParser.format("STORAGE_DELETE jobId=%d",
                                                                           ((JobIndexData)indexData).jobId
                                                                          ),
                                                       0,
                                                       resultErrorMessage
                                                      );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  errorCode = BARServer.executeCommand(StringParser.format("STORAGE_DELETE storageId=%d",
                                                                           ((StorageIndexData)indexData).storageId
                                                                          ),
                                                       0,
                                                       resultErrorMessage
                                                      );
                }
                if (errorCode == Errors.NONE)
                {
                  indexDataMap.remove(indexData);
                  Widgets.removeTreeItem(widgetStorageTree,indexData);
                  Widgets.removeTableItem(widgetStorageTable,indexData);
                }
                else
                {
                  if (!ignoreAllErrorsFlag)
                  {
                    final int[] selection = new int[1];
                    if (indexDataHashSet.size() > (n+1))
                    {
                      display.syncExec(new Runnable()
                      {
                        public void run()
                        {
                          selection[0] = Dialogs.select(shell,
                                                        "Confirmation",
                                                        "Cannot delete storage\n\n'"+info+"'\n\n(error: "+resultErrorMessage[0]+")",
                                                        new String[]{"Continue","Continue with all","Abort"},
                                                        0
                                                       );
                        }
                      });
                    }
                    else
                    {
                      display.syncExec(new Runnable()
                      {
                        public void run()
                        {
                          Dialogs.error(shell,
                                        "Cannot delete storage\n\n'"+info+"'\n\n(error: "+resultErrorMessage[0]+")"
                                       );
                        }
                      });
                    }
                    switch (selection[0])
                    {
                      case 0:
                        break;
                      case 1:
                        ignoreAllErrorsFlag = true;
                        break;
                      case 2:
                        abortFlag = true;
                        break;
                      default:
                        break;
                    }
                  }
                }

                // update progress bar
                n++;
                busyDialog.updateProgressBar(n);

                // check for abort
                if (abortFlag || busyDialog.isAborted()) break;
              }

              // close busy dialog
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  busyDialog.close();
                }
              });

              updateStorageThread.triggerUpdate();
            }
            catch (CommunicationError error)
            {
              final String errorMessage = error.getMessage();
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,"Communication error while deleting storage (error: "+errorMessage+")");
                 }
              });
            }
            catch (Exception exception)
            {
              BARServer.disconnect();
              System.err.println("ERROR: "+exception.getMessage());
              BARControl.printStackTrace(exception);
              System.exit(1);
            }
          }
        };
      }
    }
  }

  /** add storage to index database
   */
  private void addStorageIndex()
  {
    Label      label;
    Composite  composite;
    Button     button;
    final Text widgetStorageName;
    Button     widgetAdd;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,"Add storage to index database",400,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Storage name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetStorageName = Widgets.newText(composite);
      widgetStorageName.setToolTipText(BARControl.tr("Enter local or remote storage name."));
      Widgets.layout(widgetStorageName,0,1,TableLayoutData.WE);

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
      button.setToolTipText(BARControl.tr("Select local storage file."));
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget   = (Button)selectionEvent.widget;
          String fileName = Dialogs.fileOpen(shell,
                                             "Select local storage file",
                                             widgetStorageName.getText(),
                                             new String[]{"BAR files","*.bar",
                                                          "All files","*",
                                                         }
                                            );
          if (fileName != null)
          {
            widgetStorageName.setText(fileName);
          }
        }
      });
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
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
          Dialogs.close(dialog,null);
        }
      });
    }

    // add selection listeners
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        Dialogs.close(dialog,widgetStorageName.getText());
      }
    });

    // run dialog
    String storageName = (String)Dialogs.run(dialog,null);

    // add storage file
    if (storageName != null)
    {
      String[] resultErrorMessage = new String[1];
      int errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ADD name=%S",storageName),
                                               0,
                                               resultErrorMessage
                                              );
      if (errorCode == Errors.NONE)
      {
        updateStorageThread.triggerUpdate();
      }
      else
      {
        Dialogs.error(shell,"Cannot add index database for storage file\n\n'"+storageName+"'\n\n(error: "+resultErrorMessage[0]+")");
      }
    }
  }

  /** remove storage from index database
   */
  private void removeStorageIndex()
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    getCheckedIndexData(indexDataHashSet);
    getSelectedIndexData(indexDataHashSet);
    if (!indexDataHashSet.isEmpty())
    {
      if (Dialogs.confirm(shell,"Really remove index of "+indexDataHashSet.size()+" entries?"))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,"Remove indizes",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
        busyDialog.setMaximum(indexDataHashSet.size());

        new BackgroundTask(busyDialog,new Object[]{indexDataHashSet})
        {
          public void run(final BusyDialog busyDialog, Object userData)
          {
            HashSet<IndexData> indexDataHashSet = (HashSet<IndexData>)((Object[])userData)[0];

            try
            {
              long n = 0;
              for (IndexData indexData : indexDataHashSet)
              {
                // get index info
                final String info = indexData.getInfo();

                // update busy dialog
                busyDialog.updateText(info);

                // remove entry
                final String[] resultErrorMessage = new String[1];
                int errorCode = Errors.UNKNOWN;
                if      (indexData instanceof UUIDIndexData)
                {
                  errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REMOVE state=* uuid=%'S",
                                                                           ((UUIDIndexData)indexData).uuid
                                                                          ),
                                                       0,
                                                       resultErrorMessage
                                                      );
                }
                else if (indexData instanceof JobIndexData)
                {
                  errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REMOVE state=* jobId=%d",
                                                                           ((JobIndexData)indexData).jobId
                                                                          ),
                                                       0,
                                                       resultErrorMessage
                                                      );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REMOVE state=* storageId=%d",
                                                                           ((StorageIndexData)indexData).storageId
                                                                          ),
                                                       0,
                                                       resultErrorMessage
                                                      );
                }
                if (errorCode == Errors.NONE)
                {
                  indexDataMap.remove(indexData);
                  Widgets.removeTreeItem(widgetStorageTree,indexData);
                  Widgets.removeTableItem(widgetStorageTable,indexData);
                }
                else
                {
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      Dialogs.error(shell,"Cannot remove index for\n\n'"+info+"'\n\n(error: "+resultErrorMessage[0]+")");
                    }
                  });
                }

                // update progress bar
                n++;
                busyDialog.updateProgressBar(n);

                // check for abort
                if (busyDialog.isAborted()) break;
              }

              // close busy dialog
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  busyDialog.close();
                }
              });

              updateStorageThread.triggerUpdate();
            }
            catch (CommunicationError error)
            {
              final String errorMessage = error.getMessage();
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,"Communication error while removing database indizes (error: "+errorMessage+")");
                 }
              });
            }
            catch (Exception exception)
            {
              BARServer.disconnect();
              System.err.println("ERROR: "+exception.getMessage());
              BARControl.printStackTrace(exception);
              System.exit(1);
            }
          }
        };
      }
    }
  }

  /** remove all storage from index database with error
   */
  private void removeAllWithErrorStorageIndex()
  {
    try
    {
      // get number of indizes with error state
      final String[] resultErrorMessage = new String[1];
      ValueMap       resultMap          = new ValueMap();
      if (BARServer.executeCommand("INDEX_STORAGE_INFO",
                                   0,
                                   resultErrorMessage,
                                   resultMap
                                  ) != Errors.NONE
         )
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            Dialogs.error(shell,"Cannot get database indizes with error state (error: "+resultErrorMessage[0]+")");
          }
        });
        return;
      }
      long errorCount = resultMap.getLong("errorCount");

      if (errorCount > 0)
      {
        if (Dialogs.confirm(shell,String.format("Really remove %d indizes with error state?",errorCount)))
        {
          final BusyDialog busyDialog = new BusyDialog(shell,"Remove indizes with error",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
          busyDialog.autoAnimate();
          busyDialog.setMaximum(errorCount);

          new BackgroundTask(busyDialog)
          {
            public void run(final BusyDialog busyDialog, Object userData)
            {
              try
              {
                final String[] resultErrorMessage = new String[1];
                ValueMap       resultMap          = new ValueMap();

                // remove indizes with error state
                Command command = BARServer.runCommand("INDEX_STORAGE_REMOVE state=ERROR storageId=0",0);

                long n = 0;
                while (!command.endOfData() && !busyDialog.isAborted())
                {
                  if (command.getNextResult(resultErrorMessage,
                                            resultMap,
                                            Command.TIMEOUT
                                           ) == Errors.NONE
                     )
                  {
                    try
                    {
                      long        storageId           = resultMap.getLong  ("storageId"          );
                      String      name                = resultMap.getString("name"               );

                      busyDialog.updateText(String.format("%d: %s",storageId,name));

                      n++;
                      busyDialog.updateProgressBar(n);
                    }
                    catch (IllegalArgumentException exception)
                    {
                      if (Settings.debugLevel > 0)
                      {
                        System.err.println("ERROR: "+exception.getMessage());
                      }
                    }
                  }
                  else
                  {
                    display.syncExec(new Runnable()
                    {
                      public void run()
                      {
                        busyDialog.close();
                        Dialogs.error(shell,"Cannot remove database indizes with error state (error: "+resultErrorMessage[0]+")");
                      }
                    });

                    updateStorageThread.triggerUpdate();

                    return;
                  }
                }
                if (busyDialog.isAborted())
                {
                  command.abort();
                }

                // close busy dialog
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    busyDialog.close();
                  }
                });

                updateStorageThread.triggerUpdate();
              }
              catch (CommunicationError error)
              {
                final String errorMessage = error.getMessage();
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    busyDialog.close();
                    Dialogs.error(shell,"Communication error while removing database indizes (error: "+errorMessage+")");
                   }
                });
              }
              catch (Exception exception)
              {
                BARServer.disconnect();
                System.err.println("ERROR: "+exception.getMessage());
                BARControl.printStackTrace(exception);
                System.exit(1);
              }
            }
          };
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,"Communication error while removing database indizes (error: "+error.toString()+")");
    }
Dprintf.dprintf("");
  }

  /** refresh storage from index database
   */
  private void refreshStorageIndex()
  {
    try
    {
      HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

      getCheckedIndexData(indexDataHashSet);
      getSelectedIndexData(indexDataHashSet);
      if (!indexDataHashSet.isEmpty())
      {
        if (Dialogs.confirm(shell,"Really refresh index for "+indexDataHashSet.size()+" entries?"))
        {
          for (IndexData indexData : indexDataHashSet)
          {
            final String info = indexData.getInfo();

            String[] resultErrorMessage = new String[1];
            int errorCode = Errors.UNKNOWN;
            if      (indexData instanceof UUIDIndexData)
            {
              errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s uuid=%'S",
                                                                       "*",
                                                                       ((UUIDIndexData)indexData).uuid
                                                                      ),
                                                   0,
                                                   resultErrorMessage
                                                  );
            }
            else if (indexData instanceof JobIndexData)
            {
              errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s jobId=%d",
                                                                       "*",
                                                                       ((JobIndexData)indexData).jobId
                                                                      ),
                                                   0,
                                                   resultErrorMessage
                                                  );
            }
            else if (indexData instanceof StorageIndexData)
            {
              errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s storageId=%d",
                                                                       "*",
                                                                       ((StorageIndexData)indexData).storageId
                                                                      ),
                                                   0,
                                                   resultErrorMessage
                                                  );
            }
            if (errorCode == Errors.NONE)
            {
              indexData.setState(IndexStates.UPDATE_REQUESTED);
            }
            else
            {
              Dialogs.error(shell,"Cannot refresh index for\n\n'"+info+"'\n\n(error: "+resultErrorMessage[0]+")");
            }
          }
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,"Communication error while refreshing index database (error: "+error.toString()+")");
    }
  }

  /** refresh all storage from index database with error
   */
  private void refreshAllWithErrorStorageIndex()
  {
    try
    {
      if (Dialogs.confirm(shell,"Really refresh all indizes with error state?"))
      {
        String[] resultErrorMessage = new String[1];
        int errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s storageId=%d",
                                                                     "ERROR",
                                                                     0
                                                                    ),
                                                 0,
                                                 resultErrorMessage
                                                );
        if (errorCode == Errors.NONE)
        {
          updateStorageThread.triggerUpdate();
        }
        else
        {
          Dialogs.error(shell,"Cannot refresh database indizes with error state (error: "+resultErrorMessage[0]+")");
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,"Communication error while refreshing database indizes (error: "+error.toString()+")");
    }
  }

  /** restore archives
   * @param storageNamesHashSet storage name hash set
   * @param directory destination directory or ""
   * @param overwriteEntries true to overwrite existing entries
   */
  private void restoreArchives(HashSet<String> storageNamesHashSet, String directory, boolean overwriteEntries)
  {
    shell.setCursor(waitCursor);

    final BusyDialog busyDialog = new BusyDialog(shell,"Restore archives",500,100,null,BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.PROGRESS_BAR1);

    new BackgroundTask(busyDialog,new Object[]{storageNamesHashSet,directory,overwriteEntries})
    {
      public void run(final BusyDialog busyDialog, Object userData)
      {
        final HashSet<String> storageNamesHashSet = (HashSet<String>)((Object[])userData)[0];
        final String          directory           = (String         )((Object[])userData)[1];
        final boolean         overwriteEntries    = (Boolean        )((Object[])userData)[2];

        int errorCode;

        // restore entries
        try
        {
          for (final String storageName : storageNamesHashSet)
          {
            if (!directory.equals(""))
            {
              busyDialog.updateText(0,"'"+storageName+"' into '"+directory+"'");
            }
            else
            {
              busyDialog.updateText(0,"'"+storageName+"'");
            }

            Command command;
            boolean retryFlag;
            boolean passwordFlag = false;
            do
            {
              retryFlag = false;

              // start restore
              command = BARServer.runCommand(StringParser.format("RESTORE storageName=%'S destination=%'S overwriteFilesFlag=%y",
                                                                 storageName,
                                                                 directory,
                                                                 overwriteEntries
                                                                ),
                                             0
                                            );

              // read results, update/add data
              String[] resultErrorMessage = new String[1];
              ValueMap resultMap          = new ValueMap();
              while (   !command.endOfData()
                     && !busyDialog.isAborted()
                    )
              {
                if (command.getNextResult(resultErrorMessage,
                                          resultMap,
                                          60*1000
                                         ) == Errors.NONE
                   )
                {
                  try
                  {
                    String name              = resultMap.getString("name"            );
                    long   entryDoneBytes    = resultMap.getLong  ("entryDoneBytes"  );
                    long   entryTotalBytes   = resultMap.getLong  ("entryTotalBytes" );
  //                  long   storageDoneBytes  = resultMap.getLong("storageDoneBytes" );
  //                  long   storageTotalBytes = resultMap.getLong("storageTotalBytes");

                    busyDialog.updateText(1,name);
                    busyDialog.updateProgressBar(1,(entryTotalBytes > 0) ? ((double)entryDoneBytes*100.0)/(double)entryTotalBytes : 0.0);
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
//Dprintf.dprintf("command=%s",command);

              if (   (   (command.getErrorCode() == Errors.NO_CRYPT_PASSWORD)
                      || (command.getErrorCode() == Errors.INVALID_CRYPT_PASSWORD)
                      || (command.getErrorCode() == Errors.CORRUPT_DATA)
                     )
                  && !passwordFlag
                  && !busyDialog.isAborted()
                 )
              {
                // get crypt password
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    String password = Dialogs.password(shell,
                                                       BARControl.tr("Decrypt password"),
                                                       BARControl.tr("Password")+":"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("DECRYPT_PASSWORD_ADD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  ),
                                               0
                                              );
                    }
                  }
                });
                passwordFlag = true;

                // retry
                retryFlag = true;
              }
            }
            while (retryFlag && !busyDialog.isAborted());

            // abort command if requested
            if (!busyDialog.isAborted())
            {
              if (command.getErrorCode() != Errors.NONE)
              {
                final String errorText = command.getErrorText();
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    Dialogs.error(shell,"Cannot restore archive\n\n'"+storageName+"'\n\n(error: "+errorText+")");
                  }
                });
              }
            }
            else
            {
              busyDialog.updateText("Aborting\u2026");
              command.abort();
              break;
            }
          }

          // close busy dialog, restore cursor
          display.syncExec(new Runnable()
          {
            public void run()
            {
              busyDialog.close();
              shell.setCursor(null);
            }
          });
        }
        catch (CommunicationError error)
        {
          final String errorMessage = error.getMessage();
          display.syncExec(new Runnable()
          {
            public void run()
            {
              busyDialog.close();
              shell.setCursor(null);
              Dialogs.error(shell,"Error while restoring archives:\n\n%s",errorMessage);
             }
          });
        }
        catch (Exception exception)
        {
          BARServer.disconnect();
          System.err.println("ERROR: "+exception.getMessage());
          BARControl.printStackTrace(exception);
          System.exit(1);
        }
      }
    };
  }

  //-----------------------------------------------------------------------

  /** set/clear tagging of all entries
   * @param checked true for set checked, false for clear checked
   */
  private void setCheckedEntries(boolean checked)
  {
    for (TableItem tableItem : widgetEntryTable.getItems())
    {
      tableItem.setChecked(checked);

      EntryData entryData = (EntryData)tableItem.getData();
      entryData.setChecked(checked);
    }

    checkedEntryEvent.trigger();
  }

  /** get checked entries
   * @return checked entries data
   */
  private EntryData[] getCheckedEntries()
  {
    ArrayList<EntryData> entryDataArray = new ArrayList<EntryData>();

    synchronized(entryDataMap)
    {
      for (EntryData entryData : entryDataMap.values())
      {
        if (entryData.isChecked()) entryDataArray.add(entryData);
      }
    }

    return entryDataArray.toArray(new EntryData[entryDataArray.size()]);
  }

  /** check if some entry is checked
   * @return tree iff some entry is checked
   */
  private boolean checkEntriesChecked()
  {
    synchronized(entryDataMap)
    {
      for (EntryData entryData : entryDataMap.values())
      {
        if (entryData.isChecked()) return true;
      }
    }

    return false;
  }

  /** find index for insert of item in sorted entry data list
   * @param entryData data of tree item
   * @return index in table
   */
  private int findEntryListIndex(EntryData entryData)
  {
    TableItem           tableItems[]        = widgetEntryTable.getItems();
    EntryDataComparator entryDataComparator = new EntryDataComparator(widgetEntryTable);

    int index = 0;
    while (   (index < tableItems.length)
           && (entryDataComparator.compare(entryData,(EntryData)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** refresh entry list display
   */
  private void refreshEntryList()
  {
// ??? statt findFileListIndex
    EntryDataComparator entryDataComparator = new EntryDataComparator(widgetEntryTable);

    // update
    widgetEntryTable.removeAll();
    synchronized(entryDataMap)
    {
      for (EntryData entryData : entryDataMap.values())
      {
        switch (entryData.type)
        {
          case FILE:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryData),
                                    (Object)entryData,
                                    entryData.storageName,
                                    entryData.name,
                                    "FILE",
                                    Units.formatByteSize(entryData.size),
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000))
                                   );
            break;
          case IMAGE:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryData),
                                    (Object)entryData,
                                    entryData.storageName,
                                    entryData.name,
                                    "IMAGE",
                                    Units.formatByteSize(entryData.size),
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000))
                                   );
            break;
          case DIRECTORY:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryData),
                                    (Object)entryData,
                                    entryData.storageName,
                                    entryData.name,
                                    "DIR",
                                    "",
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000))
                                   );
            break;
          case LINK:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryData),
                                    (Object)entryData,
                                    entryData.storageName,
                                    entryData.name,
                                    "LINK",
                                    "",
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000))
                                   );
            break;
          case SPECIAL:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryData),
                                    (Object)entryData,
                                    entryData.storageName,
                                    entryData.name,
                                    "SPECIAL",
                                    Units.formatByteSize(entryData.size),
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000))
                                   );
            break;
          case DEVICE:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryData),
                                    (Object)entryData,
                                    entryData.storageName,
                                    entryData.name,
                                    "DEVICE",
                                    Units.formatByteSize(entryData.size),
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000))
                                   );
            break;
          case SOCKET:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryData),
                                    (Object)entryData,
                                    entryData.storageName,
                                    entryData.name,
                                    "SOCKET",
                                    "",
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000))
                                   );
            break;
        }

        Widgets.setTableItemChecked(widgetEntryTable,
                                    (Object)entryData,
                                    entryData.isChecked()
                                   );
      }
    }

    // enable/disable restore button
    checkedEntryEvent.trigger();
  }

  /** set entry pattern
   * @param string pattern string
   */
  private void setEntryPattern(String string)
  {
    string = string.trim();
    if (string.length() > 0)
    {
      if ((entryPattern == null) || !entryPattern.equals(string))
      {
        entryPattern = string.trim();
        updateEntryListThread.triggerUpdate(checkedStorageOnlyFlag,entryPattern,newestEntriesOnlyFlag,entryMaxCount);
      }
    }
    else
    {
      if (entryPattern != null)
      {
        entryPattern = null;
        updateEntryListThread.triggerUpdate(checkedStorageOnlyFlag,entryPattern,newestEntriesOnlyFlag,entryMaxCount);
      }
    }
  }

  /** restore entries
   * @param entryData entries to restore
   * @param directory destination directory or ""
   * @param overwriteEntries true to overwrite existing entries
   */
  private void restoreEntries(EntryData entryData[], String directory, boolean overwriteEntries)
  {
    shell.setCursor(waitCursor);

    final BusyDialog busyDialog = new BusyDialog(shell,"Restore entries",500,100,null,BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.PROGRESS_BAR1);

    new BackgroundTask(busyDialog,new Object[]{entryData,directory,overwriteEntries})
    {
      public void run(final BusyDialog busyDialog, Object userData)
      {
        final String[] MAP_FROM = new String[]{"\n","\r","\\"};
        final String[] MAP_TO   = new String[]{"\\n","\\r","\\\\"};

        final EntryData[] entryData_       = (EntryData[])((Object[])userData)[0];
        final String      directory        = (String     )((Object[])userData)[1];
        final boolean     overwriteEntries = (Boolean    )((Object[])userData)[2];

        int errorCode;

        // restore entries
        try
        {
          for (final EntryData entryData : entryData_)
          {
            if (!directory.isEmpty())
            {
              busyDialog.updateText(0,"'"+entryData.name+"' into '"+directory+"'");
            }
            else
            {
              busyDialog.updateText(0,"'"+entryData.name+"'");
            }

            Command command;
            boolean retryFlag;
            boolean ftpPasswordFlag     = false;
            boolean sshPasswordFlag     = false;
            boolean webdavPasswordFlag  = false;
            boolean decryptPasswordFlag = false;
            do
            {
              retryFlag = false;

              // start restore
              command = BARServer.runCommand(StringParser.format("RESTORE storageName=%'S destination=%'S overwriteFilesFlag=%y name=%'S",
                                                                 entryData.storageName,
                                                                 directory,
                                                                 overwriteEntries,
                                                                 entryData.name
                                                                ),
                                             0
                                            );

              // read results, update/add data
              String[] resultErrorMessage = new String[1];
              ValueMap resultMap          = new ValueMap();
              while (   !command.endOfData()
                     && !busyDialog.isAborted()
                    )
              {
                if (command.getNextResult(resultErrorMessage,
                                          resultMap,
                                          60*1000
                                         ) == Errors.NONE
                   )
                {
                  try
                  {
                    String name              = resultMap.getString("name"            );
                    long   entryDoneBytes    = resultMap.getLong("entryDoneBytes"    );
                    long   entryTotalBytes   = resultMap.getLong("entryTotalBytes"   );
  //                  long   storageDoneBytes  = resultMap.getLong("storageDoneBytes" );
  //                  long   storageTotalBytes = resultMap.getLong("storageTotalBytes");

                    busyDialog.updateText(1,name);
                    busyDialog.updateProgressBar(1,(entryTotalBytes > 0) ? ((double)entryDoneBytes*100.0)/(double)entryTotalBytes : 0.0);
                  }
                  catch (IllegalArgumentException exception)
                  {
                    if (Settings.debugLevel > 0)
                    {
                      System.err.println("ERROR: "+exception.getMessage());
                    }
                  }
                }
                else
                {
                  busyDialog.update();
                }
              }

              if      (   (   (command.getErrorCode() == Errors.FTP_SESSION_FAIL)
                           || (command.getErrorCode() == Errors.NO_FTP_PASSWORD)
                           || (command.getErrorCode() == Errors.INVALID_FTP_PASSWORD)
                          )
                       && !ftpPasswordFlag
                       && !busyDialog.isAborted()
                      )
              {
                // get ftp password
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    String password = Dialogs.password(shell,
                                                       BARControl.tr("FTP login password"),
                                                       BARControl.tr("Please enter FTP login password for")+": "+entryData.storageName+".",
                                                       BARControl.tr("Password")+":"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("FTP_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  ),
                                               0
                                              );
                    }
                  }
                });
                ftpPasswordFlag = true;

                // retry
                retryFlag = true;
              }
              else if (   (   (command.getErrorCode() == Errors.SSH_SESSION_FAIL)
                           || (command.getErrorCode() == Errors.NO_SSH_PASSWORD)
                           || (command.getErrorCode() == Errors.INVALID_SSH_PASSWORD)
                          )
                       && !sshPasswordFlag
                       && !busyDialog.isAborted()
                      )
              {
                // get ssh password
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    String password = Dialogs.password(shell,
                                                       BARControl.tr("SSH (TLS) login password"),
                                                       BARControl.tr("Please enter SSH (TLS) login password for")+": "+entryData.storageName+".",
                                                       BARControl.tr("Password")+":"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("SSH_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  ),
                                               0
                                              );
                    }
                  }
                });
                sshPasswordFlag = true;

                // retry
                retryFlag = true;
              }
              else if (   (   (command.getErrorCode() == Errors.WEBDAV_SESSION_FAIL)
                           || (command.getErrorCode() == Errors.NO_WEBDAV_PASSWORD)
                           || (command.getErrorCode() == Errors.INVALID_WEBDAV_PASSWORD)
                          )
                       && !webdavPasswordFlag
                       && !busyDialog.isAborted()
                      )
              {
                // get webdav password
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    String password = Dialogs.password(shell,
                                                       BARControl.tr("Webdav login password"),
                                                       BARControl.tr("Please enter Webdav login password for")+": "+entryData.storageName+".",
                                                       BARControl.tr("Password")+":"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("WEBDAV_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  ),
                                               0
                                              );
                    }
                  }
                });
                webdavPasswordFlag = true;

                // retry
                retryFlag = true;
              }
              else if (   (   (command.getErrorCode() == Errors.NO_CRYPT_PASSWORD)
                           || (command.getErrorCode() == Errors.INVALID_CRYPT_PASSWORD)
                           || (command.getErrorCode() == Errors.CORRUPT_DATA)
                          )
                       && !decryptPasswordFlag
                       && !busyDialog.isAborted()
                      )
              {
                // get crypt password
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    String password = Dialogs.password(shell,
                                                       BARControl.tr("Decrypt password"),
                                                       BARControl.tr("Please enter decrypt password for")+": "+entryData.storageName+".",
                                                       BARControl.tr("Password")+":"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("DECRYPT_PASSWORD_ADD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  ),
                                               0
                                              );
                    }
                  }
                });
                decryptPasswordFlag = true;

                // retry
                retryFlag = true;
              }
            }
            while (retryFlag && !busyDialog.isAborted());

            // abort command if requested
            if (!busyDialog.isAborted())
            {
              if (command.getErrorCode() != Errors.NONE)
              {
                final String errorText = command.getErrorText();
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    Dialogs.error(shell,"Cannot restore entry\n\n'"+entryData.name+"'\n\nfrom archive\n\n'"+entryData.storageName+"'\n\n(error: "+errorText+")");
                  }
                });
              }
            }
            else
            {
              busyDialog.updateText("Aborting\u2026");
              command.abort();
              break;
            }
          }

          // close busy dialog, restore cursor
          display.syncExec(new Runnable()
          {
            public void run()
            {
              busyDialog.close();
              shell.setCursor(null);
             }
          });
        }
        catch (CommunicationError error)
        {
          final String errorMessage = error.getMessage();
          display.syncExec(new Runnable()
          {
            public void run()
            {
              busyDialog.close();
              shell.setCursor(null);
              Dialogs.error(shell,"Error while restoring entries:\n\n%s",errorMessage);
             }
          });
        }
        catch (Exception exception)
        {
          BARServer.disconnect();
          System.err.println("ERROR: "+exception.getMessage());
          BARControl.printStackTrace(exception);
          System.exit(1);
        }
      }
    };
  }
}

/* end of file */
