/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Widgets.java,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.File;
import java.io.InputStream;
import java.net.URL;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedList;

import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
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
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
import org.eclipse.swt.widgets.Sash;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Spinner;
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

class Widgets
{
  //-----------------------------------------------------------------------

  /** list of widgets listeners
   */
  private static LinkedList<WidgetListener> listenersList = new LinkedList<WidgetListener>();

  //-----------------------------------------------------------------------

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param padX,padY padding X/Y
   * @param width,height min. width/height
   */
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height)
  {
    TableLayoutData tableLayoutData = new TableLayoutData(row,column,style,rowSpawn,columnSpawn,padX,padY); //,width,height);
    tableLayoutData.minWidth  = width;
    tableLayoutData.minHeight = height;
    tableLayoutData.maxWidth  = width;
    tableLayoutData.maxHeight = height;
    control.setLayoutData(tableLayoutData);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param padX,padY padding X/Y
   * @param size min. width/height
   */
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,padX,padY,size.x,size.y);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param pad padding X/Y
   * @param size min. width/height
   */
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, Point pad, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,pad.x,pad.y,size.x,size.y);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param padX,padY padding X/Y
   */
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,padX,padY,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param size padding size
   */
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,size.x,size.y);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   * @param pad padding X/Y
   */
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int pad)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,pad,pad);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   * @param rowSpawn,columnSpan row/column spawn (0..n)
   */
  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,0);
  }

  /** layout widget
   * @param control control to layout
   * @param row,column row,column (0..n)
   * @param style SWT style flags
   */
  static void layout(Control control, int row, int column, int style)
  {
//    layout(control,row,column,style,0,0);
    layout(control,row,column,style,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** get text height
   * @param control control
   * @return height of text
   */
  static int getTextHeight(Control control)
  {
    int height;

    GC gc = new GC(control);
    height = gc.getFontMetrics().getHeight();
    gc.dispose();

    return height;
  }

  /** get text size
   * @param control control
   * @return size of text
   */
  static Point getTextSize(Control control, String text)
  {
    Point size;

    GC gc = new GC(control);
    size = gc.textExtent(text);
    gc.dispose();

    return size;
  }

  /** get max. text size
   * @param control control
   * @return max. size of all texts
   */
  static Point getTextSize(Control control, String[] texts)
  {
    Point size;

    size = new Point(0,0);
    GC gc = new GC(control);
    for (String text : texts)
    {
      size.x = Math.max(size.x,gc.textExtent(text).x);
      size.y = Math.max(size.y,gc.textExtent(text).y);
    }
    gc.dispose();

    return size;
  }

  /** load image from jar or directory "images"
   * @param fileName image file name
   * @return image
   */
  static Image loadImage(Display display, String fileName)
  {
    // try to load from jar file
    try
    {
      InputStream inputStream = display.getClass().getClassLoader().getResourceAsStream("images/"+fileName);
      Image image = new Image(display,inputStream);
      inputStream.close();

      return image;
    }
    catch (Exception exception)
    {
//System.err.println("Widgets.java"+", "+147+": "+exception);
      // ignored
    }

    // try to load from file
    return new Image(display,"images"+File.separator+fileName);
  }

  /** get accelerator text
   * @param accelerator SWT accelerator
   * @return accelerator text
   */
  static String acceleratorToText(int accelerator)
  {
    StringBuffer text = new StringBuffer();

    if (accelerator != 0)
    {
      if ((accelerator & SWT.MOD1) == SWT.CTRL) text.append("Ctrl+");
      if ((accelerator & SWT.MOD2) == SWT.ALT ) text.append("Alt+");

      if      ((accelerator & SWT.KEY_MASK) == SWT.F1 ) text.append("F1");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F2 ) text.append("F2");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F3 ) text.append("F3");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F4 ) text.append("F4");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F5 ) text.append("F5");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F6 ) text.append("F6");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F7 ) text.append("F7");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F8 ) text.append("F8");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F9 ) text.append("F9");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F10) text.append("F10");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F11) text.append("F11");
      else if ((accelerator & SWT.KEY_MASK) == SWT.F12) text.append("F12");
      else                                              text.append((char)(accelerator & SWT.KEY_MASK));
    }

    return text.toString();
  }

  /** set enabled
   * @param control control to enable/disable
   * @param enableFlag true to enable, false to disable
   */
  static void setEnabled(Control control, boolean enableFlag)
  {
    control.setEnabled(enableFlag);
  }

  /** set visible
   * @param control control to make visible/invisible
   * @param visibleFlag true to make visible, false to make invisible
   */
  static void setVisible(Control control, boolean visibleFlag)
  {
    TableLayoutData tableLayoutData = (TableLayoutData)control.getLayoutData();
    tableLayoutData.exclude = !visibleFlag;
    control.setVisible(visibleFlag);
    if (visibleFlag)
    {
      control.getParent().layout();
    }
  }

  /** create empty space
   * @param composite composite
   * @param width,height size of empty space
   * @return canvas space
   */
  static Control newSpacer(Composite composite)
  {
    Label label = new Label(composite,SWT.NONE);

    return (Control)label;
  }

  /** create new label
   * @param composite composite
   * @param text label text
   * @param style label style
   * @return new label
   */
  static Label newLabel(Composite composite, String text, int style)
  {
    Label label;

    label = new Label(composite,style);
    label.setText(text);

    return label;
  }

  /** create new image label
   * @param composite composite
   * @param image image
   * @param style label style
   * @return new label
   */
  static Label newLabel(Composite composite, Image image, int style)
  {
    Label label;

    label = new Label(composite,style);
    label.setImage(image);

    return label;
  }

  /** create new label
   * @param composite composite
   * @param text label text
   * @return new label
   */
  static Label newLabel(Composite composite, String text)
  {
    return newLabel(composite,text,SWT.LEFT);
  }

  /** create new label
   * @param composite composite
   * @param image image
   * @return new label
   */
  static Label newLabel(Composite composite, Image image)
  {
    return newLabel(composite,image,SWT.LEFT);
  }

  /** create new label
   * @param composite composite
   * @return new label
   */
  static Label newLabel(Composite composite)
  {
    return newLabel(composite,"");
  }

  /** create new view
   * @param composite composite
   * @return new view
   */
  static Label newView(Composite composite)
  {
    Label label;

    label = new Label(composite,SWT.LEFT|SWT.BORDER);
    label.setText("");

    return label;
  }

  /** create new number view
   * @param composite composite
   * @return new view
   */
  static Label newNumberView(Composite composite)
  {
    Label label;

    label = new Label(composite,SWT.RIGHT|SWT.BORDER);
    label.setText("0");

    return label;
  }

  /** create new string view
   * @param composite composite
   * @return new view
   */
  static Label newStringView(Composite composite)
  {
    Label label;

    label = new Label(composite,SWT.LEFT|SWT.BORDER);
    label.setText("");

    return label;
  }

  /** create new button
   * @param composite composite
   * @param object object
   * @param text text
   * @return new button
   */
  static Button newButton(Composite composite, Object data, String text)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setText(text);
    button.setData(data);

    return button;
  }

  /** create new button with image
   * @param composite composite
   * @param object object
   * @param image image
   * @return new button
   */
  static Button newButton(Composite composite, Object data, Image image)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setImage(image);
    button.setData(data);

    return button;
  }

  /** create new checkbox
   * @param composite composite
   * @param object object
   * @param text text
   * @return new button
   */
  static Button newCheckbox(Composite composite, Object data, String text)
  {
    Button button;

    button = new Button(composite,SWT.CHECK);
    button.setText(text);
    button.setData(data);

    return button;
  }

  /** create new radio button
   * @param composite composite
   * @param object object
   * @param text text
   * @return new button
   */
  static Button newRadio(Composite composite, Object data, String text)
  {
    Button button;

    button = new Button(composite,SWT.RADIO);
    button.setText(text);
    button.setData(data);

    return button;
  }

  /** create new text input widget (single line)
   * @param composite composite
   * @param object object
   * @return new text widget
   */
  static Text newText(Composite composite, Object data)
  {
    Text text;

    text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE);
    text.setData(data);

    return text;
  }

  /** create new password input widget (single line)
   * @param composite composite
   * @param object object
   * @return new text widget
   */
  static Text newPassword(Composite composite, Object data)
  {
    Text text;

    text = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
    text.setData(data);

    return text;
  }

  /** create new list widget
   * @param composite composite
   * @param object object
   * @return new list widget
   */
  static List newList(Composite composite, Object data)
  {
    List list;

    list = new List(composite,SWT.BORDER|SWT.MULTI|SWT.V_SCROLL);
    list.setData(data);

    return list;
  }

  /** new combo widget
   * @param composite composite
   * @param object object
   * @return new combo widget
   */
  static Combo newCombo(Composite composite, Object data)
  {
    Combo combo;

    combo = new Combo(composite,SWT.BORDER);
    combo.setData(data);

    return combo;
  }

  /** create new option menu
   * @param composite composite
   * @param object object
   * @return new combo widget
   */
  static Combo newOptionMenu(Composite composite, Object data)
  {
    Combo combo;

    combo = new Combo(composite,SWT.RIGHT|SWT.READ_ONLY);
    combo.setData(data);

    return combo;
  }

  /** create new spinner widget
   * @param composite composite
   * @param object object
   * @return new spinner widget
   */
  static Spinner newSpinner(Composite composite, Object data)
  {
    Spinner spinner;

    spinner = new Spinner(composite,SWT.READ_ONLY);
    spinner.setData(data);

    return spinner;
  }

  /** create new table widget
   * @param composite composite
   * @param style style
   * @param object object
   * @return new table widget
   */
  static Table newTable(Composite composite, int style, Object data)
  {
    Table table;

    table = new Table(composite,style|SWT.BORDER|SWT.MULTI|SWT.FULL_SELECTION);
    table.setLinesVisible(true);
    table.setHeaderVisible(true);
    table.setData(data);

    return table;
  }

  /** add column to table widget
   * @param table table widget
   * @param columnNb column number
   * @param title column title
   * @param style style
   * @param width width of column
   * @param resizable TRUE iff resizable column
   * @return new table column
   */
  static TableColumn addTableColumn(Table table, int columnNb, String title, int style, int width, boolean resizable)
  {
    TableColumn tableColumn = new TableColumn(table,style);
    tableColumn.setText(title);
    tableColumn.setData(columnNb);
    tableColumn.setWidth(width);
    tableColumn.setResizable(resizable);
    if (width <= 0) tableColumn.pack();

    return tableColumn;
  }

  /** sort table column
   * @param table table
   * @param tableColumn table column to sort by
   * @param comparator table data comparator
   */
  static void sortTableColumn(Table table, TableColumn tableColumn, Comparator comparator)
  {
    // get sorting direction
    int sortDirection = table.getSortDirection();
    if (sortDirection == SWT.NONE) sortDirection = SWT.UP;
    if (table.getSortColumn() == tableColumn)
    {
      switch (sortDirection)
      {
        case SWT.UP:   sortDirection = SWT.DOWN; break;
        case SWT.DOWN: sortDirection = SWT.UP;   break;
      }
    }
    table.setSortColumn(tableColumn);
    table.setSortDirection(sortDirection);

    // sort column
    sortTableColumn(table,comparator);
  }

  /** sort table column
   * @param table table
   * @param comparator table data comparator
   */
  static void sortTableColumn(Table table, Comparator comparator)
  {
    TableItem[] tableItems = table.getItems();

    // get sorting direction
    int sortDirection = table.getSortDirection();
    if (sortDirection == SWT.NONE) sortDirection = SWT.UP;

    // sort column
    for (int i = 1; i < tableItems.length; i++)
    {
      boolean sortedFlag = false;
      for (int j = 0; (j < i) && !sortedFlag; j++)
      {
        switch (sortDirection)
        {
          case SWT.UP:   sortedFlag = (comparator.compare(tableItems[i].getData(),tableItems[j].getData()) < 0); break;
          case SWT.DOWN: sortedFlag = (comparator.compare(tableItems[i].getData(),tableItems[j].getData()) > 0); break;
        }
        if (sortedFlag)
        {
          // save data
          Object   data = tableItems[i].getData();
          String[] texts = new String[table.getColumnCount()];
          for (int z = 0; z < table.getColumnCount(); z++)
          {
            texts[z] = tableItems[i].getText(z);
          }
          boolean checked = tableItems[i].getChecked();

          // discard item
          tableItems[i].dispose();

          // create new item
          TableItem tableItem = new TableItem(table,SWT.NONE,j);
          tableItem.setData(data);
          tableItem.setText(texts);
          tableItem.setChecked(checked);

          tableItems = table.getItems();
        }
      }
    }
  }

  /** new progress bar widget
   * @param composite composite
   * @param variable variable
   * @return new progress bar widget
   */
  static ProgressBar newProgressBar(Composite composite, Object variable)
  {
    ProgressBar progressBar;

    progressBar = new ProgressBar(composite,SWT.HORIZONTAL);
    progressBar.setMinimum(0);
    progressBar.setMaximum(100);
    progressBar.setSelection(0);

    return progressBar;
  }

  /** new tree widget
   * @param composite composite
   * @param object object
   * @return new tree widget
   */
  static Tree newTree(Composite composite, int style, Object variable)
  {
    Tree tree;

    tree = new Tree(composite,style|SWT.BORDER|SWT.H_SCROLL|SWT.V_SCROLL);
    tree.setHeaderVisible(true);

    return tree;
  }

  /** add column to tree widget
   * @param tree tree widget
   * @param title column title
   * @param style style
   * @param width width of column
   * @param resizable TRUE iff resizable column
   * @return new tree column
   */
  static TreeColumn addTreeColumn(Tree tree, String title, int style, int width, boolean resizable)
  {
    TreeColumn treeColumn = new TreeColumn(tree,style);
    treeColumn.setText(title);
    treeColumn.setWidth(width);
    treeColumn.setResizable(resizable);
    if (width <= 0) treeColumn.pack();

    return treeColumn;
  }

  /** add tree item
   * @param tree tree widget
   * @param index index (0..n)
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  static TreeItem addTreeItem(Tree tree, int index, Object data, boolean folderFlag)
  {
    TreeItem treeItem;

    treeItem = new TreeItem(tree,SWT.CHECK,index);
    treeItem.setData(data);
    if (folderFlag) new TreeItem(treeItem,SWT.NONE);

    return treeItem;
  }

  /** add tree item at end
   * @param tree tree widget
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  static TreeItem addTreeItem(Tree tree, Object data, boolean folderFlag)
  {
    return addTreeItem(tree,0,data,folderFlag);
  }

  /** add sub-tree item
   * @param parentTreeItem parent tree item
   * @param index index (0..n)
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  static TreeItem addTreeItem(TreeItem parentTreeItem, int index, Object data, boolean folderFlag)
  {
    TreeItem treeItem;

    treeItem = new TreeItem(parentTreeItem,SWT.NONE,index);
    treeItem.setData(data);
    if (folderFlag) new TreeItem(treeItem,SWT.NONE);

    return treeItem;
  }

  /** add sub-tree item ad end
   * @param parentTreeItem parent tree item
   * @param data data
   * @param folderFlag TRUE iff foler
   * @return new tree item
   */
  static TreeItem addTreeItem(TreeItem parentTreeItem, Object data, boolean folderFlag)
  {
    return addTreeItem(parentTreeItem,0,data,folderFlag);
  }

