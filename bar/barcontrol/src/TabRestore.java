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
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.lang.ref.WeakReference;
import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
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
import org.eclipse.swt.events.MenuListener;
import org.eclipse.swt.events.MenuEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
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
import org.eclipse.swt.widgets.TabItem;
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
      @Override
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
  // max. shown entries in tree/tables
  final int MAX_SHOWN_ENTRIES = 32000;

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
  class IndexData implements Serializable
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

    public long                     indexId;

    private TreeItem                treeItem;                 // reference tree item or null
    private TreeItemUpdateRunnable  treeItemUpdateRunnable;
    private TableItem               tableItem;                // reference table item or null
    private TableItemUpdateRunnable tableItemUpdateRunnable;
    private Menu                    subMenu;                  // reference sub-menu or null
    private MenuItem                menuItem;                 // reference menu item or null
    private MenuItemUpdateRunnable  menuItemUpdateRunnable;

    /** create index data
     * @param indexId index id
     */
    IndexData(long indexId)
    {
      this.indexId       = indexId;

      this.treeItem      = null;
      this.tableItem     = null;
      this.subMenu       = null;
      this.menuItem      = null;
    }

    /** get tree item reference
     * @param tree item or null
     */
    public TreeItem getTreeItem()
    {
      return treeItem;
    }

    /** set tree item reference
     * @param treeItem tree item
     * @param treeItemUpdateRunnable tree item update runnable
     */
//TODO: obsolete?
    public void setTreeItem(TreeItem treeItem, TreeItemUpdateRunnable treeItemUpdateRunnable)
    {
      this.treeItem               = treeItem;
      this.treeItemUpdateRunnable = treeItemUpdateRunnable;
      update();
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
    protected void updateTableItem(TableItem tableItem, TableItemUpdateRunnable tableItemUpdateRunnable)
    {
      this.tableItem               = tableItem;
      this.tableItemUpdateRunnable = tableItemUpdateRunnable;
      update();
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

    /** get name
     * @return name
     */
    public String getName()
    {
      return "";
    }

    /** get date/time
     * @return date/time [s]
     */
    public long getDateTime()
    {
      return 0;
    }

    /** get number of entries
     * @return entries
     */
    public long getEntries()
    {
      return 0;
    }

    /** get size
     * @return size [bytes]
     */
    public long getSize()
    {
      return 0;
    }

    /** get index state
     * @return index state
     */
    public IndexStates getState()
    {
      return IndexStates.OK;
    }

    /** set index state
     * @param indexState index state
     */
    public void setState(IndexStates indexState)
    {
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

    /** get info string
     * @return info string
     */
    public String getInfo()
    {
      return "";
    }

    /** write index data object to object stream
     * Note: must be implented because Java serializaion API cannot write
     *       inner classes without writing outer classes, too!
     * @param out stream
     */
    private void writeObject(java.io.ObjectOutputStream out)
      throws IOException
    {
      out.writeObject(indexId);
    }

    /** read index data object from object stream
     * Note: must be implented because Java serializaion API cannot read
     *       inner classes without reading outer classes, too!
     * @param in stream
     * @return
     */
    private void readObject(java.io.ObjectInputStream in)
      throws IOException, ClassNotFoundException
    {
      indexId = (Long)in.readObject();
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Index {"+indexId+"}";
    }
  };

  /** index data comparator
   */
  static class IndexDataComparator implements Comparator<IndexData>
  {
    // sort modes
    enum SortModes
    {
      NAME,
      SIZE,
      CREATED_DATETIME,
      STATE
    }

    protected SortModes sortMode;

    /** create storage data comparator
     * @param tree storage tree
     * @param sortColumn sort column
     */
    IndexDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) sortMode = SortModes.NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SortModes.SIZE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SortModes.CREATED_DATETIME;
      else if (tree.getColumn(3) == sortColumn) sortMode = SortModes.STATE;
      else                                      sortMode = SortModes.NAME;
    }

    /** create index data comparator
     * @param table table
     * @param sortColumn sort column
     */
    IndexDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SortModes.NAME;
      else if (table.getColumn(1) == sortColumn) sortMode = SortModes.SIZE;
      else if (table.getColumn(2) == sortColumn) sortMode = SortModes.CREATED_DATETIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SortModes.STATE;
      else                                       sortMode = SortModes.NAME;
    }

    /** create index data comparator
     * @param tree tree
     */
    IndexDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
    }

    /** create index data comparator
     * @param tree storage tree
     */
    IndexDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** create index data comparator
     */
    IndexDataComparator()
    {
      sortMode = SortModes.NAME;
    }

    /** compare index data
     * @param indexData1, indexData2 index data to compare
     * @return -1 iff indexData1 < indexData2,
                0 iff indexData1 = indexData2,
                1 iff indexData1 > indexData2
     */
    @Override
    public int compare(IndexData indexData1, IndexData indexData2)
    {
      switch (sortMode)
      {
        case NAME:
          String name1 = indexData1.getName();
          String name2 = indexData2.getName();
          return name1.compareTo(name2);
        case SIZE:
          long size1 = indexData1.getSize();
          long size2 = indexData2.getSize();
          return new Long(size1).compareTo(size2);
        case CREATED_DATETIME:
          long date1 = indexData1.getDateTime();
          long date2 = indexData2.getDateTime();
          return new Long(date1).compareTo(date2);
        case STATE:
          IndexStates indexState1 = indexData1.getState();
          IndexStates indexState2 = indexData2.getState();
          return indexState1.compareTo(indexState2);
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

  /** index data transfer class (required for drag&drop)
   */
  static class IndexDataTransfer extends ByteArrayTransfer
  {
    private static final String NAME = "IndexData";
    private static final int    ID   = registerType(NAME);

    private static IndexDataTransfer instance = new IndexDataTransfer();

    /** get index data transfer instance
     * @return index data transfer instance
     */
    public static IndexDataTransfer getInstance()
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

      IndexData indexData = (IndexData)object;
      try
      {
        // write data to a byte array and then ask super to convert to pMedium
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        ObjectOutputStream outputStream = new ObjectOutputStream(byteArrayOutputStream);
        outputStream.writeObject(indexData);
        byte[] buffer = byteArrayOutputStream.toByteArray();
        outputStream.close();

        // call super to convert to pMedium
        super.javaToNative(buffer,transferData);
      }
      catch (IOException exception)
      {
        // do nothing
        if (Settings.debugLevel > 0)
        {
          BARControl.printStackTrace(exception);
        }
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

        IndexData indexData = null;
        try
        {
          ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream(buffer);
          ObjectInputStream inputStream = new ObjectInputStream(byteArrayInputStream);
          indexData = (IndexData)inputStream.readObject();
          inputStream.close ();
        }
        catch (java.lang.ClassNotFoundException exception)
        {
          if (Settings.debugLevel > 0)
          {
            BARControl.printStackTrace(exception);
          }
          return null;
        }
        catch (IOException exception)
        {
          if (Settings.debugLevel > 0)
          {
            BARControl.printStackTrace(exception);
          }
          return null;
        }

        return indexData;
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
      return (object != null && (object instanceof IndexData));
    }
  }

  /** index id set
   */
  class IndexIdSet extends HashSet<Long>
  {
    /** add/remove id
     * @param id id
     * @param enabled true to add, false to remove
     */
    public void set(long id, boolean enabled)
    {
      if (enabled)
      {
        add(id);
      }
      else
      {
        remove(id);
      }
    }
  }

  /** UUID index data
   */
  class UUIDIndexData extends IndexData
  {
    public String jobUUID;                        // job UUID
    public String name;
    public long   lastCreatedDateTime;            // last date/time when some storage was created
    public String lastErrorMessage;               // last error message
    public long   totalEntries;
    public long   totalSize;

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)uuidIndexData,
                               uuidIndexData.name,
                               Units.formatByteSize(uuidIndexData.totalSize),
                               (uuidIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(uuidIndexData.lastCreatedDateTime*1000L)) : "-",
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
     * @param indexId index id
     * @param jobUUID job uuid
     * @param name job name
     * @param lastCreatedDateTime last date/time (timestamp) when some storage was created
     * @param lastErrorMessage last error message text
     * @param totalEntries total number of entries of storage
     * @param totalSize total size of storage [byte]
     */
    UUIDIndexData(long   indexId,
                  String jobUUID,
                  String name,
                  long   lastCreatedDateTime,
                  String lastErrorMessage,
                  long   totalEntries,
                  long   totalSize
                 )
    {
      super(indexId);
      this.jobUUID             = jobUUID;
      this.name                = name;
      this.lastCreatedDateTime = lastCreatedDateTime;
      this.lastErrorMessage    = lastErrorMessage;
      this.totalEntries        = totalEntries;
      this.totalSize           = totalSize;
    }

    /** get name
     * @return name
     */
    public String getName()
    {
      return name;
    }

    /** get date/time
     * @return date/time [s]
     */
    public long getDateTime()
    {
      return lastCreatedDateTime;
    }

    /** get number of entries
     * @return entries
     */
    public long getEntries()
    {
      return totalEntries;
    }

    /** get size
     * @return size [bytes]
     */
    public long getSize()
    {
      return totalSize;
    }

    /** set tree item reference
     * @param treeItem tree item
     */
//TODO: obsolete?
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
      return "UUIDIndexData {"+jobUUID+", lastCreatedDateTime="+lastCreatedDateTime+", totalSize="+totalSize+" bytes}";
    }
  }

  /** entity index data
   */
  class EntityIndexData extends IndexData implements Serializable
  {
    public Settings.ArchiveTypes archiveType;
    public long                  lastCreatedDateTime;  // last date/time when some storage was created
    public String                lastErrorMessage;     // last error message
    public long                  totalEntries;
    public long                  totalSize;

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        EntityIndexData entityIndexData = (EntityIndexData)indexData;

Dprintf.dprintf("remove");
        Widgets.updateTreeItem(treeItem,
                               (Object)entityIndexData,
                               entityIndexData.archiveType.toString(),
                               Units.formatByteSize(entityIndexData.totalSize),
                               (entityIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-",
                               ""
                              );
      }
    };

    private final MenuItemUpdateRunnable menuItemUpdateRunnable = new MenuItemUpdateRunnable()
    {
      public void update(MenuItem menuItem, IndexData indexData)
      {
        EntityIndexData entityIndexData = (EntityIndexData)indexData;

Dprintf.dprintf("");
//        menuItem.setText(entityIndexData.name);
      }
    };

    /** create job data index
     * @param entityId entity id
     * @param name name of storage
     * @param lastCreatedDateTime last date/time (timestamp) when some storage was created
     * @param lastErrorMessage last error message text
     * @param totalEntries total number of entresi of storage
     * @param totalSize total size of storage [byte]
     */
    EntityIndexData(long                  entityId,
                    Settings.ArchiveTypes archiveType,
                    long                  lastCreatedDateTime,
                    String                lastErrorMessage,
                    long                  totalEntries,
                    long                  totalSize
                   )
    {
      super(entityId);
      this.archiveType         = archiveType;
      this.lastCreatedDateTime = lastCreatedDateTime;
      this.lastErrorMessage    = lastErrorMessage;
      this.totalEntries        = totalEntries;
      this.totalSize           = totalSize;
    }

    /** get date/time
     * @return date/time [s]
     */
    public long getDateTime()
    {
      return lastCreatedDateTime;
    }

    /** get number of entries
     * @return entries
     */
    public long getEntries()
    {
      return totalEntries;
    }

    /** get size
     * @return size [bytes]
     */
    public long getSize()
    {
      return totalSize;
    }

    /** set tree item reference
     * @param treeItem tree item
     */
//TODO: obsolete?
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

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return String.format("%d: %s",indexId,archiveType.toString());
    }

    /** write storage index data object to object stream
     * Note: must be implented because Java serializaion API cannot write
     *       inner classes without writing outer classes, too!
     * @param out stream
     */
    private void writeObject(java.io.ObjectOutputStream out)
      throws IOException
    {
      super.writeObject(out);
      out.writeObject(archiveType);
      out.writeObject(lastCreatedDateTime);
      out.writeObject(lastErrorMessage);
      out.writeObject(totalEntries);
      out.writeObject(totalSize);
    }

    /** read storage index data object from object stream
     * Note: must be implented because Java serializaion API cannot read
     *       inner classes without reading outer classes, too!
     * @param in stream
     * @return
     */
    private void readObject(java.io.ObjectInputStream in)
      throws IOException, ClassNotFoundException
    {
      super.readObject(in);
      archiveType         = (Settings.ArchiveTypes)in.readObject();
      lastCreatedDateTime = (Long)in.readObject();
      lastErrorMessage    = (String)in.readObject();
      totalEntries        = (Long)in.readObject();
      totalSize           = (Long)in.readObject();
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "EntityIndexData {"+indexId+", type="+archiveType.toString()+", lastCreatedDateTime="+lastCreatedDateTime+", totalSize="+totalSize+" bytes}";
    }
  }

  /** storage index data
   */
  class StorageIndexData extends IndexData implements Serializable
  {
    public String                jobName;                  // job name or null
    public Settings.ArchiveTypes archiveType;              // archive type
    public String                name;                     // name
    public long                  lastCreatedDateTime;      // last date/time when some storage was created
    private long                 totalEntries;
    private long                 totalSize;
    public IndexStates           indexState;               // state of index
    public IndexModes            indexMode;                // mode of index
    public long                  lastCheckedDateTime;      // last checked date/time
    public String                errorMessage;             // last error message

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        StorageIndexData storageIndexData = (StorageIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)storageIndexData,
                               storageIndexData.name,
                               Units.formatByteSize(storageIndexData.totalSize),
                               (storageIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(storageIndexData.lastCreatedDateTime*1000L)) : "-",
                               storageIndexData.indexState.toString()
                              );
      }
    };

    private final TableItemUpdateRunnable tableItemUpdateRunnable = new TableItemUpdateRunnable()
    {
      public void update(TableItem tableItem, IndexData indexData)
      {
         StorageIndexData storageIndexData = (StorageIndexData)indexData;

         Widgets.updateTableItem(tableItem,
                                 (Object)storageIndexData,
                                 storageIndexData.name,
                                 Units.formatByteSize(storageIndexData.totalSize),
                                 simpleDateFormat.format(new Date(storageIndexData.lastCreatedDateTime*1000L)),
                                 storageIndexData.indexState.toString()
                                );
      }
    };

    /** create storage data index
     * @param storageId database storage id
     * @param jobName job name or null
     * @param archiveType archive type
     * @param name name of storage
     * @param lastCreatedDateTime date/time (timestamp) when some storage was created
     * @param totalEntries number of entries
     * @param totalSize size of storage [byte]
     * @param indexState storage index state
     * @param indexMode storage index mode
     * @param lastCheckedDateTime last checked date/time (timestamp)
     * @param errorMessage error message text
     */
    StorageIndexData(long                  storageId,
                     String                jobName,
                     Settings.ArchiveTypes archiveType,
                     String                name,
                     long                  lastCreatedDateTime,
                     long                  totalEntries,
                     long                  totalSize,
                     IndexStates           indexState,
                     IndexModes            indexMode,
                     long                  lastCheckedDateTime,
                     String                errorMessage
                    )
    {
      super(storageId);
      this.jobName             = jobName;
      this.archiveType         = archiveType;
      this.name                = name;
      this.lastCreatedDateTime = lastCreatedDateTime;
      this.totalEntries        = totalEntries;
      this.totalSize           = totalSize;
      this.indexState          = indexState;
      this.indexMode           = indexMode;
      this.lastCheckedDateTime = lastCheckedDateTime;
      this.errorMessage        = errorMessage;
    }

    /** create storage data
     * @param storageId database storage id
     * @param jobName job name
     * @param archiveType archive type
     * @param name name of storage
     * @param lastCreatedDateTime date/time (timestamp) when storage was created
     * @param lastCheckedDateTime last checked date/time (timestamp)
     */
    StorageIndexData(long                  storageId,
                     String                jobName,
                     Settings.ArchiveTypes archiveType,
                     String                name,
                     long                  lastCreatedDateTime,
                     long                  lastCheckedDateTime
                    )
    {
      this(storageId,jobName,archiveType,name,lastCreatedDateTime,0L,0L,IndexStates.OK,IndexModes.MANUAL,lastCheckedDateTime,null);
Dprintf.dprintf("");
    }

    /** create storage data
     * @param storageId database storage id
     * @param entityId database entity id
     * @param jobName job name
     * @param archiveType archive type
     * @param name name of storage
     */
    StorageIndexData(long storageId, String jobName, Settings.ArchiveTypes archiveType, String name)
    {
      this(storageId,jobName,archiveType,name,0L,0L);
    }

    /** get name
     * @return name
     */
    public String getName()
    {
      return name;
    }

    /** get date/time
     * @return date/time [s]
     */
    public long getDateTime()
    {
      return lastCreatedDateTime;
    }

    /** get number of entries
     * @return entries
     */
    public long getEntries()
    {
      return totalEntries;
    }

    /** get size
     * @return size [bytes]
     */
    public long getSize()
    {
      return totalSize;
    }

    /** get index state
     * @return index state
     */
    public IndexStates getState()
    {
      return indexState;
    }

    /** set tree item reference
     * @param treeItem tree item
     */
