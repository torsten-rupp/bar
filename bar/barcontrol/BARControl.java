/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/BARControl.java,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents:
* Systems :
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.lang.Double;
import java.lang.Exception;
import java.lang.Integer;
import java.lang.Long;
import java.lang.String;
import java.lang.System;
import java.net.Socket;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.LinkedList;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.PaletteData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.RGB;
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
//import org.eclipse.swt.widgets.ProgressBar;
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

//import org.eclipse.swt.layout.FillLayout;
//import org.eclipse.swt.layout.GridData;
//import org.eclipse.swt.layout.GridLayout;

import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;

/****************************** Classes ********************************/

class ProgressBar extends Canvas
{
  Color  barColor;
  Color  barSetColor;
  double minimum;
  double maximum;
  double value;
  Point  textSize;
  String text;

  ProgressBar(Composite composite, int style)
  {
    super(composite,SWT.NONE);

    barColor    = getDisplay().getSystemColor(SWT.COLOR_WHITE);
    barSetColor = new Color(null,0xAD,0xD8,0xE6);

    addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposeEvent)
      {
        ProgressBar.this.widgetDisposed(disposeEvent);
      }
    });
    addPaintListener(new PaintListener()
    {
      public void paintControl(PaintEvent paintEvent)
      {
        ProgressBar.this.paintControl(paintEvent);
      }
    });

    setSelection(0.0);
  }

  void widgetDisposed(DisposeEvent disposeEvent)
  {
    barSetColor.dispose();
    barColor.dispose();
  }

  void paintControl(PaintEvent paintEvent)
  {
    GC        gc;
    Rectangle bounds;
    int       x,y,w,h;

    gc = paintEvent.gc;
    bounds = getBounds();
    x = 0;
    y = 0;
    w = bounds.width;
    h = bounds.height;

    // shadow
    gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_DARK_GRAY));
    gc.drawLine(x,y,  x+w-1,y    );
    gc.drawLine(x,y+1,x,    y+h-1);

    gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_WHITE));
    gc.drawLine(x+1,  y+h-1,x+w-1,y+h-1);
    gc.drawLine(x+w-1,y+1,  x+w-1,y+h-1);

    // draw bar
    gc.setBackground(barColor);
    gc.fillRectangle(x+1,y+1,w-2,h-2);
    gc.setBackground(barSetColor);
    gc.fillRectangle(x+1,y+1,(int)((double)w*value)-2,h-2);

    // draw percentage text
    gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_BLACK));
    gc.drawString(text,(w-textSize.x)/2,(h-textSize.y)/2,true);
  }

  public Point computeSize(int wHint, int hHint, boolean changed)
  {
    GC gc;
    int width,height;

    width  = 0;
    height = 0;

    width  = textSize.x;
    height = textSize.y;
    if (wHint != SWT.DEFAULT) width = wHint;
    if (hHint != SWT.DEFAULT) height = hHint;         

    return new Point(width,height);
  }

  void setMinimum(double n)
  {
    this.minimum = n;
  }

  void setMaximum(double n)
  {
    this.maximum = n;
  }

  void setSelection(double n)
  {
    GC gc;

    value = Math.min(Math.max(((maximum-minimum)>0.0)?(n-minimum)/(maximum-minimum):0.0,minimum),maximum);

    gc = new GC(this);
    text = String.format("%.1f%%",value*100.0);
    textSize = gc.stringExtent(text);
    gc.dispose();

    redraw();

  }
}

enum BARVariableTypes
{
  LONG,
  DOUBLE,
  STRING,
  ENUMERATION,
};

class BARVariable
{
  private BARVariableTypes type;
  private long             n;
  private double           d;
  private String           string;
  private String           enumeration[];

  BARVariable(long n)
  {
    this.type = BARVariableTypes.LONG;
    this.n    = n;
  }
  BARVariable(double d)
  {
    this.type = BARVariableTypes.DOUBLE;
    this.d    = d;
  }
  BARVariable(String string)
  {
    this.type   = BARVariableTypes.STRING;
    this.string = string;
  }
  BARVariable(String enumeration[])
  {
    this.type        = BARVariableTypes.ENUMERATION;
    this.enumeration = enumeration;
  }

  BARVariableTypes getType()
  {
    return type;
  }

  long getLong()
  {
    assert type == BARVariableTypes.LONG;

    return n;
  }

  double getDouble()
  {
    assert type == BARVariableTypes.DOUBLE;

    return d;
  }

  String getString()
  {
    assert type == BARVariableTypes.STRING;

    return string;
  }

  void set(long n)
  {
    assert type == BARVariableTypes.LONG;

    this.n = n;
    Widgets.modified(this);
  }
  void set(double d)
  {
    assert type == BARVariableTypes.DOUBLE;

    this.d = d;
    Widgets.modified(this);
  }

  void set(String string)
  {
    assert (type == BARVariableTypes.STRING) || (type == BARVariableTypes.ENUMERATION);

    switch (type)
    {
      case STRING:
        this.string = string;
        break;
      case ENUMERATION:
        boolean OKFlag = true;
        for (String s : enumeration)
        {
          if (s.equals(string))
          {
            OKFlag = true;
          }
        }
        if (!OKFlag) throw new Error("Unknown enumeration value '"+string+"'");
        this.string = string;
        break;
    }
System.err.println("BARControl.java"+", "+298+": ");
    Widgets.modified(this);
  }

  public boolean equals(String string)
  {
    return toString().equals(string);
  }

  public String toString()
  {
    switch (type)
    {
      case LONG  :      return Long.toString(n);
      case DOUBLE:      return Double.toString(d);
      case STRING:      return string;
      case ENUMERATION: return string;
    }
    return null;
  }
}

class BARServer
{
  private Socket             socket;
  private PrintWriter        output;
  private BufferedReader     input;
  private long               commandId;
  private LinkedList<String> lines;

  private byte[] decodeHex(String s)
  {
    byte data[] = new byte[s.length()/2];
    for (int z = 0; z < s.length()/2; z++)
    {
      data[z] = (byte)Integer.parseInt(s.substring(z*2,z*2+2),16);
    }

    return data;
  }

  private String encodeHex(byte data[])
  {
    StringBuffer stringBuffer = new StringBuffer(data.length*2);
    for (int z = 0; z < data.length; z++)
    {
      stringBuffer.append(String.format("%02x",(int)data[z] & 0xFF));
    }

    return stringBuffer.toString();
  }

  BARServer(String hostname, int port, int tlsPort, String serverPassword)
  {
    commandId = 0;

    // connect to server
    if (tlsPort != 0)
    {
System.setProperty("javax.net.ssl.trustStore","bar.jks");
      SSLSocket sslSocket;

      try
      {
        SSLSocketFactory sslSocketFactory;

        sslSocketFactory = (SSLSocketFactory)SSLSocketFactory.getDefault();
        sslSocket        = (SSLSocket)sslSocketFactory.createSocket("localhost",38524);
        sslSocket.startHandshake();

        socket = sslSocket;
        input  = new BufferedReader(new InputStreamReader(sslSocket.getInputStream()));
        output = new PrintWriter(new BufferedWriter(new OutputStreamWriter(sslSocket.getOutputStream())));
      }
      catch (Exception exception)
      {
        exception.printStackTrace();
      }
    }
    else if (port != 0)
    {
      socket = null;
    }
    else
    {
      
    }

    // read session id
    byte sessionId[];
    try
    {
      String line;

      line = input.readLine();
      assert line != null;
      String data[] = line.split(" ",2);
      assert data.length == 2;
      assert data[0].equals("SESSION");
      sessionId = decodeHex(data[1]);
    }
    catch (IOException exception)
    {
      throw new Error("read session id");
    }
//System.err.print("BARControl.java"+", "+682+": sessionId=");for (byte b : sessionId) { System.err.print(String.format("%02x",b & 0xFF)); }; System.err.println();

    // authorize
    try
    {
      byte authorizeData[] = new byte[sessionId.length];
      for (int z = 0; z < sessionId.length; z++)
      {
        authorizeData[z] = (byte)(((z < serverPassword.length())?(int)serverPassword.charAt(z):0)^(int)sessionId[z]);
      }
      commandId++;
      String command = Long.toString(commandId)+" AUTHORIZE "+encodeHex(authorizeData);
      output.println(command);
      output.flush();
//System.err.println("BARControl.java"+", "+230+": auto command "+command);

      String result = input.readLine();
      String data[] = result.split(" ",4);
      assert data.length >= 3;
      if (   (Integer.parseInt(data[0]) != commandId)
          || (Integer.parseInt(data[1]) != 1)
          || (Integer.parseInt(data[2]) != 0)
         )
      {
        throw new Error("authorize fail");
      }
    }
    catch (IOException exception)
    {
      throw new Error("authorize fail");
    }
  }

