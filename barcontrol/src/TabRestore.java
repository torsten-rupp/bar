/***********************************************************************\
*
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

import java.text.ParseException;
import java.text.SimpleDateFormat;

import java.util.ArrayDeque;
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
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;

// graphics
import org.eclipse.swt.custom.TreeEditor;
import org.eclipse.swt.dnd.ByteArrayTransfer;
import org.eclipse.swt.dnd.Clipboard;
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
import org.eclipse.swt.graphics.Font;
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

    /** get all states
     * @return all states
     */
    public static EnumSet<IndexStates> ALL()
    {
      return EnumSet.of(IndexStates.OK,IndexStates.ERROR);
    }

    /** convert data to string
     * @return string
     */
    @Override
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
     * @param separator separator string
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
    @Override
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
    @Override
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
  class IndexData implements Serializable, Comparable<IndexData>
  {
    /** tree item update runnable
     */
    abstract class TreeItemUpdateRunnable
    {
      protected abstract void update(TreeItem treeItem, IndexData indexData);
    }
    /** table item update runnable
     */
    abstract class TableItemUpdateRunnable
    {
      protected abstract void update(TableItem tableItem, IndexData indexData);
    }
    /** menu item update runnable
     */
    abstract class MenuItemUpdateRunnable
    {
      protected abstract void update(MenuItem menuItem, IndexData indexData);
    }

    public long                     id;

    private TreeItemUpdateRunnable  treeItemUpdateRunnable;
    private TableItemUpdateRunnable tableItemUpdateRunnable;
    private Menu                    subMenu;                  // reference sub-menu or null
    private MenuItem                menuItem;                 // reference menu item or null
    private MenuItemUpdateRunnable  menuItemUpdateRunnable;

    /** create index data
     * @param indexId index id
     */
    IndexData(long indexId)
    {
      this.id       = indexId;
      this.subMenu  = null;
      this.menuItem = null;
    }

    /** check if index data equals
     * @param object index data
     * @return true iff equals
     */
// TODO: requried? use comparable?
    @Override
    public boolean equals(Object object)
    {
      IndexData indexData = (IndexData)object;

      return (indexData != null) && (id == indexData.id);
    }

    /** compare with other index data
     * @return -1/0+1 iff lower/equals/greater
     */
    @Override
    public int compareTo(IndexData other)
    {
      if      (id < other.id) return -1;
      else if (id > other.id) return  1;
      else                    return  0;
    }

    /** get sub-menu reference
     * @return sub-menu
     */
    protected Menu getSubMenu()
    {
      return this.subMenu;
    }

    /** set sub-menu reference
     * @param setSubMenu sub-menu
     */
    protected void setSubMenu(Menu subMenu)
    {
      this.subMenu = subMenu;
    }

    /** clear sub-menu reference
     */
    protected void clearSubMenu()
    {
      this.subMenu = null;
    }

    /** set menu item reference
     * @param menuItem menu item
     * @param menuItemUpdateRunnable menu item update runnable
     */
    protected void setMenuItem(MenuItem menuItem, MenuItemUpdateRunnable menuItemUpdateRunnable)
    {
      this.menuItem               = menuItem;
      this.menuItemUpdateRunnable = menuItemUpdateRunnable;
    }

    /** clear table item reference
     */
    protected void clearMenuItem()
    {
      this.menuItem = null;
    }

    /** update from other index data
     * @param otherIndexData other index data
     */
    protected void update(IndexData otherIndexData)
    {
      this.id = otherIndexData.id;
    }

    /** get name
     * @return name
     */
    protected String getName()
    {
      return "";
    }

    /** get date/time
     * @return date/time [s]
     */
    protected long getDateTime()
    {
      return 0;
    }

    /** get total size
     * @return size [bytes]
     */
    protected long getTotalSize()
    {
      return 0;
    }

    /** get total number of entries
     * @return total entries
     */
    protected long getTotalEntryCount()
    {
      return 0;
    }

    /** get total size of entries
     * @return total entries size
     */
    protected long getTotalEntrySize()
    {
      return 0;
    }

    /** get index state
     * @return index state
     */
    protected IndexStates getState()
    {
      return IndexStates.OK;
    }

    /** set index state
     * @param indexState index state
     */
    protected void setState(IndexStates indexState)
    {
      // nothing to do
    }

    /** get info string
     * @return info string
     */
    protected String getInfo()
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
    @Override
    public String toString()
    {
      return "Index {"+id+"}";
    }
  };

  /** index data comparator
   */
  static class IndexDataComparator<T extends IndexData> implements Comparator<T>
  {
    // sort modes
    enum SortModes
    {
      NAME,
      HOSTNAME,
      CREATED_DATETIME,
      SIZE,
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
      else if (tree.getColumn(1) == sortColumn) this.sortMode = SortModes.HOSTNAME;
      else if (tree.getColumn(2) == sortColumn) this.sortMode = SortModes.CREATED_DATETIME;
      else if (tree.getColumn(3) == sortColumn) this.sortMode = SortModes.SIZE;
      else if (tree.getColumn(4) == sortColumn) this.sortMode = SortModes.STATE;
      else                                      this.sortMode = SortModes.NAME;
    }

    /** create index data comparator
     * @param tree tree
     */
    IndexDataComparator(Tree tree)
    {
      this(tree,tree.getSortColumn());
    }

    /** create index data comparator
     * @param table table
     * @param sortColumn sort column
     */
    IndexDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) this.sortMode = SortModes.NAME;
      else if (table.getColumn(1) == sortColumn) this.sortMode = SortModes.HOSTNAME;
      else if (table.getColumn(2) == sortColumn) this.sortMode = SortModes.CREATED_DATETIME;
      else if (table.getColumn(3) == sortColumn) this.sortMode = SortModes.SIZE;
      else if (table.getColumn(4) == sortColumn) this.sortMode = SortModes.STATE;
      else                                       this.sortMode = SortModes.NAME;
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
    static <T extends IndexData> IndexDataComparator<T> getInstance(final Tree tree)
    {
      final IndexDataComparator<T> indexDataComparator[] = new IndexDataComparator[1];
      tree.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          if (!tree.isDisposed())
          {
            indexDataComparator[0] = new IndexDataComparator(tree);
          }
        }
      });

      return indexDataComparator[0];
    }

    /** get index data comparator instance
     * @param table table widget
     * @return index data comparator
     */
    static <T extends IndexData> IndexDataComparator<T> getInstance(final Table table)
    {
      final IndexDataComparator indexDataComparator[] = new IndexDataComparator[1];
      table.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          indexDataComparator[0] = new IndexDataComparator<T>(table);
        }
      });

      return indexDataComparator[0];
    }

    /** get index data comparator instance
     * @param tree tree widget
     * @return index data comparator
     */
    static <T extends IndexData> IndexDataComparator<T> getInstance()
    {
      return new IndexDataComparator<T>();
    }

    /** compare index data
     * @param indexData1, indexData2 index data to compare
     * @return -1 iff indexData1.id != indexData2.id && indexData1 < indexData2,
                0 iff indexData1.id == indexData2.id || indexData1 = indexData2,
                1 iff indexData1.id != indexData2.id && indexData1 > indexData2
     */
    @Override
    public int compare(T indexData1, T indexData2)
    {
      SortModes nextSortMode = sortMode;
      int       result = 0;

      if ((indexData1 == null) && (indexData2 == null))
      {
        result = 0;
      }
      else if (indexData1 == null)
      {
        result = 1;
      }
      else if (indexData2 == null)
      {
        result = -1;
      }
      else
      {
        result = Long.valueOf(indexData1.id).compareTo(indexData2.id);
        if (result != 0)
        {
          boolean doneFlag = false;
          do
          {
            switch (nextSortMode)
            {
              case NAME:
                String name1 = indexData1.getName();
                String name2 = indexData2.getName();
                result = name1.compareTo(name2);
                nextSortMode = SortModes.SIZE;
                break;
              case CREATED_DATETIME:
                long date1 = indexData1.getDateTime();
                long date2 = indexData2.getDateTime();
                result = Long.valueOf(date1).compareTo(date2);
                nextSortMode = SortModes.STATE;
                break;
              case SIZE:
                long size1 = indexData1.getTotalSize();
                long size2 = indexData2.getTotalSize();
                result = Long.valueOf(size1).compareTo(size2);
                nextSortMode = SortModes.CREATED_DATETIME;
                break;
              case STATE:
                IndexStates indexState1 = indexData1.getState();
                IndexStates indexState2 = indexData2.getState();
                result = indexState1.compareTo(indexState2);
                doneFlag = true;
                break;
              default:
                doneFlag = true;
                break;
            }
          }
          while ((result == 0) && !doneFlag);
        }
      }

      return result;
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      return "IndexDataComparator {"+sortMode+"}";
    }
  }

  /** index data set
   */
  class IndexIdSet extends HashSet<Long>
  {
    /** add/remove id
     * @param indexId index id
     * @param enabled true to add, false to remove
     */
    public void set(long indexId, boolean enabled)
    {
      if (enabled)
      {
        add(indexId);
      }
      else
      {
        remove(indexId);
      }
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      StringBuilder buffer = new StringBuilder();
      for (Long indexId : this)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(indexId);
      }

      return "IndexIdSet {"+buffer.toString()+"}";
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

      HashSet<IndexData> indexDataHashSet = (HashSet<IndexData>)object;
      try
      {
        // write data to a byte array and then ask super to convert to pMedium
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        ObjectOutputStream outputStream = new ObjectOutputStream(byteArrayOutputStream);
        outputStream.writeObject(indexDataHashSet);
        byte[] buffer = byteArrayOutputStream.toByteArray();
        outputStream.close();

        // call super to convert to pMedium
        super.javaToNative(buffer,transferData);
      }
      catch (IOException exception)
      {
        // ignored
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
        HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();
        try
        {
          ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream(buffer);
          ObjectInputStream inputStream = new ObjectInputStream(byteArrayInputStream);
          indexDataHashSet = (HashSet<IndexData>)inputStream.readObject();
          inputStream.close ();
        }
        catch (java.lang.ClassNotFoundException exception)
        {
          // ignored
          if (Settings.debugLevel > 0)
          {
            BARControl.printStackTrace(exception);
          }
          return null;
        }
        catch (IOException exception)
        {
          // ignored
          if (Settings.debugLevel > 0)
          {
            BARControl.printStackTrace(exception);
          }
          return null;
        }

        return indexDataHashSet;
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
      return     (object != null)
              && (object instanceof HashSet);
    }
  }

  /** UUID index data
   */
  class UUIDIndexData extends IndexData
  {
    public String  jobUUID;                       // job UUID
    public String  scheduleUUID;                  // schedule UUID
    public String  name;
    public long    lastExecutedDateTime;          // last executed date/time stamp [s]
    public int     lastErrorCode;                 // last error code
    public String  lastErrorData;                 // last error data
    public long    totalSize;
    public long    totalEntryCount;
    public long    totalEntrySize;

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      protected void update(TreeItem treeItem, IndexData indexData)
      {
        UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)uuidIndexData,
                               uuidIndexData.name,
                               "",  // date/time drawn in event handler
                               Units.formatByteSize(uuidIndexData.totalSize),
                               ""
                              );
      }
    };

    private final MenuItemUpdateRunnable menuItemUpdateRunnable = new MenuItemUpdateRunnable()
    {
      protected void update(MenuItem menuItem, IndexData indexData)
      {
        UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

        menuItem.setText(uuidIndexData.name);
      }
    };

    /** create UUID index data
     * @param indexId index id
     * @param jobUUID job uuid
     * @param scheduleUUID schedule uuid
     * @param name job name
     * @param lastExecutedDateTime last executed date/time tamp [s]
     * @param lastErrorCOde last error code
     * @param lastErrorData last error message data
     * @param totalSize total size [byte]
     * @param totalEntryCount total number of entries of storage
     * @param totalEntrySize total suf of entry sizes
     */
    UUIDIndexData(long   indexId,
                  String jobUUID,
                  String scheduleUUID,
                  String name,
                  long   lastExecutedDateTime,
                  int    lastErrorCode,
                  String lastErrorData,
                  long   totalSize,
                  long   totalEntryCount,
                  long   totalEntrySize
                 )
    {
      super(indexId);

      assert (indexId == 0) || ((indexId & 0x0000000F) == 1) : indexId;
      assert(totalSize >= 0);
      assert(totalEntryCount >= 0);
      assert(totalEntrySize >= 0);

      this.jobUUID              = jobUUID;
      this.scheduleUUID         = scheduleUUID;
      this.name                 = name;
      this.lastExecutedDateTime = lastExecutedDateTime;
      this.lastErrorCode        = lastErrorCode;
      this.lastErrorData        = lastErrorData;
      this.totalSize            = totalSize;
      this.totalEntryCount      = totalEntryCount;
      this.totalEntrySize       = totalEntrySize;
    }

    /** create UUID data index
     * @param indexId index id
     * @param jobUUID job uuid
     * @param name job name
     * @param lastExecutedDateTime last executed date/time tamp [s]
     * @param lastErrorCOde last error code
     * @param lastErrorData last error message data
     * @param totalSize total size [byte]
     * @param totalEntryCount total number of entries of storage
     * @param totalEntrySize total suf of entry sizes
     */
    UUIDIndexData(long   indexId,
                  String jobUUID,
                  String name,
                  long   lastExecutedDateTime,
                  int    lastErrorCode,
                  String lastErrorData,
                  long   totalSize,
                  long   totalEntryCount,
                  long   totalEntrySize
                 )
    {
      this(indexId,
           jobUUID,
           (String)null, // scheduleUUID
           name,
           lastExecutedDateTime,
           lastErrorCode,
           lastErrorData,
           totalSize,
           totalEntryCount,
           totalEntrySize
          );
    }

    /** update from other index data
     * @param otherIndexData other index data
     */
    protected void update(UUIDIndexData otherIndexData)
    {
      super.update(otherIndexData);
      this.jobUUID              = otherIndexData.jobUUID;
      this.scheduleUUID         = otherIndexData.scheduleUUID;
      this.name                 = otherIndexData.name;
      this.lastExecutedDateTime = otherIndexData.lastExecutedDateTime;
      this.lastErrorCode        = otherIndexData.lastErrorCode;
      this.lastErrorData        = otherIndexData.lastErrorData;
      this.totalSize            = otherIndexData.totalSize;
      this.totalEntryCount      = otherIndexData.totalEntryCount;
      this.totalEntrySize       = otherIndexData.totalEntrySize;
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
      return lastExecutedDateTime;
    }

    /** get total size
     * @return size [bytes]
     */
    @Override
    public long getTotalSize()
    {
      return totalSize;
    }

    /** get total number of entries
     * @return entries
     */
    @Override
    public long getTotalEntryCount()
    {
      return totalEntryCount;
    }

    /** get total size of entries
     * @return total entries size
     */
    public long getTotalEntrySize()
    {
      return totalEntrySize;
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
      return String.format("#%d '%s'",id,!name.isEmpty() ? name : "unknown");
    }

    /** compare index data
     * @param uuidIndexData index data
     * @return -1/0/1 if less/equals/greater
     */
    public int compareTo(UUIDIndexData uuidIndexData)
    {
      int result;

      result = name.compareTo(uuidIndexData.name);
      if (result == 0)
      {
        result = jobUUID.compareTo(uuidIndexData.jobUUID);
        if (result == 0)
        {
          result = scheduleUUID.compareTo(uuidIndexData.scheduleUUID);
        }
      }

      return result;
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      return "UUIDIndexData {"+id+", jobUUID="+jobUUID+", scheduleUUID="+scheduleUUID+", name="+name+", lastExecutedDateTime="+lastExecutedDateTime+", totalEntryCount="+totalEntryCount+", totalEntrySize="+totalEntrySize+" bytes}";
    }
  }

  /** entity index data
   */
  class EntityIndexData extends IndexData
  {
    public String       jobUUID;
    public String       scheduleUUID;
    public ArchiveTypes archiveType;
    public long         createdDateTime;
    public int          lastErrorCode;        // last error code
    public String       lastErrorData;        // last error data
    public long         totalSize;
    public long         totalEntryCount;
    public long         totalEntrySize;
    public long         expireDateTime;       // expire date/time or 0

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      protected void update(TreeItem treeItem, IndexData indexData)
      {
        EntityIndexData entityIndexData = (EntityIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)entityIndexData,
                               entityIndexData.archiveType.toString(),
                               "",  // date/time drawn in event handler
                               Units.formatByteSize(entityIndexData.totalSize),
                               ""
                              );
      }
    };

    private final MenuItemUpdateRunnable menuItemUpdateRunnable = new MenuItemUpdateRunnable()
    {
      protected void update(MenuItem menuItem, IndexData indexData)
      {
        EntityIndexData entityIndexData = (EntityIndexData)indexData;

Dprintf.dprintf("");
//        menuItem.setText(entityIndexData.name);
      }
    };

    /** create entity index data
     * @param indexId index id
     * @param jobUUID job uuid
     * @param scheduleUUID schedule uuid
     * @param archiveType archive type
     * @param createdDateTime create date/time (timestamp)
     * @param lastErrorCode last error message code
     * @param lastErrorData last error message data
     * @param totalSize total size [byte]
     * @param totalEntryCount total number of entresi of storage
     * @param totalEntrySize total suf of entry sizes
     * @param expireDateTime expire date/time (timestamp)
     */
    EntityIndexData(long         indexId,
                    String       jobUUID,
                    String       scheduleUUID,
                    ArchiveTypes archiveType,
                    long         createdDateTime,
                    int          lastErrorCode,
                    String       lastErrorData,
                    long         totalSize,
                    long         totalEntryCount,
                    long         totalEntrySize,
                    long         expireDateTime
                   )
    {
      super(indexId);

      assert (indexId & 0x0000000F) == 2 : indexId;
      assert(totalEntryCount >= 0);
      assert(totalEntrySize >= 0);

      this.jobUUID          = jobUUID;
      this.scheduleUUID     = scheduleUUID;
      this.archiveType      = archiveType;
      this.createdDateTime  = createdDateTime;
      this.lastErrorCode    = lastErrorCode;
      this.lastErrorData    = lastErrorData;
      this.totalSize        = totalSize;
      this.totalEntryCount  = totalEntryCount;
      this.totalEntrySize   = totalEntrySize;
      this.expireDateTime   = expireDateTime;
    }

    /** compare index data
     * @param entityIndexData index data
     * @return -1/0/1 if less/equals/greater
     */
    public int compareTo(EntityIndexData entityIndexData)
    {
// TODO remove
//      EntityIndexData entityIndexData = (EntityIndexData)object;
      int             result;

      result = jobUUID.compareTo(entityIndexData.jobUUID);
      if (result == 0)
      {
        result = scheduleUUID.compareTo(entityIndexData.scheduleUUID);
      }

      return result;
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
      return createdDateTime;
    }

    /** get total size
     * @return size [bytes]
     */
    @Override
    public long getTotalSize()
    {
      return totalSize;
    }

    /** get total number of entries
     * @return entries
     */
    @Override
    public long getTotalEntryCount()
    {
      return totalEntryCount;
    }

    /** get total size of entries
     * @return total entries size
     */
    public long getTotalEntrySize()
    {
      return totalEntrySize;
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
      return String.format("#%d %s",id,archiveType.toString());
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
      out.writeObject(createdDateTime);
      out.writeObject(lastErrorCode);
      out.writeObject(lastErrorData);
      out.writeObject(totalSize);
      out.writeObject(totalEntryCount);
      out.writeObject(totalEntrySize);
      out.writeObject(expireDateTime);
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
      jobUUID          = (String)in.readObject();
      scheduleUUID     = (String)in.readObject();
      archiveType      = (ArchiveTypes)in.readObject();
      createdDateTime  = (Long)in.readObject();
      lastErrorCode    = (Integer)in.readObject();
      lastErrorData    = (String)in.readObject();
      totalSize        = (Long)in.readObject();
      totalEntryCount  = (Long)in.readObject();
      totalEntrySize   = (Long)in.readObject();
      expireDateTime   = (Long)in.readObject();
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      return "EntityIndexData {"+id+", archiveType="+archiveType.toString()+", jobUUID="+jobUUID+", scheduleUUID="+scheduleUUID+", createdDateTime="+createdDateTime+", totalSize="+totalSize+" bytes, totalEntrySize="+totalEntrySize+" bytes, expireDateTime="+expireDateTime+"}";
    }
  }

  /** storage index data
   */
  class StorageIndexData extends IndexData implements Serializable
  {
    public  String       jobUUID;                  // job UUID
    public  String       jobName;                  // job name or null
    public  long         entityId;                 // entiy index id
    public  String       hostName;                 // host name
    public  long         createdDateTime;          // date/time when some entty was created
    public  ArchiveTypes archiveType;              // archive type
    public  String       name;                     // name
    public  long         dateTime;                 // date/time when some storage was created
    private long         size;                     // storage size [bytes]
    public  IndexStates  indexState;               // state of index
    public  IndexModes   indexMode;                // mode of index
    public  long         lastCheckedDateTime;      // last checked date/time
    public  String       errorMessage;             // last error message

    private long         totalEntryCount;
    private long         totalEntrySize;

    private final TreeItemUpdateRunnable treeItemUpdateRunnable = new TreeItemUpdateRunnable()
    {
      protected void update(TreeItem treeItem, IndexData indexData)
      {
        StorageIndexData storageIndexData = (StorageIndexData)indexData;

        Widgets.updateTreeItem(treeItem,
                               (Object)storageIndexData,
                               storageIndexData.name,
                               "",  // date/time drawn in event handler
                               Units.formatByteSize(storageIndexData.size),
                               storageIndexData.indexState.toString()
                              );
      }
    };

    private final TableItemUpdateRunnable tableItemUpdateRunnable = new TableItemUpdateRunnable()
    {
      protected void update(TableItem tableItem, IndexData indexData)
      {
         StorageIndexData storageIndexData = (StorageIndexData)indexData;

         Widgets.updateTableItem(tableItem,
                                 (Object)storageIndexData,
                                 storageIndexData.name,
                                 "",  // date/time drawn in event handler
                                 Units.formatByteSize(storageIndexData.totalEntrySize),
                                 storageIndexData.indexState.toString()
                                );
      }
    };

    /** create storage index data
     * @param indexId index id
     * @param jobUUID job UUID
     * @param jobName job name or null
     * @param entityId entity index id
     * @param hostName host name
     * @param archiveType archive type
     * @param createdDateTime date/time (timestamp) when some storage was created
     * @param name name of storage
     * @param dateTime date/time (timestamp) when some storage was created
     * @param size size of storage [byte]
     * @param entryCount number of entries
     * @param indexState storage index state
     * @param indexMode storage index mode
     * @param lastCheckedDateTime last checked date/time (timestamp)
     * @param errorMessage error message text
     * @param totalEntryCount total entry count
     * @param totalEntrySize total sum of entry sizes
     */
    StorageIndexData(long         indexId,
                     String       jobUUID,
                     String       jobName,
                     long         entityId,
                     String       hostName,
                     long         createdDateTime,
                     ArchiveTypes archiveType,
                     String       name,
                     long         dateTime,
                     long         size,
                     IndexStates  indexState,
                     IndexModes   indexMode,
                     long         lastCheckedDateTime,
                     String       errorMessage,
                     long         totalEntryCount,
                     long         totalEntrySize
                    )
    {
      super(indexId);

      assert (indexId & 0x0000000F) == 3 : indexId;
      assert (entityId & 0x0000000F) == 2 : entityId;
      assert(totalEntryCount >= 0);
      assert(totalEntrySize >= 0);

      this.jobUUID             = jobUUID;
      this.jobName             = jobName;
      this.entityId            = entityId;
      this.hostName            = hostName;
      this.createdDateTime     = createdDateTime;
      this.archiveType         = archiveType;
      this.name                = name;
      this.dateTime            = dateTime;
      this.size                = size;
      this.indexState          = indexState;
      this.indexMode           = indexMode;
      this.lastCheckedDateTime = lastCheckedDateTime;
      this.errorMessage        = errorMessage;
      this.totalEntryCount     = totalEntryCount;
      this.totalEntrySize      = totalEntrySize;
    }

    /** create storage data
     * @param storageId database storage id
     * @param jobUUID job UUID
     * @param jobName job name
     * @param archiveType archive type
     * @param hostName host name
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param lastCheckedDateTime last checked date/time (timestamp)
     */
    StorageIndexData(long         storageId,
                     String       jobUUID,
                     String       jobName,
                     String       hostName,
                     ArchiveTypes archiveType,
                     String       name,
                     long         dateTime,
                     long         lastCheckedDateTime
                    )
    {
      this(storageId,
           jobUUID,
           jobName,
           0L,  // entity index id
           hostName,
           0L,  // createdDateTime
           archiveType,
           name,
           dateTime,
           0L,  // size
           IndexStates.OK,
           IndexModes.MANUAL,
           lastCheckedDateTime,
           (String)null,  // errorMessage
           0L,  // totalEntryCount
           0L  // totalEntrySize
          );
    }

    /** create storage data
     * @param storageId database storage id
     * @param jobUUID job UUID
     * @param jobName job name
     * @param hostName host name
     * @param archiveType archive type
     * @param name name of storage
     */
    StorageIndexData(long storageId, String jobUUID, String jobName, String hostName, ArchiveTypes archiveType, String name)
    {
      this(storageId,
           jobUUID,
           jobName,
           hostName,
           archiveType,
           name,
           0L,  // dateTime
           0L  // lastCheckedDateTime
          );
    }

//TOOD: make member private
    /** get name
     * @return name
     */
    @Override
    public String getName()
    {
      return name;
    }

//TOOD: make member private
    /** get date/time
     * @return date/time [s]
     */
    @Override
    public long getDateTime()
    {
      return createdDateTime;
    }

//TOOD: make member private
    /** get togal size
     * @return size [bytes]
     */
    @Override
    public long getTotalSize()
    {
      return size;
    }

//TOOD: make member private
    /** get total number of entries
     * @return entries
     */
    @Override
    public long getTotalEntryCount()
    {
      return totalEntryCount;
    }

    /** get total size of entries
     * @return total entries size
     */
    public long getTotalEntrySize()
    {
      return totalEntrySize;
    }

//TOOD: make member private
    /** get index state
     * @return index state
     */
    @Override
    public IndexStates getState()
    {
      return indexState;
    }

    /** get info string
     * @return string
     */
    public String getInfo()
    {
      return String.format("#%d '%s', '%s'",id,!jobName.isEmpty() ? jobName : "unknown",name);
    }

    /** set index state
     * @param indexState index state
     */
    public void setState(IndexStates indexState)
    {
      this.indexState = indexState;
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
      out.writeObject(hostName);
      out.writeObject(name);
      out.writeObject(createdDateTime);
      out.writeObject(size);
      out.writeObject(indexState);
      out.writeObject(indexMode);
      out.writeObject(lastCheckedDateTime);
      out.writeObject(totalEntryCount);
      out.writeObject(totalEntrySize);
    }

    /** read storage index data object from object stream
     * Note: must be implented because Java serializaion API cannot read
     *       inner classes without reading outer classes, too!
     * @param in stream
     */
    private void readObject(java.io.ObjectInputStream in)
      throws IOException, ClassNotFoundException
    {
      super.readObject(in);
      jobName             = (String)in.readObject();
      archiveType         = (ArchiveTypes)in.readObject();
      hostName            = (String)in.readObject();
      name                = (String)in.readObject();
      createdDateTime     = (Long)in.readObject();
      size                = (Long)in.readObject();
      indexState          = (IndexStates)in.readObject();
      indexMode           = (IndexModes)in.readObject();
      lastCheckedDateTime = (Long)in.readObject();
      totalEntryCount     = (Long)in.readObject();
      totalEntrySize      = (Long)in.readObject();
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      return "StorageIndexData {"+id+", hostName="+hostName+", name="+name+", createdDateTime="+createdDateTime+", size="+size+" bytes, state="+indexState+", last checked="+lastCheckedDateTime+", totalEntryCount="+totalEntryCount+" bytes, totalEntrySize="+totalEntrySize+" bytes}";
    }
  };

  /** assign-to data
   */
  class AssignToData
  {
    public String  jobUUID;                        // job UUID
    public String  scheduleUUID;                   // schedule UUID
    public String  date;
    public String  weekDays;
    public String  time;
    public String  customText;
    public boolean enabled;

    /** create assign-to data
     * @param jobUUID job UUID
     * @param scheduleUUID schedule UUID
     * @param date schedule date
     * @param weekDays schedule week days
     * @param time schedule time
     * @param customText custom text
     * @param enabled true iff enabled
     */
    public AssignToData(String  jobUUID,
                        String  scheduleUUID,
                        String  date,
                        String  weekDays,
                        String  time,
                        String  customText,
                        boolean enabled
                       )
    {
      this.jobUUID      = jobUUID;
      this.scheduleUUID = scheduleUUID;
      this.date         = date;
      this.weekDays     = weekDays;
      this.time         = time;
      this.customText   = customText;
      this.enabled      = enabled;
    }

    /** create assign-to data
     * @param jobUUID job UUID
     */
    public AssignToData(String jobUUID)
    {
      this(jobUUID,(String)null,(String)null,(String)null,(String)null,(String)null,false);
    }
  };

  /** restore types
   */
  enum RestoreTypes
  {
    ARCHIVES,
    ENTRIES;
  };

  /** restore entry modes
   */
  enum RestoreEntryModes
  {
    STOP,
    RENAME,
    OVERWRITE,
    SKIP_EXISTING
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

  /** find index for insert of item in sorted UUID menu
   * @param menu menu
   * @param uuidIndexData UUID index data
   * @return index in menu
   */
  private int findStorageMenuIndex(Menu menu, UUIDIndexData uuidIndexData)
  {
    MenuItem            menuItems[]         = menu.getItems();
    IndexDataComparator indexDataComparator = new IndexDataComparator(widgetStorageTree);

//TODO: binary search?
    int index = STORAGE_TREE_MENU_START_INDEX;
    while (   (index < menuItems.length)
           && (indexDataComparator.compare(uuidIndexData,(UUIDIndexData)menuItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** find index for insert of item in sorted entity menu
   * @param subMenu sub-menu
   * @param entityIndexData entity index data
   * @return index in menu
   */
/* TODO: remove?
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
*/

  /** update storage tree/table thread
   */
  class UpdateStorageTreeTableThread extends Thread
  {
    private final int PAGE_SIZE = 32;

    private Object           trigger                   = new Object();   // trigger update object
    private boolean          requestUpdateStorageCount = false;
    private HashSet<Integer> requestUpdateOffsets      = new HashSet<Integer>();
    private int              totalStorageCount         = 0;
    private long             totalStorageSize          = 0;
    private Command          storageTableCommand       = null;
    private String           storageName               = "";
    private String           jobUUID                   = null;
    private IndexStateSet    storageIndexStateSet      = INDEX_STATE_SET_ALL;
    private EntityStates     storageEntityState        = EntityStates.ANY;
    private boolean          requestSetUpdateIndicator = false;          // true to set color/cursor on update

    /** create update storage list thread
     */
    UpdateStorageTreeTableThread()
    {
      super();
      setDaemon(true);
      setName("BARControl Update Storage");
    }

    /** run update storage tree/list thread
     */
    public void run()
    {
      boolean                updateStorageCount = true;
      final HashSet<Integer> updateOffsets      = new HashSet<Integer>();
      boolean                setUpdateIndicator = true;
      try
      {
        while (true)
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
                  if (!widgetStorageTree.isDisposed())
                  {
                    widgetStorageTree.setForeground(COLOR_MODIFIED);
                  }
                  if (!widgetStorageTable.isDisposed())
                  {
                    widgetStorageTable.setForeground(COLOR_MODIFIED);
                  }
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
              // update table count
              if (updateStorageCount)
              {
                updateStorageTableCount();
              }

              // update tree
              HashSet<TreeItem> expandedUUIDTreeItems = new HashSet<TreeItem>();
              if (!this.requestUpdateStorageCount)
              {
                updateUUIDTreeItems(expandedUUIDTreeItems);
              }
              HashSet<TreeItem> expandedEntityTreeItems = new HashSet<TreeItem>();
              if (!this.requestUpdateStorageCount)
              {
                updateEntityTreeItems(expandedUUIDTreeItems,expandedEntityTreeItems);
              }
              if (!this.requestUpdateStorageCount)
              {
                updateStorageTreeItems(expandedEntityTreeItems);
              }

              // update table
              if (   !this.requestUpdateStorageCount  // no new update count request pending
                  && !updateOffsets.isEmpty()             // updated offset requested
                 )
              {
                updateStorageTableItems(updateOffsets);
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
            catch (Throwable throwable)
            {
              if (Settings.debugLevel > 0)
              {
                BARServer.disconnect();
                BARControl.internalError(throwable);
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
                  if (!widgetStorageTree.isDisposed())
                  {
                    widgetStorageTree.setForeground(null);
                  }
                  if (!widgetStorageTable.isDisposed())
                  {
                    widgetStorageTable.setForeground(null);
                  }
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
              this.requestSetUpdateIndicator     = false;

              try { trigger.wait(500); } catch (InterruptedException exception) { /* ignored */ };

              updateStorageCount |= this.requestUpdateStorageCount;
              updateOffsets.addAll(this.requestUpdateOffsets);
              setUpdateIndicator |= this.requestSetUpdateIndicator;
            }
            while (this.requestUpdateStorageCount || !this.requestUpdateOffsets.isEmpty());
          }

          if (updateOffsets.isEmpty())
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                if (!widgetStorageTable.isDisposed())
                {
                  updateOffsets.add(widgetStorageTable.getTopIndex());
                }
              }
            });
          }
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

    /** get total storage count
     * @return total storage count
     */
    private int getTotalStorageCount()
    {
      return totalStorageCount;
    }

    /** get total storage size
     * @return total storage size
     */
    private long getTotalStorageSize()
    {
      return totalStorageSize;
    }

    /** get storage name
     * @return storage name
     */
    private String getStorageName()
    {
      return storageName;
    }

    /** get storage index state set
     * @return storage index state set
     */
    private IndexStateSet getStorageIndexStateSet()
    {
      return storageIndexStateSet;
    }

    /** trigger update of storage tree/list
     * @param storageName new storage name
     * @param storageIndexStateSet new storage index state set
     * @param storageEntityState new storage entity state
     * @param force true to force update
     */
    private void triggerUpdate(String storageName, IndexStateSet storageIndexStateSet, EntityStates storageEntityState, boolean force)
    {
      assert storageName != null;

      synchronized(trigger)
      {
        if (   !this.storageName.equals(storageName)
            || (this.storageIndexStateSet != storageIndexStateSet)
            || (this.storageEntityState != storageEntityState)
            || force
           )
        {
          this.storageName               = storageName;
          this.storageIndexStateSet      = storageIndexStateSet;
          this.storageEntityState        = storageEntityState;
          this.requestUpdateStorageCount = true;
          this.requestSetUpdateIndicator = true;
          restart();
        }
      }
    }

    /** trigger update of storage tree/list
     * @param storageName new storage name
     */
    private void triggerUpdateStorageName(String storageName)
    {
      assert storageName != null;

      synchronized(trigger)
      {
        if (   (this.storageName == null)
            || (storageName == null)
//Note: at least 3 characters?
            || (((storageName.length() == 0) || (storageName.length() >= 3)) && !this.storageName.equals(storageName))
           )
        {
          this.storageName               = storageName;
          this.requestUpdateStorageCount = true;
          this.requestSetUpdateIndicator = true;
          restart();
        }
      }
    }

    /** trigger update of storage tree/list
     * @param jobUUID job UUID
     * @param storageIndexStateSet new storage index state set
     * @param storageEntityState new storage entity state
     */
    private void triggerUpdateStorageState(String jobUUID, IndexStateSet storageIndexStateSet, EntityStates storageEntityState)
    {
      synchronized(trigger)
      {
        if (   (this.jobUUID != jobUUID)
            || (this.storageIndexStateSet != storageIndexStateSet)
            || (this.storageEntityState != storageEntityState)
           )
        {
          this.jobUUID                   = jobUUID;
          this.storageIndexStateSet      = storageIndexStateSet;
          this.storageEntityState        = storageEntityState;
          this.requestUpdateStorageCount = true;
          this.requestSetUpdateIndicator = true;
          restart();
        }
      }
    }

    /** trigger update of storage tree/list item
     * @param index index in list to update
     */
    private void triggerUpdate(TreeItem treeItem)
    {
      synchronized(trigger)
      {
        int offset = (treeItem.getParent().indexOf(treeItem)/PAGE_SIZE)*PAGE_SIZE;
        if (!this.requestUpdateOffsets.contains(offset))
        {
          this.requestUpdateOffsets.add(offset);
          restart();
        }
      }
    }

    /** trigger update of storage tree/list item
     * @param index index in list to update
     */
    private void triggerUpdate(TableItem tableItem)
    {
      synchronized(trigger)
      {
        int offset = (tableItem.getParent().indexOf(tableItem)/PAGE_SIZE)*PAGE_SIZE;
        if (!this.requestUpdateOffsets.contains(offset))
        {
          this.requestUpdateOffsets.add(offset);
          restart();
        }
      }
    }

    /** trigger update of storage tree/list item
     * @param index index in list to update
     */
    private void triggerUpdate(int index)
    {
      synchronized(trigger)
      {
        int offset = (index/PAGE_SIZE)*PAGE_SIZE;
        if (!this.requestUpdateOffsets.contains(offset))
        {
          this.requestUpdateOffsets.add(offset);
          restart();
        }
      }
    }

    /** trigger update of storage tree/list
     */
    private void triggerUpdate()
    {
      synchronized(trigger)
      {
        this.requestUpdateStorageCount = true;
        restart();
      }
    }

    /** check if request update triggered
     * @return true iff request update triggered
     */
    private boolean isRequestUpdate()
    {
      return requestUpdateStorageCount || !requestUpdateOffsets.isEmpty();
    }

    /** restart updates
     */
    private void restart()
    {
      if (requestUpdateStorageCount)
      {
        if (storageTableCommand != null)
        {
          storageTableCommand.abort();
          storageTableCommand = null;
        }
      }
      trigger.notify();
    }

    /** update UUID tree items
     * @param expandedUUIDTreeItems expanded and updated UUID tree items or null
     */
    private void updateUUIDTreeItems(final HashSet<TreeItem> expandedUUIDTreeItems)
    {
      if (expandedUUIDTreeItems != null)
      {
        expandedUUIDTreeItems.clear();
      }

      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(false);
            }
          }
        });
      }
      try
      {
        // get UUID items for remove
        final HashSet<TreeItem> removeTreeItemSet = new HashSet<TreeItem>();
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              for (TreeItem treeItem : widgetStorageTree.getItems())
              {
                assert treeItem.getData() instanceof UUIDIndexData : treeItem.getData();
                removeTreeItemSet.add(treeItem);
              }
            }
          }
        });
        if (isRequestUpdate()) return;

        // get UUID list
        final ArrayList<UUIDIndexData> uuidIndexDataList = new ArrayList<UUIDIndexData>();
        BARServer.executeCommand(StringParser.format("INDEX_UUID_LIST indexStateSet=%s indexModeSet=* name=%'S",
                                                     storageIndexStateSet.nameList("|"),
                                                     storageName
                                                    ),
                                 2,  // debugLevel
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     long   uuidId               = valueMap.getLong  ("uuidId"              );
                                     String jobUUID              = valueMap.getString("jobUUID"             );
                                     String name                 = valueMap.getString("name"                );
                                     long   lastExecutedDateTime = valueMap.getLong  ("lastExecutedDateTime");
                                     int    lastErrorCode        = valueMap.getInt   ("lastErrorCode"       );
                                     String lastErrorData        = valueMap.getString("lastErrorData"       );
                                     long   totalSize            = valueMap.getLong  ("totalSize"           );
                                     long   totalEntryCount      = valueMap.getLong  ("totalEntryCount"     );
                                     long   totalEntrySize       = valueMap.getLong  ("totalEntrySize",     Long.MAX_VALUE);

                                     UUIDIndexData uuidIndexData = new UUIDIndexData(uuidId,
                                                                                     jobUUID,
                                                                                     name,
                                                                                     lastExecutedDateTime,
                                                                                     lastErrorCode,
                                                                                     lastErrorData,
                                                                                     totalSize,
                                                                                     totalEntryCount,
                                                                                     totalEntrySize
                                                                                    );
                                     uuidIndexDataList.add(uuidIndexData);

                                     // check if aborted
                                     if (isRequestUpdate())
                                     {
                                       abort();
                                     }
                                   }
                                 }
                                );
        if (isRequestUpdate()) return;

        // insert/update tree items
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              final IndexDataComparator indexDataComparator = IndexDataComparator.getInstance(widgetStorageTree);

              for (UUIDIndexData uuidIndexData : uuidIndexDataList)
              {
                TreeItem uuidTreeItem = Widgets.updateInsertTreeItem(widgetStorageTree,
                                                                     indexDataComparator,
                                                                     uuidIndexData,
                                                                     Widgets.TREE_ITEM_FLAG_FOLDER,
                                                                     uuidIndexData.name,
                                                                     "",  // hostName
                                                                     "",  // date/time drawn in tree item renderer
                                                                     Units.formatByteSize(uuidIndexData.totalSize),
                                                                     ""
                                                                    );
                Widgets.setTreeItemChecked(uuidTreeItem,checkedIndexIdSet.contains(uuidIndexData.id));

                if ((expandedUUIDTreeItems != null) && uuidTreeItem.getExpanded())
                {
                  expandedUUIDTreeItems.add(uuidTreeItem);
                }
                removeTreeItemSet.remove(uuidTreeItem);
              }
            }
          }
        });
        if (isRequestUpdate()) return;

        // remove not existing UUIDs from tree
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem treeItem : removeTreeItemSet)
            {
              if (!treeItem.isDisposed())
              {
                Widgets.removeTreeItem(treeItem);
              }
            }
          }
        });
      }
      catch (Exception exception)
      {
        // ignored
      }
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(true);
            }
          }
        });
      }
    }

    /** update entity tree item
     * @param uuidTreeItem UUID tree item to update
     * @param expandedEntityTreeItems expanded and updated entry tree items or null
     */
    private void updateEntityTreeItem(final TreeItem uuidTreeItem, final HashSet<TreeItem> expandedEntityTreeItems)
    {
      try
      {
        // get UUID index data, get entity items for remove
        final UUIDIndexData     uuidIndexData[]   = new UUIDIndexData[]{null};
        final HashSet<TreeItem> removeTreeItemSet = new HashSet<TreeItem>();
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!uuidTreeItem.isDisposed())
            {
              assert uuidTreeItem.getData() instanceof UUIDIndexData : uuidTreeItem.getData();

              uuidIndexData[0] = (UUIDIndexData)uuidTreeItem.getData();

              if (uuidTreeItem.getExpanded())
              {
                for (TreeItem treeItem : uuidTreeItem.getItems())
                {
                  assert treeItem.getData() instanceof EntityIndexData : treeItem.getData();
                  removeTreeItemSet.add(treeItem);
                }
              }
            }
          }
        });
        if (isRequestUpdate()) return;

        // get entity list for UUID
        final ArrayList<EntityIndexData> entityIndexDataList = new ArrayList<EntityIndexData>();
        BARServer.executeCommand(StringParser.format("INDEX_ENTITY_LIST jobUUID=%'S indexStateSet=%s indexModeSet=* name=%'S",
                                                     uuidIndexData[0].jobUUID,
                                                     storageIndexStateSet.nameList("|"),
                                                     storageName
                                                    ),
                                 2,  // debug level
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     long         entityId         = valueMap.getLong  ("entityId"        );
                                     String       jobUUID          = valueMap.getString("jobUUID"         );
                                     String       scheduleUUID     = valueMap.getString("scheduleUUID"    );
                                     ArchiveTypes archiveType      = valueMap.getEnum  ("archiveType",    ArchiveTypes.class);
                                     long         createdDateTime  = valueMap.getLong  ("createdDateTime" );
                                     int          lastErrorCode    = valueMap.getInt   ("lastErrorCode"   );
                                     String       lastErrorData    = valueMap.getString("lastErrorData"   );
                                     long         totalSize        = valueMap.getLong  ("totalSize",      0L);
                                     long         totalEntryCount  = valueMap.getLong  ("totalEntryCount",0L);
                                     long         totalEntrySize   = valueMap.getLong  ("totalEntrySize", 0L);
                                     long         expireDateTime   = valueMap.getLong  ("expireDateTime"  );

                                     // add entity data index
                                     EntityIndexData entityIndexData = new EntityIndexData(entityId,
                                                                                           jobUUID,
                                                                                           scheduleUUID,
                                                                                           archiveType,
                                                                                           createdDateTime,
                                                                                           lastErrorCode,
                                                                                           lastErrorData,
                                                                                           totalSize,
                                                                                           totalEntryCount,
                                                                                           totalEntrySize,
                                                                                           expireDateTime
                                                                                          );
                                     entityIndexDataList.add(entityIndexData);

                                     // check if aborted
                                     if (isRequestUpdate())
                                     {
                                       abort();
                                     }
                                   }
                                 }
                                );
        if (isRequestUpdate()) return;

        // insert/update tree items
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!uuidTreeItem.isDisposed())
            {
              final IndexDataComparator<EntityIndexData> indexDataComparator = IndexDataComparator.getInstance(IndexDataComparator.SortModes.CREATED_DATETIME);

              for (final EntityIndexData entityIndexData : entityIndexDataList)
              {
                assert entityIndexData != null;

                TreeItem entityTreeItem = Widgets.updateInsertTreeItem(uuidTreeItem,
                                                                       indexDataComparator,
                                                                       entityIndexData,
                                                                       Widgets.TREE_ITEM_FLAG_FOLDER,
                                                                       entityIndexData.archiveType.toString(),
                                                                       "",  // hostName
                                                                       "",  // date/time drawn in tree item renderer
                                                                       Units.formatByteSize(entityIndexData.totalSize),
                                                                       ""
                                                                      );
                Widgets.setTreeItemChecked(entityTreeItem,checkedIndexIdSet.contains(entityIndexData.id));

                if ((expandedEntityTreeItems != null) && entityTreeItem.getExpanded())
                {
                  expandedEntityTreeItems.add(entityTreeItem);
                }
                removeTreeItemSet.remove(entityTreeItem);
              }
            }
          }
        });
        if (isRequestUpdate()) return;

        // remove not existing entities from tree
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem treeItem : removeTreeItemSet)
            {
              if (!treeItem.isDisposed())
              {
                IndexData indexData = (IndexData)treeItem.getData();
                Widgets.removeTreeItem(treeItem);

                if (indexData != null)
                {
                  setStorageList(indexData.id,false);
                }
              }
            }
          }
        });
      }
      catch (Exception exception)
      {
        // ignored
      }
    }

    /** update entity tree item
     * @param uuidTreeItem UUID tree item to update
     */
    private void updateEntityTreeItem(TreeItem uuidTreeItem)
    {
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            BARControl.waitCursor();
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(false);
            }
          }
        });
      }
      try
      {
        updateEntityTreeItem(uuidTreeItem,(HashSet<TreeItem>)null);
      }
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(true);
            }
            BARControl.resetCursor();
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

      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            BARControl.waitCursor();
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(false);
            }
          }
        });
      }
      try
      {
        for (TreeItem uuidTreeItem : uuidTreeItems)
        {
          updateEntityTreeItem(uuidTreeItem,entityTreeItems);
        }
      }
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(true);
            }
            BARControl.resetCursor();
          }
        });
      }
    }

    /** update storage tree item
     * @param entityTreeItem job tree item to update
     */
    private void updateStorageTreeItem(final TreeItem entityTreeItem)
    {
      try
      {
        // get entity index data, get storage items for remove
        final EntityIndexData   entityIndexData[] = new EntityIndexData[1];
        final HashSet<TreeItem> removeTreeItemSet = new HashSet<TreeItem>();
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!entityTreeItem.isDisposed())
            {
              assert entityTreeItem.getData() instanceof EntityIndexData : entityTreeItem.getData();

              entityIndexData[0] = (EntityIndexData)entityTreeItem.getData();

              if (entityTreeItem.getExpanded())
              {
                for (TreeItem treeItem : entityTreeItem.getItems())
                {
                  assert treeItem.getData() instanceof StorageIndexData : treeItem.getData();
                  removeTreeItemSet.add(treeItem);
                }
              }
            }
          }
        });
        if (isRequestUpdate()) return;

        // get storage list for entity
        final ArrayList<StorageIndexData> storageIndexDataList = new ArrayList<StorageIndexData>();
        BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%ld jobUUID=%'S indexStateSet=%s indexModeSet=* name=%'S",
                                                     entityIndexData[0].id,
                                                     (jobUUID != null) ? jobUUID : "*",
                                                     storageIndexStateSet.nameList("|"),
                                                     storageName
                                                    ),
                                 2,  // debugLevel
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     long         storageId           = valueMap.getLong  ("storageId"                     );
                                     String       jobUUID             = valueMap.getString("jobUUID"                       );
                                     String       jobName             = valueMap.getString("jobName"                       );
                                     long         entityId            = valueMap.getLong  ("entityId"                      );
                                     String       scheduleUUID        = valueMap.getString("scheduleUUID"                  );
                                     String       hostName            = valueMap.getString("hostName"                      );
                                     long         createdDateTime     = valueMap.getLong  ("createdDateTime"               );
                                     ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",ArchiveTypes.class);
                                     String       name                = valueMap.getString("name"                          );
                                     long         dateTime            = valueMap.getLong  ("dateTime"                      );
                                     long         size                = valueMap.getLong  ("size"                          );
                                     IndexStates  indexState          = valueMap.getEnum  ("indexState",IndexStates.class  );
                                     IndexModes   indexMode           = valueMap.getEnum  ("indexMode",IndexModes.class    );
                                     long         lastCheckedDateTime = valueMap.getLong  ("lastCheckedDateTime"           );
                                     String       errorMessage_       = valueMap.getString("errorMessage"                  );
                                     long         totalEntryCount     = valueMap.getLong  ("totalEntryCount"               );
                                     long         totalEntrySize      = valueMap.getLong  ("totalEntrySize",               Long.MAX_VALUE);

                                     // add storage index data
                                     storageIndexDataList.add(new StorageIndexData(storageId,
                                                                                   jobUUID,
                                                                                   jobName,
                                                                                   entityId,
                                                                                   hostName,
                                                                                   createdDateTime,
                                                                                   archiveType,
                                                                                   name,
                                                                                   dateTime,
                                                                                   size,
                                                                                   indexState,
                                                                                   indexMode,
                                                                                   lastCheckedDateTime,
                                                                                   errorMessage_,
                                                                                   totalEntryCount,
                                                                                   totalEntrySize
                                                                                  )
                                                             );

                                     // check if aborted
                                     if (isRequestUpdate())
                                     {
                                       abort();
                                     }
                                   }
                                 }
                                );
        if (isRequestUpdate()) return;

        // insert/update tree items
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              final IndexDataComparator indexDataComparator = IndexDataComparator.getInstance(widgetStorageTree);

              for (StorageIndexData storageIndexData : storageIndexDataList)
              {
                if (!entityTreeItem.isDisposed())
                {
                  TreeItem storageTreeItem = Widgets.updateInsertTreeItem(entityTreeItem,
                                                                          indexDataComparator,
                                                                          storageIndexData,
                                                                          Widgets.TREE_ITEM_FLAG_NONE,
                                                                          storageIndexData.name,
                                                                          storageIndexData.hostName,
                                                                          "",  // date/time drawn in tree item renderer
                                                                          Units.formatByteSize(storageIndexData.size),
                                                                          storageIndexData.indexState.toString()
                                                                         );

                  removeTreeItemSet.remove(storageTreeItem);
                }
              }
            }
          }
        });
        if (isRequestUpdate()) return;

        // remove not existing stroages from tree
        display.syncExec(new Runnable()
        {
          public void run()
          {
            for (TreeItem treeItem : removeTreeItemSet)
            {
              if (!treeItem.isDisposed())
              {
                IndexData indexData = (IndexData)treeItem.getData();
                Widgets.removeTreeItem(treeItem);

                if (indexData != null)
                {
                  setStorageList(indexData.id,false);
                }
              }
            }
          }
        });
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

    /** update storage tree items
     * @param entityTreeItems job tree items to update
     */
    private void updateStorageTreeItems(final HashSet<TreeItem> entityTreeItems)
    {
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            BARControl.waitCursor();
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(false);
            }
          }
        });
      }
      try
      {
        for (final TreeItem entityTreeItem : entityTreeItems)
        {
          updateStorageTreeItem(entityTreeItem);
        }
      }
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(true);
            }
            BARControl.resetCursor();
          }
        });
      }
    }

    /** update tree item
     * @param treeItem tree item to update
     */
    private void updateTreeItem(TreeItem treeItem)
    {
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            BARControl.waitCursor();
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(false);
            }
          }
        });
      }
      try
      {
        if      (treeItem.getData() instanceof UUIDIndexData)
        {
          updateEntityTreeItem(treeItem,(HashSet<TreeItem>)null);
        }
        else if (treeItem.getData() instanceof EntityIndexData)
        {
          updateStorageTreeItem(treeItem);
        }
      }
      finally
      {
        // enable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTree.isDisposed())
            {
              widgetStorageTree.setRedraw(true);
            }
            BARControl.resetCursor();
          }
        });
      }
    }

    /** refresh storage table display count
     */
    private void updateStorageTableCount()
    {
      assert storageName != null;

      // get current total storage count
      final int oldTotalStorageCount = totalStorageCount;

      {
        // set update inidicator
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTabFolderTitle.isDisposed())
            {
              widgetStorageTabFolderTitle.setForeground(COLOR_MODIFIED);
              widgetStorageTabFolderTitle.redraw();
            }
          }
        });
      }
      try
      {
        // get total storage count, total storage size
        ValueMap valueMap = new ValueMap();
        BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST_INFO entityId=%s jobUUID=%'S indexTypeSet=UUID|ENTITY indexStateSet=%s indexModeSet=* name=%'S",
                                                     (storageEntityState != EntityStates.NONE) ? "*" : "NONE",
                                                     (jobUUID != null) ? jobUUID : "*",
                                                     storageIndexStateSet.nameList("|"),
                                                     storageName
                                                    ),
                                 2,  // debugLevel
                                 valueMap
                                );
        totalStorageCount = valueMap.getInt ("totalStorageCount",0 );
        totalStorageSize  = valueMap.getLong("totalStorageSize", 0L);
        assert(totalStorageCount >= 0);
        assert(totalStorageSize >= 0);
      }
      catch (Exception exception)
      {
        // ignored
        totalStorageCount = 0;
        totalStorageSize  = 0;
      }
      finally
      {
        // set count, clear update inidicator
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTabFolderTitle.isDisposed())
            {
              widgetStorageTabFolderTitle.setForeground(null);
              widgetStorageTabFolderTitle.redraw();
            }

            if (!widgetStorageTable.isDisposed())
            {
              widgetStorageTable.setRedraw(false);

              widgetStorageTable.clearAll();
              widgetStorageTable.setItemCount(totalStorageCount);

              widgetStorageTable.setRedraw(true);
            }
          }
        });
      }
    }

    /** refresh storage table items
     * @param offset refresh offset
     * @return true iff update done
     */
    private boolean updateStorageTableItems(final int offset)
    {
      assert(storageName != null);
      assert(offset >= 0);
      assert(totalStorageCount >= 0);

      // get limit
      final int limit = ((offset+PAGE_SIZE) < totalStorageCount) ? PAGE_SIZE : totalStorageCount-offset;

      // get sort mode, ordering
      final String[] sortMode = new String[]{"NAME"};
      final String[] ordering = new String[]{"NONE"};
      display.syncExec(new Runnable()
      {
        public void run()
        {
          if (!widgetStorageTable.isDisposed())
          {
            TableColumn tableColumn = widgetStorageTable.getSortColumn();
            if (tableColumn != null)
            {
              switch (widgetStorageTable.indexOf(tableColumn))
              {
                case 0:  sortMode[0] = "NAME";     break;
                case 1:  sortMode[0] = "HOSTNAME"; break;
                case 2:  sortMode[0] = "SIZE";     break;
                case 3:  sortMode[0] = "MODIFIED"; break;
                case 4:  sortMode[0] = "STATE";    break;
                default: sortMode[0] = "NAME";     break;
              }

              switch (widgetStorageTable.getSortDirection())
              {
                case SWT.UP  : ordering[0] = "ASCENDING";  break;
                case SWT.DOWN: ordering[0] = "DESCENDING"; break;
                case SWT.NONE: ordering[0] = "NONE";       break;
              }
            }
          }
        }
      });

      // update table
      final int[] n = new int[]{0};
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTable.isDisposed())
            {
              widgetStorageTable.setRedraw(false);
            }
          }
        });
      }
      try
      {
        // get list
        final ArrayList<StorageIndexData> storageIndexDataList = new ArrayList<StorageIndexData>();
        try
        {
          storageTableCommand = BARServer.asyncExecuteCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s jobUUID=%'S indexTypeSet=UUID|ENTITY indexStateSet=%s indexModeSet=* name=%'S offset=%d limit=%d sortMode=%s ordering=%s",
                                                                                  (storageEntityState != EntityStates.NONE) ? "*" : "NONE",
                                                                                  (jobUUID != null) ? jobUUID : "*",
                                                                                  storageIndexStateSet.nameList("|"),
                                                                                  storageName,
                                                                                  offset,
                                                                                  limit,
                                                                                  sortMode[0],
                                                                                  ordering[0]
                                                                                 ),
                                                              2,  // debugLevel
                                                              new Command.ResultHandler()
                                                              {
                                                                @Override
                                                                public void handle(int i, ValueMap valueMap)
                                                                {
                                                                  long         storageId           = valueMap.getLong  ("storageId"                                         );
                                                                  String       jobUUID             = valueMap.getString("jobUUID"                                           );
                                                                  String       jobName             = valueMap.getString("jobName"                                           );
                                                                  long         entityId            = valueMap.getLong  ("entityId"                                          );
                                                                  String       hostName            = valueMap.getString("hostName"                                          );
                                                                  long         createdDateTime     = valueMap.getLong  ("createdDateTime"                                   );
                                                                  ArchiveTypes archiveType         = valueMap.getEnum  ("archiveType",ArchiveTypes.class,ArchiveTypes.NORMAL);
                                                                  String       name                = valueMap.getString("name"                                              );
                                                                  long         dateTime            = valueMap.getLong  ("dateTime"                                          );
                                                                  long         size                = valueMap.getLong  ("size"                                              );
                                                                  IndexStates  indexState          = valueMap.getEnum  ("indexState",IndexStates.class                      );
                                                                  IndexModes   indexMode           = valueMap.getEnum  ("indexMode",IndexModes.class                        );
                                                                  long         lastCheckedDateTime = valueMap.getLong  ("lastCheckedDateTime"                               );
                                                                  String       errorMessage_       = valueMap.getString("errorMessage"                                      );
                                                                  long         totalEntryCount     = valueMap.getLong  ("totalEntryCount"                                   );
                                                                  long         totalEntrySize      = valueMap.getLong  ("totalEntrySize",                                   Long.MAX_VALUE);

                                                                  // add storage index data
                                                                  storageIndexDataList.add(new StorageIndexData(storageId,
                                                                                                                jobUUID,
                                                                                                                jobName,
                                                                                                                entityId,
                                                                                                                hostName,
                                                                                                                createdDateTime,
                                                                                                                archiveType,
                                                                                                                name,
                                                                                                                dateTime,
                                                                                                                size,
                                                                                                                indexState,
                                                                                                                indexMode,
                                                                                                                lastCheckedDateTime,
                                                                                                                errorMessage_,
                                                                                                                totalEntryCount,
                                                                                                                totalEntrySize
                                                                                                               )
                                                                                          );

                                                                  // store number of entries
                                                                  n[0] = i+1;

                                                                  // check if aborted
                                                                  if (isRequestUpdate() || (n[0] > limit))
                                                                  {
                                                                    abort();
                                                                  }
                                                                }
                                                              }
                                                             );
          BARServer.asyncCommandWait(storageTableCommand);
          storageTableCommand = null;
        }
        catch (Exception exception)
        {
          BARControl.logThrowable(exception);
        }

        // update storage table segment
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTable.isDisposed())
            {
              int i = 0;
              int n = widgetStorageTable.getItemCount();
              for (StorageIndexData storageIndexData : storageIndexDataList)
              {
                if ((offset+i) < n)
                {
                  TableItem tableItem = widgetStorageTable.getItem(offset+i);
                  if (!tableItem.isDisposed())
                  {
                    Widgets.updateTableItem(tableItem,
                                            (Object)storageIndexData,
                                            storageIndexData.name,
                                            storageIndexData.hostName,
                                            Units.formatByteSize(storageIndexData.totalEntrySize),
                                            "",  // date/time drawn in event handler
                                            storageIndexData.indexState.toString()
                                           );
                    tableItem.setChecked(checkedIndexIdSet.contains(storageIndexData.id));
                    tableItem.setBackground(storageIndexData.jobUUID.isEmpty() ? COLOR_NO_JOB_INFO : null);
                  }
                }

                i++;
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
            if (!widgetStorageTable.isDisposed())
            {
              widgetStorageTable.setRedraw(true);
            }
          }
        });
      }

      return n[0] >= limit;
    }

    /** refresh storage table items
     * @param updateOffsets segment offsets to update
     */
    private void updateStorageTableItems(HashSet<Integer> updateOffsets)
    {
      try
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTable.isDisposed())
            {
              widgetStorageTable.setRedraw(false);
            }
          }
        });

        Integer offsets[] = updateOffsets.toArray(new Integer[updateOffsets.size()]);
        for (Integer offset : offsets)
        {
          if (!updateStorageTableItems(offset)) break;
          updateOffsets.remove(offset);
        }
      }
      finally
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetStorageTable.isDisposed())
            {
              widgetStorageTable.setRedraw(true);
            }
          }
        });
      }
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

    /** check if entry has a size
     * @return true iff entry has a size
     */
    public boolean hasSize()
    {
      return    (this == FILE)
             || (this == IMAGE)
             || (this == HARDLINK);
    }

    /** get (translated) text
     * @return text
     */
    public String getText()
    {
      switch (this)
      {
        case FILE:      return BARControl.tr("file");
        case IMAGE:     return BARControl.tr("image");
        case DIRECTORY: return BARControl.tr("directory");
        case LINK:      return BARControl.tr("link");
        case HARDLINK:  return BARControl.tr("hardlink");
        case SPECIAL:   return BARControl.tr("special");
        default:        return "*";
      }
    }

    /** convert data to string
     * @return string
     */
    @Override
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
    RUNNING,
    RESTORED,
    FAILED
  };

  /** entry data
   */
  class EntryIndexData extends IndexData
  {
    String       jobName;
    ArchiveTypes archiveType;
    String       hostName;
    EntryTypes   entryType;
    String       name;
    long         dateTime;
    long         size;              // file/image/directory size
    int          fragmentCount;

    /** create entry index data
     * @param indexId index id
     * @param jobName job name
     * @param archiveType archive type
     * @param hostName host name
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     * @param size size [bytes]
     * @param fragmentCount fragment count
     */
    EntryIndexData(long         indexId,
                   String       jobName,
                   ArchiveTypes archiveType,
                   String       hostName,
                   EntryTypes   entryType,
                   String       name,
                   long         dateTime,
                   long         size,
                   int          fragmentCount
                  )
    {
      super(indexId);

      assert ((indexId & 0x0000000F) >= 4) && ((indexId & 0x0000000F) <= 10): indexId;
      assert(size >= 0);
      assert(fragmentCount >= 0);

      this.jobName       = jobName;
      this.archiveType   = archiveType;
      this.hostName      = hostName;
      this.entryType     = entryType;
      this.name          = name;
      this.dateTime      = dateTime;
      this.size          = size;
      this.fragmentCount = fragmentCount;
    }

    /** create entry index data
     * @param indexId index id
     * @param jobName job name
     * @param archiveType archive type
     * @param hostName host name
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     * @param size size [bytes]
     */
    EntryIndexData(long         indexId,
                   String       jobName,
                   ArchiveTypes archiveType,
                   String       hostName,
                   EntryTypes   entryType,
                   String       name,
                   long         dateTime,
                   long         size
                  )
    {
      this(indexId,jobName,archiveType,hostName,entryType,name,dateTime,size,0);
    }

    /** create entry data
     * @param indexId entry id
     * @param jobName job name
     * @param archiveType archive type
     * @param hostName host name
     * @param entryType entry type
     * @param name entry name
     * @param dateTime date/time (timestamp)
     */
    EntryIndexData(long         indexId,
                   String       jobName,
                   ArchiveTypes archiveType,
                   String       hostName,
                   EntryTypes   entryType,
                   String       name,
                   long         dateTime
                  )
    {
      this(indexId,jobName,archiveType,hostName,entryType,name,dateTime,0L);
    }

    /** compare index data
     * @param entryIndexData index data
     * @return -1/0/1 if less/equals/greater
     */
    public int compareTo(EntryIndexData entryIndexData)
    {
// TODO remove
//      EntryIndexData entryIndexData = (EntryIndexData)object;
      int            result;

      result = name.compareTo(entryIndexData.name);
      if (result == 0)
      {
        if      (dateTime < entryIndexData.dateTime) result = -1;
        else if (dateTime > entryIndexData.dateTime) result =  1;
        if (result == 0)
        {
          if      (size < entryIndexData.size) result = -1;
          else if (size > entryIndexData.size) result =  1;
        }
      }

      return result;
    }

    /** get total size
     * @return size [bytes]
     */
    public long getTotalSize()
    {
      return size;
    }

    /** get number of entries
     * @return entries
     */
    public long getTotalEntryCount()
    {
      return 1;
    }

    /** get total size of entries
     * @return total entries size
     */
    public long getTotalEntrySize()
    {
      return size;
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      return "Entry {"+id+", jobName="+jobName+", hostName="+hostName+", name="+name+", entryType="+entryType+", dateTime="+dateTime+", size="+size+"}";
    }
  };

  /** update entry table thread
   */
  class UpdateEntryTableThread extends Thread
  {
    private final int PAGE_SIZE = 32;

    private Object           trigger                      = new Object();   // trigger update object
    private boolean          requestUpdateTotalEntryCount = false;
    private HashSet<Integer> requestUpdateOffsets         = new HashSet<Integer>();
    private Command          totalEntryCountCommand       = null;
    private long             totalEntryCount              = 0;
    private long             totalEntrySize               = 0;
    private Command          entryTableCommand            = null;
    private EntryTypes       entryType                    = EntryTypes.ANY;
    private String           name                         = "";
    private boolean          newestOnly                   = false;
    private boolean          requestSetUpdateIndicator    = false;          // true to set color/cursor on update

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
      boolean                updateTotalEntryCount = true;
      final HashSet<Integer> updateOffsets         = new HashSet<Integer>();
      boolean                setUpdateIndicator    = true;
      try
      {
        for (;;)
        {
          // update table count, table segment
          boolean updateIndicator = false;
          {
            // set busy cursor, foreground color to inform about update, clear background color
            if (setUpdateIndicator)
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  BARControl.waitCursor();
                  if (!widgetEntryTable.isDisposed())
                  {
                    widgetEntryTable.setForeground(COLOR_MODIFIED);
                  }
                }
              });
              updateIndicator = true;
            }
            display.syncExec(new Runnable()
            {
              public void run()
              {
                if (!widgetEntryFilter.isDisposed())
                {
                  widgetEntryFilter.setBackground(null);
                }
              }
            });
          }
          try
          {
            if (updateTotalEntryCount)
            {
              updateEntryTableTotalEntryCount();
            }
            if (!updateOffsets.isEmpty())
            {
              updateEntryTableItems(updateOffsets);
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
          catch (Throwable throwable)
          {
            if (Settings.debugLevel > 0)
            {
              BARServer.disconnect();
              BARControl.internalError(throwable);
            }
          }
          finally
          {
            // reset cursor, colors
            if (updateIndicator)
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  if (!widgetEntryTable.isDisposed())
                  {
                    widgetEntryTable.setForeground(null);
                  }
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
              try { trigger.wait(5*60*1000); } catch (InterruptedException exception) { /* ignored */ };
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

              try { trigger.wait(2000); } catch (InterruptedException exception) { /* ignored */ };

              updateTotalEntryCount |= this.requestUpdateTotalEntryCount;
              updateOffsets.addAll(this.requestUpdateOffsets);
              setUpdateIndicator |= this.requestSetUpdateIndicator;
            }
            while (this.requestUpdateTotalEntryCount || !this.requestUpdateOffsets.isEmpty());
          }

          if (updateOffsets.isEmpty())
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                if (!widgetEntryTable.isDisposed())
                {
                  updateOffsets.add(widgetEntryTable.getTopIndex());
                }
              }
            });
          }
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

    /** get total count
     * @return total count
     */
    private long getTotalEntryCount()
    {
      return totalEntryCount;
    }

    /** get total size
     * @return total size [bytes]
     */
    private long getTotalEntrySize()
    {
      return totalEntrySize;
    }

    /** get entry type
     * @return entry type
     */
    private EntryTypes getEntryType()
    {
      return entryType;
    }

    /** get entry name
     * @return entry name
     */
    private String getName_()
    {
      return name;
    }

    /** get newest-only
     * @return true for newest entries only
     */
    private boolean getNewestOnly()
    {
      return newestOnly;
    }

    /** trigger update of entry list
     * @param name new entry name or null
     * @param type type or *
     * @param newestOnly flag for newest entries only
     * @param force true to force update
     */
    private void triggerUpdate(String name, String type, boolean newestOnly, boolean force)
    {
      synchronized(trigger)
      {
        if (   (this.name == null)
            || (name == null)
            || !this.name.equals(name)
            || (entryType != this.entryType)
            || (this.newestOnly != newestOnly)
            || force
           )
        {
          this.name                         = name;
          this.entryType                    = entryType;
          this.newestOnly                   = newestOnly;
          this.requestUpdateTotalEntryCount = true;
          this.requestSetUpdateIndicator    = true;
          restart();
        }
      }
    }

    /** trigger update of entry list
     * @param name new entry pattern or null
     */
    private void triggerUpdateEntryName(String name)
    {
      assert name != null;

      synchronized(trigger)
      {
        if (   (this.name == null)
            || (name == null)
//Note: at least 3 characters?
            || (((name.length() == 0) || (name.length() >= 3)) && !this.name.equals(name))
           )
        {
          this.name                         = name;
          this.requestUpdateTotalEntryCount = true;
          this.requestSetUpdateIndicator    = true;
          restart();
        }
      }
    }

    /** trigger update of entry list
     * @param entryType entry type
     */
    private void triggerUpdateEntryType(EntryTypes entryType)
    {
      synchronized(trigger)
      {
        if (entryType != this.entryType)
        {
          this.entryType                    = entryType;
          this.requestUpdateTotalEntryCount = true;
          this.requestSetUpdateIndicator    = true;
          restart();
        }
      }
    }


    /** trigger update of entry list
     * @param newestOnly flag for newest entries only or null
     */
    private void triggerUpdateNewestOnly(boolean newestOnly)
    {
      synchronized(trigger)
      {
        if (this.newestOnly != newestOnly)
        {
          this.newestOnly                   = newestOnly;
          this.requestUpdateTotalEntryCount = true;
          this.requestSetUpdateIndicator    = true;
          restart();
        }
      }
    }

    /** trigger update entry list item
     * @param index index in list to start update
     */
    private void triggerUpdateTableItem(int index)
    {
      synchronized(trigger)
      {
        int offset = (index/PAGE_SIZE)*PAGE_SIZE;
        if (!this.requestUpdateOffsets.contains(offset))
        {
          this.requestUpdateOffsets.add(offset);
          restart();
        }
      }
    }

    /** trigger update of entry list
     */
    private void triggerUpdate()
    {
      synchronized(trigger)
      {
        this.requestUpdateTotalEntryCount = true;
        restart();
      }
    }

    /** check if request update triggered
     * @return true iff request update triggered
     */
    private boolean isRequestUpdate()
    {
      return requestUpdateTotalEntryCount || !requestUpdateOffsets.isEmpty();
    }

    /** restart updates
     */
    private void restart()
    {
      if (requestUpdateTotalEntryCount)
      {
        if (totalEntryCountCommand != null) totalEntryCountCommand.abort();
        if (entryTableCommand      != null) entryTableCommand.abort();
      }
      trigger.notify();
    }

    /** refresh entry table display total count
     */
    private void updateEntryTableTotalEntryCount()
    {
      assert name != null;

      // get current entry count
      final long oldTotalEntryCount = totalEntryCount;

      {
        // set update indicator
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetEntryTableTitle.isDisposed())
            {
              widgetEntryTableTitle.setForeground(COLOR_MODIFIED);
              widgetEntryTableTitle.redraw();
            }
          }
        });
      }
      try
      {
        // get new entries count, total entry size
        try
        {
          ValueMap valueMap = new ValueMap();
          BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST_INFO entryType=%s name=%'S newestOnly=%y selectedOnly=no fragmentsCount=no",
                                                       entryType.toString(),
                                                       name,
                                                       newestOnly
                                                      ),
                                   1,  // debugLevel
                                   valueMap
                                  );
          totalEntryCount = valueMap.getLong("totalEntryCount");
          totalEntrySize  = valueMap.getLong("totalEntrySize",Long.MAX_VALUE);
          assert(totalEntryCount >= 0);
          assert(totalEntrySize >= 0);
        }
        catch (Exception exception)
        {
          // ignored
          totalEntryCount = 0;
          totalEntrySize  = 0;
          BARControl.logThrowable(exception);
        }

        // show warning if too many entries
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

        // set entries count
        if (oldTotalEntryCount != totalEntryCount)
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              if (!widgetEntryTable.isDisposed())
              {
                widgetEntryTable.setRedraw(false);

                widgetEntryTable.clearAll();
                widgetEntryTable.setItemCount((int)Math.min(totalEntryCount,MAX_SHOWN_ENTRIES));

                widgetEntryTable.setRedraw(true);
              }
            }
          });
        }
      }
      finally
      {
        // clear update indicator
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetEntryTableTitle.isDisposed())
            {
              widgetEntryTableTitle.setForeground(null);
              widgetEntryTableTitle.redraw();
            }
          }
        });
      }
    }

    /** refresh entry table items
     * @param offset refresh offset
     * @return true iff update done
     */
    private boolean updateEntryTableItems(final int offset)
    {
      assert name != null;
      assert offset >= 0;
      assert totalEntryCount >= 0;

      // get limit
      final int limit = ((offset+PAGE_SIZE) < totalEntryCount) ? PAGE_SIZE : (int)(totalEntryCount-offset);

      // get sort mode, ordering
      final String[] sortMode = new String[]{"NAME"};
      final String[] ordering = new String[]{"NONE"};
      display.syncExec(new Runnable()
      {
        public void run()
        {
          if (!widgetEntryTable.isDisposed())
          {
            TableColumn tableColumn = widgetEntryTable.getSortColumn();
            if (tableColumn != null)
            {
              switch (widgetEntryTable.indexOf(tableColumn))
              {
                case 0:  sortMode[0] = "NAME";         break;
                case 1:  sortMode[0] = "TYPE";         break;
                case 2:  sortMode[0] = "SIZE";         break;
                case 3:  sortMode[0] = "LAST_CHANGED"; break;
                case 4:  sortMode[0] = "FRAGMENT";     break;
                default: sortMode[0] = "NAME";         break;
              }

              switch (widgetEntryTable.getSortDirection())
              {
                case SWT.UP  : ordering[0] = "ASCENDING";  break;
                case SWT.DOWN: ordering[0] = "DESCENDING"; break;
                case SWT.NONE: ordering[0] = "NONE";       break;
              }
            }
          }
        }
      });

      // update table
      final int[] n = new int[]{0};
      {
        // disable redraw
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetEntryTable.isDisposed())
            {
              widgetEntryTable.setRedraw(false);
            }
          }
        });
      }
      try
      {
        // get list
        final ArrayList<EntryIndexData> entryIndexDataList = new ArrayList<EntryIndexData>();
        try
        {
          entryTableCommand = BARServer.asyncExecuteCommand(StringParser.format("INDEX_ENTRY_LIST entryType=%s name=%'S newestOnly=%y selectedOnly=no fragmentsCount=no offset=%d limit=%d sortMode=%s ordering=%s",
                                                                                entryType.toString(),
                                                                                name,
                                                                                newestOnly,
                                                                                offset,
                                                                                limit,
                                                                                sortMode[0],
                                                                                ordering[0]
                                                                               ),
                                                            2,  // debugLevel
                                                            new Command.ResultHandler()
                                                            {
                                                              @Override
                                                              public void handle(int i, ValueMap valueMap)
                                                              {
                                                                final int index = offset+i;

                                                                String           jobName     = valueMap.getString("jobName"                       );
                                                                ArchiveTypes     archiveType = valueMap.getEnum  ("archiveType",ArchiveTypes.class);
                                                                String           hostName    = valueMap.getString("hostName"                      );
                                                                long             entryId     = valueMap.getLong  ("entryId"                       );
                                                                final EntryTypes entryType   = valueMap.getEnum  ("entryType",EntryTypes.class    );

                                                                switch (entryType)
                                                                {
                                                                  case FILE:
                                                                    {
                                                                      String fileName       = valueMap.getString("name"          );
                                                                      long   dateTime       = valueMap.getLong  ("dateTime"      );
                                                                      long   size           = valueMap.getLong  ("size"          );
                                                                      int    fragmentCount  = valueMap.getInt   ("fragmentCount" );

                                                                      // add entry data index
                                                                      entryIndexDataList.add(new EntryIndexData(entryId,
                                                                                                                jobName,
                                                                                                                archiveType,
                                                                                                                hostName,
                                                                                                                EntryTypes.FILE,
                                                                                                                fileName,
                                                                                                                dateTime,
                                                                                                                size,
                                                                                                                fragmentCount
                                                                                                               )
                                                                                            );
                                                                    }
                                                                    break;
                                                                  case IMAGE:
                                                                    {
                                                                      String          imageName      = valueMap.getString("name"                                );
                                                                      long            size           = valueMap.getLong  ("size"                                );
//                                                                      FileSystemTypes fileSystemType = valueMap.getEnum  ("fileSystemType",FileSystemTypes.class);
                                                                      int             fragmentCount  = valueMap.getInt   ("fragmentCount"                       );

                                                                      // add entry data index
                                                                      entryIndexDataList.add(new EntryIndexData(entryId,
                                                                                                                jobName,
                                                                                                                archiveType,
                                                                                                                hostName,
                                                                                                                EntryTypes.IMAGE,
                                                                                                                imageName,
                                                                                                                0L,
                                                                                                                size,
                                                                                                                fragmentCount
                                                                                                               )
                                                                                            );
                                                                    }
                                                                    break;
                                                                  case DIRECTORY:
                                                                    {
                                                                      String directoryName = valueMap.getString("name"    );
                                                                      long   dateTime      = valueMap.getLong  ("dateTime");
                                                                      long   size          = valueMap.getLong  ("size"    );

                                                                      // add entry data index
                                                                      entryIndexDataList.add(new EntryIndexData(entryId,
                                                                                                                jobName,
                                                                                                                archiveType,
                                                                                                                hostName,
                                                                                                                EntryTypes.DIRECTORY,
                                                                                                                directoryName,
                                                                                                                dateTime,
                                                                                                                size
                                                                                                               )
                                                                                            );
                                                                    }
                                                                    break;
                                                                  case LINK:
                                                                    {
                                                                      String linkName        = valueMap.getString("name"           );
                                                                      String destinationName = valueMap.getString("destinationName");
                                                                      long   dateTime        = valueMap.getLong  ("dateTime"       );

                                                                      // add entry data index
                                                                      entryIndexDataList.add(new EntryIndexData(entryId,
                                                                                                                jobName,
                                                                                                                archiveType,
                                                                                                                hostName,
                                                                                                                EntryTypes.LINK,
                                                                                                                linkName,
                                                                                                                dateTime
                                                                                                               )
                                                                                            );
                                                                    }
                                                                    break;
                                                                  case HARDLINK:
                                                                    {
                                                                      String fileName       = valueMap.getString("name"          );
                                                                      long   dateTime       = valueMap.getLong  ("dateTime"      );
                                                                      long   size           = valueMap.getLong  ("size"          );
                                                                      int    fragmentCount  = valueMap.getInt   ("fragmentCount" );

                                                                      // add entry data index
                                                                      entryIndexDataList.add(new EntryIndexData(entryId,
                                                                                                                jobName,
                                                                                                                archiveType,
                                                                                                                hostName,
                                                                                                                EntryTypes.HARDLINK,
                                                                                                                fileName,
                                                                                                                dateTime,
                                                                                                                size,
                                                                                                                fragmentCount
                                                                                                               )
                                                                                            );
                                                                    }
                                                                    break;
                                                                  case SPECIAL:
                                                                    {
                                                                      String name     = valueMap.getString("name"    );
                                                                      long   dateTime = valueMap.getLong  ("dateTime");

                                                                      // add entry data index
                                                                      entryIndexDataList.add(new EntryIndexData(entryId,
                                                                                                                jobName,
                                                                                                                archiveType,
                                                                                                                hostName,
                                                                                                                EntryTypes.SPECIAL,
                                                                                                                name,
                                                                                                                dateTime
                                                                                                               )
                                                                                            );
                                                                    }
                                                                    break;
                                                                }

                                                                // store number of entries
                                                                n[0] = i+1;

                                                                // check if aborted
                                                                if (isRequestUpdate() || (n[0] > limit))
                                                                {
                                                                  abort();
                                                                }
                                                              }
                                                            }
                                                           );
          BARServer.asyncCommandWait(entryTableCommand);
        }
        catch (Exception exception)
        {
          BARControl.logThrowable(exception);
        }

        // update entry table segment
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetEntryTable.isDisposed())
            {
              int i = 0;
              int n = widgetEntryTable.getItemCount();
              for (EntryIndexData entryIndexData : entryIndexDataList)
              {
                if ((offset+i) < n)
                {
                  TableItem tableItem = widgetEntryTable.getItem(offset+i);
                  if (!tableItem.isDisposed())
                  {
                    switch (entryIndexData.entryType)
                    {
                      case FILE:
                        Widgets.updateTableItem(tableItem,
                                                (Object)entryIndexData,
                                                entryIndexData.name,
                                                entryIndexData.entryType.getText(),
                                                Units.formatByteSize(entryIndexData.size),
                                                SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                               );
                        break;
                      case IMAGE:
                        Widgets.updateTableItem(tableItem,
                                                (Object)entryIndexData,
                                                entryIndexData.name,
                                                entryIndexData.entryType.getText(),
                                                Units.formatByteSize(entryIndexData.size),
                                                SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                               );
                        break;
                      case DIRECTORY:
                        Widgets.updateTableItem(tableItem,
                                                (Object)entryIndexData,
                                                entryIndexData.name,
                                                entryIndexData.entryType.getText(),
                                                (entryIndexData.size > 0L) ? Units.formatByteSize(entryIndexData.size) : "",
                                                SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                               );
                        break;
                      case LINK:
                        Widgets.updateTableItem(tableItem,
                                                (Object)entryIndexData,
                                                entryIndexData.name,
                                                entryIndexData.entryType.getText(),
                                                "",
                                                SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                               );
                        break;
                      case HARDLINK:
                        Widgets.updateTableItem(tableItem,
                                                (Object)entryIndexData,
                                                entryIndexData.name,
                                                entryIndexData.entryType.getText(),
                                                Units.formatByteSize(entryIndexData.size),
                                                SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                               );
                        break;
                      case SPECIAL:
                        Widgets.updateTableItem(tableItem,
                                                (Object)entryIndexData,
                                                entryIndexData.name,
                                                entryIndexData.entryType.getText(),
                                                Units.formatByteSize(entryIndexData.size),
                                                SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L))
                                               );
                        break;
                    }
                    tableItem.setChecked(checkedEntryIdSet.contains(entryIndexData.id));
                  }
                }

                i++;
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
            if (!widgetEntryTable.isDisposed())
            {
              widgetEntryTable.setRedraw(true);
            }
          }
        });
      }

      return n[0] >= limit;
    }

    /** refresh entry table items
     * @param updateOffsets segment offsets to update
     */
    private void updateEntryTableItems(HashSet<Integer> updateOffsets)
    {
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetEntryTable.isDisposed())
            {
              widgetEntryTable.setRedraw(false);
            }
          }
        });
      }
      try
      {
        Integer offsets[] = updateOffsets.toArray(new Integer[updateOffsets.size()]);
        for (Integer offset : offsets)
        {
          if (!updateEntryTableItems(offset)) break;
          updateOffsets.remove(offset);
        }
      }
      finally
      {
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetEntryTable.isDisposed())
            {
              widgetEntryTable.setRedraw(true);
            }
          }
        });
      }
    }
  }

  // -------------------------- constants -------------------------------
  // colors
  private final Color COLOR_MODIFIED;
  private final Color COLOR_INFO_FOREGROUND;
  private final Color COLOR_WARNING_FOREGROUND;
  private final Color COLOR_INFO_BACKGROUND;
  private final Color COLOR_NO_JOB_INFO;

  // images
  private final Image IMAGE_DIRECTORY;

  private final Image IMAGE_CLEAR;
  private final Image IMAGE_MARK_ALL;
  private final Image IMAGE_UNMARK_ALL;

  // date/time format
  private final SimpleDateFormat SIMPLE_DATE_FORMAT  = new SimpleDateFormat("yyyy-MM-dd EEE HH:mm:ss");
  private final SimpleDateFormat SIMPLE_DATE_FORMAT1 = new SimpleDateFormat("yyyy-MM-dd");
  private final SimpleDateFormat SIMPLE_DATE_FORMAT2 = new SimpleDateFormat("EEE");
  private final SimpleDateFormat SIMPLE_DATE_FORMAT3 = new SimpleDateFormat("HH:mm:ss");

  // index data comparator
  final Comparator<IndexData> indexIdComparator = new Comparator<IndexData>()
  {
    /** compare index data ids
     * @param indexData1,indexData2 index data
     * @return true iff index id equals
     */
    @Override
    public int compare(IndexData indexData1, IndexData indexData2)
    {
      return (indexData1.id == indexData2.id) ? 0 : 1;
    }
  };

  private final int STORAGE_TREE_MENU_START_INDEX = 0;
  private final int STORAGE_LIST_MENU_START_INDEX = 6;  // entry 0..4: new "..."; entry 5: separator

  // -------------------------- variables -------------------------------

  // global variable references
  private Shell                                       shell;
  private Display                                     display;
  private Clipboard                                   clipboard;
  private TabStatus                                   tabStatus;

  // widgets
  public  Composite                                   widgetTab;
  private TabFolder                                   widgetTabFolder;

  private TabFolder                                   widgetStorageTabFolderTitle;
  private TabFolder                                   widgetStorageTabFolder;
  private Tree                                        widgetStorageTree;
  private Shell                                       widgetStorageTreeToolTip = null;
  private Table                                       widgetStorageTable;
  private Shell                                       widgetStorageTableToolTip = null;
  private Text                                        widgetStorageFilter;
  private Combo                                       widgetStorageStateFilter;
  private Menu                                        widgetStorageAssignToMenu;
  final private IndexIdSet                            checkedIndexIdSet = new IndexIdSet();
  private WidgetEvent                                 enableMarkIndexEvent = new WidgetEvent<Boolean>();  // triggered when check all/none
  private WidgetEvent                                 checkedIndexEvent = new WidgetEvent();       // triggered when checked-state of some uuid/enity/storage change

  private Label                                       widgetEntryTableTitle;
  private Table                                       widgetEntryTable;
  private Shell                                       widgetEntryTableToolTip = null;
  private Text                                        widgetEntryFilter;
  private Combo                                       widgetEntryTypeFilter;
  private Button                                      widgetEntryNewestOnly;
  final private IndexIdSet                            checkedEntryIdSet = new IndexIdSet();
  private WidgetEvent                                 enableMarkEntriesEvent = new WidgetEvent<Boolean>();  // triggered when check all/none
  private WidgetEvent                                 checkedEntryEvent = new WidgetEvent();       // triggered when checked-state of some entry changed

  private UpdateStorageTreeTableThread                updateStorageTreeTableThread = new UpdateStorageTreeTableThread();
  private TabJobs                                     tabJobs;
  private HashSet<IndexData>                          selectedIndexDataHashSet = null;

  private Timer                                       updateAssignToTimer        = new Timer(true);
  private TimerTask                                   updateAssignToTimerTask    = null;
  private Object                                      assignToLock               = new Object();
  private java.util.List<UUIDIndexData>               assignToUUIDIndexDataList  = null;
  private Map<String,java.util.List<EntityIndexData>> assignToEntityIndexDataMap = null;

  private UpdateEntryTableThread                      updateEntryTableThread = new UpdateEntryTableThread();


  // ----------------------- native functions ---------------------------

  // --------------------------- methods --------------------------------

  /** show UUID index tool tip
   * @param entityIndexData entity index data
   * @param x,y position
   */
  private void showUUIDIndexToolTip(UUIDIndexData uuidIndexData, int x, int y)
  {
    int   row;
    Label label;

    if (widgetStorageTreeToolTip != null)
    {
      widgetStorageTreeToolTip.dispose();
    }

    if (!shell.isDisposed() && (uuidIndexData != null))
    {
      widgetStorageTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetStorageTreeToolTip.setBackground(COLOR_INFO_BACKGROUND);
      widgetStorageTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetStorageTreeToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Name")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,uuidIndexData.name);
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      if (Settings.debugLevel > 0)
      {
        assert (uuidIndexData.id == 0) || ((uuidIndexData.id & 0x0000000F) == 1) : uuidIndexData;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("UUID id")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,Long.toString(uuidIndexData.id >> 4));
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Job UUID")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,uuidIndexData.jobUUID);
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last executed")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,(uuidIndexData.lastExecutedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(uuidIndexData.lastExecutedDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last error")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
// TODO: code+data
      label = Widgets.newLabel(widgetStorageTreeToolTip,uuidIndexData.lastErrorData);
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total size")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(uuidIndexData.getTotalSize()),uuidIndexData.getTotalSize())));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total entries")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("{0}",uuidIndexData.getTotalEntryCount()));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total entries size")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(uuidIndexData.getTotalEntrySize()),uuidIndexData.getTotalEntrySize())));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      Widgets.showToolTip(widgetStorageTreeToolTip,x,y);
    }
  }

  /** show entity index tool tip
   * @param entityIndexData entity index data
   * @param x,y position
   */
  private void showEntityIndexToolTip(EntityIndexData entityIndexData, int x, int y)
  {
    int       row;
    Label     label;
    Separator separator;

    if (widgetStorageTreeToolTip != null)
    {
      widgetStorageTreeToolTip.dispose();
    }

    if (!shell.isDisposed() && (entityIndexData != null))
    {
      widgetStorageTreeToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetStorageTreeToolTip.setBackground(COLOR_INFO_BACKGROUND);
      widgetStorageTreeToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetStorageTreeToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      if (Settings.debugLevel > 0)
      {
        assert (entityIndexData.id & 0x0000000F) == 2 : entityIndexData;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Entity id")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,Long.toString(entityIndexData.id >> 4));
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Job UUID")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.jobUUID);
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Schedule UUID")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.scheduleUUID);
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Created")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,(entityIndexData.createdDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.createdDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Last error")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
// TODO: code+data
      label = Widgets.newLabel(widgetStorageTreeToolTip,entityIndexData.lastErrorData);
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total size")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(entityIndexData.getTotalSize()),entityIndexData.getTotalSize())));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total entries")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("{0}",entityIndexData.getTotalEntryCount()));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Total entries size")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(entityIndexData.getTotalEntrySize()),entityIndexData.getTotalEntrySize())));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Expire at")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTreeToolTip,(entityIndexData.expireDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.expireDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      if (entityIndexData.jobUUID.isEmpty() || entityIndexData.scheduleUUID.isEmpty())
      {
        separator = Widgets.newSeparator(widgetStorageTreeToolTip);
        separator.setForeground(COLOR_WARNING_FOREGROUND);
        separator.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(separator,row,0,TableLayoutData.WE,0,2);
        row++;

        label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("Warnings")+":");
        label.setForeground(COLOR_WARNING_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);

        if (entityIndexData.jobUUID.isEmpty())
        {
          label = Widgets.newLabel(widgetStorageTreeToolTip,BARControl.tr("no job info"));
          label.setForeground(COLOR_WARNING_FOREGROUND);
          label.setBackground(COLOR_INFO_BACKGROUND);
          Widgets.layout(label,row,1,TableLayoutData.WE);
          row++;
        }
      }

      Widgets.showToolTip(widgetStorageTreeToolTip,x,y);
    }
  }

  /** show storage index tool tip
   * @param storageIndexData storage index data
   * @param x,y position
   */
  private void showStorageIndexToolTip(final StorageIndexData storageIndexData, int x, int y)
  {
    final Image IMAGE_CLIPBOARD = Widgets.loadImage(shell.getDisplay(),"clipboard.png");

    int       row;
    Label     label;
    Composite composite;
    Text      text;
    Button    button;
    Separator separator;

    if (widgetStorageTableToolTip != null)
    {
      widgetStorageTableToolTip.dispose();
    }

    if (!shell.isDisposed() && (storageIndexData != null))
    {
      widgetStorageTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetStorageTableToolTip.setBackground(COLOR_INFO_BACKGROUND);
      widgetStorageTableToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetStorageTableToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Job")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.jobName);
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Entity")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,
                                storageIndexData.archiveType
                               +((storageIndexData.createdDateTime > 0)
                                   ? ", "+SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.createdDateTime*1000L))
                                   : ""
                                )
                              );
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      if (Settings.debugLevel > 0)
      {
        assert (storageIndexData.entityId & 0x0000000F) == 2 : storageIndexData;

        label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Entity id")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTableToolTip,Long.toString(storageIndexData.entityId >> 4));
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Hostname")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.hostName);
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Storage")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      composite = Widgets.newComposite(widgetStorageTableToolTip);
      composite.setBackground(COLOR_INFO_BACKGROUND);
      composite.setLayout(new TableLayout(null,new double[]{1.0,0.0}));
      Widgets.layout(composite,row,1,TableLayoutData.WE);
      {
        label = Widgets.newLabel(composite,storageIndexData.name);
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,0,0,TableLayoutData.WE);

        button = new Button(composite,SWT.CENTER);
        button.setToolTipText(BARControl.tr("Copy name to clipboard."));
        button.setImage(IMAGE_CLIPBOARD);
        Widgets.layout(button,0,1,TableLayoutData.E);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Widgets.setClipboard(clipboard,storageIndexData.name);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }
      row++;

      if (Settings.debugLevel > 0)
      {
        assert (storageIndexData.id & 0x0000000F) == 3 : storageIndexData;

        label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Storage id")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetStorageTableToolTip,Long.toString(storageIndexData.id >> 4));
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Entries")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("{0}",storageIndexData.getTotalEntryCount()));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Size")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(storageIndexData.getTotalSize()),storageIndexData.getTotalSize())));
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("State")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.indexState.toString());
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Last checked")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,(storageIndexData.lastCheckedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(storageIndexData.lastCheckedDateTime*1000L)) : "-");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Error")+":");
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetStorageTableToolTip,storageIndexData.errorMessage);
      label.setForeground(COLOR_INFO_FOREGROUND);
      label.setBackground(COLOR_INFO_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      if (storageIndexData.jobUUID.isEmpty())
      {
        separator = Widgets.newSeparator(widgetStorageTableToolTip);
        separator.setForeground(COLOR_WARNING_FOREGROUND);
        separator.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(separator,row,0,TableLayoutData.WE,0,2);
        row++;

        label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("Warnings")+":");
        label.setForeground(COLOR_WARNING_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);

        if (storageIndexData.jobUUID.isEmpty())
        {
          label = Widgets.newLabel(widgetStorageTableToolTip,BARControl.tr("no job info"));
          label.setForeground(COLOR_WARNING_FOREGROUND);
          label.setBackground(COLOR_INFO_BACKGROUND);
          Widgets.layout(label,row,1,TableLayoutData.WE);
          row++;
        }
      }

      Widgets.showToolTip(widgetStorageTableToolTip,x,y);
    }
  }

  /** show entry data tool tip
   * @param entryIndexData entry index data
   * @param x,y position
   */
  private void showEntryToolTip(final EntryIndexData entryIndexData, int x, int y)
  {
    final Image IMAGE_CLIPBOARD = Widgets.loadImage(shell.getDisplay(),"clipboard.png");

    /** fragment data
     */
    class FragmentData
    {
      public long   storageId;
      public String storageName;
      public long   fragmentOffset;
      public long   fragmentSize;

      /** create fragment data
       * @param storageId storage index id
       * @param storageName storage name
       * @param fragmentOffset fragment offset
       * @param fragmentSize fragment size
       */
      public FragmentData(long storageId, String storageName, long fragmentOffset, long fragmentSize)
      {
        this.storageId      = storageId;
        this.storageName    = storageName;
        this.fragmentOffset = fragmentOffset;
        this.fragmentSize   = fragmentSize;
      }
    };

    final Color COLOR_FOREGROUND = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
    final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

    int         row;
    Label       label;
    Control     control;
    Composite   composite;
    Text        text;
    Button      button;
    final Table table;
    TableColumn tableColumn;
    TableItem   tableItem;

    final ArrayList<FragmentData> fragmentDataList = new ArrayList<FragmentData>();
    if (entryIndexData != null)
    {
      // get fragments list
      try
      {
        BARServer.executeCommand(StringParser.format("INDEX_ENTRY_FRAGMENT_LIST entryId=%lld",
                                                     entryIndexData.id
                                                    ),
                                 2,  // debugLevel
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     long   storageId      = valueMap.getLong  ("storageId"     );
                                     String storageName    = valueMap.getString("storageName"   );
                                     long   fragmentOffset = valueMap.getLong  ("fragmentOffset");
                                     long   fragmentSize   = valueMap.getLong  ("fragmentSize"  );

                                     fragmentDataList.add(new FragmentData(storageId,
                                                                           storageName,
                                                                           fragmentOffset,
                                                                           fragmentSize
                                                                          )
                                                         );
                                   }
                                 }
                                );
      }
      catch (BARException exception)
      {
        // ignored
        if (Settings.debugLevel > 0)
        {
          BARControl.printStackTrace(exception);
        }
      }
      catch (Exception exception)
      {
        // ignored
        BARControl.logThrowable(exception);
      }
    }

    if (widgetEntryTableToolTip != null)
    {
      widgetEntryTableToolTip.dispose();
    }

    if (!shell.isDisposed() && (entryIndexData != null))
    {
      widgetEntryTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetEntryTableToolTip.setBackground(COLOR_BACKGROUND);
      widgetEntryTableToolTip.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetEntryTableToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      if (Settings.debugLevel > 0)
      {
        assert ((entryIndexData.id & 0x0000000F) >= 5) && ((entryIndexData.id & 0x0000000F) <= 10) : entryIndexData;

        label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Entry id")+":");
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,0,TableLayoutData.W);
        label = Widgets.newLabel(widgetEntryTableToolTip,Long.toString(entryIndexData.id >> 4));
        label.setForeground(COLOR_INFO_FOREGROUND);
        label.setBackground(COLOR_INFO_BACKGROUND);
        Widgets.layout(label,row,1,TableLayoutData.WE);
        row++;
      }

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Job")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.jobName);
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Type")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.archiveType.getText());
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Hostname")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.hostName);
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Created")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,(entryIndexData.dateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L)) : "-");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      control = Widgets.newSpacer(widgetEntryTableToolTip);
      Widgets.layout(control,row,0,TableLayoutData.WE,0,2,0,0,SWT.DEFAULT,1,SWT.DEFAULT,1,SWT.DEFAULT,1);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Name")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      composite = Widgets.newComposite(widgetEntryTableToolTip);
      composite.setBackground(COLOR_BACKGROUND);
      composite.setLayout(new TableLayout(null,new double[]{1.0,0.0}));
      Widgets.layout(composite,row,1,TableLayoutData.WE);
      {
        text = Widgets.newStringView(composite,entryIndexData.name);
        text.setForeground(COLOR_FOREGROUND);
        text.setBackground(COLOR_BACKGROUND);
        Widgets.layout(text,0,0,TableLayoutData.WE);

        button = new Button(composite,SWT.CENTER);
        button.setToolTipText(BARControl.tr("Copy name to clipboard."));
        button.setImage(IMAGE_CLIPBOARD);
        Widgets.layout(button,0,1,TableLayoutData.E);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Widgets.setClipboard(clipboard,entryIndexData.name);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Type")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,entryIndexData.entryType.getText());
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Size")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,String.format(BARControl.tr("{0} ({1} {1,choice,0#bytes|1#byte|1<bytes})",Units.formatByteSize(entryIndexData.size),entryIndexData.size)));
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Date/Time")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,(entryIndexData.dateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entryIndexData.dateTime*1000L)) : "-");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetEntryTableToolTip,BARControl.tr("Fragments")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetEntryTableToolTip,(fragmentDataList.size() > 0) ? String.format("%d",fragmentDataList.size()) : "-");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      table = Widgets.newTable(widgetEntryTableToolTip,SWT.V_SCROLL|SWT.H_SCROLL);
      table.setForeground(COLOR_FOREGROUND);
      table.setBackground(COLOR_BACKGROUND);
      table.setHeaderVisible(false);
      Widgets.layout(table,row,1,TableLayoutData.NSWE,0,0,0,0,400,200);
      Widgets.addTableColumn(table,0,SWT.RIGHT,80, true);
      Widgets.addTableColumn(table,1,SWT.RIGHT,80, true);
      Widgets.addTableColumn(table,2,SWT.LEFT, 800,true);
      for (FragmentData fragmentData : fragmentDataList)
      {
        tableItem = new TableItem(table,SWT.NONE);
        tableItem.setText(0,Long.toString(fragmentData.fragmentOffset));
        tableItem.setText(1,Long.toString(fragmentData.fragmentSize));
        tableItem.setText(2,fragmentData.storageName);
      }
      row++;

      Widgets.showToolTip(widgetEntryTableToolTip,x,y);
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
    Composite   composite,subComposite,subSubComposite,subSubSubComposite;
    Label       label;
    Button      button;
    Pane        pane;
    Combo       combo;
    TreeColumn  treeColumn;
    TreeItem    treeItem;
    Text        text;
    TableColumn tableColumn;
    Control     control;
    DragSource  dragSource;
    DropTarget  dropTarget;

    // get shell, display, clipboard
    shell     = parentTabFolder.getShell();
    display   = shell.getDisplay();
    clipboard = new Clipboard(display);

    // get colors
    COLOR_MODIFIED           = display.getSystemColor(SWT.COLOR_GRAY);
    COLOR_INFO_FOREGROUND    = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
    COLOR_WARNING_FOREGROUND = display.getSystemColor(SWT.COLOR_RED);
    COLOR_INFO_BACKGROUND    = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);
    COLOR_NO_JOB_INFO        = new Color(null,0xFF,0xC0,0xA0);

    // get images
    IMAGE_DIRECTORY  = Widgets.loadImage(display,"directory.png");

    IMAGE_CLEAR      = Widgets.loadImage(display,"clear.png");
    IMAGE_MARK_ALL   = Widgets.loadImage(display,"mark.png");
    IMAGE_UNMARK_ALL = Widgets.loadImage(display,"unmark.png");

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Restore")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""),!BARServer.isSlave());
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
        if ((tabItems.length >= 3) && (tabItems[2] == tabItem))
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

          updateAssignToTimerTask = new TimerTask()
                                    {
                                      public void run()
                                      {
                                        updateAssignTo();
                                      }
                                    };
          updateAssignToTimer.schedule(updateAssignToTimerTask,0,30*1000);
        }
        else
        {
          if (updateAssignToTimerTask != null)
          {
            updateAssignToTimerTask.cancel();
            updateAssignToTimerTask = null;
          }
        }
      }
    });

    // create pane
    pane = Widgets.newPane(widgetTab,2,SWT.HORIZONTAL);
    Widgets.layout(pane,0,0,TableLayoutData.NSWE);
    pane.addListener(SWT.Resize,new Listener()
    {
      public void handleEvent(Event event)
      {
        Pane widget = (Pane)event.widget;
        Settings.restorePaneSizes = new Settings.PaneSizes(widget.getSizes());
      }
    });
    pane.setSizes(Settings.restorePaneSizes.sizes);

    // storage tree/list
    composite = pane.getComposite(0);
    composite.setLayout(new TableLayout(1.0,1.0));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);

    subComposite = Widgets.newComposite(composite);
    subComposite.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,4));
    Widgets.layout(subComposite,0,0,TableLayoutData.NSWE);
    {
      widgetStorageTabFolder = Widgets.newTabFolder(subComposite);
      Widgets.layout(widgetStorageTabFolder,0,0,TableLayoutData.NSWE);

      widgetStorageTabFolderTitle = widgetStorageTabFolder;
      widgetStorageTabFolderTitle.addPaintListener(new PaintListener()
      {
        @Override
        public void paintControl(PaintEvent paintEvent)
        {
          TabFolder widget = (TabFolder)paintEvent.widget;
          GC        gc     = paintEvent.gc;
          Rectangle bounds = widget.getBounds();
          String    text;
          Point     size;

          // get current foreground color
          Color foreground = widget.getForeground();

          // title
          text = BARControl.tr("Storages");
          size = Widgets.getTextSize(gc,text);
          gc.setForeground(display.getSystemColor(SWT.COLOR_BLACK));
          gc.drawText(text,
                      (bounds.width-size.x)/2,
                      8,
                      true
                     );

          // total number of entries
          text = BARControl.tr("Count: {0} / Size: {1}",
                               updateStorageTreeTableThread.getTotalStorageCount(),
                               Units.formatByteSize(updateStorageTreeTableThread.getTotalStorageSize())
                              );
          size = Widgets.getTextSize(gc,text);
          gc.setForeground(foreground);
          gc.drawText(text,
                      bounds.width-size.x-8,
                      8,
                      true
                     );
        }
      });

      // tree
      tab = Widgets.addTab(widgetStorageTabFolder,BARControl.tr("Jobs"));
      tab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,2));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);

      widgetStorageTree = Widgets.newTree(tab,SWT.CHECK|SWT.MULTI,new Widgets.TreeItemRenderer()
      {
//TODO: use
        @Override
        public boolean render(TreeItem treeItem, int column, GC gc, int x, int y, int width, int height)
        {
          if (column == 2)
          {
            // draw column 2: date/time
            IndexData indexData = (IndexData)treeItem.getData();

            long dateTime = indexData.getDateTime();
            if (dateTime > 0)
            {
              // render date/time with date, day, time in columns
              String t1 = SIMPLE_DATE_FORMAT1.format(new Date(dateTime*1000L));
              String t2 = SIMPLE_DATE_FORMAT2.format(new Date(dateTime*1000L));
              String t3 = SIMPLE_DATE_FORMAT3.format(new Date(dateTime*1000L));
              Point  s1 = gc.textExtent(t1);
              Point  s2 = gc.textExtent("MMM");
              Point  s3 = gc.textExtent(t3);
              gc.drawText(t1,x+0            ,y,true);
              gc.drawText(t2,x+s1.x+2       ,y,true);
              gc.drawText(t3,x+s1.x+2+s2.x+2,y,true);
            }
            else
            {
              gc.drawText("-", x+0, y, true);
            }

            return true;
          }
          else
          {
            return false;
          }
        }
      });
      widgetStorageTree.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0}));
      Widgets.layout(widgetStorageTree,1,0,TableLayoutData.NSWE);

      SelectionListener storageTreeColumnSelectionListener = new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
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
      Listener storageTreeColumnResizeListener = new Listener()
      {
        public void handleEvent(Event event)
        {
          Settings.storageTreeColumns = new Settings.ColumnWidth(Widgets.getTreeColumnWidth(widgetStorageTree));
        }
      };
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("Name"),    SWT.LEFT, 400,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn.addListener(SWT.Resize,storageTreeColumnResizeListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("Hostname"),SWT.LEFT, 150,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn.addListener(SWT.Resize,storageTreeColumnResizeListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("Created"), SWT.LEFT, 170,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for created date/time."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn.addListener(SWT.Resize,storageTreeColumnResizeListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("Size"),    SWT.RIGHT,100,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for size."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn.addListener(SWT.Resize,storageTreeColumnResizeListener);
      treeColumn = Widgets.addTreeColumn(widgetStorageTree,BARControl.tr("State"),   SWT.LEFT,  60,true);
      treeColumn.setToolTipText(BARControl.tr("Click to sort for state."));
      treeColumn.addSelectionListener(storageTreeColumnSelectionListener);
      treeColumn.addListener(SWT.Resize,storageTreeColumnResizeListener);
      Widgets.setTreeColumnWidth(widgetStorageTree,Settings.storageTreeColumns.width);

      // special case for drawing date/time
      widgetStorageTree.addListener(SWT.EraseItem, new Listener()
      {
        public void handleEvent(Event event)
        {
          TreeItem treeItem = (TreeItem)event.item;

          if (event.index == 2)
          {
            // Note: do not draw column 2 = date/time
            event.detail &= ~SWT.FOREGROUND;
          }
        }
      });
      widgetStorageTree.addListener(SWT.PaintItem, new Listener()
      {
        public void handleEvent(Event event)
        {
          TreeItem treeItem = (TreeItem)event.item;

          if (event.index == 2)
          {
            // draw column 2 => date/time
            IndexData indexData = (IndexData)treeItem.getData();
            if (indexData != null)
            {
              long dateTime = indexData.getDateTime();
              if (dateTime > 0)
              {
                String t1 = SIMPLE_DATE_FORMAT1.format(new Date(dateTime*1000L));
                String t2 = SIMPLE_DATE_FORMAT2.format(new Date(dateTime*1000L));
                String t3 = SIMPLE_DATE_FORMAT3.format(new Date(dateTime*1000L));
                Point  s1 = event.gc.textExtent(t1);
                Point  s2 = event.gc.textExtent("MMM");
                Point  s3 = event.gc.textExtent(t3);
                event.gc.drawText(t1,event.x+0            ,event.y+(event.height-s1.y)/2,true);
                event.gc.drawText(t2,event.x+s1.x+2       ,event.y+(event.height-s2.y)/2,true);
                event.gc.drawText(t3,event.x+s1.x+2+s2.x+2,event.y+(event.height-s3.y)/2,true);
              }
              else
              {
                String t1 = "-";
                Point  s1 = event.gc.textExtent(t1);
                event.gc.drawText(t1, event.x+0, event.y+(event.height-s1.y)/2, true);
              }
            }
          }
        }
      });
      widgetStorageTree.addListener(SWT.Expand,new Listener()
      {
        @Override
        public void handleEvent(final Event event)
        {
          TreeItem treeItem = (TreeItem)event.item;

          if (!treeItem.isDisposed())
          {
            treeItem.removeAll();
            updateStorageTreeTableThread.updateTreeItem(treeItem);
          }
          if (!treeItem.isDisposed())
          {
            treeItem.setExpanded(true);
          }
        }
      });
      widgetStorageTree.addListener(SWT.Collapse,new Listener()
      {
        @Override
        public void handleEvent(final Event event)
        {
          TreeItem treeItem = (TreeItem)event.item;

          if (!treeItem.isDisposed())
          {
            Widgets.removeAllTreeItems(treeItem);
            treeItem.setExpanded(false);
            new TreeItem(treeItem,SWT.NONE);
          }
        }
      });
      widgetStorageTree.addListener(SWT.MouseDoubleClick,new Listener()
      {
        @Override
        public void handleEvent(final Event event)
        {
          TreeItem treeItem = widgetStorageTree.getItem(new Point(event.x,event.y));
          if ((treeItem != null) && !treeItem.isDisposed())
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
              // nothing to do
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
          if ((treeItem != null) && !treeItem.isDisposed())
          {
            IndexData indexData = (IndexData)treeItem.getData();
            if (indexData != null)
            {
              if      (indexData instanceof UUIDIndexData)
              {
                Widgets.notify(shell,BARControl.USER_EVENT_SELECT_JOB,((UUIDIndexData)indexData).jobUUID);

                if (selectionEvent.detail == SWT.CHECK)
                {
                  // set/reset UUID checked
                  boolean isChecked = Widgets.getTreeItemChecked(treeItem);
                  setStorageList(indexData.id,isChecked);

                  // clear sub-entities if checked
                  if (isChecked)
                  {
                    for (TreeItem entityTreeItem : ((TreeItem)selectionEvent.item).getItems())
                    {
                      if ((entityTreeItem != null) && !entityTreeItem.isDisposed())
                      {
                        IndexData entityIndexData = (IndexData)entityTreeItem.getData();
                        if ((entityIndexData != null) && (entityIndexData instanceof EntityIndexData))
                        {
                          entityTreeItem.setChecked(false);
                          setStorageList(entityIndexData.id,false);

                          for (TreeItem storageTreeItem : entityTreeItem.getItems())
                          {
                            if ((storageTreeItem != null) && !storageTreeItem.isDisposed())
                            {
                              IndexData storageIndexData = (IndexData)storageTreeItem.getData();
                              if ((storageIndexData != null) && (storageIndexData instanceof StorageIndexData))
                              {
                                storageTreeItem.setChecked(false);
                              }
                            }
                          }
                        }
                      }
                    }
                  }

                  // trigger update checked
                  checkedIndexEvent.trigger();
                  updateStorageTreeTableThread.triggerUpdate();
                }
              }
              else if (indexData instanceof EntityIndexData)
              {
                if (selectionEvent.detail == SWT.CHECK)
                {
                  // set/reset entity checked
                  boolean isChecked = Widgets.getTreeItemChecked(treeItem);
                  setStorageList(indexData.id,isChecked);

                  // clear parent UUID if checked
                  if (isChecked)
                  {
                    TreeItem uuidTreeItem = ((TreeItem)selectionEvent.item).getParentItem();
                    if ((uuidTreeItem != null) && !uuidTreeItem.isDisposed())
                    {
                      IndexData uuidIndexData = (IndexData)uuidTreeItem.getData();
                      if ((uuidIndexData != null) && (uuidIndexData instanceof UUIDIndexData))
                      {
                        uuidTreeItem.setChecked(false);
                        setStorageList(uuidIndexData.id,false);
                      }
                    }
                  }

                  // trigger update checked
                  checkedIndexEvent.trigger();
                  updateStorageTreeTableThread.triggerUpdate();
                }
              }
              else if (indexData instanceof StorageIndexData)
              {
                // nothing to do
              }
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
            removeStoragesFromIndex();
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

              Widgets.setTreeItemChecked(treeItem,!Widgets.getTreeItemChecked(treeItem));

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
        @Override
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
          Point point = new Point(dragSourceEvent.x,dragSourceEvent.y);

          selectedIndexDataHashSet = getSelectedIndexData();
        }
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          dragSourceEvent.data = selectedIndexDataHashSet;
        }
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
          selectedIndexDataHashSet = null;
        }
      });
      dropTarget = new DropTarget(widgetStorageTree,DND.DROP_MOVE);
      dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance(),IndexDataTransfer.getInstance()});
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
        public void drop(DropTargetEvent dropTargetEvent)
        {
          if (dropTargetEvent.data != null)
          {
            Point point = display.map(null,widgetStorageTree,dropTargetEvent.x,dropTargetEvent.y);

            TreeItem treeItem = widgetStorageTree.getItem(point);
            if (treeItem != null)
            {
              HashSet<IndexData> fromIndexDataHashSet = (HashSet<IndexData>)dropTargetEvent.data;
              IndexData toIndexData                   = (IndexData)treeItem.getData();

              if      (toIndexData instanceof UUIDIndexData)
              {
                UUIDIndexData toUUIDIndexData = (UUIDIndexData)toIndexData;
                assignStorages(fromIndexDataHashSet,toUUIDIndexData);
              }
              else if (toIndexData instanceof EntityIndexData)
              {
                EntityIndexData toEntityIndexData = (EntityIndexData)toIndexData;
                assignStorages(fromIndexDataHashSet,toEntityIndexData);
              }
              else if (toIndexData instanceof StorageIndexData)
              {
                StorageIndexData toStorageIndexData = (StorageIndexData)toIndexData;
                if (treeItem != null)
                {
                  EntityIndexData toEntityIndexData = (EntityIndexData)treeItem.getParentItem().getData();
                  if (toEntityIndexData != null)
                  {
                    assignStorages(fromIndexDataHashSet,toEntityIndexData);
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
      tab = Widgets.addTab(widgetStorageTabFolder,BARControl.tr("Archives"),Settings.hasExpertRole());
      tab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,2));
      Widgets.layout(tab,0,0,TableLayoutData.NSWE);

      widgetStorageTable = Widgets.newTable(tab,SWT.CHECK|SWT.VIRTUAL, new Widgets.TableItemRenderer()
      {
//TODO: use
        @Override
        public boolean render(TableItem tableItem, int column, GC gc, int x, int y, int width, int height)
        {
          if (column == 2)
          {
            // draw column 2: date/time
            IndexData indexData = (IndexData)tableItem.getData();

            long dateTime = indexData.getDateTime();
            if (dateTime > 0)
            {
              // render date/time with date, day, time in columns
              String t1 = SIMPLE_DATE_FORMAT1.format(new Date(dateTime*1000L));
              String t2 = SIMPLE_DATE_FORMAT2.format(new Date(dateTime*1000L));
              String t3 = SIMPLE_DATE_FORMAT3.format(new Date(dateTime*1000L));
              Point  s1 = gc.textExtent(t1);
              Point  s2 = gc.textExtent("MMM");
              Point  s3 = gc.textExtent(t3);
              gc.drawText(t1,x+0            ,y,true);
              gc.drawText(t2,x+s1.x+2       ,y,true);
              gc.drawText(t3,x+s1.x+2+s2.x+2,y,true);
            }
            else
            {
              gc.drawText("-", x+0, y, true);
            }

            return true;
          }
          else
          {
            return false;
          }
        }
      });
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
          TableColumn         tableColumn         = (TableColumn)selectionEvent.widget;
          Table               table               = tableColumn.getParent();
          IndexDataComparator indexDataComparator = new IndexDataComparator(table,tableColumn);

          synchronized(table)
          {
            {
              BARControl.waitCursor();
            }
            try
            {
              Widgets.sortTableColumn(table,tableColumn,indexDataComparator);
            }
            finally
            {
              BARControl.resetCursor();
            }
          }
        }
      };
      Listener storageTableColumnResizeListener = new Listener()
      {
        public void handleEvent(Event event)
        {
          Settings.storageTableColumns = new Settings.ColumnWidth(Widgets.getTableColumnWidth(widgetStorageTable));
        }
      };
      tableColumn = Widgets.addTableColumn(widgetStorageTable,0,BARControl.tr("Name"),    SWT.LEFT, 450,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
      tableColumn.addListener(SWT.Resize,storageTableColumnResizeListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,1,BARControl.tr("Hostname"),SWT.LEFT, 150,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for hostname."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
      tableColumn.addListener(SWT.Resize,storageTableColumnResizeListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,2,BARControl.tr("Size"),    SWT.RIGHT, 60,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for size."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
      tableColumn.addListener(SWT.Resize,storageTableColumnResizeListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,3,BARControl.tr("Modified"),SWT.LEFT, 150,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for modification date/time."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
      tableColumn.addListener(SWT.Resize,storageTableColumnResizeListener);
      tableColumn = Widgets.addTableColumn(widgetStorageTable,4,BARControl.tr("State"),   SWT.LEFT,  60,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for state."));
      tableColumn.addSelectionListener(storageTableColumnSelectionListener);
      tableColumn.addListener(SWT.Resize,storageTableColumnResizeListener);
      Widgets.setTableColumnWidth(widgetStorageTable,Settings.storageTableColumns.width);
      Widgets.sortTable(widgetStorageTable,0,SWT.UP);

      // special case for drawing date/time
      widgetStorageTable.addListener(SWT.EraseItem, new Listener()
      {
        public void handleEvent(Event event)
        {
          TableItem tableItem = (TableItem)event.item;

          if (event.index == 3)
          {
            // Note: do not draw column 3 = date/time
            event.detail &= ~SWT.FOREGROUND;
          }
        }
      });
      widgetStorageTable.addListener(SWT.PaintItem, new Listener()
      {
        public void handleEvent(Event event)
        {
          TableItem tableItem = (TableItem)event.item;

          if (event.index == 3)
          {
            // draw column 3 => date/time
            IndexData indexData = (IndexData)tableItem.getData();
            if (indexData != null)
            {
              long dateTime = indexData.getDateTime();
              if (dateTime > 0)
              {
                String t1 = SIMPLE_DATE_FORMAT1.format(new Date(dateTime*1000L));
                String t2 = SIMPLE_DATE_FORMAT2.format(new Date(dateTime*1000L));
                String t3 = SIMPLE_DATE_FORMAT3.format(new Date(dateTime*1000L));
                Point  s1 = event.gc.textExtent(t1);
                Point  s2 = event.gc.textExtent("MMM");
                Point  s3 = event.gc.textExtent(t3);
                event.gc.drawText(t1,event.x+0            ,event.y+(event.height-s1.y)/2,true);
                event.gc.drawText(t2,event.x+s1.x+2       ,event.y+(event.height-s2.y)/2,true);
                event.gc.drawText(t3,event.x+s1.x+2+s2.x+2,event.y+(event.height-s3.y)/2,true);
              }
              else
              {
                String t1 = "-";
                Point  s1 = event.gc.textExtent(t1);
                event.gc.drawText(t1, event.x+0, event.y+(event.height-s1.y)/2, true);
              }
            }
          }
        }
      });
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
          if (event.detail == SWT.CHECK)
          {
            TableItem tableItem = widgetStorageTable.getItem(new Point(event.x,event.y));
            if (tableItem != null)
            {
              StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
              if (storageIndexData != null)
              {
                boolean isChecked = tableItem.getChecked();

                // toggle check
                tableItem.setChecked(!isChecked);
                setStorageList(storageIndexData.id,!isChecked);

                // trigger update checked
                checkedIndexEvent.trigger();

                updateStorageTreeTableThread.triggerUpdate();
              }
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
          if (selectionEvent.detail == SWT.CHECK)
          {
            TableItem tableItem = (TableItem)selectionEvent.item;
            if (tableItem != null)
            {
              StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
              if (storageIndexData != null)
              {
                boolean isChecked = tableItem.getChecked();

                // set/reset check
                setStorageList(storageIndexData.id,isChecked);

                // trigger update checked
                checkedIndexEvent.trigger();

                updateStorageTreeTableThread.triggerUpdate();
              }
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
            removeStoragesFromIndex();
          }
          else if (Widgets.isAccelerator(keyEvent,SWT.SPACE))
          {
            // toggle check
            for (TableItem tableItem : widgetStorageTable.getSelection())
            {
              IndexData indexData = (IndexData)tableItem.getData();

              tableItem.setChecked(!tableItem.getChecked());

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
        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Test")+"\u2026");
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            testStorages();
          }
        });

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
            refreshStorageIndex();
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Refresh all indices with error")+"\u2026",Settings.hasNormalRole());
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            refreshAllWithErrorStorageIndex();
          }
        });

        widgetStorageAssignToMenu = Widgets.addMenu(menu,BARControl.tr("Assign to job")+"\u2026",Settings.hasExpertRole());
        {
        }
        widgetStorageAssignToMenu.addListener(SWT.Show,new Listener()
        {
          public void handleEvent(Event event)
          {
            synchronized(assignToLock)
            {
              updateAssignToMenu(widgetStorageAssignToMenu,
                                 assignToUUIDIndexDataList,
                                 assignToEntityIndexDataMap
                                );
            }
          }
        });

        subMenu = Widgets.addMenu(menu,BARControl.tr("Set job type")+"\u2026",Settings.hasExpertRole());
        {
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("normal")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              setEntityType(ArchiveTypes.NORMAL);
            }
          });
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("full")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              setEntityType(ArchiveTypes.FULL);
            }
          });
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("incremental")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              setEntityType(ArchiveTypes.INCREMENTAL);
            }
          });
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("differential")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              setEntityType(ArchiveTypes.DIFFERENTIAL);
            }
          });
          menuItem = Widgets.addMenuItem(subMenu,
                                         null,
                                         BARControl.tr("continuous")
                                        );
          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              setEntityType(ArchiveTypes.CONTINUOUS);
            }
          });
        }

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Move to")+"\u2026",Settings.hasExpertRole());
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            moveEntityTo();
          }
        });

        Widgets.addMenuItemSeparator(menu);

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
            addStoragesToIndex();
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
            removeStoragesFromIndex();
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Remove all indices with error")+"\u2026",Settings.hasNormalRole());
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            removeAllWithErrorStorageIndex();
          }
        });

        Widgets.addMenuItemSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Mark all"));
        Widgets.addEventListener(new WidgetEventListener<Boolean>(menuItem,enableMarkIndexEvent)
        {
          @Override
          public void trigger(MenuItem menuItem, Boolean enabled)
          {
            menuItem.setEnabled(enabled);
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
            enableMarkIndexEvent.trigger(false);
            {
              setAllCheckedStorage(true);
              Widgets.refreshVirtualTable(widgetStorageTable);
              checkedIndexEvent.trigger();
            }
            enableMarkIndexEvent.trigger(true);
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Unmark all"));
        Widgets.addEventListener(new WidgetEventListener<Boolean>(menuItem,enableMarkIndexEvent)
        {
          @Override
          public void trigger(MenuItem menuItem, Boolean enabled)
          {
            menuItem.setEnabled(enabled);
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
            enableMarkIndexEvent.trigger(false);
            {
              setAllCheckedStorage(false);
              Widgets.refreshVirtualTable(widgetStorageTable);
              checkedIndexEvent.trigger();
            }
            enableMarkIndexEvent.trigger(true);
          }
        });

        Widgets.addMenuItemSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Restore")+"\u2026");
        Widgets.addEventListener(new WidgetEventListener(menuItem,checkedIndexEvent)
        {
          @Override
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(!checkedIndexIdSet.isEmpty());
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
            restore(RestoreTypes.ARCHIVES,checkedIndexIdSet);
          }
        });

        Widgets.addMenuItemSeparator(menu);

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
            deleteStorages();
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
      subSubComposite = Widgets.newComposite(subComposite);
      subSubComposite.setLayout(new TableLayout(null,new double[]{0.0,0.0,1.0,0.0,0.0,0.0}));
      Widgets.layout(subSubComposite,1,0,TableLayoutData.WE);
      {
        button = Widgets.newButton(subSubComposite,IMAGE_MARK_ALL);
        Widgets.layout(button,0,0,TableLayoutData.W);
        Widgets.addEventListener(new WidgetEventListener<Boolean>(button,enableMarkIndexEvent)
        {
          @Override
          public void trigger(Control control, Boolean enabled)
          {
            control.setEnabled(enabled);
          }
        });
        Widgets.addEventListener(new WidgetEventListener(button,checkedIndexEvent)
        {
          @Override
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (!checkedIndexIdSet.isEmpty())
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

            enableMarkIndexEvent.trigger(false);
            {
              if (!checkedIndexIdSet.isEmpty())
              {
                setAllCheckedStorage(false);

                Widgets.refreshVirtualTable(widgetStorageTable);
                checkedIndexEvent.trigger();

                button.setImage(IMAGE_MARK_ALL);
                button.setToolTipText(BARControl.tr("Mark all entries in list."));
              }
              else
              {
                setAllCheckedStorage(true);

                Widgets.refreshVirtualTable(widgetStorageTable);
                checkedIndexEvent.trigger();

                button.setImage(IMAGE_UNMARK_ALL);
                button.setToolTipText(BARControl.tr("Unmark all entries in list."));
              }
            }
            enableMarkIndexEvent.trigger(true);
          }
        });

        label = Widgets.newLabel(subSubComposite,BARControl.tr("Filter")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        widgetStorageFilter = Widgets.newText(subSubComposite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
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

        subSubSubComposite = Widgets.newComposite(subSubComposite,Settings.hasNormalRole());
        subSubSubComposite.setLayout(new TableLayout(null,0.0));
        Widgets.layout(subSubSubComposite,0,3,TableLayoutData.NONE);
        {
          label = Widgets.newLabel(subSubSubComposite,BARControl.tr("State")+":");
          Widgets.layout(label,0,0,TableLayoutData.W);

          widgetStorageStateFilter = Widgets.newOptionMenu(subSubSubComposite);
          widgetStorageStateFilter.setToolTipText(BARControl.tr("Storage states filter."));
          widgetStorageStateFilter.setItems(new String[]{"*",
                                                         BARControl.tr("ok"),
                                                         BARControl.tr("error"),
                                                         BARControl.tr("update"),
                                                         BARControl.tr("update requested"),
                                                         BARControl.tr("update/update requested"),
                                                         BARControl.tr("error/update/update requested"),
                                                         BARControl.tr("not assigned"),
                                                         BARControl.tr("no job info")
                                                        }
                                           );
          widgetStorageStateFilter.setText("*");
          Widgets.layout(widgetStorageStateFilter,0,1,TableLayoutData.W);
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

              String        jobUUID;
              IndexStateSet storageIndexStateSet;
              EntityStates  storageEntityState;
              switch (widget.getSelectionIndex())
              {
                case 0:  jobUUID = null; storageIndexStateSet = INDEX_STATE_SET_ALL;                                                                  storageEntityState = EntityStates.ANY;  break;
                case 1:  jobUUID = null; storageIndexStateSet = new IndexStateSet(IndexStates.OK);                                                    storageEntityState = EntityStates.ANY;  break;
                case 2:  jobUUID = null; storageIndexStateSet = new IndexStateSet(IndexStates.ERROR);                                                 storageEntityState = EntityStates.ANY;  break;
                case 3:  jobUUID = null; storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE);                                                storageEntityState = EntityStates.ANY;  break;
                case 4:  jobUUID = null; storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE_REQUESTED);                                      storageEntityState = EntityStates.ANY;  break;
                case 5:  jobUUID = null; storageIndexStateSet = new IndexStateSet(IndexStates.UPDATE,IndexStates.UPDATE_REQUESTED);                   storageEntityState = EntityStates.ANY;  break;
                case 6:  jobUUID = null; storageIndexStateSet = new IndexStateSet(IndexStates.ERROR,IndexStates.UPDATE,IndexStates.UPDATE_REQUESTED); storageEntityState = EntityStates.ANY;  break;
                case 7:  jobUUID = null; storageIndexStateSet = INDEX_STATE_SET_ALL;                                                                  storageEntityState = EntityStates.NONE; break;
                case 8:  jobUUID = "";   storageIndexStateSet = INDEX_STATE_SET_ALL;                                                                  storageEntityState = EntityStates.ANY;  break;
                default: jobUUID = null; storageIndexStateSet = new IndexStateSet(IndexStates.UNKNOWN);                                               storageEntityState = EntityStates.ANY;  break;
              }
              updateStorageTreeTableThread.triggerUpdateStorageState(jobUUID,storageIndexStateSet,storageEntityState);
            }
          });
          updateStorageTreeTableThread.triggerUpdateStorageState((String)null,INDEX_STATE_SET_ALL,EntityStates.ANY);
        }

        button = Widgets.newButton(subSubComposite,BARControl.tr("Restore")+"\u2026");
        button.setToolTipText(BARControl.tr("Start restoring selected archives."));
        button.setEnabled(false);
        Widgets.layout(button,0,4,TableLayoutData.DEFAULT,0,0,0,0,160,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,checkedIndexEvent)
        {
          @Override
          public void trigger(Control control)
          {
            control.setEnabled(!checkedIndexIdSet.isEmpty());
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
            restore(RestoreTypes.ARCHIVES,checkedIndexIdSet);
          }
        });
      }
    }

    // entries list
    composite = pane.getComposite(1);
    composite.setLayout(new TableLayout(1.0,1.0));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE);

    subComposite = Widgets.newComposite(composite);
    subComposite.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},1.0,4));
    Widgets.layout(subComposite,0,0,TableLayoutData.NSWE);
    {
      widgetEntryTableTitle = Widgets.newLabel(subComposite);
      Widgets.layout(widgetEntryTableTitle,0,0,TableLayoutData.WE);
      widgetEntryTableTitle.addPaintListener(new PaintListener()
      {
        @Override
        public void paintControl(PaintEvent paintEvent)
        {
          Label     widget = (Label)paintEvent.widget;
          GC        gc     = paintEvent.gc;
          Rectangle bounds = widget.getBounds();
          String    text;
          Point     size;

          // get current foreground color
          Color foreground = widget.getForeground();

          // title
          text = BARControl.tr("Entries");
          size = Widgets.getTextSize(gc,text);
          gc.setForeground(display.getSystemColor(SWT.COLOR_BLACK));
          gc.drawText(text,
                      (bounds.width-size.x)/2,
                      (bounds.height-size.y)/2,
                      true
                     );

          // total number of entries, total entry size
          text = BARControl.tr("Count: {0} / Size: {1}",
                               updateEntryTableThread.getTotalEntryCount(),
                               Units.formatByteSize(updateEntryTableThread.getTotalEntrySize())
                              );
          size = Widgets.getTextSize(gc,text);
          gc.setForeground(foreground);
          gc.drawText(text,
                      bounds.width-size.x-8,
                      (bounds.height-size.y)/2,
                      true
                     );
        }
      });

      widgetEntryTable = Widgets.newTable(subComposite,SWT.CHECK|SWT.VIRTUAL);
      widgetEntryTable.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0}));
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
          TableColumn tableColumn = (TableColumn)selectionEvent.widget;
          Table       table       = tableColumn.getParent();

          synchronized(widgetEntryTable)
          {
            {
              BARControl.waitCursor();
            }
            try
            {
              Widgets.setSortTableColumn(table,tableColumn);
              Widgets.refreshVirtualTable(table);
            }
            finally
            {
              BARControl.resetCursor();
            }
          }
        }
      };
      tableColumn = Widgets.addTableColumn(widgetEntryTable,0,BARControl.tr("Name"),     SWT.LEFT, 270,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for name."));
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetEntryTable,1,BARControl.tr("Type"),     SWT.LEFT,  90,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for type."));
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetEntryTable,2,BARControl.tr("Size"),     SWT.RIGHT, 60,true);
      tableColumn.setToolTipText(BARControl.tr("Click to sort for size."));
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn = Widgets.addTableColumn(widgetEntryTable,3,BARControl.tr("Date/Time"),SWT.LEFT, 140,true);
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
          if (event.detail == SWT.CHECK)
          {
            TableItem tableItem = widgetEntryTable.getItem(new Point(event.x,event.y));
            if (tableItem != null)
            {
              EntryIndexData entryIndexData = (EntryIndexData)tableItem.getData();
              if (entryIndexData != null)
              {
                boolean isChecked = tableItem.getChecked();

                // toggle check
                tableItem.setChecked(!isChecked);
                setEntryList(entryIndexData.id,!isChecked);

                // trigger update entries
                checkedEntryEvent.trigger();
              }
            }
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
          if (selectionEvent.detail == SWT.CHECK)
          {
            TableItem tableItem = (TableItem)selectionEvent.item;
            if (tableItem != null)
            {
              EntryIndexData entryIndexData = (EntryIndexData)tableItem.getData();
              if (entryIndexData != null)
              {
                boolean isChecked = tableItem.getChecked();

                // set/reset check
                setEntryList(entryIndexData.id,isChecked);

                // trigger update entries
                checkedEntryEvent.trigger();
              }
            }
          }
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
              Point point = display.getCursorLocation();
              if (point.x > 16) point.x -= 16;
              if (point.y > 16) point.y -= 16;

              showEntryToolTip(entryIndexData,point.x,point.y);
            }
          }
        }
      });
      Widgets.addEventListener(new WidgetEventListener(widgetEntryTable,checkedIndexEvent)
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
        Widgets.addEventListener(new WidgetEventListener<Boolean>(menuItem,enableMarkEntriesEvent)
        {
          @Override
          public void trigger(MenuItem menuItem, Boolean enabled)
          {
            menuItem.setEnabled(enabled);
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
            enableMarkEntriesEvent.trigger(false);
            {
              setAllCheckedEntries(true);
              Widgets.refreshVirtualTable(widgetEntryTable);
              checkedEntryEvent.trigger();
            }
            enableMarkEntriesEvent.trigger(true);
          }
        });

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Unmark all"));
        Widgets.addEventListener(new WidgetEventListener<Boolean>(menuItem,enableMarkEntriesEvent)
        {
          @Override
          public void trigger(MenuItem menuItem, Boolean enabled)
          {
            menuItem.setEnabled(enabled);
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
            enableMarkEntriesEvent.trigger(false);
            {
              setAllCheckedEntries(false);
              Widgets.refreshVirtualTable(widgetEntryTable);
              checkedEntryEvent.trigger();
            }
            enableMarkEntriesEvent.trigger(true);
          }
        });

        Widgets.addMenuItemSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,BARControl.tr("Restore")+"\u2026");
        menuItem.setEnabled(false);
        Widgets.addEventListener(new WidgetEventListener(menuItem,checkedEntryEvent)
        {
          @Override
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(!checkedEntryIdSet.isEmpty());
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
            restore(RestoreTypes.ENTRIES,checkedEntryIdSet);
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
      subSubComposite = Widgets.newComposite(subComposite);
      subSubComposite.setLayout(new TableLayout(null,new double[]{0.0,0.0,1.0,0.0,0.0,0.0}));
      Widgets.layout(subSubComposite,2,0,TableLayoutData.WE);
      {
        button = Widgets.newButton(subSubComposite,IMAGE_MARK_ALL);
        Widgets.layout(button,0,0,TableLayoutData.E);
        Widgets.addEventListener(new WidgetEventListener<Boolean>(button,enableMarkEntriesEvent)
        {
          @Override
          public void trigger(Control control, Boolean enabled)
          {
            control.setEnabled(enabled);
          }
        });
        Widgets.addEventListener(new WidgetEventListener(button,checkedEntryEvent)
        {
          @Override
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (!checkedEntryIdSet.isEmpty())
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

            enableMarkEntriesEvent.trigger(false);
            {
              setAllCheckedEntries(checkedEntryIdSet.isEmpty());
              Widgets.refreshVirtualTable(widgetEntryTable);
              checkedEntryEvent.trigger();
              if (!checkedEntryIdSet.isEmpty())
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
            enableMarkEntriesEvent.trigger(true);
          }
        });

        label = Widgets.newLabel(subSubComposite,BARControl.tr("Filter")+":");
        Widgets.layout(label,0,1,TableLayoutData.W);

        widgetEntryFilter = Widgets.newText(subSubComposite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
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
            widget.setBackground(COLOR_MODIFIED);
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Text widget = (Text)selectionEvent.widget;
            updateEntryTableThread.triggerUpdateEntryName(widget.getText());
            widget.setBackground(COLOR_MODIFIED);
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
            widget.setBackground(COLOR_MODIFIED);
          }
        });

        subSubSubComposite = Widgets.newComposite(subSubComposite,Settings.hasNormalRole());
        subSubSubComposite.setLayout(new TableLayout(null,0.0));
        Widgets.layout(subSubSubComposite,0,3,TableLayoutData.NONE);
        {
          widgetEntryTypeFilter = Widgets.newOptionMenu(subSubSubComposite);
          widgetEntryTypeFilter.setToolTipText(BARControl.tr("Entry type."));
          Widgets.setOptionMenuItems(widgetEntryTypeFilter,new Object[]{"*",                         EntryTypes.ANY,
                                                                        BARControl.tr("files"),      EntryTypes.FILE,
                                                                        BARControl.tr("images"),     EntryTypes.IMAGE,
                                                                        BARControl.tr("directories"),EntryTypes.DIRECTORY,
                                                                        BARControl.tr("links"),      EntryTypes.LINK,
                                                                        BARControl.tr("hardlinks"),  EntryTypes.HARDLINK,
                                                                        BARControl.tr("special"),    EntryTypes.SPECIAL
                                                                       }
                                    );
          Widgets.setSelectedOptionMenuItem(widgetEntryTypeFilter,EntryTypes.ANY);
          Widgets.layout(widgetEntryTypeFilter,0,0,TableLayoutData.W);
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

              clearEntryList();
              checkedEntryEvent.trigger();

              updateEntryTableThread.triggerUpdateEntryType(entryType);
            }
          });

          widgetEntryNewestOnly = Widgets.newCheckbox(subSubSubComposite,BARControl.tr("newest only"));
          widgetEntryNewestOnly.setToolTipText(BARControl.tr("When this checkbox is enabled, only show newest entry instances and hide all older entry instances."));
          Widgets.layout(widgetEntryNewestOnly,0,1,TableLayoutData.W);
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

              clearEntryList();
              checkedEntryEvent.trigger();

              updateEntryTableThread.triggerUpdateNewestOnly(newestOnly);
            }
          });
        }

        button = Widgets.newButton(subSubComposite,BARControl.tr("Restore")+"\u2026");
        button.setToolTipText(BARControl.tr("Start restoring selected entries."));
        button.setEnabled(false);
        Widgets.layout(button,0,5,TableLayoutData.DEFAULT,0,0,0,0,160,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,checkedEntryEvent)
        {
          @Override
          public void trigger(Control control)
          {
            control.setEnabled(!checkedEntryIdSet.isEmpty());
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
            restore(RestoreTypes.ENTRIES,checkedEntryIdSet);
          }
        });
      }
    }

    // listeners
    shell.addListener(BARControl.USER_EVENT_SELECT_SERVER,new Listener()
    {
      public void handleEvent(Event event)
      {
        if (!widgetStorageFilter.isDisposed())
        {
          widgetStorageFilter.setText("");
        }
        if (!widgetStorageStateFilter.isDisposed())
        {
          widgetStorageStateFilter.select(0);
        }
        setAllCheckedStorage(false);
        Widgets.refreshVirtualTable(widgetStorageTable);
        updateStorageTreeTableThread.triggerUpdate("",INDEX_STATE_SET_ALL,EntityStates.ANY,true);

        if (!widgetEntryFilter.isDisposed())
        {
          widgetEntryFilter.setText("");
        }
        Widgets.setSelectedOptionMenuItem(widgetEntryTypeFilter,EntryTypes.ANY);
        if (!widgetEntryNewestOnly.isDisposed())
        {
          widgetEntryNewestOnly.setSelection(false);
        }
        setAllCheckedEntries(false);
        Widgets.refreshVirtualTable(widgetEntryTable);
        updateEntryTableThread.triggerUpdate("","*",false,true);
      }
    });

    // start storage/entry update threads
    updateStorageTreeTableThread.start();
    updateEntryTableThread.start();
  }

  /** set tab status reference
   * @param tabStatus tab status object
   */
  public void setTabStatus(TabStatus tabStatus)
  {
    this.tabStatus = tabStatus;
  }

  /** set jobs tab
   * @param tabJobs jobs tab
   */
  void setTabJobs(TabJobs tabJobs)
  {
    this.tabJobs = tabJobs;
  }

  //-----------------------------------------------------------------------

  /** set/clear index list
   * @param indexId index id
   * @param checked true for set checked, false for clear checked
   */
  private void setStorageList(long indexId, boolean checked)
  {
    if (indexId != 0)
    {
      try
      {
        if (checked)
        {
          BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST_ADD storageIds=%ld",
                                                       indexId
                                                      ),
                                   2  // debugLevel
                                  );
        }
        else
        {
          BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST_REMOVE storageIds=%ld",
                                                       indexId
                                                      ),
                                   2  // debugLevel
                                  );
        }
      }
      catch (Exception exception)
      {
        throw new CommunicationError(exception);
      }

      checkedIndexIdSet.set(indexId,checked);
    }
  }

  /** clear selected index entries
   */
  private void clearStorageList()
  {
    try
    {
      BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST_CLEAR"),2);
    }
    catch (Exception exception)
    {
      throw new CommunicationError(exception);
    }

    checkedIndexIdSet.clear();
  }

  /** set selected index entries
   * @param storageIdSet storage id set
   */
  private void setStorageList(IndexIdSet storageIdSet)
  {
    StringBuilder buffer = new StringBuilder();

    try
    {
      BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST_CLEAR"),2);
    }
    catch (Exception exception)
    {
      throw new CommunicationError(exception);
    }

    // set list
    Long storageIds[] = storageIdSet.toArray(new Long[storageIdSet.size()]);
    int i = 0;
    while (i < storageIds.length)
    {
      int n = storageIds.length-i; if (n > 1024) n = 1024;
      try
      {
        BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST_ADD storageIds=%s",
                                                     StringUtils.join(storageIds,i,n,',')
                                                    ),
                                 1  // debugLevel
                                );
      }
      catch (Exception exception)
      {
        throw new CommunicationError(exception);
      }
      i += n;
    }

    for (Long storageId : storageIdSet)
    {
      checkedIndexIdSet.set(storageId,true);
    }
  }

  /** set selected index entries
   * @param indexDataHashSet index data set
   */
  private void setStorageList(HashSet<IndexData> indexDataHashSet)
  {
    clearStorageList();
//TODO: optimize send more than one entry?
    for (IndexData indexData : indexDataHashSet)
    {
      setStorageList(indexData.id,true);
    }
  }

  /** set/clear checked all storage entries
   * @param checked true for set checked, false for clear checked
   */
  private void setAllCheckedStorage(final boolean checked)
  {
    final int MAX_CONFIRM_ENTRIES = 1000;

    ValueMap valueMap = new ValueMap();

    if (!widgetStorageTabFolder.isDisposed())
    {
      switch (widgetStorageTabFolder.getSelectionIndex())
      {
        case 0:
          // tree view
          {
            // check/uncheck all entries
            IndexIdSet indexIdSet = new IndexIdSet();
            for (TreeItem uuidTreeItem : widgetStorageTree.getItems())
            {
              UUIDIndexData uuidIndexData = (UUIDIndexData)uuidTreeItem.getData();

              Widgets.setTreeItemChecked(uuidTreeItem,checked);
              checkedIndexIdSet.set(uuidIndexData.id,checked);

              if (uuidTreeItem.getExpanded())
              {
                for (TreeItem entityTreeItem : uuidTreeItem.getItems())
                {
                  EntityIndexData entityIndexData = (EntityIndexData)entityTreeItem.getData();

                  Widgets.setTreeItemChecked(entityTreeItem,checked);
                  checkedIndexIdSet.set(entityIndexData.id,checked);

                  if (entityTreeItem.getExpanded())
                  {
                    for (TreeItem storageTreeItem : entityTreeItem.getItems())
                    {
                      StorageIndexData storageIndexData = (StorageIndexData)storageTreeItem.getData();

                      indexIdSet.set(storageIndexData.id,checked);
                    }
                  }
                }
              }
            }
            setStorageList(indexIdSet);
          }
          break;
        case 1:
          // table view
          {
            final int     storageCount[] = {0};
            final boolean doit[]         = {false};

            if (checked)
            {
              try
              {
                storageCount[0] = BARServer.getInt(StringParser.format("INDEX_STORAGES_LIST_INFO indexStateSet=%s indexModeSet=* name=%'S",
                                                                       updateStorageTreeTableThread.getStorageIndexStateSet().nameList("|"),
                                                                       updateStorageTreeTableThread.getStorageName()
                                                                      ),
                                                   1,  // debugLevel
                                                   "storageCount"
                                                  );
                if (storageCount[0] > MAX_CONFIRM_ENTRIES)
                {
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      doit[0] = Dialogs.confirm(shell,
                                                Dialogs.booleanFieldUpdater(Settings.class,"showEntriesMarkInfo"),
                                                BARControl.tr("There are {0} entries. Really mark all entries?",
                                                              storageCount[0]
                                                             ),
                                                true
                                               );
                    }
                  });
                }
                else
                {
                  doit[0] = true;
                }
              }
              catch (Exception exception)
              {
                BARControl.logThrowable(exception);
              }
            }
            else
            {
              doit[0] = true;
            }

            if (checked)
            {
              if (doit[0])
              {
                // check/uncheck all entries
                clearStorageList();

                final BusyDialog busyDialog = new BusyDialog(shell,
                                                             BARControl.tr("Mark entries"),
                                                             500,
                                                             100,
                                                             (String)null,
                                                             BusyDialog.PROGRESS_BAR0|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                                            );
                try
                {
                  final IndexIdSet indexIdSet = new IndexIdSet();
                  final int        n[]        = new int[]{0};
                  busyDialog.setMaximum(storageCount[0]);

                  BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=* indexStateSet=%s indexModeSet=* name=%'S",
                                                               updateStorageTreeTableThread.getStorageIndexStateSet().nameList("|"),
                                                               updateStorageTreeTableThread.getStorageName()
                                                              ),
                                           2,  // debugLevel
                                           new Command.ResultHandler()
                                           {
                                             @Override
                                             public void handle(int i, ValueMap valueMap)
                                             {
                                               long storageId = valueMap.getLong("storageId");

                                               indexIdSet.set(storageId,checked);

                                               n[0]++;
                                               busyDialog.updateProgressBar(n[0]);

                                               if (busyDialog.isAborted())
                                               {
                                                 abort();
                                               }
                                             }
                                           }
                                          );

                  setStorageList(indexIdSet);

                  busyDialog.close();
                }
                catch (BARException exception)
                {
                  busyDialog.close();
                  if (exception.code != BARException.ABORTED)
                  {
                    Dialogs.error(shell,BARControl.tr("Cannot mark all storages!\n\n(error: {0})",exception.getMessage()));
                  }
                }
                catch (Exception exception)
                {
                  busyDialog.close();
                  Dialogs.error(shell,BARControl.tr("Cannot mark all storages!\n\n(error: {0})",exception.getMessage()));
                  BARControl.logThrowable(exception);
                }
              }
            }
            else
            {
              // clear all checked ids
              clearStorageList();
            }
          }
          break;
      }

      updateStorageTreeTableThread.triggerUpdate();
    }
  }

  /** get selected storage
   * @return selected index hash set
   */
  private HashSet<IndexData> getSelectedIndexData()
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();
    IndexData          indexData;

    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        // tree view
        for (TreeItem treeItem : widgetStorageTree.getSelection())
        {
          indexData = (IndexData)treeItem.getData();
          if (indexData != null)
          {
            indexDataHashSet.add(indexData);
          }
        }
        for (TreeItem treeItem : Widgets.getAllTreeItems(widgetStorageTree))
        {
          if (treeItem.getChecked())
          {
            indexData = (IndexData)treeItem.getData();
            if (indexData != null)
            {
              indexDataHashSet.add(indexData);
            }
          }
        }
        break;
      case 1:
        // table view
        for (TableItem tableItem : widgetStorageTable.getSelection())
        {
          indexData = (IndexData)tableItem.getData();
          if ((indexData != null) && !tableItem.getGrayed())
          {
            indexDataHashSet.add(indexData);
          }
        }
        for (TableItem tableItem : widgetStorageTable.getItems())
        {
          if (tableItem.getChecked())
          {
            indexData = (IndexData)tableItem.getData();
            if ((indexData != null) && !tableItem.getGrayed())
            {
              indexDataHashSet.add(indexData);
            }
          }
        }
        break;
    }

    return indexDataHashSet;
  }

  /** clear selected storage
   */
  private void clearSelectedIndexData()
  {
    widgetStorageTree.deselectAll();
    widgetStorageTable.deselectAll();
  }

  /** update assign-to
   */
  private void updateAssignTo()
  {
    try
    {
      // get UUID index data list
      final java.util.List<UUIDIndexData> assignToUUIDIndexDataList = new ArrayList<UUIDIndexData>();
      BARServer.executeCommand(StringParser.format("INDEX_UUID_LIST indexStateSet=* indexModeSet=*"),
                               2,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   long   uuidId               = valueMap.getLong  ("uuidId"              );
                                   String jobUUID              = valueMap.getString("jobUUID"             );
                                   String name                 = valueMap.getString("name"                );
                                   long   lastExecutedDateTime = valueMap.getLong  ("lastExecutedDateTime");
                                   int    lastErrorCode        = valueMap.getInt   ("lastErrorCode"       );
                                   String lastErrorData        = valueMap.getString("lastErrorData"       );
                                   long   totalSize            = valueMap.getLong  ("totalSize"           );
                                   long   totalEntryCount      = valueMap.getLong  ("totalEntryCount"     );
                                   long   totalEntrySize       = valueMap.getLong  ("totalEntrySize",     Long.MAX_VALUE);

                                   // add UUID index data
                                   assignToUUIDIndexDataList.add(new UUIDIndexData(uuidId,
                                                                                   jobUUID,
                                                                                   name,
                                                                                   lastExecutedDateTime,
                                                                                   lastErrorCode,
                                                                                   lastErrorData,
                                                                                   totalSize,
                                                                                   totalEntryCount,
                                                                                   totalEntrySize
                                                                                  )
                                                                );
                                 }
                               }
                              );

      // get entity index data list
      final Map<String,java.util.List<EntityIndexData>> assignToEntityIndexDataMap = new HashMap<String,java.util.List<EntityIndexData>>();
      BARServer.executeCommand(StringParser.format("INDEX_ENTITY_LIST indexStateSet=* indexModeSet=*"),
                               2,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   long         entityId         = valueMap.getLong  ("entityId"                      );
                                   String       jobUUID          = valueMap.getString("jobUUID"                       );
                                   String       scheduleUUID     = valueMap.getString("scheduleUUID"                  );
                                   ArchiveTypes archiveType      = valueMap.getEnum  ("archiveType",ArchiveTypes.class);
                                   long         createdDateTime  = valueMap.getLong  ("createdDateTime"               );
                                   int          lastErrorCode    = valueMap.getInt   ("lastErrorCode"                 );
                                   String       lastErrorData    = valueMap.getString("lastErrorData"                 );
                                   long         totalSize        = valueMap.getLong  ("totalSize"                     );
                                   long         totalEntryCount  = valueMap.getLong  ("totalEntryCount"               );
                                   long         totalEntrySize   = valueMap.getLong  ("totalEntrySize",               Long.MAX_VALUE);
                                   long         expireDateTime   = valueMap.getLong  ("expireDateTime"                );

                                   // get entity data list
                                   java.util.List<EntityIndexData> entityIndexDataList = assignToEntityIndexDataMap.get(jobUUID);
                                   if (entityIndexDataList == null)
                                   {
                                     entityIndexDataList = new ArrayList<EntityIndexData>();
                                     assignToEntityIndexDataMap.put(jobUUID,entityIndexDataList);
                                   }

                                   // add entity data index
                                   entityIndexDataList.add(new EntityIndexData(entityId,
                                                                               jobUUID,
                                                                               scheduleUUID,
                                                                               archiveType,
                                                                               createdDateTime,
                                                                               lastErrorCode,
                                                                               lastErrorData,
                                                                               totalSize,
                                                                               totalEntryCount,
                                                                               totalEntrySize,
                                                                               expireDateTime
                                                                              )
                                                        );
                                 }
                               }
                              );

      synchronized(assignToLock)
      {
        this.assignToUUIDIndexDataList  = assignToUUIDIndexDataList;
        this.assignToEntityIndexDataMap = assignToEntityIndexDataMap;
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

  /** update assign-to sub-menu
   * @param menu menu
   * @param uuidIndexDataList UUID inded data list
   * @param entityIndexDataMap entity index data map
   */
  private synchronized void updateAssignToMenu(Menu                                        menu,
                                               java.util.List<UUIDIndexData>               uuidIndexDataList,
                                               Map<String,java.util.List<EntityIndexData>> entityIndexDataMap
                                              )
  {
    // discard old menu items
    for (MenuItem menuItem : menu.getItems())
    {
      menuItem.dispose();
    }

    // update menu
    if (uuidIndexDataList != null)
    {
      Collections.sort(assignToUUIDIndexDataList,new Comparator<UUIDIndexData>()
      {
        @Override
        public int compare(UUIDIndexData uuidIndexData1, UUIDIndexData uuidIndexData2)
        {
          return uuidIndexData1.name.compareTo(uuidIndexData2.name);
        }
      });
      for (UUIDIndexData uuidIndexData : uuidIndexDataList)
      {
        Menu subMenu = Widgets.insertMenu(menu,
                                          findStorageMenuIndex(menu,uuidIndexData),
                                          (Object)uuidIndexData,
                                          uuidIndexData.name.replaceAll("&","&&")
                                         );
        uuidIndexData.setSubMenu(subMenu);

        updateAssignToMenu(subMenu,
                           uuidIndexData.jobUUID,
                           entityIndexDataMap.get(uuidIndexData.jobUUID)
                          );
      }
    }
  }

  /** update assign-to sub-menu
   * @param menu menu
   * @param jobUUID job UUID
   * @param entityIndexDataList entity index data list
   */
  private void updateAssignToMenu(Menu                            subMenu,
                                  String                          jobUUID,
                                  java.util.List<EntityIndexData> entityIndexDataList
                                 )
  {
    Menu subSubMenuNormal,subSubMenuFull,subSubMenuIncremental,subSubMenuDifferential,subSubMenuContinuous;
    Menu subSubMenu;

    // discard old menu items
    for (MenuItem menuItem : subMenu.getItems())
    {
      menuItem.dispose();
    }

    // add normal menu items
    subSubMenuNormal = Widgets.addMenu(subMenu,
                                       null,
                                       BARControl.tr("normal")
                                      );
    updateAssignToMenu(subSubMenuNormal,
                       jobUUID,
                       ArchiveTypes.NORMAL,
                       entityIndexDataList
                      );

    // add full menu items
    subSubMenuFull = Widgets.addMenu(subMenu,
                                     null,
                                     BARControl.tr("full")
                                    );
    updateAssignToMenu(subSubMenuFull,
                       jobUUID,
                       ArchiveTypes.FULL,
                       entityIndexDataList
                      );

    // add incremental menu items
    subSubMenuIncremental = Widgets.addMenu(subMenu,
                                            null,
                                            BARControl.tr("incremental")
                                           );
    updateAssignToMenu(subSubMenuIncremental,
                       jobUUID,
                       ArchiveTypes.INCREMENTAL,
                       entityIndexDataList
                      );


    // add differential menu items
    subSubMenuDifferential = Widgets.addMenu(subMenu,
                                             null,
                                             BARControl.tr("differential")
                                            );
    updateAssignToMenu(subSubMenuDifferential,
                       jobUUID,
                       ArchiveTypes.DIFFERENTIAL,
                       entityIndexDataList
                      );

    // add continuous menu items
    subSubMenuContinuous = Widgets.addMenu(subMenu,
                                           null,
                                           BARControl.tr("continuous")
                                          );
    updateAssignToMenu(subSubMenuContinuous,
                       jobUUID,
                       ArchiveTypes.CONTINUOUS,
                       entityIndexDataList
                      );
  }

  /** update assign-to sub-menu
   * @param menu menu
   * @param jobUUID job UUID
   * @param archiveType archive type
   * @param entityIndexDataList entity index data list
   */
  private void updateAssignToMenu(final Menu                      menu,
                                  final String                    jobUUID,
                                  final ArchiveTypes              archiveType,
                                  java.util.List<EntityIndexData> entityIndexDataList
                                 )
  {
    MenuItem menuItem;

    menuItem = Widgets.addMenuItem(menu,
                                   (Object)null,
                                   BARControl.tr("new")+"\u2026"
                                  );
    menuItem.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        assignStorages(jobUUID,archiveType);
      }
    });

    Widgets.addMenuItemSeparator(menu);

    if (entityIndexDataList != null)
    {
      Collections.sort(entityIndexDataList,new Comparator<EntityIndexData>()
      {
        @Override
        public int compare(EntityIndexData entityIndexData1, EntityIndexData entityIndexData2)
        {
          if      (entityIndexData1.createdDateTime < entityIndexData2.createdDateTime) return -1;
          else if (entityIndexData1.createdDateTime > entityIndexData2.createdDateTime) return  1;
          else                                                                          return  0;
        }
      });
      for (EntityIndexData entityIndexData : entityIndexDataList)
      {
        if (entityIndexData.archiveType == archiveType)
        {
          menuItem = Widgets.addMenuItem(menu,
                                         (Object)entityIndexData,
                                         ((entityIndexData.createdDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(entityIndexData.createdDateTime*1000L)) : "-")
                                        );

          menuItem.addSelectionListener(new SelectionListener()
          {
            @Override
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
            @Override
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              MenuItem        widget          = (MenuItem)selectionEvent.widget;
              EntityIndexData entityIndexData = (EntityIndexData)widget.getData();

              // Work-around for SWT bug: sometimes some sub-menu keeps open; force dispose of all menu items
              Widgets.removeAllMenuItems(widgetStorageAssignToMenu);

              assignStorages(entityIndexData);
            }
          });
        }
      }
    }
  }

  /** create entity for job and assign jobs/entities/storages to entity
   * @param indexDataHashSet index data
   * @param toJobUUID to job UUID
   * @param toScheduleUUID to schedule UUID or null
   * @param archiveType archive type
   * @return true iff assigned
   */
  private boolean assignStorages(HashSet<IndexData> indexDataHashSet,
                                 String             toJobUUID,
                                 String             toScheduleUUID,
                                 ArchiveTypes       archiveType
                                )
  {
    final SimpleDateFormat DATE_FORMATS[] = {new SimpleDateFormat("yyyy-MM-dd")};

    if (!indexDataHashSet.isEmpty())
    {
      Long dateTime = Long.valueOf(0);
      if (archiveType != ArchiveTypes.UNKNOWN)
      {
        // guess a date
        for (IndexData indexData : indexDataHashSet)
        {
          String archiveNameParts[] = StringUtils.splitArray(indexData.getName(),BARServer.filePathSeparator);
          for (String archiveNamePart : archiveNameParts)
          {
            for (SimpleDateFormat dateFormat : DATE_FORMATS)
            {
              try
              {
                Date date = dateFormat.parse(archiveNamePart);
                if (date != null)
                {
                  dateTime = date.getTime()/1000;
                }
              }
              catch (ParseException exception)
              {
                // ignored
              }
              if (dateTime != 0) break;
            }
            if (dateTime != 0) break;
          }
          if (dateTime != 0) break;
        }

        // get date
        dateTime = Dialogs.date(shell,BARControl.tr("Assign entity date"),(String)null,dateTime,BARControl.tr("Assign"));
        if (dateTime == null)
        {
          return false;
        }
      }

      final BusyDialog busyDialog;
      {
        BARControl.waitCursor();
        busyDialog = new BusyDialog(shell,
                                    BARControl.tr("Assign storages"),
                                    500,
                                    100,
                                    (String)null,
                                    BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                   );
        busyDialog.setMaximum(1+indexDataHashSet.size());
      }
      try
      {
        int n = 0;

        long entityId = 0;
        try
        {
          ValueMap valueMap = new ValueMap();
          BARServer.executeCommand(StringParser.format("INDEX_ENTITY_ADD jobUUID=%'S scheduleUUID=%'S archiveType=%s createdDateTime=%ld",
                                                       toJobUUID,
                                                       (toScheduleUUID != null) ? toScheduleUUID : "",
                                                       archiveType.toString(),
                                                       dateTime.longValue()
                                                      ),
                                   0,  // debugLevel
                                   valueMap
                                  );
          entityId = valueMap.getLong("entityId",0L);

          n++;
          busyDialog.updateProgressBar(n);
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,
                        BARControl.tr("Cannot create entity for\n\n''{0}''!\n\n(error: {1})",
                                      toJobUUID,
                                      exception.getMessage()
                                     )
                       );
          BARControl.logThrowable(exception);
          return false;
        }

        if (!busyDialog.isAborted())
        {
          for (IndexData indexData : indexDataHashSet)
          {
            String info = indexData.getInfo();

            try
            {
              ValueMap valueMap = new ValueMap();
              if      (indexData instanceof UUIDIndexData)
              {
                BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld archiveType=%s jobUUID=%'S",
                                                             entityId,
                                                             archiveType.toString(),
                                                             ((UUIDIndexData)indexData).jobUUID
                                                            ),
                                         0  // debugLevel
                                        );
              }
              else if (indexData instanceof EntityIndexData)
              {
                BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld archiveType=%s entityId=%lld",
                                                             entityId,
                                                             archiveType.toString(),
                                                             indexData.id
                                                            ),
                                         0  // debugLevel
                                        );
              }
              else if (indexData instanceof StorageIndexData)
              {
                BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld archiveType=%s storageId=%lld",
                                                             entityId,
                                                             archiveType.toString(),
                                                             indexData.id
                                                            ),
                                         0  // debugLevel
                                        );
              }
            }
            catch (Exception exception)
            {
              Dialogs.error(shell,BARControl.tr("Cannot assign index for\n\n{0}!\n\n(error: {1})",info,exception.getMessage()));
              BARControl.logThrowable(exception);
              return false;
            }

            if (busyDialog.isAborted())
            {
              break;
            }

            n++;
            busyDialog.updateProgressBar(n);
          }
        }
      }
      finally
      {
        busyDialog.close();
        BARControl.resetCursor();
      }

      updateStorageTreeTableThread.triggerUpdate();
      updateAssignTo();
    }

    return true;
  }

  /** assign jobs/entities/storages to entity
   * @param indexData index data
   * @param toJobUUID job UUID
   * @param toScheduleUUID schedule UUID
   * @param archiveType archive type
   */
  private void assignStorages(IndexData indexData, String toJobUUID, String toScheduleUUID, ArchiveTypes archiveType)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    indexDataHashSet.add(indexData);
    assignStorages(indexDataHashSet,toJobUUID,toScheduleUUID,archiveType);
  }

  /** assign selected/checked jobs/entities/storages to entity
   * @param toJobUUID job UUID
   * @param toScheduleUUID schedule UUID
   * @param archiveType archive type
   */
  private void assignStorages(String toJobUUID, String toScheduleUUID, ArchiveTypes archiveType)
  {
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
    if (assignStorages(indexDataHashSet,toJobUUID,toScheduleUUID,archiveType))
    {
      clearSelectedIndexData();
    }
  }

  /** assign selected/checked jobs/entities/storages to entity
   * @param toJobUUID job UUID
   * @param archiveType archive type
   */
  private void assignStorages(String toJobUUID, ArchiveTypes archiveType)
  {
    assignStorages(toJobUUID,(String)null,archiveType);
  }

  /** assign jobs/entities/storages to UUID
   * @param indexDataHashSet index data hash set
   * @param toUUIDIndexData UUID index data
   */
  private void assignStorages(HashSet<IndexData> indexDataHashSet, UUIDIndexData toUUIDIndexData)
  {
    if (!indexDataHashSet.isEmpty())
    {
      final BusyDialog busyDialog;
      {
        BARControl.waitCursor();
        busyDialog = new BusyDialog(shell,
                                    BARControl.tr("Assign storages"),
                                    500,
                                    100,
                                    (String)null,
                                    BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                   );
        busyDialog.setMaximum(indexDataHashSet.size());
      }
      try
      {
        int n = 0;

        for (IndexData indexData : indexDataHashSet)
        {
          final String info = indexData.getInfo();

          try
          {
            if      (indexData instanceof UUIDIndexData)
            {
              BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toJobUUID=%'S jobUUID=%'S",
                                                           toUUIDIndexData.jobUUID,
                                                           ((UUIDIndexData)indexData).jobUUID
                                                          ),
                                       0  // debugLevel
                                      );
            }
            else if (indexData instanceof EntityIndexData)
            {
              BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toJobUUID=%'S entityId=%lld",
                                                           toUUIDIndexData.jobUUID,
                                                           indexData.id
                                                          ),
                                       0  // debugLevel
                                      );
            }
            else if (indexData instanceof StorageIndexData)
            {
              // nothing to do
            }

            indexData.setState(IndexStates.UPDATE_REQUESTED);
          }
          catch (Exception exception)
          {
            Dialogs.error(shell,BARControl.tr("Cannot assign index for\n\n{0}!\n\n(error: {1})",info,exception.getMessage()));
            BARControl.logThrowable(exception);
            break;
          }

          if (busyDialog.isAborted())
          {
            break;
          }

          n++;
          busyDialog.updateProgressBar(n);
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Communication error while assigning index database\n\n(error: {0})",error.toString()));
      }
      finally
      {
        busyDialog.close();
        BARControl.resetCursor();
      }
      updateStorageTreeTableThread.triggerUpdate();
      updateAssignTo();
    }
  }

  /** assign jobs/entities to UUID
   * @param indexData index data
   * @param toUUIDIndexData UUID index data
   */
  private void assignStorages(IndexData indexData, UUIDIndexData toUUIDIndexData)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    indexDataHashSet.add(indexData);
    assignStorages(indexDataHashSet,toUUIDIndexData);
  }

  /** assign selected/checked jobs/entities to UUID
   * @param toUUIDIndexData UUID index data
   */
  private void assignStorages(UUIDIndexData toUUIDIndexData)
  {
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
    assignStorages(indexDataHashSet,toUUIDIndexData);
  }

  /** assign jobs/entities/storages to entity
   * @param indexDataHashSet index data hash set
   * @param toEntityIndexData entity index data
   */
  private void assignStorages(HashSet<IndexData> indexDataHashSet, EntityIndexData toEntityIndexData)
  {
    if (!indexDataHashSet.isEmpty())
    {
      final BusyDialog busyDialog;
      {
        BARControl.waitCursor();
        busyDialog = new BusyDialog(shell,
                                    BARControl.tr("Assign storages"),
                                    500,
                                    100,
                                    (String)null,
                                    BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                   );
        busyDialog.setMaximum(indexDataHashSet.size());
      }
      try
      {
        int n = 0;

        for (IndexData indexData : indexDataHashSet)
        {
          final String info = indexData.getInfo();

          try
          {
            if      (indexData instanceof UUIDIndexData)
            {
              BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld jobUUID=%'S",
                                                           toEntityIndexData.id,
                                                           ((UUIDIndexData)indexData).jobUUID
                                                          ),
                                       0  // debugLevel
                                      );
            }
            else if (indexData instanceof EntityIndexData)
            {
              BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld entityId=%lld",
                                                           toEntityIndexData.id,
                                                           indexData.id
                                                          ),
                                       0  // debugLevel
                                      );
            }
            else if (indexData instanceof StorageIndexData)
            {
              BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld storageId=%lld",
                                                           toEntityIndexData.id,
                                                           indexData.id
                                                          ),
                                       0  // debugLevel
                                      );
            }

            indexData.setState(IndexStates.UPDATE_REQUESTED);
          }
          catch (Exception exception)
          {
            Dialogs.error(shell,BARControl.tr("Cannot assign index for\n\n{0}!\n\n(error: {1})",info,exception.getMessage()));
            BARControl.logThrowable(exception);
            break;
          }

          if (busyDialog.isAborted())
          {
            break;
          }

          n++;
          busyDialog.updateProgressBar(n);
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Communication error while assigning index database\n\n(error: {0})",error.toString()));
      }
      finally
      {
        busyDialog.close();
        BARControl.resetCursor();
      }

      updateEntryTableThread.triggerUpdate();
    }
  }

  /** assign jobs/entities/storages to entity
   * @param indexData index data
   * @param toEntityIndexData entity index data
   */
  private void assignStorages(IndexData indexData, EntityIndexData toEntityIndexData)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    indexDataHashSet.add(indexData);
    assignStorages(indexDataHashSet,toEntityIndexData);
  }

  /** assign selected/checked jobs/entities/storages to entity
   * @param toEntityIndexData entity index data
   */
  private void assignStorages(EntityIndexData toEntityIndexData)
  {
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
    assignStorages(indexDataHashSet,toEntityIndexData);
  }

  /** set archive type of UUID/entity
   * @param entityIndexData entity index data
   * @param archiveType archive type
   */
  private void setEntityType(HashSet<IndexData> indexDataHashSet, ArchiveTypes archiveType)
  {
    if (!indexDataHashSet.isEmpty())
    {
      final BusyDialog busyDialog;
      {
        BARControl.waitCursor();
        busyDialog = new BusyDialog(shell,
                                    BARControl.tr("Set archive type"),
                                    500,
                                    100,
                                    (String)null,
                                    BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                   );
        busyDialog.setMaximum(indexDataHashSet.size());
      }
      try
      {
        int n = 0;

        for (IndexData indexData : indexDataHashSet)
        {
          final String info = indexData.getInfo();

          try
          {
            if      (indexData instanceof UUIDIndexData)
            {
              UUIDIndexData uuidIndexData = (UUIDIndexData)indexData;

              BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toJobUUID=%'S archiveType=%s jobUUID=%'S",
                                                           uuidIndexData.jobUUID,
                                                           archiveType.toString(),
                                                           uuidIndexData.jobUUID
                                                          ),
                                       0  // debugLevel
                                      );
            }
            else if (indexData instanceof EntityIndexData)
            {
              EntityIndexData entityIndexData = (EntityIndexData)indexData;

              BARServer.executeCommand(StringParser.format("INDEX_ASSIGN toEntityId=%lld archiveType=%s entityId=%lld",
                                                           entityIndexData.id,
                                                           archiveType.toString(),
                                                           entityIndexData.id
                                                          ),
                                       0  // debugLevel
                                      );
            }
            else if (indexData instanceof StorageIndexData)
            {
              // nothing to do
            }

            indexData.setState(IndexStates.UPDATE_REQUESTED);
          }
          catch (Exception exception)
          {
            Dialogs.error(shell,BARControl.tr("Cannot set entity type for\n\n{0}!\n\n(error: {1})",info,exception.getMessage()));
            BARControl.logThrowable(exception);
            break;
          }

          if (busyDialog.isAborted())
          {
            break;
          }

          n++;
          busyDialog.updateProgressBar(n);
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Communication error while set entity type in index database\n\n(error: {0})",error.toString()));
      }
      finally
      {
        busyDialog.close();
        BARControl.resetCursor();
      }
      updateStorageTreeTableThread.triggerUpdate();
      updateAssignTo();
    }
  }

  /** set entity type
   * @param entityIndexData entity index data
   * @param archiveType archive type
   */
  private void setEntityType(EntityIndexData entityIndexData, ArchiveTypes archiveType)
  {
    HashSet<IndexData> indexDataHashSet = new HashSet<IndexData>();

    indexDataHashSet.add(entityIndexData);
    setEntityType(indexDataHashSet,archiveType);
  }

  /** set entity type
   * @param archiveType archive type
   */
  private void setEntityType(ArchiveTypes archiveType)
  {
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
    setEntityType(indexDataHashSet,archiveType);
  }

  /** test storage files
   */
  private void testStorages()
  {
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
    if (!indexDataHashSet.isEmpty())
    {
      final BusyDialog busyDialog = new BusyDialog(shell,
                                                   BARControl.tr("Test archives"),
                                                   500,
                                                   300,
                                                   null,
                                                   BusyDialog.TEXT1|BusyDialog.TEXT2|BusyDialog.PROGRESS_BAR0|BusyDialog.PROGRESS_BAR1|BusyDialog.PROGRESS_BAR2|BusyDialog.LIST|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE,
                                                   250  // max. lines
                                                  );
      busyDialog.updateText(3,"%s",BARControl.tr("Failed")+":");
      busyDialog.setMaximum(0,indexDataHashSet.size());

      Background.run(new BackgroundRunnable(busyDialog,
                                            indexDataHashSet
                                           )
      {
        public void run(final BusyDialog   busyDialog,
                        HashSet<IndexData> indexDataHashSet
                       )
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
            for (IndexData indexData : indexDataHashSet)
            {
              String info    = indexData.getInfo();
              String command = null;
              if      (indexData instanceof UUIDIndexData)
              {
                command = StringParser.format("STORAGE_TEST jobUUID=%'S",
                                              ((UUIDIndexData)indexData).jobUUID
                                             );
              }
              else if (indexData instanceof EntityIndexData)
              {
                command = StringParser.format("STORAGE_TEST entityId=%lld",
                                              indexData.id
                                             );
              }
              else if (indexData instanceof StorageIndexData)
              {
                command = StringParser.format("STORAGE_TEST storageId=%lld",
                                              indexData.id
                                             );
              }

              try
              {
                BARServer.executeCommand(command,
                                         2,  // debugLevel
                                         new Command.ResultHandler()
                                         {
                                           @Override
                                           public void handle(int i, ValueMap valueMap)
                                           {
                                             long doneCount        = valueMap.getLong("doneCount");
                                             long totalCount       = valueMap.getLong("totalCount");
                                             long storageDoneSize  = valueMap.getLong("storageDoneSize");
                                             long storageTotalSize = valueMap.getLong("storageTotalSize");
                                             long entryDoneSize    = valueMap.getLong("entryDoneSize");
                                             long entryTotalSize   = valueMap.getLong("entryTotalSize");

                                             busyDialog.updateProgressBar(0,(totalCount > 0) ? (double)doneCount/(double)totalCount : 100.0);
                                             busyDialog.updateText       (1,"%s",valueMap.getString("storageName"));
                                             busyDialog.updateProgressBar(1,(storageTotalSize > 0) ? (double)storageDoneSize/(double)storageTotalSize : 100.0);
                                             busyDialog.updateText       (2,"%s",valueMap.getString("entryName"));
                                             busyDialog.updateProgressBar(2,(entryTotalSize > 0) ? (double)entryTotalSize/(double)entryTotalSize : 100.0);

                                             if (busyDialog.isAborted())
                                             {
                                               busyDialog.updateText(1,"%s",BARControl.tr("Aborting")+"\u2026");
                                               busyDialog.updateText(2,"%s",BARControl.tr("Aborting")+"\u2026");
                                               abort();
                                             }
                                           }
                                         }
                                        );
              }
              catch (Exception exception)
              {
                busyDialog.updateList(info+": "+exception.getMessage());
              }
            }

            // done busy dialog
            busyDialog.done();
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
      });
    }
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
        if (Dialogs.confirm(shell,BARControl.tr("Refresh index for {0} {0,choice,0#jobs|1#job|1<jobs}/{1} {1,choice,0#entities|1#entity|1<entities}/{2} {2,choice,0#archives|1#archive|1<archives}?",jobCount,entityCount,storageCount)))
        {
          {
            BARControl.waitCursor();
          }
          try
          {
            for (IndexData indexData : indexDataHashSet)
            {
              final String info = indexData.getInfo();

              try
              {
                if      (indexData instanceof UUIDIndexData)
                {
                  BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=* jobUUID=%'S",
                                                               ((UUIDIndexData)indexData).jobUUID
                                                              ),
                                           0  // debugLevel
                                          );
                }
                else if (indexData instanceof EntityIndexData)
                {
                  BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=* entityId=%lld",
                                                               indexData.id
                                                              ),
                                           0  // debugLevel
                                          );
                }
                else if (indexData instanceof StorageIndexData)
                {
                  BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=* storageId=%lld",
                                                               indexData.id
                                                              ),
                                           0  // debugLevel
                                          );
                }

                indexData.setState(IndexStates.UPDATE_REQUESTED);
              }
              catch (Exception exception)
              {
                Dialogs.error(shell,BARControl.tr("Cannot refresh index for\n\n{0}!\n\n(error: {1})",info,exception.getMessage()));
                BARControl.logThrowable(exception);
              }
            }
          }
          finally
          {
            BARControl.resetCursor();
          }

          if (!widgetStorageTabFolder.isDisposed())
          {
            switch (widgetStorageTabFolder.getSelectionIndex())
            {
              case 0:
                // tree view
                for (TreeItem treeItem : widgetStorageTree.getSelection())
                {
                  updateStorageTreeTableThread.triggerUpdate(treeItem);
                }
                break;
              case 1:
                // table view
                for (TableItem tableItem : widgetStorageTable.getSelection())
                {
                  updateStorageTreeTableThread.triggerUpdate(tableItem);
                }
                break;
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
      if (Dialogs.confirm(shell,BARControl.tr("Refresh all indices with error state?")))
      {
        {
          BARControl.waitCursor();
        }
        try
        {
          BARServer.executeCommand(StringParser.format("INDEX_REFRESH state=%s storageId=%lld",
                                                       "ERROR",
                                                       0
                                                      ),
                                   0  // debugLevel
                                  );
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,BARControl.tr("Cannot refresh database indices with error state!\n\n(error: {0})",exception.getMessage()));
          BARControl.logThrowable(exception);
        }
        finally
        {
          BARControl.resetCursor();
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,BARControl.tr("Communication error while refreshing database indices\n\n(error: {0})",error.toString()));
    }

    updateStorageTreeTableThread.triggerUpdate();
  }

  /** move all storages of entity to another path
   */
  private void moveEntityTo()
  {
    try
    {
      TreeItem treeItems[] = widgetStorageTree.getSelection();
      if (treeItems.length > 0)
      {
        TreeItem treeItem = treeItems[0];

        if (   !treeItem.isDisposed()
            && (treeItem.getData() instanceof EntityIndexData)
           )
        {
          EntityIndexData selectedEntityIndexData = (EntityIndexData)treeItem.getData();

          String moveToPath = Dialogs.file(shell,
                                           Dialogs.FileDialogTypes.DIRECTORY,
                                           BARControl.tr("Select move-to path"),
                                           "",
                                           new String[]{BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                       },
                                           "*",
                                           Dialogs.FILE_NONE,
                                           BARServer.remoteListDirectory(selectedEntityIndexData.jobUUID)
                                          );
          if (moveToPath != null)
          {
            final BusyDialog busyDialog = new BusyDialog(shell,
                                                         BARControl.tr("Move storages"),
                                                         500,
                                                         100,
                                                         (String)null,
                                                         BusyDialog.PROGRESS_BAR0|BusyDialog.PROGRESS_BAR1|BusyDialog.TEXT0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                                        );

            try
            {
              BARServer.executeCommand(StringParser.format("ENTITY_MOVE_TO entityId=%lld moveTo=%s",
                                                           selectedEntityIndexData.id,
                                                           moveToPath
                                                          ),
                                       0, // debugLevel
                                       new Command.ResultHandler()
                                       {
                                         @Override
                                         public void handle(int i, ValueMap valueMap)
                                         {
                                           long   storageId  = valueMap.getLong  ("storageId");
                                           String name       = valueMap.getString("name");
                                           long   n          = valueMap.getLong  ("n");
                                           long   size       = valueMap.getLong  ("size");
//                                           int    doneCount  = valueMap.getInt   ("doneCount");
                                           long   doneSize   = valueMap.getLong  ("doneSize");
//                                           int    totalCount = valueMap.getInt   ("totalCount");
                                           long   totalSize  = valueMap.getLong  ("totalSize");

                                           busyDialog.updateText(0,name);
                                           busyDialog.updateProgressBar(0,n);
                                           busyDialog.updateProgressBar(1,doneSize);
                                           busyDialog.setMaximum(0,size);
                                           busyDialog.setMaximum(1,totalSize);

                                           if (busyDialog.isAborted())
                                           {
                                             abort();
                                           }
                                         }
                                       }
                                      );

              busyDialog.close();
            }
            catch (BARException exception)
            {
              busyDialog.close();
              if (exception.code != BARException.ABORTED)
              {
                Dialogs.error(shell,BARControl.tr("Cannot move entity!\n\n(error: {0})",exception.getMessage()));
              }
            }
            catch (Exception exception)
            {
              Dialogs.error(shell,BARControl.tr("Cannot move entity!\n\n(error: {0})",exception.getMessage()));
              BARControl.logThrowable(exception);
            }
          }
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,BARControl.tr("Communication error while moving entity\n\n(error: {0})",error.toString()));
    }

    updateStorageTreeTableThread.triggerUpdate();
  }

  /** add storages to index database
   */
  private void addStoragesToIndex()
  {
    /** dialog data
     */
    class Data
    {
      StorageTypes storageType;
      String       hostName;
      int          hostPort;
      String       loginName;
      String       loginPassword;
      String       shareName;
      String       deviceName;
      String       archiveName;

      Data()
      {
        this.storageType   = StorageTypes.FILESYSTEM;
        this.hostName      = "";
        this.hostPort      = 0;
        this.loginName     = "";
        this.loginPassword = "";
        this.shareName     = "";
        this.deviceName    = "";
        this.archiveName   = "";
      }
    };

    Label     label;
    Composite composite,subComposite;
    Button    button;

    final Data data = new Data();

    // create dialog
    final Shell dialog = Dialogs.openModal(shell,BARControl.tr("Add storage to index database"),400,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

    // create widgets
    final Text               widgetArchiveName;
    final Combo              widgetStorageType;
    final BARWidgets.File    widgetFile;
    final BARWidgets.FTP     widgetFTP;
    final BARWidgets.SFTP    widgetSFTP;
    final BARWidgets.WebDAV  widgetWebDAV;
    final BARWidgets.Optical widgetOptical;
    final BARWidgets.Device  widgetDevice;
    final Button             widgetAdd;
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(new double[]{0.0,0.0,1.0},new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE,0,0,2);
    {
      label = Widgets.newLabel(composite,BARControl.tr("Archive name")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetArchiveName = Widgets.newText(composite);
      widgetArchiveName.setToolTipText(BARControl.tr("Enter local or remote storage path."));
      Widgets.layout(widgetArchiveName,0,1,TableLayoutData.WE,0,0,0,0,300,SWT.DEFAULT);

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

          String pathName = Dialogs.file(shell,
                                         Dialogs.FileDialogTypes.ENTRY,
                                         BARControl.tr("Select storage"),
                                         widgetArchiveName.getText(),
                                         ((selectionEvent.stateMask & SWT.CTRL) == 0)
                                           ? BARServer.remoteListDirectory(widgetArchiveName.getText())
                                           : BARControl.listDirectory
                                        );
          if (pathName != null)
          {
            if (!widgetArchiveName.isDisposed())
            {
              widgetArchiveName.setText(pathName);
            }
          }
        }
      });

      // destination
      label = Widgets.newLabel(composite,BARControl.tr("Source")+":");
      Widgets.layout(label,1,0,TableLayoutData.NW);
      widgetStorageType = Widgets.newOptionMenu(composite);
      widgetStorageType.setToolTipText(BARControl.tr("Storage type:\n"+
                                                     "  into file system\n"+
                                                     "  on FTP server\n"+
// TODO:implement/remove?                                                     "  on SSH server with scp (secure copy)\n"+
                                                     "  on SSH server with sftp (secure FTP)\n"+
                                                     "  on WebDAV server\n"+
                                                     "  on WebDAV secure server\n"+
                                                     "  on CD\n"+
                                                     "  on DVD\n"+
                                                     "  on BD\n"+
                                                     "  on generic device\n"
                                                    )
                                      );
      Widgets.setComboItems(widgetStorageType,
                            new Object[]{BARControl.tr("file system"),StorageTypes.FILESYSTEM,
                                                       "ftp",         StorageTypes.FTP,
// TODO:implement/remove?                                                       "scp",         StorageTypes.SCP,
                                                       "sftp",        StorageTypes.SFTP,
                                                       "webdav",      StorageTypes.WEBDAV,
                                                       "webdavs",     StorageTypes.WEBDAVS,
                                                       "CD",          StorageTypes.CD,
                                                       "DVD",         StorageTypes.DVD,
                                                       "BD",          StorageTypes.BD,
                                         BARControl.tr("device"),     StorageTypes.DEVICE
                                        }
                      );
      Widgets.setSelectedComboItem(widgetStorageType,0);
      Widgets.layout(widgetStorageType,1,1,TableLayoutData.W,0,2);

      // destination file system
      widgetFile = new BARWidgets.File(composite);
      Widgets.layout(widgetFile,2,1,TableLayoutData.WE|TableLayoutData.N,0,3);
      Widgets.setVisible(widgetFile,true);

      // destination ftp
      widgetFTP = new BARWidgets.FTP(composite);
      Widgets.layout(widgetFTP,2,1,TableLayoutData.WE|TableLayoutData.N,0,3);
      Widgets.setVisible(widgetFTP,false);

      // destination scp/sftp
      widgetSFTP = new BARWidgets.SFTP(composite);
      Widgets.layout(widgetSFTP,2,1,TableLayoutData.WE|TableLayoutData.N,0,3);
      Widgets.setVisible(widgetSFTP,false);

      // destination WebDAV
      widgetWebDAV = new BARWidgets.WebDAV(composite);
      Widgets.layout(widgetWebDAV,2,1,TableLayoutData.WE|TableLayoutData.N,0,3);
      Widgets.setVisible(widgetWebDAV,false);

      // destination cd/dvd/bd
      widgetOptical = new BARWidgets.Optical(composite);
      Widgets.layout(widgetOptical,2,1,TableLayoutData.WE|TableLayoutData.N,0,3);
      Widgets.setVisible(widgetOptical,false);

      // destination device
      widgetDevice = new BARWidgets.Device(composite);
      Widgets.layout(widgetDevice,2,1,TableLayoutData.WE|TableLayoutData.N,0,3);
      Widgets.setVisible(widgetDevice,false);
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,2,0,TableLayoutData.WE,0,0,2);
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
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // add listeners
    widgetArchiveName.addModifyListener(new ModifyListener()
    {
      @Override
      public void modifyText(ModifyEvent modifyEvent)
      {
        Text   widget      = (Text)modifyEvent.widget;
        String archiveName = widget.getText().trim();

        widgetAdd.setEnabled(!archiveName.isEmpty());
      }
    });
    widgetStorageType.addSelectionListener(new SelectionListener()
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

        Widgets.setVisible(widgetFile,   type == StorageTypes.FILESYSTEM);
        Widgets.setVisible(widgetFTP,    type == StorageTypes.FTP);
// TODO:implemente/remove?        Widgets.setVisible(scp,   type == StorageTypes.SCP);
        Widgets.setVisible(widgetSFTP,   type == StorageTypes.SFTP);
        Widgets.setVisible(widgetWebDAV,    (type == StorageTypes.WEBDAV)
                                         || (type == StorageTypes.WEBDAVS)
                          );
        Widgets.setVisible(widgetOptical,   (type == StorageTypes.CD )
                                         || (type == StorageTypes.DVD)
                                         || (type == StorageTypes.BD )
                          );
        Widgets.setVisible(widgetDevice, type == StorageTypes.DEVICE);
        dialog.pack();
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
        data.storageType = Widgets.getSelectedComboItem(widgetStorageType);
        switch (data.storageType)
        {
          case FILESYSTEM:
            data.archiveName = widgetArchiveName.getText().trim();
            break;
          case FTP:
            data.hostName      = widgetFTP.hostName.getText().trim();
            data.loginName     = widgetFTP.loginName.getText();
            data.loginPassword = widgetFTP.loginPassword.getText();
            data.archiveName   = widgetArchiveName.getText().trim();
            break;
// TODO:implemente/remove?          case StorageTypes.SCP:
          case SFTP:
            data.hostName      = widgetSFTP.hostName.getText().trim();
            data.hostPort      = widgetSFTP.hostPort.getSelection();
            data.loginName     = widgetSFTP.loginName.getText();
            data.loginPassword = widgetSFTP.loginPassword.getText();
            data.archiveName   = widgetArchiveName.getText().trim();
            break;
          case WEBDAV:
          case WEBDAVS:
            data.hostName      = widgetWebDAV.hostName.getText().trim();
            data.hostPort      = widgetWebDAV.hostPort.getSelection();
            data.loginName     = widgetWebDAV.loginName.getText();
            data.loginPassword = widgetWebDAV.loginPassword.getText();
            data.archiveName   = widgetArchiveName.getText().trim();
            break;
          case CD:
          case DVD:
          case BD:
            data.deviceName  = widgetOptical.deviceName.getText().trim();
            data.archiveName = widgetArchiveName.getText().trim();
            break;
          case DEVICE:
            data.deviceName  = widgetDevice.deviceName.getText().trim();
            data.archiveName = widgetArchiveName.getText().trim();
            break;
        }
        Dialogs.close(dialog,true);
      }
    });

    Widgets.setNextFocus(widgetArchiveName,
                         widgetStorageType,
                         widgetAdd
                        );

    // run dialog
    widgetArchiveName.forceFocus();
    dialog.pack();
    if ((Boolean)Dialogs.run(dialog,null) && !data.archiveName.isEmpty())
    {
      // add storage files
      final BusyDialog busyDialog = new BusyDialog(shell,
                                                   BARControl.tr("Add indices"),
                                                   500,200,
                                                   (String)null,
                                                   BusyDialog.PROGRESS_BAR0|BusyDialog.TEXT0|BusyDialog.LIST|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                                  );

      URIParts addURIParts = new URIParts(data.storageType,
                                          data.hostName,
                                          data.hostPort,
                                          data.loginName,
                                          data.loginPassword,
                                          data.shareName,
                                          data.deviceName,
                                          data.archiveName
                                         );

      Background.run(new BackgroundRunnable(busyDialog,addURIParts)
      {
        public void run(final BusyDialog busyDialog, final URIParts addURIParts)
        {
          final int[] n = new int[]{0};
          busyDialog.updateText(BARControl.tr("Found archives: {0}",n[0]));

          try
          {
            BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ADD name=%'S patternType=GLOB progressSteps=1000",
                                                         addURIParts.getURI()
                                                        ),
                                     0,  // debugLevel
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                       {
                                         long   storageId  = valueMap.getLong  ("storageId", 0L);
                                         String name       = valueMap.getString("name"        );
                                         long   doneCount  = valueMap.getLong  ("doneCount", 0L);
                                         long   totalCount = valueMap.getLong  ("totalCount",0L);

                                         if      ((storageId != 0) && (!name.isEmpty()))
                                         {
                                           n[0]++;
                                           busyDialog.updateText(BARControl.tr("Found archives: {0}",n[0]));
                                           busyDialog.updateList(name);
                                         }
                                         else if (totalCount > 0)
                                         {
                                           busyDialog.updateProgressBar(0,((double)doneCount*100.0)/(double)totalCount);
                                         }
                                       }
                                     },
                                     new BusyIndicator()
                                     {
                                       @Override
                                       public boolean isAborted()
                                       {
                                         return busyDialog.isAborted();
                                       }
                                     }
                                    );
            busyDialog.done();
          }
          catch (final Exception exception)
          {
            busyDialog.close();
            if (!busyDialog.isAborted())
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  Dialogs.error(shell,
                                BARControl.tr("Cannot add index to database for storage path\n\n''{0}''\n\n(error: {1})",
                                              data.archiveName,
                                              exception.getMessage()
                                             )
                               );
                }
              });
            }
          }

          updateStorageTreeTableThread.triggerUpdate();
        }
      });
    }
  }

  /** remove selected storages from index database
   */
  private void removeStoragesFromIndex()
  {
    HashSet<IndexData> indexDataHashSet = getSelectedIndexData();
    if (!indexDataHashSet.isEmpty())
    {
      // count jobs/entities/archives
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

      // get total number of entries
      long totalEntryCount;
      try
      {
        totalEntryCount = BARServer.getLong(StringParser.format("INDEX_STORAGE_LIST_INFO"),
                                            2,  // debugLevel
                                            "totalEntryCount"
                                           );
        assert(totalEntryCount >= 0);
      }
      catch (final Exception exception)
      {
        display.syncExec(new Runnable()
        {
          @Override
          public void run()
          {
            Dialogs.error(shell,BARControl.tr("Cannot get total entries from database!\n\n(error: {0})",exception.getMessage()));
          }
        });
        return;
      }

      if (Dialogs.confirm(shell,
                          BARControl.tr("Remove {0} {0,choice,0#jobs|1#job|1<jobs}/{1} {1,choice,0#entities|1#entity|1<entities}/{2} {2,choice,0#archives|1#archive|1<archives} from index with {3} {3,choice,0#entries|1#entry|1<entries}?",
                                        jobCount,
                                        entityCount,
                                        storageCount,
                                        totalEntryCount
                                       )
                         )
        )
      {
        final BusyDialog busyDialog = new BusyDialog(shell,
                                                     BARControl.tr("Remove indices"),
                                                     500,
                                                     100,
                                                     (String)null,
                                                     BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                                    );
        busyDialog.setMaximum(indexDataHashSet.size());

        Background.run(new BackgroundRunnable(busyDialog,indexDataHashSet)
        {
          public void run(final BusyDialog busyDialog, HashSet<IndexData> indexDataHashSet)
          {
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
                try
                {
                  if      (indexData instanceof UUIDIndexData)
                  {
                    BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* uuidId=%lld",
                                                                 indexData.id
                                                                ),
                                             0  // debugLevel
                                            );
                    Widgets.removeTreeItem(widgetStorageTree,indexData);
                  }
                  else if (indexData instanceof EntityIndexData)
                  {
                    BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* entityId=%lld",
                                                                 indexData.id
                                                                ),
                                              0  // debugLevel
                                             );
                    Widgets.removeTreeItem(widgetStorageTree,indexData);
                  }
                  else if (indexData instanceof StorageIndexData)
                  {
                    BARServer.executeCommand(StringParser.format("INDEX_REMOVE state=* storageId=%lld",
                                                                 indexData.id
                                                                ),
                                             0  // debugLevel
                                            );
                  }
                }
                catch (final Exception exception)
                {
                  display.syncExec(new Runnable()
                  {
                    @Override
                    public void run()
                    {
                      Dialogs.error(shell,BARControl.tr("Cannot remove index for\n\n{0}!\n\n(error: {1})",info,exception.getMessage()));
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
            }
//TODO: pass error to caller?
            catch (final CommunicationError error)
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,BARControl.tr("Communication error while removing database indices\n\n(error: {0})",error.getMessage()));
                 }
              });
            }
            catch (final ConnectionError error)
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,BARControl.tr("Connection error while removing database indices\n\n(error: {0})",error.getMessage()));
                 }
              });
            }
            catch (Throwable throwable)
            {
              if (Settings.debugLevel > 0)
              {
                BARServer.disconnect();
                BARControl.internalError(throwable);
              }
            }

            // update
            updateStorageTreeTableThread.triggerUpdate();
            updateEntryTableThread.triggerUpdate();
            updateAssignTo();
          }
        });
      }
    }
  }

  /** remove all storage from index database with error
   */
  private void removeAllWithErrorStorageIndex()
  {
    try
    {
      // get number of indices with error state
      long errorTotalStorageCount;
      try
      {
        errorTotalStorageCount = BARServer.getLong("INDEX_STORAGE_LIST_INFO entityId=* indexStateSet=ERROR indexModeSet=* name=*",
                                                   1,  // debugLevel
                                                   "totalStorageCount"
                                                  );
      }
      catch (final Exception exception)
      {
        display.syncExec(new Runnable()
        {
          @Override
          public void run()
          {
            Dialogs.error(shell,BARControl.tr("Cannot get database indices with error state!\n\n(error: {0})",exception.getMessage()));
          }
        });
        return;
      }

      if (errorTotalStorageCount > 0)
      {
        if (Dialogs.confirm(shell,BARControl.tr("Remove {0} {0,choice,0#storages|1#storage|1<storages} with error state?",errorTotalStorageCount)))
        {
          final BusyDialog busyDialog = new BusyDialog(shell,
                                                       "Remove storages with error",
                                                       500,
                                                       100,
                                                       (String)null,
                                                       BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                                      );
          busyDialog.setMaximum(errorTotalStorageCount);

          Background.run(new BackgroundRunnable(busyDialog)
          {
            public void run(final BusyDialog busyDialog)
            {
              try
              {
                final String[] errorMessage = new String[1];
                ValueMap       valueMap     = new ValueMap();

                // remove indices with error state
                Command command = BARServer.runCommand("INDEX_REMOVE state=ERROR",
                                                       0  // debug level
                                                      );

                long n = 0;
                while (   !command.endOfData()
                       && !busyDialog.isAborted()
                       && command.getNextResult(errorMessage,
                                                valueMap,
                                                Command.TIMEOUT
                                               ) == BARException.NONE
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
                      BARServer.disconnect();
                      BARControl.internalError(exception);
                    }
                  }
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

                if (command.getErrorCode() != BARException.NONE)
                {
                  display.syncExec(new Runnable()
                  {
                    @Override
                    public void run()
                    {
                      Dialogs.error(shell,BARControl.tr("Cannot remove database indices with error state!\n\n(error: {0})",errorMessage[0]));
                    }
                  });
                }
              }
//TODO: pass to caller?
              catch (final CommunicationError error)
              {
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    busyDialog.close();
                    Dialogs.error(shell,BARControl.tr("Communication error while removing database indices\n\n(error: {0})",error.getMessage()));
                   }
                });
              }
              catch (final ConnectionError error)
              {
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    busyDialog.close();
                    Dialogs.error(shell,BARControl.tr("Connection error while removing database indices\n\n(error: {0})",error.getMessage()));
                   }
                });
              }
              catch (Throwable throwable)
              {
                if (Settings.debugLevel > 0)
                {
                  BARServer.disconnect();
                  BARControl.internalError(throwable);
                }
              }

              updateStorageTreeTableThread.triggerUpdate();
              updateAssignTo();
            }
          });
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,BARControl.tr("Communication error while removing database indices\n\n(error: {0})",error.toString()));
    }
  }

  /** delete storages
   */
  private void deleteStorages()
  {
    // get checked and selected index ids
    final IndexIdSet indexIdSet = (IndexIdSet)checkedIndexIdSet.clone();
    switch (widgetStorageTabFolder.getSelectionIndex())
    {
      case 0:
        // tree view
        for (TreeItem treeItem : widgetStorageTree.getSelection())
        {
          if      (treeItem.getData() instanceof UUIDIndexData)
          {
            UUIDIndexData uuidIndexData = (UUIDIndexData)treeItem.getData();
            if (uuidIndexData != null)
            {
              indexIdSet.add(uuidIndexData.id);
            }
          }
          else if (treeItem.getData() instanceof EntityIndexData)
          {
            EntityIndexData entityIndexData = (EntityIndexData)treeItem.getData();
            if (entityIndexData != null)
            {
              indexIdSet.add(entityIndexData.id);
            }
          }
          else if (treeItem.getData() instanceof StorageIndexData)
          {
            StorageIndexData storageIndexData = (StorageIndexData)treeItem.getData();
            if (storageIndexData != null)
            {
              indexIdSet.add(storageIndexData.id);
            }
          }
        }
        widgetStorageTree.deselectAll();
        break;
      case 1:
        for (TableItem tableItem : widgetStorageTable.getSelection())
        {
          StorageIndexData storageIndexData = (StorageIndexData)tableItem.getData();
          if (storageIndexData != null)
          {
            indexIdSet.add(storageIndexData.id);
          }
        }
        widgetStorageTable.deselectAll();
        break;
    }

    if (!indexIdSet.isEmpty())
    {
      final HashMap<Long,String> storageMap        = new HashMap<Long,String>();
      long                       totalStorageCount = 0L;
      long                       totalStorageSize  = 0L;
      long                       totalEntryCount   = 0L;
      long                       totalEntrySize    = 0L;

      {
        BARControl.waitCursor();
      }
      try
      {
        // set index list
        setStorageList(indexIdSet);

        // get storage list
        try
        {
          BARServer.executeCommand("INDEX_STORAGE_LIST",
                                   1,  // debugLevel
                                   new Command.ResultHandler()
                                   {
                                     @Override
                                     public void handle(int i, ValueMap valueMap)
                                     {
                                       long   storageId       = valueMap.getLong  ("storageId");
                                       String name            = valueMap.getString("name");
                                       long   totalEntryCount = valueMap.getLong  ("totalEntryCount");
                                       long   totalEntrySize  = valueMap.getLong  ("totalEntrySize",Long.MAX_VALUE);

                                       storageMap.put(storageId,BARControl.tr("#{0}: {1}, {2} {2,choice,0#entries|1#entry|1<entries}, {3} ({4} {4,choice,0#bytes|1#byte|1<bytes})",
                                                                              storageId,
                                                                              name,
                                                                              totalEntryCount,
                                                                              Units.formatByteSize(totalEntrySize),
                                                                              totalEntrySize
                                                                             )
                                                     );
                                     }
                                   }
                                  );
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,BARControl.tr("Cannot get storages list!\n\n(error: {0})",exception.getMessage()));
          BARControl.logThrowable(exception);
          return;
        }

        // get total number of entries, total entry size
        try
        {
          ValueMap valueMap = new ValueMap();
          BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST_INFO"),
                                   2,  // debugLevel
                                   valueMap
                                  );
          totalStorageCount = valueMap.getLong("totalStorageCount");
          totalStorageSize  = valueMap.getLong("totalStorageSize",Long.MAX_VALUE);
          totalEntryCount   = valueMap.getLong("totalEntryCount"  );
          totalEntrySize    = valueMap.getLong("totalEntrySize",  Long.MAX_VALUE);
          assert(totalStorageCount >= 0);
          assert(totalStorageSize >= 0);
          assert(totalEntryCount >= 0);
          assert(totalEntrySize >= 0);
        }
        catch (Exception exception)
        {
          BARControl.logThrowable(exception);
        }
      }
      finally
      {
        BARControl.resetCursor();
      }

      // confirm
