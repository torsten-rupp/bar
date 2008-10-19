/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/BARControl.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents:
* Systems :
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.Long;
import java.lang.String;
import java.lang.System;
import java.util.LinkedList;
import java.util.ArrayList;

import java.net.Socket;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.SSLSocket;
import java.io.InputStreamReader;
import java.io.InputStream;
import java.io.BufferedReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.BufferedWriter;
import java.io.PrintWriter;
import java.io.IOException;

import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.ProgressBar;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.TabFolder;
import org.eclipse.swt.widgets.TabItem;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.swt.widgets.TableItem;
import org.eclipse.swt.widgets.Text;

/****************************** Classes ********************************/

enum BARVariableTypes
{
  LONG,
  DOUBLE,
  STRING,
};

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
      stringBuffer.append(Integer.toHexString((int)data[z] & 0xFF));
    }

    return stringBuffer.toString();
  }

  BARServer(String hostname, int port, int tlsPort, String serverPassword)
  {
    byte sessionId[];

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
    commandId = 0;

    // read session id
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
System.err.println("BARControl.java"+", "+682+": "+sessionId);

    // authorize
    try
    {
      String line;

      byte authorizeData[] = new byte[sessionId.length];
      for (int z = 0; z < sessionId.length; z++)
      {
        authorizeData[z] = (byte)(((z < serverPassword.length())?(int)serverPassword.charAt(z):0)^(int)sessionId[z]);
      }
      commandId++;
      line = Long.toString(commandId)+" AUTHORIZE "+encodeHex(authorizeData);
      output.println(line);
      output.flush();

      line = input.readLine();
      String data[] = line.split(" ",4);
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

  synchronized int executeCommand(String command, ArrayList<String> result)
  {
    String  line;
    boolean endFlag = false;
    int     errorCode = -1;

    // 
    commandId++;
    line = Long.toString(commandId)+" "+command;
    output.println(line);
    output.flush();

    // read buffer lines from list
//???

    //
    try
    {
      while (!endFlag && (line = input.readLine()) != null)
      {
  System.err.println("BARControl.java"+", "+701+": "+line);

        String data[] = line.split(" ",4);
        assert data.length >= 3;
        if (Integer.parseInt(data[0]) == commandId)
        {
          result.add(data[3]);
          if (Integer.parseInt(data[1]) != 0)
          {
            errorCode = Integer.parseInt(data[2]);
            endFlag = true;
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
}

  //-----------------------------------------------------------------------

class WidgetListener
{
  private Object      widget;
  private BARVariable variable;

  WidgetListener(Object widget, BARVariable variable)
  {
    this.widget   = widget;
    this.variable = variable;
  }

  public boolean equals(Object variable)
  {
    return (this.variable != null) && this.variable.equals(variable);
  }

  public void modified(Object widget, BARVariable variable)
  {
  }

  public void modified()
  {
    modified(widget,variable);
  }
}

class Widgets
{
  final static int NONE     = 0;
  final static int FILL_X   = 1 << 0;
  final static int FILL_Y   = 1 << 1;
  final static int FILL     = FILL_X|FILL_Y;
  final static int EXPAND_X = 1 << 2;
  final static int EXPAND_Y = 1 << 3;
  final static int EXPAND   = EXPAND_X|EXPAND_Y;

  final static int PADDING_X = 3;
  final static int PADDING_Y = 3;

  //-----------------------------------------------------------------------

  static LinkedList<WidgetListener> listenersList = new LinkedList<WidgetListener>();

  //-----------------------------------------------------------------------

  static Composite getComposite(Object widget)
  {
    if      (widget instanceof BARComposite) return ((BARComposite)widget).composite;
    else if (widget instanceof BARGroup)     return ((BARGroup)widget).group;
    else if (widget instanceof BARTab)       return ((BARTab)widget).tab.composite;
    else                                     return (Composite)widget;
  }

  static void pack(Control control, int style, int columnSpawn)
  {
    GridData gridData = new GridData();
    gridData.horizontalAlignment       = ((style & FILL_X) != 0) ? SWT.FILL : SWT.NONE;
    gridData.verticalAlignment         = ((style & FILL_Y) != 0) ? SWT.FILL : SWT.CENTER;
    gridData.grabExcessHorizontalSpace = ((style & EXPAND_X) != 0);
    gridData.grabExcessVerticalSpace   = ((style & EXPAND_Y) != 0);
    gridData.horizontalSpan            = columnSpawn;
    control.setLayoutData(gridData);
  }

  static Label addNone(Object parentWidget, int columnSpawn)
  {
    Composite composite = getComposite(parentWidget);
    Label     label;

    label = new Label(composite,SWT.NONE);

    Widgets.pack(label,Widgets.NONE,columnSpawn);

    return label;
  }

  static Label addLabel(Object parentWidget, int columnSpawn, String text)
  {
    Composite composite = getComposite(parentWidget);
    Label     label;

    label = new Label(composite,SWT.LEFT);
    label.setText(text);
//label.setBackground(Display.getDefault().getSystemColor(SWT.COLOR_BLUE));
    Widgets.pack(label,Widgets.NONE,columnSpawn);

    return label;
  }

  static Label addView(Object parentWidget, int columnSpawn, Object variable, String defaultValue)
  {
    Composite composite = getComposite(parentWidget);
    Label     label;

    label = new Label(composite,SWT.LEFT|SWT.BORDER);
    label.setText(defaultValue);
//label.setBackground(Display.getDefault().getSystemColor(SWT.COLOR_RED));
    Widgets.pack(label,Widgets.FILL_X|Widgets.EXPAND_X,columnSpawn);

    return label;
  }

  static Composite addIntegerView(Object parentWidget, int columnSpawn, BARVariable variable, int defaultValue, String unit)
  {
    Composite composite = getComposite(parentWidget);
    Composite viewComposite;
    Label     label;

    viewComposite = new Composite(composite,SWT.NONE);
    GridLayout gridLayout = new GridLayout();
    gridLayout.numColumns   = (unit != null) ? 2 : 1;
    gridLayout.marginWidth  = 0;
    gridLayout.marginHeight = 0;
    viewComposite.setLayout(gridLayout);
    Widgets.pack(viewComposite,Widgets.FILL_X|Widgets.EXPAND_X,columnSpawn);

    label = new Label(viewComposite,SWT.RIGHT|SWT.BORDER);
    label.setText(Integer.toString(defaultValue));
//label.setBackground(Display.getDefault().getSystemColor(SWT.COLOR_YELLOW));
    Widgets.pack(label,Widgets.FILL_X|Widgets.EXPAND_X,0);

    Widgets.addModifyListener(new WidgetListener(label,variable)
    {
      public void modified(Object widget, BARVariable variable)
      {
System.out.println("value "+variable);
        ((Label)widget).setText(""+variable.getLong());
      }
    });

    if (unit != null)
    {
      label = new Label(viewComposite,SWT.LEFT);
      label.setText(unit);
//label.setBackground(Display.getDefault().getSystemColor(SWT.COLOR_GREEN));
      Widgets.pack(label,Widgets.NONE,0);

      Widgets.addModifyListener(new WidgetListener(label,variable)
      {
        public void modified(Object widget, BARVariable variable)
        {
  System.out.println("value "+variable);
          ((Label)widget).setText(""+variable.getLong());
        }
      });
    }

    return viewComposite;
  }

  static Label addStringView(Object parentWidget, int columnSpawn, Object variable, String defaultValue)
  {
    Composite composite = getComposite(parentWidget);
    Label     label;

    label = new Label(composite,SWT.LEFT|SWT.BORDER);
    label.setText(defaultValue);
    Widgets.pack(label,Widgets.FILL_X|Widgets.EXPAND_X,columnSpawn);

    return label;
  }

  static Button addButton(Object parentWidget, int columnSpawn, String text, SelectionAdapter selectionAdapter)
  {
    Composite composite = getComposite(parentWidget);
    Button    button;

    button = new Button(composite,SWT.PUSH);
    button.setText(text);
    button.setLayoutData(new GridData(SWT.FILL,SWT.FILL,true,true,0,0));
    if (selectionAdapter != null) button.addSelectionListener(selectionAdapter);

    return button;
  }

  static Button addCheckbox(Object parentWidget, int columnSpawn, String text, SelectionAdapter selectionAdapter)
  {
    Composite composite = getComposite(parentWidget);
    Button    button;

    button = new Button(composite,SWT.CHECK);
    button.setText(text);
    button.setLayoutData(new GridData(SWT.FILL,SWT.FILL,true,true,0,0));
    if (selectionAdapter != null) button.addSelectionListener(selectionAdapter);

    return button;
  }

  static Text addText(Object parentWidget, int columnSpawn, SelectionAdapter selectionAdapter)
  {
    Composite composite = getComposite(parentWidget);
    Text      text;

    text = new Text(composite,SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE);
    text.setLayoutData(new GridData(SWT.FILL,SWT.FILL,true,true,0,0));
    if (selectionAdapter != null) text.addSelectionListener(selectionAdapter);

    return text;
  }

  static BARTable addTable(Object parentWidget, int columnSpawn, SelectionAdapter selectionAdapter)
  {
    Composite composite = getComposite(parentWidget);
    BARTable  barTable;

    barTable = new BARTable(composite,null);
    if (selectionAdapter != null) barTable.table.addSelectionListener(selectionAdapter);

    return barTable;
  }

  static void addProgressBar(Object parentWidget, int columnSpawn, Object variable)
  {
    Composite   composite = getComposite(parentWidget);
    ProgressBar progressBar;

    progressBar = new ProgressBar(composite,SWT.HORIZONTAL);
    Widgets.pack(progressBar,Widgets.FILL_X|Widgets.EXPAND_X,columnSpawn);
  }

  //-----------------------------------------------------------------------

  static BARComposite addComposite(Object parentWidget, int columnSpawn, int columns, int style, int paddingX, int paddingY)
  {
    Composite    composite = getComposite(parentWidget);
    BARComposite barComposite;

    barComposite = new BARComposite(composite,columns,style,paddingX,paddingY);

    return barComposite;
  }

  static BARGroup addGroup(Object parentWidget, int columnSpawn, String title, int columns, int style)
  {
    Composite composite = getComposite(parentWidget);
    BARGroup  barGroup;

    barGroup = new BARGroup(composite,title,columns,style);

    return barGroup;
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

class BARTable
{
  Table table;

  BARTable(Composite parentComposite, Object variable)
  {
    table = new Table(parentComposite,SWT.BORDER|SWT.MULTI|SWT.FULL_SELECTION);
    table.setLinesVisible (true);
    table.setHeaderVisible (true);
    Widgets.pack(table,Widgets.FILL|Widgets.EXPAND,0);
  }

  void addColumn(String title)
  {
    TableColumn tableColumn = new TableColumn(table,SWT.NONE);
    tableColumn.setText(title);
    tableColumn.pack();
  }
}

class BARGroup
{
  Group group;

  BARGroup(Composite parentComposite, String title, int columns, int style)
  {
    group = new Group(parentComposite,SWT.NONE);
    group.setText(title);
    GridLayout gridLayout = new GridLayout();
    gridLayout.numColumns = columns;
    group.setLayout(gridLayout);
    Widgets.pack(group,style,0);
  }
}

class BARComposite
{
  Composite composite;

  BARComposite(Composite parentComposite, int columns, int style, int paddingX, int paddinyY)
  {
    composite = new Composite(parentComposite,SWT.NONE);
    GridLayout gridLayout = new GridLayout();
    gridLayout.numColumns   = columns;
    gridLayout.marginWidth  = paddingX;
    gridLayout.marginHeight = paddinyY;
    composite.setLayout(gridLayout);
    Widgets.pack(composite,style,0);
  }
}

class BARTab
{
  BARComposite tab;

  BARTab(TabFolder tabFolder, String title, int columns)
  {
    tab = new BARComposite(tabFolder,columns,Widgets.FILL|Widgets.EXPAND,Widgets.PADDING_X,Widgets.PADDING_Y);
    TabItem tabItemStatus = new TabItem(tabFolder,SWT.NONE);
    tabItemStatus.setText(title);
    tabItemStatus.setControl(tab.composite);
  }
}

class BARTabStatus extends BARTab
{
  BARVariable doneFiles    = new BARVariable(0);
  BARVariable storedFiles  = new BARVariable(0);
  BARVariable skippedFiles = new BARVariable(0);
  BARVariable errorFiles   = new BARVariable(0);
  BARVariable totalFiles   = new BARVariable(0);
  BARVariable file         = new BARVariable("");

  BARTabStatus(TabFolder tabFolder)
  {
    super(tabFolder,"Status",1);

    BARTable     list;
    BARGroup     selected;
    BARComposite composite;
    Object       widget;

    list = Widgets.addTable(tab,0,null);
    list.addColumn("Name");
    list.addColumn("State");
    list.addColumn("Type");
    list.addColumn("Part size");
    list.addColumn("Compress");
    list.addColumn("Crypt");
    list.addColumn("Last executed");
    list.addColumn("Estimated time");

    selected = Widgets.addGroup(tab,0,"Selected ''",7,Widgets.FILL_X|Widgets.EXPAND_X);
    Widgets.addLabel(selected,0,"Done:");
    widget = Widgets.addIntegerView(selected,0,doneFiles,0,"files");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addLabel(selected,0,"/");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addNone(selected,2);

    Widgets.addLabel(selected,0,"Stored:");
    Widgets.addIntegerView(selected,0,null,0,"files");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addLabel(selected,0,"/");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    composite = Widgets.addComposite(selected,0,2,Widgets.FILL_X|Widgets.EXPAND_X,0,0);
    Widgets.addLabel(composite,0,"Ratio");
    Widgets.addIntegerView(composite,0,null,0,"%");
    Widgets.addIntegerView(selected,0,null,0,"bytes");

    Widgets.addLabel(selected,0,"Skipped:");
    Widgets.addIntegerView(selected,0,null,0,"files");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addLabel(selected.group,0,"/");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addNone(selected,2);

    Widgets.addLabel(selected,0,"Errors:");
    Widgets.addIntegerView(selected,0,null,0,"files");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addLabel(selected,0,"/");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addNone(selected,2);

    Widgets.addLabel(selected,0,"Total:");
    Widgets.addIntegerView(selected,0,null,0,"files");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addLabel(selected.group,0,"/");
    Widgets.addIntegerView(selected,0,null,0,"bytes");
    Widgets.addIntegerView(selected,0,null,0,"files/s");
    Widgets.addIntegerView(selected,0,null,0,"bytes/s");

    Widgets.addLabel(selected,0,"File:");
    Widgets.addView(selected,6,null,"xxxx");
    Widgets.addNone(selected,0);
    Widgets.addProgressBar(selected,6,null);

    Widgets.addLabel(selected,0,"Storage:");
    Widgets.addView(selected,6,null,"xxxx");
    Widgets.addNone(selected,0);
    Widgets.addProgressBar(selected,6,null);

    Widgets.addLabel(selected,0,"Volume:");
    Widgets.addProgressBar(selected,6,null);

    Widgets.addLabel(selected,0,"Total files:");
    Widgets.addProgressBar(selected,6,null);

    Widgets.addLabel(selected,0,"Total bytes:");
    Widgets.addProgressBar(selected,6,null);

    Widgets.addLabel(selected,0,"Message:");
    Widgets.addView(selected,6,null,"xxx");

    composite = Widgets.addComposite(tab,0,4,Widgets.NONE,Widgets.PADDING_X,Widgets.PADDING_Y);
    Widgets.addButton(composite,0,"Start",null);
    Widgets.addButton(composite,0,"Abort",null);
    Widgets.addButton(composite,0,"Pause",null);
    Widgets.addButton(composite,0,"Volume",null);
  }

  void update(BARServer barServer)
  {
    // update job list
    ArrayList<String> result = new ArrayList<String>();
    int errorCode = barServer.executeCommand("JOB_LIST",result);
System.err.println("BARControl.java"+", "+822+": "+errorCode+": "+result.toString());

    // update selected job info
  }
}

class BARTabStatusUpdate extends Thread
{
  private BARServer    barServer;
  private BARTabStatus barTabStatus;

  BARTabStatusUpdate(BARServer barServer, BARTabStatus barTabStatus)
  {
    this.barServer    = barServer;
    this.barTabStatus = barTabStatus;
  }

  public void run()
  {
    for (;;)
    {
      barTabStatus.update(barServer);

      try { Thread.sleep(1000); } catch (InterruptedException exception) {};
    }
  }
}

class BARTabJobs extends BARTab
{
  BARTabJobs(TabFolder tabFolder)
  {
    super(tabFolder,"Jobs",2);

    Widgets.addButton(tab,0,"0",new SelectionAdapter()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
System.out.println("x0");
      }
    });
    Widgets.addButton(tab,0,"1",null);
    Widgets.addButton(tab,0,"2",new SelectionAdapter()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
System.out.println("x2");
      }
    });
    Widgets.addButton(tab,0,"3",null);
    Widgets.addLabel(tab,0,"check");
    Widgets.addCheckbox(tab,0,"4",null);
    Widgets.addLabel(tab,0,"text");
    Widgets.addText(tab,0,null);

    Widgets.addButton(tab,0,"0",new SelectionAdapter()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
System.out.println("x0");
      }
    });
    Widgets.addButton(tab,0,"1",null);
    Widgets.addButton(tab,0,"2",new SelectionAdapter()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
System.out.println("x2");
      }
    });
    Widgets.addButton(tab,0,"3",null);
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
    Shell shell = new Shell(display);
    shell.setLayout(new GridLayout(1,false));

    // create tabs
    TabFolder tabFolder;
    tabFolder = new TabFolder(shell,SWT.NONE);
    Widgets.pack(tabFolder,Widgets.FILL|Widgets.EXPAND,0);

    BARTabStatus barTabStatus = new BARTabStatus(tabFolder);
    BARTabJobs   barTabJobs   = new BARTabJobs  (tabFolder);

barTabStatus.doneFiles.set(100000000);
barTabStatus.storedFiles.set(1);

    // start status update thread
    new BARTabStatusUpdate(barServer,barTabStatus).start();

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
    new BARControl(args);
  }
}

/* end of file */
