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
import java.util.Collection;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
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

    ALL,
    UNKNOWN;

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

  /** index modes
   */
  enum IndexModes
  {
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

  /** storage data
   */
  class StorageData
  {
    long        id;                       // storage id
    String      name;                     // storage name
    long        size;                     // storage size [bytes]
    long        dateTime;                 // date/time when storage was created
    String      title;                    // title to show
    IndexStates indexState;               // state of index
    IndexModes  indexMode;                // mode of index
    long        lastCheckedDateTime;      // last checked date/time
    String      errorMessage;             // last error message
    boolean     checked;                  // true iff storage entry is tagged

    /** create storage data
     * @param id database id
     * @param name name of storage
     * @param size size of storage [bytes]
     * @param dateTime date/time (timestamp) when storage was created
     * @param title title to show
     * @param indexState storage index state
     * @param indexMode storage index mode
     * @param lastCheckedDateTime last checked date/time (timestamp)
     * @param errorMessage error message text
     */
    StorageData(long id, String name, long size, long dateTime, String title, IndexStates indexState, IndexModes indexMode, long lastCheckedDateTime, String errorMessage)
    {
      this.id                  = id;
      this.name                = name;
      this.size                = size;
      this.dateTime            = dateTime;
      this.title               = title;
      this.indexState          = indexState;
      this.indexMode           = indexMode;
      this.lastCheckedDateTime = lastCheckedDateTime;
      this.errorMessage        = errorMessage;
      this.checked             = false;

    }

    /** create storage data
     * @param id database id
     * @param name name of storage
     * @param dateTime date/time (timestamp) when storage was created
     * @param title title to show
     * @param lastCheckedDateTime last checked date/time (timestamp)
     */
    StorageData(long id, String name, long dateTime, String title, long lastCheckedDateTime)
    {
      this(id,name,0,dateTime,title,IndexStates.OK,IndexModes.MANUAL,lastCheckedDateTime,null);
    }

    /** create storage data
     * @param id database id
     * @param name name of storage
     * @param title title to show
     */
    StorageData(long id, String name, String title)
    {
      this(id,name,0,title,0);
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
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Storage {"+name+", "+size+" bytes, created="+dateTime+", title="+title+", state="+indexState+", last checked="+lastCheckedDateTime+", checked="+checked+"}";
    }
  };

  /** storage data map
   */
  class StorageDataMap extends HashMap<Long,StorageData>
  {
    /** remove not checked entries
     */
    public void clearNotChecked()
    {
      String[] keys = keySet().toArray(new String[0]);
      for (String key : keys)
      {
        StorageData storageData = get(key);
        if (!storageData.isChecked()) remove(key);
      }
    }

    /** get storage data from map
     * @param id storage id
     * @return storage data
     */
    public StorageData get(long storageId)
    {
      return super.get(storageId);
    }

    /** get storage data from map
     * @param storageName storage name
     * @return storage data
     */
    public StorageData get(String storageName)
    {
      for (StorageData storageData : values())
      {
        if (storageData.name.equals(storageName)) return storageData;
      }

      return null;
    }

    /** put storage data into map
     * @param storageData storage data
     */
    public void put(StorageData storageData)
    {
      put(storageData.id,storageData);
    }

    /** remove storage data from map
     * @param storageData storage data
     */
    public void remove(StorageData storageData)
    {
      remove(storageData.id);
    }
  }

  /** storage data comparator
   */
  class StorageDataComparator implements Comparator<StorageData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_NAME             = 0;
    private final static int SORTMODE_SIZE             = 1;
    private final static int SORTMODE_CREATED_DATETIME = 2;
    private final static int SORTMODE_STATE            = 3;

    private int sortMode;

    /** create storage data comparator
     * @param table storage table
     * @param sortColumn sort column
     */
    StorageDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_CREATED_DATETIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_STATE;
      else                                       sortMode = SORTMODE_NAME;
    }

    /** create storage data comparator
     * @param table storage table
     */
    StorageDataComparator(Table table)
    {
      this(table,table.getSortColumn());
    }

    /** compare storage data
     * @param storageData1, storageData2 storage data to compare
     * @return -1 iff storageData1 < storageData2,
                0 iff storageData1 = storageData2,
                1 iff storageData1 > storageData2
     */
    public int compare(StorageData storageData1, StorageData storageData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return storageData1.title.compareTo(storageData2.title);
        case SORTMODE_SIZE:
          if      (storageData1.size < storageData2.size) return -1;
          else if (storageData1.size > storageData2.size) return  1;
          else                                            return  0;
        case SORTMODE_CREATED_DATETIME:
          if      (storageData1.dateTime < storageData2.dateTime) return -1;
          else if (storageData1.dateTime > storageData2.dateTime) return  1;
          else                                                    return  0;
        case SORTMODE_STATE:
          return storageData1.indexState.compareTo(storageData2.indexState);
        default:
          return 0;
      }
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "StorageComparator {"+sortMode+"}";
    }
  }

  /** update storage list thread
   */
  class UpdateStorageListThread extends Thread
  {
    private Object      trigger                 = new Object();   // trigger update object
    private boolean     triggeredFlag           = false;
    private int         storageMaxCount         = 100;
    private String      storagePattern          = null;
    private IndexStates storageIndexStateFilter = IndexStates.ALL;
    private boolean     setColorFlag            = false;          // true to set color at update

    /** create update storage list thread
     */
    UpdateStorageListThread()
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
          if (setColorFlag)
          {
            // set busy cursor, foreground color to inform about update
            display.syncExec(new Runnable()
            {
              public void run()
              {
                shell.setCursor(waitCursor);
                widgetStorageList.setForeground(COLOR_MODIFIED);
              }
            });
          }

          // create new storage map
          StorageDataMap newStorageDataMap = new StorageDataMap();

          // save marked entries
          synchronized(storageDataMap)
          {
            for (StorageData storageData : storageDataMap.values())
            {
              if (storageData.isChecked())
              {
                newStorageDataMap.remove(storageData);
              }
            }
          }

          // update entries
          try
          {
            Command command = BARServer.runCommand(StringParser.format("INDEX_STORAGE_LIST pattern=%'S maxCount=%d indexState=%s indexMode=%s",
                                                                       (((storagePattern != null) && !storagePattern.equals("")) ? storagePattern : "*"),
                                                                       storageMaxCount,
                                                                       ((storageIndexStateFilter != IndexStates.ALL) ? storageIndexStateFilter.name() : "*"),
                                                                       "*"
                                                                      )
                                                  );

            // read results, update/add data
            final TypeMap TYPE_MAP = new TypeMap("storageId",long.class,
                                                 "name",String.class,
                                                 "dateTime",long.class,
                                                 "size",long.class,
                                                 "indexState",IndexStates.class,
                                                 "indexMode",IndexModes.class,
                                                 "lastCheckedDateTime",long.class,
                                                 "errorMessage",String.class
                                                );
            String[] resultErrorMessage = new String[1];
            ValueMap resultMap          = new ValueMap();
            while (!command.endOfData())
            {
              if (command.getNextResult(TYPE_MAP,
                                        resultErrorMessage,
                                        resultMap,
                                        5*1000
                                       ) == Errors.NONE
                 )
              {
                long        storageId           = resultMap.getLong  ("storageId"          );
                String      name                = resultMap.getString("name"               );
                long        dateTime            = resultMap.getLong  ("dateTime"           );
                long        size                = resultMap.getLong  ("size"               );
                IndexStates indexState          = resultMap.getEnum  ("indexState"         );
                IndexModes  indexMode           = resultMap.getEnum  ("indexMode"          );
                long        lastCheckedDateTime = resultMap.getLong  ("lastCheckedDateTime");
                String      errorMessage        = resultMap.getString("errorMessage"       );

                StorageData storageData;
                synchronized(storageDataMap)
                {
                  storageData = storageDataMap.get(storageId);
                  if (storageData != null)
                  {
                    storageData.name                = name;
                    storageData.size                = size;
                    storageData.dateTime            = dateTime;
                    storageData.indexState          = indexState;
                    storageData.indexMode           = indexMode;
                    storageData.lastCheckedDateTime = lastCheckedDateTime;
                    storageData.errorMessage        = errorMessage;
                  }
                  else
                  {
                    storageData = new StorageData(storageId,
                                                  name,
                                                  size,
                                                  dateTime,
                                                  new File(name).getName(),
                                                  indexState,
                                                  indexMode,
                                                  lastCheckedDateTime,
                                                  errorMessage
                                                 );
                  }
                }
                newStorageDataMap.put(storageData);
              }
            }
          }
          catch (CommunicationError error)
          {
            // ignored
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

          // store new storage map
          storageDataMap = newStorageDataMap;

          // referesh list
          display.syncExec(new Runnable()
          {
            public void run()
            {
              refreshStorageList();
            }
          });

          if (setColorFlag)
          {
            // reset cursor, foreground color
            display.syncExec(new Runnable()
            {
              public void run()
              {
                widgetStorageList.setForeground(null);
                shell.setCursor(null);
              }
            });
          }

          // sleep a short time or get new pattern
          synchronized(trigger)
          {
            // wait for refresh request or timeout
            try { trigger.wait(30*1000); } catch (InterruptedException exception) { /* ignored */ };

            // if not triggered (timeout occurred) update is done invisible (color is not set)
            if (!triggeredFlag) setColorFlag = false;

            triggeredFlag = false;
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

    /** trigger an update
     * @param storagePattern new storage pattern
     * @param storageStateFilter new storage state filter
     * @param storageMaxCount new max. entries in list
     */
    public void triggerUpdate(String storagePattern, IndexStates storageIndexStateFilter, int storageMaxCount)
    {
      synchronized(trigger)
      {
        this.storagePattern          = storagePattern;
        this.storageIndexStateFilter = storageIndexStateFilter;
        this.storageMaxCount         = storageMaxCount;
        this.setColorFlag            = true;

        triggeredFlag = true;
        trigger.notify();
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
    long          size;
    long          dateTime;
    boolean       checked;
    RestoreStates restoreState;

    /** create entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param name entry name
     * @param type entry type
     * @param size size [bytes]
     * @param dateTime date/time (timestamp)
     */
    EntryData(String storageName, long storageDateTime, String name, EntryTypes type, long size, long dateTime)
    {
      this.storageName     = storageName;
      this.storageDateTime = storageDateTime;
      this.name            = name;
      this.type            = type;
      this.size            = size;
      this.dateTime        = dateTime;
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
      this(storageName,storageDateTime,name,type,0L,dateTime);
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
      return "Entry {"+storageName+", "+name+", "+type+", "+size+" bytes, dateTime="+dateTime+", checked="+checked+", state="+restoreState+"}";
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
              widgetEntryList.setForeground(COLOR_MODIFIED);
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
              String[]            resultErrorMessage  = new String[1];
              ArrayList<ValueMap> resultMapList       = new ArrayList<ValueMap>();

Dprintf.dprintf("process line by line");

              if (BARServer.executeCommand(StringParser.format("INDEX_ENTRIES_LIST entryPattern=%'S checkedStorageOnlyFlag=%y entryMaxCount=%d newestEntriesOnlyFlag=%y",
                                                               entryPattern,
                                                               checkedStorageOnlyFlag,
                                                               entryMaxCount,
                                                               newestEntriesOnlyFlag
                                                              ),
                                           new TypeMap("entryType",EntryTypes.class,
                                                       "storageName",String.class,
                                                       "storageDateTime",long.class,
                                                       "name",String.class,
                                                       "size",long.class,
                                                       "dateTime",long.class,
                                                       "userId",int.class,
                                                       "groupId",int.class,
                                                       "permission",long.class,
                                                       "fragmentOffset",long.class,
                                                       "fragmentSize",long.class
                                                      ),
                                           resultErrorMessage,
                                           resultMapList
                                          ) == Errors.NONE
                 )
              {
                for (ValueMap resultMap : resultMapList)
                {
                  EntryTypes entryType = resultMap.getEnum("entryType");
                  switch (entryType)
                  {
                    case FILE:
                      {
                        String storageName     = resultMap.getString("name"           );
                        long   storageDateTime = resultMap.getLong  ("storageDateTime");
                        String fileName        = resultMap.getString("name"           );
                        long   size            = resultMap.getLong  ("size"           );
                        long   dateTime        = resultMap.getLong  ("dateTime"       );
                        long   fragmentOffset  = resultMap.getLong  ("fragmentOffset" );
                        long   fragmentSize    = resultMap.getLong  ("fragmentSize"   );

                        EntryData entryData = entryDataMap.get(storageName,fileName,EntryTypes.FILE);
                        if (entryData != null)
                        {
                          entryData.size     = size;
                          entryData.dateTime = dateTime;
                        }
                        else
                        {
                          entryData = new EntryData(storageName,storageDateTime,fileName,EntryTypes.FILE,size,dateTime);
                          entryDataMap.put(entryData);
                        }
                      }
                      break;
                    case IMAGE:
                      {
                        String storageName     = resultMap.getString("name"           );
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
                          entryData = new EntryData(storageName,storageDateTime,imageName,EntryTypes.IMAGE,size,0L);
                          entryDataMap.put(entryData);
                        }
                      }
                      break;
                    case DIRECTORY:
                      {
                        String storageName     = resultMap.getString("name"           );
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
                        String storageName     = resultMap.getString("name"           );
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
                        String storageName     = resultMap.getString("name"           );
                        long   storageDateTime = resultMap.getLong  ("storageDateTime");
                        String fileName        = resultMap.getString("name"           );
                        long   size            = resultMap.getLong  ("size"           );
                        long   dateTime        = resultMap.getLong  ("dateTime"       );
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
                        String storageName     = resultMap.getString("name"           );
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
              }
              else
              {
                final String errorText = String.format("Cannot list entries (error: %s)",resultErrorMessage[0]);
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    Dialogs.error(shell,errorText);
                  }
                });
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
              widgetEntryList.setForeground(null);
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
        if (Settings.debugFlag)
        {
          BARServer.disconnect();
          System.err.println("ERROR: "+exception.getMessage());
          BARControl.printStackTrace(exception);
          System.exit(1);
        }
      }
    }

    /** trigger an update
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
  private Shell           shell;
  private Display         display;

  // widgets
  public  Composite       widgetTab;
  private TabFolder       widgetTabFolder;

  private Table           widgetStorageList;
  private Shell           widgetStorageListToolTip = null;
  private Text            widgetStoragePattern;
  private Combo           widgetStorageStateFilter;
  private Combo           widgetStorageMaxCount;
  private WidgetEvent     checkedStorageEvent = new WidgetEvent();

  private Table           widgetEntryList;
  private Shell           widgetEntryListToolTip = null;
  private WidgetEvent     checkedEntryEvent = new WidgetEvent();

  private Button          widgetRestoreTo;
  private Text            widgetRestoreToDirectory;
  private Button          widgetOverwriteEntries;
  private WidgetEvent     selectRestoreToEvent = new WidgetEvent();

  private boolean         checkedStorageOnlyFlag = false;

  UpdateStorageListThread updateStorageListThread;
  private String          storagePattern    = null;
  private IndexStates     storageIndexState = IndexStates.ALL;
  private int             storageMaxCount   = 100;
  private StorageDataMap  storageDataMap    = new StorageDataMap();

  UpdateEntryListThread   updateEntryListThread;
  private String          entryPattern          = null;
  private boolean         newestEntriesOnlyFlag = false;
  private int             entryMaxCount         = 100;
  private EntryDataMap    entryDataMap          = new EntryDataMap();

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create restore tab
   * @param parentTabFolder parent tab folder
   * @param accelerator keyboard shortcut to select tab
   */
  TabRestore(TabFolder parentTabFolder, int accelerator)
  {
    Composite   tab;
    Group       group;
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
    widgetTab = Widgets.addTab(parentTabFolder,"Restore"+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{0.5,0.5,0.0},new double[]{0.0,1.0},2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // connector button
    button = Widgets.newCheckbox(widgetTab,IMAGE_CONNECT0,IMAGE_CONNECT1);
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
    button.setToolTipText("When this connector is in state 'closed', only tagged storage archives are used for list entries.");

    // storage list
    group = Widgets.newGroup(widgetTab,"Storage");
    group.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,4));
    Widgets.layout(group,0,1,TableLayoutData.NSWE);
    {
      widgetStorageList = Widgets.newTable(group,SWT.CHECK);
      widgetStorageList.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0}));
      Widgets.layout(widgetStorageList,0,0,TableLayoutData.NSWE);
      SelectionListener storageListColumnSelectionListener = new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableColumn           tableColumn           = (TableColumn)selectionEvent.widget;
          StorageDataComparator storageDataComparator = new StorageDataComparator(widgetStorageList,tableColumn);
          synchronized(widgetStorageList)
          {
            Widgets.sortTableColumn(widgetStorageList,tableColumn,storageDataComparator);
          }
        }
      };
      tableColumn = Widgets.addTableColumn(widgetStorageList,0,"Name",    SWT.LEFT, 450,true);
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for name.");
      tableColumn = Widgets.addTableColumn(widgetStorageList,1,"Size",    SWT.RIGHT,100,true);
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for size.");
      tableColumn = Widgets.addTableColumn(widgetStorageList,2,"Modified",SWT.LEFT, 150,true);
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for modification date/time.");
      tableColumn = Widgets.addTableColumn(widgetStorageList,3,"State",   SWT.LEFT,  60,true);
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for state.");
      widgetStorageList.addListener(SWT.MouseDoubleClick,new Listener()
      {
        public void handleEvent(final Event event)
        {
          TableItem tableItem = widgetStorageList.getItem(new Point(event.x,event.y));
          if (tableItem != null)
          {
            tableItem.setChecked(!tableItem.getChecked());
            ((StorageData)tableItem.getData()).setChecked(tableItem.getChecked());

            checkedStorageEvent.trigger();
          }
        }
      });
      widgetStorageList.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableItem tableItem = (TableItem)selectionEvent.item;
          if (tableItem != null)
          {
            ((StorageData)tableItem.getData()).setChecked(tableItem.getChecked());

            BARServer.executeCommand(StringParser.format("STORAGE_LIST_CLEAR"));
            for (StorageData storageData : storageDataMap.values())
            {
              if (storageData.isChecked())
              {
                BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD jobId=%d",storageData.id));
              }
            }
          }

          checkedStorageEvent.trigger();
          if (checkedStorageOnlyFlag)
          {
            updateEntryListThread.triggerUpdate(checkedStorageOnlyFlag,entryPattern,newestEntriesOnlyFlag,entryMaxCount);
          }
        }
      });
      widgetStorageList.addMouseTrackListener(new MouseTrackListener()
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

          if (widgetStorageListToolTip != null)
          {
            widgetStorageListToolTip.dispose();
            widgetStorageListToolTip = null;
          }

          // show if table item available and mouse is in the left side
          if ((tableItem != null) && (mouseEvent.x < 64))
          {
            StorageData storageData = (StorageData)tableItem.getData();
            Label       label;

            final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
            final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

            widgetStorageListToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
            widgetStorageListToolTip.setBackground(COLOR_BACKGROUND);
            widgetStorageListToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
            Widgets.layout(widgetStorageListToolTip,0,0,TableLayoutData.NSWE);
            widgetStorageListToolTip.addMouseTrackListener(new MouseTrackListener()
            {
              public void mouseEnter(MouseEvent mouseEvent)
              {
              }

              public void mouseExit(MouseEvent mouseEvent)
              {
                widgetStorageListToolTip.dispose();
                widgetStorageListToolTip = null;
              }

              public void mouseHover(MouseEvent mouseEvent)
              {
              }
            });

            label = Widgets.newLabel(widgetStorageListToolTip,"Name:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,0,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,storageData.name);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,0,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageListToolTip,"Created:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,simpleDateFormat.format(new Date(storageData.dateTime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageListToolTip,"Size:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,2,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,String.format("%d bytes (%s)",storageData.size,Units.formatByteSize(storageData.size)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,2,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageListToolTip,"State:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,storageData.indexState.toString());
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageListToolTip,"Last checked:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,simpleDateFormat.format(new Date(storageData.lastCheckedDateTime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageListToolTip,"Error:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,5,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,storageData.errorMessage);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,5,1,TableLayoutData.WE);

            Point size = widgetStorageListToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
            Rectangle bounds = tableItem.getBounds(0);
            Point point = table.toDisplay(mouseEvent.x+16,bounds.y);
            widgetStorageListToolTip.setBounds(point.x,point.y,size.x,size.y);
            widgetStorageListToolTip.setVisible(true);
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
            menuItem.setEnabled(checkStorageChecked());
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
            MenuItem widget = (MenuItem)selectionEvent.widget;
            deleteStorage();
          }
        });
      }
      widgetStorageList.setMenu(menu);

      // storage list filter
      composite = Widgets.newComposite(group);
      composite.setLayout(new TableLayout(0.0,new double[]{0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(composite,1,0,TableLayoutData.WE);
      {
        button = Widgets.newButton(composite,IMAGE_MARK_ALL);
        Widgets.layout(button,0,0,TableLayoutData.W);
        Widgets.addEventListener(new WidgetEventListener(button,checkedStorageEvent)
        {
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (checkStorageChecked())
            {
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText("Unmark all entries in list.");
            }
            else
            {
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText("Mark all entries in list.");
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
            if (checkStorageChecked())
            {
              setCheckedStorage(false);
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText("Mark all entries in list.");
            }
            else
            {
              setCheckedStorage(true);
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText("Unmark all entries in list.");
            }
          }
        });
        button.setToolTipText("Mark all entries in list.");

        label = Widgets.newLabel(composite,"Filter:");
        Widgets.layout(label,0,1,TableLayoutData.W);

        widgetStoragePattern = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
        widgetStoragePattern.setMessage("Enter text to filter storage list");
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
        widgetStoragePattern.setToolTipText("Enter filter pattern for storage list. Wildcards: * and ?.");

        label = Widgets.newLabel(composite,"State:");
        Widgets.layout(label,0,3,TableLayoutData.W);

        widgetStorageStateFilter = Widgets.newOptionMenu(composite);
        widgetStorageStateFilter.setItems(new String[]{"*","ok","error","update","update requested"});
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
            String indexStateText = widget.getText();
            if      (indexStateText.equalsIgnoreCase("ok"))               storageIndexState = IndexStates.OK;
            else if (indexStateText.equalsIgnoreCase("error"))            storageIndexState = IndexStates.ERROR;
            else if (indexStateText.equalsIgnoreCase("update"))           storageIndexState = IndexStates.UPDATE;
            else if (indexStateText.equalsIgnoreCase("update requested")) storageIndexState = IndexStates.UPDATE_REQUESTED;
            else if (indexStateText.equalsIgnoreCase("*"))                storageIndexState = IndexStates.ALL;
            else                                                          storageIndexState = IndexStates.UNKNOWN;
            updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
          }
        });
        widgetStorageStateFilter.setToolTipText("Storage states filter.");

        label = Widgets.newLabel(composite,"Max:");
        Widgets.layout(label,0,5,TableLayoutData.W);

        widgetStorageMaxCount = Widgets.newOptionMenu(composite);
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
            updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
          }
        });
        widgetStorageMaxCount.setToolTipText("Max. number of entries in list.");

        button = Widgets.newButton(composite,"Restore");
        button.setEnabled(false);
        Widgets.layout(button,0,7,TableLayoutData.DEFAULT,0,0,0,0,60,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,checkedStorageEvent)
        {
          public void trigger(Control control)
          {
            control.setEnabled(checkStorageChecked());
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
            restoreArchives(getCheckedStorageNameHashSet(),
                            widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                            widgetOverwriteEntries.getSelection()
                           );
          }
        });
        button.setToolTipText("Start restoring selected archives.");
      }
    }

    // entries list
    group = Widgets.newGroup(widgetTab,"Entries");
    group.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,4));
    Widgets.layout(group,1,1,TableLayoutData.NSWE);
    {
      widgetEntryList = Widgets.newTable(group,SWT.CHECK);
      widgetEntryList.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(widgetEntryList,0,0,TableLayoutData.NSWE);
      SelectionListener entryListColumnSelectionListener = new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableColumn         tableColumn         = (TableColumn)selectionEvent.widget;
          EntryDataComparator entryDataComparator = new EntryDataComparator(widgetEntryList,tableColumn);
          synchronized(widgetEntryList)
          {
            shell.setCursor(waitCursor);
            Widgets.sortTableColumn(widgetEntryList,tableColumn,entryDataComparator);
            shell.setCursor(null);
          }
        }
      };
      tableColumn = Widgets.addTableColumn(widgetEntryList,0,"Archive",       SWT.LEFT, 200,true );
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for archive name.");
      tableColumn = Widgets.addTableColumn(widgetEntryList,1,"Name",          SWT.LEFT, 300,true );
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for name.");
      tableColumn = Widgets.addTableColumn(widgetEntryList,2,"Type",          SWT.LEFT,  60,true );
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for type.");
      tableColumn = Widgets.addTableColumn(widgetEntryList,3,"Size",          SWT.RIGHT, 60,true );
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for size.");
      tableColumn = Widgets.addTableColumn(widgetEntryList,4,"Date",          SWT.LEFT, 140,true );
      tableColumn.addSelectionListener(entryListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for date.");
      widgetEntryList.addListener(SWT.MouseDoubleClick,new Listener()
      {
        public void handleEvent(final Event event)
        {
          TableItem tableItem = widgetEntryList.getItem(new Point(event.x,event.y));
          if (tableItem != null)
          {
            tableItem.setChecked(!tableItem.getChecked());

            EntryData entryData = (EntryData)tableItem.getData();
            entryData.setChecked(tableItem.getChecked());

            checkedEntryEvent.trigger();
          }
        }
      });
      widgetEntryList.addSelectionListener(new SelectionListener()
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
      widgetEntryList.addMouseTrackListener(new MouseTrackListener()
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

          if (widgetEntryListToolTip != null)
          {
            widgetEntryListToolTip.dispose();
            widgetEntryListToolTip = null;
          }

          if ((tableItem != null) && (mouseEvent.x < 64))
          {
            EntryData entryData = (EntryData)tableItem.getData();
            Label     label;
            Control   control;

            final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
            final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

            widgetEntryListToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
            widgetEntryListToolTip.setBackground(COLOR_BACKGROUND);
            widgetEntryListToolTip.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
            Widgets.layout(widgetEntryListToolTip,0,0,TableLayoutData.NSWE);
            widgetEntryListToolTip.addMouseTrackListener(new MouseTrackListener()
            {
              public void mouseEnter(MouseEvent mouseEvent)
              {
              }

              public void mouseExit(MouseEvent mouseEvent)
              {
                widgetEntryListToolTip.dispose();
                widgetEntryListToolTip = null;
              }

              public void mouseHover(MouseEvent mouseEvent)
              {
              }
            });

            label = Widgets.newLabel(widgetEntryListToolTip,"Storage:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,0,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,entryData.storageName);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,0,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryListToolTip,"Created:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,simpleDateFormat.format(new Date(entryData.storageDateTime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,1,TableLayoutData.WE);

            control = Widgets.newSpacer(widgetEntryListToolTip);
            Widgets.layout(control,2,0,TableLayoutData.WE,0,2,0,0,SWT.DEFAULT,1,SWT.DEFAULT,1,SWT.DEFAULT,1);

            label = Widgets.newLabel(widgetEntryListToolTip,"Type:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,entryData.type.toString());
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryListToolTip,"Name:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,entryData.name);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryListToolTip,"Size:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,5,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,String.format("%d bytes (%s)",entryData.size,Units.formatByteSize(entryData.size)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,5,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryListToolTip,"Date:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,6,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,simpleDateFormat.format(new Date(entryData.dateTime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,6,1,TableLayoutData.WE);

            Point size = widgetEntryListToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
            Rectangle bounds = tableItem.getBounds(0);
            Point point = table.toDisplay(mouseEvent.x+16,bounds.y);
            widgetEntryListToolTip.setBounds(point.x,point.y,size.x,size.y);
            widgetEntryListToolTip.setVisible(true);
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
      widgetEntryList.setMenu(menu);

      // entry list filter
      composite = Widgets.newComposite(group);
      composite.setLayout(new TableLayout(null,new double[]{0.0,0.0,1.0,0.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(composite,1,0,TableLayoutData.WE);
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
              button.setToolTipText("Unmark all entries in list.");
            }
            else
            {
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText("Mark all entries in list.");
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
              button.setToolTipText("Mark all entries in list.");
            }
            else
            {
              setCheckedEntries(true);
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText("Unmark all entries in list.");
            }
          }
        });
        button.setToolTipText("Mark all entries in list.");

        label = Widgets.newLabel(composite,"Filter:");
        Widgets.layout(label,0,1,TableLayoutData.W);

        text = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_SEARCH|SWT.ICON_CANCEL);
        text.setMessage("Enter text to filter entry list");
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
        text.setToolTipText("Enter filter pattern for entry list. Wildcards: * and ?.");

        button = Widgets.newCheckbox(composite,"newest entries only");
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
        button.setToolTipText("When this checkbox is enabled, only show newest entry instances and hide all older entry instances.");

        label = Widgets.newLabel(composite,"Max:");
        Widgets.layout(label,0,4,TableLayoutData.W);

        combo = Widgets.newOptionMenu(composite);
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
        combo.setToolTipText("Max. number of entries in list.");

        button = Widgets.newButton(composite,"Restore");
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
        button.setToolTipText("Start restoring selected entries.");
      }
    }

    // destination
    group = Widgets.newGroup(widgetTab,"Destination");
    group.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0},4));
    Widgets.layout(group,2,0,TableLayoutData.WE,0,2);
    {
      widgetRestoreTo = Widgets.newCheckbox(group,"to");
      Widgets.layout(widgetRestoreTo,0,0,TableLayoutData.W);
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
      widgetRestoreTo.setToolTipText("Enable this checkbox and select a directory to restore entries to different location.");

      widgetRestoreToDirectory = Widgets.newText(group);
      widgetRestoreToDirectory.setEnabled(false);
      Widgets.layout(widgetRestoreToDirectory,0,1,TableLayoutData.WE);
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
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
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

      widgetOverwriteEntries = Widgets.newCheckbox(group,"overwrite existing entries");
      Widgets.layout(widgetOverwriteEntries,0,3,TableLayoutData.W);
      widgetOverwriteEntries.setToolTipText("Enable this checkbox when existing entries in destination should be overwritten.");
    }

    // start storage list update thread
    updateStorageListThread = new UpdateStorageListThread();
    updateStorageListThread.start();

    // start entry list update thread
    updateEntryListThread = new UpdateEntryListThread();
    updateEntryListThread.start();
  }

  //-----------------------------------------------------------------------

  /** set/clear tagging of all storage entries
   * @param checked true for set checked, false for clear checked
   */
  private void setCheckedStorage(boolean checked)
  {
    BARServer.executeCommand(StringParser.format("STORAGE_LIST_CLEAR"));
    for (TableItem tableItem : widgetStorageList.getItems())
    {
      tableItem.setChecked(checked);

      StorageData storageData = (StorageData)tableItem.getData();
      storageData.setChecked(checked);
      if (checked)
      {
        BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD jobId=%d",storageData.id));
      }
    }

    checkedStorageEvent.trigger();
  }

  /** get checked storage names
   * @param storageNamesHashSet storage hash set to fill
   * @return checked storage name hash set
   */
  private HashSet<String> getCheckedStorageNameHashSet(HashSet<String> storageNamesHashSet)
  {
    for (TableItem tableItem : widgetStorageList.getItems())
    {
      StorageData storageData = (StorageData)tableItem.getData();
      if ((storageData != null) && !tableItem.getGrayed() && tableItem.getChecked())
      {
        storageNamesHashSet.add(storageData.name);
      }
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
   * @param storageNamesHashSet storage hash set to fill
   * @return checked storage hash set
   */
  private HashSet<StorageData> getCheckedStorageHashSet(HashSet<StorageData> storageNamesHashSet)
  {
    for (TableItem tableItem : widgetStorageList.getItems())
    {
      StorageData storageData = (StorageData)tableItem.getData();
      if ((storageData != null) && !tableItem.getGrayed() && tableItem.getChecked())
      {
        storageNamesHashSet.add(storageData);
      }
    }

    return storageNamesHashSet;
  }

  /** get checked storage
   * @return checked storage hash set
   */
  private HashSet<StorageData> getCheckedStorageHashSet()
  {
    return getCheckedStorageHashSet(new HashSet<StorageData>());
  }

  /** get selected storage
   * @param storageHashSet storage hash set to fill
   * @return selected storage hash set
   */
  private HashSet<StorageData> getSelectedStorageHashSet(HashSet<StorageData> storageHashSet)
  {
    for (TableItem tableItem : widgetStorageList.getSelection())
    {
      StorageData storageData = (StorageData)tableItem.getData();
      if ((storageData != null) && !tableItem.getGrayed())
      {
        storageHashSet.add(storageData);
      }
    }

    return storageHashSet;
  }

  /** get selected storage
   * @return selected storage hash set
   */
  private HashSet<StorageData> getSelectedStorageHashSet()
  {
    return getSelectedStorageHashSet(new HashSet<StorageData>());
  }

  /** check if some storage entries are checked
   * @return true iff some entry is checked
   */
  private boolean checkStorageChecked()
  {
    for (TableItem tableItem : widgetStorageList.getItems())
    {
      StorageData storageData = (StorageData)tableItem.getData();
      if ((storageData != null) && !tableItem.getGrayed() && tableItem.getChecked())
      {
        return true;
      }
    }

    return false;
  }

  /** find index for insert of item in sorted storage data list
   * @param storageData data of tree item
   * @return index in table
   */
  private int findStorageListIndex(StorageData storageData)
  {
    TableItem             tableItems[]          = widgetStorageList.getItems();
    StorageDataComparator storageDataComparator = new StorageDataComparator(widgetStorageList);

    int index = 0;
    while (   (index < tableItems.length)
           && (storageDataComparator.compare(storageData,(StorageData)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** refresh storage list display
   */
  private void refreshStorageList()
  {
//??? instead of findStorageListIndex
    StorageDataComparator storageDataComparator = new StorageDataComparator(widgetStorageList);

    // refresh
    synchronized(storageDataMap)
    {
      // update/add entries
      for (StorageData storageData : storageDataMap.values())
      {
        // upate/insert
        if (!Widgets.updateTableEntry(widgetStorageList,
                                      (Object)storageData,
                                      storageData.name,
                                      Units.formatByteSize(storageData.size),
                                      simpleDateFormat.format(new Date(storageData.dateTime*1000)),
                                      storageData.indexState.toString()
                                     )
           )
        {
          Widgets.insertTableEntry(widgetStorageList,
                                   findStorageListIndex(storageData),
                                   (Object)storageData,
                                   storageData.name,
                                   Units.formatByteSize(storageData.size),
                                   simpleDateFormat.format(new Date(storageData.dateTime*1000)),
                                   storageData.indexState.toString()
                                  );
        }

        // set checkbox
        Widgets.setTableEntryChecked(widgetStorageList,
                                     (Object)storageData,
                                     storageData.isChecked()
                                    );
      }

      // remove not existing entries
      for (TableItem tableItem : widgetStorageList.getItems())
      {
        StorageData storageData = (StorageData)tableItem.getData();
        if (!storageDataMap.containsValue(storageData))
        {
          Widgets.removeTableEntry(widgetStorageList,storageData);
        }
      }
    }
  }

  /** set storage pattern
   * @param string pattern string
   */
  private void setStoragePattern(String string)
  {
    string = string.trim();
    if (string.length() > 0)
    {
      if ((storagePattern == null) || !storagePattern.equals(string))
      {
        storagePattern = string;
        updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
      }
    }
    else
    {
      if (storagePattern != null)
      {
        storagePattern = null;
        updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
      }
    }
  }

  /** delete storage
   */
  private void deleteStorage()
  {
    HashSet<StorageData> selectedStorageHashSet = new HashSet<StorageData>();

    getCheckedStorageHashSet(selectedStorageHashSet);
    getSelectedStorageHashSet(selectedStorageHashSet);
    if (!selectedStorageHashSet.isEmpty())
    {
      if (Dialogs.confirm(shell,"Really delete "+selectedStorageHashSet.size()+" storage files?"))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,"Delete storage files",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
        busyDialog.setMaximum(selectedStorageHashSet.size());

        new BackgroundTask(busyDialog,new Object[]{selectedStorageHashSet})
        {
          public void run(final BusyDialog busyDialog, Object userData)
          {
            HashSet<StorageData> selectedStorageHashSet = (HashSet<StorageData>)((Object[])userData)[0];

            try
            {
              boolean ignoreAllErrorsFlag = false;
              boolean abortFlag           = false;
              long    n                   = 0;
              for (StorageData storageData : selectedStorageHashSet)
              {
                // get archive name parts
                final ArchiveNameParts archiveNameParts = new ArchiveNameParts(storageData.name);

                // update busy dialog
                busyDialog.updateText(archiveNameParts.getPrintableName());

                // delete storage
                final String[] resultErrorMessage = new String[1];
                int errorCode = BARServer.executeCommand(StringParser.format("STORAGE_DELETE jobId=%d",storageData.id),
                                                         resultErrorMessage
                                                        );
                if (errorCode == Errors.NONE)
                {
                  synchronized(storageDataMap)
                  {
                    storageDataMap.remove(storageData);
                  }
                  Widgets.removeTableEntry(widgetStorageList,storageData);
                }
                else
                {
                  if (!ignoreAllErrorsFlag)
                  {
                    final int[] selection = new int[1];
                    display.syncExec(new Runnable()
                    {
                      public void run()
                      {
                        selection[0] = Dialogs.select(shell,
                                                      "Confirmation",
                                                      "Cannot delete strorage file\n\n'"+archiveNameParts.getPrintableName()+"'\n\n(error: "+resultErrorMessage[0]+")",
                                                      new String[]{"Continue","Continue with all","Abort"},
                                                      0
                                                     );
                      }
                    });
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

              // close busy dialog, restore cursor
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  busyDialog.close();
                }
              });

              updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
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
      label = Widgets.newLabel(composite,"Storage name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetStorageName = Widgets.newText(composite);
      Widgets.layout(widgetStorageName,0,1,TableLayoutData.WE);
      widgetStorageName.setToolTipText("Enter local or remote storage name.");

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
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
      button.setToolTipText("Select local storage file.");
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
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
                                               resultErrorMessage
                                              );
      if (errorCode == Errors.NONE)
      {
        updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
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
    HashSet<StorageData> selectedStorageHashSet = new HashSet<StorageData>();

    getCheckedStorageHashSet(selectedStorageHashSet);
    getSelectedStorageHashSet(selectedStorageHashSet);
    if (!selectedStorageHashSet.isEmpty())
    {
      if (Dialogs.confirm(shell,"Really remove index of "+selectedStorageHashSet.size()+" entries?"))
      {
        final BusyDialog busyDialog = new BusyDialog(shell,"Remove indizes",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
        busyDialog.setMaximum(selectedStorageHashSet.size());

        new BackgroundTask(busyDialog,new Object[]{selectedStorageHashSet})
        {
          public void run(final BusyDialog busyDialog, Object userData)
          {
            HashSet<StorageData> selectedStorageHashSet = (HashSet<StorageData>)((Object[])userData)[0];

            try
            {
              long n = 0;
              for (StorageData storageData : selectedStorageHashSet)
              {
                // get archive name parts
                final ArchiveNameParts archiveNameParts = new ArchiveNameParts(storageData.name);

                // update busy dialog
                busyDialog.updateText(archiveNameParts.getPrintableName());

                // remove entry
                final String[] resultErrorMessage = new String[1];
                int errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REMOVE state=* jobId=%d",storageData.id),
                                                         resultErrorMessage
                                                        );
                if (errorCode == Errors.NONE)
                {
                  synchronized(storageDataMap)
                  {
                    storageDataMap.remove(storageData);
                  }
                  Widgets.removeTableEntry(widgetStorageList,storageData);
                }
                else
                {
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      Dialogs.error(shell,"Cannot remove index database for storage file\n\n'"+archiveNameParts.getPrintableName()+"'\n\n(error: "+resultErrorMessage[0]+")");
                    }
                  });
                }

                // update progress bar
                n++;
                busyDialog.updateProgressBar(n);

                // check for abort
                if (busyDialog.isAborted()) break;
              }

              // close busy dialog, restore cursor
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  busyDialog.close();
                }
              });

              updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
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
      if (Dialogs.confirm(shell,"Really remove all indizes with error state?"))
      {
        int    errorCode;
        String[] resultErrorMessage = new String[1];
        ValueMap resultMap          = new ValueMap();
        errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_INFO"),
                                             new TypeMap("storageEntryCountError",long.class),
                                             resultErrorMessage,
                                             resultMap
                                            );
        if (errorCode == Errors.NONE)
        {
          long storageEntryCountError = resultMap.getLong("storageEntryCountError");

          final BusyDialog busyDialog = new BusyDialog(shell,"Remove indizes",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
          busyDialog.setMaximum(storageEntryCountError);

          new BackgroundTask(busyDialog)
          {
            public void run(final BusyDialog busyDialog, Object userData)
            {
              try
              {
                int            errorCode;
                final String[] resultErrorMessage = new String[1];
                final ValueMap resultMap          = new ValueMap();

                long n = 0;
                do
                {
                  errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST pattern=%'S maxCount=%d indexState=%s indexMode=%s",
                                                                           "*",
                                                                           1,
                                                                           "ERROR",
                                                                           "*"
                                                                          ),
                                                       new TypeMap("storageId",        long.class,
                                                                   "name",             String.class,
                                                                   "createdDateTime",  long.class,
                                                                   "size",             long.class,
                                                                   "indexState",       int.class,
                                                                   "indexMode",        int.class,
                                                                   "lastCheckDateTime",long.class,
                                                                   "errorMessage",     String.class
                                                                  ),
                                                       resultErrorMessage,
                                                       resultMap
                                                      );
                  if (errorCode == Errors.NONE)
                  {
                    long   storageId = resultMap.getLong  ("storageId");
                    String name      = resultMap.getString("name"     );

                    // update busy dialog
                    busyDialog.updateText("'"+name+"'");

                    // remove storage index
                    errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REMOVE state=%s jobId=%d",
                                                                             "*",
                                                                             storageId
                                                                            ),
                                                         resultErrorMessage
                                                        );
                    if (errorCode != Errors.NONE)
                    {
                      final String errorText = String.format("Cannot remove database indizes for '%s' (error: %s)",name,resultErrorMessage[0]);
                      display.syncExec(new Runnable()
                      {
                        public void run()
                        {
                          Dialogs.error(shell,errorText);
                        }
                      });
                    }

                    // update progress bar
                    n++;
                    busyDialog.updateProgressBar(n);
                  }
                  else
                  {
                    Dialogs.error(shell,"Cannot remove database indizes with error state (error: "+resultErrorMessage[0]+")");
                    break;
                  }
                }
                while (!busyDialog.isAborted());

                // close busy dialog, restore cursor
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    busyDialog.close();
                  }
                });

                updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
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
            }
          };
        }
        else
        {
          Dialogs.error(shell,"Communication error while removing database indizes (error: "+resultErrorMessage[0]+")");
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,"Communication error while removing database indizes (error: "+error.toString()+")");
    }
  }

  /** refresh storage from index database
   */
  private void refreshStorageIndex()
  {
    try
    {
      HashSet<StorageData> selectedStorageHashSet = new HashSet<StorageData>();

      getCheckedStorageHashSet(selectedStorageHashSet);
      getSelectedStorageHashSet(selectedStorageHashSet);
      if (!selectedStorageHashSet.isEmpty())
      {
        if (Dialogs.confirm(shell,"Really refresh index for "+selectedStorageHashSet.size()+" entries?"))
        {
          for (StorageData storageData : selectedStorageHashSet)
          {
            String[] resultErrorMessage = new String[1];
            int errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s jobId=%d",
                                                                         "*",
                                                                         storageData.id
                                                                        ),
                                                     resultErrorMessage
                                                    );
            if (errorCode == Errors.NONE)
            {
              storageData.indexState = IndexStates.UPDATE_REQUESTED;
            }
            else
            {
              Dialogs.error(shell,"Cannot refresh index database for storage file\n\n'"+storageData.name+"'\n\n(error: "+resultErrorMessage[0]+")");
            }
          }

          refreshStorageList();
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
        int errorCode = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_REFRESH state=%s jobId=%d",
                                                                     "ERROR",
                                                                     0
                                                                    ),
                                                 resultErrorMessage
                                                );
        if (errorCode == Errors.NONE)
        {
          updateStorageListThread.triggerUpdate(storagePattern,storageIndexState,storageMaxCount);
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
              command = BARServer.runCommand(StringParser.format("RESTORE storageName=%'S destinationDirectory=%'S overwriteFlag=%y",
                                                                 storageName,
                                                                 directory,
                                                                 overwriteEntries
                                                                )
                                            );

              // read results, update/add data
              final TypeMap TYPE_MAP = new TypeMap("name",String.class,
                                                   "entryDoneBytes",long.class,
                                                   "entryTotalBytes",long.class,
                                                   "storageDoneBytes",long.class,
                                                   "storageTotalBytes",long.class
                                                  );
              String[] resultErrorMessage = new String[1];
              ValueMap resultMap          = new ValueMap();
              while (   !command.endOfData()
                     && !busyDialog.isAborted()
                    )
              {
                if (command.getNextResult(TYPE_MAP,
                                          resultErrorMessage,
                                          resultMap,
                                          60*1000
                                         ) == Errors.NONE
                   )
                {
//Dprintf.dprintf("resultMap=%s",resultMap);
                  String name              = resultMap.getString("name"            );
                  long   entryDoneBytes    = resultMap.getLong  ("entryDoneBytes"  );
                  long   entryTotalBytes   = resultMap.getLong  ("entryTotalBytes" );
//                  long   storageDoneBytes  = resultMap.getLong("storageDoneBytes" );
//                  long   storageTotalBytes = resultMap.getLong("storageTotalBytes");

                  busyDialog.updateText(1,name);
                  busyDialog.updateProgressBar(1,(entryTotalBytes > 0) ? ((double)entryDoneBytes*100.0)/(double)entryTotalBytes : 0.0);
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
                                                       "Decrypt password",
                                                       "Password:"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("DECRYPT_PASSWORD_ADD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  )
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
      }
    };
  }

  //-----------------------------------------------------------------------

  /** set/clear tagging of all entries
   * @param checked true for set checked, false for clear checked
   */
  private void setCheckedEntries(boolean checked)
  {
    for (TableItem tableItem : widgetEntryList.getItems())
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
    TableItem           tableItems[]        = widgetEntryList.getItems();
    EntryDataComparator entryDataComparator = new EntryDataComparator(widgetEntryList);

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
    EntryDataComparator entryDataComparator = new EntryDataComparator(widgetEntryList);

    // update
    widgetEntryList.removeAll();
    synchronized(entryDataMap)
    {
      for (EntryData entryData : entryDataMap.values())
      {
        switch (entryData.type)
        {
          case FILE:
            Widgets.insertTableEntry(widgetEntryList,
                                     findEntryListIndex(entryData),
                                     (Object)entryData,
                                     entryData.storageName,
                                     entryData.name,
                                     "FILE",
                                     Units.formatByteSize(entryData.size),
                                     simpleDateFormat.format(new Date(entryData.dateTime*1000))
                                    );
            break;
          case DIRECTORY:
            Widgets.insertTableEntry(widgetEntryList,
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
            Widgets.insertTableEntry(widgetEntryList,
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
            Widgets.insertTableEntry(widgetEntryList,
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
            Widgets.insertTableEntry(widgetEntryList,
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
            Widgets.insertTableEntry(widgetEntryList,
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

        Widgets.setTableEntryChecked(widgetEntryList,
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
              command = BARServer.runCommand(StringParser.format("RESTORE storageName=%'S destinationDirectory=%'S overwriteFlag=%y name=%'S",
                                                                 entryData.storageName,
                                                                 directory,
                                                                 overwriteEntries,
                                                                 entryData.name
                                                                )
                                            );

              // read results, update/add data
              final TypeMap TYPE_MAP = new TypeMap("name",String.class,
                                                   "entryDoneBytes",long.class,
                                                   "entryTotalBytes",long.class,
                                                   "storageDoneBytes",long.class,
                                                   "storageTotalBytes",long.class
                                                  );
              String[] resultErrorMessage = new String[1];
              ValueMap resultMap          = new ValueMap();
              while (   !command.endOfData()
                     && !busyDialog.isAborted()
                    )
              {
                if (command.getNextResult(TYPE_MAP,
                                          resultErrorMessage,
                                          resultMap,
                                          60*1000
                                         ) == Errors.NONE
                   )
                {
//Dprintf.dprintf("resultMap=%s",resultMap);
                  String name              = resultMap.getString("name"            );
                  long   entryDoneBytes    = resultMap.getLong("entryDoneBytes"    );
                  long   entryTotalBytes   = resultMap.getLong("entryTotalBytes"   );
//                  long   storageDoneBytes  = resultMap.getLong("storageDoneBytes" );
//                  long   storageTotalBytes = resultMap.getLong("storageTotalBytes");

                  busyDialog.updateText(1,name);
                  busyDialog.updateProgressBar(1,(entryTotalBytes > 0) ? ((double)entryDoneBytes*100.0)/(double)entryTotalBytes : 0.0);
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
                                                       "FTP login password",
                                                       "Please enter FTP login password for: "+entryData.storageName+".",
                                                       "Password:"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("FTP_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  )
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
                                                       "SSH (TLS) login password",
                                                       "Please enter SSH (TLS) login password for: "+entryData.storageName+".",
                                                       "Password:"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("SSH_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  )
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
                                                       "Webdav login password",
                                                       "Please enter Webdav login password for: "+entryData.storageName+".",
                                                       "Password:"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("WEBDAV_PASSWORD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  )
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
                                                       "Decrypt password",
                                                       "Please enter decrypt password for: "+entryData.storageName+".",
                                                       "Password:"
                                                      );
                    if (password != null)
                    {
                      BARServer.executeCommand(StringParser.format("DECRYPT_PASSWORD_ADD encryptType=%s encryptedPassword=%S",
                                                                   BARServer.getPasswordEncryptType(),
                                                                   BARServer.encryptPassword(password)
                                                                  )
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
      }
    };
  }
}

/* end of file */
