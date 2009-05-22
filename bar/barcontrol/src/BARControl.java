/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/BARControl.java,v $
* $Revision: 1.15 $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
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

/** BAR variable types
 */
enum BARVariableTypes
{
  BOOLEAN,
  LONG,
  DOUBLE,
  STRING,
  ENUMERATION,
};

/** BAR variables
 */
class BARVariable
{
  private BARVariableTypes type;
  private boolean          b;
  private long             l;
  private double           d;
  private String           string;
  private String           enumeration[];

  /** create BAR variable
   * @param b/l/d/string/enumeration value
   */
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

  /** get variable type
   * @return type
   */
  BARVariableTypes getType()
  {
    return type;
  }

  /** get boolean value
   * @return true or false
   */
  boolean getBoolean()
  {
    assert type == BARVariableTypes.BOOLEAN;

    return b;
  }

  /** get long value
   * @return value
   */
  long getLong()
  {
    assert type == BARVariableTypes.LONG;

    return l;
  }

  /** get double value
   * @return value
   */
  double getDouble()
  {
    assert type == BARVariableTypes.DOUBLE;

    return d;
  }

  /** get string value
   * @return value
   */
  String getString()
  {
    assert (type == BARVariableTypes.STRING) || (type == BARVariableTypes.ENUMERATION);

    return string;
  }

  /** set boolean value
   * @param b value
   */
  void set(boolean b)
  {
    assert type == BARVariableTypes.BOOLEAN;

    this.b = b;
    Widgets.modified(this);
  }

  /** set long value
   * @param l value
   */
  void set(long l)
  {
    assert type == BARVariableTypes.LONG;

    this.l = l;
    Widgets.modified(this);
  }

  /** set double value
   * @param d value
   */
  void set(double d)
  {
    assert type == BARVariableTypes.DOUBLE;

    this.d = d;
    Widgets.modified(this);
  }

  /** set string value
   * @param string value
   */
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

  /** compare string values
   * @param value
   * @return true iff equal
   */
  public boolean equals(String value)
  {
    String s = toString();
    return (s != null)?s.equals(value):(value == null);
  }

  /** convert to string
   * @return string
   */
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

/** widget listener
 */
class WidgetListener
{
  private BARVariable variable;
  private Control     control;

  // cached text for widget
  private String cachedText = null;

  /** create widget listener
   */
  WidgetListener()
  {
    this.control  = null;
    this.variable = null;
  }

  /** create widget listener
   * @param control control widget
   * @param variable BAR variable
   */
  WidgetListener(Control control, BARVariable variable)
  {
    this.control  = control;
    this.variable = variable;
  }

  /** set control widget
   * @param control control widget
   */
  void setControl(Control control)
  {
    this.control = control;
  }

  /** set variable
   * @param variable BAR variable
   * @return 
   */
  void setVariable(BARVariable variable)
  {
    this.variable = variable;
  }

  /** compare variables
   * @param variable variable
   * @return true iff equal
   */
  public boolean equals(Object variable)
  {
    return (this.variable != null) && this.variable.equals(variable);
  }

  /** nofity modify variable
   * @param control control widget
   * @param variable BAR variable
   */
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
    else if (control instanceof Spinner)
    {
      int n = 0;
      switch (variable.getType())
      {
        case LONG:   n = (int)variable.getLong(); break; 
        case DOUBLE: n = (int)variable.getDouble(); break; 
      }
      ((Spinner)control).setSelection(n);
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
      throw new InternalError("Unknown widget '"+control+"' in wiget listener!");
    }
  }

  /** nofity modify variable
   * @param variable BAR variable
   */
  public void modified(BARVariable variable)
  {
    modified(control,variable);
  }

  /** nofity modify variable
   */
  public void modified()
  {
    modified(variable);
  }

  /** get string of varable
   * @param variable BAR variable
   */
  String getString(BARVariable variable)
  {
    return null;
  }
}

/** archive name parts
*/
class ArchiveNameParts
{
  public String type;
  public String loginName;
  public String loginPassword;
  public String hostName;
  public int    hostPort;
  public String deviceName;
  public String fileName;