  void disconnect()
  {
    try
    {
      input.close();
      output.close();
      socket.close();
    }
    catch (IOException exception)
    {
      // ignored
    }
  }

  synchronized int executeCommand(String command, Object result)
  {
    String  line;
    boolean completedFlag;
    int     errorCode;

    // send command
    commandId++;
    line = String.format("%d %s",commandId,command);
    output.println(line);
    output.flush();
//System.err.println("BARControl.java"+", "+279+": sent "+line);

    // read buffer lines from list
//???

    // read lines
    completedFlag = false;
    errorCode = -1;
    try
    {
      while (!completedFlag && (line = input.readLine()) != null)
      {
//System.err.println("BARControl.java"+", "+701+": received="+line);

        // line format: <id> <error code> <completed> <data>
        String data[] = line.split(" ",4);
        assert data.length == 4;
        if (Integer.parseInt(data[0]) == commandId)
        {
          /* check if completed */
          if (Integer.parseInt(data[1]) != 0)
          {
            errorCode = Integer.parseInt(data[2]);
            completedFlag = true;
          }

          /* store data */
          if      (result instanceof ArrayList)
          {
            ((ArrayList<String>)result).add(data[3]);
          }
          else if (result instanceof String[])
          {
            ((String[])result)[0] = data[3];
          }
          else
          {
            throw new Error("Invalid result data type");
          }
        }
        else
        {
          lines.add(line);
        }
      }
    }
    catch (IOException exception)
    {
      throw new Error("execute command fail");
    }

    return errorCode;
  }
}

  //-----------------------------------------------------------------------

class WidgetListener
{
  private Control     control;
  private BARVariable variable;

  // cached text for widget
  private String cachedText = null;

  WidgetListener()
  {
    this.control  = null;
    this.variable = null;
  }

  WidgetListener(Control control, BARVariable variable)
  {
    this.control  = control;
    this.variable = variable;
  }

  void setControl(Control control)
  {
    this.control = control;
  }

  void setVariable(BARVariable variable)
  {
    this.variable = variable;
  }

  public boolean equals(Object variable)
  {
    return (this.variable != null) && this.variable.equals(variable);
  }

  void modified(Control control, BARVariable variable)
  {
    if      (control instanceof Label)
    {
      String text = getString(variable);
      if (text == null)
      {
        switch (variable.getType())
        {
          case LONG:   text = Long.toString(variable.getLong()); break; 
          case DOUBLE: text = Double.toString(variable.getDouble()); break; 
          case STRING: text = variable.getString(); break; 
        }
      }
      if (!text.equals(cachedText))
      {
        ((Label)control).setText(text);
        control.getParent().layout(true,true);
        cachedText = text;
      }
    }
    else if (control instanceof Button)
    {
      String text = getString(variable);
      if (text == null)
      {
        switch (variable.getType())
        {
          case LONG:   text = Long.toString(variable.getLong()); break; 
          case DOUBLE: text = Double.toString(variable.getDouble()); break; 
          case STRING: text = variable.getString(); break; 
        }
      }
      if (!text.equals(cachedText))
      {
        ((Button)control).setText(text);
        control.getParent().layout();
        cachedText = text;
      }
    }
    else if (control instanceof Text)
    {
      String text = getString(variable);
      if (text == null)
      {
        switch (variable.getType())
        {
          case LONG:   text = Long.toString(variable.getLong()); break; 
          case DOUBLE: text = Double.toString(variable.getDouble()); break; 
          case STRING: text = variable.getString(); break; 
        }
      }
      if (!text.equals(cachedText))
      {
        ((Text)control).setText(text);
        control.getParent().layout();
        cachedText = text;
      }
    }
    else if (control instanceof ProgressBar)
    {
      double value = 0;
      switch (variable.getType())
      {
        case LONG:   value = (double)variable.getLong(); break; 
        case DOUBLE: value = variable.getDouble(); break; 
      }
      ((ProgressBar)control).setSelection(value);
    }
  }

  public void modified(BARVariable variable)
  {
    modified(control,variable);
  }

  public void modified()
  {
    modified(variable);
  }

  String getString(BARVariable variable)
  {
    return null;
  }
}

class Widgets
{
  //-----------------------------------------------------------------------

  static LinkedList<WidgetListener> listenersList = new LinkedList<WidgetListener>();

  //-----------------------------------------------------------------------

  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int width, int height)
  {
    TableLayoutData tableLayoutData = new TableLayoutData(row,column,style,rowSpawn,columnSpawn);
//    tableLayoutData.setSize(width,height);
    control.setLayoutData(tableLayoutData);
  }

  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,SWT.DEFAULT,SWT.DEFAULT);
  }

  static void layout(Control control, int row, int column, int style)
  {
    layout(control,row,column,style,0,0);
  }

  static Label newLabel(Composite composite, String text)
  {
    Label label;

    label = new Label(composite,SWT.LEFT);
    label.setText(text);

    return label;
  }

  static Label newLabel(Composite composite)
  {
    return newLabel(composite,"");
  }

  static Label newView(Composite composite)
  {
    Label label;

    label = new Label(composite,SWT.LEFT|SWT.BORDER);
    label.setText("");

    return label;
  }

  static Label newNumberView(Composite composite)
  {
    Label label;

    label = new Label(composite,SWT.RIGHT|SWT.BORDER);
    label.setText("0");

    return label;
  }

  static Label newStringView(Composite composite)
  {
    Label label;

    label = new Label(composite,SWT.LEFT|SWT.BORDER);
    label.setText("");

    return label;
  }

  static Button newButton(Composite composite, Object data, String text)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setText(text);
    button.setData(data);

    return button;
  }

  static Button newButton(Composite composite, Object data, Image image)
  {
    Button button;

    button = new Button(composite,SWT.PUSH);
    button.setImage(image);
    button.setData(data);

    return button;
  }

  static Button newCheckbox(Composite composite, Object data, String text)
  {
    Button button;

    button = new Button(composite,SWT.CHECK);
    button.setText(text);
    button.setData(data);

    return button;
  }

  static Button newRadio(Composite composite, Object data, String text)
  {
    Button button;

    button = new Button(composite,SWT.RADIO);
    button.setText(text);
    button.setData(data);

    return button;
  }

/*
  static Text newText(Composite composite, Object data)
  {
    Text text;

    text = new Text(composite,SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE|SWT.READ_ONLY);
    text.setData(data);

    return text;
  }
*/

  static Text newText(Composite composite, Object data)
  {
    Text text;

    text = new Text(composite,SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE);
    text.setData(data);

    return text;
  }

  static List newList(Composite composite, Object data, Listener listener)
  {
    List list;

    list = new List(composite,SWT.BORDER|SWT.MULTI|SWT.V_SCROLL);
    list.setData(data);

    if (listener != null) list.addListener(SWT.Selection,listener);

    return list;
  }

  static Combo newCombo(Composite composite, Object data, Listener listener)
  {
    Combo combo;

    combo = new Combo(composite,SWT.BORDER);
    combo.setData(data);

    if (listener != null) combo.addListener(SWT.Selection,listener);

    return combo;
  }

  static Combo newOptionMenu(Composite composite, Object data, Listener listener)
  {
    Combo combo;

    combo = new Combo(composite,SWT.RIGHT|SWT.READ_ONLY);
    combo.setData(data);

    if (listener != null) combo.addListener(SWT.Selection,listener);

    return combo;
  }

  static Table newTable(Composite composite, Object data, Listener listener)
  {
    Table table;

    table = new Table(composite,SWT.BORDER|SWT.MULTI|SWT.FULL_SELECTION);
    table.setLinesVisible(true);
    table.setHeaderVisible(true);
    table.setData(data);

    if (listener != null) table.addListener(SWT.Selection,listener);

    return table;
  }

  static void addTableColumn(Table table, String title, int style, int width, boolean resizable)
  {
    TableColumn tableColumn = new TableColumn(table,style);
    tableColumn.setText(title);
    tableColumn.setWidth(width);
    tableColumn.setResizable(resizable);
    if (width <= 0) tableColumn.pack();
  }

  static ProgressBar newProgressBar(Composite composite, Object variable)
  {
    ProgressBar progressBar;

    progressBar = new ProgressBar(composite,SWT.HORIZONTAL);
    progressBar.setMinimum(0);
    progressBar.setMaximum(100);
    progressBar.setSelection(0);

    return progressBar;
  }

  static Tree newTree(Composite composite, Object variable)
  {
    Tree tree;

    tree = new Tree(composite,SWT.BORDER|SWT.H_SCROLL|SWT.V_SCROLL);
    tree.setHeaderVisible(true);

    return tree;
  }

  static void addTreeColumn(Tree tree, String title, int style, int width, boolean resizable)
  {
    TreeColumn treeColumn = new TreeColumn(tree,style);
    treeColumn.setText(title);
    treeColumn.setWidth(width);
    treeColumn.setResizable(resizable);
    if (width <= 0) treeColumn.pack();
  }

  static TreeItem addTreeItem(Tree tree, int index, Object data, boolean folderFlag)
  {
    TreeItem treeItem;

    treeItem = new TreeItem(tree,SWT.NONE,index);
    treeItem.setData(data);
    if (folderFlag) new TreeItem(treeItem,SWT.NONE);

    return treeItem;
  }

  static TreeItem addTreeItem(Tree tree, Object data, boolean folderFlag)
  {
    return addTreeItem(tree,0,data,folderFlag);
  }

  static TreeItem addTreeItem(TreeItem parentTreeItem, int index, Object data, boolean folderFlag)
  {
    TreeItem treeItem;

    treeItem = new TreeItem(parentTreeItem,SWT.NONE,index);
    treeItem.setData(data);
    if (folderFlag) new TreeItem(treeItem,SWT.NONE);

    return treeItem;
  }

  static TreeItem addTreeItem(TreeItem parentTreeItem, String name, Object data, boolean folderFlag)
  {
    return addTreeItem(parentTreeItem,0,data,folderFlag);
  }

