/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/BARControl.java,v $
* $Revision: 1.7 $
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
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.VerifyEvent;
import org.eclipse.swt.events.VerifyListener;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.PaletteData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.FontMetrics;
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
          /* check if completed */
          if (Integer.parseInt(data[1]) != 0)
          {
            errorCode = Integer.parseInt(data[2]);
            if (errorCode != 0) throw new Error("communication error: "+errorCode+" "+data[3]);
            completedFlag = true;
          }

          if (result != null)
          {
            /* store data */
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
   * @return dialog shell
   */
  static Shell open(Shell parentShell, String title, int minWidth, int minHeight)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;

    final Shell shell = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
    shell.setText(title);
    tableLayout = new TableLayout(new double[]{1,0},null,4);
    tableLayout.minWidth  = minWidth;
    tableLayout.minHeight = minHeight;
    shell.setLayout(tableLayout);

    return shell;
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

  /** run dialog
   * @param dialog dialog shell
   */
  static void run(Shell dialog)
  {
    dialog.pack();
    dialog.open();
    Display display = dialog.getParent().getDisplay();
    while (!dialog.isDisposed())
    {
      if (!display.readAndDispatch()) display.sleep();
    }
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

    PaletteData paletteData = new PaletteData(0xFF0000,0x00FF00,0x0000FF);
    ImageData imageData = new ImageData(Images.error.width,Images.error.height,Images.error.depth,paletteData,1,Images.error.data);
    imageData.alphaData = Images.error.alphas;
    imageData.alpha = -1;
    Image image = new Image(shell.getDisplay(),imageData);

    /* message */
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

    /* buttons */
    composite = new Composite(shell,SWT.NONE);
    composite.setLayout(new TableLayout());
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE|TableLayoutData.EXPAND_X));
    {
      button = new Button(composite,SWT.CENTER);
      button.setText("Close");
      button.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0));
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

    PaletteData paletteData = new PaletteData(0xFF0000,0x00FF00,0x0000FF);
    ImageData imageData = new ImageData(Images.question.width,Images.question.height,Images.question.depth,paletteData,1,Images.question.data);
    imageData.alphaData = Images.question.alphas;
    imageData.alpha = -1;
    Image image = new Image(shell.getDisplay(),imageData);

    /* message */
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

    /* buttons */
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

    PaletteData paletteData = new PaletteData(0xFF0000,0x00FF00,0x0000FF);
    ImageData imageData = new ImageData(Images.question.width,Images.question.height,Images.question.depth,paletteData,1,Images.question.data);
    imageData.alphaData = Images.question.alphas;
    imageData.alpha = -1;
    Image image = new Image(shell.getDisplay(),imageData);

    /* message */
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

    /* buttons */
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

  static void setVisible(Control control, boolean visibleFlag)
  {
    TableLayoutData tableLayoutData = (TableLayoutData)control.getLayoutData();
    tableLayoutData.exclude = !visibleFlag;
    control.setVisible(visibleFlag);
    control.getParent().layout();
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

  final Shell shell;

  // widgets
  Composite   widgetTab;
  Table       widgetJobList;
  Group       widgetSelectedJob;
  Button      widgetButtonStart;
  Button      widgetButtonAbort;
  Button      widgetButtonTogglePause;
  Button      widgetButtonVolume;

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
  private String selectedJobName = null;
  private int    selectedJobId   = 0;

  private States status          = States.RUNNING;

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

  TabStatus(TabFolder parentTabFolder)
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
    widgetTab = Widgets.addTab(parentTabFolder,"Status");
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
        Table widget = (Table)selectionEvent.widget;
        TabStatus tabStatus = (TabStatus)widget.getData();

        int selectedJobId = (Integer)selectionEvent.item.getData();

        synchronized(widgetJobList)
        {
          TableItem tableItems[] = widgetJobList.getItems();
          int index = getTableItemIndex(tableItems,selectedJobId);
          tabStatus.widgetSelectedJob.setText("Selected '"+tableItems[index].getText(1)+"'");

          tabStatus.selectedJobName = tableItems[index].getText(1);
          tabStatus.selectedJobId   = selectedJobId;
        }
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });
    SelectionListener jobListColumnSelectionListener = new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        TableColumn widget = (TableColumn)selectionEvent.widget;
        int n = (Integer)widget.getData();

        synchronized(widgetJobList)
        {
          TableItem[] items = widgetJobList.getItems();

          /* get sorting direction */
          int sortDirection = widgetJobList.getSortDirection();
          if (sortDirection == SWT.NONE) sortDirection = SWT.UP;
          if (widgetJobList.getSortColumn() == widget)
          {
            switch (sortDirection)
            {
              case SWT.UP:   sortDirection = SWT.DOWN; break;
              case SWT.DOWN: sortDirection = SWT.UP;   break;
            }
          }

          /* sort column */
          Collator collator = Collator.getInstance(Locale.getDefault());
          for (int i = 1; i < items.length; i++)
          {
            boolean sortedFlag = false;
            for (int j = 0; (j < i) && !sortedFlag; j++)
            {
              switch (sortDirection)
              {
                case SWT.UP:   sortedFlag = (collator.compare(items[i].getText(n),items[j].getText(n)) < 0); break;
                case SWT.DOWN: sortedFlag = (collator.compare(items[i].getText(n),items[j].getText(n)) > 0); break;
              }
              if (sortedFlag)
              {
                /* save data */
                Object   data = items[i].getData();
                String[] texts = {items[i].getText(0),
                                  items[i].getText(1),
                                  items[i].getText(2),
                                  items[i].getText(3),
                                  items[i].getText(4),
                                  items[i].getText(5),
                                  items[i].getText(6),
                                  items[i].getText(7),
                                  items[i].getText(8)
                                 };

                /* discard item */
                items[i].dispose();

                /* create new item */
                TableItem item = new TableItem(widgetJobList,SWT.NONE,j);
                item.setData(data);
                item.setText(texts);

                items = widgetJobList.getItems();
              }
            }
          }
          widgetJobList.setSortColumn(widget);
          widgetJobList.setSortDirection(sortDirection);
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

      button = Widgets.newButton(composite,null,"Quit");
      Widgets.layout(button,0,4,TableLayoutData.E|TableLayoutData.EXPAND_X);
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

    // start status update thread
if (true) {
    TabStatusUpdateThread tabStatusUpdateThread = new TabStatusUpdateThread(this);
    tabStatusUpdateThread.setDaemon(true);
    tabStatusUpdateThread.start();
}
  }

  /** get index of item in job list
   * @param tableItems table items
   * @param id id to find
   * @return index
   */
  private int getTableItemIndex(TableItem tableItems[], int id)
  {
    for (int z = 0; z < tableItems.length; z++)
    {
      if ((Integer)tableItems[z].getData() == id) return z;
    }

    return -1;
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
      synchronized(widgetJobList)
      {
        TableItem tableItems[] = widgetJobList.getItems();
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
  //System.err.println("BARControl.java"+", "+2266+": id="+id+" index="+index+" "+tableItem.getText(1));
            }
            else
            {
              tableItem = new TableItem(widgetJobList,SWT.NONE);
            }

            /* init table item */
            tableItem.setData(id);
            tableItem.setText(0,Integer.toString(id));
            tableItem.setText(1,name);
            tableItem.setText(2,(status != States.PAUSE)?state:"pause");
            tableItem.setText(3,type);
            tableItem.setText(4,Units.formatByteSize(archivePartSize));
            tableItem.setText(5,compressAlgorithm);
            tableItem.setText(6,cryptAlgorithm+(cryptType.equals("ASYMMETRIC")?"*":""));
            tableItem.setText(7,DateFormat.getDateTimeInstance().format(new Date(lastExecutedDateTime*1000)));
            tableItem.setText(8,String.format("%2d days %02d:%02d:%02d",estimatedRestDays,estimatedRestHours,estimatedRestMinutes,estimatedRestSeconds));
          }
        }
        for (int z = 0; z < tableItems.length; z++)
        {
          if (!tableItemFlags[z]) widgetJobList.remove(z);
        }
      }
    }
  }

  /** 
   * @param 
   * @return 
   */
  private void updateJobInfo()
  {
    if (selectedJobId > 0)
    {
      // get job info
      String result[] = new String[1];
      int errorCode = BARServer.executeCommand(String.format("JOB_INFO %d",selectedJobId),result);
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
    assert selectedJobName != null;
    assert selectedJobId != 0;

    switch (Dialogs.select(shell,"Start job","Start job '"+selectedJobName+"'?",new String[]{"Full","Incremental","Cancel"}))
    {
      case 0:
        BARServer.executeCommand("JOB_START "+selectedJobId+" full");
        break;
      case 1:
        BARServer.executeCommand("JOB_START "+selectedJobId+" incremental");
        break;
      case 2:
        break;
    }
  }

  /** abort selected job
   */
  private void jobAbort()
  {
    assert selectedJobName != null;
    assert selectedJobId != 0;

    BARServer.executeCommand("JOB_ABORT "+selectedJobId);
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
    FileTypes type;
    String    name;

    FileTreeData(FileTypes type, String name)
    {
      this.type = type;
      this.name = name;
    }
  };

  final Shell shell;

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
  Tree        widgetFilesTree;
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
  private HashMap<String,Integer> jobIds           = new HashMap<String,Integer>();
  private String                  selectedJobName  = null;
  private int                     selectedJobId    = 0;
  private HashSet<String>         includedPatterns = new HashSet<String>();
  private HashSet<String>         excludedPatterns = new HashSet<String>();

  TabJobs(TabFolder parentTabFolder)
  {
    Display     display;
    ImageData   imageData;
    TabFolder   tabFolder;
    Composite   tab;
    Group       group;
    Composite   composite,subComposite;
    Label       label;
    Button      button;
    Combo       combo;
    TreeItem    treeItem;
    Text        text;
    TableColumn tableColumn;

    // get shell, display
    shell = parentTabFolder.getShell();
    display = shell.getDisplay();

    /* create images */
    PaletteData paletteData = new PaletteData(0xFF0000,0x00FF00,0x0000FF);

    imageData = new ImageData(Images.directory.width,Images.directory.height,Images.directory.depth,paletteData,1,Images.directory.data);
    imageData.alphaData = Images.directory.alphas;
    imageData.alpha = -1;
    imageDirectory = new Image(display,imageData);
    imageData = new ImageData(Images.directoryIncluded.width,Images.directoryIncluded.height,Images.directoryIncluded.depth,paletteData,1,Images.directoryIncluded.data);
    imageData.alphaData = Images.directoryIncluded.alphas;
    imageData.alpha = -1;
    imageDirectoryIncluded = new Image(display,imageData);
    imageData = new ImageData(Images.directoryExcluded.width,Images.directoryExcluded.height,Images.directoryExcluded.depth,paletteData,1,Images.directoryExcluded.data);
    imageData.alphaData = Images.directoryExcluded.alphas;
    imageData.alpha = -1;
    imageDirectoryExcluded = new Image(display,imageData);

    imageData = new ImageData(Images.file.width,Images.file.height,Images.file.depth,paletteData,1,Images.file.data);
    imageData.alphaData = Images.file.alphas;
    imageData.alpha = -1;
    imageFile = new Image(display,imageData);
    imageData = new ImageData(Images.fileIncluded.width,Images.fileIncluded.height,Images.fileIncluded.depth,paletteData,1,Images.fileIncluded.data);
    imageData.alphaData = Images.fileIncluded.alphas;
    imageData.alpha = -1;
    imageFileIncluded = new Image(display,imageData);
    imageData = new ImageData(Images.fileExcluded.width,Images.fileExcluded.height,Images.fileExcluded.depth,paletteData,1,Images.fileExcluded.data);
    imageData.alphaData = Images.fileExcluded.alphas;
    imageData.alpha = -1;
    imageFileExcluded = new Image(display,imageData);

    imageData = new ImageData(Images.link.width,Images.link.height,Images.link.depth,paletteData,1,Images.link.data);
    imageData.alphaData = Images.link.alphas;
    imageData.alpha = -1;
    imageLink = new Image(display,imageData);
    imageData = new ImageData(Images.linkIncluded.width,Images.linkIncluded.height,Images.linkIncluded.depth,paletteData,1,Images.linkIncluded.data);
    imageData.alphaData = Images.linkIncluded.alphas;
    imageData.alpha = -1;
    imageLinkIncluded = new Image(display,imageData);
    imageData = new ImageData(Images.linkExcluded.width,Images.linkExcluded.height,Images.linkExcluded.depth,paletteData,1,Images.linkExcluded.data);
    imageData.alphaData = Images.linkExcluded.alphas;
    imageData.alpha = -1;
    imageLinkExcluded = new Image(display,imageData);

    // create tab
    widgetTab = Widgets.addTab(parentTabFolder,"Jobs");
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
            updateJobData();
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
        widgetFilesTree = Widgets.newTree(tab,null);
        Widgets.layout(widgetFilesTree,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
        Widgets.addTreeColumn(widgetFilesTree,"Name",    SWT.LEFT, 500,true);
        Widgets.addTreeColumn(widgetFilesTree,"Type",    SWT.LEFT,  50,false);
        Widgets.addTreeColumn(widgetFilesTree,"Size",    SWT.RIGHT,100,false);
        Widgets.addTreeColumn(widgetFilesTree,"Modified",SWT.LEFT, 100,false);

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

              for (TreeItem treeItem : widgetFilesTree.getSelection())
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

              for (TreeItem treeItem : widgetFilesTree.getSelection())
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

              for (TreeItem treeItem : widgetFilesTree.getSelection())
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

//BARServer.debug=true;
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
                updatePatternList(PatternTypes.INCLUDE);
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
                updatePatternList(PatternTypes.INCLUDE);
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
                updatePatternList(PatternTypes.EXCLUDE);
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
                updatePatternList(PatternTypes.EXCLUDE);
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
System.err.println("BARControl.java"+", "+2608+": ");
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
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Button widget = (Button)selectionEvent.widget;
System.err.println("BARControl.java"+", "+3110+": ");
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
        SelectionListener scheduleListColumnSelectionListener = new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            TableColumn widget = (TableColumn)selectionEvent.widget;
            int         n      = (Integer)widget.getData();

            synchronized(widgetScheduleList)
            {
              TableItem[] items = widgetScheduleList.getItems();

              /* get sorting direction */
              int sortDirection = widgetScheduleList.getSortDirection();
              if (sortDirection == SWT.NONE) sortDirection = SWT.UP;
              if (widgetScheduleList.getSortColumn() == widget)
              {
                switch (sortDirection)
                {
                  case SWT.UP:   sortDirection = SWT.DOWN; break;
                  case SWT.DOWN: sortDirection = SWT.UP;   break;
                }
              }

              /* sort column */
              Collator collator = Collator.getInstance(Locale.getDefault());
              for (int i = 1; i < items.length; i++)
              {
                boolean sortedFlag = false;
                for (int j = 0; (j < i) && !sortedFlag; j++)
                {
                  switch (sortDirection)
                  {
                    case SWT.UP:   sortedFlag = (collator.compare(items[i].getText(n),items[j].getText(n)) < 0); break;
                    case SWT.DOWN: sortedFlag = (collator.compare(items[i].getText(n),items[j].getText(n)) > 0); break;
                  }
                  if (sortedFlag)
                  {
                    /* save data */
                    Object   data = items[i].getData();
                    String[] texts = {items[i].getText(0),
                                      items[i].getText(1),
                                      items[i].getText(2),
                                      items[i].getText(3),
                                      items[i].getText(4),
                                      items[i].getText(5),
                                      items[i].getText(6),
                                      items[i].getText(7),
                                      items[i].getText(8)
                                     };

                    /* discard item */
                    items[i].dispose();

                    /* create new item */
                    TableItem item = new TableItem(widgetScheduleList,SWT.NONE,j);
                    item.setData(data);
                    item.setText(texts);

                    items = widgetScheduleList.getItems();
                  }
                }
              }
              widgetScheduleList.setSortColumn(widget);
              widgetScheduleList.setSortDirection(sortDirection);
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
//selectedJobName = "database";
selectedJobName = "x2";
selectedJobId = jobIds.get(selectedJobName);
updateJobData();
    updateScheduleList();    
  }

  /** find index for insert of tree item in sort list of tree items
   * @param treeItem tree item
   * @param name name of tree item to insert
   * @param data data of tree item to insert
   * @return index in tree item
   */
  private int findFilesTreeIndex(TreeItem treeItem, String name, FileTreeData fileTreeData)
  {
    TreeItem subItems[] = treeItem.getItems();

    int index = 0;
    if (fileTreeData.type == FileTypes.DIRECTORY)
    {
      while (   (index < subItems.length)
             && (((FileTreeData)subItems[index].getData()).type == FileTypes.DIRECTORY)
             && (subItems[index].getText().compareTo(name) < 0)
            )
      {
        index++;
      }
    }
    else
    {
      while (   (index < subItems.length)
             && (   (((FileTreeData)subItems[index].getData()).type == FileTypes.DIRECTORY)
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
    FileTreeData fileTreeData = (FileTreeData)treeItem.getData();
    TreeItem     subTreeItem;
    int          index;

    ArrayList<String> result = new ArrayList<String>();
    int errorCode = BARServer.executeCommand("FILE_LIST "+StringParser.escape(fileTreeData.name),result);
    if (errorCode != 0) return;

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

        fileTreeData = new FileTreeData(FileTypes.FILE,name);

        Image image;
        if      (includedPatterns.contains(name))
          image = imageFileIncluded;
        else if (excludedPatterns.contains(name))
          image = imageFileExcluded;
        else
          image = imageFile;

        String title = new File(name).getName();
        index = findFilesTreeIndex(treeItem,title,fileTreeData);
        subTreeItem = Widgets.addTreeItem(treeItem,index,fileTreeData,false);
        subTreeItem.setText(0,title);
        subTreeItem.setText(1,"FILE");
        subTreeItem.setText(2,Long.toString(size));
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(timestamp*1000)));
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
        long   size      = (Long  )data[0];
        long   timestamp = (Long  )data[1];
        String name      = (String)data[2];

        fileTreeData = new FileTreeData(FileTypes.DIRECTORY,name);

        Image image;
        if      (includedPatterns.contains(name))
          image = imageDirectoryIncluded;
        else if (excludedPatterns.contains(name))
          image = imageDirectoryExcluded;
        else
          image = imageDirectory;

        String title = new File(name).getName();
        index = findFilesTreeIndex(treeItem,title,fileTreeData);
        subTreeItem = Widgets.addTreeItem(treeItem,index,fileTreeData,true);
        subTreeItem.setText(0,title);
        subTreeItem.setText(1,"DIR");
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(timestamp*1000)));
        subTreeItem.setImage(image);
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

        fileTreeData = new FileTreeData(FileTypes.LINK,name);

        Image image;
        if      (includedPatterns.contains(name))
          image = imageLinkIncluded;
        else if (excludedPatterns.contains(name))
          image = imageLinkExcluded;
        else
          image = imageLink;


        String title = new File(name).getName();
        index = findFilesTreeIndex(treeItem,title,fileTreeData);
        subTreeItem = Widgets.addTreeItem(treeItem,index,fileTreeData,false);
        subTreeItem.setText(0,title);
        subTreeItem.setText(1,"LINK");
        subTreeItem.setText(3,DateFormat.getDateTimeInstance().format(new Date(timestamp*1000)));
        subTreeItem.setImage(image);
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

        fileTreeData = new FileTreeData(FileTypes.SPECIAL,name);

        index = findFilesTreeIndex(treeItem,name,fileTreeData);
        subTreeItem = Widgets.addTreeItem(treeItem,index,fileTreeData,false);
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

        fileTreeData = new FileTreeData(FileTypes.DEVICE,name);

        index = findFilesTreeIndex(treeItem,name,fileTreeData);
        subTreeItem = Widgets.addTreeItem(treeItem,index,fileTreeData,false);
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

        fileTreeData = new FileTreeData(FileTypes.SOCKET,name);

        index = findFilesTreeIndex(treeItem,name,fileTreeData);
        subTreeItem = Widgets.addTreeItem(treeItem,index,fileTreeData,false);
        subTreeItem.setText(0,name);
        subTreeItem.setText(1,"SOCKET");
      }
    }
  }  

  /** add root devices
   */
  private void addRootDevices()
  {
  
    TreeItem treeItem = Widgets.addTreeItem(widgetFilesTree,new FileTreeData(FileTypes.DIRECTORY,"/"),true);
    treeItem.setText("/");
    treeItem.setImage(imageDirectory);
    widgetFilesTree.addListener(SWT.Expand,new Listener()
    {
      public void handleEvent (final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        updateFileList(treeItem);
        if (treeItem.getItemCount() == 0) new TreeItem(treeItem,SWT.NONE);
      }
    });
    widgetFilesTree.addListener(SWT.Collapse,new Listener()
    {
      public void handleEvent (final Event event)
      {
        final TreeItem treeItem = (TreeItem)event.item;
        treeItem.removeAll();
        new TreeItem(treeItem,SWT.NONE);
      }
    });
    widgetFilesTree.addListener(SWT.MouseDoubleClick,new Listener()
    {
      public void handleEvent(final Event event)
      {
        TreeItem treeItem = widgetFilesTree.getItem(new Point(event.x,event.y));
        if (treeItem != null)
        {
          Event treeEvent = new Event();
          treeEvent.item = treeItem;
          if (treeItem.getExpanded())
          {
            widgetFilesTree.notifyListeners(SWT.Collapse,treeEvent);
            treeItem.setExpanded(false);
          }
          else
          {
            widgetFilesTree.notifyListeners(SWT.Expand,treeEvent);
            treeItem.setExpanded(true);
          }
        }
      }
    });
  }

  /** find index for insert of name in sort list job list
   * @param jobs jobs
   * @param name name to insert
   * @return index in list
   */
  private int findJobsIndex(Combo jobs, String name)
  {
    String names[] = jobs.getItems();

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
    int errorCode = BARServer.executeCommand("JOB_LIST",result);
    if (errorCode != 0) return;

    // update job list
    widgetJobList.removeAll();
    jobIds.clear();
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
        int    id   = (Integer)data[0];
        String name = (String )data[1];

        int index = findJobsIndex(widgetJobList,name);
        widgetJobList.add(name,index);
        jobIds.put(name,id);
      }
    }
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