//TODO: obsolete?
    public void setTreeItem(TreeItem treeItem)
    {
      setTreeItem(treeItem,treeItemUpdateRunnable);
    }

    /** set table item reference
     * @param tableItem table item
     */
    public void XupdateItem(TableItem tableItem)
    {
//      updateTableItem(tableItem,tableItemUpdateRunnable);
      Widgets.updateTableItem(tableItem,
                              (Object)this,
                              name,
                              Units.formatByteSize(totalSize),
                              simpleDateFormat.format(new Date(lastCreatedDateTime*1000L)),
                              indexState.toString()
                             );

    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return String.format("%d: %s, %s",indexId,jobName,name);
    }

    /** set index state
     * @param indexState index state
     */
    public void setState(IndexStates indexState)
    {
      this.indexState = indexState;
      update();
    }

    /** write storage index data object to object stream
     * Note: must be implented because Java serializaion API cannot write
     *       inner classes without writing outer classes, too!
     * @param out stream
     */
    private void writeObject(java.io.ObjectOutputStream out)
      throws IOException
    {
      super.writeObject(out);
      out.writeObject(jobName);
      out.writeObject(archiveType);
      out.writeObject(name);
      out.writeObject(lastCreatedDateTime);
      out.writeObject(totalEntries);
      out.writeObject(totalSize);
      out.writeObject(indexState);
      out.writeObject(indexMode);
      out.writeObject(lastCheckedDateTime);
    }

    /** read storage index data object from object stream
     * Note: must be implented because Java serializaion API cannot read
     *       inner classes without reading outer classes, too!
     * @param in stream
     * @return
     */
    private void readObject(java.io.ObjectInputStream in)
      throws IOException, ClassNotFoundException
    {
      super.readObject(in);
      jobName             = (String)in.readObject();
      archiveType         = (Settings.ArchiveTypes)in.readObject();
      name                = (String)in.readObject();
      lastCreatedDateTime = (Long)in.readObject();
      totalEntries        = (Long)in.readObject();
      totalSize           = (Long)in.readObject();
      indexState          = (IndexStates)in.readObject();
      indexMode           = (IndexModes)in.readObject();
      lastCheckedDateTime = (Long)in.readObject();
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "StorageIndexData {"+indexId+", name="+name+", lastCreatedDateTime="+lastCreatedDateTime+", totalSize="+totalSize+" bytes, state="+indexState+", last checked="+lastCheckedDateTime+"}";
    }
  };

  /** find index for insert of item in sorted storage item list
   * @param indexData index data
   * @return index in tree
   */
  private int findStorageTreeIndex(IndexData indexData)
  {
    TreeItem            treeItems[]         = widgetStorageTree.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree);