  /** parse archive name
   * @param type archive type
   * @param loginName login name
   * @param loginPassword login password
   * @param hostName host name
   * @param hostPort host port
   * @param deviceName device name
   * @param fileName file name
   */
  public ArchiveNameParts(final String type,
                          final String loginName,
                          final String loginPassword,
                          final String hostName,
                          final int    hostPort,
                          final String deviceName,
                          final String fileName
                         )
  {
    this.type          = type;
    this.loginName     = loginName;
    this.loginPassword = loginPassword;
    this.hostName      = hostName;
    this.hostPort      = hostPort;
    this.deviceName    = deviceName;
    this.fileName      = fileName;
  }

  /** parse archive name
   * @param archiveName archive name string
   */
  public ArchiveNameParts(final String archiveName)
  {
    type          = "";
    loginName     = "";
    loginPassword = "";
    hostName      = "";
    hostPort      = 0;
    deviceName    = "";
    fileName      = "";

    if       (archiveName.startsWith("ftp://"))
    {
      // ftp
      type = "ftp";

      String specifier = archiveName.substring(6);
      Object[] data = new Object[2];
      int index = 0;
      if      (StringParser.parse(specifier.substring(index),"%s:%s@",data,StringParser.QUOTE_CHARS))
      {
        loginName     = (String)data[0];
        loginPassword = (String)data[1];
        index = specifier.indexOf('@')+1;
      }
      else if (StringParser.parse(specifier.substring(index),"%s@",data,StringParser.QUOTE_CHARS))
      {
        loginName = (String)data[0];
        index = specifier.indexOf('@')+1;
      }
      if (StringParser.parse(specifier.substring(index),"%s/%s",data,StringParser.QUOTE_CHARS))
      {
        hostName = (String)data[0];
        fileName = (String)data[1];
      }
      else
      {
        fileName = specifier;
      }
    }
    else if (archiveName.startsWith("scp://"))
    {
      // scp
      type = "scp";

      String specifier = archiveName.substring(6);
      Object[] data = new Object[3];
      int index = 0;
      if (StringParser.parse(specifier.substring(index),"%s@",data,StringParser.QUOTE_CHARS))
      {
        loginName = (String)data[0];
        index = specifier.indexOf('@')+1;
      }
      if      (StringParser.parse(specifier.substring(index),"%s:%d/%s",data,StringParser.QUOTE_CHARS))
      {
        hostName = (String)data[0];
        hostPort = (Integer)data[1];
        fileName = (String)data[2];
      }
      else if (StringParser.parse(specifier.substring(index),"%s/%s",data,StringParser.QUOTE_CHARS))
      {
        hostName = (String)data[0];
        fileName = (String)data[1];
      }
      else
      {
        fileName = specifier;
      }
    }
    else if (archiveName.startsWith("sftp://"))
    {
      // sftp
      type = "sftp";

      String specifier = archiveName.substring(7);
      Object[] data = new Object[3];
      int index = 0;
      if (StringParser.parse(specifier.substring(index),"%s@",data,StringParser.QUOTE_CHARS))
      {
        loginName = (String)data[0];
        index = specifier.indexOf('@')+1;
      }
      if      (StringParser.parse(specifier.substring(index),"%s:%d/%s",data,StringParser.QUOTE_CHARS))
      {
        hostName = (String)data[0];
        hostPort = (Integer)data[1];
        fileName = (String)data[2];
      }
      else if (StringParser.parse(specifier.substring(index),"%s/%s",data,StringParser.QUOTE_CHARS))
      {
        hostName = (String)data[0];
        fileName = (String)data[1];
      }
      else
      {
        fileName = specifier;
      }
    }
    else if (archiveName.startsWith("dvd://"))
    {
      // dvd
      type = "dvd";

      String specifier = archiveName.substring(6);
      Object[] data = new Object[2];
      if (StringParser.parse(specifier,"%S:%S",data,StringParser.QUOTE_CHARS))
      {
        deviceName = (String)data[0];
        fileName   = (String)data[1];
      }
      else
      {
        fileName = specifier;
      }
    }
    else if (archiveName.startsWith("device://"))
    {
      // dvd
      type = "device";

      String specifier = archiveName.substring(9);
      Object[] data = new Object[2];
      if (StringParser.parse(specifier,"%S:%S",data,StringParser.QUOTE_CHARS))
      {
        deviceName = (String)data[0];
        fileName   = (String)data[1];
      }
      else
      {
        fileName = specifier;
      }
    }
    else if (archiveName.startsWith("file://"))
    {
      // file
      type = "filesystem";

      String specifier = archiveName.substring(7);
      fileName = specifier.substring(2);
    }
    else
    {
      // file
      type = "filesystem";

      fileName = archiveName;
    }
  }