/*
  static void remAllTreeItems(Tree tree, boolean folderFlag)
  {
    tree.removeAll();
    if (folderFlag) new TreeItem(tree,SWT.NONE);
  }

  static void remAllTreeItems(TreeItem treeItem, boolean folderFlag)
  {
    treeItem.removeAll();
    if (folderFlag) new TreeItem(treeItem,SWT.NONE);
  }
*/

  static SashForm newSashForm(Composite composite)
  {    
    SashForm sashForm = new SashForm(composite,SWT.NONE);
    sashForm.setLayout(new TableLayout());
    layout(sashForm,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);

    return sashForm;
  }

  static TabFolder newTabFolder(Composite composite)
  {    
    TabFolder tabFolder = new TabFolder(composite,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.FILL|TableLayoutData.EXPAND));

    return tabFolder;
  }

/*
  static TabFolder newTabFolder(Composite composite)
  {    
    // create resizable tab (with help of sashForm)
    SashForm sashForm = new SashForm(composite,SWT.NONE);
    sashForm.setLayout(new TableLayout());
    layout(sashForm,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    TabFolder tabFolder = new TabFolder(sashForm,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.FILL|TableLayoutData.EXPAND));

    return tabFolder;
  }
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

  //-----------------------------------------------------------------------

  static Composite newComposite(Composite composite, int style)
  {
    Composite childComposite;

    childComposite = new Composite(composite,style);
    TableLayout tableLayout = new TableLayout();
    childComposite.setLayout(tableLayout);

    return childComposite;
  }

  static Group newGroup(Composite composite, String title, int style)
  {
    Group group;

    group = new Group(composite,style);
    group.setText(title);
    TableLayout tableLayout = new TableLayout();
    tableLayout.marginTop    = 4;
    tableLayout.marginBottom = 4;
    tableLayout.marginLeft   = 4;
    tableLayout.marginRight  = 4;
    group.setLayout(tableLayout);

    return group;
  }

  //-----------------------------------------------------------------------

  static void addModifyListener(WidgetListener widgetListener)
  {
    listenersList.add(widgetListener);
  }

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
}

class TabStatus
{
  final Shell shell;
  Composite   tab;
  Table       jobList;
  Group       selectedJob;
  Button      buttonStart;
  Button      buttonAbort;
  Button      buttonPause;
  Button      buttonVolume;

  BARVariable doneFiles             = new BARVariable(0);
  BARVariable doneBytes             = new BARVariable(0);
  BARVariable storedBytes           = new BARVariable(0);
  BARVariable skippedFiles          = new BARVariable(0);
  BARVariable skippedBytes          = new BARVariable(0);
  BARVariable errorFiles            = new BARVariable(0);
  BARVariable errorBytes            = new BARVariable(0);
  BARVariable totalFiles            = new BARVariable(0);
  BARVariable totalBytes            = new BARVariable(0);

  BARVariable filesPerSecond        = new BARVariable(0.0); 
  BARVariable bytesPerSecond        = new BARVariable(0.0);
  BARVariable storageBytesPerSecond = new BARVariable(0.0);
  BARVariable ratio                 = new BARVariable(0.0);

  BARVariable fileName              = new BARVariable("");
  BARVariable fileProgress          = new BARVariable(0.0);
  BARVariable storageName           = new BARVariable("");
  BARVariable storageProgress       = new BARVariable(0.0);
  BARVariable volumeNumber          = new BARVariable(0);
  BARVariable volumeProgress        = new BARVariable(0.0);
  BARVariable totalFilesProgress    = new BARVariable(0.0);
  BARVariable totalBytesProgress    = new BARVariable(0.0);
  BARVariable message               = new BARVariable("");

  int         selectedJobId         = 0;

  class TabStatusUpdateThread extends Thread
  {
    private BARServer barServer;
    private TabStatus tabStatus;

    TabStatusUpdateThread(BARServer barServer, TabStatus tabStatus)
    {
      this.barServer = barServer;
      this.tabStatus = tabStatus;
    }

    public void run()
    {
      for (;;)
      {
        try
        {
          tabStatus.tab.getDisplay().syncExec(new Runnable()
          {
            public void run()
            {
              tabStatus.update(barServer);
            }
          });
        }
        catch (org.eclipse.swt.SWTException exception)
        {
          // ignore SWT exceptions
        }

        try { Thread.sleep(1000); } catch (InterruptedException exception) {};
      }
    }
  }

  TabStatus(BARServer barServer, TabFolder parentTabFolder)
  {
    Group     group;
    Composite composite;
    Control   widget;
    Label     label;

    // get shell
    shell = parentTabFolder.getShell();

    // create tab
    this.tab = Widgets.addTab(parentTabFolder,"Status");
    this.tab.setLayout(new TableLayout(new double[]{1.0,0.0,0.0},
                                       null,
                                       2
                                      )
                      );
    Widgets.layout(this.tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);

    // list with jobs
    jobList = Widgets.newTable(this.tab,this,new Listener()
    {
      public void handleEvent (Event event)
      {
        TabStatus tabStatus = (TabStatus)event.widget.getData();

        tabStatus.selectedJobId = (Integer)event.item.getData();

        TableItem tableItems[] = jobList.getItems();
        int index = getTableItemIndex(tableItems,tabStatus.selectedJobId);
        tabStatus.selectedJob.setText("Selected '"+tableItems[index].getText(1)+"'");

        tabStatus.buttonStart.setEnabled(true);
//        tabStatus.buttonAbort.setEnabled(true);
//        tabStatus.buttonPause.setEnabled(true);
//        tabStatus.buttonVolume.setEnabled(true);
      }
    });
    Widgets.layout(jobList,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    Widgets.addTableColumn(jobList,"#",             SWT.RIGHT, 30,false);
    Widgets.addTableColumn(jobList,"Name",          SWT.LEFT, 100,true );
    Widgets.addTableColumn(jobList,"State",         SWT.LEFT,  60,true );
    Widgets.addTableColumn(jobList,"Type",          SWT.LEFT,   0,true );
    Widgets.addTableColumn(jobList,"Part size",     SWT.RIGHT,  0,true );
    Widgets.addTableColumn(jobList,"Compress",      SWT.LEFT,  100,true );
    Widgets.addTableColumn(jobList,"Crypt",         SWT.LEFT,  80,true );
    Widgets.addTableColumn(jobList,"Last executed", SWT.LEFT, 180,true );
    Widgets.addTableColumn(jobList,"Estimated time",SWT.LEFT, 120,true );

    // selected job group
    selectedJob = Widgets.newGroup(this.tab,"Selected ''",SWT.NONE);
    selectedJob.setLayout(new TableLayout(null,
                                          new double[]{0.0,1.0,0.0,1.0},
                                          4
                                         )
                         );
    Widgets.layout(selectedJob,1,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND);
    {
      // done files/bytes
      widget = Widgets.newLabel(selectedJob,"Done:");
      Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
      widget= Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,doneFiles));
      widget = Widgets.newLabel(selectedJob,"files");
      Widgets.layout(widget,0,2,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,0,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,doneBytes));
      widget = Widgets.newLabel(selectedJob,"bytes");
      Widgets.layout(widget,0,4,TableLayoutData.DEFAULT);
      widget = Widgets.newLabel(selectedJob,"/");
      Widgets.layout(widget,0,5,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,0,6,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,doneBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteSize(variable.getLong());
        }
      });
      widget = Widgets.newLabel(selectedJob);
      Widgets.layout(widget,0,7,TableLayoutData.DEFAULT,0,0,SWT.DEFAULT,50);
      Widgets.addModifyListener(new WidgetListener(widget,doneBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteUnit(variable.getLong());
        }
      });

      // stored files/bytes
      widget = Widgets.newLabel(selectedJob,"Stored:");
      Widgets.layout(widget,1,0,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,1,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,storedBytes));
      widget = Widgets.newLabel(selectedJob,"bytes");
      Widgets.layout(widget,1,4,TableLayoutData.DEFAULT);
      widget = Widgets.newLabel(selectedJob,"/");
      Widgets.layout(widget,1,5,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.addModifyListener(new WidgetListener(widget,storedBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteSize(variable.getLong());
        }
      });
      Widgets.layout(widget,1,6,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget = Widgets.newLabel(selectedJob);
      Widgets.layout(widget,1,7,TableLayoutData.DEFAULT);
      Widgets.addModifyListener(new WidgetListener(widget,storedBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteUnit(variable.getLong());
        }
      });

      composite = Widgets.newComposite(selectedJob,SWT.NONE);
      Widgets.layout(composite,1,8,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget = Widgets.newLabel(composite,"Ratio");
      Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(composite);
      Widgets.layout(widget,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,ratio)
      {
        public String getString(BARVariable variable)
        {
          return String.format("%.1f",variable.getDouble());
        }
      });
      widget = Widgets.newLabel(composite,"%");
      Widgets.layout(widget,0,2,TableLayoutData.DEFAULT);

      composite = Widgets.newComposite(selectedJob,SWT.NONE);
      Widgets.layout(composite,1,9,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget = Widgets.newNumberView(composite);
      Widgets.layout(widget,0,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,storageBytesPerSecond)
      {
        public String getString(BARVariable variable)
        {
          return getByteSize(variable.getDouble());
        }
      });
      widget = Widgets.newLabel(composite);
      Widgets.layout(widget,0,1,TableLayoutData.DEFAULT);
      Widgets.addModifyListener(new WidgetListener(widget,storageBytesPerSecond)
      {
        public String getString(BARVariable variable)
        {
          return getByteUnit(variable.getDouble())+"/s";
        }
      });

      // skipped files/bytes, ratio
      widget = Widgets.newLabel(selectedJob,"Skipped:");
      Widgets.layout(widget,2,0,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,2,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,skippedFiles));
      widget = Widgets.newLabel(selectedJob,"files");
      Widgets.layout(widget,2,2,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,2,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,skippedBytes));
      widget = Widgets.newLabel(selectedJob,"bytes");
      Widgets.layout(widget,2,4,TableLayoutData.DEFAULT);
      widget = Widgets.newLabel(selectedJob,"/");
      Widgets.layout(widget,2,5,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.addModifyListener(new WidgetListener(widget,skippedBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteSize(variable.getLong());
        }
      });
      Widgets.layout(widget,2,6,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget = Widgets.newLabel(selectedJob);
      Widgets.addModifyListener(new WidgetListener(widget,skippedBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteUnit(variable.getLong());
        }
      });
      Widgets.layout(widget,2,7,TableLayoutData.DEFAULT);
      Widgets.addModifyListener(new WidgetListener(widget,doneBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteUnit(variable.getLong());
        }
      });

      // error files/bytes
      widget = Widgets.newLabel(selectedJob,"Errors:");
      Widgets.layout(widget,3,0,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,3,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,errorFiles));
      widget = Widgets.newLabel(selectedJob,"files");
      Widgets.layout(widget,3,2,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,3,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,errorFiles));
      widget = Widgets.newLabel(selectedJob,"bytes");
      Widgets.layout(widget,3,4,TableLayoutData.DEFAULT);
      widget = Widgets.newLabel(selectedJob,"/");
      Widgets.layout(widget,3,5,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.addModifyListener(new WidgetListener(widget,errorBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteSize(variable.getLong());
        }
      });
      Widgets.layout(widget,3,6,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget = Widgets.newLabel(selectedJob);
      Widgets.layout(widget,3,7,TableLayoutData.DEFAULT);
      Widgets.addModifyListener(new WidgetListener(widget,errorBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteUnit(variable.getLong());
        }
      });

      // total files/bytes, files/s, bytes/s
      widget = Widgets.newLabel(selectedJob,"Total:");
      Widgets.layout(widget,4,0,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,4,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,totalFiles));
      widget = Widgets.newLabel(selectedJob,"files");
      Widgets.layout(widget,4,2,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.layout(widget,4,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,totalBytes));
      widget = Widgets.newLabel(selectedJob,"bytes");
      Widgets.layout(widget,4,4,TableLayoutData.DEFAULT);
      widget = Widgets.newLabel(selectedJob,"/");
      Widgets.layout(widget,4,5,TableLayoutData.DEFAULT);
      widget = Widgets.newNumberView(selectedJob);
      Widgets.addModifyListener(new WidgetListener(widget,totalBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteSize(variable.getLong());
        }
      });
      Widgets.layout(widget,4,6,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget = Widgets.newLabel(selectedJob);
      Widgets.layout(widget,4,7,TableLayoutData.DEFAULT);
      Widgets.addModifyListener(new WidgetListener(widget,totalBytes)
      {
        public String getString(BARVariable variable)
        {
          return getByteUnit(variable.getLong());
        }
      });

      composite = Widgets.newComposite(selectedJob,SWT.NONE);
      Widgets.layout(composite,4,8,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget = Widgets.newNumberView(composite);
      Widgets.layout(widget,0,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,filesPerSecond)
      {
        public String getString(BARVariable variable)
        {
          return String.format("%.1f",variable.getDouble());
        }
      });
      widget = Widgets.newLabel(composite,"files/s");
      Widgets.layout(widget,0,1,TableLayoutData.DEFAULT);

      composite = Widgets.newComposite(selectedJob,SWT.NONE);
      Widgets.layout(composite,4,9,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget = Widgets.newNumberView(composite);
      Widgets.layout(widget,0,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      Widgets.addModifyListener(new WidgetListener(widget,bytesPerSecond)
      {
        public String getString(BARVariable variable)
        {
          return getByteSize(variable.getDouble());
        }
      });
      widget = Widgets.newLabel(composite);
      Widgets.layout(widget,0,1,TableLayoutData.DEFAULT);
      Widgets.addModifyListener(new WidgetListener(widget,bytesPerSecond)
      {
        public String getString(BARVariable variable)
        {
          return getByteUnit(variable.getDouble())+"/s";
        }
      });

      // current file, file percentage
      widget = Widgets.newLabel(selectedJob,"File:");
      Widgets.layout(widget,5,0,TableLayoutData.DEFAULT);
      widget = Widgets.newView(selectedJob);
      Widgets.layout(widget,5,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y,0,9);
      Widgets.addModifyListener(new WidgetListener(widget,fileName));
      widget = Widgets.newProgressBar(selectedJob,null);
      Widgets.layout(widget,6,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y,0,9);
      Widgets.addModifyListener(new WidgetListener(widget,fileProgress));

      // storage file, storage percentage
      widget = Widgets.newLabel(selectedJob,"Storage:");
      Widgets.layout(widget,7,0,TableLayoutData.DEFAULT);
      widget = Widgets.newView(selectedJob);
      Widgets.layout(widget,7,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y,0,9);
      Widgets.addModifyListener(new WidgetListener(widget,storageName));
      widget = Widgets.newProgressBar(selectedJob,null);
      Widgets.layout(widget,8,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y,0,9);
      Widgets.addModifyListener(new WidgetListener(widget,storageProgress));

      // volume percentage
      widget = Widgets.newLabel(selectedJob,"Volume:");
      Widgets.layout(widget,9,0,TableLayoutData.DEFAULT);
      widget = Widgets.newProgressBar(selectedJob,null);
      Widgets.layout(widget,9,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y,0,9);
      Widgets.addModifyListener(new WidgetListener(widget,volumeProgress));

      // total files percentage
      widget = Widgets.newLabel(selectedJob,"Total files:");
      Widgets.layout(widget,10,0,TableLayoutData.DEFAULT);
      widget = Widgets.newProgressBar(selectedJob,null);
      Widgets.layout(widget,10,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y,0,9);
      Widgets.addModifyListener(new WidgetListener(widget,totalFilesProgress));

      // total bytes percentage
      widget = Widgets.newLabel(selectedJob,"Total bytes:");
      Widgets.layout(widget,11,0,TableLayoutData.DEFAULT);
      widget = Widgets.newProgressBar(selectedJob,null);
      Widgets.layout(widget,11,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y,0,9);
      Widgets.addModifyListener(new WidgetListener(widget,totalBytesProgress));

      // message
      widget = Widgets.newLabel(selectedJob,"Message:");
      Widgets.layout(widget,12,0,TableLayoutData.DEFAULT);
      widget = Widgets.newView(selectedJob);
      Widgets.layout(widget,12,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y,0,9);
      Widgets.addModifyListener(new WidgetListener(widget,message));
    }

    // buttons
    composite = Widgets.newComposite(this.tab,SWT.NONE);
    Widgets.layout(composite,2,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
    {
      buttonStart = Widgets.newButton(composite,null,"Start");
      Widgets.layout(buttonStart,0,0,TableLayoutData.DEFAULT);
      buttonStart.setEnabled(false);
      buttonAbort = Widgets.newButton(composite,null,"Abort");
      Widgets.layout(buttonAbort,0,1,TableLayoutData.DEFAULT);
      buttonAbort.setEnabled(false);
      buttonPause = Widgets.newButton(composite,null,"Pause");
      Widgets.layout(buttonPause,0,2,TableLayoutData.DEFAULT);
      buttonPause.setEnabled(false);
      buttonVolume = Widgets.newButton(composite,null,"Volume");
      Widgets.layout(buttonVolume,0,3,TableLayoutData.DEFAULT);
      buttonVolume.setEnabled(false);
      widget = Widgets.newButton(composite,null,"Quit");
      Widgets.layout(widget,0,4,TableLayoutData.RIGHT|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      widget.addListener(SWT.Selection,new Listener()
      {
        public void handleEvent(Event event)
        {
          shell.close();
        }
      });
    }

    // start status update thread
if (true) {
    TabStatusUpdateThread tabStatusUpdateThread = new TabStatusUpdateThread(barServer,this);
    tabStatusUpdateThread.setDaemon(true);
    tabStatusUpdateThread.start();
}
  }

  private int getTableItemIndex(TableItem tableItems[], int id)
  {
    for (int z = 0; z < tableItems.length; z++)
    {
      if ((Integer)tableItems[z].getData() == id) return z;
    }

    return -1;
  }

  private String getByteSize(double n)
  {
    if      (n > 1024*1024*1024) return String.format("%.1f",n/(1024*1024*1024));
    else if (n >      1024*1024) return String.format("%.1f",n/(     1024*1024));
    else if (n >           1024) return String.format("%.1f",n/(          1024));
    else                         return String.format("%d"  ,(long)n           );
  }

  private String getByteUnit(double n)
  {
    if      (n > 1024*1024*1024) return "GBytes";
    else if (n >      1024*1024) return "MBytes";
    else if (n >           1024) return "KBytes";
    else                         return "Bytes";
  }

  private String getByteShortUnit(double n)
  {
    if      (n > 1024*1024*1024) return "GB";
    else if (n >      1024*1024) return "MB";
    else if (n >           1024) return "KB";
    else                         return "B";
  }

  private String formatByteSize(long n)
  {
    return getByteSize(n)+getByteShortUnit(n);
  }

  private double getProgress(long n, long m)
  {
    return (m > 0)?((double)n*100.0)/(double)m:0.0;
  }

  private void updateJobList(BARServer barServer)
  {
    if (!jobList.isDisposed())
    {
      // get job list
      ArrayList<String> result = new ArrayList<String>();
      int errorCode = barServer.executeCommand("JOB_LIST",result);

      // update job list
      TableItem tableItems[] = jobList.getItems();
      boolean tableItemFlags[] = new boolean[tableItems.length];
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
//System.err.println("BARControl.java"+", "+1357+": "+line);
        if (StringParser.parse(line,"%d %S %S %s %d %S %S %S %ld %ld",data,StringParser.QUOTE_CHARS))
        {
//System.err.println("BARControl.java"+", "+747+": "+data[0]+"--"+data[5]+"--"+data[6]);
          /* get data */
          int    id                   = (Integer)data[0];
          String name                 = (String )data[1];
          String state                = (String )data[2];
          String type                 = (String )data[3];
          int    archivePartSize      = (Integer)data[4];
          String compressAlgorithm    = (String )data[5];
          String cryptAlgorithm       = (String )data[6];
          String cryptType            = (String )data[7];
          long   lastExecutedDateTime = (Long   )data[8];
          long   estimatedRestTime    = (Long   )data[9];

          long   estimatedRestDays    = estimatedRestTime/(24*60*60);
          long   estimatedRestHours   = estimatedRestTime%(24*60*60)/(60*60);
          long   estimatedRestMinutes = estimatedRestTime%(60*60   )/(60   );
          long   estimatedRestSeconds = estimatedRestTime%(60      );

          /* get/create table item */
          int index = getTableItemIndex(tableItems,id);
          TableItem tableItem;
          if (index >= 0)
          {
            tableItem = tableItems[index];
            tableItemFlags[index] = true;
          }
          else
          {
            tableItem = new TableItem(jobList,SWT.NONE);
          }

          /* init table item */
          tableItem.setData(id);
          tableItem.setText(0,Integer.toString(id));
          tableItem.setText(1,name);
          tableItem.setText(2,state);
          tableItem.setText(3,type);
          tableItem.setText(4,formatByteSize(archivePartSize));
          tableItem.setText(5,compressAlgorithm);
          tableItem.setText(6,cryptAlgorithm+(cryptType.equals("ASYMMETRIC")?"*":""));
          tableItem.setText(7,DateFormat.getDateTimeInstance().format(new Date(lastExecutedDateTime*1000)));
          tableItem.setText(8,String.format("%2d days %02d:%02d:%02d",estimatedRestDays,estimatedRestHours,estimatedRestMinutes,estimatedRestSeconds));
        }
      }
      for (int z = 0; z < tableItems.length; z++)
      {
        if (!tableItemFlags[z]) jobList.remove(z);
      }
    }
  }

  private void updateJobInfo(BARServer barServer)
  {
    if (selectedJobId > 0)
    {
      // get job info
      String result[] = new String[1];
      int errorCode = barServer.executeCommand(String.format("JOB_INFO %d",selectedJobId),result);
System.err.println("BARControl.java"+", "+891+": result="+result[0]);

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
//         archiveBytes.set((Long)data[13]);
         ratio.set                ((Double)data[14]);

         fileName.set             ((String)data[15]);
         fileProgress.set         (getProgress((Long)data[16],(Long)data[17]));
         storageName.set          ((String)data[18]);
         storageProgress.set      (getProgress((Long)data[19],(Long)data[20]));
         volumeNumber.set         ((Long  )data[21]);
         volumeProgress.set       ((Double)data[22]);
         totalFilesProgress.set   (getProgress((Long)data[ 2],(Long)data[ 4]));
         totalBytesProgress.set   (getProgress((Long)data[ 3],(Long)data[ 5]));
         message.set              ((String)data[ 1]);
      }
    }
  }

  void update(BARServer barServer)
  {
    // update job list
//    updateJobList(barServer);
//    updateJobInfo(barServer);
  }
}