/* Note: binary search not possible because items are not sorted
    int i0 = 0;
    int i1 = treeItems.length-1;
    while (i0 < i1)
    {
      int i = (i1-i0)/2;
      int result = indexDataComparator.compare(indexData,(IndexData)treeItems[i].getData());
      if      (result < 0) i0 = i+1;
      else if (result > 0) i1 = i-1;
      else return i;
    }

    return treeItems.length;
*/
    int index = 0;
    while (   (index < treeItems.length)
           && (indexDataComparator.compare(indexData,(IndexData)treeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of item in sorted index item tree
   * @param treeItem tree item
   * @param indexData index data
   * @return index in tree
   */
  private int findStorageTreeIndex(TreeItem treeItem, IndexData indexData)
  {
    TreeItem            subTreeItems[]      = treeItem.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree);

/* Note: binary search not possible because items are not sorted
    int i0 = 0;
    int i1 = subTreeItems.length-1;
    while (i0 < i1)
    {
      int i = (i1-i0)/2;
      int result = indexDataComparator.compare(indexData,(IndexData)subTreeItems[i].getData());
      if      (result < 0) i0 = i+1;
      else if (result > 0) i1 = i-1;
      else return i;
    }

    return subTreeItems.length;
*/
    int index = 0;
    while (   (index < subTreeItems.length)
           && (indexDataComparator.compare(indexData,(IndexData)subTreeItems[index].getData()) > 0)
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

    int index = STORAGE_TREE_MENU_START_INDEX;
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

    int index = STORAGE_LIST_MENU_START_INDEX;
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
  private int findStorageTableIndex(IndexData indexData)
  {
    TableItem           tableItems[]        = widgetStorageTable.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTable);

    int i0 = 0;
    int i1 = tableItems.length-1;
    while (i0 < i1)
    {
      int i = (i1-i0)/2;
      int result = indexDataComparator.compare(indexData,(IndexData)tableItems[i].getData());
      if      (result < 0) i0 = i+1;
      else if (result > 0) i1 = i-1;
      else return i;
    }

    return tableItems.length;
  }

  /** update storage tree/table thread
   */
  class UpdateStorageTreeTableThread extends Thread
  {
    private final int PAGE_SIZE = 64;

    private Object           trigger              = new Object();   // trigger update object
    private boolean          updateCount          = false;
    private HashSet<Integer> updateOffsets        = new HashSet<Integer>();
    private int              count                = 0;
    private String           storagePattern       = "";
    private IndexStateSet    storageIndexStateSet = INDEX_STATE_SET_ALL;
    private EntityStates     storageEntityState   = EntityStates.ANY;
    private boolean          setUpdateIndicator   = false;          // true to set color/cursor at update

    /** create update storage list thread
     */
    UpdateStorageTreeTableThread()
    {
      super();
      setDaemon(true);
      setName("BARControl Update Storage");
    }

    /** run update storage list thread
     */
    public void run()
    {
      boolean          updateCount        = true;
      HashSet<Integer> updateOffsets      = new HashSet<Integer>();
      boolean          setUpdateIndicator = true;
      try
      {
        for (;;)
        {
          boolean updateIndicator = false;
          {
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
              updateIndicator = true;
            }
          }
          try
          {
            // update tree/table
            try
            {
              if (updateCount)
              {
                updateStorageTableCount();
              }

              HashSet<TreeItem> uuidTreeItems = new HashSet<TreeItem>();
              if (!this.updateCount)
              {
                updateUUIDTreeItems(uuidTreeItems);
              }
              HashSet<TreeItem> entityTreeItems = new HashSet<TreeItem>();
              if (!this.updateCount)
              {
                updateEntityTreeItems(uuidTreeItems,entityTreeItems);
              }
              if (!this.updateCount)
              {
                updateStorageTreeItems(entityTreeItems);
              }

              if (!updateOffsets.isEmpty())
              {
                updateStorageTable(updateOffsets);
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

            // update menues
            try
            {
  //TODO
  //            updateUUIDMenus();
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
            // reset cursor and foreground color
            if (updateIndicator)
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
          }

          // wait for trigger or sleep a short time
          synchronized(trigger)
          {
            if (!this.updateCount && this.updateOffsets.isEmpty())
            {
              // wait for refresh request trigger or timeout
              try { trigger.wait(30*1000); } catch (InterruptedException exception) { /* ignored */ };
            }

            // get update count, offsets to update
            updateCount        = this.updateCount;
            updateOffsets.addAll(this.updateOffsets);
            setUpdateIndicator = this.setUpdateIndicator;

            // if not triggered (timeout occurred) update count is done invisible (color is not set)
            if (!this.updateCount && this.updateOffsets.isEmpty())
            {
              updateCount        = true;
              setUpdateIndicator = false;
            }

            // wait for immediate further triggers
            do
            {
              this.updateCount        = false;
              this.updateOffsets.clear();
              this.setUpdateIndicator = false;

              try { trigger.wait(500); } catch (InterruptedException exception) { /* ignored */ };
              updateOffsets.addAll(this.updateOffsets);
            }
            while (this.updateCount || !this.updateOffsets.isEmpty());
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

    /** get count
     * @return count
     */
    public int getCount()
    {
      return count;
    }

    /** get storage pattern
     * @return storage pattern
     */
    public String getStoragePattern()
    {
      return storagePattern;
    }

    /** get storage index state set
     * @return storage index state set
     */
    public IndexStateSet getStorageIndexStateSet()
    {
      return storageIndexStateSet;
    }

    /** trigger update of storage list
     * @param storagePattern new storage pattern
     * @param storageIndexStateSet new storage index state set
     * @param storageEntityState new storage entity state
     */
    public void triggerUpdate(String storagePattern, IndexStateSet storageIndexStateSet, EntityStates storageEntityState)
    {
      assert storagePattern != null;

      synchronized(trigger)
      {
        if (   !this.storagePattern.equals(storagePattern)
            || (this.storageIndexStateSet != storageIndexStateSet) || (this.storageEntityState != storageEntityState)
           )
        {
          this.storagePattern       = storagePattern;
          this.storageIndexStateSet = storageIndexStateSet;
          this.storageEntityState   = storageEntityState;
          this.setUpdateIndicator   = true;
          this.updateCount          = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of storage list
     * @param storagePattern new storage pattern
     */
    public void triggerUpdateStoragePattern(String storagePattern)
    {
      assert storagePattern != null;

      synchronized(trigger)
      {
        if (!this.storagePattern.equals(storagePattern))
        {
          this.storagePattern     = storagePattern;
          this.setUpdateIndicator = true;
          this.updateCount        = true;
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
          this.updateCount          = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of storage list item
     * @param index index in list to update
     */
    public void triggerUpdate(int index)
    {
      synchronized(trigger)
      {
        int offset = (index/PAGE_SIZE)*PAGE_SIZE;
        if (!updateOffsets.contains(offset))
        {
          updateOffsets.add(offset);
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
        this.updateCount        = true;
        trigger.notify();
      }
    }

    /** check if update triggered
     * @return true iff update triggered
     */
    private boolean isUpdateTriggered()
    {
      return updateCount || !updateOffsets.isEmpty();
    }

    /** update UUID tree items
     */
    private void updateUUIDTreeItems(final HashSet<TreeItem> uuidTreeItems)
    {
      // index data comparator
      final Comparator<IndexData> comparator = new Comparator<IndexData>()
      {
        public int compare(IndexData indexData1, IndexData indexData2)
        {
          return (indexData1.indexId == indexData2.indexId) ? 0 : 1;
        }
      };

//      Command                 command;
//      String[]                errorMessage          = new String[1];
//      ValueMap                valueMap              = new ValueMap();

      uuidTreeItems.clear();

      // get top tree item, UUID items
      final TreeItem[]        topTreeItem           = new TreeItem[]{null};
      final HashSet<TreeItem> removeUUIDTreeItemSet = new HashSet<TreeItem>();
      display.syncExec(new Runnable()
      {
        public void run()
        {
          topTreeItem[0] = widgetStorageTree.getTopItem();

          for (TreeItem treeItem : widgetStorageTree.getItems())
          {
            assert treeItem.getData() instanceof UUIDIndexData;
            removeUUIDTreeItemSet.add(treeItem);
          }
        }
      });
      if (isUpdateTriggered()) return;

      // get UUID list
      final ArrayList<UUIDIndexData> uuidIndexDataList = new ArrayList<UUIDIndexData>();
      BARServer.executeCommand(StringParser.format("INDEX_UUID_LIST pattern=%'S",
                                                   storagePattern
                                                  ),
                               1,
                               new CommandResultHandler()
                               {
                                 public int handleResult(int i, ValueMap valueMap)
                                 {
                                   try
                                   {
                                     long   indexId             = valueMap.getLong  ("indexId"            );
                                     String jobUUID             = valueMap.getString("jobUUID"            );
                                     String name                = valueMap.getString("name"               );
                                     long   lastCreatedDateTime = valueMap.getLong  ("lastCreatedDateTime");
                                     String lastErrorMessage    = valueMap.getString("lastErrorMessage"   );
                                     long   totalEntries        = valueMap.getLong  ("totalEntries"       );
                                     long   totalSize           = valueMap.getLong  ("totalSize"          );

                                     uuidIndexDataList.add(new UUIDIndexData(indexId,
                                                                             jobUUID,
                                                                             name,
                                                                             lastCreatedDateTime,
                                                                             lastErrorMessage,
                                                                             totalEntries,
                                                                             totalSize
                                                                            )
                                                          );
                                   }
                                   catch (IllegalArgumentException exception)
                                   {
                                     if (Settings.debugLevel > 0)
                                     {
                                       System.err.println("ERROR: "+exception.getMessage());
                                     }
                                   }

                                   // check if aborted
                                   if (isUpdateTriggered())
                                   {
Dprintf.dprintf("");
  //TODO
  //                                     abort();
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );
      if (isUpdateTriggered()) return;

      try
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(false);
          }
        });

        // update UUID list
        for(final UUIDIndexData uuidIndexData : uuidIndexDataList)
        {
          // update/insert tree item
          display.syncExec(new Runnable()
          {
            public void run()
            {
              TreeItem uuidTreeItem = Widgets.getTreeItem(widgetStorageTree,comparator,uuidIndexData);
              if (uuidTreeItem == null)
              {
                // insert tree item
                uuidTreeItem = Widgets.insertTreeItem(widgetStorageTree,
                                                      findStorageTreeIndex(uuidIndexData),
                                                      (Object)uuidIndexData,
                                                      true,  // folderFlag
                                                      uuidIndexData.name,
                                                      Units.formatByteSize(uuidIndexData.totalSize),
                                                      (uuidIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(uuidIndexData.lastCreatedDateTime*1000L)) : "-",
                                                      ""
                                                     );
//TODO
//uuidIndexData.setTreeItem(uuidTreeItem);
                uuidTreeItem.setChecked(selectedIndexIdSet.contains(uuidIndexData.indexId));
              }
              else
              {
                // update tree item
                assert uuidTreeItem.getData() instanceof UUIDIndexData;
                Widgets.updateTreeItem(uuidTreeItem,
                                       (Object)uuidIndexData,
                                       uuidIndexData.name,
                                       Units.formatByteSize(uuidIndexData.totalSize),
                                       (uuidIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(uuidIndexData.lastCreatedDateTime*1000L)) : "-",
                                       ""
                                      );
                removeUUIDTreeItemSet.remove(uuidTreeItem);
              }
              if (uuidTreeItem.getExpanded())
              {
                uuidTreeItems.add(uuidTreeItem);
              }

              if (topTreeItem[0] == null) topTreeItem[0] = uuidTreeItem;
            }
          });
        }
        if (isUpdateTriggered()) return;

        // remove not existing entries
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem treeItem : removeUUIDTreeItemSet)
            {
              if (!treeItem.isDisposed())
              {
                IndexData indexData = (IndexData)treeItem.getData();
                Widgets.removeTreeItem(widgetStorageTree,treeItem);
                indexData.clearTreeItem();
              }
            }
            if (topTreeItem[0] != null)
            {
Dprintf.dprintf("topTreeItem[0]=%s %s",topTreeItem[0],topTreeItem[0].isDisposed());
if (!topTreeItem[0].isDisposed())
            widgetStorageTree.setTopItem(topTreeItem[0]);
            }
          }
        });
      }
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(true);
          }
        });
      }
    }

    /** update entity tree items
     * @param uuidTreeItem UUID tree item to update
     * @param entityTreeItems updated job tree items
     */
    private void updateEntityTreeItems(final TreeItem uuidTreeItem, final HashSet<TreeItem> entityTreeItems)
    {
      // index data comparator
      final Comparator<IndexData> comparator = new Comparator<IndexData>()
      {
        public int compare(IndexData indexData1, IndexData indexData2)
        {
          return (indexData1.indexId == indexData2.indexId) ? 0 : 1;
        }
      };

      // get job items, UUID index data
      final HashSet<TreeItem> removeEntityTreeItemSet = new HashSet<TreeItem>();
      final UUIDIndexData     uuidIndexData[]         = new UUIDIndexData[]{null};
      display.syncExec(new Runnable()
      {
        public void run()
        {
Dprintf.dprintf("uuidTreeItem.getData()=%s",uuidTreeItem.getData());
          if (!uuidTreeItem.isDisposed())
          {
            assert uuidTreeItem.getData() instanceof UUIDIndexData;

            for (TreeItem treeItem : uuidTreeItem.getItems())
            {
              assert treeItem.getData() instanceof EntityIndexData;
              removeEntityTreeItemSet.add(treeItem);
            }

            uuidIndexData[0] = (UUIDIndexData)uuidTreeItem.getData();
          }
        }
      });
      if (isUpdateTriggered()) return;


      // get entity list
      final ArrayList<EntityIndexData> entityIndexDataList = new ArrayList<EntityIndexData>();
      BARServer.executeCommand(StringParser.format("INDEX_ENTITY_LIST jobUUID=%'S pattern=%'S",
                                                   uuidIndexData[0].jobUUID,
                                                   storagePattern
                                                  ),
                               1,
                               new CommandResultHandler()
                               {
                                 public int handleResult(int i, ValueMap valueMap)
                                 {
                                   try
                                   {
                                     long                  entityId            = valueMap.getLong  ("entityId"                               );
                                     String                jobUUID             = valueMap.getString("jobUUID"                                );
                                     String                scheduleUUID        = valueMap.getString("scheduleUUID"                           );
                                     Settings.ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
                                     long                  lastCreatedDateTime = valueMap.getLong  ("lastCreatedDateTime"                    );
                                     String                lastErrorMessage    = valueMap.getString("lastErrorMessage"                       );
                                     long                  totalEntries        = valueMap.getLong  ("totalEntries"                           );
                                     long                  totalSize           = valueMap.getLong  ("totalSize"                              );

                                     // add entity data index
                                     entityIndexDataList.add(new EntityIndexData(entityId,
                                                                                 archiveType,
                                                                                 lastCreatedDateTime,
                                                                                 lastErrorMessage,
                                                                                 totalEntries,
                                                                                 totalSize
                                                                                )
                                                            );
                                   }
                                   catch (IllegalArgumentException exception)
                                   {
                                     if (Settings.debugLevel > 0)
                                     {
                                       System.err.println("ERROR: "+exception.getMessage());
                                     }
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );
      if (isUpdateTriggered()) return;

      try
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(false);
          }
        });

        // update entity list
        for (final EntityIndexData entityIndexData : entityIndexDataList)
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              TreeItem entityTreeItem = Widgets.getTreeItem(uuidTreeItem,comparator,entityIndexData);
              if (entityTreeItem == null)
              {
                // insert tree item
                entityTreeItem = Widgets.insertTreeItem(uuidTreeItem,
                                                        findStorageTreeIndex(uuidTreeItem,entityIndexData),
                                                        (Object)entityIndexData,
                                                        true,
                                                        entityIndexData.archiveType.toString(),
                                                        Units.formatByteSize(entityIndexData.totalSize),
                                                        (entityIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-",
                                                        ""
                                                       );
//TODO
//entityIndexData.setTreeItem(entityTreeItem);
                entityTreeItem.setChecked(selectedIndexIdSet.contains(entityIndexData.indexId));
              }
              else
              {
                // update tree item
                assert entityTreeItem.getData() instanceof EntityIndexData;
                Widgets.updateTreeItem(entityTreeItem,
                                       (Object)entityIndexData,
                                       entityIndexData.archiveType.toString(),
                                       Units.formatByteSize(entityIndexData.totalSize),
                                       (entityIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-",
                                       ""
                                      );
                removeEntityTreeItemSet.remove(entityTreeItem);
              }
              if (entityTreeItem.getExpanded())
              {
                entityTreeItems.add(entityTreeItem);
              }
            }
          });
        }
        if (isUpdateTriggered()) return;

        // remove not existing entries
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem treeItem : removeEntityTreeItemSet)
            {
              if (!treeItem.isDisposed())
              {
                IndexData indexData = (IndexData)treeItem.getData();
                Widgets.removeTreeItem(widgetStorageTree,treeItem);
                indexData.clearTreeItem();
              }
            }
          }
        });
      }
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(true);
          }
        });
      }
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

    /** update storage tree item
     * @param entityTreeItem job tree item to update
     */
    private void updateStorageTreeItem(final TreeItem entityTreeItem)
    {
      // get storage items, entity index data
      final HashSet<TreeItem> removeStorageTreeItemSet = new HashSet<TreeItem>();
      final EntityIndexData   entityIndexData[]        = new EntityIndexData[1];
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
      if (isUpdateTriggered()) return;

      // update storage list
      final ArrayList<StorageIndexData> storageIndexDataList = new ArrayList<StorageIndexData>();
      BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%d storagePattern=%'S indexStateSet=%s indexModeSet=%s",
                                                         entityIndexData[0].indexId,
                                                         storagePattern,
                                                         storageIndexStateSet.nameList("|"),
                                                         "*"
                                                        ),
                               1,
                               new CommandResultHandler()
                               {
                                 public int handleResult(int i, ValueMap valueMap)
                                 {
                                   try
                                   {
                                     long                  storageId           = valueMap.getLong  ("storageId"                              );
                                     String                jobUUID             = valueMap.getString("jobUUID"                                );
                                     String                scheduleUUID        = valueMap.getString("scheduleUUID"                           );
                                     String                jobName             = valueMap.getString("jobName"                                );
                                     Settings.ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
                                     String                name                = valueMap.getString("name"                                   );
                                     long                  dateTime            = valueMap.getLong  ("dateTime"                               );
                                     long                  entries             = valueMap.getLong  ("entries"                                );
                                     long                  size                = valueMap.getLong  ("size"                                   );
                                     IndexStates           indexState          = valueMap.getEnum  ("indexState",IndexStates.class           );
                                     IndexModes            indexMode           = valueMap.getEnum  ("indexMode",IndexModes.class             );
                                     long                  lastCheckedDateTime = valueMap.getLong  ("lastCheckedDateTime"                    );
                                     String                errorMessage_       = valueMap.getString("errorMessage"                           );

                                     // add storage index data
                                     storageIndexDataList.add(new StorageIndexData(storageId,
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
                                                                                  )
                                                             );
                                   }
                                   catch (IllegalArgumentException exception)
                                   {
                                     if (Settings.debugLevel > 0)
                                     {
                                       System.err.println("ERROR: "+exception.getMessage());
                                     }
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );

      try
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(false);
          }
        });

        // insert/update tree items
        for (final StorageIndexData storageIndexData : storageIndexDataList)
        {
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
            }
          });
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
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(true);
          }
        });
      }
    }

    /** update storage tree items
     * @param entityTreeItems job tree items to update
     */
    private void updateStorageTreeItems(final HashSet<TreeItem> entityTreeItems)
    {
      for (final TreeItem entityTreeItem : entityTreeItems)
      {
        updateStorageTreeItem(entityTreeItem);
      }
    }

    /** update tree items
     * @param treeItem tree item to update
     */
    private void updateTreeItems(TreeItem treeItem)
    {
      if      (treeItem.getData() instanceof UUIDIndexData)
      {
        updateStorageTreeTableThread.updateEntityTreeItems(treeItem,new HashSet<TreeItem>());
      }
      else if (treeItem.getData() instanceof EntityIndexData)
      {
        updateStorageTreeTableThread.updateStorageTreeItem(treeItem);
      }
    }

    /** refresh storage table display count
     */
    private void updateStorageTableCount()
    {
      assert storagePattern != null;

      // get storages info
      final String[] errorMessage = new String[1];
      ValueMap       valueMap     = new ValueMap();
      if (BARServer.executeCommand(StringParser.format("INDEX_STORAGES_INFO storagePattern=%'S indexStateSet=%s",
                                                       storagePattern,
                                                       storageIndexStateSet.nameList("|")
                                                      ),
                                   0,  // debugLevel
                                   errorMessage,
                                   valueMap
                                  ) == Errors.NONE
         )
      {
        count = valueMap.getInt("count");
      }

      // set count
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetStorageTabFolderTitle.redraw();

          widgetStorageTable.setRedraw(false);

          widgetStorageTable.setItemCount(0);
          widgetStorageTable.clearAll();

          widgetStorageTable.setItemCount(count);
          widgetStorageTable.setTopIndex(0);

          widgetStorageTable.setRedraw(true);
        }
      });
    }

    /** refresh storage table items
     * @param offset refresh offset
     * @return true iff update done
     */
    private boolean updateStorageTable(final int offset)
    {
      assert storagePattern != null;
      assert offset >= 0;
      assert count >= 0;

      // get limit
      final int limit = ((offset+PAGE_SIZE) < count) ? PAGE_SIZE : count-offset;

//TODO: sort
//IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTable,tableColumn);

      // update storage table segment
      final String[] errorMessage = new String[1];
      final int[]    n            = new int[1];
      BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s storagePattern=%'S indexStateSet=%s indexModeSet=%s offset=%d limit=%d",
                                                   (storageEntityState != EntityStates.NONE) ? "*" : "0",
                                                   storagePattern,
                                                   storageIndexStateSet.nameList("|"),
                                                   "*",
                                                   offset,
                                                   limit
                                                  ),
                               1,
                               new CommandResultHandler()
                               {
                                 public int handleResult(int i, ValueMap valueMap)
                                 {
                                   final int index = offset+i;

                                   try
                                   {
                                     final long                  storageId           = valueMap.getLong  ("storageId");
                                     final String                jobUUID             = valueMap.getString("jobUUID");
                                     final String                scheduleUUID        = valueMap.getString("scheduleUUID");
                                     final String                jobName             = valueMap.getString("jobName");
                                     final Settings.ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",Settings.ArchiveTypes.class,Settings.ArchiveTypes.NORMAL);
                                     final String                name                = valueMap.getString("name");
                                     final long                  dateTime            = valueMap.getLong  ("dateTime");
                                     final long                  entries             = valueMap.getLong  ("entries");
                                     final long                  size                = valueMap.getLong  ("size");
                                     final IndexStates           indexState          = valueMap.getEnum  ("indexState",IndexStates.class);
                                     final IndexModes            indexMode           = valueMap.getEnum  ("indexMode",IndexModes.class);
                                     final long                  lastCheckedDateTime = valueMap.getLong  ("lastCheckedDateTime");
                                     final String                errorMessage_       = valueMap.getString("errorMessage");

                                     // add storage index data
                                     final StorageIndexData storageIndexData = new StorageIndexData(storageId,
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

                                     display.syncExec(new Runnable()
                                     {
                                       public void run()
                                       {
                                         TableItem tableItem = widgetStorageTable.getItem(index);

                                         Widgets.updateTableItem(tableItem,
                                                                 (Object)storageIndexData,
                                                                 storageIndexData.name,
                                                                 Units.formatByteSize(storageIndexData.totalSize),
                                                                 simpleDateFormat.format(new Date(storageIndexData.lastCreatedDateTime*1000L)),
                                                                 storageIndexData.indexState.toString()
                                                                );
                                         tableItem.setChecked(selectedIndexIdSet.contains(storageIndexData.indexId));
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

                                   // store number of entries
                                   n[0] = i+1;

                                   // check if aborted
                                   if (isUpdateTriggered() || (n[0] >= limit))
                                   {
//TODO
//                                     abort();
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );

      return n[0] >= limit;
    }

    /** refresh storage table items
     * @param updateOffsets segment offsets to update
     */
    private void updateStorageTable(HashSet<Integer> updateOffsets)
    {
      try
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTable.setRedraw(false);
          }
        });

        Integer offsets[] = updateOffsets.toArray(new Integer[updateOffsets.size()]);
        for (Integer offset : offsets)
        {
          if (!updateStorageTable(offset)) break;
          updateOffsets.remove(offset);
        }
      }
      finally
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTable.setRedraw(true);
          }
        });
      }
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
      ValueMap                     valueMap                = new ValueMap();

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
      if (isUpdateTriggered()) return;

      // update UUIDs
      command = BARServer.runCommand(StringParser.format("INDEX_UUID_LIST pattern=*"),
                                     1
                                    );
      while (   !isUpdateTriggered()
             && !command.endOfData()
             && command.getNextResult(errorMessage,
                                      valueMap,
                                      Command.TIMEOUT
                                     ) == Errors.NONE
            )
      {
        try
        {
          long   indexId             = valueMap.getLong  ("indexId"            );
          String jobUUID             = valueMap.getString("jobUUID"            );
          String name                = valueMap.getString("name"               );
          long   lastCreatedDateTime = valueMap.getLong  ("lastCreatedDateTime");
          String lastErrorMessage    = valueMap.getString("lastErrorMessage"   );
          long   totalEntries        = valueMap.getLong  ("totalEntries"       );
          long   totalSize           = valueMap.getLong  ("totalSize"          );

          // add UUID index data
          final UUIDIndexData uuidIndexData = new UUIDIndexData(indexId,
                                                                jobUUID,
                                                                name,
                                                                lastCreatedDateTime,
                                                                lastErrorMessage,
                                                                totalEntries,
                                                                totalSize
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
                                             uuidIndexData.name.replaceAll("&","&&")
                                            );
                menuItem = Widgets.addMenuItem(subMenu,
                                               null,
                                               BARControl.tr("new normal")
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
                    assignStorage(uuidIndexData,Settings.ArchiveTypes.NORMAL);
                  }
                });
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
      if (isUpdateTriggered()) return;

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
//            Widgets.removeMenu(widgetStorageTableAssignToMenu,menu);
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
            if (subMenu != null)
            {
              MenuItem menuItems[] = subMenu.getItems();
              for (int i = STORAGE_LIST_MENU_START_INDEX; i < menuItems.length; i++)
              {
                assert menuItems[i].getData() instanceof EntityIndexData;
                removeEntityMenuItemSet.add(menuItems[i]);
              }
            }
          }
        }
      });
      if (isUpdateTriggered()) return;

      // update entities
      for (UUIDIndexData uuidIndexData : uuidIndexDataSet)
      {
        final Menu subMenu = uuidIndexData.getSubMenu();

        command = BARServer.runCommand(StringParser.format("INDEX_ENTITY_LIST jobUUID=%'S pattern=*",
                                                           uuidIndexData.jobUUID
                                                          ),
                                       0
                                      );
        while (   !isUpdateTriggered()
               && !command.endOfData()
               && command.getNextResult(errorMessage,
                                        valueMap,
                                        Command.TIMEOUT
                                       ) == Errors.NONE
              )
        {
          try
          {
            long                  entityId            = valueMap.getLong  ("entityId"                               );
            String                jobUUID             = valueMap.getString("jobUUID"                                );
            String                scheduleUUID        = valueMap.getString("scheduleUUID"                           );
            Settings.ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
            long                  lastCreatedDateTime = valueMap.getLong  ("lastCreatedDateTime"                    );
            String                lastErrorMessage    = valueMap.getString("lastErrorMessage"                       );
            long                  totalEntries        = valueMap.getLong  ("totalEntries"                           );
            long                  totalSize           = valueMap.getLong  ("totalSize"                              );

            // add entity data index
            final EntityIndexData entityIndexData = new EntityIndexData(entityId,
                                                                        archiveType,
                                                                        lastCreatedDateTime,
                                                                        lastErrorMessage,
                                                                        totalEntries,
                                                                        totalSize
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
                                                    ((entityIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-")+", "+entityIndexData.archiveType.toString()
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
        if (isUpdateTriggered()) return;
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
    SOCKET,

    ANY;

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case FILE:      return "FILE";
        case IMAGE:     return "IMAGE";
        case DIRECTORY: return "DIRECTORY";
        case LINK:      return "LINK";
        case HARDLINK:  return "HARDLINK";
        case SPECIAL:   return "SPECIAL";
        default:        return "*";
      }
    }
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
  class EntryData extends IndexData
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
     * @param entryId entry id
     * @param storageName storage archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     * @param size size [bytes]
     */
    EntryData(long entryId, String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime, long size)
    {
      super(entryId);
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
     * @param entryId entry id
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     */
    EntryData(long entryId, String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime)
    {
      this(entryId,storageName,storageDateTime,entryType,name,dateTime,0L);
    }

    /** get number of entries
     * @return entries
     */
    public long getEntries()
    {
      return 1;
    }

    /** get size
     * @return size [bytes]
     */
    public long getSize()
    {
      return size;
    }

    /** set restore state of entry
     * @param restoreState restore state
     */
    public void setState(RestoreStates restoreState)
    {
      this.restoreState = restoreState;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Entry {"+storageName+", "+name+", "+entryType+", dateTime="+dateTime+", "+getSize()+" bytes, checked="+checked+", state="+restoreState+"}";
    }
  };

  /** entry data map
   */
  class EntryDataMapX extends HashMap<String,WeakReference<EntryData>>
  {
    /** update entry data
     * @param indexId index entry id
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     * @param size size [bytes]
     */
    synchronized public EntryData update(long indexId, String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime, long size)
    {
      String hashKey = getHashKey(storageName,entryType,name);

      WeakReference<EntryData> reference = get(hashKey);
      EntryData entryData = (reference != null) ? get(hashKey).get() : null;
      if (entryData != null)
      {
        entryData.indexId         = indexId;
        entryData.storageName     = storageName;
        entryData.storageDateTime = storageDateTime;
        entryData.entryType       = entryType;
        entryData.name            = name;
        entryData.dateTime        = dateTime;
        entryData.size            = size;
      }
      else
      {
        entryData = new EntryData(indexId,
                                  storageName,
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
     * @param indexId index entry id
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     */
    synchronized public EntryData update(long indexId, String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime)
    {
      String hashKey = getHashKey(storageName,entryType,name);

      WeakReference<EntryData> reference = get(hashKey);
      EntryData entryData = (reference != null) ? get(hashKey).get() : null;
      if (entryData != null)
      {
        entryData.indexId         = indexId;
        entryData.storageName     = storageName;
        entryData.storageDateTime = storageDateTime;
        entryData.entryType       = entryType;
        entryData.name            = name;
        entryData.dateTime        = dateTime;
      }
      else
      {
        entryData = new EntryData(indexId,
                                  storageName,
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
  static class EntryDataComparator implements Comparator<EntryData>
  {
    // sort modes
    enum SortModes
    {
      ARCHIVE,
      NAME,
      TYPE,
      SIZE,
      DATE
    };

    private SortModes sortMode;

    /** create entry data comparator
     * @param table entry table
     * @param sortColumn sorting column
     */
    EntryDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SortModes.ARCHIVE;
      else if (table.getColumn(1) == sortColumn) sortMode = SortModes.NAME;
      else if (table.getColumn(2) == sortColumn) sortMode = SortModes.TYPE;
      else if (table.getColumn(3) == sortColumn) sortMode = SortModes.SIZE;
      else if (table.getColumn(4) == sortColumn) sortMode = SortModes.DATE;
      else                                       sortMode = SortModes.NAME;
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
Dprintf.dprintf("");
if ((entryData1 == null) || (entryData2 == null)) return 0;
      switch (sortMode)
      {
        case ARCHIVE:
          return entryData1.storageName.compareTo(entryData2.storageName);
        case NAME:
          return entryData1.name.compareTo(entryData2.name);
        case TYPE:
          return entryData1.entryType.compareTo(entryData2.entryType);
        case SIZE:
          if      (entryData1.size < entryData2.size) return -1;
          else if (entryData1.size > entryData2.size) return  1;
          else                                        return  0;
        case DATE:
          if      (entryData1.dateTime < entryData2.dateTime) return -1;
          else if (entryData1.dateTime > entryData2.dateTime) return  1;
          else                                                return  0;
        default:
          return 0;
      }
    }
  }

  /** update entry table thread
   */
  class UpdateEntryTableThread extends Thread
  {
    private final int PAGE_SIZE = 64;

    private Object           trigger            = new Object();   // trigger update object
    private boolean          updateCount        = false;
    private HashSet<Integer> updateOffsets      = new HashSet<Integer>();
    private int              count              = 0;
    private EntryTypes       entryType          = EntryTypes.ANY;
    private String           entryPattern       = "";
    private boolean          newestEntriesOnly  = false;
    private boolean          setUpdateIndicator = false;          // true to set color/cursor at update

    /** create update entry list thread
     */
    UpdateEntryTableThread()
    {
      super();
      setDaemon(true);
      setName("BARControl Update Entry List");
    }

    /** run update entries table thread
     */
    public void run()
    {
      boolean          updateCount        = true;
      HashSet<Integer> updateOffsets      = new HashSet<Integer>();
      boolean          setUpdateIndicator = true;
      try
      {
        for (;;)
        {
          boolean updateIndicator = false;
          {
            // set busy cursor, foreground color to inform about update
            if (setUpdateIndicator)
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  BARControl.waitCursor();
                  widgetEntryTable.setForeground(COLOR_MODIFIED);
                }
              });
              updateIndicator = true;
            }
          }
          try
          {
            // update table count, table segment
            try
            {
              if (updateCount)
              {
                updateEntryTableCount();
              }
              if (!updateOffsets.isEmpty())
              {
                updateEntryTable(updateOffsets);
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
            // reset cursor, foreground color
            if (updateIndicator)
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  widgetEntryTable.setForeground(null);
                  BARControl.resetCursor();
                }
              });
            }
          }

          // wait for trigger
          synchronized(trigger)
          {
            // wait for refresh request trigger or timeout
            if (!this.updateCount && this.updateOffsets.isEmpty())
            {
              try { trigger.wait(30*1000); } catch (InterruptedException exception) { /* ignored */ };
            }

            // get update count, offsets to update
            updateCount        = this.updateCount;
            updateOffsets.addAll(this.updateOffsets);
            setUpdateIndicator = this.setUpdateIndicator;

            // if not triggered (timeout occurred) update count is done invisible (color is not set)
            if (!this.updateCount && this.updateOffsets.isEmpty())
            {
              updateCount        = true;
              setUpdateIndicator = false;
            }

            // wait for immediate further triggers
            do
            {
              this.updateCount        = false;
              this.updateOffsets.clear();
              this.setUpdateIndicator = false;

              try { trigger.wait(500); } catch (InterruptedException exception) { /* ignored */ };
              updateOffsets.addAll(this.updateOffsets);
            }
            while (this.updateCount || !this.updateOffsets.isEmpty());
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

    /** get count
     * @return count
     */
    public int getCount()
    {
      return count;
    }

    /** get entry type
     * @return entry type
     */
    public EntryTypes getEntryType()
    {
      return entryType;
    }

    /** get entry pattern
     * @return entry pattern
     */
    public String getEntryPattern()
    {
      return entryPattern;
    }

    /** get newest-entries-only
     * @return true for newest entries only
     */
    public boolean getNewestEntriesOnly()
    {
      return newestEntriesOnly;
    }

    /** trigger update of entry list
     * @param entryPattern new entry pattern or null
     * @param type type or *
     * @param newestEntriesOnly flag for newest entries only or null
     */
    public void triggerUpdate(String entryPattern, String type, boolean newestEntriesOnly)
    {
      synchronized(trigger)
      {
        if (   (this.entryPattern == null) || (entryPattern == null) || !this.entryPattern.equals(entryPattern)
            || (entryType != this.entryType)
            || (this.newestEntriesOnly != newestEntriesOnly)
           )
        {
          this.entryPattern       = entryPattern;
          this.entryType          = entryType;
          this.newestEntriesOnly  = newestEntriesOnly;
          this.setUpdateIndicator = true;
          this.updateCount        = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of entry list
     * @param entryPattern new entry pattern or null
     */
    public void triggerUpdateEntryPattern(String entryPattern)
    {
      assert entryPattern != null;

      synchronized(trigger)
      {
        if ((this.entryPattern == null) || (entryPattern == null) || !this.entryPattern.equals(entryPattern))
        {
          this.entryPattern       = entryPattern;
          this.setUpdateIndicator = true;
          this.updateCount        = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of entry list
     * @param entryType entry type
     */
    public void triggerUpdateEntryType(EntryTypes entryType)
    {
      synchronized(trigger)
      {
        if (entryType != this.entryType)
        {
          this.entryType          = entryType;
          this.setUpdateIndicator = true;
          this.updateCount        = true;
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
          this.setUpdateIndicator = true;
          this.updateCount        = true;
          trigger.notify();
        }
      }
    }

    /** trigger update entry list item
     * @param index index in list to start update
     */
    public void triggerUpdateTableItem(int index)
    {
      synchronized(trigger)
      {
        int offset = (index/PAGE_SIZE)*PAGE_SIZE;
        if (!updateOffsets.contains(offset))
        {
          updateOffsets.add(offset);
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
        updateCount = true;
        trigger.notify();
      }
    }

    /** check if update triggered
     * @return true iff update triggered
     */
    private boolean isUpdateTriggered()
    {
      return updateCount || !updateOffsets.isEmpty();
    }

    /** refresh entry table display count
     */
    private void updateEntryTableCount()
    {
      assert entryPattern != null;

      int oldCount = count;

      // get entries info
      final String[] errorMessage = new String[1];
      ValueMap       valueMap     = new ValueMap();
      if (BARServer.executeCommand(StringParser.format("INDEX_ENTRIES_INFO entryPattern=%'S indexType=%s newestEntriesOnly=%y",
                                                       entryPattern,
                                                       entryType.toString(),
                                                       newestEntriesOnly
                                                      ),
                                   0,  // debugLevel
                                   errorMessage,
                                   valueMap
                                  ) == Errors.NONE
         )
      {
        count = valueMap.getInt("count");
        if ((oldCount > 0) && (oldCount <= MAX_SHOWN_ENTRIES) && (count > MAX_SHOWN_ENTRIES))
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              Dialogs.warning(shell,
                              Dialogs.booleanFieldUpdater(Settings.class,"showEntriesExceededInfo"),
                              BARControl.tr("There are {0} entries. Only the first {1} are shown in the list.",
                                            updateEntryTableThread.getCount(),
                                            MAX_SHOWN_ENTRIES
                                           )
                             );
            }
          });
        }
      }

      if (oldCount != count)
      {
        // set count
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetEntryTableTitle.redraw();

            widgetEntryTable.setRedraw(false);

            widgetEntryTable.setItemCount(0);
            widgetEntryTable.clearAll();

            widgetEntryTable.setItemCount(Math.min(count,MAX_SHOWN_ENTRIES));
            widgetEntryTable.setTopIndex(0);

            widgetEntryTable.setRedraw(true);
          }
        });
      }
    }

    /** refresh entry table items
     * @param offset refresh offset
     * @return true iff update done
     */
    private boolean updateEntryTable(final int offset)
    {
      assert entryPattern != null;
      assert offset >= 0;
      assert count >= 0;

      // get limit
      final int limit = ((offset+PAGE_SIZE) < count) ? PAGE_SIZE : count-offset;

//TODO: sort
//IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTable,tableColumn);

      // update entry table segment
      final String[] errorMessage = new String[1];
      final int[]    n            = new int[1];
      BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST entryPattern=%'S indexType=%s newestEntriesOnly=%y offset=%d limit=%d",
                                                   entryPattern,
                                                   entryType.toString(),
                                                   newestEntriesOnly,
                                                   offset,
                                                   limit
                                                  ),
                               1,  // debugLevel
                               new CommandResultHandler()
                               {
                                 public int handleResult(int i, ValueMap valueMap)
                                 {
                                   final int index = offset+i;

                                   try
                                   {
                                     switch (valueMap.getEnum("entryType",EntryTypes.class))
                                     {
                                       case FILE:
                                         {
                                           long   entryId         = valueMap.getLong  ("entryId"        );
                                           String storageName     = valueMap.getString("storageName"    );
                                           long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                           String fileName        = valueMap.getString("name"           );
                                           long   dateTime        = valueMap.getLong  ("dateTime"       );
                                           long   size            = valueMap.getLong  ("size"           );
                                           long   fragmentOffset  = valueMap.getLong  ("fragmentOffset" );
                                           long   fragmentSize    = valueMap.getLong  ("fragmentSize"   );

                                           // add entry data index
                                           final EntryData entryData = new EntryData(entryId,
                                                                                     storageName,
                                                                                     storageDateTime,
                                                                                     EntryTypes.FILE,
                                                                                     fileName,
                                                                                     dateTime,
                                                                                     size
                                                                                    );

                                           display.syncExec(new Runnable()
                                           {
                                             public void run()
                                             {
                                               TableItem tableItem = widgetEntryTable.getItem(index);

                                               Widgets.updateTableItem(tableItem,
                                                                       (Object)entryData,
                                                                       entryData.storageName,
                                                                       entryData.name,
                                                                       "FILE",
                                                                       Units.formatByteSize(entryData.size),
                                                                       simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryData.indexId));
                                             }
                                           });
                                         }
                                         break;
                                       case IMAGE:
                                         {
                                           long   entryId         = valueMap.getLong  ("entryId"        );
                                           String storageName     = valueMap.getString("storageName"    );
                                           long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                           String imageName       = valueMap.getString("name"           );
                                           long   size            = valueMap.getLong  ("size"           );
                                           long   blockOffset     = valueMap.getLong  ("blockOffset"    );
                                           long   blockCount      = valueMap.getLong  ("blockCount"     );

                                           // add entry data index
                                           final EntryData entryData = new EntryData(entryId,
                                                                                     storageName,
                                                                                     storageDateTime,
                                                                                     EntryTypes.IMAGE,
                                                                                     imageName,
                                                                                     0L,
                                                                                     size
                                                                                    );

                                           // update/insert table item
                                           display.syncExec(new Runnable()
                                           {
                                             public void run()
                                             {
                                               TableItem tableItem = widgetEntryTable.getItem(index);

                                               Widgets.updateTableItem(tableItem,
                                                                       (Object)entryData,
                                                                       entryData.storageName,
                                                                       entryData.name,
                                                                       "IMAGE",
                                                                       Units.formatByteSize(entryData.size),
                                                                       simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryData.indexId));
                                             }
                                           });
                                         }
                                         break;
                                       case DIRECTORY:
                                         {
                                           long   entryId         = valueMap.getLong  ("entryId"        );
                                           String storageName     = valueMap.getString("storageName"    );
                                           long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                           String directoryName   = valueMap.getString("name"           );
                                           long   dateTime        = valueMap.getLong  ("dateTime"       );

                                           // add entry data index
                                           final EntryData entryData = new EntryData(entryId,
                                                                                     storageName,
                                                                                     storageDateTime,
                                                                                     EntryTypes.DIRECTORY,
                                                                                     directoryName,
                                                                                     dateTime
                                                                                    );

                                           // update/insert table item
                                           display.syncExec(new Runnable()
                                           {
                                             public void run()
                                             {
                                               TableItem tableItem = widgetEntryTable.getItem(index);

                                               Widgets.updateTableItem(tableItem,
                                                                       (Object)entryData,
                                                                       entryData.storageName,
                                                                       entryData.name,
                                                                       "DIR",
                                                                       "",
                                                                       simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryData.indexId));
                                             }
                                           });
                                         }
                                         break;
                                       case LINK:
                                         {
                                           long   entryId         = valueMap.getLong  ("entryId"        );
                                           String storageName     = valueMap.getString("storageName"    );
                                           long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                           String linkName        = valueMap.getString("name"           );
                                           String destinationName = valueMap.getString("destinationName");
                                           long   dateTime        = valueMap.getLong  ("dateTime"       );

                                           // add entry data index
                                           final EntryData entryData = new EntryData(entryId,
                                                                                     storageName,
                                                                                     storageDateTime,
                                                                                     EntryTypes.LINK,
                                                                                     linkName,
                                                                                     dateTime
                                                                                    );

                                           // update/insert table item
                                           display.syncExec(new Runnable()
                                           {
                                             public void run()
                                             {
                                               TableItem tableItem = widgetEntryTable.getItem(index);

                                               Widgets.updateTableItem(tableItem,
                                                                       (Object)entryData,
                                                                       entryData.storageName,
                                                                       entryData.name,
                                                                       "LINK",
                                                                       "",
                                                                       simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryData.indexId));
                                             }
                                           });
                                         }
                                         break;
                                       case HARDLINK:
                                         {
                                           long   entryId         = valueMap.getLong  ("entryId"        );
                                           String storageName     = valueMap.getString("storageName"    );
                                           long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                           String fileName        = valueMap.getString("name"           );
                                           long   dateTime        = valueMap.getLong  ("dateTime"       );
                                           long   size            = valueMap.getLong  ("size"           );
                                           long   fragmentOffset  = valueMap.getLong  ("fragmentOffset" );
                                           long   fragmentSize    = valueMap.getLong  ("fragmentSize"   );

                                           // add entry data index
                                           final EntryData entryData = new EntryData(entryId,
                                                                                     storageName,
                                                                                     storageDateTime,
                                                                                     EntryTypes.HARDLINK,
                                                                                     fileName,
                                                                                     dateTime,
                                                                                     size
                                                                                    );

                                           // update/insert table item
                                           display.syncExec(new Runnable()
                                           {
                                             public void run()
                                             {
                                               TableItem tableItem = widgetEntryTable.getItem(index);

                                               Widgets.updateTableItem(tableItem,
                                                                       (Object)entryData,
                                                                       entryData.storageName,
                                                                       entryData.name,
                                                                       "HARDLINK",
                                                                       Units.formatByteSize(entryData.size),
                                                                       simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryData.indexId));
                                             }
                                           });
                                         }
                                         break;
                                       case SPECIAL:
                                         {

                                           long   entryId         = valueMap.getLong  ("entryId"        );
                                           String storageName     = valueMap.getString("storageName"    );
                                           long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                           String name            = valueMap.getString("name"           );
                                           long   dateTime        = valueMap.getLong  ("dateTime"       );

                                           // add entry data index
                                           final EntryData entryData = new EntryData(entryId,
                                                                                     storageName,
                                                                                     storageDateTime,
                                                                                     EntryTypes.SPECIAL,
                                                                                     name,dateTime
                                                                                    );

                                           // update/insert table item
                                           display.syncExec(new Runnable()
                                           {
                                             public void run()
                                             {
                                               TableItem tableItem = widgetEntryTable.getItem(index);

                                               Widgets.updateTableItem(tableItem,
                                                                       (Object)entryData,
                                                                       entryData.storageName,
                                                                       entryData.name,
                                                                       "DEVICE",
                                                                       Units.formatByteSize(entryData.size),
                                                                       simpleDateFormat.format(new Date(entryData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryData.indexId));
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

                                   // store number of entries
                                   n[0] = i+1;

                                   // check if aborted
                                   if (isUpdateTriggered() || (n[0] >= limit))
                                   {
Dprintf.dprintf("");
//                                     abort();
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );

      return n[0] >= limit;
    }

    /** refresh entry table items
     * @param updateOffsets segment offsets to update
     */
    private void updateEntryTable(HashSet<Integer> updateOffsets)
    {
      try
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(false);
          }
        });

        Integer offsets[] = updateOffsets.toArray(new Integer[updateOffsets.size()]);
        for (Integer offset : offsets)
        {
          if (!updateEntryTable(offset)) break;
          updateOffsets.remove(offset);
        }
      }
      finally
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(true);
          }
        });
      }
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

  // date/time format
  private final SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  private final int STORAGE_TREE_MENU_START_INDEX = 0;
  private final int STORAGE_LIST_MENU_START_INDEX = 5;  // entry 0..3: new "..."; entry 4: separator

  // --------------------------- variables --------------------------------

  // global variable references
  private Shell                        shell;
  private Display                      display;

  // widgets
  public  Composite                    widgetTab;
  private TabFolder                    widgetTabFolder;

  private TabFolder                    widgetStorageTabFolderTitle;
  private TabFolder                    widgetStorageTabFolder;
  private Tree                         widgetStorageTree;
  private Shell                        widgetStorageTreeToolTip = null;
  private Menu                         widgetStorageTreeAssignToMenu;
  private Table                        widgetStorageTable;
  private Shell                        widgetStorageTableToolTip = null;
//TODO: NYI
  private Menu                         widgetStorageTableAssignToMenu;
  private Text                         widgetStoragePattern;
  private Combo                        widgetStorageState;
  private WidgetEvent                  checkedStorageEvent = new WidgetEvent();     // triggered when checked-state of some storage changed

  private Label                        widgetEntryTableTitle;
  private Table                        widgetEntryTable;
  private Shell                        widgetEntryTableToolTip = null;
  private WidgetEvent                  checkedEntryEvent = new WidgetEvent();       // triggered when checked-state of some entry changed

  private Button                       widgetRestoreTo;
  private Text                         widgetRestoreToDirectory;
  private Button                       widgetOverwriteEntries;
  private WidgetEvent                  selectRestoreToEvent = new WidgetEvent();

  private UpdateStorageTreeTableThread updateStorageTreeTableThread = new UpdateStorageTreeTableThread();
  private IndexData                    selectedIndexData = null;
  final private IndexIdSet             selectedIndexIdSet = new IndexIdSet();

  private UpdateEntryTableThread       updateEntryTableThread = new UpdateEntryTableThread();
  final private IndexIdSet             selectedEntryIdSet = new IndexIdSet();

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

    label = Widgets.newLabel(widgetEntryTableToolTip,String.format(BARControl.tr("%s (%d bytes)"),Units.formatByteSize(entryData.size),entryData.size));
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

Dprintf.dprintf("");
    label = Widgets.newLabel(widgetStorageTreeToolTip,"xxx");//entityIndexData.name);
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,0,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last created")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,1,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,(entityIndexData.lastCreatedDateTime > 0) ? simpleDateFormat.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,1,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last error")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,2,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.lastErrorMessage);
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,2,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total entries")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,3,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,String.format("%d",entityIndexData.getEntries()));
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,3,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total size")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,4,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("%s (%d bytes)"),Units.formatByteSize(entityIndexData.getSize()),entityIndexData.getSize()));
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

    label = Widgets.newLabel(widgetStorageTableToolTip,simpleDateFormat.format(new Date(storageIndexData.lastCreatedDateTime*1000L)));
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

    label = Widgets.newLabel(widgetStorageTableToolTip,String.format("%d",storageIndexData.getEntries()));
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,4,1,TableLayoutData.WE);

    label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Size")+":");
    label.setForeground(COLOR_INFO_FORGROUND);
    label.setBackground(COLOR_INFO_BACKGROUND);
    Widgets.layout(label,5,0,TableLayoutData.W);

    label = Widgets.newLabel(widgetStorageTableToolTip,String.format(BARControl.tr("%s (%d bytes)"),Units.formatByteSize(storageIndexData.getSize()),storageIndexData.getSize()));
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
  TabRestore(final TabFolder parentTabFolder, int accelerator)
  {
    TabFolder   tabFolder;
    Composite   tab;
    Menu        menu,subMenu;
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
    DragSource  dragSource;
    DropTarget  dropTarget;

    // get shell, display
    shell   = parentTabFolder.getShell();
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

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Restore")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);
    parentTabFolder.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        TabItem tabItem = (TabItem)selectionEvent.item;
        TabItem[] tabItems = parentTabFolder.getItems();
        if (tabItem == tabItems[2])
        {
          if (updateEntryTableThread.getCount() >= MAX_SHOWN_ENTRIES)
          {
            Dialogs.warning(shell,
                            Dialogs.booleanFieldUpdater(Settings.class,"showEntriesExceededInfo"),
                            BARControl.tr("There are {0} entries. Only the first {1} are shown in the list.",
                                          updateEntryTableThread.getCount(),
                                          MAX_SHOWN_ENTRIES
                                         )
                           );
          }
        }
      }
    });

    // create pane
    pane = Widgets.newPane(widgetTab,2,SWT.HORIZONTAL);
    Widgets.layout(pane,0,0,TableLayoutData.NSWE);

    // storage tree/list
    composite = pane.getComposite(0);
    composite.setLayout(new TableLayout(1.0,1.0));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);
    group = Widgets.newGroup(composite);  // Note: no title; title is drawn in "tab-area" together with number of entries
    group.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,4));
    Widgets.layout(group,0,0,TableLayoutData.NSWE);
    {
      widgetStorageTabFolder = Widgets.newTabFolder(group);
      Widgets.layout(widgetStorageTabFolder,0,0,TableLayoutData.NSWE);

      widgetStorageTabFolderTitle = widgetStorageTabFolder;
      widgetStorageTabFolderTitle.addPaintListener(new PaintListener()
      {
        public void paintControl(PaintEvent paintEvent)
        {
          TabFolder widget = (TabFolder)paintEvent.widget;
          GC        gc     = paintEvent.gc;
          Rectangle bounds = widget.getBounds();
          String    text;
          Point     size;

          text = BARControl.tr("Storage");
          size = Widgets.getTextSize(gc,text);
          gc.drawText(text,
                      (bounds.width-size.x)/2,
                      8
                     );

          text = BARControl.tr("Count: {0}",updateStorageTreeTableThread.getCount());
          size = Widgets.getTextSize(gc,text);
          gc.drawText(text,
                      bounds.width-size.x-8,
                      8
                     );
        }
      });

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
//TODO remove
      widgetStorageTabFolder.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetStorageTree.addListener(SWT.Expand,new Listener()
      {
        @Override
        public void handleEvent(final Event event)
        {
          TreeItem treeItem = (TreeItem)event.item;
          treeItem.removeAll();
          updateStorageTreeTableThread.updateTreeItems(treeItem);
          treeItem.setExpanded(true);
        }
      });
      widgetStorageTree.addListener(SWT.Collapse,new Listener()
      {
        @Override
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
        @Override
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
              selectedIndexIdSet.set(storageIndexData.indexId,treeItem.getChecked());
              checkedStorageEvent.trigger();
            }
          }
        }
      });
      widgetStorageTree.addSelectionListener(new SelectionListener()
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
            if (selectionEvent.detail == SWT.CHECK)
            {
              boolean   isChecked = treeItem.getChecked();
              IndexData indexData = (IndexData)treeItem.getData();

              selectedIndexIdSet.set(indexData.indexId,isChecked);
              if (treeItem.getExpanded())
              {
                // check/uncheck all
                for (TreeItem subTreeItem : Widgets.getAllTreeItems(treeItem))
                {
                  subTreeItem.setChecked(isChecked);
                }
              }

              // trigger update checked
              checkedStorageEvent.trigger();
            }
          }
        }
      });
      widgetStorageTree.addMouseTrackListener(new MouseTrackListener()
      {
        @Override
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }
        @Override
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

          // show if tree item available and mouse is in the right side
          if ((treeItem != null) && (mouseEvent.x > tree.getBounds().width/2))
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
        @Override
        public void keyPressed(KeyEvent keyEvent)
        {
        }
        @Override
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
Dprintf.dprintf("");
//              indexData.setChecked(!indexData.isChecked());

              Event treeEvent = new Event();
              treeEvent.item   = treeItem;
              treeEvent.detail = SWT.CHECK;
              widgetStorageTree.notifyListeners(SWT.Selection,treeEvent);
            }
          }
        }
      });
      dragSource = new DragSource(widgetStorageTree,DND.DROP_MOVE);
      dragSource.setTransfer(new Transfer[]{IndexDataTransfer.getInstance()});
      dragSource.addDragListener(new DragSourceListener()
      {
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
          Point point = new Point(dragSourceEvent.x,dragSourceEvent.y);

          TreeItem treeItem = widgetStorageTree.getItem(point);
          if (treeItem != null)
          {
            selectedIndexData = (IndexData)treeItem.getData();
          }
          else
          {
            dragSourceEvent.doit = false;
          }
        }
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          dragSourceEvent.data = selectedIndexData;
//TODO
Dprintf.dprintf("dragSourceEvent.data=%s",dragSourceEvent.data);
        }
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
          selectedIndexData = null;
        }
      });
      dropTarget = new DropTarget(widgetStorageTree,DND.DROP_MOVE);
      dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance(),IndexDataTransfer.getInstance()});
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
//TODO
Dprintf.dprintf("dropTargetEvent.data=%s",dropTargetEvent.data);
          if (dropTargetEvent.data != null)
          {
            Point point = display.map(null,widgetStorageTree,dropTargetEvent.x,dropTargetEvent.y);

            TreeItem treeItem = widgetStorageTree.getItem(point);
            if (treeItem != null)
            {
              IndexData fromIndexData = (IndexData)dropTargetEvent.data;
              IndexData toIndexData   = (IndexData)treeItem.getData();
//TODO
Dprintf.dprintf("fromIndexData=%s",fromIndexData);
Dprintf.dprintf("toIndexData=%s",toIndexData);

              if      (toIndexData instanceof UUIDIndexData)
              {
Dprintf.dprintf("");
              }
              else if (toIndexData instanceof EntityIndexData)
              {
                EntityIndexData toEntityIndexData = (EntityIndexData)toIndexData;
                assignStorage(fromIndexData,toEntityIndexData);
              }
              else if (toIndexData instanceof StorageIndexData)
              {
                StorageIndexData toStorageIndexData = (StorageIndexData)toIndexData;
                if (toStorageIndexData.getTreeItem() != null)
                {
Dprintf.dprintf("ubsP? toStorageIndexData.getTreeItem()=%s",toStorageIndexData.getTreeItem());
Dprintf.dprintf("ubsP? toStorageIndexData.getTreeItem()=%s",toStorageIndexData.getTreeItem().getParent());
                  EntityIndexData toEntityIndexData = (EntityIndexData)toStorageIndexData.getTreeItem().getParent().getData();
Dprintf.dprintf("ubsP? toEntityIndexData=%s",toEntityIndexData);
                  if (toEntityIndexData != null)
                  {
                    assignStorage(fromIndexData,toEntityIndexData);
                  }
                }
              }
            }
          }
          else
          {
            dropTargetEvent.detail = DND.DROP_NONE;
          }
        }
      });

      // list
      tab = Widgets.addTab(widgetStorageTabFolder,BARControl.tr("List"));
      tab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,2));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);

      widgetStorageTable = Widgets.newTable(tab,SWT.CHECK|SWT.VIRTUAL);
      widgetStorageTable.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0}));
      Widgets.layout(widgetStorageTable,1,0,TableLayoutData.NSWE);
      SelectionListener storageTableColumnSelectionListener = new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableColumn tableColumn = (TableColumn)selectionEvent.widget;
          synchronized(widgetStorageTable)
          {
            widgetStorageTable.setRedraw(false);

            Widgets.setSortTableColumn(widgetStorageTable,tableColumn);

            int count        = widgetStorageTable.getItemCount();
            int topItemIndex = widgetStorageTable.getTopIndex();

            widgetStorageTable.setItemCount(0);
            widgetStorageTable.clearAll();

            widgetStorageTable.setItemCount(count);
            widgetStorageTable.setTopIndex(topItemIndex);

            widgetStorageTable.setRedraw(true);
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
      widgetStorageTable.addListener(SWT.SetData,new Listener()
      {
        @Override
        public void handleEvent(final Event event)
        {
          TableItem tableItem = (TableItem)event.item;

          int i = widgetStorageTable.indexOf(tableItem);
          updateStorageTreeTableThread.triggerUpdate(i);
        }
      });
      widgetStorageTable.addListener(SWT.MouseDoubleClick,new Listener()
      {
        @Override
        public void handleEvent(final Event event)
        {
          TableItem tabletem = widgetStorageTable.getItem(new Point(event.x,event.y));
          if (tabletem != null)
          {
            StorageIndexData storageIndexData = (StorageIndexData)tabletem.getData();
            selectedIndexIdSet.set(storageIndexData.indexId,tabletem.getChecked());
            checkedStorageEvent.trigger();
          }
        }
      });
      widgetStorageTable.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableItem tabletem = (TableItem)selectionEvent.item;
          if ((tabletem != null) && (selectionEvent.detail == SWT.NONE))
          {
            StorageIndexData storageIndexData = (StorageIndexData)tabletem.getData();
            if (storageIndexData != null)
            {
              selectedIndexIdSet.set(storageIndexData.indexId,tabletem.getChecked());
              checkedStorageEvent.trigger();
            }
          }
        }
      });
      widgetStorageTable.addMouseTrackListener(new MouseTrackListener()
      {
        @Override
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }
        @Override
        public void mouseExit(MouseEvent mouseEvent)
        {
          if (widgetStorageTableToolTip != null)
          {
            widgetStorageTableToolTip.dispose();
            widgetStorageTableToolTip = null;
          }
        }
        @Override
        public void mouseHover(MouseEvent mouseEvent)
        {
          Table     table     = (Table)mouseEvent.widget;
          TableItem tableItem = table.getItem(new Point(mouseEvent.x,mouseEvent.y));

          if (widgetStorageTableToolTip != null)
          {
            widgetStorageTableToolTip.dispose();
            widgetStorageTableToolTip = null;
          }

          // show if table item available and mouse is in the right side
          if ((tableItem != null) && (mouseEvent.x > table.getBounds().width/2))
          {
            StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();

            Point point = table.toDisplay(mouseEvent.x+16,mouseEvent.y);
            showStorageIndexToolTip(storageIndexData,point.x,point.y);
          }
        }
      });
      widgetStorageTable.addKeyListener(new KeyListener()
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
            refreshStorageIndex();
          }
          else if (Widgets.isAccelerator(keyEvent,SWT.DEL))
          {
            removeStorageIndex();
          }
          else if (Widgets.isAccelerator(keyEvent,SWT.SPACE))
          {
            // toggle check
            for (TableItem tableItem : widgetStorageTable.getSelection())
            {
              IndexData indexData = (IndexData)tableItem.getData();
Dprintf.dprintf("");
//              indexData.setChecked(!indexData.isChecked());

              Event treeEvent = new Event();
              treeEvent.item   = tableItem;
              treeEvent.detail = SWT.CHECK;
              widgetStorageTable.notifyListeners(SWT.Selection,treeEvent);
            }
          }
        }
      });

      menu = Widgets.newPopupMenu(shell);
      {
        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Refresh index")+"\u2026");
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
            refreshStorageIndex();
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Refresh all indizes with error")+"\u2026");
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
            refreshAllWithErrorStorageIndex();
          }
        });

        widgetStorageTreeAssignToMenu = Widgets.addMenu(menu,BARControl.tr("Assign to job")+"\u2026");
        {
        }

        subMenu = Widgets.addMenu(menu,BARControl.tr("Set job type\u2026"));
        {
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("normal")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              setEntityType(Settings.ArchiveTypes.NORMAL);
            }
          });
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("full")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              setEntityType(Settings.ArchiveTypes.FULL);
            }
          });
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("incremental")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              setEntityType(Settings.ArchiveTypes.INCREMENTAL);
            }
          });
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("differential")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              setEntityType(Settings.ArchiveTypes.DIFFERENTIAL);
            }
          });
        }

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Add to index")+"\u2026");
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
            addStorageIndex();
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
            MenuItem widget = (MenuItem)selectionEvent.widget;
            removeStorageIndex();
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove all indizes with error")+"\u2026");
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            removeAllWithErrorStorageIndex();
          }
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Mark all"));
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
            setAllCheckedStorage(true);
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Unmark all"));
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
            setAllCheckedStorage(false);
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Restore")+"\u2026");
        Widgets.addEventListener(new WidgetEventListener(menuItem,checkedStorageEvent)
        {
          @Override
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(!selectedIndexIdSet.isEmpty());
          }
        });
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            restoreArchives(selectedIndexIdSet);
          }
        });

        Widgets.addMenuSeparator(menu);

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
            deleteStorage();
          }
        });
      }
      menu.addMenuListener(new MenuListener()
      {
        @Override
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
        @Override
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
          @Override
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (!selectedIndexIdSet.isEmpty())
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
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button button = (Button)selectionEvent.widget;
            if (!selectedIndexIdSet.isEmpty())
            {
              setAllCheckedStorage(false);
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText(BARControl.tr("Mark all entries in list."));
            }
            else
            {
              setAllCheckedStorage(true);
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
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            updateStorageTreeTableThread.triggerUpdateStoragePattern(widget.getText());
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            updateStorageTreeTableThread.triggerUpdateStoragePattern(widget.getText());
          }
        });
        widgetStoragePattern.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
            Text widget = (Text)keyEvent.widget;
            updateStorageTreeTableThread.triggerUpdateStoragePattern(widget.getText());
          }
        });
