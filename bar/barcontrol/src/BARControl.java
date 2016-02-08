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
import org.eclipse.swt.graphics.Color;
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
    if      (n >= 1024L*1024L*1024L*1024L) return String.format("%.1f",n/(1024L*1024L*1024L*1024L));
    else if (n >=       1024L*1024L*1024L) return String.format("%.1f",n/(      1024L*1024L*1024L));
    else if (n >=             1024L*1024L) return String.format("%.1f",n/(            1024L*1024L));
    else if (n >=                   1024L) return String.format("%.1f",n/(                  1024L));
    else                                   return String.format("%d"  ,(long)n                    );
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
    LoginData(String name, int port, int tlsPort)
    {
      final Settings.Server defaultServer = Settings.getLastServer();

      this.serverName = !name.equals("") ? name : ((defaultServer != null) ? defaultServer.name : Settings.DEFAULT_SERVER_NAME);
      this.password   = (defaultServer != null) ? defaultServer.password : "";
      this.port       = (port != 0) ? port    : ((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT);
      this.tlsPort    = (port != 0) ? tlsPort : ((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT);

      // get last used server if no name given
//      if (this.serverName.isEmpty() && (Settings.servers.size() > 0))
//      {
//        this.serverName = Settings.serverNames.toArray(new String[Settings.serverNames.size()])[Settings.serverNames.size()-1];
//      }
    }

    /** create login data
     * @param port server port
     * @param tlsPort server TLS port
     */
    LoginData(int port, int tlsPort)
    {
      this("",port,tlsPort);
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
    new Option("--select-job",                 null,Options.Types.STRING,     "selectedJobName"),
    new Option("--login-dialog",               null,Options.Types.BOOLEAN,    "loginDialogFlag"),

    new Option("--job",                        "-j",Options.Types.STRING,     "runJobName"),
    new Option("--archive-type",               null,Options.Types.ENUMERATION,"archiveType",ARCHIVE_TYPE_ENUMERATION),
    new Option("--abort",                      null,Options.Types.STRING,     "abortJobName"),
    new Option("--index-database-add",         null,Options.Types.STRING,     "indexDatabaseAddStorageName"),
    new Option("--index-database-remove",      null,Options.Types.STRING,     "indexDatabaseRemoveStorageName"),
    new Option("--index-database-refresh",     null,Options.Types.STRING,     "indexDatabaseRefreshStorageName"),
    new Option("--index-database-storage-list","-a",Options.Types.STRING,     "indexDatabaseStorageListPattern"),
    new Option("--index-database-entries-list","-e",Options.Types.STRING,     "indexDatabaseEntriesListPattern"),
    new Option("--pause",                      "-t",Options.Types.INTEGER,    "pauseTime"),
    new Option("--ping",                       "-i",Options.Types.BOOLEAN,    "pingFlag"),
    new Option("--suspend",                    "-s",Options.Types.BOOLEAN,    "suspendFlag"),
    new Option("--continue",                   "-c",Options.Types.BOOLEAN,    "continueFlag"),
    new Option("--list",                       "-l",Options.Types.BOOLEAN,    "listFlag"),

    new Option("--debug",                      "-d",Options.Types.INCREMENT,  "debugLevel"),
    new Option("--debug-quit-server",          null,Options.Types.BOOLEAN,    "debugQuitServerFlag"),

    new Option("--version",                    null,Options.Types.BOOLEAN,    "versionFlag"),
    new Option("--help",                       "-h",Options.Types.BOOLEAN,    "helpFlag"),

    // ignored
    new Option("--swing",                      null, Options.Types.BOOLEAN,   null),
  };

  // --------------------------- variables --------------------------------
  private static I18n    i18n;
  private static Display display;
  private static Shell   shell;
  private static Cursor  waitCursor;
  private static int     waitCursorCount = 0;

  private Menu       serverMenu;
  private TabFolder  tabFolder;
  private TabStatus  tabStatus;
  private TabJobs    tabJobs;
  private TabRestore tabRestore;

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
   */
  public static void waitCursor()
  {
    shell.setCursor(waitCursor);
    waitCursorCount++;
  }

  /** reset wait cursor
   */
  public static void resetCursor()
  {
    assert waitCursorCount > 0;

    waitCursorCount--;
    if (waitCursorCount <= 0)
    {
      shell.setCursor(null);
    }
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
    System.out.println("         -t|--pause=<n>                             - pause job execution for <n> seconds");
    System.out.println("         -i|--ping                                  - check connection to server");
    System.out.println("         -s|--suspend                               - suspend job execution");
    System.out.println("         -c|--continue                              - continue job execution");
    System.out.println("         -l|--list                                  - list jobs");
    System.out.println("");
    System.out.println("         --version                                  - output version");
    System.out.println("         -h|--help                                  - print this help");
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
    if (Settings.helpFlag)
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

  /** get job UUID
   * @param jobName job name
   * @return job UUID or null if not found
   */
  private static String getJobUUID(String jobName)
  {
    String[]            errorMessage  = new String[1];
    ArrayList<ValueMap> resultMapList = new ArrayList<ValueMap>();
    int error = BARServer.executeCommand(StringParser.format("JOB_LIST"),
                                         0,
                                         errorMessage,
                                         resultMapList
                                        );
    if (error == Errors.NONE)
    {
      for (ValueMap resultMap : resultMapList)
      {
        String jobUUID = resultMap.getString("jobUUID");
        String name    = resultMap.getString("name" );

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

  /** server/password dialog
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

    HashSet<String> serverHash = new HashSet<String>();
    for (Settings.Server server : Settings.servers)
    {
      serverHash.add(server.toString());
    }
    String serverNames[] = serverHash.toArray(new String[serverHash.size()]);
    Arrays.sort(serverNames);

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
        widgetServerName.setItems(serverNames);
        if (loginData.serverName != null) widgetServerName.setText(loginData.serverName);
        widgetServerName.setLayoutData(new TableLayoutData(0,0,TableLayoutData.WE));

        widgetServerPort = new Spinner(subComposite,SWT.RIGHT|SWT.BORDER);
        widgetServerPort.setMinimum(0);
        widgetServerPort.setMaximum(65535);
        widgetServerPort.setSelection(loginData.port);
        widgetServerPort.setLayoutData(new TableLayoutData(0,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT));
      }

      label = new Label(composite,SWT.LEFT);
      label.setText(BARControl.tr("Password")+":");
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
      widgetLoginButton.setText(BARControl.tr("Login"));
      widgetLoginButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.W,0,0,0,0,60,SWT.DEFAULT));
      widgetLoginButton.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          loginData.serverName = widgetServerName.getText();
          loginData.port       = widgetServerPort.getSelection();
          loginData.password   = widgetPassword.getText();
          Dialogs.close(dialog,true);
        }
      });

      button = new Button(composite,SWT.CENTER);
      button.setText(BARControl.tr("Cancel"));
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
        widgetPassword.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        String s[] = widgetServerName.getText().split(":");
        if (s.length >= 1) widgetServerName.setText(s[0]);
        if (s.length >= 2) widgetServerPort.setSelection(Integer.parseInt(s[1]));
      }
    });
    widgetPassword.addSelectionListener(new SelectionListener()
    {
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetLoginButton.forceFocus();
      }
      public void widgetSelected(SelectionEvent selectionEvent)
      {
      }
    });

    if ((loginData.serverName != null) && (loginData.serverName.length() != 0)) widgetPassword.forceFocus();

    Boolean result = (Boolean)Dialogs.run(dialog);

    // store new name, shorten list
//TODO
/*
    Settings.serverNames.remove(loginData.serverName);
    Settings.serverNames.add(loginData.serverName);
    while (Settings.serverNames.size() > 10)
    {
      String serverName = Settings.serverNames.iterator().next();
      Settings.serverNames.remove(serverName);
    }
*/

    return (result != null) ? result : false;
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

  /** upser server menu entries
   */
  private void updateServerMenu()
  {
    MenuItem menuItem;

    while (serverMenu.getItemCount() > 2)
    {
      serverMenu.getItem(2).dispose();
    }

    for (final Settings.Server server : Settings.servers)
    {
      menuItem = Widgets.addMenuItem(serverMenu,server.name+((server.port != Settings.DEFAULT_SERVER_PORT) ? ":"+server.port : ""));
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          final Settings.Server defaultServer = Settings.getLastServer();
          LoginData loginData = new LoginData((defaultServer != null) ? defaultServer.name : Settings.DEFAULT_SERVER_NAME,
                                              (defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT,
                                              (defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT
                                             );
          try
          {
            BARServer.connect(loginData.serverName,
                              loginData.port,
                              loginData.tlsPort,
                              loginData.password,
                              Settings.serverKeyFileName
                             );
            shell.setText("BAR control: "+BARServer.getInfo());
            updateServerMenu();
          }
          catch (ConnectionError error)
          {
            Dialogs.error(new Shell(),BARControl.tr("Connection fail: ")+error.getMessage());
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
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            final Settings.Server defaultServer = Settings.getLastServer();

            LoginData loginData = new LoginData((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT,
                                                (defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT
                                               );
            if (getLoginData(loginData))
            {
              try
              {
                BARServer.connect(loginData.serverName,
                                  loginData.port,
                                  loginData.tlsPort,
                                  loginData.password,
                                  Settings.serverKeyFileName
                                 );
                shell.setText("BAR control: "+BARServer.getInfo());
                updateServerMenu();
              }
              catch (ConnectionError error)
              {
                Dialogs.error(new Shell(),BARControl.tr("Connection fail: ")+error.getMessage());
              }
              catch (CommunicationError error)
              {
                Dialogs.error(new Shell(),BARControl.tr("Connection fail: ")+error.getMessage());
              }
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
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Widgets.notify(tabStatus.widgetButtonStart);
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Abort")+"\u2026",SWT.CTRL+'A');
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
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
          public void widgetSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
            tabStatus.jobPause(10*60);
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("60min"),SWT.CTRL+'P');
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

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("120min"));
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

        Widgets.addMenuSeparator(subMenu);

        menuItem = Widgets.addMenuCheckbox(subMenu,BARControl.tr("Create operation"),Settings.pauseCreateFlag);
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

        menuItem = Widgets.addMenuCheckbox(subMenu,BARControl.tr("Storage operation"),Settings.pauseStorageFlag);
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

        menuItem = Widgets.addMenuCheckbox(subMenu,BARControl.tr("Restore operation"),Settings.pauseRestoreFlag);
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
        menuItem = Widgets.addMenuCheckbox(subMenu,BARControl.tr("Index update operation"),Settings.pauseIndexUpdateFlag);
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

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Toggle suspend/continue"),SWT.CTRL+'T');
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Widgets.notify(tabStatus.widgetButtonSuspendContinue);
        }
      });

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Clear stored passwords on server"),SWT.NONE);
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
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
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          ServerSettings.serverSettings(shell);
        }
      });

      Widgets.addMenuSeparator(menu);

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Quit"),SWT.CTRL+'Q');
      menuItem.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
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
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
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
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            MenuItem widget = (MenuItem)selectionEvent.widget;
            BARServer.executeCommand(StringParser.format("DEBUG_PRINT_STATISTICS"),0);
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
            BARServer.executeCommand(StringParser.format("DEBUG_PRINT_MEMORY_INFO"),0);
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
            BARServer.executeCommand(StringParser.format("DEBUG_DUMP_MEMORY_INFO"),0);
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
      final Settings.Server defaultServer = Settings.getLastServer();
      LoginData loginData = new LoginData((defaultServer != null) ? defaultServer.name : Settings.DEFAULT_SERVER_NAME,
                                          (defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT,
                                          (defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT
                                         );
      if (Settings.serverName    != null) loginData.serverName = Settings.serverName;
      if (Settings.serverPort    != -1  ) loginData.port       = Settings.serverPort;
      if (Settings.serverTLSPort != -1  ) loginData.tlsPort    = Settings.serverTLSPort;

      // commands
      if (   (Settings.runJobName != null)
          || (Settings.abortJobName != null)
          || (Settings.indexDatabaseAddStorageName != null)
          || (Settings.indexDatabaseRemoveStorageName != null)
          || (Settings.indexDatabaseRefreshStorageName != null)
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
        // non-interactive mode

        // connect to server
        try
        {
          BARServer.connect(loginData.serverName,
                            loginData.port,
                            loginData.tlsPort,
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
        if (Settings.indexDatabaseStorageListPattern != null)
        {
          final String[] MAP_TEXT = new String[]{"\\n","\\r","\\\\"};
          final String[] MAP_BIN  = new String[]{"\n","\r","\\"};

          String[]            errorMessage  = new String[1];
          ValueMap            resultMap     = new ValueMap();
          ArrayList<ValueMap> resultMapList = new ArrayList<ValueMap>();

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
          if (BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%ld indexStateSet=%s indexModeSet=%s storagePattern=%'S offset=%ld",
                                                           0L,
                                                           "*",
                                                           "*",
                                                           Settings.indexDatabaseStorageListPattern,
                                                           0L
                                                          ),
                                       0,
                                       errorMessage,
                                       new CommandResultHandler()
                                       {
                                         public int handleResult(ValueMap resultMap)
                                         {
                                           String storageName = resultMap.getString("name"                        );
                                           long   dateTime    = resultMap.getLong  ("dateTime"                    );
                                           long   size        = resultMap.getLong  ("size"                        );
                                           IndexStates state  = resultMap.getEnum  ("indexState",IndexStates.class);
                                           IndexModes mode    = resultMap.getEnum  ("indexMode",IndexModes.class  );

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
                                      ) != Errors.NONE
             )
          {
            printError("cannot list storages index (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
        }
        if (Settings.indexDatabaseEntriesListPattern != null)
        {
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
          if (BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST entryPattern=%'S newestEntriesOnly=%y",
                                                           Settings.indexDatabaseEntriesListPattern,
                                                           false
                                                          ),
                                       0,
                                       errorMessage,
                                       new CommandResultHandler()
                                       {
                                         public int handleResult(ValueMap resultMap)
                                         {
                                           switch (resultMap.getEnum("entryType",EntryTypes.class))
                                           {
                                             case FILE:
                                               {
                                                 String storageName     = resultMap.getString("storageName"    );
                                                 long   storageDateTime = resultMap.getLong  ("storageDateTime");
                                                 String fileName        = resultMap.getString("name"           );
                                                 long   size            = resultMap.getLong  ("size"           );
                                                 long   dateTime        = resultMap.getLong  ("dateTime"       );
                                                 long   fragmentOffset  = resultMap.getLong  ("fragmentOffset" );
                                                 long   fragmentSize    = resultMap.getLong  ("fragmentSize"   );

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
                                                 String storageName     = resultMap.getString("name"           );
                                                 long   storageDateTime = resultMap.getLong  ("storageDateTime");
                                                 String imageName       = resultMap.getString("name"           );
                                                 long   size            = resultMap.getLong  ("size"           );
                                                 long   blockOffset     = resultMap.getLong  ("blockOffset"    );
                                                 long   blockCount      = resultMap.getLong  ("blockCount"     );

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
                                                 String storageName     = resultMap.getString("name"           );
                                                 long   storageDateTime = resultMap.getLong  ("storageDateTime");
                                                 String directoryName   = resultMap.getString("name"           );
                                                 long   dateTime        = resultMap.getLong  ("dateTime"       );

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
                                                 String storageName     = resultMap.getString("name"           );
                                                 long   storageDateTime = resultMap.getLong  ("storageDateTime");
                                                 String linkName        = resultMap.getString("name"           );
                                                 String destinationName = resultMap.getString("destinationName");
                                                 long   dateTime        = resultMap.getLong  ("dateTime"       );

                                                 System.out.println(String.format("%-40s %-8s %12d %-19s %s -> %s",
                                                                                  storageName,
                                                                                  "LINK",
                                                                                  DATE_FORMAT.format(new Date(dateTime*1000)),
                                                                                  linkName,
                                                                                  destinationName
                                                                                 )
                                                                   );
                                               }
                                               break;
                                             case HARDLINK:
                                               {
                                                 String storageName     = resultMap.getString("name"           );
                                                 long   storageDateTime = resultMap.getLong  ("storageDateTime");
                                                 String fileName        = resultMap.getString("name"           );
                                                 long   size            = resultMap.getLong  ("size"           );
                                                 long   dateTime        = resultMap.getLong  ("dateTime"       );
                                                 long   fragmentOffset  = resultMap.getLong  ("fragmentOffset" );
                                                 long   fragmentSize    = resultMap.getLong  ("fragmentSize"   );

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
                                                 String storageName     = resultMap.getString("name"           );
                                                 long   storageDateTime = resultMap.getLong  ("storageDateTime");
                                                 String name            = resultMap.getString("name"           );
                                                 long   dateTime        = resultMap.getLong  ("dateTime"       );

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
                                      ) != Errors.NONE
             )
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
          String[]            errorMessage  = new String[1];
          ValueMap            resultMap     = new ValueMap();
          ArrayList<ValueMap> resultMapList = new ArrayList<ValueMap>();

          // get server state
          String serverState = null;
          error = BARServer.executeCommand(StringParser.format("STATUS"),
                                           0,
                                           errorMessage,
                                           resultMap
                                          );
          if (error != Errors.NONE)
          {
            printError("cannot get state (error: %s)",errorMessage[0]);
            BARServer.disconnect();
            System.exit(1);
          }
          String type = resultMap.getString("type");
          if      (type.equalsIgnoreCase("running"))
          {
            serverState = null;
          }
          else if (type.equalsIgnoreCase("pause"))
          {
            serverState = "pause";
          }
          else if (type.equalsIgnoreCase("suspended"))
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
                                           resultMapList
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
          for (ValueMap resultMap_ : resultMapList)
          {
            // get data
            String jobUUID                = resultMap_.getString("jobUUID"                 );
            String name                   = resultMap_.getString("name"                    );
            String hostName               = resultMap_.getString("hostName",             "");
            String state                  = resultMap_.getString("state"                   );
            String archiveType            = resultMap_.getString("archiveType"             );
            long   archivePartSize        = resultMap_.getLong  ("archivePartSize"         );
            String deltaCompressAlgorithm = resultMap_.getString("deltaCompressAlgorithm"  );
            String byteCompressAlgorithm  = resultMap_.getString("byteCompressAlgorithm"   );
            String cryptAlgorithm         = resultMap_.getString("cryptAlgorithm"          );
            String cryptType              = resultMap_.getString("cryptType"               );
            String cryptPasswordMode      = resultMap_.getString("cryptPasswordMode"       );
            long   lastExecutedDateTime   = resultMap_.getLong  ("lastExecutedDateTime"    );
            long   estimatedRestTime      = resultMap_.getLong  ("estimatedRestTime"       );

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
                                loginData.port,
                                loginData.tlsPort,
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
                                loginData.port,
                                loginData.tlsPort,
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
            if (!Dialogs.confirmError(new Shell(),BARControl.tr("Connection fail"),BARControl.tr("Error: ")+error.getMessage(),BARControl.tr("Try again"),BARControl.tr("Cancel")))
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
