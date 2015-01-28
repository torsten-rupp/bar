/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TabStatus.java,v $
* $Revision: 1.23 $
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
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
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
  String uuid;
  String name;
  String state;
  String archiveType;
  long   archivePartSize;
  String deltaCompressAlgorithm;
  String byteCompressAlgorithm;
  String cryptAlgorithm;
  String cryptType;
  String cryptPasswordMode;
  long   lastExecutedDateTime;
  long   estimatedRestTime;

  // date/time format
  private final SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  /** create job data
   * @param uuid job UUID
   * @param name name
   * @param state job state
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
  JobData(String uuid, String name, String state, String archiveType, long archivePartSize, String deltaCompressAlgorithm, String byteCompressAlgorithm, String cryptAlgorithm, String cryptType, String cryptPasswordMode, long lastExecutedDateTime, long estimatedRestTime)
  {
    this.uuid                   = uuid;
    this.name                   = name;
    this.state                  = state;
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
  String formatCompressAlgorithm()
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
      return "none";
    }
  }

  /** format job crypt algorithm (including "*" for asymmetric)
   * @return crypt algorithm string
   */
  String formatCryptAlgorithm()
  {
    return cryptAlgorithm+(cryptType.equals("ASYMMETRIC") ? "*" : "");
  }

  /** format last executed date/time
   * @return date/time string
   */
  String formatLastExecutedDateTime()
  {
    if (lastExecutedDateTime > 0)
    {
      return simpleDateFormat.format(new Date(lastExecutedDateTime*1000));
    }
    else
    {
      return "-";
    }
  }

  /** format estimated rest time
   * @return estimated rest time string
   */
  String formatEstimatedRestTime()
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
  public String toString()
  {
    return "Job {"+uuid+", "+name+", "+state+", "+archiveType+"}";
  }
};

/** job data comparator
 */
class JobDataComparator implements Comparator<JobData>
{
  // Note: enum in inner classes are not possible in Java, thus use the old way...
  private final static int SORTMODE_NAME                   = 0;
  private final static int SORTMODE_STATE                  = 1;
  private final static int SORTMODE_TYPE                   = 2;
  private final static int SORTMODE_PARTSIZE               = 3;
  private final static int SORTMODE_COMPRESS               = 4;
  private final static int SORTMODE_CRYPT                  = 5;
  private final static int SORTMODE_LAST_EXECUTED_DATETIME = 6;
  private final static int SORTMODE_ESTIMATED_TIME         = 7;

  private int sortMode;

  /** create job data comparator
   * @param table job table
   * @param sortColumn sorting column
   */
  JobDataComparator(Table table, TableColumn sortColumn)
  {
    if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
    else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_STATE;
    else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TYPE;
    else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_PARTSIZE;
    else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_COMPRESS;
    else if (table.getColumn(5) == sortColumn) sortMode = SORTMODE_CRYPT;
    else if (table.getColumn(6) == sortColumn) sortMode = SORTMODE_LAST_EXECUTED_DATETIME;
    else if (table.getColumn(7) == sortColumn) sortMode = SORTMODE_ESTIMATED_TIME;
    else                                       sortMode = SORTMODE_NAME;
  }