//???
        widgetStoragePattern.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
//            Text widget = (Text)focusEvent.widget;
//            updateStorageTreeTableThread.triggerUpdateStoragePattern(widget.getText());
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
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
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
            updateStorageTreeTableThread.triggerUpdateStorageState(storageIndexStateSet,storageEntityState);
          }
        });
        updateStorageTreeTableThread.triggerUpdateStorageState(INDEX_STATE_SET_ALL,EntityStates.ANY);

        button = Widgets.newButton(composite,BARControl.tr("Restore")+"\u2026");
        button.setToolTipText(BARControl.tr("Start restoring selected archives."));
        button.setEnabled(false);
        Widgets.layout(button,0,5,TableLayoutData.DEFAULT,0,0,0,0,120,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,checkedStorageEvent)
        {
          @Override
          public void trigger(Control control)
          {
            control.setEnabled(!selectedIndexIdSet.isEmpty());
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
//TODO
Dprintf.dprintf("remove");
//            getCheckedIndexData(indexDataHashSet);
//            getSelectedIndexData(indexDataHashSet);

            restoreArchives(selectedIndexIdSet);
          }
        });
      }
    }

    // entries list
    composite = pane.getComposite(1);
    composite.setLayout(new TableLayout(1.0,1.0));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);

    group = Widgets.newGroup(composite);  // Note: no title; title is drawn in label below together with number of entries
    group.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,4));
    Widgets.layout(group,0,0,TableLayoutData.NSWE);
    {
      widgetEntryTableTitle = Widgets.newLabel(group);
      Widgets.layout(widgetEntryTableTitle,0,0,TableLayoutData.WE);
      widgetEntryTableTitle.addPaintListener(new PaintListener()
      {
        public void paintControl(PaintEvent paintEvent)
        {
          Label     widget = (Label)paintEvent.widget;
          GC        gc     = paintEvent.gc;
          Rectangle bounds = widget.getBounds();
          String    text;
          Point     size;

          // title
          text = BARControl.tr("Entries");
          size = Widgets.getTextSize(gc,text);
          gc.drawText(text,
                      (bounds.width-size.x)/2,
                      (bounds.height-size.y)/2
                     );

          // number of entries
          text = BARControl.tr("Count: {0}",updateEntryTableThread.getCount());
          size = Widgets.getTextSize(gc,text);
          gc.drawText(text,
                      bounds.width-size.x-8,
                      (bounds.height-size.y)/2
                     );
        }
      });

      widgetEntryTable = Widgets.newTable(group,SWT.CHECK|SWT.VIRTUAL);
      widgetEntryTable.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(widgetEntryTable,1,0,TableLayoutData.NSWE);
      SelectionListener entryListColumnSelectionListener = new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
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
      widgetEntryTable.addListener(SWT.SetData,new Listener()
      {
        @Override
        public void handleEvent(final Event event)
        {
          TableItem tableItem = (TableItem)event.item;

          int i = widgetEntryTable.indexOf(tableItem);
          updateEntryTableThread.triggerUpdateTableItem(i);
        }
      });
      widgetEntryTable.addListener(SWT.MouseDoubleClick,new Listener()
      {
        @Override
        public void handleEvent(final Event event)
        {
          TableItem tableItem = widgetEntryTable.getItem(new Point(event.x,event.y));
          if (tableItem != null)
          {
            tableItem.setChecked(!tableItem.getChecked());

            EntryData entryData = (EntryData)tableItem.getData();
            selectedEntryIdSet.set(entryData.indexId,tableItem.getChecked());

            checkedEntryEvent.trigger();
          }
        }
      });
      widgetEntryTable.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableItem tableItem = (TableItem)selectionEvent.item;
          if (tableItem != null)
          {
            EntryData entryData = (EntryData)tableItem.getData();
            if (entryData != null)
            {
              tableItem.setChecked(!tableItem.getChecked());
              selectedEntryIdSet.set(entryData.indexId,tableItem.getChecked());
            }
          }

          // trigger update checked
          checkedEntryEvent.trigger();
        }
      });
      widgetEntryTable.addMouseTrackListener(new MouseTrackListener()
      {
        @Override
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }
        @Override
        public void mouseExit(MouseEvent mouseEvent)
        {
          if (widgetEntryTableToolTip != null)
          {
            widgetEntryTableToolTip.dispose();
            widgetEntryTableToolTip = null;
          }
        }
        @Override
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
        @Override
        public void trigger(Control control)
        {
          updateEntryTableThread.triggerUpdate();
        }
      });

      menu = Widgets.newPopupMenu(shell);
      {
        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Mark all"));
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
            setAllCheckedEntries(true);
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Unmark all"));
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
            setAllCheckedEntries(false);
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Restore")+"\u2026");
        menuItem.setEnabled(false);
        Widgets.addEventListener(new WidgetEventListener(menuItem,checkedEntryEvent)
        {
          @Override
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(!selectedEntryIdSet.isEmpty());
          }
        });
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
            restoreEntries(selectedEntryIdSet);
          }
        });
      }
      menu.addMenuListener(new MenuListener()
      {
        @Override
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
        @Override
        public void menuHidden(MenuEvent menuEvent)
        {
        }
      });
      widgetEntryTable.setMenu(menu);

      // entry list filters
      composite = Widgets.newComposite(group);
      composite.setLayout(new TableLayout(null,new double[]{0.0,0.0,1.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(composite,2,0,TableLayoutData.WE);
      {
        button = Widgets.newButton(composite,IMAGE_MARK_ALL);
        Widgets.layout(button,0,0,TableLayoutData.E);
        Widgets.addEventListener(new WidgetEventListener(button,checkedEntryEvent)
        {
          @Override
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (!selectedEntryIdSet.isEmpty())
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
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button button = (Button)selectionEvent.widget;
            setAllCheckedEntries(selectedEntryIdSet.isEmpty());
            if (!selectedEntryIdSet.isEmpty())
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

        label = Widgets.newLabel(composite,BARControl.tr("Filter")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        text = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
        text.setToolTipText(BARControl.tr("Enter filter pattern for entry list. Wildcards: * and ?."));
        text.setMessage(BARControl.tr("Enter text to filter entry list"));
        Widgets.layout(text,0,2,TableLayoutData.WE);
        text.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text  widget = (Text)selectionEvent.widget;
            updateEntryTableThread.triggerUpdateEntryPattern(widget.getText());
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            updateEntryTableThread.triggerUpdateEntryPattern(widget.getText());
          }
        });
        text.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
            Text widget = (Text)keyEvent.widget;
            updateEntryTableThread.triggerUpdateEntryPattern(widget.getText());
          }
        });
//???
        text.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
//            Text widget = (Text)focusEvent.widget;
//            updateEntryTableThread.triggerUpdateEntryPattern(widget.getText());
          }
        });

        combo = Widgets.newOptionMenu(composite);
        combo.setToolTipText(BARControl.tr("Entry type."));
        combo.setItems(new String[]{"*","files","directories","links","hardlinks","special"});
        Widgets.setOptionMenuItems(combo,new Object[]{"*",          EntryTypes.ANY,
                                                      "files",      EntryTypes.FILE,
                                                      "images",     EntryTypes.IMAGE,
                                                      "directories",EntryTypes.DIRECTORY,
                                                      "links",      EntryTypes.LINK,
                                                      "hardlinks",  EntryTypes.HARDLINK,
                                                      "special",    EntryTypes.SPECIAL
                                                     }
                                  );
        Widgets.setSelectedOptionMenuItem(combo,EntryTypes.ANY);
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
            Combo      widget    = (Combo)selectionEvent.widget;
            EntryTypes entryType = Widgets.getSelectedOptionMenuItem(widget,EntryTypes.ANY);

            selectedEntryIdSet.clear();
            checkedEntryEvent.trigger();

            updateEntryTableThread.triggerUpdateEntryType(entryType);
          }
        });

        button = Widgets.newCheckbox(composite,BARControl.tr("newest only"));
        button.setToolTipText(BARControl.tr("When this checkbox is enabled, only show newest entry instances and hide all older entry instances."));
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
            Button widget = (Button)selectionEvent.widget;
            boolean newestEntriesOnly = widget.getSelection();

            selectedEntryIdSet.clear();
            checkedEntryEvent.trigger();

            updateEntryTableThread.triggerUpdateNewestEntriesOnly(newestEntriesOnly);
          }
        });

        button = Widgets.newButton(composite,BARControl.tr("Restore")+"\u2026");
        button.setToolTipText(BARControl.tr("Start restoring selected entries."));
        button.setEnabled(false);
        Widgets.layout(button,0,5,TableLayoutData.DEFAULT,0,0,0,0,120,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,checkedEntryEvent)
        {
          @Override
          public void trigger(Control control)
          {
            control.setEnabled(!selectedEntryIdSet.isEmpty());
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
            restoreEntries(selectedEntryIdSet);
          }
        });
      }
    }

    // destination
    group = Widgets.newGroup(widgetTab,BARControl.tr("Destination"));
    group.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0},4));
    Widgets.layout(group,1,0,TableLayoutData.WE);
    {
      // fix layout
      control = Widgets.newSpacer(group);
      Widgets.layout(control,0,0,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,1);

      widgetRestoreTo = Widgets.newCheckbox(group,BARControl.tr("to"));
      widgetRestoreTo.setToolTipText(BARControl.tr("Enable this checkbox and select a directory to restore entries to different location."));
      Widgets.layout(widgetRestoreTo,1,0,TableLayoutData.W);
      widgetRestoreTo.addSelectionListener(new SelectionListener()
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
          widgetRestoreTo.setSelection(checkedFlag);
          selectRestoreToEvent.trigger();
        }
      });

      widgetRestoreToDirectory = Widgets.newText(group);
      widgetRestoreToDirectory.setEnabled(false);
      Widgets.layout(widgetRestoreToDirectory,1,1,TableLayoutData.WE);
      Widgets.addEventListener(new WidgetEventListener(widgetRestoreToDirectory,selectRestoreToEvent)
      {
        @Override
        public void trigger(Control control)
        {
          control.setEnabled(widgetRestoreTo.getSelection());
        }
      });
      group.addMouseListener(new MouseListener()
      {
        @Override
        public void mouseDoubleClick(final MouseEvent mouseEvent)
        {
        }
        @Override
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
        @Override
        public void mouseUp(final MouseEvent mouseEvent)
        {
        }
      });

      button = Widgets.newButton(group,IMAGE_DIRECTORY);
      Widgets.layout(button,1,2,TableLayoutData.DEFAULT);
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
                                    BARControl.tr("Select path"),
                                    widgetRestoreTo.getText(),
                                    BARServer.remoteListDirectory
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
    updateStorageTreeTableThread.start();
    updateEntryTableThread.start();
  }

  //-----------------------------------------------------------------------

  /** set/clear checked all storage entries
   * @param checked true for set checked, false for clear checked
   */
  private void setAllCheckedStorage(final boolean checked)
  {
    final int MAX_CONFIRM_ENTRIES = 1000;

    final String[] errorMessage = new String[1];
    ValueMap       valueMap     = new ValueMap();

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
Dprintf.dprintf("");
//            uuidIndexData.setChecked(checked);

            if (treeItem.getExpanded())
            {
              for (TreeItem entityTreeItem : treeItem.getItems())
              {
                EntityIndexData entityIndexData = (EntityIndexData)entityTreeItem.getData();
Dprintf.dprintf("");
//                entityIndexData.setChecked(checked);

                if (entityTreeItem.getExpanded())
                {
                  for (TreeItem storageTreeItem : entityTreeItem.getItems())
                  {
                    StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
Dprintf.dprintf("");
//                    storageIndexData.setChecked(checked);
                  }
                }
              }
            }
          }
          else if (indexData instanceof EntityIndexData)
          {
            EntityIndexData entityIndexData = (EntityIndexData)indexData;
Dprintf.dprintf("");
//            entityIndexData.setChecked(checked);

            if (treeItem.getExpanded())
            {
              for (TreeItem storageTreeItem : treeItem.getItems())
              {
                StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
Dprintf.dprintf("");
//                storageIndexData.setChecked(checked);
              }
            }
          }
          else if (indexData instanceof StorageIndexData)
          {
            StorageIndexData storageIndexData = (StorageIndexData)indexData;
Dprintf.dprintf("");
//            storageIndexData.setChecked(checked);
          }
        }
        break;
      case 1:
        final int     count[] = new int[]{0};
        final boolean doit[]  = new boolean[]{true};

        if (checked)
        {
          if (BARServer.executeCommand(StringParser.format("INDEX_STORAGES_INFO storagePattern=%'S indexStateSet=%s",
                                                           updateStorageTreeTableThread.getStoragePattern(),
                                                           updateStorageTreeTableThread.getStorageIndexStateSet().nameList("|")
                                                          ),
                                       0,  // debugLevel
                                       errorMessage,
                                       valueMap
                                      ) == Errors.NONE
             )
          {
            count[0] = valueMap.getInt("count");
            if (count[0] > MAX_CONFIRM_ENTRIES)
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  doit[0] = Dialogs.confirm(shell,
                                            Dialogs.booleanFieldUpdater(Settings.class,"showEntriesExceededInfo"),
                                            BARControl.tr("There are {0} entries. Really mark all entries?",
                                                          count[0]
                                                         )
                                           );
                }
              });
            }
          }
        }

        if (doit[0])
        {
          // check/uncheck all entries
          final int n[] = new int[]{0};
          final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Mark entries"),500,100,null,BusyDialog.PROGRESS_BAR0);
          busyDialog.setMaximum(count[0]);
          int error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s storagePattern=%'S indexStateSet=%s indexModeSet=%s",
                                                                   "*",
                                                                   updateStorageTreeTableThread.getStoragePattern(),
                                                                   updateStorageTreeTableThread.getStorageIndexStateSet().nameList("|"),
                                                                   "*"
                                                                  ),
                                               1,  // debugLevel
                                               errorMessage,
                                               new CommandResultHandler()
                                               {
                                                 public int handleResult(int i, ValueMap valueMap)
                                                 {
                                                   long storageId = valueMap.getLong("storageId");

                                                   selectedIndexIdSet.set(storageId,checked);

                                                   n[0]++;
                                                   busyDialog.updateProgressBar(n[0]);

                                                   return Errors.NONE;
                                                 }
                                               }
                                              );
          busyDialog.close();
          if (error != Errors.NONE)
          {
            Dialogs.error(shell,BARControl.tr("Cannot mark all storages\n\n(error: {0})",errorMessage[0]));
            return;
          }

          // refresh table
          Widgets.refreshVirtualTable(widgetStorageTable);
        }
    }

    // trigger update checked
    checkedStorageEvent.trigger();
  }

  /** get checked storage
   * @param storageIndexDataHashSet storage index data hash set to fill
   * @return checked storage hash set
   */