  /** get archive name
   * @param fileName file name part
   * @return archive name
   */
  public String getArchiveName(String fileName)
  {
    StringBuffer archiveNameBuffer = new StringBuffer();

    if      (type.equals("ftp"))
    {
      archiveNameBuffer.append("ftp://");
      if (!loginName.equals("") || !hostName.equals(""))
      {
        if (!loginName.equals("") || !loginPassword.equals(""))
        {
          if (!loginName.equals("")) archiveNameBuffer.append(loginName);
          if (!loginPassword.equals("")) { archiveNameBuffer.append(':'); archiveNameBuffer.append(loginPassword); }
          archiveNameBuffer.append('@');
        }
        if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
        archiveNameBuffer.append('/');
      }
    }
    else if (type.equals("scp"))
    {
      archiveNameBuffer.append("scp://");
      if (!loginName.equals("") || !hostName.equals(""))
      {
        if (!loginName.equals("")) { archiveNameBuffer.append(loginName); archiveNameBuffer.append('@'); }
        if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
        if (hostPort > 0) { archiveNameBuffer.append(':'); archiveNameBuffer.append(hostPort); }
        archiveNameBuffer.append('/');
      }
    }
    else if (type.equals("sftp"))
    {
      archiveNameBuffer.append("sftp://");
      if (!loginName.equals("") || !hostName.equals(""))
      {
        if (!loginName.equals("")) { archiveNameBuffer.append(loginName); archiveNameBuffer.append('@'); }
        if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
        if (hostPort > 0) { archiveNameBuffer.append(':'); archiveNameBuffer.append(hostPort); }
        archiveNameBuffer.append('/');
      }
    }
    else if (type.equals("dvd"))
    {
      archiveNameBuffer.append("dvd://");
      if (!deviceName.equals(""))
      {
        archiveNameBuffer.append(deviceName);
        archiveNameBuffer.append(':');
      }
    }
    else if (type.equals("device"))
    {
      archiveNameBuffer.append("device://");
      if (!deviceName.equals(""))
      {
        archiveNameBuffer.append(deviceName);
        archiveNameBuffer.append(':');
      }
    }
    archiveNameBuffer.append(fileName);

    return archiveNameBuffer.toString();
  }

  /** get archive name
   * @return archive name
   */
  public String getArchiveName()
  {
    return getArchiveName(this.fileName);
  }
}

/** units
 */
class Units
{
  /** get byte size string
   * @param n byte value
   * @return string
   */
  public static String getByteSize(double n)
  {
    if      (n >= 1024*1024*1024) return String.format("%.1f",n/(1024*1024*1024));
    else if (n >=      1024*1024) return String.format("%.1f",n/(     1024*1024));
    else if (n >=           1024) return String.format("%.1f",n/(          1024));
    else                          return String.format("%d"  ,(long)n           );
  }

  /** get byte size unit
   * @param n byte value
   * @return unit
   */
  public static String getByteUnit(double n)
  {
    if      (n >= 1024*1024*1024) return "GBytes";
    else if (n >=      1024*1024) return "MBytes";
    else if (n >=           1024) return "KBytes";
    else                          return "bytes";
  }

  /** get byte size short unit
   * @param n byte value
   * @return unit
   */
  public static String getByteShortUnit(double n)
  {
    if      (n >= 1024*1024*1024) return "GB";
    else if (n >=      1024*1024) return "MB";
    else if (n >=           1024) return "KB";
    else                          return "B";
  }

  /** parse byte size string
   * @param string string to parse (<n>(%|B|M|MB|G|GB)
   * @return byte value
   */
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
    else if (string.endsWith("B"))
    {
      return (long)Double.parseDouble(string.substring(0,string.length()-1));
    }
    else
    {
      return (long)Double.parseDouble(string);
    }
  }

  /** format byte size
   * @param n byte value
   * @return string with unit
   */
  public static String formatByteSize(long n)
  {
    return getByteSize(n)+getByteShortUnit(n);
  }
}

