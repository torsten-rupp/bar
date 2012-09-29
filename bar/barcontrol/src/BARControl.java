/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/BARControl.java,v $
* $Revision: 1.30 $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
// base
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.security.KeyStore;
import java.text.SimpleDateFormat;
import java.text.NumberFormat;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.LinkedHashSet;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

// graphics
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

/** storage types
 */
enum StorageTypes
{
  NONE,

  FILESYSTEM,
  FTP,
  SCP,
  SFTP,
  CD,
  DVD,
  BD,
  DEVICE;

  /** parse type string
   * @param string type string
   * @return priority
   */
  static StorageTypes parse(String string)
  {
    StorageTypes type;

    if      (string.equalsIgnoreCase("filesystem"))
    {
      type = StorageTypes.FILESYSTEM;
    }
    else if (string.equalsIgnoreCase("ftp"))
    {
      type = StorageTypes.FTP;
    }
    else if (string.equalsIgnoreCase("scp"))
    {
      type = StorageTypes.SCP;
    }
    else if (string.equalsIgnoreCase("sftp"))
    {
      type = StorageTypes.SFTP;
    }
    else if (string.equalsIgnoreCase("cd"))
    {
      type = StorageTypes.CD;
    }
    else if (string.equalsIgnoreCase("dvd"))
    {
      type = StorageTypes.DVD;
    }
    else if (string.equalsIgnoreCase("bd"))
    {
      type = StorageTypes.BD;
    }
    else if (string.equalsIgnoreCase("device"))
    {
      type = StorageTypes.DEVICE;
    }
    else
    {
      type = StorageTypes.NONE;
    }

    return type;
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    switch (this)
    {
      case FILESYSTEM: return "filesystem";
      case FTP:        return "ftp";
      case SCP:        return "scp";
      case SFTP:       return "sftp";
      case CD:         return "cd";
      case DVD:        return "dvd";
      case BD:         return "bd";
      case DEVICE:     return "device";
      default:         return "";
    }
  }
}

/** archive name parts
*/
class ArchiveNameParts
{
  public StorageTypes type;           // type
  public String       loginName;      // login name
  public String       loginPassword;  // login password
  public String       hostName;       // host name
  public int          hostPort;       // host port
  public String       deviceName;     // device name
  public String       fileName;       // file name

