/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: status tab
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

// graphics
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MenuEvent;
import org.eclipse.swt.events.MenuListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.events.ShellAdapter;
import org.eclipse.swt.events.ShellEvent;
import org.eclipse.swt.graphics.Color;
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
import org.eclipse.swt.widgets.Widget;

/****************************** Classes ********************************/

/** job data
 */
class JobData
{
  /** job states
   */
  static enum States
  {
    NONE,
    WAITING,
    RUNNING,
    NO_STORAGE,
    DRY_RUNNING,
    REQUEST_FTP_PASSWORD,
    REQUEST_SSH_PASSWORD,
    REQUEST_WEBDAV_PASSWORD,
    REQUEST_CRYPT_PASSWORD,
    REQUEST_VOLUME,
    DONE,
    ERROR,
    ABORTED,
    DISCONNECTED;

    /** get (translated) state text
     * @return state text
     */
    public String getText()
    {
      switch (this)
      {
        case NONE:                    return "";
        case WAITING:                 return BARControl.tr("waiting");
        case RUNNING:                 return BARControl.tr("running");
        case NO_STORAGE:              return BARControl.tr("running (no storage)");
        case DRY_RUNNING:             return BARControl.tr("dry run");
        case REQUEST_FTP_PASSWORD:    return BARControl.tr("request FTP password");
        case REQUEST_SSH_PASSWORD:    return BARControl.tr("request SSH password");
        case REQUEST_WEBDAV_PASSWORD: return BARControl.tr("request webDAV password");
        case REQUEST_CRYPT_PASSWORD:  return BARControl.tr("request crypt password");
        case REQUEST_VOLUME:          return BARControl.tr("request volume");
        case DONE:                    return BARControl.tr("done");
        case ERROR:                   return BARControl.tr("ERROR");
        case ABORTED:                 return BARControl.tr("aborted");
        case DISCONNECTED:            return BARControl.tr("disconnected");
      }

      return "";
    }
  };

  /** slave states
   */
  static enum SlaveStates
  {
    OFFLINE,
    ONLINE,
    PAIRED;
  };

  /** get state text
   * @return state text
   */
  public static String formatStateText(States state, String slaveHostName, SlaveStates slaveState)
  {
    StringBuilder buffer = new StringBuilder();

    buffer.append(state.getText());

    if (!slaveHostName.isEmpty() && (slaveState != SlaveStates.PAIRED))
    {
      if (buffer.length() > 0)
      {
        buffer.append(" ");
        switch (slaveState)
        {
          case OFFLINE: buffer.append(BARControl.tr("(offline)"));      break;
          case ONLINE:  buffer.append(BARControl.tr("(wait pairing)")); break;
        }
      }
      else
      {
        switch (slaveState)
        {
          case OFFLINE: buffer.append(BARControl.tr("offline"));      break;
          case ONLINE:  buffer.append(BARControl.tr("wait pairing")); break;
        }
      }
    }

    if (buffer.length() == 0)
    {
      buffer.append("-");
    }

    return buffer.toString();
  }

  String       uuid;
  String       master;
  String       name;
  States       state;
  String       slaveHostName;
  SlaveStates  slaveState;
  ArchiveTypes archiveType;
  long         archivePartSize;
  String       deltaCompressAlgorithm;
  String       byteCompressAlgorithm;
  String       cryptAlgorithm;
  String       cryptType;
  String       cryptPasswordMode;
  long         lastExecutedDateTime;
  long         estimatedRestTime;

  // date/time format
  private final SimpleDateFormat SIMPLE_DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  /** create job data
   * @param uuid job UUID
   * @[aram master master or ''
   * @param name name
   * @param state job state
   * @param slaveHostName slave host name
   * @param slaveState slave state
   * @param archiveType archive type
   * @param archivePartSize archive part size
   * @param deltaCompressAlgorithm delta compress algorithm
   * @param byteCompressAlgorithm byte compress algorithm
   * @param cryptAlgorithm crypt algorithm
   * @param cryptType crypt type
   * @param cryptPasswordMode crypt password mode
   * @param lastExecutedDateTime last executed date/time [s]
   * @param estimatedRestTime estimated rest time [s]
   */
  JobData(String       uuid,
          String       master,
          String       name,
          States       state,
          String       slaveHostName,
          SlaveStates  slaveState,
          ArchiveTypes archiveType,
          long         archivePartSize,
          String       deltaCompressAlgorithm,
          String       byteCompressAlgorithm,
          String       cryptAlgorithm,
          String       cryptType,
          String       cryptPasswordMode,
          long         lastExecutedDateTime,
          long         estimatedRestTime
         )
  {
    this.uuid                   = uuid;
    this.master                 = master;
    this.name                   = name;
    this.state                  = state;
    this.slaveHostName          = slaveHostName;
    this.slaveState             = slaveState;
    this.archiveType            = archiveType;
    this.archivePartSize        = archivePartSize;
    this.deltaCompressAlgorithm = deltaCompressAlgorithm;
    this.byteCompressAlgorithm  = byteCompressAlgorithm;
    this.cryptAlgorithm         = cryptAlgorithm;
    this.cryptType              = cryptType;
    this.cryptPasswordMode      = cryptPasswordMode;
    this.lastExecutedDateTime   = lastExecutedDateTime;
    this.estimatedRestTime      = estimatedRestTime;
  }

  /** format job compress algorithms
   * @return compress algorithm string
   */
  public String formatCompressAlgorithm()
  {
    boolean deltaCompressIsNone = deltaCompressAlgorithm.equals("none");
    boolean byteCompressIsNone  = byteCompressAlgorithm.equals("none");

    if (!deltaCompressIsNone || !byteCompressIsNone)
    {
      StringBuilder buffer = new StringBuilder();

      if (!deltaCompressIsNone)
      {
        buffer.append(deltaCompressAlgorithm);
      }
      if (!byteCompressIsNone)
      {
        if (buffer.length() > 0) buffer.append('+');
        buffer.append(byteCompressAlgorithm);
      }

      return buffer.toString();
    }
    else
    {
      return "";
    }
  }

  /** format job crypt algorithm (including "*" for asymmetric)
   * @return crypt algorithm string
   */
  public String formatCryptAlgorithm()
  {
    boolean cryptIsNone = cryptAlgorithm.equals("none");

    return !cryptIsNone ? cryptAlgorithm+(cryptType.equals("ASYMMETRIC") ? "*" : "") : "";
  }

  /** format last executed date/time
   * @return date/time string
   */
  public String formatLastExecutedDateTime()
  {
    if (lastExecutedDateTime > 0)
    {
      return SIMPLE_DATE_FORMAT.format(new Date(lastExecutedDateTime*1000));
    }
    else
    {
      return "-";
    }
  }

  /** format estimated rest time
   * @return estimated rest time string
   */
  public String formatEstimatedRestTime()
  {
    long   estimatedRestDays    = estimatedRestTime/(24*60*60);
    long   estimatedRestHours   = estimatedRestTime%(24*60*60)/(60*60);
    long   estimatedRestMinutes = estimatedRestTime%(60*60   )/(60   );
    long   estimatedRestSeconds = estimatedRestTime%(60      );

    return String.format("%2d days %02d:%02d:%02d",
                         estimatedRestDays,
                         estimatedRestHours,
                         estimatedRestMinutes,
                         estimatedRestSeconds
                        );
  }

  /** convert data to string
   * @return string
   */
  @Override
  public String toString()
  {
    return "Job {"+uuid+", '"+master+"', '"+name+"', "+state+", '"+slaveHostName+"', "+archiveType+"}";
  }
};

/** job data comparator
 */
class JobDataComparator implements Comparator<JobData>
{
  // sort modes
  enum SortModes
  {
    NAME,
    STATE,
    TYPE,
    PARTSIZE,
    COMPRESS,
    CRYPT,
    LAST_EXECUTED_DATETIME,
    ESTIMATED_TIME
  };

  private SortModes sortMode;

  /** create job data comparator
   * @param table job table
   * @param sortColumn sorting column
   */
  JobDataComparator(Table table, TableColumn sortColumn)
  {
    if      (table.getColumn(0) == sortColumn) sortMode = SortModes.NAME;
    else if (table.getColumn(1) == sortColumn) sortMode = SortModes.STATE;
    else if (table.getColumn(2) == sortColumn) sortMode = SortModes.TYPE;
    else if (table.getColumn(3) == sortColumn) sortMode = SortModes.PARTSIZE;
    else if (table.getColumn(4) == sortColumn) sortMode = SortModes.COMPRESS;
    else if (table.getColumn(5) == sortColumn) sortMode = SortModes.CRYPT;
    else if (table.getColumn(6) == sortColumn) sortMode = SortModes.LAST_EXECUTED_DATETIME;
    else if (table.getColumn(7) == sortColumn) sortMode = SortModes.ESTIMATED_TIME;
    else                                       sortMode = SortModes.NAME;
  }

  /** create job data comparator
   * @param table job table
   */
  JobDataComparator(Table table)
  {
    TableColumn sortColumn = table.getSortColumn();

    if      (table.getColumn(0) == sortColumn) sortMode = SortModes.NAME;
    else if (table.getColumn(1) == sortColumn) sortMode = SortModes.STATE;
    else if (table.getColumn(2) == sortColumn) sortMode = SortModes.TYPE;
    else if (table.getColumn(3) == sortColumn) sortMode = SortModes.PARTSIZE;
    else if (table.getColumn(4) == sortColumn) sortMode = SortModes.COMPRESS;
    else if (table.getColumn(5) == sortColumn) sortMode = SortModes.CRYPT;
    else if (table.getColumn(6) == sortColumn) sortMode = SortModes.LAST_EXECUTED_DATETIME;
    else if (table.getColumn(7) == sortColumn) sortMode = SortModes.ESTIMATED_TIME;
    else                                       sortMode = SortModes.NAME;
  }

  /** compare job data
   * @param jobData1, jobData2 file tree data to compare
   * @return -1 iff jobData1 < jobData2,
              0 iff jobData1 = jobData2,
              1 iff jobData1 > jobData2
   */
  public int compare(JobData jobData1, JobData jobData2)
  {
    switch (sortMode)
    {
      case NAME:
        return jobData1.name.compareTo(jobData2.name);
      case STATE:
        return jobData1.state.compareTo(jobData2.state);
      case TYPE:
        return jobData1.archiveType.compareTo(jobData2.archiveType);
      case PARTSIZE:
        if      (jobData1.archivePartSize < jobData2.archivePartSize) return -1;
        else if (jobData1.archivePartSize > jobData2.archivePartSize) return  1;
        else                                                          return  0;
      case COMPRESS:
        int result = jobData1.deltaCompressAlgorithm.compareTo(jobData2.deltaCompressAlgorithm);
        if (result == 0) result = jobData1.byteCompressAlgorithm.compareTo(jobData2.byteCompressAlgorithm);
      case CRYPT:
        String crypt1 = jobData1.cryptAlgorithm+(jobData1.cryptType.equals("ASYMMETRIC") ?"*" : "");
        String crypt2 = jobData2.cryptAlgorithm+(jobData2.cryptType.equals("ASYMMETRIC") ?"*" : "");

        return crypt1.compareTo(crypt2);
      case LAST_EXECUTED_DATETIME:
        if      (jobData1.lastExecutedDateTime < jobData2.lastExecutedDateTime) return -1;
        else if (jobData1.lastExecutedDateTime > jobData2.lastExecutedDateTime) return  1;
        else                                                                    return  0;
      case ESTIMATED_TIME:
        if      (jobData1.estimatedRestTime < jobData2.estimatedRestTime) return -1;
        else if (jobData1.estimatedRestTime > jobData2.estimatedRestTime) return  1;
        else                                                              return  0;
      default:
        return 0;
    }
  }
}