public class BARControl
{
  class LoginData
  {
    String serverName;
    String password;
    int    port;
    int    tlsPort;

    LoginData(String serverName, int port, int tlsPort)
    {
      this.serverName = !serverName.equals("")?serverName:Settings.serverName;
      this.password   = Settings.serverPassword;
      this.port       = (port != 0)?port:Settings.serverPort;
      this.tlsPort    = (port != 0)?tlsPort:Settings.serverTLSPort;
    }
  }

  // --------------------------- constants --------------------------------
  final static String DEFAULT_SERVER_NAME     = "localhost";
  final static int    DEFAULT_SERVER_PORT     = 38523;
  final static int    DEFAULT_SERVER_TLS_PORT = 38524;

  // --------------------------- variables --------------------------------
  private String     serverName        = DEFAULT_SERVER_NAME;
  private int        serverPort        = DEFAULT_SERVER_PORT;
  private int        serverTLSPort     = DEFAULT_SERVER_TLS_PORT;
  private boolean    loginDialogFlag   = false;
  private String     serverKeyFileName = null;

  private boolean    debug = false;

  private Display    display;
  private Shell      shell;
  private TabFolder  tabFolder;
  private TabStatus  tabStatus;
  private TabJobs    tabJobs;
  private TabRestore tabRestore;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** print program usage
   * @param 
   * @return 
   */
  private void printUsage()
  {
    System.out.println("barcontrol usage: <options> --");
    System.out.println("");
    System.out.println("Options: -p|--port=<n>          - server port");
    System.out.println("         --tls-port=<n>         - TLS server port");
    System.out.println("         --login-dialog         - force to open login dialog");
    System.out.println("         --key-file=<file name> - key file name");
    System.out.println("         -h|--help              - print this help");
  }

  /** parse arguments
   * @param args arguments
   */
  private void parseArguments(String[] args)
  {
    // parse arguments
    int z = 0;
    while (z < args.length)
    {
      if      (args[z].startsWith("-h") || args[z].startsWith("--help"))
      {
        printUsage();
        System.exit(0);
      }
      else if (args[z].startsWith("-p=") || args[z].startsWith("--port="))
      {
        String value = args[z].substring(args[z].indexOf('=')+1);
        try
        {
          serverPort = Integer.parseInt(value);
        }
        catch (NumberFormatException exception)
        {
          throw new Error("Invalid value '"+value+"' for option --port (error: "+exception.getMessage()+")!");          
        }
        z += 1;
      }
      else if (args[z].equals("-p") || args[z].equals("--port"))
      {
        if ((z+1) >= args.length)
        {
          throw new Error("Expected value for option --port!");
        }
        try
        {
          serverPort = Integer.parseInt(args[z+1]);
        }
        catch (NumberFormatException exception)
        {
          throw new Error("Invalid value '"+args[z+1]+"' for option --port (error: "+exception.getMessage()+")!");          
        }
        z += 2;
      }
      else if (args[z].startsWith("--tls-port="))
      {
        String value = args[z].substring(args[z].indexOf('=')+1);
        try
        {
          serverTLSPort = Integer.parseInt(value);
        }
        catch (NumberFormatException exception)
        {
          throw new Error("Invalid value '"+value+"' for option --tls-port (error: "+exception.getMessage()+")!");          
        }
        z += 1;
      }
      else if (args[z].equals("--tls-port"))
      {
        if ((z+1) >= args.length)
        {
          throw new Error("Expected value for option --tls-port!");
        }
        try
        {
          serverTLSPort = Integer.parseInt(args[z+1]);
        }
        catch (NumberFormatException exception)
        {
          throw new Error("Invalid value '"+args[z+1]+"' for option --tls-port (error: "+exception.getMessage()+")!");          
        }
        z += 2;
      }
      else if (args[z].startsWith("--login-dialog="))
      {
        String value = args[z].substring(args[z].indexOf('=')+1).toLowerCase();
        if      (value.equals("yes") || value.equals("on")  || value.equals("1"))
        {
          loginDialogFlag = true;
        }
        else if (value.equals("no") || value.equals("off")  || value.equals("0"))
        {
          loginDialogFlag = false;
        }
        else
        {
          throw new Error("Invalid value '"+value+"' for option --login-dialog (error: expected yes,on,1 or no,off,0)!");
        }
        z += 1;
      }
      else if (args[z].equals("--login-dialog"))
      {
        loginDialogFlag = true;
        z += 1;
      }
      else if (args[z].startsWith("--key-file="))
      {
        serverKeyFileName = args[z].substring(args[z].indexOf('=')+1);
        z += 1;
      }
      else if (args[z].equals("--key-file"))
      {
        if ((z+1) >= args.length)
        {
          throw new Error("Expected value for option --key-file!");
        }
        serverKeyFileName = args[z+1];
        z += 2;
      }
      else if (args[z].equals("--debug"))
      {
        debug = true;
        z += 1;
      }
      else if (args[z].equals("--bar-server-debug"))
      {
        BARServer.debug = true;
        z += 1;
      }
      else if (args[z].equals("--"))
      {
        z += 1;
        break;
      }
      else if (args[z].startsWith("--"))
      {
        throw new Error("Unknown option '"+args[z]+"'!");
      }
      else
      {
        serverName = args[z];
        z += 1;
      }
    }
  }