//TODO: obsolete
  private HashSet<IndexData> getCheckedIndexData(HashSet<IndexData> indexDataHashSet)
  {
Dprintf.dprintf("remove");
    IndexData indexData;
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        // tree view
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          indexData = (IndexData)uuidTreeItem.getData();
          if      (!uuidTreeItem.getGrayed())// && indexData.isChecked())
          {
            indexDataHashSet.add(indexData);
          }
          else if (uuidTreeItem.getExpanded())
          {
            for (TreeItem entityTreeItem : uuidTreeItem.getItems())
            {
              indexData = (IndexData)entityTreeItem.getData();
              if      (!entityTreeItem.getGrayed())// && indexData.isChecked())
              {
                indexDataHashSet.add(indexData);
              }
              else if (entityTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : entityTreeItem.getItems())
                {
                  indexData = (IndexData)storageTreeItem.getData();
                  if (!storageTreeItem.getGrayed())// && indexData.isChecked())
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
        // table view
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
//TODO: obsolete?
  private HashSet<IndexData> getSelectedIndexData(HashSet<IndexData> indexDataHashSet)
  {
Dprintf.dprintf("");

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
        // table view
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
        ValueMap valueMap     = new ValueMap();

        if      (treeItem.getData() instanceof UUIDIndexData)
        {
          // get job index data
          final HashSet<TreeItem> removeEntityTreeItemSet = new HashSet<TreeItem>();
          display.syncExec(new Runnable()
          {
            @Override
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
          while (   !command.endOfData()
                 && command.getNextResult(errorMessage,
                                          valueMap,
                                          Command.TIMEOUT
                                         ) == Errors.NONE
                )
          {
            try
            {
              long                  entityId            = valueMap.getLong  ("entityId"                               );
              String                jobUUID             = valueMap.getString("jobUUID"                                );
              String                scheuduleUUID       = valueMap.getString("scheduleUUID"                           );
              Settings.ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
              long                  lastCreatedDateTime = valueMap.getLong  ("lastCreatedDateTime"                    );
              String                lastErrorMessage    = valueMap.getString("lastErrorMessage"                       );
              long                  totalEntries        = valueMap.getLong  ("totalEntries"                           );
              long                  totalSize           = valueMap.getLong  ("totalSize"                              );

              // add entity data index
              final EntityIndexData entityIndexData = new EntityIndexData(entityId,
                                                                          archiveType,
                                                                          lastCreatedDateTime,
                                                                          lastErrorMessage,
                                                                          totalEntries,
                                                                          totalSize
                                                                         );

              // insert/update tree item
              display.syncExec(new Runnable()
              {
                @Override
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

          // remove not existing entries
          display.syncExec(new Runnable()
          {
            @Override
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
            @Override
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
// TODO
assert storagePattern != null;
          Command command = BARServer.runCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%d maxCount=%d storagePattern=%'S indexStateSet=%s indexModeSet=%s offset=0",
                                                                     entityIndexData.indexId,
                                                                     -1,
                                                                     storagePattern,
                                                                     "*",
                                                                     "*"
                                                                    ),
                                                 1
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
              long                  storageId           = valueMap.getLong  ("storageId"                              );
              String                jobUUID             = valueMap.getString("jobUUID"                                );
              String                scheduleUUID        = valueMap.getString("scheduleUUID"                           );
              String                jobName             = valueMap.getString("jobName"                                );
              Settings.ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
              String                name                = valueMap.getString("name"                                   );
              long                  dateTime            = valueMap.getLong  ("dateTime"                               );
              long                  entries             = valueMap.getLong  ("entries"                                );
              long                  size                = valueMap.getLong  ("size"                                   );
              IndexStates           indexState          = valueMap.getEnum  ("indexState",IndexStates.class           );
              IndexModes            indexMode           = valueMap.getEnum  ("indexMode",IndexModes.class             );
              long                  lastCheckedDateTime = valueMap.getLong  ("lastCheckedDateTime"                    );
              String                errorMessage_       = valueMap.getString("errorMessage"                           );

              // add storage index data
              final StorageIndexData storageIndexData = new StorageIndexData(storageId,
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
                @Override
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

          // remove not existing entries
          display.syncExec(new Runnable()
          {
            @Override
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

  /** create entity for job and assing selected/checked job/entity/storage to job
   * @param toUUIDIndexData UUID index data
   * @param archiveType archive type
   */
  private void assignStorage(HashSet<IndexData> indexDataHashSet, UUIDIndexData toUUIDIndexData, Settings.ArchiveTypes archiveType)
  {
    if (!indexDataHashSet.isEmpty())
    {
      try
      {
        int      error        = Errors.UNKNOWN;
        String[] errorMessage = new String[1];
        ValueMap valueMap     = new ValueMap();

        error = BARServer.executeCommand(StringParser.format("INDEX_ENTITY_ADD jobUUID=%'S archiveType=%s",
                                                             toUUIDIndexData.jobUUID,
                                                             archiveType.toString()
                                                            ),
                                         0,  // debugLevel
                                         errorMessage,
                                         valueMap
                                        );
        if (error == Errors.NONE)
        {
          long entityId = valueMap.getLong("entityId");

          for (IndexData indexData : indexDataHashSet)
          {
            final String info = indexData.getInfo();

            if      (indexData instanceof UUIDIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toJobUUID=%'S toEntityId=0 archiveType=%s jobUUID=%'S",
                                                                   toUUIDIndexData.jobUUID,
                                                                   archiveType.toString(),
                                                                   ((UUIDIndexData)indexData).jobUUID
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            else if (indexData instanceof EntityIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toJobUUID=%'S toEntityId=0 archiveType=%s entityId=%lld",
                                                                   toUUIDIndexData.jobUUID,
                                                                   archiveType.toString(),
                                                                   indexData.indexId
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            else if (indexData instanceof StorageIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toJobUUID=%'S toEntityId=0 archiveType=%s storageId=%lld",
                                                                   toUUIDIndexData.jobUUID,
                                                                   archiveType.toString(),
                                                                   indexData.indexId
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            if (error != Errors.NONE)
            {
              Dialogs.error(shell,BARControl.tr("Cannot assign index for\n\n''{0}''\n\n(error: {1})",info,errorMessage[0]));
            }
          }
        }
        else
        {
          Dialogs.error(shell,BARControl.tr("Cannot create entity for\n\n''{0}''\n\n(error: {1})",toUUIDIndexData.jobUUID,errorMessage[0]));
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Communication error while assigning index database\n\n(error: {0})",error.toString()));
      }
      updateStorageTreeTableThread.triggerUpdate();
    }
  }

  /** assing storage to entity
   * @param indexData index data
   * @param toUUIDIndexData UUID index data
   * @param archiveType archive type
   */
  private void assignStorage(IndexData indexData, UUIDIndexData toUUIDIndexData, Settings.ArchiveTypes archiveType)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    indexDataHashSet.add(indexData);
    assignStorage(indexDataHashSet,toUUIDIndexData,archiveType);
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
    assignStorage(indexDataHashSet,toUUIDIndexData,archiveType);
  }

  /** assing storage to entity
   * @param indexDataHashSet index data hash set
   * @param toEntityIndexData entity index data
   */
  private void assignStorage(HashSet<IndexData> indexDataHashSet, EntityIndexData toEntityIndexData)
  {
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
            error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld jobUUID=%'S",
                                                                 toEntityIndexData.indexId,
                                                                 ((UUIDIndexData)indexData).jobUUID
                                                                ),
                                             0,  // debugLevel
                                             errorMessage
                                            );
          }
          else if (indexData instanceof EntityIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld entityId=%lld",
                                                                 toEntityIndexData.indexId,
                                                                 indexData.indexId
                                                                ),
                                             0,  // debugLevel
                                             errorMessage
                                            );
          }
          else if (indexData instanceof StorageIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld storageId=%lld",
                                                                 toEntityIndexData.indexId,
                                                                 indexData.indexId
                                                                ),
                                             0,  // debugLevel
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
      updateStorageTreeTableThread.triggerUpdate();
    }
  }

  /** assing storage to entity
   * @param indexData index data
   * @param toEntityIndexData entity index data
   */
  private void assignStorage(IndexData indexData, EntityIndexData toEntityIndexData)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    indexDataHashSet.add(indexData);
    assignStorage(indexDataHashSet,toEntityIndexData);
  }

  /** assing storage to entity
   * @param toEntityIndexData entity index data
   */
  private void assignStorage(EntityIndexData toEntityIndexData)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    getCheckedIndexData(indexDataHashSet);
    getSelectedIndexData(indexDataHashSet);
    assignStorage(indexDataHashSet,toEntityIndexData);
  }

  /** set entity type
   * @param entityIndexData entity index data
   * @param archiveType archive type
   */
  private void setEntityType(HashSet<IndexData> indexDataHashSet, Settings.ArchiveTypes archiveType)
  {
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
            UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

            error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toJobUUID=%'S archiveType=%s jobUUID=%'S",
                                                                 uuidIndexData.jobUUID,
                                                                 archiveType.toString(),
                                                                 uuidIndexData.jobUUID
                                                                ),
                                             0,  // debugLevel
                                             errorMessage
                                            );
          }
          else if (indexData instanceof EntityIndexData)
          {
            EntityIndexData entityIndexData = (EntityIndexData)indexData;

            error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ASSIGN toEntityId=%lld archiveType=%s entityId=%lld",
                                                                 entityIndexData.indexId,
                                                                 archiveType.toString(),
                                                                 entityIndexData.indexId
                                                                ),
                                             0,  // debugLevel
                                             errorMessage
                                            );
          }
          else if (indexData instanceof StorageIndexData)
          {
            // nothing to do
          }
          if (error == Errors.NONE)
          {
            indexData.setState(IndexStates.UPDATE_REQUESTED);
          }
          else
          {
            Dialogs.error(shell,BARControl.tr("Cannot set entity type for\n\n''{0}''\n\n(error: {1})",info,errorMessage[0]));
          }
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Communication error while set entity type in index database\n\n(error: {0})",error.toString()));
      }
      updateStorageTreeTableThread.triggerUpdate();
    }
  }

  /** set entity type
   * @param entityIndexData entity index data
   * @param archiveType archive type
   */
  private void setEntityType(EntityIndexData entityIndexData, Settings.ArchiveTypes archiveType)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    indexDataHashSet.add(entityIndexData);
    setEntityType(indexDataHashSet,archiveType);
  }

  /** set entity type
   * @param archiveType archive type
   */
  private void setEntityType(Settings.ArchiveTypes archiveType)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    getCheckedIndexData(indexDataHashSet);
    getSelectedIndexData(indexDataHashSet);
    setEntityType(indexDataHashSet,archiveType);
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
              error = BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=%s jobUUID=%'S",
                                                                   "*",
                                                                   ((UUIDIndexData)indexData).jobUUID
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            else if (indexData instanceof EntityIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=%s entityId=%d",
                                                                   "*",
                                                                   indexData.indexId
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            else if (indexData instanceof StorageIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=%s storageId=%d",
                                                                   "*",
                                                                   indexData.indexId
                                                                  ),
                                               0,  // debugLevel
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
        int error = BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=%s storageId=%d",
                                                                 "ERROR",
                                                                 0
                                                                ),
                                             0,  // debugLevel
                                             errorMessage
                                            );
        if (error == Errors.NONE)
        {
          updateStorageTreeTableThread.triggerUpdate();
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
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
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
          Dialogs.close(dialog,null);
        }
      });
    }

    // add selection listeners
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
        Dialogs.close(dialog,widgetStorageName.getText());
      }
    });

    // run dialog
    String storageName = (String)Dialogs.run(dialog,null);

    // add storage file
    if (storageName != null)
    {
      String[] errorMessage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ADD pattern=%S",storageName),
                                           0,  // debugLevel
                                           errorMessage
                                          );
      if (error == Errors.NONE)
      {
        updateStorageTreeTableThread.triggerUpdate();
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
          @Override
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
                busyDialog.updateText(0,"%s",info);

                // remove entry
                int            error        = Errors.UNKNOWN;
                final String[] errorMessage = new String[1];
                if      (indexData instanceof UUIDIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* jobUUID=%'S",
                                                                       ((UUIDIndexData)indexData).jobUUID
                                                                      ),
                                                   0,  // debugLevel
                                                   errorMessage
                                                  );
                }
                else if (indexData instanceof EntityIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* entityId=%d",
                                                                        indexData.indexId
                                                                       ),
                                                    0,  // debugLevel
                                                    errorMessage
                                                   );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* storageId=%d",
                                                                        indexData.indexId
                                                                      ),
                                                   0,  // debugLevel
                                                   errorMessage
                                                  );
                }
                if (error == Errors.NONE)
                {
                  Widgets.removeTreeItem(widgetStorageTree,indexData);
                  Widgets.removeTableItem(widgetStorageTable,indexData);
                }
                else
                {
                  display.syncExec(new Runnable()
                  {
                    @Override
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
                @Override
                public void run()
                {
                  busyDialog.close();
                }
              });

              updateStorageTreeTableThread.triggerUpdate();
            }
            catch (CommunicationError error)
            {
              final String errorMessage = error.getMessage();
              display.syncExec(new Runnable()
              {
                @Override
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
      ValueMap       valueMap     = new ValueMap();
      if (BARServer.executeCommand("INDEX_STORAGES_INFO storagePattern='*' indexStateSet=ERROR",
                                   0,  // debugLevel
                                   errorMessage,
                                   valueMap
                                  ) != Errors.NONE
         )
      {
        display.syncExec(new Runnable()
        {
          @Override
          public void run()
          {
            Dialogs.error(shell,BARControl.tr("Cannot get database indizes with error state (error: {0})",errorMessage[0]));
          }
        });
        return;
      }
      long errorCount = valueMap.getLong("count");

      if (errorCount > 0)
      {
        if (Dialogs.confirm(shell,BARControl.tr("Remove {0} {0,choice,0#indizes|1#index|1<indizes} with error state?",errorCount)))
        {
          final BusyDialog busyDialog = new BusyDialog(shell,"Remove indizes with error",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE);
          busyDialog.setMaximum(errorCount);

          new BackgroundTask(busyDialog)
          {
            @Override
            public void run(final BusyDialog busyDialog, Object userData)
            {
              try
              {
                final String[] errorMessage = new String[1];
                ValueMap       valueMap     = new ValueMap();

                // remove indizes with error state
                Command command = BARServer.runCommand("INDEX_REMOVE state=ERROR storageId=0",0);

                long n = 0;
                while (   !command.endOfData()
                       && !busyDialog.isAborted()
                       && command.getNextResult(errorMessage,
                                                valueMap,
                                                Command.TIMEOUT
                                               ) == Errors.NONE
                      )
                {
                  try
                  {
                    long        storageId = valueMap.getLong  ("storageId");
                    String      name      = valueMap.getString("name"     );

                    busyDialog.updateText(0,"%d: %s",storageId,name);

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
                if (command.getErrorCode() != Errors.NONE)
                {
                  display.syncExec(new Runnable()
                  {
                    @Override
                    public void run()
                    {
                      busyDialog.close();
                      Dialogs.error(shell,BARControl.tr("Cannot remove database indizes with error state (error: {0})",errorMessage[0]));
                    }
                  });

                  updateStorageTreeTableThread.triggerUpdate();

                  return;
                }
                if (busyDialog.isAborted())
                {
                  command.abort();
                }

                // close busy dialog
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    busyDialog.close();
                  }
                });

                updateStorageTreeTableThread.triggerUpdate();
              }
              catch (CommunicationError error)
              {
                final String errorMessage = error.getMessage();
                display.syncExec(new Runnable()
                {
                  @Override
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
        entries += indexData.getEntries();
        size    += indexData.getSize();
      }

      // confirm
      if (Dialogs.confirm(shell,BARControl.tr("Delete {0} {0,choice,0#jobs/entities/storage files|1#job/entity/storage file|1<jobs/entities/storage files} with {1} {1,choice,0#entries|1#entry|1<entries}/{2} {2,choice,0#bytes|1#byte|1<bytes}?",indexDataHashSet.size(),entries,size)))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,"Delete storage indizes and storage files",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
        busyDialog.setMaximum(indexDataHashSet.size());

        new BackgroundTask(busyDialog,new Object[]{indexDataHashSet})
        {
          @Override
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
                busyDialog.updateText(0,"%s",info);

                // delete storage
                int            error        = Errors.UNKNOWN;
                final String[] errorMessage = new String[1];
                if      (indexData instanceof UUIDIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("STORAGE_DELETE jobUUID=%'S",
                                                                       ((UUIDIndexData)indexData).jobUUID
                                                                      ),
                                                   0,  // debugLevel
                                                   errorMessage
                                                  );
                }
                else if (indexData instanceof EntityIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("STORAGE_DELETE entityId=%d",
                                                                       indexData.indexId
                                                                      ),
                                                   0,  // debugLevel
                                                   errorMessage
                                                  );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("STORAGE_DELETE storageId=%d",
                                                                        indexData.indexId
                                                                       ),
                                                    0,  // debugLevel
                                                    errorMessage
                                                   );
                }
                if (error == Errors.NONE)
                {
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
                        @Override
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
                        @Override
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
                @Override
                public void run()
                {
                  busyDialog.close();
                }
              });

              updateStorageTreeTableThread.triggerUpdate();
            }
            catch (CommunicationError error)
            {
              final String errorMessage = error.getMessage();
              display.syncExec(new Runnable()
              {
                @Override
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
   */
  private void restoreArchives(IndexIdSet indexIdSet)
  {
    Label      label;
    Composite  composite,subComposite;
    Button     button;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Restore archives"),400,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Label  widgetCount;
    final Label  widgetSize;
    final Button widgetRestoreTo;
    final Text   widgetRestoreToDirectory;
    final Button widgetOverwriteEntries;
    final Button widgetRestore;
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Archives:"));
      Widgets.layout(label,0,0,TableLayoutData.W);
      widgetCount = Widgets.newLabel(composite,"-");
      Widgets.layout(widgetCount,0,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Total size:"));
      Widgets.layout(label,1,0,TableLayoutData.W);
      widgetSize = Widgets.newLabel(composite,"-");
      Widgets.layout(widgetSize,1,1,TableLayoutData.WE);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0}));
      Widgets.layout(subComposite,2,0,TableLayoutData.WE,0,2);
      {
        widgetRestoreTo = Widgets.newCheckbox(subComposite,BARControl.tr("to"));
        widgetRestoreTo.setToolTipText(BARControl.tr("Enable this checkbox and select a directory to restore entries to different location."));
        Widgets.layout(widgetRestoreTo,0,0,TableLayoutData.W);
        widgetRestoreTo.addSelectionListener(new SelectionListener()
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
            widgetRestoreTo.setSelection(checkedFlag);
            selectRestoreToEvent.trigger();
          }
        });

        widgetRestoreToDirectory = Widgets.newText(subComposite);
        widgetRestoreToDirectory.setEnabled(false);
        Widgets.layout(widgetRestoreToDirectory,0,1,TableLayoutData.WE);
        Widgets.addEventListener(new WidgetEventListener(widgetRestoreToDirectory,selectRestoreToEvent)
        {
          @Override
          public void trigger(Control control)
          {
            control.setEnabled(widgetRestoreTo.getSelection());
          }
        });
        subComposite.addMouseListener(new MouseListener()
        {
          @Override
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
          }
          @Override
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
          @Override
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });

        button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
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
                                      Dialogs.FileDialogTypes.DIRECTORY,
                                      BARControl.tr("Select path"),
                                      widgetRestoreTo.getText(),
                                      BARServer.remoteListDirectory
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
      }

      widgetOverwriteEntries = Widgets.newCheckbox(composite,BARControl.tr("Overwrite existing entries"));
      widgetOverwriteEntries.setToolTipText(BARControl.tr("Enable this checkbox when existing entries in destination should be overwritten."));
      Widgets.layout(widgetOverwriteEntries,3,0,TableLayoutData.W,0,2);
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      widgetRestore = Widgets.newButton(composite,BARControl.tr("Start restore"));
      widgetRestore.setEnabled(false);
      Widgets.layout(widgetRestore,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

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
          Dialogs.close(dialog,null);
        }
      });
    }

    // add selection listeners
