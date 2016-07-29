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
import org.eclipse.swt.events.MenuEvent;
import org.eclipse.swt.events.MenuListener;
import org.eclipse.swt.events.ModifyEvent;
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
    ANY;
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

    public long                     id;

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
      this.id        = indexId;
      this.treeItem  = null;
      this.tableItem = null;
      this.subMenu   = null;
      this.menuItem  = null;
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

    /** get total number of entries
     * @return entries
     */
    public long getEntryCount()
    {
      return 0;
    }

    /** get total size of entries
     * @return size [bytes]
     */
    public long getEntrySize()
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
      out.writeObject(id);
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
      id = (Long)in.readObject();
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Index {"+id+"}";
    }
  };

  /** index data comparator
   */
  static class IndexDataComparator implements Comparator<IndexData>
  {
    // sort modes
    enum SortModes
    {
      ID,
      NAME,
      SIZE,
      CREATED_DATETIME,
      STATE;
    }

    protected SortModes sortMode;

    /** create index data comparator
     * @param sortMode sort mode
     */
    IndexDataComparator(SortModes sortMode)
    {
      this.sortMode = sortMode;;
    }

    /** create index data comparator
     * @param tree storage tree
     * @param sortColumn sort column
     */
    IndexDataComparator(Tree tree, TreeColumn sortColumn)
    {
      if      (tree.getColumn(0) == sortColumn) this.sortMode = SortModes.NAME;
      else if (tree.getColumn(1) == sortColumn) this.sortMode = SortModes.SIZE;
      else if (tree.getColumn(2) == sortColumn) this.sortMode = SortModes.CREATED_DATETIME;
      else if (tree.getColumn(3) == sortColumn) this.sortMode = SortModes.STATE;
      else                                      this.sortMode = SortModes.NAME;
    }

    /** create index data comparator
     * @param table table
     * @param sortColumn sort column
     */
    IndexDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) this.sortMode = SortModes.NAME;
      else if (table.getColumn(1) == sortColumn) this.sortMode = SortModes.SIZE;
      else if (table.getColumn(2) == sortColumn) this.sortMode = SortModes.CREATED_DATETIME;
      else if (table.getColumn(3) == sortColumn) this.sortMode = SortModes.STATE;
      else                                       this.sortMode = SortModes.NAME;
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
      sortMode = SortModes.ID;
    }

    /** get index data comparator instance
     * @param sortMode sort mode
     * @return index data comparator
     */
    static IndexDataComparator getInstance(SortModes sortMode)
    {
      return new IndexDataComparator(sortMode);
    }

    /** get index data comparator instance
     * @param tree tree widget
     * @return index data comparator
     */
    static IndexDataComparator getInstance(final Tree tree)
    {
      final IndexDataComparator indexDataComparator[] = new IndexDataComparator[1];
      if (!tree.isDisposed())
      {
        tree.getDisplay().syncExec(new Runnable()
        {
          public void run()
          {
            indexDataComparator[0] = new IndexDataComparator(tree);
          }
        });
      }

      return indexDataComparator[0];
    }

    /** get index data comparator instance
     * @param table table widget
     * @return index data comparator
     */
    static IndexDataComparator getInstance(final Table table)
    {
      final IndexDataComparator indexDataComparator[] = new IndexDataComparator[1];
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          indexDataComparator[0] = new IndexDataComparator(table);
        }
      });

      return indexDataComparator[0];
    }

    /** get index data comparator instance
     * @param tree tree widget
     * @return index data comparator
     */
    static IndexDataComparator getInstance()
    {
      return new IndexDataComparator();
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
      SortModes nextSortMode = sortMode;
      int       result;

      if ((indexData1 == null) && (indexData2 == null))
      {
        return 0;
      }
      else if (indexData1 == null)
      {
        return 1;
      }
      else if (indexData2 == null)
      {
        return -1;
      }
      else
      {
        result = 0;
        do
        {
          switch (nextSortMode)
          {
            case ID:
              return new Long(indexData1.id).compareTo(indexData2.id);
            case NAME:
              String name1 = indexData1.getName();
              String name2 = indexData2.getName();
              result = name1.compareTo(name2);
              nextSortMode = SortModes.SIZE;
              break;
            case SIZE:
              long size1 = indexData1.getEntrySize();
              long size2 = indexData2.getEntrySize();
              result = new Long(size1).compareTo(size2);
              nextSortMode = SortModes.CREATED_DATETIME;
              break;
            case CREATED_DATETIME:
              long date1 = indexData1.getDateTime();
              long date2 = indexData2.getDateTime();
              result = new Long(date1).compareTo(date2);
              nextSortMode = SortModes.STATE;
              break;
            case STATE:
              IndexStates indexState1 = indexData1.getState();
              IndexStates indexState2 = indexData2.getState();
              result = indexState1.compareTo(indexState2);
              return result;
            default:
              return result;
          }
        }
        while (result == 0);
      }

      return result;
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

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      StringBuilder buffer = new StringBuilder();
      for (long indexId : this)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(indexId);
      }

      return "IndexIdSet {"+buffer.toString()+"}";
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
    public long   totalEntryCount;
    public long   totalEntrySize;

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)uuidIndexData,
                               uuidIndexData.name,
                               Units.formatByteSize(uuidIndexData.totalEntrySize),
                               (uuidIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(uuidIndexData.lastCreatedDateTime*1000L)) : "-",
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
     * @param totalEntryCount total number of entries of storage
     * @param totalEntrySize total size of storage [byte]
     */
    UUIDIndexData(long   indexId,
                  String jobUUID,
                  String name,
                  long   lastCreatedDateTime,
                  String lastErrorMessage,
                  long   totalEntryCount,
                  long   totalEntrySize
                 )
    {
      super(indexId);
      this.jobUUID             = jobUUID;
      this.name                = name;
      this.lastCreatedDateTime = lastCreatedDateTime;
      this.lastErrorMessage    = lastErrorMessage;
      this.totalEntryCount     = totalEntryCount;
      this.totalEntrySize      = totalEntrySize;
    }

    /** get name
     * @return name
     */
    @Override
    public String getName()
    {
      return name;
    }

    /** get date/time
     * @return date/time [s]
     */
    @Override
    public long getDateTime()
    {
      return lastCreatedDateTime;
    }

    /** get total number of entries
     * @return entries
     */
    @Override
    public long getEntryCount()
    {
      return totalEntryCount;
    }

    /** get total size of entries
     * @return size [bytes]
     */
    @Override
    public long getEntrySize()
    {
      return totalEntrySize;
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
      return String.format("#%d: %s",id,!name.isEmpty() ? name : "unknown");
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "UUIDIndexData {"+id+", jobUUID="+jobUUID+", name="+name+", lastCreatedDateTime="+lastCreatedDateTime+", totalEntryCount="+totalEntryCount+", totalEntrySize="+totalEntrySize+" bytes}";
    }
  }

  /** entity index data
   */
  class EntityIndexData extends IndexData implements Serializable
  {
    public String                jobUUID;
    public String                scheduleUUID;
    public Settings.ArchiveTypes archiveType;
    public long                  lastCreatedDateTime;  // last date/time when some storage was created
    public String                lastErrorMessage;     // last error message
    public long                  totalEntryCount;
    public long                  totalEntrySize;

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        EntityIndexData entityIndexData = (EntityIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)entityIndexData,
                               entityIndexData.archiveType.toString(),
                               Units.formatByteSize(entityIndexData.totalEntrySize),
                               (entityIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-",
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
     * @param totalEntryCount total number of entresi of storage
     * @param totalEntrySize total size of storage [byte]
     */
    EntityIndexData(long                  entityId,
                    String                jobUUID,
                    String                scheduleUUID,
                    Settings.ArchiveTypes archiveType,
                    long                  lastCreatedDateTime,
                    String                lastErrorMessage,
                    long                  totalEntryCount,
                    long                  totalEntrySize
                   )
    {
      super(entityId);
      this.jobUUID             = jobUUID;
      this.scheduleUUID        = scheduleUUID;
      this.archiveType         = archiveType;
      this.lastCreatedDateTime = lastCreatedDateTime;
      this.lastErrorMessage    = lastErrorMessage;
      this.totalEntryCount     = totalEntryCount;
      this.totalEntrySize      = totalEntrySize;
    }

    /** get name
     * @return name
     */
    @Override
    public String getName()
    {
      return archiveType.toString();
    }

    /** get date/time
     * @return date/time [s]
     */
    @Override
    public long getDateTime()
    {
      return lastCreatedDateTime;
    }

    /** get total number of entries
     * @return entries
     */
    @Override
    public long getEntryCount()
    {
      return totalEntryCount;
    }

    /** get total size of entries
     * @return size [bytes]
     */
    @Override
    public long getEntrySize()
    {
      return totalEntrySize;
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
      return String.format("#%d: %s",id,archiveType.toString());
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
      out.writeObject(jobUUID);
      out.writeObject(scheduleUUID);
      out.writeObject(archiveType);
      out.writeObject(lastCreatedDateTime);
      out.writeObject(lastErrorMessage);
      out.writeObject(totalEntryCount);
      out.writeObject(totalEntrySize);
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
      jobUUID             = (String)in.readObject();
      scheduleUUID        = (String)in.readObject();
      archiveType         = (Settings.ArchiveTypes)in.readObject();
      lastCreatedDateTime = (Long)in.readObject();
      lastErrorMessage    = (String)in.readObject();
      totalEntryCount     = (Long)in.readObject();
      totalEntrySize      = (Long)in.readObject();
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "EntityIndexData {"+id+", type="+archiveType.toString()+", lastCreatedDateTime="+lastCreatedDateTime+", totalEntrySize="+totalEntrySize+" bytes}";
    }
  }

  /** storage index data
   */
  class StorageIndexData extends IndexData implements Serializable
  {
    public  String                jobName;                  // job name or null
    public  Settings.ArchiveTypes archiveType;              // archive type
    public  String                name;                     // name
    public  long                  lastCreatedDateTime;      // last date/time when some storage was created
    public  IndexStates           indexState;               // state of index
    public  IndexModes            indexMode;                // mode of index
    public  long                  lastCheckedDateTime;      // last checked date/time
    public  String                errorMessage;             // last error message

    private long                  totalEntryCount;
    private long                  totalEntrySize;

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      public void update(TreeItem treeItem, IndexData indexData)
      {
        StorageIndexData storageIndexData = (StorageIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)storageIndexData,
                               storageIndexData.name,
                               Units.formatByteSize(storageIndexData.totalEntrySize),
                               (storageIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.lastCreatedDateTime*1000L)) : "-",
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
                                 Units.formatByteSize(storageIndexData.totalEntrySize),
                                 SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.lastCreatedDateTime*1000L)),
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
     * @param totalEntryCount number of entries
     * @param totalEntrySize size of storage [byte]
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
                     long                  totalEntryCount,
                     long                  totalEntrySize,
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
      this.totalEntryCount     = totalEntryCount;
      this.totalEntrySize      = totalEntrySize;
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
    @Override
    public String getName()
    {
      return name;
    }

    /** get date/time
     * @return date/time [s]
     */
    @Override
    public long getDateTime()
    {
      return lastCreatedDateTime;
    }

    /** get total number of entries
     * @return entries
     */
    @Override
    public long getEntryCount()
    {
      return totalEntryCount;
    }

    /** get togal size of entries
     * @return size [bytes]
     */
    @Override
    public long getEntrySize()
    {
      return totalEntrySize;
    }

    /** get index state
     * @return index state
     */
    @Override
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
                              Units.formatByteSize(totalEntrySize),
                              SIMPLE_DATE_FORMAT.format(new Date(lastCreatedDateTime*1000L)),
                              indexState.toString()
                             );

    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return String.format("#%d: %s, %s",id,!jobName.isEmpty() ? jobName : "unknown",name);
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
      out.writeObject(totalEntryCount);
      out.writeObject(totalEntrySize);
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
      totalEntryCount     = (Long)in.readObject();
      totalEntrySize      = (Long)in.readObject();
      indexState          = (IndexStates)in.readObject();
      indexMode           = (IndexModes)in.readObject();
      lastCheckedDateTime = (Long)in.readObject();
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "StorageIndexData {"+id+", name="+name+", lastCreatedDateTime="+lastCreatedDateTime+", totalEntrySize="+totalEntrySize+" bytes, state="+indexState+", last checked="+lastCheckedDateTime+"}";
    }
  };

  /** restore types
   */
  enum RestoreTypes
  {
    ARCHIVES,
    ENTRIES;
  };

  /** find index for insert of item in sorted storage item list
   * @param indexData index data
   * @param indexDataComparator index data comparator
   * @return index in tree
   */
  private int findStorageTreeIndex(IndexData indexData, IndexDataComparator indexDataComparator)
  {
    TreeItem treeItems[] = widgetStorageTree.getItems();

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

//TODO
/*
//Dprintf.dprintf("---------- %d",treeItems.length);
    int index = 0;

    if (treeItems.length > 0)
    {
      int i      = 0;
      int i0     = 0;
      int i1     = treeItems.length-1;
      int result = -1;
      while (i0 < i1)
      {
        i = (i1+i0)/2;
//  Dprintf.dprintf("i=%d: %s <-> %s",i,indexData,(IndexData)treeItems[i].getData());
        result = indexDataComparator.compare(indexData,(IndexData)treeItems[i].getData());
        if      (result < 0) i1 = i-1;     // before i
        else if (result > 0) i0 = i+1;     // after i
        else                 i0 = i1 = i;  // exactly found at i
      }
//Dprintf.dprintf("%d: %d %d %d",result,i,i0,i1);
      if (indexDataComparator.compare(indexData,(IndexData)treeItems[i0].getData()) < 0)
      {
        index = i0;
      }
      else
      {
        index = i0+1;
      }
    }

//Dprintf.dprintf("at=%d",index);
    return index;
*/
  }

  /** find index for insert of item in sorted storage item list
   * @param indexData index data
   * @return index in tree
   */
  private int findStorageTreeIndex(IndexData indexData)
  {
    return findStorageTreeIndex(indexData,new IndexDataComparator(widgetStorageTree));
  }

  /** find index for insert of item in sorted index item tree
   * @param treeItem tree item
   * @param indexData index data
   * @param indexDataComparator index data comparator
   * @return index in tree
   */
  private int findStorageTreeIndex(TreeItem treeItem, IndexData indexData, IndexDataComparator indexDataComparator)
  {
    TreeItem subTreeItems[] = treeItem.getItems();

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
/*
    int index = 0;
    while (   (index < subTreeItems.length)
           && (indexDataComparator.compare(indexData,(IndexData)subTreeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;*/

    int index = 0;

    if (subTreeItems.length > 0)
    {
      int i      = 0;
      int i0     = 0;
      int i1     = subTreeItems.length-1;
      int result = -1;
      while (i0 < i1)
      {
        i = (i1+i0)/2;
        result = indexDataComparator.compare(indexData,(IndexData)subTreeItems[i].getData());
        if      (result < 0) i1 = i-1;     // before i
        else if (result > 0) i0 = i+1;     // after i
        else                 i0 = i1 = i;  // exactly found at i
      }
      if (indexDataComparator.compare(indexData,(IndexData)subTreeItems[i0].getData()) < 0)
      {
        index = i0;
      }
      else
      {
        index = i0+1;
      }
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
    return findStorageTreeIndex(treeItem,indexData,new IndexDataComparator(widgetStorageTree));
  }

  /** find index for insert of item in sorted storage menu
   * @param uuidIndexData UUID index data
   * @return index in menu
   */
  private int findStorageMenuIndex(UUIDIndexData uuidIndexData)
  {
    MenuItem            menuItems[]         = widgetStorageTreeAssignToMenu.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree);

//TODO: binary search
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

//TODO: binary search
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
   * @param storageIndexData storage index data
   * @param indexDataComparator index data comparator
   * @return index in table
   */
  private int findStorageTableIndex(StorageIndexData storageIndexData, IndexDataComparator indexDataComparator)
  {
    TableItem tableItems[] = widgetStorageTable.getItems();

/*
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

    return tableItems.length;*/

    int index = 0;
Dprintf.dprintf("cirrect?");

    if (tableItems.length > 0)
    {
      int i      = 0;
      int i0     = 0;
      int i1     = tableItems.length-1;
      int result = -1;
      while (i0 < i1)
      {
        i = (i1+i0)/2;
        result = indexDataComparator.compare(storageIndexData,(StorageIndexData)tableItems[i].getData());
        if      (result < 0) i1 = i-1;     // before i
        else if (result > 0) i0 = i+1;     // after i
        else                 i0 = i1 = i;  // exactly found at i
      }
      if (indexDataComparator.compare(storageIndexData,(StorageIndexData)tableItems[i0].getData()) < 0)
      {
        index = i0;
      }
      else
      {
        index = i0+1;
      }
    }

    return index;
  }

  /** find index for insert of item in sorted storage data table
   * @param storageIndexData storage index data
   * @return index in table
   */
  private int findStorageTableIndex(StorageIndexData storageIndexData)
  {
    return findStorageTableIndex(storageIndexData,new IndexDataComparator(widgetStorageTable));
  }

  /** update storage tree/table thread
   */
  class UpdateStorageTreeTableThread extends Thread
  {
    private final int PAGE_SIZE = 32;

    private Object           trigger                   = new Object();   // trigger update object
    private boolean          requestUpdateStorageCount = false;
    private HashSet<Integer> requestUpdateOffsets      = new HashSet<Integer>();
    private int              storageCount              = 0;
    private String           storageName               = "";
    private IndexStateSet    storageIndexStateSet      = INDEX_STATE_SET_ALL;
    private EntityStates     storageEntityState        = EntityStates.ANY;
    private boolean          requestSetUpdateIndicator = false;          // true to set color/cursor at update

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
      boolean          updateStorageCount = true;
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
              // update count
              if (!this.requestUpdateStorageCount && updateStorageCount)
              {
                updateStorageTableCount();
              }

              // update tree
              HashSet<TreeItem> uuidTreeItems = new HashSet<TreeItem>();
              if (!this.requestUpdateStorageCount)
              {
                updateUUIDTreeItems(uuidTreeItems);
              }
              HashSet<TreeItem> entityTreeItems = new HashSet<TreeItem>();
              if (!this.requestUpdateStorageCount)
              {
                updateEntityTreeItems(uuidTreeItems,entityTreeItems);
              }
              if (!this.requestUpdateStorageCount)
              {
                updateStorageTreeItems(entityTreeItems);
              }

              // update table
              if (!this.requestUpdateStorageCount && !updateOffsets.isEmpty())
              {
                updateStorageTable(updateOffsets);
              }
            }
            catch (CommunicationError error)
            {
              // ignored
            }
            catch (ConnectionError error)
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
            catch (ConnectionError error)
            {
              // ignored
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
            if (!this.requestUpdateStorageCount && this.requestUpdateOffsets.isEmpty())
            {
              // wait for refresh request trigger or timeout
              try { trigger.wait(30*1000); } catch (InterruptedException exception) { /* ignored */ };
            }

            // get update count, offsets to update
            updateStorageCount = this.requestUpdateStorageCount;
            updateOffsets.addAll(this.requestUpdateOffsets);
            setUpdateIndicator = this.requestSetUpdateIndicator;

            // if not triggered (timeout occurred) update count is done invisible (color is not set)
            if (!this.requestUpdateStorageCount && this.requestUpdateOffsets.isEmpty())
            {
              updateStorageCount = true;
              setUpdateIndicator = false;
            }

            // wait for immediate further triggers
            do
            {
              this.requestUpdateStorageCount = false;
              this.requestUpdateOffsets.clear();
              this.requestSetUpdateIndicator = false;

              try { trigger.wait(500); } catch (InterruptedException exception) { /* ignored */ };
              updateOffsets.addAll(this.requestUpdateOffsets);
            }
            while (this.requestUpdateStorageCount || !this.requestUpdateOffsets.isEmpty());
          }
        }
      }
      catch (Throwable throwable)
      {
        if (Settings.debugLevel > 0)
        {
          BARServer.disconnect();
          System.err.println("ERROR: "+throwable.getMessage());
          BARControl.printStackTrace(throwable);
          System.exit(1);
        }
      }
    }

    /** get storage count
     * @return storage count
     */
    public int getStorageCount()
    {
      return storageCount;
    }

    /** get storage name
     * @return storage name
     */
    public String getStorageName()
    {
      return storageName;
    }

    /** get storage index state set
     * @return storage index state set
     */
    public IndexStateSet getStorageIndexStateSet()
    {
      return storageIndexStateSet;
    }

    /** trigger update of storage list
     * @param storageName new storage name
     * @param storageIndexStateSet new storage index state set
     * @param storageEntityState new storage entity state
     */
    public void triggerUpdate(String storageName, IndexStateSet storageIndexStateSet, EntityStates storageEntityState)
    {
      assert storageName != null;

      synchronized(trigger)
      {
        if (   !this.storageName.equals(storageName)
            || (this.storageIndexStateSet != storageIndexStateSet) || (this.storageEntityState != storageEntityState)
           )
        {
          this.storageName               = storageName;
          this.storageIndexStateSet      = storageIndexStateSet;
          this.storageEntityState        = storageEntityState;
          this.requestUpdateStorageCount = true;
          this.requestSetUpdateIndicator = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of storage list
     * @param storageName new storage name
     */
    public void triggerUpdateStorageName(String storageName)
    {
      assert storageName != null;

      synchronized(trigger)
      {
//        if (!this.storageName.equals(storageName))
        if (   (this.storageName == null)
            || (storageName == null)
//Note: at least 3 characters?
            || (((storageName.length() == 0) || (storageName.length() >= 3)) && !this.storageName.equals(storageName))
           )
        {
          this.storageName               = storageName;
          this.requestUpdateStorageCount = true;
          this.requestSetUpdateIndicator = true;
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
          this.storageIndexStateSet      = storageIndexStateSet;
          this.storageEntityState        = storageEntityState;
          this.requestUpdateStorageCount = true;
          this.requestSetUpdateIndicator = true;
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
        if (!this.requestUpdateOffsets.contains(offset))
        {
          this.requestUpdateOffsets.add(offset);
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
        this.requestUpdateStorageCount = true;
        this.requestSetUpdateIndicator = true;
        trigger.notify();
      }
    }

    /** check if update triggered
     * @return true iff update triggered
     */
    private boolean isUpdateTriggered()
    {
      return requestUpdateStorageCount || !requestUpdateOffsets.isEmpty();
    }

    /** update UUID tree items
     */
    private void updateUUIDTreeItems(final HashSet<TreeItem> uuidTreeItems)
    {
      uuidTreeItems.clear();

      // get UUID items
      final HashSet<TreeItem> removeUUIDTreeItemSet = new HashSet<TreeItem>();
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
      if (isUpdateTriggered()) return;

      // get UUID list
      final ArrayList<UUIDIndexData> uuidIndexDataList = new ArrayList<UUIDIndexData>();
      BARServer.executeCommand(StringParser.format("INDEX_UUID_LIST name=%'S",
                                                   storageName
                                                  ),
                               1,  // debugLevel
                               new CommandResultHandler()
                               {
                                 public int handleResult(int i, ValueMap valueMap)
                                 {
                                   try
                                   {
                                     long   uuidId              = valueMap.getLong  ("uuidId"             );
                                     String jobUUID             = valueMap.getString("jobUUID"            );
                                     String name                = valueMap.getString("name"               );
                                     long   lastCreatedDateTime = valueMap.getLong  ("lastCreatedDateTime");
                                     String lastErrorMessage    = valueMap.getString("lastErrorMessage"   );
                                     long   totalEntryCount     = valueMap.getLong  ("totalEntryCount"    );
                                     long   totalEntrySize      = valueMap.getLong  ("totalEntrySize"     );

                                     uuidIndexDataList.add(new UUIDIndexData(uuidId,
                                                                             jobUUID,
                                                                             name,
                                                                             lastCreatedDateTime,
                                                                             lastErrorMessage,
                                                                             totalEntryCount,
                                                                             totalEntrySize
                                                                            )
                                                          );
                                   }
                                   catch (IllegalArgumentException exception)
                                   {
                                     if (Settings.debugLevel > 0)
                                     {
                                       System.err.println("ERROR: "+exception.getMessage());
                                       System.exit(1);
                                     }
                                   }

                                   // check if aborted
                                   if (isUpdateTriggered())
                                   {
                                     abort();
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );
      if (isUpdateTriggered()) return;

      // get comperator
      final IndexDataComparator indexDataComparator = IndexDataComparator.getInstance(widgetStorageTree);

      // get pre-sorted array with index data
      final UUIDIndexData uuidIndexDataArray[] = uuidIndexDataList.toArray(new UUIDIndexData[uuidIndexDataList.size()]);
      Arrays.sort(uuidIndexDataArray,indexDataComparator);

      // update UUID tree
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(false);
          }
        });
      }
      try
      {
        // add/update tree items
        display.syncExec(new Runnable()
        {
          public void run()
          {
//TODO: remove if? always insert/update?
            if (true ||widgetStorageTree.getItemCount() > 0)
            {
              for (final UUIDIndexData uuidIndexData : uuidIndexDataArray)
              {
                TreeItem uuidTreeItem = Widgets.getTreeItem(widgetStorageTree,indexIdComperator,uuidIndexData);
                if (uuidTreeItem == null)
                {
                  // insert tree item
                  uuidTreeItem = Widgets.insertTreeItem(widgetStorageTree,
                                                        findStorageTreeIndex(uuidIndexData,indexDataComparator),
                                                        (Object)uuidIndexData,
                                                        true,  // folderFlag
                                                        uuidIndexData.name,
                                                        Units.formatByteSize(uuidIndexData.totalEntrySize),
                                                        (uuidIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(uuidIndexData.lastCreatedDateTime*1000L)) : "-",
                                                        ""
                                                       );
//TODO
//uuidIndexData.setTreeItem(uuidTreeItem);
                  uuidTreeItem.setChecked(selectedIndexIdSet.contains(uuidIndexData.id));
                }
                else
                {
                  // update tree item
                  assert uuidTreeItem.getData() instanceof UUIDIndexData;
                  Widgets.updateTreeItem(uuidTreeItem,
                                         (Object)uuidIndexData,
                                         uuidIndexData.name,
                                         Units.formatByteSize(uuidIndexData.totalEntrySize),
                                         (uuidIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(uuidIndexData.lastCreatedDateTime*1000L)) : "-",
                                         ""
                                        );
                  removeUUIDTreeItemSet.remove(uuidTreeItem);
                }
                if (uuidTreeItem.getExpanded())
                {
                  uuidTreeItems.add(uuidTreeItem);
                }
              }
            }
            else
            {
              for (final UUIDIndexData uuidIndexData : uuidIndexDataArray)
              {
                // add tree item
                TreeItem uuidTreeItem = Widgets.addTreeItem(widgetStorageTree,
                                                            (Object)uuidIndexData,
                                                            true,  // folderFlag
                                                            uuidIndexData.name,
                                                            Units.formatByteSize(uuidIndexData.totalEntrySize),
                                                            (uuidIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(uuidIndexData.lastCreatedDateTime*1000L)) : "-",
                                                            ""
                                                           );
//TODO
//uuidIndexData.setTreeItem(uuidTreeItem);
                uuidTreeItem.setChecked(selectedIndexIdSet.contains(uuidIndexData.id));
                if (uuidTreeItem.getExpanded())
                {
                  uuidTreeItems.add(uuidTreeItem);
                }
              }
            }
          }
        });
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
     * @param entityTreeItems updated job tree items or null
     */
    private void updateEntityTreeItems(final TreeItem uuidTreeItem, final HashSet<TreeItem> entityTreeItems)
    {
      // get UUID index data, entity items
      final HashSet<TreeItem> removeEntityTreeItemSet = new HashSet<TreeItem>();
      final UUIDIndexData     uuidIndexData[]         = new UUIDIndexData[]{null};
      display.syncExec(new Runnable()
      {
        public void run()
        {
          if (!uuidTreeItem.isDisposed())
          {
            assert uuidTreeItem.getData() instanceof UUIDIndexData;

            uuidIndexData[0] = (UUIDIndexData)uuidTreeItem.getData();

            for (TreeItem treeItem : uuidTreeItem.getItems())
            {
              assert treeItem.getData() instanceof EntityIndexData;
              removeEntityTreeItemSet.add(treeItem);
            }
          }
        }
      });
      if (isUpdateTriggered()) return;

      // get entity list
      final ArrayList<EntityIndexData> entityIndexDataList = new ArrayList<EntityIndexData>();
      BARServer.executeCommand(StringParser.format("INDEX_ENTITY_LIST uuidId=%lld pattern=%'S",
                                                   uuidIndexData[0].id,
                                                   storageName
                                                  ),
                               1,  // debug level
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
                                     long                  totalEntryCount     = valueMap.getLong  ("totalEntryCount"                        );
                                     long                  totalEntrySize      = valueMap.getLong  ("totalEntrySize"                         );

                                     // add entity data index
                                     entityIndexDataList.add(new EntityIndexData(entityId,
                                                                                 jobUUID,
                                                                                 scheduleUUID,
                                                                                 archiveType,
                                                                                 lastCreatedDateTime,
                                                                                 lastErrorMessage,
                                                                                 totalEntryCount,
                                                                                 totalEntrySize
                                                                                )
                                                            );
                                   }
                                   catch (IllegalArgumentException exception)
                                   {
                                     if (Settings.debugLevel > 0)
                                     {
                                       System.err.println("ERROR: "+exception.getMessage());
                                       System.exit(1);
                                     }
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );
      if (isUpdateTriggered()) return;

      // get comperator
      final IndexDataComparator indexDataComparator = IndexDataComparator.getInstance(IndexDataComparator.SortModes.CREATED_DATETIME);

      // get pre-sorted array with index data
      final EntityIndexData entityIndexDataArray[] = entityIndexDataList.toArray(new EntityIndexData[entityIndexDataList.size()]);
      Arrays.sort(entityIndexDataArray,indexDataComparator);

      // update entity tree
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(false);
          }
        });
      }
      try
      {
        for (final EntityIndexData entityIndexData : entityIndexDataList)
        {
          assert entityIndexData != null;

          display.syncExec(new Runnable()
          {
            public void run()
            {
              TreeItem entityTreeItem = Widgets.getTreeItem(uuidTreeItem,indexIdComperator,entityIndexData);
              if (entityTreeItem == null)
              {
                // insert tree item
//Dprintf.dprintf("entityIndexData=%s %d",entityIndexData,findStorageTreeIndex(uuidTreeItem,entityIndexData,indexDataComparator));
                entityTreeItem = Widgets.insertTreeItem(uuidTreeItem,
                                                        findStorageTreeIndex(uuidTreeItem,entityIndexData,indexDataComparator),
                                                        (Object)entityIndexData,
                                                        true,
                                                        entityIndexData.archiveType.toString(),
                                                        Units.formatByteSize(entityIndexData.totalEntrySize),
                                                        (entityIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-",
                                                        ""
                                                       );
//TODO
//entityIndexData.setTreeItem(entityTreeItem);
                entityTreeItem.setChecked(selectedIndexIdSet.contains(entityIndexData.id));
              }
              else
              {
                // update tree item
                assert entityTreeItem.getData() instanceof EntityIndexData;
                Widgets.updateTreeItem(entityTreeItem,
                                       (Object)entityIndexData,
                                       entityIndexData.archiveType.toString(),
                                       Units.formatByteSize(entityIndexData.totalEntrySize),
                                       (entityIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-",
                                       ""
                                      );
                removeEntityTreeItemSet.remove(entityTreeItem);
              }
              if ((entityTreeItems != null) && entityTreeItem.getExpanded())
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
                setStorageList(indexData.id,false);
//TODO: remove?
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
     * @param uuidTreeItem UUID tree item to update
     */
    private void updateEntityTreeItems(TreeItem uuidTreeItem)
    {
      updateEntityTreeItems(uuidTreeItem,(HashSet<TreeItem>)null);
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
      // get entity index data, storage items
      final HashSet<TreeItem> removeStorageTreeItemSet = new HashSet<TreeItem>();
      final EntityIndexData   entityIndexData[]        = new EntityIndexData[1];
      display.syncExec(new Runnable()
      {
        public void run()
        {
          if (!entityTreeItem.isDisposed())
          {
            assert entityTreeItem.getData() instanceof EntityIndexData;

            entityIndexData[0] = (EntityIndexData)entityTreeItem.getData();

            for (TreeItem treeItem : entityTreeItem.getItems())
            {
              assert treeItem.getData() instanceof StorageIndexData;
              removeStorageTreeItemSet.add(treeItem);
            }
          }
        }
      });
      if (isUpdateTriggered()) return;

      // get storage list for entity
      final ArrayList<StorageIndexData> storageIndexDataList = new ArrayList<StorageIndexData>();
      BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%ld indexStateSet=%s indexModeSet=%s name=%'S",
                                                   entityIndexData[0].id,
                                                   storageIndexStateSet.nameList("|"),
                                                   "*",
                                                   storageName
                                                  ),
                               1,  // debugLevel
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
                                     long                  totalEntryCount     = valueMap.getLong  ("totalEntryCount"                        );
                                     long                  totalEntrySize      = valueMap.getLong  ("totalEntrySize"                         );
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
                                                                                   totalEntryCount,
                                                                                   totalEntrySize,
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
                                       System.exit(1);
                                     }
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );

      // update storage tree items
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTree.setRedraw(false);
          }
        });
      }
      try
      {
        for (final StorageIndexData storageIndexData : storageIndexDataList)
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              if (!entityTreeItem.isDisposed())
              {
                TreeItem storageTreeItem = Widgets.getTreeItem(entityTreeItem,indexIdComperator,storageIndexData);
                if (storageTreeItem == null)
                {
                  // insert tree item
                  storageTreeItem = Widgets.insertTreeItem(entityTreeItem,
                                                           findStorageTreeIndex(entityTreeItem,storageIndexData),
                                                           (Object)storageIndexData,
                                                           false,
                                                           storageIndexData.name,
                                                           Units.formatByteSize(storageIndexData.totalEntrySize),
                                                           (storageIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.lastCreatedDateTime*1000L)) : "-",
                                                           storageIndexData.indexState.toString()
                                                          );
                  storageTreeItem.setChecked(selectedIndexIdSet.contains(storageIndexData.id));
                }
                else
                {
                  // update tree item
                  assert storageTreeItem.getData() instanceof StorageIndexData;
                  Widgets.updateTreeItem(storageTreeItem,
                                         (Object)storageIndexData,
                                         storageIndexData.name,
                                         Units.formatByteSize(storageIndexData.totalEntrySize),
                                         (storageIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.lastCreatedDateTime*1000L)) : "-",
                                         storageIndexData.indexState.toString()
                                        );
                  removeStorageTreeItemSet.remove(storageTreeItem);
                }
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
              setStorageList(indexData.id,false);
//TODO: remove?
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
        updateStorageTreeTableThread.updateEntityTreeItems(treeItem);
      }
      else if (treeItem.getData() instanceof EntityIndexData)
      {
        updateStorageTreeTableThread.updateStorageTreeItems(treeItem);
      }
    }

    /** refresh storage table display count
     */
    private void updateStorageTableCount()
    {
      assert storageName != null;

      // set update inidicator
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetStorageTabFolderTitle.setForeground(COLOR_MODIFIED);
          widgetStorageTabFolderTitle.redraw();
        }
      });

      // get storages info
      final String[] errorMessage = new String[1];
      ValueMap       valueMap     = new ValueMap();
      if (BARServer.executeCommand(StringParser.format("INDEX_STORAGES_INFO entityId=%s indexStateSet=%s indexModeSet=* name=%'S",
                                                       (storageEntityState != EntityStates.NONE) ? "*" : "NONE",
                                                       storageIndexStateSet.nameList("|"),
                                                       storageName
                                                      ),
                                   1,  // debugLevel
                                   errorMessage,
                                   valueMap
                                  ) == Errors.NONE
         )
      {
        storageCount = valueMap.getInt("storageCount");
      }
