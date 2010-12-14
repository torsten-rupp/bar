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
import java.util.LinkedList;

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

/** tab status
 */
class TabStatus
{
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

  /** job data
   */
  class JobData
  {
    int    id;
    String name;
    String state;
    String type;
    long   archivePartSize;
    String compressAlgorithm;
    String cryptAlgorithm;
    String cryptType;
    String cryptPasswordMode;
    long   lastExecutedDateTime;
    long   estimatedRestTime;

    // date/time format
    private final SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

    /** create job data
     */
    JobData(int id, String name, String state, String type, long archivePartSize, String compressAlgorithm, String cryptAlgorithm, String cryptType, String cryptPasswordMode, long lastExecutedDateTime, long estimatedRestTime)
    {
      this.id                   = id;
      this.name                 = name;
      this.state                = state;
      this.type                 = type;
      this.archivePartSize      = archivePartSize;
      this.compressAlgorithm    = compressAlgorithm;
      this.cryptAlgorithm       = cryptAlgorithm;
      this.cryptType            = cryptType;
      this.cryptPasswordMode    = cryptPasswordMode;
      this.lastExecutedDateTime = lastExecutedDateTime;
      this.estimatedRestTime    = estimatedRestTime;
    }

    /** get job crypt algorithm (including "*" for asymmetric)
     * @return crypt algorithm
     */
    String getCryptAlgorithm()
    {
      return cryptAlgorithm+(cryptType.equals("ASYMMETRIC")?"*":"");
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
      return "Job {"+id+", "+name+", "+state+", "+type+"}";
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
          return jobData1.type.compareTo(jobData2.type);
        case SORTMODE_PARTSIZE:
          if      (jobData1.archivePartSize < jobData2.archivePartSize) return -1;
          else if (jobData1.archivePartSize > jobData2.archivePartSize) return  1;
          else                                                          return  0;
        case SORTMODE_COMPRESS:
          return jobData1.compressAlgorithm.compareTo(jobData2.compressAlgorithm);
        case SORTMODE_CRYPT:
          String crypt1 = jobData1.cryptAlgorithm+(jobData1.cryptType.equals("ASYMMETRIC")?"*":"");
          String crypt2 = jobData2.cryptAlgorithm+(jobData2.cryptType.equals("ASYMMETRIC")?"*":"");

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

  // global variable references
  private Shell       shell;
  private Display     display;
  private TabJobs     tabJobs;

  // widgets
  public  Composite   widgetTab;
  private Table       widgetJobList;
  private Shell       widgetMessageToolTip = null;
  private Group       widgetSelectedJob;
  public  Button      widgetButtonStart;
  public  Button      widgetButtonAbort;
  public  Button      widgetButtonPause;
  public  Button      widgetButtonSuspendContinue;
  private Button      widgetButtonVolume;
  public  Button      widgetButtonQuit;

  // BAR variables
  private WidgetVariable doneFiles             = new WidgetVariable(0);
  private WidgetVariable doneBytes             = new WidgetVariable(0);
  private WidgetVariable storedBytes           = new WidgetVariable(0);
  private WidgetVariable skippedFiles          = new WidgetVariable(0);
  private WidgetVariable skippedBytes          = new WidgetVariable(0);
  private WidgetVariable errorFiles            = new WidgetVariable(0);
  private WidgetVariable errorBytes            = new WidgetVariable(0);
  private WidgetVariable totalFiles            = new WidgetVariable(0);
  private WidgetVariable totalBytes            = new WidgetVariable(0);

  private WidgetVariable filesPerSecond        = new WidgetVariable(0.0); 
  private WidgetVariable bytesPerSecond        = new WidgetVariable(0.0);
  private WidgetVariable storageBytesPerSecond = new WidgetVariable(0.0);
  private WidgetVariable ratio                 = new WidgetVariable(0.0);

  private WidgetVariable fileName              = new WidgetVariable("");
  private WidgetVariable fileProgress          = new WidgetVariable(0.0);
  private WidgetVariable storageName           = new WidgetVariable("");
  private WidgetVariable storageProgress       = new WidgetVariable(0.0);
  private WidgetVariable volumeNumber          = new WidgetVariable(0);
  private WidgetVariable volumeProgress        = new WidgetVariable(0.0);
  private WidgetVariable totalFilesProgress    = new WidgetVariable(0.0);
  private WidgetVariable totalBytesProgress    = new WidgetVariable(0.0);
  private WidgetVariable requestedVolumeNumber = new WidgetVariable(0);
  private WidgetVariable message               = new WidgetVariable("");

  // variables
  private HashMap<String,JobData> jobList         = new HashMap<String,JobData> ();
  private JobData                 selectedJobData = null;
  private States                  status          = States.RUNNING;

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
    }