/*
static int rr = 0;
static String indent(int n)
{
  String s="";

  while (n>0) { s=s+"  "; n--; }
  return s;
}
private static void printSubTree(int n, TreeItem parentTreeItem)
{
  System.out.println(indent(n)+parentTreeItem+" ("+parentTreeItem.hashCode()+") count="+parentTreeItem.getItemCount()+" expanded="+parentTreeItem.getExpanded());
  for (TreeItem treeItem : parentTreeItem.getItems())
  {
    printSubTree(n+1,treeItem);
  }
}
private static void printTree(Tree tree)
{
  for (TreeItem treeItem : tree.getItems())
  {
    printSubTree(0,treeItem);
  }
}
*/

  /** re-created tree item (required when sorting by column)
   * @param tree tree
   * @param parentTreeItem parent tree item
   * @param treeItem tree item to re-create
   * @param index index (0..n)
   */
  private static TreeItem recreateTreeItem(Tree tree, TreeItem parentTreeItem, TreeItem treeItem, int index)
  {
    // save data
    Object   data = treeItem.getData();
    String[] texts = new String[tree.getColumnCount()];
    for (int z = 0; z < tree.getColumnCount(); z++)
    {
      texts[z] = treeItem.getText(z);
    }
    boolean checked = treeItem.getChecked();
    Image image = treeItem.getImage();

    // recreate item
    TreeItem newTreeItem = new TreeItem(parentTreeItem,SWT.NONE,index);
    newTreeItem.setData(data);
    newTreeItem.setText(texts);
    newTreeItem.setChecked(checked);
    newTreeItem.setImage(image);
    for (TreeItem subTreeItem : treeItem.getItems())
    {
      recreateTreeItem(tree,newTreeItem,subTreeItem);
    }

    // discard old item
    treeItem.dispose();

    return newTreeItem;
  }

  /** re-created tree item (required when sorting by column)
   * @param tree tree
   * @param parentTreeItem parent tree item
   * @param treeItem tree item to re-create
   */
  private static TreeItem recreateTreeItem(Tree tree, TreeItem parentTreeItem, TreeItem treeItem)
  {
    return recreateTreeItem(tree,parentTreeItem,treeItem,parentTreeItem.getItemCount());
  }

  /** sort tree column
   * @param tree tree
   * @param treeItem tree item
   * @param sortDirection sort directory (SWT.UP, SWT.DOWN)
   * @param comparator comperator to compare two tree items
   */
  private static void sortSubTreeColumn(Tree tree, TreeItem treeItem, int sortDirection, Comparator comparator)
  {
//rr++;

//System.err.println(indent(rr)+"A "+treeItem+" "+treeItem.hashCode()+" "+treeItem.getItemCount()+" open="+treeItem.getExpanded());
    for (TreeItem subTreeItem : treeItem.getItems())
    {
      sortSubTreeColumn(tree,subTreeItem,sortDirection,comparator);
    }
//System.err.println(indent(rr)+"B "+treeItem+" ("+treeItem.hashCode()+") "+treeItem.hashCode()+" "+treeItem.getItemCount()+" open="+treeItem.getExpanded());
    
    // sort sub-tree
//boolean xx = treeItem.getExpanded();
    TreeItem[] subTreeItems = treeItem.getItems();
    for (int i = 0; i < subTreeItems.length; i++)
    {     
      boolean sortedFlag = false;
      for (int j = 0; (j <= i) && !sortedFlag; j++)
      {
        switch (sortDirection)
        {
          case SWT.UP:   sortedFlag = (j >= i) || (comparator.compare(subTreeItems[i].getData(),treeItem.getItem(j).getData()) < 0); break;
          case SWT.DOWN: sortedFlag = (j >= i) || (comparator.compare(subTreeItems[i].getData(),treeItem.getItem(j).getData()) > 0); break;
        }
        if (sortedFlag)
        {
          recreateTreeItem(tree,treeItem,subTreeItems[i],j);
        }
      }
    }
//treeItem.setExpanded(xx);

//rr--;
  }

  /** get expanded (open) directories in tree
   * @param expandedDirectories hash-set for expanded directories
   * @param treeItem tree item to start
   */
   private static void getExpandedDiretories(HashSet expandedDirectories, TreeItem treeItem)
   {
     if (treeItem.getExpanded()) expandedDirectories.add(treeItem.getData());
     for (TreeItem subTreeItem : treeItem.getItems())
     {
       getExpandedDiretories(expandedDirectories,subTreeItem);
     }
   }  

  /** re-expand directories
   * @param expandedDirectories directories to re-expand
   * @return treeItem tree item to start
   */
   private static void reExpandDiretories(HashSet expandedDirectories, TreeItem treeItem)
   {
     treeItem.setExpanded(expandedDirectories.contains(treeItem.getData()));
     for (TreeItem subTreeItem : treeItem.getItems())
     {
       reExpandDiretories(expandedDirectories,subTreeItem);
     }
   }  

  /** sort tree column
   * @param tree tree
   * @param tableColumn table column to sort by
   * @param comparator table data comparator
   */
  static void sortTreeColumn(Tree tree, TreeColumn treeColumn, Comparator comparator)
  {
    TreeItem[] treeItems = tree.getItems();

    // get sorting direction
    int sortDirection = tree.getSortDirection();
    if (sortDirection == SWT.NONE) sortDirection = SWT.UP;
    if (tree.getSortColumn() == treeColumn)
    {
      switch (sortDirection)
      {
        case SWT.UP:   sortDirection = SWT.DOWN; break;
        case SWT.DOWN: sortDirection = SWT.UP;   break;
      }
    }

    // save expanded sub-trees.
    // Note: sub-tree cannot be expanded when either no children exist or the
    // parent is not expanded. Because for sort the tree entries they are copied
    // (recreated) the state of the expanded sub-trees are stored here and will
    // late be restored when the complete new tree is created.
    HashSet expandedDirectories = new HashSet();
    for (TreeItem treeItem : tree.getItems())
    {
      getExpandedDiretories(expandedDirectories,treeItem);
    }
//System.err.println("BARControl.java"+", "+1627+": "+expandedDirectories.toString());

    // sort column
//System.err.println("1 ---------------");
//printTree(tree);
    for (TreeItem treeItem : tree.getItems())
    {
      sortSubTreeColumn(tree,treeItem,sortDirection,comparator);
    }

    // restore expanded sub-trees
    for (TreeItem treeItem : tree.getItems())
    {
      reExpandDiretories(expandedDirectories,treeItem);
    }

    // set column sort indicators
    tree.setSortColumn(treeColumn);
    tree.setSortDirection(sortDirection);
//System.err.println("2 ---------------");
//printTree(tree);
  }

  /** create new sash widget
   * @param composite parent composite
   * @return new sash widget
   */
  static Sash newSash(Composite composite, int style)
  {    
    Sash sash = new Sash(composite,style);

    return sash;
  }

  /** create new sash form widget
   * @param composite parent composite
   * @return new sash form widget
   */
  static SashForm newSashForm(Composite composite)
  {    
    SashForm sashForm = new SashForm(composite,SWT.NONE);

    return sashForm;
  }

  /** create new tab folder
   * @param composite parent composite
   * @return new tab folder widget
   */
  static TabFolder newTabFolder(Composite composite)
  {    
    TabFolder tabFolder = new TabFolder(composite,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE));

    return tabFolder;
  }

  /** add tab widget
   * @param tabFolder tab folder
   * @param title title of tab
   * @return new composite widget
   */
  static Composite addTab(TabFolder tabFolder, String title)
  {
    TabItem tabItem = new TabItem(tabFolder,SWT.NONE);
    tabItem.setText(title);
    Composite composite = new Composite(tabFolder,SWT.NONE);
    TableLayout tableLayout = new TableLayout();
    tableLayout.marginTop    = 2;
    tableLayout.marginBottom = 2;
    tableLayout.marginLeft   = 2;
    tableLayout.marginRight  = 2;
    composite.setLayout(tableLayout);
    tabItem.setControl(composite);

    return composite;
  }

  /** show tab
   * @param tabFolder tab folder
   * @param composite tab to show
   */
  static void showTab(TabFolder tabFolder, Composite composite)
  {
    for (TabItem tabItem : tabFolder.getItems())
    {
      if (tabItem.getControl() == composite)
      {
        tabFolder.setSelection(tabItem);
        break;
      }
    }
  }

  /** create new canvas widget
   * @param composite parent composite
   * @param style style
   * @return new canvas widget
   */
  static Canvas newCanvas(Composite composite, int style)
  {    
    Canvas canvas = new Canvas(composite,style);

    return canvas;
  }

  /** create new menu bar
   * @param shell shell
   * @return new menu bar
   */
  static Menu newMenuBar(Shell shell)
  {
    Menu menu = new Menu(shell,SWT.BAR);
    shell.setMenuBar(menu);

    return menu;
  }

  /** create new menu
   * @param menu menu bar
   * @param text menu text
   * @return new menu
   */
  static Menu addMenu(Menu menu, String text)
  {
    MenuItem menuItem = new MenuItem(menu,SWT.CASCADE);
    menuItem.setText(text);
    Menu subMenu = new Menu(menu.getShell(),SWT.DROP_DOWN);
    menuItem.setMenu(subMenu);

    return subMenu;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @param accelerator accelerator key or 0
   * @return new menu item
   */
  static MenuItem addMenuItem(Menu menu, String text, int accelerator)
  {
    if (accelerator != 0)
    {
      char key = (char)(accelerator & SWT.KEY_MASK);
      int index = text.indexOf(key);
      if (index >= 0)
      {
        text = text.substring(0,index)+'&'+text.substring(index);
      }
      text = text+"\t"+acceleratorToText(accelerator);
    }
    MenuItem menuItem = new MenuItem(menu,SWT.DROP_DOWN);
    menuItem.setText(text);
    if (accelerator != 0) menuItem.setAccelerator(accelerator);

    return menuItem;
  }

  /** add new menu item
   * @param menu menu
   * @param text menu item text
   * @return new menu item
   */
  static MenuItem addMenuItem(Menu menu, String text)
  {
    return addMenuItem(menu,text,0);
  }

  /** add new menu separator
   * @param menu menu
   * @return new menu item
   */
  static MenuItem addMenuSeparator(Menu menu)
  {
    MenuItem menuItem = new MenuItem(menu,SWT.SEPARATOR);

    return menuItem;
  }

  //-----------------------------------------------------------------------

  /** add new composite widget
   * @param composite parent composite widget
   * @param style style
   * @param margin margin or 0
   * @return new composite widget
   */
  static Composite newComposite(Composite composite, int style, int margin)
  {
    Composite childComposite;

    childComposite = new Composite(composite,style);
    TableLayout tableLayout = new TableLayout(margin);
    childComposite.setLayout(tableLayout);

    return childComposite;
  }

  /** add new composite widget
   * @param composite parent composite widget
   * @param style style
   * @return new composite widget
   */
  static Composite newComposite(Composite composite, int style)
  {
    return newComposite(composite,style,0);
  }

  /** add new group widget
   * @param composite parent composite widget
   * @param title group title
   * @param style style
   * @param margin margin or 0
   * @return new group widget
   */
  static Group newGroup(Composite composite, String title, int style, int margin)
  {
    Group group;

    group = new Group(composite,style);
    group.setText(title);
    TableLayout tableLayout = new TableLayout(margin);
    group.setLayout(tableLayout);

    return group;
  }

  /** add new group widget
   * @param composite parent composite widget
   * @param title group title
   * @param style style
   * @return new group widget
   */
  static Group newGroup(Composite composite, String title, int style)
  {
    return newGroup(composite,title,style,0);
  }

  //-----------------------------------------------------------------------

  /** add modify listener
   * @param widgetListener listener to add
   */
  static void addModifyListener(WidgetListener widgetListener)
  {
    listenersList.add(widgetListener);
  }

  /** execute modify listeners
   * @param variable modified variable
   */
  static void modified(Object variable)
  {
    for (WidgetListener widgetListener : listenersList)
    {
      if (widgetListener.equals(variable))
      {
        widgetListener.modified();
      }
    }
  }

  /** signal modified
   * @param control control
   * @param type event type to generate
   * @param widget widget of event
   * @param item item of event
   */
  static void notify(Control control, int type, Widget widget, Widget item)
  {
    if (control.isEnabled())
    {
      Event event = new Event();
      event.widget = widget;
      event.item   = item;
      control.notifyListeners(type,event);
    }
  }

  /** signal modified
   * @param control control
   * @param type event type to generate
   * @param widget widget of event
   */
  static void notify(Control control, int type, Widget widget)
  {
    notify(control,type,widget,null);
  }

  /** signal modified
   * @param control control
   * @param type event type to generate
   */
  static void notify(Control control, int type)
  {
    notify(control,type,control,null);
  }

  /** signal modified
   * @param control control
   */
  static void notify(Control control)
  {
    notify(control,SWT.Selection,control,null);
  }
}

/* end of file */
