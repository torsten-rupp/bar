/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/BARControl.java,v $
* $Revision: 1.9 $
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
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.PrintWriter;
import java.io.Serializable;
import java.lang.Double;
import java.lang.Exception;
import java.lang.Integer;
import java.lang.Long;
import java.lang.String;
import java.lang.System;
import java.net.Socket;
import java.text.DateFormat;
import java.text.Collator;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.Locale;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.dnd.ByteArrayTransfer;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.dnd.TransferData;
import org.eclipse.swt.dnd.DND;
import org.eclipse.swt.dnd.DragSource;
import org.eclipse.swt.dnd.DragSourceEvent;
import org.eclipse.swt.dnd.DragSourceListener;
import org.eclipse.swt.dnd.DropTarget;
import org.eclipse.swt.dnd.DropTargetAdapter;
import org.eclipse.swt.dnd.DropTargetEvent;
import org.eclipse.swt.dnd.TextTransfer;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.events.KeyEvent;
import org.eclipse.swt.events.KeyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.events.VerifyEvent;
import org.eclipse.swt.events.VerifyListener;
import org.eclipse.swt.graphics.Cursor;
import org.eclipse.swt.graphics.FontMetrics;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.PaletteData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.DirectoryDialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Group;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.List;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.swt.widgets.MenuItem;
//import org.eclipse.swt.widgets.ProgressBar;
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

// progress bar
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
  BOOLEAN,
  LONG,
  DOUBLE,
  STRING,
  ENUMERATION,
};

class BARVariable
{
  private BARVariableTypes type;
  private boolean          b;
  private long             l;
  private double           d;
  private String           string;
  private String           enumeration[];

  BARVariable(boolean b)
  {
    this.type = BARVariableTypes.BOOLEAN;
    this.b    = b;
  }
  BARVariable(long l)
  {
    this.type = BARVariableTypes.LONG;
    this.l    = l;
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

  boolean getBoolean()
  {
    assert type == BARVariableTypes.BOOLEAN;

    return b;
  }

  long getLong()
  {
    assert type == BARVariableTypes.LONG;

    return l;
  }

  double getDouble()
  {
    assert type == BARVariableTypes.DOUBLE;

    return d;
  }

  String getString()
  {
    assert (type == BARVariableTypes.STRING) || (type == BARVariableTypes.ENUMERATION);

    return string;
  }

  void set(boolean b)
  {
    assert type == BARVariableTypes.BOOLEAN;

    this.b = b;
    Widgets.modified(this);
  }

  void set(long l)
  {
    assert type == BARVariableTypes.LONG;

    this.l = l;
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
        boolean OKFlag = false;
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
    Widgets.modified(this);
  }

  public boolean equals(String value)
  {
    String s = toString();
    return (s != null)?s.equals(value):(value == null);
  }

  public String toString()
  {
    switch (type)
    {
      case BOOLEAN:     return Boolean.toString(b);
      case LONG:        return Long.toString(l);
      case DOUBLE:      return Double.toString(d);
      case STRING:      return string;
      case ENUMERATION: return string;
    }
    return "";
  }
}

class BARServer
{
  private static Socket             socket;
  private static PrintWriter        output;
  private static BufferedReader     input;
  private static long               commandId;
  private static LinkedList<String> lines;

  static boolean debug = false;

  private static byte[] decodeHex(String s)
  {
    byte data[] = new byte[s.length()/2];
    for (int z = 0; z < s.length()/2; z++)
    {
      data[z] = (byte)Integer.parseInt(s.substring(z*2,z*2+2),16);
    }

    return data;
  }

  private static String encodeHex(byte data[])
  {
    StringBuffer stringBuffer = new StringBuffer(data.length*2);
    for (int z = 0; z < data.length; z++)
    {
      stringBuffer.append(String.format("%02x",(int)data[z] & 0xFF));
    }

    return stringBuffer.toString();
  }

  static void connect(String hostname, int port, int tlsPort, String serverPassword)
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

  static void disconnect()
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

  static synchronized int executeCommand(String command, Object result)
  {
    String  line;
    boolean completedFlag;
    int     errorCode;

    // send command
    commandId++;
    line = String.format("%d %s",commandId,command);
    output.println(line);
    if (debug) System.err.println("Server: sent '"+line+"'");
    output.flush();

    // read buffer lines from list
//???

    // clear result data
    if (result != null)
    {
      if      (result instanceof ArrayList)
      {
        ((ArrayList<String>)result).clear();
      }
      else if (result instanceof String[])
      {
        ((String[])result)[0] = "";
      }
      else
      {
        throw new Error("Invalid result data type");
      }
    }

    // read result
    completedFlag = false;
    errorCode = -1;
    try
    {
      while (!completedFlag && (line = input.readLine()) != null)
      {
        if (debug) System.err.println("Server: received '"+line+"'");

        // line format: <id> <error code> <completed> <data>
        String data[] = line.split(" ",4);
        assert data.length == 4;
        if (Integer.parseInt(data[0]) == commandId)
        {
          // check if completed
          if (Integer.parseInt(data[1]) != 0)
          {
            errorCode = Integer.parseInt(data[2]);
            if (errorCode != 0) throw new Error("communication error: "+errorCode+" "+data[3]);
            completedFlag = true;
          }

          if (result != null)
          {
            // store data
            if      (result instanceof ArrayList)
            {
              ((ArrayList<String>)result).add(data[3]);
            }
            else if (result instanceof String[])
            {
              ((String[])result)[0] = data[3];
            }
          }
        }
        else
        {
System.err.println("BARControl.java"+", "+505+": "+commandId+"::"+line);
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

  static int executeCommand(String command)
  {
    return executeCommand(command,null);
  }

  static boolean getBoolean(int jobId, String name)
  {
    String[] result = new String[1];

    executeCommand("OPTION_GET "+jobId+" "+name,result);
    return    result[0].toLowerCase().equals("yes")
           || result[0].toLowerCase().equals("on")
           || result[0].equals("1");
  }

  static int getInt(int jobId, String name)
  {
    String[] result = new String[1];

    executeCommand("OPTION_GET "+jobId+" "+name,result);
    return Integer.parseInt(result[0]);
  }

  static long getLong(int jobId, String name)
  {
    String[] result = new String[1];

    executeCommand("OPTION_GET "+jobId+" "+name,result);
    return Long.parseLong(result[0]);
  }

  static String getString(int jobId, String name)
  {
    String[] result = new String[1];

    executeCommand("OPTION_GET "+jobId+" "+name,result);
    return StringParser.unescape(result[0]);
  }

  static void set(int jobId, String name, boolean b)
  {
    executeCommand("OPTION_SET "+jobId+" "+name+" "+(b?"yes":"no"));
  }

  static void set(int jobId, String name, long n)
  {
    executeCommand("OPTION_SET "+jobId+" "+name+" "+n);
  }

  static void set(int jobId, String name, String s)
  {
    executeCommand("OPTION_SET "+jobId+" "+name+" "+StringParser.escape(s));
  }
}

  //-----------------------------------------------------------------------

class Dialogs
{
  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @param rowWeights row weights or null
   * @param columnWeights column weights or null
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight, double[] rowWeights, double[] columnWeights)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;

    final Shell shell = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
    shell.setText(title);
    tableLayout = new TableLayout(rowWeights,columnWeights,4);
    tableLayout.minWidth  = minWidth;
    tableLayout.minHeight = minHeight;
    shell.setLayout(tableLayout);

    return shell;
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @param minWidth minimal width
   * @param minHeight minimal height
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight)
  {
    return open(parentShell,title,minWidth,minHeight,new double[]{1,0},null);
  }

  /** open a new dialog
   * @param parentShell parent shell
   * @param title title string
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title)
  {
    return open(parentShell,title,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** close a dialog
   * @param dialog dialog shell
   */
  static void close(Shell dialog, Object returnValue)
  {
    dialog.setData(returnValue);
    dialog.close();
  }

  /** close a dialog
   * @param dialog dialog shell
   */
  static void close(Shell dialog)
  {
    close(dialog,null);
  }

  /** run dialog
   * @param dialog dialog shell
   */
  static Object run(Shell dialog)
  {
    final Object[] result = new Object[1];

    dialog.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        Shell widget = (Shell)event.widget;

        result[0] = widget.getData();
      }
    });

    dialog.pack();
    dialog.open();
    Display display = dialog.getParent().getDisplay();
    while (!dialog.isDisposed())
    {
      if (!display.readAndDispatch()) display.sleep();
    }

    return result[0];
  }

  /** info dialog
   * @param parentShell parent shell
   * @param message info message
   */
  static void info(Shell parentShell, String title, Image image, String message)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final Shell shell = open(parentShell,title,200,70);
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    // message
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setImage(image);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,0));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE,0,0));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText("Close");
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.DEFAULT|TableLayoutData.EXPAND_X));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    run(shell);
  }

  /** info dialog
   * @param parentShell parent shell
   * @param message info message
   */
  static void info(Shell parentShell, String title, String message)
  {
/*
    PaletteData paletteData = new PaletteData(0xFF0000,0x00FF00,0x0000FF);
    ImageData imageData = new ImageData(Images.info.width,Images.info.height,Images.info.depth,paletteData,1,Images.info.data);
    imageData.alphaData = Images.info.alphas;
    imageData.alpha = -1;
    Image image = new Image(parentShell.getDisplay(),imageData);
*/
    Image image = new Image(parentShell.getDisplay(),"images/info.gif");

    info(parentShell,title,image,message);
  }

  /** error dialog
   * @param parentShell parent shell
   * @param message error message
   */
  static void error(Shell parentShell, String message)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final Shell shell = open(parentShell,"Error",200,70);
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    Image image = new Image(shell.getDisplay(),"images/error.gif");

    // message
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setImage(image);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,0));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE,0,0));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText("Close");
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.DEFAULT|TableLayoutData.EXPAND_X));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    run(shell);
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param yesText yes-text
   * @param noText no-text
   */
  static boolean confirm(Shell parentShell, String title, String message, String yesText, String noText)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final boolean[] result = new boolean[1];

    final Shell shell = open(parentShell,title,200,70);
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    Image image = new Image(shell.getDisplay(),"images/question.gif");

    // message
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setImage(image);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,0));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText(yesText);
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          result[0] = true;
          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText(noText);
      button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          result[0] = false;
          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    run(shell);

    return result[0];
  }

  /** confirmation dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   */
  static boolean confirm(Shell parentShell, String title, String message)
  {
    return confirm(parentShell,title,message,"Yes","No");
  }

  /** multiple select dialog
   * @param parentShell parent shell
   * @param title title string
   * @param message confirmation message
   * @param texts array with texts
   */
  static int select(Shell parentShell, String title, String message, String[] texts)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final int[] result = new int[1];

//    final Shell shell = open(parentShell,title,200,SWT.DEFAULT);
    final Shell shell = open(parentShell,title);
//    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));
    shell.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    Image image = new Image(shell.getDisplay(),"images/question.gif");

    // message
    composite = new Composite(shell,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{1,0},4);
    composite.setLayout(tableLayout);
//    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setImage(image);
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,0));

      label = new Label(composite,SWT.LEFT|SWT.WRAP);
      label.setText(message);
//      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE|TableLayoutData.EXPAND));
      label.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE));
    }

    // buttons
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      int textWidth = 0;
      GC gc = new GC(composite);
      for (String text : texts)
      {
        textWidth = Math.max(textWidth,gc.textExtent(text).x);
      }
      gc.dispose();

      int n = 0;
      for (String text : texts)
      {
        button = new Button(composite,SWT.CENTER);
        button.setText(text);
        button.setData(n);
        button.setLayoutData(new TableLayoutData(0,n,TableLayoutData.EXPAND_X,0,0,0,0,textWidth,SWT.DEFAULT));
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;

            result[0] = ((Integer)widget.getData()).intValue();
            shell.close();
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });

        n++;
      }
    }

    run(shell);

    return result[0];
  }

  /** open a file dialog
   * @param parentShell parent shell
   * @param type SWT.OPEN or SWT.SAVE
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @return file name or null
   */
  private static String file(Shell parentShell, int type, String title, String fileName, String[] fileExtensions)
  {
    FileDialog dialog = new FileDialog(parentShell,type);
    dialog.setText(title);
    if (fileName != null)
    {
      dialog.setFilterPath(new File(fileName).getParent());
      dialog.setFileName(new File(fileName).getName());
    }
    dialog.setOverwrite(false);
    if (fileExtensions != null)
    {
      assert (fileExtensions.length % 2) == 0;

      String[] fileExtensionNames = new String[fileExtensions.length/2];
      for (int z = 0; z < fileExtensions.length/2; z++)
      {
        fileExtensionNames[z] = fileExtensions[z*2+0]+" ("+fileExtensions[z*2+1]+")";
      }
      String[] fileExtensionPatterns = new String[(fileExtensions.length+1)/2];
      for (int z = 0; z < fileExtensions.length/2; z++)
      {
        fileExtensionPatterns[z] = fileExtensions[z*2+1];
      }
      dialog.setFilterNames(fileExtensionNames);
      dialog.setFilterExtensions(fileExtensionPatterns);
    }

    return dialog.open();  
  }

  /** file dialog for open file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @return file name or null
   */
  static String fileOpen(Shell parentShell, String title, String fileName, String[] fileExtensions)
  {
    return file(parentShell,SWT.OPEN,title,fileName,fileExtensions);
  }

  /** file dialog for save file
   * @param parentShell parent shell
   * @param title title text
   * @param fileName fileName or null
   * @param fileExtensions array with {name,pattern} or null
   * @return file name or null
   */
  static String fileSave(Shell parentShell, String title, String fileName, String[] fileExtensions)
  {
    return file(parentShell,SWT.SAVE,title,fileName,fileExtensions);
  }

  /** directory dialog
   * @param parentShell parent shell
   * @param title title text
   * @param pathName path name or null
   * @return directory name or null
   */
  static String directory(Shell parentShell, String title, String pathName)
  {
    DirectoryDialog dialog = new DirectoryDialog(parentShell);
    dialog.setText(title);
    if (pathName != null)
    {
      dialog.setFilterPath(pathName);
    }

    return dialog.open();  
  }
}