  /** parse archive name
   * @param type archive type
   * @param loginName login name
   * @param loginPassword login password
   * @param hostName host name
   * @param hostPort host port
   * @param deviceName device name
   * @param fileName file name
   */
  public ArchiveNameParts(StorageTypes type,
                          String       loginName,
                          String       loginPassword,
                          String       hostName,
                          int          hostPort,
                          String       deviceName,
                          String       fileName
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
   *   ftp://<login name>:<login password>@<host name>[:<host port>]/<file name>
   *   scp://<login name>@<host name>:<host port>/<file name>
   *   sftp://<login name>@<host name>:<host port>/<file name>
   *   cd://<device name>/<file name>
   *   dvd://<device name>/<file name>
   *   bd://<device name>/<file name>
   *   device://<device name>/<file name>
   *   file://<file name>
   *   <file name>
   */
  public ArchiveNameParts(String archiveName)
  {
    Matcher matcher;

    type          = StorageTypes.NONE;
    loginName     = "";
    loginPassword = "";
    hostName      = "";
    hostPort      = 0;
    deviceName    = "";
    fileName      = "";

    if       (archiveName.startsWith("ftp://"))
    {
      // ftp
      type = StorageTypes.FTP;

      String specifier = archiveName.substring(6);
//Dprintf.dprintf("specifier=%s",specifier);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<login name>:<login password>@<host name>:<host port>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        hostPort      = Integer.parseInt(matcher.group(5));
        fileName      = matcher.group(6);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<login name>@<host name>:<host port>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        hostPort  = Integer.parseInt(matcher.group(4));
        fileName  = matcher.group(5);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<host name>:<host port>/<file name>
        hostName = matcher.group(1);
        hostPort = Integer.parseInt(matcher.group(2));
        fileName = matcher.group(3);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else
      {
        // ftp://<file name>
        fileName = specifier;
      }
    }
    else if (archiveName.startsWith("scp://"))
    {
      // scp
      type = StorageTypes.SCP;

      String specifier = archiveName.substring(6);
//Dprintf.dprintf("specifier=%s",specifier);
      if      ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // scp://<login name>@<host name>:<host port>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        hostPort  = Integer.parseInt(matcher.group(4));
        fileName  = matcher.group(5);
//Dprintf.dprintf("%s: loginName=%s hostName=%s hostPort=%d fileName=%s",matcher.group(0),loginName,hostName,hostPort,fileName);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // scp://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
//Dprintf.dprintf("%s: loginName=%s hostName=%s fileName=%s",matcher.group(0),loginName,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // scp://<host name>:<host port>/<file name>
        hostName  = matcher.group(1);
        hostPort  = Integer.parseInt(matcher.group(2));
        fileName  = matcher.group(3);
//Dprintf.dprintf("%s: loginName=%s hostName=%s hostPort=%d fileName=%s",matcher.group(0),loginName,hostName,hostPort,fileName);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // scp://<host name>/<file name>
        hostName  = matcher.group(1);
        fileName  = matcher.group(2);
//Dprintf.dprintf("%s: loginName=%s hostName=%s fileName=%s",matcher.group(0),loginName,hostName,fileName);
      }
      else
      {
//Dprintf.dprintf("");
        // scp://<file name>
        fileName = specifier;
      }
    }
    else if (archiveName.startsWith("sftp://"))
    {
      // sftp
      type = StorageTypes.SFTP;

      String specifier = archiveName.substring(7);
//Dprintf.dprintf("specifier=%s",specifier);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>:<login password>@<host name>:<host port>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        hostPort      = Integer.parseInt(matcher.group(5));
        fileName      = matcher.group(6);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s hostPort=%d fileName=%s",matcher.group(0),loginName,loginPassword,hostName,hostPort,fileName);
      }
      else if ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>@<host name>:<host port>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        hostPort  = Integer.parseInt(matcher.group(4));
        fileName  = matcher.group(5);
//Dprintf.dprintf("%s: loginName=%s hostName=%s hostPort=%d fileName=%s",matcher.group(0),loginName,hostName,hostPort,fileName);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<host name>:<host port>/<file name>
        hostName = matcher.group(1);
        hostPort = Integer.parseInt(matcher.group(2));
        fileName = matcher.group(3);
//Dprintf.dprintf("%s: hostName=%s hostPort=%d fileName=%s",matcher.group(0),hostName,hostPort,fileName);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<host name>/<file name>
        hostName = matcher.group(1);
        fileName = matcher.group(2);
//Dprintf.dprintf("%s: hostName=%s fileName=%s",matcher.group(0),hostName,fileName);
      }
      else
      {
        // sftp://<file name>
        fileName = specifier;
      }
    }
    else if (archiveName.startsWith("cd://"))
    {
      // cd
      type = StorageTypes.CD;

      String specifier = archiveName.substring(5);
//Dprintf.dprintf("specifier=%s",specifier);
      Object[] data = new Object[2];
      if      ((matcher = Pattern.compile("^([^:]*?):(.*)$").matcher(specifier)).matches())
      {
        // cd://<device name>:<file name>
        deviceName = matcher.group(1);
        fileName   = matcher.group(2);
//Dprintf.dprintf("%s: deviceName=%s fileName=%s",matcher.group(0),deviceName,fileName);
      }
      else
      {
        // cd://<file name>
        fileName = specifier;
//Dprintf.dprintf("%s: deviceName=%s fileName=%s",matcher.group(0),deviceName,fileName);
      }
    }
    else if (archiveName.startsWith("dvd://"))
    {
      // dvd
      type = StorageTypes.DVD;

      String specifier = archiveName.substring(6);
      if      ((matcher = Pattern.compile("^([^:]*?):(.*)$").matcher(specifier)).matches())
      {
        // dvd://<device name>:<file name>
        deviceName = matcher.group(1);
        fileName   = matcher.group(2);
//Dprintf.dprintf("%s: deviceName=%s fileName=%s",matcher.group(0),deviceName,fileName);
      }
      else
      {
        // dvd://<file name>
        fileName = specifier;
//Dprintf.dprintf("%s: deviceName=%s fileName=%s",matcher.group(0),deviceName,fileName);
      }
    }
    else if (archiveName.startsWith("bd://"))
    {
      // bd
      type = StorageTypes.BD;

      String specifier = archiveName.substring(5);
//Dprintf.dprintf("specifier=%s",specifier);
      if      ((matcher = Pattern.compile("^([^:]*?):(.*)$").matcher(specifier)).matches())
      {
        // bd://<device name>:<file name>
        deviceName = matcher.group(1);
        fileName   = matcher.group(2);
//Dprintf.dprintf("%s: deviceName=%s fileName=%s",matcher.group(0),deviceName,fileName);
      }
      else
      {
        // bd://<file name>
        fileName = specifier;
//Dprintf.dprintf("%s: deviceName=%s fileName=%s",matcher.group(0),deviceName,fileName);
      }
    }
    else if (archiveName.startsWith("device://"))
    {
      // device0
      type = StorageTypes.DEVICE;

      String specifier = archiveName.substring(9);
//Dprintf.dprintf("specifier=%s",specifier);
      if      ((matcher = Pattern.compile("^([^:]*?):(.*)$").matcher(specifier)).matches())
      {
        // device://<device name>:<file name>
        deviceName = matcher.group(1);
        fileName   = matcher.group(2);
//Dprintf.dprintf("%s: deviceName=%s fileName=%s",matcher.group(0),deviceName,fileName);
      }
      else
      {
        // device://<file name>
        fileName = specifier;
//Dprintf.dprintf("%s: deviceName=%s fileName=%s",matcher.group(0),deviceName,fileName);
      }
    }
    else if (archiveName.startsWith("file://"))
    {
      // file
      type = StorageTypes.FILESYSTEM;

      String specifier = archiveName.substring(7);
      fileName = specifier.substring(2);
    }
    else
    {
      // file
      type = StorageTypes.FILESYSTEM;

      fileName = archiveName;
    }
  }

  /** get archive name
   * @param fileName file name part
   * @return archive name
   */
  public String getName(String fileName)
  {
    StringBuilder archiveNameBuffer = new StringBuilder();

    switch (type)
    {
      case FILESYSTEM:
        break;
      case FTP:
        archiveNameBuffer.append("ftp://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("") || !loginPassword.equals(""))
          {
            if (!loginName.equals("")) archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.equals("")) { archiveNameBuffer.append(':'); archiveNameBuffer.append(loginPassword); }
            archiveNameBuffer.append('@');
          }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
          archiveNameBuffer.append('/');
        }
        break;
      case SCP:
        archiveNameBuffer.append("scp://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("")) { archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); archiveNameBuffer.append('@'); }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
          if (hostPort > 0) { archiveNameBuffer.append(':'); archiveNameBuffer.append(hostPort); }
          archiveNameBuffer.append('/');
        }
        break;
      case SFTP:
        archiveNameBuffer.append("sftp://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("")) { archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); archiveNameBuffer.append('@'); }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
          if (hostPort > 0) { archiveNameBuffer.append(':'); archiveNameBuffer.append(hostPort); }
          archiveNameBuffer.append('/');
        }
        break;
      case CD:
        archiveNameBuffer.append("cd://");
        if (!deviceName.equals(""))
        {
          archiveNameBuffer.append(deviceName);
          archiveNameBuffer.append(':');
        }
        break;
      case DVD:
        archiveNameBuffer.append("dvd://");
        if (!deviceName.equals(""))
        {
          archiveNameBuffer.append(deviceName);
          archiveNameBuffer.append(':');
        }
        break;
      case BD:
        archiveNameBuffer.append("bd://");
        if (!deviceName.equals(""))
        {
          archiveNameBuffer.append(deviceName);
          archiveNameBuffer.append(':');
        }
        break;
      case DEVICE:
        archiveNameBuffer.append("device://");
        if (!deviceName.equals(""))
        {
          archiveNameBuffer.append(deviceName);
          archiveNameBuffer.append(':');
        }
        break;
    }
    if (fileName != null)
    {
      archiveNameBuffer.append(fileName);
    }

    return archiveNameBuffer.toString();
  }

  /** get archive path name
   * @return archive path name (archive name without file name)
   */
  public String getPath()
  {
    File file = new File(fileName);
    return getName(file.getParent());
  }

  /** get archive name
   * @return archive name
   */
  public String getName()
  {
    return getName(fileName);
  }

  /** get archive name
   * @param fileName file name part
   * @return archive name
   */
  public String getPrintableName(String fileName)
  {
    StringBuilder archiveNameBuffer = new StringBuilder();

    switch (type)
    {
      case FILESYSTEM:
        break;
      case FTP:
        archiveNameBuffer.append("ftp://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("") || !loginPassword.equals(""))
          {
            if (!loginName.equals("")) archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            archiveNameBuffer.append('@');
          }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
          archiveNameBuffer.append('/');
        }
        break;
      case SCP:
        archiveNameBuffer.append("scp://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("")) { archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); archiveNameBuffer.append('@'); }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
          if (hostPort > 0) { archiveNameBuffer.append(':'); archiveNameBuffer.append(hostPort); }
          archiveNameBuffer.append('/');
        }
        break;
      case SFTP:
        archiveNameBuffer.append("sftp://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("")) { archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); archiveNameBuffer.append('@'); }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
          if (hostPort > 0) { archiveNameBuffer.append(':'); archiveNameBuffer.append(hostPort); }
          archiveNameBuffer.append('/');
        }
        break;
      case CD:
        archiveNameBuffer.append("cd://");
        if (!deviceName.equals(""))
        {
          archiveNameBuffer.append(deviceName);
          archiveNameBuffer.append(':');
        }
        break;
      case DVD:
        archiveNameBuffer.append("dvd://");
        if (!deviceName.equals(""))
        {
          archiveNameBuffer.append(deviceName);
          archiveNameBuffer.append(':');
        }
        break;
      case BD:
        archiveNameBuffer.append("bd://");
        if (!deviceName.equals(""))
        {
          archiveNameBuffer.append(deviceName);
          archiveNameBuffer.append(':');
        }
        break;
      case DEVICE:
        archiveNameBuffer.append("device://");
        if (!deviceName.equals(""))
        {
          archiveNameBuffer.append(deviceName);
          archiveNameBuffer.append(':');
        }
        break;
    }
    if (fileName != null)
    {
      archiveNameBuffer.append(fileName);
    }

    return archiveNameBuffer.toString();
  }

  /** get archive name
   * @return archive name
   */
  public String getPrintableName()
  {
    return getPrintableName(fileName);
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    return getName();
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
    if      (n >= 1024*1024*1024) return String.format(Locale.US,"%.1f",n/(1024*1024*1024));
    else if (n >=      1024*1024) return String.format(Locale.US,"%.1f",n/(     1024*1024));
    else if (n >=           1024) return String.format(Locale.US,"%.1f",n/(          1024));
    else                          return String.format(Locale.US,"%d"  ,(long)n           );
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
    if      (n >= 1024*1024*1024) return "G";
    else if (n >=      1024*1024) return "M";
    else if (n >=           1024) return "K";
    else                          return "";
  }

  /** parse byte size string
   * @param string string to parse (<n>.<n>(%|B|M|MB|G|GB)
   * @return byte value
   */
  public static long parseByteSize(String string)
    throws NumberFormatException
  {
    string = string.toUpperCase();

    // try to parse with default locale
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

  /** parse byte size string
   * @param string string to parse (<n>(%|B|M|MB|G|GB)
   * @param defaultValue default value if number cannot be parsed
   * @return byte value
   */
  public static long parseByteSize(String string, long defaultValue)
  {
    long n;

    try
    {
      n = Units.parseByteSize(string);
    }
    catch (NumberFormatException exception)
    {
      n = defaultValue;
    }

    return n;
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

/** BARControl
 */
public class BARControl
{
  /** login data
   */
  class LoginData
  {
    String serverName;       // server name
    String password;         // login password
    int    port;             // server port
    int    tlsPort;          // server TLS port

    /** create login data
     * @param serverName server name
     * @param port server port
     * @param tlsPort server TLS port
     */
    LoginData(String serverName, int port, int tlsPort)
    {
      this.serverName = !serverName.equals("")?serverName:Settings.serverName;
      this.password   = Settings.serverPassword;
      this.port       = (port != 0)?port:Settings.serverPort;
      this.tlsPort    = (port != 0)?tlsPort:Settings.serverTLSPort;
    }
  }

  // --------------------------- constants --------------------------------

  // command line options
  private static final OptionEnumeration[] archiveTypeEnumeration =
  {
    new OptionEnumeration("normal",      Settings.ArchiveTypes.NORMAL),
    new OptionEnumeration("full",        Settings.ArchiveTypes.FULL),
    new OptionEnumeration("incremental", Settings.ArchiveTypes.INCREMENTAL),
    new OptionEnumeration("differential",Settings.ArchiveTypes.DIFFERENTIAL),
  };

  private static final Option[] options =
  {
    new Option("--port",                       "-p",Options.Types.INTEGER,    "serverPort"),
    new Option("--tls-port",                   null,Options.Types.INTEGER,    "serverTLSPort"),
    new Option("--key-file",                   null,Options.Types.STRING,     "serverKeyFileName"),
    new Option("--select-job",                 null,Options.Types.STRING,     "selectedJobName"),
    new Option("--login-dialog",               null,Options.Types.BOOLEAN,    "loginDialogFlag"),

    new Option("--job",                        "-j",Options.Types.STRING,     "runJobName"),
    new Option("--archive-type",               null,Options.Types.ENUMERATION,"archiveType",archiveTypeEnumeration),
    new Option("--abort",                      null,Options.Types.STRING,     "abortJobName"),
    new Option("--index-database-add",         null,Options.Types.STRING,     "indexDatabaseAddStorageName"),
    new Option("--index-database-remove",      null,Options.Types.STRING,     "indexDatabaseRemoveStorageName"),
    new Option("--index-database-storage-list","-a",Options.Types.STRING,     "indexDatabaseStorageListPattern"),
    new Option("--index-database-entries-list","-e",Options.Types.STRING,     "indexDatabaseEntriesListPattern"),
    new Option("--pause",                      "-t",Options.Types.INTEGER,    "pauseTime"),
    new Option("--ping",                       "-i",Options.Types.BOOLEAN,    "pingFlag"),
    new Option("--suspend",                    "-s",Options.Types.BOOLEAN,    "suspendFlag"),
    new Option("--continue",                   "-c",Options.Types.BOOLEAN,    "continueFlag"),
    new Option("--list",                       "-l",Options.Types.BOOLEAN,    "listFlag"),

    new Option("--debug",                      null,Options.Types.BOOLEAN,    "debugFlag"),
    new Option("--debug-server",               null,Options.Types.BOOLEAN,    "debugServerFlag"),
    new Option("--debug-quit-server",          null,Options.Types.BOOLEAN,    "debugQuitServerFlag"),

    new Option("--help",                       "-h",Options.Types.BOOLEAN,    "helpFlag"),

    // ignored
    new Option("--swing",                      null, Options.Types.BOOLEAN,   null),
  };

  // --------------------------- variables --------------------------------
  private Display    display;
  private Shell      shell;
  private TabFolder  tabFolder;
  private TabStatus  tabStatus;
  private TabJobs    tabJobs;
  private TabRestore tabRestore;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** print error to stderr
   * @param format format string
   * @param args optional arguments
   */
  public static void printError(String format, Object... args)
  {
    System.err.println("ERROR: "+String.format(format,args));
  }

  /** print warning to stderr
   * @param format format string
   * @param args optional arguments
   */
  public static void printWarning(String format, Object... args)
  {
    System.err.println("Warning: "+String.format(format,args));
  }

  /** print internal error to stderr
   * @param format format string
   * @param args optional arguments
   */
  public static void printInternalError(String format, Object... args)
  {
    System.err.println("INTERNAL ERROR: "+String.format(format,args));
  }

  /** renice i/o exception (remove java.io.IOExcpetion text from exception)
   * @param exception i/o exception to renice
   * @return reniced exception
   */
  public static IOException reniceIOException(IOException exception)
  {
    final Pattern PATTERN = Pattern.compile("^(.*?)\\s*java.io.IOException: error=\\d+,\\s*(.*)$",Pattern.CASE_INSENSITIVE);

    Matcher matcher;
    if ((matcher = PATTERN.matcher(exception.getMessage())).matches())
    {
      exception = new IOException(matcher.group(1)+" "+matcher.group(2));
    }

    return exception;
  }

  // ----------------------------------------------------------------------

  /** print program usage
   * @param
   * @return
   */
  private void printUsage()
  {
    System.out.println("barcontrol usage: <options> --");
    System.out.println("");
    System.out.println("Options: -p|--port=<n>                              - server port (default: "+Settings.DEFAULT_SERVER_PORT+")");
    System.out.println("         --tls-port=<n>                             - TLS server port (default: "+Settings.DEFAULT_SERVER_TLS_PORT+")");
    System.out.println("         --select-job=<name>                        - select job <name>");
    System.out.println("         --login-dialog                             - force to open login dialog");
    System.out.println("         --key-file=<file name>                     - key file name (default: ");
    System.out.println("                                                        ."+File.separator+BARServer.JAVA_SSL_KEY_FILE_NAME+" or ");
    System.out.println("                                                        "+System.getProperty("user.home")+File.separator+".bar"+File.separator+BARServer.JAVA_SSL_KEY_FILE_NAME+" or ");
    System.out.println("                                                        "+Config.CONFIG_DIR+File.separator+BARServer.JAVA_SSL_KEY_FILE_NAME);
    System.out.println("                                                      )" );
    System.out.println("");
    System.out.println("         -j|--job=<name>                            - start execution of job <name>");
    System.out.println("         --archive-type=<mode>                      - archive type");
    System.out.println("                                                        normal (default)");
    System.out.println("                                                        full");
    System.out.println("                                                        incremental");
    System.out.println("                                                        differential");
    System.out.println("         --abort=<name>                             - abort execution of job <name>");
    System.out.println("         --index-database-add=<name>                - add storage archive <name> to index");
    System.out.println("         --index-database-remove=<pattern>          - remove storage archive <name> from index");
    System.out.println("         -a|--index-database-storage-list=<pattern> - list storage archives matching pattern <pattern>");
    System.out.println("         -e|--index-database-entries-list=<pattern> - list entries matching pattern <pattern>");
    System.out.println("         -p|--pause=<n>                             - pause job execution for <n> seconds");
    System.out.println("         -i|--ping                                  - check connection to server");
    System.out.println("         -s|--suspend                               - suspend job execution");
    System.out.println("         -c|--continue                              - continue job execution");
    System.out.println("         -l|--list                                  - list jobs");
    System.out.println("");
    System.out.println("         -h|--help                                  - print this help");
  }

  /** parse arguments
   * @param args arguments
   */
  private void parseArguments(String[] args)
  {
    // parse arguments
    int z = 0;
    boolean endOfOptions = false;
    while (z < args.length)
    {
      if      (!endOfOptions && args[z].equals("--"))
      {
        endOfOptions = true;
        z++;
      }
      else if (!endOfOptions && (args[z].startsWith("--") || args[z].startsWith("-")))
      {
        int i = Options.parse(options,args,z,Settings.class);
        if (i < 0)
        {
          throw new Error("Unknown option '"+args[z]+"'!");
        }
        z = i;
      }
      else
      {
        Settings.serverName = args[z];
        z++;
      }
    }

    // help
    if (Settings.helpFlag)
    {
      printUsage();
      System.exit(0);
    }

    // check arguments
    if (Settings.serverKeyFileName != null)
    {
      // check if JKS file is readable
      try
      {
        KeyStore keyStore = java.security.KeyStore.getInstance("JKS");
        keyStore.load(new java.io.FileInputStream(Settings.serverKeyFileName),null);
      }
      catch (java.security.NoSuchAlgorithmException exception)
      {
        throw new Error(exception.getMessage());
      }
      catch (java.security.cert.CertificateException exception)
      {
        throw new Error(exception.getMessage());
      }
      catch (java.security.KeyStoreException exception)
      {
        throw new Error(exception.getMessage());
      }
      catch (FileNotFoundException exception)
      {
        throw new Error("Java Key Store (JKS) file '"+Settings.serverKeyFileName+"' not found");
      }
      catch (IOException exception)
      {
        throw new Error("not a Java Key Store (JKS) file '"+Settings.serverKeyFileName+"'",exception);
      }
    }
  }

  /** get job id
   * @param name job name
   * @return job id or -1 if not found
   */
  private static int getJobId(String name)
  {
    ArrayList<String> result = new ArrayList<String>();
    BARServer.executeCommand("JOB_LIST",result);
    for (String line : result)
    {
      Object data[] = new Object[12];
      /* format:
         <id>
         <name>
         <state>
         <type>
         <archivePartSize>
         <compressAlgorithms>
         <cryptAlgorithm>
         <cryptType>
         <cryptPasswordMode>
         <lastExecutedDateTime>
         <estimatedRestTime>
      */
      if (StringParser.parse(line,"%d %S %S %s %ld %S %S %S %S %ld %ld",data,StringParser.QUOTE_CHARS))
      {
        if (name.equalsIgnoreCase((String)data[1]))
        {
          return (Integer)data[0];
        }
      }
      else
      {
        if (Settings.debugFlag)
        {
          printWarning("unknown server response '%s'",line);
          BARServer.disconnect();
          System.exit(1);
        }
      }
    }

    return -1;
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

    final Shell dialog = Dialogs.openModal(new Shell(),"Login BAR server",250,SWT.DEFAULT);

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
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          loginData.serverName = widgetServerName.getText();
          loginData.password   = widgetPassword.getText();
          Dialogs.close(dialog,true);
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText("Cancel");
      button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          Dialogs.close(dialog,false);
        }
      });
    }

    // install handlers
    widgetServerName.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Text widget = (Text)selectionEvent.widget;
        widgetPassword.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
      }
    });
    widgetPassword.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        Text widget = (Text)selectionEvent.widget;
        widgetLoginButton.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
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
  private void createTabs(String selectedJobName)
  {
    // create tab
    tabFolder = Widgets.newTabFolder(shell);
    Widgets.layout(tabFolder,0,0,TableLayoutData.NSWE);
    tabStatus  = new TabStatus (tabFolder,SWT.F1);
    tabJobs    = new TabJobs   (tabFolder,SWT.F2);
    tabRestore = new TabRestore(tabFolder,SWT.F3);
    tabStatus.setTabJobs(tabJobs);
    tabJobs.setTabStatus(tabStatus);

    // pre-select job
    if (selectedJobName != null)
    {
      tabStatus.selectJob(selectedJobName);
      tabJobs.selectJob(selectedJobName);
    }

    // add tab listener
    display.addFilter(SWT.KeyDown,new Listener()
    {
      public void handleEvent(Event event)
      {
        switch (event.keyCode)
        {
          case SWT.F1:
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
    Menu     menu,subMenu;
    MenuItem menuItem;

    // create menu
    menuBar = Widgets.newMenuBar(shell);

    menu = Widgets.addMenu(menuBar,"Program");
    {
      menuItem = Widgets.addMenuItem(menu,"Start\u2026",SWT.CTRL+'S');
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          Widgets.notify(tabStatus.widgetButtonStart);
        }
      });

      menuItem = Widgets.addMenuItem(menu,"Abort\u2026",SWT.CTRL+'A');
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          Widgets.notify(tabStatus.widgetButtonAbort);
        }
      });

      subMenu = Widgets.addMenu(menu,"Pause");
      {
        menuItem = Widgets.addMenuItem(subMenu,"10min");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            tabStatus.jobPause(10*60);
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,"60min",SWT.CTRL+'P');
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            tabStatus.jobPause(60*60);
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,"120min");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            tabStatus.jobPause(120*60);
          }
        });

        menuItem = Widgets.addMenuSeparator(subMenu);

        menuItem = Widgets.addMenuCheckbox(subMenu,"Create operation",Settings.pauseCreateFlag);
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            Settings.pauseCreateFlag = widget.getSelection();
            tabStatus.jobPause(60*60);
          }
        });

        menuItem = Widgets.addMenuCheckbox(subMenu,"Storage operation",Settings.pauseStorageFlag);
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            Settings.pauseStorageFlag = widget.getSelection();
            tabStatus.jobPause(60*60);
          }
        });

        menuItem = Widgets.addMenuCheckbox(subMenu,"Restore operation",Settings.pauseRestoreFlag);
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            Settings.pauseRestoreFlag = widget.getSelection();
            tabStatus.jobPause(60*60);
          }
        });
        menuItem = Widgets.addMenuCheckbox(subMenu,"Index update operation",Settings.pauseIndexUpdateFlag);
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            Settings.pauseIndexUpdateFlag = widget.getSelection();
          }
        });
      }

      menuItem = Widgets.addMenuItem(menu,"Toggle suspend/continue",SWT.CTRL+'S');
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          Widgets.notify(tabStatus.widgetButtonSuspendContinue);
        }
      });

      menuItem = Widgets.addMenuItem(menu,"Clear stored passwords on server",SWT.NONE);
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String[] result = new String[1];
          int errorCode = BARServer.executeCommand("PASSWORDS_CLEAR",result);
          if (errorCode != Errors.NONE)
          {
            Dialogs.error(shell,"Cannot clear passwords on server:\n\n"+result[0]);
          }
        }
      });

      Widgets.addMenuSeparator(menu);
      menuItem = Widgets.addMenuItem(menu,"Quit",SWT.CTRL+'Q');

      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          Widgets.notify(tabStatus.widgetButtonQuit);
        }
      });
    }

    menu = Widgets.addMenu(menuBar,"Help");
    {
      menuItem = Widgets.addMenuItem(menu,"About\u2026");
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem widget = (MenuItem)selectionEvent.widget;
          Dialogs.info(shell,"About","BAR control "+Config.VERSION_MAJOR+"."+Config.VERSION_MINOR+".\n\nWritten by Torsten Rupp.\n\nThanx to Matthias Albert.");
        }
      });
    }

    if (Settings.debugFlag)
    {
      menu = Widgets.addMenu(menuBar,"Debug");
      {
        menuItem = Widgets.addMenuItem(menu,"Print debug statistics");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            BARServer.executeCommand("DEBUG_PRINT_STATISTICS");
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Print debug memory info");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            BARServer.executeCommand("DEBUG_PRINT_MEMORY_INFO");
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Dump debug memory info");
        menuItem.addSelectionListener(new SelectionListener()
        {
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            BARServer.executeCommand("DEBUG_DUMP_MEMORY_INFO");
          }
        });
      }
    }
  }

  /** run application
   */
  private void run()
  {
    // set window size, manage window (approximate height according to height of a text line)
    shell.setSize(840,600+5*(Widgets.getTextHeight(shell)+4));
    shell.open();

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

  /** barcontrol main
   * @param args command line arguments
   */
  BARControl(String[] args)
  {
    final SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

    try
    {
      // load settings
      Settings.load();

      // parse arguments
      parseArguments(args);

      // commands
      if (   (Settings.runJobName != null)
          || (Settings.abortJobName != null)
          || (Settings.indexDatabaseAddStorageName != null)
          || (Settings.indexDatabaseRemoveStorageName != null)
          || (Settings.indexDatabaseStorageListPattern != null)
          || (Settings.indexDatabaseEntriesListPattern != null)
          || (Settings.pauseTime > 0)
          || (Settings.pingFlag)
          || (Settings.suspendFlag)
          || (Settings.continueFlag)
          || (Settings.listFlag)
          || (Settings.debugQuitServerFlag)
         )
      {
        // connect to server
        try
        {
          BARServer.connect(Settings.serverName,
                            Settings.serverPort,
                            Settings.serverTLSPort,
                            Settings.serverPassword,
                            Settings.serverKeyFileName
                           );
        }
        catch (ConnectionError error)
        {
          printError("cannot connect to server (error: %s)",error.getMessage());
          System.exit(1);
        }

        // execute commands
        if (Settings.runJobName != null)
        {
          // get job id
          int jobId = getJobId(Settings.runJobName);
          if (jobId < 0)
          {
            printError("job '%s' not found",Settings.runJobName);
            BARServer.disconnect();
            System.exit(1);
          }

          // start job
          String[] result = new String[1];
          int errorCode = BARServer.executeCommand("JOB_START "+jobId+" "+Settings.archiveType.toString(),result);
          if (errorCode != Errors.NONE)
          {
            printError("cannot start job '%s' (error: %s)",Settings.runJobName,result[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.indexDatabaseAddStorageName != null)
        {
          // add index for storage
          String[] result = new String[1];
          int errorCode = BARServer.executeCommand("INDEX_STORAGE_ADD "+StringUtils.escape(Settings.indexDatabaseAddStorageName),result);
          if (errorCode != Errors.NONE)
          {
            printError("cannot add index for storage '%s' to index (error: %s)",Settings.indexDatabaseAddStorageName,result[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.indexDatabaseRemoveStorageName != null)
        {
          // remote index for storage
          String[] result = new String[1];
          int errorCode = BARServer.executeCommand("INDEX_STORAGE_REMOVE "+StringUtils.escape(Settings.indexDatabaseRemoveStorageName),result);
          if (errorCode != Errors.NONE)
          {
            printError("cannot remove index for storage '%s' from index (error: %s)",Settings.indexDatabaseRemoveStorageName,result[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.indexDatabaseStorageListPattern != null)
        {
          final String[] MAP_TEXT = new String[]{"\\n","\\r","\\\\"};
          final String[] MAP_BIN  = new String[]{"\n","\r","\\"};

          // list storage index
          Command command = BARServer.runCommand("INDEX_STORAGE_LIST 0 * * "+StringUtils.escape(Settings.indexDatabaseStorageListPattern));
          String   line;
          Object[] data = new Object[8];
          while (!command.endOfData())
          {
            line = command.getNextResult(5*1000);
            if (line != null)
            {
              if      (StringParser.parse(line,"%ld %S %ld %ld %S %S %ld %S",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     id
                     storage name
                     date/time
                     size
                     state
                     mode
                     last checked date/time
                     error message
                */
                String storageName = StringUtils.map((String)data[1],MAP_TEXT,MAP_BIN);
                long   datetime    = (Long)data[2];
                long   size        = (Long)data[3];
                String state       = (String)data[4];
                String mode        = (String)data[5];

                System.out.println(String.format("%12d %5s %5s %s %s",
                                                 size,
                                                 simpleDateFormat.format(new Date(datetime*1000)),
                                                 state,
                                                 mode,
                                                 storageName
                                                )
                                  );

              }
              else if (!line.isEmpty())
              {
                if (Settings.debugFlag)
                {
                  printWarning("unknown server response '%s'",line);
                  BARServer.disconnect();
                  System.exit(1);
                }
              }
            }
          }
          if (command.getErrorCode() != Errors.NONE)
          {
            printError("cannot list storage index (error: %s)",command.getErrorText());
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.indexDatabaseEntriesListPattern != null)
        {
          final String[] MAP_TEXT = new String[]{"\\n","\\r","\\\\"};
          final String[] MAP_BIN  = new String[]{"\n","\r","\\"};

          // list storage index
          Command  command = BARServer.runCommand("INDEX_ENTRIES_LIST 0 * 0 "+StringUtils.escape(Settings.indexDatabaseEntriesListPattern));
          String   line;
          Object[] data = new Object[10];
          while (!command.endOfData())
          {
            line = command.getNextResult(5*1000);
            if (line != null)
            {
              if      (StringParser.parse(line,"FILE %S %ld %S %ld %ld %d %d %d %ld %ld",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     storage name
                     storage date/time
                     file name
                     size
                     date/time
                     user id
                     group id
                     permission
                     fragment offset
                     fragment size
                */
                String storageName    = StringUtils.map((String)data[0],MAP_TEXT,MAP_BIN);
                String fileName       = StringUtils.map((String)data[2],MAP_TEXT,MAP_BIN);
                long   size           = (Long)data[3];
                long   datetime       = (Long)data[4];
                long   fragmentOffset = (Long)data[8];
                long   fragmentSize   = (Long)data[9];

                System.out.println(String.format("%s: FILE %-40s %12d",
                                                 storageName,
                                                 fileName,
                                                 size
                                                )
                                  );
              }
              else if (StringParser.parse(line,"IMAGE %S %ld %S %ld %ld %ld",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     storage name
                     storage date/time
                     name
                     size
                     blockOffset
                     blockCount
                */
                String storageName = StringUtils.map((String)data[0],MAP_TEXT,MAP_BIN);
                String imageName   = StringUtils.map((String)data[2],MAP_TEXT,MAP_BIN);
                long   size        = (Long)data[3];
                long   blockOffset = (Long)data[4];
                long   blockCount  = (Long)data[5];

                System.out.println(String.format("%s: IMAGE %-40s %12d",
                                                 storageName,
                                                 imageName,
                                                 size
                                                )
                                  );
              }
              else if (StringParser.parse(line,"DIRECTORY %S %ld %S %ld %d %d %d",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     storage name
                     storage date/time
                     directory name
                     date/time
                     user id
                     group id
                     permission
                */
                String storageName   = StringUtils.map((String)data[0],MAP_TEXT,MAP_BIN);
                String directoryName = StringUtils.map((String)data[2],MAP_TEXT,MAP_BIN);
                long   datetime      = (Long)data[3];

                System.out.println(String.format("%s: DIR %-40s",
                                                 storageName,
                                                 directoryName
                                                )
                                  );
              }
              else if (StringParser.parse(line,"LINK %S %ld %S %S %ld %d %d %d",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     storage name
                     storage date/time
                     link name
                     destination name
                     date/time
                     user id
                     group id
                     permission
                */
                String storageName     = StringUtils.map((String)data[0],MAP_TEXT,MAP_BIN);
                String linkName        = StringUtils.map((String)data[2],MAP_TEXT,MAP_BIN);
                String destinationName = StringUtils.map((String)data[3],MAP_TEXT,MAP_BIN);
                long   datetime        = (Long)data[4];

                System.out.println(String.format("%s: LINK %s -> %s",
                                                 storageName,
                                                 linkName,
                                                 destinationName
                                                )
                                  );
              }
              else if (StringParser.parse(line,"HARDLINK %S %ld %S %ld %ld %d %d %d %ld %ld",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     storage name
                     storage date/time
                     file name
                     size
                     date/time
                     user id
                     group id
                     permission
                     fragment offset
                     fragment size
                */
                String storageName     = StringUtils.map((String)data[0],MAP_TEXT,MAP_BIN);
                String fileName        = StringUtils.map((String)data[2],MAP_TEXT,MAP_BIN);
                long   size            = (Long)data[3];
                long   datetime        = (Long)data[4];
                long   fragmentOffset  = (Long)data[8];
                long   fragmentSize    = (Long)data[9];

                System.out.println(String.format("%s: HARDLINK %-40s %12d",
                                                 storageName,
                                                 fileName,
                                                 size
                                                )
                                  );
              }
              else if (StringParser.parse(line,"SPECIAL %S %ld %S %ld %d %d %d",data,StringParser.QUOTE_CHARS))
              {
                /* get data
                   format:
                     storage name
                     storage date/time
                     name
                     date/time
                     user id
                     group id
                     permission
                */
                String storageName = StringUtils.map((String)data[0],MAP_TEXT,MAP_BIN);
                String name        = StringUtils.map((String)data[2],MAP_TEXT,MAP_BIN);
                long   datetime    = (Long)data[3];

                System.out.println(String.format("%s: SPECIAL %-40s",
                                                 storageName,
                                                 name
                                                )
                                  );
              }
              else if (!line.isEmpty())
              {
                if (Settings.debugFlag)
                {
                  printWarning("unknown server response '%s'",line);
                  BARServer.disconnect();
                  System.exit(1);
                }
              }
            }
          }
          if (command.getErrorCode() != Errors.NONE)
          {
            printError("cannot list archive files index (error: %s)",command.getErrorText());
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.pauseTime > 0)
        {
          // pause
          String[] result = new String[1];
          int errorCode = BARServer.executeCommand("PAUSE "+Settings.pauseTime,result);
          if (errorCode != Errors.NONE)
          {
            printError("cannot pause (error: %s)",Settings.runJobName,result[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.pingFlag)
        {
        }
        if (Settings.suspendFlag)
        {
          // suspend
          String[] result = new String[1];
          int errorCode = BARServer.executeCommand("SUSPEND",result);
          if (errorCode != Errors.NONE)
          {
            printError("cannot suspend (error: %s)",Settings.runJobName,result[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.continueFlag)
        {
          // continue
          String[] result = new String[1];
          int errorCode = BARServer.executeCommand("CONTINUE",result);
          if (errorCode != Errors.NONE)
          {
            printError("cannot continue (error: %s)",Settings.runJobName,result[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.abortJobName != null)
        {
          // get job id
          int jobId = getJobId(Settings.abortJobName);
          if (jobId < 0)
          {
            printError("job '%s' not found",Settings.abortJobName);
            BARServer.disconnect();
            System.exit(1);
          }

          // abort job
          String[] result = new String[1];
          int errorCode = BARServer.executeCommand("JOB_ABORT "+jobId,result);
          if (errorCode != Errors.NONE)
          {
            printError("cannot abort job '%s' (error: %s)",Settings.abortJobName,result[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.listFlag)
        {
          Object data[] = new Object[12];

          // get server state
          String serverState = null;
          String[] result1 = new String[1];
          if (BARServer.executeCommand("STATUS",result1) != Errors.NONE)
          {
            printError("cannot get state (error: %s)",result1[0]);
            BARServer.disconnect();
            System.exit(1);
          }
          if      (StringParser.parse(result1[0],"running",data,StringParser.QUOTE_CHARS))
          {
          }
          else if (StringParser.parse(result1[0],"pause %ld",data,StringParser.QUOTE_CHARS))
          {
            serverState = "pause";
          }
          else if (StringParser.parse(result1[0],"suspended",data,StringParser.QUOTE_CHARS))
          {
            serverState = "suspended";
          }
          else
          {
            printWarning("unknown server response '%s'",result1[0]);
            BARServer.disconnect();
            System.exit(1);
          }

          // get joblist
          ArrayList<String> result2 = new ArrayList<String>();
          int errorCode = BARServer.executeCommand("JOB_LIST",result2);
          if (errorCode != Errors.NONE)
          {
            printError("cannot get job list (error: %s)",result2.get(0));
            BARServer.disconnect();
            System.exit(1);
          }
          for (String line : result2)
          {
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
              int    id                     = (Integer)data[ 0];
              String name                   = (String) data[ 1];
              String state                  = (String) data[ 2];
              String type                   = (String) data[ 3];
              long   archivePartSize        = (Long)   data[ 4];
              String compressAlgorithms     = (String) data[ 5];
              String cryptAlgorithm         = (String) data[ 6];
              String cryptType              = (String) data[ 7];
              String cryptPasswordMode      = (String) data[ 8];
              Long   lastExecutedDateTime   = (Long)   data[ 9];
              Long   estimatedRestTime      = (Long)   data[10];

              System.out.println(String.format("%2d: %-40s %-10s %-11s %12d %-25s %-12s %-10s %-8s %s %8d",
                                               id,
                                               name,
                                               (serverState == null)?state:serverState,
                                               type,
                                               archivePartSize,
                                               compressAlgorithms,
                                               cryptAlgorithm,
                                               cryptType,
                                               cryptPasswordMode,
                                               simpleDateFormat.format(new Date(lastExecutedDateTime*1000)),
                                               estimatedRestTime
                                              )
                                );
            }
          }
        }
        if (Settings.debugQuitServerFlag)
        {
          // quit server
          String[] result = new String[1];
          if (!BARServer.quit(result))
          {
            printError("cannot quit server");
            BARServer.disconnect();
            System.exit(1);
          }
        }

        // disconnect
        BARServer.disconnect();
      }
      else
      {
        // interactive mode

        // init display
        display = new Display();

        // connect to server
        LoginData loginData = new LoginData(Settings.serverName,Settings.serverPort,Settings.serverTLSPort);
        boolean connectOkFlag = false;
        if (   (loginData.serverName != null)
            && !loginData.serverName.equals("")
            && !Settings.loginDialogFlag
           )
        {
          if (   !connectOkFlag
              && (loginData.password != null)
              && !loginData.password.equals("")
             )
          {
            // try to connect to server with preset data
            try
            {
              BARServer.connect(loginData.serverName,
                                loginData.port,
                                loginData.tlsPort,
                                loginData.password,
                                Settings.serverKeyFileName
                               );
              connectOkFlag = true;
            }
            catch (ConnectionError error)
            {
            }
          }
          if (!connectOkFlag)
          {
            // try to connect to server with empty password
            try
            {
              BARServer.connect(loginData.serverName,
                                loginData.port,
                                loginData.tlsPort,
                                "",
                                Settings.serverKeyFileName
                               );
              connectOkFlag = true;
            }
            catch (ConnectionError error)
            {
            }
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

          // try to connect to server
          try
          {
            BARServer.connect(loginData.serverName,
                              loginData.port,
                              loginData.tlsPort,
                              loginData.password,
                              Settings.serverKeyFileName
                             );
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
        createTabs(Settings.selectedJobName);
        createMenu();

        // run
        run();

        // disconnect
        BARServer.disconnect();

        // save settings
        Settings.save();
      }
    }
    catch (org.eclipse.swt.SWTException exception)
    {
      System.err.println("ERROR graphics: "+exception.getCause());
      if (Settings.debugFlag)
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
      for (StackTraceElement stackTraceElement : error.getStackTrace())
      {
        System.err.println("  "+stackTraceElement);
      }
      System.err.println("");
      System.err.println("Please report this internal error to torsten.rupp@gmx.net.");
    }
    catch (Error error)
    {
      System.err.println("ERROR: "+error.getMessage());
      if (Settings.debugFlag)
      {
        for (StackTraceElement stackTraceElement : error.getStackTrace())
        {
          System.err.println("  "+stackTraceElement);
        }
      }
    }
  }

  /** main
   * @param args command line arguments
   */
  public static void main(String[] args)
  {
    new BARControl(args);
  }
}

/* end of file */
