/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TabRestore.java,v $
* $Revision: 1.15 $
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
import java.util.LinkedHashSet;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

import java.util.Collection;

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
    INDEX_UPDATE_REQUESTED,
    INDEX_UPDATE,
    ERROR,
    UNKNOWN;

    /** convert to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case OK:                     return "ok";
        case CREATE:                 return "creating";
        case INDEX_UPDATE_REQUESTED: return "update requested";
        case INDEX_UPDATE:           return "update";
        case ERROR:                  return "error";
        default:                     return "ok";
      }
    }
  };

  /** storage data
   */
  class StorageData
  {
    String      name;
    long        size;
    long        datetime;
    String      title;
    IndexStates state;
    String      errorMessage;
    boolean     tagged;

    /** create storage data
     * @param name name
     * @param size size [bytes]
     * @param datetime date/time (timestamp)
     * @param title title to show
     * @param state storage state
     * @param errorMessage error message text
     */
    StorageData(String name, long size, long datetime, String title, IndexStates state, String errorMessage)
    {
      this.name         = name;
      this.size         = size;
      this.datetime     = datetime;
      this.title        = title;
      this.state        = state;
      this.errorMessage = errorMessage;
      this.tagged       = false;
    }

    /** create storage data
     * @param name name
     * @param datetime date/time (timestamp)
     * @param title title to show
     */
    StorageData(String name, long datetime, String title)
    {
      this(name,0,datetime,title,IndexStates.OK,null);
    }

    /** create storage data
     * @param name name
     * @param title title to show
     */
    StorageData(String name, String title)
    {
      this(name,0,title);
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

    /** convert to string
     * @return string
     */
    public String toString()
    {
      return "Storage {"+name+", "+size+" bytes, datetime="+datetime+", title="+title+", state="+state+", tagged="+tagged+"}";
    }
  };

  /** storage data map
   */
  class StorageDataMap extends HashMap<String,StorageData>
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

    /** put storage data into map
     * @param storageData storage data
     */
    public void put(StorageData storageData)
    {
      put(storageData.name,storageData);
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
          return storageData1.state.compareTo(storageData2.state);
        default:
          return 0;
      }
    }

    /** convert to string
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
    private Object  trigger            = new Object();   // trigger update object
    private int     newStorageMaxCount = -1;
    private String  newStoragePattern  = null;           // new storage pattern
    private boolean setColorFlag       = false;          // true to set color at update

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
      int    storageMaxCount = 100;
      String storagePattern  = null;
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
          for (String storageName : storageDataMap.keySet())
          {
            storageNameHashSet.add(storageName);
          }
        }

        // update
        try
        {
          String commandString = "INDEX_STORAGE_LIST "+
                                 storageMaxCount+" "+
                                 "* "+
                                 (((storagePattern != null) && !storagePattern.equals("")) ? StringUtils.escape(storagePattern) : "*");
  //Dprintf.dprintf("commandString=%s",commandString);
          Command command = BARServer.runCommand(commandString);

          // read results, update/add data
          String line;
          Object data[] = new Object[5];
          while (!command.endOfData())
          {
            line = command.getNextResult(5*1000);
            if (line != null)
            {
              if      (StringParser.parse(line,"%ld %ld %S %S %S",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     size
                     date/time
                     state
                     storage name
                     error message
                */
                long        size         = (Long)data[0];
                long        datetime     = (Long)data[1];
                IndexStates state        = Enum.valueOf(IndexStates.class,(String)data[2]);
                String      storageName  = (String)data[3];
                String      errorMessage = (String)data[4];

                synchronized(storageDataMap)
                {
                  StorageData storageData = storageDataMap.get(storageName);
                  if (storageData != null)
                  {
                    storageData.size         = size;
                    storageData.datetime     = datetime;
                    storageData.state        = state;
                    storageData.errorMessage = errorMessage;
                  }
                  else
                  {
                    storageData = new StorageData(storageName,size,datetime,new File(storageName).getName(),state,errorMessage);
                    storageDataMap.put(storageData);
                  }
                }

                storageNameHashSet.remove(storageName);
              }
  //else {
  //Dprintf.dprintf("xxxxxxxxxxx "+line);
  //}
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
            if (!storageData.isTagged())
            {
              storageDataMap.remove(storageName);
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
          // wait for trigger
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
        }
      }
    }

    /** trigger an update
     * @param storageMaxCount new max. entries in list
     * @param storagePattern new storage pattern
     */
    public void triggerUpdate(int storageMaxCount, String storagePattern)
    {
      synchronized(trigger)
      {
        newStorageMaxCount = storageMaxCount;
        newStoragePattern  = storagePattern;
        setColorFlag       = true;
        trigger.notify();
      }
    }

    /** trigger an update
     * @param storageMaxCount new max. entries in list
     */
    public void triggerUpdate(int storageMaxCount)
    {
      triggerUpdate(storageMaxCount,null);
    }

    /** trigger an update
     * @param storagePattern new storage pattern
     */
    public void triggerUpdate(String storagePattern)
    {
      triggerUpdate(-1,storagePattern);
    }

    /** trigger an update
     */
    public void triggerUpdate()
    {
      triggerUpdate(-1,null);
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
    String        name;
    EntryTypes    type;
    long          size;
    long          datetime;
    boolean       tagged;
    RestoreStates restoreState;

    /** create entry data
     * @param storageName archive name
     * @param name entry name
     * @param type entry type
     * @param size size [bytes]
     * @param datetime date/time (timestamp)
     */
    EntryData(String storageName, String name, EntryTypes type, long size, long datetime)
    {
      this.storageName  = storageName;
      this.name         = name;
      this.type         = type;
      this.size         = size;
      this.datetime     = datetime;
      this.tagged       = false;
      this.restoreState = RestoreStates.UNKNOWN;
    }

    /** create entry data
     * @param storageName archive name
     * @param name entry name
     * @param type entry type
     * @param datetime date/time (timestamp)
     */
    EntryData(String storageName, String name, EntryTypes type, long datetime)
    {
      this(storageName,name,type,0L,datetime);
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

    /** convert to string
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
    private final static int SORTMODE_RESTORE_STATE = 5;

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
      else if (table.getColumn(5) == sortColumn) sortMode = SORTMODE_RESTORE_STATE;
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
        case SORTMODE_RESTORE_STATE:
          return entryData1.restoreState.compareTo(entryData2.restoreState);
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
                                   StringUtils.escape(entryPattern);
//Dprintf.dprintf("commandString=%s",commandString);
            if (BARServer.executeCommand(commandString,result) == Errors.NONE)
            {
              // read results
              Object data[] = new Object[10];
              for (String line : result)
              {
                if      (StringParser.parse(line,"FILE %ld %ld %d %d %d %ld %ld %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
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
                  long   size           = (Long  )data[0];
                  long   datetime       = (Long  )data[1];
                  long   fragmentOffset = (Long  )data[5];
                  long   fragmentSize   = (Long  )data[6];
                  String storageName    = (String)data[7];
                  String fileName       = (String)data[8];

                  EntryData entryData = entryDataMap.get(storageName,fileName,EntryTypes.FILE);
                  if (entryData != null)
                  {
                    entryData.size     = size;
                    entryData.datetime = datetime;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,fileName,EntryTypes.FILE,size,datetime);
                    entryDataMap.put(entryData);
                  }
                }
                else if (StringParser.parse(line,"IMAGE %ld %ld %ld %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       size
                       blockOffset
                       blockCount
                       storage name
                       name
                  */
                  long   size        = (Long  )data[0];
                  long   blockOffset = (Long  )data[1];
                  long   blockCount  = (Long  )data[2];
                  String storageName = (String)data[3];
                  String imageName   = (String)data[4];

                  EntryData entryData = entryDataMap.get(storageName,imageName,EntryTypes.IMAGE);
                  if (entryData != null)
                  {
                    entryData.size = size;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,imageName,EntryTypes.IMAGE,size,0L);
                    entryDataMap.put(entryData);
                  }
                }
                else if (StringParser.parse(line,"DIRECTORY %ld %d %d %d %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       date/time
                       user id
                       group id
                       permission
                       storage name
                       directory name
                  */
                  long   datetime      = (Long  )data[0];
                  String storageName   = (String)data[4];
                  String directoryName = (String)data[5];

                  EntryData entryData = entryDataMap.get(storageName,directoryName,EntryTypes.DIRECTORY);
                  if (entryData != null)
                  {
                    entryData.datetime = datetime;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,directoryName,EntryTypes.DIRECTORY,datetime);
                    entryDataMap.put(entryData);
                  }
                }
                else if (StringParser.parse(line,"LINK %ld %d %d %d %S %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       date/time
                       user id
                       group id
                       permission
                       storage name
                       link name
                       destination name
                  */
                  long   datetime    = (Long  )data[0];
                  String storageName = (String)data[4];
                  String linkName    = (String)data[5];

                  EntryData entryData = entryDataMap.get(storageName,linkName,EntryTypes.LINK);
                  if (entryData != null)
                  {
                    entryData.datetime = datetime;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,linkName,EntryTypes.LINK,datetime);
                    entryDataMap.put(entryData);
                  }
                }
                else if (StringParser.parse(line,"SPECIAL %ld %d %d %d %S %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       date/time
                       user id
                       group id
                       permission
                       storage name
                       name
                  */
                  long   datetime    = (Long  )data[0];
                  String storageName = (String)data[4];
                  String name        = (String)data[5];

                  EntryData entryData = entryDataMap.get(storageName,name,EntryTypes.SPECIAL);
                  if (entryData != null)
                  {
                    entryData.datetime = datetime;
                  }
                  else
                  {
                    entryData = new EntryData(storageName,name,EntryTypes.SPECIAL,datetime);
                    entryDataMap.put(entryData);
                  }
                }
//else {
//Dprintf.dprintf("rrrrrrrrrrrrrrrr=%s\n",line);
//}
              }
            }
            else
            {
Dprintf.dprintf("r=%s",result);
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
  private Combo           widgetStorageMaxCount;

  private Table           widgetEntryList;
  private Shell           widgetEntryListToolTip = null;
  private Text            widgetEntryPattern;
  private Combo           widgetEntryMaxCount;

  private Button          widgetRestoreArchivesButton;
  private MenuItem        menuItemRestoreArchivesButton;
  private Button          widgetRestoreEntriesButton;
  private MenuItem        menuItemRestoreEntriesButton;

  private Button          widgetRestoreTo;
  private Text            widgetRestoreToDirectory;
  private Button          widgetRestoreToSelectButton;
  private Button          widgetOverwriteEntries;

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

/*
    // path
    composite = Widgets.newComposite(widgetTab);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0}));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,"Path:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPath = Widgets.newCombo(composite);
      Widgets.layout(widgetPath,0,1,TableLayoutData.WE);
      widgetPath.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Combo  widget   = (Combo)selectionEvent.widget;
          String pathName = widget.getText();
          setArchivePath(pathName);
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo widget = (Combo)selectionEvent.widget;
          String pathName = widget.getText();
          setArchivePath(pathName);
        }
      });
      widgetPath.addFocusListener(new FocusListener()
      {
        public void focusGained(FocusEvent focusEvent)
        {
        }
        public void focusLost(FocusEvent focusEvent)
        {
          Combo  widget   = (Combo)focusEvent.widget;
          String pathName = widget.getText();
          setArchivePath(pathName);
        }
      });

      button = Widgets.newButton(composite,IMAGE_DIRECTORY);
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          String pathName = Dialogs.directory(shell,
                                              "Select path",
                                              widgetPath.getText()
                                             );
          if (pathName != null)
          {
            widgetPath.setText(pathName);
            setArchivePath(pathName);
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }
*/

    // storage list
    group = Widgets.newGroup(widgetTab,"Storage");
    group.setLayout(new TableLayout(new double[]{1.0,0.0},1.0,4));
    Widgets.layout(group,0,0,TableLayoutData.NSWE);
    {
      widgetStorageList = Widgets.newTable(group,SWT.CHECK);
      Widgets.layout(widgetStorageList,0,0,TableLayoutData.NSWE);
      SelectionListener storageListColumnSelectionListener = new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableColumn           tableColumn           = (TableColumn)selectionEvent.widget;
          StorageDataComparator storageDataComparator = new StorageDataComparator(widgetStorageList,tableColumn);
          synchronized(widgetStorageList)
          {
            Widgets.sortTableColumn(widgetStorageList,tableColumn,storageDataComparator);
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      };
      tableColumn = Widgets.addTableColumn(widgetStorageList,0,"Name",    SWT.LEFT, 450,true);
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for name.");
      tableColumn = Widgets.addTableColumn(widgetStorageList,1,"Size",    SWT.RIGHT,100,false);
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for size.");
      tableColumn = Widgets.addTableColumn(widgetStorageList,2,"Modified",SWT.LEFT, 150,false);
      tableColumn.addSelectionListener(storageListColumnSelectionListener);
      tableColumn.setToolTipText("Click to sort for modification date/time.");
      tableColumn = Widgets.addTableColumn(widgetStorageList,3,"State",   SWT.LEFT,  30,false);
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

            boolean storageTagged = checkStorageTagged();
            widgetRestoreArchivesButton.setEnabled(storageTagged);
            menuItemRestoreArchivesButton.setEnabled(storageTagged);
          }
        }
      });
      widgetStorageList.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableItem tableItem = (TableItem)selectionEvent.item;
          if (tableItem != null)
          {
            StorageData storageData = (StorageData)tableItem.getData();
            storageData.setChecked(tableItem.getChecked());
          }

          boolean storageTagged = checkStorageTagged();
          widgetRestoreArchivesButton.setEnabled(storageTagged);
          menuItemRestoreArchivesButton.setEnabled(storageTagged);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
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

            label = Widgets.newLabel(widgetStorageListToolTip,"Size:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,String.format("%d bytes (%s)",storageData.size,Units.formatByteSize(storageData.size)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageListToolTip,"Date:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,2,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,simpleDateFormat.format(new Date(storageData.datetime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,2,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetStorageListToolTip,"State:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetStorageListToolTip,storageData.state.toString());
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
            Point point = table.toDisplay(bounds.x,bounds.y);
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
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            addStorageIndex();
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Remove");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            removeStorageIndex();
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Refresh");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            refreshStorageIndex();
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Refresh all with error");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            refreshAllWithErrorStorageIndex();
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItem = Widgets.addMenuItem(menu,"Mark all");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setTaggedStorage(true);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Unmark all");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setTaggedStorage(false);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItemRestoreArchivesButton = Widgets.addMenuItem(menu,"Restore");
        menuItemRestoreArchivesButton.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            restoreArchives(getTaggedStorageNameHashSet(),
                            widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                            widgetOverwriteEntries.getSelection()
                           );
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }
      widgetStorageList.setMenu(menu);

      // storage list filter
      composite = Widgets.newComposite(group);
      composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(composite,1,0,TableLayoutData.WE);
      {
        label = Widgets.newLabel(composite,"Filter:");
        Widgets.layout(label,0,0,TableLayoutData.W);

        widgetStoragePattern = Widgets.newText(composite);
        Widgets.layout(widgetStoragePattern,0,1,TableLayoutData.WE);
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

        button = Widgets.newButton(composite,IMAGE_CLEAR);
        Widgets.layout(button,0,2,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            widgetStoragePattern.setText("");
            setStoragePattern("");
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        button.setToolTipText("Clear storage filter pattern.");

        widgetRestoreArchivesButton = Widgets.newButton(composite,"Restore");
        widgetRestoreArchivesButton.setEnabled(false);
        Widgets.layout(widgetRestoreArchivesButton,0,3,TableLayoutData.DEFAULT,0,0,0,0,60,SWT.DEFAULT);
        widgetRestoreArchivesButton.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            restoreArchives(getTaggedStorageNameHashSet(),
                            widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                            widgetOverwriteEntries.getSelection()
                           );
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        widgetRestoreArchivesButton.setToolTipText("Start restoring selected archives.");

        button = Widgets.newButton(composite,IMAGE_MARK_ALL);
        Widgets.layout(button,0,4,TableLayoutData.E);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            setTaggedStorage(true);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        button.setToolTipText("Mark all entries in list.");

        button = Widgets.newButton(composite,IMAGE_UNMARK_ALL);
        Widgets.layout(button,0,5,TableLayoutData.E);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            setTaggedStorage(false);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        button.setToolTipText("Unmark all entries in list.");

        widgetStorageMaxCount = Widgets.newOptionMenu(composite);
        widgetStorageMaxCount.setItems(new String[]{"10","50","100","500","1000"});
        widgetStorageMaxCount.setText("100");
        Widgets.layout(widgetStorageMaxCount,0,6,TableLayoutData.W);
        widgetStorageMaxCount.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo widget = (Combo)selectionEvent.widget;
            updateStorageListThread.triggerUpdate(Integer.parseInt(widget.getText()));
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        widgetStorageMaxCount.setToolTipText("Max. number of entries in list.");
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
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
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
      tableColumn = Widgets.addTableColumn(widgetEntryList,5,"State",         SWT.LEFT,  60,true );
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
            entryData.setTagged(tableItem.getChecked());

            boolean entriesTagged = checkEntriesTagged();
            widgetRestoreEntriesButton.setEnabled(entriesTagged);
            menuItemRestoreEntriesButton.setEnabled(entriesTagged);
          }
        }
      });
      widgetEntryList.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableItem tableItem = (TableItem)selectionEvent.item;
          if (tableItem != null)
          {
            EntryData entryData = (EntryData)tableItem.getData();
            entryData.setTagged(tableItem.getChecked());
          }

          boolean entriesTagged = checkEntriesTagged();
          widgetRestoreEntriesButton.setEnabled(entriesTagged);
          menuItemRestoreEntriesButton.setEnabled(entriesTagged);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
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

            final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
            final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

            widgetEntryListToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
            widgetEntryListToolTip.setBackground(COLOR_BACKGROUND);
            widgetEntryListToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
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

            label = Widgets.newLabel(widgetEntryListToolTip,"Type:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,entryData.type.toString());
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,1,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryListToolTip,"Name:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,2,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,entryData.name);
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,2,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryListToolTip,"Size:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,String.format("%d bytes (%s)",entryData.size,Units.formatByteSize(entryData.size)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,3,1,TableLayoutData.WE);

            label = Widgets.newLabel(widgetEntryListToolTip,"Date:");
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,0,TableLayoutData.W);

            label = Widgets.newLabel(widgetEntryListToolTip,simpleDateFormat.format(new Date(entryData.datetime*1000)));
            label.setForeground(COLOR_FORGROUND);
            label.setBackground(COLOR_BACKGROUND);
            Widgets.layout(label,4,1,TableLayoutData.WE);

            Point size = widgetEntryListToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
            Rectangle bounds = tableItem.getBounds(0);
            Point point = table.toDisplay(bounds.x,bounds.y);
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
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setTaggedEntries(true);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Unmark all");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            setTaggedEntries(false);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        Widgets.addMenuSeparator(menu);

        menuItemRestoreEntriesButton = Widgets.addMenuItem(menu,"Restore");
        menuItemRestoreEntriesButton.setEnabled(false);
        menuItemRestoreEntriesButton.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            restoreEntries(getTaggedEntries(),
                           widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                           widgetOverwriteEntries.getSelection()
                          );
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }
      widgetEntryList.setMenu(menu);

      // entry list filter
      composite = Widgets.newComposite(group);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0,0.0,0.0,0.0}));
      Widgets.layout(composite,1,0,TableLayoutData.WE);
      {
        label = Widgets.newLabel(composite,"Filter:");
        Widgets.layout(label,0,0,TableLayoutData.W);

        widgetEntryPattern = Widgets.newText(composite);
        Widgets.layout(widgetEntryPattern,0,1,TableLayoutData.WE);
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

        button = Widgets.newButton(composite,IMAGE_CLEAR);
        Widgets.layout(button,0,2,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            widgetEntryPattern.setText("");
            setEntryPattern("");
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        button.setToolTipText("Clear filter entry pattern.");

        button = Widgets.newCheckbox(composite,"newest entries only");
        button.setSelection(newestEntriesOnlyFlag);
        Widgets.layout(button,0,3,TableLayoutData.W);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button  widget = (Button)selectionEvent.widget;
            newestEntriesOnlyFlag = widget.getSelection();
            updateEntryListThread.triggerUpdate(entryPattern);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        button.setToolTipText("When this checkbox is enabled, only show newest entry instances and hide all older entry instances.");

  //      control = Widgets.newSpacer(composite);
  //      Widgets.layout(control,0,4,TableLayoutData.WE,0,0,30,0);

        widgetRestoreEntriesButton = Widgets.newButton(composite,"Restore");
        widgetRestoreEntriesButton.setEnabled(false);
        Widgets.layout(widgetRestoreEntriesButton,0,4,TableLayoutData.DEFAULT,0,0,0,0,60,SWT.DEFAULT);
        widgetRestoreEntriesButton.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            restoreEntries(getTaggedEntries(),
                           widgetRestoreTo.getSelection() ? widgetRestoreToDirectory.getText() : "",
                           widgetOverwriteEntries.getSelection()
                          );
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        widgetRestoreEntriesButton.setToolTipText("Start restoring selected entries.");

        button = Widgets.newButton(composite,IMAGE_MARK_ALL);
        Widgets.layout(button,0,5,TableLayoutData.E);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            setTaggedEntries(true);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        button.setToolTipText("Mark all entries in list.");

        button = Widgets.newButton(composite,IMAGE_UNMARK_ALL);
        Widgets.layout(button,0,7,TableLayoutData.E);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
            setTaggedEntries(false);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        button.setToolTipText("Unmark all entries in list.");

        widgetEntryMaxCount = Widgets.newOptionMenu(composite);
        widgetEntryMaxCount.setItems(new String[]{"10","50","100","500","1000"});
        widgetEntryMaxCount.setText("100");
        Widgets.layout(widgetEntryMaxCount,0,8,TableLayoutData.W);
        widgetEntryMaxCount.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Combo widget = (Combo)selectionEvent.widget;
            updateEntryListThread.triggerUpdate(Integer.parseInt(widget.getText()));
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        widgetEntryMaxCount.setToolTipText("Max. number of entries in list.");
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
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button  widget      = (Button)selectionEvent.widget;
          boolean checkedFlag = widget.getSelection();
          widgetRestoreTo.setSelection(checkedFlag);
          widgetRestoreToDirectory.setEnabled(checkedFlag);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetRestoreTo.setToolTipText("Enable this checkbox and select a directory to restore entries to different directory.");

      widgetRestoreToDirectory = Widgets.newText(group);
      widgetRestoreToDirectory.setEnabled(false);
      Widgets.layout(widgetRestoreToDirectory,0,1,TableLayoutData.WE);

      widgetRestoreToSelectButton = Widgets.newButton(group,IMAGE_DIRECTORY);
      Widgets.layout(widgetRestoreToSelectButton,0,2,TableLayoutData.DEFAULT);
      widgetRestoreToSelectButton.addSelectionListener(new SelectionListener()
      {
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
            widgetRestoreToDirectory.setEnabled(true);
            widgetRestoreToDirectory.setText(pathName);
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
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

  /** set archive name in tree widget
   * @param newArchivePathName new archive path name
   */
  private void setArchivePath(String newArchivePathName)
  {
/*
    TreeItem treeItem;

    if (!archivePathName.equals(newArchivePathName))
    {
      archivePathName      = newArchivePathName;
      archivePathNameParts = new ArchiveNameParts(newArchivePathName);

      switch (archivePathNameParts.type)
      {
        case FILESYSTEM:
        case SFTP:
        case DVD:
        case DEVICE:
          widgetStorageList.removeAll();

          treeItem = Widgets.addTreeItem(widgetStorageList,
                                         new StorageData(archivePathName,
                                                         EntryTypes.DIRECTORY,
                                                         archivePathName
                                                        ),
                                         true
                                        );
          treeItem.setText(archivePathName);
          treeItem.setImage(IMAGE_DIRECTORY);
          treeItem.setGrayed(true);
          break;
        case FTP:
          Dialogs.error(shell,"Sorry, FTP protocol does not support required operations to list archive content.");
          break;
        case SCP:
          if (Dialogs.confirm(shell,"SCP protocol does not support required operations to list archive content.\n\nTry to open archive with SFTP protocol?"))
          {
            archivePathNameParts = new ArchiveNameParts(StorageTypes.SFTP,
                                                        archivePathNameParts.loginName,
                                                        archivePathNameParts.loginPassword,
                                                        archivePathNameParts.hostName,
                                                        archivePathNameParts.hostPort,
                                                        archivePathNameParts.deviceName,
                                                        archivePathNameParts.fileName
                                                       );
            String pathName = archivePathNameParts.getArchiveName();

            widgetStorageList.removeAll();

            treeItem = Widgets.addTreeItem(widgetStorageList,
                                           new StorageData(pathName,
                                                           EntryTypes.DIRECTORY,
                                                           pathName
                                                          ),
                                           true
                                          );
            treeItem.setText(pathName);
            treeItem.setImage(IMAGE_DIRECTORY);
            treeItem.setGrayed(true);
          }
          break;
        default:
          break;
      }
    }
*/
  }

  /** update archive path list
   */
  private void updateArchivePathList()
  {
    if (widgetPath!=null && !widgetPath.isDisposed())
    {
      // get job list
      ArrayList<String> result = new ArrayList<String>();
      if (BARServer.executeCommand("JOB_LIST",result) != Errors.NONE) return;

      // get archive path names from jobs
      HashSet <String> pathNames = new HashSet<String>();
      for (String line : result)
      {
        Object data[] = new Object[11];
        /* format:
           <id>
           <name>
           <state>
           <type>
           <archivePartSize>
           <compressAlgorithm>
           <cryptAlgorithm>
           <cryptType>
           <cryptPasswordMode>
           <lastExecutedDateTime>
           <estimatedRestTime>
        */
        if (StringParser.parse(line,"%d %S %S %s %ld %S %S %S %S %ld %ld",data,StringParser.QUOTE_CHARS))
        {
          // get data
          int id = (Integer)data[0];

          // get archive name
          String archiveName = BARServer.getStringOption(id,"archive-name");

          // parse archive name
          ArchiveNameParts archiveNameParts = new ArchiveNameParts(archiveName);

          if (   (archiveNameParts.type == StorageTypes.FILESYSTEM)
              || (archiveNameParts.type == StorageTypes.SCP)
              || (archiveNameParts.type == StorageTypes.SFTP)
              || (archiveNameParts.type == StorageTypes.DVD)
              || (archiveNameParts.type == StorageTypes.DEVICE)
             )
          {
            // get and save path
            pathNames.add(archiveNameParts.getArchivePathName());
          }
        }

        // update path list
        widgetPath.removeAll();
        widgetPath.add("/");
        for (String path : pathNames)
        {
          widgetPath.add(path);
        }
      }
    }
  }

  /** set/clear tagging of all storage entries
   * @param true for set tagged, false for clear tagged
   */
  private void setTaggedStorage(boolean flag)
  {
    for (TableItem tableItem : widgetStorageList.getItems())
    {
      tableItem.setChecked(flag);
    }

    boolean storageTagged = checkStorageTagged();
    widgetRestoreArchivesButton.setEnabled(storageTagged);
    menuItemRestoreArchivesButton.setEnabled(storageTagged);
  }

  /** get tagged storage names
   * @return tagged storage name hash set
   */
  private LinkedHashSet<String> getTaggedStorageNameHashSet()
  {
    LinkedHashSet<String> storageNamesHashSet = new LinkedHashSet<String>();

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

  /** get selected storage
   * @return selected storage hash set
   */
  private LinkedHashSet<StorageData> getSelectedStorageHashSet()
  {
    LinkedHashSet<StorageData> storageHashSet = new LinkedHashSet<StorageData>();

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
                                      storageData.state.toString()
                                     )
           )
        {
          Widgets.insertTableEntry(widgetStorageList,
                                   findStorageListIndex(storageData),
                                   (Object)storageData,
                                   storageData.name,
                                   Units.formatByteSize(storageData.size),
                                   simpleDateFormat.format(new Date(storageData.datetime*1000)),
                                   storageData.state.toString()
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
        storagePattern = string.trim();
        updateStorageListThread.triggerUpdate(storagePattern);
      }
    }
    else
    {
      if (storagePattern != null)
      {
        storagePattern = null;
        updateStorageListThread.triggerUpdate(storagePattern);
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
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
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
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,null);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        Dialogs.close(dialog,widgetStorageName.getText());
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
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
        Dialogs.error(shell,"Cannot add database index for storage file '"+storageName+"' (error: "+result[0]+")");
      }
    }
  }

  /** remove storage from database index
   */
  private void removeStorageIndex()
  {
    try
    {
      HashSet<StorageData> selectedStorageHashSet = getSelectedStorageHashSet();
      if (!selectedStorageHashSet.isEmpty())
      {
        if (Dialogs.confirm(shell,"Really remove index for "+selectedStorageHashSet.size()+" entries?"))
        {
          for (StorageData storageData : selectedStorageHashSet)
          {
            String[] result = new String[1];
            int errorCode = BARServer.executeCommand("INDEX_STORAGE_REMOVE "+StringUtils.escape(storageData.name),result);
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
              Dialogs.error(shell,"Cannot remove database index for storage file '"+storageData.name+"' (error: "+result[0]+")");
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

  /** refresh storage from database index
   */
  private void refreshStorageIndex()
  {
    try
    {
      HashSet<StorageData> selectedStorageHashSet = getSelectedStorageHashSet();
      if (!selectedStorageHashSet.isEmpty())
      {
        if (Dialogs.confirm(shell,"Really refresh index for "+selectedStorageHashSet.size()+" entries?"))
        {
          for (StorageData storageData : selectedStorageHashSet)
          {
            String[] result = new String[1];
            int errorCode = BARServer.executeCommand("INDEX_STORAGE_REFRESH "+
                                                     "* "+
                                                     StringUtils.escape(storageData.name),
                                                     result
                                                    );
            if (errorCode == Errors.NONE)
            {
              storageData.state = IndexStates.INDEX_UPDATE_REQUESTED;
            }
            else
            {
              Dialogs.error(shell,"Cannot refresh database index for storage file '"+storageData.name+"' (error: "+result[0]+")");
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
                                                 "*",
                                                 result
                                                );
        if (errorCode == Errors.NONE)
        {
          updateStorageListThread.triggerUpdate();
        }
        else
        {
          Dialogs.error(shell,"Cannot refresh database index with error state' (error: "+result[0]+")");
        }
      }
    }
    catch (CommunicationError error)
    {
      Dialogs.error(shell,"Communication error while refreshing database index (error: "+error.toString()+")");
    }
  }

  /** restore archives
   * @param storageNamesHashSet storage name hash set
   * @param directory destination directory or ""
   * @param overwriteEntries true to overwrite existing entries
   */
  private void restoreArchives(LinkedHashSet<String> storageNamesHashSet, String directory, boolean overwriteEntries)
  {
    shell.setCursor(waitCursor);

    final BusyDialog busyDialog = new BusyDialog(shell,"Restore archives",500,100,null,BusyDialog.TEXT0|BusyDialog.TEXT1|BusyDialog.PROGRESS_BAR1);

    new BackgroundTask(busyDialog,new Object[]{storageNamesHashSet,directory,overwriteEntries})
    {
      public void run(final BusyDialog busyDialog, Object userData)
      {
        final LinkedHashSet<String> storageNamesHashSet = (LinkedHashSet<String>)((Object[])userData)[0];
        final String                directory           = (String               )((Object[])userData)[1];
        final boolean               overwriteEntries    = (Boolean              )((Object[])userData)[2];

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

            ArrayList<String> result = new ArrayList<String>();
            String commandString = "RESTORE "+
                                   StringUtils.escape(storageName)+" "+
                                   StringUtils.escape(directory)+" "+
                                   (overwriteEntries?"1":"0")
                                   ;
  //Dprintf.dprintf("command=%s",commandString);
            Command command = BARServer.runCommand(commandString);

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
   * @param true for set tagged, false for clear tagged
   */
  private void setTaggedEntries(boolean tagged)
  {
    for (TableItem tableItem : widgetEntryList.getItems())
    {
      tableItem.setChecked(tagged);
    }

    boolean entriesTagged = checkEntriesTagged();
    widgetRestoreEntriesButton.setEnabled(entriesTagged);
    menuItemRestoreEntriesButton.setEnabled(entriesTagged);
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

    // enable/disable restore buttons
    boolean entriesTagged = checkEntriesTagged();
    widgetRestoreEntriesButton.setEnabled(entriesTagged);
    menuItemRestoreEntriesButton.setEnabled(entriesTagged);
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
        final EntryData[] entryData_       = (EntryData[])((Object[])userData)[0];
        final String      directory        = (String     )((Object[])userData)[1];
        final boolean     overwriteEntries = (Boolean    )((Object[])userData)[2];

        int errorCode;

        // restore entries
        try
        {
          for (final EntryData entryData : entryData_)
          {
            if (!directory.equals(""))
            {
              busyDialog.updateText("'"+entryData.name+"' into '"+directory+"'");
            }
            else
            {
              busyDialog.updateText("'"+entryData.name+"'");
            }

            ArrayList<String> result = new ArrayList<String>();
            String commandString = "RESTORE "+
                                   StringUtils.escape(entryData.storageName)+" "+
                                   StringUtils.escape(directory)+" "+
                                   (overwriteEntries?"1":"0")+" "+
                                   StringUtils.escape(entryData.name)
                                   ;
  //Dprintf.dprintf("command=%s",commandString);
            Command command = BARServer.runCommand(commandString);

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

  /** update all data
   */
  private void update()
  {
    updateArchivePathList();
  }
}

/* end of file */
