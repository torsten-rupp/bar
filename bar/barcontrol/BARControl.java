/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/BARControl.java,v $
* $Revision: 1.5 $
* $Author: torsten $
* Contents:
* Systems :
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.lang.Double;
import java.lang.Integer;
import java.lang.Long;
import java.lang.Exception;
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
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Label;
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
};

class BARVariable
{
  private BARVariableTypes type;
  private long             n;
  private double           d;
  private String           s;

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
  BARVariable(String s)
  {
    this.type = BARVariableTypes.STRING;
    this.s    = s;
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

    return s;
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
  void set(String s)
  {
    assert type == BARVariableTypes.STRING;

    this.s = s;
    Widgets.modified(this);
  }

  public String toString()
  {
    switch (type)
    {
      case LONG  : return Long.toString(n);
      case DOUBLE: return Double.toString(d);
      case STRING: return s;
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
System.err.println("BARControl.java"+", "+279+": sent "+line);

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

  String getString(BARVariable variable)
  {
    return null;
  }

  public void modified()
  {
    modified(control,variable);
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

  static Button newCheckbox(Composite composite, Object data, String text)
  {
    Button button;

    button = new Button(composite,SWT.CHECK);
    button.setText(text);
    button.setData(data);

    return button;
  }

  static Text newText(Composite composite, Object data)
  {
    Text text;

    text = new Text(composite,SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE|SWT.READ_ONLY);
    text.setData(data);

    return text;
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

  static void newTableColumn(Table table, String title, int style, int width, boolean resizable)
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
    progressBar.setSelection(40);
//    progressBar.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_BLUE));

    return progressBar;
  }

  static Tree newTree(Composite composite, Object variable)
  {
    Tree tree;

    tree = new Tree(composite,SWT.HORIZONTAL);

    return tree;
  }

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

/*
  static Composite newTab(TabFolder tabFolder, String title)
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
*/

  //-----------------------------------------------------------------------

  static Composite newComposite(Composite composite)
  {
    Composite childComposite;

    childComposite = new Composite(composite,SWT.NONE);
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

class BARTabStatus
{
  final Shell shell;
  Composite   statusTab;
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
    private BARServer    barServer;
    private BARTabStatus barTabStatus;

    TabStatusUpdateThread(BARServer barServer, BARTabStatus barTabStatus)
    {
      this.barServer    = barServer;
      this.barTabStatus = barTabStatus;
    }

    public void run()
    {
      for (;;)
      {
        try
        {
          barTabStatus.statusTab.getDisplay().syncExec(new Runnable()
          {
            public void run()
            {
              barTabStatus.update(barServer);
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

  BARTabStatus(BARServer barServer, TabFolder tabFolder)
  {
    Group     group;
    Composite composite;
    Control   widget;
    Label     label;

    // get shell
    shell = tabFolder.getShell();

    // create tab
    statusTab = Widgets.addTab(tabFolder,"Status");
    statusTab.setLayout(new TableLayout(new double[]{1.0,0.0,0.0},
                                        null,
                                        2
                                       )
                       );
    Widgets.layout(statusTab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);

    // list with jobs
    jobList = Widgets.newTable(statusTab,this,new Listener()
    {
      public void handleEvent (Event event)
      {
        BARTabStatus barTabStatus = (BARTabStatus)event.widget.getData();

        barTabStatus.selectedJobId = (Integer)event.item.getData();

        TableItem tableItems[] = jobList.getItems();
        int index = getTableItemIndex(tableItems,barTabStatus.selectedJobId);
        barTabStatus.selectedJob.setText("Selected '"+tableItems[index].getText(1)+"'");

        barTabStatus.buttonStart.setEnabled(true);
//        barTabStatus.buttonAbort.setEnabled(true);
//        barTabStatus.buttonPause.setEnabled(true);
//        barTabStatus.buttonVolume.setEnabled(true);
      }
    });
    Widgets.layout(jobList,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    Widgets.newTableColumn(jobList,"#",             SWT.RIGHT, 30,false);
    Widgets.newTableColumn(jobList,"Name",          SWT.LEFT, 100,true );
    Widgets.newTableColumn(jobList,"State",         SWT.LEFT,  60,true );
    Widgets.newTableColumn(jobList,"Type",          SWT.LEFT,   0,true );
    Widgets.newTableColumn(jobList,"Part size",     SWT.RIGHT,  0,true );
    Widgets.newTableColumn(jobList,"Compress",      SWT.LEFT,   0,true );
    Widgets.newTableColumn(jobList,"Crypt",         SWT.LEFT,  80,true );
    Widgets.newTableColumn(jobList,"Last executed", SWT.LEFT, 180,true );
    Widgets.newTableColumn(jobList,"Estimated time",SWT.LEFT, 120,true );

    // selected job group
    selectedJob = Widgets.newGroup(statusTab,"Selected ''",SWT.NONE);
    selectedJob.setLayout(new TableLayout(null,
                                          new double[]{0.0,1.0,0.0,1.0},
                                          4
                                         )
                         );
    Widgets.layout(selectedJob,1,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND);

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

    composite = Widgets.newComposite(selectedJob);
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

    composite = Widgets.newComposite(selectedJob);
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

    composite = Widgets.newComposite(selectedJob);
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

    composite = Widgets.newComposite(selectedJob);
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

    // buttons
    composite = Widgets.newComposite(statusTab);
    Widgets.layout(composite,2,0,TableLayoutData.FILL_X|TableLayoutData.EXPAND_X|TableLayoutData.CENTER_Y);
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

  private String formatByteSize(long n)
  {
    return getByteSize(n)+getByteUnit(n);
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
        if (StringParser.parse(line,"%d %S %S %s %d %S %S %S %ld %ld",data,StringParser.QUOTE_CHARS))
        {
//  System.err.println("BARControl.java"+", "+747+": "+data[0]+"--"+data[1]);
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
          tableItem.setText(5,cryptAlgorithm+(cryptType.equals("ASYMMETRIC")?"*":""));
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
    updateJobList(barServer);
    updateJobInfo(barServer);
  }
}

class BARTabJobs
{
  final Shell shell;
  Composite   jobTab;
  Tree        filesTree;
  Table       scheduleList;

  BARTabJobs(BARServer barServer, TabFolder jobTabFolder)
  {
    TabFolder tabFolder;
    Composite tab;
    Group     group;
    Composite composite;
    Control   widget;
    Label     label;

    // get shell
    shell = jobTabFolder.getShell();

    // create tab
    jobTab = Widgets.addTab(jobTabFolder,"Jobs");
    jobTab.setLayout(new TableLayout(new double[]{0.0,1.0,0.0},
                                     null,
                                     2
                                    )
                    );
    Widgets.layout(jobTab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);

    // job selector

    // tabs
    jobTabFolder = Widgets.newTabFolder(jobTab);
    Widgets.layout(jobTabFolder,1,0,TableLayoutData.FILL|TableLayoutData.EXPAND);

    tab = Widgets.addTab(jobTabFolder,"Files");
    Widgets.layout(tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    {
      // file tree
      filesTree = Widgets.newTree(tab,null);
      Widgets.layout(filesTree,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);

      // buttons
    }

    tab = Widgets.addTab(jobTabFolder,"Filters");
    Widgets.layout(tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    {
      // included list

      // buttons

      // excluded list

      // buttons
    }

    tab = Widgets.addTab(jobTabFolder,"Storage");
    Widgets.layout(tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    {
      // part size

      // compress

      // crypt

      // mode

      // file name

      // destination
    }

    tab = Widgets.addTab(jobTabFolder,"Schedule");
    Widgets.layout(tab,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    {
      // list
      scheduleList = Widgets.newTable(tab,this,new Listener()
      {
        public void handleEvent (Event event)
        {
/*
          BARTabStatus barTabStatus = (BARTabStatus)event.widget.getData();

          barTabStatus.selectedJobId = (Integer)event.item.getData();

          TableItem tableItems[] = jobList.getItems();
          int index = getTableItemIndex(tableItems,barTabStatus.selectedJobId);
          barTabStatus.selectedJob.setText("Selected '"+tableItems[index].getText(1)+"'");
*/
        }
      });
      Widgets.layout(scheduleList,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
      Widgets.newTableColumn(scheduleList,"Date",     SWT.LEFT,100,false);
      Widgets.newTableColumn(scheduleList,"Week day", SWT.LEFT,100,true );
      Widgets.newTableColumn(scheduleList,"Time",     SWT.LEFT,100,false);
      Widgets.newTableColumn(scheduleList,"Type",     SWT.LEFT,  0,true );
    }
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
    sashForm.setLayout(new TableLayout(new double[]{1.0},new double[]{1.0}));
    Widgets.layout(sashForm,0,0,TableLayoutData.FILL|TableLayoutData.EXPAND);
    TabFolder tabFolder = new TabFolder(sashForm,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.FILL|TableLayoutData.EXPAND));

    BARTabStatus barTabStatus = new BARTabStatus(barServer,tabFolder);
    BARTabJobs   barTabJobs   = new BARTabJobs  (barServer,tabFolder);
/*

//barTabStatus.doneFiles.set(100000000);
//barTabStatus.storedFiles.set(1);
*/
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
