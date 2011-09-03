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

  /** storage data
   */
  class StorageData
  {
    long        id;
    String      name;
    long        size;
    long        datetime;
    String      title;
    IndexStates indexState;
    String      errorMessage;
    boolean     tagged;

    /** create storage data
     * @param id database id
     * @param name name
     * @param size size [bytes]
     * @param datetime date/time (timestamp)
     * @param title title to show
     * @param indexState storage index state
     * @param errorMessage error message text
     */
    StorageData(long id, String name, long size, long datetime, String title, IndexStates indexState, String errorMessage)
    {
      this.id           = id;
      this.name         = name;
      this.size         = size;
      this.datetime     = datetime;
      this.title        = title;
      this.indexState   = indexState;
      this.errorMessage = errorMessage;
      this.tagged       = false;

    }

    /** create storage data
     * @param id database id
     * @param name name
     * @param datetime date/time (timestamp)
     * @param title title to show
     */
    StorageData(long id, String name, long datetime, String title)
    {
      this(id,name,0,datetime,title,IndexStates.OK,null);
    }

    /** create storage data
     * @param id database id
     * @param name name
     * @param title title to show
     */
    StorageData(long id, String name, String title)
    {
      this(id,name,0,title);
    }

    /** check if tagged
     * @return true if entry is tagged, false otherwise
     */
    public boolean isTagged()
    {
      return tagged;
    }

    /** set checked state
     * @param checked checked state
     */
    public void setChecked(boolean tagged)
    {
      this.tagged = tagged;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Storage {"+name+", "+size+" bytes, datetime="+datetime+", title="+title+", state="+indexState+", tagged="+tagged+"}";
    }
  };

  /** storage data map
   */
  class StorageDataMap extends HashMap<Long,StorageData>
  {
    /** remove not tagged entries
     */
    public void clearNotTagged()
    {
      String[] keys = keySet().toArray(new String[0]);
      for (String key : keys)
      {
        StorageData storageData = get(key);
        if (!storageData.isTagged()) remove(key);
      }
    }

    /** get storage data from map
     * @param id storage id
     */
    public StorageData get(long storageId)
    {
      return super.get(storageId);
    }

    /** get storage data from map
     * @param storageName storage name
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
    private final static int SORTMODE_NAME     = 0;
    private final static int SORTMODE_SIZE     = 1;
    private final static int SORTMODE_DATETIME = 2;
    private final static int SORTMODE_STATE    = 3;

    private int sortMode;

    /** create storage data comparator
     * @param table storage table
     * @param sortColumn sort column
     */
    StorageDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_DATETIME;
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
        case SORTMODE_DATETIME:
          if      (storageData1.datetime < storageData2.datetime) return -1;
          else if (storageData1.datetime > storageData2.datetime) return  1;
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
    private Object      trigger                    = new Object();   // trigger update object
    private int         newStorageMaxCount         = -1;
    private String      newStoragePattern          = null;           // new storage pattern
    private IndexStates newStorageIndexStateFilter = null;           // new storage index state filter
    private boolean     setColorFlag               = false;          // true to set color at update

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
      int         storageMaxCount         = 100;
      String      storagePattern          = null;
      IndexStates storageIndexStateFilter = IndexStates.ALL;
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

        // get names
        HashSet<String> storageNameHashSet = new HashSet<String>();
        synchronized(storageDataMap)
        {
          for (StorageData storageData : storageDataMap.values())
          {
            storageNameHashSet.add(storageData.name);
          }
        }

        // update
        try
        {
          String commandString = "INDEX_STORAGE_LIST "+
                                 storageMaxCount+" "+
                                 ((storageIndexStateFilter != IndexStates.ALL) ? storageIndexStateFilter.name() : "*")+" "+
                                 (((storagePattern != null) && !storagePattern.equals("")) ? StringUtils.escape(storagePattern) : "*");
//Dprintf.dprintf("commandString=%s",commandString);
          Command command = BARServer.runCommand(commandString);

          // read results, update/add data
          String line;
          Object data[] = new Object[6];
          while (!command.endOfData())
          {
            line = command.getNextResult(5*1000);
            if (line != null)
            {
              if      (StringParser.parse(line,"%ld %ld %ld %S %S %S",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     id
                     date/time
                     size
                     state
                     storage name
                     error message
                */
                long        id           = (Long)data[0];
                long        datetime     = (Long)data[1];
                long        size         = (Long)data[2];
                IndexStates indexState   = Enum.valueOf(IndexStates.class,(String)data[3]);
                String      storageName  = (String)data[4];
                String      errorMessage = (String)data[5];

                synchronized(storageDataMap)
                {
                  StorageData storageData = storageDataMap.get(id);
                  if (storageData != null)
                  {
                    storageData.size         = size;
                    storageData.datetime     = datetime;
                    storageData.indexState   = indexState;
                    storageData.errorMessage = errorMessage;
                  }
                  else
                  {
                    storageData = new StorageData(id,
                                                  storageName,
                                                  size,
                                                  datetime,
                                                  new File(storageName).getName(),
                                                  indexState,
                                                  errorMessage
                                                 );
                    storageDataMap.put(storageData);
                  }
                }

                storageNameHashSet.remove(storageName);
              }
            }
          }
        }
        catch (CommunicationError error)
        {
          /* ignored */
        }

        // remove not existing entries, but keep marked entries
        synchronized(storageDataMap)
        {
          for (String storageName : storageNameHashSet)
          {
            StorageData storageData = storageDataMap.get(storageName);
            if ((storageData != null) && !storageData.isTagged())
            {
              storageDataMap.remove(storageData);
            }
          }
        }

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
          // wait for trigger/timeout
          try { trigger.wait(30*1000); } catch (InterruptedException exception) { /* ignored */ };

          // get max. count
          if (newStorageMaxCount != -1)
          {
            storageMaxCount    = newStorageMaxCount;
            newStorageMaxCount = -1;
            setColorFlag       = false;
          }

          // get pattern
          if (newStoragePattern != null)
          {
            storagePattern    = newStoragePattern;
            newStoragePattern = null;
            setColorFlag      = false;
          }

          // get state filter
          if (newStorageIndexStateFilter != null)
          {
            storageIndexStateFilter    = newStorageIndexStateFilter;
            newStorageIndexStateFilter = null;
            setColorFlag               = false;
          }
        }
      }
    }

    /** trigger an update
     * @param storageMaxCount new max. entries in list
     * @param storagePattern new storage pattern
     * @param storageStateFilter new storage state filter
     */
    public void triggerUpdate(int storageMaxCount, String storagePattern, IndexStates storageIndexStateFilter)
    {
      synchronized(trigger)
      {
        newStorageMaxCount         = storageMaxCount;
        newStoragePattern          = storagePattern;
        newStorageIndexStateFilter = storageIndexStateFilter;
        setColorFlag               = true;
        trigger.notify();
      }
    }

    /** trigger an update
     * @param storagePattern new storage pattern
     * @param storageStateFilter
     */
    public void triggerUpdate(String storagePattern, IndexStates storageIndexStateFilter)
    {
      triggerUpdate(-1,storagePattern,storageIndexStateFilter);
    }

    /** trigger an update
     * @param storageMaxCount new max. entries in list
     */
    public void triggerUpdate(int storageMaxCount)
    {
      triggerUpdate(storageMaxCount,null,null);
    }

    /** trigger an update
     * @param storagePattern new storage pattern
     */
    public void triggerUpdate(String storagePattern)
    {
      triggerUpdate(-1,storagePattern,null);
    }

    /** trigger an update
     * @param storagePattern new storage pattern
     */
    public void triggerUpdate(IndexStates storageIndexStateFilter)
    {
      triggerUpdate(-1,null,storageIndexStateFilter);
    }

    /** trigger an update
     */
    public void triggerUpdate()
    {
      triggerUpdate(-1,null,null);
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
    long          datetime;
    boolean       tagged;
    RestoreStates restoreState;

    /** create entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param name entry name
     * @param type entry type
     * @param size size [bytes]
     * @param datetime date/time (timestamp)
     */
    EntryData(String storageName, long storageDateTime, String name, EntryTypes type, long size, long datetime)
    {
      this.storageName     = storageName;
      this.storageDateTime = storageDateTime;
      this.name            = name;
      this.type            = type;
      this.size            = size;
      this.datetime        = datetime;
      this.tagged          = false;
      this.restoreState    = RestoreStates.UNKNOWN;
    }

    /** create entry data
     * @param storageName archive name
     * @param storageDateTime archive date/time (timestamp)
     * @param name entry name
     * @param type entry type
     * @param datetime date/time (timestamp)
     */
    EntryData(String storageName, long storageDateTime, String name, EntryTypes type, long datetime)
    {
      this(storageName,storageDateTime,name,type,0L,datetime);
    }

    /** set restore state of entry
     * @param restoreState restore state
     */
    public void setState(RestoreStates restoreState)
    {
      this.restoreState = restoreState;
    }

    /** check if entry is tagged
     * @return true if entry is checked, false otherwise
     */
    public boolean isTagged()
    {
      return tagged;
    }

    /** set tagged state
     * @param checked checked state
     */
    public void setTagged(boolean tagged)
    {
      this.tagged = tagged;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "Entry {"+storageName+", "+name+", "+type+", "+size+" bytes, datetime="+datetime+", tagged="+tagged+", state="+restoreState+"}";
    }
  };

  /** entry data map
   */
  class EntryDataMap extends HashMap<String,EntryData>
  {
    /** remove not tagged entries
     */
    public void clearNotTagged()
    {
      String[] keys = keySet().toArray(new String[0]);
      for (String key : keys)
      {
        EntryData entryData = get(key);
        if (!entryData.isTagged()) remove(key);
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
    private final static int SORTMODE_ARCHIVE       = 0;
    private final static int SORTMODE_NAME          = 1;
    private final static int SORTMODE_TYPE          = 2;
    private final static int SORTMODE_SIZE          = 3;
    private final static int SORTMODE_DATE          = 4;
//    private final static int SORTMODE_RESTORE_STATE = 5;

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
//      else if (table.getColumn(5) == sortColumn) sortMode = SORTMODE_RESTORE_STATE;
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
          if      (entryData1.datetime < entryData2.datetime) return -1;
          else if (entryData1.datetime > entryData2.datetime) return  1;
          else                                                return  0;
//        case SORTMODE_RESTORE_STATE:
//          return entryData1.restoreState.compareTo(entryData2.restoreState);
        default:
          return 0;
      }
    }
  }

  /** update entry list thread
   */
  class UpdateEntryListThread extends Thread
  {
    private Object trigger          = new Object();   // trigger update object
    private int    newEntryMaxCount = -1;
    private String newEntryPattern  = null;           // entry pattern

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
      final String[] PATTERN_MAP_FROM  = new String[]{"\n","\r","\\"};
      final String[] PATTERN_MAP_TO    = new String[]{"\\n","\\r","\\\\"};
      final String[] FILENAME_MAP_FROM = new String[]{"\\n","\\r","\\\\"};
      final String[] FILENAME_MAP_TO   = new String[]{"\n","\r","\\"};

      int    entryMaxCount = 100;
      String entryPattern  = null;
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
          // clear not tagged entries
          entryDataMap.clearNotTagged();

          // get matching entries
          if (entryPattern != null)
          {
            // execute command
            ArrayList<String> result = new ArrayList<String>();
            String commandString = "INDEX_ENTRIES_LIST "+
                                   entryMaxCount+" "+
                                   (newestEntriesOnlyFlag ? "1" : "0")+" "+
                                   StringUtils.escape(StringUtils.map(entryPattern,PATTERN_MAP_FROM,PATTERN_MAP_TO));
//Dprintf.dprintf("commandString=%s",commandString);
            if (BARServer.executeCommand(commandString,result) == Errors.NONE)
            {
              // read results
              Object data[] = new Object[10];
              for (String line : result)
              {
                if      (StringParser.parse(line,"FILE %ld %ld %ld %d %d %d %ld %ld %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       storage date/time
                       size
                       date/time
                       user id
                       group id
                       permission
                       fragment offset
                       fragment size
                       storage name
                       file name
                  */
                  long   storageDateTime = (Long  )data[0];
                  long   size            = (Long  )data[1];
                  long   datetime        = (Long  )data[2];
                  long   fragmentOffset  = (Long  )data[6];
                  long   fragmentSize    = (Long  )data[7];
                  String storageName     = (String)data[8];
                  String fileName        = StringUtils.map((String)data[9],FILENAME_MAP_FROM,FILENAME_MAP_TO);

                  EntryData entryData = entryDataMap.get(storageName,fileName,EntryTypes.FILE);
                  if (entryData != null)
                  {
                    entryData.size     = size;
                    entryData.datetime = datetime;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,storageDateTime,fileName,EntryTypes.FILE,size,datetime);
                    entryDataMap.put(entryData);
                  }
                }
                else if (StringParser.parse(line,"IMAGE %ld %ld %ld %ld %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       storage date/time
                       size
                       blockOffset
                       blockCount
                       storage name
                       name
                  */
                  long   storageDateTime = (Long  )data[0];
                  long   size            = (Long  )data[1];
                  long   blockOffset     = (Long  )data[2];
                  long   blockCount      = (Long  )data[3];
                  String storageName     = (String)data[4];
                  String imageName       = StringUtils.map((String)data[5],FILENAME_MAP_FROM,FILENAME_MAP_TO);

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
                else if (StringParser.parse(line,"DIRECTORY %ld %ld %d %d %d %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       storage date/time
                       date/time
                       user id
                       group id
                       permission
                       storage name
                       directory name
                  */
                  long   storageDateTime = (Long  )data[0];
                  long   datetime        = (Long  )data[1];
                  String storageName     = (String)data[5];
                  String directoryName   = StringUtils.map((String)data[6],FILENAME_MAP_FROM,FILENAME_MAP_TO);

                  EntryData entryData = entryDataMap.get(storageName,directoryName,EntryTypes.DIRECTORY);
                  if (entryData != null)
                  {
                    entryData.datetime = datetime;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,storageDateTime,directoryName,EntryTypes.DIRECTORY,datetime);
                    entryDataMap.put(entryData);
                  }
                }
                else if (StringParser.parse(line,"LINK %ld %ld %d %d %d %S %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       storage date/time
                       date/time
                       user id
                       group id
                       permission
                       storage name
                       link name
                       destination name
                  */
                  long   storageDateTime = (Long  )data[0];
                  long   datetime        = (Long  )data[1];
                  String storageName     = StringUtils.map((String)data[5],FILENAME_MAP_FROM,FILENAME_MAP_TO);
                  String linkName        = StringUtils.map((String)data[6],FILENAME_MAP_FROM,FILENAME_MAP_TO);

                  EntryData entryData = entryDataMap.get(storageName,linkName,EntryTypes.LINK);
                  if (entryData != null)
                  {
                    entryData.datetime = datetime;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,storageDateTime,linkName,EntryTypes.LINK,datetime);
                    entryDataMap.put(entryData);
                  }
                }
                else if (StringParser.parse(line,"SPECIAL %ld %ld %d %d %d %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       storage date/time
                       date/time
                       user id
                       group id
                       permission
                       storage name
                       name
                  */
                  long   storageDateTime = (Long  )data[0];
                  long   datetime        = (Long  )data[1];
                  String storageName     = StringUtils.map((String)data[5],FILENAME_MAP_FROM,FILENAME_MAP_TO);
                  String name            = StringUtils.map((String)data[6],FILENAME_MAP_FROM,FILENAME_MAP_TO);

                  EntryData entryData = entryDataMap.get(storageName,name,EntryTypes.SPECIAL);
                  if (entryData != null)
                  {
                    entryData.datetime = datetime;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,storageDateTime,name,EntryTypes.SPECIAL,datetime);
                    entryDataMap.put(entryData);
                  }
                }
              }
            }
            else
            {
              final String errorText = result.get(0);
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  Dialogs.error(shell,"Cannot list entries (error: "+errorText+")");
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
          // wait for trigger
          while ((newEntryMaxCount == -1) && (newEntryPattern == null))
          {
            try { trigger.wait(); } catch (InterruptedException exception) { /* ignored */ };
          }

          // get pattern
          if (newEntryMaxCount != -1)
          {
            entryMaxCount    = newEntryMaxCount;
            newEntryMaxCount = -1;
          }
          if (newEntryPattern != null)
          {
            entryPattern    = newEntryPattern;
            newEntryPattern = null;
          }
        }
      }
    }

    /** trigger an update
     * @param entryMaxCount max. entries in list
     * @param entryPattern new entry pattern
     */
    public void triggerUpdate(int entryMaxCount, String entryPattern)
    {
      synchronized(trigger)
      {
        newEntryMaxCount = entryMaxCount;
        newEntryPattern  = entryPattern;
        trigger.notify();
      }
    }

    /** trigger an update
     * @param entryMaxCount max. entries in list
     */
    public void triggerUpdate(int entryMaxCount)
    {
      triggerUpdate(entryMaxCount,null);
    }

    /** trigger an update
     * @param entryPattern new entry pattern
     */
    public void triggerUpdate(String entryPattern)
    {
      triggerUpdate(-1,entryPattern);
    }
  }

  // --------------------------- constants --------------------------------
  // colors
  private final Color COLOR_MODIFIED;

  // images
  private final Image IMAGE_DIRECTORY;
  private final Image IMAGE_DIRECTORY_INCLUDED;
  private final Image IMAGE_DIRECTORY_EXCLUDED;
  private final Image IMAGE_FILE;
  private final Image IMAGE_FILE_INCLUDED;
  private final Image IMAGE_FILE_EXCLUDED;
  private final Image IMAGE_LINK;
  private final Image IMAGE_LINK_INCLUDED;
  private final Image IMAGE_LINK_EXCLUDED;

  private final Image IMAGE_CLEAR;
  private final Image IMAGE_MARK_ALL;
  private final Image IMAGE_UNMARK_ALL;

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
  private Combo           widgetPath;

  private Table           widgetStorageList;
  private Shell           widgetStorageListToolTip = null;
  private Text            widgetStoragePattern;
  private Combo           widgetStorageStateFilter;
  private Combo           widgetStorageMaxCount;
  private WidgetEvent     taggedStorageEvent = new WidgetEvent();

  private Table           widgetEntryList;
  private Shell           widgetEntryListToolTip = null;
  private Text            widgetEntryPattern;
  private Combo           widgetEntryMaxCount;
  private WidgetEvent     taggedEntryEvent = new WidgetEvent();

  private Button          widgetRestoreTo;
  private Text            widgetRestoreToDirectory;
  private Button          widgetOverwriteEntries;
  private WidgetEvent     selectRestoreToEvent = new WidgetEvent();

  UpdateStorageListThread updateStorageListThread;
  private String          storagePattern = null;
  private StorageDataMap  storageDataMap = new StorageDataMap();

  UpdateEntryListThread   updateEntryListThread;
  private String          entryPattern          = null;
  private boolean         newestEntriesOnlyFlag = true;
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
    IMAGE_DIRECTORY          = Widgets.loadImage(display,"directory.gif");
    IMAGE_DIRECTORY_INCLUDED = Widgets.loadImage(display,"directoryIncluded.gif");
    IMAGE_DIRECTORY_EXCLUDED = Widgets.loadImage(display,"directoryExcluded.gif");
    IMAGE_FILE               = Widgets.loadImage(display,"file.gif");
    IMAGE_FILE_INCLUDED      = Widgets.loadImage(display,"fileIncluded.gif");
    IMAGE_FILE_EXCLUDED      = Widgets.loadImage(display,"fileExcluded.gif");
    IMAGE_LINK               = Widgets.loadImage(display,"link.gif");
    IMAGE_LINK_INCLUDED      = Widgets.loadImage(display,"linkIncluded.gif");
    IMAGE_LINK_EXCLUDED      = Widgets.loadImage(display,"linkExcluded.gif");

    IMAGE_CLEAR              = Widgets.loadImage(display,"clear.gif");
    IMAGE_MARK_ALL           = Widgets.loadImage(display,"mark.gif");
    IMAGE_UNMARK_ALL         = Widgets.loadImage(display,"unmark.gif");

    // get cursors
    waitCursor = new Cursor(display,SWT.CURSOR_WAIT);

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Restore"+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{0.5,0.5,0.0},1.0,2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // storage list
    group = Widgets.newGroup(widgetTab,"Storage");
    group.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,4));
    Widgets.layout(group,0,0,TableLayoutData.NSWE);
    {
      widgetStorageList = Widgets.newTable(group,SWT.CHECK);
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
      tableColumn = Widgets.addTableColumn(widgetStorageList,3,"State",   SWT.LEFT,  30,true);
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

            StorageData storageData = (StorageData)tableItem.getData();
            storageData.setChecked(tableItem.getChecked());

            taggedStorageEvent.trigger();
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
            StorageData storageData = (StorageData)tableItem.getData();
            storageData.setChecked(tableItem.getChecked());
          }

          taggedStorageEvent.trigger();
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

            label = Widgets.newLabel(widgetStorageListToolTip,simpleDateFormat.format(new Date(storageData.datetime*1000)));
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

            label = Widgets.newLabel(widgetStorageListToolTip,"Error:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,storageData.errorMessage);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,1,TableLayoutData.WE);

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
        menuItem = Widgets.addMenuItem(menu,"Add...");
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

        menuItem = Widgets.addMenuItem(menu,"Remove...");
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

        menuItem = Widgets.addMenuItem(menu,"Remove all with error...");
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

        menuItem = Widgets.addMenuItem(menu,"Refresh...");
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

        menuItem = Widgets.addMenuItem(menu,"Refresh all with error...");
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

        menuItem = Widgets.addMenuItem(menu,"Mark all");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setTaggedStorage(true);
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
            setTaggedStorage(false);
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,"Restore");
        Widgets.addEventListener(new WidgetEventListener(menuItem,taggedStorageEvent)
        {
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(checkStorageTagged());
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
            restoreArchives(getTaggedStorageNameHashSet(),
                            widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                            widgetOverwriteEntries.getSelection()
                           );
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
        Widgets.addEventListener(new WidgetEventListener(button,taggedStorageEvent)
        {
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (checkStorageTagged())
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
            if (checkStorageTagged())
            {
              setTaggedStorage(false);
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText("Mark all entries in list.");
            }
            else
            {
              setTaggedStorage(true);
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText("Unmark all entries in list.");
            }
          }
        });
        button.setToolTipText("Mark all entries in list.");

        label = Widgets.newLabel(composite,"Filter:");
        Widgets.layout(label,0,1,TableLayoutData.W);

        widgetStoragePattern = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_CANCEL);
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
            IndexStates indexState = IndexStates.UNKNOWN;
            String indexStateText = widgetStorageStateFilter.getText();
            if      (indexStateText.equalsIgnoreCase("ok"))               indexState = IndexStates.OK;
            else if (indexStateText.equalsIgnoreCase("error"))            indexState = IndexStates.ERROR;
            else if (indexStateText.equalsIgnoreCase("update"))           indexState = IndexStates.UPDATE;
            else if (indexStateText.equalsIgnoreCase("update requested")) indexState = IndexStates.UPDATE_REQUESTED;
            else if (indexStateText.equalsIgnoreCase("*"))                indexState = IndexStates.ALL;
            else                                                          indexState = IndexStates.UNKNOWN;
            updateStorageListThread.triggerUpdate(widgetStoragePattern.getText(),indexState);
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
            updateStorageListThread.triggerUpdate(Integer.parseInt(widget.getText()));
          }
        });
        widgetStorageMaxCount.setToolTipText("Max. number of entries in list.");

        button = Widgets.newButton(composite,"Restore");
        button.setEnabled(false);
        Widgets.layout(button,0,7,TableLayoutData.DEFAULT,0,0,0,0,60,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,taggedStorageEvent)
        {
          public void trigger(Control control)
          {
            control.setEnabled(checkStorageTagged());
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
            restoreArchives(getTaggedStorageNameHashSet(),
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
    Widgets.layout(group,1,0,TableLayoutData.NSWE);
    {
      widgetEntryList = Widgets.newTable(group,SWT.CHECK);
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
//      tableColumn = Widgets.addTableColumn(widgetEntryList,5,"State",         SWT.LEFT,  60,true );
//      tableColumn.addSelectionListener(entryListColumnSelectionListener);
//      tableColumn.setToolTipText("Click to sort for state.");
      widgetEntryList.addListener(SWT.MouseDoubleClick,new Listener()
      {
        public void handleEvent(final Event event)
        {
          TableItem tableItem = widgetEntryList.getItem(new Point(event.x,event.y));
          if (tableItem != null)
          {
            tableItem.setChecked(!tableItem.getChecked());

            EntryData entryData = (EntryData)tableItem.getData();
            entryData.setTagged(tableItem.getChecked());

            taggedEntryEvent.trigger();
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
            entryData.setTagged(tableItem.getChecked());
          }

          taggedEntryEvent.trigger();
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

            label = Widgets.newLabel(widgetEntryListToolTip,simpleDateFormat.format(new Date(entryData.datetime*1000)));
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
            setTaggedEntries(true);
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
            setTaggedEntries(false);
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,"Restore");
        menuItem.setEnabled(false);
        Widgets.addEventListener(new WidgetEventListener(menuItem,taggedEntryEvent)
        {
          public void trigger(MenuItem menuItem)
          {
            menuItem.setEnabled(checkEntriesTagged());
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
            restoreEntries(getTaggedEntries(),
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
        Widgets.addEventListener(new WidgetEventListener(button,taggedEntryEvent)
        {
          public void trigger(Control control)
          {
            Button button = (Button)control;
            if (checkEntriesTagged())
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
            if (checkEntriesTagged())
            {
              setTaggedEntries(false);
              button.setImage(IMAGE_MARK_ALL);
              button.setToolTipText("Mark all entries in list.");
            }
            else
            {
              setTaggedEntries(true);
              button.setImage(IMAGE_UNMARK_ALL);
              button.setToolTipText("Unmark all entries in list.");
            }
          }
        });
        button.setToolTipText("Mark all entries in list.");

        label = Widgets.newLabel(composite,"Filter:");
        Widgets.layout(label,0,1,TableLayoutData.W);

        widgetEntryPattern = Widgets.newText(composite,SWT.SEARCH|SWT.ICON_CANCEL);
        Widgets.layout(widgetEntryPattern,0,2,TableLayoutData.WE);
        widgetEntryPattern.addSelectionListener(new SelectionListener()
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
        widgetEntryPattern.addKeyListener(new KeyListener()
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
        widgetEntryPattern.addFocusListener(new FocusListener()
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
        widgetEntryPattern.setToolTipText("Enter filter pattern for entry list. Wildcards: * and ?.");

        button = Widgets.newCheckbox(composite,"newest entries only");
        button.setSelection(newestEntriesOnlyFlag);
        Widgets.layout(button,0,3,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button  widget = (Button)selectionEvent.widget;
            newestEntriesOnlyFlag = widget.getSelection();
            updateEntryListThread.triggerUpdate(entryPattern);
          }
        });
        button.setToolTipText("When this checkbox is enabled, only show newest entry instances and hide all older entry instances.");

        label = Widgets.newLabel(composite,"Max:");
        Widgets.layout(label,0,4,TableLayoutData.W);

        widgetEntryMaxCount = Widgets.newOptionMenu(composite);
        widgetEntryMaxCount.setItems(new String[]{"10","50","100","500","1000"});
        widgetEntryMaxCount.setText("100");
        Widgets.layout(widgetEntryMaxCount,0,5,TableLayoutData.W);
        widgetEntryMaxCount.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo widget = (Combo)selectionEvent.widget;
            updateEntryListThread.triggerUpdate(Integer.parseInt(widget.getText()));
          }
        });
        widgetEntryMaxCount.setToolTipText("Max. number of entries in list.");

        button = Widgets.newButton(composite,"Restore");
        button.setEnabled(false);
        Widgets.layout(button,0,6,TableLayoutData.DEFAULT,0,0,0,0,60,SWT.DEFAULT);
        Widgets.addEventListener(new WidgetEventListener(button,taggedEntryEvent)
        {
          public void trigger(Control control)
          {
            control.setEnabled(checkEntriesTagged());
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
            restoreEntries(getTaggedEntries(),
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
    Widgets.layout(group,2,0,TableLayoutData.WE);
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
      widgetRestoreTo.setToolTipText("Enable this checkbox and select a directory to restore entries to different directory.");

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
      widgetOverwriteEntries.setToolTipText("Enable this checkbox when existing entries should be overwritten.");
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
   * @param tagged true for set tagged, false for clear tagged
   */
  private void setTaggedStorage(boolean tagged)
  {
    for (TableItem tableItem : widgetStorageList.getItems())
    {
      tableItem.setChecked(tagged);

      StorageData storageData = (StorageData)tableItem.getData();
      storageData.setChecked(tagged);
    }

    taggedStorageEvent.trigger();
  }

  /** get tagged storage names
   * @param storageNamesHashSet storage hash set to fill
   * @return tagged storage name hash set
   */
  private HashSet<String> getTaggedStorageNameHashSet(HashSet<String> storageNamesHashSet)
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

  /** get tagged storage names
   * @return tagged storage name hash set
   */
  private HashSet<String> getTaggedStorageNameHashSet()
  {
    return getTaggedStorageNameHashSet(new HashSet<String>());
  }

  /** get tagged storage
   * @param storageNamesHashSet storage hash set to fill
   * @return tagged storage hash set
   */
  private HashSet<StorageData> getTaggedStorageHashSet(HashSet<StorageData> storageNamesHashSet)
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

  /** get tagged storage
   * @return tagged storage hash set
   */
  private HashSet<StorageData> getTaggedStorageHashSet()
  {
    return getTaggedStorageHashSet(new HashSet<StorageData>());
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

  /** check if some storage entries are tagged
   * @return true iff some entry is tagged
   */
  private boolean checkStorageTagged()
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
//??? statt findStorageListIndex
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
                                      simpleDateFormat.format(new Date(storageData.datetime*1000)),
                                      storageData.indexState.toString()
                                     )
           )
        {
          Widgets.insertTableEntry(widgetStorageList,
                                   findStorageListIndex(storageData),
                                   (Object)storageData,
                                   storageData.name,
                                   Units.formatByteSize(storageData.size),
                                   simpleDateFormat.format(new Date(storageData.datetime*1000)),
                                   storageData.indexState.toString()
                                  );
        }

        // set checkbox
        Widgets.setTableEntryChecked(widgetStorageList,
                                     (Object)storageData,
                                     storageData.isTagged()
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
        updateStorageListThread.triggerUpdate(string);
      }
    }
    else
    {
      if (storagePattern != null)
      {
        storagePattern = null;
        updateStorageListThread.triggerUpdate(string);
      }
    }
  }

  /** add storage to database index
   */
  private void addStorageIndex()
  {
    Label      label;
    Composite  composite;
    Button     button;
    final Text widgetStorageName;
    Button     widgetAdd;

    // create dialog
    final Shell dialog = Dialogs.open(shell,"Add storage to database index",400,SWT.DEFAULT,new double[]{1.0,0.0},1.0);

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
      String[] result = new String[1];
      int errorCode = BARServer.executeCommand("INDEX_STORAGE_ADD "+StringUtils.escape(storageName),result);
      if (errorCode == Errors.NONE)
      {
        updateStorageListThread.triggerUpdate(storagePattern);
      }
      else
      {
        Dialogs.error(shell,"Cannot add database index for storage file\n\n'"+storageName+"'\n\n(error: "+result[0]+")");
      }
    }
  }

  /** remove storage from database index
   */
  private void removeStorageIndex()
  {
    try
    {
      HashSet<StorageData> selectedStorageHashSet = new HashSet<StorageData>();

      getTaggedStorageHashSet(selectedStorageHashSet);
      getSelectedStorageHashSet(selectedStorageHashSet);
      if (!selectedStorageHashSet.isEmpty())
      {
        if (Dialogs.confirm(shell,"Really remove index for "+selectedStorageHashSet.size()+" entries?"))
        {
          for (StorageData storageData : selectedStorageHashSet)
          {
            // get archive name parts
            ArchiveNameParts archiveNameParts = new ArchiveNameParts(storageData.name);

            // remove entry
            String[] result = new String[1];
            int errorCode = BARServer.executeCommand("INDEX_STORAGE_REMOVE "+
                                                     "* "+
                                                     storageData.id,
                                                     result
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
              Dialogs.error(shell,"Cannot remove database index for storage file\n\n'"+archiveNameParts.getPrintableName()+"'\n\n(error: "+result[0]+")");
            }
          }
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,"Communication error while removing database index (error: "+error.toString()+")");
    }
  }

  /** remove all storage from database index with error
   */
  private void removeAllWithErrorStorageIndex()
  {
    try
    {
      if (Dialogs.confirm(shell,"Really remove all indizes with error state?"))
      {
        String[] result = new String[1];
        int errorCode = BARServer.executeCommand("INDEX_STORAGE_REMOVE "+
                                                 "ERROR "+
                                                 "0",
                                                 result
                                                );
        if (errorCode == Errors.NONE)
        {
          updateStorageListThread.triggerUpdate();
        }
        else
        {
          Dialogs.error(shell,"Cannot remove database indizes with error state' (error: "+result[0]+")");
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,"Communication error while removing database indizes (error: "+error.toString()+")");
    }
  }

  /** refresh storage from database index
   */
  private void refreshStorageIndex()
  {
    try
    {
      HashSet<StorageData> selectedStorageHashSet = new HashSet<StorageData>();

      getTaggedStorageHashSet(selectedStorageHashSet);
      getSelectedStorageHashSet(selectedStorageHashSet);
      if (!selectedStorageHashSet.isEmpty())
      {
        if (Dialogs.confirm(shell,"Really refresh index for "+selectedStorageHashSet.size()+" entries?"))
        {
          for (StorageData storageData : selectedStorageHashSet)
          {
            String[] result = new String[1];
            int errorCode = BARServer.executeCommand("INDEX_STORAGE_REFRESH "+
                                                     "* "+
                                                     storageData.id,
                                                     result
                                                    );
            if (errorCode == Errors.NONE)
            {
              storageData.indexState = IndexStates.UPDATE_REQUESTED;
            }
            else
            {
              Dialogs.error(shell,"Cannot refresh database index for storage file\n\n'"+storageData.name+"'\n\n(error: "+result[0]+")");
            }
          }

          refreshStorageList();
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,"Communication error while refreshing database index (error: "+error.toString()+")");
    }
  }

  /** refresh all storage from database index with error
   */
  private void refreshAllWithErrorStorageIndex()
  {
    try
    {
      if (Dialogs.confirm(shell,"Really refresh all indizes with error state?"))
      {
        String[] result = new String[1];
        int errorCode = BARServer.executeCommand("INDEX_STORAGE_REFRESH "+
                                                 "ERROR "+
                                                 "0",
                                                 result
                                                );
        if (errorCode == Errors.NONE)
        {
          updateStorageListThread.triggerUpdate();
        }
        else
        {
          Dialogs.error(shell,"Cannot refresh database indizes with error state' (error: "+result[0]+")");
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

              ArrayList<String> result = new ArrayList<String>();
              String commandString = "RESTORE "+
                                     StringUtils.escape(storageName)+" "+
                                     StringUtils.escape(directory)+" "+
                                     (overwriteEntries?"1":"0")
                                     ;
//Dprintf.dprintf("command=%s",commandString);
              command = BARServer.runCommand(commandString);

              // read results, update/add data
              String line;
              Object data[] = new Object[5];
              while (   !command.isCompleted()
                     && !busyDialog.isAborted()
                    )
              {
                line = command.getNextResult(60*1000);
                if (line != null)
                {
//Dprintf.dprintf("line=%s",line);
                  if      (StringParser.parse(line,"%ld %ld %ld %ld %S",data,StringParser.QUOTE_CHARS))
                  {
                    /* get data
                       format:
                         doneBytes
                         totalBytes
                         archiveDoneBytes
                         archiveTotalBytes
                         name
                    */
                    long   doneBytes         = (Long)data[0];
                    long   totalBytes        = (Long)data[1];
                    long   archiveDoneBytes  = (Long)data[2];
                    long   archiveTotalBytes = (Long)data[3];
                    String name              = (String)data[4];

                    busyDialog.updateText(1,name);
                    busyDialog.updateProgressBar(1,(totalBytes > 0) ? ((double)doneBytes*100.0)/(double)totalBytes : 0.0);
                  }
                }
                else
                {
                  busyDialog.update();
                }
              }
//Dprintf.dprintf("command=%s",command);

              if (   (   (command.getErrorCode() == Errors.NO_CRYPT_PASSWORD)
                      || (command.getErrorCode() == Errors.INVALID_PASSWORD)
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
                      BARServer.executeCommand("DECRYPT_PASSWORD_ADD "+StringUtils.escape(password));
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
              busyDialog.updateText("Aborting...");
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
   * @param tagged true for set tagged, false for clear tagged
   */
  private void setTaggedEntries(boolean tagged)
  {
    for (TableItem tableItem : widgetEntryList.getItems())
    {
      tableItem.setChecked(tagged);

      EntryData entryData = (EntryData)tableItem.getData();
      entryData.setTagged(tagged);
    }

    taggedEntryEvent.trigger();
  }

  /** get tagged entries
   * @return tagged entries data
   */
  private EntryData[] getTaggedEntries()
  {
    ArrayList<EntryData> entryDataArray = new ArrayList<EntryData>();

    synchronized(entryDataMap)
    {
      for (EntryData entryData : entryDataMap.values())
      {
        if (entryData.isTagged()) entryDataArray.add(entryData);
      }
    }

    return entryDataArray.toArray(new EntryData[entryDataArray.size()]);
  }

  /** check if some entry is tagged
   * @return tree iff some entry is tagged
   */
  private boolean checkEntriesTagged()
  {
    synchronized(entryDataMap)
    {
      for (EntryData entryData : entryDataMap.values())
      {
        if (entryData.isTagged()) return true;
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
                                     simpleDateFormat.format(new Date(entryData.datetime*1000))
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
                                     simpleDateFormat.format(new Date(entryData.datetime*1000))
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
                                     simpleDateFormat.format(new Date(entryData.datetime*1000))
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
                                     simpleDateFormat.format(new Date(entryData.datetime*1000))
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
                                     simpleDateFormat.format(new Date(entryData.datetime*1000))
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
                                     simpleDateFormat.format(new Date(entryData.datetime*1000))
                                    );
            break;
        }

        Widgets.setTableEntryChecked(widgetEntryList,
                                     (Object)entryData,
                                     entryData.isTagged()
                                    );
      }
    }

    // enable/disable restore button
    taggedEntryEvent.trigger();
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
        updateEntryListThread.triggerUpdate(entryPattern);
      }
    }
    else
    {
      if (entryPattern != null)
      {
        entryPattern = null;
        updateEntryListThread.triggerUpdate(entryPattern);
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
        final String[] FILENAME_MAP_FROM = new String[]{"\n","\r","\\"};
        final String[] FILENAME_MAP_TO   = new String[]{"\\n","\\r","\\\\"};

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
            boolean passwordFlag = false;
            do
            {
              retryFlag = false;

              ArrayList<String> result = new ArrayList<String>();
              String commandString = "RESTORE "+
                                     StringUtils.escape(entryData.storageName)+" "+
                                     StringUtils.escape(directory)+" "+
                                     (overwriteEntries?"1":"0")+" "+
                                     StringUtils.escape(StringUtils.map(entryData.name,FILENAME_MAP_FROM,FILENAME_MAP_TO))
                                     ;
//Dprintf.dprintf("command=%s",commandString);
              command = BARServer.runCommand(commandString);

              // read results, update/add data
              String line;
              Object data[] = new Object[5];
              while (   !command.isCompleted()
                     && !busyDialog.isAborted()
                    )
              {
                line = command.getNextResult(60*1000);
                if (line != null)
                {
//Dprintf.dprintf("line=%s",line);
                  if      (StringParser.parse(line,"%ld %ld %ld %ld %S",data,StringParser.QUOTE_CHARS))
                  {
                    /* get data
                       format:
                         doneBytes
                         totalBytes
                         archiveDoneBytes
                         archiveTotalBytes
                         name
                    */
                    long   doneBytes         = (Long)data[0];
                    long   totalBytes        = (Long)data[1];
                    long   archiveDoneBytes  = (Long)data[2];
                    long   archiveTotalBytes = (Long)data[3];
                    String name              = StringUtils.map((String)data[4],FILENAME_MAP_FROM,FILENAME_MAP_TO);

                    busyDialog.updateText(1,name);
                    busyDialog.updateProgressBar(1,(totalBytes > 0) ? ((double)doneBytes*100.0)/(double)totalBytes : 0.0);
                  }
                }
                else
                {
                  busyDialog.update();
                }
              }

              if (   (   (command.getErrorCode() == Errors.NO_CRYPT_PASSWORD)
                      || (command.getErrorCode() == Errors.INVALID_PASSWORD)
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
                      BARServer.executeCommand("DECRYPT_PASSWORD_ADD "+StringUtils.escape(password));
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
                    Dialogs.error(shell,"Cannot restore entry\n\n'"+entryData.name+"'\n\nfrom archive\n\n'"+entryData.storageName+"'\n\n(error: "+errorText+")");
                  }
                });
              }
            }
            else
            {
              busyDialog.updateText("Aborting...");
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
