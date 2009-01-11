/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TabRestore.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
//import java.io.ByteArrayInputStream;
//import java.io.ByteArrayOutputStream;
import java.io.File;
//import java.io.FileReader;
//import java.io.BufferedReader;
//import java.io.IOException;
//import java.io.ObjectInputStream;
//import java.io.ObjectOutputStream;
//import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
//import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;

import org.eclipse.swt.custom.SashForm;
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
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseMoveListener;
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
import org.eclipse.swt.widgets.Canvas;
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

    ArchiveFileTreeData(String name, FileTypes type, long size, long datetime, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = size;
      this.datetime = datetime;
      this.title    = title;
    }

    ArchiveFileTreeData(String name, FileTypes type, long datetime, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = 0;
      this.datetime = datetime;
      this.title    = title;
    }

    ArchiveFileTreeData(String name, FileTypes type, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = 0;
      this.datetime = 0;
      this.title    = title;
    }

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
//System.err.println("BARControl.java"+", "+2734+": file1="+archiveTreeData1+" file=2"+archiveTreeData2+" "+sortMode);
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

    FileData()
    {
    }

    public String toString()
    {
      return "File {}";
    }
  };

  /** file data comparator
   */
  class FileDataComparator implements Comparator<FileData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_DATE    = 0;
    private final static int SORTMODE_WEEKDAY = 1;
    private final static int SORTMODE_TIME    = 2;
    private final static int SORTMODE_ENABLED = 3;
    private final static int SORTMODE_TYPE    = 4;

    private int sortMode;

    /** create file data comparator
     * @param table file table
     * @param sortColumn sorting column
     */
    FileDataComparator(Table table, TableColumn sortColumn)
    {
      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_WEEKDAY;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_ENABLED;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_TYPE;
      else                                       sortMode = SORTMODE_DATE;
    }

    /** create file data comparator
     * @param table file table
     */
    FileDataComparator(Table table)
    {
      TableColumn sortColumn = table.getSortColumn();

      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_WEEKDAY;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_ENABLED;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_TYPE;
      else                                       sortMode = SORTMODE_DATE;
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
        case SORTMODE_DATE:
        case SORTMODE_WEEKDAY:
        case SORTMODE_TIME:
        case SORTMODE_ENABLED:
        case SORTMODE_TYPE:
        default:
          return 0;
      }
    }
  }

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

  // global variable references
  Shell       shell;
  TabStatus   tabStatus;

  // widgets
  Composite   widgetTab;
  TabFolder   widgetTabFolder;
  Combo       widgetPath;
  Tree        widgetArchiveFileTree;
  Combo       widgetFileFilter;
  Table       widgetFileList;
  Text        widgetTo;
  Button      widgetToSelectButton;

  // variables
  private     HashMap<String,Integer>  jobIds          = new HashMap<String,Integer>();
  private     String                   selectedJobName = null;
  private     int                      selectedJobId   = 0;
  private     LinkedList<FileData>     fileList        = new LinkedList<FileData>();

  private     SimpleDateFormat         simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

  TabRestore(TabFolder parentTabFolder, int accelerator)
  {
    Display     display;
    Composite   tab;
    Group       group;
    Composite   composite,subComposite;
    Label       label;
    Button      button;
    Combo       combo;
    TreeColumn  treeColumn;
    TreeItem    treeItem;
    Text        text;
    Spinner     spinner;
    TableColumn tableColumn;

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

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Restore"+((accelerator != 0)?" ("+Widgets.acceleratorToText(accelerator)+")":""));
    widgetTab.setLayout(new TableLayout(new double[]{0,0.5,0,0.5},
                                        null,
                                        2
                                       )
                       );
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);

    // path
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,"Path:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPath = Widgets.newCombo(composite,null);
      Widgets.layout(widgetPath,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      widgetPath.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Combo  widget   = (Combo)selectionEvent.widget;
          String pathName = widget.getText();
          setPath(pathName);
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo widget = (Combo)selectionEvent.widget;
          String pathName = widget.getText();
          setPath(pathName);
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
          setPath(pathName);
        }
      });


      button = Widgets.newButton(composite,null,IMAGE_DIRECTORY);
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
            setPath(pathName);
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // archives tree
    widgetArchiveFileTree = Widgets.newTree(widgetTab,null);
    Widgets.layout(widgetArchiveFileTree,1,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
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

    // path
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    Widgets.layout(composite,2,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,"Filter:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetFileFilter = Widgets.newCombo(composite,null);
      Widgets.layout(widgetFileFilter,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      widgetFileFilter.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Combo  widget = (Combo)selectionEvent.widget;
          String s      = widget.getText();
          try
          {
//              long n = Units.parseByteSize(s);
//              archivePartSize.set(n);
//              BARServer.set(selectedJobId,"archive-part-size",n);
          }
          catch (NumberFormatException exception)
          {
            Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
          }
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo widget = (Combo)selectionEvent.widget;
          long  n      = Units.parseByteSize(widget.getText());
//            archivePartSize.set(n);
//            BARServer.set(selectedJobId,"archive-part-size",n);
        }
      });
      widgetFileFilter.addFocusListener(new FocusListener()
      {
        public void focusGained(FocusEvent focusEvent)
        {
        }
        public void focusLost(FocusEvent focusEvent)
        {
          Combo  widget = (Combo)focusEvent.widget;
          String s      = widget.getText();
          try
          {
//              long n = Units.parseByteSize(s);
//              archivePartSize.set(n);
//              BARServer.set(selectedJobId,"archive-part-size",n);
          }
          catch (NumberFormatException exception)
          {
            Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
            widget.forceFocus();
          }
        }
      });
    }

    // file list
    widgetFileList = Widgets.newTable(widgetTab,this);
    Widgets.layout(widgetFileList,3,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
    widgetFileList.addListener(SWT.MouseDoubleClick,new Listener()
    {
      public void handleEvent(final Event event)
      {
//        scheduleEdit();
      }
    });
    SelectionListener fileListColumnSelectionListener = new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        TableColumn        tableColumn = (TableColumn)selectionEvent.widget;
        FileDataComparator fileDataComparator = new FileDataComparator(widgetFileList,tableColumn);
        synchronized(widgetFileList)
        {
          Widgets.sortTableColumn(widgetFileList,tableColumn,fileDataComparator);
        }
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    };
    tableColumn = Widgets.addTableColumn(widgetFileList,0,"Name",          SWT.LEFT, 200,true );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,1,"Type",          SWT.LEFT,  30,true  );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,2,"Size",          SWT.LEFT,  30,true  );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,3,"Date",          SWT.LEFT,  30,true  );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,4,"Compress",      SWT.LEFT,  80,true );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetFileList,5,"Crypt",         SWT.LEFT,  70,true );
    tableColumn.addSelectionListener(fileListColumnSelectionListener);

    // buttons
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    Widgets.layout(composite,4,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      button = Widgets.newButton(composite,null,"Restore");
      Widgets.layout(button,0,0,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
//          scheduleNew();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      button = Widgets.newCheckbox(composite,null,"to");
      Widgets.layout(button,0,1,TableLayoutData.W);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button  widget      = (Button)selectionEvent.widget;
          boolean checkedFlag = widget.getSelection();
          widgetTo.setEnabled(checkedFlag);
          widgetToSelectButton.setEnabled(checkedFlag);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
      widgetTo = Widgets.newText(composite,null);
      widgetTo.setEnabled(false);
      Widgets.layout(widgetTo,0,2,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      widgetTo.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
          Text widget = (Text)selectionEvent.widget;
//            storageFileName.set(widget.getText());
//            BARServer.set(selectedJobId,"archive-name",getArchiveName());
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
throw new Error("NYI");
        }
      });
      widgetTo.addFocusListener(new FocusListener()
      {
        public void focusGained(FocusEvent focusEvent)
        {
        }
        public void focusLost(FocusEvent focusEvent)
        {
          Text widget = (Text)focusEvent.widget;
//            storageFileName.set(widget.getText());
//            BARServer.set(selectedJobId,"archive-name",getArchiveName());
        }
      });
      widgetToSelectButton = Widgets.newButton(composite,null,IMAGE_DIRECTORY);
      widgetToSelectButton.setEnabled(false);
      Widgets.layout(widgetToSelectButton,0,3,TableLayoutData.DEFAULT);
      widgetToSelectButton.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          String pathName = Dialogs.directory(shell,
                                              "Select path",
                                              widgetTo.getText()
                                             );
          if (pathName != null)
          {
            widgetTo.setText(pathName);
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // update data
    updatePathList();
  }

  //-----------------------------------------------------------------------

  /** set path to archives
   */
  private void setPath(String pathName)
  {
    widgetArchiveFileTree.removeAll();

    TreeItem treeItem = Widgets.addTreeItem(widgetArchiveFileTree,new ArchiveFileTreeData(pathName,FileTypes.DIRECTORY,pathName),true);
    treeItem.setText(pathName);
    treeItem.setImage(IMAGE_DIRECTORY);
    widgetArchiveFileTree.addListener(SWT.Expand,new Listener()
    {
      public void handleEvent(final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        updateArchiveFileList(treeItem);
      }
    });
    widgetArchiveFileTree.addListener(SWT.Collapse,new Listener()
    {
      public void handleEvent(final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
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
        }
      }
    });
  }

  /** clear all data
   */
  private void clear()
  {
  }

  /** update path list
   */
  private void updatePathList()
  {
    if (!widgetPath.isDisposed())
    {
      // get job list
      ArrayList<String> result = new ArrayList<String>();
      int errorCode = BARServer.executeCommand("JOB_LIST",result);
      if (errorCode != 0) return;

      // get archive path names
      HashSet <String> pathNames = new HashSet<String>();
      for (String line : result)
      {
        Object data[] = new Object[10];
        /* format:
           <id>
           <name>
           <state>
           <type>
           <archivePartSize>
           <compressAlgorithm>
           <cryptAlgorithm>
           <cryptTyp>
           <lastExecutedDateTime>
           <estimatedRestTime>
        */
        if (StringParser.parse(line,"%d %S %S %s %d %S %S %S %ld %ld",data,StringParser.QUOTE_CHARS))
        {
          // get data
          int id = (Integer)data[0];

          // get archive name
          String archiveName = BARServer.getString(id,"archive-name");

          // save path
          String path = new File(archiveName).getParent();
          if (path != null) pathNames.add(path);
        }

        // update path list
        widgetPath.removeAll();
        for (String path : pathNames)
        {
          widgetPath.add(path);
        }
      }
    }
  }

  /** find index for insert of tree item in sort list of tree items
   * @param treeItem tree item
   * @param name name of tree item to insert
   * @param data data of tree item to insert
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
  private void updateArchiveFileList(TreeItem treeItem)
  {
    ArchiveFileTreeData archiveFileTreeData = (ArchiveFileTreeData)treeItem.getData();
    TreeItem            subTreeItem;

    ArrayList<String> result = new ArrayList<String>();
    BARServer.executeCommand("FILE_LIST "+StringParser.escape(archiveFileTreeData.name),result);

    treeItem.removeAll();
    for (String line : result)
    {
//System.err.println("BARControl.java"+", "+1733+": "+line);
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

        archiveFileTreeData = new ArchiveFileTreeData(name,FileTypes.FILE,size,datetime,new File(name).getName());

        subTreeItem = Widgets.addTreeItem(treeItem,findArchiveFilesTreeIndex(treeItem,archiveFileTreeData),archiveFileTreeData,false);
        subTreeItem.setText(0,archiveFileTreeData.title);
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

        archiveFileTreeData = new ArchiveFileTreeData(name,FileTypes.DIRECTORY,new File(name).getName());

        subTreeItem = Widgets.addTreeItem(treeItem,findArchiveFilesTreeIndex(treeItem,archiveFileTreeData),archiveFileTreeData,true);
        subTreeItem.setText(0,archiveFileTreeData.title);
        subTreeItem.setText(1,"DIR");
        subTreeItem.setText(3,simpleDateFormat.format(new Date(datetime*1000)));
        subTreeItem.setImage(IMAGE_DIRECTORY);
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

        archiveFileTreeData = new ArchiveFileTreeData(name,FileTypes.LINK,0,datetime,new File(name).getName());

        subTreeItem = Widgets.addTreeItem(treeItem,findArchiveFilesTreeIndex(treeItem,archiveFileTreeData),archiveFileTreeData,false);
        subTreeItem.setText(0,archiveFileTreeData.title);
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
  }

  /** update all data
   */
  private void update()
  {
    updatePathList();
  }
}

/* end of file */
