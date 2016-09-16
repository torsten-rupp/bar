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
import java.io.Console;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.security.KeyStore;
import java.text.DecimalFormat;
import java.text.NumberFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
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
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.Device;
//import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Cursor;
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

import org.xnap.commons.i18n.I18n;
import org.xnap.commons.i18n.I18nFactory;

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
  WEBDAV,
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
    else if (string.equalsIgnoreCase("webdav"))
    {
      type = StorageTypes.WEBDAV;
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
      case WEBDAV:     return "webdav";
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
   *   webdav://<login name>@<host name>/<file name>
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
    else if (archiveName.startsWith("webdav://"))
    {
      // webdav
      type = StorageTypes.WEBDAV;

      String specifier = archiveName.substring(9);
//Dprintf.dprintf("specifier=%s",specifier);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
//Dprintf.dprintf("%s: loginName=%s loginPassword=%s hostName=%s fileName=%s",matcher.group(0),loginName,loginPassword,hostName,fileName);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<host name>/<file name>
        hostName = matcher.group(1);
        fileName = matcher.group(2);
//Dprintf.dprintf("%s: hostName=%s fileName=%s",matcher.group(0),hostName,fileName);
      }
      else
      {
        // webdav://<file name>
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
      case WEBDAV:
        archiveNameBuffer.append("webdav://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("")) { archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); archiveNameBuffer.append('@'); }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
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
      case WEBDAV:
        archiveNameBuffer.append("webdav://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("")) { archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); archiveNameBuffer.append('@'); }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
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
/*
    if      (n >= 1024L*1024L*1024L*1024L) return String.format("%.1f",n/(1024L*1024L*1024L*1024L));
    else if (n >=       1024L*1024L*1024L) return String.format("%.1f",n/(      1024L*1024L*1024L));
    else if (n >=             1024L*1024L) return String.format("%.1f",n/(            1024L*1024L));
    else if (n >=                   1024L) return String.format("%.1f",n/(                  1024L));
    else                                   return String.format("%d"  ,(long)n                    );
*/
    DecimalFormat decimalFormat = new DecimalFormat(".#");

    if      (n >= 1024L*1024L*1024L*1024L) return decimalFormat.format(n/(1024L*1024L*1024L*1024L));
    else if (n >=       1024L*1024L*1024L) return decimalFormat.format(n/(      1024L*1024L*1024L));
    else if (n >=             1024L*1024L) return decimalFormat.format(n/(            1024L*1024L));
    else if (n >=                   1024L) return decimalFormat.format(n/(                  1024L));
    else                                   return String.format("%d",(long)n);
  }

  /** get byte size unit
   * @param n byte value
   * @return unit
   */
  public static String getByteUnit(double n)
  {
    if      (n >= 1024L*1024L*1024L*1024L) return BARControl.tr("TBytes");
    else if (n >=       1024L*1024L*1024L) return BARControl.tr("GBytes");
    else if (n >=             1024L*1024L) return BARControl.tr("MBytes");
    else if (n >=                   1024L) return BARControl.tr("KBytes");
    else                                   return BARControl.tr("bytes");
  }

  /** get byte size short unit
   * @param n byte value
   * @return unit
   */
  public static String getByteShortUnit(double n)
  {
    if      (n >= 1024L*1024L*1024L*1024L) return "T";
    else if (n >=       1024L*1024L*1024L) return "G";
    else if (n >=             1024L*1024L) return "M";
    else if (n >=                   1024L) return "K";
    else                            return "";
  }

  /** parse byte size string
   * @param string string to parse (<n>.<n>(%|B|M|MB|G|GB|TB)
   * @return byte value
   */
  public static long parseByteSize(String string)
    throws NumberFormatException
  {
    string = string.toUpperCase();

    // try to parse with default locale
    try
    {
      if      (string.endsWith("TB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*1024L*1024L*1024L*1024L);
      }
      else if (string.endsWith("T"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*1024L*1024L*1024L*1024L);
      }
      else if (string.endsWith("GB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*1024L*1024L*1024L);
      }
      else if (string.endsWith("G"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*1024L*1024L*1024L);
      }
      else if (string.endsWith("MB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*1024L*1024L);
      }
      else if (string.endsWith("M"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*1024L*1024L);
      }
      else if (string.endsWith("KB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*1024L);
      }
      else if (string.endsWith("K"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*1024L);
      }
      else if (string.endsWith("B"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue());
      }
      else
      {
        return (long)NumberFormat.getInstance().parse(string).doubleValue();
      }
    }
    catch (ParseException exception)
    {
      throw new NumberFormatException(exception.getMessage());
    }
  }

  /** parse byte size string
   * @param string string to parse (<n>(%|B|M|MB|G|GB|TB)
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

  /** get time string
   * @param n time
   * @return string
   */
  public static String getTime(double n)
  {
    if      (((long)n % (7L*24L*60L*60L)) == 0) return String.format("%d",(long)(n/(7L*24L*60L*60L)));
    else if (((long)n % (   24L*60L*60L)) == 0) return String.format("%d",(long)(n/(   24L*60L*60L)));
    else if (((long)n % (       60L*60L)) == 0) return String.format("%d",(long)(n/(       60L*60L)));
    else if (((long)n % (           60L)) == 0) return String.format("%d",(long)(n/(           60L)));
    else                                        return String.format("%d",(long)n                   );
  }

  /** get time unit
   * @param n time
   * @return unit
   */
  public static String getTimeUnit(double n)
  {
    if      (((long)n % (7L*24L*60L*60L)) == 0) return BARControl.tr("weeks");
    else if (((long)n % (   24L*60L*60L)) == 0) return BARControl.tr("days");
    else if (((long)n % (       60L*60L)) == 0) return BARControl.tr("h");
    else if (((long)n % (           60L)) == 0) return BARControl.tr("min");
    else                                        return BARControl.tr("s");
  }

  /** parse byte size string
   * @param string string to parse (<n>.<n>(weeks|days|h|min|s)
   * @return time
   */
  public static long parseTime(String string)
    throws NumberFormatException
  {
    string = string.toUpperCase();

    // try to parse with default locale
    try
    {
      if       (string.endsWith("WEEK"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-4)).doubleValue()*7L*24L*60L*60L);
      }
      if       (string.endsWith("WEEKS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-5)).doubleValue()*7L*24L*60L*60L);
      }
      else if (string.endsWith("DAY"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-3)).doubleValue()*24L*60L*60L);
      }
      else if (string.endsWith("DAYS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-4)).doubleValue()*24L*60L*60L);
      }
      else if (string.endsWith("H"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*60L*60L);
      }
      else if (string.endsWith("HOUR"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-4)).doubleValue()*60L*60L);
      }
      else if (string.endsWith("HOURS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-5)).doubleValue()*60L*60L);
      }
      else if (string.endsWith("M"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*60L*60L);
      }
      else if (string.endsWith("MIN"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-3)).doubleValue()*60L);
      }
      else if (string.endsWith("MINS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-4)).doubleValue()*60L);
      }
      else if (string.endsWith("S"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue());
      }
      else if (string.endsWith("SECOND"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-6)).doubleValue());
      }
      else if (string.endsWith("SECONDS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-7)).doubleValue());
      }
      else
      {
        return (long)NumberFormat.getInstance().parse(string).doubleValue();
      }
    }
    catch (ParseException exception)
    {
      throw new NumberFormatException(exception.getMessage());
    }
  }

  /** parse to,e string
   * @param string string to parse (<n>.<n>(weeks|days|h|min|s)
   * @param defaultValue default value if number cannot be parsed
   * @return time
   */
  public static long parseTime(String string, long defaultValue)
  {
    long n;

    try
    {
      n = Units.parseTime(string);
    }
    catch (NumberFormatException exception)
    {
      n = defaultValue;
    }

    return n;
  }

  /** format time
   * @param n time
   * @return string with unit
   */
  public static String formatTime(long n)
  {
    return getTime(n)+getTimeUnit(n);
  }
}

/** actions
 */
enum Actions
{
  NONE,
  REQUEST_PASSWORD,
  REQUEST_VOLUME,
  CONFIRM;
};

/** password types
 */
enum PasswordTypes
{
  NONE,
  FTP,
  SSH,
  WEBDAV,
  CRYPT;

  /** check if login password
   * @return true iff login password
   */
  public boolean isLogin()
  {
    return (this == FTP) || (this == SSH) || (this == WEBDAV);
  }

  /** check if crypt password
   * @return true iff crypt password
   */
  public boolean isCrypt()
  {
    return (this == CRYPT);
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    switch (this)
    {
      case NONE:   return "none";
      case FTP:    return "FTP";
      case SSH:    return "SSH";
      case WEBDAV: return "WebDAV";
      case CRYPT:  return "encryption";
    }

    return "";
  }
};

/** BARControl
 */
public class BARControl
{
  /** login data
   */
  class LoginData
  {
    String serverName;       // server name
    int    serverPort;       // server port
    int    serverTLSPort;    // server TLS port
    String password;         // login password

    /** create login data
     * @param serverName server name
     * @param port server port
     * @param tlsPort server TLS port
     * @param password server password
     */
    LoginData(String name, int port, int tlsPort, String password)
    {
      final Settings.Server defaultServer = Settings.getLastServer();

      this.serverName    = !name.isEmpty()     ? name     : ((defaultServer != null) ? defaultServer.name : Settings.DEFAULT_SERVER_NAME);
      this.serverPort    = (port != 0        ) ? port     : ((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT);
      this.serverTLSPort = (port != 0        ) ? tlsPort  : ((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT);
      this.password      = !password.isEmpty() ? password : "";

      // get last used server if no name given
//TODO
//      if (this.serverName.isEmpty() && (Settings.servers.size() > 0))
//      {
//        this.serverName = Settings.serverNames.toArray(new String[Settings.serverNames.size()])[Settings.serverNames.size()-1];
//      }
    }

    /** create login data
     * @param serverName server name
     * @param port server port
     * @param tlsPort server TLS port
     */
    LoginData(String name, int port, int tlsPort)
    {
      this(name,port,tlsPort,"");
    }

    /** create login data
     * @param port server port
     * @param tlsPort server TLS port
     */
    LoginData(int port, int tlsPort)
    {
      this("",port,tlsPort);
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return "LoginData {"+serverName+", "+serverPort+", "+serverTLSPort+"}";
    }
  }

  // --------------------------- constants --------------------------------

  /** host system
   */
  public enum HostSystems
  {
    UNKNOWN,
    LINUX,
    SOLARIS,
    WINDOWS,
    MACOS;
  };

  /** index states
   */
  enum IndexStates
  {
    NONE,
    OK,
    CREATE,
    UPDATE_REQUESTED,
    UPDATE,
    ERROR,
    UNKNOWN;
  };

  /** index modes
   */
  enum IndexModes
  {
    MANUAL,
    AUTO,
    UNKNOWN;
  };

  /** entry types
   */
  enum EntryTypes
  {
    FILE,
    IMAGE,
    DIRECTORY,
    LINK,
    HARDLINK,
    SPECIAL,
    DEVICE,
    SOCKET;
  };

  // user events
  final static int USER_EVENT_NEW_SERVER = 0xFFFF+0;
  final static int USER_EVENT_NEW_JOB    = 0xFFFF+1;


  // string with "all files" extension
  public static final String ALL_FILE_EXTENSION;

  private static final HostSystems hostSystem;

  // command line options
  private static final OptionEnumeration[] ARCHIVE_TYPE_ENUMERATION =
  {
    new OptionEnumeration("normal",      Settings.ArchiveTypes.NORMAL),
    new OptionEnumeration("full",        Settings.ArchiveTypes.FULL),
    new OptionEnumeration("incremental", Settings.ArchiveTypes.INCREMENTAL),
    new OptionEnumeration("differential",Settings.ArchiveTypes.DIFFERENTIAL),
  };

  private static final Option[] OPTIONS =
  {
    new Option("--password",                   null,Options.Types.STRING,     "serverPassword"),
    new Option("--port",                       "-p",Options.Types.INTEGER,    "serverPort"),
    new Option("--tls-port",                   null,Options.Types.INTEGER,    "serverTLSPort"),
    new Option("--key-file",                   null,Options.Types.STRING,     "serverKeyFileName"),
    new Option("--force-ssl",                  null,Options.Types.BOOLEAN,    "serverForceSSL"),
    new Option("--select-job",                 null,Options.Types.STRING,     "selectedJobName"),
    new Option("--login-dialog",               null,Options.Types.BOOLEAN,    "loginDialogFlag"),

    new Option("--job",                        "-j",Options.Types.STRING,     "runJobName"),
    new Option("--archive-type",               null,Options.Types.ENUMERATION,"archiveType",ARCHIVE_TYPE_ENUMERATION),
    new Option("--abort",                      null,Options.Types.STRING,     "abortJobName"),
    new Option("--pause",                      "-t",Options.Types.INTEGER,    "pauseTime"),
    new Option("--ping",                       "-i",Options.Types.BOOLEAN,    "pingFlag"),
    new Option("--suspend",                    "-s",Options.Types.BOOLEAN,    "suspendFlag"),
    new Option("--continue",                   "-c",Options.Types.BOOLEAN,    "continueFlag"),
    new Option("--list",                       "-l",Options.Types.BOOLEAN,    "listFlag"),

    new Option("--index-database-add",         null,Options.Types.STRING,     "indexDatabaseAddStorageName"),
    new Option("--index-database-remove",      null,Options.Types.STRING,     "indexDatabaseRemoveStorageName"),
    new Option("--index-database-refresh",     null,Options.Types.STRING,     "indexDatabaseRefreshStorageName"),
    new Option("--index-database-storage-list","-a",Options.Types.STRING,     "indexDatabaseStorageListPattern"),
    new Option("--index-database-entries-list","-e",Options.Types.STRING,     "indexDatabaseEntriesListName"),

    new Option("--restore",                    null,Options.Types.STRING,     "restoreStorageName"),
    new Option("--destination",                null,Options.Types.STRING,     "destination"),
    new Option("--overwrite-entries",          null,Options.Types.BOOLEAN,    "overwriteEntriesFlag"),

    new Option("--version",                    null,Options.Types.BOOLEAN,    "versionFlag"),
    new Option("--help",                       "-h",Options.Types.BOOLEAN,    "helpFlag"),
    new Option("--xhelp",                      null,Options.Types.BOOLEAN,    "xhelpFlag"),

    new Option("--debug",                      "-d",Options.Types.INCREMENT,  "debugLevel"),
    new Option("--debug-quit-server",          null,Options.Types.BOOLEAN,    "debugQuitServerFlag"),

    // ignored
    new Option("--swing",                      null, Options.Types.BOOLEAN,   null),
  };

  // --------------------------- variables --------------------------------
  private static I18n    i18n;
  private static Display display;
  private static Shell   shell;
  private static Cursor  waitCursor;
  private static int     waitCursorCount = 0;

  private LoginData       loginData;
  private Menu            serverMenu;
  private TabFolder       tabFolder;
  private TabStatus       tabStatus;
  private TabJobs         tabJobs;
  private TabRestore      tabRestore;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  static
  {
    // detect host system
    String osName = System.getProperty("os.name").toLowerCase();

    if      (osName.indexOf("linux")   >= 0) hostSystem = HostSystems.LINUX;
    else if (osName.indexOf("solaris") >= 0) hostSystem = HostSystems.SOLARIS;
    else if (osName.indexOf("mac")     >= 0) hostSystem = HostSystems.MACOS;
    else if (osName.indexOf("win")     >= 0) hostSystem = HostSystems.WINDOWS;
    else                                     hostSystem = HostSystems.LINUX;

    // get all-files extension
//TODO: system dependent?
//    ALL_FILE_EXTENSION = (hostSystem == HostSystems.WINDOWS) ? "*.*" : "*";
    ALL_FILE_EXTENSION = "*";
  }

  /** get internationalized text
   * @param text text
   * @param arguments text
   * @return internationalized text
   */
  static String tr(String text, Object... arguments)
  {
    return i18n.tr(text,arguments);
  }

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

  /** print stack trace
   * @param throwable throwable
   */
  public static void printStackTrace(Throwable throwable)
  {
    for (StackTraceElement stackTraceElement : throwable.getStackTrace())
    {
      System.err.println("  "+stackTraceElement);
    }
    Throwable cause = throwable.getCause();
    while (cause != null)
    {
      System.err.println("Caused by:");
      for (StackTraceElement stackTraceElement : cause.getStackTrace())
      {
        System.err.println("  "+stackTraceElement);
      }
      cause = cause.getCause();
    }
  }

  /** renice i/o exception (remove java.io.IOExcpetion text from exception)
   * @param exception i/o exception to renice
   * @return reniced exception
   */
  public static IOException reniceIOException(IOException exception)
  {
    final Pattern PATTERN1 = Pattern.compile("^(.*?)\\s*java.io.IOException:\\s*error=\\d+,\\s*(.*)$",Pattern.CASE_INSENSITIVE);
    final Pattern PATTERN2 = Pattern.compile("^.*\\.SunCertPathBuilderException:\\s*(.*)$",Pattern.CASE_INSENSITIVE);

    Matcher matcher;
    if      ((matcher = PATTERN1.matcher(exception.getMessage())).matches())
    {
      exception = new IOException(matcher.group(1)+" "+matcher.group(2),exception);
    }
    else if ((matcher = PATTERN2.matcher(exception.getMessage())).matches())
    {
      exception = new IOException(matcher.group(1),exception);
    }

    return exception;
  }

  /** set wait cursor
   * @param shell shell
   */
  public static void waitCursor(Shell shell)
  {
    if (!shell.isDisposed())
    {
      shell.setCursor(waitCursor);
    }
    waitCursorCount++;
  }

  /** reset wait cursor
   * @param shell shell
   */
  public static void resetCursor(Shell shell)
  {
    assert waitCursorCount > 0;

    waitCursorCount--;
    if (waitCursorCount <= 0)
    {
      if (!shell.isDisposed())
      {
        shell.setCursor(null);
      }
    }
  }

  /** set wait cursor
   */
  public static void waitCursor()
  {
    waitCursor(shell);
  }

  /** reset wait cursor
   */
  public static void resetCursor()
  {
    resetCursor(shell);
  }

  // ----------------------------------------------------------------------

  /** print program usage
   */
  private void printUsage()
  {
    System.out.println("barcontrol usage: <options> [--] [<host name>]");
    System.out.println("");
    System.out.println("Options: -p|--port=<n>                              - server port (default: "+Settings.DEFAULT_SERVER_PORT+")");
    System.out.println("         --tls-port=<n>                             - TLS server port (default: "+Settings.DEFAULT_SERVER_TLS_PORT+")");
    System.out.println("         --password=<password>                      - server password (use with care!)");
    System.out.println("         --select-job=<name>                        - select job <name>");
    System.out.println("         --login-dialog                             - force to open login dialog");
    System.out.println("         --key-file=<file name>                     - key file name (default: ");
    System.out.println("                                                        ."+File.separator+BARServer.JAVA_SSL_KEY_FILE_NAME+" or ");
    System.out.println("                                                        "+System.getProperty("user.home")+File.separator+".bar"+File.separator+BARServer.JAVA_SSL_KEY_FILE_NAME+" or ");
    System.out.println("                                                        "+Config.CONFIG_DIR+File.separator+BARServer.JAVA_SSL_KEY_FILE_NAME);
    System.out.println("                                                      )" );
    System.out.println("         --force-ssl                                - force SSL connection");
    System.out.println("");
    System.out.println("         --select-job=<name>                        - select job <name>");
    System.out.println("         -j|--job=<name>                            - start execution of job <name>");
    System.out.println("         --archive-type=<mode>                      - archive type");
    System.out.println("                                                        normal (default)");
    System.out.println("                                                        full");
    System.out.println("                                                        incremental");
    System.out.println("                                                        differential");
    System.out.println("         --abort=<name>                             - abort execution of job <name>");
    System.out.println("         -t|--pause=<n>                             - pause job execution for <n> seconds");
    System.out.println("         -i|--ping                                  - check connection to server");
    System.out.println("         -s|--suspend                               - suspend job execution");
    System.out.println("         -c|--continue                              - continue job execution");
    System.out.println("         -l|--list                                  - list jobs");
    System.out.println("");
    System.out.println("         --index-database-add=<name|directory>      - add storage archive <name> or all .bar files to index");
    System.out.println("         --index-database-remove=<text>             - remove matching storage archives from index");
    System.out.println("         --index-database-refresh=<text>            - refresh matching storage archive in index");
    System.out.println("         -a|--index-database-storage-list=<text>    - list matching storage archives");
    System.out.println("         -e|--index-database-entries-list=<text>    - list matching entries");
    System.out.println("");
    System.out.println("         --restore=<name>                           - restore storage <name>");
    System.out.println("         --destination=<directory>                  - destination to restore entries");
    System.out.println("         --overwrite-entries                        - overwrite existing entries on restore");
    System.out.println("");
    System.out.println("         --version                                  - output version");
    System.out.println("         -h|--help                                  - print this help");
    if (Settings.xhelpFlag)
    {
      System.out.println("");
      System.out.println("         -d|--debug                                 - enable debug mode");
      System.out.println("         --debug-quit-server                        - send quit-command to server");
    }
  }

  /** print program version
   */
  private void printVersion()
  {
    System.out.println("barcontrol "+Config.VERSION_MAJOR+"."+Config.VERSION_MINOR+" ("+Config.VERSION_SVN+")");
  }

  /** parse arguments
   * @param args arguments
   */
  private void parseArguments(String[] args)
  {
    // parse arguments
    int     z = 0;
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
        int i = Options.parse(OPTIONS,args,z,Settings.class);
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
    if (Settings.helpFlag || Settings.xhelpFlag)
    {
      printUsage();
      System.exit(0);
    }

    // version
    if (Settings.versionFlag)
    {
      printVersion();
      System.exit(0);
    }

    // add/update server
//        Settings.serverNames.remove(args[z]);
//        Settings.serverNames.add(args[z]);

    // check arguments
    if ((Settings.serverKeyFileName != null) && !Settings.serverKeyFileName.isEmpty())
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

  /** get job UUID
   * @param jobName job name
   * @return job UUID or null if not found
   */
  private static String getJobUUID(String jobName)
  {
    String[]            errorMessage  = new String[1];
    ArrayList<ValueMap> valueMapList = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("JOB_LIST"),
                                         0,
                                         errorMessage,
                                         valueMapList
                                        );
    if (error == Errors.NONE)
    {
      for (ValueMap valueMap : valueMapList)
      {
        String jobUUID = valueMap.getString("jobUUID");
        String name    = valueMap.getString("name" );

        if (jobName.equalsIgnoreCase(name))
        {
          return jobUUID;
        }
      }
    }
    else
    {
      if (Settings.debugLevel > 0)
      {
        printError("cannot get job list (error: %s)",errorMessage[0]);
        BARServer.disconnect();
        System.exit(1);
      }
    }

    return null;
  }

  /** login server/password dialog
   * @param loginData server login data
   * @return true iff login data ok, false otherwise
   */
  private boolean getLoginData(final LoginData loginData)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite,subComposite;
    Label           label;
    Button          button;

    final Shell dialog = Dialogs.openModal(new Shell(),BARControl.tr("Login BAR server"),250,SWT.DEFAULT);

    // get sorted servers
    final Settings.Server servers[] = Settings.servers.toArray(new Settings.Server[Settings.servers.size()]);
    Arrays.sort(servers,new Comparator<Settings.Server>()
    {
      public int compare(Settings.Server server1, Settings.Server server2)
      {
        return server1.getData().compareTo(server2.getData());
      }
    });

    // get server data
    String serverData[] = new String[servers.length];
    for (int i = 0; i < servers.length; i++)
    {
      serverData[i] = servers[i].getData();
    }

    // password
    final Combo   widgetServerName;
    final Spinner widgetServerPort;
    final Text    widgetPassword;
    final Button  widgetLoginButton;
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},2));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
    {
      label = new Label(composite,SWT.LEFT);
      label.setText(BARControl.tr("Server")+":");
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W));

      subComposite = new Composite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(null,new double[]{1.0,0.0},2));
      subComposite.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE));
      {
        widgetServerName = new Combo(subComposite,SWT.LEFT|SWT.BORDER);
        widgetServerName.setItems(serverData);
        if (loginData.serverName != null) widgetServerName.setText(loginData.serverName);
        widgetServerName.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));

        widgetServerPort = new Spinner(subComposite,SWT.RIGHT|SWT.BORDER);
        widgetServerPort.setMinimum(0);
        widgetServerPort.setMaximum(65535);
        widgetServerPort.setSelection(loginData.serverPort);
        widgetServerPort.setLayoutData(new TableLayoutData(0,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT));
      }

      label = new Label(composite,SWT.LEFT);
      label.setText(BARControl.tr("Password")+":");
      label.setLayoutData(new TableLayoutData(1,0,TableLayoutData.W));

      widgetPassword = new Text(composite,SWT.LEFT|SWT.BORDER|SWT.PASSWORD);
      if ((loginData.password != null) && !loginData.password.isEmpty()) widgetPassword.setText(loginData.password);
      widgetPassword.setLayoutData(new TableLayoutData(1,1,TableLayoutData.WE));
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE));
    {
      widgetLoginButton = new Button(composite,SWT.CENTER);
      widgetLoginButton.setText(BARControl.tr("Login"));
      widgetLoginButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT));
      widgetLoginButton.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          loginData.serverName = widgetServerName.getText();
          loginData.serverPort = widgetServerPort.getSelection();
          loginData.password   = widgetPassword.getText();
          Dialogs.close(dialog,true);
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText(BARControl.tr("Cancel"));
      button.setLayoutData(new TableLayoutData(0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT));
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
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
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetPassword.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Settings.Server server = servers[widgetServerName.getSelectionIndex()];
        widgetServerName.setText((server.name != null) ? server.name : "");
        widgetServerPort.setSelection(server.port);
        widgetPassword.setText(((server.password != null) && !server.password.isEmpty()) ? server.password : "");
      }
    });
    widgetPassword.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetLoginButton.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
      }
    });

    Widgets.setNextFocus(widgetServerName,
                         widgetServerPort,
                         widgetPassword,
                         widgetLoginButton
                        );
    if ((loginData.serverName != null) && (loginData.serverName.length() != 0))
    {
      widgetPassword.forceFocus();
    }
    else
    {
      widgetServerName.forceFocus();
    }
    Boolean result = (Boolean)Dialogs.run(dialog);
    if ((result != null) && result && ((loginData.serverPort != 0) || (loginData.serverTLSPort != 0)))
    {
      // store new name+port, shorten list
      Settings.addServer(loginData.serverName,
                         (loginData.serverPort != 0) ? loginData.serverPort : loginData.serverTLSPort,
                         loginData.password
                        );

      return true;
    }
    else
    {
      return false;
    }
  }

  /** create main window
   */
  private void createWindow()
  {
    // create shell window
    shell = new Shell(display);
    if (BARServer.getInfo() != null)
    {
      shell.setText("BAR control: "+BARServer.getInfo());
    }
    else
    {
      shell.setText("BAR control");
    }
    shell.setLayout(new TableLayout(1.0,1.0));

    // get cursors
    waitCursor = new Cursor(display,SWT.CURSOR_WAIT);
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

    // start auto update
    tabStatus.startUpdate();

    // pre-select job
    if (selectedJobName != null)
    {
      tabStatus.selectJob(selectedJobName);
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

  /** update server menu entries
   */
  private void updateServerMenu()
  {
    // clear menu
    while (serverMenu.getItemCount() > 2)
    {
      serverMenu.getItem(2).dispose();
    }

    // get sorted servers
    Settings.Server servers[] = Settings.servers.toArray(new Settings.Server[Settings.servers.size()]);
    Arrays.sort(servers,new Comparator<Settings.Server>()
    {
      public int compare(Settings.Server server1, Settings.Server server2)
      {
        return server1.getData().compareTo(server2.getData());
      }
    });

    // create server menu items
    for (Settings.Server server : servers)
    {
      MenuItem menuItem = Widgets.addMenuRadio(serverMenu,server.name+":"+server.port);
      menuItem.setData(server);
      menuItem.setSelection(server.name.equals(BARServer.getName()) && (server.port == BARServer.getPort()));
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem        menuItem = (MenuItem)selectionEvent.widget;
          Settings.Server server   = (Settings.Server)menuItem.getData();

          if (menuItem.getSelection())
          {
            boolean connectOkFlag = false;

            // try to connect to server with current credentials
            if (!connectOkFlag)
            {
              loginData = new LoginData((!server.name.isEmpty()    ) ? server.name     : Settings.DEFAULT_SERVER_NAME,
                                        (server.port != 0          ) ? server.port     : Settings.DEFAULT_SERVER_PORT,
                                        (server.port != 0          ) ? server.port     : Settings.DEFAULT_SERVER_PORT,
                                        (!server.password.isEmpty()) ? server.password : ""
                                       );
              try
              {
                BARServer.connect(loginData.serverName,
                                  loginData.serverPort,
                                  loginData.serverTLSPort,
                                  loginData.password,
                                  Settings.serverKeyFileName
                                 );
                shell.setText("BAR control: "+BARServer.getInfo());
                Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);

                connectOkFlag = true;
              }
              catch (ConnectionError error)
              {
                Dialogs.error(new Shell(),BARControl.tr("Connection fail")+":\n\n"+error.getMessage());
              }
              catch (CommunicationError error)
              {
                Dialogs.error(new Shell(),BARControl.tr("Connection fail")+":\n\n"+error.getMessage());
              }
            }

            // try to connect to server with new credentials
            if (!connectOkFlag)
            {
              loginData = new LoginData((!server.name.isEmpty()) ? server.name : Settings.DEFAULT_SERVER_NAME,
                                        (server.port != 0      ) ? server.port : Settings.DEFAULT_SERVER_PORT,
                                        (server.port != 0      ) ? server.port : Settings.DEFAULT_SERVER_PORT
                                       );
              if (getLoginData(loginData))
              {
                try
                {
                  BARServer.connect(loginData.serverName,
                                    loginData.serverPort,
                                    loginData.serverTLSPort,
                                    loginData.password,
                                    Settings.serverKeyFileName
                                   );
                  shell.setText("BAR control: "+BARServer.getInfo());
                  Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);

                  connectOkFlag = true;
                }
                catch (ConnectionError error)
                {
                  Dialogs.error(new Shell(),BARControl.tr("Connection fail")+":\n\n"+error.getMessage());
                }
                catch (CommunicationError error)
                {
                  Dialogs.error(new Shell(),BARControl.tr("Connection fail")+":\n\n"+error.getMessage());
                }
              }
            }

            updateServerMenu();
          }
        }
      });
    }
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

    menu = Widgets.addMenu(menuBar,BARControl.tr("Program"));
    {
      serverMenu = Widgets.addMenu(menu,BARControl.tr("Connect"));
      {
        menuItem = Widgets.addMenuItem(serverMenu,"\u2026",SWT.CTRL+'O');
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            final Settings.Server defaultServer = Settings.getLastServer();
            loginData = new LoginData((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT,
                                      (defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT
                                     );
            if (getLoginData(loginData))
            {
              try
              {
                BARServer.connect(loginData.serverName,
                                  loginData.serverPort,
                                  loginData.serverTLSPort,
                                  loginData.password,
                                  Settings.serverKeyFileName
                                 );
                shell.setText("BAR control: "+BARServer.getInfo());

                Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);
              }
              catch (ConnectionError error)
              {
                Dialogs.error(new Shell(),BARControl.tr("Connection fail")+":\n\n"+error.getMessage());
              }
              catch (CommunicationError error)
              {
                Dialogs.error(new Shell(),BARControl.tr("Connection fail")+":\n\n"+error.getMessage());
              }

              updateServerMenu();
            }
          }
        });

        Widgets.addMenuSeparator(serverMenu);

        updateServerMenu();
      }

      Widgets.addMenuSeparator(menu);

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Start")+"\u2026",SWT.CTRL+'S');
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Widgets.notify(tabStatus.widgetButtonStart);
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Abort")+"\u2026",SWT.CTRL+'A');
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Widgets.notify(tabStatus.widgetButtonAbort);
        }
      });

      subMenu = Widgets.addMenu(menu,BARControl.tr("Pause"));
      {
        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("10min"));
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            tabStatus.jobPause(10*60);
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("60min"),SWT.CTRL+'P');
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            tabStatus.jobPause(60*60);
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("120min"));
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            tabStatus.jobPause(120*60);
          }
        });

        Widgets.addMenuSeparator(subMenu);

        menuItem = Widgets.addMenuCheckbox(subMenu,BARControl.tr("Create operation"),Settings.pauseCreateFlag);
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            Settings.pauseCreateFlag = widget.getSelection();
            tabStatus.jobPause(60*60);
          }
        });

        menuItem = Widgets.addMenuCheckbox(subMenu,BARControl.tr("Storage operation"),Settings.pauseStorageFlag);
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            Settings.pauseStorageFlag = widget.getSelection();
            tabStatus.jobPause(60*60);
          }
        });

        menuItem = Widgets.addMenuCheckbox(subMenu,BARControl.tr("Restore operation"),Settings.pauseRestoreFlag);
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            Settings.pauseRestoreFlag = widget.getSelection();
            tabStatus.jobPause(60*60);
          }
        });
        menuItem = Widgets.addMenuCheckbox(subMenu,BARControl.tr("Index update operation"),Settings.pauseIndexUpdateFlag);
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            Settings.pauseIndexUpdateFlag = widget.getSelection();
          }
        });
      }

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Toggle suspend/continue"),SWT.CTRL+'T');
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Widgets.notify(tabStatus.widgetButtonSuspendContinue);
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clear stored passwords on server"),SWT.NONE);
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String[] errorMessage = new String[1];
          int error = BARServer.executeCommand(StringParser.format("PASSWORDS_CLEAR"),0,errorMessage);
          if (error != Errors.NONE)
          {
            Dialogs.error(shell,BARControl.tr("Cannot clear passwords on server:\n\n")+errorMessage[0]);
          }
        }
      });

      Widgets.addMenuSeparator(menu);

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Server settings")+"\u2026",SWT.CTRL+'W');
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          ServerSettings.serverSettings(shell);
        }
      });

      Widgets.addMenuSeparator(menu);

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Quit"),SWT.CTRL+'Q');
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Widgets.notify(tabStatus.widgetButtonQuit);
        }
      });
    }

    menu = Widgets.addMenu(menuBar,BARControl.tr("Help"));
    {
      menuItem = Widgets.addMenuItem(menu,BARControl.tr("About")+"\u2026");
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.info(shell,
                       BARControl.tr("About"),
                       "BAR control "+Config.VERSION_MAJOR+"."+Config.VERSION_MINOR+".\n\n"+BARControl.tr("Written by Torsten Rupp")+"\n"
                      );
        }
      });
    }

    if (Settings.debugLevel > 0)
    {
      menu = Widgets.addMenu(menuBar,"Debug");
      {
        menuItem = Widgets.addMenuItem(menu,"Print debug statistics");
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            BARServer.executeCommand(StringParser.format("DEBUG_PRINT_STATISTICS"),0);
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Print debug memory info");
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;

            final BusyDialog busyDialog = new BusyDialog(shell,"Print debug memory dump",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
            new BackgroundTask(busyDialog)
            {
              @Override
              public void run(final BusyDialog busyDialog, Object userData)
              {
                // dump memory info
                BARServer.executeCommand(StringParser.format("DEBUG_PRINT_MEMORY_INFO"),
                                         0,  // debugLevel
                                         null,  // errorMessage
                                         new Command.ResultHandler()
                                         {
                                           public int handle(int i, ValueMap valueMap)
                                           {
                                             String type  = valueMap.getString("type");
                                             long   n     = valueMap.getLong("n");
                                             long   count = valueMap.getLong("count");

                                             busyDialog.setMaximum(count);
                                             busyDialog.updateText(String.format("Printing '%s' info...",type));
                                             busyDialog.updateProgressBar(n);

                                             if (busyDialog.isAborted())
                                             {
                                               abort();
                                             }

                                             return Errors.NONE;
                                           }
                                         }
                                        );
                // close busy dialog
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    busyDialog.close();
                  }
                });
              }
            };
          }
        });

        menuItem = Widgets.addMenuItem(menu,"Dump debug memory info");
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;

            final BusyDialog busyDialog = new BusyDialog(shell,"Store debug memory dump",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0);
            new BackgroundTask(busyDialog)
            {
              @Override
              public void run(final BusyDialog busyDialog, Object userData)
              {
                // dump memory info
                BARServer.executeCommand(StringParser.format("DEBUG_DUMP_MEMORY_INFO"),
                                         0,  // debugLevel
                                         null,  // errorMessage
                                         new Command.ResultHandler()
                                         {
                                           public int handle(int i, ValueMap valueMap)
                                           {
                                             String type  = valueMap.getString("type");
                                             long   n     = valueMap.getLong("n");
                                             long   count = valueMap.getLong("count");

                                             busyDialog.setMaximum(count);
                                             busyDialog.updateText(String.format("Dumping '%s' info...",type));
                                             busyDialog.updateProgressBar(n);

                                             if (busyDialog.isAborted())
                                             {
                                               abort();
                                             }

                                             return Errors.NONE;
                                           }
                                         }
                                        );
                // close busy dialog
                display.syncExec(new Runnable()
                {
                  @Override
                  public void run()
                  {
                    busyDialog.close();
                  }
                });
              }
            };
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
    shell.setSize(840,600+5*(Widgets.getTextHeight(shell)+4));

    // add close listener
    shell.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        shell.dispose();
      }
    });

    // SWT event loop
    boolean connectOkFlag = true;
    while (!shell.isDisposed() && connectOkFlag)
    {
//System.err.print(".");
      try
      {
        if (!display.readAndDispatch())
        {
          display.sleep();
        }
      }
      catch (ConnectionError error)
      {
        // disconnected -> try to reconnect
        connectOkFlag = false;

        // try to reconnect
        if (Dialogs.confirmError(new Shell(),BARControl.tr("Connection lost"),BARControl.tr("Error: ")+error.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
        {
          // try to connect to last server
          while (!connectOkFlag)
          {
            try
            {
              BARServer.connect(loginData.serverName,
                                loginData.serverPort,
                                loginData.serverTLSPort,
                                loginData.password,
                                Settings.serverKeyFileName
                               );
              shell.setText("BAR control: "+BARServer.getInfo());
              Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);

              connectOkFlag = true;
            }
            catch (ConnectionError reconnectError)
            {
              if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+reconnectError.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
              {
                break;
              }
            }
            catch (CommunicationError reconnectError)
            {
              if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+reconnectError.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
              {
                break;
              }
            }
          }

          // try to connect to new server
          while (   !connectOkFlag
                 && getLoginData(loginData)
                 && ((loginData.serverPort != 0) || (loginData.serverTLSPort != 0))
                )
          {
            // try to connect to server
            try
            {
              BARServer.connect(loginData.serverName,
                                loginData.serverPort,
                                loginData.serverTLSPort,
                                loginData.password,
                                Settings.serverKeyFileName
                               );
              shell.setText("BAR control: "+BARServer.getInfo());
              Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);

              connectOkFlag = true;
            }
            catch (ConnectionError reconnectError)
            {
              if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+reconnectError.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
              {
                break;
              }
            }
            catch (CommunicationError reconnectError)
            {
              if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+reconnectError.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
              {
                break;
              }
            }
          }
        }

        // stop if not connected
        if (!connectOkFlag)
        {
          break;
        }
      }
      catch (Throwable throwable)
      {
        if (Settings.debugLevel > 0)
        {
          System.err.println("ERROR: "+throwable.getMessage());
          printStackTrace(throwable);
        }
        Dialogs.error(new Shell(),BARControl.tr("Internal error"),BARControl.tr("Error: ")+throwable.getMessage(),BARControl.tr("Abort"));
        break;
      }
    }
  }

  /** barcontrol main
   * @param args command line arguments
   */
  BARControl(String[] args)
  {
    final SimpleDateFormat DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
    final char             MAIL_AT     = '@';

    Thread.currentThread().setName("BARControl");

    // init localization
    i18n = I18nFactory.getI18n(getClass(),"app.i18n.Messages",Locale.getDefault(),I18nFactory.FALLBACK);
    Dialogs.init(i18n);
    BusyDialog.init(i18n);

    try
    {
      // load settings
      Settings.load();

      // parse arguments
      parseArguments(args);

      // server login data
      Settings.Server server = null;
      if ((server == null)) server = Settings.getServer(Settings.serverName,(Settings.serverPort    != -1) ? Settings.serverPort    : Settings.DEFAULT_SERVER_PORT);
      if ((server == null)) server = Settings.getServer(Settings.serverName,(Settings.serverTLSPort != -1) ? Settings.serverTLSPort : Settings.DEFAULT_SERVER_PORT);
      loginData = new LoginData((server != null) ? server.name     : Settings.DEFAULT_SERVER_NAME,
                                (server != null) ? server.port     : Settings.DEFAULT_SERVER_PORT,
                                (server != null) ? server.port     : Settings.DEFAULT_SERVER_PORT,
                                (server != null) ? server.password : ""
                               );
      if (Settings.serverName     != null) loginData.serverName    = Settings.serverName;
      if (Settings.serverPort     != -1  ) loginData.serverPort    = Settings.serverPort;
      if (Settings.serverTLSPort  != -1  ) loginData.serverTLSPort = Settings.serverTLSPort;
      if (Settings.serverPassword != null) loginData.password      = Settings.serverPassword;

      // commands
      if (   (Settings.runJobName != null)
          || (Settings.abortJobName != null)
          || (Settings.indexDatabaseAddStorageName != null)
          || (Settings.indexDatabaseRemoveStorageName != null)
          || (Settings.indexDatabaseRefreshStorageName != null)
          || (Settings.indexDatabaseStorageListName != null)
          || (Settings.indexDatabaseEntriesListName != null)
          || (Settings.pauseTime > 0)
          || (Settings.pingFlag)
          || (Settings.suspendFlag)
          || (Settings.continueFlag)
          || (Settings.listFlag)
          || (Settings.restoreStorageName != null)
          || (Settings.debugQuitServerFlag)
         )
      {
        // non-interactive mode

        // connect to server
        try
        {
          BARServer.connect(loginData.serverName,
                            loginData.serverPort,
                            loginData.serverTLSPort,
                            loginData.password,
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
          int      error;
          String[] errorMessage  = new String[1];

          // get job UUID
          String jobUUID = getJobUUID(Settings.runJobName);
          if (jobUUID == null)
          {
            printError("job '%s' not found",Settings.runJobName);
            BARServer.disconnect();
            System.exit(1);
          }

          // start job
          error = BARServer.executeCommand(StringParser.format("JOB_START jobUUID=%s archiveType=%s dryRun=no",
                                                               jobUUID,
                                                               Settings.archiveType.toString()
                                                              ),
                                           0,
                                           errorMessage
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot start job '%s' (error: %s)",Settings.runJobName,errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.indexDatabaseAddStorageName != null)
        {
          int      error;
          String[] errorMessage  = new String[1];

          // add index for storage
          error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ADD pattern=%'S patternType=GLOB",
                                                               Settings.indexDatabaseAddStorageName
                                                              ),
                                           0,
                                           errorMessage
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot add index for storage '%s' to index (error: %s)",Settings.indexDatabaseAddStorageName,errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.indexDatabaseRefreshStorageName != null)
        {
          int      error;
          String[] errorMessage  = new String[1];

          // remote index for storage
          error = BARServer.executeCommand(StringParser.format("INDEX_REFRESH pattern=%'S patternType=GLOB",
                                                               Settings.indexDatabaseRefreshStorageName
                                                              ),
                                           0,
                                           errorMessage
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot refresh index for storage '%s' from index (error: %s)",Settings.indexDatabaseRefreshStorageName,errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.indexDatabaseRemoveStorageName != null)
        {
          int      error;
          String[] errorMessage  = new String[1];

          // remote index for storage
          error = BARServer.executeCommand(StringParser.format("INDEX_REMOVE pattern=%'S patternType=GLOB",
                                                               Settings.indexDatabaseRemoveStorageName
                                                              ),
                                           0,
                                           errorMessage
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot remove index for storage '%s' from index (error: %s)",Settings.indexDatabaseRemoveStorageName,errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.indexDatabaseStorageListName != null)
        {
          final String[] MAP_TEXT = new String[]{"\\n","\\r","\\\\"};
          final String[] MAP_BIN  = new String[]{"\n","\r","\\"};

          int                 error;
          String[]            errorMessage  = new String[1];
          ValueMap            valueMap     = new ValueMap();
          ArrayList<ValueMap> valueMapList = new ArrayList<ValueMap>();

          // list storage index
          System.out.println(String.format("%-12s %-19s %-5s %-5s %s",
                                           "Size",
                                           "Date/Time",
                                           "State",
                                           "Mode",
                                           "Name"
                                          )
                            );
          System.out.println(StringUtils.repeat("-",12+1+19+1+5+1+5+40));
          error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=* indexStateSet=* indexModeSet=* name=%'S",
                                                               Settings.indexDatabaseStorageListName
                                                              ),
                                           0,
                                           errorMessage,
                                           new Command.ResultHandler()
                                           {
                                             public int handle(int i, ValueMap valueMap)
                                             {
                                               String storageName = valueMap.getString("name"                        );
                                               long   dateTime    = valueMap.getLong  ("dateTime"                    );
                                               long   size        = valueMap.getLong  ("size"                        );
                                               IndexStates state  = valueMap.getEnum  ("indexState",IndexStates.class);
                                               IndexModes mode    = valueMap.getEnum  ("indexMode",IndexModes.class  );

                                               System.out.println(String.format("%12d %-19s %-5s %-5s %s",
                                                                                size,
                                                                                DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                state,
                                                                                mode,
                                                                                storageName
                                                                               )
                                                                 );

                                               return Errors.NONE;
                                             }
                                           }
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot list storages index (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.indexDatabaseEntriesListName != null)
        {
          int      error;
          String[] errorMessage = new String[1];

          // list storage index
          System.out.println(String.format("%-40s %-8s %-12s %-19s %s",
                                           "Storage",
                                           "Type",
                                           "Size",
                                           "Date/Time",
                                           "Name"
                                          )
                            );
          System.out.println(StringUtils.repeat("-",40+1+8+1+12+1+19+40));
          error = BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST name=%'S indexType=* newestOnly=no",
                                                               Settings.indexDatabaseEntriesListName
                                                              ),
                                           0,
                                           errorMessage,
                                           new Command.ResultHandler()
                                           {
                                             public int handle(int i, ValueMap valueMap)
                                             {
                                               switch (valueMap.getEnum("entryType",EntryTypes.class))
                                               {
                                                 case FILE:
                                                   {
                                                     String storageName     = valueMap.getString("storageName"    );
                                                     long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                                     String fileName        = valueMap.getString("name"           );
                                                     long   size            = valueMap.getLong  ("size"           );
                                                     long   dateTime        = valueMap.getLong  ("dateTime"       );
                                                     long   fragmentOffset  = valueMap.getLong  ("fragmentOffset" );
                                                     long   fragmentSize    = valueMap.getLong  ("fragmentSize"   );

                                                     System.out.println(String.format("%-40s %-8s %12d %-19s %s",
                                                                                      storageName,
                                                                                      "FILE",
                                                                                      size,
                                                                                      DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                      fileName
                                                                                     )
                                                                       );
                                                   }
                                                   break;
                                                 case IMAGE:
                                                   {
                                                     String storageName     = valueMap.getString("storageName"    );
                                                     long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                                     String imageName       = valueMap.getString("name"           );
                                                     long   size            = valueMap.getLong  ("size"           );
                                                     long   blockOffset     = valueMap.getLong  ("blockOffset"    );
                                                     long   blockCount      = valueMap.getLong  ("blockCount"     );

                                                     System.out.println(String.format("%-40s %-8s %12d %-19s %s",
                                                                                      storageName,
                                                                                      "IMAGE",
                                                                                      size,
                                                                                      "",
                                                                                      imageName
                                                                                     )
                                                                       );
                                                   }
                                                   break;
                                                 case DIRECTORY:
                                                   {
                                                     String storageName     = valueMap.getString("storageName"    );
                                                     long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                                     String directoryName   = valueMap.getString("name"           );
                                                     long   dateTime        = valueMap.getLong  ("dateTime"       );

                                                     System.out.println(String.format("%-40s %-8s %12s %-19s %s",
                                                                                      storageName,
                                                                                      "DIR",
                                                                                      "",
                                                                                      DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                      directoryName
                                                                                     )
                                                                       );
                                                   }
                                                   break;
                                                 case LINK:
                                                   {
                                                     String storageName     = valueMap.getString("storageName"    );
                                                     long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                                     String linkName        = valueMap.getString("name"           );
                                                     String destinationName = valueMap.getString("destinationName");
                                                     long   dateTime        = valueMap.getLong  ("dateTime"       );

                                                     System.out.println(String.format("%-40s %-8s %12s %-19s %s -> %s",
                                                                                      storageName,
                                                                                      "LINK",
                                                                                      "",
                                                                                      DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                      linkName,
                                                                                      destinationName
                                                                                     )
                                                                       );
                                                   }
                                                   break;
                                                 case HARDLINK:
                                                   {
                                                     String storageName     = valueMap.getString("storageName"    );
                                                     long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                                     String fileName        = valueMap.getString("name"           );
                                                     long   size            = valueMap.getLong  ("size"           );
                                                     long   dateTime        = valueMap.getLong  ("dateTime"       );
                                                     long   fragmentOffset  = valueMap.getLong  ("fragmentOffset" );
                                                     long   fragmentSize    = valueMap.getLong  ("fragmentSize"   );

                                                     System.out.println(String.format("%-40s %-8s %12d %-19s %s",
                                                                                      storageName,
                                                                                      "HARDLINK",
                                                                                      size,
                                                                                      DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                      fileName
                                                                                     )
                                                                       );
                                                   }
                                                   break;
                                                 case SPECIAL:
                                                   {
                                                     String storageName     = valueMap.getString("storageName"    );
                                                     long   storageDateTime = valueMap.getLong  ("storageDateTime");
                                                     String name            = valueMap.getString("name"           );
                                                     long   dateTime        = valueMap.getLong  ("dateTime"       );

                                                     System.out.println(String.format("%-40s %-8s %12s %-19s %s",
                                                                                      storageName,
                                                                                      "SPECIAL",
                                                                                      "",
                                                                                      DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                      name
                                                                                     )
                                                                       );
                                                   }
                                                   break;
                                               }

                                               return Errors.NONE;
                                             }
                                           }
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot list entries index (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.pauseTime > 0)
        {
          int      error;
          String[] errorMessage  = new String[1];

          // pause
          error = BARServer.executeCommand(StringParser.format("PAUSE time=%d modeMask=%s",
                                                               Settings.pauseTime,
                                                               "ALL"
                                                              ),
                                           0,
                                           errorMessage
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot pause (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.pingFlag)
        {
          // nothing to do
        }

        if (Settings.suspendFlag)
        {
          int      error;
          String[] errorMessage  = new String[1];

          // suspend
          error = BARServer.executeCommand(StringParser.format("SUSPEND modeMask=CREATE"),
                                           0,
                                           errorMessage
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot suspend (error: %s)",Settings.runJobName,errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.continueFlag)
        {
          int      error;
          String[] errorMessage  = new String[1];

          // continue
          error = BARServer.executeCommand(StringParser.format("CONTINUE"),
                                           0,
                                           errorMessage
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot continue (error: %s)",Settings.runJobName,errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.abortJobName != null)
        {
          int      error;
          String[] errorMessage  = new String[1];

          // get job id
          String jobUUID = getJobUUID(Settings.abortJobName);
          if (jobUUID == null)
          {
            printError("job '%s' not found",Settings.abortJobName);
            BARServer.disconnect();
            System.exit(1);
          }

          // abort job
          error = BARServer.executeCommand(StringParser.format("JOB_ABORT jobUUID=%s",
                                                               jobUUID
                                                              ),
                                           0,
                                           errorMessage
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot abort job '%s' (error: %s)",Settings.abortJobName,errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.listFlag)
        {
          int                 error;
          String[]            errorMessage = new String[1];
          ValueMap            valueMap     = new ValueMap();
          ArrayList<ValueMap> valueMapList = new ArrayList<ValueMap>();

          // get server state
          String serverState = null;
          error = BARServer.executeCommand(StringParser.format("STATUS"),
                                           0,
                                           errorMessage,
                                           valueMap
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot get state (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
          serverState = valueMap.getString("state");
          if      (serverState.equalsIgnoreCase("running"))
          {
            serverState = null;
          }
          else if (serverState.equalsIgnoreCase("pause"))
          {
            serverState = "pause";
          }
          else if (serverState.equalsIgnoreCase("suspended"))
          {
            serverState = "suspended";
          }
          else
          {
            printWarning("unknown server response '%s'",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }

          // get joblist
          error = BARServer.executeCommand(StringParser.format("JOB_LIST"),
                                           0,
                                           errorMessage,
                                           valueMapList
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot get job list (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
          System.out.println(String.format("%-40s %-20s %-10s %-11s %-12s %-25s %-12s %-10s %-8s %-19s %-12s",
                                           "Name",
                                           "Host name",
                                           "State",
                                           "Type",
                                           "Part size",
                                           "Compress",
                                           "Crypt",
                                           "Crypt type",
                                           "Mode",
                                           "Last executed",
                                           "Estimated"
                                          )
                            );
          System.out.println(StringUtils.repeat("-",40+1+20+1+10+1+11+1+12+1+25+1+12+1+10+1+8+1+19+1+12));
          for (ValueMap valueMap_ : valueMapList)
          {
            // get data
            String jobUUID                = valueMap_.getString("jobUUID"                 );
            String name                   = valueMap_.getString("name"                    );
            String hostName               = valueMap_.getString("hostName",             "");
            String state                  = valueMap_.getString("state"                   );
            String archiveType            = valueMap_.getString("archiveType"             );
            long   archivePartSize        = valueMap_.getLong  ("archivePartSize"         );
            String deltaCompressAlgorithm = valueMap_.getString("deltaCompressAlgorithm"  );
            String byteCompressAlgorithm  = valueMap_.getString("byteCompressAlgorithm"   );
            String cryptAlgorithm         = valueMap_.getString("cryptAlgorithm"          );
            String cryptType              = valueMap_.getString("cryptType"               );
            String cryptPasswordMode      = valueMap_.getString("cryptPasswordMode"       );
            long   lastExecutedDateTime   = valueMap_.getLong  ("lastExecutedDateTime"    );
            long   estimatedRestTime      = valueMap_.getLong  ("estimatedRestTime"       );

            String compressAlgorithms;
            if      (!deltaCompressAlgorithm.equalsIgnoreCase("none") && !byteCompressAlgorithm.equalsIgnoreCase("none")) compressAlgorithms = deltaCompressAlgorithm+"+"+byteCompressAlgorithm;
            else if (!deltaCompressAlgorithm.equalsIgnoreCase("none")                                                   ) compressAlgorithms = deltaCompressAlgorithm;
            else if (                                                    !byteCompressAlgorithm.equalsIgnoreCase("none")) compressAlgorithms = byteCompressAlgorithm;
            else                                                                                                          compressAlgorithms = "-";
            if (cryptAlgorithm.equalsIgnoreCase("none"))
            {
              cryptAlgorithm    = "-";
              cryptType         = "-";
              cryptPasswordMode = "-";
            }

            System.out.println(String.format("%-40s %-20s %-10s %-11s %12d %-25s %-12s %-10s %-8s %-19s %12d",
                                             name,
                                             hostName,
                                             (serverState == null) ? state : serverState,
                                             archiveType,
                                             archivePartSize,
                                             compressAlgorithms,
                                             cryptAlgorithm,
                                             cryptType,
                                             cryptPasswordMode,
                                             DATE_FORMAT.format(new Date(lastExecutedDateTime*1000)),
                                             estimatedRestTime
                                            )
                              );
          }
        }

        if (Settings.restoreStorageName != null)
        {
          int      error;
          String[] errorMessage  = new String[1];

          // set archives to restore
          error = BARServer.executeCommand(StringParser.format("STORAGE_LIST_CLEAR"),0);
          if (error != Errors.NONE)
          {
            printError("cannot set restore list (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
          error = BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s indexStateSet=%s indexModeSet=%s storagePattern=%'S offset=%ld",
                                                               "*",
                                                               "*",
                                                               "*",
                                                               Settings.restoreStorageName,
                                                               0L
                                                              ),
                                           0,
                                           errorMessage,
                                           new Command.ResultHandler()
                                           {
                                             public int handle(int i, ValueMap valueMap)
                                             {
                                               long   storageId   = valueMap.getLong  ("storageId");
                                               String storageName = valueMap.getString("name"     );
//Dprintf.dprintf("storageId=%d: %s",storageId,storageName);

                                               return BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD indexId=%ld",
                                                                                                   storageId
                                                                                                  ),
                                                                               0  // debugLevel
                                                                              );
                                             }
                                           }
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot list storages index (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }

          // restore
          error = BARServer.executeCommand(StringParser.format("RESTORE type=ARCHIVES destination=%'S overwriteFiles=%y",
                                                               Settings.destination,
false//                                                                   Settings.overwriteFilesFlag
                                                              ),
                                           0,  // debugLevel
                                           errorMessage,
                                           new Command.ResultHandler()
                                           {
                                             public int handle(int i, ValueMap valueMap)
                                             {
                                               // parse and update progresss
                                               try
                                               {
                                                 if (valueMap.containsKey("action"))
                                                 {
                                                   Actions       action       = valueMap.getEnum  ("action",Actions.class);
                                                   PasswordTypes passwordType = valueMap.getEnum  ("passwordType",PasswordTypes.class,PasswordTypes.NONE);
                                                   String        passwordText = valueMap.getString("passwordText","");
                                                   String        volume       = valueMap.getString("volume","");
                                                   int           error        = valueMap.getInt   ("error",Errors.NONE);
                                                   String        errorMessage = valueMap.getString("errorMessage","");
                                                   String        storage      = valueMap.getString("storage","");
                                                   String        entry        = valueMap.getString("entry","");

                                                   switch (action)
                                                   {
                                                     case REQUEST_PASSWORD:
                                                       Console console = System.console();

                                                       // get password
                                                       if (passwordType.isLogin())
                                                       {
                                                         System.out.println(BARControl.tr("Please enter {0} login for: {1}",passwordType,passwordText));
                                                         String name       = console.readLine    ("  "+BARControl.tr("Name")+": ");
                                                         char   password[] = console.readPassword("  "+BARControl.tr("Password")+": ");
                                                         if ((password != null) && (password.length > 0))
                                                         {
                                                           BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d name=%S encryptType=%s encryptedPassword=%S",
                                                                                                        Errors.NONE,
                                                                                                        name,
                                                                                                        BARServer.getPasswordEncryptType(),
                                                                                                        BARServer.encryptPassword(new String(password))
                                                                                                       ),
                                                                                    0  // debugLevel
                                                                                   );
                                                         }
                                                         else
                                                         {
                                                           BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d",
                                                                                                        Errors.NO_PASSWORD
                                                                                                       ),
                                                                                    0  // debugLevel
                                                                                   );
                                                         }
                                                       }
                                                       else
                                                       {
                                                         System.out.println(BARControl.tr("Please enter {0} password for: {1}",passwordType,passwordText));
                                                         char password[] = console.readPassword("  "+BARControl.tr("Password")+": ");
                                                         if ((password != null) && (password.length > 0))
                                                         {
                                                           BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d encryptType=%s encryptedPassword=%S",
                                                                                                        Errors.NONE,
                                                                                                        BARServer.getPasswordEncryptType(),
                                                                                                        BARServer.encryptPassword(new String(password))
                                                                                                       ),
                                                                                    0  // debugLevel
                                                                                   );
                                                         }
                                                         else
                                                         {
                                                           BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d",
                                                                                                        Errors.NO_PASSWORD
                                                                                                       ),
                                                                                    0  // debugLevel
                                                                                   );
                                                         }
                                                       }
                                                       break;
                                                     case REQUEST_VOLUME:
Dprintf.dprintf("still not supported");
//System.exit(1);
                                                       break;
                                                     case CONFIRM:
                                                       System.err.println(BARControl.tr("Cannot restore ''{0}'': {1} - skipped", !entry.isEmpty() ? entry : storage,errorMessage));
                                                       BARServer.executeCommand(StringParser.format("ACTION_RESULT error=%d",
                                                                                                    Errors.NONE
                                                                                                   ),
                                                                                0  // debugLevel
                                                                               );
                                                       break;
                                                   }
                                                 }
                                                 else
                                                 {
//                                                   RestoreStates state            = valueMap.getEnum  ("state",RestoreStates.class);
                                                   String        storageName      = valueMap.getString("storageName");
                                                   long          storageDoneSize  = valueMap.getLong  ("storageDoneSize");
                                                   long          storageTotalSize = valueMap.getLong  ("storageTotalSize");
                                                   String        entryName        = valueMap.getString("entryName");
                                                   long          entryDoneSize    = valueMap.getLong  ("entryDoneSize");
                                                   long          entryTotalSize   = valueMap.getLong  ("entryTotalSize");

                                                   //TODO
                                                 }
                                               }
                                               catch (IllegalArgumentException exception)
                                               {
                                                 if (Settings.debugLevel > 0)
                                                 {
                                                   System.err.println("ERROR: "+exception.getMessage());
                                                   System.exit(1);
                                                 }
                                               }

                                               return Errors.NONE;
                                             }
                                           }
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot list storages index (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }

        if (Settings.debugQuitServerFlag)
        {
          // quit server
          if (!BARServer.quit())
          {
            printError("cannot quit server");
            BARServer.disconnect();
            System.exit(1);
          }
        }
      }
      else
      {
        // interactive mode

        // init display
        if (Settings.debugLevel > 0)
        {
          Device.DEBUG=true;
        }
        display = new Display();

        // connect to server
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
                                loginData.serverPort,
                                loginData.serverTLSPort,
                                loginData.password,
                                Settings.serverKeyFileName
                               );
              connectOkFlag = true;
            }
            catch (ConnectionError error)
            {
              // ignored
            }
          }
          if (!connectOkFlag)
          {
            // try to connect to server with empty password
            try
            {
              BARServer.connect(loginData.serverName,
                                loginData.serverPort,
                                loginData.serverTLSPort,
                                "",
                                Settings.serverKeyFileName
                               );
              connectOkFlag = true;
            }
            catch (ConnectionError error)
            {
              // ignored
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
          if ((loginData.serverPort == 0) && (loginData.serverTLSPort == 0))
          {
            throw new Error("Cannot connect to server. No server ports specified!");
          }
/// ??? host name scheck

          // try to connect to server
          try
          {
            BARServer.connect(loginData.serverName,
                              loginData.serverPort,
                              loginData.serverTLSPort,
                              loginData.password,
                              Settings.serverKeyFileName
                             );
            connectOkFlag = true;
          }
          catch (ConnectionError error)
          {
            if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+error.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
            {
              System.exit(1);
            }
          }
          catch (CommunicationError error)
          {
            if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+error.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
            {
              break;
            }
          }
        }

        // open main window
        createWindow();
        createTabs(Settings.selectedJobName);
        createMenu();
        Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);

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
      if (Settings.debugLevel > 0)
      {
        printStackTrace(exception);
      }
    }
    catch (CommunicationError communicationError)
    {
      System.err.println("ERROR communication: "+communicationError.getMessage());
    }
    catch (AssertionError assertionError)
    {
      System.err.println("INTERNAL ERROR: "+assertionError.toString());
      printStackTrace(assertionError);
      System.err.println("");
      System.err.println("Please report this assertion error to torsten.rupp"+MAIL_AT+"gmx.net."); // use MAIL_AT to avoid SPAM
    }
    catch (InternalError error)
    {
      System.err.println("INTERNAL ERROR: "+error.getMessage());
      printStackTrace(error);
      System.err.println("");
      System.err.println("Please report this internal error to torsten.rupp"+MAIL_AT+"gmx.net."); // use MAIL_AT to avoid SPAM
    }
    catch (Error error)
    {
      System.err.println("ERROR: "+error.getMessage());
      if (Settings.debugLevel > 0)
      {
        printStackTrace(error);
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