  /** create job data comparator
   * @param table job table
   */
  JobDataComparator(Table table)
  {
    TableColumn sortColumn = table.getSortColumn();

    if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
    else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_STATE;
    else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TYPE;
    else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_PARTSIZE;
    else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_COMPRESS;
    else if (table.getColumn(5) == sortColumn) sortMode = SORTMODE_CRYPT;
    else if (table.getColumn(6) == sortColumn) sortMode = SORTMODE_LAST_EXECUTED_DATETIME;
    else if (table.getColumn(7) == sortColumn) sortMode = SORTMODE_ESTIMATED_TIME;
    else                                       sortMode = SORTMODE_NAME;
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
      case SORTMODE_NAME:
        return jobData1.name.compareTo(jobData2.name);
      case SORTMODE_STATE:
        return jobData1.state.compareTo(jobData2.state);
      case SORTMODE_TYPE:
        return jobData1.archiveType.compareTo(jobData2.archiveType);
      case SORTMODE_PARTSIZE:
        if      (jobData1.archivePartSize < jobData2.archivePartSize) return -1;
        else if (jobData1.archivePartSize > jobData2.archivePartSize) return  1;
        else                                                          return  0;
      case SORTMODE_COMPRESS:
        int result = jobData1.deltaCompressAlgorithm.compareTo(jobData2.deltaCompressAlgorithm);
        if (result == 0) result = jobData1.byteCompressAlgorithm.compareTo(jobData2.byteCompressAlgorithm);
      case SORTMODE_CRYPT:
        String crypt1 = jobData1.cryptAlgorithm+(jobData1.cryptType.equals("ASYMMETRIC") ?"*" : "");
        String crypt2 = jobData2.cryptAlgorithm+(jobData2.cryptType.equals("ASYMMETRIC") ?"*" : "");

        return crypt1.compareTo(crypt2);
      case SORTMODE_LAST_EXECUTED_DATETIME:
        if      (jobData1.lastExecutedDateTime < jobData2.lastExecutedDateTime) return -1;
        else if (jobData1.lastExecutedDateTime > jobData2.lastExecutedDateTime) return  1;
        else                                                                    return  0;
      case SORTMODE_ESTIMATED_TIME:
        if      (jobData1.estimatedRestTime < jobData2.estimatedRestTime) return -1;
        else if (jobData1.estimatedRestTime > jobData2.estimatedRestTime) return  1;
        else                                                              return  0;
      default:
        return 0;
    }
  }
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
        for (;;)
        {
          // update
          try
          {
            tabStatus.update();
          }
          catch (org.eclipse.swt.SWTException exception)
          {
            // ignore SWT exceptions
            if (Settings.debugLevel > 2)
            {
              BARControl.printStackTrace(exception);
            }
          }

          // sleep a short time
          try { Thread.sleep(1000); } catch (InterruptedException exception) { /* ignored */ };
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
  }

  /** running states
   */
  enum States
  {
    RUNNING,
    PAUSE,
    SUSPEND,
  };

  /** pause modes
   */
  enum PauseModes
  {
    NONE,
    CREATE,
    RESTORE,
    UPDATE_INDEX,
    NETWORK;
  };

  // global variable references
  private Shell                 shell;
  private Display               display;
  private TabStatusUpdateThread tabStatusUpdateThread;
  private TabJobs               tabJobs;

  // widgets
  public  Composite   widgetTab;
  private Table       widgetJobTable;
  private Shell       widgetMessageToolTip = null;
  private Group       widgetSelectedJob;
  public  Button      widgetButtonStart;
  public  Button      widgetButtonAbort;
  public  Button      widgetButtonPause;
  public  Button      widgetButtonSuspendContinue;
  private Button      widgetButtonVolume;
  public  Button      widgetButtonQuit;

  // BAR variables
  private WidgetVariable doneEntries           = new WidgetVariable(0);
  private WidgetVariable doneBytes             = new WidgetVariable(0);
  private WidgetVariable storageTotalBytes     = new WidgetVariable(0);
  private WidgetVariable skippedEntries        = new WidgetVariable(0);
  private WidgetVariable skippedBytes          = new WidgetVariable(0);
  private WidgetVariable errorEntries          = new WidgetVariable(0);
  private WidgetVariable errorBytes            = new WidgetVariable(0);
  private WidgetVariable totalEntries          = new WidgetVariable(0);
  private WidgetVariable totalBytes            = new WidgetVariable(0);

  private WidgetVariable filesPerSecond        = new WidgetVariable(0.0);
  private WidgetVariable bytesPerSecond        = new WidgetVariable(0.0);
  private WidgetVariable storageBytesPerSecond = new WidgetVariable(0.0);
  private WidgetVariable compressionRatio      = new WidgetVariable(0.0);

  private WidgetVariable fileName              = new WidgetVariable("");
  private WidgetVariable fileProgress          = new WidgetVariable(0.0);
  private WidgetVariable storageName           = new WidgetVariable("");
  private WidgetVariable storageProgress       = new WidgetVariable(0.0);
  private WidgetVariable volumeNumber          = new WidgetVariable(0);
  private WidgetVariable volumeProgress        = new WidgetVariable(0.0);
  private WidgetVariable totalEntriesProgress  = new WidgetVariable(0.0);
  private WidgetVariable totalBytesProgress    = new WidgetVariable(0.0);
  private WidgetVariable collectTotalSumDone   = new WidgetVariable(false);
  private WidgetVariable requestedVolumeNumber = new WidgetVariable(0);
  private WidgetVariable message               = new WidgetVariable("");

  // variables
  private HashMap<String,JobData> jobDataMap      = new HashMap<String,JobData>();
  private JobData                 selectedJobData = null;
  private States                  status          = States.RUNNING;

  /** create status tab
   * @param parentTabFolder parent tab folder
   * @param accelerator keyboard shortcut to select tab
   */
  TabStatus(TabFolder parentTabFolder, int accelerator)
  {
    TableColumn tableColumn;
    Menu        menu;
    MenuItem    menuItem;
    Group       group;
    Composite   composite;
    Button      button;
    Control     control;
    Label       label;
    ProgressBar progressBar;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,BARControl.tr("Status")+((accelerator != 0) ? " ("+Widgets.acceleratorToText(accelerator)+")" : ""));
    widgetTab.setLayout(new TableLayout(new double[]{1.0,0.0,0.0},1.0,2));
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
    widgetJobTable = Widgets.newTable(widgetTab,SWT.NONE);
    widgetJobTable.setToolTipText(BARControl.tr("List with job entries.\nClick to select job, right-click to open context menu."));
    widgetJobTable.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0}));
    Widgets.layout(widgetJobTable,0,0,TableLayoutData.NSWE);
    widgetJobTable.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        setSelectedJob((JobData)selectionEvent.item.getData());
      }
    });
    SelectionListener jobListColumnSelectionListener = new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        TableColumn       tableColumn       = (TableColumn)selectionEvent.widget;
        JobDataComparator jobDataComparator = new JobDataComparator(widgetJobTable,tableColumn);

        Widgets.sortTableColumn(widgetJobTable,tableColumn,jobDataComparator);
      }
    };
    tableColumn = Widgets.addTableColumn(widgetJobTable,0,BARControl.tr("Name"),          SWT.LEFT, 110,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for name."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,1,BARControl.tr("State"),         SWT.LEFT,  90,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for state."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,2,BARControl.tr("Type"),          SWT.LEFT,  90,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for type."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,3,BARControl.tr("Part size"),     SWT.RIGHT, 80,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for part size."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,4,BARControl.tr("Compress"),      SWT.LEFT,  80,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for used compress algorithm."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,5,BARControl.tr("Crypt"),         SWT.LEFT, 100,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for used encryption algorithm."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,6,BARControl.tr("Last executed"), SWT.LEFT, 150,true );
    tableColumn.setToolTipText(BARControl.tr("Click to sort for last date/time job was executed."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobTable,7,BARControl.tr("Estimated time"),SWT.LEFT, 120,false);
    tableColumn.setToolTipText(BARControl.tr("Click to sort for estimated rest time to execute job."));
    tableColumn.addSelectionListener(jobListColumnSelectionListener);

    menu = Widgets.newPopupMenu(shell);
    {
      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Start\u2026"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          jobStart();
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Abort\u2026"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          jobAbort();
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Pause"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          jobPause(60*60);
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Continue"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          jobSuspendContinue();
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Volume"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          volume();
        }
      });

      Widgets.addMenuSeparator(menu);

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("New\u2026"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobNew();
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clone\u2026"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobClone();
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Rename\u2026"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobRename();
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Delete\u2026"));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          jobDelete();
        }
      });
    }
    widgetJobTable.setMenu(menu);

    // selected job group
    widgetSelectedJob = Widgets.newGroup(widgetTab,BARControl.tr("Selected")+" ''",SWT.NONE);
    widgetSelectedJob.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0,1.0,0.0,0.0,1.0,0.0,1.0,1.0},4));
    Widgets.layout(widgetSelectedJob,1,0,TableLayoutData.WE);
    {
      // fix layout
      control = Widgets.newSpacer(widgetSelectedJob);
      Widgets.layout(control,0,0,TableLayoutData.WE,0,0,0,0,SWT.DEFAULT,1);

      // done files/bytes, files/s, bytes/s
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Done")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,1,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,doneEntries));
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("files"));
      Widgets.layout(label,1,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,1,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,doneBytes));
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,1,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,1,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,1,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,doneBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,1,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetModifyListener(label,doneBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      composite = Widgets.newComposite(widgetSelectedJob);
      composite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(composite,1,8,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(label,filesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return String.format("%.1f",variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,BARControl.tr("files/s"));
        Widgets.layout(label,0,1,TableLayoutData.W);
      }

      composite = Widgets.newComposite(widgetSelectedJob);
      composite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(composite,1,9,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(label,bytesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return Units.getByteSize(variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,BARControl.tr("bytes/s"));
        Widgets.layout(label,0,1,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes/s","KBytes/s","MBytes/s","GBytes/s"}));
        Widgets.addModifyListener(new WidgetModifyListener(label,bytesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return Units.getByteUnit(variable.getDouble())+"/s";
          }
        });
      }

      // stored files/bytes
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Stored")+":");
      Widgets.layout(label,2,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,2,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,storageTotalBytes));
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,2,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,2,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,2,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,storageTotalBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,2,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetModifyListener(label,storageTotalBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      composite = Widgets.newComposite(widgetSelectedJob);
      composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0}));
      Widgets.layout(composite,2,8,TableLayoutData.WE);
      {
        label = Widgets.newLabel(composite,BARControl.tr("Ratio"));
        Widgets.layout(label,0,0,TableLayoutData.W);
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,1,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(label,compressionRatio)
        {
          public String getString(WidgetVariable variable)
          {
            return String.format("%.1f",variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,"%");
        Widgets.layout(label,0,2,TableLayoutData.W);
      }

      composite = Widgets.newComposite(widgetSelectedJob);
      composite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(composite,2,9,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetModifyListener(label,storageBytesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return Units.getByteSize(variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,BARControl.tr("bytes/s"));
        Widgets.layout(label,0,1,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes/s","KBytes/s","MBytes/s","GBytes/s"}));
        Widgets.addModifyListener(new WidgetModifyListener(label,storageBytesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return Units.getByteUnit(variable.getDouble())+"/s";
          }
        });
      }

      // skipped files/bytes
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Skipped")+":");
      Widgets.layout(label,3,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,skippedEntries));
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("files"));
      Widgets.layout(label,3,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,skippedBytes));
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,3,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,3,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,skippedBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,3,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetModifyListener(label,skippedBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // error files/bytes
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Errors")+":");
      Widgets.layout(label,4,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,errorEntries));
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("files"));
      Widgets.layout(label,4,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,errorBytes));
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,4,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,4,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,errorBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,4,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetModifyListener(label,errorBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // total files/bytes
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Total")+":");
      Widgets.layout(label,5,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,5,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,totalEntries));
      Widgets.addModifyListener(new WidgetModifyListener(label,collectTotalSumDone)
      {
        public void modified(Control control, WidgetVariable widgetVariable)
        {
          final Color COLOR_IN_PROGRESS = display.getSystemColor(SWT.COLOR_DARK_GRAY);
          control.setForeground(widgetVariable.getBoolean() ? null : COLOR_IN_PROGRESS);
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("files"));
      Widgets.layout(label,5,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,5,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,totalBytes));
      Widgets.addModifyListener(new WidgetModifyListener(label,collectTotalSumDone)
      {
        public void modified(Control control, WidgetVariable widgetVariable)
        {
          final Color COLOR_IN_PROGRESS = display.getSystemColor(SWT.COLOR_DARK_GRAY);
          control.setForeground(widgetVariable.getBoolean() ? null : COLOR_IN_PROGRESS);
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,5,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,5,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,5,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetModifyListener(label,totalBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      Widgets.addModifyListener(new WidgetModifyListener(label,collectTotalSumDone)
      {
        public void modified(Control control, WidgetVariable widgetVariable)
        {
          final Color COLOR_IN_PROGRESS = display.getSystemColor(SWT.COLOR_DARK_GRAY);
          control.setForeground(widgetVariable.getBoolean() ? null : COLOR_IN_PROGRESS);
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("bytes"));
      Widgets.layout(label,5,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetModifyListener(label,totalBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // current file, file percentage
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("File")+":");
      Widgets.layout(label,6,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,6,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(label,fileName));
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,7,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,fileProgress));

      // storage file, storage percentage
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Storage")+":");
      Widgets.layout(label,8,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,8,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(label,storageName));
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,9,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,storageProgress));

      // volume percentage
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Volume")+":");
      Widgets.layout(label,10,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,10,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,volumeProgress));

      // total files percentage
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Total files")+":");
      Widgets.layout(label,11,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,11,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,totalEntriesProgress));

      // total bytes percentage
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Total bytes")+":");
      Widgets.layout(label,12,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,12,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(progressBar,totalBytesProgress));

      // message
      label = Widgets.newLabel(widgetSelectedJob,BARControl.tr("Message")+":");
      Widgets.layout(label,13,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,13,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetModifyListener(label,message));
      label.addMouseTrackListener(new MouseTrackListener()
      {
        public void mouseEnter(MouseEvent mouseEvent)
        {
        }

        public void mouseExit(MouseEvent mouseEvent)
        {
        }

        public void mouseHover(MouseEvent mouseEvent)
        {
          Label label = (Label)mouseEvent.widget;
          Text  text;

          if (widgetMessageToolTip != null)
          {
            widgetMessageToolTip.dispose();
            widgetMessageToolTip = null;
          }

          if (!message.getString().equals(""))
          {
            final Color COLOR_FORGROUND  = display.getSystemColor(SWT.COLOR_INFO_FOREGROUND);
            final Color COLOR_BACKGROUND = display.getSystemColor(SWT.COLOR_INFO_BACKGROUND);

            widgetMessageToolTip = new Shell(shell,SWT.ON_TOP|SWT.NO_FOCUS|SWT.TOOL);
            widgetMessageToolTip.setBackground(COLOR_BACKGROUND);
            widgetMessageToolTip.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},2));
            Widgets.layout(widgetMessageToolTip,0,0,TableLayoutData.NSWE);
            widgetMessageToolTip.addMouseTrackListener(new MouseTrackListener()
            {
              public void mouseEnter(MouseEvent mouseEvent)
              {
              }

              public void mouseExit(MouseEvent mouseEvent)
              {
                widgetMessageToolTip.dispose();
                widgetMessageToolTip = null;
              }

              public void mouseHover(MouseEvent mouseEvent)
              {
              }
            });

            text = Widgets.newText(widgetMessageToolTip,SWT.LEFT|SWT.V_SCROLL|SWT.MULTI|SWT.WRAP);
            text.setText(message.getString());
            text.setForeground(COLOR_FORGROUND);
            text.setBackground(COLOR_BACKGROUND);
            Widgets.layout(text,0,0,TableLayoutData.NSWE,0,0,0,0,300,100);

            Point size = widgetMessageToolTip.computeSize(SWT.DEFAULT,SWT.DEFAULT);
            Rectangle bounds = label.getBounds();
            Point point = label.getParent().toDisplay(bounds.x,bounds.y);
            widgetMessageToolTip.setBounds(point.x+2,point.y+2,size.x,size.y);
            widgetMessageToolTip.setVisible(true);
          }
        }
      });

    }

    // buttons
    composite = Widgets.newComposite(widgetTab);
    composite.setLayout(new TableLayout(null,new double[]{0.0,0.0,0.0,0.0,1.0}));
    Widgets.layout(composite,2,0,TableLayoutData.WE);
    {
      widgetButtonStart = Widgets.newButton(composite,null,BARControl.tr("Start\u2026"));
      widgetButtonStart.setToolTipText(BARControl.tr("Start selected job."));
      Widgets.layout(widgetButtonStart,0,0,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
      widgetButtonStart.setEnabled(false);
      widgetButtonStart.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobStart();
        }
      });

      widgetButtonAbort = Widgets.newButton(composite,null,BARControl.tr("Abort\u2026"));
      widgetButtonAbort.setToolTipText(BARControl.tr("Abort selected job."));
      Widgets.layout(widgetButtonAbort,0,1,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
      widgetButtonAbort.setEnabled(false);
      widgetButtonAbort.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobAbort();
        }
      });

      widgetButtonPause = Widgets.newButton(composite,null,BARControl.tr("Pause"));
      widgetButtonPause.setToolTipText(BARControl.tr("Pause selected job for a specific time."));
      Widgets.layout(widgetButtonPause,0,2,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT); // how to calculate correct min. width? ,0,0,Widgets.getTextSize(widgetButtonSuspendContinue,new String[]{"Puase [xxxxs]"}));
      widgetButtonPause.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobPause(60*60);
        }
      });

      widgetButtonSuspendContinue = Widgets.newButton(composite,null,BARControl.tr("Continue"));
      widgetButtonSuspendContinue.setToolTipText(BARControl.tr("Suspend selected job for an infinite time."));
      Widgets.layout(widgetButtonSuspendContinue,0,3,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT); // how to calculate correct min. width? ,0,0,Widgets.getTextSize(widgetButtonSuspendContinue,new String[]{BARControl.tr("Suspend"),BARControl.tr("Continue")}));
      widgetButtonSuspendContinue.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobSuspendContinue();
        }
      });

      widgetButtonVolume = Widgets.newButton(composite,null,BARControl.tr("Volume"));
      widgetButtonVolume.setToolTipText(BARControl.tr("Click when a new volume is available in drive."));
      Widgets.layout(widgetButtonVolume,0,4,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT);
      widgetButtonVolume.setEnabled(false);
      widgetButtonVolume.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          volume();
        }
      });

      widgetButtonQuit = Widgets.newButton(composite,null,BARControl.tr("Quit"));
      widgetButtonQuit.setToolTipText(BARControl.tr("Quit BARControl program."));
      Widgets.layout(widgetButtonQuit,0,5,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      widgetButtonQuit.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          // send close-evemnt to shell
          Event event = new Event();
          shell.notifyListeners(SWT.Close,event);
        }
      });
    }

    // start status update thread
    tabStatusUpdateThread = new TabStatusUpdateThread(this);
    tabStatusUpdateThread.setDaemon(true);