/*
    widgetRestore.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        Dialogs.close(dialog,widgetStorageName.getText());
      }
    });
*/

    Dialogs.show(dialog);

    new BackgroundTask((BusyDialog)null,new Object[]{indexIdSet})
    {
      @Override
      public void run(BusyDialog busyDialog, Object userData)
      {
        final IndexIdSet indexIdSet = (IndexIdSet)((Object[])userData)[0];

        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              BARControl.waitCursor(dialog);
            }
          });
        }
        try
        {
          // set entries to restore
          BARServer.executeCommand(StringParser.format("STORAGE_LIST_CLEAR"),0);
  //TODO: optimize send more than one entry?
          for (Long indexId : indexIdSet)
          {
            BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD indexId=%ld",
                                                         indexId
                                                        ),
                                     0  // debugLevel
                                    );
          }

          // get number entries, size
          final String[] errorMessage = new String[1];
          ValueMap       valueMap     = new ValueMap();
          if (BARServer.executeCommand(StringParser.format("STORAGE_LIST_INFO"),
                                       0,  // debugLevel
                                       errorMessage,
                                       valueMap
                                      ) == Errors.NONE
             )
          {
            final long count = valueMap.getLong("count");
            final long size  = valueMap.getLong("size");

            display.syncExec(new Runnable()
            {
              public void run()
              {
                widgetCount.setText(Long.toString(count));
                widgetSize.setText(String.format(BARControl.tr("%s (%d bytes)"),Units.formatByteSize(size),size));
              }
            });
          }

          display.syncExec(new Runnable()
          {
            public void run()
            {
              widgetRestore.setEnabled(!indexIdSet.isEmpty());
            }
          });
        }
        finally
        {
          // reset cursor and foreground color
          display.syncExec(new Runnable()
          {
            public void run()
            {
              BARControl.resetCursor(dialog);
            }
          });
        }
      }
    };

