/***********************************************************************\
*
* $Revision: 1.30 $
* $Date: 2019-01-17 21:06:25 +0100 (Thu, 17 Jan 2019) $
* $Author: torsten $
* Contents: BARControl (frontend for BAR)
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
// base
import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.Console;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;

import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;

import java.security.KeyStore;

import java.text.DecimalFormat;
import java.text.NumberFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
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
import java.awt.Desktop;
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
import org.eclipse.swt.SWTException;
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

/* jline 3.9.0
//import org.jline.terminal.TerminalBuilder;
import jline.TerminalFactory;

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
  @Override
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
   *   ftp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   scp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   sftp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   webdav://[[<login name>[:<login password>]@<host name>[:<host port>]/]<file name>
   *   cd://[<device name>:]<file name>
   *   dvd://[<device name>:]<file name>
   *   bd://[<device name>:]<file name>
   *   device://[<device name>:]<file name>
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
          if (!loginName.equals("") || !loginPassword.equals(""))
          {
            if (!loginName.equals("")) archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.equals("")) { archiveNameBuffer.append(':'); archiveNameBuffer.append(loginPassword); }
            archiveNameBuffer.append('@');
          }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
          if (hostPort > 0) { archiveNameBuffer.append(':'); archiveNameBuffer.append(hostPort); }
          archiveNameBuffer.append('/');
        }
        break;
      case SFTP:
        archiveNameBuffer.append("sftp://");
        if (!loginName.equals("") || !hostName.equals(""))
        {
          if (!loginName.equals("") || !loginPassword.equals(""))
          {
            if (!loginName.equals("")) archiveNameBuffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.equals("")) { archiveNameBuffer.append(':'); archiveNameBuffer.append(loginPassword); }
            archiveNameBuffer.append('@');
          }
          if (!hostName.equals("")) { archiveNameBuffer.append(hostName); }
          if (hostPort > 0) { archiveNameBuffer.append(':'); archiveNameBuffer.append(hostPort); }
          archiveNameBuffer.append('/');
        }
        break;
      case WEBDAV:
        archiveNameBuffer.append("webdav://");
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
  @Override
  public String toString()
  {
    return getName();
  }
}

/** archive types
 */
enum ArchiveTypes
{
  NONE,
  NORMAL,
  FULL,
  INCREMENTAL,
  DIFFERENTIAL,
  CONTINUOUS;

  /** convert to string
   * @return string
   */
  @Override
  public String toString()
  {
    switch (this)
    {
      case NONE:         return null;
      case NORMAL:       return BARControl.tr("normal");
      case FULL:         return BARControl.tr("full");
      case INCREMENTAL:  return BARControl.tr("incremental");
      case DIFFERENTIAL: return BARControl.tr("differential");
      case CONTINUOUS:   return BARControl.tr("continuous");
      default:           return BARControl.tr("normal");
    }
  }
};

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
   * @param n time [s]
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
   * @param n time [s]
   * @return unit
   */
  public static String getTimeUnit(double n)
  {
    if      (((long)n % (7L*24L*60L*60L)) == 0) return (((long)n / (7L*24L*60L*60L)) != 1) ? "weeks" : "week";
    else if (((long)n % (   24L*60L*60L)) == 0) return (((long)n / (   24L*60L*60L)) != 1) ? "days"  : "day";
    else if (((long)n % (       60L*60L)) == 0) return "h";
    else if (((long)n % (           60L)) == 0) return "min";
    else                                        return "s";
  }

  /** get localized time unit
   * @param n time [s]
   * @return localized unit
   */
  public static String getLocalizedTimeUnit(double n)
  {
    if      (((long)n > 0) && (((long)n % (7L*24L*60L*60L)) == 0)) return (((long)n / (7L*24L*60L*60L)) != 1) ? BARControl.tr("weeks") : BARControl.tr("week");
    else if (((long)n > 0) && (((long)n % (   24L*60L*60L)) == 0)) return (((long)n / (   24L*60L*60L)) != 1) ? BARControl.tr("days" ) : BARControl.tr("day" );
    else if (((long)n > 0) && (((long)n % (       60L*60L)) == 0)) return BARControl.tr("h");
    else if (((long)n > 0) && (((long)n % (           60L)) == 0)) return BARControl.tr("min");
    else if ((long)n > 0)                                          return BARControl.tr("s");
    else                                                           return "";
  }

  /** parse time string
   * @param string string to parse (<n>.<n>(weeks|days|h|min|s)
   * @return time [s]
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

  /** parse localized time string
   * @param string string to parse (<n>.<n>(weeks|days|h|min|s)
   * @return time [s]
   */
  public static long parseLocalizedTime(String string)
    throws NumberFormatException
  {
    final String WEEK    = BARControl.tr("week"   ).toUpperCase();
    final String WEEKS   = BARControl.tr("weeks"  ).toUpperCase();
    final String DAY     = BARControl.tr("day"    ).toUpperCase();
    final String DAYS    = BARControl.tr("days"   ).toUpperCase();
    final String H       = BARControl.tr("h"      ).toUpperCase();
    final String HOUR    = BARControl.tr("hour"   ).toUpperCase();
    final String HOURS   = BARControl.tr("hours"  ).toUpperCase();
    final String M       = BARControl.tr("m"      ).toUpperCase();
    final String MIN     = BARControl.tr("min"    ).toUpperCase();
    final String MINS    = BARControl.tr("mins"   ).toUpperCase();
    final String S       = BARControl.tr("s"      ).toUpperCase();
    final String SECOND  = BARControl.tr("second" ).toUpperCase();
    final String SECONDS = BARControl.tr("seconds").toUpperCase();

    string = string.toUpperCase();

    // try to parse with default locale
    if       (string.endsWith(WEEK))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-WEEK.length()))*7L*24L*60L*60L);
    }
    if       (string.endsWith(WEEKS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-WEEKS.length()))*7L*24L*60L*60L);
    }
    else if (string.endsWith(DAY))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-DAY.length()))*24L*60L*60L);
    }
    else if (string.endsWith(DAYS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-DAYS.length()))*24L*60L*60L);
    }
    else if (string.endsWith(H))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-H.length()))*60L*60L);
    }
    else if (string.endsWith(HOUR))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-HOUR.length()))*60L*60L);
    }
    else if (string.endsWith(HOURS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-HOURS.length()))*60L*60L);
    }
    else if (string.endsWith(M))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-M.length()))*60L*60L);
    }
    else if (string.endsWith(MIN))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-MIN.length()))*60L);
    }
    else if (string.endsWith(MINS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-MINS.length()))*60L);
    }
    else if (string.endsWith(S))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-S.length())));
    }
    else if (string.endsWith(SECOND))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SECOND.length())));
    }
    else if (string.endsWith(SECONDS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SECONDS.length())));
    }
    else
    {
      return (long)Double.parseDouble(string);
    }
  }

  /** parse time string
   * @param string string to parse (<n>.<n>(weeks|days|h|min|s)
   * @param defaultValue default value if string cannot be parsed
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

  /** parse localized time string
   * @param string string to parse (<n>.<n>(weeks|days|h|min|s)
   * @param defaultValue default value if string cannot be parsed
   * @return time
   */
  public static long parseLocalizedTime(String string, long defaultValue)
  {
    long n;

    try
    {
      n = Units.parseLocalizedTime(string);
    }
    catch (NumberFormatException exception)
    {
      n = defaultValue;
    }

    return n;
  }

  /** format time
   * @param n time [s]
   * @return string with unit
   */
  public static String formatTime(long n)
  {
    return getTime(n)+getTimeUnit(n);
  }

  /** format time localized
   * @param n time [s]
   * @return localized string with unit
   */
  public static String formatLocalizedTime(long n)
  {
    return getTime(n)+getLocalizedTimeUnit(n);
  }
}

/** keep
 */
class Keep
{
  public final static int ALL = -1;

  /** format keep
   * @param keep keep
   * @return formated
   */
  public static String format(int keep)
  {
    if (keep == ALL) return BARControl.tr("-");
    else             return String.format("%d",keep);
  }
}

/** age
 */
class Age
{
  public final static int FOREVER = -1;