System.err.println("BARControl.java"+", "+3065+": "+archiveName);
    if      (StringParser.parse(archiveName,"ftp:%s@%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("ftp");
      storageLoginName.set ((String)data[0]);
      storageHostName.set  ((String)data[1]);
      storageDeviceName.set("");
      storageFileName.set  ((String)data[2]);
System.err.println("BARControl.java"+", "+3073+": ");
    }
    else if (StringParser.parse(archiveName,"scp:%s@%s:%s",data,StringParser.QUOTE_CHARS))
    {
System.err.println("BARControl.java"+", "+3118+": "+(String)data[0]+"-"+(String)data[1]+"-"+(String)data[2]);
      storageType.set      ("scp");
      storageLoginName.set ((String)data[0]);
      storageHostName.set  ((String)data[1]);
      storageDeviceName.set("");
      storageFileName.set  ((String)data[2]);
System.err.println("BARControl.java"+", "+3082+": ");
    }
    else if (StringParser.parse(archiveName,"sftp:%s@%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("sftp");
      storageLoginName.set ((String)data[0]);
      storageHostName.set  ((String)data[1]);
      storageDeviceName.set("");
      storageFileName.set  ((String)data[2]);
System.err.println("BARControl.java"+", "+3091+": ");
    }
    else if (StringParser.parse(archiveName,"dvd:%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("dvd");
      storageLoginName.set ("");
      storageHostName.set  ("");
      storageDeviceName.set((String)data[0]);
      storageFileName.set  ((String)data[1]);
System.err.println("BARControl.java"+", "+3100+": ");
    }
    else if (StringParser.parse(archiveName,"dvd:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("dvd");
      storageFileName.set  ("");
      storageLoginName.set ("");
      storageDeviceName.set("");
      storageFileName.set  ((String)data[0]);
System.err.println("BARControl.java"+", "+3109+": ");
    }
    else if (StringParser.parse(archiveName,"device:%s:%s",data,StringParser.QUOTE_CHARS))
    {
      storageType.set      ("device");
      storageLoginName.set ("");
      storageHostName.set  ("");
      storageDeviceName.set((String)data[0]);
      storageFileName.set  ((String)data[1]);
System.err.println("BARControl.java"+", "+3118+": ");
    }
    else
    {
      storageType.set      ("filesystem");
      storageLoginName.set ("");
      storageHostName.set  ("");
      storageDeviceName.set("");
      storageFileName.set  (archiveName);
System.err.println("BARControl.java"+", "+3127+": ");
    }
  }

  private String getStorageFileName(String archiveName)
  {
return archiveName;
  }

  private void clearJobData()
  {
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
System.err.println("BARControl.java"+", "+2594+": "+selectedJobId);
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
           && (patterns[index].compareTo(pattern) < 0)
          )
    {
      index++;
    }

    return index;
  }

  /** 
   * @param 
   * @return 
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
        break;
      case EXCLUDE:
        excludedPatterns.clear();
        break;
    }

    for (String line : result)
    {
      Object[] data = new Object[2];
      if (StringParser.parse(line,"%s %S",data,StringParser.QUOTE_CHARS))
      {
        /* get data */
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

  /** update schedule list
   */
  private void updateScheduleList()
  {
    // get job list
    ArrayList<String> result = new ArrayList<String>();
    int errorCode = BARServer.executeCommand("SCHEDULE_LIST "+selectedJobId,result);
    if (errorCode != 0) return;

    // update job list
    widgetScheduleList.removeAll();
    for (String line : result)
    {
      Object data[] = new Object[4];
      /* format:
         <date>
         <weekDay>
         <time>
         <type>
      */
//System.err.println("BARControl.java"+", "+1357+": "+line);
      if (StringParser.parse(line,"%S %S %S %S",data,StringParser.QUOTE_CHARS))
      {
//System.err.println("BARControl.java"+", "+747+": "+data[0]+"--"+data[5]+"--"+data[6]);
        /* get data */
        String date    = (String)data[0];
        String weekDay = (String)data[1];
        String time    = (String)data[2];
        String type    = (String)data[3];

        if (!date.equals("") && !weekDay.equals("") && !time.equals("") && !type.equals(""))
        {
          /* add table item */
          TableItem tableItem = new TableItem(widgetScheduleList,SWT.NONE);
          tableItem.setText(0,date);
          tableItem.setText(1,weekDay);
          tableItem.setText(2,time);
          tableItem.setText(3,type);
        }
      }
    }
  }

  /** add a new job
   */
  private void jobNew()
  {
    Composite composite;
    Label     label;
    Button    button;

    final Shell  dialog = Dialogs.open(shell,"New job",300,70);

    /* create widgets */
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

    /* add selection listeners */
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

    /* create widgets */
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

    /* add selection listeners */
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

  /** add new include/exclude pattern
   * @param patternType pattern type
   * @param pattern pattern to add to included/exclude list
   */
  private void patternNew(final PatternTypes patternType, String pattern)
  {
    assert selectedJobId != 0;

BARServer.debug=true;
    switch (patternType)
    {
      case INCLUDE:
        {
          BARServer.executeCommand("INCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(pattern));

          includedPatterns.add(pattern);

          widgetIncludedPatterns.removeAll();
          for (String s : includedPatterns)
          {
            widgetIncludedPatterns.add(s,findPatternsIndex(widgetIncludedPatterns,s));
          }
        }
        break;
      case EXCLUDE:
        {
          BARServer.executeCommand("EXCLUDE_PATTERNS_ADD "+selectedJobId+" GLOB "+StringParser.escape(pattern));

          excludedPatterns.add(pattern);

          widgetExcludedPatterns.removeAll();
          for (String s : excludedPatterns)
          {
            widgetExcludedPatterns.add(s,findPatternsIndex(widgetIncludedPatterns,s));
          }
        }
        break;
    }
BARServer.debug=false;
  }

  /** add new include/exclude pattern
   * @param patternType pattern type
   */
  private void patternNew(final PatternTypes patternType)
  {
    Composite composite;
    Label     label;
    Button    button;

    assert selectedJobId != 0;

    /* create dialog */
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
    final Shell  dialog = Dialogs.open(shell,title,300,70);

    /* create widgets */
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

    /* add selection listeners */
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
        String newPattern = widgetPattern.getText();
        if (!newPattern.equals(""))
        {
          patternNew(patternType,newPattern);
        }
        widget.getShell().close();
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
      }
    });

    Dialogs.run(dialog);
  }

  /** delete include/exclude pattern
   * @param patternType pattern type
   * @param pattern pattern to remove from include/exclude list
   */
  private void patternDelete(PatternTypes patternType, String pattern)
  {
    assert selectedJobId != 0;

BARServer.debug=true;
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
BARServer.debug=false;
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
//    BARServer barServer = new BARServer(hostname,port,tlsPort,password);

    BARServer.connect(hostname,port,tlsPort,password);

    // open main window
    Display display = new Display();
//    final Shell shell = new Shell(display);
    Shell shell = new Shell(display);
    shell.setLayout(new TableLayout());

    // create resizable tab (with help of sashForm)
    SashForm sashForm = new SashForm(shell,SWT.NONE);
    sashForm.setLayout(new TableLayout());
    Widgets.layout(sashForm,0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND);
    TabFolder tabFolder = new TabFolder(sashForm,SWT.NONE);
    tabFolder.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE|TableLayoutData.EXPAND));

    TabStatus tabStatus = new TabStatus(tabFolder);
    TabJobs   tabJobs   = new TabJobs  (tabFolder);

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
