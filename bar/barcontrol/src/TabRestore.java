/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TabRestore.java,v $
* $Revision: 1.14 $
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
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

// graphics
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Cursor;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
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
  /** file types
   */
  enum FileTypes
  {
    FILE,
    DIRECTORY,
    LINK,
    SPECIAL,
    DEVICE,
    SOCKET
  };

  /** archive file tree data
   */
  class ArchiveFileTreeData
  {
    String    name;
    FileTypes type;
    long      size;
    long      datetime;
    String    title;

    /** create archive file tree data
     * @param name name
     * @param type file type
     * @param size size [bytes]
     * @param datetime date/time (timestamp)
     * @param title title to show
     */
    ArchiveFileTreeData(String name, FileTypes type, long size, long datetime, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = size;
      this.datetime = datetime;
      this.title    = title;
    }

    /** create archive file tree data
     * @param name name
     * @param type file type
     * @param datetime date/time (timestamp)
     * @param title title to show
     */
    ArchiveFileTreeData(String name, FileTypes type, long datetime, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = 0;
      this.datetime = datetime;
      this.title    = title;
    }

    /** create archive file tree data
     * @param name name
     * @param type file type
     * @param title title to show
     */
    ArchiveFileTreeData(String name, FileTypes type, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = 0;
      this.datetime = 0;
      this.title    = title;
    }

    /** convert to string
     * @return string
     */
    public String toString()
    {
      return "File {"+name+", "+type+", "+size+" bytes, datetime="+datetime+", title="+title+"}";
    }
  };

  /** file data comparator
   */
  class ArchiveFileTreeDataComparator implements Comparator<ArchiveFileTreeData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_NAME     = 0;
    private final static int SORTMODE_TYPE     = 1;
    private final static int SORTMODE_SIZE     = 2;
    private final static int SORTMODE_DATETIME = 3;

    private int sortMode;

    /** create file data comparator
     * @param tree file tree
     */
    ArchiveFileTreeDataComparator(Tree tree)
    {
      TreeColumn sortColumn = tree.getSortColumn();

      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (tree.getColumn(3) == sortColumn) sortMode = SORTMODE_DATETIME;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** compare file tree data without take care about type
     * @param archiveFileTreeData1, archiveFileTreeData2 file tree data to compare
     * @return -1 iff archiveFileTreeData1 < archiveFileTreeData2,
                0 iff archiveFileTreeData1 = archiveFileTreeData2, 
                1 iff archiveFileTreeData1 > archiveFileTreeData2
     */
    private int compareWithoutType(ArchiveFileTreeData archiveFileTreeData1, ArchiveFileTreeData archiveFileTreeData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return archiveFileTreeData1.title.compareTo(archiveFileTreeData2.title);
        case SORTMODE_TYPE:
          return archiveFileTreeData1.type.compareTo(archiveFileTreeData2.type);
        case SORTMODE_SIZE:
          if      (archiveFileTreeData1.size < archiveFileTreeData2.size) return -1;
          else if (archiveFileTreeData1.size > archiveFileTreeData2.size) return  1;
          else                                                            return  0;
        case SORTMODE_DATETIME:
          if      (archiveFileTreeData1.datetime < archiveFileTreeData2.datetime) return -1;
          else if (archiveFileTreeData1.datetime > archiveFileTreeData2.datetime) return  1;
          else                                                                    return  0;
        default:
          return 0;
      }
    }

    /** compare file tree data
     * @param archiveFileTreeData1, archiveFileTreeData2 file tree data to compare
     * @return -1 iff archiveFileTreeData1 < archiveFileTreeData2,
                0 iff archiveFileTreeData1 = archiveFileTreeData2, 
                1 iff archiveFileTreeData1 > archiveFileTreeData2
     */
    public int compare(ArchiveFileTreeData archiveFileTreeData1, ArchiveFileTreeData archiveFileTreeData2)
    {
      if (archiveFileTreeData1.type == FileTypes.DIRECTORY)
      {
        if (archiveFileTreeData2.type == FileTypes.DIRECTORY)
        {
          return compareWithoutType(archiveFileTreeData1,archiveFileTreeData2);
        }
        else
        {
          return -1;
        }
      }
      else
      {
        if (archiveFileTreeData2.type == FileTypes.DIRECTORY)
        {
          return 1;
        }
        else
        {
          return compareWithoutType(archiveFileTreeData1,archiveFileTreeData2);
        }
      }
    }

    public String toString()
    {
      return "FileComparator {"+sortMode+"}";
    }
  }

  /** file data
   */
  class FileData
  {
    String    archiveName;
    String    name;
    FileTypes type;
    long      size;
    long      datetime;

    /** create file data
     * @param archiveName archive name
     * @param name file name
     * @param type file type
     * @param size size [bytes]
     * @param datetime date/time (timestamp)
     */
    FileData(String archiveName, String name, FileTypes type, long size, long datetime)
    {
      this.archiveName = archiveName;
      this.name        = name;
      this.type        = type;
      this.size        = size;
      this.datetime    = datetime;
    }

    /** create file data
     * @param archiveName archive name
     * @param name file name
     * @param type file type
     * @param datetime date/time (timestamp)
     */
    FileData(String archiveName, String name, FileTypes type, long datetime)
    {
      this.archiveName = archiveName;
      this.name        = name;
      this.type        = type;
      this.size        = 0;
      this.datetime    = datetime;
    }

    /** convert to string
     * @return string
     */
    public String toString()
    {
      return "File {"+archiveName+", "+name+", "+type+", "+size+" bytes, datetime="+datetime+"}";
    }
  };

  /** file data comparator
   */
  class FileDataComparator implements Comparator<FileData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_ARCHIVE = 0;
    private final static int SORTMODE_NAME    = 1;
    private final static int SORTMODE_TYPE    = 2;
    private final static int SORTMODE_SIZE    = 3;
    private final static int SORTMODE_DATE    = 4;

    private int sortMode;

    /** create file data comparator
     * @param table file table
     * @param sortColumn sorting column
     */
    FileDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_ARCHIVE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_DATE;
      else                                       sortMode = SORTMODE_NAME;
    }

    /** create file data comparator
     * @param table file table
     */
    FileDataComparator(Table table)
    {
      TableColumn sortColumn = table.getSortColumn();

      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_ARCHIVE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_DATE;
      else                                       sortMode = SORTMODE_NAME;
    }
    /** compare file data
     * @param fileData1, fileData2 file tree data to compare
     * @return -1 iff fileData1 < fileData2,
                0 iff fileData1 = fileData2, 
                1 iff fileData1 > fileData2
     */
    public int compare(FileData fileData1, FileData fileData2)
    {
      switch (sortMode)
      {
        case SORTMODE_ARCHIVE:
          return fileData1.archiveName.compareTo(fileData2.archiveName);
        case SORTMODE_NAME:
          return fileData1.name.compareTo(fileData2.name);
        case SORTMODE_TYPE:
          return fileData1.type.compareTo(fileData2.type);
        case SORTMODE_SIZE:
          if      (fileData1.size < fileData2.size) return -1;
          else if (fileData1.size > fileData2.size) return  1;
          else                                      return  0;
        case SORTMODE_DATE:
          if      (fileData1.datetime < fileData2.datetime) return -1;
          else if (fileData1.datetime > fileData2.datetime) return  1;
          else                                              return  0;
        default:
          return 0;
      }
    }
  }

  // --------------------------- constants --------------------------------
  // colors
  private final Color COLOR_WHITE;
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

  private final Image IMAGE_MARK_ALL;
  private final Image IMAGE_UNMARK_ALL;

  // date/time format
  private final SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  // cursors
  private final Cursor         waitCursor;

  // --------------------------- variables --------------------------------

  // global variable references
  private Shell                shell;
  private Display              display;

  // widgets
  public  Composite            widgetTab;
  private TabFolder            widgetTabFolder;
  private Combo                widgetPath;
  private Tree                 widgetArchiveFileTree;
  private Button               widgetListButton;
  private Combo                widgetFilePattern;
  private Table                widgetFileList;
  private Button               widgetRestoreButton;
  private Button               widgetRestoreTo;
  private Text                 widgetRestoreToDirectory;
  private Button               widgetRestoreToSelectButton;
  private Button               widgetOverwriteFiles;

  private String               archivePathName = "";
  private ArchiveNameParts     archivePathNameParts;

  private Pattern              filePattern        = null;
  private boolean              newestFileOnlyFlag = true;
  private LinkedList<FileData> fileList           = new LinkedList<FileData>();

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
    COLOR_WHITE    = shell.getDisplay().getSystemColor(SWT.COLOR_WHITE);
    COLOR_MODIFIED = new Color(null,0xFF,0xA0,0xA0);

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

    IMAGE_MARK_ALL           = Widgets.loadImage(display,"mark.gif");
    IMAGE_UNMARK_ALL         = Widgets.loadImage(display,"unmark.gif");

    // get cursors
    waitCursor = new Cursor(display,SWT.CURSOR_WAIT);

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Restore"+((accelerator != 0)?" ("+Widgets.acceleratorToText(accelerator)+")":""));
    widgetTab.setLayout(new TableLayout(new double[]{0,0.5,0,0.5,0.0,0.0},1.0,2));
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE);

    // path
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
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

    // archives tree
    widgetArchiveFileTree = Widgets.newTree(widgetTab,SWT.CHECK);
    Widgets.layout(widgetArchiveFileTree,1,0,TableLayoutData.NSWE);
    SelectionListener filesTreeColumnSelectionListener = new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        TreeColumn                    treeColumn = (TreeColumn)selectionEvent.widget;
        ArchiveFileTreeDataComparator archiveFileTreeDataComparator = new ArchiveFileTreeDataComparator(widgetArchiveFileTree);
        synchronized(widgetArchiveFileTree)
        {
          Widgets.sortTreeColumn(widgetArchiveFileTree,treeColumn,archiveFileTreeDataComparator);
        }
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    };
    treeColumn = Widgets.addTreeColumn(widgetArchiveFileTree,"Name",    SWT.LEFT, 500,true);
    treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
    treeColumn = Widgets.addTreeColumn(widgetArchiveFileTree,"Type",    SWT.LEFT,  50,false);
    treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
    treeColumn = Widgets.addTreeColumn(widgetArchiveFileTree,"Size",    SWT.RIGHT,100,false);
    treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
    treeColumn = Widgets.addTreeColumn(widgetArchiveFileTree,"Modified",SWT.LEFT, 100,false);
    treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
    widgetArchiveFileTree.addListener(SWT.Expand,new Listener()
    {
      public void handleEvent(final Event event)
      {
        TreeItem treeItem = (TreeItem)event.item;

        updateArchiveFilesTree(treeItem);
      }
    });
    widgetArchiveFileTree.addListener(SWT.Collapse,new Listener()
    {
      public void handleEvent(final Event event)
      {
        TreeItem treeItem = (TreeItem)event.item;
        treeItem.removeAll();
        new TreeItem(treeItem,SWT.NONE);
      }
    });
    widgetArchiveFileTree.addListener(SWT.MouseDoubleClick,new Listener()
    {
      public void handleEvent(final Event event)
      {
        TreeItem treeItem = widgetArchiveFileTree.getItem(new Point(event.x,event.y));
        if (treeItem != null)
        {
          ArchiveFileTreeData archiveFileTreeData = (ArchiveFileTreeData)treeItem.getData();
          switch (archiveFileTreeData.type)
          {
            case FILE:
              treeItem.setChecked(!treeItem.getChecked());
              widgetListButton.setEnabled(checkArchiveFileSelected());
              break;
            case DIRECTORY:
              Event treeEvent = new Event();
              treeEvent.item = treeItem;
              if (treeItem.getExpanded())
              {
                widgetArchiveFileTree.notifyListeners(SWT.Collapse,treeEvent);
                treeItem.setExpanded(false);
              }
              else
              {
                widgetArchiveFileTree.notifyListeners(SWT.Expand,treeEvent);
                treeItem.setExpanded(true);
              }
              break;
            default:
              break;
          }
        }
      }
    });
    widgetArchiveFileTree.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        widgetListButton.setEnabled(checkArchiveFileSelected());
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    // list
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,new double[]{1.0,0.0,0.0}));
    Widgets.layout(composite,2,0,TableLayoutData.WE);
    {
      widgetListButton = Widgets.newButton(composite,"List");
      widgetListButton.setEnabled(false);
      Widgets.layout(widgetListButton,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);
      widgetListButton.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          listArchiveFiles(getSelectedArchiveNames(),
                           widgetFilePattern.getText()
                          );
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,IMAGE_MARK_ALL);
      Widgets.layout(button,0,1,TableLayoutData.E);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          setSelectedArchiveNames(true);
          widgetListButton.setEnabled(checkArchiveFileSelected());
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,IMAGE_UNMARK_ALL);
      Widgets.layout(button,0,2,TableLayoutData.E);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          setSelectedArchiveNames(false);
          widgetListButton.setEnabled(checkArchiveFileSelected());
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // file list
    widgetFileList = Widgets.newTable(widgetTab,SWT.CHECK);
    Widgets.layout(widgetFileList,3,0,TableLayoutData.NSWE);
    SelectionListener fileListColumnSelectionListener = new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        TableColumn        tableColumn = (TableColumn)selectionEvent.widget;
        FileDataComparator fileDataComparator = new FileDataComparator(widgetFileList,tableColumn);
        synchronized(widgetFileList)
        {
          shell.setCursor(waitCursor);
          Widgets.sortTableColumn(widgetFileList,tableColumn,fileDataComparator);
          shell.setCursor(null);
        }
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    };
    tableColumn = Widgets.addTableColumn(widgetFileList,0,"Archive",       SWT.LEFT, 200,true );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,0,"Name",          SWT.LEFT, 200,true );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,1,"Type",          SWT.LEFT,  60,true );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,2,"Size",          SWT.RIGHT, 60,true );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,3,"Date",          SWT.LEFT, 140,true );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    widgetFileList.addListener(SWT.MouseDoubleClick,new Listener()
    {
      public void handleEvent(final Event event)
      {
        TableItem tableItem = widgetFileList.getItem(new Point(event.x,event.y));
        if (tableItem != null)
        {
          tableItem.setChecked(!tableItem.getChecked());
          widgetRestoreButton.setEnabled(checkFilesSelected());
        }
      }
    });
    widgetFileList.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        widgetRestoreButton.setEnabled(checkFilesSelected());
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0,0.0,0.0,0.0,0.0}));
    Widgets.layout(composite,4,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,"Filter:");
      Widgets.layout(label,0,0,TableLayoutData.W);
      widgetFilePattern = Widgets.newCombo(composite);
      Widgets.layout(widgetFilePattern,0,1,TableLayoutData.WE);
      widgetFilePattern.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Combo  widget = (Combo)selectionEvent.widget;
          setFilePattern(widget.getText());
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo widget = (Combo)selectionEvent.widget;
          setFilePattern(widget.getText());
        }
      });
      widgetFilePattern.addFocusListener(new FocusListener()
      {
        public void focusGained(FocusEvent focusEvent)
        {
        }
        public void focusLost(FocusEvent focusEvent)
        {
          Combo  widget = (Combo)focusEvent.widget;
          setFilePattern(widget.getText());
        }
      });

      button = Widgets.newCheckbox(composite,"newest file only");
      button.setSelection(newestFileOnlyFlag);
      Widgets.layout(button,0,2,TableLayoutData.W);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button  widget = (Button)selectionEvent.widget;
          newestFileOnlyFlag = widget.getSelection();
          updateFileList();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      control = Widgets.newSpacer(composite);
      Widgets.layout(control,0,3,TableLayoutData.WE,0,0,30,0);

      button = Widgets.newButton(composite,IMAGE_MARK_ALL);
      Widgets.layout(button,0,4,TableLayoutData.E);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          setSelectedFiles(true);
          widgetRestoreButton.setEnabled(checkFilesSelected());
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,IMAGE_UNMARK_ALL);
      Widgets.layout(button,0,5,TableLayoutData.E);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          setSelectedFiles(false);
          widgetRestoreButton.setEnabled(checkFilesSelected());
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // restore
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,0.0,1.0,0.0,0.0}));
    Widgets.layout(composite,5,0,TableLayoutData.WE);
    {
      widgetRestoreButton = Widgets.newButton(composite,"Restore");
      widgetRestoreButton.setEnabled(false);
      Widgets.layout(widgetRestoreButton,0,0,TableLayoutData.DEFAULT,0,0,0,0,60,SWT.DEFAULT);
      widgetRestoreButton.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          restoreFiles(getSelectedFiles(),
                       widgetRestoreTo.getSelection()?widgetRestoreToDirectory.getText():"",
                       widgetOverwriteFiles.getSelection()
                      );
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      widgetRestoreTo = Widgets.newCheckbox(composite,"to");
      Widgets.layout(widgetRestoreTo,0,1,TableLayoutData.W);
      widgetRestoreTo.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button  widget      = (Button)selectionEvent.widget;
          boolean checkedFlag = widget.getSelection();
          widgetRestoreTo.setEnabled(checkedFlag);
          widgetRestoreToSelectButton.setEnabled(checkedFlag);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      widgetRestoreToDirectory = Widgets.newText(composite);
      widgetRestoreToDirectory.setEnabled(false);
      Widgets.layout(widgetRestoreToDirectory,0,2,TableLayoutData.WE);

      widgetRestoreToSelectButton = Widgets.newButton(composite,IMAGE_DIRECTORY);
      Widgets.layout(widgetRestoreToSelectButton,0,3,TableLayoutData.DEFAULT);
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

      widgetOverwriteFiles = Widgets.newCheckbox(composite,"overwrite existing files");
      Widgets.layout(widgetOverwriteFiles,0,4,TableLayoutData.W);
/*
      widgetOverwriteFiles.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button  widget        = (Button)selectionEvent.widget;
          boolean overwriteFlag = widget.getSelection();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
*/
    }

    // update data
    updateArchivePathList();
    setArchivePath("/");
  }

  //-----------------------------------------------------------------------

  /** set archive name in tree widget
   * @param newArchivePathName new archive path name
   */
  private void setArchivePath(String newArchivePathName)
  {
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
          widgetArchiveFileTree.removeAll();

          treeItem = Widgets.addTreeItem(widgetArchiveFileTree,
                                         new ArchiveFileTreeData(archivePathName,
                                                                 FileTypes.DIRECTORY,
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

            widgetArchiveFileTree.removeAll();

            treeItem = Widgets.addTreeItem(widgetArchiveFileTree,
                                           new ArchiveFileTreeData(pathName,
                                                                   FileTypes.DIRECTORY,
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
  }

  /** update archive path list
   */
  private void updateArchivePathList()
  {
    if (!widgetPath.isDisposed())
    {
      // get job list
      ArrayList<String> result = new ArrayList<String>();
      if (BARServer.executeCommand("JOB_LIST",result) != 0) return;

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

  /** find index for insert of tree item in sorted list of tree items
   * @param treeItem tree item
   * @param archiveFileTreeData data of tree item
   * @return index in tree item
   */
  private int findArchiveFilesTreeIndex(TreeItem treeItem, ArchiveFileTreeData archiveFileTreeData)
  {
    TreeItem                      subTreeItems[] = treeItem.getItems();
    ArchiveFileTreeDataComparator archiveFileTreeDataComparator = new ArchiveFileTreeDataComparator(widgetArchiveFileTree);

    int index = 0;
    while (   (index < subTreeItems.length)
           && (archiveFileTreeDataComparator.compare(archiveFileTreeData,(ArchiveFileTreeData)subTreeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update file list of tree item
   * @param treeItem tree item to update
   */
  private void updateArchiveFilesTree(TreeItem treeItem)
  {
    ArchiveFileTreeData archiveFileTreeData = (ArchiveFileTreeData)treeItem.getData();

    shell.setCursor(waitCursor);
    BusyDialog busyDialog = new BusyDialog(shell,"List archives",500,100);

    treeItem.removeAll();

    new BackgroundTask(busyDialog,new Object[]{treeItem,archiveFileTreeData})
    {
      public void run(final BusyDialog busyDialog, Object userData)
      {
        final TreeItem            treeItem            = (TreeItem           )((Object[])userData)[0];
        final ArchiveFileTreeData archiveFileTreeData = (ArchiveFileTreeData)((Object[])userData)[1];

        // start command
        String commandString = "FILE_LIST "+
                               StringUtils.escape(archiveFileTreeData.name)
                               ;
        Command command = BARServer.runCommand(commandString);

        // read results
        long n = 0;
        while (!command.endOfData() && !busyDialog.isAborted())
        {
          final String line = command.getNextResult(250);
          if (line != null)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
//Dprintf.dprintf("rrr=%s\n",line);
                Object data[] = new Object[10];
                if      (StringParser.parse(line,"FILE %ld %ld %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       size
                       date/time
                       name
                  */
                  long   size     = (Long  )data[0];
                  long   datetime = (Long  )data[1];
                  String name     = (String)data[2];

                  ArchiveFileTreeData subArchiveFileTreeData = new ArchiveFileTreeData(name,FileTypes.FILE,size,datetime,new File(name).getName());

                  TreeItem subTreeItem = Widgets.addTreeItem(treeItem,findArchiveFilesTreeIndex(treeItem,subArchiveFileTreeData),subArchiveFileTreeData,false);
                  subTreeItem.setText(0,subArchiveFileTreeData.title);
                  subTreeItem.setText(1,"FILE");
                  subTreeItem.setText(2,Long.toString(size));
                  subTreeItem.setText(3,simpleDateFormat.format(new Date(datetime*1000)));
                  subTreeItem.setImage(IMAGE_FILE);
                }
                else if (StringParser.parse(line,"DIRECTORY %ld %ld %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       size
                       date/time
                       name
                  */
                  long   size     = (Long  )data[0];
                  long   datetime = (Long  )data[1];
                  String name     = (String)data[2];

                  ArchiveFileTreeData subArchiveFileTreeData = new ArchiveFileTreeData(name,FileTypes.DIRECTORY,new File(name).getName());

                  TreeItem subTreeItem = Widgets.addTreeItem(treeItem,findArchiveFilesTreeIndex(treeItem,subArchiveFileTreeData),subArchiveFileTreeData,true);
                  subTreeItem.setText(0,subArchiveFileTreeData.title);
                  subTreeItem.setText(1,"DIR");
                  subTreeItem.setText(3,simpleDateFormat.format(new Date(datetime*1000)));
                  subTreeItem.setImage(IMAGE_DIRECTORY);
                  subTreeItem.setGrayed(true);
                }
                else if (StringParser.parse(line,"LINK %ld %S",data,StringParser.QUOTE_CHARS))
                {
                  /* get data
                     format:
                       date/time
                       name
                  */
                  long   datetime = (Long  )data[0];
                  String name     = (String)data[1];

                  ArchiveFileTreeData subArchiveFileTreeData = new ArchiveFileTreeData(name,FileTypes.LINK,0,datetime,new File(name).getName());

                  TreeItem subTreeItem = Widgets.addTreeItem(treeItem,findArchiveFilesTreeIndex(treeItem,subArchiveFileTreeData),subArchiveFileTreeData,false);
                  subTreeItem.setText(0,subArchiveFileTreeData.title);
                  subTreeItem.setText(1,"LINK");
                  subTreeItem.setText(3,simpleDateFormat.format(new Date(datetime*1000)));
                  subTreeItem.setImage(IMAGE_LINK);
                }
                else if (StringParser.parse(line,"SPECIAL %ld %S",data,StringParser.QUOTE_CHARS))
                {
                }
                else if (StringParser.parse(line,"DEVICE %S",data,StringParser.QUOTE_CHARS))
                {
                }
                else if (StringParser.parse(line,"SOCKET %S",data,StringParser.QUOTE_CHARS))
                {
                }
              }
            });

            n++;
          }

          busyDialog.update("Reading files..."+((n > 0)?n:""));
        }

        // abort command if requested
        if (busyDialog.isAborted())
        {
          busyDialog.update("Aborting...");
          BARServer.abortCommand(command);
        }

        // close busy dialog, restore cursor
        display.syncExec(new Runnable()
        {
          public void run()
          {
            treeItem.setExpanded(true);
            busyDialog.close();
            shell.setCursor(null);
           }
        });

        // check command error
        if (!busyDialog.isAborted() && (command.getErrorCode() != Errors.NONE))
        {
          final String errorText = command.getErrorText();
          display.syncExec(new Runnable()
          {
            public void run()
            {
              Dialogs.error(shell,"Cannot open '"+archiveFileTreeData.title+"' (error: "+errorText+")");
            }
          });
        }
      }
    };
  }

  /** get selected archive file names
   * @param treeItem tree item to check
   * @return true iff tree item or some sub-tree item is selected
   */
  private void setSelectedArchiveNames(TreeItem treeItem, boolean flag)
  {
    treeItem.setChecked(flag);

    for (TreeItem subTreeItem : treeItem.getItems())
    {
      setSelectedArchiveNames(subTreeItem,flag);
    }
  }

  /** get selected archive file names
   * @return tree selected archive file names
   */
  private void setSelectedArchiveNames(boolean flag)
  {
    for (TreeItem treeItem : widgetArchiveFileTree.getItems())
    {
      setSelectedArchiveNames(treeItem,flag);
    }
  }

  /** get selected archive file names
   * @param treeItem tree item to check
   * @return true iff tree item or some sub-tree item is selected
   */
  private void getSelectedArchiveNames(HashSet<String> archiveNamesHashSet, TreeItem treeItem)
  {
    ArchiveFileTreeData archiveFileTreeData = (ArchiveFileTreeData)treeItem.getData();

    if ((archiveFileTreeData != null) && !treeItem.getGrayed() && treeItem.getChecked())
    {
      archiveNamesHashSet.add(archiveFileTreeData.name);
    }

    for (TreeItem subTreeItem : treeItem.getItems())
    {
      getSelectedArchiveNames(archiveNamesHashSet,subTreeItem);
    }
  }

  /** get selected archive file names
   * @return tree selected archive file names
   */
  private String[] getSelectedArchiveNames()
  {
    HashSet<String> archiveNamesHashSet = new HashSet<String>();

    for (TreeItem treeItem : widgetArchiveFileTree.getItems())
    {
      getSelectedArchiveNames(archiveNamesHashSet,treeItem);
    }

    String archiveNames[] = new String[archiveNamesHashSet.size()];
    int i = 0;
    for (String archiveName : archiveNamesHashSet)
    {
      archiveNames[i] = archivePathNameParts.getArchiveName(archiveName);
      i++;
    }

    return archiveNames;
  }

  /** check if some archive file tree item is selected
   * @return tree iff some tree item is selected
   */
  private boolean checkArchiveFileSelected()
  {
    return getSelectedArchiveNames().length > 0;
  }

  /** list content of archives
   * @param archiveNames archive names
   * @param filterPattern filter pattern
   */
  private void listArchiveFiles(String archiveNames[], String filterPattern)
  {
    shell.setCursor(waitCursor);
    BusyDialog busyDialog = new BusyDialog(shell,"List archives","Archive:");

    fileList.clear();
    new BackgroundTask(busyDialog,new Object[]{archiveNames,fileList})
    {
      public void run(final BusyDialog busyDialog, Object userData)
      {
        final String[]             archiveNames = (String[]            )((Object[])userData)[0];
        final LinkedList<FileData> fileList     = (LinkedList<FileData>)((Object[])userData)[1];

        try
        {
          for (final String archiveName : archiveNames)
          {
  //Dprintf.dprintf("s=%s\n",archiveName);
            /* get archive content list */
            busyDialog.setMessage("Archive: '"+archiveName+"'");

            // start command
            busyDialog.update("Open...");
            String commandString = "ARCHIVE_LIST "+
                                   StringUtils.escape(archiveName)+" "+
                                   StringUtils.escape("")
                                   ;
            Command         command = null;
            int             errorCode = Errors.UNKNOWN;
            final boolean[] tryAgainFlag = new boolean[1];
            tryAgainFlag[0] = true;
            while (tryAgainFlag[0] && !busyDialog.isAborted())
            {
              tryAgainFlag[0] = false;

              /* try reading archive content */
              command = BARServer.runCommand(commandString);
              while (!command.waitForResult(250) && !busyDialog.isAborted())
              {
                busyDialog.update();
              }

              if (!busyDialog.isAborted())
              {
                /* ask for crypt password if password error */
                errorCode = command.getErrorCode();
                if (   (errorCode == Errors.CORRUPT_DATA     )
                    || (errorCode == Errors.NO_CRYPT_PASSWORD)
                    || (errorCode == Errors.INVALID_PASSWORD )
                   )
                {
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      String password = Dialogs.password(shell,
                                                         "Crypt password",
                                                         "Archive: "+archiveName,
                                                         "Crypt password"
                                                        );
                      if (password != null)
                      {
                        String[] result = new String[1];
                        BARServer.executeCommand("DECRYPT_PASSWORD_ADD "+StringUtils.escape(password),result);
                        tryAgainFlag[0] = true;
                      }
                    }
                  });
                }
              }
              else
              {
                busyDialog.update("Aborting...");
                BARServer.abortCommand(command);
                busyDialog.close();
              }
            }

            if (!busyDialog.isAborted())
            {
              if (errorCode == Errors.NONE)
              {
                // read results
                busyDialog.update("Reading files...");
                long n = 0;
                while (!command.endOfData() && !busyDialog.isAborted())
                {
                  final String line = command.getNextResult(250);
                  if (line != null)
                  {
                    display.syncExec(new Runnable()
                    {
                      public void run()
                      {
                        Object data[] = new Object[10];
                        if      (StringParser.parse(line,"FILE %ld %ld %ld %ld %ld %S",data,StringParser.QUOTE_CHARS))
                        {
                          /* get data
                             format:
                               size
                               date/time
                               archive file size
                               fragment offset
                               fragment size
                               name
                          */
                          long   size     = (Long  )data[0];
                          long   datetime = (Long  )data[1];
                          String name     = (String)data[5];

                          FileData fileData = new FileData(archiveName,name,FileTypes.FILE,size,datetime);
                          fileList.add(fileData);
                        }
                        else if (StringParser.parse(line,"DIRECTORY %ld %S",data,StringParser.QUOTE_CHARS))
                        {
                          /* get data
                             format:
                               date/time
                               name
                          */
                          long   datetime      = (Long  )data[0];
                          String directoryName = (String)data[1];

                          FileData fileData = new FileData(archiveName,directoryName,FileTypes.DIRECTORY,datetime);
                          fileList.add(fileData);
                        }
                        else if (StringParser.parse(line,"LINK %ld %S %S",data,StringParser.QUOTE_CHARS))
                        {
                          /* get data
                             format:
                               date/time
                               name
                          */
                          long   datetime = (Long  )data[0];
                          String linkName = (String)data[1];
                          String fileName = (String)data[2];

                          FileData fileData = new FileData(archiveName,linkName,FileTypes.LINK,datetime);
                          fileList.add(fileData);
                        }
                        else if (StringParser.parse(line,"SPECIAL %ld %S",data,StringParser.QUOTE_CHARS))
                        {
                        }
                        else if (StringParser.parse(line,"DEVICE %S",data,StringParser.QUOTE_CHARS))
                        {
                        }
                        else if (StringParser.parse(line,"SOCKET %S",data,StringParser.QUOTE_CHARS))
                        {
                        }
                      }
                    });

                    n++;
                  }

                  busyDialog.update("Reading files..."+((n > 0)?n:""));
                }

                // abort command if requested
                if (busyDialog.isAborted())
                {
                  busyDialog.update("Aborting...");
                  BARServer.abortCommand(command);
                  busyDialog.close();
                }

                // check command error
                if (busyDialog.isAborted() || (command.getErrorCode() != Errors.NONE))
                {
                  final String errorText = command.getErrorText();
                  display.syncExec(new Runnable()
                  {
                    public void run()
                    {
                      Dialogs.error(shell,"Cannot list archive '"+archiveName+"' (error: "+errorText+")");
                    }
                  });
                }
              }
              else
              {
                final String errorText = command.getErrorText();
                display.syncExec(new Runnable()
                {
                  public void run()
                  {
                    Dialogs.error(shell,"Cannot list archive '"+archiveName+"' (error: "+errorText+")");
                  }
                });
              }
            }

            if (busyDialog.isAborted()) break;
          }
        }
        catch (CommunicationError error)
        {
          // sort, update
          final String errorText = error.toString();
          display.syncExec(new Runnable()
          {
            public void run()
            {
              Dialogs.error(shell,"Communication error while processing archives (error: "+errorText+")");
            }
          });
        }

        if (!busyDialog.isAborted())
        {
          // sort, update
          display.syncExec(new Runnable()
          {
            public void run()
            {
              busyDialog.update("Sort...");
              shell.getDisplay().update();
              updateFileList();
            }
          });
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
    };
  }

  /** set file pattern
   * @param string pattern string
   */
  private void setFilePattern(String string)
  {
    if (string.length() > 0)
    {
      try
      {
        StringBuffer patternString = new StringBuffer();
        patternString.append(".*");
        for (int z = 0; z < string.length(); z++)
        {
          char ch = string.charAt(z);
          switch (ch)
          {
            case '\\':
            case '[':
            case ']':
            case '(':
            case ')':
            case '{':
            case '}':
            case '+':
            case '|':
            case '^':
            case '$':
              patternString.append('\\');
              patternString.append(ch);
              break;
            case '*':
              patternString.append(".*");
              break;
            case '?':
              patternString.append(".?");
              break;
            default:
              patternString.append(ch);
              break;
          }
        }
        patternString.append(".*");
        filePattern = Pattern.compile(patternString.toString(),Pattern.CASE_INSENSITIVE);
      }
      catch (PatternSyntaxException exception)
      {
        Dialogs.error(shell,"'"+string+"' is not valid pattern!\n.");
      }
    }
    else
    {
      filePattern = null;
    }
    updateFileList();
  }

  /** check if file pattern matches
   * @param fileName
   * @return true iff file pattern matches
   */
  private boolean matchFilePattern(String fileName)
  {
    return (filePattern == null) || filePattern.matcher(fileName).matches();
  }

  private void addFileList(final FileData fileData)
  {
    switch (fileData.type)
    {
      case FILE:
        if (matchFilePattern(fileData.name))
        {
          TableItem tableItem = new TableItem(widgetFileList,SWT.NONE);
          tableItem.setData(fileData);
          tableItem.setText(0,fileData.archiveName);
          tableItem.setText(1,fileData.name);
          tableItem.setText(2,"FILE");
          tableItem.setText(3,Long.toString(fileData.size));
          tableItem.setText(4,simpleDateFormat.format(new Date(fileData.datetime*1000)));
        }
        break;
      case DIRECTORY:
        if (matchFilePattern(fileData.name))
        {
          TableItem tableItem = new TableItem(widgetFileList,SWT.NONE);
          tableItem.setData(fileData);
          tableItem.setText(0,fileData.archiveName);
          tableItem.setText(1,fileData.name);
          tableItem.setText(2,"DIR");
          tableItem.setText(3,"");
          tableItem.setText(4,simpleDateFormat.format(new Date(fileData.datetime*1000)));
        }
        break;
      case LINK:
        if (matchFilePattern(fileData.name))
        {
          TableItem tableItem = new TableItem(widgetFileList,SWT.NONE);
          tableItem.setData(fileData);
          tableItem.setText(0,fileData.archiveName);
          tableItem.setText(1,fileData.name);
          tableItem.setText(2,"LINK");
          tableItem.setText(3,"");
          tableItem.setText(4,simpleDateFormat.format(new Date(fileData.datetime*1000)));
        }
        break;
    }
  }

  /** update file list, match filter pattern
   */
  private void updateFileList()
  {
    /* update list */
    widgetFileList.removeAll();
    if (newestFileOnlyFlag)
    {
      /* collect newest files */
      HashMap<String,FileData> fileListHashMap = new HashMap<String,FileData>();
      for (FileData fileData : fileList)
      {
        FileData newestFileData = fileListHashMap.get(fileData.name);
        if (   (newestFileData == null)
            || (fileData.datetime > newestFileData.datetime)
           )
        {
          fileListHashMap.put(fileData.name,fileData);
        }
      }

      for (FileData fileData : fileListHashMap.values())
      {
        addFileList(fileData);
      }
    }
    else
    {
      /* show all files */
      for (FileData fileData : fileList)
      {
        addFileList(fileData);
      }
    }

    /* sort */
    FileDataComparator fileDataComparator = new FileDataComparator(widgetFileList);
    synchronized(widgetFileList)
    {
      Widgets.sortTableColumn(widgetFileList,fileDataComparator);
    }
  }

  /** set selected files
   * @param flag true iff selected
   */
  private void setSelectedFiles(boolean flag)
  {
    for (TableItem tableItem : widgetFileList.getItems())
    {
      tableItem.setChecked(flag);
    }
  }

  /** get selected files
   * @return selected files data
   */
  private FileData[] getSelectedFiles()
  {
    ArrayList<FileData> fileDataArray = new ArrayList<FileData>();

    for (TableItem tableItem : widgetFileList.getItems())
    {
      if (tableItem.getChecked())
      {
        fileDataArray.add((FileData)tableItem.getData());
      }
    }

    return fileDataArray.toArray(new FileData[fileDataArray.size()]);
  }

  /** check if some file item is selected
   * @return tree iff some file is selected
   */
  private boolean checkFilesSelected()
  {
    return getSelectedFiles().length > 0;
  }

  /** restore files
   * @param files files to restore
   * @param directory destination directory or ""
   * @param overwriteFiles true to overwrite existing files
   */
  private void restoreFiles(FileData files[], String directory, boolean overwriteFiles)
  {
    shell.setCursor(waitCursor);

    final BusyDialog busyDialog = new BusyDialog(shell,"Restore files",500,100);

    new BackgroundTask(busyDialog,new Object[]{files,directory,overwriteFiles})
    {
      public void run(final BusyDialog busyDialog, Object userData)
      {
        final FileData[] files          = (FileData[])((Object[])userData)[0];
        final String     directory      = (String    )((Object[])userData)[1];
        final boolean    overwriteFiles = (Boolean   )((Object[])userData)[2];

        int errorCode;

        // restore files
        for (final FileData fileData : files)
        {
          if (!directory.equals(""))
          {
            busyDialog.update("'"+fileData.name+"' into '"+directory+"'");
          }
          else
          {
            busyDialog.update("'"+fileData.name+"'");
          }

          ArrayList<String> result = new ArrayList<String>();
          String command = "RESTORE "+
                           StringUtils.escape(fileData.archiveName)+" "+
                           StringUtils.escape(directory)+" "+
                           (overwriteFiles?"1":"0")+" "+
                           StringUtils.escape(fileData.name)
                           ;
//Dprintf.dprintf("command=%s",command);
          errorCode = BARServer.executeCommand(command,
                                               result,
                                               new BusyIndicator()
                                               {
                                                 public void busy(long n)
                                                 {
                                                   busyDialog.update();
                                                 }

                                                 public boolean isAborted()
                                                 {
                                                   return busyDialog.isAborted();
                                                 }
                                               }
                                              );
          // abort command if requested
          if (!busyDialog.isAborted())
          {
            if (errorCode != Errors.NONE)
            {
              final String errorText = result.get(0);
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  Dialogs.error(shell,"Cannot restore file '"+fileData.name+"' from archive\n'"+fileData.archiveName+"' (error: "+errorText+")");
                }
              });
            }
          }
          else
          {
            busyDialog.update("Aborting...");
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