Dprintf.dprintf("");
String directory = "/tmp/x";
boolean overwriteFiles = false;

    // run dialog
    if ((Boolean)Dialogs.run(dialog,false))
    {
      final BusyDialog busyDialog = new BusyDialog(shell,
                                                   !directory.isEmpty() ? BARControl.tr("Restore archives to: {0}",directory) : BARControl.tr("Restore archives"),
                                                   500,
                                                   300,
                                                   null,
                                                   BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.PROGRESS_BAR0|BusyDialog.PROGRESS_BAR1|BusyDialog.LIST
                                                  );
      busyDialog.updateText(2,"%s",BARControl.tr("Failed entries:"));

      new BackgroundTask(busyDialog,new Object[]{directory,overwriteFiles})
      {
        @Override
        public void run(final BusyDialog busyDialog, Object userData)
        {
          final String  directory      = (String )((Object[])userData)[0];
          final boolean overwriteFiles = (Boolean)((Object[])userData)[1];

          int errorCode;

          // restore entries
          try
          {
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  BARControl.waitCursor();
                }
              });
            }
            try
            {
              boolean       retryFlag;
              long          errorCount    = 0;
              final boolean skipAllFlag[] = new boolean[]{false};
              Command       command;
              boolean       passwordFlag  = false;
              do
              {
                retryFlag  = false;
                errorCount = 0;

                // start restore
                command = BARServer.runCommand(StringParser.format("RESTORE destination=%'S overwriteFiles=%y",
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
                       && command.getNextResult(errorMessage,
                                                resultMap,
                                                60*1000
                                               ) == Errors.NONE
                      )
                {
                  // parse and update progresss
                  try
                  {
                    String storageName       = resultMap.getString("storageName"      );
                    long   storageDoneBytes  = resultMap.getLong  ("storageDoneBytes" );
                    long   storageTotalBytes = resultMap.getLong  ("storageTotalBytes");
                    String entryName         = resultMap.getString("entryName"        );
                    long   entryDoneBytes    = resultMap.getLong  ("entryDoneBytes"   );
                    long   entryTotalBytes   = resultMap.getLong  ("entryTotalBytes"  );

                    busyDialog.updateText(0,"%s",storageName);
                    busyDialog.updateProgressBar(0,(storageTotalBytes > 0) ? ((double)storageDoneBytes*100.0)/(double)storageTotalBytes : 0.0);
                    busyDialog.updateText(1,"%s",new File(directory,entryName).getPath());
                    busyDialog.updateProgressBar(1,(entryTotalBytes > 0) ? ((double)entryDoneBytes*100.0)/(double)entryTotalBytes : 0.0);
                  }
                  catch (IllegalArgumentException exception)
                  {
                    if (Settings.debugLevel > 0)
                    {
                      System.err.println("ERROR: "+exception.getMessage());
                    }
                  }

                  // discard waiting results to avoid heap overflow
                  command.purgeResults();
                }

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
                    @Override
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
                                                 0  // debugLevel
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

                  busyDialog.updateList(errorText);
                  errorCount++;

                  if (!skipAllFlag[0])
                  {
                    display.syncExec(new Runnable()
                    {
                      @Override
                      public void run()
                      {
                        switch (Dialogs.select(shell,
                                               BARControl.tr("Confirmation"),
                                               BARControl.tr("Cannot restore archive:\n\n {0}",
                                                             errorText
                                                            ),
                                               new String[]{BARControl.tr("Skip"),BARControl.tr("Skip all"),BARControl.tr("Abort")},
                                               0
                                              )
                               )
                        {
                          case 0:
                            break;
                          case 1:
                            skipAllFlag[0] = true;
                            break;
                          case 2:
                            busyDialog.abort();
                            break;
                        }
                      }
                    });
                  };
                }
              }
              else
              {
                busyDialog.updateText(0,"%s",BARControl.tr("Aborting")+"\u2026");
                command.abort();
              }

              // close/done busy dialog, restore cursor
              if (errorCount == 0)
              {
                busyDialog.close();
              }
              else
              {
                busyDialog.done();
              }
            }
            finally
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  BARControl.resetCursor();
                }
              });
            }
          }
          catch (CommunicationError error)
          {
            final String errorMessage = error.getMessage();
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                busyDialog.close();
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
  }

  //-----------------------------------------------------------------------

  /** set/clear checked all entries
   * @param checked true for set checked, false for clear checked
   */
  private void setAllCheckedEntries(final boolean checked)
  {
    final int MAX_CONFIRM_ENTRIES = 1000;

    final String[] errorMessage = new String[1];
    ValueMap       valueMap     = new ValueMap();

    // confirm check if there are many entries
    final int     count[] = new int[]{0};
    final boolean doit[]  = new boolean[]{true};
    if (checked)
    {
      if (BARServer.executeCommand(StringParser.format("INDEX_ENTRIES_INFO entryPattern=%'S indexType=%s newestEntriesOnly=%y",
                                                       updateEntryTableThread.getEntryPattern(),
                                                       updateEntryTableThread.getEntryType().toString(),
                                                       updateEntryTableThread.getNewestEntriesOnly()
                                                      ),
                                   0,  // debugLevel
                                   errorMessage,
                                   valueMap
                                  ) == Errors.NONE
         )
      {
        count[0] = valueMap.getInt("count");
        if (count[0] > MAX_CONFIRM_ENTRIES)
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              doit[0] = Dialogs.confirm(shell,
                                        Dialogs.booleanFieldUpdater(Settings.class,"showEntriesExceededInfo"),
                                        BARControl.tr("There are {0} entries. Really mark all entries?",
                                                      count[0]
                                                     )
                                       );
            }
          });
        }
      }
    }

    // check/uncheck all entries
    if (checked)
    {
      if (doit[0])
      {
        final int n[] = new int[]{0};
        final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Mark entries"),500,100,null,BusyDialog.PROGRESS_BAR0);
        busyDialog.setMaximum(count[0]);
        int error = BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST entryPattern=%'S indexType=%s newestEntriesOnly=%y",
                                                                 updateEntryTableThread.getEntryPattern(),
                                                                 updateEntryTableThread.getEntryType().toString(),
                                                                 updateEntryTableThread.getNewestEntriesOnly()
                                                                ),
                                             1,
                                             errorMessage,
                                             new CommandResultHandler()
                                             {
                                               public int handleResult(int i, ValueMap valueMap)
                                               {
                                                 long entryId = valueMap.getLong("entryId");

                                                 selectedEntryIdSet.set(entryId,checked);

                                                 n[0]++;
                                                 busyDialog.updateProgressBar(n[0]);

                                                 if (busyDialog.isAborted())
                                                 {
                                                   abort();
                                                 }

                                                 return Errors.NONE;
                                               }
                                             }
                                            );
        busyDialog.close();
        if (error != Errors.NONE)
        {
          Dialogs.error(shell,BARControl.tr("Cannot mark all index entries\n\n(error: {0})",errorMessage[0]));
          return;
        }

        // refresh table
        Widgets.refreshVirtualTable(widgetEntryTable);

        // trigger update checked
        checkedEntryEvent.trigger();
      }
    }
    else
    {
      selectedEntryIdSet.clear();

      // refresh table
      Widgets.refreshVirtualTable(widgetEntryTable);

      // trigger update checked
      checkedEntryEvent.trigger();
    }
  }

  /** get checked entries
   * @return checked data entries
   */
  private EntryData[] getCheckedEntries()
  {
    ArrayList<EntryData> entryDataArray = new ArrayList<EntryData>();

Dprintf.dprintf("");
    for (TableItem tableItem : widgetEntryTable.getItems())
    {
      if (tableItem.getChecked())
      {
        entryDataArray.add((EntryData)tableItem.getData());
      }
    }

    return entryDataArray.toArray(new EntryData[entryDataArray.size()]);
  }

  /** find index for insert of item in sorted entry data list
   * @param entryData data of tree item
   * @return index in table
   */
  private int findEntryListIndex(EntryData entryData)
  {
Dprintf.dprintf("");
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
Dprintf.dprintf("");
    widgetEntryTable.removeAll();
/*
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
*/
    // enable/disable restore button
    checkedEntryEvent.trigger();
  }

  /** restore entries
   * @param entryData entries to restore
   */
  private void restoreEntries(IndexIdSet entryIdSet)
  {
    Label      label;
    Composite  composite,subComposite;
    Button     button;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Restore entries"),400,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Label  widgetCount;
    final Label  widgetSize;
    final Button widgetRestoreTo;
    final Text   widgetRestoreToDirectory;
    final Button widgetOverwriteEntries;
    final Button widgetRestore;
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Entries:"));
      Widgets.layout(label,0,0,TableLayoutData.W);
      widgetCount = Widgets.newLabel(composite,"-");
      Widgets.layout(widgetCount,0,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite,BARControl.tr("Total size:"));
      Widgets.layout(label,1,0,TableLayoutData.W);
      widgetSize = Widgets.newLabel(composite,"-");
      Widgets.layout(widgetSize,1,1,TableLayoutData.WE);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0}));
      Widgets.layout(subComposite,2,0,TableLayoutData.WE,0,2);
      {
        widgetRestoreTo = Widgets.newCheckbox(subComposite,BARControl.tr("to"));
        widgetRestoreTo.setToolTipText(BARControl.tr("Enable this checkbox and select a directory to restore entries to different location."));
        Widgets.layout(widgetRestoreTo,0,0,TableLayoutData.W);
        widgetRestoreTo.addSelectionListener(new SelectionListener()
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
            widgetRestoreTo.setSelection(checkedFlag);
            selectRestoreToEvent.trigger();
          }
        });

        widgetRestoreToDirectory = Widgets.newText(subComposite);
        widgetRestoreToDirectory.setEnabled(false);
        Widgets.layout(widgetRestoreToDirectory,0,1,TableLayoutData.WE);
        Widgets.addEventListener(new WidgetEventListener(widgetRestoreToDirectory,selectRestoreToEvent)
        {
          @Override
          public void trigger(Control control)
          {
            control.setEnabled(widgetRestoreTo.getSelection());
          }
        });
        subComposite.addMouseListener(new MouseListener()
        {
          @Override
          public void mouseDoubleClick(final MouseEvent mouseEvent)
          {
          }
          @Override
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
          @Override
          public void mouseUp(final MouseEvent mouseEvent)
          {
          }
        });

        button = Widgets.newButton(subComposite,IMAGE_DIRECTORY);
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
                                      Dialogs.FileDialogTypes.DIRECTORY,
                                      BARControl.tr("Select path"),
                                      widgetRestoreTo.getText(),
                                      BARServer.remoteListDirectory
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
      }

      widgetOverwriteEntries = Widgets.newCheckbox(composite,BARControl.tr("Overwrite existing entries"));
      widgetOverwriteEntries.setToolTipText(BARControl.tr("Enable this checkbox when existing entries in destination should be overwritten."));
      Widgets.layout(widgetOverwriteEntries,3,0,TableLayoutData.W,0,2);
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      widgetRestore = Widgets.newButton(composite,BARControl.tr("Start restore"));
      widgetRestore.setEnabled(false);
      Widgets.layout(widgetRestore,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

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
          Dialogs.close(dialog,null);
        }
      });
    }

    // add selection listeners