  /** format age
   * @param age age
   * @return formated
   */
  public static String format(int age)
  {
    if      (age == FOREVER  ) return BARControl.tr("forever"                                         );
    else if (age == 365      ) return BARControl.tr("{0} {0,choice,0#years|1#year|1<years}",   1      );
    else if ((age % 365) == 0) return BARControl.tr("{0} {0,choice,0#years|1#year|1<years}",   age/365);
    else if (age == 30       ) return BARControl.tr("{0} {0,choice,0#months|1#month|1<months}",1      );
    else if ((age % 30) == 0 ) return BARControl.tr("{0} {0,choice,0#months|1#month|1<months}",age/30 );
    else if (age == 7        ) return BARControl.tr("{0} {0,choice,0#weeks|1#week|1<weeks}",   1      );
    else if ((age % 7) == 0  ) return BARControl.tr("{0} {0,choice,0#weeks|1#week|1<weeks}",   age/7  );
    else                       return BARControl.tr("{0} {0,choice,0#days|1#day|1<days}",      age    );
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
  @Override
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
    String  serverName;       // server name
    int     serverPort;       // server port
    int     serverTLSPort;    // server TLS port
    boolean forceSSL;         // force SSL
    String  password;         // login password
    Roles   role;             // role

    /** create login data
     * @param serverName server name
     * @param port server port
     * @param tlsPort server TLS port
     * @param password server password
     * @param forceSSL TRUE to force SSL
     * @param role role
     */
    LoginData(String name, int port, int tlsPort, boolean forceSSL, String password, Roles role)
    {
      final Settings.Server defaultServer = Settings.getLastServer();

      this.serverName    = !name.isEmpty()     ? name     : ((defaultServer != null) ? defaultServer.name : Settings.DEFAULT_SERVER_NAME    );
      this.serverPort    = (port != 0        ) ? port     : ((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT    );
      this.serverTLSPort = (port != 0        ) ? tlsPort  : ((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_TLS_PORT);
      this.forceSSL      = forceSSL;
      this.password      = !password.isEmpty() ? password : "";
      this.role          = role;

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
     * @param forceSSL TRUE to force SSL
     * @param role role
     */
    LoginData(String name, int port, int tlsPort, boolean forceSSL, Roles role)
    {
      this(name,port,tlsPort,forceSSL,"",role);
    }

    /** create login data
     * @param port server port
     * @param tlsPort server TLS port
     * @param forceSSL TRUE to force SSL
     * @param role role
     */
    LoginData(int port, int tlsPort, boolean forceSSL, Roles role)
    {
      this("",port,tlsPort,forceSSL,role);
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      return "LoginData {"+serverName+", "+serverPort+", "+serverTLSPort+", "+(forceSSL ? "SSL" : "plain")+"}";
    }
  }

  /** list remote directory
   */
  public static ListDirectory<File> listDirectory = new ListDirectory<File>()
  {
    /** get new file instance
     * @param name name
     * @return file
     */
    @Override
    public File newFileInstance(String name)
    {
      return new File(name);
    }

    /** get shortcut files
     * @return shortcut files
     */
    @Override
    public void getShortcuts(java.util.List<File> shortcutList)
    {
      final HashMap<String,File> shortcutMap = new HashMap<String,File>();

      // add manual shortcuts
      for (String name : Settings.shortcuts)
      {
        shortcutMap.put(name,new File(name));
      }

      // add root shortcuts
      shortcutMap.put("/",new File("/"));

      // create sorted list
      shortcutList.clear();
      for (File shortcut : shortcutMap.values())
      {
        shortcutList.add(shortcut);
      }
      Collections.sort(shortcutList,this);
    }

    /** remove shortcut file
     * @param name shortcut name
     */
    @Override
    public void addShortcut(File shortcut)
    {
      Settings.shortcuts.add(shortcut.getAbsolutePath());
    }

    /** remove shortcut file
     * @param shortcut shortcut file
     */
    @Override
    public void removeShortcut(File shortcut)
    {
      Settings.shortcuts.remove(shortcut.getAbsolutePath());
    }

    /** open list files in directory
     * @param pathName path name
     * @return true iff open
     */
    @Override
    public boolean open(File path)
    {
      files = path.listFiles();
      index = 0;

      return files != null;
    }

    /** close list files in directory
     */
    @Override
    public void close()
    {
    }

    /** get next entry in directory
     * @return entry
     */
    @Override
    public File getNext()
    {
      File file;

      if (index < files.length)
      {
        file = files[index];
        index++;
      }
      else
      {
        file = null;
      }

      return file;
    }

    private File files[];
    private int  index;
  };

  // --------------------------- constants --------------------------------

  // exit codes
  public static int EXITCODE_OK             =   0;
  public static int EXITCODE_FAIL           =   1;
  public static int EXITCODE_RESTART        =  64;
  public static int EXITCODE_INTERNAL_ERROR = 127;

  /** roles
   */
  public static enum Roles
  {
    BASIC,
    NORMAL,
    EXPERT
  };

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

  /** restore states
   */
  enum RestoreStates
  {
    RUNNING,
    RESTORED,
    FAILED;
  };

  // user events
  final static int USER_EVENT_NEW_SERVER = 0xFFFF+0;
  final static int USER_EVENT_NEW_JOB    = 0xFFFF+1;

  // string with "all files" extension
  public static final String ALL_FILE_EXTENSION;

  // version, email address, homepage URL
  public static final String VERSION          = Config.VERSION_MAJOR+"."+Config.VERSION_MINOR+" ("+Config.VERSION_REVISION+")";
  public static final char   MAIL_AT          = '@';  // use MAIL_AT to avoid spam
  public static final String EMAIL_ADDRESS    = "torsten.rupp"+MAIL_AT+"gmx.net";
  public static final String URL              = "http://www.kigen.de/projects/bar";
  public static final String URL_VERSION_FILE = URL+"/version";

  // host system
  private static final HostSystems hostSystem;

  // command line options
  private static final OptionEnumeration[] ARCHIVE_TYPE_ENUMERATION =
  {
    new OptionEnumeration("normal",      ArchiveTypes.NORMAL),
    new OptionEnumeration("full",        ArchiveTypes.FULL),
    new OptionEnumeration("incremental", ArchiveTypes.INCREMENTAL),
    new OptionEnumeration("differential",ArchiveTypes.DIFFERENTIAL),
  };

  private static final OptionEnumeration[] ROLE_ENUMERATION =
  {
    new OptionEnumeration("basic", Roles.BASIC),
    new OptionEnumeration("normal",Roles.NORMAL),
    new OptionEnumeration("expert",Roles.EXPERT),
  };

  private static final Option[] OPTIONS =
  {
    new Option("--config",                       null,Options.Types.STRING,     "configFileName"),
//TODO: use server structure
    new Option("--port",                         "-p",Options.Types.INTEGER,    "serverPort"),
    new Option("--tls-port",                     null,Options.Types.INTEGER,    "serverTLSPort"),
    new Option("--ca-file",                      null,Options.Types.STRING,     "serverCAFileName"),
    new Option("--cert-file",                    null,Options.Types.STRING,     "serverCertificateFileName"),
    new Option("--key-file",                     null,Options.Types.STRING,     "serverKeyFileName"),
    new Option("--force-ssl",                    null,Options.Types.BOOLEAN,    "forceSSL"),
    new Option("--password",                     null,Options.Types.STRING,     "serverPassword"),
    new Option("--login-dialog",                 null,Options.Types.BOOLEAN,    "loginDialogFlag"),
    new Option("--pair-master",                  null,Options.Types.BOOLEAN,    "pairMasterFlag"),

    new Option("--select-job",                   null,Options.Types.STRING,     "selectedJobName"),
    new Option("--job",                          "-j",Options.Types.STRING,     "runJobName"),
    new Option("--archive-type",                 null,Options.Types.ENUMERATION,"archiveType",ARCHIVE_TYPE_ENUMERATION),
    new Option("--abort",                        null,Options.Types.STRING,     "abortJobName"),
    new Option("--pause",                        "-t",Options.Types.INTEGER,    "pauseTime",new Object[]{"s",1,"m",60,"h",60*60}),
    new Option("--ping",                         "-i",Options.Types.BOOLEAN,    "pingFlag"),
    new Option("--suspend",                      "-s",Options.Types.BOOLEAN,    "suspendFlag"),
    new Option("--continue",                     "-c",Options.Types.BOOLEAN,    "continueFlag"),
    new Option("--list",                         "-l",Options.Types.BOOLEAN,    "listFlag"),

    new Option("--index-database-add",           null,Options.Types.STRING,     "indexDatabaseAddStorageName"),
    new Option("--index-database-remove",        null,Options.Types.STRING,     "indexDatabaseRemoveStorageName"),
    new Option("--index-database-refresh",       null,Options.Types.STRING,     "indexDatabaseRefreshStorageName"),
    new Option("--index-database-entities-list", "-n",Options.Types.STRING,     "indexDatabaseEntitiesListName",""),
    new Option("--index-database-storages-list", "-a",Options.Types.STRING,     "indexDatabaseStoragesListName",""),
    new Option("--index-database-entries-list",  "-e",Options.Types.STRING,     "indexDatabaseEntriesListName"),
    new Option("--index-database-entries-newest",null,Options.Types.BOOLEAN,    "indexDatabaseEntriesNewestOnly"),
    new Option("--index-database-history-list",  null,Options.Types.BOOLEAN,    "indexDatabaseHistoryList"),

    new Option("--restore",                      null,Options.Types.STRING,     "restoreStorageName"),
    new Option("--destination",                  null,Options.Types.STRING,     "destination"),
    new Option("--overwrite-entries",            null,Options.Types.BOOLEAN,    "overwriteEntriesFlag"),

    new Option("--role",                         null,Options.Types.ENUMERATION,"role",ROLE_ENUMERATION),

    new Option("--version",                      null,Options.Types.BOOLEAN,    "versionFlag"),
    new Option("--help",                         "-h",Options.Types.BOOLEAN,    "helpFlag"),
    new Option("--xhelp",                        null,Options.Types.BOOLEAN,    "xhelpFlag"),

    new Option("--debug",                        "-d",Options.Types.INCREMENT,  "debugLevel"),
    new Option("--debug-ignore-protocol-version",null,Options.Types.BOOLEAN,    "debugIgnoreProtocolVersion"),
    new Option("--debug-quit-server",            null,Options.Types.BOOLEAN,    "debugQuitServerFlag"),

    // ignored
    new Option("--swing",                        null, Options.Types.BOOLEAN,   null),
  };

  // --------------------------- variables --------------------------------
  private static I18n     i18n;
  private static Display  display;
  private static Shell    shell;
  private static Cursor   waitCursor;
  private static int      waitCursorCount = 0;

  private LoginData       loginData;
  private Menu            serverMenu;
  private MenuItem        masterMenuItem;
  private TabFolder       tabFolder;
  private TabStatus       tabStatus;
  private TabJobs         tabJobs;
  private TabRestore      tabRestore;
  private boolean         quitFlag = false;

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

  /** stack trace to string list
   * @param throwable throwable
   * @return string list
   */
  public static java.util.List<String> getStackTraceList(Throwable throwable)
  {
    ArrayList<String> stringList = new ArrayList<String>();
    for (StackTraceElement stackTraceElement : throwable.getStackTrace())
    {
      stringList.add("  "+stackTraceElement);
    }
    Throwable cause = throwable.getCause();
    while (cause != null)
    {
      stringList.add("Caused by:");
      for (StackTraceElement stackTraceElement : cause.getStackTrace())
      {
        stringList.add("  "+stackTraceElement);
      }
      cause = cause.getCause();
    }

    return stringList;
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

  /** print internal error to stderr
   * @param throwable throwable
   */
  public static void printInternalError(Throwable throwable)
  {
    printInternalError("%s",throwable.getMessage());
    printStackTrace(throwable);
    System.err.println("Version "+VERSION);
    System.err.println("Please report this error to "+EMAIL_ADDRESS+".");
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

  /** show fatal program error
   * @param throwable fatal program error
   */
  public static void showFatalError(final Throwable throwable)
  {
    // process SWT event queue to avoid multiple errors
    boolean doneFlag = true;
    do
    {
      try
      {
        doneFlag = !display.readAndDispatch();
      }
      catch (Throwable dummyThrowable)
      {
        // ignored
      }
    }
    while (!doneFlag);

    // get cause message
    final String message[] = new String[1];
    Throwable    cause = throwable;
    do
    {
      message[0] = cause.toString();
      cause = cause.getCause();
    }
    while (cause != null);

    // show error dialog
    display.syncExec(new Runnable()
    {
      public void run()
      {
        Dialogs.error(new Shell(),
                      BARControl.getStackTraceList(throwable),
                      BARControl.tr("INTERNAL ERROR")+": "+message[0]+"\n"+
                      "\n"+
                      "Version "+VERSION+"\n"+
                      "\n"+
                      BARControl.tr("Please report this error to ")+BARControl.EMAIL_ADDRESS+"." // use MAIL_AT to avoid SPAM
                     );
      }
    });
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
    System.out.println("Options: --config=<file name>                       - configuration file name (default: "+Settings.DEFAULT_BARCONTROL_CONFIG_FILE_NAME+")");
    System.out.println("         -p|--port=<n>                              - server port (default: "+Settings.DEFAULT_SERVER_PORT+")");
    System.out.println("         --tls-port=<n>                             - TLS server port (default: "+Settings.DEFAULT_SERVER_TLS_PORT+")");
    System.out.println("         --password=<password>                      - server password (use with care!)");
    System.out.println("         --ca-file=<file name>                      - certificate authority file name (PEM format)");
    System.out.println("         --cert-file=<file name>                    - certificate file name (PEM format)");
    System.out.println("         --key-file=<file name>                     - key file name (default: ");
    System.out.println("                                                        ."+File.separator+BARServer.DEFAULT_JAVA_KEY_FILE_NAME+" or ");
    System.out.println("                                                        "+System.getProperty("user.home")+File.separator+".bar"+File.separator+BARServer.DEFAULT_JAVA_KEY_FILE_NAME+" or ");
    System.out.println("                                                        "+Config.CONFIG_DIR+File.separator+BARServer.DEFAULT_JAVA_KEY_FILE_NAME);
    System.out.println("                                                      )" );
    System.out.println("         --force-ssl                                - force SSL connection");
    System.out.println("         --login-dialog                             - force to open login dialog");
    System.out.println("         --pair-master                              - start pairing new master");
    System.out.println("");
    System.out.println("         --select-job=<name>                        - select job <name>");
    System.out.println("         -j|--job=<name>                            - start execution of job <name>");
    System.out.println("         --archive-type=<mode>                      - archive type:");
    System.out.println("                                                        normal (default)");
    System.out.println("                                                        full");
    System.out.println("                                                        incremental");
    System.out.println("                                                        differential");
    System.out.println("         --abort=<name>                             - abort execution of job <name>");
    System.out.println("         -i|--ping                                  - check connection to server");
    System.out.println("         -t|--pause=<n>[s|m|h]                      - pause job execution for <n> seconds/minutes/hours");
    System.out.println("         -s|--suspend                               - suspend job execution");
    System.out.println("         -c|--continue                              - continue job execution");
    System.out.println("         -l|--list                                  - list jobs");
    System.out.println("");
    System.out.println("         --index-database-add=<name|directory>      - add storage archive <name> or all .bar files to index");
    System.out.println("         --index-database-remove=<text>             - remove matching storage archives from index");
    System.out.println("         --index-database-refresh=<text>            - refresh matching storage archive in index");
    System.out.println("         -n|--index-database-entities-list[=<text>] - list index entities");
    System.out.println("         -a|--index-database-storages-list=<text>   - list index storage archives");
    System.out.println("         -e|--index-database-entries-list=<text>    - list index entries");
    System.out.println("         --index-database-entries-newest            - list index newest entries only");
    System.out.println("         --index-database-history-list              - list index history");
    System.out.println("");
    System.out.println("         --restore=<name>                           - restore storage <name>");
    System.out.println("         --destination=<directory>                  - destination to restore entries");
    System.out.println("         --overwrite-entries                        - overwrite existing entries on restore");
    System.out.println("");
    System.out.println("         --role=<role>                              - select role:");
    System.out.println("                                                        basic (default)");
    System.out.println("                                                        normal");
    System.out.println("                                                        expert");
    System.out.println("");
    System.out.println("         --version                                  - output version");
    System.out.println("         -h|--help                                  - print this help");
    if (Settings.xhelpFlag)
    {
      System.out.println("");
      System.out.println("         -d|--debug                                 - enable debug mode");
      System.out.println("         --debug-ignore-protocol-version            - ignore protocol version");
      System.out.println("         --debug-quit-server                        - send quit-command to server");
    }
  }

  /** print program version
   */
  private void printVersion()
  {
    System.out.println("barcontrol "+VERSION);
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
      System.exit(EXITCODE_OK);
    }

    // version
    if (Settings.versionFlag)
    {
      printVersion();
      System.exit(EXITCODE_OK);
    }

    // add/update server
//        Settings.serverNames.remove(args[z]);
//        Settings.serverNames.add(args[z]);

    // check arguments
if (false) {
//TODO: check PEM
    if ((Settings.serverKeyFileName != null) && !Settings.serverKeyFileName.isEmpty())
    {
      // check if PEM/JKS file is readable
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
  }

  /** get database id
   * @param indexId index id
   * @return database id
   */
  private static long getDatabaseId(long indexId)
  {
    return (indexId & 0xFFFFFFF0) >> 4;
  }

  /** get job UUID
   * @param jobName job name
   * @return job UUID or null if not found
   */
  private static String getJobUUID(String jobName)
  {
    try
    {
//TODO: handler
      ArrayList<ValueMap> valueMapList = new ArrayList<ValueMap>();
      BARServer.executeCommand(StringParser.format("JOB_LIST"),
                               1,  // debug level
                               valueMapList
                              );

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
    catch (BARException exception)
    {
      if (Settings.debugLevel > 0)
      {
        printError("cannot get job list (error: %s)",exception.getText());
        BARServer.disconnect();
        System.exit(EXITCODE_FAIL);
      }
    }

    return null;
  }

  /** init loaded classes/JARs watchdog
   */
  private void initClassesWatchDog()
  {
    // get timestamp of all classes/JAR files
    final HashMap<File,Long> classModifiedMap = new HashMap<File,Long>();
    LinkedList<File> directoryList = new LinkedList<File>();
    for (String name : System.getProperty("java.class.path").split(File.pathSeparator))
    {
      File file = new File(name);
      if (file.isDirectory())
      {
        directoryList.add(file);
      }
      else
      {
        classModifiedMap.put(file,new Long(file.lastModified()));
      }
    }
    while (directoryList.size() > 0)
    {
      File directory = directoryList.removeFirst();
      File[] files = directory.listFiles();
      if (files != null)
      {
        for (File file : files)
        {
          if (file.isDirectory())
          {
            directoryList.add(file);
          }
          else
          {
            classModifiedMap.put(file,new Long(file.lastModified()));
          }
        }
      }
    }

    // periodically check timestamp of classes/JAR files
    Thread classWatchDogThread = new Thread()
    {
      private String                  homepageVersionMajor    = null;
      private String                  homepageVersionMinor    = null;
      private String                  homepageVersionRevision = null;
      private final ArrayList<String> homepageChangeLog       = new ArrayList<String>();

      public void run()
      {
        final long REMINDER_TIME       =  5*60*1000; // [ms]
        final long VERSION_CHECK_TIME  = 60*60*1000; // [ms]

        long            lastRemindedTimestamp       = 0L;
        long            lastVersionCheckedTimestamp = 0L;
        final boolean[] reminderFlag                = new boolean[]{true};

        for (;;)
        {
          // check timestamps of classes/JAR files, show warning dialog
          for (final File file : classModifiedMap.keySet())
          {
            if (   reminderFlag[0]
                && (file.lastModified() > classModifiedMap.get(file))
                && (System.currentTimeMillis() > (lastRemindedTimestamp+REMINDER_TIME))
               )
            {
//Dprintf.dprintf("file=%s %d -> %d",file,file.lastModified(),classModifiedMap.get(file));
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  switch (Dialogs.select(shell,"Warning","Class/JAR file '"+file.getName()+"' changed. Is is recommended to restart BARControl now.",new String[]{"Restart","Remind me again in 5min","Ignore"},0))
                  {
                    case 0:
                      // send close event with restart
                      Widgets.notify(shell,SWT.Close,EXITCODE_RESTART);
                      break;
                    case 1:
                      break;
                    case 2:
                      reminderFlag[0] = false;
                      break;
                  }
                }
              });
              lastRemindedTimestamp = System.currentTimeMillis();
            }
          }

          if (Settings.showNewVersionInformation && (System.currentTimeMillis() > (lastVersionCheckedTimestamp+VERSION_CHECK_TIME)))
          {
            // get version on homepage, show warning dialog
            getServerVersionInfo();

            // check if newer version is available
            if (   ((homepageVersionMajor != null) && (homepageVersionMinor != null))
                && (   (homepageVersionMajor.compareTo(Config.VERSION_MAJOR) > 0)
                    || (homepageVersionMinor.compareTo(Config.VERSION_MINOR) > 0)
//                    || ((homepageVersionRevision != null) && (homepageVersionRevision.compareTo(Config.VERSION_MAJOR) > 0))
                   )
               )
            {
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  showNewVersionInfo(shell);
                }
              });
            }

            lastVersionCheckedTimestamp = System.currentTimeMillis();
          }

          // sleep a short time
          try { Thread.sleep(30*1000); } catch (InterruptedException exception) { /* ignored */ }
        }
      }

      /** get servers version info
       */
      private void getServerVersionInfo()
      {
        final Pattern PATTERN_MAJOR    = Pattern.compile("MAJOR=(.*)");
        final Pattern PATTERN_MINOR    = Pattern.compile("MINOR=(.*)");
        final Pattern PATTERN_REVISION = Pattern.compile("REVISION=(.*)");

        BufferedReader input = null;
        try
        {
          input = new BufferedReader(new InputStreamReader(new URL(URL_VERSION_FILE).openStream(),"UTF-8"));

          // get version/change log available on server
          String  line;
          Matcher matcher;
          while (((line = input.readLine()) != null) && !line.isEmpty())
          {
//Dprintf.dprintf("homepage1 %s",line);
            if      ((matcher = PATTERN_MAJOR.matcher(line)).matches())
            {
              homepageVersionMajor    = matcher.group(1);
            }
            else if ((matcher = PATTERN_MINOR.matcher(line)).matches())
            {
              homepageVersionMinor    = matcher.group(1);
            }
            else if ((matcher = PATTERN_REVISION.matcher(line)).matches())
            {
              homepageVersionRevision = matcher.group(1);
            }
          }
          homepageChangeLog.clear();
          while ((line = input.readLine()) != null)
          {
//Dprintf.dprintf("homepage2 %s",line);
            homepageChangeLog.add(line);
          }

          input.close(); input = null;
        }
        catch (Exception exception)
        {
          // nothing to do
        }
        finally
        {
          try { input.close(); } catch (Exception exception) { /* nothing to do */ }
        }
      }

      /** show version info, change log
       * @param shell parent shell
       */
      private void showNewVersionInfo(final Shell shell)
      {
        Composite composite;
        Label     label;
        Text      text;
        Button    button;

        String version = String.format("%s.%s%s",
                                       homepageVersionMajor,
                                       homepageVersionMinor,
                                       (homepageVersionRevision != null) ? " (revision "+homepageVersionRevision+")" : ""
                                      );

        StringBuilder changeLog = new StringBuilder();
        for (String line : homepageChangeLog)
        {
          changeLog.append(line+Text.DELIMITER);
        }

        final Shell dialog = Dialogs.openModal(shell,"Confirmation",new double[]{1.0,0.0},1.0);
        dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

        final Button widgetShowAgain;
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
        composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
        {
          // message
          label = Widgets.newImage(composite,Widgets.loadImage(shell.getDisplay(),"question.png"));
          Widgets.layout(label,0,0,TableLayoutData.W,0,0,10);

          label = Widgets.newLabel(composite,
                                   String.format("A newer version %s of BARControl is available. You\ncan download it from the BARControl homepage:\n\n%s\n\nChangeLog:",
                                                 version,
                                                 URL
                                                ),
                                   SWT.LEFT|SWT.WRAP
                                  );
          Widgets.layout(label,0,1,TableLayoutData.W,0,0,4);

          // change log
          text = Widgets.newStringView(composite,SWT.BORDER|SWT.WRAP);
          text.setText(changeLog.toString());
          Widgets.layout(text,1,1,TableLayoutData.NSWE,0,0,4);

          // show again
          widgetShowAgain = Widgets.newCheckbox(composite,"show again");
          widgetShowAgain.setSelection(true);
          Widgets.layout(widgetShowAgain,2,1,TableLayoutData.W,0,0,4);
        }

        // buttons
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(0.0,1.0));
        composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4));
        {
          button = Widgets.newButton(composite,"Open browser");
          button.setFocus();
          Widgets.layout(button,0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Dialogs.close(dialog,true);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });

          button = Widgets.newButton(composite,"Cancel");
          Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
          {
            public void widgetSelected(SelectionEvent selectionEvent)
            {
              Dialogs.close(dialog,false);
            }
            public void widgetDefaultSelected(SelectionEvent selectionEvent)
            {
            }
          });
        }

        Dialogs.run(dialog,
                    true,
                    new DialogRunnable()
                    {
                      public void done(Object result)
                      {
                        if ((Boolean)result)
                        {
                          try
                          {
                            if (Desktop.isDesktopSupported())
                            {
                              Desktop.getDesktop().browse(new URI(URL));
                            }
                          }
                          catch (URISyntaxException exception)
                          {
                            Dialogs.error(shell,"Cannot open default web browser:\n\n"+exception.getMessage());
                          }
                          catch (IOException exception)
                          {
                            Dialogs.error(shell,"Cannot open default web browser:\n\n"+reniceIOException(exception).getMessage());
                          }
                          catch (Exception exception)
                          {
                            Dialogs.error(shell,"Cannot open default web browser:\n\n"+exception.getMessage());
                          }
                        }
                        else
                        {
                          // nothting to do
                        }
                        Settings.showNewVersionInformation = widgetShowAgain.getSelection();
                      }
                    }
                   );
      }
    };
    classWatchDogThread.setDaemon(true);
    classWatchDogThread.start();
  }

  /** create main window
   */
  private void createWindow()
  {
    // create shell window
    shell = new Shell(display);
    shell.setText("BAR control");
    shell.setLayout(new TableLayout(1.0,1.0));

    // get cursors
    waitCursor = new Cursor(display,SWT.CURSOR_WAIT);
  }

  /** create tabs
   */
  private void createTabs()
  {
    // create tabs
    tabFolder = Widgets.newTabFolder(shell);
    Widgets.layout(tabFolder,0,0,TableLayoutData.NSWE);
    tabStatus  = new TabStatus (tabFolder,SWT.F1);
    tabJobs    = new TabJobs   (tabFolder,SWT.F2);
    tabRestore = new TabRestore(tabFolder,SWT.F3);
    tabStatus.setTabJobs(tabJobs);
    tabJobs.setTabStatus(tabStatus);
    tabRestore.setTabStatus(tabStatus);
    tabRestore.setTabJobs(tabJobs);

    // start auto update
    tabStatus.startUpdate();

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

    menu = Widgets.addMenu(menuBar,BARControl.tr("Program"));
    {
      serverMenu = Widgets.addMenu(menu,BARControl.tr("Connect"),BARServer.isMaster() && Settings.hasExpertRole());
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
                                      (defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_TLS_PORT,
                                      Settings.forceSSL,
                                      Settings.role
                                     );
            if (getLoginData(loginData,false))
            {
              try
              {
                BARServer.connect(display,
                                  loginData.serverName,
                                  loginData.serverPort,
                                  loginData.serverTLSPort,
                                  loginData.forceSSL,
                                  loginData.password,
                                  Settings.serverCAFileName,
                                  Settings.serverCertificateFileName,
                                  Settings.serverKeyFileName
                                 );
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

              // notify new server
              Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);
            }
          }
        });

        Widgets.addMenuItemSeparator(serverMenu);

        updateServerMenu();
      }

      Widgets.addMenuItemSeparator(menu,Settings.hasExpertRole());

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Start")+"\u2026",SWT.CTRL+'S',BARServer.isMaster());
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
      tabStatus.addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
      {
        @Override
        public void handle(Widget widget, JobData jobData)
        {
          MenuItem menuItem = (MenuItem)widget;
//TODO: required?
assert jobData != null;
          menuItem.setEnabled(   (jobData.state != JobData.States.RUNNING    )
                              && (jobData.state != JobData.States.DRY_RUNNING)
                              && (jobData.state != JobData.States.WAITING    )
                             );
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Abort")+"\u2026",SWT.CTRL+'A',BARServer.isMaster());
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
      tabStatus.addUpdateJobStateListener(new UpdateJobStateListener(menuItem)
      {
        @Override
        public void handle(Widget widget, JobData jobData)
        {
          MenuItem menuItem = (MenuItem)widget;
//TODO: required?
assert jobData != null;
          menuItem.setEnabled(   (jobData.state == JobData.States.WAITING       )
                              || (jobData.state == JobData.States.RUNNING       )
                              || (jobData.state == JobData.States.DRY_RUNNING   )
                              || (jobData.state == JobData.States.REQUEST_VOLUME)
                             );
        }
      });

      subMenu = Widgets.addMenu(menu,BARControl.tr("Pause"),BARServer.isMaster() && Settings.hasNormalRole());
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

        Widgets.addMenuItemSeparator(subMenu);

        menuItem = Widgets.addMenuItemCheckbox(subMenu,BARControl.tr("Create operation"),Settings.pauseCreateFlag,true);
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

        menuItem = Widgets.addMenuItemCheckbox(subMenu,BARControl.tr("Storage operation"),Settings.pauseStorageFlag,true);
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

        menuItem = Widgets.addMenuItemCheckbox(subMenu,BARControl.tr("Restore operation"),Settings.pauseRestoreFlag,true);
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

        menuItem = Widgets.addMenuItemCheckbox(subMenu,BARControl.tr("Index update operation"),Settings.pauseIndexUpdateFlag,true);
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

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Toggle suspend/continue"),SWT.CTRL+'T',BARServer.isMaster());
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

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clear stored passwords on server"),SWT.NONE,Settings.hasExpertRole());
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          try
          {
            BARServer.executeCommand(StringParser.format("PASSWORDS_CLEAR"),0);
          }
          catch (BARException exception)
          {
            Dialogs.error(shell,BARControl.tr("Cannot clear passwords on server:\n\n")+exception.getText());
          }
        }
      });

      Widgets.addMenuItemSeparator(menu,Settings.hasExpertRole());

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Server settings")+"\u2026",SWT.CTRL+'W',Settings.hasExpertRole());
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

      masterMenuItem = Widgets.addMenuItemCheckbox(menu,BARControl.tr("Pair master")+"\u2026",Settings.hasExpertRole());
      masterMenuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          MenuItem menuItem = (MenuItem)selectionEvent.widget;

          if (menuItem.getSelection())
          {
            pairMaster();
          }
          else
          {
            clearMaster();
          }
        }
      });
      shell.addListener(BARControl.USER_EVENT_NEW_SERVER,new Listener()
      {
        public void handleEvent(Event event)
        {
          switch (BARServer.getMode())
          {
            case MASTER:
              masterMenuItem.setEnabled(false);
              break;
            case SLAVE:
              masterMenuItem.setEnabled(true);

              String name = BARServer.getMaster();
              if (!name.isEmpty())
              {
                masterMenuItem.setText(BARControl.tr("Master")+": "+name);
                masterMenuItem.setSelection(true);
              }
              else
              {
                masterMenuItem.setText(BARControl.tr("Pair master")+"\u2026");
                masterMenuItem.setSelection(false);
              }
              break;
          }
        }
      });

      Widgets.addMenuItemSeparator(menu);

      subMenu = Widgets.addMenu(menu,BARControl.tr("Role"));
      {
        menuItem = Widgets.addMenuItemRadio(subMenu,BARControl.tr("Basic"));
        menuItem.setSelection(Settings.role == Roles.BASIC);
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem menuItem = (MenuItem)selectionEvent.widget;
            if (menuItem.getSelection())
            {
              Settings.role = Roles.BASIC;
              shell.dispose();
            }
          }
        });

        menuItem = Widgets.addMenuItemRadio(subMenu,BARControl.tr("Normal"));
        menuItem.setSelection(Settings.role == Roles.NORMAL);
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem menuItem = (MenuItem)selectionEvent.widget;
            if (menuItem.getSelection())
            {
              Settings.role = Roles.NORMAL;
              shell.dispose();
            }
          }
        });

        menuItem = Widgets.addMenuItemRadio(subMenu,BARControl.tr("Expert"));
        menuItem.setSelection(Settings.role == Roles.EXPERT);
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem menuItem = (MenuItem)selectionEvent.widget;
            if (menuItem.getSelection())
            {
              Settings.role = Roles.EXPERT;
              shell.dispose();
            }
          }
        });
      }

      Widgets.addMenuItemSeparator(menu);

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
            try
            {
              BARServer.executeCommand(StringParser.format("DEBUG_PRINT_STATISTICS"),0);
            }
            catch (BARException exception)
            {
              // ignored
            }
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
            Background.run(new BackgroundRunnable(busyDialog)
            {
              public void run(final BusyDialog busyDialog)
              {
                // dump memory info
                try
                {
                  BARServer.executeCommand(StringParser.format("DEBUG_PRINT_MEMORY_INFO"),
                                           1,  // debugLevel
                                           new Command.ResultHandler()
                                           {
                                             @Override
                                             public void handle(int i, ValueMap valueMap)
                                               throws BARException
                                             {
                                               String type  = valueMap.getString("type");
                                               long   n     = valueMap.getLong("n");
                                               long   count = valueMap.getLong("count");

                                               busyDialog.updateText(String.format("Printing '%s' info...",type));
                                               if (count > 0)
                                               {
                                                 busyDialog.updateProgressBar(((double)n*100.0)/(double)count);
                                               }

                                               if (busyDialog.isAborted())
                                               {
                                                 abort();
                                               }
                                             }
                                           }
                                          );
                }
                catch (BARException exception)
                {
                  // ignored
                }
                finally
                {
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
              }
            });
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

            final BusyDialog busyDialog = new BusyDialog(shell,"Store debug memory dump",500,100,null,BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.ABORT_CLOSE);
            Background.run(new BackgroundRunnable(busyDialog)
            {
              public void run(final BusyDialog busyDialog)
              {
                // dump memory info
                try
                {
                  BARServer.executeCommand(StringParser.format("DEBUG_DUMP_MEMORY_INFO"),
                                           1,  // debugLevel
                                           new Command.ResultHandler()
                                           {
                                             @Override
                                             public void handle(int i, ValueMap valueMap)
                                               throws BARException
                                             {
                                               String type  = valueMap.getString("type");
                                               long   n     = valueMap.getLong("n");
                                               long   count = valueMap.getLong("count");

                                               busyDialog.updateText(String.format("Dumping '%s' info...",type));
                                               if (count > 0)
                                               {
                                                 busyDialog.updateProgressBar(((double)n*100.0)/(double)count);
                                               }

                                               if (busyDialog.isAborted())
                                               {
                                                 abort();
                                               }
                                             }
                                           }
                                          );
                }
                catch (BARException exception)
                {
                  // ignored
                }
                finally
                {
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
              }
            });
          }
        });
      }
    }

    updateServerMenu();
  }

  /** get terminal width
   * @return terminal width or 78 as default
   */
  private int getTerminalWidth()
  {
    int width = 0;

/* jline 3.9.0
    try
    {
      width = TerminalBuilder.terminal().getWidth();
    }
    catch (IOException exception)
    {
      width = 78;
    }
*/
    width = jline.TerminalFactory.get().getWidth();

    return width;
  }

  /** try reconnect
   * @param message error message
   * @return TRUE iff reconnected
   */
  private boolean tryReconnect(String message)
  {
    boolean connectOkFlag = false;

    // try to reconnect
    if (Dialogs.confirmError(new Shell(),BARControl.tr("Connection lost"),BARControl.tr("Error: ")+message,BARControl.tr("Try again"),BARControl.tr("Cancel")))
    {
      // try to connect to last server
      while (!connectOkFlag)
      {
        try
        {
          BARServer.connect(display,
                            loginData.serverName,
                            loginData.serverPort,
                            loginData.serverTLSPort,
                            loginData.forceSSL,
                            loginData.password,
                            Settings.serverCAFileName,
                            Settings.serverCertificateFileName,
                            Settings.serverKeyFileName
                           );
          connectOkFlag = true;
        }
        catch (ConnectionError reconnectError)
        {
          if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+reconnectError.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
          {
            quitFlag = true;
            break;
          }
        }
        catch (CommunicationError reconnectError)
        {
          if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+reconnectError.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
          {
            quitFlag = true;
            break;
          }
        }
      }

      // try to connect to new server
      while (   !connectOkFlag
             && getLoginData(loginData,false)
             && ((loginData.serverPort != 0) || (loginData.serverTLSPort != 0))
            )
      {
        // try to connect to server
        try
        {
          BARServer.connect(display,
                            loginData.serverName,
                            loginData.serverPort,
                            loginData.serverTLSPort,
                            loginData.forceSSL,
                            loginData.password,
                            Settings.serverCAFileName,
                            Settings.serverCertificateFileName,
                            Settings.serverKeyFileName
                           );
          connectOkFlag = true;
        }
        catch (ConnectionError reconnectError)
        {
          if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+reconnectError.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
          {
            quitFlag = true;
            break;
          }
        }
        catch (CommunicationError reconnectError)
        {
          if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+reconnectError.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
          {
            quitFlag = true;
            break;
          }
        }
      }
    }

    // refresh or stop if not connected
    if (connectOkFlag)
    {
      // SWT bug/limitation work-around: current tab is not refreshed, force refresh by switching tabs
      int currentTabItemIndex = tabFolder.getSelectionIndex();
      if (currentTabItemIndex == 0)
      {
        tabFolder.setSelection(1);
      }
      else
      {
        tabFolder.setSelection(0);
      }
      tabFolder.setSelection(currentTabItemIndex);

      // notifiy new server
      Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);
    }
    else
    {
      // stop
      quitFlag = true;
    }

    return connectOkFlag;
  }

  /** run application
   */
  private void run()
  {
    // set window size+title, manage window (approximate height according to height of a text line)
    shell.setSize(840,600+5*(Widgets.getTextHeight(shell)+4));
    shell.open();
    shell.setSize(840,600+5*(Widgets.getTextHeight(shell)+4));
    shell.setText("BAR control "+BARServer.getMode()+": "+BARServer.getInfo());

    // listeners
    shell.addListener(BARControl.USER_EVENT_NEW_SERVER,new Listener()
    {
      public void handleEvent(Event event)
      {
        Shell widget = (Shell)event.widget;

        widget.setText("BAR control "+BARServer.getMode()+": "+BARServer.getInfo());

        updateMaster();
      }
    });
    shell.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        quitFlag = true;
        shell.dispose();
      }
    });

    // pre-select job
    if (Settings.selectedJobName != null)
    {
      JobData jobData = tabStatus.getJobByName(Settings.selectedJobName);
      if (jobData != null)
      {
        Widgets.notify(shell,BARControl.USER_EVENT_NEW_JOB,jobData);
      }
    }

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
        connectOkFlag = tryReconnect(error.getMessage());
      }
      catch (SWTException exception)
      {
        Throwable connectionError = exception.getCause();
        while ((connectionError != null) && !(connectionError instanceof ConnectionError))
        {
          connectionError = connectionError.getCause();
        }
        if (connectionError != null)
        {
          connectOkFlag = tryReconnect(exception.getCause().getMessage());
        }
        else
        {
          printInternalError(exception);
          showFatalError(exception);
          System.exit(EXITCODE_INTERNAL_ERROR);
        }
      }
      catch (AssertionError error)
      {
        printInternalError(error);
        showFatalError(error);
        System.exit(BARControl.EXITCODE_INTERNAL_ERROR);
      }
      catch (InternalError error)
      {
        printInternalError(error);
        showFatalError(error);
        System.exit(EXITCODE_INTERNAL_ERROR);
      }
      catch (Throwable throwable)
      {
        if (Settings.debugLevel > 0)
        {
          printInternalError(throwable);
        }
        showFatalError(throwable);
        System.exit(EXITCODE_INTERNAL_ERROR);
      }
    }
  }

  /** login server/password dialog
   * @param loginData server login data
   * @param roleFlag true to select role, false otherwise
   * @return true iff login data ok, false otherwise
   */
  private boolean getLoginData(final LoginData loginData, final boolean roleFlag)
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

    final Combo   widgetServerName;
    final Spinner widgetServerPort;
    final Button  widgetForceSSL;
    final Text    widgetPassword;
    final Button  widgetRoleBasic,widgetRoleNormal,widgetRoleExpert;
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
        widgetServerName = Widgets.newCombo(subComposite,SWT.LEFT|SWT.BORDER);
        widgetServerName.setItems(serverData);
        if (loginData.serverName != null) widgetServerName.setText(loginData.serverName);
        Widgets.layout(widgetServerName,0,0,TableLayoutData.WE);

        widgetServerPort = Widgets.newSpinner(subComposite,SWT.RIGHT|SWT.BORDER);
        widgetServerPort.setMinimum(0);
        widgetServerPort.setMaximum(65535);
        widgetServerPort.setSelection(loginData.serverPort);
        Widgets.layout(widgetServerPort,0,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

        widgetForceSSL = Widgets.newCheckbox(subComposite,BARControl.tr("SSL"));
        widgetForceSSL.setSelection(loginData.forceSSL);
        Widgets.layout(widgetForceSSL,0,2,TableLayoutData.W);
      }

      label = Widgets.newLabel(composite);
      label.setText(BARControl.tr("Password")+":");
      Widgets.layout(label,1,0,TableLayoutData.W);

      widgetPassword = Widgets.newPassword(composite);
      if ((loginData.password != null) && !loginData.password.isEmpty()) widgetPassword.setText(loginData.password);
      Widgets.layout(widgetPassword,1,1,TableLayoutData.WE);

      if (roleFlag)
      {
        label = Widgets.newLabel(composite);
        label.setText(BARControl.tr("Role")+":");
        Widgets.layout(label,2,0,TableLayoutData.W);

        subComposite = new Composite(composite,SWT.NONE);
        subComposite.setLayout(new TableLayout(null,0.0,2));
        subComposite.setLayoutData(new TableLayoutData(2,1,TableLayoutData.WE));
        {
          widgetRoleBasic = Widgets.newRadio(subComposite,BARControl.tr("Basic"));
          widgetRoleBasic.setSelection(loginData.role == Roles.BASIC);
          Widgets.layout(widgetRoleBasic,0,0,TableLayoutData.W);

          widgetRoleNormal = Widgets.newRadio(subComposite,BARControl.tr("Normal"));
          widgetRoleNormal.setSelection(loginData.role == Roles.NORMAL);
          Widgets.layout(widgetRoleNormal,0,1,TableLayoutData.W);

          widgetRoleExpert = Widgets.newRadio(subComposite,BARControl.tr("Expert"));
          widgetRoleExpert.setSelection(loginData.role == Roles.EXPERT);
          Widgets.layout(widgetRoleExpert,0,2,TableLayoutData.W);
        }
      }
      else
      {
        widgetRoleBasic  = null;
        widgetRoleNormal = null;
        widgetRoleExpert = null;
      }
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE));
    {
      widgetLoginButton = Widgets.newButton(composite);
      widgetLoginButton.setText(BARControl.tr("Login"));
      Widgets.layout(widgetLoginButton,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);
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
          loginData.forceSSL   = widgetForceSSL.getSelection();
          loginData.password   = widgetPassword.getText();
          if (roleFlag)
          {
            if      (widgetRoleBasic.getSelection() ) loginData.role = Roles.BASIC;
            else if (widgetRoleNormal.getSelection()) loginData.role = Roles.NORMAL;
            else if (widgetRoleExpert.getSelection()) loginData.role = Roles.EXPERT;
            else                                      loginData.role = Roles.BASIC;
          }
          Dialogs.close(dialog,true);
        }
      });

      button = Widgets.newButton(composite);
      button.setText(BARControl.tr("Cancel"));
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
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
                         widgetForceSSL,
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
      MenuItem menuItem = Widgets.addMenuItemRadio(serverMenu,server.name+":"+server.port);
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
            String  errorMessage  = null;

            // try to connect to server with current credentials
            if (!connectOkFlag)
            {
              loginData = new LoginData((!server.name.isEmpty()    ) ? server.name     : Settings.DEFAULT_SERVER_NAME,
                                        (server.port != 0          ) ? server.port     : Settings.DEFAULT_SERVER_PORT,
                                        (server.port != 0          ) ? server.port     : Settings.DEFAULT_SERVER_TLS_PORT,
                                        Settings.forceSSL,
                                        (!server.password.isEmpty()) ? server.password : "",
                                        Settings.role
                                       );
              try
              {
                BARServer.connect(display,
                                  loginData.serverName,
                                  loginData.serverPort,
                                  loginData.serverTLSPort,
                                  loginData.forceSSL,
                                  loginData.password,
                                  Settings.serverCAFileName,
                                  Settings.serverCertificateFileName,
                                  Settings.serverKeyFileName
                                 );
                connectOkFlag = true;
              }
              catch (ConnectionError error)
              {
                if ((errorMessage == null) && (error.getMessage() != null)) errorMessage = error.getMessage();
              }
              catch (CommunicationError error)
              {
                if ((errorMessage == null) && (error.getMessage() != null)) errorMessage = error.getMessage();
              }
            }

            // try to connect to server without TLS/SSL
            if (!connectOkFlag && loginData.forceSSL)
            {
              if (Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Connection fail. Try to connect without TLS/SSL?"),BARControl.tr("Try without TLS/SSL"),BARControl.tr("Cancel")))
              {
                try
                {
                  BARServer.connect(display,
                                    loginData.serverName,
                                    loginData.serverPort,
                                    loginData.serverTLSPort,
                                    false,  // forceSSL
                                    loginData.password,
                                    (String)null,  // serverCAFileName
                                    (String)null,  // serverCertificateFileName
                                    (String)null  // serverKeyFileName
                                   );
                  connectOkFlag = true;
                }
                catch (ConnectionError error)
                {
                  if ((errorMessage == null) && (error.getMessage() != null)) errorMessage = error.getMessage();
                }
                catch (CommunicationError error)
                {
                  if ((errorMessage == null) && (error.getMessage() != null)) errorMessage = error.getMessage();
                }
              }
            }

            // try to connect to server with new credentials
            if (!connectOkFlag)
            {
              loginData = new LoginData((!server.name.isEmpty()) ? server.name : Settings.DEFAULT_SERVER_NAME,
                                        (server.port != 0      ) ? server.port : Settings.DEFAULT_SERVER_PORT,
                                        (server.port != 0      ) ? server.port : Settings.DEFAULT_SERVER_TLS_PORT,
                                        Settings.forceSSL,
                                        Settings.role
                                       );
              if (getLoginData(loginData,false))
              {
                try
                {
                  BARServer.connect(display,
                                    loginData.serverName,
                                    loginData.serverPort,
                                    loginData.serverTLSPort,
                                    loginData.forceSSL,
                                    loginData.password,
                                    Settings.serverCAFileName,
                                    Settings.serverCertificateFileName,
                                    Settings.serverKeyFileName
                                   );
                  connectOkFlag = true;
                }
                catch (ConnectionError error)
                {
                  if ((errorMessage == null) && (error.getMessage() != null)) errorMessage = error.getMessage();
                }
                catch (CommunicationError error)
                {
                  if ((errorMessage == null) && (error.getMessage() != null)) errorMessage = error.getMessage();
                }
              }
            }

            if (connectOkFlag)
            {
              updateServerMenu();

              // notify new server
              Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);
            }
            else
            {
              if (errorMessage != null)
              {
                Dialogs.error(new Shell(),BARControl.tr("Connection fail")+":\n\n"+errorMessage);
              }
              else
              {
                Dialogs.error(new Shell(),BARControl.tr("Connection fail"));
              }
            }
          }
        }
      });
    }
  }

  /** pair new master
   * @return true iff paired, false otherwise
   */
  private boolean pairMaster()
  {
    class Data
    {
      String masterName;

      Data()
      {
        this.masterName = "";
      }
    };

    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite,subComposite;
    Label           label;
    Button          button;

    final Data data = new Data();

    final Shell dialog = Dialogs.openModal(new Shell(),BARControl.tr("Pair new master"),250,SWT.DEFAULT);

    final BackgroundRunnable pairMasterRunnable[] = {null};

    final ProgressBar widgetProgressBar;
    final Label       widgetMasterName;
    final Button      widgetRestartButton;
    final Button      widgetOKButton;

    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},2));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));
    {
      label = Widgets.newLabel(composite);
      label.setText(BARControl.tr("Wait for pairing")+":");
      label.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W));

      widgetProgressBar = new ProgressBar(composite);
      widgetProgressBar.setLayoutData(new TableLayoutData(0,1,TableLayoutData.WE));

      label = Widgets.newLabel(composite);
      label.setText(BARControl.tr("Master")+":");
      label.setLayoutData(new TableLayoutData(1,0,TableLayoutData.W));

      widgetMasterName = Widgets.newView(composite);
      widgetMasterName.setLayoutData(new TableLayoutData(1,1,TableLayoutData.WE));
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE));
    {
      widgetOKButton = Widgets.newButton(composite);
      widgetOKButton.setText(BARControl.tr("OK"));
      widgetOKButton.setEnabled(false);
      Widgets.layout(widgetOKButton,0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT);
      widgetOKButton.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,true);
        }
      });

      widgetRestartButton = Widgets.newButton(composite);
      widgetRestartButton.setText(BARControl.tr("Restart"));
      widgetRestartButton.setEnabled(false);
      Widgets.layout(widgetRestartButton,0,1,TableLayoutData.NONE,0,0,0,0,60,SWT.DEFAULT);
      widgetRestartButton.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          widgetOKButton.setEnabled(false);
          widgetRestartButton.setEnabled(false);
          Background.run(pairMasterRunnable[0]);
        }
      });

      button = Widgets.newButton(composite);
      button.setText(BARControl.tr("Cancel"));
      Widgets.layout(button,0,2,TableLayoutData.E,0,0,0,0,60,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Dialogs.close(dialog,false);
        }
      });
    }

    // install handlers

    // set new master
    pairMasterRunnable[0] = new BackgroundRunnable(dialog,widgetProgressBar)
    {
      public void run(final Shell dialog, final ProgressBar widgetProgressBar)
      {
        try
        {
Dprintf.dprintf("starafsf");
          BARServer.executeCommand(StringParser.format("MASTER_WAIT"),
                                   1,  // debugLevel
                                   new Command.ResultHandler()
                                   {
                                     @Override
                                     public void handle(int i, ValueMap valueMap)
                                       throws BARException
                                     {
                                       final int restTime  = valueMap.getInt("restTime" );
                                       final int totalTime = valueMap.getInt("totalTime");

                                       display.syncExec(new Runnable()
                                       {
                                         public void run()
                                         {
                                           widgetProgressBar.setRange(0,totalTime);
                                           widgetProgressBar.setSelection("%.0fs",restTime);
                                         }
                                       });
                                     }
                                   },
                                   new Command.Handler()
                                   {
                                     @Override
                                     public void handle(int errorCode, String errorData, ValueMap valueMap)
                                     {
                                       data.masterName = valueMap.getString("name");

                                       // enable/disable OK button
                                       display.syncExec(new Runnable()
                                       {
                                         public void run()
                                         {
                                           widgetMasterName.setText(data.masterName);

                                           if (!widgetOKButton.isDisposed() && !data.masterName.isEmpty())
                                           {
                                             widgetOKButton.setEnabled(true);
                                           }
                                         }
                                       });
                                     }
                                   }
                                  );
        }
        catch (final BARException exception)
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              Dialogs.close(dialog,false);
              Dialogs.error(shell,BARControl.tr("Cannot set new master:\n\n")+exception.getText());
            }
          });
          return;
        }

        // enable/disable restart button
        display.syncExec(new Runnable()
        {
          public void run()
          {
            if (!widgetRestartButton.isDisposed())
            {
              widgetRestartButton.setEnabled(true);
            }
          }
        });
      }
    };
    Background.run(pairMasterRunnable[0]);

    // run dialog
    Boolean result = (Boolean)Dialogs.run(dialog);
    if ((result != null) && result)
    {
      if (!data.masterName.isEmpty())
      {
        try
        {
          BARServer.setMaster();
          result = true;
        }
        catch (final BARException exception)
        {
          display.syncExec(new Runnable()
          {
            public void run()
            {
              Dialogs.close(dialog,false);
              Dialogs.error(shell,BARControl.tr("Cannot set new master:\n\n")+exception.getText());
            }
          });
          result = false;
        }
      }
      else
      {
        result = false;
      }
    }
    else
    {
      result = false;
    }

    updateMaster();

    return result;
  }

  /** clear paired master
   * @return true iff cleared, false otherwise
   */
  private boolean clearMaster()
  {
    try
    {
      BARServer.clearMaster();
      updateMaster();

      return true;
    }
    catch (BARException exception)
    {
      Dialogs.error(shell,BARControl.tr("Cannot clear master:\n\n")+exception.getText());
      return false;
    }
  }

  /** update master info
   */
  private void updateMaster()
  {
    String name = BARServer.getMaster();

    if (!name.isEmpty())
    {
      masterMenuItem.setText(BARControl.tr("Master")+": "+name);
      masterMenuItem.setSelection(true);
    }
    else
    {
      masterMenuItem.setText(BARControl.tr("Pair master")+"\u2026");
      masterMenuItem.setSelection(false);
    }
  }

  /** barcontrol main
   * @param args command line arguments
   */
  BARControl(String[] args)
  {
    final SimpleDateFormat DATE_FORMAT = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");

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
      if ((server == null)) server = Settings.getServer(Settings.serverName,(Settings.serverPort    != -1) ? Settings.serverPort    : Settings.DEFAULT_SERVER_PORT    );
      if ((server == null)) server = Settings.getServer(Settings.serverName,(Settings.serverTLSPort != -1) ? Settings.serverTLSPort : Settings.DEFAULT_SERVER_TLS_PORT);
      loginData = new LoginData((server != null) ? server.name     : Settings.DEFAULT_SERVER_NAME,
                                (server != null) ? server.port     : Settings.DEFAULT_SERVER_PORT,
                                (server != null) ? server.port     : Settings.DEFAULT_SERVER_TLS_PORT,
                                Settings.forceSSL,
                                (server != null) ? server.password : "",
                                Settings.role
                               );
      if (Settings.serverName     != null) loginData.serverName    = Settings.serverName;
      if (Settings.serverPort     != -1  ) loginData.serverPort    = Settings.serverPort;
      if (Settings.serverTLSPort  != -1  ) loginData.serverTLSPort = Settings.serverTLSPort;
      if (Settings.serverPassword != null) loginData.password      = Settings.serverPassword;

      // commands
      if (   (Settings.pairMasterFlag)
          || (Settings.runJobName != null)
          || (Settings.abortJobName != null)
          || (Settings.pauseTime > 0)
          || (Settings.pingFlag)
          || (Settings.suspendFlag)
          || (Settings.continueFlag)
          || (Settings.listFlag)
          || (Settings.indexDatabaseAddStorageName != null)
          || (Settings.indexDatabaseRemoveStorageName != null)
          || (Settings.indexDatabaseRefreshStorageName != null)
          || (Settings.indexDatabaseEntitiesListName != null)
          || (Settings.indexDatabaseStoragesListName != null)
          || (Settings.indexDatabaseEntriesListName != null)
          || Settings.indexDatabaseHistoryList
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
                            loginData.forceSSL,
                            loginData.password,
                            Settings.serverCAFileName,
                            Settings.serverCertificateFileName,
                            Settings.serverKeyFileName
                           );
        }
        catch (ConnectionError error)
        {
          printError("cannot connect to server (error: %s)",error.getMessage());
          System.exit(EXITCODE_FAIL);
        }

        // execute commands
        if (Settings.pairMasterFlag)
        {
          System.out.print("Wait for pairing new master...    ");

          // set new master
          try
          {
            final String masterName[] = new String[]{""};
            BARServer.executeCommand(StringParser.format("MASTER_SET"),
                                     1,  // debugLevel
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         final int restTime = valueMap.getInt("restTime",0);

                                         System.out.print(String.format("\b\b\b\b%3ds",restTime));
                                       }
                                     },
                                     new Command.Handler()
                                     {
                                       @Override
                                       public void handle(int errorCode, String errorData, ValueMap valueMap)
                                       {
                                         masterName[0] = valueMap.getString("name");
                                       }
                                     }
                                    );

  //TODO
  /*
            // wait for pairing
            int          time         = PAIRING_MASTER_TIMEOUT;
            while (masterName[0].isEmpty() && (time > 0))
            {
              // update rest time
              System.out.print(String.format("\b\b\b\b%3ds",time));

              // check if master paired
              error = BARServer.executeCommand(StringParser.format("MASTER_GET"),
                                               1,  // debugLevel
                                               new Command.ResultHandler()
                                               {
                                                 @Override
                                                 public void handle(int i, ValueMap valueMap)
                                                   throws BARException
                                                 public int handle(int i, ValueMap valueMap)
                                                 {
                                                   masterName[0] = valueMap.getString("name");

                                                   return BARException.NONE;
                                                 }
                                               }
                                              );
              if (error.code != BARException.NONE)
              {
                printError("Cannot get master pairing name ("+error.getText()+")");
                BARServer.disconnect();
                System.exit(EXITCODE_FAIL);
              }

              // sleep
              try { Thread.sleep(1000); } catch (InterruptedException exception) {  }
              time--;
            }
  */
            System.out.print("\b\b\b\b");
            if (!masterName[0].isEmpty())
            {
              System.out.println(String.format("'%s' - OK",masterName[0]));
            }
            else
            {
              System.out.println("FAIL!");
            }
          }
          catch (BARException exception)
          {
            printError("Cannot set new master ("+exception.getText()+")");
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.runJobName != null)
        {
          // get job UUID
          String jobUUID = getJobUUID(Settings.runJobName);
          if (jobUUID == null)
          {
            printError("job '%s' not found",Settings.runJobName);
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }

          // start job
          try
          {
            BARServer.executeCommand(StringParser.format("JOB_START jobUUID=%s archiveType=%s dryRun=no",
                                                         jobUUID,
                                                         Settings.archiveType.toString()
                                                        ),
                                     1  // debug level
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot start job '%s' (error: %s)",Settings.runJobName,exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.pauseTime > 0)
        {
          // pause
          try
          {
            BARServer.executeCommand(StringParser.format("PAUSE time=%d modeMask=%s",
                                                         Settings.pauseTime,
                                                         "ALL"
                                                        ),
                                     1  // debug level
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot pause (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.pingFlag)
        {
          // nothing to do
        }

        if (Settings.suspendFlag)
        {
          // suspend
          try
          {
            BARServer.executeCommand(StringParser.format("SUSPEND modeMask=CREATE"),
                                     1  // debug level
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot suspend (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.continueFlag)
        {
          // continue
          try
          {
            BARServer.executeCommand(StringParser.format("CONTINUE"),
                                     1  // debug level
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot continue (error: %s)",Settings.runJobName,exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.abortJobName != null)
        {
          // get job id
          String jobUUID = getJobUUID(Settings.abortJobName);
          if (jobUUID == null)
          {
            printError("job '%s' not found",Settings.abortJobName);
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }

          // abort job
          try
          {
            BARServer.executeCommand(StringParser.format("JOB_ABORT jobUUID=%s",
                                                         jobUUID
                                                        ),
                                     1  // debug level
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot abort job '%s' (error: %s)",Settings.abortJobName,exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.listFlag)
        {
          final int           n[]          = new int[]{0};

          // get server state
          String serverState = null;
          try
          {
            ValueMap valueMap = new ValueMap();
            BARServer.executeCommand(StringParser.format("STATUS"),
                                     1,  // debug level
                                     valueMap
                                    );
            serverState = valueMap.getString("state");
          }
          catch (BARException exception)
          {
            printError("cannot get state (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
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
            printWarning("unknown server response '%s'",serverState);
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }

          // get joblist
          try
          {
            ArrayList<ValueMap> valueMapList = new ArrayList<ValueMap>();
            BARServer.executeCommand(StringParser.format("JOB_LIST"),
                                     1,  // debug level
                                     valueMapList
                                    );
            System.out.println(String.format("%-32s %-20s %-10s %-12s %-14s %-25s %-14s %-10s %-8s %-19s %-12s",
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
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
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

              System.out.println(String.format("%-32s %-20s %-10s %-12s %14d %-25s %-14s %-10s %-8s %-19s %12d",
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
              n[0]++;
            }
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            System.out.println(String.format("%d jobs",n[0]));
          }
          catch (BARException exception)
          {
            printError("cannot get job list (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.indexDatabaseAddStorageName != null)
        {
          // add index for storage
          try
          {
            BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ADD pattern=%'S patternType=GLOB",
                                                         Settings.indexDatabaseAddStorageName
                                                        ),
                                     1  // debug level
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot add index for storage '%s' to index (error: %s)",Settings.indexDatabaseAddStorageName,exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.indexDatabaseRefreshStorageName != null)
        {
          // remote index for storage
          try
          {
            BARServer.executeCommand(StringParser.format("INDEX_REFRESH name=%'S",
                                                         Settings.indexDatabaseRefreshStorageName
                                                        ),
                                     1  // debug level
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot refresh index for storage '%s' from index (error: %s)",Settings.indexDatabaseRefreshStorageName,exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.indexDatabaseRemoveStorageName != null)
        {
          // remote index for storage
          try
          {
            BARServer.executeCommand(StringParser.format("INDEX_REMOVE pattern=%'S patternType=GLOB",
                                                         Settings.indexDatabaseRemoveStorageName
                                                        ),
                                     1  // debug level
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot remove index for storage '%s' from index (error: %s)",Settings.indexDatabaseRemoveStorageName,exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.indexDatabaseEntitiesListName != null)
        {
          // list storage index
          final String[] MAP_TEXT = new String[]{"\\n","\\r","\\\\"};
          final String[] MAP_BIN  = new String[]{"\n","\r","\\"};

          try
          {
            final int n[] = new int[]{0};

            System.out.println(String.format("%-8s %-12s %-14s %-14s %-19s %s",
                                             "Id",
                                             "Type",
                                             "Entry count",
                                             "Entry size",
                                             "Date/Time",
                                             "Job"
                                            )
                              );
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            BARServer.executeCommand(StringParser.format("INDEX_ENTITY_LIST indexStateSet=* indexModeSet=* name=%'S",
                                                         !Settings.indexDatabaseEntitiesListName.isEmpty() ? Settings.indexDatabaseEntitiesListName : ""
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         long         entityId        = valueMap.getLong  ("entityId"           );
                                         String       jobName         = valueMap.getString("jobName"            );
                                         ArchiveTypes archiveType     = valueMap.getEnum  ("archiveType",ArchiveTypes.class);
                                         long         createdDateTime = valueMap.getLong  ("createdDateTime"    );
                                         long         totalEntryCount = valueMap.getLong  ("totalEntryCount"    );
                                         long         totalEntrySize  = valueMap.getLong  ("totalEntrySize"     );

                                         System.out.println(String.format("%8d %-12s %14d %14d %-19s %s",
                                                                          getDatabaseId(entityId),
                                                                          archiveType.toString(),
                                                                          totalEntryCount,
                                                                          totalEntrySize,
                                                                          (createdDateTime > 0L) ? DATE_FORMAT.format(new Date(createdDateTime*1000)) : "-",
                                                                          jobName
                                                                         )
                                                           );
                                         n[0]++;
                                       }
                                     }
                                    );
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            System.out.println(String.format("%d entities",n[0]));
          }
          catch (BARException exception)
          {
            printError("cannot list storages index (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.indexDatabaseStoragesListName != null)
        {
          // list storage index
          final String[] MAP_TEXT = new String[]{"\\n","\\r","\\\\"};
          final String[] MAP_BIN  = new String[]{"\n","\r","\\"};

          try
          {
            final int n[] = new int[]{0};

            System.out.println(String.format("%-8s %-14s %-19s %-16s %-5s %s",
                                             "Id",
                                             "Size",
                                             "Date/Time",
                                             "State",
                                             "Mode",
                                             "Name"
                                            )
                              );
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=* indexStateSet=* indexModeSet=* name=%'S",
                                                         Settings.indexDatabaseStoragesListName
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         long        storageId   = valueMap.getLong  ("storageId"                   );
                                         String      storageName = valueMap.getString("name"                        );
                                         long        dateTime    = valueMap.getLong  ("dateTime"                    );
                                         long        size        = valueMap.getLong  ("size"                        );
                                         IndexStates state       = valueMap.getEnum  ("indexState",IndexStates.class);
                                         IndexModes  mode        = valueMap.getEnum  ("indexMode",IndexModes.class  );

                                         System.out.println(String.format("%8d %14d %-19s %-16s %-5s %s",
                                                                          getDatabaseId(storageId),
                                                                          size,
                                                                          DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                          state,
                                                                          mode,
                                                                          storageName
                                                                         )
                                                           );
                                         n[0]++;
                                       }
                                     }
                                    );
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            System.out.println(String.format("%d storages",n[0]));
          }
          catch (BARException exception)
          {
            printError("cannot list storages index (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.indexDatabaseEntriesListName != null)
        {
          // list storage index
          try
          {
            final int n[] = new int[]{0};

            System.out.println(String.format("%-8s %-40s %-8s %-14s %-19s %s",
                                             "Id",
                                             "Storage",
                                             "Type",
                                             "Size",
                                             "Date/Time",
                                             "Name"
                                            )
                              );
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST indexType=* newestOnly=%y name=%'S limit=1024",
                                                         Settings.indexDatabaseEntriesNewestOnly,
                                                         Settings.indexDatabaseEntriesListName
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         long   entryId         = valueMap.getLong  ("entryId"        );
                                         String storageName     = valueMap.getString("storageName"    );
                                         long   storageDateTime = valueMap.getLong  ("storageDateTime");

                                         switch (valueMap.getEnum("entryType",EntryTypes.class))
                                         {
                                           case FILE:
                                             {
                                               String fileName        = valueMap.getString("name"           );
                                               long   size            = valueMap.getLong  ("size"           );
                                               long   dateTime        = valueMap.getLong  ("dateTime"       );
                                               long   fragmentOffset  = valueMap.getLong  ("fragmentOffset" );
                                               long   fragmentSize    = valueMap.getLong  ("fragmentSize"   );

                                               System.out.println(String.format("%8d %-40s %-8s %14d %-19s %s",
                                                                                entryId,
                                                                                storageName,
                                                                                "FILE",
                                                                                size,
                                                                                DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                fileName
                                                                               )
                                                                 );
                                               n[0]++;
                                             }
                                             break;
                                           case IMAGE:
                                             {
                                               String imageName       = valueMap.getString("name"           );
                                               long   size            = valueMap.getLong  ("size"           );
                                               long   blockOffset     = valueMap.getLong  ("blockOffset"    );
                                               long   blockCount      = valueMap.getLong  ("blockCount"     );

                                               System.out.println(String.format("%8d %-40s %-8s %14d %-19s %s",
                                                                                entryId,
                                                                                storageName,
                                                                                "IMAGE",
                                                                                size,
                                                                                "",
                                                                                imageName
                                                                               )
                                                                 );
                                               n[0]++;
                                             }
                                             break;
                                           case DIRECTORY:
                                             {
                                               String directoryName   = valueMap.getString("name"           );
                                               long   dateTime        = valueMap.getLong  ("dateTime"       );

                                               System.out.println(String.format("%8d %-40s %-8s %14s %-19s %s",
                                                                                entryId,
                                                                                storageName,
                                                                                "DIR",
                                                                                "",
                                                                                DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                directoryName
                                                                               )
                                                                 );
                                               n[0]++;
                                             }
                                             break;
                                           case LINK:
                                             {
                                               String linkName        = valueMap.getString("name"           );
                                               String destinationName = valueMap.getString("destinationName");
                                               long   dateTime        = valueMap.getLong  ("dateTime"       );

                                               System.out.println(String.format("%8d %-40s %-8s %14s %-19s %s -> %s",
                                                                                entryId,
                                                                                storageName,
                                                                                "LINK",
                                                                                "",
                                                                                DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                linkName,
                                                                                destinationName
                                                                               )
                                                                 );
                                               n[0]++;
                                             }
                                             break;
                                           case HARDLINK:
                                             {
                                               String fileName        = valueMap.getString("name"           );
                                               long   size            = valueMap.getLong  ("size"           );
                                               long   dateTime        = valueMap.getLong  ("dateTime"       );
                                               long   fragmentOffset  = valueMap.getLong  ("fragmentOffset" );
                                               long   fragmentSize    = valueMap.getLong  ("fragmentSize"   );

                                               System.out.println(String.format("%8d %-40s %-8s %14d %-19s %s",
                                                                                entryId,
                                                                                storageName,
                                                                                "HARDLINK",
                                                                                size,
                                                                                DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                fileName
                                                                               )
                                                                 );
                                               n[0]++;
                                             }
                                             break;
                                           case SPECIAL:
                                             {
                                               String name            = valueMap.getString("name"           );
                                               long   dateTime        = valueMap.getLong  ("dateTime"       );

                                               System.out.println(String.format("%8d %-40s %-8s %14s %-19s %s",
                                                                                entryId,
                                                                                storageName,
                                                                                "SPECIAL",
                                                                                "",
                                                                                DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                name
                                                                               )
                                                                 );
                                               n[0]++;
                                             }
                                             break;
                                         }
                                       }
                                     }
                                    );
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            System.out.println(String.format("%d entries",n[0]));
          }
          catch (BARException exception)
          {
            printError("cannot list entries index (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.indexDatabaseHistoryList)
        {
          // list history
          try
          {
            final int n[] = new int[]{0};

            System.out.println(String.format("%-32s %-20s %-12s %-19s %-8s %-21s %-21s %-21s %s",
                                             "Job",
                                             "Hostname",
                                             "Type",
                                             "Date/Time",
                                             "Duration",
                                             "Total         [bytes]",
                                             "Skipped       [bytes]",
                                             "Errors        [bytes]",
                                             "Message"
                                            )
                              );
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            BARServer.executeCommand(StringParser.format("INDEX_HISTORY_LIST"
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         String jobUUID           = valueMap.getString("jobUUID"            );
                                         String jobName           = valueMap.getString("jobName"            );
                                         String scheduleUUID      = valueMap.getString("scheduleUUID"       );
                                         String hostName          = valueMap.getString("hostName",        "");
                                         String archiveType       = valueMap.getString("archiveType"        );
                                         long   createdDateTime   = valueMap.getLong  ("createdDateTime"    );
                                         String errorMessage      = valueMap.getString("errorMessage"       );
                                         long   duration          = valueMap.getLong  ("duration"           );
                                         long   totalEntryCount   = valueMap.getLong  ("totalEntryCount"    );
                                         long   totalEntrySize    = valueMap.getLong  ("totalEntrySize"     );
                                         long   skippedEntryCount = valueMap.getLong  ("skippedEntryCount"  );
                                         long   skippedEntrySize  = valueMap.getLong  ("skippedEntrySize"   );
                                         long   errorEntryCount   = valueMap.getLong  ("errorEntryCount"    );
                                         long   errorEntrySize    = valueMap.getLong  ("errorEntrySize"     );

                                         System.out.println(String.format("%-32s %-20s %-12s %-14s %02d:%02d:%02d %10d %10d %10d %10d %10d %10d %s",
                                                                          !jobName.isEmpty() ? jobName : jobUUID,
                                                                          hostName,
                                                                          archiveType,
                                                                          DATE_FORMAT.format(new Date(createdDateTime*1000)),
                                                                          duration/(60*60),(duration/60)%60,duration%60,
                                                                          totalEntryCount,
                                                                          totalEntrySize,
                                                                          skippedEntryCount,
                                                                          skippedEntrySize,
                                                                          errorEntryCount,
                                                                          errorEntrySize,
                                                                          errorMessage
                                                                         )
                                                           );
                                         n[0]++;
                                       }
                                     }
                                    );
            System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            System.out.println(String.format("%d entries",n[0]));
          }
          catch (BARException exception)
          {
            printError("cannot list history (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.restoreStorageName != null)
        {
          // set archives to restore
          try
          {
            BARServer.executeCommand(StringParser.format("STORAGE_LIST_CLEAR"),
                                     1  // debug level
                                    );

            final ArrayList<Long> indexIds = new ArrayList<Long>();
            BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s indexStateSet=%s indexModeSet=%s name=%'S offset=%ld",
                                                         "*",
                                                         "*",
                                                         "*",
                                                         Settings.restoreStorageName,
                                                         0L
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         long   storageId   = valueMap.getLong  ("storageId");
                                         String storageName = valueMap.getString("name"     );

                                         indexIds.add(storageId);
                                       }
                                     }
                                    );
            if (indexIds.isEmpty())
            {
              throw new BARException(BARException.ARCHIVE_NOT_FOUND,Settings.restoreStorageName);
            }

            BARServer.executeCommand(StringParser.format("STORAGE_LIST_ADD indexIds=%s",
                                                         StringUtils.join(indexIds,',')
                                                        ),
                                     1  // debugLevel
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot set restore list (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }


          // restore
          try
          {
            BARServer.executeCommand(StringParser.format("RESTORE type=ARCHIVES destination=%'S directoryContent=%y restoreEntryMode=%s",
                                                         Settings.destination,
                                                         true,
                                                         Settings.overwriteEntriesFlag ? "OVERWRITE" : "STOP"
                                                        ),
                                     1,  // debugLevel
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
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
                                             int           error        = valueMap.getInt   ("error",BARException.NONE);
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
                                                     BARServer.executeCommand(StringParser.format("ACTION_RESULT errorCode=%d name=%S encryptType=%s encryptedPassword=%S",
                                                                                                  BARException.NONE,
                                                                                                  name,
                                                                                                  BARServer.getPasswordEncryptType(),
                                                                                                  BARServer.encryptPassword(new String(password))
                                                                                                 ),
                                                                              1  // debugLevel
                                                                             );
                                                   }
                                                   else
                                                   {
                                                     BARServer.executeCommand(StringParser.format("ACTION_RESULT errorCode=%d",
                                                                                                  BARException.NO_PASSWORD
                                                                                                 ),
                                                                              1  // debugLevel
                                                                             );
                                                   }
                                                 }
                                                 else
                                                 {
                                                   System.out.println(BARControl.tr("Please enter {0} password for: {1}",passwordType,passwordText));
                                                   char password[] = console.readPassword("  "+BARControl.tr("Password")+": ");
                                                   if ((password != null) && (password.length > 0))
                                                   {
                                                     BARServer.executeCommand(StringParser.format("ACTION_RESULT errorCode=%d encryptType=%s encryptedPassword=%S",
                                                                                                  BARException.NONE,
                                                                                                  BARServer.getPasswordEncryptType(),
                                                                                                  BARServer.encryptPassword(new String(password))
                                                                                                 ),
                                                                              1  // debugLevel
                                                                             );
                                                   }
                                                   else
                                                   {
                                                     BARServer.executeCommand(StringParser.format("ACTION_RESULT errorCode=%d",
                                                                                                  BARException.NO_PASSWORD
                                                                                                 ),
                                                                              1  // debugLevel
                                                                             );
                                                   }
                                                 }
                                                 break;
                                               case REQUEST_VOLUME:
Dprintf.dprintf("still not supported");
//System.exit(EXITCODE_FAIL);
                                                 break;
                                               case CONFIRM:
                                                 System.err.println(BARControl.tr("Cannot restore ''{0}'': {1} - skipped", !entry.isEmpty() ? entry : storage,errorMessage));
                                                 BARServer.executeCommand(StringParser.format("ACTION_RESULT errorCode=%d",
                                                                                              BARException.NONE
                                                                                             ),
                                                                          1  // debugLevel
                                                                         );
                                                 break;
                                             }
                                           }
                                           else
                                           {
                                             RestoreStates state            = valueMap.getEnum  ("state",RestoreStates.class);
                                             String        storageName      = valueMap.getString("storageName","");
                                             long          storageDoneSize  = valueMap.getLong  ("storageDoneSize",0L);
                                             long          storageTotalSize = valueMap.getLong  ("storageTotalSize",0L);
                                             String        entryName        = valueMap.getString("entryName","");
                                             long          entryDoneSize    = valueMap.getLong  ("entryDoneSize",0L);
                                             long          entryTotalSize   = valueMap.getLong  ("entryTotalSize",0L);

                                             switch (state)
                                             {
                                               case RESTORED:
                                                 printError("Restored storage '%s', %d bytes",storageName,storageDoneSize);
                                                 break;
                                               case FAILED:
                                                 printError("cannot restore storage '%s'",storageName);
                                                 break;
                                             }
                                           }
                                         }
                                         catch (IllegalArgumentException exception)
                                         {
                                           if (Settings.debugLevel > 0)
                                           {
                                             System.err.println("ERROR: "+exception.getMessage());
                                             System.exit(EXITCODE_FAIL);
                                           }
                                         }
                                       }
                                     }
                                    );
          }
          catch (BARException exception)
          {
            printError("cannot restore storages (error: %s)",exception.getText());
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
          }
        }

        if (Settings.debugQuitServerFlag)
        {
          // quit server
          if (!BARServer.quit())
          {
            printError("cannot quit server");
            BARServer.disconnect();
            System.exit(EXITCODE_FAIL);
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
              BARServer.connect(display,
                                loginData.serverName,
                                loginData.serverPort,
                                loginData.serverTLSPort,
                                loginData.forceSSL,
                                loginData.password,
                                Settings.serverCAFileName,
                                Settings.serverCertificateFileName,
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
              BARServer.connect(display,
                                loginData.serverName,
                                loginData.serverPort,
                                loginData.serverTLSPort,
                                loginData.forceSSL,
                                "",  // password
                                Settings.serverCAFileName,
                                Settings.serverCertificateFileName,
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
          if (!getLoginData(loginData,true))
          {
            System.exit(EXITCODE_OK);
          }
          if ((loginData.serverPort == 0) && (loginData.serverTLSPort == 0))
          {
            throw new Error("Cannot connect to server. No server ports specified!");
          }
/// ??? host name scheck

          // try to connect to server
          String errorMessage = null;
          if (!connectOkFlag)
          {
            try
            {
              BARServer.connect(display,
                                loginData.serverName,
                                loginData.serverPort,
                                loginData.serverTLSPort,
                                loginData.forceSSL,
                                loginData.password,
                                Settings.serverCAFileName,
                                Settings.serverCertificateFileName,
                                Settings.serverKeyFileName
                               );
              connectOkFlag = true;
            }
            catch (ConnectionError error)
            {
              errorMessage = error.getMessage();
            }
            catch (CommunicationError error)
            {
              if (Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+error.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
              {
                continue;
              }
              else
              {
                System.exit(EXITCODE_FAIL);
              }
            }
          }
          if (!connectOkFlag && loginData.forceSSL)
          {
            if (Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Connection fail. Try to connect without TLS/SSL?"),BARControl.tr("Try without TLS/SSL"),BARControl.tr("Cancel")))
            {
              try
              {
                BARServer.connect(display,
                                  loginData.serverName,
                                  loginData.serverPort,
                                  loginData.serverTLSPort,
                                  false,  // forceSSL
                                  loginData.password,
                                  (String)null,  // serverCAFileName
                                  (String)null,  // serverCertificateFileName
                                  (String)null  // serverKeyFileName
                                 );
                connectOkFlag = true;
              }
              catch (ConnectionError error)
              {
                errorMessage = error.getMessage();
              }
              catch (CommunicationError error)
              {
                if (Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+error.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
                {
                  continue;
                }
                else
                {
                  System.exit(EXITCODE_FAIL);
                }
              }
            }
            else
            {
              System.exit(EXITCODE_FAIL);
            }
          }

          // check if connected
          if (!connectOkFlag)
          {
            if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+errorMessage,BARControl.tr("Try again"),BARControl.tr("Cancel")))
            {
              System.exit(EXITCODE_FAIL);
            }
          }
        }
        Settings.forceSSL = loginData.forceSSL;
        Settings.role     = loginData.role;

        do
        {
          // add watchdog for loaded classes/JARs
          initClassesWatchDog();

          // create main window
          createWindow();
          createTabs();
          createMenu();

          // notify new server
          Widgets.notify(shell,BARControl.USER_EVENT_NEW_SERVER);

          // run
          run();
        }
        while (!quitFlag);

        // disconnect
        BARServer.disconnect();

        // save settings
        Settings.save();
      }
    }
    catch (org.eclipse.swt.SWTException exception)
    {
      System.err.println("ERROR graphics: "+exception.getCause());
      printStackTrace(exception);
      System.err.println("Version "+VERSION);
      System.err.println("Please report this error to "+EMAIL_ADDRESS+"."); // use MAIL_AT to avoid SPAM
    }
    catch (CommunicationError error)
    {
      System.err.println("ERROR communication: "+error.getMessage());
    }
    catch (AssertionError error)
    {
      printInternalError(error);
    }
    catch (InternalError error)
    {
      printInternalError(error);
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