class WidgetListener
{
  private BARVariable variable;
  private Control     control;

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
      if      ((((Button)control).getStyle() & SWT.PUSH) == SWT.PUSH)
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
      else if ((((Button)control).getStyle() & SWT.CHECK) == SWT.CHECK)
      {
        boolean selection = false;
        switch (variable.getType())
        {
          case BOOLEAN: selection = variable.getBoolean(); break;
          case LONG:    selection = (variable.getLong() != 0); break; 
          case DOUBLE:  selection = (variable.getDouble() != 0); break; 
        }
        ((Button)control).setSelection(selection);
      }
      else if ((((Button)control).getStyle() & SWT.RADIO) == SWT.RADIO)
      {
        boolean selection = false;
        switch (variable.getType())
        {
          case BOOLEAN: selection = variable.getBoolean(); break;
          case LONG:    selection = (variable.getLong() != 0); break; 
          case DOUBLE:  selection = (variable.getDouble() != 0); break; 
        }
        ((Button)control).setSelection(selection);
      }
    }
    else if (control instanceof Combo)
    {
      String text = getString(variable);
      if (text == null)
      {
        switch (variable.getType())
        {
          case BOOLEAN:     text = Boolean.toString(variable.getBoolean()); break;
          case LONG:        text = Long.toString(variable.getLong()); break; 
          case DOUBLE:      text = Double.toString(variable.getDouble()); break; 
          case STRING:      text = variable.getString(); break;
          case ENUMERATION: text = variable.getString(); break;
        }
      }
      assert text != null;
      if (!text.equals(cachedText))
      {
        ((Combo)control).setText(text);
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
    else
    {
      throw new Error("Unknown widget '"+control+"' in wiget listener!");
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

  private static LinkedList<WidgetListener> listenersList = new LinkedList<WidgetListener>();

  //-----------------------------------------------------------------------

  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height)
  {
    TableLayoutData tableLayoutData = new TableLayoutData(row,column,style,rowSpawn,columnSpawn,padX,padY,width,height);
//    tableLayoutData.minWidth  = width;
//    tableLayoutData.minHeight = height;
    control.setLayoutData(tableLayoutData);
  }

  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,padX,padY,size.x,size.y);
  }

  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, Point pad, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,pad.x,pad.y,size.x,size.y);
  }

  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, int width, int height)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,0,0,width,height);
  }

  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn, Point size)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,size.x,size.y);
  }

  static void layout(Control control, int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    layout(control,row,column,style,rowSpawn,columnSpawn,0,0,SWT.DEFAULT,SWT.DEFAULT);
  }

  static void layout(Control control, int row, int column, int style)
  {
    layout(control,row,column,style,0,0);
  }

  static Point getTextSize(Control control, String text)
  {
    Point size;

    GC gc = new GC(control);
    size = gc.textExtent(text);
    gc.dispose();

    return size;
  }

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

  static void setVisible(Control control, boolean visibleFlag)
  {
    TableLayoutData tableLayoutData = (TableLayoutData)control.getLayoutData();
    tableLayoutData.exclude = !visibleFlag;
    control.setVisible(visibleFlag);
    control.getParent().layout();
  }

  static Label newLabel(Composite composite, String text, int style)
  {
    Label label;

    label = new Label(composite,style);
    label.setText(text);

    return label;
  }

  static Label newLabel(Composite composite, Image image, int style)
  {
    Label label;

    label = new Label(composite,style);
    label.setImage(image);

    return label;
  }

  static Label newLabel(Composite composite, String text)
  {
    return newLabel(composite,text,SWT.LEFT);
  }

  static Label newLabel(Composite composite, Image image)
  {
    return newLabel(composite,image,SWT.LEFT);
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

  static Text newText(Composite composite, Object data)
  {
    Text text;

    text = new Text(composite,SWT.BORDER|SWT.V_SCROLL|SWT.SINGLE);
    text.setData(data);

    return text;
  }

  static List newList(Composite composite, Object data)
  {
    List list;

    list = new List(composite,SWT.BORDER|SWT.MULTI|SWT.V_SCROLL);
    list.setData(data);

    return list;
  }

  static Combo newCombo(Composite composite, Object data)
  {
    Combo combo;

    combo = new Combo(composite,SWT.BORDER);
    combo.setData(data);

    return combo;
  }

  static Combo newOptionMenu(Composite composite, Object data)
  {
    Combo combo;

    combo = new Combo(composite,SWT.RIGHT|SWT.READ_ONLY);
    combo.setData(data);

    return combo;
  }

  static Spinner newSpinner(Composite composite, Object data)
  {
    Spinner spinner;

    spinner = new Spinner(composite,SWT.READ_ONLY);
    spinner.setData(data);

    return spinner;
  }

  static Table newTable(Composite composite, Object data)
  {
    Table table;

    table = new Table(composite,SWT.BORDER|SWT.MULTI|SWT.FULL_SELECTION);
    table.setLinesVisible(true);
    table.setHeaderVisible(true);
    table.setData(data);

    return table;
  }

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
    TableItem[] tableItems = table.getItems();

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

          // discard item
          tableItems[i].dispose();

          // create new item
          TableItem tableItem = new TableItem(table,SWT.NONE,j);
          tableItem.setData(data);
          tableItem.setText(texts);

          tableItems = table.getItems();
        }
      }
    }
    table.setSortColumn(tableColumn);
    table.setSortDirection(sortDirection);
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

  static TreeColumn addTreeColumn(Tree tree, String title, int style, int width, boolean resizable)
  {
    TreeColumn treeColumn = new TreeColumn(tree,style);
    treeColumn.setText(title);
    treeColumn.setWidth(width);
    treeColumn.setResizable(resizable);
    if (width <= 0) treeColumn.pack();

    return treeColumn;
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

  private static TreeItem recreateTreeItem(Tree tree, TreeItem parentTreeItem, TreeItem treeItem, int index)
  {
    // save data
    Object   data = treeItem.getData();
    String[] texts = new String[tree.getColumnCount()];
    for (int z = 0; z < tree.getColumnCount(); z++)
    {
      texts[z] = treeItem.getText(z);
    }
    Image image = treeItem.getImage();

    // recreate item
    TreeItem newTreeItem = new TreeItem(parentTreeItem,SWT.NONE,index);
    newTreeItem.setData(data);
    newTreeItem.setText(texts);
    newTreeItem.setImage(image);
    for (TreeItem subTreeItem : treeItem.getItems())
    {
      recreateTreeItem(tree,newTreeItem,subTreeItem);
    }

    // discard old item
    treeItem.dispose();

    return newTreeItem;
  }

  private static TreeItem recreateTreeItem(Tree tree, TreeItem parentTreeItem, TreeItem treeItem)
  {
    return recreateTreeItem(tree,parentTreeItem,treeItem,parentTreeItem.getItemCount());
  }

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

   private static void getExpandedDiretories(HashSet expandedDirectories, TreeItem treeItem)
   {
     if (treeItem.getExpanded()) expandedDirectories.add(treeItem.getData());
     for (TreeItem subTreeItem : treeItem.getItems())
     {
       getExpandedDiretories(expandedDirectories,subTreeItem);
     }
   }  

   private static void rexpandDiretories(HashSet expandedDirectories, TreeItem treeItem)
   {
     treeItem.setExpanded(expandedDirectories.contains(treeItem.getData()));
     for (TreeItem subTreeItem : treeItem.getItems())
     {
       rexpandDiretories(expandedDirectories,subTreeItem);
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
      rexpandDiretories(expandedDirectories,treeItem);
    }

    // set column sort indicators
    tree.setSortColumn(treeColumn);
    tree.setSortDirection(sortDirection);
//System.err.println("2 ---------------");
//printTree(tree);
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
    layout(sashForm,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);

    return sashForm;
  }

  static TabFolder newTabFolder(Composite composite)
  {    
    TabFolder tabFolder = new TabFolder(composite,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));

    return tabFolder;
  }