/*
    widgetRestore.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        Dialogs.close(dialog,widgetStorageName.getText());
      }
    });
*/

    Dialogs.show(dialog);

    new BackgroundTask((BusyDialog)null,new Object[]{entryIdSet})
    {
      @Override
      public void run(BusyDialog busyDialog, Object userData)
      {
        IndexIdSet entryIdSet = (IndexIdSet)((Object[])userData)[0];

        // set entries to restore
        BARServer.executeCommand(StringParser.format("ENTRY_LIST_CLEAR"),0);
    //TODO: optimize send more than one entry?
        for (Long entryId : entryIdSet)
        {
          BARServer.executeCommand(StringParser.format("ENTRY_LIST_ADD entryId=%ld",
                                                       entryId
                                                      ),
                                   0  // debugLevel
                                  );
        }

        // get number entries, size
        final String[] errorMessage = new String[1];
        ValueMap       valueMap     = new ValueMap();
        if (BARServer.executeCommand(StringParser.format("ENTRY_LIST_INFO"),
                                     0,  // debugLevel
                                     errorMessage,
                                     valueMap
                                    ) == Errors.NONE
           )
        {
          final long count = valueMap.getLong("count");
          final long size  = valueMap.getLong("size");

          display.syncExec(new Runnable()
          {
            public void run()
            {
              widgetCount.setText(Long.toString(count));
              widgetSize.setText(String.format(BARControl.tr("%s (%d bytes)"),Units.formatByteSize(size),size));
            }
          });
        }

        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetRestore.setEnabled(true);
          }
        });
      }
    };

//TODO
String directory = "/tmp/x";
boolean overwriteFiles = false;
    // run dialog
    if ((Boolean)Dialogs.run(dialog,false))
    {
      final BusyDialog busyDialog = new BusyDialog(shell,
                                                   !directory.isEmpty() ? BARControl.tr("Restore entries to: {0}",directory) : BARControl.tr("Restore entries"),
                                                   500,
                                                   300,
                                                   null,
                                                   BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.PROGRESS_BAR0|BusyDialog.PROGRESS_BAR1|BusyDialog.LIST
                                                  );
      busyDialog.updateText(2,"%s",BARControl.tr("Failed entries:"));

      new BackgroundTask(busyDialog,new Object[]{entryIdSet,directory,overwriteFiles})
      {
        @Override
        public void run(final BusyDialog busyDialog, Object userData)
        {
          final IndexIdSet entryIdSet     = (IndexIdSet)((Object[])userData)[0];
          final String     directory      = (String    )((Object[])userData)[1];
          final boolean    overwriteFiles = (Boolean   )((Object[])userData)[2];

          int errorCode;

          // restore entries
          try
          {
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  BARControl.waitCursor();
                }
              });
            }
            try
            {
              long  errorCount            = 0;
              final boolean skipAllFlag[] = new boolean[]{false};
              int n = 0;
              for (final Long entryId : entryIdSet)
              {
  Dprintf.dprintf("");
  //              busyDialog.updateText(0,"%s",entryData.storageName);
  //              busyDialog.updateProgressBar(0,((double)n*100.0)/(double)entryData_.length);
  Dprintf.dprintf("");
  //              busyDialog.updateText(1,"%s",new File(directory,entryData.name).getPath());

                Command command;
                boolean retryFlag;
                boolean ftpPasswordFlag     = false;
                boolean sshPasswordFlag     = false;
                boolean webdavPasswordFlag  = false;
                boolean decryptPasswordFlag = false;
                do
                {
                  retryFlag = false;

  //TODO: restore with one command: send entry list before
                  // start restore
                  command = BARServer.runCommand(StringParser.format("RESTORE storageName=%'S destination=%'S overwriteFiles=%y name=%'S",
  "xxx",//                                                                   entryData.storageName,
                                                                     directory,
                                                                     overwriteFiles,
  "xxx"//                                                                   entryData.name
                                                                    ),
                                                 0
                                                );

                  // read results, update/add data
                  String[] errorMessage = new String[1];
                  ValueMap valueMap     = new ValueMap();
                  while (   !command.endOfData()
                         && !busyDialog.isAborted()
                         && command.getNextResult(errorMessage,
                                                  valueMap,
                                                  60*1000
                                                 ) == Errors.NONE
                        )
                  {
                    // parse and update progresss
                    try
                    {
                      String storageName       = valueMap.getString("storageName"      );
    //                  long   storageDoneBytes  = valueMap.getLong  ("storageDoneBytes" );
    //                  long   storageTotalBytes = valueMap.getLong  ("storageTotalBytes");
                      String entryName         = valueMap.getString("entryName"        );
                      long   entryDoneBytes    = valueMap.getLong  ("entryDoneBytes"   );
                      long   entryTotalBytes   = valueMap.getLong  ("entryTotalBytes"  );

                      busyDialog.updateText(0,"%s",storageName);
    //                  busyDialog.updateProgressBar(0,(storageTotalBytes > 0) ? ((double)storageDoneBytes*100.0)/(double)storageTotalBytes : 0.0);
                      busyDialog.updateText(1,"%s",new File(directory,entryName).getPath());
                      busyDialog.updateProgressBar(1,(entryTotalBytes > 0) ? ((double)entryDoneBytes*100.0)/(double)entryTotalBytes : 0.0);
                    }
                    catch (IllegalArgumentException exception)
                    {
                      if (Settings.debugLevel > 0)
                      {
                        System.err.println("ERROR: "+exception.getMessage());
                      }
                    }

                    // discard waiting results to avoid heap overflow
                    command.purgeResults();
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
                      @Override
                      public void run()
                      {
                        String password = Dialogs.password(shell,
                                                           BARControl.tr("FTP login password"),
  "xxx",//                                                         BARControl.tr("Please enter FTP login password for: {0}.",entryData.storageName),
                                                           BARControl.tr("Password")+":"
                                                          );
                        if (password != null)
                        {
                          BARServer.executeCommand(StringParser.format("FTP_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                       BARServer.getPasswordEncryptType(),
                                                                       BARServer.encryptPassword(password)
                                                                      ),
                                                   0  // debugLevel
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
                      @Override
                      public void run()
                      {
                        String password = Dialogs.password(shell,
                                                           BARControl.tr("SSH (TLS) login password"),
  "xxx",//                                                         BARControl.tr("Please enter SSH (TLS) login password for: {0}.",entryData.storageName),
                                                           BARControl.tr("Password")+":"
                                                          );
                        if (password != null)
                        {
                          BARServer.executeCommand(StringParser.format("SSH_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                       BARServer.getPasswordEncryptType(),
                                                                       BARServer.encryptPassword(password)
                                                                      ),
                                                   0  // debugLevel
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
                      @Override
                      public void run()
                      {
                        String password = Dialogs.password(shell,
                                                           BARControl.tr("Webdav login password"),
  "xxx",//                                                         BARControl.tr("Please enter Webdav login password for: {0}.",entryData.storageName),
                                                           BARControl.tr("Password")+":"
                                                          );
                        if (password != null)
                        {
                          BARServer.executeCommand(StringParser.format("WEBDAV_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                       BARServer.getPasswordEncryptType(),
                                                                       BARServer.encryptPassword(password)
                                                                      ),
                                                   0  // debugLevel
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
                      @Override
                      public void run()
                      {
                        String password = Dialogs.password(shell,
                                                           BARControl.tr("Decrypt password"),
  "xxx",//                                                         BARControl.tr("Please enter decrypt password for: {0}.",entryData.storageName),
                                                           BARControl.tr("Password")+":"
                                                          );
                        if (password != null)
                        {
                          BARServer.executeCommand(StringParser.format("DECRYPT_PASSWORD_ADD encryptType=%s encryptedPassword=%S",
                                                                       BARServer.getPasswordEncryptType(),
                                                                       BARServer.encryptPassword(password)
                                                                      ),
                                                   0  // debugLevel
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
  Dprintf.dprintf("");
  //                  busyDialog.updateList(entryData.name);
                    errorCount++;

                    if (!skipAllFlag[0])
                    {
                      final String errorText = command.getErrorText();
                      display.syncExec(new Runnable()
                      {
                        @Override
                        public void run()
                        {
  Dprintf.dprintf("");
                          switch (Dialogs.select(shell,
                                                 BARControl.tr("Confirmation"),
                                                 BARControl.tr("Cannot restore entry\n\n''{0}''\n\nfrom archive\n\n''{1}''\n\n(error: {2})",
  "xxx",//                                                             entryData.name,
  "xxx",//                                                             entryData.storageName,
                                                               errorText
                                                              ),
                                                 new String[]{BARControl.tr("Skip"),BARControl.tr("Skip all"),BARControl.tr("Abort")},
                                                 0
                                                )
                                 )
                          {
                            case 0:
                              break;
                            case 1:
                              skipAllFlag[0] = true;
                              break;
                            case 2:
                              busyDialog.abort();
                              break;
                          }
                        }
                      });
                    }
                  }
                }
                else
                {
                  busyDialog.updateText(0,"%s",BARControl.tr("Aborting")+"\u2026");
                  command.abort();
                  break;
                }

                n++;
              }

              // close/done busy dialog, restore cursor
              if (errorCount == 0)
              {
                busyDialog.close();
              }
              else
              {
                busyDialog.done();
              }
            }
            finally
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  BARControl.resetCursor();
                }
              });
            }
          }
          catch (CommunicationError error)
          {
            final String errorMessage = error.getMessage();
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                busyDialog.close();
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
}

/* end of file */
