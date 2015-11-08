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
import java.lang.ref.WeakReference;
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
import org.eclipse.swt.events.MenuListener;
import org.eclipse.swt.events.MenuEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
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
public class TabRestore
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

  /** entity states
   */
  enum EntityStates
  {
    NONE,
    OK,
    ANY
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
  class IndexData implements Comparable<IndexData>
  {
    /** tree item update runnable
     */
    abstract class TreeItemUpdateRunnable
    {
      public abstract void update(TreeItem treeItem, IndexData indexData);
    }
    /** table item update runnable
     */
    abstract class TableItemUpdateRunnable
    {
      public abstract void update(TableItem tableItem, IndexData indexData);
    }
    /** menu item update runnable
     */
    abstract class MenuItemUpdateRunnable
    {
      public abstract void update(MenuItem menuItem, IndexData indexData);
    }

    public String                   name;                     // name
    public long                     dateTime;                 // date/time when storage was created or last time some storage was created
    public long                     size;                     // storage size or total size [bytes]
    public IndexStates              indexState;               // state of index
    public String                   errorMessage;             // last error message

    private TreeItem                treeItem;                 // reference tree item or null
    private TreeItemUpdateRunnable  treeItemUpdateRunnable;
    private TableItem               tableItem;                // reference table item or null
    private TableItemUpdateRunnable tableItemUpdateRunnable;
    private Menu                    subMenu;                  // reference sub-menu or null
    private MenuItem                menuItem;                 // reference menu item or null
    private MenuItemUpdateRunnable  menuItemUpdateRunnable;
    private boolean                 checked;                  // true iff storage entry is tagged

    /** create index data
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param size size of storage [byte]
     * @param errorMessage error message text
     */
    IndexData(String name, long dateTime, long size, String errorMessage)
    {
      this.name          = name;
      this.dateTime      = dateTime;
      this.size          = size;
      this.indexState    = IndexStates.NONE;
      this.errorMessage  = errorMessage;
      this.treeItem      = null;
      this.tableItem     = null;
      this.subMenu       = null;
      this.menuItem      = null;
      this.checked       = false;
    }

    /** create index data
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param lastCheckedDateTime last checked date/time (timestamp)
     */
    IndexData(String name, long dateTime)
    {
      this(name,dateTime,0L,null);
    }

    /** create index data
     * @param name name of storage
     * @param uuid uuid
     */
    IndexData(String name)
    {
      this(name,0L);
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
    protected void setTableItem(TableItem tableItem, TableItemUpdateRunnable tableItemUpdateRunnable)
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

    /** get sub-menu reference
     * @return sub-menu
     */
    public Menu getSubMenu()
    {
      return this.subMenu;
    }

    /** set sub-menu reference
     * @param setSubMenu sub-menu
     */
    public void setSubMenu(Menu subMenu)
    {
      this.subMenu = subMenu;
    }

    /** clear sub-menu reference
     */
    public void clearSubMenu()
    {
      this.subMenu = null;
    }

    /** set menu item reference
     * @param menuItem menu item
     * @param menuItemUpdateRunnable menu item update runnable
     */
    public void setMenuItem(MenuItem menuItem, MenuItemUpdateRunnable menuItemUpdateRunnable)
    {
      this.menuItem               = menuItem;
      this.menuItemUpdateRunnable = menuItemUpdateRunnable;
    }

    /** clear table item reference
     */
    public void clearMenuItem()
    {
      this.menuItem = null;
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
      if ((treeItem != null) && !treeItem.isDisposed())
      {
        treeItemUpdateRunnable.update(treeItem,this);
      }
      if ((tableItem != null) && !tableItem.isDisposed())
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
      if ((treeItem != null) && !treeItem.isDisposed())
      {
        treeItem.setChecked(checked);
      }
      if ((tableItem != null) && !tableItem.isDisposed())
      {
        tableItem.setChecked(checked);
      }
    }

    /** compare index data
     * @return <0/=0/>0 if name </=/> indexData.name
     */
    public int compareTo(IndexData indexData)
    {
      return name.compareTo(indexData.name);
    }

    /** get info string
     * @return info string
     */
    public String getInfo()
    {
      return "";
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
          return indexData1.compareTo(indexData2);
        case SORTMODE_SIZE:
          if      (indexData1.size < indexData2.size) return -1;
          else if (indexData1.size > indexData2.size) return  1;
          else                                        return  0;
        case SORTMODE_CREATED_DATETIME:
          if      (indexData1.dateTime < indexData2.dateTime) return -1;
          else if (indexData1.dateTime > indexData2.dateTime) return  1;
          else                                                return  0;
        case SORTMODE_STATE:
          return indexData1.indexState.compareTo(indexData2.indexState);
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
    public String jobUUID;                       // job UUID
    public long   totalEntries;                  // total number of entries

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)uuidIndexData,
                               uuidIndexData.name,
                               Units.formatByteSize(uuidIndexData.size),
                               (uuidIndexData.dateTime > 0) ? simpleDateFormat.format(new Date(uuidIndexData.dateTime*1000L)) : "-",
                               ""
                              );
      }
    };

    private final MenuItemUpdateRunnable menuItemUpdateRunnable = new MenuItemUpdateRunnable()
    {
      public void update(MenuItem menuItem, IndexData indexData)
      {
        UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

        menuItem.setText(uuidIndexData.name);
      }
    };

    /** create UUID data index
     * @param jobUUID job uuid
     * @param name job name
     * @param lastDateTime last date/time (timestamp) when storage was created
     * @param totalEntries total number of entries of storage
     * @param totalSize total size of storage [byte]
     * @param lastErrorMessage last error message text
     */
    UUIDIndexData(String jobUUID,
                  String name,
                  long   lastDateTime,
                  long   totalEntries,
                  long   totalSize,
                  String lastErrorMessage
                 )
    {
      super(name,lastDateTime,totalSize,lastErrorMessage);
      this.jobUUID      = jobUUID;
      this.totalEntries = totalEntries;
    }

    /** set tree item reference
     * @param treeItem tree item
     */
    public void setTreeItem(TreeItem treeItem)
    {
      setTreeItem(treeItem,treeItemUpdateRunnable);
    }

    /** set menu item reference
     * @param menuItem menu item
     */
    public void setMenuItem(MenuItem menuItem)
    {
      setMenuItem(menuItem,menuItemUpdateRunnable);
    }

    /** compare index data
     * @return <0/=0/>0 if name </=/> indexData.name
     */
    public int compareTo(IndexData indexData)
    {
      return name.compareTo(indexData.name);
    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return name;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "UUIDIndexData {"+jobUUID+", created="+dateTime+", size="+size+" bytes, checked="+isChecked()+"}";
    }
  }

  /** UUID data map
   */
  class UUIDIndexDataMap extends HashMap<String,UUIDIndexData>
  {
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
      put(uuidIndexData.jobUUID,uuidIndexData);
    }

    /** remove UUID data from map
     * @param uuidIndexData UUID data
     */
    public void remove(UUIDIndexData uuidIndexData)
    {
      remove(uuidIndexData.jobUUID);
    }
  }

  /** entity index data
   */
  class EntityIndexData extends IndexData
  {
    public long                  entityId;
    public Settings.ArchiveTypes archiveType;
    public long                  totalEntries;

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        EntityIndexData entityIndexData = (EntityIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)entityIndexData,
                               entityIndexData.archiveType.toString(),
                               Units.formatByteSize(entityIndexData.size),
                               (entityIndexData.dateTime > 0) ? simpleDateFormat.format(new Date(entityIndexData.dateTime*1000L)) : "-",
                               ""
                              );
      }
    };

    private final MenuItemUpdateRunnable menuItemUpdateRunnable = new MenuItemUpdateRunnable()
    {
      public void update(MenuItem menuItem, IndexData indexData)
      {
        EntityIndexData entityIndexData = (EntityIndexData)indexData;

        menuItem.setText(entityIndexData.name);
      }
    };

    /** create job data index
     * @param entityId entity id
     * @param name name of storage
     * @param lastDateTime last date/time (timestamp) when storage was created
     * @param totalEntries total number of entresi of storage
     * @param totalSize total size of storage [byte]
     * @param lastErrorMessage last error message text
     */
    EntityIndexData(long                  entityId,
                    Settings.ArchiveTypes archiveType,
                    long                  lastDateTime,
                    long                  totalEntries,
                    long                  totalSize,
                    String                lastErrorMessage
                   )
    {
      super("",lastDateTime,totalSize,lastErrorMessage);
      this.entityId     = entityId;
      this.archiveType  = archiveType;
      this.totalEntries = totalEntries;
    }

    /** set tree item reference
     * @param treeItem tree item
     */
    public void setTreeItem(TreeItem treeItem)
    {
      setTreeItem(treeItem,treeItemUpdateRunnable);
    }

    /** set menu item reference
     * @param menuItem menu item
     */
    public void setMenuItem(MenuItem menuItem)
    {
      setMenuItem(menuItem,menuItemUpdateRunnable);
    }

    /** compare index data
     * @return <0/=0/>0 if name </=/> entityIndexData.date/entityIndexData.archiveType
     */
    public int compareTo(IndexData indexData)
    {
      EntityIndexData entityIndexData = (EntityIndexData)indexData;
      int             result;

      if      (dateTime < entityIndexData.dateTime)
      {
        result = -1;
      }
      else if (dateTime > entityIndexData.dateTime)
      {
        result = 1;
      }
      else
      {
        result = archiveType.toString().compareTo(entityIndexData.archiveType.toString());
      }

      return result;
    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return String.format("%d: %s",entityId,archiveType.toString());
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "EntityIndexData {"+entityId+", type="+archiveType.toString()+", created="+dateTime+", size="+size+" bytes, checked="+isChecked()+"}";
    }
  }

  /** job data map
   */
  class EntityIndexDataMap extends HashMap<Long,EntityIndexData>
  {
    /** get job data from map
     * @param entityId database id
     * @return job data
     */
    public EntityIndexData get(long entityId)
    {
      return super.get(entityId);
    }

    /** get job data from map by job name
     * @param jobName job name
     * @return job data
     */
    public EntityIndexData getByName(String jobName)
    {
      for (EntityIndexData entityIndexData : values())
      {
        if (entityIndexData.name.equals(jobName)) return entityIndexData;
      }

      return null;
    }

    /** put job data into map
     * @param entityIndexData job data
     */
    public void put(EntityIndexData entityIndexData)
    {
      put(entityIndexData.entityId,entityIndexData);
    }

    /** remove job data from map
     * @param entityIndexData job data
     */
    public void remove(EntityIndexData entityIndexData)
    {
      remove(entityIndexData.entityId);
    }
  }

  /** storage index data
   */
  class StorageIndexData extends IndexData
  {
    public long                  storageId;                // database storage id
    public String                jobName;                  // job name or null
    public Settings.ArchiveTypes archiveType;              // archive type
    public long                  entries;                  // number of entries
    public IndexModes            indexMode;                // mode of index
    public long                  lastCheckedDateTime;      // last checked date/time

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        StorageIndexData storageIndexData = (StorageIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)storageIndexData,
                               storageIndexData.name,
                               Units.formatByteSize(storageIndexData.size),
                               (storageIndexData.dateTime > 0) ? simpleDateFormat.format(new Date(storageIndexData.dateTime*1000L)) : "-",
                               storageIndexData.indexState.toString()
                              );
      }
    };

    private final TableItemUpdateRunnable tableItemUpdateRunnable = new TableItemUpdateRunnable()
    {
      public void update(TableItem tableItem, IndexData indexData)
      {
         StorageIndexData storageIndexData = (StorageIndexData)indexData;

         Widgets.updateTableItem(widgetStorageTable,
                                 (Object)storageIndexData,
                                 storageIndexData.name,
                                 Units.formatByteSize(storageIndexData.size),
                                 simpleDateFormat.format(new Date(storageIndexData.dateTime*1000L)),
                                 storageIndexData.indexState.toString()
                                );
      }
    };

    /** create storage data index
     * @param storageId database storage id
     * @param jobName job name or null
     * @param archiveType archive type
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param entries number of entries
     * @param size size of storage [byte]
     * @param indexState storage index state
     * @param indexMode storage index mode
     * @param lastCheckedDateTime last checked date/time (timestamp)
     * @param errorMessage error message text
     */
    StorageIndexData(long                  storageId,
                     String                jobName,
                     Settings.ArchiveTypes archiveType,
                     String                name,
                     long                  dateTime,
                     long                  entries,
                     long                  size,
                     IndexStates           indexState,
                     IndexModes            indexMode,
                     long                  lastCheckedDateTime,
                     String                errorMessage
                    )
    {
      super(name,dateTime,size,errorMessage);
      this.storageId           = storageId;
      this.jobName             = jobName;
      this.archiveType         = archiveType;
      this.entries             = entries;
      this.indexState          = indexState;
      this.indexMode           = indexMode;
      this.lastCheckedDateTime = lastCheckedDateTime;
    }

    /** create storage data
     * @param storageId database storage id
     * @param jobName job name
     * @param archiveType archive type
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param lastCheckedDateTime last checked date/time (timestamp)
     */
    StorageIndexData(long                  storageId,
                     String                jobName,
                     Settings.ArchiveTypes archiveType,
                     String                name,
                     long                  dateTime,
                     long                  lastCheckedDateTime
                    )
    {
      this(storageId,jobName,archiveType,name,dateTime,0L,0L,IndexStates.OK,IndexModes.MANUAL,lastCheckedDateTime,null);
    }

    /** create storage data
     * @param storageId database storage id
     * @param entityId database entity id
     * @param jobName job name
     * @param archiveType archive type
     * @param name name of storage
     * @param uuid uuid
     */
    StorageIndexData(long storageId, String jobName, Settings.ArchiveTypes archiveType, String name)
    {
      this(storageId,jobName,archiveType,name,0L,0L);
    }

    /** set tree item reference
     * @param treeItem tree item
     */
    public void setTreeItem(TreeItem treeItem)
    {
      setTreeItem(treeItem,treeItemUpdateRunnable);
    }

    public void setTableItem(TableItem tableItem)
    {
      setTableItem(tableItem,tableItemUpdateRunnable);
    }

    /** compare index data
     * @return <0/=0/>0 if name </=/> indexData.name
     */
    public int compareTo(IndexData indexData)
    {
      return name.compareTo(indexData.name);
    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return String.format("%d: %s, %s",storageId,jobName,name);
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "StorageIndexData {"+name+", created="+dateTime+", size="+size+" bytes, state="+indexState+", last checked="+lastCheckedDateTime+", checked="+isChecked()+"}";
    }
  };

  /** storage data map
   */
  class StorageIndexDataMap extends HashMap<Long,StorageIndexData>
  {
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
    private HashMap<Long,EntityIndexData>  entityIndexDataMap;
    private HashMap<Long,StorageIndexData> storageIndexDataMap;

    /** constructor
     */
    public IndexDataMap()
    {
      this.uuidIndexDataMap    = new HashMap<String,UUIDIndexData>();
      this.entityIndexDataMap  = new HashMap<Long,EntityIndexData>();
      this.storageIndexDataMap = new HashMap<Long,StorageIndexData>();
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
     * @param jobUUID job UUID
     * @param name job name
     * @param lastDateTime last date/time (timestamp) when storage was created
     * @param totalEntries total number of entresi of storage
     * @param totalSize total size of storage [byte]
     * @param lastErrorMessage last error message text
     */
    synchronized public UUIDIndexData updateUUIDIndexData(String jobUUID, String name, long lastDateTime, long totalEntries, long totalSize, String lastErrorMessage)
    {
      UUIDIndexData uuidIndexData = uuidIndexDataMap.get(jobUUID);
      if (uuidIndexData != null)
      {
        uuidIndexData.name         = name;
        uuidIndexData.dateTime     = lastDateTime;
        uuidIndexData.totalEntries = totalEntries;
        uuidIndexData.size         = totalSize;
        uuidIndexData.errorMessage = lastErrorMessage;
      }
      else
      {
        uuidIndexData = new UUIDIndexData(jobUUID,
                                          name,
                                          lastDateTime,
                                          totalEntries,
                                          totalSize,
                                          lastErrorMessage
                                         );
        uuidIndexDataMap.put(jobUUID,uuidIndexData);
      }

      return uuidIndexData;
    }

    /** get job index data from map by job id
     * @param entityId database entity id
     * @return job index data
     */
    public EntityIndexData getEntityIndexData(long entityId)
    {
      return entityIndexDataMap.get(entityId);
    }

    /** get job index data from map by job name
     * @param name job name
     * @return UUID data
     */
    public EntityIndexData getEntityIndexDataByName(String name)
    {
      for (EntityIndexData entityIndexData : entityIndexDataMap.values())
      {
        if (entityIndexData.name.equals(name)) return entityIndexData;
      }

      return null;
    }

    /** update entity data index
     * @param entityId job id
     * @param name name of storage
     * @param lastDateTime last date/time (timestamp) when storage was created
     * @param totalEntries total number of entresi of storage
     * @param totalSize total size of storage [byte]
     * @param lastErrorMessage last error message text
     */
    public synchronized EntityIndexData updateEntityIndexData(long entityId, Settings.ArchiveTypes archiveType, long lastDateTime, long totalEntries, long totalSize, String lastErrorMessage)
    {
      EntityIndexData entityIndexData = entityIndexDataMap.get(entityId);
      if (entityIndexData != null)
      {
        entityIndexData.entityId     = entityId;
        entityIndexData.archiveType  = archiveType;
        entityIndexData.dateTime     = lastDateTime;
        entityIndexData.totalEntries = totalEntries;
        entityIndexData.size         = totalSize;
        entityIndexData.errorMessage = lastErrorMessage;
      }
      else
      {
        entityIndexData = new EntityIndexData(entityId,
                                              archiveType,
                                              lastDateTime,
                                              totalEntries,
                                              totalSize,
                                              lastErrorMessage
                                             );
        entityIndexDataMap.put(entityId,entityIndexData);
      }

      return entityIndexData;
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
     * @param jobName job name
     * @param archiveType archive type
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param entries number of entries
     * @param size size of storage [byte]
     * @param indexState storage index state
     * @param indexMode storage index mode
     * @param lastCheckedDateTime last checked date/time (timestamp)
     * @param errorMessage error message text
     */
    public synchronized StorageIndexData updateStorageIndexData(long storageId, String jobName, Settings.ArchiveTypes archiveType, String name, long dateTime, long entries, long size, IndexStates indexState, IndexModes indexMode, long lastCheckedDateTime, String errorMessage)
    {
      StorageIndexData storageIndexData = storageIndexDataMap.get(storageId);
      if (storageIndexData != null)
      {
        storageIndexData.jobName             = jobName;
        storageIndexData.archiveType         = archiveType;
        storageIndexData.name                = name;
        storageIndexData.dateTime            = dateTime;
        storageIndexData.entries             = entries;
        storageIndexData.size                = size;
        storageIndexData.indexState          = indexState;
        storageIndexData.indexMode           = indexMode;
        storageIndexData.lastCheckedDateTime = lastCheckedDateTime;
        storageIndexData.errorMessage        = errorMessage;
      }
      else
      {
        storageIndexData = new StorageIndexData(storageId,
                                                jobName,
                                                archiveType,
                                                name,
                                                dateTime,
                                                entries,
                                                size,
                                                indexState,
                                                indexMode,
                                                lastCheckedDateTime,
                                                errorMessage
//                                                new File(name).getName()
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
      else if (indexData instanceof EntityIndexData)
      {
        entityIndexDataMap.remove((EntityIndexData)indexData);
      }
      else if (indexData instanceof StorageIndexData)
      {
        storageIndexDataMap.remove((StorageIndexData)indexData);
      }
    }
  }

  /** find index for insert of item in sorted storage data list
   * @param indexData index data
   * @return index in tree
   */
  private int findStorageTreeIndex(IndexData indexData)
  {
    TreeItem            treeItems[]         = widgetStorageTree.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree);

    int index = 0;
    while (   (index < treeItems.length)
           && (indexDataComparator.compare(indexData,(IndexData)treeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of item in sorted index data tree
   * @param treeItem tree item
   * @param indexData index data
   * @return index in tree
   */
  private int findStorageTreeIndex(TreeItem treeItem, IndexData indexData)
  {
    TreeItem            treeItems[]         = treeItem.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree);

    int index = 0;
    while (   (index < treeItems.length)
           && (indexDataComparator.compare(indexData,(IndexData)treeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of item in sorted storage menu
   * @param uuidIndexData UUID index data
   * @return index in menu
   */
  private int findStorageMenuIndex(UUIDIndexData uuidIndexData)
  {
    MenuItem            menuItems[]         = widgetStorageTreeAssignToMenu.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree);

    int index = 0;
    while (   (index < menuItems.length)
           && (indexDataComparator.compare(uuidIndexData,(UUIDIndexData)menuItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of item in sorted storage menu
   * @param subMenu sub-menu
   * @param entityIndexData entity index data
   * @return index in menu
   */
  private int findStorageMenuIndex(Menu subMenu, EntityIndexData entityIndexData)
  {
    MenuItem            menuItems[]         = subMenu.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree);

    int index = 4;
    while (   (index < menuItems.length)
           && (indexDataComparator.compare(entityIndexData,(EntityIndexData)menuItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of item in sorted storage data table
   * @param storageIndexData data of tree item
   * @return index in table
   */
  private int findStorageTableIndex(StorageIndexData storageIndexData)
  {
    TableItem           tableItems[]        = widgetStorageTable.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTable);

    int index = 0;
    while (   (index < tableItems.length)
           && (indexDataComparator.compare(storageIndexData,(StorageIndexData)tableItems[index].getData()) > 0)
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
    private Object        trigger              = new Object();   // trigger update object
    private boolean       triggeredFlag        = false;
    private int           storageMaxCount      = 100;
    private String        storagePattern       = null;
    private IndexStateSet storageIndexStateSet = INDEX_STATE_SET_ALL;
    private EntityStates  storageEntityState   = EntityStates.ANY;
    private boolean       setUpdateIndicator   = false;          // true to set color/cursor at update

    /** create update storage list thread
     */
    UpdateStorageThread()
    {
      super();
      setDaemon(true);
      setName("BARControl Update Storage");
    }

    /** run status update thread
     */
    public void run()
    {
      try
      {
        for (;;)
        {
          boolean updateIndicatorFlag = false;

          // set busy cursor and foreground color to inform about update
          if (setUpdateIndicator)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                BARControl.waitCursor();
                widgetStorageTree.setForeground(COLOR_MODIFIED);
                widgetStorageTable.setForeground(COLOR_MODIFIED);
              }
            });
            updateIndicatorFlag = true;
          }

          // update tree/table
          try
          {
            HashSet<TreeItem> uuidTreeItems = new HashSet<TreeItem>();
            if (!triggeredFlag)
            {
              updateUUIDTreeItems(uuidTreeItems);
            }

            HashSet<TreeItem> entityTreeItems = new HashSet<TreeItem>();
            if (!triggeredFlag)
            {
              updateEntityTreeItems(uuidTreeItems,entityTreeItems);
            }

            if (!triggeredFlag)
            {
              updateStorageTreeItems(entityTreeItems);
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

          // update menues
          try
          {
            updateUUIDMenus();
          }
          catch (CommunicationError error)
          {
            // ignored
          }

          // reset cursor and foreground color
          if (updateIndicatorFlag)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                widgetStorageTree.setForeground(null);
                widgetStorageTable.setForeground(null);
                BARControl.resetCursor();
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

    /** trigger update of storage list
     * @param storagePattern new storage pattern
     * @param storageIndexStateSet new storage index state set
     * @param storageEntityState new storage entity state
     * @param storageMaxCount new max. entries in list
     */
    public void triggerUpdate(String storagePattern, IndexStateSet storageIndexStateSet, EntityStates storageEntityState, int storageMaxCount)
    {
      synchronized(trigger)
      {
        if (   (this.storagePattern == null) || (storagePattern == null) || !this.storagePattern.equals(storagePattern)
            || (this.storageIndexStateSet != storageIndexStateSet) || (this.storageEntityState != storageEntityState)
            || (this.storageMaxCount != storageMaxCount)
           )
        {
          this.storagePattern       = storagePattern;
          this.storageIndexStateSet = storageIndexStateSet;
          this.storageEntityState   = storageEntityState;
          this.storageMaxCount      = storageMaxCount;
          this.setUpdateIndicator   = true;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of storage list
     * @param storagePattern new storage pattern
     */
    public void triggerUpdateStoragePattern(String storagePattern)
    {
      synchronized(trigger)
      {
        if ((this.storagePattern == null) || (storagePattern == null) || !this.storagePattern.equals(storagePattern))
        {
          this.storagePattern     = storagePattern;
          this.setUpdateIndicator = true;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of storage list
     * @param storageIndexStateSet new storage index state set
     * @param storageEntityState new storage entity state
     */
    public void triggerUpdateStorageState(IndexStateSet storageIndexStateSet, EntityStates storageEntityState)
    {
      synchronized(trigger)
      {
        if ((this.storageIndexStateSet != storageIndexStateSet) || (this.storageEntityState != storageEntityState))
        {
          this.storageIndexStateSet = storageIndexStateSet;
          this.storageEntityState   = storageEntityState;
          this.setUpdateIndicator   = true;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of storage list
     * @param storageMaxCount new max. entries in list
     */
    public void triggerUpdateStorageMaxCount(int storageMaxCount)
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

    /** trigger update of storage list
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
      String[]                errorMessage          = new String[1];
      ValueMap                resultMap             = new ValueMap();

      uuidTreeItems.clear();

      // get UUID items
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
      command = BARServer.runCommand(StringParser.format("INDEX_UUID_LIST maxCount=%d pattern=%'S",
                                                         storageMaxCount,
                                                         (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                        ),
                                     0
                                    );
      while (!command.endOfData() && !triggeredFlag)
      {
        if (command.getNextResult(errorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            String jobUUID          = resultMap.getString("jobUUID"         );
            String name             = resultMap.getString("name"            );
            long   lastDateTime     = resultMap.getLong  ("lastDateTime"    );
            long   totalEntries     = resultMap.getLong  ("totalEntries"    );
            long   totalSize        = resultMap.getLong  ("totalSize"       );
            String lastErrorMessage = resultMap.getString("lastErrorMessage");

            // add/update index map
            final UUIDIndexData uuidIndexData = indexDataMap.updateUUIDIndexData(jobUUID,
                                                                                 name,
                                                                                 lastDateTime,
                                                                                 totalEntries,
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
                  uuidIndexData.setTreeItem(uuidTreeItem);
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

    /** update entity tree items
     * @param uuidTreeItem UUID tree item to update
     * @param entityTreeItems updated job tree items
     */
    private void updateEntityTreeItems(final TreeItem uuidTreeItem, final HashSet<TreeItem> entityTreeItems)
    {
      final HashSet<TreeItem> removeEntityTreeItemSet = new HashSet<TreeItem>();
      Command                 command;
      String[]                errorMessage            = new String[1];
      ValueMap                resultMap               = new ValueMap();

      // get job items, UUID index data
      final UUIDIndexData uuidIndexData[] = new UUIDIndexData[1];
      display.syncExec(new Runnable()
      {
        public void run()
        {
          assert uuidTreeItem.getData() instanceof UUIDIndexData;

          for (TreeItem treeItem : uuidTreeItem.getItems())
          {
            assert treeItem.getData() instanceof EntityIndexData;
            removeEntityTreeItemSet.add(treeItem);
          }

          uuidIndexData[0] = (UUIDIndexData)uuidTreeItem.getData();
        }
      });
      if (triggeredFlag) return;

      // update entity list
      command = BARServer.runCommand(StringParser.format("INDEX_ENTITY_LIST jobUUID=%'S pattern=%'S",
                                                         uuidIndexData[0].jobUUID,
                                                         (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                        ),
                                     0
                                    );
      while (!command.endOfData())
      {
        if (command.getNextResult(errorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            long                  entityId         = resultMap.getLong  ("entityId"                               );
            String                jobUUID          = resultMap.getString("jobUUID"                                );
            String                scheduleUUID     = resultMap.getString("scheduleUUID"                           );
            Settings.ArchiveTypes archiveType      = resultMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
            long                  lastDateTime     = resultMap.getLong  ("lastDateTime"                           );
            long                  totalEntries     = resultMap.getLong  ("totalEntries"                           );
            long                  totalSize        = resultMap.getLong  ("totalSize"                              );
            String                lastErrorMessage = resultMap.getString("lastErrorMessage"                       );

            // add/update job data index
            final EntityIndexData entityIndexData = indexDataMap.updateEntityIndexData(entityId,
                                                                                       archiveType,
                                                                                       lastDateTime,
                                                                                       totalEntries,
                                                                                       totalSize,
                                                                                       lastErrorMessage
                                                                                      );

            // insert/update tree item
            display.syncExec(new Runnable()
            {
              public void run()
              {
                TreeItem entityTreeItem = Widgets.getTreeItem(widgetStorageTree,entityIndexData);
                if (entityTreeItem == null)
                {
                  // insert tree item
                  entityTreeItem = Widgets.insertTreeItem(uuidTreeItem,
                                                          findStorageTreeIndex(uuidTreeItem,entityIndexData),
                                                          (Object)entityIndexData,
                                                          true
                                                         );
                  entityIndexData.setTreeItem(entityTreeItem);
                }
                else
                {
                  assert entityTreeItem.getData() instanceof EntityIndexData;

                  // keep tree item
                  removeEntityTreeItemSet.remove(entityTreeItem);
                }
                if (entityTreeItem.getExpanded())
                {
                  entityTreeItems.add(entityTreeItem);
                }

                // update view
                entityIndexData.update();
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
          for (TreeItem treeItem : removeEntityTreeItemSet)
          {
            IndexData indexData = (IndexData)treeItem.getData();
            Widgets.removeTreeItem(widgetStorageTree,treeItem);
            indexData.clearTreeItem();
          }
        }
      });
    }

    /** update entity tree items
     * @param uuidTreeItems UUID tree items to update
     * @param entityTreeItems updated job tree items
     */
    private void updateEntityTreeItems(final HashSet<TreeItem> uuidTreeItems, final HashSet<TreeItem> entityTreeItems)
    {
      entityTreeItems.clear();

      for (final TreeItem uuidTreeItem : uuidTreeItems)
      {
        updateEntityTreeItems(uuidTreeItem,entityTreeItems);
      }
    }

    /** update storage tree items
     * @param entityTreeItem job tree item to update
     */
    private void updateStorageTreeItems(final TreeItem entityTreeItem)
    {
      final HashSet<TreeItem> removeStorageTreeItemSet = new HashSet<TreeItem>();
      Command                 command;
      String[]                errorMessage             = new String[1];
      ValueMap                resultMap                = new ValueMap();

      // get storage items, job index data
      final EntityIndexData entityIndexData[] = new EntityIndexData[1];
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TreeItem treeItem : entityTreeItem.getItems())
          {
            assert treeItem.getData() instanceof StorageIndexData;
            removeStorageTreeItemSet.add(treeItem);
          }

          entityIndexData[0] = (EntityIndexData)entityTreeItem.getData();
        }
      });
      if (triggeredFlag) return;

      // update storage list
      command = BARServer.runCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%d maxCount=%d indexState=%s indexMode=%s pattern=%'S",
                                                         entityIndexData[0].entityId,
                                                         storageMaxCount,
                                                         storageIndexStateSet.nameList("|"),
                                                         "*",
                                                         (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                        ),
                                     0
                                    );
      while (!command.endOfData())
      {
        if (command.getNextResult(errorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            long                  storageId           = resultMap.getLong  ("storageId"                              );
            String                jobUUID             = resultMap.getString("jobUUID"                                );
            String                scheduleUUID        = resultMap.getString("scheduleUUID"                           );
            String                jobName             = resultMap.getString("jobName"                                );
            Settings.ArchiveTypes archiveType         = resultMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
            String                name                = resultMap.getString("name"                                   );
            long                  dateTime            = resultMap.getLong  ("dateTime"                               );
            long                  entries             = resultMap.getLong  ("entries"                                );
            long                  size                = resultMap.getLong  ("size"                                   );
            IndexStates           indexState          = resultMap.getEnum  ("indexState",IndexStates.class           );
            IndexModes            indexMode           = resultMap.getEnum  ("indexMode",IndexModes.class             );
            long                  lastCheckedDateTime = resultMap.getLong  ("lastCheckedDateTime"                    );
            String                errorMessage_       = resultMap.getString("errorMessage"                           );

            // add/update storage data
            final StorageIndexData storageIndexData = indexDataMap.updateStorageIndexData(storageId,
                                                                                          jobName,
                                                                                          archiveType,
                                                                                          name,
                                                                                          dateTime,
                                                                                          entries,
                                                                                          size,
                                                                                          indexState,
                                                                                          indexMode,
                                                                                          lastCheckedDateTime,
                                                                                          errorMessage_
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
                  storageTreeItem = Widgets.insertTreeItem(entityTreeItem,
                                                           findStorageTreeIndex(entityTreeItem,storageIndexData),
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
     * @param entityTreeItems job tree items to update
     */
    private void updateStorageTreeItems(final HashSet<TreeItem> entityTreeItems)
    {
      for (final TreeItem entityTreeItem : entityTreeItems)
      {
        updateStorageTreeItems(entityTreeItem);
      }
    }

    /** update tree items
     * @param treeItem tree item to update
     */
    private void updateTreeItems(TreeItem treeItem)
    {
      if      (treeItem.getData() instanceof UUIDIndexData)
      {
        updateStorageThread.updateEntityTreeItems(treeItem,new HashSet<TreeItem>());
      }
      else if (treeItem.getData() instanceof EntityIndexData)
      {
        updateStorageThread.updateStorageTreeItems(treeItem);
      }
    }

    /** update storage table items
     */
    private void updateStorageTableItems()
    {
      Command  command;
      String[] errorMessage = new String[1];
      ValueMap resultMap    = new ValueMap();

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
      command = BARServer.runCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s maxCount=%d indexState=%s indexMode=%s pattern=%'S",
                                                         (storageEntityState != EntityStates.NONE) ? "*" : "0",
                                                         storageMaxCount,
                                                         storageIndexStateSet.nameList("|"),
                                                         "*",
                                                         (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                        ),
                                     0
                                    );
      while (!command.endOfData() && !triggeredFlag)
      {
        if (command.getNextResult(errorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            long                  storageId           = resultMap.getLong  ("storageId"                              );
            String                jobUUID             = resultMap.getString("jobUUID"                                );
            String                scheduleUUID        = resultMap.getString("scheduleUUID"                           );
            String                jobName             = resultMap.getString("jobName"                                );
            Settings.ArchiveTypes archiveType         = resultMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
            String                name                = resultMap.getString("name"                                   );
            long                  dateTime            = resultMap.getLong  ("dateTime"                               );
            long                  entries             = resultMap.getLong  ("entries"                                );
            long                  size                = resultMap.getLong  ("size"                                   );
            IndexStates           indexState          = resultMap.getEnum  ("indexState",IndexStates.class           );
            IndexModes            indexMode           = resultMap.getEnum  ("indexMode",IndexModes.class             );
            long                  lastCheckedDateTime = resultMap.getLong  ("lastCheckedDateTime"                    );
            String                errorMessage_       = resultMap.getString("errorMessage"                           );

            // add/update to index map
            final StorageIndexData storageIndexData = indexDataMap.updateStorageIndexData(storageId,
                                                                                          jobName,
                                                                                          archiveType,
                                                                                          name,
                                                                                          dateTime,
                                                                                          entries,
                                                                                          size,
                                                                                          indexState,
                                                                                          indexMode,
                                                                                          lastCheckedDateTime,
                                                                                          errorMessage_
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
                                                      findStorageTableIndex(storageIndexData),
                                                      (Object)storageIndexData
                                                     );
                  storageIndexData.setTableItem(tableItem);
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

    /** update UUID menus
     */
    private void updateUUIDMenus()
    {
      final HashSet<Menu>          removeUUIDMenuSet       = new HashSet<Menu>();
      final HashSet<UUIDIndexData> uuidIndexDataSet        = new HashSet<UUIDIndexData>();
      final HashSet<MenuItem>      removeEntityMenuItemSet = new HashSet<MenuItem>();
      Command                      command;
      String[]                     errorMessage            = new String[1];
      ValueMap                     resultMap               = new ValueMap();

      // get UUID menus
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (MenuItem menuItem : widgetStorageTreeAssignToMenu.getItems())
          {
            assert menuItem.getData() instanceof UUIDIndexData;

            Menu menu = menuItem.getMenu();
            assert(menu != null);

            removeUUIDMenuSet.add(menu);
          }
        }
      });
      if (triggeredFlag) return;

      // update UUIDs
      command = BARServer.runCommand(StringParser.format("INDEX_UUID_LIST pattern=*"),
                                     0
                                    );
      while (!command.endOfData() && !triggeredFlag)
      {
        if (command.getNextResult(errorMessage,
                                  resultMap,
                                  Command.TIMEOUT
                                 ) == Errors.NONE
           )
        {
          try
          {
            String jobUUID          = resultMap.getString("jobUUID"         );
            String name             = resultMap.getString("name"            );
            long   lastDateTime     = resultMap.getLong  ("lastDateTime"    );
            long   totalEntries     = resultMap.getLong  ("totalEntries"    );
            long   totalSize        = resultMap.getLong  ("totalSize"       );
            String lastErrorMessage = resultMap.getString("lastErrorMessage");

            // add/update index map
            final UUIDIndexData uuidIndexData = indexDataMap.updateUUIDIndexData(jobUUID,
                                                                                 name,
                                                                                 lastDateTime,
                                                                                 totalEntries,
                                                                                 totalSize,
                                                                                 lastErrorMessage
                                                                                );

            // update/insert menu item
            display.syncExec(new Runnable()
            {
              public void run()
              {
                Menu subMenu = Widgets.getMenu(widgetStorageTreeAssignToMenu,uuidIndexData);
                if (subMenu == null)
                {
                  MenuItem menuItem;

                  // insert sub-menu
                  subMenu = Widgets.insertMenu(widgetStorageTreeAssignToMenu,
                                               findStorageMenuIndex(uuidIndexData),
                                               (Object)uuidIndexData,
                                               uuidIndexData.name
                                              );
                  menuItem = Widgets.addMenuItem(subMenu,
                                                 null,
                                                 BARControl.tr("new full")
                                                );
                  menuItem.addSelectionListener(new SelectionListener()
                  {
                    public void widgetDefaultSelected(SelectionEvent selectionEvent)
                    {
                    }
                    public void widgetSelected(SelectionEvent selectionEvent)
                    {
                      MenuItem widget = (MenuItem)selectionEvent.widget;

                      UUIDIndexData uuidIndexData = (UUIDIndexData)widget.getParent().getData();
                      assignStorage(uuidIndexData,Settings.ArchiveTypes.FULL);
                    }
                  });
                  menuItem = Widgets.addMenuItem(subMenu,
                                                 null,
                                                 BARControl.tr("new incremental")
                                                );
                  menuItem.addSelectionListener(new SelectionListener()
                  {
                    public void widgetDefaultSelected(SelectionEvent selectionEvent)
                    {
                    }
                    public void widgetSelected(SelectionEvent selectionEvent)
                    {
                      MenuItem widget = (MenuItem)selectionEvent.widget;

                      UUIDIndexData uuidIndexData = (UUIDIndexData)widget.getParent().getData();
                      assignStorage(uuidIndexData,Settings.ArchiveTypes.INCREMENTAL);
                    }
                  });
                  menuItem = Widgets.addMenuItem(subMenu,
                                                 null,
                                                 BARControl.tr("new differenial")
                                                );
                  menuItem.addSelectionListener(new SelectionListener()
                  {
                    public void widgetDefaultSelected(SelectionEvent selectionEvent)
                    {
                    }
                    public void widgetSelected(SelectionEvent selectionEvent)
                    {
                      MenuItem widget = (MenuItem)selectionEvent.widget;

                      UUIDIndexData uuidIndexData = (UUIDIndexData)widget.getParent().getData();
                      assignStorage(uuidIndexData,Settings.ArchiveTypes.DIFFERENTIAL);
                    }
                  });
                  Widgets.addMenuSeparator(subMenu);

                  uuidIndexData.setSubMenu(subMenu);
                }
                else
                {
                  assert subMenu.getData() instanceof UUIDIndexData;

                  // keep menu item
                  removeUUIDMenuSet.remove(subMenu);
                }

                uuidIndexDataSet.add(uuidIndexData);
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

      // remove not existing UUID menus
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (Menu menu : removeUUIDMenuSet)
          {
            UUIDIndexData uuidIndexData = (UUIDIndexData)menu.getData();
//Dprintf.dprintf("remove uuidIndexData=%s",entityIndexData);
            Widgets.removeMenu(widgetStorageTreeAssignToMenu,menu);
            Widgets.removeMenu(widgetStorageTableAssignToMenu,menu);
            uuidIndexData.clearSubMenu();
          }
        }
      });

      // get entity menu items
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (UUIDIndexData uuidIndexData : uuidIndexDataSet)
          {
            Menu subMenu = uuidIndexData.getSubMenu();
            assert subMenu != null;

            MenuItem menuItems[] = subMenu.getItems();
            for (int i =4; i < menuItems.length; i++)
            {
              assert menuItems[i].getData() instanceof EntityIndexData;
              removeEntityMenuItemSet.add(menuItems[i]);
            }
          }
        }
      });
      if (triggeredFlag) return;

      // update entities
      for (UUIDIndexData uuidIndexData : uuidIndexDataSet)
      {
        final Menu subMenu = uuidIndexData.getSubMenu();

        command = BARServer.runCommand(StringParser.format("INDEX_ENTITY_LIST jobUUID=%'S pattern=*",
                                                           uuidIndexData.jobUUID
                                                          ),
                                       0
                                      );
        while (!command.endOfData() && !triggeredFlag)
        {
          if (command.getNextResult(errorMessage,
                                    resultMap,
                                    Command.TIMEOUT
                                   ) == Errors.NONE
             )
          {
            try
            {
              long                  entityId         = resultMap.getLong  ("entityId"                               );
              String                jobUUID          = resultMap.getString("jobUUID"                                );
              String                scheduleUUID     = resultMap.getString("scheduleUUID"                           );
              Settings.ArchiveTypes archiveType      = resultMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
              long                  lastDateTime     = resultMap.getLong  ("lastDateTime"                           );
              long                  totalEntries     = resultMap.getLong  ("totalEntries"                           );
              long                  totalSize        = resultMap.getLong  ("totalSize"                              );
              String                lastErrorMessage = resultMap.getString("lastErrorMessage"                       );

              // add/update job data index
              final EntityIndexData entityIndexData = indexDataMap.updateEntityIndexData(entityId,
                                                                                         archiveType,
                                                                                         lastDateTime,
                                                                                         totalEntries,
                                                                                         totalSize,
                                                                                         lastErrorMessage
                                                                                        );

              // update/insert menu item
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  MenuItem menuItem = Widgets.getMenuItem(subMenu,entityIndexData);
                  if (menuItem == null)
                  {
                    // insert menu item
                    menuItem = Widgets.insertMenuItem(subMenu,
                                                      findStorageMenuIndex(subMenu,entityIndexData),
                                                      (Object)entityIndexData,
                                                      ((entityIndexData.dateTime > 0) ? simpleDateFormat.format(new Date(entityIndexData.dateTime*1000L)) : "-")+", "+entityIndexData.archiveType.toString()
                                                     );
                    menuItem.addSelectionListener(new SelectionListener()
                    {
                      public void widgetDefaultSelected(SelectionEvent selectionEvent)
                      {
                      }
                      public void widgetSelected(SelectionEvent selectionEvent)
                      {
                        MenuItem widget = (MenuItem)selectionEvent.widget;

                        EntityIndexData entityIndexData = (EntityIndexData)widget.getData();

                        assignStorage(entityIndexData);
                      }
                    });
                    entityIndexData.setMenuItem(menuItem);
                  }
                  else
                  {
                    assert menuItem.getData() instanceof EntityIndexData;

                    // keep menu item
                    removeEntityMenuItemSet.remove(menuItem);
                  }
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
      }

      // remove not existing entity menu items
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (MenuItem menuItem : removeEntityMenuItemSet)
          {
            EntityIndexData entityIndexData = (EntityIndexData)menuItem.getData();
            Widgets.removeMenuItem(widgetStorageTreeAssignToMenu,menuItem);
//            Widgets.removeMenuItem(widgetStorageTableAssignToMenu,menuItem);
            entityIndexData.clearMenuItem();
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
    EntryTypes    entryType;
    String        name;
    long          dateTime;
    long          size;
    boolean       checked;
    RestoreStates restoreState;

    /** create entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     * @param size size [bytes]
     */
    EntryData(String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime, long size)
    {
      this.storageName     = storageName;
      this.storageDateTime = storageDateTime;
      this.entryType       = entryType;
      this.name            = name;
      this.dateTime        = dateTime;
      this.size            = size;
      this.checked         = false;
      this.restoreState    = RestoreStates.UNKNOWN;
    }

    /** create entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     */
    EntryData(String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime)
    {
      this(storageName,storageDateTime,entryType,name,dateTime,0L);
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
      return "Entry {"+storageName+", "+name+", "+entryType+", dateTime="+dateTime+", "+size+" bytes, checked="+checked+", state="+restoreState+"}";
    }
  };

  /** entry data map
   */
  class EntryDataMap extends HashMap<String,WeakReference<EntryData>>
  {
    /** update entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     * @param size size [bytes]
     */
    synchronized public EntryData update(String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime, long size)
    {
      String hashKey = getHashKey(storageName,entryType,name);

      WeakReference<EntryData> reference = get(hashKey);
      EntryData entryData = (reference != null) ? get(hashKey).get() : null;
      if (entryData != null)
      {
        entryData.storageName     = storageName;
        entryData.storageDateTime = storageDateTime;
        entryData.entryType       = entryType;
        entryData.name            = name;
        entryData.dateTime        = dateTime;
        entryData.size            = size;
      }
      else
      {
        entryData = new EntryData(storageName,
                                  storageDateTime,
                                  entryType,
                                  name,
                                  dateTime,
                                  size
                                 );
        put(hashKey,new WeakReference<EntryData>(entryData));
      }

      return entryData;
    }

    /** update entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     */
    synchronized public EntryData update(String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime)
    {
      String hashKey = getHashKey(storageName,entryType,name);

      WeakReference<EntryData> reference = get(hashKey);
      EntryData entryData = (reference != null) ? get(hashKey).get() : null;
      if (entryData != null)
      {
        entryData.storageName     = storageName;
        entryData.storageDateTime = storageDateTime;
        entryData.entryType       = entryType;
        entryData.name            = name;
        entryData.dateTime        = dateTime;
      }
      else
      {
        entryData = new EntryData(storageName,
                                  storageDateTime,
                                  entryType,
                                  name,
                                  dateTime
                                 );
        put(hashKey,new WeakReference<EntryData>(entryData));
      }

      return entryData;
    }

    /** get hash key from data
     * @param storageName storage name
     * @param entryType entry type
     * @param name name
     * @return hash key string
     */
    private String getHashKey(String storageName, EntryTypes entryType, String name)
    {
      return storageName+entryType.toString()+name;
    }

    /** get hash key from entry data
     * @param entryData
     * @return hash key string
     */
    private String getHashKey(EntryData entryData)
    {
      return getHashKey(entryData.storageName,entryData.entryType,entryData.name);
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
          return entryData1.entryType.compareTo(entryData2.entryType);
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
    private Object  trigger            = new Object();   // trigger update object
    private boolean triggeredFlag      = false;

    private boolean checkedStorageOnly = false;
    private int     entryMaxCount      = 100;
    private String  entryPattern       = null;
    private boolean newestEntriesOnly  = false;

    /** create update entry list thread
     */
    UpdateEntryListThread()
    {
      super();
      setDaemon(true);
      setName("BARControl Update Entry");
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
              BARControl.waitCursor();
              widgetEntryTable.setForeground(COLOR_MODIFIED);
            }
          });

          // update table
          try
          {
            updateEntryList();
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

          // reset cursor, foreground color
          display.syncExec(new Runnable()
          {
            public void run()
            {
              widgetEntryTable.setForeground(null);
              BARControl.resetCursor();
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

    /** trigger update of entry list
     * @param checkedStorageOnly checked storage only or null
     * @param entryPattern new entry pattern or null
     * @param newestEntriesOnly flag for newest entries only or null
     * @param entryMaxCount max. entries in list or null
     */
    public void triggerUpdate(boolean checkedStorageOnly, String entryPattern, boolean newestEntriesOnly, int entryMaxCount)
    {
      synchronized(trigger)
      {
        if (   (this.checkedStorageOnly != checkedStorageOnly)
            || (this.entryPattern == null) || (entryPattern == null) || !this.entryPattern.equals(entryPattern)
            || (this.newestEntriesOnly != newestEntriesOnly)
            || (this.entryMaxCount != entryMaxCount)
           )
        {
          this.checkedStorageOnly = checkedStorageOnly;
          this.entryPattern       = entryPattern;
          this.newestEntriesOnly  = newestEntriesOnly;
          this.entryMaxCount      = entryMaxCount;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of entry list
     * @param entryPattern new entry pattern or null
     */
    public void triggerUpdateEntryPattern(String entryPattern)
    {
      synchronized(trigger)
      {
        if ((this.entryPattern == null) || (entryPattern == null) || !this.entryPattern.equals(entryPattern))
        {
          this.entryPattern = entryPattern;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of entry list
     * @param checkedStorageOnly checked storage only or null
     * @param newestEntriesOnly flag for newest entries only or null
     */
    public void triggerUpdateCheckedStorageOnly(boolean checkedStorageOnly)
    {
      synchronized(trigger)
      {
        if (this.checkedStorageOnly != checkedStorageOnly)
        {
          this.checkedStorageOnly = checkedStorageOnly;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of entry list
     * @param newestEntriesOnly flag for newest entries only or null
     */
    public void triggerUpdateNewestEntriesOnly(boolean newestEntriesOnly)
    {
      synchronized(trigger)
      {
        if (this.newestEntriesOnly != newestEntriesOnly)
        {
          this.newestEntriesOnly  = newestEntriesOnly;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of entry list
     * @param entryMaxCount max. entries in list
     */
    public void triggerUpdateEntryMaxCount(int entryMaxCount)
    {
      synchronized(trigger)
      {
        if (this.entryMaxCount != entryMaxCount)
        {
          this.entryMaxCount = entryMaxCount;

          triggeredFlag = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of entry list
     */
    public void triggerUpdate()
    {
      synchronized(trigger)
      {
        triggeredFlag = true;
        trigger.notify();
      }
    }

    /** refresh entry list display
     */
    private void updateEntryList()
    {
// ??? statt findFileListIndex
//      EntryDataComparator entryDataComparator = new EntryDataComparator(widgetEntryTable);

      // get entries
      final HashSet<TableItem> removeTableItemSet = new HashSet<TableItem>();
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TableItem tableItem : widgetEntryTable.getItems())
          {
            if (!tableItem.getChecked())
            {
              removeTableItemSet.add(tableItem);
            }
          }
        }
      });
      if (triggeredFlag) return;

      // update table
      Command command = BARServer.runCommand(StringParser.format("INDEX_ENTRIES_LIST entryPattern=%'S checkedStorageOnly=%y entryMaxCount=%d newestEntriesOnly=%y",
                                                                 (((entryPattern != null) && !entryPattern.equals("")) ? entryPattern : "*"),
                                                                 checkedStorageOnly,
                                                                 entryMaxCount,
                                                                 newestEntriesOnly
                                                                ),
                                             0
                                            );
      String[] errorMessage = new String[1];
      ValueMap resultMap    = new ValueMap();
      while (!command.endOfData() && !triggeredFlag)
      {
        if (command.getNextResult(errorMessage,
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

                  // add/update entry data map
                  final EntryData entryData = entryDataMap.update(storageName,storageDateTime,EntryTypes.FILE,fileName,dateTime,size);

                  // update/insert table item
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      TableItem tableItem = Widgets.getTableItem(widgetEntryTable,entryData);
                      if (tableItem == null)
                      {
                        // insert tree item
                        tableItem = Widgets.insertTableItem(widgetEntryTable,
                                                            findEntryListIndex(entryData),
                                                            (Object)entryData
                                                           );
                      }
                      else
                      {
                        assert tableItem.getData() instanceof EntryData;

                        // keep tree item
                        removeTableItemSet.remove(tableItem);
                      }

                      // update view
                      Widgets.updateTableItem(tableItem,
                                              (Object)entryData,
                                              entryData.storageName,
                                              entryData.name,
                                              "FILE",
                                              Units.formatByteSize(entryData.size),
                                              simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                             );
                    }
                  });
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

                  // add/update entry data map
                  final EntryData entryData = entryDataMap.update(storageName,storageDateTime,EntryTypes.IMAGE,imageName,0L,size);

                  // update/insert table item
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      TableItem tableItem = Widgets.getTableItem(widgetEntryTable,entryData);
                      if (tableItem == null)
                      {
                        // insert tree item
                        tableItem = Widgets.insertTableItem(widgetEntryTable,
                                                            findEntryListIndex(entryData),
                                                            (Object)entryData
                                                           );
                      }
                      else
                      {
                        assert tableItem.getData() instanceof EntryData;

                        // keep tree item
                        removeTableItemSet.remove(tableItem);
                      }

                      // update view
                      Widgets.updateTableItem(tableItem,
                                              (Object)entryData,
                                              entryData.storageName,
                                              entryData.name,
                                              "IMAGE",
                                              Units.formatByteSize(entryData.size),
                                              simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                             );
                    }
                  });
                }
                break;
              case DIRECTORY:
                {
                  String storageName     = resultMap.getString("storageName"    );
                  long   storageDateTime = resultMap.getLong  ("storageDateTime");
                  String directoryName   = resultMap.getString("name"           );
                  long   dateTime        = resultMap.getLong  ("dateTime"       );

                  // add/update entry data map
                  final EntryData entryData = entryDataMap.update(storageName,storageDateTime,EntryTypes.DIRECTORY,directoryName,dateTime);

                  // update/insert table item
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      TableItem tableItem = Widgets.getTableItem(widgetEntryTable,entryData);
                      if (tableItem == null)
                      {
                        // insert tree item
                        tableItem = Widgets.insertTableItem(widgetEntryTable,
                                                            findEntryListIndex(entryData),
                                                            (Object)entryData
                                                           );
                      }
                      else
                      {
                        assert tableItem.getData() instanceof EntryData;

                        // keep tree item
                        removeTableItemSet.remove(tableItem);
                      }

                      // update view
                      Widgets.updateTableItem(tableItem,
                                              (Object)entryData,
                                              entryData.storageName,
                                              entryData.name,
                                              "DIR",
                                              "",
                                              simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                             );
                    }
                  });
                }
                break;
              case LINK:
                {
                  String storageName     = resultMap.getString("storageName"    );
                  long   storageDateTime = resultMap.getLong  ("storageDateTime");
                  String linkName        = resultMap.getString("name"           );
                  String destinationName = resultMap.getString("destinationName");
                  long   dateTime        = resultMap.getLong  ("dateTime"       );

                  // add/update entry data map
                  final EntryData entryData = entryDataMap.update(storageName,storageDateTime,EntryTypes.LINK,linkName,dateTime);

                  // update/insert table item
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      TableItem tableItem = Widgets.getTableItem(widgetEntryTable,entryData);
                      if (tableItem == null)
                      {
                        // insert tree item
                        tableItem = Widgets.insertTableItem(widgetEntryTable,
                                                            findEntryListIndex(entryData),
                                                            (Object)entryData
                                                           );
                      }
                      else
                      {
                        assert tableItem.getData() instanceof EntryData;

                        // keep tree item
                        removeTableItemSet.remove(tableItem);
                      }

                      // update view
                      Widgets.updateTableItem(tableItem,
                                              (Object)entryData,
                                              entryData.storageName,
                                              entryData.name,
                                              "LINK",
                                              "",
                                              simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                             );
                    }
                  });
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

                  // add/update entry data map
                  final EntryData entryData = entryDataMap.update(storageName,storageDateTime,EntryTypes.HARDLINK,fileName,dateTime,size);

                  // update/insert table item
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      TableItem tableItem = Widgets.getTableItem(widgetEntryTable,entryData);
                      if (tableItem == null)
                      {
                        // insert tree item
                        tableItem = Widgets.insertTableItem(widgetEntryTable,
                                                            findEntryListIndex(entryData),
                                                            (Object)entryData
                                                           );
                      }
                      else
                      {
                        assert tableItem.getData() instanceof EntryData;

                        // keep tree item
                        removeTableItemSet.remove(tableItem);
                      }

                      // update view
                      Widgets.updateTableItem(tableItem,
                                              (Object)entryData,
                                              entryData.storageName,
                                              entryData.name,
                                              "HARDLINK",
                                              Units.formatByteSize(entryData.size),
                                              simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                             );
                    }
                  });
                }
                break;
              case SPECIAL:
                {
                  String storageName     = resultMap.getString("storageName"    );
                  long   storageDateTime = resultMap.getLong  ("storageDateTime");
                  String name            = resultMap.getString("name"           );
                  long   dateTime        = resultMap.getLong  ("dateTime"       );

                  // add/update entry data map
                  final EntryData entryData = entryDataMap.update(storageName,storageDateTime,EntryTypes.SPECIAL,name,dateTime);

                  // update/insert table item
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      TableItem tableItem = Widgets.getTableItem(widgetEntryTable,entryData);
                      if (tableItem == null)
                      {
                        // insert tree item
                        tableItem = Widgets.insertTableItem(widgetEntryTable,
                                                            findEntryListIndex(entryData),
                                                            (Object)entryData
                                                           );
                      }
                      else
                      {
                        assert tableItem.getData() instanceof EntryData;

                        // keep tree item
                        removeTableItemSet.remove(tableItem);
                      }

                      // update view
                      Widgets.updateTableItem(tableItem,
                                              (Object)entryData,
                                              entryData.storageName,
                                              entryData.name,
                                              "DEVICE",
                                              Units.formatByteSize(entryData.size),
                                              simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                             );
                    }
                  });
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
      if (triggeredFlag) return;

      // remove not existing entries
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (TableItem tableItem : removeTableItemSet)
          {
            EntryData entryData = (EntryData)tableItem.getData();
            Widgets.removeTableItem(widgetEntryTable,tableItem);
            entryDataMap.remove(entryData);
          }
        }
      });
      if (triggeredFlag) return;

      // enable/disable restore button
      display.syncExec(new Runnable()
      {
        public void run()
        {
          checkedEntryEvent.trigger();
        }
      });
    }
  }

  // --------------------------- constants --------------------------------
  // colors
  private final Color COLOR_MODIFIED;
  private final Color COLOR_INFO_FORGROUND;
  private final Color COLOR_INFO_BACKGROUND;

  // images
  private final Image IMAGE_DIRECTORY;

  private final Image IMAGE_CLEAR;
  private final Image IMAGE_MARK_ALL;
  private final Image IMAGE_UNMARK_ALL;

  private final Image IMAGE_CONNECT0;
  private final Image IMAGE_CONNECT1;

  // date/time format
  private final SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

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

  // --------------------------- variables --------------------------------

  // global variable references
  private Shell               shell;
  private Display             display;

  // widgets
  public  Composite           widgetTab;
  private Button              widgetConnectButton;
  private TabFolder           widgetTabFolder;

  private TabFolder           widgetStorageTabFolder;
  private Tree                widgetStorageTree;
  private Shell               widgetStorageTreeToolTip = null;
  private Menu                widgetStorageTreeAssignToMenu;
  private Table               widgetStorageTable;
  private Shell               widgetStorageTableToolTip = null;
  private Menu                widgetStorageTableAssignToMenu;
  private Text                widgetStoragePattern;
  private Combo               widgetStorageState;
  private Combo               widgetStorageMaxCount;
  private WidgetEvent         checkedStorageEvent = new WidgetEvent();        // triggered when some checked storage changed

  private Table               widgetEntryTable;
  private Shell               widgetEntryTableToolTip = null;
  private WidgetEvent         checkedEntryEvent = new WidgetEvent();          // triggered when some checked entry changed

  private Button              widgetRestoreTo;
  private Text                widgetRestoreToDirectory;
  private Button              widgetOverwriteEntries;
  private WidgetEvent         selectRestoreToEvent = new WidgetEvent();

  UpdateStorageThread         updateStorageThread = new UpdateStorageThread();
  private IndexDataMap        indexDataMap        = new IndexDataMap();

  UpdateEntryListThread       updateEntryListThread = new UpdateEntryListThread();
  private EntryDataMap        entryDataMap          = new EntryDataMap();

  public ListDirectory remoteListDirectory = new ListDirectory()
  {
    /** remote file
     */
    class RemoteFile extends File
    {
      private FileTypes fileType;
      private long      size;
      private long      dateTime;

      /** create remote file
       * @param name name
       * @param fileType file type
       * @param size size [bytes]
       * @param dateTime last modified date/time
       */
      public RemoteFile(String name, FileTypes fileType, long size, long dateTime)
      {
        super(name);

        this.fileType = fileType;
        this.size     = size;
        this.dateTime = dateTime;
      }

      /** create remote file
       * @param name name
       * @param fileType file type
       * @param dateTime last modified date/time
       */
      public RemoteFile(String name, FileTypes fileType, long dateTime)
      {
        this(name,fileType,0,dateTime);
      }

      /** get file size
       * @return size [bytes]
       */
      public long length()
      {
        return size;
      }

      /** get last modified
       * @return last modified date/time
       */
      public long lastModified()
      {
        return dateTime*1000;
      }

      /** check if file is file
       * @return true iff file
       */
      public boolean isFile()
      {
        return fileType == FileTypes.FILE;
      }

      /** check if file is directory
       * @return true iff directory
       */
      public boolean isDirectory()
      {
        return fileType == FileTypes.DIRECTORY;
      }

      /** check if file is hidden
       * @return always false
       */
      public boolean isHidden()
      {
        return getName().startsWith(".");
      }

      /** check if file exists
       * @return always true
       */
      public boolean exists()
      {
        return true;
      }
    };

    private ArrayList<ValueMap> resultMapList = new ArrayList<ValueMap>();
    Iterator<ValueMap>          iterator;

    public String[] getShortcuts()
    {
      ArrayList<String> shortcutList = new ArrayList<String>();

      String[] resultErrorMessage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("ROOT_LIST"),
                                           0,
                                           resultErrorMessage,
                                           resultMapList
                                          );
      if (error == Errors.NONE)
      {
        for (ValueMap resultMap : resultMapList)
        {
          shortcutList.add(resultMap.getString("name"));
        }
      }

      return shortcutList.toArray(new String[shortcutList.size()]);
    }

    public void setShortcuts(String shortcuts[])
    {
Dprintf.dprintf("");
    }

    public boolean open(String pathName)
    {
      String[] resultErrorMessage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("FILE_LIST directory=%'S",
                                                               pathName
                                                              ),
                                           0,
                                           resultErrorMessage,
                                           resultMapList
                                          );
      if (error == Errors.NONE)
      {
        iterator = resultMapList.listIterator();
        return true;
      }
      else
      {
        return false;
      }
    }
    public void close()
    {
      iterator = null;
    }
    public File getNext()
    {
      File file = null;

      if (iterator.hasNext())
      {
        ValueMap valueMap = iterator.next();
        try
        {
          FileTypes fileType = valueMap.getEnum("fileType",FileTypes.class);
          switch (fileType)
          {
            case FILE:
              {
                String  name         = valueMap.getString ("name"         );
                long    size         = valueMap.getLong   ("size"         );
                long    dateTime     = valueMap.getLong   ("dateTime"     );
                boolean noDumpFlag   = valueMap.getBoolean("noDump", false);

                file = new RemoteFile(name,FileTypes.FILE,size,dateTime);
              }
              break;
            case DIRECTORY:
              {
                String  name         = valueMap.getString ("name"          );
                long    dateTime     = valueMap.getLong   ("dateTime"      );
                boolean noBackupFlag = valueMap.getBoolean("noBackup",false);
                boolean noDumpFlag   = valueMap.getBoolean("noDump",  false);

                file = new RemoteFile(name,FileTypes.DIRECTORY,dateTime);
              }
              break;
            case LINK:
              {
                String  name         = valueMap.getString ("name"    );
                long    dateTime     = valueMap.getLong   ("dateTime");
                boolean noDumpFlag   = valueMap.getBoolean("noDump", false);

                file = new RemoteFile(name,FileTypes.LINK,dateTime);
              }
              break;
            case HARDLINK:
              {
                String  name         = valueMap.getString ("name"    );
                long    size         = valueMap.getLong   ("size"    );
                long    dateTime     = valueMap.getLong   ("dateTime");
                boolean noDumpFlag   = valueMap.getBoolean("noDump", false);

                file = new RemoteFile(name,FileTypes.HARDLINK,size,dateTime);
              }
              break;
            case SPECIAL:
              {
                String  name         = valueMap.getString ("name"          );
                long    size         = valueMap.getLong   ("size",    0L   );
                long    dateTime     = valueMap.getLong   ("dateTime"      );
                boolean noBackupFlag = valueMap.getBoolean("noBackup",false);
                boolean noDumpFlag   = valueMap.getBoolean("noDump",  false);

                file = new RemoteFile(name,FileTypes.SPECIAL,dateTime);
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

      return file;
    }
  };

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** show entry data tool tip
   * @param entryIndexData entry index data
   * @param x,y positions
   */
  private void showEntryToolTip(EntryData entryData, int x, int y)
  {
    Label label;
    Control   control;

    final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
    final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

    if (widgetEntryTableToolTip != null)
    {
      widgetEntryTableToolTip.dispose();
    }
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
        if (widgetEntryTableToolTip != null)
        {
          widgetEntryTableToolTip.dispose();
          widgetEntryTableToolTip = null;
        }
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

    label = Widgets.newLabel(widgetEntryTableToolTip,simpleDateFormat.format(new Date(entryData.storageDateTime*1000L)));
    label.setForeground(COLOR_FORGROUND);
    label.setBackground(COLOR_BACKGROUND);
    Widgets.layout(label,1,1,TableLayoutData.WE);

    control = Widgets.newSpacer(widgetEntryTableToolTip);
    Widgets.layout(control,2,0,TableLayoutData.WE,0,2,0,0,SWT.DEFAULT,1,SWT.DEFAULT,1,SWT.DEFAULT,1);

    label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Type")+":");
    label.setForeground(COLOR_FORGROUND);
    label.setBackground(COLOR_BACKGROUND);
    Widgets.layout(label,3,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetEntryTableToolTip,entryData.entryType.toString());
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

    label = Widgets.newLabel(widgetEntryTableToolTip,(entryData.dateTime > 0) ? simpleDateFormat.format(new Date(entryData.dateTime*1000L)) : "-");
    label.setForeground(COLOR_FORGROUND);
    label.setBackground(COLOR_BACKGROUND);
    Widgets.layout(label,6,1,TableLayoutData.WE);

    Point size = widgetEntryTableToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
    widgetEntryTableToolTip.setBounds(x,y,size.x,size.y);
    widgetEntryTableToolTip.setVisible(true);
  }

  /** show entity index tool tip
   * @param entityIndexData entity index data
   * @param x,y positions
   */
  private void showEntityIndexToolTip(EntityIndexData entityIndexData, int x, int y)
  {
    Label label;

    if (widgetStorageTreeToolTip != null)
    {
      widgetStorageTreeToolTip.dispose();
    }
    widgetStorageTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
    widgetStorageTreeToolTip.setBackground(COLOR_INFO_BACKGROUND);
    widgetStorageTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
    Widgets.layout(widgetStorageTreeToolTip,0,0,TableLayoutData.NSWE);
    widgetStorageTreeToolTip.addMouseTrackListener(new MouseTrackListener()
    {
      public void mouseEnter(MouseEvent mouseEvent)
      {
      }

      public void mouseExit(MouseEvent mouseEvent)
      {
        if (widgetStorageTreeToolTip != null)
        {
          widgetStorageTreeToolTip.dispose();
          widgetStorageTreeToolTip = null;
        }
      }

      public void mouseHover(MouseEvent mouseEvent)
      {
      }
    });

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Name")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,0,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.name);
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,0,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last created")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,1,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,(entityIndexData.dateTime > 0) ? simpleDateFormat.format(new Date(entityIndexData.dateTime*1000L)) : "-");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,1,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total entries")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,2,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,String.format("%d",entityIndexData.totalEntries));
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,2,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total size")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,3,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("%d bytes (%s)"),entityIndexData.size,Units.formatByteSize(entityIndexData.size)));
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,3,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last error")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,4,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.errorMessage);
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,4,1,TableLayoutData.WE);

    Point size = widgetStorageTreeToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
    widgetStorageTreeToolTip.setBounds(x,y,size.x,size.y);
    widgetStorageTreeToolTip.setVisible(true);
  }

  /** show storage index tool tip
   * @param storageIndexData storage index data
   * @param x,y positions
   */
  private void showStorageIndexToolTip(StorageIndexData storageIndexData, int x, int y)
  {
    Label label;

    if (widgetStorageTableToolTip != null)
    {
      widgetStorageTableToolTip.dispose();
    }
    widgetStorageTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
    widgetStorageTableToolTip.setBackground(COLOR_INFO_BACKGROUND);
    widgetStorageTableToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
    Widgets.layout(widgetStorageTableToolTip,0,0,TableLayoutData.NSWE);
    widgetStorageTableToolTip.addMouseTrackListener(new MouseTrackListener()
    {
      public void mouseEnter(MouseEvent mouseEvent)
      {
      }

      public void mouseExit(MouseEvent mouseEvent)
      {
        if (widgetStorageTableToolTip != null)
        {
          widgetStorageTableToolTip.dispose();
          widgetStorageTableToolTip = null;
        }
      }

      public void mouseHover(MouseEvent mouseEvent)
      {
      }
    });

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Job")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,0,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.jobName);
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,0,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Name")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,1,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.name);
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,1,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Created")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,2,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,simpleDateFormat.format(new Date(storageIndexData.dateTime*1000L)));
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,2,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Type")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,3,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.archiveType.toString());
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,3,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Entries")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,4,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,String.format("%d",storageIndexData.entries));
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,4,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Size")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,5,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,String.format(BARControl.tr("%d bytes (%s)"),storageIndexData.size,Units.formatByteSize(storageIndexData.size)));
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,5,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("State")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,6,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.indexState.toString());
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,6,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Last checked")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,7,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,(storageIndexData.lastCheckedDateTime > 0) ? simpleDateFormat.format(new Date(storageIndexData.lastCheckedDateTime*1000L)) : "-");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,7,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Error")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,8,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.errorMessage);
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,8,1,TableLayoutData.WE);

    Point size = widgetStorageTableToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
    widgetStorageTableToolTip.setBounds(x,y,size.x,size.y);
    widgetStorageTableToolTip.setVisible(true);
  }

  /** create restore tab
   * @param parentTabFolder parent tab folder
   * @param accelerator keyboard shortcut to select tab
   */
  TabRestore(TabFolder parentTabFolder, int accelerator)
  {
    TabFolder   tabFolder;
    Composite   tab;
    Menu        menu;
    MenuItem    menuItem;
    Composite   composite,subComposite;
    Label       label;
    Button      button;
    Pane        pane;
    Group       group;
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
    COLOR_MODIFIED        = display.getSystemColor(SWT.COLOR_GRAY);
    COLOR_INFO_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
    COLOR_INFO_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

    // get images
    IMAGE_DIRECTORY  = Widgets.loadImage(display,"directory.png");

    IMAGE_CLEAR      = Widgets.loadImage(display,"clear.png");
    IMAGE_MARK_ALL   = Widgets.loadImage(display,"mark.png");
    IMAGE_UNMARK_ALL = Widgets.loadImage(display,"unmark.png");

    IMAGE_CONNECT0   = Widgets.loadImage(display,"connect0.png");
    IMAGE_CONNECT1   = Widgets.loadImage(display,"connect1.png");

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Restore")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{1.0,0.0},new double[]{0.0,1.0},2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // connector button
    widgetConnectButton = Widgets.newCheckbox(widgetTab,IMAGE_CONNECT0,IMAGE_CONNECT1);
    widgetConnectButton.setToolTipText(BARControl.tr("When this connector is in state 'closed', only tagged storage archives are used for list entries."));
    Widgets.layout(widgetConnectButton,0,0,TableLayoutData.W);
    widgetConnectButton.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        boolean checkedStorageOnly = widget.getSelection();
        updateEntryListThread.triggerUpdateCheckedStorageOnly(checkedStorageOnly);
      }
    });

    // create pane
    pane = Widgets.newPane(widgetTab,2,SWT.HORIZONTAL);
    Widgets.layout(pane,0,1,TableLayoutData.NSWE);
    pane.addResizeListener(new Listener()
    {
      public void handleEvent(Event event)
      {
        Rectangle bounds = widgetConnectButton.getBounds();
        bounds.y = event.detail-bounds.height/2+Pane.SIZE/2;
        widgetConnectButton.setBounds(bounds);
      }
    });

    // storage tree/list
    composite = pane.getComposite(0);
    composite.setLayout(new TableLayout(1.0,1.0));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);
    group = Widgets.newGroup(composite,BARControl.tr("Storage"));
    group.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,4));
    Widgets.layout(group,0,0,TableLayoutData.NSWE);
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

      widgetStorageTree = Widgets.newTree(tab,SWT.CHECK|SWT.MULTI);
      widgetStorageTree.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0}));
      Widgets.layout(widgetStorageTree,1,0,TableLayoutData.NSWE);
      SelectionListener storageTreeColumnSelectionListener = new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TreeColumn          treeColumn          = (TreeColumn)selectionEvent.widget;
          IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree,treeColumn);
          synchronized(widgetStorageTree)
          {
            Widgets.sortTreeColumn(widgetStorageTree,treeColumn,indexDataComparator);
          }
        }
      };
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("Name"),    SWT.LEFT, 450,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("Size"),    SWT.RIGHT,100,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for size."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("Modified"),SWT.LEFT, 160,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for modification date/time."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("State"),   SWT.LEFT,  60,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for state."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);

      widgetStorageTabFolder.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          updateCheckedStorageList();
          updateEntryListThread.triggerUpdate();
        }
      });
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
                     || (treeItem.getData() instanceof EntityIndexData)
                    )
            {
              // expand/collapse sub-tree
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
              // toggle check
              StorageIndexData storageIndexData = (StorageIndexData)treeItem.getData();
              storageIndexData.setChecked(!storageIndexData.isChecked());

              checkedStorageEvent.trigger();
            }
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
          if (treeItem != null)
          {
            if (selectionEvent.detail == SWT.CHECK)
            {
              IndexData indexData = (IndexData)treeItem.getData();

              // set checked
              indexData.setChecked(treeItem.getChecked());

              // set checked for sub-items: jobs, storage
              if      (indexData instanceof UUIDIndexData)
              {
                if (treeItem.getExpanded())
                {
                  for (TreeItem entityTreeItem : treeItem.getItems())
                  {
                    EntityIndexData entityIndexData = (EntityIndexData)entityTreeItem.getData();
                    entityIndexData.setChecked(indexData.isChecked());

                    if (entityTreeItem.getExpanded())
                    {
                      for (TreeItem storageTreeItem : entityTreeItem.getItems())
                      {
                        StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
                        storageIndexData.setChecked(indexData.isChecked());
                      }
                    }
                  }
                }
              }
              else if (indexData instanceof EntityIndexData)
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

              // update checked storage list
              updateCheckedStorageList();

              // trigger update checked
              checkedStorageEvent.trigger();
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
          if (widgetStorageTreeToolTip != null)
          {
            widgetStorageTreeToolTip.dispose();
            widgetStorageTreeToolTip = null;
          }
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
          if ((treeItem != null) && (mouseEvent.x < tree.getBounds().width/2))
          {
            if      (treeItem.getData() instanceof UUIDIndexData)
            {
              // TODO: show something?
            }
            else if (treeItem.getData() instanceof EntityIndexData)
            {
              EntityIndexData entityIndexData = (EntityIndexData)treeItem.getData();

              Point point = tree.toDisplay(mouseEvent.x+16,mouseEvent.y);
              showEntityIndexToolTip(entityIndexData,point.x,point.y);
            }
            else if (treeItem.getData() instanceof StorageIndexData)
            {
              StorageIndexData storageIndexData = (StorageIndexData)treeItem.getData();

              Point point = tree.toDisplay(mouseEvent.x+16,mouseEvent.y);
              showStorageIndexToolTip(storageIndexData,point.x,point.y);
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
          else if (Widgets.isAccelerator(keyEvent,SWT.CR) || Widgets.isAccelerator(keyEvent,SWT.KEYPAD_CR))
          {
            // expand/collaps sub-item
            for (TreeItem treeItem : widgetStorageTree.getSelection())
            {
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
          }
          else if (Widgets.isAccelerator(keyEvent,SWT.SPACE))
          {
            // toggle check
            for (TreeItem treeItem : widgetStorageTree.getSelection())
            {
              IndexData indexData = (IndexData)treeItem.getData();
              indexData.setChecked(!indexData.isChecked());

              Event treeEvent = new Event();
              treeEvent.item   = treeItem;
              treeEvent.detail = SWT.CHECK;
              widgetStorageTree.notifyListeners(SWT.Selection,treeEvent);
            }
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
      SelectionListener storageTableColumnSelectionListener = new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableColumn         tableColumn         = (TableColumn)selectionEvent.widget;
          IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTable,tableColumn);
          synchronized(widgetStorageTable)
          {
            Widgets.sortTableColumn(widgetStorageTable,tableColumn,indexDataComparator);
          }
        }
      };
      tableColumn = Widgets.addTableColumn(widgetStorageTable,0,"Name",    SWT.LEFT, 450,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,1,"Size",    SWT.RIGHT,100,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for size."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,2,"Modified",SWT.LEFT, 150,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for modification date/time."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,3,"State",   SWT.LEFT,  60,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for state."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
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

            // update checked storage list
            updateCheckedStorageList();

            // trigger update checked
            checkedStorageEvent.trigger();
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
          if (widgetStorageTableToolTip != null)
          {
            widgetStorageTableToolTip.dispose();
            widgetStorageTableToolTip = null;
          }
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
          if ((tableItem != null) && (mouseEvent.x < table.getBounds().width/2))
          {
            StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();

            Point point = table.toDisplay(mouseEvent.x+16,mouseEvent.y);
            showStorageIndexToolTip(storageIndexData,point.x,point.y);
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
        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Refresh index\u2026"));
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

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Refresh all indizes with error\u2026"));
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

        widgetStorageTreeAssignToMenu = Widgets.addMenu(menu,BARControl.tr("Assign to job\u2026"));
        {
        }

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add to index\u2026"));
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

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove from index\u2026"));
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

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove all indizes with error\u2026"));
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

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Mark all"));
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setCheckedAllStorage(true);
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Unmark all"));
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setCheckedAllStorage(false);
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Restore"));
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

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Delete\u2026"));
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
      menu.addMenuListener(new MenuListener()
      {
        public void menuShown(MenuEvent menuEvent)
        {
          if (widgetStorageTreeToolTip != null)
          {
            widgetStorageTreeToolTip.dispose();
            widgetStorageTreeToolTip = null;
          }
          if (widgetStorageTableToolTip != null)
          {
            widgetStorageTableToolTip.dispose();
            widgetStorageTableToolTip = null;
          }
        }
        public void menuHidden(MenuEvent menuEvent)
        {
        }
      });
      widgetStorageTree.setMenu(menu);
      widgetStorageTable.setMenu(menu);

      // storage tree filters
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
              setCheckedAllStorage(false);
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText(BARControl.tr("Mark all entries in list."));
            }
            else
            {
              setCheckedAllStorage(true);
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

        widgetStorageState = Widgets.newOptionMenu(composite);
        widgetStorageState.setToolTipText(BARControl.tr("Storage states filter."));
        widgetStorageState.setItems(new String[]{"*","ok","error","update","update requested","update/update requested","error/update/update requested","not assigned"});
        widgetStorageState.setText("*");
        Widgets.layout(widgetStorageState,0,4,TableLayoutData.W);
        widgetStorageState.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo widget = (Combo)selectionEvent.widget;
            IndexStateSet storageIndexStateSet;
            EntityStates storageEntityState;
            switch (widget.getSelectionIndex())
            {
              case 0:  storageIndexStateSet = INDEX_STATE_SET_ALL;                                                                  storageEntityState = EntityStates.ANY;  break;
              case 1:  storageIndexStateSet = new IndexStateSet(IndexStates.OK);                                                    storageEntityState = EntityStates.ANY;  break;
              case 2:  storageIndexStateSet = new IndexStateSet(IndexStates.ERROR);                                                 storageEntityState = EntityStates.ANY;  break;
              case 3:  storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE);                                                storageEntityState = EntityStates.ANY;  break;
              case 4:  storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE_REQUESTED);                                      storageEntityState = EntityStates.ANY;  break;
              case 5:  storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE,IndexStates.UPDATE_REQUESTED);                   storageEntityState = EntityStates.ANY;  break;
              case 6:  storageIndexStateSet = new IndexStateSet(IndexStates.ERROR,IndexStates.UPDATE,IndexStates.UPDATE_REQUESTED); storageEntityState = EntityStates.ANY;  break;
              case 7:  storageIndexStateSet = INDEX_STATE_SET_ALL;                                                                  storageEntityState = EntityStates.NONE; break;
              default: storageIndexStateSet = new IndexStateSet(IndexStates.UNKNOWN);                                               storageEntityState = EntityStates.ANY;  break;

            }
            updateStorageThread.triggerUpdateStorageState(storageIndexStateSet,storageEntityState);
          }
        });
        updateStorageThread.triggerUpdateStorageState(INDEX_STATE_SET_ALL,EntityStates.ANY);

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
            try
            {
              int storageMaxCount = Integer.parseInt(widget.getText());
              updateStorageThread.triggerUpdateStorageMaxCount(storageMaxCount);
            }
            catch (NumberFormatException exception)
            {
              // ignored
            }
          }
        });
        updateStorageThread.triggerUpdateStorageMaxCount(100);

        button = Widgets.newButton(composite,BARControl.tr("Restore"));
        button.setToolTipText(BARControl.tr("Start restoring selected archives."));
        button.setEnabled(false);
        Widgets.layout(button,0,7,TableLayoutData.DEFAULT,0,0,0,0,80,SWT.DEFAULT);
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
    composite = pane.getComposite(1);
    composite.setLayout(new TableLayout(1.0,1.0));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);

    group = Widgets.newGroup(composite,BARControl.tr("Entries"));
    group.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,4));
    Widgets.layout(group,0,0,TableLayoutData.NSWE);
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
            {
              BARControl.waitCursor();
            }
            try
            {
              Widgets.sortTableColumn(widgetEntryTable,tableColumn,entryDataComparator);
            }
            finally
            {
              BARControl.resetCursor();
            }
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
          if (widgetEntryTableToolTip != null)
          {
            widgetEntryTableToolTip.dispose();
            widgetEntryTableToolTip = null;
          }
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

            Point point = table.toDisplay(mouseEvent.x+16,mouseEvent.y);
            showEntryToolTip(entryData,point.x,point.y);
          }
        }
      });
      Widgets.addEventListener(new WidgetEventListener(widgetEntryTable,checkedStorageEvent)
      {
        public void trigger(Control control)
        {
          updateEntryListThread.triggerUpdate();
        }
      });

      menu = Widgets.newPopupMenu(shell);
      {
        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Mark all"));
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

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Unmark all"));
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

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Restore"));
        menuItem.setEnabled(false);
        Widgets.addEventListener(new WidgetEventListener(menuItem,checkedEntryEvent)
        {
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(isSomeEntryChecked());
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
      menu.addMenuListener(new MenuListener()
      {
        public void menuShown(MenuEvent menuEvent)
        {
          if (widgetStorageTreeToolTip != null)
          {
            widgetStorageTreeToolTip.dispose();
            widgetStorageTreeToolTip = null;
          }
          if (widgetEntryTableToolTip != null)
          {
            widgetEntryTableToolTip.dispose();
            widgetEntryTableToolTip = null;
          }
        }
        public void menuHidden(MenuEvent menuEvent)
        {
        }
      });
      widgetEntryTable.setMenu(menu);

      // entry list filters
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
            if (isSomeEntryChecked())
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
            if (isSomeEntryChecked())
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
            boolean newestEntriesOnly = widget.getSelection();
            updateEntryListThread.triggerUpdateNewestEntriesOnly(newestEntriesOnly);
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
            try
            {
              int entryMaxCount = Integer.parseInt(widget.getText());
              updateEntryListThread.triggerUpdateEntryMaxCount(entryMaxCount);
            }
            catch (NumberFormatException exception)
            {
              // ignored
            }
          }
        });
        updateEntryListThread.triggerUpdateEntryMaxCount(100);

        button = Widgets.newButton(composite,BARControl.tr("Restore"));
        button.setToolTipText(BARControl.tr("Start restoring selected entries."));
        button.setEnabled(false);
        Widgets.layout(button,0,6,TableLayoutData.DEFAULT,0,0,0,0,80,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,checkedEntryEvent)
        {
          public void trigger(Control control)
          {
            control.setEnabled(isSomeEntryChecked());
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
    Widgets.layout(group,1,0,TableLayoutData.WE,0,2);
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
          String pathName;
          if ((selectionEvent.stateMask & SWT.CTRL) == 0)
          {
            pathName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.DIRECTORY,
                                    BARControl.tr("Select path"),
                                    widgetRestoreTo.getText(),
                                    remoteListDirectory
                                   );
          }
          else
          {
           pathName = Dialogs.directory(shell,
                                        BARControl.tr("Select path"),
                                        widgetRestoreTo.getText()
                                       );
          }
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

    // start storage/entry update threads
    updateStorageThread.start();
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
        // tree view
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          UUIDIndexData uuidIndexData = (UUIDIndexData)uuidTreeItem.getData();
          if (uuidIndexData.isChecked())
          {
            BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD jobUUID=%'S",uuidIndexData.jobUUID),0);
          }

          if (uuidTreeItem.getExpanded())
          {
            for (TreeItem entityTreeItem : uuidTreeItem.getItems())
            {
              EntityIndexData entityIndexData = (EntityIndexData)entityTreeItem.getData();
              if (entityIndexData.isChecked())
              {
                BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD entityId=%d",entityIndexData.entityId),0);
              }

              if (entityTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : entityTreeItem.getItems())
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
        // list view
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

  /** set/clear checked all storage entries
   * @param checked true for set checked, false for clear checked
   */
  private void setCheckedAllStorage(boolean checked)
  {
    // set checked
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        // tree view
        TreeItem treeItems[] = widgetStorageTree.getSelection();
        if (treeItems.length <= 0) treeItems = widgetStorageTree.getItems();

        for (TreeItem treeItem : treeItems)
        {
          IndexData indexData = (IndexData)treeItem.getData();

          if      (indexData instanceof UUIDIndexData)
          {
            UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;
            uuidIndexData.setChecked(checked);

            if (treeItem.getExpanded())
            {
              for (TreeItem entityTreeItem : treeItem.getItems())
              {
                EntityIndexData entityIndexData = (EntityIndexData)entityTreeItem.getData();
                entityIndexData.setChecked(checked);

                if (entityTreeItem.getExpanded())
                {
                  for (TreeItem storageTreeItem : entityTreeItem.getItems())
                  {
                    StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
                    storageIndexData.setChecked(checked);
                  }
                }
              }
            }
          }
          else if (indexData instanceof EntityIndexData)
          {
            EntityIndexData entityIndexData = (EntityIndexData)indexData;
            entityIndexData.setChecked(checked);

            if (treeItem.getExpanded())
            {
              for (TreeItem storageTreeItem : treeItem.getItems())
              {
                StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
                storageIndexData.setChecked(checked);
              }
            }
          }
          else if (indexData instanceof StorageIndexData)
          {
            StorageIndexData storageIndexData = (StorageIndexData)indexData;
            storageIndexData.setChecked(checked);
          }
        }
        break;
      case 1:
        // list view
        for (TableItem tableItem : widgetStorageTable.getItems())
        {
          StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
          storageIndexData.setChecked(checked);
        }
        break;
    }

    // update checked storage list
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
        // tree view
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
        // list view
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
        // tree view
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          indexData = (IndexData)uuidTreeItem.getData();
          if      (!uuidTreeItem.getGrayed() && indexData.isChecked())
          {
            indexDataHashSet.add(indexData);
          }
          else if (uuidTreeItem.getExpanded())
          {
            for (TreeItem entityTreeItem : uuidTreeItem.getItems())
            {
              indexData = (IndexData)entityTreeItem.getData();
              if      (!entityTreeItem.getGrayed() && indexData.isChecked())
              {
                indexDataHashSet.add(indexData);
              }
              else if (entityTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : entityTreeItem.getItems())
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
        // list view
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
        // tree view
        IndexData indexData;
        for (TreeItem treeItem : widgetStorageTree.getSelection())
        {
          indexData = (IndexData)treeItem.getData();

          indexDataHashSet.add(indexData);

          if      (indexData instanceof UUIDIndexData)
          {
          }
          else if (indexData instanceof EntityIndexData)
          {
          }
          else if (indexData instanceof StorageIndexData)
          {
          }
        }
        break;
      case 1:
        // list view
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
        // tree view
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          if (!uuidTreeItem.getGrayed() && uuidTreeItem.getChecked()) return true;

          if (uuidTreeItem.getExpanded())
          {
            for (TreeItem entityTreeItem : uuidTreeItem.getItems())
            {
              if (!entityTreeItem.getGrayed() && entityTreeItem.getChecked()) return true;

              if (entityTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : entityTreeItem.getItems())
                {
                  if (!storageTreeItem.getGrayed() && storageTreeItem.getChecked()) return true;
                }
              }
            }
          }
        }
        break;
      case 1:
        // list view
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

  /** update storage tree item
   * @param treeItem tree item to update
   * @param storagePattern storage pattern or null
   */
  private void updateStorageTree(final TreeItem treeItem, String storagePattern)
  {
    {
      BARControl.waitCursor();
    }
    try
    {
      try
      {
        String[] errorMessage = new String[1];
        ValueMap resultMap    = new ValueMap();

        if      (treeItem.getData() instanceof UUIDIndexData)
        {
          // get job index data
          final HashSet<TreeItem> removeEntityTreeItemSet = new HashSet<TreeItem>();
          display.syncExec(new Runnable()
          {
            public void run()
            {
              for (TreeItem entityTreeItem : treeItem.getItems())
              {
                assert entityTreeItem.getData() instanceof EntityIndexData;
                removeEntityTreeItemSet.add(entityTreeItem);
              }
            }
          });

          // update job list
          UUIDIndexData uuidIndexData = (UUIDIndexData)treeItem.getData();
          Command command = BARServer.runCommand(StringParser.format("INDEX_ENTITY_LIST jobUUID=%'S pattern=%'S",
                                                                     uuidIndexData.jobUUID,
                                                                     (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                                    ),
                                                 0
                                                );
          while (!command.endOfData())
          {
            if (command.getNextResult(errorMessage,
                                      resultMap,
                                      Command.TIMEOUT
                                     ) == Errors.NONE
               )
            {
              try
              {
                long                  entityId         = resultMap.getLong  ("entityId"                               );
                String                jobUUID          = resultMap.getString("jobUUID"                                );
                String                scheuduleUUID    = resultMap.getString("scheduleUUID"                           );
                Settings.ArchiveTypes archiveType      = resultMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
                long                  lastDateTime     = resultMap.getLong  ("lastDateTime"                           );
                long                  totalEntries     = resultMap.getLong  ("totalEntries"                           );
                long                  totalSize        = resultMap.getLong  ("totalSize"                              );
                String                lastErrorMessage = resultMap.getString("lastErrorMessage"                       );

                // add/update job data index
                final EntityIndexData entityIndexData = indexDataMap.updateEntityIndexData(entityId,
                                                                                           archiveType,
                                                                                           lastDateTime,
                                                                                           totalEntries,
                                                                                           totalSize,
                                                                                           lastErrorMessage
                                                                                          );

                // insert/update tree item
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    TreeItem entityTreeItem = Widgets.getTreeItem(widgetStorageTree,entityIndexData);
                    if (entityTreeItem == null)
                    {
                      // insert tree item
                      entityTreeItem = Widgets.insertTreeItem(treeItem,
                                                              findStorageTreeIndex(treeItem,entityIndexData),
                                                              (Object)entityIndexData,
                                                              true
                                                             );
                      entityIndexData.setTreeItem(entityTreeItem);
                    }
                    else
                    {
                      assert entityTreeItem.getData() instanceof EntityIndexData;

                      // keep tree item
                      removeEntityTreeItemSet.remove(entityTreeItem);
                    }

                    // update view
                    entityIndexData.update();
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
              for (TreeItem treeItem : removeEntityTreeItemSet)
              {
                IndexData indexData = (IndexData)treeItem.getData();
                Widgets.removeTreeItem(widgetStorageTree,treeItem);
                indexData.clearTreeItem();
              }
            }
          });
        }
        else if (treeItem.getData() instanceof EntityIndexData)
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
          EntityIndexData entityIndexData = (EntityIndexData)treeItem.getData();
          Command command = BARServer.runCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%d maxCount=%d indexState=%s indexMode=%s pattern=%'S",
                                                                     entityIndexData.entityId,
                                                                     -1,
                                                                     "*",
                                                                     "*",
                                                                     (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*")
                                                                    ),
                                                 0
                                                );
          while (!command.endOfData())
          {
            if (command.getNextResult(errorMessage,
                                      resultMap,
                                      Command.TIMEOUT
                                     ) == Errors.NONE
               )
            {
              try
              {
                long                  storageId           = resultMap.getLong  ("storageId"                              );
                String                jobUUID             = resultMap.getString("jobUUID"                                );
                String                scheduleUUID        = resultMap.getString("scheduleUUID"                           );
                String                jobName             = resultMap.getString("jobName"                                );
                Settings.ArchiveTypes archiveType         = resultMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
                String                name                = resultMap.getString("name"                                   );
                long                  dateTime            = resultMap.getLong  ("dateTime"                               );
                long                  entries             = resultMap.getLong  ("entries"                                );
                long                  size                = resultMap.getLong  ("size"                                   );
                IndexStates           indexState          = resultMap.getEnum  ("indexState",IndexStates.class           );
                IndexModes            indexMode           = resultMap.getEnum  ("indexMode",IndexModes.class             );
                long                  lastCheckedDateTime = resultMap.getLong  ("lastCheckedDateTime"                    );
                String                errorMessage_       = resultMap.getString("errorMessage"                           );

                // add/update storage data
                final StorageIndexData storageIndexData = indexDataMap.updateStorageIndexData(storageId,
                                                                                              jobName,
                                                                                              archiveType,
                                                                                              name,
                                                                                              dateTime,
                                                                                              entries,
                                                                                              size,
                                                                                              indexState,
                                                                                              indexMode,
                                                                                              lastCheckedDateTime,
                                                                                              errorMessage_
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
    }
    finally
    {
      BARControl.resetCursor();
    }
  }

  /** set storage filter pattern
   * @param string pattern string
   */
  private void setStoragePattern(String string)
  {
    string = string.trim();
    if (string.length() > 0)
    {
      updateStorageThread.triggerUpdateStoragePattern(string);
    }
    else
    {
      updateStorageThread.triggerUpdateStoragePattern(null);
    }
  }

  /** assing storage to entity
   * @param toUUIDIndexData UUID index data
   * @param archiveType archive type
   */
  private void assignStorage(UUIDIndexData toUUIDIndexData, Settings.ArchiveTypes archiveType)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    getCheckedIndexData(indexDataHashSet);
    getSelectedIndexData(indexDataHashSet);
    if (!indexDataHashSet.isEmpty())
    {
      try
      {
        for (IndexData indexData : indexDataHashSet)
        {
          final String info = indexData.getInfo();

          int      error        = Errors.UNKNOWN;
          String[] errorMessage = new String[1];
          if      (indexData instanceof UUIDIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toJobUUID=%'S toEntityId=0 archiveType=%s jobUUID=%'S",
                                                                 toUUIDIndexData.jobUUID,
                                                                 archiveType.toString(),
                                                                 ((UUIDIndexData)indexData).jobUUID
                                                                ),
                                             0,
                                             errorMessage
                                            );
          }
          else if (indexData instanceof EntityIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toJobUUID=%'S toEntityId=0 archiveType=%s entityId=%lld",
                                                                 toUUIDIndexData.jobUUID,
                                                                 archiveType.toString(),
                                                                 ((EntityIndexData)indexData).entityId
                                                                ),
                                             0,
                                             errorMessage
                                            );
          }
          else if (indexData instanceof StorageIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toJobUUID=%'S toEntityId=0 archiveType=%s storageId=%lld",
                                                                 toUUIDIndexData.jobUUID,
                                                                 archiveType.toString(),
                                                                 ((StorageIndexData)indexData).storageId
                                                                ),
                                             0,
                                             errorMessage
                                            );
          }
          if (error != Errors.NONE)
          {
            Dialogs.error(shell,BARControl.tr("Cannot assign index for\n\n''{0}''\n\n(error: {1})",info,errorMessage[0]));
          }
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Communication error while assigning index database\n\n(error: {0})",error.toString()));
      }
      updateStorageThread.triggerUpdate();
    }
  }

  /** assing storage to entity
   * @param toEntityIndexData entity index data
   */
  private void assignStorage(EntityIndexData toEntityIndexData)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    getCheckedIndexData(indexDataHashSet);
    getSelectedIndexData(indexDataHashSet);
    if (!indexDataHashSet.isEmpty())
    {
      try
      {
        for (IndexData indexData : indexDataHashSet)
        {
          final String info = indexData.getInfo();

          int      error        = Errors.UNKNOWN;
          String[] errorMessage = new String[1];
          if      (indexData instanceof UUIDIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toEntityId=%lld jobUUID=%'S",
                                                                 toEntityIndexData.entityId,
                                                                 ((UUIDIndexData)indexData).jobUUID
                                                                ),
                                             0,
                                             errorMessage
                                            );
          }
          else if (indexData instanceof EntityIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toEntityId=%lld entityId=%lld",
                                                                 toEntityIndexData.entityId,
                                                                 ((EntityIndexData)indexData).entityId
                                                                ),
                                             0,
                                             errorMessage
                                            );
          }
          else if (indexData instanceof StorageIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toEntityId=%lld storageId=%lld",
                                                                 toEntityIndexData.entityId,
                                                                 ((StorageIndexData)indexData).storageId
                                                                ),
                                             0,
                                             errorMessage
                                            );
          }
          if (error == Errors.NONE)
          {
            indexData.setState(IndexStates.UPDATE_REQUESTED);
          }
          else
          {
            Dialogs.error(shell,BARControl.tr("Cannot assign index for\n\n''{0}''\n\n(error: {1})",info,errorMessage[0]));
          }
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Communication error while assigning index database\n\n(error: {0})",error.toString()));
      }
      updateStorageThread.triggerUpdate();
    }
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
        if (Dialogs.confirm(shell,BARControl.tr("Refresh index for {0} {0,choice,0#entries|1#entry|1<entries}?",indexDataHashSet.size())))
        {
          for (IndexData indexData : indexDataHashSet)
          {
            final String info = indexData.getInfo();

            int      error        = Errors.UNKNOWN;
            String[] errorMessage = new String[1];
            if      (indexData instanceof UUIDIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s jobUUID=%'S",
                                                                   "*",
                                                                   ((UUIDIndexData)indexData).jobUUID
                                                                  ),
                                               0,
                                               errorMessage
                                              );
            }
            else if (indexData instanceof EntityIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s entityId=%d",
                                                                   "*",
                                                                   ((EntityIndexData)indexData).entityId
                                                                  ),
                                               0,
                                               errorMessage
                                              );
            }
            else if (indexData instanceof StorageIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s storageId=%d",
                                                                   "*",
                                                                   ((StorageIndexData)indexData).storageId
                                                                  ),
                                               0,
                                               errorMessage
                                              );
            }
            if (error == Errors.NONE)
            {
              indexData.setState(IndexStates.UPDATE_REQUESTED);
            }
            else
            {
              Dialogs.error(shell,BARControl.tr("Cannot refresh index for\n\n''{0}''\n\n(error: {1})",info,errorMessage[0]));
            }
          }
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,BARControl.tr("Communication error while refreshing index database\n\n(error: {0})",error.toString()));
    }
  }

  /** refresh all storage from index database with error
   */
  private void refreshAllWithErrorStorageIndex()
  {
    try
    {
      if (Dialogs.confirm(shell,BARControl.tr("Refresh all indizes with error state?")))
      {
        String[] errorMessage = new String[1];
        int error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s storageId=%d",
                                                                 "ERROR",
                                                                 0
                                                                ),
                                             0,
                                             errorMessage
                                            );
        if (error == Errors.NONE)
        {
          updateStorageThread.triggerUpdate();
        }
        else
        {
          Dialogs.error(shell,BARControl.tr("Cannot refresh database indizes with error state (error: {0})",errorMessage[0]));
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,BARControl.tr("Communication error while refreshing database indizes\n\n(error: {0})",error.toString()));
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
    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Add storage to index database"),400,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

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
                                             BARControl.tr("Select local storage file"),
                                             widgetStorageName.getText(),
                                             new String[]{BARControl.tr("BAR files"),"*.bar",
                                                          BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
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
      String[] errorMessage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ADD name=%S",storageName),
                                           0,
                                           errorMessage
                                          );
      if (error == Errors.NONE)
      {
        updateStorageThread.triggerUpdate();
      }
      else
      {
        Dialogs.error(shell,BARControl.tr("Cannot add index database for storage file\n\n''{0}''\n\n(error: {1})",storageName,errorMessage[0]));
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
      if (Dialogs.confirm(shell,BARControl.tr("Remove index of {0} {0,choice,0#entries|1#entry|1<entries}?",indexDataHashSet.size())))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Remove indizes"),500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
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
                int            error        = Errors.UNKNOWN;
                final String[] errorMessage = new String[1];
                if      (indexData instanceof UUIDIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REMOVE state=* jobUUID=%'S",
                                                                       ((UUIDIndexData)indexData).jobUUID
                                                                      ),
                                                   0,
                                                   errorMessage
                                                  );
                }
                else if (indexData instanceof EntityIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REMOVE state=* entityId=%d",
                                                                        ((EntityIndexData)indexData).entityId
                                                                       ),
                                                    0,
                                                    errorMessage
                                                   );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REMOVE state=* storageId=%d",
                                                                       ((StorageIndexData)indexData).storageId
                                                                      ),
                                                   0,
                                                   errorMessage
                                                  );
                }
                if (error == Errors.NONE)
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
                      Dialogs.error(shell,BARControl.tr("Cannot remove index for\n\n''{0}''\n\n(error: {1})",info,errorMessage[0]));
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
                  Dialogs.error(shell,BARControl.tr("Communication error while removing database indizes\n\n(error: {0})",errorMessage));
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
      final String[] errorMessage = new String[1];
      ValueMap       resultMap    = new ValueMap();
      if (BARServer.executeCommand("INDEX_STORAGE_INFO",
                                   0,
                                   errorMessage,
                                   resultMap
                                  ) != Errors.NONE
         )
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            Dialogs.error(shell,BARControl.tr("Cannot get database indizes with error state (error: {0})",errorMessage[0]));
          }
        });
        return;
      }
      long errorCount = resultMap.getLong("errorCount");

      if (errorCount > 0)
      {
        if (Dialogs.confirm(shell,BARControl.tr("Remove {0} {0,choice,0#indizes|1#index|1<indizes} with error state?",errorCount)))
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
                final String[] errorMessage = new String[1];
                ValueMap       resultMap    = new ValueMap();

                // remove indizes with error state
                Command command = BARServer.runCommand("INDEX_STORAGE_REMOVE state=ERROR storageId=0",0);

                long n = 0;
                while (!command.endOfData() && !busyDialog.isAborted())
                {
                  if (command.getNextResult(errorMessage,
                                            resultMap,
                                            Command.TIMEOUT
                                           ) == Errors.NONE
                     )
                  {
                    try
                    {
                      long        storageId = resultMap.getLong  ("storageId");
                      String      name      = resultMap.getString("name"     );

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
                        Dialogs.error(shell,BARControl.tr("Cannot remove database indizes with error state (error: {0})",errorMessage[0]));
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
                    Dialogs.error(shell,BARControl.tr("Communication error while removing database indizes\n\n(error: {0})",errorMessage));
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
      Dialogs.error(shell,BARControl.tr("Communication error while removing database indizes\n\n(error: {0})",error.toString()));
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
      // get number of entries
      int  entries = 0;
      long size    = 0L;
      for (IndexData indexData : indexDataHashSet)
      {
        if      (indexData instanceof UUIDIndexData)
        {
          entries += ((UUIDIndexData)indexData).totalEntries;
          size    += ((UUIDIndexData)indexData).size;
        }
        else if (indexData instanceof EntityIndexData)
        {
          entries += ((EntityIndexData)indexData).totalEntries;
          size    += ((EntityIndexData)indexData).size;
        }
        else if (indexData instanceof StorageIndexData)
        {
          entries += ((StorageIndexData)indexData).entries;
          size    += ((StorageIndexData)indexData).size;
        }
      }

      // confirm
      if (Dialogs.confirm(shell,BARControl.tr("Delete {0} {0,choice,0#jobs/entities/storage files|1#job/entity/storage file|1<jobs/entities/storage files} with {1} {1,choice,0#entries|1#entry|1<entries}/{2} {2,choice,0#bytes|1#byte|1<bytes}?",indexDataHashSet.size(),entries,size)))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,"Delete storage indizes and storage files",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
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
                int            error        = Errors.UNKNOWN;
                final String[] errorMessage = new String[1];
                if      (indexData instanceof UUIDIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("STORAGE_DELETE jobUUID=%'S",
                                                                       ((UUIDIndexData)indexData).jobUUID
                                                                      ),
                                                   0,
                                                   errorMessage
                                                  );
                }
                else if (indexData instanceof EntityIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("STORAGE_DELETE entityId=%d",
                                                                       ((EntityIndexData)indexData).entityId
                                                                      ),
                                                   0,
                                                   errorMessage
                                                  );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("STORAGE_DELETE storageId=%d",
                                                                        ((StorageIndexData)indexData).storageId
                                                                       ),
                                                    0,
                                                    errorMessage
                                                   );
                }
                if (error == Errors.NONE)
                {
                  indexDataMap.remove(indexData);
                  Widgets.removeTreeItem(widgetStorageTree,indexData);
                  indexData.clearTreeItem();
                  Widgets.removeTableItem(widgetStorageTable,indexData);
                  indexData.clearTableItem();
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
                                                        BARControl.tr("Confirmation"),
                                                        BARControl.tr("Cannot delete storage\n\n''{0}''\n\n(error: {1})",info,errorMessage[0]),
                                                        new String[]{BARControl.tr("Continue"),BARControl.tr("Continue with all"),BARControl.tr("Abort")},
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
                                        BARControl.tr("Cannot delete storage\n\n''{0}''\n\n(error: {1})",info,errorMessage[0])
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
                  Dialogs.error(shell,BARControl.tr("Communication error while deleting storage\n\n(error: {0})",errorMessage));
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

  /** restore archives
   * @param storageNamesHashSet storage name hash set
   * @param directory destination directory or ""
   * @param overwriteFiles true to overwrite existing files
   */
  private void restoreArchives(HashSet<String> storageNamesHashSet, String directory, boolean overwriteFiles)
  {
    BARControl.waitCursor();
    final BusyDialog busyDialog = new BusyDialog(shell,"Restore archives",500,100,null,BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.PROGRESS_BAR1);

    new BackgroundTask(busyDialog,new Object[]{storageNamesHashSet,directory,overwriteFiles})
    {
      public void run(final BusyDialog busyDialog, Object userData)
      {
        final HashSet<String> storageNamesHashSet = (HashSet<String>)((Object[])userData)[0];
        final String          directory           = (String         )((Object[])userData)[1];
        final boolean         overwriteFiles      = (Boolean        )((Object[])userData)[2];

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
              command = BARServer.runCommand(StringParser.format("RESTORE storageName=%'S destination=%'S overwriteFiles=%y",
                                                                 storageName,
                                                                 directory,
                                                                 overwriteFiles
                                                                ),
                                             0
                                            );

              // read results, update/add data
              String[] errorMessage = new String[1];
              ValueMap resultMap    = new ValueMap();
              while (   !command.endOfData()
                     && !busyDialog.isAborted()
                    )
              {
                if (command.getNextResult(errorMessage,
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
                    Dialogs.error(shell,BARControl.tr("Cannot restore archive\n\n''{0}''\n\n(error: {1})",storageName,errorText));
                  }
                });
              }
            }
            else
            {
              busyDialog.updateText(BARControl.tr("Aborting\u2026"));
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
              BARControl.resetCursor();
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
              BARControl.resetCursor();
              Dialogs.error(shell,BARControl.tr("Error while restoring archives:\n\n{0}",errorMessage));
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
   * @return checked data entries
   */
  private EntryData[] getCheckedEntries()
  {
    ArrayList<EntryData> entryDataArray = new ArrayList<EntryData>();

    for (TableItem tableItem : widgetEntryTable.getItems())
    {
      if (tableItem.getChecked())
      {
        entryDataArray.add((EntryData)tableItem.getData());
      }
    }

    return entryDataArray.toArray(new EntryData[entryDataArray.size()]);
  }

  /** check if some data entry is checked
   * @return tree iff some entry is checked
   */
  private boolean isSomeEntryChecked()
  {
    for (TableItem tableItem : widgetEntryTable.getItems())
    {
      if (tableItem.getChecked())
      {
        return true;
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
      for (WeakReference<EntryData> entryDataR : entryDataMap.values())
      {
        EntryData entryData = entryDataR.get();
        switch (entryData.entryType)
        {
          case FILE:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryData),
                                    (Object)entryData,
                                    entryData.storageName,
                                    entryData.name,
                                    "FILE",
                                    Units.formatByteSize(entryData.size),
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000L))
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
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000L))
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
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000L))
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
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000L))
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
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000L))
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
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000L))
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
                                    simpleDateFormat.format(new Date(entryData.dateTime*1000L))
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
      updateEntryListThread.triggerUpdateEntryPattern(string);
    }
    else
    {
      updateEntryListThread.triggerUpdateEntryPattern(null);
    }
  }

  /** restore entries
   * @param entryData entries to restore
   * @param directory destination directory or ""
   * @param overwriteFiles true to overwrite existing files
   */
  private void restoreEntries(EntryData entryData[], String directory, boolean overwriteFiles)
  {
    BARControl.waitCursor();

    final BusyDialog busyDialog = new BusyDialog(shell,"Restore entries",500,100,null,BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.PROGRESS_BAR1);

    new BackgroundTask(busyDialog,new Object[]{entryData,directory,overwriteFiles})
    {
      public void run(final BusyDialog busyDialog, Object userData)
      {
        final String[] MAP_FROM = new String[]{"\n","\r","\\"};
        final String[] MAP_TO   = new String[]{"\\n","\\r","\\\\"};

        final EntryData[] entryData_     = (EntryData[])((Object[])userData)[0];
        final String      directory      = (String     )((Object[])userData)[1];
        final boolean     overwriteFiles = (Boolean    )((Object[])userData)[2];

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
              command = BARServer.runCommand(StringParser.format("RESTORE storageName=%'S destination=%'S overwriteFiles=%y name=%'S",
                                                                 entryData.storageName,
                                                                 directory,
                                                                 overwriteFiles,
                                                                 entryData.name
                                                                ),
                                             0
                                            );

              // read results, update/add data
              String[] errorMessage = new String[1];
              ValueMap resultMap    = new ValueMap();
              while (   !command.endOfData()
                     && !busyDialog.isAborted()
                    )
              {
                if (command.getNextResult(errorMessage,
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
                                                       BARControl.tr("Please enter FTP login password for: {0}.",entryData.storageName),
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
                                                       BARControl.tr("Please enter SSH (TLS) login password for: {0}.",entryData.storageName),
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
                                                       BARControl.tr("Please enter Webdav login password for: {0}.",entryData.storageName),
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
                                                       BARControl.tr("Please enter decrypt password for: {0}.",entryData.storageName),
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
                    Dialogs.error(shell,BARControl.tr("Cannot restore entry\n\n''{0}''\n\nfrom archive\n\n''{1}''\n\n(error: {2})",entryData.name,entryData.storageName,errorText));
                  }
                });
              }
            }
            else
            {
              busyDialog.updateText(BARControl.tr("Aborting\u2026"));
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
              BARControl.resetCursor();
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
              BARControl.resetCursor();
              Dialogs.error(shell,BARControl.tr("Error while restoring entries:\n\n{0}",errorMessage));
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