//    tabStatusUpdateThread.start();
  }

  /** set jobs tab
   * @param tabJobs jobs tab
   */
  void setTabJobs(TabJobs tabJobs)
  {
    this.tabJobs = tabJobs;
  }

  public void startUpdate()
  {
    tabStatusUpdateThread.start();
  }

  /** update job list
   */
  public void updateJobList()
  {
    if (!widgetJobTable.isDisposed())
    {
      try
      {
        // get job list
        HashMap<String,JobData>   newJobDataMap      = new HashMap<String,JobData>();
        String[]                  resultErrorMessage = new String[1];
        final ArrayList<ValueMap> resultMapList      = new ArrayList<ValueMap>();
        int error = BARServer.executeCommand(StringParser.format("JOB_LIST"),
                                             2,
                                             resultErrorMessage,
                                             resultMapList
                                            );
        if (error != Errors.NONE)
        {
          Dialogs.error(shell,BARControl.tr("Cannot get job list:\n\n{0}",resultErrorMessage[0]));
          return;
        }
        for (ValueMap resultMap : resultMapList)
        {
          // get data
          String jobUUID                = resultMap.getString("jobUUID"               );
          String name                   = resultMap.getString("name"                  );
          String state                  = resultMap.getString("state"                 );
          String archiveType            = resultMap.getString("archiveType"           );
          long   archivePartSize        = resultMap.getLong  ("archivePartSize"       );
          String deltaCompressAlgorithm = resultMap.getString("deltaCompressAlgorithm");
          String byteCompressAlgorithm  = resultMap.getString("byteCompressAlgorithm" );
          String cryptAlgorithm         = resultMap.getString("cryptAlgorithm"        );
          String cryptType              = resultMap.getString("cryptType"             );
          String cryptPasswordMode      = resultMap.getString("cryptPasswordMode"     );
          long   lastExecutedDateTime   = resultMap.getLong  ("lastExecutedDateTime"  );
          long   estimatedRestTime      = resultMap.getLong  ("estimatedRestTime"     );

          JobData jobData = jobDataMap.get(jobUUID);
          if (jobData != null)
          {
            jobData.name                   = name;
            jobData.state                  = state;
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
                                  name,
                                  state,
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
        jobDataMap = newJobDataMap;

        // update job table
        display.syncExec(new Runnable()
        {
          public void run()
          {
            synchronized(jobDataMap)
            {
              // get table items
              HashSet<TableItem> removeTableItemSet = new HashSet<TableItem>();
              for (TableItem tableItem : widgetJobTable.getItems())
              {
                removeTableItemSet.add(tableItem);
              }

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
                                          (status == States.RUNNING) ? jobData.state : BARControl.tr("suspended"),
                                          jobData.archiveType,
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
                                                      (status == States.RUNNING) ? jobData.state : BARControl.tr("suspended"),
                                                      jobData.archiveType,
                                                      (jobData.archivePartSize > 0) ? Units.formatByteSize(jobData.archivePartSize) : BARControl.tr("unlimited"),
                                                      jobData.formatCompressAlgorithm(),
                                                      jobData.formatCryptAlgorithm(),
                                                      jobData.formatLastExecutedDateTime(),
                                                      jobData.formatEstimatedRestTime()
                                                     );
                  tableItem.setData(jobData);
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

        // update tab jobs list
        synchronized(jobDataMap)
        {
          tabJobs.updateJobList(jobDataMap.values());
        }
      }
      catch (CommunicationError error)
      {
        Dialogs.error(shell,BARControl.tr("Cannot get job list:\n\n{0}",error.getMessage()));
        return;
      }
    }
  }

  /** select job by UUID
   * @param uuid job UUID
   */
  public void selectJob(String uuid)
  {
    setSelectedJob(jobDataMap.get(uuid));
  }

  /** set selected job
   * @param jobData job data
   */
  public void setSelectedJob(JobData jobData)
  {
    selectedJobData = jobData;

    Widgets.setSelectedTableItem(widgetJobTable,selectedJobData);
    widgetSelectedJob.setText(BARControl.tr("Selected")+" '"+((selectedJobData != null) ? selectedJobData.name : "")+"'");

    if (tabJobs != null) tabJobs.setSelectedJob(selectedJobData);
  }

  //-----------------------------------------------------------------------

  /** getProgress
   * @param n,m process current/max. value
   * @return progress value [%]
   */
  private double getProgress(long n, long m)
  {
    return (m > 0) ? ((double)n*100.0)/(double)m : 0.0;
  }

  /** update status
   */
  private void updateStatus()
  {
    // get status
    String[] resultErrorMessage = new String[1];
    ValueMap resultMap          = new ValueMap();
    int error = BARServer.executeCommand(StringParser.format("STATUS"),
                                         2,
                                         resultErrorMessage,
                                         resultMap
                                        );
    if (error != Errors.NONE)
    {
      return;
    }

    String type = resultMap.getString("type");
    if      (type.equals("pause"))
    {
      final long pauseTime = resultMap.getLong("time");

      status = States.PAUSE;
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetButtonPause.setText(String.format(BARControl.tr("Pause [%3dmin]"),(pauseTime > 0) ? (pauseTime+59)/60:1));
          widgetButtonSuspendContinue.setText(BARControl.tr("Continue"));
        }
      });
    }
    else if (type.equals("suspended"))
    {
      status = States.SUSPEND;
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetButtonPause.setText(BARControl.tr("Pause"));
          widgetButtonSuspendContinue.setText(BARControl.tr("Continue"));
        }
      });
    }
    else
    {
      status = States.RUNNING;
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetButtonPause.setText(BARControl.tr("Pause"));
          widgetButtonSuspendContinue.setText(BARControl.tr("Suspend"));
        }
      });
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
      final String resultErrorMessage[] = new String[1];
      final ValueMap resultMap          = new ValueMap();
      int error = BARServer.executeCommand(StringParser.format("JOB_INFO jobUUID=%s",selectedJobData.uuid),
                                           1,
                                           resultErrorMessage,
                                           resultMap
                                          );
      if (error != Errors.NONE)
      {
        return;
      }

      display.syncExec(new Runnable()
      {
        public void run()
        {
          String state = resultMap.getString("state");

          doneEntries.set          (resultMap.getLong("doneEntries"            ));
          doneBytes.set            (resultMap.getLong("doneBytes"              ));
          storageTotalBytes.set    (resultMap.getLong("storageTotalBytes"      ));
          skippedEntries.set       (resultMap.getLong("skippedEntries"         ));
          skippedBytes.set         (resultMap.getLong("skippedBytes"           ));
          errorEntries.set         (resultMap.getLong("errorEntries"           ));
          errorBytes.set           (resultMap.getLong("errorBytes"             ));
          totalEntries.set         (resultMap.getLong("totalEntries"           ));
          totalBytes.set           (resultMap.getLong("totalBytes"             ));
          collectTotalSumDone.set  (resultMap.getBoolean("collectTotalSumDone" ));
          filesPerSecond.set       (resultMap.getDouble("entriesPerSecond"     ));
          bytesPerSecond.set       (resultMap.getDouble("bytesPerSecond"       ));
          storageBytesPerSecond.set(resultMap.getDouble("storageBytesPerSecond"));
          compressionRatio.set     (resultMap.getDouble("compressionRatio"     ));

          fileName.set             (resultMap.getString("entryName"));
          fileProgress.set         (getProgress(resultMap.getLong("entryDoneBytes"),resultMap.getLong("entryTotalBytes")));
          storageName.set          (resultMap.getString("storageName"));
          storageProgress.set      (getProgress(resultMap.getLong("storageDoneBytes"),resultMap.getLong("storageTotalBytes")));
          volumeNumber.set         (resultMap.getLong("volumeNumber"));
          volumeProgress.set       (resultMap.getDouble("volumeProgress")*100.0);
          totalEntriesProgress.set (getProgress(resultMap.getLong("doneEntries"),resultMap.getLong("totalEntries")));
          totalBytesProgress.set   (getProgress(resultMap.getLong("doneBytes"),resultMap.getLong("totalBytes")));
          requestedVolumeNumber.set(resultMap.getInt("requestedVolumeNumber"));
          message.set              (resultMap.getString("message"));

          widgetButtonStart.setEnabled(   !state.equals("running")
                                       && !state.equals("incremental")
                                       && !state.equals("differential")
                                       && !state.equals("dry-run")
                                       && !state.equals("waiting")
                                       && !state.equals("pause")
                                      );
          widgetButtonAbort.setEnabled(   state.equals("waiting")
                                       || state.equals("running")
                                       || state.equals("incremental")
                                       || state.equals("differential")
                                       || state.equals("dry-run")
                                       || state.equals("request volume")
                                      );
          widgetButtonVolume.setEnabled(state.equals("request volume"));
        }
      });
    }
  }

  /** update status, job list, job data
   */
  void update()
  {
    // update job list
    updateStatus();
    updateJobList();
    updateJobInfo();
  }

  /** start selected job
   */
  private void jobStart()
  {
    int mode;
    int errorCode;

    assert selectedJobData != null;

    errorCode = Errors.UNKNOWN;

    // get job mode
    mode = Dialogs.select(shell,
                          BARControl.tr("Confirmation"),
                          BARControl.tr("Start job ''{0}''?",selectedJobData.name),
                          new String[]{BARControl.tr("Normal"),
                                       BARControl.tr("Full"),
                                       BARControl.tr("Incremental"),
                                       BARControl.tr("Differential"),
                                       BARControl.tr("Dry-run"),
                                       BARControl.tr("Cancel")
                                      },
                          new String[]{BARControl.tr("Store all files."),
                                       BARControl.tr("Store all files and create incremental data file."),
                                       BARControl.tr("Store changed files since last incremental or full storage and update incremental data file."),
                                       BARControl.tr("Store changed files since last full storage."),
                                       BARControl.tr("Collect and process all files, but do not create archives.")
                                      },
                          4
                         );
    if ((mode != 0) && (mode != 1) && (mode != 2) && (mode != 3) && (mode != 4))
    {
      return;
    }

    if (selectedJobData.cryptPasswordMode.equals("ask"))
    {
      // get crypt password
      String password = Dialogs.password(shell,
                                         BARControl.tr("Crypt password"),
                                         null,
                                         BARControl.tr("Crypt password")+":",
                                         BARControl.tr("Verify")+":"
                                        );
      if (password == null)
      {
        return;
      }

      // set crypt password
      String[] resultErrorMessage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("CRYPT_PASSWORD jobUUID=%s encryptType=%s encryptedPassword=%S",
                                                               selectedJobData.uuid,
                                                               BARServer.getPasswordEncryptType(),
                                                               BARServer.encryptPassword(password)
                                                              ),
                                           0,
                                           resultErrorMessage
                                          );
      if (error != Errors.NONE)
      {
        Dialogs.error(shell,BARControl.tr("Cannot start job (error: {0})",resultErrorMessage[0]));
        return;
      }
    }

    // start (ignore error code here; error is reported via state-update)
    switch (mode)
    {
      case 0:
        BARServer.executeCommand(StringParser.format("JOB_START jobUUID=%s type=normal",selectedJobData.uuid),0);
        break;
      case 1:
        BARServer.executeCommand(StringParser.format("JOB_START jobUUID=%s type=full",selectedJobData.uuid),0);
        break;
      case 2:
        BARServer.executeCommand(StringParser.format("JOB_START jobUUID=%s type=incremental",selectedJobData.uuid),0);
        break;
      case 3:
        BARServer.executeCommand(StringParser.format("JOB_START jobUUID=%s type=differential",selectedJobData.uuid),0);
        break;
      case 4:
        BARServer.executeCommand(StringParser.format("JOB_START jobUUID=%s type=dry-run",selectedJobData.uuid),0);
        break;
      case 5:
        break;
    }
  }

  /** abort selected job
   */
  private void jobAbort()
  {
    assert selectedJobData != null;

    if (!selectedJobData.state.equals("running") || Dialogs.confirm(shell,BARControl.tr("Abort running job ''{0}''?",selectedJobData.name),false))
    {
      final BusyDialog busyDialog = new BusyDialog(shell,BARControl.tr("Abort"),300,100,BARControl.tr("Abort job")+" '"+selectedJobData.name+"'...",BusyDialog.TEXT0);
      busyDialog.autoAnimate();

      new BackgroundTask(busyDialog)
      {
        public void run(final BusyDialog busyDialog, Object userData)
        {
          try
          {
            // abort job
            final String[] resultErrorMessage = new String[1];
            int error = BARServer.executeCommand(StringParser.format("JOB_ABORT jobUUID=%s",selectedJobData.uuid),
                                                 0,
                                                 resultErrorMessage
                                                );
            if (error == Errors.NONE)
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  busyDialog.close();
                }
              });
            }
            else
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  busyDialog.close();
                  Dialogs.error(shell,BARControl.tr("Cannot abort job (error: {0})",resultErrorMessage[0]));
                }
              });
            }
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
      String[] resultErrorMessage = new String[1];
      int error = BARServer.executeCommand(StringParser.format("PAUSE time=%d modeMask=%s",
                                                               pauseTime,
                                                               buffer.toString()
                                                              ),
                                           0,
                                           resultErrorMessage
                                          );
      if (error != Errors.NONE)
      {
        Dialogs.error(shell,BARControl.tr("Cannot pause job (error: {0})",resultErrorMessage[0]));
      }
    }
  }

  /** suspend/continue paused jobs
   */
  private void jobSuspendContinue()
  {
    String[] errorMessage = new String[1];
    int      error  = Errors.NONE;
    switch (status)
    {
      case RUNNING: error = BARServer.executeCommand(StringParser.format("SUSPEND") ,0,errorMessage); break;
      case PAUSE:
      case SUSPEND: error = BARServer.executeCommand(StringParser.format("CONTINUE"),0,errorMessage); break;
    }
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot suspend job (error: {0})",errorMessage[0]));
    }
  }

  /** new volume
   */
  private void volume()
  {
    assert selectedJobData != null;

    long     volumeNumber = requestedVolumeNumber.getLong();
    String[] resultErrorMessage = new String[1];
    int      error              = Errors.NONE;
    switch (Dialogs.select(shell,BARControl.tr("Volume request"),BARControl.tr("Load volume number {0}.",volumeNumber),new String[]{BARControl.tr("OK"),BARControl.tr("Unload tray"),BARControl.tr("Cancel")},0))
    {
      case 0:
        error = BARServer.executeCommand(StringParser.format("VOLUME_LOAD jobUUID=%s volumeNumber=%d",selectedJobData.uuid,volumeNumber),0,resultErrorMessage);
        break;
      case 1:
        error = BARServer.executeCommand(StringParser.format("VOLUME_UNLOAD jobUUID=%s",selectedJobData.uuid),0,resultErrorMessage);
        break;
      case 2:
        break;
    }
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,BARControl.tr("Cannot change volume job (error: {0})",resultErrorMessage[0]));
    }
  }

  /** create new job
   */
  private void jobNew()
  {
    assert selectedJobData != null;

    assert selectedJobData != null;

    tabJobs.jobNew();
    updateJobList();
  }

  /** clone selected job
   */
  private void jobClone()
  {
    assert selectedJobData != null;

    assert selectedJobData != null;

    tabJobs.jobClone(selectedJobData);
    updateJobList();
  }

  /** rename selected job
   */
  private void jobRename()
  {
    assert selectedJobData != null;

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

    if (!selectedJobData.state.equals("running"))
    {
      tabJobs.jobDelete(selectedJobData);
      updateJobList();
    }
  }
}

/* end of file */