//TODO
      if (storageCount < 0)
      {
        if (Settings.debugLevel > 0)
        {
          System.err.println("Warning: storageCount %d"+storageCount);
        }
        storageCount = 0;
      }

      // set count
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetStorageTabFolderTitle.setForeground(null);
          widgetStorageTabFolderTitle.redraw();

          widgetStorageTable.setRedraw(false);

          widgetStorageTable.setItemCount(0);
          widgetStorageTable.clearAll();

          widgetStorageTable.setItemCount(storageCount);
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
      assert storageName != null;
      assert offset >= 0;
      assert storageCount >= 0;

      // get limit
      final int limit = ((offset+PAGE_SIZE) < storageCount) ? PAGE_SIZE : storageCount-offset;

//TODO: sort
Dprintf.dprintf("/TODO: updateStorageTable sort");
//IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTable,tableColumn);

      // update storage table segment
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTable.setRedraw(false);
          }
        });
      }
      final String[] errorMessage = new String[1];
      final int[]    n            = new int[1];
      try
      {
        BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s indexStateSet=%s indexModeSet=* name=%'S offset=%d limit=%d",
                                                     (storageEntityState != EntityStates.NONE) ? "*" : "NONE",
                                                     storageIndexStateSet.nameList("|"),
                                                     storageName,
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
                                       final long                  storageId           = valueMap.getLong  ("storageId");
                                       final String                jobUUID             = valueMap.getString("jobUUID");
                                       final String                scheduleUUID        = valueMap.getString("scheduleUUID");
                                       final String                jobName             = valueMap.getString("jobName");
                                       final Settings.ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",Settings.ArchiveTypes.class,Settings.ArchiveTypes.NORMAL);
                                       final String                name                = valueMap.getString("name");
                                       final long                  dateTime            = valueMap.getLong  ("dateTime");
                                       final long                  totalEntryCount     = valueMap.getLong  ("totalEntryCount");
                                       final long                  totalEntrySize      = valueMap.getLong  ("totalEntrySize");
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
                                                                                                      totalEntryCount,
                                                                                                      totalEntrySize,
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
                                                                   Units.formatByteSize(storageIndexData.totalEntrySize),
                                                                   SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.lastCreatedDateTime*1000L)),
                                                                   storageIndexData.indexState.toString()
                                                                  );
                                           tableItem.setChecked(selectedIndexIdSet.contains(storageIndexData.id));
                                         }
                                       });
                                     }
                                     catch (IllegalArgumentException exception)
                                     {
                                       if (Settings.debugLevel > 0)
                                       {
                                         System.err.println("ERROR: "+exception.getMessage());
                                         System.exit(1);
                                       }
                                     }

                                     // store number of entries
                                     n[0] = i+1;

                                     // check if aborted
                                     if (isUpdateTriggered() || (n[0] >= limit))
                                     {
                                       abort();
                                     }

                                     return Errors.NONE;
                                   }
                                 }
                                );
      }
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetStorageTable.setRedraw(true);
          }
        });
      }

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
      final HashSet<MenuItem> removeUUIDMenuItemSet   = new HashSet<MenuItem>();
      final HashSet<MenuItem> removeEntityMenuItemSet = new HashSet<MenuItem>();

      // get all existing UUID menus
      removeUUIDMenuItemSet.clear();
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (MenuItem menuItem : widgetStorageTreeAssignToMenu.getItems())
          {
            assert menuItem.getData() instanceof UUIDIndexData;

            removeUUIDMenuItemSet.add(menuItem);
          }
        }
      });
      if (isUpdateTriggered()) return;

      // get comperator
      final IndexDataComparator indexDataComparator = IndexDataComparator.getInstance(IndexDataComparator.SortModes.ID);

      // get and update UUIDs menu items
      final HashMap<String,Menu> uuidMenuMap = new HashMap<String,Menu>();
      BARServer.executeCommand(StringParser.format("INDEX_UUID_LIST"),
                               1,  // debugLevel
                               new CommandResultHandler()
                               {
                                 public int handleResult(int i, ValueMap valueMap)
                                 {
                                   try
                                   {
                                     long         uuidId              = valueMap.getLong  ("uuidId"             );
                                     final String jobUUID             = valueMap.getString("jobUUID"            );
                                     String       name                = valueMap.getString("name"               );
                                     long         lastCreatedDateTime = valueMap.getLong  ("lastCreatedDateTime");
                                     String       lastErrorMessage    = valueMap.getString("lastErrorMessage"   );
                                     long         totalEntryCount     = valueMap.getLong  ("totalEntryCount"    );
                                     long         totalEntrySize      = valueMap.getLong  ("totalEntrySize"     );

                                     // add UUID index data
                                     final UUIDIndexData uuidIndexData = new UUIDIndexData(uuidId,
                                                                                           jobUUID,
                                                                                           name,
                                                                                           lastCreatedDateTime,
                                                                                           lastErrorMessage,
                                                                                           totalEntryCount,
                                                                                           totalEntrySize
                                                                                          );

                                     // update/insert menu item
                                     display.syncExec(new Runnable()
                                     {
                                       public void run()
                                       {
                                         Menu subMenu = Widgets.getMenu(widgetStorageTreeAssignToMenu,uuidIndexData,indexDataComparator);
                                         if (subMenu == null)
                                         {
                                           MenuItem menuItem;

                                           // insert new sub-menu
                                           subMenu = Widgets.insertMenu(widgetStorageTreeAssignToMenu,
                                                                        findStorageMenuIndex(uuidIndexData),
                                                                        (Object)uuidIndexData,
                                                                        uuidIndexData.name.replaceAll("&","&&")
                                                                       );

                                           // insert new normal/full/incremental/differential menu items
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
                                           menuItem = Widgets.addMenuItem(subMenu,
                                                                          null,
                                                                          BARControl.tr("new continuous")
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
                                               assignStorage(uuidIndexData,Settings.ArchiveTypes.CONTINUOUS);
                                             }
                                           });
                                           Widgets.addMenuSeparator(subMenu);

                                           // store sub-menu for UUID index data
                                           uuidIndexData.setSubMenu(subMenu);
                                         }
                                         else
                                         {
                                           MenuItem menuItem;

                                           assert subMenu.getData() instanceof UUIDIndexData;

                                           menuItem = subMenu.getParentItem();

                                           // update sub-menu text
                                           menuItem.setText(uuidIndexData.name.replaceAll("&","&&"));

                                           // keep sub-menu
                                           removeUUIDMenuItemSet.remove(menuItem);
                                         }

                                         // store create/updated uuid index
                                         uuidMenuMap.put(jobUUID,subMenu);
                                       }
                                     });

                                   }
                                   catch (IllegalArgumentException exception)
                                   {
                                     if (Settings.debugLevel > 0)
                                     {
                                       System.err.println("ERROR: "+exception.getMessage());
                                       System.exit(1);
                                     }
                                   }

                                   // check if aborted
                                   if (isUpdateTriggered())
                                   {
                                     abort();
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );
      if (isUpdateTriggered()) return;

      // remove not existing UUID sub-menus
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (MenuItem menuItem : removeUUIDMenuItemSet)
          {
            menuItem.dispose();
          }
        }
      });
      if (isUpdateTriggered()) return;

      // get all entity menu items
      removeEntityMenuItemSet.clear();
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (MenuItem menuItem : widgetStorageTreeAssignToMenu.getItems())
          {
            assert menuItem.getData() instanceof UUIDIndexData;

            Menu subMenu = menuItem.getMenu();
            assert(subMenu != null);

            MenuItem menuItems[] = subMenu.getItems();
            for (int i = STORAGE_LIST_MENU_START_INDEX; i < menuItems.length; i++)
            {
              assert menuItems[i].getData() instanceof EntityIndexData;
              removeEntityMenuItemSet.add(menuItems[i]);
            }
          }
        }
      });
      if (isUpdateTriggered()) return;

      // update entity menu items
      BARServer.executeCommand(StringParser.format("INDEX_ENTITY_LIST"),
                               1,  // debugLevel
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
                                     long                  totalEntryCount     = valueMap.getLong  ("totalEntryCount"                           );
                                     long                  totalEntrySize      = valueMap.getLong  ("totalEntrySize"                              );

                                     // get UUID menu
                                     final Menu subMenu = uuidMenuMap.get(jobUUID);
                                     if (subMenu != null)
                                     {
                                       // add entity data index
                                       final EntityIndexData entityIndexData = new EntityIndexData(entityId,
                                                                                                   jobUUID,
                                                                                                   scheduleUUID,
                                                                                                   archiveType,
                                                                                                   lastCreatedDateTime,
                                                                                                   lastErrorMessage,
                                                                                                   totalEntryCount,
                                                                                                   totalEntrySize
                                                                                                  );

                                       // update/insert menu item
                                       display.syncExec(new Runnable()
                                       {
                                         public void run()
                                         {
                                           MenuItem menuItem = Widgets.getMenuItem(subMenu,entityIndexData,indexDataComparator);
                                           if (menuItem == null)
                                           {
                                             // insert menu item
                                             menuItem = Widgets.insertMenuItem(subMenu,
                                                                               findStorageMenuIndex(subMenu,entityIndexData),
                                                                               (Object)entityIndexData,
                                                                               ((entityIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-")+", "+entityIndexData.archiveType.toString()
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
                                             // update menu item
                                             assert menuItem.getData() instanceof EntityIndexData;

                                             Widgets.updateMenuItem(subMenu,
                                                                    (Object)entityIndexData,
                                                                    ((entityIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-")+", "+entityIndexData.archiveType.toString()
                                                                   );

                                             removeEntityMenuItemSet.remove(menuItem);
                                           }
                                         }
                                       });
                                     }
                                   }
                                   catch (IllegalArgumentException exception)
                                   {
                                     if (Settings.debugLevel > 0)
                                     {
                                       System.err.println("ERROR: "+exception.getMessage());
                                       System.exit(1);
                                     }
                                   }

                                   // check if aborted
                                   if (isUpdateTriggered())
                                   {
                                     abort();
                                   }

                                   return Errors.NONE;
                                 }
                               }
                              );
      if (isUpdateTriggered()) return;

      // remove not existing entity menu items
      display.syncExec(new Runnable()
      {
        public void run()
        {
          for (MenuItem menuItem : removeEntityMenuItemSet)
          {
            EntityIndexData entityIndexData = (EntityIndexData)menuItem.getData();
            entityIndexData.clearMenuItem();
            menuItem.dispose();
          }
        }
      });
      if (isUpdateTriggered()) return;
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

  /** restore states
   */
  enum RestoreStates
  {
    NONE,
    WAITING,
    RUNNING,
    DRY_RUNNING,
    REQUEST_VOLUME,
    DONE,
    ERROR,
    ABORTED;
  };


  /** entry restore states
   */
//TODO
/*
  enum RestoreStates
  {
    UNKNOWN,
    RESTORED,
    ERROR
  }*/

  /** actions
   */
  enum Actions
  {
    NONE,
    REQUEST_PASSWORD,
    REQUEST_VOLUME,
    CONFIRM;
  };

  /** password types
   */
  enum PasswordTypes
  {
    NONE,
    FTP,
    SSH,
    WEBDAV,
    CRYPT;

    /** check if login password
     * @return true iff login password
     */
    public boolean isLogin()
    {
      return (this == FTP) || (this == SSH) || (this == WEBDAV);
    }

    /** check if crypt password
     * @return true iff crypt password
     */
    public boolean isCrypt()
    {
      return (this == CRYPT);
    }

    /** convert to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case NONE:   return "none";
        case FTP:    return "FTP";
        case SSH:    return "SSH";
        case WEBDAV: return "WebDAV";
        case CRYPT:  return "encryption";
      }

      return "";
    }
  };

  /** entry data
   */
  class EntryIndexData extends IndexData
  {
    String                jobName;
    Settings.ArchiveTypes archiveType;
    String                storageName;
    long                  storageDateTime;
    EntryTypes            entryType;
    String                name;
    long                  dateTime;
    long                  size;              // file/directory size
    boolean               checked;           // true iff check mark set
    RestoreStates         restoreState;      // current restore state

    /** create entry data
     * @param entryId entry id
     * @param jobName job name
     * @param archiveType archive type
     * @param storageName storage archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     * @param size size [bytes]
     */
    EntryIndexData(long entryId, String jobName, Settings.ArchiveTypes archiveType, String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime, long size)
    {
      super(entryId);
      this.jobName         = jobName;
      this.archiveType     = archiveType;
      this.storageName     = storageName;
      this.storageDateTime = storageDateTime;
      this.entryType       = entryType;
      this.name            = name;
      this.dateTime        = dateTime;
      this.size            = size;
      this.checked         = false;
      this.restoreState    = RestoreStates.NONE;
    }

    /** create entry data
     * @param entryId entry id
     * @param jobName job name
     * @param archiveType archive type
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     */
    EntryIndexData(long entryId, String jobName, Settings.ArchiveTypes archiveType, String storageName, long storageDateTime, EntryTypes entryType, String name, long dateTime)
    {
      this(entryId,jobName,archiveType,storageName,storageDateTime,entryType,name,dateTime,0L);
    }

    /** get number of entries
     * @return entries
     */
    public long getEntryCount()
    {
      return 1;
    }

    /** get size
     * @return size [bytes]
     */
    public long getEntrySize()
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
      return "Entry {"+storageName+", "+name+", "+entryType+", dateTime="+dateTime+", size="+size+", checked="+checked+", state="+restoreState+"}";
    }
  };

  /** entry data comparator
   */
  static class EntryIndexDataComparator implements Comparator<EntryIndexData>
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
    EntryIndexDataComparator(Table table, TableColumn sortColumn)
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
    EntryIndexDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare entry data
     * @param entryIndexData1, entryIndexData2 entry data to compare
     * @return -1 iff entryIndexData1 < entryIndexData2,
                0 iff entryIndexData1 = entryIndexData2,
                1 iff entryIndexData1 > entryIndexData2
     */
    public int compare(EntryIndexData entryIndexData1, EntryIndexData entryIndexData2)
    {
//Dprintf.dprintf("");
//if ((entryIndexData1 == null) || (entryIndexData2 == null)) return 0;
      switch (sortMode)
      {
        case ARCHIVE:
          return entryIndexData1.storageName.compareTo(entryIndexData2.storageName);
        case NAME:
          return entryIndexData1.name.compareTo(entryIndexData2.name);
        case TYPE:
          return entryIndexData1.entryType.compareTo(entryIndexData2.entryType);
        case SIZE:
          if      (entryIndexData1.size < entryIndexData2.size) return -1;
          else if (entryIndexData1.size > entryIndexData2.size) return  1;
          else                                                  return  0;
        case DATE:
          if      (entryIndexData1.dateTime < entryIndexData2.dateTime) return -1;
          else if (entryIndexData1.dateTime > entryIndexData2.dateTime) return  1;
          else                                                          return  0;
        default:
          return 0;
      }
    }
  }

  /** update entry table thread
   */
  class UpdateEntryTableThread extends Thread
  {
    private final int PAGE_SIZE = 32;

    private Object           trigger                      = new Object();   // trigger update object
    private boolean          requestUpdateTotalEntryCount = false;
    private HashSet<Integer> requestUpdateOffsets         = new HashSet<Integer>();
    private long             totalEntryCount              = 0;
    private EntryTypes       entryType                    = EntryTypes.ANY;
    private String           entryName                    = "";
    private boolean          newestOnly                   = false;
    private boolean          requestSetUpdateIndicator    = false;          // true to set color/cursor at update

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
      boolean          updateTotalEntryCount = true;
      HashSet<Integer> updateOffsets         = new HashSet<Integer>();
      boolean          setUpdateIndicator    = true;
      try
      {
        for (;;)
        {
          // update table count, table segment
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
            if (!this.requestUpdateTotalEntryCount && updateTotalEntryCount)
            {
              updateEntryTableTotalEntryCount();
            }
            if (!this.requestUpdateTotalEntryCount && !updateOffsets.isEmpty())
            {
              updateEntryTable(updateOffsets);
            }
          }
          catch (CommunicationError error)
          {
            // ignored
          }
          catch (ConnectionError error)
          {
            // ignored
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
            if (!this.requestUpdateTotalEntryCount && this.requestUpdateOffsets.isEmpty())
            {
              try { trigger.wait(30*1000); } catch (InterruptedException exception) { /* ignored */ };
            }

            // check if update count, offsets to update
            updateTotalEntryCount = this.requestUpdateTotalEntryCount;
            updateOffsets.addAll(this.requestUpdateOffsets);
            setUpdateIndicator = this.requestSetUpdateIndicator;

            // if not triggered (timeout occurred) update count is done invisible (color is not set)
            if (!this.requestUpdateTotalEntryCount && this.requestUpdateOffsets.isEmpty())
            {
              updateTotalEntryCount = true;
              setUpdateIndicator    = false;
            }

            // wait for immediate further triggers
            do
            {
              this.requestUpdateTotalEntryCount = false;
              this.requestUpdateOffsets.clear();
              this.requestSetUpdateIndicator    = false;

              try { trigger.wait(500); } catch (InterruptedException exception) { /* ignored */ };
              updateOffsets.addAll(this.requestUpdateOffsets);
            }
            while (this.requestUpdateTotalEntryCount || !this.requestUpdateOffsets.isEmpty());
          }
        }
      }
      catch (Throwable throwable)
      {
        if (Settings.debugLevel > 0)
        {
          BARServer.disconnect();
          System.err.println("ERROR: "+throwable.getMessage());
          BARControl.printStackTrace(throwable);
          System.exit(1);
        }
      }
    }

    /** get total count
     * @return total count
     */
    public long getTotalEntryCount()
    {
      return totalEntryCount;
    }

    /** get entry type
     * @return entry type
     */
    public EntryTypes getEntryType()
    {
      return entryType;
    }

    /** get entry name
     * @return entry name
     */
    public String getEntryName()
    {
      return entryName;
    }

    /** get newest-only
     * @return true for newest entries only
     */
    public boolean getNewestOnly()
    {
      return newestOnly;
    }

    /** trigger update of entry list
     * @param entryName new entry name or null
     * @param type type or *
     * @param newestOnly flag for newest entries only
     */
    public void triggerUpdate(String entryName, String type, boolean newestOnly)
    {
      synchronized(trigger)
      {
        if (   (this.entryName == null) || (entryName == null) || !this.entryName.equals(entryName)
            || (entryType != this.entryType)
            || (this.newestOnly != newestOnly)
           )
        {
          this.entryName                    = entryName;
          this.entryType                    = entryType;
          this.newestOnly                   = newestOnly;
          this.requestUpdateTotalEntryCount = true;
          this.requestSetUpdateIndicator    = true;
          trigger.notify();
        }
      }
    }

    /** trigger update of entry list
     * @param entryName new entry pattern or null
     */
    public void triggerUpdateEntryName(String entryName)
    {
      assert entryName != null;

      synchronized(trigger)
      {
        // if ((this.entryName == null) || (entryName == null) || !this.entryName.equals(entryName))
        if (   (this.entryName == null)
            || (entryName == null)
//Note: at least 3 characters?
            || (((entryName.length() == 0) || (entryName.length() >= 3)) && !this.entryName.equals(entryName))
           )
        {
          this.entryName                    = entryName;
          this.requestUpdateTotalEntryCount = true;
          this.requestSetUpdateIndicator    = true;
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
          this.entryType                    = entryType;
          this.requestUpdateTotalEntryCount = true;
          this.requestSetUpdateIndicator    = true;
          trigger.notify();
        }
      }
    }


    /** trigger update of entry list
     * @param newestOnly flag for newest entries only or null
     */
    public void triggerUpdateNewestOnly(boolean newestOnly)
    {
      synchronized(trigger)
      {
        if (this.newestOnly != newestOnly)
        {
          this.newestOnly                   = newestOnly;
          this.requestUpdateTotalEntryCount = true;
          this.requestSetUpdateIndicator    = true;
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
        if (!this.requestUpdateOffsets.contains(offset))
        {
          this.requestUpdateOffsets.add(offset);
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
        this.requestUpdateTotalEntryCount = true;
        trigger.notify();
      }
    }

    /** check if update triggered
     * @return true iff update triggered
     */
    private boolean isUpdateTriggered()
    {
      return requestUpdateTotalEntryCount || !requestUpdateOffsets.isEmpty();
    }

    /** refresh entry table display total count
     */
    private void updateEntryTableTotalEntryCount()
    {
      assert entryName != null;

      final long oldTotalEntryCount = totalEntryCount;

      // set update indicator
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetEntryTableTitle.setForeground(COLOR_MODIFIED);
          widgetEntryTableTitle.redraw();
        }
      });

      // get entries info
      final String[] errorMessage = new String[1];
      ValueMap       valueMap     = new ValueMap();
      if (BARServer.executeCommand(StringParser.format("INDEX_ENTRIES_INFO name=%'S indexType=%s newestOnly=%y",
                                                       entryName,
                                                       entryType.toString(),
                                                       newestOnly
                                                      ),
                                   1,  // debugLevel
                                   errorMessage,
                                   valueMap
                                  ) == Errors.NONE
         )
      {
        totalEntryCount = valueMap.getLong("totalEntryCount");
        if ((oldTotalEntryCount > 0) && (oldTotalEntryCount <= MAX_SHOWN_ENTRIES) && (totalEntryCount > MAX_SHOWN_ENTRIES))
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              Dialogs.warning(shell,
                              Dialogs.booleanFieldUpdater(Settings.class,"showEntriesExceededInfo"),
                              BARControl.tr("There are {0} entries. Only the first {1} are shown in the list.",
                                            updateEntryTableThread.getTotalEntryCount(),
                                            MAX_SHOWN_ENTRIES
                                           )
                             );
            }
          });
        }
      }

      // set count
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetEntryTableTitle.setForeground(null);
          widgetEntryTableTitle.redraw();

          if (oldTotalEntryCount != totalEntryCount)
          {
            widgetEntryTable.setRedraw(false);

            widgetEntryTable.setItemCount(0);
            widgetEntryTable.clearAll();

            widgetEntryTable.setItemCount((int)Math.min(totalEntryCount,MAX_SHOWN_ENTRIES));
            widgetEntryTable.setTopIndex(0);

            widgetEntryTable.setRedraw(true);
          }
        }
      });
    }

    /** refresh entry table items
     * @param offset refresh offset
     * @return true iff update done
     */
    private boolean updateEntryTable(final int offset)
    {
      assert entryName != null;
      assert offset >= 0;
      assert totalEntryCount >= 0;

      // get limit
      final int limit = ((offset+PAGE_SIZE) < totalEntryCount) ? PAGE_SIZE : (int)(totalEntryCount-offset);

//TODO: sort
//IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTable,tableColumn);

      // update entry table segment
      final String[] errorMessage = new String[1];
      final int[]    n            = new int[1];
      BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST name=%'S indexType=%s newestOnly=%y offset=%d limit=%d",
                                                   entryName,
                                                   entryType.toString(),
                                                   newestOnly,
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
                                     String                jobName         = valueMap.getString("jobName"                                );
                                     Settings.ArchiveTypes archiveType     = valueMap.getEnum  ("archiveType",Settings.ArchiveTypes.class);
                                     long                  entryId         = valueMap.getLong  ("entryId"                                );
                                     String                storageName     = valueMap.getString("storageName"                            );
                                     long                  storageDateTime = valueMap.getLong  ("storageDateTime"                        );

                                     switch (valueMap.getEnum("entryType",EntryTypes.class))
                                     {
                                       case FILE:
                                         {
                                           String fileName       = valueMap.getString("name"          );
                                           long   dateTime       = valueMap.getLong  ("dateTime"      );
                                           long   size           = valueMap.getLong  ("size"          );
                                           long   fragmentOffset = valueMap.getLong  ("fragmentOffset");
                                           long   fragmentSize   = valueMap.getLong  ("fragmentSize"  );

                                           // add entry data index
                                           final EntryIndexData entryIndexData = new EntryIndexData(entryId,
                                                                                                    jobName,
                                                                                                    archiveType,
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
                                                                       (Object)entryIndexData,
                                                                       entryIndexData.storageName,
                                                                       entryIndexData.name,
                                                                       "FILE",
                                                                       Units.formatByteSize(entryIndexData.size),
                                                                       SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryIndexData.id));
                                             }
                                           });
                                         }
                                         break;
                                       case IMAGE:
                                         {
                                           String imageName   = valueMap.getString("name"       );
                                           long   size        = valueMap.getLong  ("size"       );
                                           long   blockOffset = valueMap.getLong  ("blockOffset");
                                           long   blockCount  = valueMap.getLong  ("blockCount" );

                                           // add entry data index
                                           final EntryIndexData entryIndexData = new EntryIndexData(entryId,
                                                                                                    jobName,
                                                                                                    archiveType,
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
                                                                       (Object)entryIndexData,
                                                                       entryIndexData.storageName,
                                                                       entryIndexData.name,
                                                                       "IMAGE",
                                                                       Units.formatByteSize(entryIndexData.size),
                                                                       SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryIndexData.id));
                                             }
                                           });
                                         }
                                         break;
                                       case DIRECTORY:
                                         {
                                           String directoryName = valueMap.getString("name"    );
                                           long   dateTime      = valueMap.getLong  ("dateTime");
                                           long   size          = valueMap.getLong  ("size"    );

                                           // add entry data index
                                           final EntryIndexData entryIndexData = new EntryIndexData(entryId,
                                                                                                    jobName,
                                                                                                    archiveType,
                                                                                                    storageName,
                                                                                                    storageDateTime,
                                                                                                    EntryTypes.DIRECTORY,
                                                                                                    directoryName,
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
                                                                       (Object)entryIndexData,
                                                                       entryIndexData.storageName,
                                                                       entryIndexData.name,
                                                                       "DIR",
                                                                       "",
                                                                       SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryIndexData.id));
                                             }
                                           });
                                         }
                                         break;
                                       case LINK:
                                         {
                                           String linkName        = valueMap.getString("name"           );
                                           String destinationName = valueMap.getString("destinationName");
                                           long   dateTime        = valueMap.getLong  ("dateTime"       );

                                           // add entry data index
                                           final EntryIndexData entryIndexData = new EntryIndexData(entryId,
                                                                                                    jobName,
                                                                                                    archiveType,
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
                                                                       (Object)entryIndexData,
                                                                       entryIndexData.storageName,
                                                                       entryIndexData.name,
                                                                       "LINK",
                                                                       "",
                                                                       SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryIndexData.id));
                                             }
                                           });
                                         }
                                         break;
                                       case HARDLINK:
                                         {
                                           String fileName       = valueMap.getString("name"          );
                                           long   dateTime       = valueMap.getLong  ("dateTime"      );
                                           long   size           = valueMap.getLong  ("size"          );
                                           long   fragmentOffset = valueMap.getLong  ("fragmentOffset");
                                           long   fragmentSize   = valueMap.getLong  ("fragmentSize"  );

                                           // add entry data index
                                           final EntryIndexData entryIndexData = new EntryIndexData(entryId,
                                                                                                    jobName,
                                                                                                    archiveType,
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
                                                                       (Object)entryIndexData,
                                                                       entryIndexData.storageName,
                                                                       entryIndexData.name,
                                                                       "HARDLINK",
                                                                       Units.formatByteSize(entryIndexData.size),
                                                                       SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryIndexData.id));
                                             }
                                           });
                                         }
                                         break;
                                       case SPECIAL:
                                         {

                                           String name     = valueMap.getString("name"    );
                                           long   dateTime = valueMap.getLong  ("dateTime");

                                           // add entry data index
                                           final EntryIndexData entryIndexData = new EntryIndexData(entryId,
                                                                                                    jobName,
                                                                                                    archiveType,
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
                                                                       (Object)entryIndexData,
                                                                       entryIndexData.storageName,
                                                                       entryIndexData.name,
                                                                       "DEVICE",
                                                                       Units.formatByteSize(entryIndexData.size),
                                                                       SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                                                      );
                                               tableItem.setChecked(selectedEntryIdSet.contains(entryIndexData.id));
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
                                       System.exit(1);
                                     }
                                   }

                                   // store number of entries
                                   n[0] = i+1;

                                   // check if aborted
                                   if (isUpdateTriggered() || (n[0] >= limit))
                                   {
                                     abort();
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
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            widgetEntryTable.setRedraw(false);
          }
        });
      }
      try
      {
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
            widgetEntryTable.setRedraw(true);
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
  private final SimpleDateFormat SIMPLE_DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  // index data comparator
  final Comparator<IndexData> indexIdComperator = new Comparator<IndexData>()
  {
    /** compare index data ids
     * @param indexData1,indexData2 index data
     * @return true iff index id equals
     */
    public int compare(IndexData indexData1, IndexData indexData2)
    {
      return (indexData1.id == indexData2.id) ? 0 : 1;
    }
  };

  private final int STORAGE_TREE_MENU_START_INDEX = 0;
  private final int STORAGE_LIST_MENU_START_INDEX = 6;  // entry 0..4: new "..."; entry 5: separator

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
  private Text                         widgetStorageFilter;
  private Combo                        widgetStorageStateFilter;
  private WidgetEvent                  checkedStorageEvent = new WidgetEvent();     // triggered when checked-state of some storage changed

  private Label                        widgetEntryTableTitle;
  private Table                        widgetEntryTable;
  private Shell                        widgetEntryTableToolTip = null;
  private Text                         widgetEntryFilter;
  private Combo                        widgetEntryTypeFilter;
  private Button                       widgetEntryNewestOnly;
  private WidgetEvent                  checkedEntryEvent = new WidgetEvent();       // triggered when checked-state of some entry changed

  private UpdateStorageTreeTableThread updateStorageTreeTableThread = new UpdateStorageTreeTableThread();
  private IndexData                    selectedIndexData = null;
  final private IndexIdSet             selectedIndexIdSet = new IndexIdSet();

  private UpdateEntryTableThread       updateEntryTableThread = new UpdateEntryTableThread();
  final private IndexIdSet             selectedEntryIdSet = new IndexIdSet();

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** show UUID index tool tip
   * @param entityIndexData entity index data
   * @param x,y positions
   */
  private void showUUIDIndexToolTip(UUIDIndexData uuidIndexData, int x, int y)
  {
    int   row;
    Label label;

    if (widgetStorageTreeToolTip != null)
    {
      widgetStorageTreeToolTip.dispose();
    }

    if (uuidIndexData != null)
    {
      widgetStorageTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetStorageTreeToolTip.setBackground(COLOR_INFO_BACKGROUND);
      widgetStorageTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetStorageTreeToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Name")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,uuidIndexData.name);
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      if (Settings.debugLevel > 0)
      {
        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Id")+":");
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,Long.toString(uuidIndexData.id));
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Job UUID")+":");
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,uuidIndexData.jobUUID);
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last created")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,(uuidIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(uuidIndexData.lastCreatedDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last error")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,uuidIndexData.lastErrorMessage);
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total entries")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,String.format("%d",uuidIndexData.getEntryCount()));
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total size")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(uuidIndexData.getEntrySize()),uuidIndexData.getEntrySize())));
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      Point size = widgetStorageTreeToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
      widgetStorageTreeToolTip.setBounds(x,y,size.x,size.y);
      widgetStorageTreeToolTip.setVisible(true);

      widgetStorageTreeToolTip.addMouseTrackListener(new MouseTrackListener()
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
            // check if inside sub-widget
            Point point = new Point(mouseEvent.x,mouseEvent.y);
            for (Control control : widgetStorageTreeToolTip.getChildren())
            {
              if (control.getBounds().contains(point))
              {
                return;
              }
            }

            // close tooltip
            widgetStorageTreeToolTip.dispose();
            widgetStorageTreeToolTip = null;
          }
        }

        @Override
        public void mouseHover(MouseEvent mouseEvent)
        {
        }
      });
    }
  }

  /** show entity index tool tip
   * @param entityIndexData entity index data
   * @param x,y positions
   */
  private void showEntityIndexToolTip(EntityIndexData entityIndexData, int x, int y)
  {
    int   row;
    Label label;

    if (widgetStorageTreeToolTip != null)
    {
      widgetStorageTreeToolTip.dispose();
    }

    if (entityIndexData != null)
    {
      widgetStorageTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetStorageTreeToolTip.setBackground(COLOR_INFO_BACKGROUND);
      widgetStorageTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetStorageTreeToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      if (Settings.debugLevel > 0)
      {
        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Id")+":");
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,Long.toString(entityIndexData.id));
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Job UUID")+":");
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.jobUUID);
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Schedule UUID")+":");
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.scheduleUUID);
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last created")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,(entityIndexData.lastCreatedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.lastCreatedDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last error")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.lastErrorMessage);
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total entries")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,String.format("%d",entityIndexData.getEntryCount()));
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total size")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(entityIndexData.getEntrySize()),entityIndexData.getEntrySize())));
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      Point size = widgetStorageTreeToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
      widgetStorageTreeToolTip.setBounds(x,y,size.x,size.y);
      widgetStorageTreeToolTip.setVisible(true);

      widgetStorageTreeToolTip.addMouseTrackListener(new MouseTrackListener()
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
            // check if inside sub-widget
            Point point = new Point(mouseEvent.x,mouseEvent.y);
            for (Control control : widgetStorageTreeToolTip.getChildren())
            {
              if (control.getBounds().contains(point))
              {
                return;
              }
            }

            // close tooltip
            widgetStorageTreeToolTip.dispose();
            widgetStorageTreeToolTip = null;
          }
        }

        @Override
        public void mouseHover(MouseEvent mouseEvent)
        {
        }
      });
    }
  }

  /** show storage index tool tip
   * @param storageIndexData storage index data
   * @param x,y positions
   */
  private void showStorageIndexToolTip(StorageIndexData storageIndexData, int x, int y)
  {
    int   row;
    Label label;

    if (widgetStorageTableToolTip != null)
    {
      widgetStorageTableToolTip.dispose();
    }

    if (storageIndexData != null)
    {
      widgetStorageTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetStorageTableToolTip.setBackground(COLOR_INFO_BACKGROUND);
      widgetStorageTableToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetStorageTableToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Job")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.jobName);
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Name")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.name);
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      if (Settings.debugLevel > 0)
      {
        label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Id")+":");
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTableToolTip,Long.toString(storageIndexData.id));
        label.setForeground(COLOR_INFO_FORGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Created")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.lastCreatedDateTime*1000L)));
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Type")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.archiveType.toString());
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Entries")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,String.format("%d",storageIndexData.getEntryCount()));
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Size")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(storageIndexData.getEntrySize()),storageIndexData.getEntrySize())));
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("State")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.indexState.toString());
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Last checked")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,(storageIndexData.lastCheckedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.lastCheckedDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Error")+":");
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.errorMessage);
      label.setForeground(COLOR_INFO_FORGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      Point size = widgetStorageTableToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
      widgetStorageTableToolTip.setBounds(x,y,size.x,size.y);
      widgetStorageTableToolTip.setVisible(true);

      widgetStorageTableToolTip.addMouseTrackListener(new MouseTrackListener()
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
            // check if inside sub-widget
            Point point = new Point(mouseEvent.x,mouseEvent.y);
            for (Control control : widgetStorageTableToolTip.getChildren())
            {
              if (control.getBounds().contains(point))
              {
                return;
              }
            }

            // close tooltip
            widgetStorageTableToolTip.dispose();
            widgetStorageTableToolTip = null;
          }
        }

        @Override
        public void mouseHover(MouseEvent mouseEvent)
        {
        }
      });
    }
  }

  /** show entry data tool tip
   * @param entryIndexData entry index data
   * @param x,y positions
   */
  private void showEntryToolTip(EntryIndexData entryIndexData, int x, int y)
  {
    int     row;
    Label   label;
    Control control;

    final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
    final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

    if (widgetEntryTableToolTip != null)
    {
      widgetEntryTableToolTip.dispose();
    }

    if (entryIndexData != null)
    {
      widgetEntryTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetEntryTableToolTip.setBackground(COLOR_BACKGROUND);
      widgetEntryTableToolTip.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetEntryTableToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Job")+":");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.jobName);
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Type")+":");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.archiveType.toString());
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Storage")+":");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.storageName);
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Created")+":");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.storageDateTime*1000L)));
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      control = Widgets.newSpacer(widgetEntryTableToolTip);
      Widgets.layout(control,row,0,TableLayoutData.WE,0,2,0,0,SWT.DEFAULT,1,SWT.DEFAULT,1,SWT.DEFAULT,1);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Name")+":");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.name);
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Type")+":");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.entryType.toString());
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Size")+":");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(entryIndexData.size),entryIndexData.size)));
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Date")+":");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,(entryIndexData.dateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L)) : "-");
      label.setForeground(COLOR_FORGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      Point size = widgetEntryTableToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
      widgetEntryTableToolTip.setBounds(x,y,size.x,size.y);
      widgetEntryTableToolTip.setVisible(true);

      widgetEntryTableToolTip.addMouseTrackListener(new MouseTrackListener()
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
            // check if inside sub-widget
            Point point = new Point(mouseEvent.x,mouseEvent.y);
            for (Control control : widgetEntryTableToolTip.getChildren())
            {
              if (control.getBounds().contains(point))
              {
                return;
              }
            }

            // close tooltip
            widgetEntryTableToolTip.dispose();
            widgetEntryTableToolTip = null;
          }
        }

        @Override
        public void mouseHover(MouseEvent mouseEvent)
        {
        }
      });
    }
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
          if (updateEntryTableThread.getTotalEntryCount() >= MAX_SHOWN_ENTRIES)
          {
            Dialogs.warning(shell,
                            Dialogs.booleanFieldUpdater(Settings.class,"showEntriesExceededInfo"),
                            BARControl.tr("There are {0} entries. Only the first {1} are shown in the list.",
                                          updateEntryTableThread.getTotalEntryCount(),
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
          int       count;
          String    text;
          Point     size;

          // get current foreground color
          Color foreground = widgetEntryTableTitle.getForeground();

          // title
          text = BARControl.tr("Storage");
          size = Widgets.getTextSize(gc,text);
          gc.setForeground(display.getSystemColor(SWT.COLOR_BLACK));
          gc.drawText(text,
                      (bounds.width-size.x)/2,
                      8
                     );

          // number of entries
          count = updateStorageTreeTableThread.getStorageCount();
          text = (count >= 0) ? BARControl.tr("Count: {0}",count) : "-";
          size = Widgets.getTextSize(gc,text);
          gc.setForeground(foreground);
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

          // remove all selected sub-ids
          for (TreeItem subTreeItem : Widgets.getAllTreeItems(treeItem))
          {
            setStorageList(((IndexData)subTreeItem.getData()).id,false);
          }

          // close sub-tree
          treeItem.removeAll();
          new TreeItem(treeItem,SWT.NONE);
          treeItem.setExpanded(false);

          // trigger update checked
          checkedStorageEvent.trigger();
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
              // toggled check
              StorageIndexData storageIndexData = (StorageIndexData)treeItem.getData();
              setStorageList(storageIndexData.id,treeItem.getChecked());

              // trigger update checked
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

              // set/reset checked in sub-tree
              for (TreeItem subTreeItem : Widgets.getAllTreeItems(treeItem))
              {
                subTreeItem.setChecked(isChecked);
              }
              setStorageList(indexData.id,isChecked);

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
        }

        @Override
        public void mouseHover(MouseEvent mouseEvent)
        {
          Tree     tree     = (Tree)mouseEvent.widget;
          TreeItem treeItem = tree.getItem(new Point(mouseEvent.x,mouseEvent.y));

          if (widgetStorageTreeToolTip != null)
          {
            widgetStorageTreeToolTip.dispose();
            widgetStorageTreeToolTip = null;
          }

          // show tooltip if tree item available and mouse is in the right side
          if ((treeItem != null) && (mouseEvent.x > tree.getBounds().width/2))
          {
            Point point = display.getCursorLocation();
            if (point.x > 16) point.x -= 16;
            if (point.y > 16) point.y -= 16;

            if      (treeItem.getData() instanceof UUIDIndexData)
            {
              showUUIDIndexToolTip((UUIDIndexData)treeItem.getData(),point.x,point.y);
            }
            else if (treeItem.getData() instanceof EntityIndexData)
            {
              showEntityIndexToolTip((EntityIndexData)treeItem.getData(),point.x,point.y);
            }
            else if (treeItem.getData() instanceof StorageIndexData)
            {
              showStorageIndexToolTip((StorageIndexData)treeItem.getData(),point.x,point.y);
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

              if      (toIndexData instanceof UUIDIndexData)
              {
              }
              else if (toIndexData instanceof EntityIndexData)
              {
                EntityIndexData toEntityIndexData = (EntityIndexData)toIndexData;
                assignStorage(fromIndexData,toEntityIndexData);
              }
              else if (toIndexData instanceof StorageIndexData)
              {
                StorageIndexData toStorageIndexData = (StorageIndexData)toIndexData;
                if (treeItem != null)
                {
                  EntityIndexData toEntityIndexData = (EntityIndexData)treeItem.getParentItem().getData();
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
          if (i >= 0)
          {
            updateStorageTreeTableThread.triggerUpdate(i);
          }
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
            if (storageIndexData != null)
            {
              // set/reset checked
              setStorageList(storageIndexData.id,tabletem.getChecked());

              // trigger update checked
              checkedStorageEvent.trigger();
            }
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
              // set/reset checked
              setStorageList(storageIndexData.id,tabletem.getChecked());

              // trigger update checked
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
            if (storageIndexData != null)
            {
              Point point = display.getCursorLocation();
              if (point.x > 16) point.x -= 16;
              if (point.y > 16) point.y -= 16;

              showStorageIndexToolTip(storageIndexData,point.x,point.y);
            }
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

        subMenu = Widgets.addMenu(menu,BARControl.tr("Set job type")+"\u2026");
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
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("continuous")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem widget = (MenuItem)selectionEvent.widget;

              setEntityType(Settings.ArchiveTypes.CONTINUOUS);
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
            restore(RestoreTypes.ARCHIVES,selectedIndexIdSet);
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

        Widgets.addMenuSeparator(menu);

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
            switch (widgetStorageTabFolder.getSelectionIndex())
            {
              case 0:
                TreeItem treeItems[] = widgetStorageTree.getSelection();
                if (treeItems.length > 0)
                {
                  if (widgetStorageTreeToolTip != null)
                  {
                    widgetStorageTreeToolTip.dispose();
                    widgetStorageTreeToolTip = null;
                  }

                  if (treeItems[0] != null)
                  {
                    Point point = display.getCursorLocation();
                    if (point.x > 16) point.x -= 16;
                    if (point.y > 16) point.y -= 16;

                    if      (treeItems[0].getData() instanceof UUIDIndexData)
                    {
                      showUUIDIndexToolTip((UUIDIndexData)treeItems[0].getData(),point.x,point.y);
                    }
                    else if (treeItems[0].getData() instanceof EntityIndexData)
                    {
                      showEntityIndexToolTip((EntityIndexData)treeItems[0].getData(),point.x,point.y);
                    }
                    else if (treeItems[0].getData() instanceof StorageIndexData)
                    {
                      showStorageIndexToolTip((StorageIndexData)treeItems[0].getData(),point.x,point.y);
                    }
                  }
                }
                break;
              case 1:
                TableItem tableItems[] = widgetStorageTable.getSelection();
                if (tableItems.length > 0)
                {
                  if (widgetStorageTreeToolTip != null)
                  {
                    widgetStorageTreeToolTip.dispose();
                    widgetStorageTreeToolTip = null;
                  }

                  if (tableItems[0] != null)
                  {
                    Point point = display.getCursorLocation();
                    if (point.x > 16) point.x -= 16;
                    if (point.y > 16) point.y -= 16;

                    showStorageIndexToolTip((StorageIndexData)tableItems[0].getData(),point.x,point.y);
                  }
                }
                break;
            }
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

        widgetStorageFilter = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
        widgetStorageFilter.setToolTipText(BARControl.tr("Enter filter key words for storage list."));
        widgetStorageFilter.setMessage(BARControl.tr("Enter text to filter storage list"));
        Widgets.layout(widgetStorageFilter,0,2,TableLayoutData.WE);
        widgetStorageFilter.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            updateStorageTreeTableThread.triggerUpdateStorageName(widget.getText());
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            updateStorageTreeTableThread.triggerUpdateStorageName(widget.getText());
          }
        });
        widgetStorageFilter.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
            Text widget = (Text)keyEvent.widget;
            updateStorageTreeTableThread.triggerUpdateStorageName(widget.getText());
          }
        });
//???
        widgetStorageFilter.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
Dprintf.dprintf("");
//            Text widget = (Text)focusEvent.widget;
//            updateStorageTreeTableThread.triggerUpdateStorageName(widget.getText());
          }
        });

        label = Widgets.newLabel(composite,BARControl.tr("State")+":");
        Widgets.layout(label,0,3,TableLayoutData.W);

        widgetStorageStateFilter = Widgets.newOptionMenu(composite);
        widgetStorageStateFilter.setToolTipText(BARControl.tr("Storage states filter."));
        widgetStorageStateFilter.setItems(new String[]{"*","ok","error","update","update requested","update/update requested","error/update/update requested","not assigned"});
        widgetStorageStateFilter.setText("*");
        Widgets.layout(widgetStorageStateFilter,0,4,TableLayoutData.W);
        widgetStorageStateFilter.addSelectionListener(new SelectionListener()
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
            EntityStates  storageEntityState;
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
        Widgets.layout(button,0,5,TableLayoutData.DEFAULT,0,0,0,0,140,SWT.DEFAULT);
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

            restore(RestoreTypes.ARCHIVES,selectedIndexIdSet);
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

          // get current foreground color
          Color foreground = widgetEntryTableTitle.getForeground();

          // title
          text = BARControl.tr("Entries");
          size = Widgets.getTextSize(gc,text);
          gc.setForeground(display.getSystemColor(SWT.COLOR_BLACK));
          gc.drawText(text,
                      (bounds.width-size.x)/2,
                      (bounds.height-size.y)/2
                     );

          // number of entries
          text = BARControl.tr("Count: {0}",updateEntryTableThread.getTotalEntryCount());
          size = Widgets.getTextSize(gc,text);
          gc.setForeground(foreground);
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
          TableColumn              tableColumn         = (TableColumn)selectionEvent.widget;
          EntryIndexDataComparator entryIndexDataComparator = new EntryIndexDataComparator(widgetEntryTable,tableColumn);
          synchronized(widgetEntryTable)
          {
            {
              BARControl.waitCursor();
            }
            try
            {
              Widgets.sortTableColumn(widgetEntryTable,tableColumn,entryIndexDataComparator);
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
          if (i >= 0)
          {
            updateEntryTableThread.triggerUpdateTableItem(i);
          }
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

            EntryIndexData entryIndexData = (EntryIndexData)tableItem.getData();
            selectedEntryIdSet.set(entryIndexData.id,tableItem.getChecked());

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
            EntryIndexData entryIndexData = (EntryIndexData)tableItem.getData();
            if (entryIndexData != null)
            {
              tableItem.setChecked(!tableItem.getChecked());
              selectedEntryIdSet.set(entryIndexData.id,tableItem.getChecked());
            }
          }

          // trigger update entries
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

          if ((tableItem != null) && (mouseEvent.x > table.getBounds().width/2))
          {
            EntryIndexData entryIndexData = (EntryIndexData)tableItem.getData();
            if (entryIndexData != null)
            {
              Point point = table.toDisplay(mouseEvent.x+16,mouseEvent.y);
              showEntryToolTip(entryIndexData,point.x,point.y);
            }
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
            restore(RestoreTypes.ENTRIES,selectedEntryIdSet);
          }
        });

        Widgets.addMenuSeparator(menu);

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
            TableItem tableItems[] = widgetEntryTable.getSelection();
            if (tableItems.length > 0)
            {
              if (widgetEntryTableToolTip != null)
              {
                widgetEntryTableToolTip.dispose();
                widgetEntryTableToolTip = null;
              }

              if (tableItems[0] != null)
              {
                EntryIndexData entryIndexData = (EntryIndexData)tableItems[0].getData();
                if (entryIndexData != null)
                {
                  Point point = display.getCursorLocation();
                  if (point.x > 16) point.x -= 16;
                  if (point.y > 16) point.y -= 16;
                  showEntryToolTip(entryIndexData,point.x,point.y);
                }
              }
            }
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

        widgetEntryFilter = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
        widgetEntryFilter.setToolTipText(BARControl.tr("Enter filter key words for entry list."));
        widgetEntryFilter.setMessage(BARControl.tr("Enter text to filter entry list"));
        Widgets.layout(widgetEntryFilter,0,2,TableLayoutData.WE);
        widgetEntryFilter.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            Text  widget = (Text)selectionEvent.widget;
            updateEntryTableThread.triggerUpdateEntryName(widget.getText());
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            updateEntryTableThread.triggerUpdateEntryName(widget.getText());
          }
        });
        widgetEntryFilter.addKeyListener(new KeyListener()
        {
          @Override
          public void keyPressed(KeyEvent keyEvent)
          {
          }
          @Override
          public void keyReleased(KeyEvent keyEvent)
          {
            Text widget = (Text)keyEvent.widget;
            updateEntryTableThread.triggerUpdateEntryName(widget.getText());
          }
        });