class TabJobs
{
  BARServer   barServer;
  final Shell shell;

  // images
  Image imageFolder;
//  Image imageFolderOpen;
  Image imageFolderIncluded;
//  Image imageFolderIncludedOpen;
  Image imageFolderExcluded;
  Image imageFile;
  Image imageFileIncluded;
  Image imageFileExcluded;
  Image imageLink;
  Image imageLinkIncluded;
  Image imageLinkExcluded;

  // widgets
  Composite   tab;
  Tree        filesTree;
  List        includedList;
  List        excludedList;
  Composite   destination = null;
  Composite   destinationFileSystem;
  Composite   destinationFTP;
  Composite   destinationSCPSFTP;
  Composite   destinationDVD;
  Composite   destinationDevice;
  Table       scheduleList;

  // variables
  BARVariable storageType = new BARVariable(new String[]{"filesystem","ftp","scp","sftp","dvd","device"});

  TabJobs(BARServer barServer, TabFolder parentTabFolder)
  {
    Display   display;
    TabFolder tabFolder;
    Composite tab;
    Group     group;
    Composite composite;
    Control   widget;
    Button    button;
    Combo     combo;
    ImageData imageData;
    TreeItem  treeItem;

    this.barServer = barServer;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    /* create images */
    PaletteData paletteData = new PaletteData(0xFF0000,0x00FF00,0x0000FF);

    imageData = new ImageData(Images.folder.width,Images.folder.height,Images.folder.depth,paletteData,1,Images.folder.data);
    imageData.transparentPixel = paletteData.getPixel(new RGB(255,255,255));
    imageFolder = new Image(display,imageData);
//    imageData = new ImageData(Images.folderOpen.width,Images.folderOpen.height,Images.folderOpen.depth,paletteData,1,Images.folderOpen.data);
//    imageData.transparentPixel = paletteData.getPixel(new RGB(255,255,255));
//    imageFolderOpen = new Image(display,imageData);

    imageData = new ImageData(Images.file.width,Images.file.height,Images.file.depth,paletteData,1,Images.file.data);
    imageData.transparentPixel = paletteData.getPixel(new RGB(255,255,255));
    imageFile = new Image(display,imageData);
    imageData = new ImageData(Images.fileIncluded.width,Images.fileIncluded.height,Images.fileIncluded.depth,paletteData,1,Images.fileIncluded.data);
    imageData.transparentPixel = paletteData.getPixel(new RGB(255,255,255));
    imageFileIncluded = new Image(display,imageData);
    imageData = new ImageData(Images.fileExcluded.width,Images.fileExcluded.height,Images.fileExcluded.depth,paletteData,1,Images.fileExcluded.data);
    imageData.transparentPixel = paletteData.getPixel(new RGB(255,255,255));
    imageFileExcluded = new Image(display,imageData);

    imageData = new ImageData(Images.link.width,Images.link.height,Images.link.depth,paletteData,1,Images.link.data);
    imageData.transparentPixel = paletteData.getPixel(new RGB(255,255,255));
    imageLink = new Image(display,imageData);
    imageData = new ImageData(Images.linkIncluded.width,Images.linkIncluded.height,Images.linkIncluded.depth,paletteData,1,Images.linkIncluded.data);
    imageData.transparentPixel = paletteData.getPixel(new RGB(255,255,255));
    imageLinkIncluded = new Image(display,imageData);
    imageData = new ImageData(Images.linkExcluded.width,Images.linkExcluded.height,Images.linkExcluded.depth,paletteData,1,Images.linkExcluded.data);
    imageData.transparentPixel = paletteData.getPixel(new RGB(255,255,255));
    imageLinkExcluded = new Image(display,imageData);

    // create tab
    this.tab = Widgets.addTab(parentTabFolder,"Jobs");
    this.tab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},
                                       null,
                                       2
                                      )
                      );
    Widgets.layout(this.tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);

    // job selector
    composite = Widgets.newComposite(this.tab,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X);
    {
      widget = Widgets.newLabel(composite,"Name:");
      Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
      combo = Widgets.newOptionMenu(composite,null,null);
  //    combo.setItems(new String[]{"none","zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9","bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9"});
      Widgets.layout(combo,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
      button = Widgets.newButton(composite,null,"New");
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addListener(SWT.Selection,new Listener()
      {
        public void handleEvent(Event event)
        {
        }
      });
      button = Widgets.newButton(composite,null,"Rename");
      Widgets.layout(button,0,3,TableLayoutData.DEFAULT);
      button.addListener(SWT.Selection,new Listener()
      {
        public void handleEvent(Event event)
        {
        }
      });
      button = Widgets.newButton(composite,null,"Delete");
      Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
      button.addListener(SWT.Selection,new Listener()
      {
        public void handleEvent(Event event)
        {
        }
      });
    }

    // sub-tabs
    tabFolder = Widgets.newTabFolder(this.tab);
    Widgets.layout(tabFolder,1,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    {
      tab = Widgets.addTab(tabFolder,"Files");
      Widgets.layout(tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
      {
        // file tree
        filesTree = Widgets.newTree(tab,null);
        Widgets.layout(filesTree,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
        Widgets.addTreeColumn(filesTree,"Name",    SWT.LEFT, 500,true);
        Widgets.addTreeColumn(filesTree,"Type",    SWT.LEFT,  50,false);
        Widgets.addTreeColumn(filesTree,"Size",    SWT.RIGHT,100,false);
        Widgets.addTreeColumn(filesTree,"Modified",SWT.LEFT, 100,false);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          button = Widgets.newButton(composite,null,"*");
          Widgets.layout(button,0,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
          button = Widgets.newButton(composite,null,"+");
          Widgets.layout(button,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
          button = Widgets.newButton(composite,null,"-");
          Widgets.layout(button,0,2,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
        }
      }

      tab = Widgets.addTab(tabFolder,"Filters");
      Widgets.layout(tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
      {
        // included list
        widget = Widgets.newLabel(tab,"Included:");
        Widgets.layout(widget,0,0,TableLayoutData.FILL_Y);
        includedList = Widgets.newList(tab,null,null);
        Widgets.layout(includedList,0,1,TableLayoutData.FILL|TableLayoutData.EXPAND);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
        }

        // excluded list
        widget = Widgets.newLabel(tab,"Excluded:");
        Widgets.layout(widget,2,0,TableLayoutData.FILL_Y);
        excludedList = Widgets.newList(tab,null,null);
        Widgets.layout(excludedList,2,1,TableLayoutData.FILL|TableLayoutData.EXPAND);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,3,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
        }

        // options
        widget = Widgets.newLabel(tab,"Options:");
        Widgets.layout(widget,4,0,TableLayoutData.FILL_Y);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,4,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        button = Widgets.newCheckbox(composite,null,"skip unreadable files");
        Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
      }

      tab = Widgets.addTab(tabFolder,"Storage");
      Widgets.layout(tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
      {
        // part size
        widget = Widgets.newLabel(tab,"Part size:");
        Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          button = Widgets.newRadio(composite,null,"unlimited");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
          button = Widgets.newRadio(composite,null,"limit to");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          combo = Widgets.newCombo(composite,null,null);
          combo.setItems(new String[]{"32M","64M","128M","256M","512M","1G","2G"});
          Widgets.layout(combo,0,2,TableLayoutData.DEFAULT);
        }

        // compress
        widget = Widgets.newLabel(tab,"Compress:");
        Widgets.layout(widget,1,0,TableLayoutData.DEFAULT);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          combo = Widgets.newOptionMenu(composite,null,null);
          combo.setItems(new String[]{"none","zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9","bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9"});
          Widgets.layout(combo,0,0,TableLayoutData.DEFAULT);
        }

        // crypt
        widget = Widgets.newLabel(tab,"Crypt:");
        Widgets.layout(widget,2,0,TableLayoutData.DEFAULT);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,2,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          combo = Widgets.newOptionMenu(composite,null,null);
          combo.setItems(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256"});
          Widgets.layout(combo,0,0,TableLayoutData.DEFAULT);
          button = Widgets.newRadio(composite,null,"symmetric");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button = Widgets.newRadio(composite,null,"asymmetric");
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
        }

        // mode
        widget = Widgets.newLabel(tab,"Mode:");
        Widgets.layout(widget,3,0,TableLayoutData.DEFAULT);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,3,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          button = Widgets.newRadio(composite,null,"normal");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
          button = Widgets.newRadio(composite,null,"full");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button = Widgets.newRadio(composite,null,"incremental");
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
          widget = Widgets.newText(composite,null);
          Widgets.layout(widget,0,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          button = Widgets.newButton(composite,null,imageFolder);
          Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
        }

        // file name
        widget = Widgets.newLabel(tab,"File name:");
        Widgets.layout(widget,4,0,TableLayoutData.DEFAULT);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,4,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          widget = Widgets.newText(composite,null);
          Widgets.layout(widget,0,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          button = Widgets.newButton(composite,null,imageFolder);
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
        }

        // destination
        widget = Widgets.newLabel(tab,"Destination:");
        Widgets.layout(widget,5,0,TableLayoutData.DEFAULT);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,5,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          button = Widgets.newRadio(composite,null,"File system");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
          button.addListener(SWT.MouseDown,new Listener()
          {
            public void handleEvent(Event event)
            {
              storageType.set("filesystem");
            }
          });
          button = Widgets.newRadio(composite,null,"ftp");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addListener(SWT.MouseDown,new Listener()
          {
            public void handleEvent(Event event)
            {
              storageType.set("ftp");
            }
          });
          button = Widgets.newRadio(composite,null,"scp");
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
          button.addListener(SWT.MouseDown,new Listener()
          {
            public void handleEvent(Event event)
            {
              storageType.set("scp");
            }
          });
          button = Widgets.newRadio(composite,null,"sftp");
          Widgets.layout(button,0,3,TableLayoutData.DEFAULT);
          button.addListener(SWT.MouseDown,new Listener()
          {
            public void handleEvent(Event event)
            {
              storageType.set("sftp");
            }
          });
          button = Widgets.newRadio(composite,null,"DVD");
          Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
          button.addListener(SWT.MouseDown,new Listener()
          {
            public void handleEvent(Event event)
            {
              storageType.set("dvd");
            }
          });
          button = Widgets.newRadio(composite,null,"Device");
          Widgets.layout(button,0,5,TableLayoutData.DEFAULT);
          button.addListener(SWT.MouseDown,new Listener()
          {
            public void handleEvent(Event event)
            {
              storageType.set("device");
            }
          });
        }

        destinationFileSystem = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(destinationFileSystem,6,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.TOP);
        {
          button = Widgets.newCheckbox(destinationFileSystem,null,"overwrite archive files");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
        }
        destinationFileSystem.setVisible(false);
        Widgets.addModifyListener(new WidgetListener(destinationFileSystem,storageType)
        {
          public void modified(BARVariable variable)
          {
            boolean visibleFlag = variable.equals("filesystem");

            TableLayoutData tableLayoutData = (TableLayoutData)destinationFileSystem.getLayoutData();
            tableLayoutData.exclude = !visibleFlag;
            destinationFileSystem.setVisible(visibleFlag);
          }
        });

        destinationFTP = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(destinationFTP,6,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.TOP);
        {
          widget = Widgets.newLabel(destinationFTP,"Login:");
          Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
          composite = Widgets.newComposite(destinationFTP,SWT.NONE);
          Widgets.layout(composite,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          {
            widget = Widgets.newText(composite,null);
            Widgets.layout(widget,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
            widget = Widgets.newLabel(composite,"Host:");
            Widgets.layout(widget,0,2,TableLayoutData.DEFAULT);
            widget = Widgets.newText(composite,null);
            Widgets.layout(widget,0,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          }

          widget = Widgets.newLabel(destinationFTP,"Max. band width:");
          Widgets.layout(widget,1,0,TableLayoutData.DEFAULT);
          composite = Widgets.newComposite(destinationFTP,SWT.NONE);
          Widgets.layout(composite,1,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          {
            button = Widgets.newRadio(composite,null,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
            button = Widgets.newRadio(composite,null,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
            combo = Widgets.newCombo(composite,null,null);
            combo.setItems(new String[]{"32K","64K","128K","256K","512K"});
            Widgets.layout(combo,0,2,TableLayoutData.DEFAULT);
          }
        }
        destinationFTP.setVisible(false);
        Widgets.addModifyListener(new WidgetListener(destinationFTP,storageType)
        {
          public void modified(BARVariable variable)
          {
            boolean visibleFlag = variable.equals("ftp");

            TableLayoutData tableLayoutData = (TableLayoutData)destinationFTP.getLayoutData();
            tableLayoutData.exclude = !visibleFlag;
            destinationFTP.setVisible(visibleFlag);
          }
        });

        destinationSCPSFTP = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(destinationSCPSFTP,6,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.TOP);
        {
          widget = Widgets.newLabel(destinationSCPSFTP,"Login:");
          Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
          composite = Widgets.newComposite(destinationSCPSFTP,SWT.NONE);
          Widgets.layout(composite,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          {
            widget = Widgets.newText(composite,null);
            Widgets.layout(widget,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
            widget = Widgets.newLabel(composite,"Host:");
            Widgets.layout(widget,0,2,TableLayoutData.DEFAULT);
            widget = Widgets.newText(composite,null);
            Widgets.layout(widget,0,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          }

          widget = Widgets.newLabel(destinationSCPSFTP,"SSH public key:");
          Widgets.layout(widget,1,0,TableLayoutData.DEFAULT);
          widget = Widgets.newText(destinationSCPSFTP,null);
          Widgets.layout(widget,1,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          widget = Widgets.newLabel(destinationSCPSFTP,"SSH private key:");
          Widgets.layout(widget,2,0,TableLayoutData.DEFAULT);
          widget = Widgets.newText(destinationSCPSFTP,null);
          Widgets.layout(widget,2,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);

          widget = Widgets.newLabel(destinationSCPSFTP,"Max. band width:");
          Widgets.layout(widget,3,0,TableLayoutData.DEFAULT);
          composite = Widgets.newComposite(destinationSCPSFTP,SWT.NONE);
          Widgets.layout(composite,3,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          {
            button = Widgets.newRadio(composite,null,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
            button = Widgets.newRadio(composite,null,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
            combo = Widgets.newCombo(composite,null,null);
            combo.setItems(new String[]{"32K","64K","128K","256K","512K"});
            Widgets.layout(combo,0,2,TableLayoutData.DEFAULT);
          }
        }
        destinationSCPSFTP.setVisible(false);
        Widgets.addModifyListener(new WidgetListener(destinationFTP,storageType)
        {
          public void modified(BARVariable variable)
          {
            boolean visibleFlag = variable.equals("scp") || variable.equals("sftp");

            TableLayoutData tableLayoutData = (TableLayoutData)destinationSCPSFTP.getLayoutData();
            tableLayoutData.exclude = !visibleFlag;
            destinationSCPSFTP.setVisible(visibleFlag);
          }
        });

        destinationDVD = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(destinationDVD,6,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.TOP);
        {
          widget = Widgets.newLabel(destinationDVD,"Device:");
          Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
          widget = Widgets.newText(destinationDVD,null);
          Widgets.layout(widget,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);

          widget = Widgets.newLabel(destinationDVD,"Size:");
          Widgets.layout(widget,1,0,TableLayoutData.DEFAULT);
          composite = Widgets.newComposite(destinationDVD,SWT.NONE);
          Widgets.layout(composite,1,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          {
            combo = Widgets.newCombo(composite,null,null);
            combo.setItems(new String[]{"2G","3G","3.6G","4G"});
            Widgets.layout(combo,0,0,TableLayoutData.DEFAULT);
            widget = Widgets.newLabel(composite,"bytes");
            Widgets.layout(widget,0,1,TableLayoutData.DEFAULT);
          }

          widget = Widgets.newLabel(destinationDVD,"Options:");
          Widgets.layout(widget,3,0,TableLayoutData.DEFAULT);
          composite = Widgets.newComposite(destinationDVD,SWT.NONE);
          Widgets.layout(composite,3,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          {
            button = Widgets.newCheckbox(composite,null,"add error-correction codes");
            Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
          }
        }
        destinationDVD.setVisible(false);
        Widgets.addModifyListener(new WidgetListener(destinationDVD,storageType)
        {
          public void modified(BARVariable variable)
          {
            boolean visibleFlag = variable.equals("dvd");

            TableLayoutData tableLayoutData = (TableLayoutData)destinationDVD.getLayoutData();
            tableLayoutData.exclude = !visibleFlag;
            destinationDVD.setVisible(visibleFlag);
          }
        });
 
        destinationDevice = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(destinationDevice,6,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.TOP);
        {
          widget = Widgets.newLabel(destinationDevice,"Device:");
          Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
         widget = Widgets.newText(destinationDevice,null);
          Widgets.layout(widget,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);

          widget = Widgets.newLabel(destinationDevice,"Size:");
          Widgets.layout(widget,1,0,TableLayoutData.DEFAULT);
          composite = Widgets.newComposite(destinationDevice,SWT.NONE);
          Widgets.layout(composite,1,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          {
            combo = Widgets.newCombo(composite,null,null);
            combo.setItems(new String[]{"2G","3G","3.6G","4G"});
            Widgets.layout(combo,0,0,TableLayoutData.DEFAULT);
            widget = Widgets.newLabel(composite,"bytes");
            Widgets.layout(widget,0,1,TableLayoutData.DEFAULT);
          }

        }
        destinationDevice.setVisible(false);
        Widgets.addModifyListener(new WidgetListener(destinationDevice,storageType)
        {
          public void modified(BARVariable variable)
          {
            boolean visibleFlag = variable.equals("device");

            TableLayoutData tableLayoutData = (TableLayoutData)destinationDevice.getLayoutData();
            tableLayoutData.exclude = !visibleFlag;
            destinationDevice.setVisible(visibleFlag);
          }
        });
      }

      tab = Widgets.addTab(tabFolder,"Schedule");
      Widgets.layout(tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
      {
        // list
        scheduleList = Widgets.newTable(tab,this,new Listener()
        {
          public void handleEvent (Event event)
          {
  /*
            TabStatus barTabStatus = (BARTabStatus)event.widget.getData();

            barTabStatus.selectedJobId = (Integer)event.item.getData();

            TableItem tableItems[] = jobList.getItems();
            int index = getTableItemIndex(tableItems,barTabStatus.selectedJobId);
            barTabStatus.selectedJob.setText("Selected '"+tableItems[index].getText(1)+"'");
  */
          }
        });
        Widgets.layout(scheduleList,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
        Widgets.addTableColumn(scheduleList,"Date",     SWT.LEFT,100,false);
        Widgets.addTableColumn(scheduleList,"Week day", SWT.LEFT,100,true );
        Widgets.addTableColumn(scheduleList,"Time",     SWT.LEFT,100,false);
        Widgets.addTableColumn(scheduleList,"Type",     SWT.LEFT,  0,true );

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
          button = Widgets.newButton(composite,null,"Edit");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
          button.addListener(SWT.Selection,new Listener()
          {
            public void handleEvent(Event event)
            {
            }
          });
        }
      }
    }

    // add root devices
    treeItem = Widgets.addTreeItem(filesTree,new String("/"),true);
    treeItem.setText("/");
    treeItem.setImage(imageFolder);
    filesTree.addListener(SWT.Expand, new Listener()
    {
      public void handleEvent (final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        updateFileList(treeItem);
        if (treeItem.getItemCount() == 0) new TreeItem(treeItem,SWT.NONE);
      }
    });
    filesTree.addListener(SWT.Collapse, new Listener()
    {
      public void handleEvent (final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        treeItem.removeAll();
        new TreeItem(treeItem,SWT.NONE);
      }
    });
    filesTree.addListener(SWT.MouseDoubleClick,new Listener()
    {
      public void handleEvent(final Event event)
      {
        TreeItem treeItem = filesTree.getItem(new Point(event.x,event.y));
        if (treeItem != null)
        {
          Event treeEvent = new Event();
          treeEvent.item = treeItem;
          if (treeItem.getExpanded())
          {
            filesTree.notifyListeners(SWT.Collapse,treeEvent);
            treeItem.setExpanded(false);
          }
          else
          {
            filesTree.notifyListeners(SWT.Expand,treeEvent);
            treeItem.setExpanded(true);
          }
        }
      }
    });

    // get jobs

  }

  /** find index for insert of tree item in sort list of tree items
   * @param treeItem tree item
   * @param name name of tree item to insert
   * @param data data of tree item to insert
   * @return index in tree item
   */
  private int findTreeItemIndex(TreeItem treeItem, String name, Object data)
  {
    TreeItem subItems[] = treeItem.getItems();

    int index = 0;
    if (data != null)
    {
      while (   (index < subItems.length)
             && (subItems[index].getData() != null)
             && (subItems[index].getText().compareTo(name) < 0)
            )
      {
        index++;
      }
    }
    else
    {
      while (   (index < subItems.length)
             && (   (subItems[index].getData() != null)
                 || (subItems[index].getText().compareTo(name) < 0)
                )
            )
      {
        index++;
      }
    }

    return index;
  }

  /** update file list of tree item
   * @param treeItem tree item to update
   */
  private void updateFileList(TreeItem treeItem)
  {
    String   directoryName = (String)treeItem.getData();
    TreeItem subTreeItem;
    int      index;

    ArrayList<String> result = new ArrayList<String>();
    int errorCode = barServer.executeCommand("FILE_LIST "+StringParser.escape(directoryName),result);

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
        long   size      = (Long  )data[0];
        long   timestamp = (Long  )data[1];
        String name      = (String)data[2];

        String title = new File(name).getName();
        index = findTreeItemIndex(treeItem,title,null);
        subTreeItem = Widgets.addTreeItem(treeItem,index,null,false);
        subTreeItem.setText(0,title);
        subTreeItem.setText(1,"FILE");
        subTreeItem.setText(2,Long.toString(size));
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(timestamp*1000)));
        subTreeItem.setImage(imageFile);
      }
      else if (StringParser.parse(line,"DIRECTORY %ld %ld %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             size
             date/time
             name
        */
        long   size      = (Long  )data[0];
        long   timestamp = (Long  )data[1];
        String name      = (String)data[2];

        String title = new File(name).getName();
        index = findTreeItemIndex(treeItem,title,name);
        subTreeItem = Widgets.addTreeItem(treeItem,index,name,true);
        subTreeItem.setText(0,title);
        subTreeItem.setText(1,"DIR");
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(timestamp*1000)));
        subTreeItem.setImage(imageFolder);
      }
      else if (StringParser.parse(line,"LINK %ld %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             date/time
             name
        */
        long   timestamp = (Long  )data[0];
        String name      = (String)data[1];

        String title = new File(name).getName();
        index = findTreeItemIndex(treeItem,title,null);
        subTreeItem = Widgets.addTreeItem(treeItem,index,null,false);
        subTreeItem.setText(0,title);
        subTreeItem.setText(1,"LINK");
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(timestamp*1000)));
        subTreeItem.setImage(imageLink);
      }
      else if (StringParser.parse(line,"SPECIAL %ld %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             date/time
             name
        */
        long   timestamp = (Long  )data[0];
        String name      = (String)data[1];

        index = findTreeItemIndex(treeItem,name,null);
        subTreeItem = Widgets.addTreeItem(treeItem,index,null,false);
        subTreeItem.setText(0,name);
        subTreeItem.setText(1,"SPECIAL");
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(timestamp*1000)));
      }
      else if (StringParser.parse(line,"DEVICE %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             name
        */
        String name = (String)data[0];

        index = findTreeItemIndex(treeItem,name,null);
        subTreeItem = Widgets.addTreeItem(treeItem,index,null,false);
        subTreeItem.setText(0,name);
        subTreeItem.setText(1,"DEVICE");
      }
      else if (StringParser.parse(line,"SOCKET %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             name
        */
        String name = (String)data[0];

        index = findTreeItemIndex(treeItem,name,null);
        subTreeItem = Widgets.addTreeItem(treeItem,index,null,false);
        subTreeItem.setText(0,name);
        subTreeItem.setText(1,"SOCKET");
      }
    }
  }  

  private void showDestination(String storageType)
  {
    Button    button;
    Combo     combo;
    Composite composite;
    Control   widget;

    if (destination != null) destination.dispose();

    if      (storageType.equals("filesystem"))
    {
System.err.println("BARControl.java"+", "+2492+": ");
      destination = Widgets.newComposite(tab,SWT.BORDER);
      Widgets.layout(destination,6,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND|TableLayoutData.CENTER_Y);
      {
        button = Widgets.newCheckbox(destination,null,"overwrite archive files");
        Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
      }
    }
    else if (storageType.equals("ftp"))
    {
System.err.println("BARControl.java"+", "+2502+": ");
      destination = Widgets.newComposite(tab,SWT.BORDER);
      Widgets.layout(destination,6,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND|TableLayoutData.CENTER_Y);
      {
        widget = Widgets.newLabel(destination,"Login:");
        Widgets.layout(widget,0,0,TableLayoutData.DEFAULT);
        composite = Widgets.newComposite(destination,SWT.NONE);
        Widgets.layout(composite,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          widget = Widgets.newText(composite,null);
          Widgets.layout(widget,0,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
          widget = Widgets.newLabel(composite,"Host:");
          Widgets.layout(widget,0,2,TableLayoutData.DEFAULT);
          widget = Widgets.newText(composite,null);
          Widgets.layout(widget,0,3,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        }

        widget = Widgets.newLabel(destination,"Max. band width:");
        Widgets.layout(widget,1,0,TableLayoutData.DEFAULT);
        composite = Widgets.newComposite(destination,SWT.NONE);
        Widgets.layout(composite,1,1,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
        {
          button = Widgets.newRadio(composite,null,"unlimited");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT);
          button = Widgets.newRadio(composite,null,"limit to");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          combo = Widgets.newCombo(composite,null,null);
          combo.setItems(new String[]{"32K","64K","128K","256K","512K"});
          Widgets.layout(combo,0,2,TableLayoutData.DEFAULT);
        }
      }
    }
    tab.layout(true);
  }

  private void updateDestination()
  {
  }
}

public class BARControl
{
  // --------------------------- constants --------------------------------

  final static String DEFAULT_HOSTNAME = "localhost";
  final static int    DEFAULT_PORT     = 38523;
  final static int    DEFAULT_TLS_PORT = 38524;

  // --------------------------- variables --------------------------------
  private String hostname = DEFAULT_HOSTNAME;
  private int    port     = DEFAULT_PORT;
  private int    tlsPort  = DEFAULT_TLS_PORT;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  BARControl(String[] args)
  {
    // connect to server
String password = "y7G7EGj2";
    BARServer barServer = new BARServer(hostname,port,tlsPort,password);

    // open main window
    Display display = new Display();
    final Shell shell = new Shell(display);
    shell.setLayout(new TableLayout());

    // create resizable tab (with help of sashForm)
    SashForm sashForm = new SashForm(shell,SWT.NONE);
    sashForm.setLayout(new TableLayout());
    Widgets.layout(sashForm,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    TabFolder tabFolder = new TabFolder(sashForm,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(0,0,TableLayoutData.FILL|TableLayoutData.EXPAND));

    TabStatus tabStatus = new TabStatus(barServer,tabFolder);
    TabJobs   tabJobs   = new TabJobs  (barServer,tabFolder);

    // set window size, manage window
    shell.setSize(800,600);
    shell.open();

    // SWT event loop
    while (!shell.isDisposed())
    {
      if (!display.readAndDispatch())
      {
        display.sleep();
      }
    }
  }

  public static void main(String[] args)
  {
    BARControl barControl = new BARControl(args);
  }
}

/* end of file */