/** update job state listener
 */
abstract class UpdateJobStateListener
{
  private Widget widget;

  /** create update job state listener
   * @param widget widget
   */
  UpdateJobStateListener(Widget widget)
  {
    this.widget = widget;
  }

  /** call to signal job state modified
   * @param jobData job data
   */
  public void modified(JobData jobData)
  {
    assert(jobData != null);

    if ((widget == null) || !widget.isDisposed())
    {
      handle(widget,jobData);
    }
  }

  /** handle update job state
   * @param widget widget
   * @param jobData job data
   */
  abstract void handle(Widget wddget, JobData jobData);
}

/** tab status
 */
public class TabStatus
{
  /** status update thread
   */
  class TabStatusUpdateThread extends Thread
  {
    private TabStatus tabStatus;

    /** initialize status update thread
     * @param tabStatus tab status
     */
    TabStatusUpdateThread(TabStatus tabStatus)
    {
      this.tabStatus = tabStatus;
      setDaemon(true);
      setName("BARControl Update Status");
    }

    /** run status update thread
     */
    public void run()
    {
      try
      {
        int i = 0;
        for (;;)
        {
          // sleep a short time
          try { Thread.sleep(1000); } catch (InterruptedException exception) { /* ignored */ };

          // update
          try
          {
            // update status, running job info
            tabStatus.updateStatus();
            tabStatus.updateJobInfo();

            // update job list
            if (i == 0) tabStatus.updateJobList();
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

          i = (i+1)%10;
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

  // colors
  private final Color            COLOR_RUNNING;
  private final Color            COLOR_REQUEST;
  private final Color            COLOR_ERROR;
  private final Color            COLOR_ABORTED;

  // date/time format
  private final SimpleDateFormat SIMPLE_DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  // global variable references
  private Shell                           shell;
  private Display                         display;
  private TabStatusUpdateThread           tabStatusUpdateThread;
  private TabJobs                         tabJobs;

  // menu/widgets
  private Menu                            menuTriggerJob;

  public  Composite                       widgetTab;
  private Table                           widgetJobTable;
  private Shell                           widgetJobTableToolTip = null;
  private Menu                            widgetJobTableHeaderMenu;
  private Menu                            widgetJobTableBodyMenu;
  private Shell                           widgetMessageToolTip = null;
  private Separator                       widgetSelectedJob;
  public  Button                          widgetButtonStart;
  public  Button                          widgetButtonAbort;
  public  Button                          widgetButtonPause;
  public  Button                          widgetButtonSuspendContinue;
  private Button                          widgetButtonVolume;
  public  Button                          widgetButtonQuit;

  // BAR variables
  private WidgetVariable                  doneCount               = new WidgetVariable<Long>(0L);
  private WidgetVariable                  doneSize                = new WidgetVariable<Long>(0L);
  private WidgetVariable                  storageTotalSize        = new WidgetVariable<Long>(0L);
  private WidgetVariable                  skippedEntryCount       = new WidgetVariable<Long>(0L);
  private WidgetVariable                  skippedEntrySize        = new WidgetVariable<Long>(0L);
  private WidgetVariable                  errorEntryCount         = new WidgetVariable<Long>(0L);
  private WidgetVariable                  errorEntrySize          = new WidgetVariable<Long>(0L);
  private WidgetVariable                  totalEntryCount         = new WidgetVariable<Long>(0L);
  private WidgetVariable                  totalEntrySize          = new WidgetVariable<Long>(0L);

  private WidgetVariable                  filesPerSecond          = new WidgetVariable<Double>(0.0);
  private WidgetVariable                  bytesPerSecond          = new WidgetVariable<Double>(0.0);
  private WidgetVariable                  storageBytesPerSecond   = new WidgetVariable<Double>(0.0);
  private WidgetVariable                  compressionRatio        = new WidgetVariable<Double>(0.0);

  private WidgetVariable                  fileName                = new WidgetVariable<String>("");
  private WidgetVariable                  fileProgress            = new WidgetVariable<Double>(0.0);
  private WidgetVariable                  storageName             = new WidgetVariable<String>("");
  private WidgetVariable                  storageProgress         = new WidgetVariable<Double>(0.0);
  private WidgetVariable                  volumeNumber            = new WidgetVariable<Long>(0L);
  private WidgetVariable                  volumeProgress          = new WidgetVariable<Double>(0.0);
  private WidgetVariable                  totalEntriesProgress    = new WidgetVariable<Double>(0.0);
  private WidgetVariable                  totalBytesProgress      = new WidgetVariable<Double>(0.0);
  private WidgetVariable                  collectTotalSumDone     = new WidgetVariable<Boolean>(false);
  private WidgetVariable                  requestedVolumeNumber   = new WidgetVariable<Integer>(0);
  private WidgetVariable                  message                 = new WidgetVariable<String>("");

  // variables
  private HashMap<String,JobData>         jobDataMap              = new HashMap<String,JobData>();
  private HashSet<UpdateJobStateListener> updateJobStateListeners = new HashSet<UpdateJobStateListener>();
  private JobData                         selectedJobData         = null;
  private BARServer.States                serverState             = BARServer.States.RUNNING;
  private int                             updateStatusFailCount   = 0;

  /** create status tab
   * @param parentTabFolder parent tab folder
   * @param accelerator keyboard shortcut to select tab
   */
  TabStatus(TabFolder parentTabFolder, int accelerator)
  {
    TableColumn tableColumn;
    Menu        menu,subMenu;
    MenuItem    menuItem;
    Group       group;
    Composite   composite,subComposite;
    Button      button;
    Control     control;
    Label       label;
    ProgressBar progressBar;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    // get colors
    COLOR_RUNNING = new Color(null,0xA0,0xFF,0xA0);
    COLOR_REQUEST = new Color(null,0xFF,0xFF,0xA0);
    COLOR_ERROR   = new Color(null,0xFF,0xA0,0xA0);
    COLOR_ABORTED = new Color(null,0xC0,0xC0,0xC0);

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Status")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{1.0,0.0,0.0,0.0},1.0,2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);
    widgetTab.addListener(SWT.Show,new Listener()
    {
      public void handleEvent(Event event)
      {
        // make sure data is updated when jobs tab is shown
        update();
      }
    });

    // list with jobs
    final String COLUMN_NAMES[] = new String[]{"Name","State","Slave","Type","Part size","Compress","Crypt","Last executed","Estimated time"};
    widgetJobTable = Widgets.newTable(widgetTab,SWT.NONE);
    widgetJobTable.setToolTipText(BARControl.tr("List with job entries.\nClick to select job, right-click to open context menu."));
    widgetJobTable.setLayout(new TableLayout(null,null));
    Widgets.layout(widgetJobTable,0,0,TableLayoutData.NSWE);
    widgetJobTable.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        JobData jobData = (JobData)selectionEvent.item.getData();
        Widgets.notify(shell,BARControl.USER_EVENT_NEW_JOB,jobData);
      }
    });
    SelectionListener jobListColumnSelectionListener = new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        TableColumn       tableColumn       = (TableColumn)selectionEvent.widget;
        JobDataComparator jobDataComparator = new JobDataComparator(widgetJobTable,tableColumn);

        Widgets.sortTableColumn(widgetJobTable,tableColumn,jobDataComparator);
      }
    };
    Listener jobListColumnMoveListener = new Listener()
    {
      public void handleEvent(Event event)
      {
        int[]    columnOrder = widgetJobTable.getColumnOrder();
        String[] names       = new String[columnOrder.length];
        for (int i = 0; i < columnOrder.length; i++)
        {
          names[i] = COLUMN_NAMES[columnOrder[i]];
        }
        Settings.jobListColumnOrder = new SettingUtils.SimpleStringArray(names);
      }
    };
    Listener jobListColumnResizeListener = new Listener()
    {
      public void handleEvent(Event event)
      {
        Settings.jobListColumns = new Settings.ColumnSizes(Widgets.getTableColumnWidth(widgetJobTable));
      }
    };
    tableColumn = Widgets.addTableColumn(widgetJobTable,0,BARControl.tr("Name"),          SWT.LEFT, 110,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for name."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,1,BARControl.tr("State"),         SWT.LEFT,  90,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for state."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,2,BARControl.tr("Slave"),         SWT.LEFT, 130,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for host."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,3,BARControl.tr("Type"),          SWT.LEFT,  90,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for type."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,4,BARControl.tr("Part size"),     SWT.RIGHT, 80,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for part size."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,5,BARControl.tr("Compress"),      SWT.LEFT,  80,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for used compress algorithm."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,6,BARControl.tr("Crypt"),         SWT.LEFT, 100,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for used encryption algorithm."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,7,BARControl.tr("Last executed"), SWT.LEFT, 150,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for last date/time job was executed."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,8,BARControl.tr("Estimated time"),SWT.LEFT, 120,false);
    tableColumn.setToolTipText(BARControl.tr("Click to sort for estimated rest time to execute job."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    Widgets.setTableColumnWidth(widgetJobTable,Settings.jobListColumns.width);
    widgetJobTable.setColumnOrder(Settings.jobListColumnOrder.getMap(COLUMN_NAMES));
    for (TableColumn tableColumn_ : widgetJobTable.getColumns())
    {
      tableColumn_.setMoveable(true);
      tableColumn_.addListener(SWT.Move,jobListColumnMoveListener);
      tableColumn_.addListener(SWT.Resize,jobListColumnResizeListener);
    }

    widgetJobTableHeaderMenu = Widgets.newPopupMenu(shell);
    {
      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("Name"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,0,widget.getSelection());
        }
      });

      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("State"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,1,widget.getSelection());
        }
      });

      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("Slave"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,2,widget.getSelection());
        }
      });

      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("Type"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,3,widget.getSelection());
        }
      });

      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("Part size"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,4,widget.getSelection());
        }
      });

      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("Compress"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,5,widget.getSelection());
        }
      });

      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("Crypt"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,6,widget.getSelection());
        }
      });

      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("Last executed"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,7,widget.getSelection());
        }
      });

      menuItem = Widgets.addMenuItemCheckbox(widgetJobTableHeaderMenu,BARControl.tr("Estimated time"));
      menuItem.setSelection(true);
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
          Widgets.showTableColumn(widgetJobTable,8,widget.getSelection());
        }
      });

      Widgets.addMenuItemSeparator(widgetJobTableHeaderMenu);

      menuItem = Widgets.addMenuItem(widgetJobTableHeaderMenu,BARControl.tr("Set optimal column width"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Widgets.adjustTableColumnWidth(widgetJobTable);
        }
      });
    }

    widgetJobTableBodyMenu = Widgets.newPopupMenu(shell);
    {
      subMenu = Widgets.addMenu(widgetJobTableBodyMenu,BARControl.tr("Start")+"\u2026",BARServer.isMaster());
      {
        menuItem = Widgets.addMenuItem(subMenu,"\u2026",BARServer.isMaster());
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
              jobStart();
            }
          }
        });
        addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
        {
          @Override
          public void handle(Widget widget, JobData jobData)
          {
            MenuItem menuItem = (MenuItem)widget;
            menuItem.setEnabled(   (jobData.state != JobData.States.RUNNING    )
                                && (jobData.state != JobData.States.NO_STORAGE )
                                && (jobData.state != JobData.States.DRY_RUNNING)
                                && (jobData.state != JobData.States.WAITING    )
                               );
          }
        });

        Widgets.addMenuItemSeparator(subMenu);

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("normal"),BARServer.isMaster());
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
              jobStart(ArchiveTypes.NORMAL,false,false);
            }
          }
        });
        addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
        {
          @Override
          public void handle(Widget widget, JobData jobData)
          {
            MenuItem menuItem = (MenuItem)widget;
            menuItem.setEnabled(   (jobData.state != JobData.States.RUNNING    )
                                && (jobData.state != JobData.States.NO_STORAGE )
                                && (jobData.state != JobData.States.DRY_RUNNING)
                                && (jobData.state != JobData.States.WAITING    )
                               );
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("full"),BARServer.isMaster());
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
              jobStart(ArchiveTypes.FULL,false,false);
            }
          }
        });
        addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
        {
          @Override
          public void handle(Widget widget, JobData jobData)
          {
            MenuItem menuItem = (MenuItem)widget;
            menuItem.setEnabled(   (jobData.state != JobData.States.RUNNING    )
                                && (jobData.state != JobData.States.NO_STORAGE )
                                && (jobData.state != JobData.States.DRY_RUNNING)
                                && (jobData.state != JobData.States.WAITING    )
                               );
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("incremental"),BARServer.isMaster());
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
              jobStart(ArchiveTypes.INCREMENTAL,false,false);
            }
          }
        });
        addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
        {
          @Override
          public void handle(Widget widget, JobData jobData)
          {
            MenuItem menuItem = (MenuItem)widget;
            menuItem.setEnabled(   (jobData.state != JobData.States.RUNNING    )
                                && (jobData.state != JobData.States.NO_STORAGE )
                                && (jobData.state != JobData.States.DRY_RUNNING)
                                && (jobData.state != JobData.States.WAITING    )
                               );
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("differential"),BARServer.isMaster());
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
              jobStart(ArchiveTypes.DIFFERENTIAL,false,false);
            }
          }
        });
        addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
        {
          @Override
          public void handle(Widget widget, JobData jobData)
          {
            MenuItem menuItem = (MenuItem)widget;
            menuItem.setEnabled(   (jobData.state != JobData.States.RUNNING    )
                                && (jobData.state != JobData.States.NO_STORAGE )
                                && (jobData.state != JobData.States.DRY_RUNNING)
                                && (jobData.state != JobData.States.WAITING    )
                               );
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("no storage"),BARServer.isMaster());
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
              jobStart(ArchiveTypes.FULL,true,false);
            }
          }
        });
        addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
        {
          @Override
          public void handle(Widget widget, JobData jobData)
          {
            MenuItem menuItem = (MenuItem)widget;
            menuItem.setEnabled(   (jobData.state != JobData.States.RUNNING    )
                                && (jobData.state != JobData.States.NO_STORAGE )
                                && (jobData.state != JobData.States.DRY_RUNNING)
                                && (jobData.state != JobData.States.WAITING    )
                               );
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("dry-run"),BARServer.isMaster());
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
              jobStart(ArchiveTypes.NORMAL,false,true);
            }
          }
        });
        addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
        {
          @Override
          public void handle(Widget widget, JobData jobData)
          {
            MenuItem menuItem = (MenuItem)widget;
            menuItem.setEnabled(   (jobData.state != JobData.States.RUNNING    )
                                && (jobData.state != JobData.States.NO_STORAGE )
                                && (jobData.state != JobData.States.DRY_RUNNING)
                                && (jobData.state != JobData.States.WAITING    )
                               );
          }
        });
      }

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Abort")+"\u2026",BARServer.isMaster());
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
            jobAbort();
          }
        }
      });
      addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
      {
        @Override
        public void handle(Widget widget, JobData jobData)
        {
          MenuItem menuItem = (MenuItem)widget;
          menuItem.setEnabled(   (jobData.state == JobData.States.WAITING       )
                              || (jobData.state == JobData.States.RUNNING       )
                              || (jobData.state == JobData.States.NO_STORAGE    )
                              || (jobData.state == JobData.States.DRY_RUNNING   )
                              || (jobData.state == JobData.States.REQUEST_VOLUME)
                             );
        }
      });

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Pause"),BARServer.isMaster() && Settings.hasNormalRole());
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobPause(60*60);
        }
      });

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Continue"),BARServer.isMaster() && Settings.hasNormalRole());
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobSuspendContinue();
        }
      });

      menuTriggerJob = Widgets.addMenu(widgetJobTableBodyMenu,BARControl.tr("Trigger"),BARServer.isMaster());
      {
      }

      Widgets.addMenuItemSeparator(widgetJobTableBodyMenu,BARServer.isMaster());

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Volume"),BARServer.isMaster());
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
            volume();
          }
        }
      });
      addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
      {
        @Override
        public void handle(Widget widget, JobData jobData)
        {
          MenuItem menuItem = (MenuItem)widget;
          menuItem.setEnabled(jobData.state == JobData.States.REQUEST_VOLUME);
        }
      });

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Reset state"),BARServer.isMaster());
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
            jobReset();
          }
        }
      });

      Widgets.addMenuItemSeparator(widgetJobTableBodyMenu,BARServer.isMaster());

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("New")+"\u2026",BARServer.isMaster() && Settings.hasNormalRole());
      menuItem.addSelectionListener(new SelectionListener()
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

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Clone")+"\u2026",BARServer.isMaster() && Settings.hasNormalRole());
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
            jobClone();
          }
        }
      });

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Rename")+"\u2026",BARServer.isMaster() && Settings.hasNormalRole());
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
            jobRename();
          }
        }
      });

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Delete")+"\u2026",BARServer.isMaster() && Settings.hasNormalRole());
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
            jobDelete();
          }
        }
      });

      Widgets.addMenuItemSeparator(widgetJobTableBodyMenu,BARServer.isMaster() && Settings.hasNormalRole());

      menuItem = Widgets.addMenuItem(widgetJobTableBodyMenu,BARControl.tr("Info")+"\u2026",BARServer.isMaster());
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          TableItem tableItems[] = widgetJobTable.getSelection();
          if (tableItems.length > 0)
          {
            if (widgetJobTableToolTip != null)
            {
              widgetJobTableToolTip.dispose();
              widgetJobTableToolTip = null;
            }

            if (tableItems[0] != null)
            {
              JobData jobData = (JobData)tableItems[0].getData();
              if (jobData != null)
              {
                Point point = display.getCursorLocation();
                if (point.x > 16) point.x -= 16;
                if (point.y > 16) point.y -= 16;
                showJobToolTip(jobData,point.x,point.y);
              }
            }
          }
        }
      });
    }
    widgetJobTableBodyMenu.addMenuListener(new MenuListener()
    {
      @Override
      public void menuShown(MenuEvent menuEvent)
      {
        if (widgetJobTableToolTip != null)
        {
          widgetJobTableToolTip.dispose();
          widgetJobTableToolTip = null;
        }
      }
      @Override
      public void menuHidden(MenuEvent menuEvent)
      {
      }
    });
    widgetJobTable.addListener(SWT.MenuDetect, new Listener()
    {
      @Override
      public void handleEvent(Event event)
      {
        Table widget = (Table)event.widget;

        Point p = display.map(null, widget, new Point(event.x, event.y));
        Rectangle clientArea = widget.getClientArea();
        boolean isInHeader =    (clientArea.y <= p.y)
                             && (p.y < (clientArea.y + widget.getHeaderHeight()));
        widget.setMenu(isInHeader ? widgetJobTableHeaderMenu : widgetJobTableBodyMenu);
      }
    });
    widgetJobTable.addListener(SWT.Dispose, new Listener()
    {
      @Override
      public void handleEvent(Event event)
      {
        widgetJobTableHeaderMenu.dispose();
        widgetJobTableBodyMenu.dispose();
      }
    });

    // selected job group
    widgetSelectedJob = Widgets.newSeparator(widgetTab,BARControl.tr("Selected")+" ''",SWT.NONE);
    Widgets.layout(widgetSelectedJob,1,0,TableLayoutData.WE);

    composite = Widgets.newComposite(widgetTab);
    composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0,1.0,0.0,0.0,1.0,0.0,1.0,1.0},4));
    Widgets.layout(composite,2,0,TableLayoutData.WE);
    {
      // done files/bytes, files/s, bytes/s
      label = Widgets.newLabel(composite,BARControl.tr("Done")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,0,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,doneCount));
      label = Widgets.newLabel(composite,BARControl.tr("files"));
      Widgets.layout(label,0,2,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,0,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,doneSize));
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,0,4,TableLayoutData.W);
      label = Widgets.newLabel(composite,"/");
      Widgets.layout(label,0,5,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,0,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,doneSize)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,0,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{BARControl.tr("bytes"),
                                                                                                BARControl.tr("KBytes"),
                                                                                                BARControl.tr("MBytes"),
                                                                                                BARControl.tr("GBytes"),
                                                                                                BARControl.tr("TBytes")
                                                                                               }
                                                                            )
                    );
      Widgets.addModifyListener(new WidgetModifyListener(label,doneSize)
      {
        @Override
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,0,8,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(subComposite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(label,filesPerSecond)
        {
          @Override
          public String getString(WidgetVariable variable)
          {
            return String.format("%.1f",variable.getDouble());
          }
        });
        label = Widgets.newLabel(subComposite,BARControl.tr("files/s"));
        Widgets.layout(label,0,1,TableLayoutData.W);
      }

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,0,9,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(subComposite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(label,bytesPerSecond)
        {
          @Override
          public String getString(WidgetVariable variable)
          {
            return Units.getByteSize(variable.getDouble());
          }
        });
        label = Widgets.newLabel(subComposite,BARControl.tr("bytes/s"));
        Widgets.layout(label,0,1,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{BARControl.tr("bytes/s"),
                                                                                                  BARControl.tr("KBytes/s"),
                                                                                                  BARControl.tr("MBytes/s"),
                                                                                                  BARControl.tr("GBytes/s"),
                                                                                                  BARControl.tr("TBytes/s")
                                                                                                 }
                                                                              )
                      );
        Widgets.addModifyListener(new WidgetModifyListener(label,bytesPerSecond)
        {
          @Override
          public String getString(WidgetVariable variable)
          {
            return Units.getByteUnit(variable.getDouble())+"/s";
          }
        });
      }

      // stored files/bytes
      label = Widgets.newLabel(composite,BARControl.tr("Stored")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,1,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,storageTotalSize));
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,1,4,TableLayoutData.W);
      label = Widgets.newLabel(composite,"/");
      Widgets.layout(label,1,5,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,1,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,storageTotalSize)
      {
        @Override
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,1,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{BARControl.tr("bytes"),
                                                                                                BARControl.tr("KBytes"),
                                                                                                BARControl.tr("MBytes"),
                                                                                                BARControl.tr("GBytes"),
                                                                                                BARControl.tr("TBytes")
                                                                                               }
                                                                            )
                    );
      Widgets.addModifyListener(new WidgetModifyListener(label,storageTotalSize)
      {
        @Override
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0}));
      Widgets.layout(subComposite,1,8,TableLayoutData.WE);
      {
        label = Widgets.newLabel(subComposite,BARControl.tr("Ratio"));
        Widgets.layout(label,0,0,TableLayoutData.W);
        label = Widgets.newNumberView(subComposite);
        Widgets.layout(label,0,1,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(label,compressionRatio)
        {
          @Override
          public String getString(WidgetVariable variable)
          {
            return String.format("%.1f",variable.getDouble());
          }
        });
        label = Widgets.newLabel(subComposite,"%");
        Widgets.layout(label,0,2,TableLayoutData.W);
      }

      subComposite = Widgets.newComposite(composite);
      subComposite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(subComposite,1,9,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(subComposite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(label,storageBytesPerSecond)
        {
          @Override
          public String getString(WidgetVariable variable)
          {
            return Units.getByteSize(variable.getDouble());
          }
        });
        label = Widgets.newLabel(subComposite,BARControl.tr("bytes/s"));
        Widgets.layout(label,0,1,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{BARControl.tr("bytes/s"),
                                                                                                  BARControl.tr("KBytes/s"),
                                                                                                  BARControl.tr("MBytes/s"),
                                                                                                  BARControl.tr("GBytes/s"),
                                                                                                  BARControl.tr("TBytes/s")
                                                                                                 }
                                                                              )
                      );
        Widgets.addModifyListener(new WidgetModifyListener(label,storageBytesPerSecond)
        {
          @Override
          public String getString(WidgetVariable variable)
          {
            return Units.getByteUnit(variable.getDouble())+"/s";
          }
        });
      }

      // skipped files/bytes
      label = Widgets.newLabel(composite,BARControl.tr("Skipped")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,2,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,skippedEntryCount));
      label = Widgets.newLabel(composite,BARControl.tr("files"));
      Widgets.layout(label,2,2,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,2,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,skippedEntrySize));
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,2,4,TableLayoutData.W);
      label = Widgets.newLabel(composite,"/");
      Widgets.layout(label,2,5,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,2,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,skippedEntrySize)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,2,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{BARControl.tr("bytes"),
                                                                                                BARControl.tr("KBytes"),
                                                                                                BARControl.tr("MBytes"),
                                                                                                BARControl.tr("GBytes"),
                                                                                                BARControl.tr("TBytes")
                                                                                               }
                                                                            )
                    );
      Widgets.addModifyListener(new WidgetModifyListener(label,skippedEntrySize)
      {
        @Override
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // error files/bytes
      label = Widgets.newLabel(composite,BARControl.tr("Errors")+":");
      Widgets.layout(label,3,0,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,3,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,errorEntryCount));
      label = Widgets.newLabel(composite,BARControl.tr("files"));
      Widgets.layout(label,3,2,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,3,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,errorEntrySize));
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,3,4,TableLayoutData.W);
      label = Widgets.newLabel(composite,"/");
      Widgets.layout(label,3,5,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,3,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,errorEntrySize)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,3,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{BARControl.tr("bytes"),
                                                                                                BARControl.tr("KBytes"),
                                                                                                BARControl.tr("MBytes"),
                                                                                                BARControl.tr("GBytes"),
                                                                                                BARControl.tr("TBytes")
                                                                                               }
                                                                            )
                    );
      Widgets.addModifyListener(new WidgetModifyListener(label,errorEntrySize)
      {
        @Override
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // total files/bytes
      label = Widgets.newLabel(composite,BARControl.tr("Total")+":");
      Widgets.layout(label,4,0,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,4,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,totalEntryCount));
      Widgets.addModifyListener(new WidgetModifyListener(label,collectTotalSumDone)
      {
        @Override
        public void modified(Control control, WidgetVariable widgetVariable)
        {
          final Color COLOR_IN_PROGRESS = display.getSystemColor(SWT.COLOR_DARK_GRAY);
          control.setForeground(widgetVariable.getBoolean() ? null : COLOR_IN_PROGRESS);
        }
      });
      label = Widgets.newLabel(composite,BARControl.tr("files"));
      Widgets.layout(label,4,2,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,4,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,totalEntrySize));
      Widgets.addModifyListener(new WidgetModifyListener(label,collectTotalSumDone)
      {
        @Override
        public void modified(Control control, WidgetVariable widgetVariable)
        {
          final Color COLOR_IN_PROGRESS = display.getSystemColor(SWT.COLOR_DARK_GRAY);
          control.setForeground(widgetVariable.getBoolean() ? null : COLOR_IN_PROGRESS);
        }
      });
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,4,4,TableLayoutData.W);
      label = Widgets.newLabel(composite,"/");
      Widgets.layout(label,4,5,TableLayoutData.W);
      label = Widgets.newNumberView(composite);
      Widgets.layout(label,4,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,totalEntrySize)
      {
        @Override
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(label,collectTotalSumDone)
      {
        @Override
        public void modified(Control control, WidgetVariable widgetVariable)
        {
          final Color COLOR_IN_PROGRESS = display.getSystemColor(SWT.COLOR_DARK_GRAY);
          control.setForeground(widgetVariable.getBoolean() ? null : COLOR_IN_PROGRESS);
        }
      });
      label = Widgets.newLabel(composite,BARControl.tr("bytes"));
      Widgets.layout(label,4,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{BARControl.tr("bytes"),
                                                                                                BARControl.tr("KBytes"),
                                                                                                BARControl.tr("MBytes"),
                                                                                                BARControl.tr("GBytes"),
                                                                                                BARControl.tr("TBytes")
                                                                                               }
                                                                            )
                    );
      Widgets.addModifyListener(new WidgetModifyListener(label,totalEntrySize)
      {
        @Override
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // current file, file percentage
      label = Widgets.newLabel(composite,BARControl.tr("File")+":");
      Widgets.layout(label,5,0,TableLayoutData.W);
      label = Widgets.newView(composite);
      Widgets.layout(label,5,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(label,fileName));
      progressBar = Widgets.newProgressBar(composite);
      Widgets.layout(progressBar,6,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,fileProgress));

      // storage file, storage percentage
      label = Widgets.newLabel(composite,BARControl.tr("Storage")+":");
      Widgets.layout(label,7,0,TableLayoutData.W);
      label = Widgets.newView(composite);
      Widgets.layout(label,7,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(label,storageName));
      progressBar = Widgets.newProgressBar(composite);
      Widgets.layout(progressBar,8,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,storageProgress));

      // volume percentage
      label = Widgets.newLabel(composite,BARControl.tr("Volume")+":");
      Widgets.layout(label,9,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(composite);
      Widgets.layout(progressBar,9,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,volumeProgress));

      // total files percentage
      label = Widgets.newLabel(composite,BARControl.tr("Total files")+":");
      Widgets.layout(label,10,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(composite);
      Widgets.layout(progressBar,10,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,totalEntriesProgress));

      // total bytes percentage
      label = Widgets.newLabel(composite,BARControl.tr("Total bytes")+":");
      Widgets.layout(label,11,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(composite);
      Widgets.layout(progressBar,11,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,totalBytesProgress));

      // message
      label = Widgets.newLabel(composite,BARControl.tr("Message")+":");
      Widgets.layout(label,12,0,TableLayoutData.W);
      label = Widgets.newView(composite);
      Widgets.layout(label,12,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(label,message)
      {
        @Override
        public String getString(WidgetVariable variable)
        {
          return message.getString().replaceAll("\\n+"," ");
        }
      });
      label.addMouseTrackListener(new MouseTrackListener()
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
          Label label = (Label)mouseEvent.widget;
          Text  text;

          if (widgetMessageToolTip != null)
          {
            widgetMessageToolTip.dispose();
            widgetMessageToolTip = null;
          }

          if (!message.getString().isEmpty())
          {
            final Color COLOR_FOREGROUND = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
            final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

            widgetMessageToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
            widgetMessageToolTip.setBackground(COLOR_BACKGROUND);
            widgetMessageToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
            Widgets.layout(widgetMessageToolTip,0,0,TableLayoutData.NSWE);

            text = Widgets.newText(widgetMessageToolTip,SWT.LEFT|SWT.V_SCROLL|SWT.MULTI|SWT.WRAP);
            text.setText(message.getString());
            text.setForeground(COLOR_FOREGROUND);
            text.setBackground(COLOR_BACKGROUND);
            Widgets.layout(text,0,0,TableLayoutData.NSWE,0,0,0,0,300,100);

            Point size = widgetMessageToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
            Rectangle bounds = label.getBounds();
            Point point = label.getParent().toDisplay(bounds.x,bounds.y);
            widgetMessageToolTip.setBounds(point.x+2,point.y+2,size.x,size.y);
            widgetMessageToolTip.setVisible(true);

            shell.addMouseTrackListener(new MouseTrackListener()
            {
              @Override
              public void mouseEnter(MouseEvent mouseEvent)
              {
              }

              @Override
              public void mouseExit(MouseEvent mouseEvent)
              {
                if ((widgetMessageToolTip != null) && !widgetMessageToolTip.isDisposed())
                {
                  // check if inside widget
                  Point point = shell.toDisplay(new Point(mouseEvent.x,mouseEvent.y));
                  if (widgetMessageToolTip.getBounds().contains(point))
                  {
                    return;
                  }

                  // check if inside sub-widget
                  for (Control control : widgetMessageToolTip.getChildren())
                  {
                    if (control.getBounds().contains(point))
                    {
                      return;
                    }
                  }

                  // close tooltip
                  widgetMessageToolTip.dispose();
                  widgetMessageToolTip = null;
                }
              }

              @Override
              public void mouseHover(MouseEvent mouseEvent)
              {
              }
            });
          }
        }
      });
    }

    // buttons
    composite = Widgets.newComposite(widgetTab);
    composite.setLayout(new TableLayout(null,new double[]{0.0,0.0,0.0,0.0,1.0}));
    Widgets.layout(composite,3,0,TableLayoutData.WE);
    {
      widgetButtonStart = Widgets.newButton(composite,null,BARControl.tr("Start")+"\u2026",BARServer.isMaster());
      widgetButtonStart.setToolTipText(BARControl.tr("Start selected job."));
      widgetButtonStart.setEnabled(false);
      Widgets.layout(widgetButtonStart,0,0,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
      widgetButtonStart.addSelectionListener(new SelectionListener()
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
            jobStart();
          }
        }
      });
      addUpdateJobStateListener(new UpdateJobStateListener(widgetButtonStart)
      {
        @Override
        public void handle(Widget widget, JobData jobData)
        {
          Button button = (Button)widget;
          button.setEnabled(   (jobData.state == JobData.States.NONE   )
                            || (jobData.state == JobData.States.DONE   )
                            || (jobData.state == JobData.States.ERROR  )
                            || (jobData.state == JobData.States.ABORTED)
                           );
        }
      });

      widgetButtonAbort = Widgets.newButton(composite,null,BARControl.tr("Abort")+"\u2026",BARServer.isMaster());
      widgetButtonAbort.setToolTipText(BARControl.tr("Abort selected job."));
      widgetButtonAbort.setEnabled(false);
      Widgets.layout(widgetButtonAbort,0,1,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
      widgetButtonAbort.addSelectionListener(new SelectionListener()
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
            jobAbort();
          }
        }
      });
      addUpdateJobStateListener(new UpdateJobStateListener(widgetButtonAbort)
      {
        @Override
        public void handle(Widget widget, JobData jobData)
        {
          Button button = (Button)widget;
          button.setEnabled(   (jobData.state == JobData.States.WAITING       )
                            || (jobData.state == JobData.States.RUNNING       )
                            || (jobData.state == JobData.States.NO_STORAGE    )
                            || (jobData.state == JobData.States.DRY_RUNNING   )
                            || (jobData.state == JobData.States.REQUEST_VOLUME)
                           );
        }
      });

      widgetButtonPause = Widgets.newButton(composite,null,BARControl.tr("Pause"),BARServer.isMaster() && Settings.hasNormalRole());
      widgetButtonPause.setToolTipText(BARControl.tr("Pause selected job for a specific time."));
      Widgets.layout(widgetButtonPause,0,2,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT); // how to calculate correct min. width? ,0,0,Widgets.getTextSize(widgetButtonSuspendContinue,new String[]{"Puase [xxxxs]"}));
      widgetButtonPause.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobPause(60*60);
        }
      });

      widgetButtonSuspendContinue = Widgets.newButton(composite,null,BARControl.tr("Continue"),BARServer.isMaster() && Settings.hasNormalRole());
      widgetButtonSuspendContinue.setToolTipText(BARControl.tr("Suspend selected job for an infinite time."));
      Widgets.layout(widgetButtonSuspendContinue,0,3,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT); // how to calculate correct min. width? ,0,0,Widgets.getTextSize(widgetButtonSuspendContinue,new String[]{BARControl.tr("Suspend"),BARControl.tr("Continue")}));
      widgetButtonSuspendContinue.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobSuspendContinue();
        }
      });

      widgetButtonVolume = Widgets.newButton(composite,null,BARControl.tr("Volume"),BARServer.isMaster());
      widgetButtonVolume.setToolTipText(BARControl.tr("Click when a new volume is available in drive."));
      widgetButtonVolume.setEnabled(false);
      Widgets.layout(widgetButtonVolume,0,4,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
      widgetButtonVolume.addSelectionListener(new SelectionListener()
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
            volume();
          }
        }
      });
      addUpdateJobStateListener(new UpdateJobStateListener(widgetButtonVolume)
      {
        @Override
        public void handle(Widget widget, JobData jobData)
        {
          Button button = (Button)widget;
          button.setEnabled(jobData.state == JobData.States.REQUEST_VOLUME);
        }
      });

      widgetButtonQuit = Widgets.newButton(composite,null,BARControl.tr("Quit"));
      widgetButtonQuit.setToolTipText(BARControl.tr("Quit BARControl program."));
      Widgets.layout(widgetButtonQuit,0,5,TableLayoutData.E,0,0,0,0,80,SWT.DEFAULT);
      widgetButtonQuit.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          // send close-event to shell
          Event event = new Event();
          shell.notifyListeners(SWT.Close,event);
        }
      });
    }

    // listeners
    shell.addListener(BARControl.USER_EVENT_NEW_SERVER,new Listener()
    {
      public void handleEvent(Event event)
      {
        clearSelectedJob();
        updateJobList();
      }
    });
    shell.addListener(BARControl.USER_EVENT_NEW_JOB,new Listener()
    {
      public void handleEvent(Event event)
      {
Dprintf.dprintf("new jobUUID=%s",event.text);
        updateJobList();

        JobData jobData = jobDataMap.get(event.text);
Dprintf.dprintf("new job=%s",jobData);
        setSelectedJob(jobData);
      }
    });
    shell.addListener(BARControl.USER_EVENT_UPDATE_JOB,new Listener()
    {
      public void handleEvent(Event event)
      {
Dprintf.dprintf("update jobData=%s",event.data);
        updateJobList();
      }
    });
    shell.addListener(BARControl.USER_EVENT_DELETE_JOB,new Listener()
    {
      public void handleEvent(Event event)
      {
Dprintf.dprintf("delete jobUUID=%s",event.text);
        clearSelectedJob();
        updateJobList();
      }
    });
    shell.addListener(BARControl.USER_EVENT_SELECT_JOB,new Listener()
    {
      public void handleEvent(Event event)
      {
        JobData jobData = (JobData)event.data;
Dprintf.dprintf("select jobData=%s",jobData);
        setSelectedJob(jobData);
      }
    });

    // create status update thread
    tabStatusUpdateThread = new TabStatusUpdateThread(this);
    tabStatusUpdateThread.setDaemon(true);
  }

  /** set jobs tab
   * @param tabJobs jobs tab
   */
  void setTabJobs(TabJobs tabJobs)
  {
    this.tabJobs = tabJobs;
  }

  /** start update job data
   */
  public void startUpdate()
  {
    update();
    tabStatusUpdateThread.start();
  }

  /** get job by name
   * @param name job name
   * @return job data or null
   */
  public JobData getJobByName(String name)
  {
    for (JobData jobData : jobDataMap.values())
    {
      if (jobData.name.equals(name)) return jobData;
    }
    return null;
  }

  /** add update job state listener
   * @param updateJobStateLister update job state listener
   */
  public void addUpdateJobStateListener(UpdateJobStateListener updateJobStateListener)
  {
    updateJobStateListeners.add(updateJobStateListener);
  }

  /** remove update job state listener
   * @param updateJobStateLister update job state listener
   */
  public void removeUpdateJobStateListener(UpdateJobStateListener updateJobStateListener)
  {
    updateJobStateListeners.remove(updateJobStateListener);
  }

  //-----------------------------------------------------------------------

  /** update job list
   */
  private void updateJobList()
  {
    try
    {
      // get job list
      final HashMap<String,JobData> newJobDataMap = new HashMap<String,JobData>();
      BARServer.executeCommand(StringParser.format("JOB_LIST"),
                               3,  // debugLevel
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   // get data
                                   String              jobUUID                = valueMap.getString("jobUUID"                             );
                                   String              master                 = valueMap.getString("master",""                           );
                                   String              name                   = valueMap.getString("name"                                );
                                   JobData.States      state                  = valueMap.getEnum  ("state",JobData.States.class          );
                                   String              slaveHostName          = valueMap.getString("slaveHostName",""                    );
                                   JobData.SlaveStates slaveState             = valueMap.getEnum  ("slaveState",JobData.SlaveStates.class);
                                   ArchiveTypes        archiveType            = valueMap.getEnum  ("archiveType",ArchiveTypes.class      );
                                   long                archivePartSize        = valueMap.getLong  ("archivePartSize"                     );
//TODO: enum?
                                   String              deltaCompressAlgorithm = valueMap.getString("deltaCompressAlgorithm"              );
//TODO: enum?
                                   String              byteCompressAlgorithm  = valueMap.getString("byteCompressAlgorithm"               );
//TODO: enum?
                                   String              cryptAlgorithm         = valueMap.getString("cryptAlgorithm"                      );
//TODO: enum?
                                   String              cryptType              = valueMap.getString("cryptType"                           );
//TODO: enum?
                                   String              cryptPasswordMode      = valueMap.getString("cryptPasswordMode"                   );
                                   long                lastExecutedDateTime   = valueMap.getLong  ("lastExecutedDateTime"                );
                                   long                estimatedRestTime      = valueMap.getLong  ("estimatedRestTime"                   );

                                   JobData jobData = jobDataMap.get(jobUUID);
                                   if (jobData != null)
                                   {
                                     jobData.name                   = name;
                                     jobData.master                 = master;
                                     jobData.state                  = state;
                                     jobData.slaveHostName          = slaveHostName;
                                     jobData.slaveState             = slaveState;
                                     jobData.archiveType            = archiveType;
                                     jobData.archivePartSize        = archivePartSize;
                                     jobData.deltaCompressAlgorithm = deltaCompressAlgorithm;
                                     jobData.byteCompressAlgorithm  = byteCompressAlgorithm;
                                     jobData.cryptAlgorithm         = cryptAlgorithm;
                                     jobData.cryptType              = cryptType;
                                     jobData.cryptPasswordMode      = cryptPasswordMode;
                                     jobData.lastExecutedDateTime   = lastExecutedDateTime;
                                     jobData.estimatedRestTime      = estimatedRestTime;
                                   }
                                   else
                                   {
                                     jobData = new JobData(jobUUID,
                                                           master,
                                                           name,
                                                           state,
                                                           slaveHostName,
                                                           slaveState,
                                                           archiveType,
                                                           archivePartSize,
                                                           deltaCompressAlgorithm,
                                                           byteCompressAlgorithm,
                                                           cryptAlgorithm,
                                                           cryptType,
                                                           cryptPasswordMode,
                                                           lastExecutedDateTime,
                                                           estimatedRestTime
                                                          );
                                   }
                                   newJobDataMap.put(jobUUID,jobData);
                                 }
                               }
                              );
      synchronized(jobDataMap)
      {
        jobDataMap = newJobDataMap;
      }

      // update job table
      display.syncExec(new Runnable()
      {
        public void run()
        {
          if (!widgetJobTable.isDisposed())
          {
            // get table items
            HashSet<TableItem> removeTableItemSet = new HashSet<TableItem>();
            for (TableItem tableItem : widgetJobTable.getItems())
            {
              removeTableItemSet.add(tableItem);
            }

            synchronized(jobDataMap)
            {
              for (JobData jobData : jobDataMap.values())
              {
                // find table item
                TableItem tableItem = Widgets.getTableItem(widgetJobTable,jobData);

                // update/create table item
                if (tableItem != null)
                {
                  Widgets.updateTableItem(tableItem,
                                          jobData,
                                          jobData.name,
                                          (serverState == BARServer.States.RUNNING)
                                            ? JobData.formatStateText(jobData.state,jobData.slaveHostName,jobData.slaveState)
                                            : BARControl.tr("suspended"),
                                          jobData.slaveHostName,
                                          jobData.archiveType.getText(),
                                          (jobData.archivePartSize > 0) ? Units.formatByteSize(jobData.archivePartSize) : BARControl.tr("unlimited"),
                                          jobData.formatCompressAlgorithm(),
                                          jobData.formatCryptAlgorithm(),
                                          jobData.formatLastExecutedDateTime(),
                                          jobData.formatEstimatedRestTime()
                                         );

                  // keep table item
                  removeTableItemSet.remove(tableItem);
                }
                else
                {
                  // insert new item
                  tableItem = Widgets.insertTableItem(widgetJobTable,
                                                      findJobTableItemIndex(jobData),
                                                      jobData,
                                                      jobData.name,
                                                      (serverState == BARServer.States.RUNNING)
                                                        ? JobData.formatStateText(jobData.state,jobData.slaveHostName,jobData.slaveState)
                                                        : BARControl.tr("suspended"),
                                                      jobData.slaveHostName,
                                                      jobData.archiveType.toString(),
                                                      (jobData.archivePartSize > 0) ? Units.formatByteSize(jobData.archivePartSize) : BARControl.tr("unlimited"),
                                                      jobData.formatCompressAlgorithm(),
                                                      jobData.formatCryptAlgorithm(),
                                                      jobData.formatLastExecutedDateTime(),
                                                      jobData.formatEstimatedRestTime()
                                                     );
                  tableItem.setData(jobData);
                }

                switch (jobData.state)
                {
                  case RUNNING:
                  case NO_STORAGE:
                  case DRY_RUNNING:
                    tableItem.setBackground(COLOR_RUNNING);
                    break;
                  case REQUEST_FTP_PASSWORD:
                  case REQUEST_SSH_PASSWORD:
                  case REQUEST_WEBDAV_PASSWORD:
                  case REQUEST_CRYPT_PASSWORD:
                  case REQUEST_VOLUME:
                    tableItem.setBackground(COLOR_REQUEST);
                    break;
                  case ERROR:
                    tableItem.setBackground(COLOR_ERROR);
                    break;
                  case ABORTED:
                    tableItem.setBackground(COLOR_ABORTED);
                    break;
                  default:
                    tableItem.setBackground(null);
                    break;
                }
              }
            }

            // remove not existing entries
            for (TableItem tableItem : removeTableItemSet)
            {
              Widgets.removeTableItem(widgetJobTable,tableItem);
            }
          }
        }
      });

    }
    catch (Exception exception)
    {
      // ignored
    }
    catch (ConnectionError error)
    {
      // ignored
    }

    // update tab jobs list
    tabJobs.updateJobList(jobDataMap.values());
  }

  /** update job trigger
   */
  private void updateJobTrigger()
  {
    // clear trigger menu
    display.syncExec(new Runnable()
    {
      public void run()
      {
        MenuItem[] menuItems = menuTriggerJob.getItems();
        for (int i = 0; i < menuItems.length; i++)
        {
          menuItems[i].dispose();
        }
      }
    });

    // set trigger menu
    if (selectedJobData != null)
    {
      try
      {
//TODO: sort
        // get schedule list
        BARServer.executeCommand(StringParser.format("SCHEDULE_LIST jobUUID=%s",
                                                     selectedJobData.uuid
                                                    ),
                                 0,  // debugLevel
                                 new Command.ResultHandler()
                                 {
                                   @Override
                                   public void handle(int i, ValueMap valueMap)
                                   {
                                     // get data
                                     final String       scheduleUUID = valueMap.getString ("scheduleUUID"                  );
                                     final String       date         = valueMap.getString ("date"                          );
                                     final String       weekDays     = valueMap.getString ("weekDays"                      );
                                     final String       time         = valueMap.getString ("time"                          );
                                     final ArchiveTypes archiveType  = valueMap.getEnum   ("archiveType",ArchiveTypes.class);

                                     display.syncExec(new Runnable()
                                     {
                                       public void run()
                                       {
                                         MenuItem menuItem = Widgets.addMenuItem(menuTriggerJob,
                                                                                 String.format("%s %s %s %s",date,weekDays,time,archiveType)
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
                                             try
                                             {
                                               BARServer.executeCommand(StringParser.format("SCHEDULE_TRIGGER jobUUID=%s scheduleUUID=%s",
                                                                                            selectedJobData.uuid,
                                                                                            scheduleUUID
                                                                                           ),
                                                                        0  // debugLevel
                                                                       );
                                             }
                                             catch (Exception exception)
                                             {
                                               Dialogs.error(shell,BARControl.tr("Cannot trigger schedule of job ''{0}'':\n\n{1}",
                                                                                 selectedJobData.name.replaceAll("&","&&"),
                                                                                 exception.getMessage()
                                                                                )
                                                            );
                                               return;
                                             }
                                           }
                                         });
                                       }
                                     });
                                   }
                                 }
                                );
      }
      catch (Exception exception)
      {
        // ignored
      }
      catch (ConnectionError error)
      {
        // ignored
      }
    }
  }

  /** set selected job
   * @param jobData job data
   */
  private void setSelectedJob(JobData jobData)
  {
    selectedJobData = jobData;

    if (selectedJobData != null)
    {
      Widgets.setSelectedTableItem(widgetJobTable,selectedJobData);
    }
    else
    {
      Widgets.clearSelectedTableItems(widgetJobTable);
    }
    updateJobTrigger();
    widgetSelectedJob.setText(BARControl.tr("Selected")+" '"+((selectedJobData != null)
                                                                ? selectedJobData.name
                                                                : ""
                                                             )+"'"
                             );
  }

  /** clear selected job
   */
  private void clearSelectedJob()
  {
    selectedJobData = null;

    Widgets.clearSelectedTableItems(widgetJobTable);
    updateJobTrigger();
    widgetSelectedJob.setText(BARControl.tr("Selected")+" ''");
  }

  /** getProgress
   * @param n,m process current/max. value
   * @return progress value [%]
   */
  private double getProgress(long n, long m)
  {
    return (m > 0) ? ((double)n*100.0)/(double)m : 0.0;
  }

  /** update server status
   */
  private void updateStatus()
    throws Exception
  {
    try
    {
      ValueMap valueMap = new ValueMap();
      BARServer.executeCommand(StringParser.format("STATUS"),
                               3,  // debugLevel
                               valueMap
                              );
      serverState = valueMap.getEnum("state",BARServer.States.class,BARServer.States.RUNNING);

      switch (serverState)
      {
        case PAUSED:
          final long pauseTime = valueMap.getLong("time");
          display.syncExec(new Runnable()
          {
            public void run()
            {
              if (!widgetButtonPause.isDisposed())
              {
                widgetButtonPause.setText(String.format(BARControl.tr("Pause [%3dmin]"),(pauseTime > 0) ? (pauseTime+59)/60:1));
              }
              if (!widgetButtonSuspendContinue.isDisposed())
              {
                widgetButtonSuspendContinue.setText(BARControl.tr("Continue"));
              }
            }
          });
          break;
        case SUSPENDED:
          display.syncExec(new Runnable()
          {
            public void run()
            {
              if (!widgetButtonPause.isDisposed())
              {
                widgetButtonPause.setText(BARControl.tr("Pause"));
              }
              if (!widgetButtonSuspendContinue.isDisposed())
              {
                widgetButtonSuspendContinue.setText(BARControl.tr("Continue"));
              }
            }
          });
          break;
        default:
          display.syncExec(new Runnable()
          {
            public void run()
            {
              if (!widgetButtonPause.isDisposed())
              {
                widgetButtonPause.setText(BARControl.tr("Pause"));
              }
              if (!widgetButtonSuspendContinue.isDisposed())
              {
                widgetButtonSuspendContinue.setText(BARControl.tr("Suspend"));
              }
            }
          });
          break;
      }

      updateStatusFailCount = 0;
    }
    catch (Exception exception)
    {
      updateStatusFailCount++;
      if (updateStatusFailCount > 5)
      {
        throw exception;
      }
    }
  }

  /** get table item for job by name
   * @param name job name
   * @return table item or null if not found
   */
  private TableItem getJobTableItemByName(String name)
  {
    TableItem tableItems[] = widgetJobTable.getItems();
    for (int i = 0; i < tableItems.length; i++)
    {
      if (((JobData)tableItems[i].getData()).name.equals(name))
      {
        return tableItems[i];
      }
    }

    return null;
  }

  /** get table item for job by UUID
   * @param jobUUID job UUID
   * @return table item or null if not found
   */
  private TableItem getJobTableItemByUUID(String jobUUID)
  {
    TableItem tableItems[] = widgetJobTable.getItems();
    for (int i = 0; i < tableItems.length; i++)
    {
      if (((JobData)tableItems[i].getData()).uuid.equals(jobUUID))
      {
        return tableItems[i];
      }
    }

    return null;
  }

  /** find index of table item for job
   * @param tableItems table items
   * @param id job id to find
   * @return index or 0 if not found
   */
  private int findJobTableItemIndex(JobData jobData)
  {
    TableItem         tableItems[]      = widgetJobTable.getItems();
    JobDataComparator jobDataComparator = new JobDataComparator(widgetJobTable);

    int index = 0;
    while (   (index < tableItems.length)
           && (jobDataComparator.compare(jobData,(JobData)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update job information
   */
  private void updateJobInfo()
  {
    if (selectedJobData != null)
    {
      // get job info
      try
      {
        final ValueMap valueMap = new ValueMap();
        BARServer.executeCommand(StringParser.format("JOB_STATUS jobUUID=%s",selectedJobData.uuid),
                                 3,  // debugLevel
                                 valueMap
                                );

        display.syncExec(new Runnable()
        {
          public void run()
          {
            JobData.States state     = valueMap.getEnum  ("state",JobData.States.class);
            int            errorCode = valueMap.getInt   ("errorCode");
            String         errorData = valueMap.getString("errorData");

            doneCount.set            (valueMap.getLong   ("doneCount"            ));
            doneSize.set             (valueMap.getLong   ("doneSize"             ));
            storageTotalSize.set     (valueMap.getLong   ("storageTotalSize"     ));
            skippedEntryCount.set    (valueMap.getLong   ("skippedEntryCount"    ));
            skippedEntrySize.set     (valueMap.getLong   ("skippedEntrySize"     ));
            errorEntryCount.set      (valueMap.getLong   ("errorEntryCount"      ));
            errorEntrySize.set       (valueMap.getLong   ("errorEntrySize"       ));
            totalEntryCount.set      (valueMap.getLong   ("totalEntryCount"      ));
            totalEntrySize.set       (valueMap.getLong   ("totalEntrySize"       ));
            collectTotalSumDone.set  (valueMap.getBoolean("collectTotalSumDone"  ));
            filesPerSecond.set       (valueMap.getDouble ("entriesPerSecond"     ));
            bytesPerSecond.set       (valueMap.getDouble ("bytesPerSecond"       ));
            storageBytesPerSecond.set(valueMap.getDouble ("storageBytesPerSecond"));
            compressionRatio.set     (valueMap.getDouble ("compressionRatio"     ));

            fileName.set             (valueMap.getString("entryName"));
            fileProgress.set         (getProgress(valueMap.getLong("entryDoneSize"),valueMap.getLong("entryTotalSize")));
            storageName.set          (valueMap.getString("storageName"));
            storageProgress.set      (getProgress(valueMap.getLong("storageDoneSize"),valueMap.getLong("storageTotalSize")));
            volumeNumber.set         (valueMap.getLong("volumeNumber"));
            volumeProgress.set       (valueMap.getDouble("volumeProgress")*100.0);
            totalEntriesProgress.set (getProgress(doneCount.getLong(),totalEntryCount.getLong()));
            totalBytesProgress.set   (getProgress(doneSize.getLong(),totalEntrySize.getLong()));
            requestedVolumeNumber.set(valueMap.getInt("requestedVolumeNumber"));
            message.set              (valueMap.getString("message"));

            // trigger update job state listeners
            if (selectedJobData != null)
            {
              for (UpdateJobStateListener updateJobStateListener : updateJobStateListeners)
              {
                updateJobStateListener.modified(selectedJobData);
              }
            }

            // set message
            switch (state)
            {
              case NONE:
              case WAITING:
                message.set("");
                break;
              case RUNNING:
              case NO_STORAGE:
              case DRY_RUNNING:
                break;
              case REQUEST_FTP_PASSWORD:
              case REQUEST_SSH_PASSWORD:
              case REQUEST_WEBDAV_PASSWORD:
              case REQUEST_CRYPT_PASSWORD:
                break;
              case REQUEST_VOLUME:
                if (message.getString().isEmpty())
                {
                  message.set(BARControl.tr("Please insert volume #{0}",requestedVolumeNumber.getInteger()));
                }
                else
                {
                  message.set(BARControl.tr("Please insert replacement volume #{0}:\n\n{1}",requestedVolumeNumber.getInteger(),message.getString()));
                }
                break;
              case DONE:
              case ERROR:
              case ABORTED:
                message.set(BARException.getText(errorCode,0,errorData));
                break;
            }
          }
        });
      }
      catch (Exception exception)
      {
        // ignored
      }
    }
    else
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          doneCount.set            (0L);
          doneSize.set             (0L);
          storageTotalSize.set     (0L);
          skippedEntryCount.set    (0L);
          skippedEntrySize.set     (0L);
          errorEntryCount.set      (0L);
          errorEntrySize.set       (0L);
          totalEntryCount.set      (0L);
          totalEntrySize.set       (0L);
          collectTotalSumDone.set  (false);
          filesPerSecond.set       (0.0);
          bytesPerSecond.set       (0.0);
          storageBytesPerSecond.set(0.0);
          compressionRatio.set     (0.0);

          fileName.set             ("");
          fileProgress.set         (getProgress(0L,0L));
          storageName.set          ("");
          storageProgress.set      (getProgress(0L,0L));
          volumeNumber.set         (0L);
          volumeProgress.set       (0.0);
          totalEntriesProgress.set (getProgress(0L,0L));
          totalBytesProgress.set   (getProgress(0L,0L));
          requestedVolumeNumber.set(0);
          message.set              ("");

          if (!widgetButtonStart.isDisposed())
          {
            widgetButtonStart.setEnabled(false);
          }
          if (!widgetButtonAbort.isDisposed())
          {
            widgetButtonAbort.setEnabled(false);
          }
          if (!widgetButtonVolume.isDisposed())
          {
            widgetButtonVolume.setEnabled(false);
          }
        }
      });
    }
  }

  /** update status, job list, job data
   */
  private void update()
  {
    try
    {
      updateJobList();

      updateStatus();
      updateJobInfo();
    }
    catch (Throwable throwable)
    {
      // ignored
    }
  }

  /** start selected job
   * @param archiveType archive type
   * @param password password or null
   * @param noStorageFlag true for no storage
   * @param dryRunFlag true for dry-run
   */
  private void jobStart(ArchiveTypes archiveType, String password, boolean noStorageFlag, boolean dryRunFlag)
  {
    if (selectedJobData != null)
    {
      if (password != null)
      {
        // set crypt password
        try
        {
          BARServer.executeCommand(StringParser.format("CRYPT_PASSWORD jobUUID=%s encryptType=%s encryptedPassword=%S",
                                                        selectedJobData.uuid,
                                                        BARServer.getPasswordEncryptType(),
                                                        BARServer.encryptPassword(password)
                                                       ),
                                   0  // debugLevel
                                  );
        }
        catch (Exception exception)
        {
          Dialogs.error(shell,
                        BARControl.tr("Cannot set crypt password for job ''{0}'' (error: {1})",
                                      selectedJobData.name.replaceAll("&","&&"),
                                      exception.getMessage()
                                     )
                       );
          return;
        }
      }

      // start
      try
      {
        BARServer.executeCommand(StringParser.format("JOB_START jobUUID=%s archiveType=%s noStorage=%y dryRun=%y",
                                                     selectedJobData.uuid,
                                                     archiveType.toString(),
                                                     noStorageFlag,
                                                     dryRunFlag
                                                    ),
                                 0  // debugLevel
                                );
        updateJobList();
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,
                      BARControl.tr("Cannot start job ''{0}'' (error: {1})",
                                    selectedJobData.name.replaceAll("&","&&"),
                                    exception.getMessage()
                                   )
                     );
        return;
      }
    }
  }

  /** start selected job
   * @param archiveType archive type
   * @param noStorageFlag true for no storage
   * @param dryRunFlag true for dry-run
   */
  private void jobStart(ArchiveTypes archiveType, boolean noStorageFlag, boolean dryRunFlag)
  {
    if (selectedJobData != null)
    {
      String password;

      if (selectedJobData.cryptPasswordMode.equals("ask"))
      {
        // get crypt password
        password = Dialogs.password(shell,
                                    BARControl.tr("Crypt password"),
                                    null,  // message
                                    null,  // name
                                    BARControl.tr("Crypt password")+":",
                                    BARControl.tr("Verify")+":"
                                   );
        if (password == null)
        {
          return;
        }
      }
      else
      {
        password = null;
      }

      // start
      jobStart(archiveType,password,noStorageFlag,dryRunFlag);
    }
  }

  /** start selected job
   */
  private void jobStart()
  {
    if (selectedJobData != null)
    {
      ArchiveTypes archiveType;
      boolean      noStorageFlag,dryRunFlag;

      // get archive type
      archiveType = ArchiveTypes.NORMAL;
      dryRunFlag  = false;
      switch (Dialogs.select(shell,
                             BARControl.tr("Confirmation"),
                             BARControl.tr("Start job ''{0}''?",selectedJobData.name.replaceAll("&","&&")),
                             new String[]{Settings.hasNormalRole() ? BARControl.tr("Normal") : null,
                                          BARControl.tr("Full"),
                                          BARControl.tr("Incremental"),
                                          Settings.hasExpertRole() ? BARControl.tr("Differential") : null,
                                          Settings.hasExpertRole() ? BARControl.tr("No storage") : null,
                                          Settings.hasExpertRole() ? BARControl.tr("Dry-run") : null,
                                          BARControl.tr("Cancel")
                                         },
                             new String[]{BARControl.tr("Store all files."),
                                          BARControl.tr("Store all files and create incremental data file."),
                                          BARControl.tr("Store changed files since last incremental or full storage and update incremental data file."),
                                          BARControl.tr("Store changed files since last full storage."),
                                          BARControl.tr("Collect all files and create incremental info data (bid-files) only."),
                                          BARControl.tr("Collect and process all files, but do not create archives.")
                                         },
                             4
                            )
             )
      {
        case 0: archiveType = ArchiveTypes.NORMAL;       noStorageFlag = false; dryRunFlag = false; break;
        case 1: archiveType = ArchiveTypes.FULL;         noStorageFlag = false; dryRunFlag = false; break;
        case 2: archiveType = ArchiveTypes.INCREMENTAL;  noStorageFlag = false; dryRunFlag = false; break;
        case 3: archiveType = ArchiveTypes.DIFFERENTIAL; noStorageFlag = false; dryRunFlag = false; break;
        case 4: archiveType = ArchiveTypes.FULL;         noStorageFlag = true;  dryRunFlag = false; break;
        case 5: archiveType = ArchiveTypes.NORMAL;       noStorageFlag = false; dryRunFlag = true;  break;
        default: return;
      }

      // start
      jobStart(archiveType,noStorageFlag,dryRunFlag);
    }
  }

  /** abort selected job
   */
  private void jobAbort()
  {
    assert selectedJobData != null;

    if ((selectedJobData.state != JobData.States.RUNNING) || Dialogs.confirm(shell,BARControl.tr("Abort running job ''{0}''?",selectedJobData.name.replaceAll("&","&&")),false))
    {
      final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Abort"),300,100,BARControl.tr("Abort job")+" '"+selectedJobData.name+"'\u2026",BusyDialog.TEXT0|BusyDialog.AUTO_ANIMATE|BusyDialog.ABORT_CLOSE);

      Background.run(new BackgroundRunnable(busyDialog)
      {
        public void run(final BusyDialog busyDialog)
        {
          try
          {
            // abort job
            BARServer.executeCommand(StringParser.format("JOB_ABORT jobUUID=%s",selectedJobData.uuid),
                                     0  // debugLevel
                                    );
            display.syncExec(new Runnable()
            {
              public void run()
              {
                updateJobList();
                busyDialog.close();
              }
            });
          }
          catch (final BARException exception)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                busyDialog.close();
                Dialogs.error(shell,BARControl.tr("Cannot abort job (error: {0})",exception.getMessage()));
              }
            });
          }
          catch (final CommunicationError error)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                busyDialog.close();
                Dialogs.error(shell,BARControl.tr("Communication error while abort job\n\n(error: {0})",error.getMessage()));
               }
            });
          }
          catch (final ConnectionError error)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                busyDialog.close();
                Dialogs.error(shell,BARControl.tr("Connection error while abort job\n\n(error: {0})",error.getMessage()));
               }
            });
          }
          catch (Exception exception)
          {
            BARServer.disconnect();
            BARControl.internalError(exception);
          }
        }
      });
    }
  }

  /** pause create/restore for all jobs
   * @param pauseTime pause time [s]
   */
  public void jobPause(long pauseTime)
  {
    StringBuffer buffer = new StringBuffer();
    if (Settings.pauseCreateFlag     ) { if (buffer.length() > 0) buffer.append(','); buffer.append("CREATE"      ); }
    if (Settings.pauseStorageFlag    ) { if (buffer.length() > 0) buffer.append(','); buffer.append("STORAGE"     ); }
    if (Settings.pauseRestoreFlag    ) { if (buffer.length() > 0) buffer.append(','); buffer.append("RESTORE"     ); }
    if (Settings.pauseIndexUpdateFlag) { if (buffer.length() > 0) buffer.append(','); buffer.append("INDEX_UPDATE"); }

    if (buffer.length() > 0)
    {
      try
      {
        BARServer.executeCommand(StringParser.format("PAUSE time=%d modeMask=%s",
                                                     pauseTime,
                                                     buffer.toString()
                                                    ),
                                 0  // debugLevel
                                );
      }
      catch (Exception exception)
      {
        Dialogs.error(shell,BARControl.tr("Cannot pause job (error: {0})",exception.getMessage()));
      }
    }
  }

  /** suspend/continue paused jobs
   */
  private void jobSuspendContinue()
  {
    try
    {
      switch (serverState)
      {
        case RUNNING:   BARServer.executeCommand(StringParser.format("SUSPEND") ,0); break;
        case PAUSED:
        case SUSPENDED: BARServer.executeCommand(StringParser.format("CONTINUE"),0); break;
      }
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,BARControl.tr("Cannot suspend job (error: {0})",exception.getMessage()));
    }
  }

  /** new volume
   */
  private void volume()
  {
    assert selectedJobData != null;

    try
    {
      long volumeNumber = requestedVolumeNumber.getInteger();
      switch (Dialogs.select(shell,
                             BARControl.tr("Volume request"),
                             BARControl.tr("Load volume number {0}.",volumeNumber),
                             new String[]{BARControl.tr("OK"),
                                          BARControl.tr("Unload tray"),
                                          BARControl.tr("Abort"),
                                          BARControl.tr("Cancel")
                                         },
                             0
                            )
             )
      {
        case 0:
          BARServer.executeCommand(StringParser.format("VOLUME_LOAD jobUUID=%s volumeNumber=%d",
                                                       selectedJobData.uuid,
                                                       volumeNumber
                                                      ),
                                   0  // debugLevel
                                  );
          break;
        case 1:
          BARServer.executeCommand(StringParser.format("VOLUME_UNLOAD jobUUID=%s",
                                                       selectedJobData.uuid
                                                      ),
                                   0  // debugLevel
                                  );
          break;
        case 2:
          BARServer.executeCommand(StringParser.format("JOB_ABORT jobUUID=%s",
                                                       selectedJobData.uuid
                                                      ),
                                   0  // debugLevel
                                  );
          break;
        case 3:
          break;
      }
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,BARControl.tr("Cannot change volume (error: {0})",exception.getMessage()));
    }
  }

  /** reset job state
   */
  private void jobReset()
  {
    try
    {
      BARServer.executeCommand(StringParser.format("JOB_RESET jobUUID=%s",selectedJobData.uuid),0);
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,BARControl.tr("Cannot reset job (error: {0})",exception.getMessage()));
    }
  }

  /** create new job
   */
  private void jobNew()
  {
    tabJobs.jobNew();
    updateJobList();
  }

  /** clone selected job
   */
  private void jobClone()
  {
    assert selectedJobData != null;

    tabJobs.jobClone(selectedJobData);
    updateJobList();
  }

  /** rename selected job
   */
  private void jobRename()
  {
    assert selectedJobData != null;

    if (tabJobs.jobRename(selectedJobData))
    {
      updateJobList();
    }
  }

  /** delete selected job
   */
  private void jobDelete()
  {
    assert selectedJobData != null;

    if (selectedJobData.state != JobData.States.RUNNING)
    {
      if (tabJobs.jobDelete(selectedJobData))
      {
        clearSelectedJob();
        updateJobList();
      }
    }
  }

  /** show job tool tip
   * @param entryIndexData entry index data
   * @param x,y positions
   */
  private void showJobToolTip(JobData jobData, int x, int y)
  {
    int     row;
    Label   label;
    Table   table;
    Control control;

    final Color COLOR_FOREGROUND = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
    final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

    if (widgetJobTableToolTip != null)
    {
      widgetJobTableToolTip.dispose();
    }

    try
    {
      final ValueMap valueMap = new ValueMap();
      BARServer.executeCommand(StringParser.format("JOB_INFO jobUUID=%s",jobData.uuid),
                               0,  // debugLevel
                               valueMap
                              );
      long lastExecutedDateTime        = valueMap.getLong("lastExecutedDateTime");
      long executionCountNormal        = valueMap.getLong("executionCountNormal");
      long executionCountFull          = valueMap.getLong("executionCountFull");
      long executionCountIncremental   = valueMap.getLong("executionCountIncremental");
      long executionCountDifferential  = valueMap.getLong("executionCountDifferential");
      long executionCountContinuous    = valueMap.getLong("executionCountContinuous");
      long averageDurationNormal       = valueMap.getLong("averageDurationNormal");
      long averageDurationFull         = valueMap.getLong("averageDurationFull");
      long averageDurationIncremental  = valueMap.getLong("averageDurationIncremental");
      long averageDurationDifferential = valueMap.getLong("averageDurationDifferential");
      long averageDurationContinuous   = valueMap.getLong("averageDurationContinuous");
      long totalEntityCount            = valueMap.getLong("totalEntityCount");
      long totalStorageCount           = valueMap.getLong("totalStorageCount");
      long totalStorageSize            = valueMap.getLong("totalStorageSize");
      long totalEntryCount             = valueMap.getLong("totalEntryCount");
      long totalEntrySize              = valueMap.getLong("totalEntrySize");

      widgetJobTableToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
      widgetJobTableToolTip.setBackground(COLOR_BACKGROUND);
      widgetJobTableToolTip.setLayout(new TableLayout(0.0,new double[]{0.0,1.0},2));
      Widgets.layout(widgetJobTableToolTip,0,0,TableLayoutData.NSWE);

      row = 0;

      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("Job")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetJobTableToolTip,jobData.name.replaceAll("&","&&"));
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("Entities")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("{0}",totalEntityCount));
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("Storages")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("{0}",totalStorageCount));
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetJobTableToolTip,String.format(BARControl.tr("{0} ({1} bytes)",Units.formatByteSize(totalStorageSize),totalStorageSize)));
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("Entries")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("{0}",totalEntryCount));
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetJobTableToolTip,String.format(BARControl.tr("{0} ({1} bytes)",Units.formatByteSize(totalEntrySize),totalEntrySize)));
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("Last executed")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.W);
      label = Widgets.newLabel(widgetJobTableToolTip,(lastExecutedDateTime > 0) ? SIMPLE_DATE_FORMAT.format(new Date(lastExecutedDateTime*1000)) : "");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,1,TableLayoutData.WE);
      row++;

      label = Widgets.newLabel(widgetJobTableToolTip,BARControl.tr("Statistics")+":");
      label.setForeground(COLOR_FOREGROUND);
      label.setBackground(COLOR_BACKGROUND);
      Widgets.layout(label,row,0,TableLayoutData.NW);
      table = Widgets.newTable(widgetJobTableToolTip);
      table.setForeground(COLOR_FOREGROUND);
      table.setBackground(COLOR_BACKGROUND);
      table.setLayout(new TableLayout(null,1.0));
      Widgets.layout(table,row,1,TableLayoutData.W);
      Widgets.addTableColumn(table,0,BARControl.tr("Type"),        SWT.LEFT, 100,false);
      Widgets.addTableColumn(table,1,BARControl.tr("Count"),       SWT.RIGHT, 60,false);
      Widgets.addTableColumn(table,2,BARControl.tr("Average time"),SWT.LEFT, 120,false);
      Widgets.addTableItem(table,null,BARControl.tr("normal"     ),executionCountNormal,      String.format("%02d:%02d:%02d",averageDurationNormal      /(60*60),averageDurationNormal      %(60*60)/60,averageDurationNormal      %60));
      Widgets.addTableItem(table,null,BARControl.tr("full"       ),executionCountFull,        String.format("%02d:%02d:%02d",averageDurationFull        /(60*60),averageDurationFull        %(60*60)/60,averageDurationFull        %60));
      Widgets.addTableItem(table,null,BARControl.tr("incremental"),executionCountIncremental, String.format("%02d:%02d:%02d",averageDurationIncremental /(60*60),averageDurationIncremental %(60*60)/60,averageDurationIncremental %60));
      Widgets.addTableItem(table,null,BARControl.tr("differental"),executionCountDifferential,String.format("%02d:%02d:%02d",averageDurationDifferential/(60*60),averageDurationDifferential%(60*60)/60,averageDurationDifferential%60));
      Widgets.addTableItem(table,null,BARControl.tr("continuous" ),executionCountContinuous,  String.format("%02d:%02d:%02d",averageDurationContinuous  /(60*60),averageDurationContinuous  %(60*60)/60,averageDurationContinuous  %60));
      row++;

      Point size = widgetJobTableToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
      widgetJobTableToolTip.setBounds(x,y,size.x,size.y);
      widgetJobTableToolTip.setVisible(true);

      shell.addMouseTrackListener(new MouseTrackListener()
      {
        @Override
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }
        @Override
        public void mouseExit(MouseEvent mouseEvent)
        {
          if ((widgetJobTableToolTip != null) && !widgetJobTableToolTip.isDisposed())
          {
            // check if inside sub-widget
            Point point = shell.toDisplay(new Point(mouseEvent.x,mouseEvent.y));
            if (widgetJobTableToolTip.getBounds().contains(point))
            {
              return;
            }
            for (Control control : widgetJobTableToolTip.getChildren())
            {
              if (control.getBounds().contains(point))
              {
                return;
              }
            }

            // close tooltip
            widgetJobTableToolTip.dispose();
            widgetJobTableToolTip = null;
          }
        }
        @Override
        public void mouseHover(MouseEvent mouseEvent)
        {
        }
      });
    }
    catch (Exception exception)
    {
      // ignored
    }
  }
}

/* end of file */