  /** server/password dialog
   * @param loginData server login data 
   * @return true iff login data ok, false otherwise
   */
  private boolean getLoginData(final LoginData loginData)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite;
    Label           label;
    Button          button;

    final Shell dialog = Dialogs.open(new Shell(),"Login BAR server",250,SWT.DEFAULT);

    // password
    final Text   widgetServerName;
    final Text   widgetPassword;
    final Button widgetLoginButton;
    composite = new Composite(dialog,SWT.NONE);
    tableLayout = new TableLayout(null,new double[]{0.0,1.0},2);
    composite.setLayout(tableLayout);
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setText("Server:");
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W));

      widgetServerName = new Text(composite,SWT.LEFT|SWT.BORDER);
      if (loginData.serverName != null) widgetServerName.setText(loginData.serverName);
      widgetServerName.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE));

      label = new Label(composite,SWT.LEFT);
      label.setText("Password:");
      label.setLayoutData(new TableLayoutData(1,0,TableLayoutData.W));

      widgetPassword = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
      if (loginData.password != null) widgetPassword.setText(loginData.password);
//      widgetPassword.setSelection(0,loginData.password.length());
      widgetPassword.setLayoutData(new TableLayoutData(1,1,TableLayoutData.WE));
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE));
    {
      widgetLoginButton = new Button(composite,SWT.CENTER);
      widgetLoginButton.setText("Login");
      widgetLoginButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT));
      widgetLoginButton.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          loginData.serverName = widgetServerName.getText();
          loginData.password   = widgetPassword.getText();
          Dialogs.close(dialog,true);
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText("Cancel");
      button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT));
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

    // install handlers
    widgetServerName.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Text widget = (Text)selectionEvent.widget;
        widgetPassword.forceFocus();
      }
    });
    widgetPassword.addSelectionListener(new SelectionListener()
    {
      public void widgetSelected(SelectionEvent selectionEvent)
      {
      }
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Text widget = (Text)selectionEvent.widget;
        widgetLoginButton.forceFocus();
      }
    });

    if ((loginData.serverName != null) && (loginData.serverName.length() != 0)) widgetPassword.forceFocus();

    Boolean result = (Boolean)Dialogs.run(dialog);

    return (result != null)?result:false;
  }

  /** create main window
   */
  private void createWindow()
  {
    shell = new Shell(display);
    shell.setText("BAR control");
    shell.setLayout(new TableLayout(1.0,1.0));
  }

  /** create tabs
   */
  private void createTabs()
  {
    // create tab
    tabFolder = Widgets.newTabFolder(shell);
    Widgets.layout(tabFolder,0,0,TableLayoutData.NSWE);
    tabStatus  = new TabStatus (tabFolder,SWT.F1);
    tabJobs    = new TabJobs   (tabFolder,SWT.F2);
    tabRestore = new TabRestore(tabFolder,SWT.F3);
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
          case SWT.F3:
            Widgets.showTab(tabFolder,tabRestore.widgetTab);
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
    {
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
    }

    menu = Widgets.addMenu(menuBar,"Help");
    {
      menuItem = Widgets.addMenuItem(menu,"About");
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          Dialogs.info(shell,"About","BAR control "+Config.VERSION_MAJOR+"."+Config.VERSION_MINOR+".\n\nWritten by Torsten Rupp.\n\nThanx to Matthias Albert.");
        }
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
      });
    }
  }

  /** run application
   */
  private void run()
  {
    // set window size, manage window (approximate height according to height of a text line)
    shell.setSize(800,600+5*(Widgets.getTextHeight(shell)+4));
    shell.open();

//Dialogs.info(shell,"Test","test ddddddddddddddddddddddddd");
//Dialogs.error(shell,"test ddddddddddddddddddddddddd");
//Dialogs.confirm(shell,"Test","test ddddddddddddddddddddddddd");
//Dialogs.select(shell,"Test","test ddddddddddddddddddddddddd",new String[]{"1","2","3"},0);

    // add close listener
    shell.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        shell.dispose();
      }
    });

    // SWT event loop
    while (!shell.isDisposed())
    {
//System.err.print(".");
      if (!display.readAndDispatch())
      {
        display.sleep();
      }
    }
  }

  BARControl(String[] args)
  {
    try
    {
      // load settings
      Settings.load();

      // parse arguments
      parseArguments(args);

      // init display
      display = new Display();

      // connect to server
      LoginData loginData = new LoginData(serverName,serverPort,serverTLSPort);
      boolean connectOkFlag = false;
      if ((loginData.serverName != null) && !loginData.serverName.equals("") && (loginData.password != null) && !loginData.password.equals("") && !loginDialogFlag)
      {
        // connect to server with preset data
        try
        {
          BARServer.connect(loginData.serverName,loginData.port,loginData.tlsPort,loginData.password,serverKeyFileName);
          connectOkFlag = true;
        }
        catch (ConnectionError error)
        {
        }
      }
      while (!connectOkFlag)
      {
        // get login data
        if (!getLoginData(loginData))
        {
          System.exit(0);
        }
        if ((loginData.port == 0) && (loginData.tlsPort == 0))
        {
          throw new Error("Cannot connect to server. No server ports specified!");
        }
/// ??? host name scheck

        // connect to server
        try
        {
          BARServer.connect(loginData.serverName,loginData.port,loginData.tlsPort,loginData.password,serverKeyFileName);
          connectOkFlag = true;
        }
        catch (ConnectionError error)
        {
          if (!Dialogs.confirmError(new Shell(),"Connection fail","Error: "+error.getMessage(),"Try again","Cancel"))
          {
            System.exit(1);
          }
        }
      }

      // open main window
      createWindow();
      createTabs();
      createMenu();

      // run
      run();

      // disconnect
      BARServer.disconnect();

      // save settings
      Settings.save();
    }
    catch (org.eclipse.swt.SWTException exception)
    {
      System.err.println("ERROR graphics: "+exception.getCause());
      if (debug)
      {
        for (StackTraceElement stackTraceElement : exception.getStackTrace())
        {
          System.err.println("  "+stackTraceElement);
        }
      }
    }
    catch (CommunicationError communicationError)
    {
      System.err.println("ERROR communication: "+communicationError.getMessage());
    }
    catch (AssertionError assertionError)
    {
      System.err.println("INTERNAL ERROR: "+assertionError.toString());
      for (StackTraceElement stackTraceElement : assertionError.getStackTrace())
      {
        System.err.println("  "+stackTraceElement);
      }
      System.err.println("");
      System.err.println("Please report this assertion error to torsten.rupp@gmx.net.");
    }
    catch (InternalError error)
    {
      System.err.println("INTERNAL ERROR: "+error.getMessage());
    }
    catch (Error error)
    {
      System.err.println("ERROR: "+error.getMessage());
      if (debug)
      {
        for (StackTraceElement stackTraceElement : error.getStackTrace())
        {
          System.err.println("  "+stackTraceElement);
        }
      }
    }
  }

  public static void main(String[] args)
  {
    BARControl barControl = new BARControl(args);
  }
}

/* end of file */