/*
  static TabFolder newTabFolder(Composite composite)
  {    
    // create resizable tab (with help of sashForm)
    SashForm sashForm = new SashForm(composite,SWT.NONE);
    sashForm.setLayout(new TableLayout());
    layout(sashForm,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
    TabFolder tabFolder = new TabFolder(sashForm,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));

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

  static Canvas newCanvas(Composite composite, int style)
  {    
    Canvas canvas = new Canvas(composite,style);
//    canvas.setLayoutData(new TableLayoutData(1,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));

    return canvas;
  }

  static Menu newMenuBar(Shell shell)
  {
    Menu menu = new Menu(shell,SWT.BAR);
    shell.setMenuBar(menu);

    return menu;
  }

  static Menu addMenu(Menu menu, String text)
  {
    MenuItem menuItem = new MenuItem(menu,SWT.CASCADE);
    menuItem.setText(text);
    Menu subMenu = new Menu(menu.getShell(),SWT.DROP_DOWN);
    menuItem.setMenu(subMenu);

    return subMenu;
  }

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

  static MenuItem addMenuItem(Menu menu, String text)
  {
    return addMenuItem(menu,text,0);
  }

  static MenuItem addMenuSeparator(Menu menu)
  {
    MenuItem menuItem = new MenuItem(menu,SWT.SEPARATOR);

    return menuItem;
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

  static void notify(Control control, int type, Widget widget, Widget item)
  {
    if (control.isEnabled())
    {
      Event event = new Event();
      event.widget = widget;
      event.item   = item;
      control.notifyListeners(type,event);
System.err.println("BARControl.java"+", "+1923+": ");
    }
  }

  static void notify(Control control, int type, Widget widget)
  {
    notify(control,type,widget,null);
  }

  static void notify(Control control, int type)
  {
    notify(control,type,control,null);
  }

  static void notify(Control control)
  {
    notify(control,SWT.Selection,control,null);
  }
}

class Units
{
  public static String getByteSize(double n)
  {
    if      (n >= 1024*1024*1024) return String.format("%.1f",n/(1024*1024*1024));
    else if (n >=      1024*1024) return String.format("%.1f",n/(     1024*1024));
    else if (n >=           1024) return String.format("%.1f",n/(          1024));
    else                          return String.format("%d"  ,(long)n           );
  }

  public static String getByteUnit(double n)
  {
    if      (n >= 1024*1024*1024) return "GBytes";
    else if (n >=      1024*1024) return "MBytes";
    else if (n >=           1024) return "KBytes";
    else                          return "bytes";
  }

  public static String getByteShortUnit(double n)
  {
    if      (n >= 1024*1024*1024) return "GB";
    else if (n >=      1024*1024) return "MB";
    else if (n >=           1024) return "KB";
    else                          return "";
  }

  public static long parseByteSize(String string)
  {
    string = string.toUpperCase();

    if      (string.endsWith("GB"))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-2))*1024*1024*1024);
    }
    else if (string.endsWith("G"))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-1))*1024*1024*1024);
    }
    else if (string.endsWith("MB"))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-2))*1024*1024);
    }
    else if (string.endsWith("M"))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-1))*1024*1024);
    }
    else if (string.endsWith("KB"))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-2))*1024);
    }
    else if (string.endsWith("K"))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-1))*1024);
    }
    else
    {
      return (long)Double.parseDouble(string);
    }
  }

  public static String formatByteSize(long n)
  {
    return getByteSize(n)+getByteShortUnit(n);
  }
}

class TabStatus
{
  enum States
  {
    RUNNING,
    PAUSE,
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
    long   lastExecutedDateTime;
    long   estimatedRestTime;

    /** create job data
     */
    JobData(int id, String name, String state, String type, long archivePartSize, String compressAlgorithm, String cryptAlgorithm, String cryptType, long lastExecutedDateTime, long estimtedRestTime)
    {
      this.id                   = id;
      this.name                 = name;
      this.state                = state;
      this.type                 = type;
      this.archivePartSize      = archivePartSize;
      this.compressAlgorithm    = compressAlgorithm;
      this.cryptAlgorithm       = cryptAlgorithm;
      this.cryptType            = cryptType;
      this.lastExecutedDateTime = lastExecutedDateTime;
      this.estimatedRestTime    = estimatedRestTime;
    }

    /** get id of job data
     * @return job id
     */
    int getId()
    {
      return id;
    }

    /** get job name
     * @return job name
     */
    String getName()
    {
      return name;
    }

    /** get job state
     * @return state
     */
    String getState()
    {
      return state;
    }

    /** get job type
     * @return job type
     */
    String getType()
    {
      return type;
    }

    /** get job archive part size
     * @return archive part size (bytes)
     */
    long getArchivePartSize()
    {
      return archivePartSize;
    }

    /** get job compress algorithm
     * @return compress algorithm
     */
    String getCompressAlgorithm()
    {
      return compressAlgorithm;
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
      return DateFormat.getDateTimeInstance().format(new Date(lastExecutedDateTime*1000));
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

      return String.format("%2d days %02d:%02d:%02d",estimatedRestDays,estimatedRestHours,estimatedRestMinutes,estimatedRestSeconds);
    }

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
     */
    JobDataComparator(Table table)
    {
      TableColumn sortColumn = table.getSortColumn();

      if      (table.getColumn(1) == sortColumn) sortMode = SORTMODE_NAME;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_STATE;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (table.getColumn(4) == sortColumn) sortMode = SORTMODE_PARTSIZE;
      else if (table.getColumn(5) == sortColumn) sortMode = SORTMODE_COMPRESS;
      else if (table.getColumn(6) == sortColumn) sortMode = SORTMODE_CRYPT;
      else if (table.getColumn(7) == sortColumn) sortMode = SORTMODE_LAST_EXECUTED_DATETIME;
      else if (table.getColumn(8) == sortColumn) sortMode = SORTMODE_ESTIMATED_TIME;
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
  Shell       shell;
  TabJobs     tabJobs;

  // widgets
  Composite   widgetTab;
  Table       widgetJobList;
  Group       widgetSelectedJob;
  Button      widgetButtonStart;
  Button      widgetButtonAbort;
  Button      widgetButtonTogglePause;
  Button      widgetButtonVolume;
  Button      widgetButtonQuit;

  // BAR variables
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

  // variables
  private HashMap<String,JobData> jobList = new HashMap<String,JobData> ();
  private JobData selectedJobData         = null;
  private States  status                  = States.RUNNING;

boolean xxx=false;

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
        try
        {
          tabStatus.widgetTab.getDisplay().syncExec(new Runnable()
          {
            public void run()
            {
              tabStatus.update();
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

  TabStatus(TabFolder parentTabFolder, int accelerator)
  {
    TableColumn tableColumn;
    Group       group;
    Composite   composite;
    Button      button;
    Label       label;
    ProgressBar progressBar;

    // get shell
    shell = parentTabFolder.getShell();

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Status"+((accelerator != 0)?" ("+Widgets.acceleratorToText(accelerator)+")":""));
    widgetTab.setLayout(new TableLayout(new double[]{1,0,0},
                                        null,
                                        2
                                       )
                       );
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);

    // list with jobs
    widgetJobList = Widgets.newTable(widgetTab,this);
    Widgets.layout(widgetJobList,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
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
        TableColumn       tableColumn = (TableColumn)selectionEvent.widget;
        JobDataComparator jobDataComparator = new JobDataComparator(widgetJobList);

        synchronized(jobList)
        {
          Widgets.sortTableColumn(widgetJobList,tableColumn,jobDataComparator);
        }
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    };
    tableColumn = Widgets.addTableColumn(widgetJobList,0,"#",             SWT.RIGHT, 30,false);
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobList,1,"Name",          SWT.LEFT, 100,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobList,2,"State",         SWT.LEFT,  60,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobList,3,"Type",          SWT.LEFT,  80,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobList,4,"Part size",     SWT.RIGHT,  0,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobList,5,"Compress",      SWT.LEFT,  100,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobList,6,"Crypt",         SWT.LEFT,  80,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobList,7,"Last executed", SWT.LEFT, 180,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);
    tableColumn = Widgets.addTableColumn(widgetJobList,8,"Estimated time",SWT.LEFT, 120,true );
    tableColumn.addSelectionListener(jobListColumnSelectionListener);

    // selected job group
    widgetSelectedJob = Widgets.newGroup(widgetTab,"Selected ''",SWT.NONE);
    widgetSelectedJob.setLayout(new TableLayout(null,
                                                new double[]{0,1,0,1,0,0,1,0},
                                                4
                                               )
                               );
    Widgets.layout(widgetSelectedJob,1,0,TableLayoutData.WE);
    {
      // done files/bytes
      label = Widgets.newLabel(widgetSelectedJob,"Done:");
      Widgets.layout(label,0,0,TableLayoutData.W);
      label= Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,doneFiles));
      label = Widgets.newLabel(widgetSelectedJob,"files");
      Widgets.layout(label,0,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,0,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,doneBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,0,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,0,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,0,6,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,doneBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob);
      Widgets.layout(label,0,7,TableLayoutData.W,0,0,Widgets.getTextSize(label,new String[]{"bytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,doneBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // stored files/bytes
      label = Widgets.newLabel(widgetSelectedJob,"Stored:");
      Widgets.layout(label,1,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,1,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,storedBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,1,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,1,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,1,6,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,storedBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob);
      Widgets.layout(label,1,7,TableLayoutData.W,0,0,Widgets.getTextSize(label,new String[]{"bytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,storedBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      composite = Widgets.newComposite(widgetSelectedJob,SWT.NONE);
      Widgets.layout(composite,1,8,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      {
        label = Widgets.newLabel(composite,"Ratio");
        Widgets.layout(label,0,0,TableLayoutData.W);
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(label,ratio)
        {
          public String getString(BARVariable variable)
          {
            return String.format("%.1f",variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,"%");
        Widgets.layout(label,0,2,TableLayoutData.W);
      }

      composite = Widgets.newComposite(widgetSelectedJob,SWT.NONE);
      Widgets.layout(composite,1,9,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(label,storageBytesPerSecond)
        {
          public String getString(BARVariable variable)
          {
            return Units.getByteSize(variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite);
        Widgets.layout(label,0,1,TableLayoutData.W,0,0,Widgets.getTextSize(label,new String[]{"bytes","MBytes","GBytes"}));
        Widgets.addModifyListener(new WidgetListener(label,storageBytesPerSecond)
        {
          public String getString(BARVariable variable)
          {
            return Units.getByteUnit(variable.getDouble())+"/s";
          }
        });
      }

      // skipped files/bytes, ratio
      label = Widgets.newLabel(widgetSelectedJob,"Skipped:");
      Widgets.layout(label,2,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,2,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,skippedFiles));
      label = Widgets.newLabel(widgetSelectedJob,"files");
      Widgets.layout(label,2,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,2,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,skippedBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,2,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,2,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,2,6,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,skippedBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob);
      Widgets.layout(label,2,7,TableLayoutData.W,0,0,Widgets.getTextSize(label,new String[]{"bytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,skippedBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // error files/bytes
      label = Widgets.newLabel(widgetSelectedJob,"Errors:");
      Widgets.layout(label,3,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,errorFiles));
      label = Widgets.newLabel(widgetSelectedJob,"files");
      Widgets.layout(label,3,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,errorFiles));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,3,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,3,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,3,6,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,errorBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob);
      Widgets.layout(label,3,7,TableLayoutData.W,0,0,Widgets.getTextSize(label,new String[]{"bytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,errorBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      // total files/bytes, files/s, bytes/s
      label = Widgets.newLabel(widgetSelectedJob,"Total:");
      Widgets.layout(label,4,0,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,totalFiles));
      label = Widgets.newLabel(widgetSelectedJob,"files");
      Widgets.layout(label,4,2,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,totalBytes));
      label = Widgets.newLabel(widgetSelectedJob,"bytes");
      Widgets.layout(label,4,4,TableLayoutData.W);
      label = Widgets.newLabel(widgetSelectedJob,"/");
      Widgets.layout(label,4,5,TableLayoutData.W);
      label = Widgets.newNumberView(widgetSelectedJob);
      Widgets.layout(label,4,6,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      Widgets.addModifyListener(new WidgetListener(label,totalBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteSize(variable.getLong());
        }
      });
      label = Widgets.newLabel(widgetSelectedJob);
      Widgets.layout(label,4,7,TableLayoutData.W,0,0,Widgets.getTextSize(label,new String[]{"bytes","MBytes","GBytes"}));
      Widgets.addModifyListener(new WidgetListener(label,totalBytes)
      {
        public String getString(BARVariable variable)
        {
          return Units.getByteUnit(variable.getLong());
        }
      });

      composite = Widgets.newComposite(widgetSelectedJob,SWT.NONE);
      Widgets.layout(composite,4,8,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(label,filesPerSecond)
        {
          public String getString(BARVariable variable)
          {
            return String.format("%.1f",variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite,"files/s");
        Widgets.layout(label,0,1,TableLayoutData.W);
      }

      composite = Widgets.newComposite(widgetSelectedJob,SWT.NONE);
      Widgets.layout(composite,4,9,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      {
        label = Widgets.newNumberView(composite);
        Widgets.layout(label,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(label,bytesPerSecond)
        {
          public String getString(BARVariable variable)
          {
            return Units.getByteSize(variable.getDouble());
          }
        });
        label = Widgets.newLabel(composite);
        Widgets.layout(label,0,1,TableLayoutData.W,0,0,Widgets.getTextSize(label,new String[]{"bytes","MBytes","GBytes"}));
        Widgets.addModifyListener(new WidgetListener(label,bytesPerSecond)
        {
          public String getString(BARVariable variable)
          {
            return Units.getByteUnit(variable.getDouble())+"/s";
          }
        });
      }

      // current file, file percentage
      label = Widgets.newLabel(widgetSelectedJob,"File:");
      Widgets.layout(label,5,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,5,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,9);
      Widgets.addModifyListener(new WidgetListener(label,fileName));
      progressBar = Widgets.newProgressBar(widgetSelectedJob,null);
      Widgets.layout(progressBar,6,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,fileProgress));

      // storage file, storage percentage
      label = Widgets.newLabel(widgetSelectedJob,"Storage:");
      Widgets.layout(label,7,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,7,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,9);
      Widgets.addModifyListener(new WidgetListener(label,storageName));
      progressBar = Widgets.newProgressBar(widgetSelectedJob,null);
      Widgets.layout(progressBar,8,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,storageProgress));

      // volume percentage
      label = Widgets.newLabel(widgetSelectedJob,"Volume:");
      Widgets.layout(label,9,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob,null);
      Widgets.layout(progressBar,9,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,volumeProgress));

      // total files percentage
      label = Widgets.newLabel(widgetSelectedJob,"Total files:");
      Widgets.layout(label,10,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob,null);
      Widgets.layout(progressBar,10,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,totalFilesProgress));

      // total bytes percentage
      label = Widgets.newLabel(widgetSelectedJob,"Total bytes:");
      Widgets.layout(label,11,0,TableLayoutData.W);
      progressBar = Widgets.newProgressBar(widgetSelectedJob,null);
      Widgets.layout(progressBar,11,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,9);
      Widgets.addModifyListener(new WidgetListener(progressBar,totalBytesProgress));

      // message
      label = Widgets.newLabel(widgetSelectedJob,"Message:");
      Widgets.layout(label,12,0,TableLayoutData.W);
      label = Widgets.newView(widgetSelectedJob);
      Widgets.layout(label,12,1,TableLayoutData.WE|TableLayoutData.EXPAND_X,0,9);
      Widgets.addModifyListener(new WidgetListener(label,message));
    }

    // buttons
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    Widgets.layout(composite,2,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      widgetButtonStart = Widgets.newButton(composite,null,"Start");
      Widgets.layout(widgetButtonStart,0,0,TableLayoutData.DEFAULT);
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

      widgetButtonAbort = Widgets.newButton(composite,null,"Abort");
      Widgets.layout(widgetButtonAbort,0,1,TableLayoutData.DEFAULT);
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

      widgetButtonTogglePause = Widgets.newButton(composite,null,"Continue");
      Widgets.layout(widgetButtonTogglePause,0,2,TableLayoutData.DEFAULT); // how to calculate correct max. width? ,0,0,Widgets.getTextSize(widgetButtonTogglePause,new String[]{"Pause","Continue"}));
      widgetButtonTogglePause.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          jobTogglePause();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      widgetButtonVolume = Widgets.newButton(composite,null,"Volume");
      Widgets.layout(widgetButtonVolume,0,3,TableLayoutData.DEFAULT);
      widgetButtonVolume.setEnabled(false);
      widgetButtonVolume.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      widgetButtonQuit = Widgets.newButton(composite,null,"Quit");
      Widgets.layout(widgetButtonQuit,0,4,TableLayoutData.E|TableLayoutData.EXPAND_X);
      widgetButtonQuit.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          shell.close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // start status update thread
    TabStatusUpdateThread tabStatusUpdateThread = new TabStatusUpdateThread(this);
    tabStatusUpdateThread.setDaemon(true);
    tabStatusUpdateThread.start();
  }

  //-----------------------------------------------------------------------

  void setTabJobs(TabJobs tabJobs)
  {
    this.tabJobs = tabJobs;
  }

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
    BARServer.executeCommand("STATUS",result);

    if (result[0].equals("pause"))
    {
      status = States.PAUSE;
      widgetButtonTogglePause.setText("Continue");
    }
    else
    {
      status = States.RUNNING;
      widgetButtonTogglePause.setText("Pause");
    }
//widgetButtonTogglePause.getParent().layout();
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

  /** find index for insert of job data in sorted job table
   * @param jobData job data
   * @return index in job table
   */
  private int findJobListIndex(JobData jobData)
  {
    TableItem         tableItems[] = widgetJobList.getItems();
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
      ArrayList<String> result = new ArrayList<String>();
      int errorCode = BARServer.executeCommand("JOB_LIST",result);
      if (errorCode != 0) return;

      // update job list
      synchronized(jobList)
      {
        jobList.clear();
        TableItem[] tableItems     = widgetJobList.getItems();
        boolean[]   tableItemFlags = new boolean[tableItems.length];
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
            // get data
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

            // get/create table item
            TableItem tableItem;
            JobData   jobData = new JobData(id,name,state,type,archivePartSize,compressAlgorithm,cryptAlgorithm,cryptType,lastExecutedDateTime,estimatedRestTime);;
            int index = getTableItemIndex(tableItems,id);
            if (index >= 0)
            {
              tableItem = tableItems[index];
              tableItemFlags[index] = true;
  //System.err.println("BARControl.java"+", "+2266+": id="+id+" index="+index+" "+tableItem.getText(1));              
            }
            else
            {
              tableItem = new TableItem(widgetJobList,SWT.NONE,findJobListIndex(jobData));
            }
            tableItem.setData(jobData);

            jobList.put(name,jobData);
            tableItem.setText(0,Integer.toString(jobData.getId()));
            tableItem.setText(1,jobData.getName());
            tableItem.setText(2,(status != States.PAUSE)?jobData.getState():"pause");
            tableItem.setText(3,jobData.getType());
            tableItem.setText(4,Units.formatByteSize(jobData.getArchivePartSize()));
            tableItem.setText(5,jobData.getCompressAlgorithm());
            tableItem.setText(6,jobData.getCryptAlgorithm());
            tableItem.setText(7,jobData.formatLastExecutedDateTime());
            tableItem.setText(8,jobData.formatEstimatedRestTime());
          }
        }
        for (int z = 0; z < tableItems.length; z++)
        {
          if (!tableItemFlags[z]) widgetJobList.remove(z);
        }
      }
    }
  }

  /** update job information
   */
  private void updateJobInfo()
  {
    if (selectedJobData != null)
    {
      // get job info
      String result[] = new String[1];
      int errorCode = BARServer.executeCommand(String.format("JOB_INFO %d",selectedJobData.id),result);
      if (errorCode != 0) return;

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

         widgetButtonStart.setEnabled(!state.equals("running") && !state.equals("waiting") && !state.equals("pause"));
         widgetButtonAbort.setEnabled(state.equals("waiting") || state.equals("running"));
//         widgetButtonVolume.setEnabled(state.equals("running"));
      }
    }
  }

  /** update status, job list, job data
   */
  void update()
  {
    // update job list
    updateStatus();
    updateJobList();
//if (!xxx) { updateJobList(); xxx=true; }
    updateJobInfo();
  }

  /** start selected job
   */
  private void jobStart()
  {
    assert selectedJobData != null;

    switch (Dialogs.select(shell,"Start job","Start job '"+selectedJobData.name+"'?",new String[]{"Full","Incremental","Cancel"}))
    {
      case 0:
        BARServer.executeCommand("JOB_START "+selectedJobData.id+" full");
        break;
      case 1:
        BARServer.executeCommand("JOB_START "+selectedJobData.id+" incremental");
        break;
      case 2:
        break;
    }
  }

  /** abort selected job
   */
  private void jobAbort()
  {
    assert selectedJobData != null;

    BARServer.executeCommand("JOB_ABORT "+selectedJobData.id);
  }

  /** toggle pause all jobs
   */
  private void jobTogglePause()
  {
    switch (status)
    {
      case RUNNING: BARServer.executeCommand("PAUSE"   ); break;
      case PAUSE:   BARServer.executeCommand("CONTINUE"); break;
    }
  }
}

class TabJobs
{
  /** pattern types
   */
  enum PatternTypes
  {
    INCLUDE,
    EXCLUDE,
  };

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

  /** file tree data
   */
  class FileTreeData
  {
    String    name;
    FileTypes type;
    long      size;
    long      datetime;
    String    title;

    FileTreeData(String name, FileTypes type, long size, long datetime, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = size;
      this.datetime = datetime;
      this.title    = title;
    }

    FileTreeData(String name, FileTypes type, long datetime, String title)
    {
      this.name     = name;
      this.type     = type;
      this.size     = 0;
      this.datetime = datetime;
      this.title    = title;
    }

    FileTreeData(String name, FileTypes type, String title)
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
  class FileTreeDataComparator implements Comparator<FileTreeData>
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
    FileTreeDataComparator(Tree tree)
    {
      TreeColumn sortColumn = tree.getSortColumn();

      if      (tree.getColumn(0) == sortColumn) sortMode = SORTMODE_NAME;
      else if (tree.getColumn(1) == sortColumn) sortMode = SORTMODE_TYPE;
      else if (tree.getColumn(2) == sortColumn) sortMode = SORTMODE_SIZE;
      else if (tree.getColumn(3) == sortColumn) sortMode = SORTMODE_DATETIME;
      else                                      sortMode = SORTMODE_NAME;
    }

    /** compare file tree data without take care about type
     * @param fileTreeData1, fileTreeData2 file tree data to compare
     * @return -1 iff fileTreeData1 < fileTreeData2,
                0 iff fileTreeData1 = fileTreeData2, 
                1 iff fileTreeData1 > fileTreeData2
     */
    private int compareWithoutType(FileTreeData fileTreeData1, FileTreeData fileTreeData2)
    {
      switch (sortMode)
      {
        case SORTMODE_NAME:
          return fileTreeData1.title.compareTo(fileTreeData2.title);
        case SORTMODE_TYPE:
          return fileTreeData1.type.compareTo(fileTreeData2.type);
        case SORTMODE_SIZE:
          if      (fileTreeData1.size < fileTreeData2.size) return -1;
          else if (fileTreeData1.size > fileTreeData2.size) return  1;
          else                                              return  0;
        case SORTMODE_DATETIME:
          if      (fileTreeData1.datetime < fileTreeData2.datetime) return -1;
          else if (fileTreeData1.datetime > fileTreeData2.datetime) return  1;
          else                                                      return  0;
        default:
          return 0;
      }
    }

    /** compare file tree data
     * @param fileTreeData1, fileTreeData2 file tree data to compare
     * @return -1 iff fileTreeData1 < fileTreeData2,
                0 iff fileTreeData1 = fileTreeData2, 
                1 iff fileTreeData1 > fileTreeData2
     */
    public int compare(FileTreeData fileTreeData1, FileTreeData fileTreeData2)
    {
//System.err.println("BARControl.java"+", "+2734+": file1="+fileTreeData1+" file=2"+fileTreeData2+" "+sortMode);
      if (fileTreeData1.type == FileTypes.DIRECTORY)
      {
        if (fileTreeData2.type == FileTypes.DIRECTORY)
        {
          return compareWithoutType(fileTreeData1,fileTreeData2);
        }
        else
        {
          return -1;
        }
      }
      else
      {
        if (fileTreeData2.type == FileTypes.DIRECTORY)
        {
          return 1;
        }
        else
        {
          return compareWithoutType(fileTreeData1,fileTreeData2);
        }
      }
    }

    public String toString()
    {
      return "FileComparator {"+sortMode+"}";
    }
  }

  /** schedule data
   */
  class ScheduleData
  {
    final static String ANY          = "*";
    final static String DEFAULT_TYPE = "*";

    String year,month,day,weekDay;
    String hour,minute;
    String type;

    ScheduleData()
    {
      this.year    = ScheduleData.ANY;
      this.month   = ScheduleData.ANY;
      this.day     = ScheduleData.ANY;
      this.weekDay = ScheduleData.ANY;
      this.hour    = ScheduleData.ANY;
      this.minute  = ScheduleData.ANY;
      this.type    = ScheduleData.DEFAULT_TYPE;
    }

    ScheduleData(String year, String month, String day, String weekDay, String hour, String minute, String type)
    {
      this.year    = year;
      this.month   = month;
      this.day     = day;
      this.weekDay = weekDay;
      this.hour    = hour;
      this.minute  = minute;
      this.type    = type;
    }

    String getDate()
    {
      return String.format("%s-%s-%s",year,month,day);
    }

    String getWeekDay()
    {
      return weekDay;
    }

    String getTime()
    {
      return String.format("%s:%s",hour,minute);
    }

    String getType()
    {
      return type;
    }

    public String toString()
    {
      return "File {"+getDate()+", "+getWeekDay()+", "+getTime()+", "+getType()+"}";
    }
  };

  /** schedule data comparator
   */
  class ScheduleDataComparator implements Comparator<ScheduleData>
  {
    // Note: enum in inner classes are not possible in Java, thus use the old way...
    private final static int SORTMODE_DATE    = 0;
    private final static int SORTMODE_WEEKDAY = 1;
    private final static int SORTMODE_TIME    = 2;
    private final static int SORTMODE_TYPE    = 3;

    private int sortMode;

    /** create schedule data comparator
     * @param table schedule table
     */
    ScheduleDataComparator(Table table)
    {
      TableColumn sortColumn = table.getSortColumn();

      if      (table.getColumn(0) == sortColumn) sortMode = SORTMODE_DATE;
      else if (table.getColumn(1) == sortColumn) sortMode = SORTMODE_WEEKDAY;
      else if (table.getColumn(2) == sortColumn) sortMode = SORTMODE_TIME;
      else if (table.getColumn(3) == sortColumn) sortMode = SORTMODE_TYPE;
      else                                       sortMode = SORTMODE_DATE;
    }

    /** compare schedule data
     * @param scheduleData1, scheduleData2 file tree data to compare
     * @return -1 iff scheduleData1 < scheduleData2,
                0 iff scheduleData1 = scheduleData2, 
                1 iff scheduleData1 > scheduleData2
     */
    public int compare(ScheduleData scheduleData1, ScheduleData scheduleData2)
    {
      switch (sortMode)
      {
        case SORTMODE_DATE:
          String date1 = scheduleData1.year+"-"+scheduleData1.month+"-"+scheduleData1.day;
          String date2 = scheduleData2.year+"-"+scheduleData2.month+"-"+scheduleData2.day;

          return date1.compareTo(date2);
        case SORTMODE_WEEKDAY:
          return scheduleData1.weekDay.compareTo(scheduleData2.weekDay);
        case SORTMODE_TIME:
          String time1 = scheduleData1.hour+":"+scheduleData1.minute;
          String time2 = scheduleData2.hour+":"+scheduleData2.minute;

          return time1.compareTo(time2);
        case SORTMODE_TYPE:
          return scheduleData1.type.compareTo(scheduleData2.type);
        default:
          return 0;
      }
    }
  }

  // global variable references
  Shell       shell;
  TabStatus   tabStatus;

  // images
  Image       imageDirectory;
  Image       imageDirectoryIncluded;
  Image       imageDirectoryExcluded;
  Image       imageFile;
  Image       imageFileIncluded;
  Image       imageFileExcluded;
  Image       imageLink;
  Image       imageLinkIncluded;
  Image       imageLinkExcluded;

  // widgets
  Composite   widgetTab;
  Combo       widgetJobList;
  Tree        widgetFileTree;
  List        widgetIncludedPatterns;
  List        widgetExcludedPatterns;
  Combo       widgetArchivePartSize;
  Combo       widgetFTPMaxBandWidth;
  Combo       widgetSCPSFTPMaxBandWidth;
  Table       widgetScheduleList;

  // BAR variables
  BARVariable skipUnreadable          = new BARVariable(false);
  BARVariable overwriteFiles          = new BARVariable(false);

  BARVariable archiveType             = new BARVariable(new String[]{"normal","full","incremental"});
  BARVariable archivePartSizeFlag     = new BARVariable(false);
  BARVariable archivePartSize         = new BARVariable(0);
  BARVariable compressAlgorithm       = new BARVariable(new String[]{"none","zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9","bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9"});
  BARVariable cryptAlgorithm          = new BARVariable(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256"});
  BARVariable cryptType               = new BARVariable(new String[]{"","symmetric","asymmetric"});
  BARVariable incrementalListFileName = new BARVariable("");
  BARVariable storageType             = new BARVariable(new String[]{"filesystem","ftp","scp","sftp","dvd","device"});
  BARVariable storageFileName         = new BARVariable("");
  BARVariable storageLoginName        = new BARVariable("");
  BARVariable storageHostName         = new BARVariable("");
  BARVariable storageDeviceName       = new BARVariable("");
  BARVariable overwriteArchiveFiles   = new BARVariable(false);
  BARVariable sshPublicKeyFileName    = new BARVariable("");
  BARVariable sshPrivateKeyFileName   = new BARVariable("");
  BARVariable maxBandWidthFlag        = new BARVariable(false);
  BARVariable maxBandWidth            = new BARVariable(0);
  BARVariable volumeSize              = new BARVariable(0);
  BARVariable ecc                     = new BARVariable(false);

  // variables
  private     HashMap<String,Integer>  jobIds           = new HashMap<String,Integer>();
  private     String                   selectedJobName  = null;
  private     int                      selectedJobId    = 0;
  private     HashSet<String>          includedPatterns = new HashSet<String>();
  private     HashSet<String>          excludedPatterns = new HashSet<String>();
  private     LinkedList<ScheduleData> scheduleList     = new LinkedList<ScheduleData>();

  TabJobs(TabFolder parentTabFolder, int accelerator)
  {
    Display     display;
    TabFolder   tabFolder;
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

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    imageDirectory = new Image(display,"images/directory.gif");
    imageDirectoryIncluded = new Image(display,"images/directory.gif");
    imageDirectoryExcluded = new Image(display,"images/directory.gif");

    imageFile = new Image(display,"images/file.gif");
    imageFileIncluded = new Image(display,"images/fileIncluded.gif");
    imageFileExcluded = new Image(display,"images/fileExcluded.gif");

    imageLink = new Image(display,"images/link.gif");
    imageLinkIncluded = new Image(display,"images/linkIncluded.gif");
    imageLinkExcluded = new Image(display,"images/linkExcluded.gif");

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Jobs"+((accelerator != 0)?" ("+Widgets.acceleratorToText(accelerator)+")":""));
    widgetTab.setLayout(new TableLayout(new double[]{0,1,0},
                                        null,
                                        2
                                       )
                       );
    Widgets.layout(widgetTab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);

    // job selector
    composite = Widgets.newComposite(widgetTab,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      label = Widgets.newLabel(composite,"Name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobList = Widgets.newOptionMenu(composite,null);
      Widgets.layout(widgetJobList,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
      widgetJobList.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Combo widget = (Combo)selectionEvent.widget;

          int index = widget.getSelectionIndex();
          if (index >= 0)
          {
            selectedJobName = widgetJobList.getItem(index);
            selectedJobId   = jobIds.get(selectedJobName);
            update();
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,null,"New");
      Widgets.layout(button,0,2,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          jobNew();
          updateJobList();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,null,"Rename");
      Widgets.layout(button,0,3,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          if (selectedJobId > 0)
          {
            jobRename();
            updateJobList();
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = Widgets.newButton(composite,null,"Delete");
      Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          if (selectedJobId > 0)
          {
            jobDelete();
            updateJobList();
          }
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // sub-tabs
    tabFolder = Widgets.newTabFolder(widgetTab);
    Widgets.layout(tabFolder,1,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
    {
      tab = Widgets.addTab(tabFolder,"Files");
      Widgets.layout(tab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
      {
        // file tree
        widgetFileTree = Widgets.newTree(tab,null);
        Widgets.layout(widgetFileTree,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
        SelectionListener filesTreeColumnSelectionListener = new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TreeColumn             treeColumn = (TreeColumn)selectionEvent.widget;
            FileTreeDataComparator fileTreeDataComparator = new FileTreeDataComparator(widgetFileTree);
System.err.println("BARControl.java"+", "+3092+": "+fileTreeDataComparator);

            synchronized(scheduleList)
            {
              Widgets.sortTreeColumn(widgetFileTree,treeColumn,fileTreeDataComparator);
            }
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        };
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Name",    SWT.LEFT, 500,true);
        treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Type",    SWT.LEFT,  50,false);
        treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Size",    SWT.RIGHT,100,false);
        treeColumn.addSelectionListener(filesTreeColumnSelectionListener);
        treeColumn = Widgets.addTreeColumn(widgetFileTree,"Modified",SWT.LEFT, 100,false);
        treeColumn.addSelectionListener(filesTreeColumnSelectionListener);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newButton(composite,null,"*");
          Widgets.layout(button,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                patternDelete(PatternTypes.INCLUDE,fileTreeData.name);
                patternDelete(PatternTypes.EXCLUDE,fileTreeData.name);
                switch (fileTreeData.type)
                {
                  case FILE:      treeItem.setImage(imageFile);      break;
                  case DIRECTORY: treeItem.setImage(imageDirectory); break;
                  case LINK:      treeItem.setImage(imageLink);      break;
                  case SPECIAL:   treeItem.setImage(imageFile);      break;
                  case DEVICE:    treeItem.setImage(imageFile);      break;
                  case SOCKET:    treeItem.setImage(imageFile);      break;
                }
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,null,"+");
          Widgets.layout(button,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                patternNew(PatternTypes.INCLUDE,fileTreeData.name);
                patternDelete(PatternTypes.EXCLUDE,fileTreeData.name);
                switch (fileTreeData.type)
                {
                  case FILE:      treeItem.setImage(imageFileIncluded);      break;
                  case DIRECTORY: treeItem.setImage(imageDirectoryIncluded); break;
                  case LINK:      treeItem.setImage(imageLinkIncluded);      break;
                  case SPECIAL:   treeItem.setImage(imageFileIncluded);      break;
                  case DEVICE:    treeItem.setImage(imageFileIncluded);      break;
                  case SOCKET:    treeItem.setImage(imageFileIncluded);      break;
                }
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,null,"-");
          Widgets.layout(button,0,2,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              for (TreeItem treeItem : widgetFileTree.getSelection())
              {
                FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
                patternDelete(PatternTypes.INCLUDE,fileTreeData.name);
                patternNew(PatternTypes.EXCLUDE,fileTreeData.name);
                switch (fileTreeData.type)
                {
                  case FILE:      treeItem.setImage(imageFileExcluded);      break;
                  case DIRECTORY: treeItem.setImage(imageDirectoryExcluded); break;
                  case LINK:      treeItem.setImage(imageLinkExcluded);      break;
                  case SPECIAL:   treeItem.setImage(imageFileExcluded);      break;
                  case DEVICE:    treeItem.setImage(imageFileExcluded);      break;
                  case SOCKET:    treeItem.setImage(imageFileExcluded);      break;
                }
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }
      }

      tab = Widgets.addTab(tabFolder,"Filters");
      Widgets.layout(tab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
      {
        // included list
        label = Widgets.newLabel(tab,"Included:");
        Widgets.layout(label,0,0,TableLayoutData.NS);
        widgetIncludedPatterns = Widgets.newList(tab,null);
        Widgets.layout(widgetIncludedPatterns,0,1,TableLayoutData.NSWE|TableLayoutData.EXPAND);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                patternNew(PatternTypes.INCLUDE);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                patternDelete(PatternTypes.INCLUDE);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // excluded list
        label = Widgets.newLabel(tab,"Excluded:");
        Widgets.layout(label,2,0,TableLayoutData.NS);
        widgetExcludedPatterns = Widgets.newList(tab,null);
        Widgets.layout(widgetExcludedPatterns,2,1,TableLayoutData.NSWE|TableLayoutData.EXPAND);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                patternNew(PatternTypes.EXCLUDE);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              if (selectedJobId > 0)
              {
                patternDelete(PatternTypes.EXCLUDE);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // options
        label = Widgets.newLabel(tab,"Options:");
        Widgets.layout(label,4,0,TableLayoutData.NS);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,4,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newCheckbox(composite,null,"skip unreadable files");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              boolean checkedFlag = widget.getSelection();

              skipUnreadable.set(checkedFlag);
              BARServer.set(selectedJobId,"skip-unreadable",checkedFlag);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,skipUnreadable));
        }
      }

      tab = Widgets.addTab(tabFolder,"Storage");
      Widgets.layout(tab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
      {
        // part size
        label = Widgets.newLabel(tab,"Part size:");
        Widgets.layout(label,0,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newRadio(composite,null,"unlimited");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

               archivePartSizeFlag.set(false);
               archivePartSize.set(0);
               BARServer.set(selectedJobId,"archive-part-size",0);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
          {
            public void modified(Control control, BARVariable archivePartSizeFlag)
            {
              ((Button)control).setSelection(!archivePartSizeFlag.getBoolean());
              widgetArchivePartSize.setEnabled(!archivePartSizeFlag.getBoolean());
            }
          });

          button = Widgets.newRadio(composite,null,"limit to");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

               archivePartSizeFlag.set(true);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
          {
            public void modified(Control control, BARVariable archivePartSizeFlag)
            {
              ((Button)control).setSelection(archivePartSizeFlag.getBoolean());
              widgetArchivePartSize.setEnabled(archivePartSizeFlag.getBoolean());
            }
          });

          widgetArchivePartSize = Widgets.newCombo(composite,null);
          widgetArchivePartSize.setItems(new String[]{"32M","64M","128M","256M","512M","1G","2G"});
          Widgets.layout(widgetArchivePartSize,0,2,TableLayoutData.W);
          widgetArchivePartSize.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
               Combo widget = (Combo)modifyEvent.widget;
               Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
               try
               {
                 long n = Units.parseByteSize(widget.getText());
                 if (archivePartSize.getLong() == n) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
               }
               catch (NumberFormatException exception)
               {
               }
               widget.setForeground(color);
            }
          });
          widgetArchivePartSize.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
               Combo widget = (Combo)selectionEvent.widget;
               String s = widget.getText();
               try
               {
                 long n = Units.parseByteSize(s);
                 archivePartSize.set(n);
                 BARServer.set(selectedJobId,"archive-part-size",n);
               }
               catch (NumberFormatException exception)
               {
                 Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
               }
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
               Combo widget = (Combo)selectionEvent.widget;
               long n = Units.parseByteSize(widget.getText());
               archivePartSize.set(n);
               BARServer.set(selectedJobId,"archive-part-size",n);
            }
          });
          Widgets.addModifyListener(new WidgetListener(widgetArchivePartSize,archivePartSize)
          {
            public String getString(BARVariable variable)
            {
              return Units.formatByteSize(variable.getLong());
            }
          });
        }

        // compress
        label = Widgets.newLabel(tab,"Compress:");
        Widgets.layout(label,1,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          combo = Widgets.newOptionMenu(composite,null);
          combo.setItems(new String[]{"none","zip0","zip1","zip2","zip3","zip4","zip5","zip6","zip7","zip8","zip9","bzip1","bzip2","bzip3","bzip4","bzip5","bzip6","bzip7","bzip8","bzip9"});
          Widgets.layout(combo,0,0,TableLayoutData.W);
          combo.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
               Combo widget = (Combo)selectionEvent.widget;
               String s = widget.getText();
               compressAlgorithm.set(s);
               BARServer.set(selectedJobId,"compress-algorithm",s);
            }
          });
          Widgets.addModifyListener(new WidgetListener(combo,compressAlgorithm));
        }

        // crypt
        label = Widgets.newLabel(tab,"Crypt:");
        Widgets.layout(label,2,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,2,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          combo = Widgets.newOptionMenu(composite,null);
          combo.setItems(new String[]{"none","3DES","CAST5","BLOWFISH","AES128","AES192","AES256","TWOFISH128","TWOFISH256"});
          Widgets.layout(combo,0,0,TableLayoutData.W);
          combo.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
               Combo widget = (Combo)selectionEvent.widget;
               String s = widget.getText();
               cryptAlgorithm.set(s);
               BARServer.set(selectedJobId,"crypt-algorithm",s);
            }
          });
          Widgets.addModifyListener(new WidgetListener(combo,cryptAlgorithm));

          button = Widgets.newRadio(composite,null,"symmetric");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

               cryptType.set("symmetric");
               BARServer.set(selectedJobId,"crypt-type","symmetric");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,cryptType)
          {
            public void modified(Control control, BARVariable cryptType)
            {
              ((Button)control).setSelection(cryptType.equals("symmetric"));
            }
          });

          button = Widgets.newRadio(composite,null,"asymmetric");
          Widgets.layout(button,0,2,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

               cryptType.set("asymmetric");
               BARServer.set(selectedJobId,"crypt-type","asymmetric");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,cryptType)
          {
            public void modified(Control control, BARVariable cryptType)
            {
              ((Button)control).setSelection(cryptType.equals("asymmetric"));
            }
          });
        }

        // archive type
        label = Widgets.newLabel(tab,"Mode:");
        Widgets.layout(label,3,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newRadio(composite,null,"normal");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

               archiveType.set("normal");
               BARServer.set(selectedJobId,"archive-type","normal");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archiveType)
          {
            public void modified(Control control, BARVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("normal"));
            }
          });

          button = Widgets.newRadio(composite,null,"full");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

               archiveType.set("full");
               BARServer.set(selectedJobId,"archive-type","full");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archiveType)
          {
            public void modified(Control control, BARVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("full"));
            }
          });

          button = Widgets.newRadio(composite,null,"incremental");
          Widgets.layout(button,0,2,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

               archiveType.set("incremental");
               BARServer.set(selectedJobId,"archive-type","incremental");
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,archiveType)
          {
            public void modified(Control control, BARVariable archiveType)
            {
              ((Button)control).setSelection(archiveType.equals("incremental"));
            }
          });

          text = Widgets.newText(composite,null);
          Widgets.layout(text,0,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
               Text widget = (Text)modifyEvent.widget;
               Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
               try
               {
                 String s = widget.getText();
                 if (incrementalListFileName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
               }
               catch (NumberFormatException exception)
               {
               }
               widget.setForeground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
               Text widget = (Text)selectionEvent.widget;
               String string = widget.getText();
               incrementalListFileName.set(string);
               BARServer.set(selectedJobId,"incremental-list-file",StringParser.escape(string));
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,incrementalListFileName));

          button = Widgets.newButton(composite,null,imageDirectory);
          Widgets.layout(button,0,4,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              String fileName = Dialogs.fileSave(shell,
                                                 "Select incremental file",
                                                 incrementalListFileName.getString(),
                                                 new String[]{"BAR incremental data","*.bid",
                                                              "All files","*",
                                                             }
                                                );
              if (fileName != null)
              {
                incrementalListFileName.set(fileName);
              }
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // file name
        label = Widgets.newLabel(tab,"File name:");
        Widgets.layout(label,4,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,4,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          text = Widgets.newText(composite,null);
          Widgets.layout(text,0,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
               Text widget = (Text)modifyEvent.widget;
               Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
               try
               {
                 String s = widget.getText();
                 if (storageFileName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
               }
               catch (NumberFormatException exception)
               {
               }
               widget.setForeground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
               Text widget = (Text)selectionEvent.widget;
               storageFileName.set(widget.getText());
               BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,storageFileName));

          button = Widgets.newButton(composite,null,imageDirectory);
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
System.err.println("BARControl.java"+", "+4265+": ");

              storageFileNameEdit();
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        // destination
        label = Widgets.newLabel(tab,"Destination:");
        Widgets.layout(label,5,0,TableLayoutData.W);
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,5,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newRadio(composite,null,"File system");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              storageType.set("filesystem");
              BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("filesystem"));
            }
          });

          button = Widgets.newRadio(composite,null,"ftp");
          Widgets.layout(button,0,1,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              storageType.set("ftp");
              BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("ftp"));
            }
          });

          button = Widgets.newRadio(composite,null,"scp");
          Widgets.layout(button,0,2,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              storageType.set("scp");
              BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("scp"));
            }
          });

          button = Widgets.newRadio(composite,null,"sftp");
          Widgets.layout(button,0,3,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              storageType.set("sftp");
              BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("sftp"));
            }
          });

          button = Widgets.newRadio(composite,null,"DVD");
          Widgets.layout(button,0,4,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              storageType.set("dvd");
              BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("dvd"));
            }
          });

          button = Widgets.newRadio(composite,null,"Device");
          Widgets.layout(button,0,5,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;

              storageType.set("device");
              BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,storageType)
          {
            public void modified(Control control, BARVariable storageType)
            {
              ((Button)control).setSelection(storageType.equals("device"));
            }
          });
        }

        // destination file system
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,6,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("filesystem"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          button = Widgets.newCheckbox(composite,null,"overwrite archive files");
          Widgets.layout(button,0,0,TableLayoutData.W);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
              boolean checkedFlag = widget.getSelection();

              overwriteArchiveFiles.set(checkedFlag);
              BARServer.set(selectedJobId,"overwrite-archive-files",checkedFlag);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          Widgets.addModifyListener(new WidgetListener(button,overwriteArchiveFiles));
        }

        // destiniation ftp
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,6,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("ftp"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Server");
          Widgets.layout(label,0,0,TableLayoutData.W);
          composite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(composite,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            label = Widgets.newLabel(composite,"Login:");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(composite,null);
            Widgets.layout(text,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                 Text widget = (Text)modifyEvent.widget;
                 Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
                 try
                 {
                   String s = widget.getText();
                   if (storageLoginName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
                 }
                 catch (NumberFormatException exception)
                 {
                 }
                 widget.setForeground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                 Text widget = (Text)selectionEvent.widget;
                 storageLoginName.set(widget.getText());
                 BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageLoginName));

            label = Widgets.newLabel(composite,"Host:");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(composite,null);
            Widgets.layout(text,0,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                 Text widget = (Text)modifyEvent.widget;
                 Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
                 try
                 {
                   String s = widget.getText();
                   if (storageHostName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
                 }
                 catch (NumberFormatException exception)
                 {
                 }
                 widget.setForeground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                 Text widget = (Text)selectionEvent.widget;
                 storageHostName.set(widget.getText());
                 BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageHostName));
          }

/*
          label = Widgets.newLabel(composite,"Max. band width:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          composite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(composite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            button = Widgets.newRadio(composite,null,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;

                 maxBandWidthFlag.set(false);
                 maxBandWidth.set(0);
                 BARServer.set(selectedJobId,"max-band-width",0);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, BARVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            button = Widgets.newRadio(composite,null,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;

                 archivePartSizeFlag.set(true);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, BARVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(maxBandWidthFlag.getBoolean());
                widgetFTPMaxBandWidth.setEnabled(maxBandWidthFlag.getBoolean());
              }
            });

            widgetFTPMaxBandWidth = Widgets.newCombo(composite,null);
            widgetFTPMaxBandWidth.setItems(new String[]{"32K","64K","128K","256K","512K"});
            Widgets.layout(widgetFTPMaxBandWidth,0,2,TableLayoutData.W);
          }
*/
        }

        // destination scp/sftp
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,6,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("scp") || variable.equals("sftp"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Server");
          Widgets.layout(label,0,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            label = Widgets.newLabel(subComposite,"Login:");
            Widgets.layout(label,0,0,TableLayoutData.W);

            text = Widgets.newText(subComposite,null);
            Widgets.layout(text,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                 Text widget = (Text)modifyEvent.widget;
                 Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
                 try
                 {
                   String s = widget.getText();
                   if (storageLoginName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
                 }
                 catch (NumberFormatException exception)
                 {
                 }
                 widget.setForeground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                 Text widget = (Text)selectionEvent.widget;
                 storageLoginName.set(widget.getText());
                 BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageLoginName));

            label = Widgets.newLabel(subComposite,"Host:");
            Widgets.layout(label,0,2,TableLayoutData.W);

            text = Widgets.newText(subComposite,null);
            Widgets.layout(text,0,3,TableLayoutData.WE|TableLayoutData.EXPAND_X);
            text.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                 Text widget = (Text)modifyEvent.widget;
                 Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
                 try
                 {
                   String s = widget.getText();
                   if (storageHostName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
                 }
                 catch (NumberFormatException exception)
                 {
                 }
                 widget.setForeground(color);
              }
            });
            text.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                 Text widget = (Text)selectionEvent.widget;
                 storageHostName.set(widget.getText());
                 BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
  throw new Error("NYI");
              }
            });
            Widgets.addModifyListener(new WidgetListener(text,storageHostName));
          }

          label = Widgets.newLabel(composite,"SSH public key:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          text = Widgets.newText(composite,null);
          Widgets.layout(text,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
               Text widget = (Text)modifyEvent.widget;
               Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
               try
               {
                 String s = widget.getText();
                 if (sshPublicKeyFileName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
               }
               catch (NumberFormatException exception)
               {
               }
               widget.setForeground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
               Text widget = (Text)selectionEvent.widget;
               String string = widget.getText();
               sshPublicKeyFileName.set(string);
               BARServer.set(selectedJobId,"ssh-public-key",StringParser.escape(string));
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,sshPublicKeyFileName));

          label = Widgets.newLabel(composite,"SSH private key:");
          Widgets.layout(label,2,0,TableLayoutData.W);
          text = Widgets.newText(composite,null);
          Widgets.layout(text,2,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
               Text widget = (Text)modifyEvent.widget;
               Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
               try
               {
                 String s = widget.getText();
                 if (sshPrivateKeyFileName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
               }
               catch (NumberFormatException exception)
               {
               }
               widget.setForeground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
               Text widget = (Text)selectionEvent.widget;
               String string = widget.getText();
               sshPrivateKeyFileName.set(string);
               BARServer.set(selectedJobId,"ssh-private-key",StringParser.escape(string));
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,sshPrivateKeyFileName));

/*
          label = Widgets.newLabel(composite,"Max. band width:");
          Widgets.layout(label,3,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            button = Widgets.newRadio(subComposite,null,"unlimited");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;

                 maxBandWidthFlag.set(false);
                 maxBandWidth.set(0);
                 BARServer.set(selectedJobId,"max-band-width",0);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, BARVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetSCPSFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            button = Widgets.newRadio(subComposite,null,"limit to");
            Widgets.layout(button,0,1,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;

                 maxBandWidthFlag.set(false);
                 maxBandWidth.set(0);
                 BARServer.set(selectedJobId,"max-band-width",0);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,archivePartSizeFlag)
            {
              public void modified(Control control, BARVariable archivePartSizeFlag)
              {
                ((Button)control).setSelection(!maxBandWidthFlag.getBoolean());
                widgetSCPSFTPMaxBandWidth.setEnabled(!maxBandWidthFlag.getBoolean());
              }
            });

            widgetSCPSFTPMaxBandWidth = Widgets.newCombo(subComposite,null);
            widgetSCPSFTPMaxBandWidth.setItems(new String[]{"32K","64K","128K","256K","512K"});
            Widgets.layout(widgetSCPSFTPMaxBandWidth,0,2,TableLayoutData.W);
          }
*/
        }

        // destination dvd
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,6,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("dvd"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Device:");
          Widgets.layout(label,0,0,TableLayoutData.W);
          text = Widgets.newText(composite,null);
          Widgets.layout(text,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
               Text widget = (Text)modifyEvent.widget;
               Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
               try
               {
                 String s = widget.getText();
                 if (storageDeviceName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
               }
               catch (NumberFormatException exception)
               {
               }
               widget.setForeground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
               Text widget = (Text)selectionEvent.widget;
               storageDeviceName.set(widget.getText());
               BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,storageDeviceName));

          label = Widgets.newLabel(composite,"Size:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            combo = Widgets.newCombo(subComposite,null);
            combo.setItems(new String[]{"2G","3G","3.6G","4G"});
            Widgets.layout(combo,0,0,TableLayoutData.W);
            combo.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                 Combo widget = (Combo)modifyEvent.widget;
                 Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
                 try
                 {
                   long n = Units.parseByteSize(widget.getText());
                   if (volumeSize.getLong() == n) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
                 }
                 catch (NumberFormatException exception)
                 {
                 }
                 widget.setForeground(color);
              }
            });
            combo.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                 Combo widget = (Combo)selectionEvent.widget;
                 String s = widget.getText();
                 try
                 {
                   long n = Units.parseByteSize(s);
                   volumeSize.set(n);
                   BARServer.set(selectedJobId,"volume-size",n);
                 }
                 catch (NumberFormatException exception)
                 {
                   Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
                 }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                 Combo widget = (Combo)selectionEvent.widget;
                 long n = Units.parseByteSize(widget.getText());
                 volumeSize.set(n);
                 BARServer.set(selectedJobId,"volume-size",n);
              }
            });
            Widgets.addModifyListener(new WidgetListener(combo,volumeSize)
            {
              public String getString(BARVariable variable)
              {
                return Units.formatByteSize(variable.getLong());
              }
            });

            label = Widgets.newLabel(subComposite,"bytes");
            Widgets.layout(label,0,1,TableLayoutData.W);
          }

          label = Widgets.newLabel(composite,"Options:");
          Widgets.layout(label,3,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,3,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            button = Widgets.newCheckbox(subComposite,null,"add error-correction codes");
            Widgets.layout(button,0,0,TableLayoutData.W);
            button.addSelectionListener(new SelectionListener()
            {
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                Button widget = (Button)selectionEvent.widget;
                boolean checkedFlag = widget.getSelection();

                ecc.set(checkedFlag);
                BARServer.set(selectedJobId,"ecc",checkedFlag);
              }
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
              }
            });
            Widgets.addModifyListener(new WidgetListener(button,ecc));
          }
        }

        // destination device 
        composite = Widgets.newComposite(tab,SWT.BORDER);
        Widgets.layout(composite,6,1,TableLayoutData.WE|TableLayoutData.N|TableLayoutData.EXPAND_X);
        Widgets.addModifyListener(new WidgetListener(composite,storageType)
        {
          public void modified(Control control, BARVariable variable)
          {
            Widgets.setVisible(control,variable.equals("device"));
          }
        });
        Widgets.setVisible(composite,false);
        {
          label = Widgets.newLabel(composite,"Device:");
          Widgets.layout(label,0,0,TableLayoutData.W);
          text = Widgets.newText(composite,null);
          Widgets.layout(text,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          text.addModifyListener(new ModifyListener()
          {
            public void modifyText(ModifyEvent modifyEvent)
            {
               Text widget = (Text)modifyEvent.widget;
               Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
               try
               {
                 String s = widget.getText();
                 if (storageDeviceName.getString().equals(s)) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
               }
               catch (NumberFormatException exception)
               {
               }
               widget.setForeground(color);
            }
          });
          text.addSelectionListener(new SelectionListener()
          {
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
               Text widget = (Text)selectionEvent.widget;
               storageDeviceName.set(widget.getText());
               BARServer.set(selectedJobId,"archive-name",StringParser.escape(getArchiveName()));
            }
            public void widgetSelected(SelectionEvent selectionEvent)
            {
throw new Error("NYI");
            }
          });
          Widgets.addModifyListener(new WidgetListener(text,storageDeviceName));

          label = Widgets.newLabel(composite,"Size:");
          Widgets.layout(label,1,0,TableLayoutData.W);
          subComposite = Widgets.newComposite(composite,SWT.NONE);
          Widgets.layout(subComposite,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
          {
            combo = Widgets.newCombo(subComposite,null);
            combo.setItems(new String[]{"2G","3G","3.6G","4G"});
            Widgets.layout(combo,0,0,TableLayoutData.W);
            combo.addModifyListener(new ModifyListener()
            {
              public void modifyText(ModifyEvent modifyEvent)
              {
                 Combo widget = (Combo)modifyEvent.widget;
                 Color color = shell.getDisplay().getSystemColor(SWT.COLOR_RED);
                 try
                 {
                   long n = Units.parseByteSize(widget.getText());
                   if (volumeSize.getLong() == n) color = shell.getDisplay().getSystemColor(SWT.COLOR_BLACK);
                 }
                 catch (NumberFormatException exception)
                 {
                 }
                 widget.setForeground(color);
              }
            });
            combo.addSelectionListener(new SelectionListener()
            {
              public void widgetDefaultSelected(SelectionEvent selectionEvent)
              {
                 Combo widget = (Combo)selectionEvent.widget;
                 String s = widget.getText();
                 try
                 {
                   long n = Units.parseByteSize(s);
                   volumeSize.set(n);
                   BARServer.set(selectedJobId,"volume-size",n);
                 }
                 catch (NumberFormatException exception)
                 {
                   Dialogs.error(shell,"'"+s+"' is not valid size!\n\nEnter a number or a number with unit KB, MB or GB.");
                 }
              }
              public void widgetSelected(SelectionEvent selectionEvent)
              {
                 Combo widget = (Combo)selectionEvent.widget;
                 long n = Units.parseByteSize(widget.getText());
                 volumeSize.set(n);
                 BARServer.set(selectedJobId,"volume-size",n);
              }
            });
            Widgets.addModifyListener(new WidgetListener(combo,volumeSize)
            {
              public String getString(BARVariable variable)
              {
                return Units.formatByteSize(variable.getLong());
              }
            });

            label = Widgets.newLabel(subComposite,"bytes");
            Widgets.layout(label,0,1,TableLayoutData.W);
          }
        }
      }

      tab = Widgets.addTab(tabFolder,"Schedule");
      Widgets.layout(tab,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
      {
        // list
        widgetScheduleList = Widgets.newTable(tab,this);
        Widgets.layout(widgetScheduleList,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
        widgetScheduleList.addListener(SWT.MouseDoubleClick,new Listener()
        {
          public void handleEvent(final Event event)
          {
            scheduleEdit();
          }
        });
        SelectionListener scheduleListColumnSelectionListener = new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn            tableColumn = (TableColumn)selectionEvent.widget;
            ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleList);

            synchronized(scheduleList)
            {
              Widgets.sortTableColumn(widgetScheduleList,tableColumn,scheduleDataComparator);
            }
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        };
        tableColumn = Widgets.addTableColumn(widgetScheduleList,0,"Date",     SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,1,"Week day", SWT.LEFT,100,true );
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,2,"Time",     SWT.LEFT,100,false);
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);
        tableColumn = Widgets.addTableColumn(widgetScheduleList,3,"Type",     SWT.LEFT,  0,true );
        tableColumn.addSelectionListener(scheduleListColumnSelectionListener);

        // buttons
        composite = Widgets.newComposite(tab,SWT.NONE);
        Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
        {
          button = Widgets.newButton(composite,null,"Add");
          Widgets.layout(button,0,0,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
System.err.println("BARControl.java"+", "+3133+": ");

              scheduleNew();
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          button = Widgets.newButton(composite,null,"Edit");
          Widgets.layout(button,0,1,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
System.err.println("BARControl.java"+", "+3148+": ");

              scheduleEdit();
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
          button = Widgets.newButton(composite,null,"Rem");
          Widgets.layout(button,0,2,TableLayoutData.DEFAULT,0,0,60,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Button widget = (Button)selectionEvent.widget;
System.err.println("BARControl.java"+", "+3159+": ");

              scheduleDelete();
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }
      }
    }

    // add root devices
    addRootDevices();

    // update data
    updateJobList();
selectJob("x2");
  }

  /** select job
   * @param name job name
   */
  void selectJob(String name)
  {
    synchronized(widgetJobList)
    {
      int index = 0;
      while (   (index < widgetJobList.getItemCount())
             && !name.equals(widgetJobList.getItem(index))
            )
      {
        index++;
      }
      if (index < widgetJobList.getItemCount())
      {
        selectedJobName = name;
        selectedJobId   = jobIds.get(name);
        widgetJobList.select(index);
        update();
      }
    }
  }

  //-----------------------------------------------------------------------

  void setTabStatus(TabStatus tabStatus)
  {
    this.tabStatus = tabStatus;
  }

  /** add root devices
   */
  private void addRootDevices()
  {
  
    TreeItem treeItem = Widgets.addTreeItem(widgetFileTree,new FileTreeData("/",FileTypes.DIRECTORY,"/"),true);
    treeItem.setText("/");
    treeItem.setImage(imageDirectory);
    widgetFileTree.addListener(SWT.Expand,new Listener()
    {
      public void handleEvent(final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        updateFileList(treeItem);
      }
    });
    widgetFileTree.addListener(SWT.Collapse,new Listener()
    {
      public void handleEvent(final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        treeItem.removeAll();
        new TreeItem(treeItem,SWT.NONE);
      }
    });
    widgetFileTree.addListener(SWT.MouseDoubleClick,new Listener()
    {
      public void handleEvent(final Event event)
      {
        TreeItem treeItem = widgetFileTree.getItem(new Point(event.x,event.y));
        if (treeItem != null)
        {
          Event treeEvent = new Event();
          treeEvent.item = treeItem;
          if (treeItem.getExpanded())
          {
            widgetFileTree.notifyListeners(SWT.Collapse,treeEvent);
            treeItem.setExpanded(false);
          }
          else
          {
            widgetFileTree.notifyListeners(SWT.Expand,treeEvent);
            treeItem.setExpanded(true);
          }
        }
      }
    });
  }

  private String getArchiveName()
  {
    if      (storageType.equals("ftp"))
      return String.format("ftp:%s@%s:%s",storageLoginName,storageHostName,storageFileName);
    else if (storageType.equals("scp"))
      return String.format("scp:%s@%s:%s",storageLoginName,storageHostName,storageFileName);
    else if (storageType.equals("sftp"))
      return String.format("sftp:%s@%s:%s",storageLoginName,storageHostName,storageFileName);
    else if (storageType.equals("dvd"))
      return String.format("dvd:%s:%s",storageDeviceName,storageFileName);
    else if (storageType.equals("device"))
      return String.format("device:%s:%s",storageDeviceName,storageFileName);
    else
      return storageFileName.toString();
  }

  private void parseArchiveName(String archiveName)
  {
    Object[] data = new Object[3];

    if      (StringParser.parse(archiveName,"ftp:%s@%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("ftp");
      storageLoginName.set ((String)data[0]);
      storageHostName.set  ((String)data[1]);
      storageDeviceName.set("");
      storageFileName.set  ((String)data[2]);
    }
    else if (StringParser.parse(archiveName,"scp:%s@%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("scp");
      storageLoginName.set ((String)data[0]);
      storageHostName.set  ((String)data[1]);
      storageDeviceName.set("");
      storageFileName.set  ((String)data[2]);
    }
    else if (StringParser.parse(archiveName,"sftp:%s@%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("sftp");
      storageLoginName.set ((String)data[0]);
      storageHostName.set  ((String)data[1]);
      storageDeviceName.set("");
      storageFileName.set  ((String)data[2]);
    }
    else if (StringParser.parse(archiveName,"dvd:%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("dvd");
      storageLoginName.set ("");
      storageHostName.set  ("");
      storageDeviceName.set((String)data[0]);
      storageFileName.set  ((String)data[1]);
    }
    else if (StringParser.parse(archiveName,"dvd:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("dvd");
      storageFileName.set  ("");
      storageLoginName.set ("");
      storageDeviceName.set("");
      storageFileName.set  ((String)data[0]);
    }
    else if (StringParser.parse(archiveName,"device:%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("device");
      storageLoginName.set ("");
      storageHostName.set  ("");
      storageDeviceName.set((String)data[0]);
      storageFileName.set  ((String)data[1]);
    }
    else
    {
      storageType.set      ("filesystem");
      storageLoginName.set ("");
      storageHostName.set  ("");
      storageDeviceName.set("");
      storageFileName.set  (archiveName);
    }
  }

  //-----------------------------------------------------------------------

  /** find index for insert of tree item in sort list of tree items
   * @param treeItem tree item
   * @param name name of tree item to insert
   * @param data data of tree item to insert
   * @return index in tree item
   */
  private int findFilesTreeIndex(TreeItem treeItem, FileTreeData fileTreeData)
  {
    TreeItem               subTreeItems[] = treeItem.getItems();
    FileTreeDataComparator fileTreeDataComparator = new FileTreeDataComparator(widgetFileTree);

    int index = 0;
    while (   (index < subTreeItems.length)
           && (fileTreeDataComparator.compare(fileTreeData,(FileTreeData)subTreeItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update file list of tree item
   * @param treeItem tree item to update
   */
  private void updateFileList(TreeItem treeItem)
  {
    FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
    TreeItem     subTreeItem;

    ArrayList<String> result = new ArrayList<String>();
    BARServer.executeCommand("FILE_LIST "+StringParser.escape(fileTreeData.name),result);

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

        fileTreeData = new FileTreeData(name,FileTypes.FILE,size,datetime,new File(name).getName());

        Image image;
        if      (includedPatterns.contains(name))
          image = imageFileIncluded;
        else if (excludedPatterns.contains(name))
          image = imageFileExcluded;
        else
          image = imageFile;

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"FILE");
        subTreeItem.setText(2,Long.toString(size));
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(datetime*1000)));
        subTreeItem.setImage(image);
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

        fileTreeData = new FileTreeData(name,FileTypes.DIRECTORY,new File(name).getName());

        Image image;
        if      (includedPatterns.contains(name))
          image = imageDirectoryIncluded;
        else if (excludedPatterns.contains(name))
          image = imageDirectoryExcluded;
        else
          image = imageDirectory;

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,true);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"DIR");
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(datetime*1000)));
        subTreeItem.setImage(image);
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

        fileTreeData = new FileTreeData(name,FileTypes.LINK,0,datetime,new File(name).getName());

        Image image;
        if      (includedPatterns.contains(name))
          image = imageLinkIncluded;
        else if (excludedPatterns.contains(name))
          image = imageLinkExcluded;
        else
          image = imageLink;


        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"LINK");
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(datetime*1000)));
        subTreeItem.setImage(image);
      }
      else if (StringParser.parse(line,"SPECIAL %ld %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             date/time
             name
        */
        long   datetime = (Long  )data[0];
        String name     = (String)data[1];

        fileTreeData = new FileTreeData(name,FileTypes.SPECIAL,0,datetime,name);

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"SPECIAL");
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(datetime*1000)));
      }
      else if (StringParser.parse(line,"DEVICE %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             name
        */
        String name = (String)data[0];

        fileTreeData = new FileTreeData(name,FileTypes.DEVICE,name);

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"DEVICE");
      }
      else if (StringParser.parse(line,"SOCKET %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data
           format:
             name
        */
        String name = (String)data[0];

        fileTreeData = new FileTreeData(name,FileTypes.SOCKET,name);

        subTreeItem = Widgets.addTreeItem(treeItem,findFilesTreeIndex(treeItem,fileTreeData),fileTreeData,false);
        subTreeItem.setText(0,fileTreeData.title);
        subTreeItem.setText(1,"SOCKET");
      }
    }
  }  

  //-----------------------------------------------------------------------

  /** find index for insert of job in sort list job list
   * @param jobs jobs
   * @param name name to insert
   * @return index in list
   */
  private int findJobListIndex(String name)
  {
    String names[] = widgetJobList.getItems();

    int index = 0;
    while (   (index < names.length)
           && (names[index].compareTo(name) < 0)
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
    // get job list
    ArrayList<String> result = new ArrayList<String>();
    BARServer.executeCommand("JOB_LIST",result);

    // update job list
    synchronized(widgetJobList)
    {
      jobIds.clear();
      widgetJobList.removeAll();
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
          // get data
          int    id   = (Integer)data[0];
          String name = (String )data[1];

          int index = findJobListIndex(name);
          widgetJobList.add(name,index);
          jobIds.put(name,id);
        }
      }
    }
  }

  //-----------------------------------------------------------------------

  /** add a new job
   */
  private void jobNew()
  {
    Composite composite;
    Label     label;
    Button    button;

    final Shell  dialog = Dialogs.open(shell,"New job",300,70);

    // create widgets
    final Text   widgetJobName;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND);
    {
      label = Widgets.newLabel(composite,"Name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetJobName = Widgets.newText(composite,null);
      Widgets.layout(widgetJobName,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      widgetAdd = Widgets.newButton(composite,null,"Add");
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          widget.getShell().close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetJobName.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetAdd.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        String jobName = widgetJobName.getText();
        if (!jobName.equals(""))
        {
          BARServer.executeCommand("JOB_NEW "+StringParser.escape(jobName));
        }
        widget.getShell().close();
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    Dialogs.run(dialog);
  }

  /** rename selected job
   */
  private void jobRename()
  {
    Composite composite;
    Label     label;
    Button    button;

    assert selectedJobName != null;
    assert selectedJobId != 0;

    final Shell  dialog = Dialogs.open(shell,"Rename job",300,70);

    // create widgets
    final Text   widgetNewJobName;
    final Button widgetRename;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND);
    {
      label = Widgets.newLabel(composite,"Old name:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      label = Widgets.newLabel(composite,selectedJobName);
      Widgets.layout(label,0,1,TableLayoutData.W);

      label = Widgets.newLabel(composite,"New name:");
      Widgets.layout(label,1,0,TableLayoutData.W);

      widgetNewJobName = Widgets.newText(composite,null);
      Widgets.layout(widgetNewJobName,1,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      widgetRename = Widgets.newButton(composite,null,"Rename");
      Widgets.layout(widgetRename,0,0,TableLayoutData.W);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          widget.getShell().close();
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetNewJobName.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetRename.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetRename.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;
        String newJobName = widgetNewJobName.getText();
System.err.println("BARControl.java"+", "+3905+": "+newJobName);
        if (!newJobName.equals(""))
        {
          BARServer.executeCommand("JOB_RENAME "+selectedJobId+" "+StringParser.escape(newJobName));
        }
        widget.getShell().close();
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    Dialogs.run(dialog);
  }

  /** delete selected job
   */
  private void jobDelete()
  {
    assert selectedJobName != null;
    assert selectedJobId != 0;

    if (Dialogs.confirm(shell,"Delete job","Delete job '"+selectedJobName+"'?"))
    {
      BARServer.executeCommand("JOB_DELETE "+selectedJobId);
    }
  }

  //-----------------------------------------------------------------------

  /** find index for insert of name in sort list job list
   * @param list list
   * @param pattern pattern to insert
   * @return index in list
   */
  private int findPatternsIndex(List list, String pattern)
  {
    String patterns[] = list.getItems();

    int index = 0;
    while (   (index < patterns.length)
           && (pattern.compareTo(patterns[index]) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update pattern list
   * @param patternType pattern type
   */
  private void updatePatternList(PatternTypes patternType)
  {
    assert selectedJobId != 0;

    ArrayList<String> result = new ArrayList<String>();
    switch (patternType)
    {
      case INCLUDE:
        BARServer.executeCommand("INCLUDE_PATTERNS_LIST "+selectedJobId,result);
        break;
      case EXCLUDE:
        BARServer.executeCommand("EXCLUDE_PATTERNS_LIST "+selectedJobId,result);
        break;
    }

    switch (patternType)
    {
      case INCLUDE:
        includedPatterns.clear();
        widgetIncludedPatterns.removeAll();
        break;
      case EXCLUDE:
        excludedPatterns.clear();
        widgetExcludedPatterns.removeAll();
        break;
    }

    for (String line : result)
    {
      Object[] data = new Object[2];
      if (StringParser.parse(line,"%s %S",data,StringParser.QUOTE_CHARS))
      {
        // get data
        String type    = (String)data[0];
        String pattern = (String)data[1];

        if (!pattern.equals(""))
        {
          switch (patternType)
          {
            case INCLUDE:
              includedPatterns.add(pattern);
              widgetIncludedPatterns.add(pattern,findPatternsIndex(widgetIncludedPatterns,pattern));
              break;
            case EXCLUDE:
              excludedPatterns.add(pattern);
              widgetExcludedPatterns.add(pattern,findPatternsIndex(widgetExcludedPatterns,pattern));
              break;
          }
        }
      }
    }
  }

  /** add new include/exclude pattern
   * @param patternType pattern type
   */
  private boolean patternEdit(final PatternTypes patternType, final String pattern[], String title, String buttonText)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert selectedJobId != 0;

    // create dialog
    final Shell  dialog = Dialogs.open(shell,title,300,70);

    // create widgets
    final Text   widgetPattern;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.WE|TableLayoutData.EXPAND);
    {
      label = Widgets.newLabel(composite,"Pattern:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetPattern = Widgets.newText(composite,null);
      Widgets.layout(widgetPattern,0,1,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      widgetAdd = Widgets.newButton(composite,null,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          Dialogs.close(dialog,false);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetPattern.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetAdd.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;

        pattern[0] = widgetPattern.getText();
        Dialogs.close(dialog,true);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    return (Boolean)Dialogs.run(dialog);
  }

  /** add new include/exclude pattern
   * @param patternType pattern type
   * @param pattern pattern to add to included/exclude list
   */
  private void patternNew(PatternTypes patternType, String pattern)
  {
    assert selectedJobId != 0;

    switch (patternType)
    {
      case INCLUDE:
        {
          BARServer.executeCommand("INCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(pattern));

          includedPatterns.add(pattern);
          widgetIncludedPatterns.add(pattern,findPatternsIndex(widgetIncludedPatterns,pattern));
        }
        break;
      case EXCLUDE:
        {
          BARServer.executeCommand("EXCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(pattern));

          excludedPatterns.add(pattern);
          widgetExcludedPatterns.add(pattern,findPatternsIndex(widgetIncludedPatterns,pattern));
        }
        break;
    }
  }

  /** add new include/exclude pattern
   * @param patternType pattern type
   * @param pattern pattern to add to included/exclude list
   */
  private void patternNew(PatternTypes patternType)
  {
    assert selectedJobId != 0;

    String title = null;
    switch (patternType)
    {
      case INCLUDE:
        title = "New include pattern";
        break;
      case EXCLUDE:
        title = "New exclude pattern";
        break;
    }
    String[] pattern = new String[1];
    if (patternEdit(patternType,pattern,title,"Add"))
    {
      patternNew(patternType,pattern[0]);
    }
  }

  /** delete include/exclude pattern
   * @param patternType pattern type
   * @param pattern pattern to remove from include/exclude list
   */
  private void patternDelete(PatternTypes patternType, String pattern)
  {
    assert selectedJobId != 0;

    switch (patternType)
    {
      case INCLUDE:
        {
          includedPatterns.remove(pattern);

          BARServer.executeCommand("INCLUDE_PATTERNS_CLEAR "+selectedJobId);
          widgetIncludedPatterns.removeAll();
          for (String s : includedPatterns)
          {
            BARServer.executeCommand("INCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(s));
            widgetIncludedPatterns.add(s,findPatternsIndex(widgetIncludedPatterns,s));
          }
        }
        break;
      case EXCLUDE:
        {
          excludedPatterns.remove(pattern);

          BARServer.executeCommand("EXCLUDE_PATTERNS_CLEAR "+selectedJobId);
          widgetExcludedPatterns.removeAll();
          for (String s : excludedPatterns)
          {
            BARServer.executeCommand("EXCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(s));
            widgetExcludedPatterns.add(s,findPatternsIndex(widgetExcludedPatterns,s));
          }
        }
        break;
    }
  }

  /** delete selected include/exclude pattern
   * @param patternType pattern type
   */
  private void patternDelete(PatternTypes patternType)
  {
    assert selectedJobId != 0;

    int index;
    String pattern = null;
    switch (patternType)
    {
      case INCLUDE:
        index = widgetIncludedPatterns.getSelectionIndex();
        if (index >= 0) pattern = widgetIncludedPatterns.getItem(index);
        break;
      case EXCLUDE:
        index = widgetExcludedPatterns.getSelectionIndex();
        if (index >= 0) pattern = widgetExcludedPatterns.getItem(index);
        break;
    }

    if (pattern != null)
    {
      patternDelete(patternType,pattern);
    }
  }

  //-----------------------------------------------------------------------

  /** storage name part data
   */
  class StorageNamePart implements Serializable
  {
    String    string;
    Rectangle bounds;

    /** create name part
     * @param string string or null
     */
    StorageNamePart(String string)
    {
      this.string = string;
      this.bounds = new Rectangle(0,0,0,0);
    }

    /** write storage name part object to object stream
     * Note: must be implented because Java serializaion API cannot write
     *       inner classes without writing outer classes, too!
     * @param out stream
     */
    private void writeObject(java.io.ObjectOutputStream out)
      throws IOException
    {
      out.writeObject(string);
      out.writeObject(bounds);
    }

    /** read storage name part object from object stream
     * Note: must be implented because Java serializaion API cannot read
     *       inner classes without reading outer classes, too!
     * @param in stream
     * @return 
     */
    private void readObject(java.io.ObjectInputStream in)
      throws IOException, ClassNotFoundException
    {
      string = (String)in.readObject();
      bounds = (Rectangle)in.readObject();
    }

    public String toString()
    {
      return "Part {string="+string+", "+bounds+"}";
    }
  }

  /** storage name part transfer class (required for drag&drop)
   */
  static class StorageNamePartTransfer extends ByteArrayTransfer
  {
    private static final String NAME = "StorageNamePart";
    private static final int    ID   = registerType(NAME);

    private static StorageNamePartTransfer instance = new StorageNamePartTransfer();

    public static StorageNamePartTransfer getInstance()
    {
      return instance;
    }

    public void javaToNative(Object object, TransferData transferData)
    {
      if (!validate(object) || !isSupportedType(transferData))
      {
        DND.error(DND.ERROR_INVALID_DATA);
      }

      StorageNamePart storageNamePart = (StorageNamePart)object;
      try
      {
        // write data to a byte array and then ask super to convert to pMedium
        ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
        ObjectOutputStream outputStream = new ObjectOutputStream(byteArrayOutputStream);
        outputStream.writeObject(storageNamePart);
        byte[] buffer = byteArrayOutputStream.toByteArray();
        outputStream.close();

        // call super to convert to pMedium
        super.javaToNative(buffer,transferData);
      }
      catch (IOException exception)
      {
        // do nothing
      }
   }

   public Object nativeToJava (TransferData transferData)
   {
     if (isSupportedType(transferData))
     {
       byte[] buffer = (byte[])super.nativeToJava(transferData);
       if (buffer == null) return null;

       StorageNamePart storageNamePart = null;
       try
       {
         ByteArrayInputStream byteArrayInputStream = new ByteArrayInputStream (buffer);
         ObjectInputStream inputStream = new ObjectInputStream(byteArrayInputStream);
         storageNamePart = (StorageNamePart)inputStream.readObject();
         inputStream.close ();
       }
       catch (java.lang.ClassNotFoundException exception)
       {
         return null;
       }
       catch (IOException exception)
       {
         return null;
       }

       return storageNamePart;
     }

     return null;
    }

    protected String[] getTypeNames()
    {
      return new String[]{NAME};
    }

    protected int[] getTypeIds()
    {
      return new int[]{ID};
    }

    protected boolean validate(Object object)
    {
      return (object != null && (object instanceof StorageNamePart));
    }
  }

  /** storage name editor
   */
  class StorageFileNameEditor
  {
    // global variables
    final Display display;

    // colors
    final Color   textForegroundColor;
    final Color   textBackgroundColor;
    final Color   textHighlightColor;
    final Color   separatorForegroundColor;
    final Color   separatorBackgroundColor;
    final Color   separatorHighlightColor;

    // widgets
    final Canvas  widgetFileName;
    final Label   widgetExample;
    final Text    widgetText;

    // variables
    LinkedList<StorageNamePart> storageNamePartList = new LinkedList<StorageNamePart>();
    StorageNamePart             selectedNamePart    = null;
    StorageNamePart             highlightedNamePart = null;

    /** create name part editor
     * @param parentComposite parent composite
     */
    StorageFileNameEditor(Composite parentComposite, String fileName)
    {
      Composite  composite;
      Label      label;
      DragSource dragSource;
      DropTarget dropTarget;

      display = parentComposite.getDisplay();

      textForegroundColor      = display.getSystemColor(SWT.COLOR_BLACK);
      textBackgroundColor      = display.getSystemColor(SWT.COLOR_GRAY);
      textHighlightColor       = new Color(null,0xFA,0x0A,0x0A);
      separatorForegroundColor = textForegroundColor;
      separatorBackgroundColor = new Color(null,0xAD,0xD8,0xE6);
      separatorHighlightColor  = textHighlightColor;

      composite = Widgets.newComposite(parentComposite,SWT.NONE);
      Widgets.layout(composite,0,0,TableLayoutData.WE);
      {
        label = Widgets.newLabel(composite,"File name:");
        Widgets.layout(label,0,0,TableLayoutData.WE);

        widgetFileName = Widgets.newCanvas(composite,SWT.BORDER);
        widgetFileName.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_WHITE));
        Widgets.layout(widgetFileName,0,1,TableLayoutData.WE|TableLayoutData.EXPAND,0,0,SWT.DEFAULT,24);
        widgetFileName.addMouseTrackListener(new MouseTrackListener()
        {
          public void mouseEnter(MouseEvent mouseEvent)
          {
          }
          public void mouseExit(MouseEvent mouseEvent)
          {
            clearHighlight();
          }
          public void mouseHover(MouseEvent mouseEvent)
          {
          }
        });
        // Note: needed, because MouseTrackListener.hover() has a delay
        widgetFileName.addMouseMoveListener(new MouseMoveListener()
        {
          public void mouseMove(MouseEvent mouseEvent)
          {
            Point point = new Point(mouseEvent.x,mouseEvent.y);
            setHighlight(point);
          }
        });
        widgetFileName.addKeyListener(new KeyListener()
        {
          public void keyPressed(KeyEvent keyEvent)
          {
//System.err.println("BARControl.java"+", "+6268+": "+selectedNamePart+" "+highlightedNamePart);
            if ((highlightedNamePart != null) && (highlightedNamePart.string != null) && ((keyEvent.keyCode == SWT.DEL) || (keyEvent.keyCode == SWT.BS)))
            {
              remPart(highlightedNamePart);
            }
          }
          public void keyReleased(KeyEvent keyEvent)
          {
          }
        });
        dragSource = new DragSource(widgetFileName,DND.DROP_MOVE);
        dragSource.setTransfer(new Transfer[]{StorageNamePartTransfer.getInstance()});
        dragSource.addDragListener(new DragSourceListener()
        {
          public void dragStart(DragSourceEvent dragSourceEvent)
          {
            Point point = new Point(dragSourceEvent.x,dragSourceEvent.y);
            StorageNamePart storageNamePart = getPart(point);
            if ((storageNamePart != null) && (storageNamePart.string != null))
            {
              selectedNamePart = storageNamePart;
            }
            else
            {
              dragSourceEvent.doit = false;
            }
          }
          public void dragSetData(DragSourceEvent dragSourceEvent)
          {
            dragSourceEvent.data = selectedNamePart;
          }
          public void dragFinished(DragSourceEvent dragSourceEvent)
          {
//System.err.println("BARControl.java"+", "+6261+": "+dragSourceEvent.data+" "+dragSourceEvent.doit+" "+dragSourceEvent.detail+" "+selectedNamePart);
            if (dragSourceEvent.detail == DND.DROP_MOVE)
            {
              remPart(selectedNamePart);
            }
            selectedNamePart = null;
            widgetFileName.redraw();
          }
        });
        dropTarget = new DropTarget(widgetFileName,DND.DROP_MOVE|DND.DROP_COPY);
        dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance(),StorageNamePartTransfer.getInstance()});
	dropTarget.addDropListener(new DropTargetAdapter()
        {
          public void dragLeave(DropTargetEvent dropTargetEvent)
          {
            clearHighlight();
          }
          public void dragOver(DropTargetEvent dropTargetEvent)
          {
            Point point = display.map(shell,widgetFileName,dropTargetEvent.x,dropTargetEvent.y);
            setHighlight(point);
          }
          public void drop(DropTargetEvent dropTargetEvent)
          {
            if (dropTargetEvent.data != null)
            {
//System.err.println("BARControl.java"+", "+6128+": "+dropTargetEvent+" "+dropTargetEvent.item+" "+dropTargetEvent.x+" "+dropTargetEvent.y);
              Point point = display.map(shell,widgetFileName,dropTargetEvent.x,dropTargetEvent.y);
              synchronized(storageNamePartList)
              {
                // find part to replace
                int index = 0;
                while ((index < storageNamePartList.size()) && !storageNamePartList.get(index).bounds.contains(point))
                {
                  index++;
                }

                // replace/insert part
                addPart(index,(String)dropTargetEvent.data);
              }
            }
            else
            {
              dropTargetEvent.detail = DND.DROP_NONE;
            }
          }
	});
        widgetFileName.addPaintListener(new PaintListener()
        {
          public void paintControl(PaintEvent paintEvent)
          {
            redraw(paintEvent);
          }
        }); 

        label = Widgets.newLabel(composite,new Image(display,"images/trashcan.gif"),SWT.BORDER);
        Widgets.layout(label,0,2,TableLayoutData.DEFAULT);
        dropTarget = new DropTarget(label,DND.DROP_MOVE);
        dropTarget.setTransfer(new Transfer[]{TextTransfer.getInstance(),StorageNamePartTransfer.getInstance()});
	dropTarget.addDropListener(new DropTargetAdapter()
        {
          public void dragLeave(DropTargetEvent dropTargetEvent)
          {
          }
          public void dragOver(DropTargetEvent dropTargetEvent)
          {
//System.err.println("BARControl.java"+", "+6353+": "+dropTargetEvent);
          }
          public void drop(DropTargetEvent dropTargetEvent)
          {
//System.err.println("BARControl.java"+", "+6350+": "+dropTargetEvent.data+" "+dropTargetEvent.currentDataType);
            if (dropTargetEvent.data != null)
            {
              if      (dropTargetEvent.data instanceof String)
              {
                // ignored
              }
              else if (dropTargetEvent.data instanceof StorageNamePart)
              {
                // OK
              }
              else
              {
                dropTargetEvent.detail = DND.DROP_NONE;
              }
            }
            else
            {
              dropTargetEvent.detail = DND.DROP_NONE;
            }
          }
	});

        label = Widgets.newLabel(composite,"Example:");
        Widgets.layout(label,1,0,TableLayoutData.W);

        widgetExample = Widgets.newView(composite);
        Widgets.layout(widgetExample,1,1,TableLayoutData.WE,0,2);
      }

      composite = Widgets.newComposite(parentComposite,SWT.NONE);
      Widgets.layout(composite,1,0,TableLayoutData.NSWE);
      {
        // column 1
        addDragAndDrop(composite,"-","text '-'",                          0, 0);
        addDragAndDrop(composite,".bar","text '.bar'",                    1, 0);
        widgetText = Widgets.newText(composite,null);
        addDragAndDrop(composite,"Text",widgetText,                       2, 0);

        addDragAndDrop(composite,"#","part number 1 digit",              4, 0);
        addDragAndDrop(composite,"##","part number 2 digits",            5, 0);
        addDragAndDrop(composite,"###","part number 3 digits",           6, 0);
        addDragAndDrop(composite,"####","part number 4 digits",          7, 0);

        addDragAndDrop(composite,"%type","archive type: full,incremental",9, 0);
        addDragAndDrop(composite,"%last","'-last' if last archive part",  10,0);

        // column 2
        addDragAndDrop(composite,"%d","day 01..31",                  0, 1);
        addDragAndDrop(composite,"%j","day of year 001..366",        1, 1);
        addDragAndDrop(composite,"%m","month 01..12",                2, 1);
        addDragAndDrop(composite,"%b","month name",                  3, 1);
        addDragAndDrop(composite,"%B","full month name",             4, 1);
        addDragAndDrop(composite,"%H","hour 00..23",                 5, 1);
        addDragAndDrop(composite,"%I","hour 00..12",                 6, 1);
        addDragAndDrop(composite,"%M","minute 00..59",               7, 1);
        addDragAndDrop(composite,"%p","'AM' or 'PM'",                8, 1);
        addDragAndDrop(composite,"%P","'am' or 'pm'",                9, 1);
        addDragAndDrop(composite,"%a","week day name",               10,1);
        addDragAndDrop(composite,"%A","full week day name",          11,1);
        addDragAndDrop(composite,"%u","day of week 1..7",            12,1);
        addDragAndDrop(composite,"%w","day of week 0..6",            13,1);
        addDragAndDrop(composite,"%U","week number 1..52",           14,1);
        addDragAndDrop(composite,"%C","century two digits",          15,1);
        addDragAndDrop(composite,"%Y","year four digits",            16,1);
        addDragAndDrop(composite,"%S","seconds since 1.1.1970 00:00",17,1);
        addDragAndDrop(composite,"%Z","time-zone abbreviation",      18,1);

        // column 3
        addDragAndDrop(composite,"%%","%",                           0, 2);
        addDragAndDrop(composite,"%#","#",                           1, 2);
      }

      // set name
      setFileName(fileName);
    }

    /** set file name
     * @param file name
     */
    void setFileName(String fileName)
    {
      synchronized(storageNamePartList)
      {
        // clear existing list
        storageNamePartList.clear();

        // parse file name
        storageNamePartList.add(new StorageNamePart(null));
        int z = 0;
        while (z < fileName.length())
        {
          StringBuffer part;

          // get next text part
          part = new StringBuffer();
          while ((z < fileName.length()) && (fileName.charAt(z) != '%'))
          {
            part.append(fileName.charAt(z)); z++;
          }
          storageNamePartList.add(new StorageNamePart(part.toString()));
          storageNamePartList.add(new StorageNamePart(null));

          if ((z < fileName.length()) && (fileName.charAt(z) == '%'))
          {
            // add next variable part
            part = new StringBuffer();
            part.append('%'); z++;
            if ((z < fileName.length()) && (fileName.charAt(z) == '%'))
            {
              part.append('%'); z++;
            }
            else
            {
              while ((z < fileName.length()) && (Character.isLetterOrDigit(fileName.charAt(z))))
              {
                part.append(fileName.charAt(z)); z++;
              }
            }
            storageNamePartList.add(new StorageNamePart(part.toString()));
            storageNamePartList.add(new StorageNamePart(null));
          }
        }
      }

      // redraw
      widgetFileName.redraw();
      updateExample();
    }

    /** get file name
     * @return file name
     */
    String getFileName()
    {
      StringBuffer fileName = new StringBuffer();
      for (StorageNamePart storageNamePart : storageNamePartList)
      {
        if (storageNamePart.string != null)
        {
          fileName.append(storageNamePart.string);
        }
      }

      return fileName.toString();
    }

    //-----------------------------------------------------------------------

    /** add part
     * @param composite composite to add into
     * @param text text to show
     * @param description of part
     * @param row,column row/column
     */
    private void addDragAndDrop(Composite composite, String text, String description, int row, int column)
    {
      Label label;

      label = Widgets.newLabel(composite,text,SWT.LEFT|SWT.BORDER);
      label.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_GRAY));
      label.setData(text);
      Widgets.layout(label,row,column*2+0,TableLayoutData.W);
      DragSource dragSource = new DragSource(label,DND.DROP_MOVE|DND.DROP_COPY);
      dragSource.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dragSource.addDragListener(new DragSourceListener()
      {
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
        }
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          Control control = ((DragSource)dragSourceEvent.widget).getControl();
          dragSourceEvent.data = (String)control.getData();
        }
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
        }
      });

      label = Widgets.newLabel(composite,description,SWT.LEFT|SWT.BORDER);
      Widgets.layout(label,row,column*2+1,TableLayoutData.W);
    }

    /** add part
     * @param composite composite to add into
     * @param text text to show
     * @param control control to add
     * @param row,column row/column
     */
    private void addDragAndDrop(Composite composite, String text, Control control, int row, int column)
    {
      Label label;

      label = Widgets.newLabel(composite,text,SWT.LEFT|SWT.BORDER);
      label.setBackground(composite.getDisplay().getSystemColor(SWT.COLOR_GRAY));
      label.setData(control);
      Widgets.layout(label,row,column*2+0,TableLayoutData.W);
      DragSource dragSource = new DragSource(label,DND.DROP_MOVE|DND.DROP_COPY);
      dragSource.setTransfer(new Transfer[]{TextTransfer.getInstance()});
      dragSource.addDragListener(new DragSourceListener()
      {
        public void dragStart(DragSourceEvent dragSourceEvent)
        {
          Control control = ((DragSource)dragSourceEvent.widget).getControl();
          Widget widget = (Widget)control.getData();
          if (widget instanceof Text)
          {
            String text = ((Text)widget).getText();
            if ((text == null) || (text.length() == 0)) dragSourceEvent.doit = false;
          }
        }
        public void dragSetData(DragSourceEvent dragSourceEvent)
        {
          Control control = ((DragSource)dragSourceEvent.widget).getControl();
          Widget widget = (Widget)control.getData();
          if (widget instanceof Text)
          {
            dragSourceEvent.data = ((Text)widget).getText();
            if (dragSourceEvent.data.equals("")) dragSourceEvent.doit = false;
          }
        }
        public void dragFinished(DragSourceEvent dragSourceEvent)
        {
        }
      });

      Widgets.layout(control,row,column*2+1,TableLayoutData.WE);
    }

    private void addPart(int index, String string)
    {
      boolean redrawFlag = false;

      synchronized(storageNamePartList)
      {
        if (index < storageNamePartList.size())
        {
          if (storageNamePartList.get(index).string != null)
          {
            // replace
            storageNamePartList.get(index).string = string;
          }
          else
          {
            // insert
            storageNamePartList.add(index+1,new StorageNamePart(string));
            storageNamePartList.add(index+2,new StorageNamePart(null));
          }
          redrawFlag = true;
        }
      }

      if (redrawFlag)
      {
        widgetFileName.redraw();
        updateExample();
      }
    }

    private void remPart(StorageNamePart storageNamePart)
    {
      boolean redrawFlag = false;

      synchronized(storageNamePartList)
      {
        // find part to delete
        int index = 0;
        while ((index < storageNamePartList.size()) && (storageNamePartList.get(index) != storageNamePart))
        {
          index++;
        }

        // delete part and separator
        if (index < storageNamePartList.size())
        {
          storageNamePartList.remove(index);
          if ((index < storageNamePartList.size()) && (storageNamePartList.get(index).string == null))
          {
            storageNamePartList.remove(index);
          }
          redrawFlag = true;
        }
      }

      if (redrawFlag)
      {
        widgetFileName.redraw();
        updateExample();
      }
    }

    /** redraw part widget content
     * @param paintEvent paint event
     */
    private void redraw(PaintEvent paintEvent)
    {
      GC        gc         = paintEvent.gc;
      Rectangle clientArea = widgetFileName.getClientArea();
      Color     color;

      int x = 0;
      synchronized(storageNamePartList)
      {
        for (StorageNamePart storageNamePart : storageNamePartList)
        {
          if (storageNamePart.string != null)
          {
            Point size = Widgets.getTextSize(widgetFileName,storageNamePart.string);
            if   ((storageNamePart == highlightedNamePart) || (storageNamePart == selectedNamePart)) color = textHighlightColor;
            else                                                                                     color = textBackgroundColor;
            gc.setBackground(color);
            gc.setForeground(textForegroundColor);
            gc.drawString(storageNamePart.string,x,0);
            storageNamePart.bounds = new Rectangle(x,0,size.x,clientArea.height);
            x += size.x;
          }
          else
          {
            if      (storageNamePart == highlightedNamePart) color = separatorHighlightColor;
            else                                             color = separatorBackgroundColor;
            gc.setBackground(color);
            gc.fillRectangle(x,0,8,clientArea.height-1);
            gc.setForeground(separatorForegroundColor);
            gc.drawRectangle(x,0,8,clientArea.height-1);
            storageNamePart.bounds = new Rectangle(x,0,8,clientArea.height);
            x += 8+1;
          }
        }
      }
    }

    /** update example line
     * @param 
     * @return 
     */
    private void updateExample()
    {
      StringBuffer exampleName = new StringBuffer();

      synchronized(storageNamePartList)
      {
        for (StorageNamePart storageNamePart : storageNamePartList)
        {
          if (storageNamePart.string != null)
          {
            if      (storageNamePart.string.equals("#"))
              exampleName.append("1");
            else if (storageNamePart.string.equals("##"))
              exampleName.append("12");
            else if (storageNamePart.string.equals("###"))
              exampleName.append("123");
            else if (storageNamePart.string.equals("####"))
              exampleName.append("1234");
            else if (storageNamePart.string.equals("%type"))
              exampleName.append("full");
            else if (storageNamePart.string.equals("%last"))
              exampleName.append("-last");
            else if (storageNamePart.string.equals("%d"))
              exampleName.append("24");
            else if (storageNamePart.string.equals("%j"))
              exampleName.append("354");
            else if (storageNamePart.string.equals("%m"))
              exampleName.append("12");
            else if (storageNamePart.string.equals("%b"))
              exampleName.append("Dec");
            else if (storageNamePart.string.equals("%B"))
              exampleName.append("December");
            else if (storageNamePart.string.equals("%H"))
              exampleName.append("23");
            else if (storageNamePart.string.equals("%I"))
              exampleName.append("11");
            else if (storageNamePart.string.equals("%M"))
              exampleName.append("55");
            else if (storageNamePart.string.equals("%p"))
              exampleName.append("PM");
            else if (storageNamePart.string.equals("%P"))
              exampleName.append("pm");
            else if (storageNamePart.string.equals("%a"))
              exampleName.append("Mon");
            else if (storageNamePart.string.equals("%A"))
              exampleName.append("Monday");
            else if (storageNamePart.string.equals("%u"))
              exampleName.append("1");
            else if (storageNamePart.string.equals("%w"))
              exampleName.append("0");
            else if (storageNamePart.string.equals("%U"))
              exampleName.append("51");
            else if (storageNamePart.string.equals("%C"))
              exampleName.append("07");
            else if (storageNamePart.string.equals("%Y"))
              exampleName.append("2007");
            else if (storageNamePart.string.equals("%S"))
              exampleName.append("1198598100");
            else if (storageNamePart.string.equals("%Z"))
              exampleName.append("JST");
            else if (storageNamePart.string.equals("%%"))
              exampleName.append("%");
            else if (storageNamePart.string.equals("%#"))
              exampleName.append("#");
            else
              exampleName.append(storageNamePart.string);
          }
        }
      }
      widgetExample.setText(exampleName.toString());
    }

  /** find part at location x,y
   * @param point location
   * @return part or null
   */
    private StorageNamePart getPart(Point point)
    {
      synchronized(storageNamePartList)
      {
        for (StorageNamePart storageNamePart : storageNamePartList)
        {
          if (storageNamePart.bounds.contains(point))
          {
            return storageNamePart;
          }
        }
      }

      return null;
    }

    /** clear part highlighting
     */
    private void clearHighlight()
    {
      if (highlightedNamePart != null)
      {
        highlightedNamePart = null;
        widgetFileName.redraw();
      }
    }

    /** set highlighting of part
     * @param point mouse position
     */
    private void setHighlight(Point point)
    {
      boolean redrawFlag = false;

      synchronized(storageNamePartList)
      {
        // find part to highlight
        StorageNamePart storageNamePart = getPart(point);

        // clear previous highlighting
        if ((highlightedNamePart != null) && (storageNamePart != highlightedNamePart))
        {
          highlightedNamePart = null;
          redrawFlag = true;
        }

        // highlight part
        if (storageNamePart != null)
        {
          highlightedNamePart = storageNamePart;
          redrawFlag = true;
        }
      }

      if (redrawFlag) widgetFileName.redraw();
    }
  };

  /** edit storage file name
   */
//  private void storageFileNameEdit()
void storageFileNameEdit()
  {
    Composite composite;
    Label     label;
    Button    button;
    Composite subComposite;

    assert selectedJobId != 0;

    // create dialog
    final Shell dialog = Dialogs.open(shell,
                                      "Edit storage file name",
                                      SWT.DEFAULT,SWT.DEFAULT,
                                      new double[]{0,1,0},null
                                     );

    // create widgets
    final StorageFileNameEditor storageFileNameEditor;
    final Button                widgetSave;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    storageFileNameEditor = new StorageFileNameEditor(composite,storageFileName.getString());

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,2,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      widgetSave = Widgets.newButton(composite,null,"Save");
      Widgets.layout(widgetSave,0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          Dialogs.close(dialog,false);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
    widgetSave.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;

        storageFileName.set(storageFileNameEditor.getFileName());
        Dialogs.close(dialog,true);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    Dialogs.run(dialog);
  }

  //-----------------------------------------------------------------------

  /** find index for insert of schedule data in sorted schedule table
   * @param scheduleData schedule data
   * @return index in schedule table
   */
  private int findScheduleListIndex(ScheduleData scheduleData)
  {
    TableItem              tableItems[] = widgetScheduleList.getItems();
    ScheduleDataComparator scheduleDataComparator = new ScheduleDataComparator(widgetScheduleList);

    int index = 0;
    while (   (index < tableItems.length)
           && (scheduleDataComparator.compare(scheduleData,(ScheduleData)tableItems[index].getData()) > 0)
          )
    {
      index++;
    }

    return index;
  }

  /** update schedule list
   */
  private void updateScheduleList()
  {
    // get schedule list
    ArrayList<String> result = new ArrayList<String>();
    BARServer.executeCommand("SCHEDULE_LIST "+selectedJobId,result);

    // update schedule list
    synchronized(scheduleList)
    {
      scheduleList.clear();
      widgetScheduleList.removeAll();
      for (String line : result)
      {
        Object data[] = new Object[7];
        /* format:
           <date>
           <weekDay>
           <time>
           <type>
        */
//System.err.println("BARControl.java"+", "+1357+": "+line);
        if (StringParser.parse(line,"%S-%S-%S %S %S:%S %S",data,StringParser.QUOTE_CHARS))
        {
//System.err.println("BARControl.java"+", "+747+": "+data[0]+"--"+data[5]+"--"+data[6]);
          // get data
          String year    = (String)data[0];
          String month   = (String)data[1];
          String day     = (String)data[2];
          String weekDay = (String)data[3];
          String hour    = (String)data[4];
          String minute  = (String)data[5];
          String type    = (String)data[6];

          ScheduleData scheduleData = new ScheduleData(year,month,day,weekDay,hour,minute,type);

          scheduleList.add(scheduleData);
          TableItem tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
          tableItem.setData(scheduleData);
          tableItem.setText(0,String.format("%s-%s-%s",scheduleData.year,scheduleData.month,scheduleData.day));
          tableItem.setText(1,scheduleData.weekDay);
          tableItem.setText(2,String.format("%s:%s",scheduleData.hour,scheduleData.minute));
          tableItem.setText(3,scheduleData.type);
        }
      }
    }
  }

  /** edit schedule data
   * @param scheduleData schedule data
   * @param title title text
   * @param buttonText button text
   * @return true if edit OK, false otherwise
   */
  private boolean scheduleEdit(final ScheduleData scheduleData, String title, String buttonText)
  {
    Composite composite;
    Label     label;
    Button    button;
    Composite subComposite;

    assert selectedJobId != 0;

    // create dialog
    final Shell dialog = Dialogs.open(shell,title,300,70);

    // create widgets
    final Combo  widgetYear,widgetMonth,widgetDay,widgetWeekDay;
    final Combo  widgetHour,widgetMinute;
    final Button widgetTypeDefault,widgetTypeNormal,widgetTypeFull,widgetTypeIncremental;
    final Button widgetAdd;
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite,"Date:");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetYear = Widgets.newOptionMenu(subComposite,null);
        widgetYear.setItems(new String[]{"*","2008","2009","2010","2011","2012","2013","2014","2015"});
        widgetYear.setText(scheduleData.year);
        Widgets.layout(widgetYear,0,0,TableLayoutData.W);

        widgetMonth = Widgets.newOptionMenu(subComposite,null);
        widgetMonth.setItems(new String[]{"*","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"});
        widgetMonth.setText(scheduleData.month);
        Widgets.layout(widgetMonth,0,1,TableLayoutData.W);

        widgetDay = Widgets.newOptionMenu(subComposite,null);
        widgetDay.setItems(new String[]{"*","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23","24","25","26","27","28","29","30","31"});
        widgetDay.setText(scheduleData.day);
        Widgets.layout(widgetDay,0,2,TableLayoutData.W);

        widgetWeekDay = Widgets.newOptionMenu(subComposite,null);
        widgetWeekDay.setItems(new String[]{"*","Mon","Tue","Wed","Thu","Fri","Sat","Sun"});
        widgetWeekDay.setText(scheduleData.weekDay);
        Widgets.layout(widgetWeekDay,0,3,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,"Time:");
      Widgets.layout(label,1,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,1,1,TableLayoutData.WE);
      {
        widgetHour = Widgets.newOptionMenu(subComposite,null);
        widgetHour.setItems(new String[]{"*","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23"});
        widgetHour.setText(scheduleData.hour);
        Widgets.layout(widgetHour,0,0,TableLayoutData.W);

        widgetMinute = Widgets.newOptionMenu(subComposite,null);
        widgetMinute.setItems(new String[]{"*","0","5","10","15","20","30","35","40","45","50","55"});
        widgetMinute.setText(scheduleData.minute);
        Widgets.layout(widgetMinute,0,1,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite,"Type:");
      Widgets.layout(label,2,0,TableLayoutData.W);

      subComposite = Widgets.newComposite(composite,SWT.NONE);
      Widgets.layout(subComposite,2,1,TableLayoutData.WE);
      {
        widgetTypeDefault = Widgets.newRadio(subComposite,null,"*");
        Widgets.layout(widgetTypeDefault,0,0,TableLayoutData.W);
        widgetTypeDefault.setSelection(scheduleData.type.equals("*"));

        widgetTypeNormal = Widgets.newRadio(subComposite,null,"normal");
        Widgets.layout(widgetTypeNormal,0,1,TableLayoutData.W);
        widgetTypeNormal.setSelection(scheduleData.type.equals("normal"));

        widgetTypeFull = Widgets.newRadio(subComposite,null,"full");
        Widgets.layout(widgetTypeFull,0,2,TableLayoutData.W);
        widgetTypeFull.setSelection(scheduleData.type.equals("full"));

        widgetTypeIncremental = Widgets.newRadio(subComposite,null,"incremental");
        Widgets.layout(widgetTypeIncremental,0,3,TableLayoutData.W);
        widgetTypeIncremental.setSelection(scheduleData.type.equals("incremental"));
      }
    }

    // buttons
    composite = Widgets.newComposite(dialog,SWT.NONE);
    Widgets.layout(composite,1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X);
    {
      widgetAdd = Widgets.newButton(composite,null,buttonText);
      Widgets.layout(widgetAdd,0,0,TableLayoutData.W|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);

      button = Widgets.newButton(composite,null,"Cancel");
      Widgets.layout(button,0,1,TableLayoutData.E|TableLayoutData.EXPAND_X,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;

          Dialogs.close(dialog,false);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }

    // add selection listeners
/*
    widgetPattern.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetAdd.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
throw new Error("NYI");
      }
    });
*/
    widgetAdd.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Button widget = (Button)selectionEvent.widget;

        scheduleData.year    = widgetYear.getText();
        scheduleData.month   = widgetMonth.getText();
        scheduleData.day     = widgetDay.getText();
        scheduleData.weekDay = widgetWeekDay.getText();
        scheduleData.hour    = widgetHour.getText();
        scheduleData.minute  = widgetMinute.getText();
        if      (widgetTypeNormal.getSelection())      scheduleData.type = "normal";
        else if (widgetTypeFull.getSelection())        scheduleData.type = "full";
        else if (widgetTypeIncremental.getSelection()) scheduleData.type = "incremental";
        else                                           scheduleData.type = "*";

        Dialogs.close(dialog,true);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    return (Boolean)Dialogs.run(dialog);
  }

  /** create new schedule entry
   */
  private void scheduleNew()
  {
    assert selectedJobId != 0;

    ScheduleData scheduleData = new ScheduleData();
    if (scheduleEdit(scheduleData,"New schedule","Add"))
    {
      BARServer.executeCommand("SCHEDULE_ADD "+selectedJobId+" "+scheduleData.getDate()+" "+scheduleData.getWeekDay()+" "+scheduleData.getTime()+" "+scheduleData.getType());

      scheduleList.add(scheduleData);
      TableItem tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
      tableItem.setData(scheduleData);
      tableItem.setText(0,scheduleData.getDate()   );
      tableItem.setText(1,scheduleData.getWeekDay());
      tableItem.setText(2,scheduleData.getTime()   );
      tableItem.setText(3,scheduleData.getType()   );   
    }
  }

  /** edit schedule entry
   */
  private void scheduleEdit()
  {
    assert selectedJobId != 0;

    int index = widgetScheduleList.getSelectionIndex();
    if (index >= 0)
    {
      TableItem tableItem = widgetScheduleList.getItem(index);

      ScheduleData scheduleData = (ScheduleData)tableItem.getData();
      if (scheduleEdit(scheduleData,"Edit schedule","Save"))
      {
        scheduleList.add(scheduleData);

        BARServer.executeCommand("SCHEDULE_CLEAR "+selectedJobId);
        for (ScheduleData data : scheduleList)
        {
          BARServer.executeCommand("SCHEDULE_ADD "+selectedJobId+" "+data.getDate()+" "+data.getWeekDay()+" "+data.getTime()+" "+data.getType());
        }
        
        tableItem.dispose();
        tableItem = new TableItem(widgetScheduleList,SWT.NONE,findScheduleListIndex(scheduleData));
        tableItem.setData(scheduleData);
        tableItem.setText(0,scheduleData.getDate()   );
        tableItem.setText(1,scheduleData.getWeekDay());
        tableItem.setText(2,scheduleData.getTime()   );
        tableItem.setText(3,scheduleData.getType()   );   
      }
    }
  }

  /** delete schedule entry
   */
  private void scheduleDelete()
  {
    assert selectedJobId != 0;

    int index = widgetScheduleList.getSelectionIndex();
    if (index >= 0)
    {
      TableItem tableItem = widgetScheduleList.getItem(index);

      ScheduleData scheduleData = (ScheduleData)tableItem.getData();

      scheduleList.remove(scheduleData);

      BARServer.executeCommand("SCHEDULE_CLEAR "+selectedJobId);
      for (ScheduleData data : scheduleList)
      {
        BARServer.executeCommand("SCHEDULE_ADD "+selectedJobId+" "+data.getDate()+" "+data.getWeekDay()+" "+data.getTime()+" "+data.getType());
      }

      tableItem.dispose();
    }
  }

  //-----------------------------------------------------------------------

  private void clearJobData()
  {
// NYI: rest?
    widgetIncludedPatterns.removeAll();
    widgetExcludedPatterns.removeAll();
  }

  /** update job data
   * @param name name of job
   */
  private void updateJobData()
  {
    ArrayList<String> result = new ArrayList<String>();
    Object[]          data;

    // clear
    clearJobData();

    if (selectedJobId > 0)
    {
      // get job data
      skipUnreadable.set(BARServer.getBoolean(selectedJobId,"skip-unreadable"));
      overwriteFiles.set(BARServer.getBoolean(selectedJobId,"overwrite-files"));

      parseArchiveName(BARServer.getString(selectedJobId,"archive-name"));
      archiveType.set(BARServer.getString(selectedJobId,"archive-type"));
      archivePartSize.set(Units.parseByteSize(BARServer.getString(selectedJobId,"archive-part-size")));
      archivePartSizeFlag.set(archivePartSize.getLong() > 0);
      compressAlgorithm.set(BARServer.getString(selectedJobId,"compress-algorithm"));
      cryptAlgorithm.set(BARServer.getString(selectedJobId,"crypt-algorithm"));
      cryptType.set(BARServer.getString(selectedJobId,"crypt-type"));
      incrementalListFileName.set(BARServer.getString(selectedJobId,"incremental-list-file"));
      overwriteArchiveFiles.set(BARServer.getBoolean(selectedJobId,"overwrite-archive-files"));
      sshPublicKeyFileName.set(BARServer.getString(selectedJobId,"ssh-public-key"));
      sshPrivateKeyFileName.set(BARServer.getString(selectedJobId,"ssh-private-key"));
/* NYI ???
      maxBandWidth.set(Units.parseByteSize(BARServer.getString(jobId,"max-band-width")));
      maxBandWidthFlag.set(maxBandWidth.getLong() > 0);
*/
      volumeSize.set(Units.parseByteSize(BARServer.getString(selectedJobId,"volume-size")));
      ecc.set(BARServer.getBoolean(selectedJobId,"ecc"));

      updatePatternList(PatternTypes.INCLUDE);
      updatePatternList(PatternTypes.EXCLUDE);
    }
  }

  /** update all data
   */
  private void update()
  {
    updateJobData();
    updateScheduleList();
  }
}

public class BARControl
{
  // --------------------------- constants --------------------------------

  final static String DEFAULT_HOSTNAME = "localhost";
  final static int    DEFAULT_PORT     = 38523;
  final static int    DEFAULT_TLS_PORT = 38524;

  // --------------------------- variables --------------------------------
  private String    hostname = DEFAULT_HOSTNAME;
  private int       port     = DEFAULT_PORT;
  private int       tlsPort  = DEFAULT_TLS_PORT;

  private Display   display;
  private Shell     shell;
  private TabFolder tabFolder;
  private TabStatus tabStatus;
  private TabJobs   tabJobs;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create main window
   */
  private void createWindow()
  {
    display = new Display();
    shell = new Shell(display);
    shell.setLayout(new TableLayout());
  }

  /** create tabs
   */
  private void createTabs()
  {
    // create resizable tab (with help of sashForm)
    SashForm sashForm = new SashForm(shell,SWT.NONE);
    sashForm.setLayout(new TableLayout());
    Widgets.layout(sashForm,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
    tabFolder = new TabFolder(sashForm,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));
    tabStatus = new TabStatus(tabFolder,SWT.F1);
    tabJobs   = new TabJobs  (tabFolder,SWT.F2);
    tabStatus.setTabJobs(tabJobs);
    tabJobs.setTabStatus(tabStatus);

    // add tab listener
    display.addFilter(SWT.KeyDown,new Listener()
    {
      public void handleEvent(Event event)
      {
        switch (event.keyCode)
        {
          case SWT.F1:
        System.out.println(event);
            Widgets.showTab(tabFolder,tabStatus.widgetTab);
            event.doit = false;
            break;
          case SWT.F2:
            Widgets.showTab(tabFolder,tabJobs.widgetTab);
            event.doit = false;
            break;
          default:
            break;
        }
      }
    });
  }

  /** create menu
   */
  private void createMenu()
  {
    Menu     menuBar;
    Menu     menu;
    MenuItem menuItem;

    // create menu
    menuBar = Widgets.newMenuBar(shell);
    menu = Widgets.addMenu(menuBar,"Program");
    menuItem = Widgets.addMenuItem(menu,"Start",SWT.CTRL+'S');
    menuItem.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        MenuItem widget = (MenuItem)selectionEvent.widget;

        Widgets.notify(tabStatus.widgetButtonStart);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    menuItem = Widgets.addMenuItem(menu,"Abort",SWT.CTRL+'A');
    menuItem.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        MenuItem widget = (MenuItem)selectionEvent.widget;

        Widgets.notify(tabStatus.widgetButtonAbort);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    menuItem = Widgets.addMenuItem(menu,"Toggle pause/continue",SWT.CTRL+'P');
    menuItem.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        MenuItem widget = (MenuItem)selectionEvent.widget;

        Widgets.notify(tabStatus.widgetButtonTogglePause);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    Widgets.addMenuSeparator(menu);
    menuItem = Widgets.addMenuItem(menu,"Quit",SWT.CTRL+'Q');
    menuItem.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        MenuItem widget = (MenuItem)selectionEvent.widget;

        Widgets.notify(tabStatus.widgetButtonQuit);
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    menu = Widgets.addMenu(menuBar,"Help");
    menuItem = Widgets.addMenuItem(menu,"About");
    menuItem.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        MenuItem widget = (MenuItem)selectionEvent.widget;

        Dialogs.info(shell,"About","BAR control.\n\nWritten by Torsten Rupp.\n\nThanx to Matthias Albert.");
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
  }

  /** run application
   */
  private void run()
  {
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


  BARControl(String[] args)
  {
    // connect to server
String password = "y7G7EGj2";
    BARServer.connect(hostname,port,tlsPort,password);

    // open main window
    createWindow();
    createTabs();
    createMenu();
//tabJobs.storageFileNameEdit();
//System.exit(0);

    // run
    run();

    // disconnect
    BARServer.disconnect();
  }

  public static void main(String[] args)
  {
    BARControl barControl = new BARControl(args);
  }
}

/* end of file */