//???
        widgetEntryFilter.addFocusListener(new FocusListener()
        {
          @Override
          public void focusGained(FocusEvent focusEvent)
          {
          }
          @Override
          public void focusLost(FocusEvent focusEvent)
          {
//            Text widget = (Text)focusEvent.widget;
//            updateEntryTableThread.triggerUpdateEntryName(widget.getText());
          }
        });

        widgetEntryTypeFilter = Widgets.newOptionMenu(composite);
        widgetEntryTypeFilter.setToolTipText(BARControl.tr("Entry type."));
        widgetEntryTypeFilter.setItems(new String[]{"*","files","directories","links","hardlinks","special"});
        Widgets.setOptionMenuItems(widgetEntryTypeFilter,new Object[]{"*",          EntryTypes.ANY,
                                                                      "files",      EntryTypes.FILE,
                                                                      "images",     EntryTypes.IMAGE,
                                                                      "directories",EntryTypes.DIRECTORY,
                                                                      "links",      EntryTypes.LINK,
                                                                      "hardlinks",  EntryTypes.HARDLINK,
                                                                      "special",    EntryTypes.SPECIAL
                                                                     }
                                  );
        Widgets.setSelectedOptionMenuItem(widgetEntryTypeFilter,EntryTypes.ANY);
        Widgets.layout(widgetEntryTypeFilter,0,3,TableLayoutData.W);
        widgetEntryTypeFilter.addSelectionListener(new SelectionListener()
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

        widgetEntryNewestOnly = Widgets.newCheckbox(composite,BARControl.tr("newest only"));
        widgetEntryNewestOnly.setToolTipText(BARControl.tr("When this checkbox is enabled, only show newest entry instances and hide all older entry instances."));
        Widgets.layout(widgetEntryNewestOnly,0,4,TableLayoutData.W);
        widgetEntryNewestOnly.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            boolean newestOnly = widget.getSelection();

            selectedEntryIdSet.clear();
            checkedEntryEvent.trigger();

            updateEntryTableThread.triggerUpdateNewestOnly(newestOnly);
          }
        });

        button = Widgets.newButton(composite,BARControl.tr("Restore")+"\u2026");
        button.setToolTipText(BARControl.tr("Start restoring selected entries."));
        button.setEnabled(false);
        Widgets.layout(button,0,5,TableLayoutData.DEFAULT,0,0,0,0,140,SWT.DEFAULT);
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
            restore(RestoreTypes.ENTRIES,selectedEntryIdSet);
          }
        });
      }
    }

    // listeners
    shell.addListener(BARControl.USER_EVENT_NEW_SERVER,new Listener()
    {
      public void handleEvent(Event event)
      {
        widgetStorageFilter.setText("");
        widgetStorageStateFilter.select(0);
        updateStorageTreeTableThread.triggerUpdate("",INDEX_STATE_SET_ALL,EntityStates.ANY);
        setAllCheckedStorage(false);

        widgetEntryFilter.setText("");
        Widgets.setSelectedOptionMenuItem(widgetEntryTypeFilter,EntryTypes.ANY);
        widgetEntryNewestOnly.setSelection(false);
        updateEntryTableThread.triggerUpdate("","*",false);
        setAllCheckedEntries(false);
      }
    });

    // start storage/entry update threads
    updateStorageTreeTableThread.start();
    updateEntryTableThread.start();
  }

  //-----------------------------------------------------------------------

  /** clear selected storage entries
   */
  private void clearStorageList()
  {
    BARServer.executeCommand(StringParser.format("STORAGE_LIST_CLEAR"),0);
  }

  /** set/clear selected storage entry
   * @param indexId index id
   * @param checked true for set checked, false for clear checked
   */
  private void setStorageList(long indexId, boolean checked)
  {
    if (checked)
    {
      BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD indexId=%ld",
                                                   indexId
                                                  ),
                               0  // debugLevel
                              );
    }
    else
    {
      BARServer.executeCommand(StringParser.format("STORAGE_LIST_REMOVE indexId=%ld",
                                                   indexId
                                                  ),
                               0  // debugLevel
                              );
    }
    selectedIndexIdSet.set(indexId,checked);
  }

  /** set selected storage entry
   * @param indexId index id
   */
  private void setStorageList(long indexId)
  {
    setStorageList(indexId,true);
  }

  /** set selected storage entries
   * @param indexIdSet index id set
   */
  private void setStorageList(IndexIdSet indexIdSet)
  {
    clearStorageList();
//TODO: optimize send more than one entry?
    for (Long indexId : indexIdSet)
    {
      setStorageList(indexId);
    }
  }

  /** set/clear checked all storage entries
   * @param checked true for set checked, false for clear checked
   */
  private void setAllCheckedStorage(final boolean checked)
  {
    final int MAX_CONFIRM_ENTRIES = 1000;

    final String[] errorMessage = new String[1];
    ValueMap       valueMap     = new ValueMap();

    clearStorageList();

    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        // tree view
        for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
        {
          uuidTreeItem.setChecked(checked);

          UUIDIndexData uuidIndexData = (UUIDIndexData)uuidTreeItem.getData();
          setStorageList(uuidIndexData.id,checked);

          if (uuidTreeItem.getExpanded())
          {
            for (TreeItem entityTreeItem : uuidTreeItem.getItems())
            {
              entityTreeItem.setChecked(checked);

              EntityIndexData entityIndexData = (EntityIndexData)entityTreeItem.getData();
              setStorageList(entityIndexData.id,checked);

              if (entityTreeItem.getExpanded())
              {
                for (TreeItem storageTreeItem : entityTreeItem.getItems())
                {
                  storageTreeItem.setChecked(checked);

                  StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();
                  setStorageList(storageIndexData.id,checked);
                }
              }
            }
          }
        }

        // trigger update storage
        checkedStorageEvent.trigger();
        break;
      case 1:
        final int     totalEntryCount[] = new int[]{0};
        final boolean doit[]            = new boolean[]{true};

        if (checked)
        {
          if (BARServer.executeCommand(StringParser.format("INDEX_STORAGES_INFO name=%'S indexStateSet=%s",
                                                           updateStorageTreeTableThread.getStorageName(),
                                                           updateStorageTreeTableThread.getStorageIndexStateSet().nameList("|")
                                                          ),
                                       1,  // debugLevel
                                       errorMessage,
                                       valueMap
                                      ) == Errors.NONE
             )
          {
            totalEntryCount[0] = valueMap.getInt("totalEntryCount");
            if (totalEntryCount[0] > MAX_CONFIRM_ENTRIES)
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  doit[0] = Dialogs.confirm(shell,
                                            Dialogs.booleanFieldUpdater(Settings.class,"showEntriesExceededInfo"),
                                            BARControl.tr("There are {0} entries. Really mark all entries?",
                                                          totalEntryCount[0]
                                                         )
                                           );
                }
              });
            }
          }
        }

        if (checked)
        {
          if (doit[0])
          {
            // check/uncheck all entries
            final int n[] = new int[]{0};
            final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Mark entries"),500,100,null,BusyDialog.PROGRESS_BAR0|BusyDialog.ABORT_CLOSE);
            busyDialog.setMaximum(totalEntryCount[0]);
            int error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s indexStateSet=%s indexModeSet=%s name=%'S",
                                                                     "*",
                                                                     updateStorageTreeTableThread.getStorageIndexStateSet().nameList("|"),
                                                                     "*",
                                                                     updateStorageTreeTableThread.getStorageName()
                                                                    ),
                                                 1,  // debugLevel
                                                 errorMessage,
                                                 new CommandResultHandler()
                                                 {
                                                   public int handleResult(int i, ValueMap valueMap)
                                                   {
                                                     long storageId = valueMap.getLong("storageId");

                                                     setStorageList(storageId,checked);

                                                     n[0]++;
                                                     busyDialog.updateProgressBar(n[0]);

                                                     return Errors.NONE;
                                                   }
                                                 }
                                                );
            busyDialog.close();
            if (error != Errors.NONE)
            {
              Dialogs.error(shell,BARControl.tr("Cannot mark all storages!\n\n(error: {0})",errorMessage[0]));
              return;
            }

            // refresh table
            Widgets.refreshVirtualTable(widgetStorageTable);

            // trigger update storage
            checkedStorageEvent.trigger();
          }
        }
        else
        {
          selectedIndexIdSet.clear();

          // refresh table
          Widgets.refreshVirtualTable(widgetStorageTable);

          // trigger update storage
          checkedStorageEvent.trigger();
        }
        break;
    }
  }

  /** get selected storage
   * @return selected index hash set
   */
  private HashSet<IndexData> getSelectedIndexData()
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

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

  /** create entity for job and assing selected/checked job/entity/storage to job
   * @param toUUIDIndexData UUID index data
   * @param archiveType archive type
   */
  private void assignStorage(HashSet<IndexData> indexDataHashSet, UUIDIndexData toUUIDIndexData, Settings.ArchiveTypes archiveType)
  {
    if (!indexDataHashSet.isEmpty())
    {
      long dateTime = 0;
      if (archiveType != Settings.ArchiveTypes.NONE)
      {
        dateTime = Dialogs.date(shell,"New entity date",(String)null,"New");
      }

      try
      {
        int      error        = Errors.UNKNOWN;
        String[] errorMessage = new String[1];
        ValueMap valueMap     = new ValueMap();

        error = BARServer.executeCommand(StringParser.format("INDEX_ENTITY_ADD jobUUID=%'S archiveType=%s createdDateTime=%ld",
                                                             toUUIDIndexData.jobUUID,
                                                             archiveType.toString(),
                                                             dateTime
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
              error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld archiveType=%s jobUUID=%'S",
                                                                   entityId,
                                                                   archiveType.toString(),
                                                                   ((UUIDIndexData)indexData).jobUUID
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            else if (indexData instanceof EntityIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld archiveType=%s entityId=%lld",
                                                                   entityId,
                                                                   archiveType.toString(),
                                                                   indexData.id
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            else if (indexData instanceof StorageIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld archiveType=%s storageId=%lld",
                                                                   entityId,
                                                                   archiveType.toString(),
                                                                   indexData.id
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            if (error != Errors.NONE)
            {
              Dialogs.error(shell,BARControl.tr("Cannot assign index for\n\n''{0}''!\n\n(error: {1})",info,errorMessage[0]));
            }
          }
        }
        else
        {
          Dialogs.error(shell,BARControl.tr("Cannot create entity for\n\n''{0}''!\n\n(error: {1})",toUUIDIndexData.jobUUID,errorMessage[0]));
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
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
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
                                                                 toEntityIndexData.id,
                                                                 ((UUIDIndexData)indexData).jobUUID
                                                                ),
                                             0,  // debugLevel
                                             errorMessage
                                            );
          }
          else if (indexData instanceof EntityIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld entityId=%lld",
                                                                 toEntityIndexData.id,
                                                                 indexData.id
                                                                ),
                                             0,  // debugLevel
                                             errorMessage
                                            );
          }
          else if (indexData instanceof StorageIndexData)
          {
            error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld storageId=%lld",
                                                                 toEntityIndexData.id,
                                                                 indexData.id
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
            Dialogs.error(shell,BARControl.tr("Cannot assign index for\n\n''{0}''!\n\n(error: {1})",info,errorMessage[0]));
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
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
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

            error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toJobUUID=%'S archiveType=%s jobUUID=%'S",
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

            error = BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld archiveType=%s entityId=%lld",
                                                                 entityIndexData.id,
                                                                 archiveType.toString(),
                                                                 entityIndexData.id
                                                                ),
                                             0,  // debugLevel
                                             errorMessage
                                            );
          }
          else if (indexData instanceof StorageIndexData)
          {
            // nothing to do
            error = Errors.NONE;
          }
          if (error == Errors.NONE)
          {
            indexData.setState(IndexStates.UPDATE_REQUESTED);
          }
          else
          {
            Dialogs.error(shell,BARControl.tr("Cannot set entity type for\n\n''{0}''!\n\n(error: {1})",info,errorMessage[0]));
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
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
    setEntityType(indexDataHashSet,archiveType);
  }

  /** refresh storage from index database
   */
  private void refreshStorageIndex()
  {
    try
    {
      HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
      if (!indexDataHashSet.isEmpty())
      {
        int jobCount     = 0;
        int entityCount  = 0;
        int storageCount = 0;
        for (IndexData indexData : indexDataHashSet)
        {
          if      (indexData instanceof UUIDIndexData)
          {
            jobCount++;
          }
          else if (indexData instanceof EntityIndexData)
          {
            entityCount++;
          }
          else if (indexData instanceof StorageIndexData)
          {
            storageCount++;
          }
        }
        if (Dialogs.confirm(shell,BARControl.tr("Refresh index for {0}{0,choice,0#jobs|1#job|1<jobs}/{1}{1,choice,0#entities|1#entity|1<entities}/{2} {2,choice,0#archives|1#archive|1<archives}?",jobCount,entityCount,storageCount)))
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
              error = BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=%s entityId=%lld",
                                                                   "*",
                                                                   indexData.id
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage
                                              );
            }
            else if (indexData instanceof StorageIndexData)
            {
              error = BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=%s storageId=%lld",
                                                                   "*",
                                                                   indexData.id
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
              Dialogs.error(shell,BARControl.tr("Cannot refresh index for\n\n''{0}''!\n\n(error: {1})",info,errorMessage[0]));
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
        int error = BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=%s storageId=%lld",
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
          Dialogs.error(shell,BARControl.tr("Cannot refresh database indizes with error state!\n\n(error: {0})",errorMessage[0]));
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
    final Text widgetStoragePath;
    Button     widgetAdd;

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Add storage to index database"),400,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Storage path")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetStoragePath = Widgets.newText(composite);
      widgetStoragePath.setToolTipText(BARControl.tr("Enter local or remote storage path."));
      Widgets.layout(widgetStoragePath,0,1,TableLayoutData.WE);

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

          String pathName;
          if ((selectionEvent.stateMask & SWT.CTRL) == 0)
          {
            pathName = Dialogs.file(shell,
                                    Dialogs.FileDialogTypes.DIRECTORY,
                                    BARControl.tr("Select storage directory"),
                                    widgetStoragePath.getText(),
                                    BARServer.remoteListDirectory
                                   );
          }
          else
          {
            pathName = Dialogs.directory(shell,
                                        BARControl.tr("Select local storage directory"),
                                        widgetStoragePath.getText()
                                       );
          }
          if (pathName != null)
          {
            widgetStoragePath.setText(pathName);
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
        Dialogs.close(dialog,widgetStoragePath.getText());
      }
    });

    // run dialog
    String storagePath = (String)Dialogs.run(dialog,null);

    // add storage files
    if (storagePath != null)
    {
      final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Add indizes"),500,100,null,BusyDialog.TEXT0|BusyDialog.LIST|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE);

      new BackgroundTask(busyDialog,new Object[]{storagePath})
      {
        @Override
        public void run(final BusyDialog busyDialog, Object userData)
        {
          final String storagePath = (String)((Object[])userData)[0];

          final int[] n = new int[]{0};
          busyDialog.updateText(BARControl.tr("Found archives: {0}",n[0]));

          final String[] errorMessage = new String[1];
          int error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ADD pattern=%'S",
                                                                   new File(storagePath,"*").getPath()
                                                                  ),
                                               0,  // debugLevel
                                               errorMessage,
                                               new CommandResultHandler()
                                               {
                                                 public int handleResult(int i, ValueMap valueMap)
                                                 {
                                                   try
                                                   {
                                                     final long   storageId = valueMap.getLong  ("storageId");
                                                     final String name      = valueMap.getString("name"     );

                                                     n[0]++;
                                                     busyDialog.updateText(BARControl.tr("Found archives: {0}",n[0]));
                                                     busyDialog.updateList(name);
                                                   }
                                                   catch (IllegalArgumentException exception)
                                                   {
                                                     if (Settings.debugLevel > 0)
                                                     {
                                                       System.err.println("ERROR: "+exception.getMessage());
                                                       System.exit(1);
                                                     }
                                                   }

                                                   // check if aborted
                                                   if (busyDialog.isAborted())
                                                   {
                                                     abort();
                                                   }

                                                   return Errors.NONE;
                                                 }
                                               }
                                              );
          busyDialog.close();
          if (error == Errors.NONE)
          {
            updateStorageTreeTableThread.triggerUpdate();
          }
          else
          {
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                Dialogs.error(shell,BARControl.tr("Cannot add index to database for storage path!\n\n''{0}''\n\n(error: {1})",storagePath,errorMessage[0]));
              }
            });
          }
        }
      };
    }
  }

  /** remove storage from index database
   */
  private void removeStorageIndex()
  {
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
    if (!indexDataHashSet.isEmpty())
    {
      int jobCount     = 0;
      int entityCount  = 0;
      int storageCount = 0;
      for (IndexData indexData : indexDataHashSet)
      {
        if      (indexData instanceof UUIDIndexData)
        {
          jobCount++;
        }
        else if (indexData instanceof EntityIndexData)
        {
          entityCount++;
        }
        else if (indexData instanceof StorageIndexData)
        {
          storageCount++;
        }
      }
      if (Dialogs.confirm(shell,BARControl.tr("Remove index with {0} {0,choice,0#jobs|1#job|1<jobs}/{1} {1,choice,0#entities|1#entity|1<entities}/{2} {2,choice,0#archives|1#archive|1<archives}?",jobCount,entityCount,storageCount)))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Remove indizes"),500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE);
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
                  error = BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* uuidId=%lld",
                                                                       indexData.id
                                                                      ),
                                                   0,  // debugLevel
                                                   errorMessage
                                                  );
                }
                else if (indexData instanceof EntityIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* entityId=%lld",
                                                                       indexData.id
                                                                      ),
                                                    0,  // debugLevel
                                                    errorMessage
                                                   );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* storageId=%lld",
                                                                        indexData.id
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
                      Dialogs.error(shell,BARControl.tr("Cannot remove index for\n\n''{0}''!\n\n(error: {1})",info,errorMessage[0]));
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
//TODO: pass error to calelr?
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
            catch (ConnectionError error)
            {
              final String errorMessage = error.getMessage();
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,BARControl.tr("Connection error while removing database indizes\n\n(error: {0})",errorMessage));
                 }
              });
            }
            catch (Throwable throwable)
            {
              BARServer.disconnect();
              System.err.println("ERROR: "+throwable.getMessage());
              BARControl.printStackTrace(throwable);
              System.exit(1);
            }

            // update entry list
            updateEntryTableThread.triggerUpdate();
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
      if (BARServer.executeCommand("INDEX_STORAGES_INFO indexStateSet=ERROR",
                                   1,  // debugLevel
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
            Dialogs.error(shell,BARControl.tr("Cannot get database indizes with error state!\n\n(error: {0})",errorMessage[0]));
          }
        });
        return;
      }
      long errorTotalEntryCount = valueMap.getLong("totalEntryCount");

      if (errorTotalEntryCount > 0)
      {
        if (Dialogs.confirm(shell,BARControl.tr("Remove {0} {0,choice,0#indizes|1#index|1<indizes} with error state?",errorTotalEntryCount)))
        {
          final BusyDialog busyDialog = new BusyDialog(shell,"Remove indizes with error",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE);
          busyDialog.setMaximum(errorTotalEntryCount);

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
                Command command = BARServer.runCommand("INDEX_REMOVE state=ERROR",0);

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
                      System.exit(1);
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
                      Dialogs.error(shell,BARControl.tr("Cannot remove database indizes with error state!\n\n(error: {0})",errorMessage[0]));
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
//TODO: pass to caller?
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
              catch (ConnectionError error)
              {
                final String errorMessage = error.getMessage();
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    busyDialog.close();
                    Dialogs.error(shell,BARControl.tr("Connection error while removing database indizes\n\n(error: {0})",errorMessage));
                   }
                });
              }
              catch (Throwable throwable)
              {
                BARServer.disconnect();
                System.err.println("ERROR: "+throwable.getMessage());
                BARControl.printStackTrace(throwable);
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
/*
    if (!selectedIndexIdSet.isEmpty())
    {
      // set storage achives to restore
      setStorageList(indexIdSet);

      // get total number entries, size
      int  totalEntryCount = 0;
      long totalEntrySize  = 0L;
      if (BARServer.executeCommand(StringParser.format("STORAGE_LIST_INFO"),
                                   1,  // debugLevel
                                   errorMessage,
                                   valueMap
                                  ) == Errors.NONE
         )
      {
        totalEntryCount = valueMap.getLong("totalEntryCount");
        totalEntrySize  = valueMap.getLong("totalEntrySize" );
      }

      // confirm
      if (Dialogs.confirm(shell,BARControl.tr("Delete {0} {0,choice,0#jobs/entities/storage files|1#job/entity/storage file|1<jobs/entities/storage files} with {1} {1,choice,0#entries|1#entry|1<entries}/{2} {2,choice,0#bytes|1#byte|1<bytes}?",indexDataHashSet.size(),totalEntryCount,totalEntrySize)))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,"Delete storage indizes and storage files",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE);
        busyDialog.setMaximum(indexDataHashSet.size());

        new BackgroundTask(busyDialog,new Object[]{selectedIndexIdSet})
        {
          @Override
          public void run(final BusyDialog busyDialog, Object userData)
          {
            IndexIdSet indexIdSet = (IndexIdSet)((Object[])userData)[0];

            try
            {
              boolean ignoreAllErrorsFlag = false;
              boolean abortFlag           = false;
              long    n                   = 0;
Dprintf.dprintf("indexDataHashSet.size=%d",indexDataHashSet.size());
              for (Long indexId : indexIdSet)
              {
                // get index info
                final String info = indexData.getInfo();
Dprintf.dprintf("info=%s",info);

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
                  error = BARServer.executeCommand(StringParser.format("STORAGE_DELETE entityId=%lld",
                                                                       indexData.id
                                                                      ),
                                                   0,  // debugLevel
                                                   errorMessage
                                                  );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  error = BARServer.executeCommand(StringParser.format("STORAGE_DELETE storageId=%lld",
                                                                        indexData.id
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
                          Dialogs.error(shell,BARControl.tr("Cannot delete storage:\n\n''{0}''\n\n(error: {1})",info,errorMessage[0]));
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
//TODO: pass to caller?
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
            catch (ConnectionError error)
            {
              final String errorMessage = error.getMessage();
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,BARControl.tr("Connection error while removing database indizes\n\n(error: {0})",errorMessage));
                 }
              });
            }
            catch (Throwable throwable)
            {
              BARServer.disconnect();
              System.err.println("ERROR: "+throwable.getMessage());
              BARControl.printStackTrace(throwable);
              System.exit(1);
            }
          }
        };
      }
    }
*/
  }

  //-----------------------------------------------------------------------

  /** clear selected entries
   */
  private void clearEntryList()
  {
    BARServer.executeCommand(StringParser.format("ENTRY_LIST_CLEAR"),0);
  }

  /** set/clear selected storage entry
   * @param entryId entry id
   * @param checked true for set checked, false for clear checked
   */
  private void setEntryList(long entryId, boolean checked)
  {
    if (checked)
    {
      BARServer.executeCommand(StringParser.format("ENTRY_LIST_ADD entryId=%ld",
                                                   entryId
                                                  ),
                               0  // debugLevel
                              );
    }
    else
    {
      BARServer.executeCommand(StringParser.format("ENTRY_LIST_REMOVE entryId=%ld",
                                                   entryId
                                                  ),
                               0  // debugLevel
                              );
    }
  }

  /** set selected storage entry
   * @param entryId entry id
   */
  private void setEntryList(long entryId)
  {
    setEntryList(entryId,true);
  }

  /** set selected storage entries
   * @param indexIdSet index id set
   */
  private void setEntryList(IndexIdSet entryIdSet)
  {
    clearEntryList();
//TODO: optimize send more than one entry?
    for (Long entryId : entryIdSet)
    {
      setEntryList(entryId);
    }
  }

  /** set/clear checked all entries
   * @param checked true for set checked, false for clear checked
   */
  private void setAllCheckedEntries(final boolean checked)
  {
    final int MAX_CONFIRM_ENTRIES = 1000;

    final String[] errorMessage = new String[1];
    ValueMap       valueMap     = new ValueMap();

//TODO: use setEntryList

    // confirm check if there are many entries
    final int     totalEntryCount[] = new int[]{0};
    final boolean doit[]            = new boolean[]{true};
    if (checked)
    {
      if (BARServer.executeCommand(StringParser.format("INDEX_ENTRIES_INFO name=%'S indexType=%s newestOnly=%y",
                                                       updateEntryTableThread.getEntryName(),
                                                       updateEntryTableThread.getEntryType().toString(),
                                                       updateEntryTableThread.getNewestOnly()
                                                      ),
                                   0,  // debugLevel
                                   errorMessage,
                                   valueMap
                                  ) == Errors.NONE
         )
      {
        totalEntryCount[0] = valueMap.getInt("totalEntryCount");
        if (totalEntryCount[0] > MAX_CONFIRM_ENTRIES)
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              doit[0] = Dialogs.confirm(shell,
                                        Dialogs.booleanFieldUpdater(Settings.class,"showEntriesExceededInfo"),
                                        BARControl.tr("There are {0} entries. Really mark all entries?",
                                                      totalEntryCount[0]
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
        final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Mark entries"),500,100,null,BusyDialog.PROGRESS_BAR0|BusyDialog.ABORT_CLOSE);
        busyDialog.setMaximum(totalEntryCount[0]);
        int error = BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST name=%'S indexType=%s newestOnly=%y",
                                                                 updateEntryTableThread.getEntryName(),
                                                                 updateEntryTableThread.getEntryType().toString(),
                                                                 updateEntryTableThread.getNewestOnly()
                                                                ),
                                             1,  // debugLevel
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
          Dialogs.error(shell,BARControl.tr("Cannot mark all index entries!\n\n(error: {0})",errorMessage[0]));
          return;
        }

        // refresh table
        Widgets.refreshVirtualTable(widgetEntryTable);

        // trigger update entries
        checkedEntryEvent.trigger();
      }
    }
    else
    {
      selectedEntryIdSet.clear();

      // refresh table
      Widgets.refreshVirtualTable(widgetEntryTable);

      // trigger update entries
      checkedEntryEvent.trigger();
    }
  }

  /** get checked entries
   * @return checked data entries
   */
  private EntryIndexData[] getCheckedEntries()
  {
    ArrayList<EntryIndexData> entryIndexDataArray = new ArrayList<EntryIndexData>();

Dprintf.dprintf("");
    for (TableItem tableItem : widgetEntryTable.getItems())
    {
      if (tableItem.getChecked())
      {
        entryIndexDataArray.add((EntryIndexData)tableItem.getData());
      }
    }

    return entryIndexDataArray.toArray(new EntryIndexData[entryIndexDataArray.size()]);
  }

  /** find index for insert of item in sorted entry data list
   * @param entryIndexData data of tree item
   * @return index in table
   */
  private int findEntryListIndex(EntryIndexData entryIndexData)
  {
Dprintf.dprintf("");
    TableItem                tableItems[]             = widgetEntryTable.getItems();
    EntryIndexDataComparator entryIndexDataComparator = new EntryIndexDataComparator(widgetEntryTable);

    int index = 0;
    while (   (index < tableItems.length)
           && (entryIndexDataComparator.compare(entryIndexData,(EntryIndexData)tableItems[index].getData()) > 0)
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
    EntryIndexDataComparator entryIndexDataComparator = new EntryIndexDataComparator(widgetEntryTable);

    // update
Dprintf.dprintf("");
    widgetEntryTable.removeAll();
/*
    synchronized(entryIndexDataMap)
    {
      for (WeakReference<EntryIndexData> entryIndexDataR : entryIndexDataMap.values())
      {
        EntryIndexData entryIndexData = entryIndexDataR.get();
        switch (entryIndexData.entryType)
        {
          case FILE:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryIndexData),
                                    (Object)entryIndexData,
                                    entryIndexData.storageName,
                                    entryIndexData.name,
                                    "FILE",
                                    Units.formatByteSize(entryIndexData.size),
                                    SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                   );
            break;
          case IMAGE:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryIndexData),
                                    (Object)entryIndexData,
                                    entryIndexData.storageName,
                                    entryIndexData.name,
                                    "IMAGE",
                                    Units.formatByteSize(entryIndexData.size),
                                    SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                   );
            break;
          case DIRECTORY:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryIndexData),
                                    (Object)entryIndexData,
                                    entryIndexData.storageName,
                                    entryIndexData.name,
                                    "DIR",
                                    "",
                                    SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                   );
            break;
          case LINK:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryIndexData),
                                    (Object)entryIndexData,
                                    entryIndexData.storageName,
                                    entryIndexData.name,
                                    "LINK",
                                    "",
                                    SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                   );
            break;
          case SPECIAL:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryIndexData),
                                    (Object)entryIndexData,
                                    entryIndexData.storageName,
                                    entryIndexData.name,
                                    "SPECIAL",
                                    Units.formatByteSize(entryIndexData.size),
                                    SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                   );
            break;
          case DEVICE:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryIndexData),
                                    (Object)entryIndexData,
                                    entryIndexData.storageName,
                                    entryIndexData.name,
                                    "DEVICE",
                                    Units.formatByteSize(entryIndexData.size),
                                    SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                   );
            break;
          case SOCKET:
            Widgets.insertTableItem(widgetEntryTable,
                                    findEntryListIndex(entryIndexData),
                                    (Object)entryIndexData,
                                    entryIndexData.storageName,
                                    entryIndexData.name,
                                    "SOCKET",
                                    "",
                                    SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                   );
            break;
        }

        Widgets.setTableItemChecked(widgetEntryTable,
                                    (Object)entryIndexData,
                                    entryIndexData.isChecked()
                                   );
      }
    }
*/
    // trigger update entries
    checkedEntryEvent.trigger();
  }

  /** restore archives/entries
   * @param restoreType restore type
   * @param indexIdSet index id set
   */
  private void restore(final RestoreTypes restoreType, IndexIdSet indexIdSet)
  {
    /** dialog data
     */
    class Data
    {
      long    totalEntryCount;
      long    totalEntrySize,totalEntryContentSize;
      String  restoreToDirectory;
      boolean directoryContent;
      boolean overwriteEntries;

      Data()
      {
        this.totalEntryCount       = 0;
        this.totalEntrySize        = 0L;
        this.totalEntryContentSize = 0L;
        this.restoreToDirectory    = null;
        this.directoryContent      = false;
        this.overwriteEntries      = false;
      }
    };

    final Data data = new Data();

    String    title = null;
    Label     label;
    Composite composite,subComposite;
    Button    button;

    // create dialog
    switch (restoreType)
    {
      case ARCHIVES: title = BARControl.tr("Restore archives"); break;
      case ENTRIES:  title = BARControl.tr("Restore entries");  break;
    }
    final Shell dialog = Dialogs.openModal(shell,title,600,400,new double[]{1.0,0.0},1.0);

    final WidgetEvent selectRestoreToEvent = new WidgetEvent();

    // create widgets
    final Table  widgetRestoreTable;
    final Label  widgetTotal;
    final Button widgetRestoreTo;
    final Text   widgetRestoreToDirectory;
    final Button widgetDirectoryContent;
    final Button widgetOverwriteEntries;
    final Button widgetRestore;
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(new double[]{0.0,1.0,0.0,0.0,0.0},new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);
    {
      switch (restoreType)
      {
        case ARCHIVES: title = BARControl.tr("Archives"); break;
        case ENTRIES:  title = BARControl.tr("Entries");  break;
      }
      label = Widgets.newLabel(composite,title+":");
      Widgets.layout(label,0,0,TableLayoutData.NW,0,2);
      widgetRestoreTable = Widgets.newTable(composite);
      Widgets.layout(widgetRestoreTable,1,0,TableLayoutData.NSWE,0,2,0,4);
      Widgets.addTableColumn(widgetRestoreTable,0,BARControl.tr("Name"),SWT.LEFT, 430,true);
      switch (restoreType)
      {
        case ARCHIVES: Widgets.addTableColumn(widgetRestoreTable,1,BARControl.tr("Entries"),SWT.RIGHT, 80,true); break;
        case ENTRIES:  Widgets.addTableColumn(widgetRestoreTable,1,BARControl.tr("Type"),   SWT.LEFT, 100,true); break;
      }
      Widgets.addTableColumn(widgetRestoreTable,2,BARControl.tr("Size"),SWT.RIGHT, 60,true);

      label = Widgets.newLabel(composite,BARControl.tr("Total")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);
      widgetTotal = Widgets.newLabel(composite,"-");
      Widgets.layout(widgetTotal,2,1,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0}));
      Widgets.layout(subComposite,3,0,TableLayoutData.WE,0,2);
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

      widgetDirectoryContent = Widgets.newCheckbox(composite,BARControl.tr("Directory content"));
      widgetDirectoryContent.setToolTipText(BARControl.tr("Restore content of selected directories, too."));
      widgetDirectoryContent.setEnabled(restoreType == RestoreTypes.ENTRIES);
      Widgets.layout(widgetDirectoryContent,4,0,TableLayoutData.W,0,2);
      widgetDirectoryContent.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          data.directoryContent = widget.getSelection();
          widgetTotal.setText(BARControl.tr("{0} {0,choice,0#entries|1#entry|1<entries}/{1} ({2} {2,choice,0#bytes|1#byte|1<bytes})",
                                            data.totalEntryCount,
                                            Units.formatByteSize(data.directoryContent ? data.totalEntryContentSize : data.totalEntrySize),
                                            data.directoryContent ? data.totalEntryContentSize : data.totalEntrySize
                                           )
                             );
          widgetTotal.pack();
        }
      });

      widgetOverwriteEntries = Widgets.newCheckbox(composite,BARControl.tr("Overwrite existing entries"));
      widgetOverwriteEntries.setToolTipText(BARControl.tr("Enable this checkbox when existing entries in destination should be overwritten."));
      Widgets.layout(widgetOverwriteEntries,5,0,TableLayoutData.W,0,2);
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      widgetRestore = Widgets.newButton(composite,BARControl.tr("Start restore"));
      widgetRestore.setEnabled(false);
      Widgets.layout(widgetRestore,0,0,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
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

          data.restoreToDirectory = widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : null;
          data.directoryContent   = widgetDirectoryContent.getSelection();
          data.overwriteEntries   = widgetOverwriteEntries.getSelection();

          Dialogs.close(dialog,true);
        }
      });

      button = Widgets.newButton(composite,BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,120,SWT.DEFAULT);
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

    Dialogs.show(dialog);

    // get number/size of archievs/entries to restore
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
          final String[] errorMessage = new String[1];
          ValueMap       valueMap     = new ValueMap();
          switch (restoreType)
          {
            case ARCHIVES:
              // set storage achives to restore
              setStorageList(indexIdSet);

              // get archives
              BARServer.executeCommand(StringParser.format("STORAGE_LIST"),
                                       1,  // debugLevel
                                       errorMessage,
                                       new CommandResultHandler()
                                       {
                                         public int handleResult(int i, ValueMap valueMap)
                                         {
                                           try
                                           {
                                             final long   storageId       = valueMap.getLong  ("storageId"      );
                                             final String name            = valueMap.getString("name"           );
                                             final long   totalEntryCount = valueMap.getLong  ("totalEntryCount");
                                             final long   totalEntrySize  = valueMap.getLong  ("totalEntrySize" );

                                             display.syncExec(new Runnable()
                                             {
                                               public void run()
                                               {
                                                  if (!widgetRestoreTable.isDisposed())
                                                  {
                                                    Widgets.addTableItem(widgetRestoreTable,
                                                                         storageId,
                                                                         name,
                                                                         Long.toString(totalEntryCount),
                                                                         Units.formatByteSize(totalEntrySize)
                                                                        );
                                                  }
                                               }
                                             });
                                           }
                                           catch (IllegalArgumentException exception)
                                           {
                                             if (Settings.debugLevel > 0)
                                             {
                                               System.err.println("ERROR: "+exception.getMessage());
                                               System.exit(1);
                                             }
                                           }

                                           return Errors.NONE;
                                         }
                                       }
                                      );

              // get total number entries, size
              if (BARServer.executeCommand(StringParser.format("STORAGE_LIST_INFO"),
                                           1,  // debugLevel
                                           errorMessage,
                                           valueMap
                                          ) == Errors.NONE
                 )
              {
                data.totalEntryCount       = valueMap.getLong("totalEntryCount"      );
                data.totalEntrySize        = valueMap.getLong("totalEntrySize"       );
                data.totalEntryContentSize = valueMap.getLong("totalEntryContentSize");

                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    if (!widgetTotal.isDisposed())
                    {
                      widgetTotal.setText(BARControl.tr("{0} {0,choice,0#entries|1#entry|1<entries}/{1} ({2} {2,choice,0#bytes|1#byte|1<bytes})",
                                                        data.totalEntryCount,
                                                        Units.formatByteSize(data.totalEntrySize),
                                                        data.totalEntrySize
                                                       )
                                         );
                      widgetTotal.pack();
                    }
                  }
                });
              }
              break;
            case ENTRIES:
              // set entries to restore
              setEntryList(indexIdSet);

              // get entries
              BARServer.executeCommand(StringParser.format("ENTRY_LIST"),
                                       1,  // debugLevel
                                       new CommandResultHandler()
                                       {
                                         public int handleResult(int i, ValueMap valueMap)
                                         {
                                           try
                                           {
                                             final long   entryId = valueMap.getLong  ("entryId");
                                             final String name    = valueMap.getString("name"   );
                                             final String type    = valueMap.getString("type"   );
                                             final long   size    = valueMap.getLong  ("size"   );

                                             display.syncExec(new Runnable()
                                             {
                                               public void run()
                                               {
                                                  if (!widgetRestoreTable.isDisposed())
                                                  {
                                                    Widgets.addTableItem(widgetRestoreTable,
                                                                         entryId,
                                                                         name,
                                                                         type,
                                                                         (size > 0L) ? Units.formatByteSize(size) : ""
                                                                        );
                                                  }
                                               }
                                             });
                                           }
                                           catch (IllegalArgumentException exception)
                                           {
                                             if (Settings.debugLevel > 0)
                                             {
                                               System.err.println("ERROR: "+exception.getMessage());
                                               System.exit(1);
                                             }
                                           }

                                           return Errors.NONE;
                                         }
                                       }
                                      );

              // get total number entries, size
              if (BARServer.executeCommand(StringParser.format("ENTRY_LIST_INFO"),
                                           1,  // debugLevel
                                           errorMessage,
                                           valueMap
                                          ) == Errors.NONE
                 )
              {
                data.totalEntryCount       = valueMap.getLong("totalEntryCount");
                data.totalEntrySize        = valueMap.getLong("totalEntrySize");
                data.totalEntryContentSize = valueMap.getLong("totalEntryContentSize");
//Dprintf.dprintf("data.totalEntrySize=%d data.totalEntryContentSize=%d",data.totalEntrySize,data.totalEntryContentSize);

                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    if (!widgetTotal.isDisposed())
                    {
                      widgetTotal.setText(BARControl.tr("{0} {0,choice,0#entries|1#entry|1<entries}/{1} ({2} {2,choice,0#bytes|1#byte|1<bytes})",
                                                        data.totalEntryCount,
                                                        Units.formatByteSize(data.directoryContent ? data.totalEntryContentSize : data.totalEntrySize),
                                                        data.directoryContent ? data.totalEntryContentSize : data.totalEntrySize
                                                       )
                                         );
                      widgetTotal.pack();
                    }
                  }
                });
              }
              break;
          }

          display.syncExec(new Runnable()
          {
            public void run()
            {
              if (!widgetRestore.isDisposed())
              {
                widgetRestore.setEnabled(!indexIdSet.isEmpty());
              }
            }
          });
        }
        catch (IllegalArgumentException exception)
        {
          if (Settings.debugLevel > 0)
          {
            System.err.println("ERROR: "+exception.getMessage());
            System.exit(1);
          }
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

    // run dialog
    if ((Boolean)Dialogs.run(dialog,false))
    {
      switch (restoreType)
      {
        case ARCHIVES: title = (data.restoreToDirectory != null) ? BARControl.tr("Restore archives to: {0}",data.restoreToDirectory) : BARControl.tr("Restore archives"); break;
        case ENTRIES:  title = (data.restoreToDirectory != null) ? BARControl.tr("Restore entries to: {0}",data.restoreToDirectory) : BARControl.tr("Restore entries");  break;
      }
      final BusyDialog busyDialog = new BusyDialog(shell,
                                                   title,
                                                   500,
                                                   300,
                                                   null,
                                                   BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.PROGRESS_BAR0|BusyDialog.PROGRESS_BAR1|BusyDialog.LIST|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE,
                                                   250  // max. lines
                                                  );
      busyDialog.updateText(2,"%s",BARControl.tr("Failed entries")+":");

      new BackgroundTask(busyDialog,new Object[]{indexIdSet,data.restoreToDirectory,data.directoryContent,data.overwriteEntries})
      {
        @Override
        public void run(final BusyDialog busyDialog, Object userData)
        {
          final IndexIdSet indexIdSet         = (IndexIdSet)((Object[])userData)[0];
          final String     restoreToDirectory = (String    )((Object[])userData)[1];
          final boolean    directoryContent   = (Boolean   )((Object[])userData)[2];
          final boolean    overwriteEntries   = (Boolean   )((Object[])userData)[3];

          int errorCode;

          // restore archives
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
            final long     errorCount[]  = new long[]{0};
            final boolean  skipAllFlag[] = new boolean[]{false};
            final String[] errorMessage  = new String[1];

            // set archives/entries to restore
            switch (restoreType)
            {
              case ARCHIVES:
                setStorageList(indexIdSet);
                break;
              case ENTRIES:
                setEntryList(indexIdSet);
                break;
            }

            // start restore
            String command = null;
            switch (restoreType)
            {
              case ARCHIVES:
                command = StringParser.format("RESTORE type=ARCHIVES destination=%'S overwriteEntries=%y",
                                              restoreToDirectory,
                                              overwriteEntries
                                             );
                break;
              case ENTRIES:
                command = StringParser.format("RESTORE type=ENTRIES destination=%'S directoryContent=%y overwriteEntries=%y",
                                              restoreToDirectory,
                                              directoryContent,
                                              overwriteEntries
                                             );
                break;
            }
            int error = BARServer.executeCommand(command,
                                                 0,  // debugLevel
                                                 errorMessage,
                                                 new CommandResultHandler()
                                                 {
                                                   public int handleResult(int i, ValueMap valueMap)
                                                   {
                                                     // parse and update progresss
                                                     try
                                                     {
                                                       if (valueMap.containsKey("action"))
                                                       {
                                                         Actions             action       = valueMap.getEnum  ("action",Actions.class);
                                                         final String        name         = valueMap.getString("name","");
                                                         final PasswordTypes passwordType = valueMap.getEnum  ("passwordType",PasswordTypes.class,PasswordTypes.NONE);
                                                         final String        passwordText = valueMap.getString("passwordText","");
                                                         final String        volume       = valueMap.getString("volume","");
                                                         final int           error        = valueMap.getInt   ("error",Errors.NONE);
                                                         final String        errorMessage = valueMap.getString("errorMessage","");
                                                         final String        storage      = valueMap.getString("storage","");
                                                         final String        entry        = valueMap.getString("entry","");

                                                         switch (action)
                                                         {
                                                           case REQUEST_PASSWORD:
                                                             // get password
                                                             display.syncExec(new Runnable()
                                                             {
                                                               @Override
                                                               public void run()
                                                               {
                                                                 if (passwordType.isLogin())
                                                                 {
                                                                   String[] data = Dialogs.login(shell,
                                                                                                 BARControl.tr("{0} login password",passwordType),
                                                                                                 BARControl.tr("Please enter {0} login for: {1}",passwordType,passwordText),
                                                                                                 name,
                                                                                                 BARControl.tr("Password")+":"
                                                                                                );
                                                                   if (data != null)
                                                                   {
                                                                     BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d name=%S encryptType=%s encryptedPassword=%S",
                                                                                                                  Errors.NONE,
                                                                                                                  data[0],
                                                                                                                  BARServer.getPasswordEncryptType(),
                                                                                                                  BARServer.encryptPassword(data[1])
                                                                                                                 ),
                                                                                              0  // debugLevel
                                                                                             );
                                                                   }
                                                                   else
                                                                   {
                                                                     BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d",
                                                                                                                  Errors.NO_PASSWORD
                                                                                                                 ),
                                                                                              0  // debugLevel
                                                                                             );
                                                                   }
                                                                 }
                                                                 else
                                                                 {
                                                                   String password = Dialogs.password(shell,
                                                                                                      BARControl.tr("{0} login password",passwordType),
                                                                                                      BARControl.tr("Please enter {0} password for: {1}",passwordType,passwordText),
                                                                                                      BARControl.tr("Password")+":"
                                                                                                     );
                                                                   if (password != null)
                                                                   {
                                                                     BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d encryptType=%s encryptedPassword=%S",
                                                                                                                  Errors.NONE,
                                                                                                                  BARServer.getPasswordEncryptType(),
                                                                                                                  BARServer.encryptPassword(password)
                                                                                                                 ),
                                                                                              0  // debugLevel
                                                                                             );
                                                                   }
                                                                   else
                                                                   {
                                                                     BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d",
                                                                                                                  Errors.NO_PASSWORD
                                                                                                                 ),
                                                                                              0  // debugLevel
                                                                                             );
                                                                   }
                                                                 }
                                                               }
                                                             });
                                                             break;
                                                           case REQUEST_VOLUME:
Dprintf.dprintf("REQUEST_VOLUME");
System.exit(1);
                                                             break;
                                                           case CONFIRM:
                                                             busyDialog.updateList(!entry.isEmpty() ? entry : storage);
                                                             errorCount[0]++;

                                                             final int resultError[] = new int[]{error};
                                                             if (!skipAllFlag[0])
                                                             {
                                                               display.syncExec(new Runnable()
                                                               {
                                                                 @Override
                                                                 public void run()
                                                                 {
                                                                   switch (Dialogs.select(shell,
                                                                                          BARControl.tr("Confirmation"),
                                                                                          BARControl.tr("Cannot restore:\n\n {0}\n\nReason: {1}",
                                                                                                        !entry.isEmpty() ? entry : storage,
                                                                                                        errorMessage
                                                                                                       ),
                                                                                          new String[]{BARControl.tr("Skip"),BARControl.tr("Skip all"),BARControl.tr("Abort")},
                                                                                          0
                                                                                         )
                                                                          )
                                                                   {
                                                                     case 0:
                                                                       resultError[0] = Errors.NONE;
                                                                       break;
                                                                     case 1:
                                                                       resultError[0] = Errors.NONE;
                                                                       skipAllFlag[0] = true;
                                                                       break;
                                                                     case 2:
                                                                       abort();
                                                                       break;
                                                                   }
                                                                 }
                                                               });
                                                             }
                                                             else
                                                             {
                                                               resultError[0] = Errors.NONE;
                                                             }
                                                             BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d",
                                                                                                          resultError[0]
                                                                                                         ),
                                                                                      0  // debugLevel
                                                                                     );
                                                             break;
                                                         }
                                                       }
                                                       else
                                                       {
//TODO
Dprintf.dprintf("valueMap=%s",valueMap);
                                                         RestoreStates state            = valueMap.getEnum  ("state",RestoreStates.class);
                                                         String        storageName      = valueMap.getString("storageName");
                                                         long          storageDoneSize  = valueMap.getLong  ("storageDoneSize");
                                                         long          storageTotalSize = valueMap.getLong  ("storageTotalSize");
                                                         String        entryName        = valueMap.getString("entryName");
                                                         long          entryDoneSize    = valueMap.getLong  ("entryDoneSize");
                                                         long          entryTotalSize   = valueMap.getLong  ("entryTotalSize");

                                                         busyDialog.updateText(0,"%s",storageName);
                                                         busyDialog.updateProgressBar(0,(storageTotalSize > 0) ? ((double)storageDoneSize*100.0)/(double)storageTotalSize : 0.0);
                                                         busyDialog.updateText(1,"%s",new File(restoreToDirectory,entryName).getPath());
                                                         busyDialog.updateProgressBar(1,(entryTotalSize > 0) ? ((double)entryDoneSize*100.0)/(double)entryTotalSize : 0.0);
                                                       }
                                                     }
                                                     catch (IllegalArgumentException exception)
                                                     {
                                                       if (Settings.debugLevel > 0)
                                                       {
                                                         System.err.println("ERROR: "+exception.getMessage());
                                                         System.exit(1);
                                                       }
                                                     }

                                                     if (busyDialog.isAborted())
                                                     {
                                                       busyDialog.updateText(0,"%s",BARControl.tr("Aborting")+"\u2026");
                                                       busyDialog.updateText(1,"");
                                                       abort();
                                                     }

                                                     return Errors.NONE;
                                                   }
                                                 }
                                                );

            if ((error != Errors.NONE) && (error != Errors.ABORTED))
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  Dialogs.error(shell,BARControl.tr("Cannot restore!\n\n(error: {0})",errorMessage[0]));
                }
              });
              busyDialog.close();
              return;
            }

            // close/done busy dialog, restore cursor
            if (errorCount[0] > 0)
            {
              busyDialog.done();
            }
            else
            {
              busyDialog.close();
            }
          }
//TODO: pass to caller?
          catch (CommunicationError error)
          {
            final String errorMessage = error.getMessage();
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                busyDialog.close();
                Dialogs.error(shell,BARControl.tr("Error while restoring:\n\n{0}",errorMessage));
               }
            });
          }
          catch (ConnectionError error)
          {
            final String errorMessage = error.getMessage();
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                busyDialog.close();
                Dialogs.error(shell,BARControl.tr("Connection error while restoring\n\n(error: {0})",errorMessage));
               }
            });
          }
          catch (Throwable throwable)
          {
            BARServer.disconnect();
            System.err.println("ERROR: "+throwable.getMessage());
            BARControl.printStackTrace(throwable);
            System.exit(1);
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
      };
    }
  }
}

/* end of file */