//TODO: dialog with all information
      if (Dialogs.confirm(shell,
                          BARControl.tr("Delete {0} {0,choice,0#storage files|1#storage file|1<storage files} with {1} ({2} {2,choice,0#bytes|1#byte|1<bytes})?",
                                        storageMap.size(),
                                        Units.formatByteSize(totalStorageSize),
                                        totalStorageSize
                                       )
                         )
         )
      {
        final BusyDialog busyDialog = new BusyDialog(shell,
                                                     BARControl.tr("Delete storage indices and archives"),
                                                     500,
                                                     150,
                                                     (String)null,
                                                     BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                                    );
        busyDialog.setMaximum(storageMap.size());

        Background.run(new BackgroundRunnable(busyDialog,storageMap)
        {
          public void run(final BusyDialog busyDialog, HashMap<Long,String> storageMap)
          {
            try
            {
              boolean ignoreAllErrorsFlag = false;
              boolean abortFlag           = false;
              long    n                   = 0;
              for (Long storageId : storageMap.keySet())
              {
                // get info
                final String info = storageMap.get(storageId);

                // update busy dialog
                busyDialog.updateText(0,"%s",info);

                try
                {
                  BARServer.executeCommand(StringParser.format("STORAGE_DELETE storageId=%lld",
                                                               storageId
                                                              ),
                                           0  // debugLevel
                                          );
                }
                catch (final Exception exception)
                {
                  if (!ignoreAllErrorsFlag)
                  {
                    final int[] selection = new int[1];
                    if (storageMap.size() > (n+1))
                    {
                      display.syncExec(new Runnable()
                      {
                        @Override
                        public void run()
                        {
                          selection[0] = Dialogs.select(shell,
                                                        BARControl.tr("Confirmation"),
                                                        BARControl.tr("Cannot delete storage\n\n''{0}''\n\n(error: {1})",info,exception.getMessage()),
                                                        new String[]{BARControl.tr("Continue"),BARControl.tr("Continue with all"),BARControl.tr("Abort")},
                                                        0
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
                catch (ConnectionError eonnectionError)
                {
                  abortFlag = true;
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
              updateAssignTo();

              Widgets.notify(shell,BARControl.USER_EVENT_UPDATE_JOB);
            }
//TODO: pass to caller?
            catch (final CommunicationError error)
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,BARControl.tr("Communication error while deleting storage\n\n(error: {0})",error.getMessage()));
                 }
              });
            }
            catch (final ConnectionError error)
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,BARControl.tr("Connection error while removing database indices\n\n(error: {0})",error.getMessage()));
                 }
              });
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
        });

        setAllCheckedStorage(false);
      }
    }
  }

  //-----------------------------------------------------------------------

  /** set/clear selected storage entry
   * @param entryId entry id
   * @param checked true for set checked, false for clear checked
   */
  private void setEntryList(long entryId, boolean checked)
  {
    try
    {
      if (checked)
      {
        BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST_ADD entryIds=%ld",
                                                     entryId
                                                    ),
                                 1  // debugLevel
                                );
      }
      else
      {
        BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST_REMOVE entryIds=%ld",
                                                     entryId
                                                    ),
                                 1  // debugLevel
                                );
      }
    }
    catch (Exception exception)
    {
      throw new CommunicationError(exception);
    }

    checkedEntryIdSet.set(entryId,checked);
  }

  /** clear selected storage entries
   */
  private void clearEntryList()
  {
    try
    {
      BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST_CLEAR"),
                               1  // debugLevel
                              );
    }
    catch (Exception exception)
    {
      throw new CommunicationError(exception);
    }

    checkedEntryIdSet.clear();
  }

  /** set selected storage entries
   * @param entryDataSet index data set
   */
  private void setEntryList(IndexIdSet entryIdSet)
  {
    // clear list
    try
    {
      BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST_CLEAR"),
                               1  // debugLevel
                              );
    }
    catch (Exception exception)
    {
      throw new CommunicationError(exception);
    }

    // set list
    Long entryIds[] = entryIdSet.toArray(new Long[entryIdSet.size()]);
    int i = 0;
    while (i < entryIds.length)
    {
      int n = entryIds.length-i; if (n > 1024) n = 1024;
      try
      {
        BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST_ADD entryIds=%s",
                                                     StringUtils.join(entryIds,i,n,',')
                                                    ),
                                 1  // debugLevel
                                );
      }
      catch (Exception exception)
      {
        throw new CommunicationError(exception);
      }
      i += n;
    }

    for (Long entryId : entryIds)
    {
      checkedEntryIdSet.set(entryId,true);
    }
  }

  /** set/clear checked all entries
   * @param checked true for set checked, false for clear checked
   */
  private void setAllCheckedEntries(final boolean checked)
  {
    final int MAX_CONFIRM_ENTRIES = 1000;

    // confirm check if there are many entries
    final int     totalEntryCount[] = new int[]{0};
    final boolean doit[]            = new boolean[]{false};
    if (checked)
    {
      try
      {
        // get total number of entries
        totalEntryCount[0] = BARServer.getInt(StringParser.format("INDEX_ENTRY_LIST_INFO entryType=%s name=%'S newestOnly=%y selectedOnly=no",
                                                                  updateEntryTableThread.getEntryType().toString(),
                                                                  updateEntryTableThread.getName_(),
                                                                  updateEntryTableThread.getNewestOnly()
                                                                 ),
                                              0,  // debugLevel
                                              "totalEntryCount"
                                             );
        if (totalEntryCount[0] > MAX_CONFIRM_ENTRIES)
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              doit[0] = Dialogs.confirm(shell,
                                        Dialogs.booleanFieldUpdater(Settings.class,"showEntriesMarkInfo"),
                                        BARControl.tr("There are {0} entries. Really mark all entries?",
                                                      totalEntryCount[0]
                                                     ),
                                        true
                                       );
            }
          });
        }
        else
        {
          doit[0] = true;
        }
      }
      catch (Exception exception)
      {
        throw new CommunicationError(exception);
      }
    }
    else
    {
      doit[0] = true;
    }

    // check/uncheck all entries
    if (checked)
    {
      if (doit[0])
      {
        clearEntryList();

        final BusyDialog busyDialog = new BusyDialog(shell,
                                                     BARControl.tr("Mark entries"),
                                                     500,
                                                     100,
                                                     (String)null,
                                                     BusyDialog.PROGRESS_BAR0|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE
                                                    );
        try
        {
          final IndexIdSet entryIdSet = new IndexIdSet();
          final int        n[]        = new int[]{0};
          busyDialog.setMaximum(totalEntryCount[0]);

          BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST entryType=%s name=%'S newestOnly=%y selectedOnly=no fragmentsCount=no",
                                                       updateEntryTableThread.getEntryType().toString(),
                                                       updateEntryTableThread.getName_(),
                                                       updateEntryTableThread.getNewestOnly()
                                                      ),
                                   2,  // debugLevel
                                   new Command.ResultHandler()
                                   {
                                     @Override
                                     public void handle(int i, ValueMap valueMap)
                                     {
                                       long entryId = valueMap.getLong("entryId",0L);

                                       entryIdSet.set(entryId,checked);

                                       n[0]++;
                                       busyDialog.updateProgressBar(n[0]);

                                       if (busyDialog.isAborted())
                                       {
                                         abort();
                                       }
                                     }
                                   }
                                  );

          clearEntryList();
          setEntryList(entryIdSet);

          busyDialog.close();
        }
        catch (BARException exception)
        {
          busyDialog.close();
          if (exception.code != BARException.ABORTED)
          {
            Dialogs.error(shell,BARControl.tr("Cannot mark all index entries!\n\n(error: {0})",exception.getMessage()));
          }
        }
        catch (Exception exception)
        {
          busyDialog.close();
          Dialogs.error(shell,BARControl.tr("Cannot mark all index entries!\n\n(error: {0})",exception.getMessage()));
          BARControl.logThrowable(exception);
        }
      }
    }
    else
    {
      clearEntryList();
    }
  }

  /** get checked entries
   * @return checked data entries
   */
  private EntryIndexData[] getCheckedEntries()
  {
    ArrayList<EntryIndexData> entryIndexDataArray = new ArrayList<EntryIndexData>();

    for (TableItem tableItem : widgetEntryTable.getItems())
    {
      if (tableItem.getChecked())
      {
        entryIndexDataArray.add((EntryIndexData)tableItem.getData());
      }
    }

    return entryIndexDataArray.toArray(new EntryIndexData[entryIndexDataArray.size()]);
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
      long              totalStorageCount;
      long              totalStorageSize;
      long              totalEntryCount;
      long              totalEntrySize;
      String            restoreToDirectory;
      boolean           directoryContent;
      boolean           sparse;
      boolean           skipVerifySignatures;
      RestoreEntryModes restoreEntryMode;

      Data()
      {
        this.totalStorageCount     = 0;
        this.totalStorageSize      = 0L;
        this.totalEntryCount       = 0;
        this.totalEntrySize        = 0L;
        this.restoreToDirectory    = null;
        this.directoryContent      = false;
        this.sparse                = false;
        this.skipVerifySignatures  = false;
        this.restoreEntryMode      = RestoreEntryModes.STOP;
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
    final Shell dialog = Dialogs.openModal(shell,title,800,400,new double[]{1.0,0.0},1.0);

    final WidgetEvent selectRestoreToEvent = new WidgetEvent();

    // create widgets
    final Table  widgetRestoreTable;
    final Label  widgetTotal;
    final Button widgetRestoreTo;
    final Text   widgetRestoreToDirectory;
    final Button widgetDirectoryContent;
    final Button widgetSparse;
    final Button widgetSkipVerifySignatures;
    final Combo  widgetRestoreEntryMode;
    final Button widgetRestore;
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(new double[]{0.0,1.0,0.0,0.0,0.0},new double[]{0.0,1.0}));
    Widgets.layout(composite,0,0,TableLayoutData.NSWE,0,0,2);
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
      switch (restoreType)
      {
        case ARCHIVES:
          Widgets.addTableColumn(widgetRestoreTable,0,BARControl.tr("Name"   ),SWT.LEFT, 530,true);
          Widgets.addTableColumn(widgetRestoreTable,1,BARControl.tr("Entries"),SWT.RIGHT, 80,true);
          Widgets.addTableColumn(widgetRestoreTable,2,BARControl.tr("Size"   ),SWT.RIGHT, 60,true);
          break;
        case ENTRIES:
          Widgets.addTableColumn(widgetRestoreTable,0,BARControl.tr("Name"     ),SWT.LEFT, 360,true);
          Widgets.addTableColumn(widgetRestoreTable,1,BARControl.tr("Type"     ),SWT.LEFT, 100,true);
          Widgets.addTableColumn(widgetRestoreTable,2,BARControl.tr("Size"     ),SWT.RIGHT, 60,true);
          Widgets.addTableColumn(widgetRestoreTable,3,BARControl.tr("Date/Time"),SWT.LEFT, 180,true);
          Widgets.addTableColumn(widgetRestoreTable,3,BARControl.tr("Fragments"),SWT.LEFT,  80,true);
          break;
      }

      label = Widgets.newLabel(composite,BARControl.tr("Total")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);
      widgetTotal = Widgets.newLabel(composite,"-");
      Widgets.layout(widgetTotal,2,1,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,Settings.hasNormalRole());
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
                                      widgetRestoreToDirectory.getText(),
                                      BARServer.remoteListDirectory
                                     );
            }
            else
            {
             pathName = Dialogs.directory(shell,
                                          BARControl.tr("Select path"),
                                          widgetRestoreToDirectory.getText()
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

      widgetDirectoryContent = Widgets.newCheckbox(composite,BARControl.tr("Directory content"),Settings.hasNormalRole());
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
                                            Units.formatByteSize(data.totalEntrySize),
                                            data.totalEntrySize
                                           )
                             );
          widgetTotal.pack();
        }
      });

      widgetSparse = Widgets.newCheckbox(composite,BARControl.tr("Sparse files/hardlinks"),Settings.hasNormalRole());
      widgetSparse.setToolTipText(BARControl.tr("Restore as sparse files/hardlinks."));
      Widgets.layout(widgetSparse,5,0,TableLayoutData.W,0,2);

      widgetSkipVerifySignatures = Widgets.newCheckbox(composite,BARControl.tr("Skip verify signatures"),Settings.hasExpertRole());
      widgetSkipVerifySignatures.setToolTipText(BARControl.tr("Enable this checkbox when verification of signatures should be skipped."));
      Widgets.layout(widgetSkipVerifySignatures,6,0,TableLayoutData.W,0,2);

      subComposite = Widgets.newComposite(composite,Settings.hasNormalRole());
      subComposite.setLayout(new TableLayout(null,new double[]{0.0,1.0}));
      Widgets.layout(subComposite,7,0,TableLayoutData.WE,0,2);
      {
        label = Widgets.newLabel(subComposite,BARControl.tr("Entry mode")+":");
        Widgets.layout(label,0,0,TableLayoutData.W);
        widgetRestoreEntryMode = Widgets.newOptionMenu(subComposite);
        widgetRestoreEntryMode.setToolTipText(BARControl.tr("If set to 'rename' then the new entry is renamed if entry already exists.\nIf set to 'overwrite' then existing entries are overwritten.\nOtherwise stop with an error if entry exists."));
        Widgets.setComboItems(widgetRestoreEntryMode,new Object[]{BARControl.tr("stop if exists"  ),RestoreEntryModes.STOP,
                                                                  BARControl.tr("rename if exists"),RestoreEntryModes.RENAME,
                                                                  BARControl.tr("overwrite"       ),RestoreEntryModes.OVERWRITE,
                                                                  BARControl.tr("skip existing"   ),RestoreEntryModes.SKIP_EXISTING,
                                                                 }
                                              );
        Widgets.setSelectedComboItem(widgetRestoreEntryMode,RestoreEntryModes.STOP);
        Widgets.layout(widgetRestoreEntryMode,0,1,TableLayoutData.W);
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,2);
    {
      widgetRestore = Widgets.newButton(composite,BARControl.tr("Start restore"));
      widgetRestore.setEnabled(false);
      Widgets.layout(widgetRestore,0,0,TableLayoutData.W,0,0,0,0,160,SWT.DEFAULT);
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

          data.restoreToDirectory   = widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : null;
          data.directoryContent     = widgetDirectoryContent.getSelection();
          data.sparse               = widgetSparse.getSelection();
          data.skipVerifySignatures = widgetSkipVerifySignatures.getSelection();
          data.restoreEntryMode     = Widgets.getSelectedComboItem(widgetRestoreEntryMode,RestoreEntryModes.STOP);

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

    // get number/size of archives/entries to restore
    Background.run(new BackgroundRunnable(indexIdSet)
    {
      public void run(final IndexIdSet indexIdSet)
      {
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
          switch (restoreType)
          {
            case ARCHIVES:
              try
              {
                // get total number entries, size to restore
                BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST_INFO"),
                                         2,  // debugLevel
                                         new Command.ResultHandler()
                                         {
                                           @Override
                                           public void handle(int i, ValueMap valueMap)
                                           {
                                             data.totalStorageCount = valueMap.getLong("totalStorageCount"              );
                                             data.totalStorageSize  = valueMap.getLong("totalStorageSize"               );
                                             data.totalEntryCount   = valueMap.getLong("totalEntryCount"                );
                                             data.totalEntrySize    = valueMap.getLong("totalEntrySize",  Long.MAX_VALUE);
                                           }
                                         }
                                        );
                assert(data.totalStorageCount >= 0);
                assert(data.totalStorageSize >= 0);
                assert(data.totalEntryCount >= 0);
                assert(data.totalEntrySize >= 0);

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

                // get storages to restore
                BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST sortMode=NAME ordering=ASCENDING"),
                                         1,  // debugLevel
                                         new Command.ResultHandler()
                                         {
                                           @Override
                                           public void handle(int i, ValueMap valueMap)
                                           {
                                             final long   storageId       = valueMap.getLong  ("storageId"                    );
                                             final String name            = valueMap.getString("name"                         );
                                             final long   totalEntryCount = valueMap.getLong  ("totalEntryCount"              );
                                             final long   totalEntrySize  = valueMap.getLong  ("totalEntrySize",Long.MAX_VALUE);

// TODO: mp syncExec inside executeCommand()?
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
                                         }
                                        );
              }
              catch (Exception exception)
              {
                throw new CommunicationError(exception);
              }
              break;
            case ENTRIES:
              try
              {
                // get total number of entries, total entry size, total content size to restore
                BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST_INFO name='' entryType=* newestOnly=no selectedOnly=yes"),
                                         1,  // debugLevel
                                         new Command.ResultHandler()
                                         {
                                           @Override
                                           public void handle(int i, ValueMap valueMap)
                                           {
                                             data.totalStorageCount = valueMap.getLong("totalStorageCount"              );
                                             data.totalStorageSize  = valueMap.getLong("totalStorageSize",Long.MAX_VALUE);
                                             data.totalEntryCount   = valueMap.getLong("totalEntryCount"                );
                                             data.totalEntrySize    = valueMap.getLong("totalEntrySize",  Long.MAX_VALUE);
                                           }
                                         }
                                        );
                assert(data.totalStorageCount >= 0);
                assert(data.totalStorageSize >= 0);
                assert(data.totalEntryCount >= 0);
                assert(data.totalEntrySize >= 0);

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

                // get entries to restore
                BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST newestOnly=no selectedOnly=yes fragmentsCount=yes"),
                                         1,  // debugLevel
                                         new Command.ResultHandler()
                                         {
                                           @Override
                                           public void handle(int i, ValueMap valueMap)
                                           {
                                             final long       entryId   = valueMap.getLong  ("entryId"                   );
                                             final EntryTypes entryType = valueMap.getEnum  ("entryType",EntryTypes.class);

// TODO: mp syncExec inside executeCommand()?
                                             switch (entryType)
                                             {
                                               case FILE:
                                                 {
                                                   final String fileName       = valueMap.getString("name"          );
                                                   final long   dateTime       = valueMap.getLong  ("dateTime"      );
                                                   final long   size           = valueMap.getLong  ("size"          );
                                                   final int    fragmentCount  = valueMap.getInt   ("fragmentCount" );

                                                   // add entry data index
                                                   display.syncExec(new Runnable()
                                                   {
                                                     public void run()
                                                     {
                                                        if (!widgetRestoreTable.isDisposed())
                                                        {
                                                          Widgets.addTableItem(widgetRestoreTable,
                                                                               entryId,
                                                                               fileName,
                                                                               entryType.getText(),
                                                                               Units.formatByteSize(size),
                                                                               SIMPLE_DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                               String.format("%d",fragmentCount)
                                                                              );
                                                        }
                                                     }
                                                   });
                                                 }
                                                 break;
                                               case IMAGE:
                                                 {
                                                   final String imageName      = valueMap.getString("name"          );
                                                   final long   size           = valueMap.getLong  ("size"          );
                                                   final int    fragmentCount  = valueMap.getInt   ("fragmentCount" );

                                                   // add entry data index
                                                   display.syncExec(new Runnable()
                                                   {
                                                     public void run()
                                                     {
                                                       if (!widgetRestoreTable.isDisposed())
                                                       {
                                                         Widgets.addTableItem(widgetRestoreTable,
                                                                              entryId,
                                                                              imageName,
                                                                              entryType.getText(),
                                                                              Units.formatByteSize(size),
                                                                              "",
                                                                              String.format("%d",fragmentCount)
                                                                             );
                                                       }
                                                     }
                                                   });
                                                 }
                                                 break;
                                               case DIRECTORY:
                                                 {
                                                   final String directoryName = valueMap.getString("name"    );
                                                   final long   dateTime      = valueMap.getLong  ("dateTime");
                                                   final long   size          = valueMap.getLong  ("size"    );

                                                   // add entry data index
                                                   display.syncExec(new Runnable()
                                                   {
                                                     public void run()
                                                     {
                                                        if (!widgetRestoreTable.isDisposed())
                                                        {
                                                          Widgets.addTableItem(widgetRestoreTable,
                                                                               entryId,
                                                                               directoryName,
                                                                               entryType.getText(),
                                                                               "",
                                                                               SIMPLE_DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                               ""
                                                                              );
                                                        }
                                                     }
                                                   });
                                                 }
                                                 break;
                                               case LINK:
                                                 {
                                                   final String linkName        = valueMap.getString("name"           );
                                                   final String destinationName = valueMap.getString("destinationName");
                                                   final long   dateTime        = valueMap.getLong  ("dateTime"       );

                                                   // add entry data index
                                                   display.syncExec(new Runnable()
                                                   {
                                                     public void run()
                                                     {
                                                        if (!widgetRestoreTable.isDisposed())
                                                        {
                                                          Widgets.addTableItem(widgetRestoreTable,
                                                                               entryId,
                                                                               linkName,
                                                                               entryType.getText(),
                                                                               "",
                                                                               SIMPLE_DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                               ""
                                                                              );
                                                        }
                                                     }
                                                   });
                                                 }
                                                 break;
                                               case HARDLINK:
                                                 {
                                                   final String fileName       = valueMap.getString("name"          );
                                                   final long   dateTime       = valueMap.getLong  ("dateTime"      );
                                                   final long   size           = valueMap.getLong  ("size"          );
                                                   final int    fragmentCount  = valueMap.getInt   ("fragmentCount" );

                                                   // add entry data index
                                                   display.syncExec(new Runnable()
                                                   {
                                                     public void run()
                                                     {
                                                        if (!widgetRestoreTable.isDisposed())
                                                        {
                                                          Widgets.addTableItem(widgetRestoreTable,
                                                                               entryId,
                                                                               fileName,
                                                                               entryType.getText(),
                                                                               Units.formatByteSize(size),
                                                                               SIMPLE_DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                               String.format("%d",fragmentCount)
                                                                              );
                                                        }
                                                     }
                                                   });
                                                 }
                                                 break;
                                               case SPECIAL:
                                                 {
                                                   final String name     = valueMap.getString("name"    );
                                                   final long   dateTime = valueMap.getLong  ("dateTime");

                                                   // add entry data index
                                                   display.syncExec(new Runnable()
                                                   {
                                                     public void run()
                                                     {
                                                        if (!widgetRestoreTable.isDisposed())
                                                        {
                                                          Widgets.addTableItem(widgetRestoreTable,
                                                                               entryId,
                                                                               name,
                                                                               entryType.getText(),
                                                                               "",
                                                                               SIMPLE_DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                               ""
                                                                              );
                                                        }
                                                     }
                                                   });
                                                 }
                                                 break;
                                             }
                                           }
                                         }
                                        );

              }
              catch (Exception exception)
              {
                throw new CommunicationError(exception);
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
          // ignored
          if (Settings.debugLevel > 0)
          {
            BARControl.printStackTrace(exception);
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
    });

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
                                                   BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.TEXT2|BusyDialog.PROGRESS_BAR0|BusyDialog.PROGRESS_BAR1|BusyDialog.PROGRESS_BAR2|BusyDialog.LIST|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE|BusyDialog.ENABLE_ABORT_CLOSE,
                                                   250  // max. lines
                                                  );
      busyDialog.updateText(0,"%s",BARControl.tr("Storages"));
      busyDialog.updateText(3,"%s",BARControl.tr("Failed entries")+":");

      Background.run(new BackgroundRunnable(busyDialog,
                                            indexIdSet,
                                            data.restoreToDirectory,
                                            data.directoryContent,
                                            data.sparse,
                                            data.skipVerifySignatures,
                                            data.restoreEntryMode
                                           )
      {
        public void run(final BusyDialog  busyDialog,
                        IndexIdSet        indexIdSet,
                        String            restoreToDirectory,
                        Boolean           directoryContent,
                        Boolean           sparse,
                        Boolean           skipVerifySignatures,
                        RestoreEntryModes restoreEntryMode
                       )
        {
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
                command = StringParser.format("RESTORE type=ARCHIVES destination=%'S sparse=%y skipVerifySignatures=%y restoreEntryMode=%s",
                                              (restoreToDirectory != null) ? restoreToDirectory : "",
                                              sparse,
                                              skipVerifySignatures,
                                              restoreEntryMode.toString()
                                             );
                break;
              case ENTRIES:
                command = StringParser.format("RESTORE type=ENTRIES destination=%'S directoryContent=%y sparse=%y skipVerifySignatures=%y restoreEntryMode=%s",
                                              (restoreToDirectory != null) ? restoreToDirectory : "",
                                              directoryContent,
                                              sparse,
                                              skipVerifySignatures,
                                              restoreEntryMode.toString()
                                             );
                break;
            }
            BARServer.executeCommand(command,
                                     0,  // debugLevel
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                       {
                                         RestoreStates state = valueMap.getEnum("state",RestoreStates.class);
                                         switch (state)
                                         {
                                           case NONE:
                                             break;
                                           case RUNNING:
                                             {
                                               long           doneCount        = valueMap.getLong  ("doneCount"                           );
                                               long           doneSize         = valueMap.getLong  ("doneSize"                            );
// TODO: use
//                                               long           totalCount       = valueMap.getLong  ("totalCount");
//                                               long           totalSize        = valueMap.getLong  ("totalSize");
                                               String         storageName      = valueMap.getString("storageName"      );
                                               long           storageDoneSize  = valueMap.getLong  ("storageDoneSize", 0L);
                                               long           storageTotalSize = valueMap.getLong  ("storageTotalSize",0L);
                                               String         entryName        = valueMap.getString("entryName"        );
                                               long           entryDoneSize    = valueMap.getLong  ("entryDoneSize",   0L);
                                               long           entryTotalSize   = valueMap.getLong  ("entryTotalSize",  0L);

                                               busyDialog.updateProgressBar(0,(data.totalStorageCount > 0L) ? ((double)doneCount*100.0)/(double)data.totalStorageCount : 0.0);
                                               busyDialog.updateText(1,"%s",storageName);
                                               busyDialog.updateProgressBar(1,(storageTotalSize > 0L) ? ((double)storageDoneSize*100.0)/(double)storageTotalSize : 0.0);
                                               busyDialog.updateText(2,"%s",entryName);
                                               busyDialog.updateProgressBar(2,(entryTotalSize > 0L) ? ((double)entryDoneSize*100.0)/(double)entryTotalSize : 0.0);
                                             }
                                             break;
                                           case RESTORED:
                                             busyDialog.updateProgressBar(0,100.0);
                                             busyDialog.updateProgressBar(1,100.0);
                                             busyDialog.updateProgressBar(2,100.0);
                                             break;
                                           case FAILED:
                                             {
                                               int    errorNumber = valueMap.getInt   ("errorNumber");
                                               String errorData   = valueMap.getString("errorData");
                                               String storageName = valueMap.getString("storageName");
                                               String entryName   = valueMap.getString("entryName"  );

                                               StringBuilder line = new StringBuilder();
                                               if (!storageName.isEmpty()) line.append(storageName);
                                               if (line.length() > 0) line.append(": ");
                                               if (!entryName.isEmpty()) line.append(entryName);
                                               busyDialog.updateList("%s",line.toString());

                                               errorCount[0]++;
                                             }
                                             break;
                                         }

                                         if (busyDialog.isAborted())
                                         {
                                           busyDialog.updateText(0,"%s",BARControl.tr("Aborting")+"\u2026");
                                           busyDialog.updateText(1,"");
                                           abort();
                                         }
                                       }
                                     }
                                    );

            // done busy dialog
            busyDialog.done();
          }
          catch (final BARException exception)
          {
            if (exception.code != BARException.ABORTED)
            {
              display.syncExec(new Runnable()
              {
                @Override
                public void run()
                {
                  Dialogs.error(shell,BARControl.tr("Cannot restore:\n\n{0}",exception.getMessage()));
                }
              });
            }
            busyDialog.close();
          }
          catch (final Exception exception)
          {
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                Dialogs.error(shell,BARControl.tr("Cannot restore:\n\n{0}",exception.getMessage()));
              }
            });
            busyDialog.close();
          }
//TODO: pass to caller?
          catch (final CommunicationError error)
          {
            busyDialog.close();
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                Dialogs.error(shell,BARControl.tr("Error while restoring:\n\n{0}",error.getMessage()));
               }
            });
          }
          catch (final ConnectionError error)
          {
            busyDialog.close();
            display.syncExec(new Runnable()
            {
              @Override
              public void run()
              {
                Dialogs.error(shell,BARControl.tr("Connection error while restoring\n\n(error: {0})",error.getMessage()));
               }
            });
          }
          catch (Throwable throwable)
          {
            if (Settings.debugLevel > 0)
            {
              BARServer.disconnect();
              BARControl.internalError(throwable);
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
      });
    }
  }
}

/* end of file */