    /** run status update thread
     */
    public void run()
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
        }

        // sleep a short time
        try { Thread.sleep(1000); } catch (InterruptedException exception) { /* ignored */ };
      }
    }
  }

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
    Label       label;
    ProgressBar progressBar;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Status"+((accelerator != 0)?" ("+Widgets.acceleratorToText(accelerator)+")":""));
    widgetTab.setLayout(new TableLayout(new double[]{1.0,0.0,0.0},1.0,2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // list with jobs
    widgetJobList = Widgets.newTable(widgetTab,SWT.NONE);
    Widgets.layout(widgetJobList,0,0,TableLayoutData.NSWE);
    widgetJobList.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Table     widget    = (Table)selectionEvent.widget;
        TabStatus tabStatus = (TabStatus)widget.getData();
        selectedJobData = (JobData)selectionEvent.item.getData();
        if (tabJobs != null) tabJobs.selectJob(selectedJobData.name);
        widgetSelectedJob.setText("Selected '"+selectedJobData.name+"'");
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    SelectionListener jobListColumnSelectionListener = new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        TableColumn       tableColumn       = (TableColumn)selectionEvent.widget;
        JobDataComparator jobDataComparator = new JobDataComparator(widgetJobList,tableColumn);
        synchronized(jobList)
        {
          Widgets.sortTableColumn(widgetJobList,tableColumn,jobDataComparator);
        }
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    };
    tableColumn = Widgets.addTableColumn(widgetJobList,0,"Name",          SWT.LEFT, 110,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn.setToolTipText("Click to sort for name.");
    tableColumn = Widgets.addTableColumn(widgetJobList,1,"State",         SWT.LEFT,  90,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn.setToolTipText("Click to sort for state.");
    tableColumn = Widgets.addTableColumn(widgetJobList,2,"Type",          SWT.LEFT,  90,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn.setToolTipText("Click to sort for type.");
    tableColumn = Widgets.addTableColumn(widgetJobList,3,"Part size",     SWT.RIGHT, 80,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn.setToolTipText("Click to sort for part size.");
    tableColumn = Widgets.addTableColumn(widgetJobList,4,"Compress",      SWT.LEFT,  80,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn.setToolTipText("Click to sort for used compress algorithm.");
    tableColumn = Widgets.addTableColumn(widgetJobList,5,"Crypt",         SWT.LEFT, 100,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn.setToolTipText("Click to sort for used encryption algorithm.");
    tableColumn = Widgets.addTableColumn(widgetJobList,6,"Last executed", SWT.LEFT, 150,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn.setToolTipText("Click to sort for last date/time job was executed.");
    tableColumn = Widgets.addTableColumn(widgetJobList,7,"Estimated time",SWT.LEFT, 120,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn.setToolTipText("Click to sort for estimated rest time to execute job.");

    menu = Widgets.newPopupMenu(shell);
    {
      menuItem = Widgets.addMenuItem(menu,"Start");
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          jobStart();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      menuItem = Widgets.addMenuItem(menu,"Abort");
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          jobAbort();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      menuItem = Widgets.addMenuItem(menu,"Pause");
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          jobPause(60*60);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      menuItem = Widgets.addMenuItem(menu,"Continue");
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          jobSuspendContinue();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      menuItem = Widgets.addMenuItem(menu,"Volume");
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          volume();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }
    widgetJobList.setMenu(menu);
    widgetJobList.setToolTipText("List with job entries.\nClick to select job, right-click to open context menu.");

    // selected job group
    widgetSelectedJob = Widgets.newGroup(widgetTab,"Selected ''",SWT.NONE);
    widgetSelectedJob.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0,1.0,0.0,0.0,1.0,0.0,1.0,1.0},4));
    Widgets.layout(widgetSelectedJob,1,0,TableLayoutData.WE);
    {
      // done files/bytes, files/s, bytes/s
      label = Widgets.newLabel(widgetSelectedJob,"Done:");
      Widgets.layout(label,0,0,TableLayoutData.W);
      label= Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,0,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,doneFiles));
      label = Widgets.newLabel(widgetSelectedJob,"files");
      Widgets.layout(label,0,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,0,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,doneBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,0,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,0,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,0,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,doneBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,0,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,doneBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      composite = Widgets.newComposite(widgetSelectedJob);
      composite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(composite,0,8,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetListener(label,filesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return String.format("%.1f",variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,"files/s");
        Widgets.layout(label,0,1,TableLayoutData.W);
      }

      composite = Widgets.newComposite(widgetSelectedJob);
      composite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0}));
      Widgets.layout(composite,0,9,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetListener(label,bytesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return Units.getByteSize(variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,"bytes/s");
        Widgets.layout(label,0,1,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes/s","KBytes/s","MBytes/s","GBytes/s"}));
        Widgets.addModifyListener(new WidgetListener(label,bytesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return Units.getByteUnit(variable.getDouble())+"/s";
          }
        });
      }

      // stored files/bytes
      label = Widgets.newLabel(widgetSelectedJob,"Stored:");
      Widgets.layout(label,1,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,1,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,storedBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,1,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,1,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,1,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,storedBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,1,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,storedBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      composite = Widgets.newComposite(widgetSelectedJob);
      composite.setLayout(new TableLayout(0.0,new double[]{0.0,1.0,0.0}));
      Widgets.layout(composite,1,8,TableLayoutData.WE);
      {
        label = Widgets.newLabel(composite,"Ratio");
        Widgets.layout(label,0,0,TableLayoutData.W);
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,1,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetListener(label,ratio)
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
      Widgets.layout(composite,1,9,TableLayoutData.WE);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE);
        Widgets.addModifyListener(new WidgetListener(label,storageBytesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return Units.getByteSize(variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,"bytes/s");        
        Widgets.layout(label,0,1,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes/s","KBytes/s","MBytes/s","GBytes/s"}));
        Widgets.addModifyListener(new WidgetListener(label,storageBytesPerSecond)
        {
          public String getString(WidgetVariable variable)
          {
            return Units.getByteUnit(variable.getDouble())+"/s";
          }
        });
      }

      // skipped files/bytes, ratio
      label = Widgets.newLabel(widgetSelectedJob,"Skipped:");
      Widgets.layout(label,2,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,2,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,skippedFiles));
      label = Widgets.newLabel(widgetSelectedJob,"files");
      Widgets.layout(label,2,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,2,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,skippedBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,2,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,2,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,2,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,skippedBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,2,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,skippedBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // error files/bytes
      label = Widgets.newLabel(widgetSelectedJob,"Errors:");
      Widgets.layout(label,3,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,errorFiles));
      label = Widgets.newLabel(widgetSelectedJob,"files");
      Widgets.layout(label,3,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,errorBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,3,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,3,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,errorBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,3,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,errorBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // total files/bytes
      label = Widgets.newLabel(widgetSelectedJob,"Total:");
      Widgets.layout(label,4,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,1,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,totalFiles));
      label = Widgets.newLabel(widgetSelectedJob,"files");
      Widgets.layout(label,4,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,3,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,totalBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,4,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,4,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,6,TableLayoutData.WE);
      Widgets.addModifyListener(new WidgetListener(label,totalBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,4,7,TableLayoutData.W,0,0,0,0,Widgets.getTextSize(label,new String[]{"bytes","KBytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,totalBytes)
      {
        public String getString(WidgetVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // current file, file percentage
      label = Widgets.newLabel(widgetSelectedJob,"File:");
      Widgets.layout(label,5,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,5,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetListener(label,fileName));
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,6,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,fileProgress));

      // storage file, storage percentage
      label = Widgets.newLabel(widgetSelectedJob,"Storage:");
      Widgets.layout(label,7,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,7,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetListener(label,storageName));
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,8,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,storageProgress));

      // volume percentage
      label = Widgets.newLabel(widgetSelectedJob,"Volume:");
      Widgets.layout(label,9,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,9,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,volumeProgress));

      // total files percentage
      label = Widgets.newLabel(widgetSelectedJob,"Total files:");
      Widgets.layout(label,10,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,10,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,totalFilesProgress));

      // total bytes percentage
      label = Widgets.newLabel(widgetSelectedJob,"Total bytes:");
      Widgets.layout(label,11,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob);
      Widgets.layout(progressBar,11,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,totalBytesProgress));

      // message
      label = Widgets.newLabel(widgetSelectedJob,"Message:");
      Widgets.layout(label,12,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,12,1,TableLayoutData.WE,0,9);
      Widgets.addModifyListener(new WidgetListener(label,message));
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
      widgetButtonStart = Widgets.newButton(composite,null,"Start");
      Widgets.layout(widgetButtonStart,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);
      widgetButtonStart.setEnabled(false);
      widgetButtonStart.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobStart();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetButtonStart.setToolTipText("Start selected job.");

      widgetButtonAbort = Widgets.newButton(composite,null,"Abort");
      Widgets.layout(widgetButtonAbort,0,1,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);
      widgetButtonAbort.setEnabled(false);
      widgetButtonAbort.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobAbort();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetButtonAbort.setToolTipText("Abort selected job.");

      widgetButtonPause = Widgets.newButton(composite,null,"Pause");
      Widgets.layout(widgetButtonPause,0,2,TableLayoutData.W,0,0,0,0,120,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT); // how to calculate correct min. width? ,0,0,Widgets.getTextSize(widgetButtonSuspendContinue,new String[]{"Puase [xxxxs]"}));
      widgetButtonPause.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobPause(60*60);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetButtonPause.setToolTipText("Pause selected job for a specific time.");

      widgetButtonSuspendContinue = Widgets.newButton(composite,null,"Continue");
      Widgets.layout(widgetButtonSuspendContinue,0,3,TableLayoutData.W,0,0,0,0,80,SWT.DEFAULT,SWT.DEFAULT,SWT.DEFAULT); // how to calculate correct min. width? ,0,0,Widgets.getTextSize(widgetButtonSuspendContinue,new String[]{"Suspend","Continue"}));
      widgetButtonSuspendContinue.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          jobSuspendContinue();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetButtonSuspendContinue.setToolTipText("Suspend selected job for an infinite time.");

      widgetButtonVolume = Widgets.newButton(composite,null,"Volume");
      Widgets.layout(widgetButtonVolume,0,4,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);
      widgetButtonVolume.setEnabled(false);
      widgetButtonVolume.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          volume();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetButtonVolume.setToolTipText("Click when a new volume is available in drive.");

      widgetButtonQuit = Widgets.newButton(composite,null,"Quit");
      Widgets.layout(widgetButtonQuit,0,5,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      widgetButtonQuit.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          // send close-evemnt to shell
          Event event = new Event();
          shell.notifyListeners(SWT.Close,event);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetButtonQuit.setToolTipText("Quit BARControl program.");
    }

    // start status update thread
    TabStatusUpdateThread tabStatusUpdateThread = new TabStatusUpdateThread(this);
    tabStatusUpdateThread.setDaemon(true);
    tabStatusUpdateThread.start();

    // get initial job list
    updateJobList();
  }

  /** set jobs tab
   * @param tabJobs jobs tab
   */
  void setTabJobs(TabJobs tabJobs)
  {
    this.tabJobs = tabJobs;
  }

  void selectJob(String name)
  {
    synchronized(jobList)
    {
      selectedJobData = jobList.get(name);
      widgetSelectedJob.setText("Selected '"+((selectedJobData != null)?selectedJobData.name:"")+"'");
    }
  }

  //-----------------------------------------------------------------------

  /** getProgress
   * @param n,m process current/max. value
   * @return progress value (in %)
   */
  private double getProgress(long n, long m)
  {
    return (m > 0)?((double)n*100.0)/(double)m:0.0;
  }

  /** update status
   */
  private void updateStatus()
  {
    // get status
    String[] result = new String[1];
    if (BARServer.executeCommand("STATUS",result) != Errors.NONE) return;

    Object[] data = new Object[1];
    if      (StringParser.parse(result[0],"pause %ld",data,StringParser.QUOTE_CHARS))
    {
      final long pauseTime = (Long)data[0];

      status = States.PAUSE;
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetButtonPause.setText(String.format("Pause [%3dmin]",(pauseTime > 0)?(pauseTime+59)/60:1));
          widgetButtonSuspendContinue.setText("Continue");
        }
      });
    }
    else if (StringParser.parse(result[0],"suspended",data,StringParser.QUOTE_CHARS))
    {
      status = States.SUSPEND;
      display.syncExec(new Runnable()
      {
        public void run()
        {
          widgetButtonPause.setText("Pause");
          widgetButtonSuspendContinue.setText("Continue");
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
          widgetButtonPause.setText("Pause");
          widgetButtonSuspendContinue.setText("Suspend");
        }
      });
    }
  }

  /** find index of table item for job
   * @param tableItems table items
   * @param id job id to find
   * @return table item or null if not found
   */
  private int getTableItemIndex(TableItem[] tableItems, int id)
  {
    for (int z = 0; z < tableItems.length; z++)
    {
      if (((JobData)tableItems[z].getData()).id == id) return z;
    }

    return -1;
  }

  /** find index for insert of job data in sorted job list
   * @param jobData job data
   * @return index in job table
   */
  private int findJobListIndex(JobData jobData)
  {
    TableItem         tableItems[]      = widgetJobList.getItems();
    JobDataComparator jobDataComparator = new JobDataComparator(widgetJobList);

    int index = 0;
    while (   (index < tableItems.length)
           && (jobDataComparator.compare(jobData,(JobData)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update job list
   */
  private void updateJobList()
  {
    if (!widgetJobList.isDisposed())
    {
      // get job list
      final ArrayList<String> result = new ArrayList<String>();
      if (BARServer.executeCommand("JOB_LIST",result) != Errors.NONE) return;

      display.syncExec(new Runnable()
      {
        public void run()
        {
          // update entries in job list
          synchronized(jobList)
          {
            jobList.clear();
            TableItem[] tableItems     = widgetJobList.getItems();
            boolean[]   tableItemFlags = new boolean[tableItems.length];
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
                int    id                   = (Integer)data[0];
                String name                 = (String )data[1];
                String state                = (String )data[2];
                String type                 = (String )data[3];
                long   archivePartSize      = (Long   )data[4];
                String compressAlgorithm    = (String )data[5];
                String cryptAlgorithm       = (String )data[6];
                String cryptType            = (String )data[7];
                String cryptPasswordMode    = (String )data[8];
                long   lastExecutedDateTime = (Long   )data[9];
                long   estimatedRestTime    = (Long   )data[10];

                // get/create table item
                TableItem tableItem;
                JobData   jobData = new JobData(id,
                                                name,
                                                state,
                                                type,
                                                archivePartSize,
                                                compressAlgorithm,
                                                cryptAlgorithm,
                                                cryptType,
                                                cryptPasswordMode,
                                                lastExecutedDateTime,
                                                estimatedRestTime
                                               );
                int index = getTableItemIndex(tableItems,id);
                if (index >= 0)
                {
                  tableItem = tableItems[index];
                  tableItemFlags[index] = true;
                }
                else
                {
                  tableItem = new TableItem(widgetJobList,SWT.NONE,findJobListIndex(jobData));
                }
                tableItem.setData(jobData);

                jobList.put(name,jobData);
                tableItem.setText(0,jobData.name);
                tableItem.setText(1,(status == States.RUNNING)?jobData.state:"suspended");
                tableItem.setText(2,jobData.type);
                tableItem.setText(3,(jobData.archivePartSize > 0)?Units.formatByteSize(jobData.archivePartSize):"unlimited");
                tableItem.setText(4,jobData.compressAlgorithm);
                tableItem.setText(5,jobData.getCryptAlgorithm());
                tableItem.setText(6,jobData.formatLastExecutedDateTime());
                tableItem.setText(7,jobData.formatEstimatedRestTime());
              }
            }

            // remove not existing entries
            for (int z = 0; z < tableItems.length; z++)
            {
              if (!tableItemFlags[z]) Widgets.removeTableEntry(widgetJobList,tableItems);
            }
          }
        }
      });
    }
  }

  /** update job information
   */
  private void updateJobInfo()
  {
    if (selectedJobData != null)
    {
      // get job info
      final String result[] = new String[1];
      if (BARServer.executeCommand(String.format("JOB_INFO %d",selectedJobData.id),result) != Errors.NONE) return;

      display.syncExec(new Runnable()
      {
        public void run()
        {
          // update job info
          Object data[] = new Object[24];
          /* format:
            <state>
            <error>
            <doneFiles>
            <doneBytes>
            <totalFiles>
            <totalBytes>
            <skippedFiles>
            <skippedBytes>
            <errorFiles>
            <errorBytes>
            <filesPerSecond>
            <bytesPerSecond>
            <storageBytesPerSecond>
            <archiveBytes>
            <ratio \
            <fileName>
            <fileDoneBytes>
            <fileTotalBytes>
            <storageName>
            <storageDoneBytes>
            <storageTotalBytes>
            <volumeNumber>
            <volumeProgress>
            <requestedVolumeNumber\>
          */
          if (StringParser.parse(result[0],"%S %S %ld %ld %ld %ld %ld %ld %ld %ld %f %f %f %ld %f %S %ld %ld %S %ld %ld %ld %f %d",data,StringParser.QUOTE_CHARS))
          {
             String state = (String)data[0];

             doneFiles.set            ((Long  )data[ 2]);
             doneBytes.set            ((Long  )data[ 3]);
             storedBytes.set          ((Long  )data[20]);
             skippedFiles.set         ((Long  )data[ 6]);
             skippedBytes.set         ((Long  )data[ 7]);
             errorFiles.set           ((Long  )data[ 8]);
             errorBytes.set           ((Long  )data[ 9]);
             totalFiles.set           ((Long  )data[ 4]);
             totalBytes.set           ((Long  )data[ 5]);

             filesPerSecond.set       ((Double)data[10]);
             bytesPerSecond.set       ((Double)data[11]);
             storageBytesPerSecond.set((Double)data[12]);
//             archiveBytes.set((Long)data[13]);
             ratio.set                ((Double)data[14]);

             fileName.set             ((String)data[15]);
             fileProgress.set         (getProgress((Long)data[16],(Long)data[17]));
             storageName.set          ((String)data[18]);
             storageProgress.set      (getProgress((Long)data[19],(Long)data[20]));
             volumeNumber.set         ((Long  )data[21]);
             volumeProgress.set       ((Double)data[22]*100.0);
             totalFilesProgress.set   (getProgress((Long)data[ 2],(Long)data[ 4]));
             totalBytesProgress.set   (getProgress((Long)data[ 3],(Long)data[ 5]));
             requestedVolumeNumber.set((Integer)data[23]);
             message.set              ((String)data[ 1]);

             widgetButtonStart.setEnabled(!state.equals("running") && !state.equals("waiting") && !state.equals("pause"));
             widgetButtonAbort.setEnabled(state.equals("waiting") || state.equals("running")  || state.equals("request volume"));
             widgetButtonVolume.setEnabled(state.equals("request volume"));
          }
//          else { Dprintf.dprintf("unexecpted "+result[0]); }
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
                          "Confirmation","Start job '"+selectedJobData.name+"'?",
                          new String[]{"Normal","Full","Incremental","Differential","Dry-run","Cancel"},
                          new String[]{"Store all files.","Store all files and create incremental data file.","Store changed files since last incremental or full storage and update incremental data file.","Store changed files since last full storage.","Collect and process all files, but do not create archives."},
                          4
                         );
    if ((mode != 0) && (mode != 1) && (mode != 2) && (mode != 3))
    {
      return;
    }

    if (selectedJobData.cryptPasswordMode.equals("ask"))
    {
      // set crypt password
      String password = Dialogs.password(shell,
                                         "Crypt password",
                                         null,
                                         "Crypt password:",
                                         "Verify:"
                                        );
      if (password == null)
      {
        return;
      }

      errorCode = BARServer.executeCommand("CRYPT_PASSWORD "+selectedJobData.id+" "+StringUtils.escape(password));
    }

    // start
    switch (mode)
    {
      case 0:
        errorCode = BARServer.executeCommand("JOB_START "+selectedJobData.id+" normal");
        break;
      case 1:
        errorCode = BARServer.executeCommand("JOB_START "+selectedJobData.id+" full");
        break;
      case 2:
        errorCode = BARServer.executeCommand("JOB_START "+selectedJobData.id+" incremental");
        break;
      case 3:
        errorCode = BARServer.executeCommand("JOB_START "+selectedJobData.id+" differential");
        break;
      case 4:
        errorCode = BARServer.executeCommand("JOB_START "+selectedJobData.id+" dry-run");
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

    if (Dialogs.confirm(shell,"Abort job '"+selectedJobData.name+"'?",false))
    {
      String[] result = new String[1];
      if (BARServer.executeCommand("JOB_ABORT "+selectedJobData.id,result) != Errors.NONE)
      {
        Dialogs.error(shell,"Cannot abort job (error: %s)",result[0]);
      }
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
      String[] result = new String[1];
      if (BARServer.executeCommand("PAUSE "+pauseTime+" "+buffer.toString(),result) != Errors.NONE)
      {
        Dialogs.error(shell,"Cannot pause job (error: %s)",result[0]);
      }
    }
  }

  /** suspend/continue paused jobs
   */
  private void jobSuspendContinue()
  {
    String[] result = new String[1];
    int      error  = Errors.NONE;   
    switch (status)
    {
      case RUNNING: error = BARServer.executeCommand("SUSPEND" ,result); break;
      case PAUSE:
      case SUSPEND: error = BARServer.executeCommand("CONTINUE",result); break;
    }
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,"Cannot suspend job (error: %s)",result[0]);
    }
  }

  /** new volume
   */
  private void volume()
  {
    assert selectedJobData != null;

    long     volumeNumber = requestedVolumeNumber.getLong();
    String[] result       = new String[1];
    int      error        = Errors.NONE;   
    switch (Dialogs.select(shell,"Volume request","Load volume number "+volumeNumber+".",new String[]{"OK","Unload tray","Cancel"},0))
    {
      case 0:
        error = BARServer.executeCommand("VOLUME_LOAD "+selectedJobData.id+" "+volumeNumber,result);
        break;
      case 1:
        error = BARServer.executeCommand("VOLUME_UNLOAD "+selectedJobData.id,result);
        break;
      case 2:
        break;
    }
    if (error != Errors.NONE)
    {
      Dialogs.error(shell,"Cannot change volume job (error: %s)",result[0]);
    }
  }
}

/* end of file */
