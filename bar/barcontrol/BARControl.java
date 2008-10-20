/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/BARControl.java,v $
* $Revision: 1.2 $
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
import java.lang.String;
import java.lang.System;
import java.net.Socket;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.LinkedList;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Widget;
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
      byte authorizeData[] = new byte[sessionId.length];
      for (int z = 0; z < sessionId.length; z++)
      {
        authorizeData[z] = (byte)(((z < serverPassword.length())?(int)serverPassword.charAt(z):0)^(int)sessionId[z]);
      }
      commandId++;
      String command = Long.toString(commandId)+" AUTHORIZE "+encodeHex(authorizeData);
      output.println(command);
      output.flush();
System.err.println("BARControl.java"+", "+230+": auto command "+command);

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
//  System.err.println("BARControl.java"+", "+701+": "+line);

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

  void addColumn(String title, int style, int width, boolean resizable)
  {
    TableColumn tableColumn = new TableColumn(table,style);
    tableColumn.setText(title);
    tableColumn.setWidth(width);
    tableColumn.setResizable(resizable);
    if (width <= 0) tableColumn.pack();
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
  BARTable    list;

  BARVariable doneFiles    = new BARVariable(0);
  BARVariable storedFiles  = new BARVariable(0);
  BARVariable skippedFiles = new BARVariable(0);
  BARVariable errorFiles   = new BARVariable(0);
  BARVariable totalFiles   = new BARVariable(0);
  BARVariable file         = new BARVariable("");

  BARTabStatus(TabFolder tabFolder)
  {
    super(tabFolder,"Status",1);

    BARGroup     selected;
    BARComposite composite;
    Object       widget;

    list = Widgets.addTable(tab,0,null);
    list.addColumn("#",             SWT.RIGHT, 30,false);
    list.addColumn("Name",          SWT.LEFT, 100,true );
    list.addColumn("State",         SWT.LEFT,  60,true );
    list.addColumn("Type",          SWT.LEFT,   0,true );
    list.addColumn("Part size",     SWT.RIGHT,  0,true );
    list.addColumn("Compress",      SWT.LEFT,   0,true );
    list.addColumn("Crypt",         SWT.LEFT,  80,true );
    list.addColumn("Last executed", SWT.LEFT, 180,true );
    list.addColumn("Estimated time",SWT.LEFT, 120,true );

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

  private int getTableItemIndex(TableItem tableItems[], int id)
  {
    for (int z = 0; z < tableItems.length; z++)
    {
      if ((Integer)tableItems[z].getData() == id) return z;
    }

    return -1;
  }

  private String formatByteSize(long n)
  {
    if      (n > 1024*1024*1024) return String.format("%.1fG",(double)n/(1024*1024*1024));
    else if (n >      1024*1024) return String.format("%.1fM",(double)n/(     1024*1024));
    else if (n >           1024) return String.format("%.1fK",(double)n/(          1024));
    else                         return String.format("%d"   ,n                         );
  }

  private void updateJobList(BARServer barServer)
  {
    if (!list.table.isDisposed())
    {
      // get job list
      ArrayList<String> result = new ArrayList<String>();
      int errorCode = barServer.executeCommand("JOB_LIST",result);

      // update job list
      TableItem tableItems[] = list.table.getItems();
      boolean tableItemFlags[] = new boolean[tableItems.length];
      for (String line : result)
      {
        Object data[] = new Object[10];
        if (StringParser.parse(line,"%d %S %S %s %d %S %S %S %ld %ld",data,StringParser.QUOTE_CHARS))
        {
//  System.err.println("BARControl.java"+", "+747+": "+data[0]+"--"+data[1]);
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
            tableItem = new TableItem(list.table,SWT.NONE);
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
        if (!tableItemFlags[z]) list.table.remove(z);
      }
    }
  }

  private void updateJobInfo(BARServer barServer)
  {
  }

  void update(BARServer barServer)
  {
    // update job list
    updateJobList(barServer);
    updateJobInfo(barServer);
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
      barTabStatus.tab.composite.getDisplay().syncExec(new Runnable()
      {
        public void run()
        {
          barTabStatus.update(barServer);
        }
      });

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
    shell.setSize(800,800);
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
