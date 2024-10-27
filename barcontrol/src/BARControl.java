/***********************************************************************\
*
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
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.PrintStream;
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

import javax.net.ssl.SSLException;

// graphics
import java.awt.Desktop;
import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.dnd.ByteArrayTransfer;
import org.eclipse.swt.dnd.Clipboard;
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
  FILESYSTEM,
  FTP,
  SCP,
  SFTP,
  WEBDAV,
  WEBDAVS,
  SMB,
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
    else if (string.equalsIgnoreCase("webdavs"))
    {
      type = StorageTypes.WEBDAVS;
    }
    else if (string.equalsIgnoreCase("smb"))
    {
      type = StorageTypes.SMB;
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
      type = StorageTypes.FILESYSTEM;
    }

    return type;
  }

  /** get (translated) text
   * @return text
   */
  public String getText()
  {
    switch (this)
    {
      case FILESYSTEM:
      default:
                       return BARControl.tr("filesystem");
      case FTP:        return "ftp";
      case SCP:        return "scp";
      case SFTP:       return "sftp";
      case WEBDAV:     return "webdav";
      case WEBDAVS:    return "webdavs";
      case SMB:        return "smb";
      case CD:         return "cd";
      case DVD:        return "dvd";
      case BD:         return "bd";
      case DEVICE:     return BARControl.tr("device");
    }
  }

  /** convert to string
   * @return string
   */
  @Override
  public String toString()
  {
    switch (this)
    {
      case FILESYSTEM:
      default:
                       return "filesystem";
      case FTP:        return "ftp";
      case SCP:        return "scp";
      case SFTP:       return "sftp";
      case WEBDAV:     return "webdav";
      case WEBDAVS:    return "webdavs";
      case SMB:        return "smb";
      case CD:         return "cd";
      case DVD:        return "dvd";
      case BD:         return "bd";
      case DEVICE:     return "device";
    }
  }
}

/** URI name parts
*/
class URIParts implements Cloneable
{
  public StorageTypes type;           // type
  public String       hostName;       // host name
  public int          hostPort;       // host port
  public String       loginName;      // login name
  public String       loginPassword;  // login password
  public String       shareName;      // share name
  public String       deviceName;     // device name
  public String       fileName;       // file name

  /** parse URI
   * @param type archive type
   * @param hostName host name
   * @param hostPort host port
   * @param loginName login name
   * @param loginPassword login password
   * @param shareName share name
   * @param deviceName device name
   * @param fileName file name
   */
  public URIParts(StorageTypes type,
                  String       hostName,
                  int          hostPort,
                  String       loginName,
                  String       loginPassword,
                  String       shareName,
                  String       deviceName,
                  String       fileName
                 )
  {
    this.type          = type;
    this.hostName      = hostName;
    this.hostPort      = hostPort;
    this.loginName     = loginName;
    this.loginPassword = loginPassword;
    this.shareName     = shareName;
    this.deviceName    = deviceName;
    this.fileName      = fileName;
  }

  /** parse URI
   * @param uri uri string
   *   ftp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   scp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   sftp://[[<login name>[:<login password>]@]<host name>[:<host port>]/]<file name>
   *   webdav://[[<login name>[:<login password>]@<host name>[:<host port>]/]<file name>
   *   smb://[[<login name>[:<login password>]@<host name>[:<share>]/]<file name>
   *   cd://[<device name>:]<file name>
   *   dvd://[<device name>:]<file name>
   *   bd://[<device name>:]<file name>
   *   device://[<device name>:]<file name>
   *   file://<file name>
   *   <file name>
   */
  public URIParts(String uri)
  {
    Matcher matcher;

    type          = StorageTypes.FILESYSTEM;
    hostName      = "";
    hostPort      = 0;
    loginName     = "";
    loginPassword = "";
    shareName     = "";
    deviceName    = "";
    fileName      = "";

    if       (uri.startsWith("ftp://"))
    {
      // ftp
      type = StorageTypes.FTP;

      String specifier = uri.substring(6);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<login name>:<login password>@<host name>:<host port>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        hostPort      = Integer.parseInt(matcher.group(5));
        fileName      = matcher.group(6);
      }
      else if ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<login name>@<host name>:<host port>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        hostPort  = Integer.parseInt(matcher.group(4));
        fileName  = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // ftp://<host name>:<host port>/<file name>
        hostName = matcher.group(1);
        hostPort = Integer.parseInt(matcher.group(2));
        fileName = matcher.group(3);
      }
      else
      {
        // ftp://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("scp://"))
    {
      // scp
      type = StorageTypes.SCP;

      String specifier = uri.substring(6);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>:<login password>@<host name>:<host port>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        hostPort      = Integer.parseInt(matcher.group(5));
        fileName      = matcher.group(6);
      }
      else if ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // scp://<login name>@<host name>:<host port>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        hostPort  = Integer.parseInt(matcher.group(4));
        fileName  = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // scp://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // scp://<host name>:<host port>/<file name>
        hostName  = matcher.group(1);
        hostPort  = Integer.parseInt(matcher.group(2));
        fileName  = matcher.group(3);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // scp://<host name>/<file name>
        hostName  = matcher.group(1);
        fileName  = matcher.group(2);
      }
      else
      {
        // scp://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("sftp://"))
    {
      // sftp
      type = StorageTypes.SFTP;

      String specifier = uri.substring(7);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>:<login password>@<host name>:<host port>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        hostPort      = Integer.parseInt(matcher.group(5));
        fileName      = matcher.group(6);
      }
      else if ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>@<host name>:<host port>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        hostPort  = Integer.parseInt(matcher.group(4));
        fileName  = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<host name>:<host port>/<file name>
        hostName = matcher.group(1);
        hostPort = Integer.parseInt(matcher.group(2));
        fileName = matcher.group(3);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // sftp://<host name>/<file name>
        hostName = matcher.group(1);
        fileName = matcher.group(2);
      }
      else
      {
        // sftp://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("webdav://"))
    {
      // webdav
      type = StorageTypes.WEBDAV;

      String specifier = uri.substring(9);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>:<login password>@<host name>:<host port>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        hostPort      = Integer.parseInt(matcher.group(5));
        fileName      = matcher.group(6);
      }
      else if ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>@<host name>:<host port>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        hostPort  = Integer.parseInt(matcher.group(4));
        fileName  = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<host name>:<host port>/<file name>
        hostName = matcher.group(1);
        hostPort = Integer.parseInt(matcher.group(2));
        fileName = matcher.group(3);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<host name>/<file name>
        hostName = matcher.group(1);
        fileName = matcher.group(2);
      }
      else
      {
        // webdav://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("webdavs://"))
    {
      // webdav
      type = StorageTypes.WEBDAVS;

      String specifier = uri.substring(10);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>:<login password>@<host name>:<host port>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        hostPort      = Integer.parseInt(matcher.group(5));
        fileName      = matcher.group(6);
      }
      else if ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>@<host name>:<host port>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        hostPort  = Integer.parseInt(matcher.group(4));
        fileName  = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):(\\d*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<host name>:<host port>/<file name>
        hostName = matcher.group(1);
        hostPort = Integer.parseInt(matcher.group(2));
        fileName = matcher.group(3);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // webdav://<host name>/<file name>
        hostName = matcher.group(1);
        fileName = matcher.group(2);
      }
      else
      {
        // webdav://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("smb://"))
    {
      // sftp
      type = StorageTypes.SMB;

      String specifier = uri.substring(6);
      if      ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?):([^:]*?)/(.*)$").matcher(specifier)).matches())
      {
        // smb://<login name>:<login password>@<host name>:<share>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        shareName     = matcher.group(5);
        fileName      = matcher.group(6);
      }
      else if ((matcher = Pattern.compile("^([^:]*?):(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // smb://<login name>:<login password>@<host name>/<file name>
        loginName     = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        loginPassword = matcher.group(2);
        hostName      = matcher.group(4);
        fileName      = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?):([^:]*?)/(.*)$").matcher(specifier)).matches())
      {
        // smb://<login name>@<host name>:<share>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        shareName = matcher.group(4);
        fileName  = matcher.group(5);
      }
      else if ((matcher = Pattern.compile("^(([^@]|\\@)*?)@([^@/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // smb://<login name>@<host name>/<file name>
        loginName = StringUtils.map(matcher.group(1),new String[]{"\\@"},new String[]{"@"});
        hostName  = matcher.group(3);
        fileName  = matcher.group(4);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?):([^:]*?)/(.*)$").matcher(specifier)).matches())
      {
        // smb://<host name>:<share>/<file name>
        hostName  = matcher.group(1);
        shareName = matcher.group(2);
        fileName  = matcher.group(3);
      }
      else if ((matcher = Pattern.compile("^([^@:/]*?)/(.*)$").matcher(specifier)).matches())
      {
        // smb://<host name>/<file name>
        hostName = matcher.group(1);
        fileName = matcher.group(2);
      }
      else
      {
        // smb://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("cd://"))
    {
      // cd
      type = StorageTypes.CD;

      String specifier = uri.substring(5);
      Object[] data = new Object[2];
      if      ((matcher = Pattern.compile("^([^:]*?):(.*)$").matcher(specifier)).matches())
      {
        // cd://<device name>:<file name>
        deviceName = matcher.group(1);
        fileName   = matcher.group(2);
      }
      else
      {
        // cd://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("dvd://"))
    {
      // dvd
      type = StorageTypes.DVD;

      String specifier = uri.substring(6);
      if      ((matcher = Pattern.compile("^([^:]*?):(.*)$").matcher(specifier)).matches())
      {
        // dvd://<device name>:<file name>
        deviceName = matcher.group(1);
        fileName   = matcher.group(2);
      }
      else
      {
        // dvd://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("bd://"))
    {
      // bd
      type = StorageTypes.BD;

      String specifier = uri.substring(5);
      if      ((matcher = Pattern.compile("^([^:]*?):(.*)$").matcher(specifier)).matches())
      {
        // bd://<device name>:<file name>
        deviceName = matcher.group(1);
        fileName   = matcher.group(2);
      }
      else
      {
        // bd://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("device://"))
    {
      // device0
      type = StorageTypes.DEVICE;

      String specifier = uri.substring(9);
      if      ((matcher = Pattern.compile("^([^:]*?):(.*)$").matcher(specifier)).matches())
      {
        // device://<device name>:<file name>
        deviceName = matcher.group(1);
        fileName   = matcher.group(2);
      }
      else
      {
        // device://<file name>
        fileName = specifier;
      }
    }
    else if (uri.startsWith("file://"))
    {
      // file
      type = StorageTypes.FILESYSTEM;

      String specifier = uri.substring(7);
      fileName = specifier.substring(2);
    }
    else
    {
      // file
      type = StorageTypes.FILESYSTEM;

      fileName = uri;
    }
  }

  /** clone URI data
   * @return cloned of object
   */
  public URIParts clone()
  {
    return new URIParts(type,
                        hostName,
                        hostPort,
                        loginName,
                        loginPassword,
                        shareName,
                        deviceName,
                        fileName
                       );
  }

  /** get URI
   * @param fileName file name part
   * @return URI
   */
  public String getURI(String fileName)
  {
    StringBuilder buffer = new StringBuilder();

    switch (type)
    {
      case FILESYSTEM:
        break;
      case FTP:
        buffer.append("ftp://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty() || !loginPassword.equals(""))
          {
            if (!loginName.isEmpty()) buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.equals("")) { buffer.append(':'); buffer.append(loginPassword); }
            buffer.append('@');
          }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
        }
        break;
      case SCP:
        buffer.append("scp://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty() || !loginPassword.isEmpty())
          {
            if (!loginName.isEmpty()) buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.isEmpty()) { buffer.append(':'); buffer.append(loginPassword); }
            buffer.append('@');
          }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          if (hostPort > 0) { buffer.append(':'); buffer.append(hostPort); }
        }
        break;
      case SFTP:
        buffer.append("sftp://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty() || !loginPassword.isEmpty())
          {
            if (!loginName.isEmpty()) buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.isEmpty()) { buffer.append(':'); buffer.append(loginPassword); }
            buffer.append('@');
          }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          if (hostPort > 0) { buffer.append(':'); buffer.append(hostPort); }
        }
        break;
      case WEBDAV:
        buffer.append("webdav://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty() || !loginPassword.isEmpty())
          {
            if (!loginName.isEmpty()) buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.isEmpty()) { buffer.append(':'); buffer.append(loginPassword); }
            buffer.append('@');
          }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          if (hostPort > 0) { buffer.append(':'); buffer.append(hostPort); }
        }
        break;
      case WEBDAVS:
        buffer.append("webdavs://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty() || !loginPassword.isEmpty())
          {
            if (!loginName.isEmpty()) buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.isEmpty()) { buffer.append(':'); buffer.append(loginPassword); }
            buffer.append('@');
          }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          if (hostPort > 0) { buffer.append(':'); buffer.append(hostPort); }
        }
        break;
      case SMB:
        buffer.append("smb://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty() || !loginPassword.isEmpty())
          {
            if (!loginName.isEmpty()) buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            if (!loginPassword.isEmpty()) { buffer.append(':'); buffer.append(loginPassword); }
            buffer.append('@');
          }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          if (!shareName.isEmpty()) { buffer.append(':'); buffer.append(shareName); }
        }
        break;
      case CD:
        buffer.append("cd://");
        if (!deviceName.isEmpty())
        {
          buffer.append(deviceName);
          buffer.append(':');
        }
        break;
      case DVD:
        buffer.append("dvd://");
        if (!deviceName.isEmpty())
        {
          buffer.append(deviceName);
          buffer.append(':');
        }
        break;
      case BD:
        buffer.append("bd://");
        if (!deviceName.isEmpty())
        {
          buffer.append(deviceName);
          buffer.append(':');
        }
        break;
      case DEVICE:
        buffer.append("device://");
        if (!deviceName.isEmpty())
        {
          buffer.append(deviceName);
          buffer.append(':');
        }
        break;
    }
    if (fileName != null)
    {
      if ((buffer.length() > 0) && (buffer.charAt(buffer.length()-1) != '/')) buffer.append('/');
      buffer.append(fileName);
    }

    return buffer.toString();
  }

  /** get URI path name
   * @return URI path name (URI name without file name)
   */
  public String getURIPath()
  {
    File file = new File(fileName);
    return getURI(file.getParent());
  }

  /** get URI name
   * @return URI name
   */
  public String getURI()
  {
    return getURI(fileName);
  }

  /** get printable URI (without password)
   * @param fileName file name part
   * @return URI
   */
  public String getPrintableURI(String fileName)
  {
    StringBuilder buffer = new StringBuilder();

    switch (type)
    {
      case FILESYSTEM:
        break;
      case FTP:
        buffer.append("ftp://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty() || !loginPassword.isEmpty())
          {
            if (!loginName.isEmpty()) buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"}));
            buffer.append('@');
          }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          buffer.append('/');
        }
        break;
      case SCP:
        buffer.append("scp://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty()) { buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); buffer.append('@'); }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          if (hostPort > 0) { buffer.append(':'); buffer.append(hostPort); }
          buffer.append('/');
        }
        break;
      case SFTP:
        buffer.append("sftp://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty()) { buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); buffer.append('@'); }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          if (hostPort > 0) { buffer.append(':'); buffer.append(hostPort); }
          buffer.append('/');
        }
        break;
      case WEBDAV:
        buffer.append("webdav://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty()) { buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); buffer.append('@'); }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          buffer.append('/');
        }
        break;
      case SMB:
        buffer.append("smb://");
        if (!loginName.isEmpty() || !hostName.isEmpty())
        {
          if (!loginName.isEmpty()) { buffer.append(StringUtils.map(loginName,new String[]{"@"},new String[]{"\\@"})); buffer.append('@'); }
          if (!hostName.isEmpty()) { buffer.append(hostName); }
          if (!shareName.isEmpty()) { buffer.append(':'); buffer.append(shareName); }
          buffer.append('/');
        }
        break;
      case CD:
        buffer.append("cd://");
        if (!deviceName.isEmpty())
        {
          buffer.append(deviceName);
          buffer.append(':');
        }
        break;
      case DVD:
        buffer.append("dvd://");
        if (!deviceName.isEmpty())
        {
          buffer.append(deviceName);
          buffer.append(':');
        }
        break;
      case BD:
        buffer.append("bd://");
        if (!deviceName.isEmpty())
        {
          buffer.append(deviceName);
          buffer.append(':');
        }
        break;
      case DEVICE:
        buffer.append("device://");
        if (!deviceName.isEmpty())
        {
          buffer.append(deviceName);
          buffer.append(':');
        }
        break;
    }
    if (fileName != null)
    {
      buffer.append(fileName);
    }

    return buffer.toString();
  }

  /** get printable URI (without password)
   * @return URI
   */
  public String getPrintableURI()
  {
    return getPrintableURI(fileName);
  }

  /** convert to string
   * @return string
   */
  @Override
  public String toString()
  {
    return "URIParts {"+type+", "+loginName+", "+loginPassword+", "+hostName+", "+hostPort+", "+shareName+", "+deviceName+", "+fileName+"}";
  }
}

/** archive types
 */
enum ArchiveTypes
{
  NORMAL,
  FULL,
  INCREMENTAL,
  DIFFERENTIAL,
  CONTINUOUS,

  UNKNOWN;

  /** get (translated) text
   * @return text
   */
  public String getText()
  {
    switch (this)
    {
      case NORMAL:       return BARControl.tr("normal");
      case FULL:         return BARControl.tr("full");
      case INCREMENTAL:  return BARControl.tr("incremental");
      case DIFFERENTIAL: return BARControl.tr("differential");
      case CONTINUOUS:   return BARControl.tr("continuous");
      default:           return BARControl.tr("unknown");
    }
  }

  /** convert to string
   * @return string
   */
  @Override
  public String toString()
  {
    switch (this)
    {
      case NORMAL:       return "normal";
      case FULL:         return "full";
      case INCREMENTAL:  return "incremental";
      case DIFFERENTIAL: return "differential";
      case CONTINUOUS:   return "continuous";
      default:           return "unknown";
    }
  }
};

/** units
 */
class Units
{
  public static long K = 1024L;
  public static long M = 1024L*K;
  public static long G = 1024L*M;
  public static long T = 1024L*G;
  public static long P = 1024L*T;

  public static long SECOND = 1L;
  public static long MINUTE = 60L*SECOND;
  public static long HOUR   = 60L*MINUTE;
  public static long DAY    = 24L*HOUR;
  public static long WEEK   = 7L*DAY;

  /** get size string
   * @param n value
   * @return string
   */
  public static String getSize(double n)
  {
    final DecimalFormat DECIMAL_FORMAT = new DecimalFormat(".#");

    String result;

    if (n > 0)
    {
      if      ((n % P) == 0) result = String.format("%d.0",(long)n/P);
      else if (n >= P      ) result = DECIMAL_FORMAT.format(n/P);
      else if ((n % T) == 0) result = String.format("%d.0",(long)n/T);
      else if (n >= T      ) result = DECIMAL_FORMAT.format(n/T);
      else if ((n % G) == 0) result = String.format("%d.0",(long)n/G);
      else if (n >= G      ) result = DECIMAL_FORMAT.format(n/G);
      else if ((n % M) == 0) result = String.format("%d.0",(long)n/M);
      else if (n >= M      ) result = DECIMAL_FORMAT.format(n/M);
      else if ((n % K) == 0) result = String.format("%d.0",(long)n/K);
      else if (n >= K      ) result = DECIMAL_FORMAT.format(n/K);
      else                   result = String.format("%d",(long)n);

      return result;
    }
    else
    {
      return "0";
    }
  }

  /** get size short unit
   * @param n byte value
   * @return unit
   */
  public static String getUnit(double n)
  {
    String result;

    if      (n >= P) result =  "P";
    else if (n >= T) result =  "T";
    else if (n >= G) result =  "G";
    else if (n >= M) result =  "M";
    else if (n >= K) result =  "K";
    else             result =  "";

    return result;
  }

  /** get byte size unit
   * @param n byte value
   * @return unit
   */
  public static String getByteUnit(double n)
  {
    String result;

    if      (n >= P) result =  BARControl.tr("PiBytes");
    else if (n >= T) result =  BARControl.tr("TiBytes");
    else if (n >= G) result =  BARControl.tr("GiBytes");
    else if (n >= M) result =  BARControl.tr("MiBytes");
    else if (n >= K) result =  BARControl.tr("KiBytes");
    else             result =  BARControl.tr("bytes");

    return result;
  }

  /** get max. byte size unit text size
   * @param control widget
   * @param suffix suffix
   * @return max. text size
   */
  public static Point getMaxByteUnitTextSize(Control control, String suffix)
  {
    return Widgets.getTextSize(control,new String[]{BARControl.tr("bytes")  +suffix,
                                                    BARControl.tr("KiBytes")+suffix,
                                                    BARControl.tr("MiBytes")+suffix,
                                                    BARControl.tr("GiBytes")+suffix,
                                                    BARControl.tr("TiBytes")+suffix,
                                                    BARControl.tr("PiBytes")+suffix
                                                   }
                              );
  }

  /** get max. byte size unit text size
   * @param control widget
   * @return max. text size
   */
  public static Point getMaxByteUnitTextSize(Control control)
  {
    return getMaxByteUnitTextSize(control,"");
  }

  /** get byte size short unit
   * @param n byte value
   * @return unit
   */
  public static String getByteShortUnit(double n)
  {
    String result;

    if      (n >= P) result =  "PiB";
    else if (n >= T) result =  "TiB";
    else if (n >= G) result =  "GiB";
    else if (n >= M) result =  "MiB";
    else if (n >= K) result =  "KiB";
    else             result =  "B";

    return result;
  }

  /** get max. byte size short unit text size
   * @param control widget
   * @param suffix suffix
   * @return max. text size
   */
  public static Point getMaxByteShortUnitTextSize(Control control, String suffix)
  {
    return Widgets.getTextSize(control,new String[]{BARControl.tr("B")  +suffix,
                                                    BARControl.tr("KiB")+suffix,
                                                    BARControl.tr("MiB")+suffix,
                                                    BARControl.tr("GiB")+suffix,
                                                    BARControl.tr("TiB")+suffix,
                                                    BARControl.tr("PiB")+suffix
                                                   }
                              );
  }

  /** get max. byte size short unit text size
   * @param control widget
   * @return max. text size
   */
  public static Point getMaxByteShortUnitTextSize(Control control)
  {
    return getMaxByteShortUnitTextSize(control,"");
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
      if      (string.endsWith("PB") || string.endsWith("PiB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*P);
      }
      else if (string.endsWith("P") || string.endsWith("Pi"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*P);
      }
      else if  (string.endsWith("TB") || string.endsWith("TiB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*T);
      }
      else if (string.endsWith("T") || string.endsWith("Ti"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*T);
      }
      else if (string.endsWith("GB") || string.endsWith("GiB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*G);
      }
      else if (string.endsWith("G") || string.endsWith("Gi"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*G);
      }
      else if (string.endsWith("MB") || string.endsWith("MiB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*M);
      }
      else if (string.endsWith("M") || string.endsWith("Mi"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*M);
      }
      else if (string.endsWith("KB") || string.endsWith("KiB"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-2)).doubleValue()*K);
      }
      else if (string.endsWith("K") || string.endsWith("Ki"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*K);
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

  /** format size
   * @param n value
   * @return string with unit
   */
  public static String formatSize(long n)
  {
    return getSize(n)+" "+getUnit(n);
  }

  /** format byte size
   * @param n byte value
   * @return string with unit
   */
  public static String formatByteSize(long n)
  {
    return getSize(n)+" "+getByteShortUnit(n);
  }

  /** get time string
   * @param n time [s]
   * @return string
   */
  public static String getTime(double n)
  {
    if      (((long)n         ) == 0) return "";
    else if (((long)n % WEEK  ) == 0) return String.format("%d",(long)(n/WEEK  ));
    else if (((long)n % DAY   ) == 0) return String.format("%d",(long)(n/DAY   ));
    else if (((long)n % HOUR  ) == 0) return String.format("%d",(long)(n/HOUR  ));
    else if (((long)n % MINUTE) == 0) return String.format("%d",(long)(n/MINUTE));
    else                              return String.format("%d",(long)n         );
  }

  /** get time unit
   * @param n time [s]
   * @return unit
   */
  public static String getTimeUnit(double n)
  {
    if      (((long)n         ) == 0) return "";
    else if (((long)n % WEEK  ) == 0) return (((long)n / WEEK) != 1) ? "weeks" : "week";
    else if (((long)n % DAY   ) == 0) return (((long)n / DAY ) != 1) ? "days"  : "day";
    else if (((long)n % HOUR  ) == 0) return "h";
    else if (((long)n % MINUTE) == 0) return "min";
    else                              return "s";
  }

  /** get localized time unit
   * @param n time [s]
   * @return localized unit
   */
  public static String getLocalizedTimeUnit(double n)
  {
    if      (((long)n                           ) == 0 ) return "";
    else if (((long)n > 0) && (((long)n % WEEK  ) == 0)) return (((long)n / WEEK) != 1) ? BARControl.tr("weeks") : BARControl.tr("week");
    else if (((long)n > 0) && (((long)n % DAY   ) == 0)) return (((long)n / DAY ) != 1) ? BARControl.tr("days" ) : BARControl.tr("day" );
    else if (((long)n > 0) && (((long)n % HOUR  ) == 0)) return BARControl.tr("h");
    else if (((long)n > 0) && (((long)n % MINUTE) == 0)) return BARControl.tr("min");
    else if ((long)n > 0)                                return BARControl.tr("s");
    else                                                 return "";
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
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-4)).doubleValue()*WEEK);
      }
      if       (string.endsWith("WEEKS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-5)).doubleValue()*WEEK);
      }
      else if (string.endsWith("DAY"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-3)).doubleValue()*DAY);
      }
      else if (string.endsWith("DAYS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-4)).doubleValue()*DAY);
      }
      else if (string.endsWith("H"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*HOUR);
      }
      else if (string.endsWith("HOUR"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-4)).doubleValue()*HOUR);
      }
      else if (string.endsWith("HOURS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-5)).doubleValue()*HOUR);
      }
      else if (string.endsWith("M"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-1)).doubleValue()*MINUTE);
      }
      else if (string.endsWith("MIN"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-3)).doubleValue()*MINUTE);
      }
      else if (string.endsWith("MINS"))
      {
        return (long)(NumberFormat.getInstance().parse(string.substring(0,string.length()-4)).doubleValue()*MINUTE);
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
    final String SUFFIX_WEEK    = BARControl.tr("week"   ).toUpperCase();
    final String SUFFIX_WEEKS   = BARControl.tr("weeks"  ).toUpperCase();
    final String SUFFIX_DAY     = BARControl.tr("day"    ).toUpperCase();
    final String SUFFIX_DAYS    = BARControl.tr("days"   ).toUpperCase();
    final String SUFFIX_H       = BARControl.tr("h"      ).toUpperCase();
    final String SUFFIX_HOUR    = BARControl.tr("hour"   ).toUpperCase();
    final String SUFFIX_HOURS   = BARControl.tr("hours"  ).toUpperCase();
    final String SUFFIX_M       = BARControl.tr("m"      ).toUpperCase();
    final String SUFFIX_MIN     = BARControl.tr("min"    ).toUpperCase();
    final String SUFFIX_MINS    = BARControl.tr("mins"   ).toUpperCase();
    final String SUFFIX_S       = BARControl.tr("s"      ).toUpperCase();
    final String SUFFIX_SECOND  = BARControl.tr("second" ).toUpperCase();
    final String SUFFIX_SECONDS = BARControl.tr("seconds").toUpperCase();

    string = string.toUpperCase();

    // try to parse with default locale
    if       (string.endsWith(SUFFIX_WEEK))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_WEEK.length()))*WEEK);
    }
    if       (string.endsWith(SUFFIX_WEEKS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_WEEKS.length()))*WEEK);
    }
    else if (string.endsWith(SUFFIX_DAY))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_DAY.length()))*DAY);
    }
    else if (string.endsWith(SUFFIX_DAYS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_DAYS.length()))*DAY);
    }
    else if (string.endsWith(SUFFIX_H))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_H.length()))*HOUR);
    }
    else if (string.endsWith(SUFFIX_HOUR))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_HOUR.length()))*HOUR);
    }
    else if (string.endsWith(SUFFIX_HOURS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_HOURS.length()))*HOUR);
    }
    else if (string.endsWith(SUFFIX_M))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_M.length()))*MINUTE);
    }
    else if (string.endsWith(SUFFIX_MIN))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_MIN.length()))*MINUTE);
    }
    else if (string.endsWith(SUFFIX_MINS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_MINS.length()))*MINUTE);
    }
    else if (string.endsWith(SUFFIX_S))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_S.length())));
    }
    else if (string.endsWith(SUFFIX_SECOND))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_SECOND.length())));
    }
    else if (string.endsWith(SUFFIX_SECONDS))
    {
      return (long)(Double.parseDouble(string.substring(0,string.length()-SUFFIX_SECONDS.length())));
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
    if (keep == ALL) return "-";
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
    if      (age == FOREVER  ) return BARControl.tr("forever");

    // years
    if (age == 365      ) return BARControl.tr("{0} {0,choice,0#years|1#year|1<years}",   1);
    if ((age % 365) == 0) return BARControl.tr("{0} {0,choice,0#years|1#year|1<years}",   age/365);

    // months
    if (age >= 365)       return BARControl.tr("{0} {0,choice,0#months|1#month|1<months}",(int)((double)age/365.0*12.0));
    if (age == 30       ) return BARControl.tr("{0} {0,choice,0#months|1#month|1<months}",1);
    if ((age % 30) == 0 ) return BARControl.tr("{0} {0,choice,0#months|1#month|1<months}",age/30 );

    // weeks
    if (age == 7        ) return BARControl.tr("{0} {0,choice,0#weeks|1#week|1<weeks}",   1);
    if ((age % 7) == 0  ) return BARControl.tr("{0} {0,choice,0#weeks|1#week|1<weeks}",   age/7);

    // days
    return BARControl.tr("{0} {0,choice,0#days|1#day|1<days}",age);
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

  /** get (translated) text
   * @return text
   */
  public String getText()
  {
    switch (this)
    {
      case NONE:   return "";
      case FTP:    return "FTP";
      case SSH:    return "SSH";
      case WEBDAV: return "WebDAV";
      case CRYPT:  return BARControl.tr("encryption");
    }

    return "";
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
      case CRYPT:  return BARControl.tr("encryption");
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
    String             name;            // server name
    int                port;            // server port
    int                tlsPort;         // server TLS port
    BARServer.TLSModes tlsMode;         // TLS mode
    String             password;        // login password
    Roles              role;            // role

    /** create login data
     * @param serverName server name
     * @param port server port
     * @param tlsPort server TLS port
     * @param tlsMode TLS mode; see BARServer.TLSModes
     * @param password server password
     * @param role role
     */
    LoginData(String name, int port, int tlsPort, BARServer.TLSModes tlsMode, String password, Roles role)
    {
      final Settings.Server lastServer = Settings.getLastServer();

      this.name     = !name.isEmpty()     ? name     : ((lastServer != null) ? lastServer.name : Settings.DEFAULT_SERVER_NAME    );
      this.port     = (port != 0        ) ? port     : ((lastServer != null) ? lastServer.port : Settings.DEFAULT_SERVER_PORT    );
      this.tlsPort  = (port != 0        ) ? tlsPort  : ((lastServer != null) ? lastServer.port : Settings.DEFAULT_SERVER_TLS_PORT);
      this.tlsMode  = tlsMode;
      this.password = !password.isEmpty() ? password : "";
      this.role     = role;
    }

    /** create login data
     * @param serverName server name
     * @param port server port
     * @param tlsPort server TLS port
     * @param tlsMode TLS mode; see BARServer.TLSModes
     * @param role role
     */
    LoginData(String name, int port, int tlsPort, BARServer.TLSModes tlsMode, Roles role)
    {
      this(name,port,tlsPort,tlsMode,"",role);
    }

    /** create login data
     * @param port server port
     * @param tlsPort server TLS port
     * @param tlsMode TLS mode; see BARServer.TLSModes
     * @param role role
     */
    LoginData(int port, int tlsPort, BARServer.TLSModes tlsMode, Roles role)
    {
      this("",port,tlsPort,tlsMode,role);
    }

    /** convert data to string
     * @return string
     */
    @Override
    public String toString()
    {
      return "LoginData {"+name+", "+port+", "+tlsPort+", "+tlsMode+"}";
    }
  }

  /** job info
   */
  class JobInfo
  {
    String              jobUUID;
    String              name;
    JobData.States      state;
    String              slaveHostName;
    JobData.SlaveStates slaveState;
    String              archiveType;
    long                archivePartSize;
// TODO: enum
    String              deltaCompressAlgorithm;
// TODO: enum
    String              byteCompressAlgorithm;
// TODO: enum
    String              cryptAlgorithm;
// TODO: enum
    String              cryptType;
// TODO: enum
    String              cryptPasswordMode;
    long                lastExecutedDateTime;
    long                estimatedRestTime;

    /** create login data
     * @param jobUUID job UUID
     * @param name name
     * @param state state
     * @param slaveHostName slave host name
     * @param slaveState slave state
     * @param archiveType archive type
     * @param archivePartSize archive part size [bytes]
     * @param deltaCompressAlgorithm delta compress algorithm
     * @param byteCompressAlgorithm byte compress algorithm
     * @param cryptAlgorithm crypt algorithm
     * @param cryptType crypt type
     * @param cryptPasswordMode crypt password mode
     * @param lastExecutedDateTime last executed date/time
     * @param estimatedRestTime estimated rest time [s]
     */
    JobInfo(String              jobUUID,
            String              name,
            JobData.States      state,
            String              slaveHostName,
            JobData.SlaveStates slaveState,
            String              archiveType,
            long                archivePartSize,
            String              deltaCompressAlgorithm,
            String              byteCompressAlgorithm,
            String              cryptAlgorithm,
            String              cryptType,
            String              cryptPasswordMode,
            long                lastExecutedDateTime,
            long                estimatedRestTime
           )
    {
      this.jobUUID                = jobUUID;
      this.name                   = name;
      this.state                  = state;
      this.slaveHostName          = slaveHostName;
      this.slaveState             = slaveState;
      this.archiveType            = archiveType;
      this.archivePartSize        = archivePartSize;
      this.deltaCompressAlgorithm = deltaCompressAlgorithm;
      this.byteCompressAlgorithm  = byteCompressAlgorithm;
      this.cryptAlgorithm         = cryptAlgorithm;
      this.cryptType              = cryptType;
      this.cryptPasswordMode      = cryptPasswordMode;
      this.lastExecutedDateTime   = lastExecutedDateTime;
      this.estimatedRestTime      = estimatedRestTime;
    }
  };

  /** schedule info
   */
  class ScheduleInfo
  {
    final static int NONE = 0;
    final static int ANY  = -1;
    final static int MON  = 0;
    final static int TUE  = 1;
    final static int WED  = 2;
    final static int THU  = 3;
    final static int FRI  = 4;
    final static int SAT  = 5;
    final static int SUN  = 6;

    String       jobName;
    String       uuid;
    int          year,month,day;
    int          weekDays;
    int          hour,minute;
    ArchiveTypes archiveType;
    int          interval;
    int          beginHour,beginMinute;
    int          endHour,endMinute;
    String       customText;
    boolean      testCreatedArchives;
    boolean      noStorage;
    boolean      enabled;
    long         lastExecutedDateTime;
    long         nextExecutedDateTime;
    long         totalEntities,totalEntryCount,totalEntrySize;

    /** create schedule data
     * @param jobName job name
     * @param uuid schedule UUID
     * @param year year
     * @param month month [1..12]
     * @param day day [1..31]
     * @param weekDays week days
     * @param hour hour [0..23]
     * @param minute minute [0..59]
     * @param archiveType archive type string
     * @param interval continuous interval [min]
     * @param customText custom text
     * @param testCreatedArchives true to test created archives
     * @param noStorage true to skip storage
     * @param enabled true iff enabled
     * @param lastExecutedDateTime date/time of last execution
     * @param nextExecutedDateTime date/time of next execution
     * @param totalEntities total number of existing entities for schedule
     * @param totalEntryCount total number of existing entries for schedule
     * @param totalEntrySize total size of existing entries for schedule [bytes]
     */
    ScheduleInfo(String       jobName,
                 String       uuid,
                 int          year,
                 int          month,
                 int          day,
                 int          weekDays,
                 int          hour,
                 int          minute,
                 ArchiveTypes archiveType,
                 int          interval,
                 int          beginHour,
                 int          beginMinute,
                 int          endHour,
                 int          endMinute,
                 String       customText,
                 boolean      testCreatedArchives,
                 boolean      noStorage,
                 boolean      enabled,
                 long         lastExecutedDateTime,
                 long         nextExecutedDateTime,
                 long         totalEntities,
                 long         totalEntryCount,
                 long         totalEntrySize
                )
    {
      this.jobName              = jobName;
      this.uuid                 = uuid;
      this.year                 = year;
      this.month                = month;
      this.day                  = day;
      this.weekDays             = weekDays;
      this.hour                 = hour;
      this.minute               = minute;
      this.archiveType          = archiveType;
      this.interval             = interval;
      this.beginHour            = beginHour;
      this.beginMinute          = beginMinute;
      this.endHour              = endHour;
      this.endMinute            = endMinute;
      this.customText           = customText;
      this.testCreatedArchives  = testCreatedArchives;
      this.noStorage            = noStorage;
      this.enabled              = enabled;
      this.lastExecutedDateTime = lastExecutedDateTime;
      this.nextExecutedDateTime = nextExecutedDateTime;
      this.totalEntities        = totalEntities;
      this.totalEntryCount      = totalEntryCount;
      this.totalEntrySize       = totalEntrySize;
    }

    /** create schedule data
     * @param jobName job name
     * @param uuid schedule UUID
     * @param date date string (<year>-<month>-<day>)
     * @param weekDays week days string; values separated by ','
     * @param time time string (<hour>:<minute>)
     * @param archiveType archive type string
     * @param interval continuous interval [min]
     * @param beginTime,endTime continous begin/end time
     * @param customText custom text
     * @param testCreatedArchives true to test created archives
     * @param noStorage true to skip storage
     * @param enabled true iff enabled
     * @param lastExecutedDateTime date/time of last execution
     * @param nextExecutedDateTime date/time of next execution
     * @param totalEntities total number of existing entities for schedule
     * @param totalEntryCount total number of existing entries for schedule
     * @param totalEntrySize total size of existing entries for schedule [bytes]
     */
    ScheduleInfo(String       jobName,
                 String       uuid,
                 String       date,
                 String       weekDays,
                 String       time,
                 ArchiveTypes archiveType,
                 int          interval,
                 String       beginTime,
                 String       endTime,
                 String       customText,
                 boolean      testCreatedArchives,
                 boolean      noStorage,
                 boolean      enabled,
                 long         lastExecutedDateTime,
                 long         nextExecutedDateTime,
                 long         totalEntities,
                 long         totalEntryCount,
                 long         totalEntrySize
                )
    {
      this.jobName              = jobName;
      this.uuid                 = uuid;
      setDate(date);
      setWeekDays(weekDays);
      setTime(time);
      this.archiveType          = archiveType;
      this.interval             = interval;
      setBeginTime(beginTime);
      setEndTime(endTime);
      this.customText           = customText;
      this.testCreatedArchives  = testCreatedArchives;
      this.noStorage            = noStorage;
      this.enabled              = enabled;
      this.lastExecutedDateTime = lastExecutedDateTime;
      this.nextExecutedDateTime = nextExecutedDateTime;
      this.totalEntities        = totalEntities;
      this.totalEntryCount      = totalEntryCount;
      this.totalEntrySize       = totalEntrySize;
    }

    /** get date value
     * @return date string
     */
    String getDate()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getYear());
      buffer.append('-');
      buffer.append(getMonth());
      buffer.append('-');
      buffer.append(getDay());

      return buffer.toString();
    }

    /** set date
     * @param year year value
     * @param month month value
     * @param day day value
     */
    private void setDate(String year, String month, String day)
    {
      this.year  = !year.equals ("*") ? Integer.parseInt(year ) : ANY;
      this.month = !month.equals("*") ? Integer.parseInt(month) : ANY;
      this.day   = !day.equals  ("*") ? Integer.parseInt(day  ) : ANY;
      assert (this.year == ANY) || (this.year >= 0) : this.hour;
      assert (this.month == ANY) || ((this.month >= 1) && (this.month <= 12)) : this.hour;
      assert (this.day == ANY) || ((this.day >= 1) && (this.day <= 31)) : this.hour;
    }

    /** set date
     * @param date date string
     */
    private void setDate(String date)
    {
      String[] parts = date.split("-");
      setDate(parts[0],parts[1],parts[2]);
    }

    /** get year value
     * @return year string
     */
    String getYear()
    {
      assert (year == ANY) || (year >= 1) : year;

      return (year != ANY) ? String.format("%04d",year) : "*";
    }

    /** get month value
     * @return month string
     */
    String getMonth()
    {
      assert (month == ANY) || ((month >= 1) && (month <= 12)) : month;

      return (month != ANY) ? String.format("%02d",month) : "*";
    }

    /** get day value
     * @return day string
     */
    String getDay()
    {
      assert (day == ANY) || ((day >= 1) && (day <= 31)) : day;

      return (day != ANY) ? String.format("%02d",day) : "*";
    }

    /** get week days value
     * @return week days string
     */
    String getWeekDays()
    {
      assert    (weekDays == ANY)
             || ((weekDays & ~(  (1 << ScheduleInfo.MON)
                               | (1 << ScheduleInfo.TUE)
                               | (1 << ScheduleInfo.WED)
                               | (1 << ScheduleInfo.THU)
                               | (1 << ScheduleInfo.FRI)
                               | (1 << ScheduleInfo.SAT)
                               | (1 << ScheduleInfo.SUN)
                              )) == 0
                ) : weekDays;

      if (weekDays == ANY)
      {
        return "*";
      }
      else
      {
        StringBuilder buffer = new StringBuilder();

        if ((weekDays & (1 << ScheduleInfo.MON)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Mon")); }
        if ((weekDays & (1 << ScheduleInfo.TUE)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Tue")); }
        if ((weekDays & (1 << ScheduleInfo.WED)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Wed")); }
        if ((weekDays & (1 << ScheduleInfo.THU)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Thu")); }
        if ((weekDays & (1 << ScheduleInfo.FRI)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Fri")); }
        if ((weekDays & (1 << ScheduleInfo.SAT)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Sat")); }
        if ((weekDays & (1 << ScheduleInfo.SUN)) != 0) { if (buffer.length() > 0) buffer.append(','); buffer.append(BARControl.tr("Sun")); }

        return buffer.toString();
      }
    }

    /** set week days
     * @param weekDays week days string; values separated by ','
     */
    void setWeekDays(String weekDays)
    {
      if (weekDays.equals("*"))
      {
        this.weekDays = ScheduleInfo.ANY;
      }
      else
      {
        this.weekDays = ScheduleInfo.NONE;
        for (String name : weekDays.split(","))
        {
          if      (name.toLowerCase().equals("mon")) this.weekDays |= (1 << ScheduleInfo.MON);
          else if (name.toLowerCase().equals("tue")) this.weekDays |= (1 << ScheduleInfo.TUE);
          else if (name.toLowerCase().equals("wed")) this.weekDays |= (1 << ScheduleInfo.WED);
          else if (name.toLowerCase().equals("thu")) this.weekDays |= (1 << ScheduleInfo.THU);
          else if (name.toLowerCase().equals("fri")) this.weekDays |= (1 << ScheduleInfo.FRI);
          else if (name.toLowerCase().equals("sat")) this.weekDays |= (1 << ScheduleInfo.SAT);
          else if (name.toLowerCase().equals("sun")) this.weekDays |= (1 << ScheduleInfo.SUN);
        }
      }
    }

    /** set week days
     * @param monFlag true for Monday
     * @param tueFlag true for Tuesday
     * @param wedFlag true for Wednesday
     * @param thuFlag true for Thursday
     * @param friFlag true for Friday
     * @param satFlag true for Saturday
     * @param sunFlag true for Sunday
     */
    void setWeekDays(boolean monFlag,
                     boolean tueFlag,
                     boolean wedFlag,
                     boolean thuFlag,
                     boolean friFlag,
                     boolean satFlag,
                     boolean sunFlag
                    )
    {

      if (   monFlag
          && tueFlag
          && wedFlag
          && thuFlag
          && friFlag
          && satFlag
          && sunFlag
         )
      {
        this.weekDays = ScheduleInfo.ANY;
      }
      else
      {
        this.weekDays = ScheduleInfo.NONE;
        if (monFlag) this.weekDays |= (1 << ScheduleInfo.MON);
        if (tueFlag) this.weekDays |= (1 << ScheduleInfo.TUE);
        if (wedFlag) this.weekDays |= (1 << ScheduleInfo.WED);
        if (thuFlag) this.weekDays |= (1 << ScheduleInfo.THU);
        if (friFlag) this.weekDays |= (1 << ScheduleInfo.FRI);
        if (satFlag) this.weekDays |= (1 << ScheduleInfo.SAT);
        if (sunFlag) this.weekDays |= (1 << ScheduleInfo.SUN);
      }
    }

    /** get hour value
     * @return hour string
     */
    String getHour()
    {
      assert (hour == ANY) || ((hour >= 0) && (hour <= 23)) : hour;

      return (hour != ANY) ? String.format("%02d",hour) : "*";
    }

    /** get minute value
     * @return minute string
     */
    String getMinute()
    {
      assert (minute == ANY) || ((minute >= 0) && (minute <= 59)) : minute;

      return (minute != ANY) ? String.format("%02d",minute) : "*";
    }

    /** get time value
     * @return time string
     */
    String getTime()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getHour());
      buffer.append(':');
      buffer.append(getMinute());

      return buffer.toString();
    }

    /** set time
     * @param hour hour value
     * @param minute minute value
     */
    void setTime(String hour, String minute)
    {
      this.hour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.minute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
      assert (this.hour == ANY) || ((this.hour >= 0) && (this.hour <= 23)) : this.hour;
      assert (this.minute == ANY) || ((this.minute >= 0) && (this.minute <= 59)) : this.minute;
    }

    /** set time
     * @param time time string
     */
    void setTime(String time)
    {
      String[] parts = time.split(":");
      setTime(parts[0],parts[1]);
    }

    /** get archive type
     * @return archive type
     */
    ArchiveTypes getArchiveType()
    {
      return archiveType;
    }

    /** get begin hour value
     * @return begin hour string
     */
    String getBeginHour()
    {
      assert (beginHour == ANY) || ((beginHour >= 0) && (beginHour <= 23)) : beginHour;

      return (beginHour != ANY) ? String.format("%02d",beginHour) : "*";
    }

    /** get begin minute value
     * @return begin minute string
     */
    String getBeginMinute()
    {
      assert (endMinute == ANY) || ((endMinute >= 0) && (endMinute <= 59)) : endMinute;

      return (endMinute != ANY) ? String.format("%02d",endMinute) : "*";
    }

    /** get begin time value
     * @return begin time string
     */
    String getBeginTime()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getBeginHour());
      buffer.append(':');
      buffer.append(getBeginMinute());

      return buffer.toString();
    }

    /** set begin time
     * @param hour hour value
     * @param minute minute value
     */
    void setBeginTime(String hour, String minute)
    {
      this.beginHour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.beginMinute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
      assert (beginHour == ANY) || ((beginHour >= 0) && (beginHour <= 23)) : beginHour;
      assert (beginMinute == ANY) || ((beginMinute >= 0) && (beginMinute <= 59)) : beginMinute;
    }

    /** set beginn time
     * @param time time string
     */
    void setBeginTime(String time)
    {
      String[] parts = time.split(":");
      setBeginTime(parts[0],parts[1]);
    }

    /** get end hour value
     * @return end hour string
     */
    String getEndHour()
    {
      assert (endHour == ANY) || ((endHour >= 0) && (endHour <= 23)) : endHour;

      return (endHour != ANY) ? String.format("%02d",endHour) : "*";
    }

    /** get end minute value
     * @return end minute string
     */
    String getEndMinute()
    {
      assert (endMinute == ANY) || ((endMinute >= 0) && (endMinute <= 59)) : endMinute;

      return (endMinute != ANY) ? String.format("%02d",endMinute) : "*";
    }

    /** get end time value
     * @return end time string
     */
    String getEndTime()
    {
      StringBuilder buffer = new StringBuilder();

      buffer.append(getEndHour());
      buffer.append(':');
      buffer.append(getEndMinute());

      return buffer.toString();
    }

    /** set end time
     * @param hour hour value
     * @param minute minute value
     */
    void setEndTime(String hour, String minute)
    {
      this.endHour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.endMinute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
      assert (endHour == ANY) || ((endHour >= 0) && (endHour <= 23)) : endHour;
      assert (endMinute == ANY) || ((endMinute >= 0) && (endMinute <= 59)) : endMinute;
    }


    /** set end time
     * @param time time string
     */
    void setEndTime(String time)
    {
      String[] parts = time.split(":");
      setEndTime(parts[0],parts[1]);
    }

    /** set date
     * @param year year value
     * @param month month value
     * @param day day value
     * @param hour hour value
     * @param minute minute value
     */
    private void setLastExecutedDateTime(String year, String month, String day, String hour, String minute)
    {
      this.year  = !year.equals ("*") ? Integer.parseInt(year ) : ANY;
      this.month = !month.equals("*") ? Integer.parseInt(month) : ANY;
      this.day   = !day.equals  ("*") ? Integer.parseInt(day  ) : ANY;
      assert (this.year == ANY) || (this.year >= 0) : this.hour;
      assert (this.month == ANY) || ((this.month >= 1) && (this.month <= 12)) : this.hour;
      assert (this.day == ANY) || ((this.day >= 1) && (this.day <= 31)) : this.hour;

      this.hour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.minute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
      assert (this.hour == ANY) || ((this.hour >= 0) && (this.hour <= 23)) : this.hour;
      assert (this.minute == ANY) || ((this.minute >= 0) && (this.minute <= 59)) : this.minute;
    }

    /** set date
     * @param year year value
     * @param month month value
     * @param day day value
     * @param hour hour value
     * @param minute minute value
     */
    private void setNextExecutedDateTime(String year, String month, String day, String hour, String minute)
    {
      this.year  = !year.equals ("*") ? Integer.parseInt(year ) : ANY;
      this.month = !month.equals("*") ? Integer.parseInt(month) : ANY;
      this.day   = !day.equals  ("*") ? Integer.parseInt(day  ) : ANY;
      assert (this.year == ANY) || (this.year >= 0) : this.hour;
      assert (this.month == ANY) || ((this.month >= 1) && (this.month <= 12)) : this.hour;
      assert (this.day == ANY) || ((this.day >= 1) && (this.day <= 31)) : this.hour;

      this.hour   = !hour.equals  ("*") ? Integer.parseInt(hour,  10) : ANY;
      this.minute = !minute.equals("*") ? Integer.parseInt(minute,10) : ANY;
      assert (this.hour == ANY) || ((this.hour >= 0) && (this.hour <= 23)) : this.hour;
      assert (this.minute == ANY) || ((this.minute >= 0) && (this.minute <= 59)) : this.minute;
    }

    /** convert data to string
     */
    public String toString()
    {
      return "ScheduleInfo {"+jobName+", "+uuid+", "+getDate()+", "+getWeekDays()+", "+getTime()+", "+archiveType+", "+noStorage+", "+enabled+", "+lastExecutedDateTime+", "+nextExecutedDateTime+"}";
    }
  }

  /** entry info
   */
  static class EntryInfo
  {
    // file
    class File
    {
      String name;
      long   size;
      long   dateTime;
      long   fragmentCount;
    };

    // image
    class Image
    {
      String name;
      long   size;
      long   blockOffset;
      long   blockCount;
    };

    // directory
    class Directory
    {
      String name;
      long   dateTime;
    };

    // link
    class Link
    {
      String name;
      String destinationName;
      long   dateTime;
    };

    // hardlink
    class HardLink
    {
      String name;
      long   size;
      long   dateTime;
      long   fragmentCount;
    };

    // special
    class Special
    {
      String name;
      long   dateTime;
    };

    EntryTypes entryType;
    long       entryId;
    String     storageName;

    File       file;
    Image      image;
    Directory  directory;
    Link       link;
    HardLink   hardLink;
    Special    special;

    public static EntryInfo newFile(long   entryId,
                                    String storageName,
                                    String fileName,
                                    long   size,
                                    long   dateTime,
                                    long   fragmentCount
                                   )
    {
      EntryInfo entryInfo = new EntryInfo();
      entryInfo.entryType          = EntryTypes.FILE;
      entryInfo.entryId            = entryId;
      entryInfo.storageName        = storageName;
      entryInfo.file = entryInfo.new File();
      entryInfo.file.name          = fileName;
      entryInfo.file.size          = size;
      entryInfo.file.dateTime      = dateTime;
      entryInfo.file.fragmentCount = fragmentCount;

      return entryInfo;
    };

    public static EntryInfo newImage(long   entryId,
                                     String storageName,
                                     String imageName,
                                     long   size,
                                     long   blockOffset,
                                     long   blockCount
                                    )
    {
      EntryInfo entryInfo = new EntryInfo();
      entryInfo.entryType         = EntryTypes.IMAGE;
      entryInfo.entryId           = entryId;
      entryInfo.storageName       = storageName;
      entryInfo.image = entryInfo.new Image();
      entryInfo.image.name        = imageName;
      entryInfo.image.size        = size;
      entryInfo.image.blockOffset = blockOffset;
      entryInfo.image.blockCount  = blockCount;

      return entryInfo;
    }

    public static EntryInfo newDirectory(long   entryId,
                                         String storageName,
                                         String directoryName,
                                         long   dateTime
                                        )
    {
      EntryInfo entryInfo = new EntryInfo();
      entryInfo.entryType          = EntryTypes.DIRECTORY;
      entryInfo.entryId            = entryId;
      entryInfo.storageName        = storageName;
      entryInfo.directory = entryInfo.new Directory();
      entryInfo.directory.name     = directoryName;
      entryInfo.directory.dateTime = dateTime;

      return entryInfo;
    }

    public static EntryInfo newLink(long   entryId,
                                    String storageName,
                                    String linkName,
                                    String destinationName
                                   )
    {
      EntryInfo entryInfo = new EntryInfo();
      entryInfo.entryType            = EntryTypes.LINK;
      entryInfo.entryId              = entryId;
      entryInfo.storageName          = storageName;
      entryInfo.link = entryInfo.new Link();
      entryInfo.link.name            = linkName;
      entryInfo.link.destinationName = destinationName;

      return entryInfo;
    }

    public static EntryInfo newHardLink(long   entryId,
                                        String storageName,
                                        String fileName,
                                        long   size,
                                        long   dateTime,
                                        long   fragmentCount
                                       )
    {
      EntryInfo entryInfo = new EntryInfo();
      entryInfo.entryType              = EntryTypes.HARDLINK;
      entryInfo.entryId                = entryId;
      entryInfo.storageName            = storageName;
      entryInfo.hardLink = entryInfo.new HardLink();
      entryInfo.hardLink.name          = fileName;
      entryInfo.hardLink.size          = size;
      entryInfo.hardLink.dateTime      = dateTime;
      entryInfo.hardLink.fragmentCount = fragmentCount;

      return entryInfo;
    }

    public static EntryInfo newSpecial(long   entryId,
                                       String storageName,
                                       String name,
                                       long   dateTime
                                      )
    {
      EntryInfo entryInfo = new EntryInfo();
      entryInfo.entryType        = EntryTypes.SPECIAL;
      entryInfo.entryId          = entryId;
      entryInfo.storageName      = storageName;
      entryInfo.special = entryInfo.new Special();
      entryInfo.special.name     = name;
      entryInfo.special.dateTime = dateTime;

      return entryInfo;
    }
  };

  /** list local directory
   */
  public static ListDirectory<File> listDirectory = new ListDirectory<File>()
  {
    /** get new file instance
     * @param path path (can be null)
     * @param name name
     * @return file instance
     */
    @Override
    public File newFileInstance(String path, String name)
    {
      if (path != null)
      {
        return new File(path,name);
      }
      else
      {
        return new File(name);
      }
    }

    /** get parent file instance
     * @param file file
     * @return parent file instance or null
     */
    @Override
    public File getParentFile(File file)
    {
      return file.getParentFile();
    }

    /** get absolute path
     * @param path path
     * @return absolute path
     */
    @Override
    public String getAbsolutePath(File file)
    {
      return file.getAbsolutePath();
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

    /** check if directory is root entry
     * @param directory directory to check
     * @return true iff is root entry
     */
    @Override
    public boolean isRoot(File directory)
    {
      return directory.getAbsolutePath() == "/";
    }

    /** check if directory
     * @param file file to check
     * @return true if file is directory
     */
    @Override
    public boolean isDirectory(File file)
    {
      return file.isDirectory();
    }

    /** check if file
     * @param file file to check
     * @return true if file is file
     */
    @Override
    public boolean isFile(File file)
    {
      return file.isFile();
    }

    /** check if hidden
     * @param file file to check
     * @return true if file is hidden
     */
    @Override
    public boolean isHidden(File file)
    {
      return file.isHidden();
    }

    /** check if exists
     * @param file file to check
     * @return true if file exists
     */
    @Override
    public boolean exists(File file)
    {
      return file.exists();
    }

    /** make directory
     * @param directory directory to create
     * @return true if directory created
     */
    @Override
    public void mkdir(File directory)
      throws IOException
    {
      directory.mkdir();
    }

    /** delete file or directory
     * @param file file or directory to delete
     * @return true if file or directory deleted
     */
    @Override
    public void delete(File file)
      throws IOException
    {
      // do not delete root file
      for (File root : getRoots())
      {
        if (root.compareTo(file) == 0) return;
      }

      file.delete();
    }

    private File files[];
    private int  index;
  };

  // -------------------------- constants -------------------------------

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
  final static int USER_EVENT_SELECT_SERVER = 0xFFFF+0;
  final static int USER_EVENT_NEW_JOB       = 0xFFFF+1;
  final static int USER_EVENT_UPDATE_JOB    = 0xFFFF+2;
  final static int USER_EVENT_DELETE_JOB    = 0xFFFF+3;
  final static int USER_EVENT_SELECT_JOB    = 0xFFFF+4;

  // string with "all files" extension
  public static final String ALL_FILE_EXTENSION;

  // version, email address, homepage URL
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

  private static final OptionSpecial OPTION_GEOMETRY = new OptionSpecial()
  {
    public void parse(String string, Object userData)
    {
      final Pattern PATTERN1 = Pattern.compile("^(\\d+)x(\\d+)$",Pattern.CASE_INSENSITIVE);
      final Pattern PATTERN2 = Pattern.compile("^\\+(\\d+)\\+(\\d+)$",Pattern.CASE_INSENSITIVE);
      final Pattern PATTERN3 = Pattern.compile("^(\\d+)x(\\d+)\\+(\\d+)\\+(\\d+)$",Pattern.CASE_INSENSITIVE);

      Settings.Geometry geometry = (Settings.Geometry)userData;

      Matcher matcher;

      if      ((matcher = PATTERN1.matcher(string)).matches())
      {
        geometry.width  = Integer.parseInt(matcher.group(1));
        geometry.height = Integer.parseInt(matcher.group(2));
      }
      else if ((matcher = PATTERN2.matcher(string)).matches())
      {
        geometry.x      = Integer.parseInt(matcher.group(1));
        geometry.y      = Integer.parseInt(matcher.group(2));
      }
      else if ((matcher = PATTERN3.matcher(string)).matches())
      {
        geometry.width  = Integer.parseInt(matcher.group(1));
        geometry.height = Integer.parseInt(matcher.group(2));
        geometry.x      = Integer.parseInt(matcher.group(3));
        geometry.y      = Integer.parseInt(matcher.group(4));
      }
      else
      {
        throw new Error("Invalid geometry '"+string+"'");
      }
    }
  };

  private static final Option[] OPTIONS =
  {
    new Option("--config",                       null,Options.Types.STRING,     "configFileName"),
    new Option("--port",                         "-p",Options.Types.INTEGER,    "serverPort"),
    new Option("--tls-port",                     null,Options.Types.INTEGER,    "serverTLSPort"),
    new Option("--ca-file",                      null,Options.Types.STRING,     "serverCAFileName"),
    new Option("--keystore-file",                null,Options.Types.STRING,     "serverKeystoreFileName"),
    new Option("--no-tls",                       null,Options.Types.BOOLEAN,    "serverNoTLS"),
    new Option("--force-tls",                    null,Options.Types.BOOLEAN,    "serverForceTLS"),
    new Option("--insecure-tls",                 null,Options.Types.BOOLEAN,    "serverInsecureTLS"),
    new Option("--password",                     null,Options.Types.STRING,     "serverPassword"),

    new Option("--login-dialog",                 null,Options.Types.BOOLEAN,    "loginDialogFlag"),
    new Option("--pair-master",                  null,Options.Types.BOOLEAN,    "pairMasterFlag"),

    new Option("--select-job",                   null,Options.Types.STRING,     "selectedJobName"),
    new Option("--job",                          "-j",Options.Types.STRING,     "runJobNames"),
    new Option("--archive-type",                 null,Options.Types.ENUMERATION,"archiveType",ARCHIVE_TYPE_ENUMERATION),
    new Option("--abort",                        null,Options.Types.STRING,     "abortJobName"),
    new Option("--pause",                        "-t",Options.Types.INTEGER,    "pauseTime",new Object[]{"s",1,"m",60,"h",60*60}),
    new Option("--maintenance",                  "-m",Options.Types.INTEGER,    "maintenanceTime",new Object[]{"s",1,"seconds",1,"m",60,"min",60,"minutes",60,"h",60*60,"hours",60*60}),
    new Option("--ping",                         "-i",Options.Types.BOOLEAN,    "pingFlag"),
    new Option("--suspend",                      "-s",Options.Types.BOOLEAN,    "suspendFlag"),
    new Option("--continue",                     "-c",Options.Types.BOOLEAN,    "continueFlag"),
// TODO: deprecated. remove
    new Option("--list",                         null,Options.Types.BOOLEAN,    "listJobsFlag"),
    new Option("--list-jobs",                    "-l",Options.Types.BOOLEAN,    "listJobsFlag"),
    new Option("--list-schedules",               null,Options.Types.BOOLEAN,    "listSchedulesFlag"),
    new Option("--info",                         null,Options.Types.STRING,     "infoJobName"),

    new Option("--index-database-info",          null,Options.Types.BOOLEAN,    "indexDatabaseInfo"),
    new Option("--index-database-add",           null,Options.Types.STRING,     "indexDatabaseAddStorageName"),
    new Option("--index-database-remove",        null,Options.Types.STRING,     "indexDatabaseRemoveStorageName"),
    new Option("--index-database-refresh",       null,Options.Types.STRING,     "indexDatabaseRefreshStorageName"),
    new Option("--index-database-entities-list", "-n",Options.Types.STRING,     "indexDatabaseEntitiesListName",""),
    new Option("--index-database-storages-list", "-a",Options.Types.STRING,     "indexDatabaseStoragesListName",""),
    new Option("--index-database-entries-list",  "-e",Options.Types.STRING,     "indexDatabaseEntriesListName",""),
    new Option("--index-database-entries-newest",null,Options.Types.BOOLEAN,    "indexDatabaseEntriesNewestOnly"),
    new Option("--index-database-history-list",  null,Options.Types.BOOLEAN,    "indexDatabaseHistoryList"),

    new Option("--restore",                      null,Options.Types.STRING,     "restoreStorageName"),
    new Option("--destination",                  null,Options.Types.STRING,     "destination"),
    new Option("--overwrite-entries",            null,Options.Types.BOOLEAN,    "overwriteEntriesFlag"),

    new Option("--role",                         null,Options.Types.ENUMERATION,"role",ROLE_ENUMERATION),

    new Option("--geometry",                     null,Options.Types.SPECIAL,    "geometry",OPTION_GEOMETRY),

    new Option("--long-format",                  "-L",Options.Types.BOOLEAN,    "longFormatFlag"),
    new Option("--no-header-footer",             null,Options.Types.BOOLEAN,    "noHeaderFooterFlag"),

    new Option("--version",                      null,Options.Types.BOOLEAN,    "versionFlag"),
    new Option("--help",                         "-h",Options.Types.BOOLEAN,    "helpFlag"),
    new Option("--xhelp",                        null,Options.Types.BOOLEAN,    "xhelpFlag"),

    new Option("--debug",                        "-d",Options.Types.INCREMENT,  "debugLevel"),
    new Option("--debug-ignore-protocol-version",null,Options.Types.BOOLEAN,    "debugIgnoreProtocolVersion"),
    new Option("--debug-fake-tls",               null,Options.Types.BOOLEAN,    "debugFakeTLSFlag"),
    new Option("--debug-quit-server",            null,Options.Types.BOOLEAN,    "debugQuitServerFlag"),

    // deprecated
    new Option("--no-ssl",                       null,Options.Types.BOOLEAN,    "serverNoTLS"),
    new Option("--force-ssl",                    null,Options.Types.BOOLEAN,    "serverForceTLS"),

    // ignored
    new Option("--swing",                        null, Options.Types.BOOLEAN,   null),
  };

  // -------------------------- variables -------------------------------
  // images
  private static Image IMAGE_ERROR;
  private static Image IMAGE_LOCK;
  private static Image IMAGE_LOCK_INSECURE;

  private static I18n      i18n;
  private static Display   display;
  private static Shell     shell;
  private static Clipboard clipboard;
  private static Cursor    waitCursor;
  private static int       waitCursorCount = 0;

  private LoginData        loginData;
  private Menu             serverMenu;
  private MenuItem         serverMenuLastSelectedItem = null;
  private MenuItem         masterMenuItem;
  private TabFolder        tabFolder;
  private TabStatus        tabStatus;
  private TabJobs          tabJobs;
  private TabRestore       tabRestore;
  private boolean          quitFlag = false;

  // ----------------------- native functions ---------------------------

  // --------------------------- methods --------------------------------

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
   * @param text text to translate
   * @param arguments for text
   * @return internationalized text
   */
  static String tr(String text, Object... arguments)
  {
    // see https://github.com/jgettext/gettext-commons/wiki/Tutorial
    return i18n.tr(text,arguments);
  }

  /** get internationalized text with context
   * @param context context string
   * @param text text to translate
   * @param arguments text
   * @return internationalized text
   */
  static String trc(String context, String text)
  {
    // see https://github.com/jgettext/gettext-commons/wiki/Tutorial
    return i18n.trc(context,text);
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
   * @param output output stream
   * @param throwable throwable
   */
  public static void printStackTrace(PrintStream output, Throwable throwable)
  {
    for (StackTraceElement stackTraceElement : throwable.getStackTrace())
    {
      output.println("  "+stackTraceElement);
    }
    Throwable cause = throwable.getCause();
    while (cause != null)
    {
      StackTraceElement stackTrace[] = cause.getStackTrace();
      output.println("Caused by:");
      for (StackTraceElement stackTraceElement : stackTrace)
      {
        output.println("  "+stackTraceElement);
      }
      cause = cause.getCause();
    }
  }

  /** print stack trace
   * @param throwable throwable
   */
  public static void printStackTrace(Throwable throwable)
  {
    printStackTrace(System.err,throwable);
  }

  /** log throwable information
   * @param throwable throwable
   */
  public static void logThrowable(Throwable throwable)
  {
    final String BARCONTROL_LOG_FILE_NAME = System.getProperty("user.home")+File.separator+".bar"+File.separator+"barcontrol.log";

    // output to console
    if (Settings.debugLevel > 0)
    {
      printError(throwable);
    }

    // store into log file
    PrintStream output = null;
    try
    {
      File file = new File(BARCONTROL_LOG_FILE_NAME);

      // create directory
      File directory = file.getParentFile();
      if ((directory != null) && !directory.exists()) directory.mkdirs();

      // open file
      output = new PrintStream(new FileOutputStream(file,true));

      // write version+stack trace
      output.println("Date/Time: "+new Date().toString());
      output.println("Version: "+Version.STRING);
      output.println("Protocol version: "+BARServer.PROTOCOL_VERSION);
      output.println("Java version: "+System.getProperty("java.version"));
      output.println("Error: "+throwable.getMessage());
      printStackTrace(output,throwable);
      output.println("---");

      // close file
      output.close(); output = null;
    }
    catch (IOException exception)
    {
      // ignored
    }
    finally
    {
      if (output != null) output.close();
    }
  }

  /** print error to stderr
   * @param throwable throwable
   */
  public static void printError(Throwable throwable)
  {
    System.err.println(BARControl.tr("ERROR")+": "+((throwable.getMessage() != null) ? throwable.getMessage() : throwable.toString()));
  }

  /** print error to stderr
   * @param format format string
   * @param args optional arguments
   */
  public static void printError(String format, Object... args)
  {
    System.err.println(BARControl.tr("ERROR")+": "+String.format(format,args));
  }

  /** print warning to stderr
   * @param format format string
   * @param args optional arguments
   */
  public static void printWarning(String format, Object... args)
  {
    System.err.println(BARControl.tr("Warning")+": "+String.format(format,args));
  }

  /** print internal error to stderr
   * @param format format string
   * @param args optional arguments
   */
  public static void printInternalError(String format, Object... args)
  {
    System.err.println("INTERNAL ERROR: "+String.format(format,args));
    System.err.println("Version "+Version.STRING);
    System.err.println("Please report this error to "+EMAIL_ADDRESS+".");
  }

  /** print internal error to stderr
   * @param throwable throwable
   */
  public static void printInternalError(Throwable throwable)
  {
    System.err.println("INTERNAL ERROR: "+throwable.getMessage());
    printStackTrace(throwable);
    logThrowable(throwable);
    System.err.println("Version "+Version.STRING);
    System.err.println("Please report this error to "+EMAIL_ADDRESS+".");
  }

  /** renice SSL exception (remove java.security.cert.CertPathValidatorException text from exception)
   * @param exception i/o exception to renice
   * @return reniced exception
   */
  public static SSLException reniceSSLException(SSLException exception)
  {
    final Pattern PATTERN1 = Pattern.compile("^.*\\.CertPathValidatorException:\\s*(.*)$",Pattern.CASE_INSENSITIVE);

    Matcher matcher;
    if      ((matcher = PATTERN1.matcher(exception.getMessage())).matches())
    {
      exception = new SSLException(matcher.group(1),exception);
    }

    return exception;
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
                      "Version "+Version.STRING+"\n"+
                      "\n"+
                      BARControl.tr("Please report this error to ")+BARControl.EMAIL_ADDRESS+"." // use MAIL_AT to avoid SPAM
                     );
      }
    });
  }

  /** print internal error and exit
   * @param throwable throwable
   */
  public static void internalError(Throwable throwable)
  {
    printInternalError(throwable);
    showFatalError(throwable);
    System.exit(ExitCodes.INTERNAL_ERROR);
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

  // ---------------------------------------------------------------------

  /** print program usage
   */
  private void printUsage()
  {
    System.out.println("barcontrol usage: <options> [--] [<host name>[|:<port>]]");
    System.out.println("");
    System.out.println("Options: --config=<file name>                       - configuration file name (default: "+Settings.DEFAULT_BARCONTROL_CONFIG_FILE_NAME+")");
    System.out.println("         -p|--port=<n>                              - server port (default: "+Settings.DEFAULT_SERVER_PORT+")");
    System.out.println("         --tls-port=<n>                             - TLS server port (default: "+Settings.DEFAULT_SERVER_TLS_PORT+")");
    System.out.println("         --password=<password>                      - server password (use with care!)");
    System.out.println("         --ca-file=<file name>                      - additional trusted certificate authority file name (PEM format)");
    System.out.println("         --keystore-file=<file name>                - keystore file name (JKS format, default: ");
    System.out.println("                                                        ."+File.separator+BARServer.DEFAULT_KEYSTORE_FILE_NAME+" or ");
    System.out.println("                                                        "+System.getProperty("user.home")+File.separator+".bar"+File.separator+BARServer.DEFAULT_KEYSTORE_FILE_NAME+" or ");
    System.out.println("                                                        "+Config.CONFIG_DIR+File.separator+BARServer.DEFAULT_KEYSTORE_FILE_NAME);
    System.out.println("                                                      )" );
    System.out.println("         --no-tls                                   - no TLS connection");
    System.out.println("         --force-tls                                - force TLS connection");
    System.out.println("         --insecure-tls                             - allow insecure TLS connections");
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
    System.out.println("         -s|--suspend                               - suspend job execution");
    System.out.println("         -c|--continue                              - continue job execution");
    System.out.println("         -t|--pause=<n>[s|m|h]                      - pause job execution for <n> seconds/minutes/hours");
    System.out.println("         -m|--maintenance=<n>[s|m|h]                - set intermediate maintenance for <n> seconds/minutes/hours");
    System.out.println("         -i|--ping                                  - check connection to server");
    System.out.println("         -l|--list-jobs                             - list jobs");
    System.out.println("         --list-schedules                           - list schedules");
    System.out.println("");
    System.out.println("         --index-database-info                      - print index info");
    System.out.println("         --index-database-add=<name|directory>      - add storage archive <name> or all .bar files to index");
    System.out.println("         --index-database-remove=<text>             - remove matching storage archives from index");
    System.out.println("         --index-database-refresh=<text>            - refresh matching storage archive in index");
    System.out.println("         -n|--index-database-entities-list <text>   - list index entities");
    System.out.println("         -a|--index-database-storages-list <text>   - list index storage archives");
    System.out.println("         -e|--index-database-entries-list <text>    - list index entries");
    System.out.println("         --index-database-entries-newest            - list index newest entries only");
    System.out.println("         --index-database-history-list              - list index history");
    System.out.println("");
    System.out.println("         --restore=<name>                           - restore storage <name>");
    System.out.println("         --destination=<directory>                  - destination to restore entries");
    System.out.println("         --overwrite-entries                        - overwrite existing entries on restore");
    System.out.println("");
    System.out.println("         -L|--long-format                           - list in long format");
    System.out.println("         --no-header-footer                         - no header/footer output in list");
    System.out.println("");
    System.out.println("         --role=<role>                              - select role:");
    System.out.println("                                                        basic (default)");
    System.out.println("                                                        normal");
    System.out.println("                                                        expert");
    System.out.println("");
    System.out.println("         --geometry=<x>x<y>[+<x0>+<y0>]             - window geometry");
    System.out.println("");
    System.out.println("         --version                                  - output version");
    System.out.println("         -h|--help                                  - print this help");
    if (Settings.xhelpFlag)
    {
      System.out.println("");
      System.out.println("         -d|--debug                                 - enable debug mode");
      System.out.println("         --debug-ignore-protocol-version            - ignore protocol version");
      System.out.println("         --debug-fake-tls                           - fake TLS connections");
      System.out.println("         --debug-quit-server                        - send quit-command to server");
    }
  }

  /** print program version
   */
  private void printVersion()
  {
    System.out.println("barcontrol "+Version.STRING);
  }

  /** parse arguments
   * @param args arguments
   */
  private void parseArguments(String[] args)
  {
    // parse arguments
    int     i = 0;
    boolean endOfOptions = false;
    while (i < args.length)
    {
      if      (!endOfOptions && args[i].equals("--"))
      {
        endOfOptions = true;
        i++;
      }
      else if (!endOfOptions && (args[i].startsWith("--") || args[i].startsWith("-")))
      {
        int j = Options.parse(OPTIONS,args,i,Settings.class);
        if (j < 0)
        {
          throw new Error("Unknown option '"+args[i]+"'!");
        }
        i = j;
      }
      else
      {
        final Pattern PATTERN_SERVER_PORT = Pattern.compile("^(.*):(\\d+)$");

        Matcher matcher;
        if ((matcher = PATTERN_SERVER_PORT.matcher(args[i])).matches())
        {
          Settings.serverName = matcher.group(1);
          Settings.serverPort = Integer.parseInt(matcher.group(2));
        }
        else
        {
          Settings.serverName = args[i];
        }
        i++;
      }
    }

    // help
    if (Settings.helpFlag || Settings.xhelpFlag)
    {
      printUsage();
      System.exit(ExitCodes.OK);
    }

    // version
    if (Settings.versionFlag)
    {
      printVersion();
      System.exit(ExitCodes.OK);
    }

    // check arguments
//TODO: check PEM
if (false) {
    if ((Settings.serverKeystoreFileName != null) && !Settings.serverKeystoreFileName.isEmpty())
    {
      // check if PEM/JKS file is readable
      try
      {
        KeyStore keyStore = java.security.KeyStore.getInstance("JKS");
        keyStore.load(new java.io.FileInputStream(Settings.serverKeystoreFileName),null);
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
        throw new Error("Java Key Store (JKS) file '"+Settings.serverKeystoreFileName+"' not found");
      }
      catch (IOException exception)
      {
        throw new Error("not a Java Key Store (JKS) file '"+Settings.serverKeystoreFileName+"'",exception);
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
  private static String getJobUUID(final String jobName)
  {
    final String jobUUID[] = {null};

    try
    {
      BARServer.executeCommand(StringParser.format("JOB_LIST"),
                               1,  // debug level
                               new Command.ResultHandler()
                               {
                                 @Override
                                 public void handle(int i, ValueMap valueMap)
                                 {
                                   String jobUUID_ = valueMap.getString("jobUUID");
                                   String name     = valueMap.getString("name" );

                                   if (jobName.equalsIgnoreCase(name))
                                   {
                                     jobUUID[0] = jobUUID_;
                                   }
                                 }
                               }
                              );
    }
    catch (Exception exception)
    {
      if (Settings.debugLevel > 0)
      {
        printError("cannot get job list (error: %s)",exception.getMessage());
        logThrowable(exception);
        BARServer.disconnect();
        System.exit(ExitCodes.FAIL);
      }
    }

    return jobUUID[0];
  }

  /** connect to server
   * @param name server name
   * @param port server port
   * @param tlsPort server TLS port
   * @param caFileName server certificate authority file
   * @param keystoreFileName Java keystore file name (JKS only)
   * @param tlsMode TLS mode; see BARServer.TLSModes
   * @param insecureTLS TRUE to accept insecure TLS connections (no certificates check)
   * @param password login password
   */
  private void connect(String             name,
                       int                port,
                       int                tlsPort,
                       String             caFileName,
                       String             keystoreFileName,
                       BARServer.TLSModes tlsMode,
                       boolean            insecureTLS,
                       String             password
                      )
    throws ConnectionError
  {
    BusyDialog busyDialog = null;
    {
      if (display != null)
      {
        // process SWT events
        while (display.readAndDispatch())
        {
          // nothing to do
        }

        // busy dialog
        if (shell != null)
        {
          busyDialog = new BusyDialog(shell,
                                      BARControl.tr("Connect"),
                                      400,
                                      60,
                                      BARControl.tr("Try to connect to ''")+name+"'\u2026",
                                      BusyDialog.AUTO_ANIMATE
                                     );
        }
      }
    }
    try
    {
      BARServer.connect(display,
                        name,
                        port,
                        tlsPort,
                        caFileName,
                        keystoreFileName,
                        tlsMode,
                        insecureTLS,
                        password
                       );
    }
    finally
    {
      if (busyDialog != null) busyDialog.close();
    }
  }

  /** reconnect
   * @param tlsMode TLS mode; see BARServer.TLSModes
   * @param insecureTLS true to allow insecure TLS
   */
  private void reconnect(BARServer.TLSModes tlsMode, boolean insecureTLS)
    throws ConnectionError,CommunicationError
  {
    connect(loginData.name,
            loginData.port,
            loginData.tlsPort,
            Settings.serverCAFileName,
            Settings.serverKeystoreFileName,
            tlsMode,
            insecureTLS,
            loginData.password
           );

    // show warning if no TLS connection established
    if ((loginData.tlsMode == BARServer.TLSModes.TRY) && !BARServer.isTLSConnection())
    {
      Dialogs.warning(new Shell(),
                      Dialogs.booleanFieldUpdater(Settings.class,"showNoneTLSWarning"),
                      BARControl.tr("Established a none-TLS connection only.\nTransmitted data may be vulnerable!")
                     );
    }
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
      private String                  homepageVersionPatch    = null;
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
              display.syncExec(new Runnable()
              {
                public void run()
                {
                  if (!shell.isDisposed())
                  {
                    switch (Dialogs.select(shell,
                                           BARControl.tr("Warning"),
                                           BARControl.tr("Class/JAR file ''{0}'' changed. Is is recommended to restart BARControl now.",file.getName()),
                                           new String[]{"Restart","Remind me again in 5min","Ignore"},
                                           0
                                          )
                           )
                    {
                      case 0:
                        // send close event with restart
                        Widgets.notify(shell,SWT.Close,ExitCodes.RESTART);
                        break;
                      case 1:
                        break;
                      case 2:
                        reminderFlag[0] = false;
                        break;
                    }
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
                && (   (homepageVersionMajor.compareTo(Version.MAJOR) > 0)
                    || (   (homepageVersionMajor.compareTo(Version.MAJOR) >= 0)
                        && (homepageVersionMinor.compareTo(Version.MINOR) > 0))
                    || (   (homepageVersionPatch != null)
                        && (homepageVersionMajor.compareTo(Version.MAJOR) >= 0)
                        && (homepageVersionMinor.compareTo(Version.MINOR) >= 0)
                        && (homepageVersionPatch.compareTo(Version.PATCH) > 0)
                       )
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
        final Pattern PATTERN_PATCH    = Pattern.compile("PATCH=(.*)");
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
            if      ((matcher = PATTERN_MAJOR.matcher(line)).matches())
            {
              homepageVersionMajor    = matcher.group(1);
            }
            else if ((matcher = PATTERN_MINOR.matcher(line)).matches())
            {
              homepageVersionMinor    = matcher.group(1);
            }
            else if ((matcher = PATTERN_PATCH.matcher(line)).matches())
            {
              homepageVersionPatch    = matcher.group(1);
            }
            else if ((matcher = PATTERN_REVISION.matcher(line)).matches())
            {
              homepageVersionRevision = matcher.group(1);
            }
          }
          homepageChangeLog.clear();
          while ((line = input.readLine()) != null)
          {
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

        String version = String.format("%s.%s%s%s",
                                       homepageVersionMajor,
                                       homepageVersionMinor,
                                       homepageVersionPatch,
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
        Widgets.layout(composite,0,0,TableLayoutData.NSWE);
        {
          // message
          label = Widgets.newImage(composite,Widgets.loadImage(shell.getDisplay(),"question.png"));
          Widgets.layout(label,0,0,TableLayoutData.W,0,0,10);

          label = Widgets.newLabel(composite,
                                   String.format("A newer version %s of BARControl is available. You\ncan download it from the BAR homepage:\n\n%s\n\nChangeLog:",
                                                 version,
                                                 URL
                                                ),
                                   SWT.LEFT|SWT.WRAP
                                  );
          Widgets.layout(label,0,1,TableLayoutData.W,0,0,4);

          // change log
          text = Widgets.newStringView(composite,SWT.BORDER|SWT.WRAP|SWT.H_SCROLL|SWT.V_SCROLL);
          text.setText(changeLog.toString());
          Widgets.layout(text,1,1,TableLayoutData.NSWE,0,0,4,0,300,300);

          // show again
          widgetShowAgain = Widgets.newCheckbox(composite,"show again");
          widgetShowAgain.setSelection(true);
          Widgets.layout(widgetShowAgain,2,1,TableLayoutData.W,0,0,4);
        }

        // buttons
        composite = new Composite(dialog,SWT.NONE);
        composite.setLayout(new TableLayout(0.0,1.0));
        Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
        {
          button = Widgets.newButton(composite,"Open browser");
          button.setFocus();
          Widgets.layout(button,0,0,TableLayoutData.W,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT);
          button.addSelectionListener(new SelectionListener()
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

          button = Widgets.newButton(composite,"Continue");
          Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT);
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

        Dialogs.run(dialog,
                    false,
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
                            Dialogs.error(shell,BARControl.tr("Cannot open default web browser:\n\n{0}",exception.getMessage()));
                          }
                          catch (IOException exception)
                          {
                            Dialogs.error(shell,BARControl.tr("Cannot open default web browser:\n\n{0}",reniceIOException(exception).getMessage()));
                          }
                          catch (Exception exception)
                          {
                            Dialogs.error(shell,BARControl.tr("Cannot open default web browser:\n\n{0}",exception.getMessage()));
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

  /** show about dialog
   * @param shell parent shell
   */
  private void showAbout(Shell shell)
  {
    final Image IMAGE_INFO      = Widgets.loadImage(shell.getDisplay(),"info.png");
    final Image IMAGE_CLIPBOARD = Widgets.loadImage(shell.getDisplay(),"clipboard.png");

    Composite composite;
    Label     label;
    Button    button;

    if (!shell.isDisposed())
    {
      final Shell dialog = Dialogs.openModal(shell,BARControl.tr("About"),300,70);
      dialog.setLayout(new TableLayout(new double[]{1.0,0.0},1.0));

      // message
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},4));
      Widgets.layout(composite,0,0,TableLayoutData.NSWE);
      {
        label = new Label(composite,SWT.LEFT);
        label.setImage(IMAGE_INFO);
        Widgets.layout(label,0,0,TableLayoutData.W,0,0,10);

        label = new Label(composite,SWT.LEFT|SWT.WRAP);
        label.setText("BAR control "+Version.STRING+".\n"+
                      "\n"+
                      BARControl.tr("Written by Torsten Rupp")+"\n"
                     );
        Widgets.layout(label,0,1,TableLayoutData.NS|TableLayoutData.W,0,0,4);

        button = new Button(composite,SWT.CENTER);
        button.setToolTipText(BARControl.tr("Copy version info to clipboard."));
        button.setImage(IMAGE_CLIPBOARD);
        Widgets.layout(button,0,2,TableLayoutData.NE);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Widgets.setClipboard(clipboard,"BAR version "+Version.STRING);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }

      // buttons
      composite = new Composite(dialog,SWT.NONE);
      composite.setLayout(new TableLayout(0.0,1.0));
      Widgets.layout(composite,1,0,TableLayoutData.WE,0,0,4);
      {
        button = new Button(composite,SWT.CENTER);
        button.setText(BARControl.tr("Close"));
        Widgets.layout(button,0,0,TableLayoutData.NONE,0,0,0,0,SWT.DEFAULT,SWT.DEFAULT,120,SWT.DEFAULT);
        button.addSelectionListener(new SelectionListener()
        {
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            Dialogs.close(dialog);
          }
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
        });
      }

      Dialogs.run(dialog);
    }
  }

  /** create main window
   */
  private void createWindow()
  {
    // create shell window
    shell = new Shell(display);
    shell.setText("BAR control");
    shell.setLayout(new TableLayout(1.0,1.0));

    // create clipboard
    clipboard = new Clipboard(display);

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
    tabFolder.addPaintListener(new PaintListener()
    {
      @Override
      public void paintControl(PaintEvent paintEvent)
      {
        TabFolder widget = (TabFolder)paintEvent.widget;
        GC        gc     = paintEvent.gc;
        Rectangle bounds;

        if (BARServer.isTLSConnection())
        {
          Image image = BARServer.isInsecureTLSConnection() ? IMAGE_LOCK_INSECURE : IMAGE_LOCK;
          bounds = image.getBounds();
          gc.drawImage(image,
                       widget.getBounds().width-bounds.width,
                       bounds.height/2-2
                      );
        }
      }
    });

    tabStatus  = new TabStatus (tabFolder,SWT.F1);
    tabJobs    = new TabJobs   (tabFolder,SWT.F2);
    tabRestore = new TabRestore(tabFolder,SWT.F3);
    tabStatus.setTabJobs(tabJobs);
    tabJobs.setTabStatus(tabStatus);
    tabRestore.setTabStatus(tabStatus);
    tabRestore.setTabJobs(tabJobs);

    // start auto update
    tabStatus.startUpdate();
    tabJobs.startUpdate();

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
            // disable menu
            serverMenu.setEnabled(false);

            // login
            Settings.Server    defaultServer = Settings.getLastServer();
            BARServer.TLSModes tlsMode;
            if      (Settings.serverForceTLS) tlsMode = BARServer.TLSModes.FORCE;
            else if (Settings.serverNoTLS)    tlsMode = BARServer.TLSModes.NONE;
            else                              tlsMode = BARServer.TLSModes.TRY;
            loginData = new LoginData((defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_PORT,
                                      (defaultServer != null) ? defaultServer.port : Settings.DEFAULT_SERVER_TLS_PORT,
                                      tlsMode,
                                      Settings.role
                                     );
            if (login(loginData,
                      true  // loginDialogFlag
                     )
               )
            {
              updateServerMenu();

              // notify new server
              Widgets.notify(shell,BARControl.USER_EVENT_SELECT_SERVER);
            }
            else
            {
              // revert selection
              if (serverMenuLastSelectedItem != null)
              {
                serverMenuLastSelectedItem.setSelection(true);
              }
            }

            // enable menu
            serverMenu.setEnabled(true);
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
          menuItem.setEnabled(   (jobData.state == JobData.States.WAITING    )
                              || (jobData.state == JobData.States.RUNNING    )
                              || (jobData.state == JobData.States.DRY_RUNNING)
                             );
        }
      });

      subMenu = Widgets.addMenu(menu,BARControl.tr("Pause"),BARServer.isMaster() && Settings.hasNormalRole());
      {
        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("10 min"));
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            tabStatus.jobPause(10*60);
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("60 min"),SWT.CTRL+'P');
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
            tabStatus.jobPause(60*60);
          }
        });

        menuItem = Widgets.addMenuItem(subMenu,BARControl.tr("120 min"));
        menuItem.addSelectionListener(new SelectionListener()
        {
          @Override
          public void widgetDefaultSelected(SelectionEvent selectionEvent)
          {
          }
          @Override
          public void widgetSelected(SelectionEvent selectionEvent)
          {
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

        menuItem = Widgets.addMenuItemCheckbox(subMenu,BARControl.tr("Index maintenance operation"),Settings.pauseIndexMaintenanceFlag,true);
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
            Settings.pauseIndexMaintenanceFlag = widget.getSelection();
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
          catch (Exception exception)
          {
            Dialogs.error(shell,BARControl.tr("Cannot clear passwords on server:\n\n{0}",exception.getMessage()));
          }
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
      shell.addListener(BARControl.USER_EVENT_SELECT_SERVER,new Listener()
      {
        public void handleEvent(Event event)
        {
          if (!masterMenuItem.isDisposed())
          {
            switch (BARServer.getMode())
            {
              case MASTER:
                masterMenuItem.setEnabled(false);
                break;
              case SLAVE:
                masterMenuItem.setEnabled(true);
                break;
            }
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

      menuItem = Widgets.addMenuItem(menu,BARControl.tr("Set additional certificate authority (CA)")+"\u2026",SWT.NONE);
      menuItem.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          String fileName = Dialogs.file(shell,
                                         Dialogs.FileDialogTypes.OPEN,
                                         BARControl.tr("Select certificate authority (CA) file"),
                                         Settings.serverCAFileName,
                                         new String[]{BARControl.tr("Certificate files PEM"),"*.pem",
                                                      BARControl.tr("Certificate files"),"*.crt",
                                                      BARControl.tr("All files"),BARControl.ALL_FILE_EXTENSION
                                                     },
                                         "*.pem",
                                         Dialogs.FILE_NONE,
                                         BARControl.listDirectory
                                        );
          if (fileName != null)
          {
            Settings.serverCAFileName = fileName;
            if (Dialogs.confirm(shell,BARControl.tr("Reconnect to server with new certificate authority?")))
            {
              boolean reconnected           = false;
              String  reconnectErrorMessage = null;

              if (!reconnected)
              {
                // try reconnect with existing TLS settings
                try
                {
                  reconnect(BARServer.TLSModes.FORCE,false);

                  Settings.serverNoTLS       = false;
                  Settings.serverForceTLS    = true;
                  Settings.serverInsecureTLS = false;

                  reconnected = true;
                }
                catch (ConnectionError error)
                {
                  reconnectErrorMessage = error.getMessage();
                }
                catch (CommunicationError error)
                {
                  reconnectErrorMessage = error.getMessage();
                }
              }

              if (!reconnected)
              {
                switch (Dialogs.select(shell,
                                       BARControl.tr("Reconnection fail"),
                                       IMAGE_ERROR,
                                       (reconnectErrorMessage != null)
                                         ? reconnectErrorMessage
                                         : BARControl.tr("Reconnect with new certificate authority fail"),
                                       new String[]
                                       {
                                         BARControl.tr("Try with insecure TLS (SSL)"),
                                         BARControl.tr("Cancel")
                                       },
                                       0
                                      )
                       )
                {
                  case 0:
                    // try reconnect with insecure TLS
                    try
                    {
                      reconnect(BARServer.TLSModes.FORCE,true);

                      Settings.serverNoTLS       = false;
                      Settings.serverForceTLS    = true;
                      Settings.serverInsecureTLS = true;

                      reconnected = true;
                    }
                    catch (ConnectionError error)
                    {
                      reconnectErrorMessage = error.getMessage();
                    }
                    catch (CommunicationError error)
                    {
                      reconnectErrorMessage = error.getMessage();
                    }
                    break;
                  case 1:
                    return;
                }
              }

              if (!reconnected)
              {
                Dialogs.error(shell,
                              (reconnectErrorMessage != null)
                                ? reconnectErrorMessage
                                : BARControl.tr("Reconnect with new certificate authority fail")
                             );
              }
            }
          }
        }
      });

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
          showAbout(shell);
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
            catch (Exception exception)
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

            final BusyDialog busyDialog = new BusyDialog(shell,
                                                         BARControl.tr("Print debug memory dump"),
                                                         500,100,
                                                         BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0
                                                        );
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
                catch (Exception exception)
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

            final BusyDialog busyDialog = new BusyDialog(shell,
                                                         BARControl.tr("Store debug memory dump"),
                                                         500,100,
                                                         BusyDialog.TEXT0|BusyDialog.PROGRESS_BAR0|BusyDialog.ABORT_CLOSE
                                                        );
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
                catch (Exception exception)
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

        menuItem = Widgets.addMenuItem(menu,"Quit server");
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
              BARServer.executeCommand(StringParser.format("QUIT"),
                                       1  // debugLevel
                                      );
            }
            catch (Exception exception)
            {
              // ignored
            }
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
    int width = 78;

/* jline 3.9.0
    try
    {
      width = TerminalBuilder.terminal().getWidth();
    }
    catch (Throwable throwable)
    {
      // ignored
    }
*/
    try
    {
      width = jline.TerminalFactory.get().getWidth();
    }
    catch (Throwable throwable)
    {
      // ignored
    }

    return width;
  }

  /** try reconnect
   * @param message error message
   * @return TRUE iff reconnected
   */
  private boolean tryReconnect(String message)
  {
    boolean connectedFlag                 = false;
    String  connectErrorMessage           = null;
    String  connectErrorExtendedMessage[] = null;

    // try to reconnect
    if (Dialogs.confirmError(new Shell(),
                             BARControl.tr("Connection lost"),
                             message,
                             BARControl.tr("Try again"),
                             BARControl.tr("Cancel")
                            )
       )
    {
      connectedFlag = login(loginData,
                            false  // loginDialogFlag
                           );
    }

    // refresh or stop if not connected
    if (connectedFlag)
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
      Widgets.notify(shell,BARControl.USER_EVENT_SELECT_SERVER);
    }
    else
    {
      // stop
      quitFlag = true;
    }

    return connectedFlag;
  }

  /** run application
   */
  private void run()
  {
    if (Settings.geometry.width  < 0) Settings.geometry.width  = 840;
    if (Settings.geometry.height < 0) Settings.geometry.height = 600+5*(Widgets.getTextHeight(shell)+4);

    // set window size/location+title, manage window (approximate height according to height of a text line)
    shell.setSize(Settings.geometry.width,Settings.geometry.height);
    if ((Settings.geometry.x >= 0) && (Settings.geometry.y >= 0))
    {
      shell.setLocation(Settings.geometry.x,Settings.geometry.y);
    }
    shell.open();
    shell.setSize(Settings.geometry.width,Settings.geometry.height);
    if ((Settings.geometry.x >= 0) && (Settings.geometry.y >= 0))
    {
      shell.setLocation(Settings.geometry.x,Settings.geometry.y);
    }
    shell.setText("BAR control "+BARServer.getMode()+": "+BARServer.getInfo());

    // listeners
    shell.addListener(BARControl.USER_EVENT_SELECT_SERVER,new Listener()
    {
      public void handleEvent(Event event)
      {
        Shell widget = (Shell)event.widget;
        if (!widget.isDisposed())
        {
          widget.setText("BAR control "+BARServer.getMode()+": "+BARServer.getInfo());
          updateMaster();
        }
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
        Widgets.notify(shell,BARControl.USER_EVENT_SELECT_JOB,jobData.uuid);
      }
    }

    // SWT event loop
    boolean connectedFlag = true;
    while (!shell.isDisposed() && connectedFlag)
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
        connectedFlag = tryReconnect(error.getMessage());
      }
      catch (SWTException exception)
      {
        // check if connection error
        Throwable throwable = exception.getCause();
        while ((throwable != null) && !(throwable instanceof ConnectionError))
        {
          throwable = throwable.getCause();
        }

        // try reconnect
        if (throwable != null)
        {
          connectedFlag = tryReconnect(exception.getCause().getMessage());
        }
        else
        {
          printInternalError(exception);
          showFatalError(exception);
          System.exit(ExitCodes.INTERNAL_ERROR);
        }
      }
      catch (AssertionError error)
      {
        printInternalError(error);
        showFatalError(error);
        System.exit(ExitCodes.INTERNAL_ERROR);
      }
      catch (InternalError error)
      {
        printInternalError(error);
        showFatalError(error);
        System.exit(ExitCodes.INTERNAL_ERROR);
      }
      catch (Throwable throwable)
      {
        printInternalError(throwable);
        showFatalError(throwable);
        System.exit(ExitCodes.INTERNAL_ERROR);
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
      @Override
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

    final Combo   widgetName;
    final Spinner widgetPort;
    final Combo   widgetTLSMode;
    final Text    widgetPassword;
    final Button  widgetRoleBasic,widgetRoleNormal,widgetRoleExpert;
    final Button  widgetLoginButton;
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(null,new double[]{0.0,1.0},2));
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = new Label(composite,SWT.LEFT);
      label.setText(BARControl.tr("Server")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      subComposite = new Composite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(null,new double[]{1.0,0.0,0.0,0.0},2));
      Widgets.layout(subComposite,0,1,TableLayoutData.WE);
      {
        widgetName = Widgets.newCombo(subComposite,SWT.LEFT|SWT.BORDER);
        widgetName.setItems(serverData);
        if (loginData.name != null) widgetName.setText(loginData.name);
        Widgets.layout(widgetName,0,0,TableLayoutData.WE);

        widgetPort = Widgets.newSpinner(subComposite,SWT.RIGHT|SWT.BORDER);
        widgetPort.setMinimum(0);
        widgetPort.setMaximum(65535);
        widgetPort.setSelection(loginData.port);
        Widgets.layout(widgetPort,0,1,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);

        label = new Label(subComposite,SWT.LEFT);
        label.setText(BARControl.tr("TLS"));
        Widgets.layout(label,0,2,TableLayoutData.W);

        widgetTLSMode = Widgets.newOptionMenu(subComposite);
        Widgets.setOptionMenuItems(widgetTLSMode,new Object[]{BARControl.tr("none" ),BARServer.TLSModes.NONE,
                                                              BARControl.tr("try"  ),BARServer.TLSModes.TRY,
                                                              BARControl.tr("force"),BARServer.TLSModes.FORCE
                                                             }
                                  );
        Widgets.setSelectedOptionMenuItem(widgetTLSMode,loginData.tlsMode);
        Widgets.layout(widgetTLSMode,0,3,TableLayoutData.W);
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
        Widgets.layout(subComposite,2,1,TableLayoutData.WE);
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
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      widgetLoginButton = Widgets.newButton(composite);
      widgetLoginButton.setText(BARControl.tr("Login"));
      Widgets.layout(widgetLoginButton,0,0,TableLayoutData.W,0,0,0,0,80,SWT.DEFAULT);
      widgetLoginButton.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          loginData.name     = widgetName.getText();
          loginData.port     = widgetPort.getSelection();
          loginData.tlsMode  = Widgets.getSelectedOptionMenuItem(widgetTLSMode,BARServer.TLSModes.TRY);
          loginData.password = widgetPassword.getText();
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
      Widgets.layout(button,0,1,TableLayoutData.E,0,0,0,0,80,SWT.DEFAULT);
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
    widgetName.addSelectionListener(new SelectionListener()
    {
      @Override
      public void widgetDefaultSelected(SelectionEvent selectionEvent)
      {
        widgetPassword.forceFocus();
      }
      @Override
      public void widgetSelected(SelectionEvent selectionEvent)
      {
        Settings.Server server = servers[widgetName.getSelectionIndex()];
        widgetName.setText((server.name != null) ? server.name : "");
        widgetPort.setSelection(server.port);
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

    Widgets.setNextFocus(widgetName,
                         widgetPort,
                         widgetPassword,
                         widgetLoginButton
                        );
    if ((loginData.name != null) && (loginData.name.length() != 0))
    {
      widgetPassword.forceFocus();
    }
    else
    {
      widgetName.forceFocus();
    }
    Boolean result = (Boolean)Dialogs.run(dialog);
    if ((result != null) && result && ((loginData.port != 0) || (loginData.tlsPort != 0)))
    {
      // store new name+port, shorten list
      Settings.addServer(loginData.name,
                         (loginData.port != 0) ? loginData.port : loginData.tlsPort,
                         loginData.password
                        );
      Settings.save();

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
      @Override
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
            // try to connect to server with current credentials
            BARServer.TLSModes tlsMode;
            if      (Settings.serverForceTLS) tlsMode = BARServer.TLSModes.FORCE;
            else if (Settings.serverNoTLS)    tlsMode = BARServer.TLSModes.NONE;
            else                              tlsMode = BARServer.TLSModes.TRY;

            loginData = new LoginData((!server.name.isEmpty()    ) ? server.name     : Settings.DEFAULT_SERVER_NAME,
                                      (server.port != 0          ) ? server.port     : Settings.DEFAULT_SERVER_PORT,
                                      (server.port != 0          ) ? server.port     : Settings.DEFAULT_SERVER_TLS_PORT,
                                      tlsMode,
                                      (!server.password.isEmpty()) ? server.password : "",
                                      Settings.role
                                     );
            if (login(loginData,
                      false  // loginDialogFlag
                     )
               )
            {
              updateServerMenu();

              // notify new server
              Widgets.notify(shell,BARControl.USER_EVENT_SELECT_SERVER);
            }
            else
            {
              // revert selection
              if (serverMenuLastSelectedItem != null)
              {
                menuItem.setSelection(false);
                serverMenuLastSelectedItem.setSelection(true);
              }
            }
          }
          else
          {
            serverMenuLastSelectedItem = menuItem;
          }
        }
      });
    }
  }

  /** update master info
   */
  private void updateMaster()
  {
    String masterName = "";
    try
    {
      ValueMap resultMap = new ValueMap();
      BARServer.executeCommand(StringParser.format("MASTER_GET"),
                               0, // debugLevel
                               resultMap
                              );
      assert resultMap.size() > 0;

      masterName = resultMap.getString("name");
    }
    catch (Exception exception)
    {
      // ignored
    }

    if (!masterMenuItem.isDisposed())
    {
      if (!masterName.isEmpty())
      {
        masterMenuItem.setText(BARControl.tr("Master")+": "+masterName);
        masterMenuItem.setSelection(true);
      }
      else
      {
        masterMenuItem.setText(BARControl.tr("Pair master")+"\u2026");
        masterMenuItem.setSelection(false);
      }
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
      int    restTime;

      Data()
      {
        this.masterName = "";
        this.restTime   = 0;
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
    Widgets.layout(composite,0,0,TableLayoutData.WE);
    {
      label = Widgets.newLabel(composite);
      label.setText(BARControl.tr("Wait for pairing")+":");
      Widgets.layout(label,0,0,TableLayoutData.W);

      widgetProgressBar = new ProgressBar(composite);
      Widgets.layout(widgetProgressBar,0,1,TableLayoutData.WE);

      label = Widgets.newLabel(composite);
      label.setText(BARControl.tr("Master")+":");
      label.setLayoutData(new TableLayoutData(1,0,TableLayoutData.W));
      Widgets.layout(label,1,0,TableLayoutData.W);

      widgetMasterName = Widgets.newView(composite);
      Widgets.layout(widgetMasterName,1,1,TableLayoutData.WE);
    }

    // buttons
    composite = Widgets.newComposite(dialog);
    composite.setLayout(new TableLayout(0.0,1.0));
    Widgets.layout(composite,1,0,TableLayoutData.WE);
    {
      widgetOKButton = Widgets.newButton(composite);
      widgetOKButton.setText(BARControl.tr("OK"));
      widgetOKButton.setEnabled(false);
      Widgets.layout(widgetOKButton,0,0,TableLayoutData.W,0,0,0,0,100,SWT.DEFAULT);
      widgetOKButton.addSelectionListener(new SelectionListener()
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
            BARServer.executeCommand(StringParser.format("MASTER_PAIRING_STOP pair=yes"),
                                     1  // debugLevel
                                    );
          }
          catch (final BARException exception)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                Dialogs.close(dialog,false);
                Dialogs.error(shell,BARControl.tr("Cannot set new master:\n\n{0}",exception.getMessage()));
              }
            });
            return;
          }
          catch (final IOException exception)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                Dialogs.close(dialog,false);
                Dialogs.error(shell,BARControl.tr("Cannot set new master:\n\n{0}",exception.getMessage()));
              }
            });
            return;
          }

          Dialogs.close(dialog,true);
        }
      });

      widgetRestartButton = Widgets.newButton(composite);
      widgetRestartButton.setText(BARControl.tr("Restart"));
      widgetRestartButton.setEnabled(false);
      Widgets.layout(widgetRestartButton,0,1,TableLayoutData.NONE,0,0,0,0,100,SWT.DEFAULT);
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

          try
          {
            BARServer.executeCommand(StringParser.format("MASTER_PAIRING_START"),
                                     1  // debugLevel
                                    );
          }
          catch (final BARException exception)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                Dialogs.close(dialog,false);
                Dialogs.error(shell,BARControl.tr("Cannot restart pairing master:\n\n{0}",exception.getMessage()));
              }
            });
            return;
          }
          catch (final IOException exception)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                Dialogs.close(dialog,false);
                Dialogs.error(shell,BARControl.tr("Cannot restart pairing master:\n\n{0}",exception.getMessage()));
              }
            });
            return;
          }
          Background.run(pairMasterRunnable[0]);
        }
      });

      button = Widgets.newButton(composite);
      button.setText(BARControl.tr("Cancel"));
      Widgets.layout(button,0,2,TableLayoutData.E,0,0,0,0,100,SWT.DEFAULT);
      button.addSelectionListener(new SelectionListener()
      {
        @Override
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        @Override
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          // stop pairing
          pairMasterRunnable[0].abort();
          try
          {
            BARServer.executeCommand(StringParser.format("MASTER_PAIRING_STOP pair=no"),
                                     1  // debugLevel
                                    );
          }
          catch (final BARException exception)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                Dialogs.close(dialog,false);
                Dialogs.error(shell,BARControl.tr("Cannot stop pairing master:\n\n{0}",exception.getMessage()));
              }
            });
            return;
          }
          catch (final IOException exception)
          {
            display.syncExec(new Runnable()
            {
              public void run()
              {
                Dialogs.close(dialog,false);
                Dialogs.error(shell,BARControl.tr("Cannot stop pairing master:\n\n{0}",exception.getMessage()));
              }
            });
            return;
          }

          // close dialog
          Dialogs.close(dialog,false);
        }
      });
    }

    // install handlers

    // new master runnable
    pairMasterRunnable[0] = new BackgroundRunnable(dialog,widgetProgressBar)
    {
      private final boolean abortFlag[] = {false};

      public void run(final Shell dialog, final ProgressBar widgetProgressBar)
      {
        try
        {
          final long restTime[]  = {0};
          final long totalTime[] = {0};

          do
          {
            BARServer.executeCommand(StringParser.format("MASTER_PAIRING_STATUS"),
                                     1,  // debugLevel
                                     new Command.Handler()
                                     {
                                       @Override
                                       public void handle(int errorCode, String errorData, ValueMap valueMap)
                                       {
                                         data.masterName = valueMap.getString("name");

                                         restTime[0]  = valueMap.getInt("restTime" );
                                         totalTime[0] = valueMap.getInt("totalTime");

                                         display.syncExec(new Runnable()
                                         {
                                           public void run()
                                           {
                                             if (!widgetProgressBar.isDisposed())
                                             {
                                               widgetProgressBar.setRange(0,totalTime[0]);
                                               widgetProgressBar.setSelection("%.0fs",restTime[0]);

                                               widgetMasterName.setText(data.masterName);

                                               if (!data.masterName.isEmpty())
                                               {
                                                 widgetOKButton.setEnabled(true);
                                               }
                                             }
                                             else
                                             {
                                               abort();
                                             }
                                           }
                                         });
                                       }
                                     }
                                    );

            // sleep a short time
            if (data.masterName.isEmpty() && (restTime[0] > 0) && !abortFlag[0])
            {
              try { Thread.sleep(1000); } catch (Throwable throwable) { /* ignored */ }
            }
          }
          while (data.masterName.isEmpty() && (restTime[0] > 0) && !abortFlag[0]);
        }
        catch (final BARException exception)
        {
          printStackTrace(exception);
          return;
        }
        catch (final IOException exception)
        {
          printStackTrace(exception);
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

      public void abort()
      {
        abortFlag[0] = true;
      }
    };

    // start pairing
    try
    {
      BARServer.executeCommand(StringParser.format("MASTER_PAIRING_START"),
                               1  // debugLevel
                              );
    }
    catch (final BARException exception)
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          Dialogs.error(shell,BARControl.tr("Cannot start pairing master:\n\n{0}",exception.getMessage()));
        }
      });
      return false;
    }
    catch (final IOException exception)
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          Dialogs.error(shell,BARControl.tr("Cannot start pairing master:\n\n{0}",exception.getMessage()));
        }
      });
      return false;
    }
    Background.run(pairMasterRunnable[0]);

    // run dialog
    Boolean result = (Boolean)Dialogs.run(dialog);

    updateMaster();

    return (result != null) ? result : false;
  }

  /** clear paired master
   * @return true iff cleared, false otherwise
   */
  private boolean clearMaster()
  {
    try
    {
      BARServer.executeCommand(StringParser.format("MASTER_CLEAR"),
                               0  // debugLevel
                              );
      updateMaster();

      return true;
    }
    catch (Exception exception)
    {
      Dialogs.error(shell,BARControl.tr("Cannot clear master:\n\n{0}",exception.getMessage()));
      return false;
    }
  }

  /**_login on server
   * @param loginData login data
   * @param loginDialogFlag true to show login dialog
   * return true iff login done
   */
  private boolean login(LoginData loginData, boolean loginDialogFlag)
  {
    boolean retryFlag;
    boolean connectedFlag                 = false;
    String  connectErrorMessage           = null;
    String  connectErrorExtendedMessage[] = null;

    if (loginDialogFlag)
    {
      if (!getLoginData(loginData,true))
      {
        return false;
      }
    }

    do
    {
      retryFlag = true;

      if (!connectedFlag)
      {
        // try to connect to server with default settings
        try
        {
          connect(loginData.name,
                  loginData.port,
                  loginData.tlsPort,
                  Settings.serverCAFileName,
                  Settings.serverKeystoreFileName,
                  loginData.tlsMode,
                  Settings.serverInsecureTLS,
                  loginData.password

                 );
          connectedFlag = true;
        }
        catch (ConnectionError error)
        {
          connectErrorMessage         = error.getMessage();
          connectErrorExtendedMessage = error.getExtendedMessage();
        }
        catch (CommunicationError error)
        {
          connectErrorMessage         = error.getMessage();
          connectErrorExtendedMessage = error.getExtendedMessage();
        }
      }

      if (!connectedFlag && (loginData.tlsMode == BARServer.TLSModes.FORCE))
      {
        if (loginData.tlsMode == BARServer.TLSModes.FORCE)
        {
          if (!Settings.serverInsecureTLS)
          {
            // try again with insecure TLS/without TLS
            switch (Dialogs.select(new Shell(),
                                   BARControl.tr("Connection fail"),
                                   IMAGE_ERROR,
                                   BARControl.tr("Certificate:"),
                                   connectErrorExtendedMessage,
                                   (connectErrorMessage != null)
                                     ? connectErrorMessage
                                     : BARControl.tr("Connection fail"),
                                   new String[]
                                   {
                                     BARControl.tr("Try with insecure TLS (SSL)"),
                                     BARControl.tr("Try without TLS (SSL)"),
                                     BARControl.tr("Cancel")
                                   },
                                   0
                                  )
                   )
            {
              case 0:
                // try to connect to server with insecure TLS/SSL
                connectErrorMessage         = null;
                connectErrorExtendedMessage = null;
                try
                {
                  BARServer.connect(loginData.name,
                                    loginData.port,
                                    loginData.tlsPort,
                                    (String)null,  // caFileName
                                    (String)null,  // keyFileName
                                    loginData.tlsMode,
                                    true, // insecureTLS,
                                    loginData.password
                                   );
                  connectedFlag = true;
                }
                catch (ConnectionError error)
                {
                  connectErrorMessage         = error.getMessage();
                  connectErrorExtendedMessage = error.getExtendedMessage();
                }
                catch (CommunicationError error)
                {
                  connectErrorMessage         = error.getMessage();
                  connectErrorExtendedMessage = error.getExtendedMessage();
                }
                break;
              case 1:
                // try to connect to server without TLS/SSL
                connectErrorMessage         = null;
                connectErrorExtendedMessage = null;
                try
                {
                  BARServer.connect(loginData.name,
                                    loginData.port,
                                    loginData.tlsPort,
                                    (String)null,  // caFileName
                                    (String)null,  // keyFileName
                                    BARServer.TLSModes.NONE,
                                    false, // insecureTLS,
                                    loginData.password
                                   );
                  connectedFlag = true;
                }
                catch (ConnectionError error)
                {
                  connectErrorMessage = error.getMessage();
                }
                catch (CommunicationError error)
                {
                  connectErrorMessage = error.getMessage();
                }
                break;
              case 2:
                retryFlag = false;
                break;
            }
          }
          else
          {
            // try again without TLS
            switch (Dialogs.select(new Shell(),
                                   BARControl.tr("Connection fail"),
                                   IMAGE_ERROR,
                                   (connectErrorMessage != null)
                                     ? connectErrorMessage
                                     : BARControl.tr("Connection fail"),
                                   new String[]
                                   {
                                     BARControl.tr("Try without TLS (SSL)"),
                                     BARControl.tr("Cancel")
                                   },
                                   0
                                  )
                   )
            {
              case 0:
                // try to connect to server without TLS/SSL
                connectErrorMessage         = null;
                connectErrorExtendedMessage = null;
                try
                {
                  BARServer.connect(loginData.name,
                                    loginData.port,
                                    loginData.tlsPort,
                                    (String)null,  // caFileName
                                    (String)null,  // keyFileName
                                    BARServer.TLSModes.NONE,
                                    false, // insecureTLS,
                                    loginData.password
                                   );
                  connectedFlag = true;
                }
                catch (ConnectionError error)
                {
                  connectErrorMessage = error.getMessage();
                }
                catch (CommunicationError error)
                {
                  connectErrorMessage = error.getMessage();
                }
                break;
              case 1:
                retryFlag = false;
                break;
            }
          }
        }
      }

      if (!connectedFlag && retryFlag)
      {
        // show error dialog
        Dialogs.error(new Shell(),
        connectErrorExtendedMessage,
                      (connectErrorMessage != null)
                        ? connectErrorMessage
                        : BARControl.tr("Connection fail")
                     );

        // get login data
        if (!getLoginData(loginData,true))
        {
          break;
        }
        if ((loginData.port == 0) && (loginData.tlsPort == 0))
        {
          throw new Error(BARControl.tr("Cannot connect to server. No server ports specified!"));
        }
      }
    }
    while (!connectedFlag && retryFlag);

    // check if connected
    if (connectedFlag)
    {
      // show warning if no TLS connection established
      if ((loginData.tlsMode == BARServer.TLSModes.TRY) && !BARServer.isTLSConnection())
      {
        Dialogs.warning(new Shell(),
                        Dialogs.booleanFieldUpdater(Settings.class,"showNoneTLSWarning"),
                        BARControl.tr("Established a none-TLS connection only.\nTransmitted data may be vulnerable!")
                       );
      }

      return true;
    }
    else
    {
      return false;
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
    i18n = I18nFactory.getI18n(getClass(),
                               "app.i18n.Messages",
                               Locale.getDefault(),
                               I18nFactory.FALLBACK
                              );
    Dialogs.init(i18n);
    BusyDialog.init(i18n);

    try
    {
      // parse arguments (initial)
      parseArguments(args);

      // load settings
      Settings.load();

      // parse arguments (final to overwrite loaded setting values)
      parseArguments(args);

      // server login data
      Settings.Server server = null;
      if      (Settings.serverName != null)
      {
        server = Settings.getLastServer().clone();
        server.name = Settings.serverName;
        if (Settings.serverPort != -1) server.port = Settings.serverPort;
        Settings.addServer(server);
      }
      else if (Settings.serverPort != -1)
      {
        server = Settings.getLastServer().clone();
        server.port = Settings.serverPort;
        Settings.addServer(server);
      }
      else
      {
        server = Settings.getLastServer();
      }
      BARServer.TLSModes tlsMode = BARServer.TLSModes.TRY;
      if (Settings.serverForceTLS) tlsMode = BARServer.TLSModes.FORCE;
      if (Settings.serverNoTLS)    tlsMode = BARServer.TLSModes.NONE;
      loginData = new LoginData((server != null) ? server.name     : Settings.DEFAULT_SERVER_NAME,
                                (server != null) ? server.port     : Settings.DEFAULT_SERVER_PORT,
                                (server != null) ? server.port     : Settings.DEFAULT_SERVER_TLS_PORT,
                                tlsMode,
                                (server != null) ? server.password : "",
                                Settings.role
                               );
      // support deprecated server settings
      if (Settings.serverName     != null) loginData.name     = Settings.serverName;
      if (Settings.serverPort     != -1  ) loginData.port     = Settings.serverPort;
      if (Settings.serverTLSPort  != -1  ) loginData.tlsPort  = Settings.serverTLSPort;
      if (Settings.serverPassword != null) loginData.password = Settings.serverPassword;

      // commands
      if (   Settings.pairMasterFlag
          || !Settings.runJobNames.isEmpty()
          || (Settings.abortJobName != null)
          || (Settings.pauseTime > 0)
          || (Settings.maintenanceTime > 0)
          || Settings.pingFlag
          || Settings.suspendFlag
          || Settings.continueFlag
          || Settings.listJobsFlag
          || Settings.listSchedulesFlag
          || (Settings.infoJobName != null)
          || Settings.indexDatabaseInfo
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
          connect(loginData.name,
                  loginData.port,
                  loginData.tlsPort,
                  Settings.serverCAFileName,
                  Settings.serverKeystoreFileName,
                  loginData.tlsMode,
                  Settings.serverInsecureTLS,
                  loginData.password
                 );
        }
        catch (ConnectionError error)
        {
          printError(BARControl.tr("cannot connect to server (error: {0})",error.getMessage()));
          System.exit(ExitCodes.FAIL);
        }

        // show warning if no TLS connection established
        if ((loginData.tlsMode == BARServer.TLSModes.TRY) && !BARServer.isTLSConnection())
        {
          printWarning(BARControl.tr("Established a none-TLS connection only. Transmitted data may be vulnerable!"));
        }

        // execute commands
        if (Settings.pairMasterFlag)
        {
          System.out.print(BARControl.tr("Wait for pairing new master")+"...    ");

          // set new master
          try
          {
            final String masterName[] = new String[]{""};
            final long restTime[]     = {0};
            final long totalTime[]    = {0};

            BARServer.executeCommand(StringParser.format("MASTER_PAIRING_START"),
                                     1  // debugLevel
                                    );

            do
            {
              BARServer.executeCommand(StringParser.format("MASTER_PAIRING_STATUS"),
                                       1,  // debugLevel
                                       new Command.Handler()
                                       {
                                         @Override
                                         public void handle(int errorCode, String errorData, ValueMap valueMap)
                                         {
                                           masterName[0] = valueMap.getString("name");
                                           restTime[0]   = valueMap.getInt("restTime" );
                                           totalTime[0]  = valueMap.getInt("totalTime");

                                           System.out.print(String.format("\b\b\b\b%3ds",restTime));
                                         }
                                       }
                                      );

              // sleep a short time
              if (masterName[0].isEmpty() && (restTime[0] > 0))
              {
                try { Thread.sleep(1000); } catch (Throwable throwable) { /* ignored */ }
              }
            }
            while (masterName[0].isEmpty() && (restTime[0] > 0));
            System.out.print("\b\b\b\b");

            BARServer.executeCommand(StringParser.format("MASTER_PAIRING_STOP pair=%y",!masterName[0].isEmpty()),
                                     1  // debugLevel
                                    );

            if (!masterName[0].isEmpty())
            {
              System.out.println(String.format(BARControl.tr("''{0}'' - OK",masterName[0])));
            }
            else
            {
              System.out.println("FAIL!");
            }
          }
          catch (final BARException exception)
          {
            printError(BARControl.tr("cannot set new master ({0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot set new master ({0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        for (String runJobName : Settings.runJobNames)
        {
          // get job UUID
          String jobUUID = getJobUUID(runJobName);
          if (jobUUID == null)
          {
            printError(BARControl.tr("job ''{0}'' not found",runJobName));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot start job ''{0}'' (error: {1})",runJobName,exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot pause (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.maintenanceTime > 0)
        {
          // maintenance
          try
          {
            BARServer.executeCommand(StringParser.format("MAINTENANCE time=%d",
                                                         Settings.maintenanceTime
                                                        ),
                                     1  // debug level
                                    );
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot set maintenance (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot suspend (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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
          catch (Exception exception)
          {
            printError("cannot continue (error: %s)",exception.getMessage());
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.abortJobName != null)
        {
          // get job id
          String jobUUID = getJobUUID(Settings.abortJobName);
          if (jobUUID == null)
          {
            printError(BARControl.tr("job ''{0}'' not found",Settings.abortJobName));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot abort job ''{0}'' (error: {1})",Settings.abortJobName,exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.listJobsFlag)
        {
          // get server state
          final BARServer.States serverState[] = {BARServer.States.RUNNING};
          try
          {
            ValueMap valueMap = new ValueMap();
            BARServer.executeCommand(StringParser.format("STATUS"),
                                     1,  // debug level
                                     valueMap
                                    );
            serverState[0] = valueMap.getEnum("state",BARServer.States.class,BARServer.States.RUNNING);
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot get state (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }

          // get joblist
          final ArrayList<JobInfo> jobInfoList = new ArrayList<JobInfo>();
          try
          {
            BARServer.executeCommand(StringParser.format("JOB_LIST"),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         // get data
                                         String              jobUUID                = valueMap.getString("jobUUID"                             );
                                         String              name                   = valueMap.getString("name"                                );
                                         JobData.States      state                  = valueMap.getEnum  ("state",JobData.States.class          );
                                         String              slaveHostName          = valueMap.getString("slaveHostName",""                    );
                                         JobData.SlaveStates slaveState             = valueMap.getEnum  ("slaveState",JobData.SlaveStates.class);
                                         String              archiveType            = valueMap.getString("archiveType"                         );
                                         long                archivePartSize        = valueMap.getLong  ("archivePartSize"                     );
// TODO: enum
                                         String              deltaCompressAlgorithm = valueMap.getString("deltaCompressAlgorithm"              );
// TODO: enum
                                         String              byteCompressAlgorithm  = valueMap.getString("byteCompressAlgorithm"               );
// TODO: enum
                                         String              cryptAlgorithm         = valueMap.getString("cryptAlgorithm"                      );
// TODO: enum
                                         String              cryptType              = valueMap.getString("cryptType"                           );
// TODO: enum
                                         String              cryptPasswordMode      = valueMap.getString("cryptPasswordMode"                   );
                                         long                lastExecutedDateTime   = valueMap.getLong  ("lastExecutedDateTime"                );
                                         long                estimatedRestTime      = valueMap.getLong  ("estimatedRestTime"                   );

                                         jobInfoList.add(new JobInfo(jobUUID,
                                                                     name,
                                                                     state,
                                                                     slaveHostName,
                                                                     slaveState,
                                                                     archiveType,
                                                                     archivePartSize,
                                                                     deltaCompressAlgorithm,
                                                                     byteCompressAlgorithm,
                                                                     cryptAlgorithm,
                                                                     cryptType,
                                                                     cryptPasswordMode,
                                                                     lastExecutedDateTime,
                                                                     estimatedRestTime
                                                                    )
                                                        );
                                       }
                                     }
                                    );

            Collections.sort(jobInfoList,new Comparator<JobInfo>()
            {
              @Override
              public int compare(JobInfo jobInfo1, JobInfo jobInfo2)
              {
                return jobInfo1.name.compareTo(jobInfo2.name);
              }
            });

            System.out.println(String.format("%-32s %-18s %-20s %-12s %-14s %-25s %-15s %-10s %-8s %-19s %-13s",
                                             BARControl.tr("Name"),
                                             BARControl.tr("State"),
                                             BARControl.tr("Host name"),
                                             BARControl.tr("Type"),
                                             BARControl.tr("Part size"),
                                             BARControl.tr("Compress"),
                                             BARControl.tr("Crypt"),
                                             BARControl.tr("Crypt type"),
                                             BARControl.tr("Mode"),
                                             BARControl.tr("Last executed"),
                                             BARControl.tr("Estimated [s]")
                                            )
                              );

            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            }
            for (JobInfo jobInfo : jobInfoList)
            {
              String compressAlgorithms;
              if      (!jobInfo.deltaCompressAlgorithm.equalsIgnoreCase("none") && !jobInfo.byteCompressAlgorithm.equalsIgnoreCase("none")) compressAlgorithms = jobInfo.deltaCompressAlgorithm+"+"+jobInfo.byteCompressAlgorithm;
              else if (!jobInfo.deltaCompressAlgorithm.equalsIgnoreCase("none")                                                           ) compressAlgorithms = jobInfo.deltaCompressAlgorithm;
              else if (                                                            !jobInfo.byteCompressAlgorithm.equalsIgnoreCase("none")) compressAlgorithms = jobInfo.byteCompressAlgorithm;
              else                                                                                                                          compressAlgorithms = "-";
              if (jobInfo.cryptAlgorithm.equalsIgnoreCase("none"))
              {
                jobInfo.cryptAlgorithm    = "-";
                jobInfo.cryptType         = "-";
                jobInfo.cryptPasswordMode = "-";
              }

              System.out.println(String.format("%-32s %-18s %-20s %-12s %14d %-25s %-15s %-10s %-8s %-19s %13d",
                                              jobInfo.name,
                                              (serverState[0] == BARServer.States.RUNNING)
                                                ? JobData.formatStateText(jobInfo.state,jobInfo.slaveHostName,jobInfo.slaveState)
                                                : BARControl.tr("suspended"),
                                              jobInfo.slaveHostName,
                                              jobInfo.archiveType.toString(),
                                              jobInfo.archivePartSize,
                                              compressAlgorithms,
                                              jobInfo.cryptAlgorithm,
                                              jobInfo.cryptType,
                                              jobInfo.cryptPasswordMode,
                                              (jobInfo.lastExecutedDateTime > 0) ? DATE_FORMAT.format(new Date(jobInfo.lastExecutedDateTime*1000)) : "",
                                              jobInfo.estimatedRestTime
                                             )
                               );
            }
            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
              System.out.println(BARControl.tr("{0} {0,choice,0#jobs|1#job|1<jobs}",jobInfoList.size()));
            }
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot get job list (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.listSchedulesFlag)
        {
          final ArrayList<ScheduleInfo> nextScheduleList = new ArrayList<ScheduleInfo>();
          try
          {
            BARServer.executeCommand(StringParser.format("SCHEDULE_LIST"),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         // get data
                                         String       jobName              = valueMap.getString ("jobName"                       );
                                         String       jobUUID              = valueMap.getString ("jobUUID"                       );
                                         String       scheduleUUID         = valueMap.getString ("scheduleUUID"                  );
                                         String       date                 = valueMap.getString ("date"                          );
                                         String       weekDays             = valueMap.getString ("weekDays"                      );
                                         String       time                 = valueMap.getString ("time"                          );
                                         ArchiveTypes archiveType          = valueMap.getEnum   ("archiveType",ArchiveTypes.class);
                                         int          interval             = valueMap.getInt    ("interval"                      );
                                         String       customText           = valueMap.getString ("customText"                    );
                                         String       beginTime            = valueMap.getString ("beginTime"                     );
                                         String       endTime              = valueMap.getString ("endTime"                       );
                                         boolean      testCreatedArchives  = valueMap.getBoolean("testCreatedArchives"           );
                                         boolean      noStorage            = valueMap.getBoolean("noStorage"                     );
                                         boolean      enabled              = valueMap.getBoolean("enabled"                       );
                                         long         lastExecutedDateTime = valueMap.getLong   ("lastExecutedDateTime"          );
                                         long         nextExecutedDateTime = valueMap.getLong   ("nextExecutedDateTime"          );
                                         long         totalEntities        = valueMap.getLong   ("totalEntities"                 );
                                         long         totalEntryCount      = valueMap.getLong   ("totalEntryCount"               );
                                         long         totalEntrySize       = valueMap.getLong   ("totalEntrySize",               Long.MAX_VALUE);

                                         nextScheduleList.add(new ScheduleInfo(jobName,
                                                                               scheduleUUID,
                                                                               date,
                                                                               weekDays,
                                                                               time,
                                                                               archiveType,
                                                                               interval,
                                                                               beginTime,
                                                                               endTime,
                                                                               customText,
                                                                               testCreatedArchives,
                                                                               noStorage,
                                                                               enabled,
                                                                               lastExecutedDateTime,
                                                                               nextExecutedDateTime,
                                                                               totalEntities,
                                                                               totalEntryCount,
                                                                               totalEntrySize
                                                                              )
                                                             );
                                       }
                                     }
                                    );

            Collections.sort(nextScheduleList,new Comparator<ScheduleInfo>()
            {
              @Override
              public int compare(ScheduleInfo scheduleInfo1, ScheduleInfo scheduleInfo2)
              {
                int result = 0;

                if (result == 0)
                {
                  result = scheduleInfo1.jobName.compareTo(scheduleInfo2.jobName);
                }
                if (result == 0)
                {
                  if      (scheduleInfo1.lastExecutedDateTime < scheduleInfo2.lastExecutedDateTime) result = -1;
                  else if (scheduleInfo1.lastExecutedDateTime > scheduleInfo2.lastExecutedDateTime) result =  1;
                }

                return result;
              }
            });

            System.out.println(String.format("%-20s %-36s %-10s %-23s %-5s %-11s %-8s %-15s %-5s %-5s %-3s %-3s %-3s %-19s %-19s",
                                             BARControl.tr("Job"),
                                             BARControl.tr("UUID"),
                                             BARControl.tr("Date"),
                                             BARControl.tr("Week days"),
                                             BARControl.tr("Time"),
                                             BARControl.tr("Type"),
                                             BARControl.tr("Interval"),
                                             BARControl.tr("Text"),
                                             BARControl.tr("Begin"),
                                             BARControl.tr("End"),
                                             BARControl.tr("T"),
                                             BARControl.tr("S"),
                                             BARControl.tr("E"),
                                             BARControl.tr("Last"),
                                             BARControl.tr("Next")
                                            )
                              );

            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            }
            for (ScheduleInfo scheduleInfo : nextScheduleList)
            {
              System.out.println(String.format("%-20s %-36s %-10s %-23s %-5s %-11s %8d %-15s %-5s %-5s %-3s %-3s %-3s %-19s %-19s",
                                               scheduleInfo.jobName,
                                               scheduleInfo.uuid,
                                               scheduleInfo.getDate(),
                                               scheduleInfo.getWeekDays(),
                                               scheduleInfo.getTime(),
                                               scheduleInfo.getArchiveType(),
                                               scheduleInfo.interval,
                                               scheduleInfo.customText,
                                               scheduleInfo.getBeginTime(),
                                               scheduleInfo.getEndTime(),
                                               scheduleInfo.testCreatedArchives ? "yes" : "no",
                                               scheduleInfo.noStorage ? "yes" : "no",
                                               scheduleInfo.enabled ? "yes" : "no",
                                               (scheduleInfo.lastExecutedDateTime > 0) ? DATE_FORMAT.format(new Date(scheduleInfo.lastExecutedDateTime*1000)) : "-",
                                               (scheduleInfo.nextExecutedDateTime > 0) ? DATE_FORMAT.format(new Date(scheduleInfo.nextExecutedDateTime*1000)) : "-"
                                              )
                               );
            }
            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
              System.out.println(BARControl.tr("{0} {0,choice,0#jobs|1#schedule|1<schedules}",nextScheduleList.size()));
            }
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot get schedule list (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.infoJobName != null)
        {
          // get job UUID
          String jobUUID = getJobUUID(Settings.infoJobName);
          if (jobUUID == null)
          {
            printError(BARControl.tr("job ''{0}'' not found",Settings.infoJobName));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }

          // get job info
          try
          {
            BARServer.executeCommand(StringParser.format("JOB_STATUS jobUUID=%s",
                                                         jobUUID,
                                                         Settings.archiveType.toString()
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                       {
                                         int    errorCode = valueMap.getInt   ("errorCode");
                                         String errorData = valueMap.getString("errorData");

                                         System.out.println(String.format("Job '%s':",Settings.infoJobName));
                                         System.out.println(String.format("  Total entries      : %d / %s",valueMap.getInt("totalEntryCount"),Units.formatByteSize(valueMap.getLong("totalEntrySize"))));
                                         System.out.println(String.format("  Skipped entries    : %d / %s",valueMap.getInt("skippedEntryCount"),Units.formatByteSize(valueMap.getLong("skippedEntrySize"))));
                                         System.out.println(String.format("  Error entries      : %d / %s",valueMap.getInt("errorEntryCount"),Units.formatByteSize(valueMap.getLong("errorEntrySize"))));
                                         System.out.println(String.format("  Compression ratio  : %.1f%%",valueMap.getDouble("compressionRatio")));
                                         System.out.println(String.format("  Entries/s          : %.1f",valueMap.getDouble("entriesPerSecond")));
                                         System.out.println(String.format("  Bytes/s            : %.1f",valueMap.getDouble("bytesPerSecond")));
                                         System.out.println(String.format("  Storage done       : %s",Units.formatByteSize(valueMap.getLong("storageTotalSize"))));
                                         System.out.println(String.format("          total      : %s",Units.formatByteSize(valueMap.getLong("storageTotalSize"))));
                                         System.out.println(String.format("          bytes/s    : %.1f",valueMap.getDouble("storageBytesPerSecond")));
                                         System.out.println(String.format("  Estimated rest time: %ds",valueMap.getLong("estimatedRestTime")));
                                         System.out.println(String.format("  Last error         : %s",(errorCode != 0) ? BARException.getText(errorCode,0,errorData) : "none"));
                                       }
                                     }
                                    );
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot get job info ''{0}'' (error: {1})",Settings.infoJobName,exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.indexDatabaseInfo)
        {
          // add index for storage
          try
          {
            BARServer.executeCommand(StringParser.format("INDEX_INFO"),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         long   totalNormalEntityCount           = valueMap.getLong  ("totalNormalEntityCount"          );
                                         long   totalFullEntityCount             = valueMap.getLong  ("totalFullEntityCount"            );
                                         long   totalIncrementalEntityCount      = valueMap.getLong  ("totalIncrementalEntityCount"     );
                                         long   totalDifferentialEntityCount     = valueMap.getLong  ("totalDifferentialEntityCount"    );
                                         long   totalContinuousEntityCount       = valueMap.getLong  ("totalContinuousEntityCount"      );
                                         long   totalLockedEntityCount           = valueMap.getLong  ("totalLockedEntityCount"          );
                                         long   totalDeletedEntityCount          = valueMap.getLong  ("totalDeletedEntityCount"         );

                                         long   totalEntryCount                  = valueMap.getLong  ("totalEntryCount"                 );
                                         long   totalEntrySize                   = valueMap.getLong  ("totalEntrySize",                 Long.MAX_VALUE);
                                         long   totalFileCount                   = valueMap.getLong  ("totalFileCount"                  );
                                         long   totalFileSize                    = valueMap.getLong  ("totalFileSize",                  Long.MAX_VALUE);
                                         long   totalImageCount                  = valueMap.getLong  ("totalImageCount"                 );
                                         long   totalImageSize                   = valueMap.getLong  ("totalImageSize",                 Long.MAX_VALUE);
                                         long   totalDirectoryCount              = valueMap.getLong  ("totalDirectoryCount"             );
                                         long   totalLinkCount                   = valueMap.getLong  ("totalLinkCount"                  );
                                         long   totalHardlinkCount               = valueMap.getLong  ("totalHardlinkCount"              );
                                         long   totalHardlinkSize                = valueMap.getLong  ("totalHardlinkSize",              Long.MAX_VALUE);
                                         long   totalSpecialCount                = valueMap.getLong  ("totalSpecialCount"               );

                                         long   totalEntryCountNewest            = valueMap.getLong  ("totalEntryCountNewest"           );
                                         long   totalEntrySizeNewest             = valueMap.getLong  ("totalEntrySizeNewest",           Long.MAX_VALUE);
                                         long   totalFileCountNewest             = valueMap.getLong  ("totalFileCountNewest"            );
                                         long   totalFileSizeNewest              = valueMap.getLong  ("totalFileSizeNewest",            Long.MAX_VALUE);
                                         long   totalImageCountNewest            = valueMap.getLong  ("totalImageCountNewest"           );
                                         long   totalImageSizeNewest             = valueMap.getLong  ("totalImageSizeNewest",           Long.MAX_VALUE);
                                         long   totalDirectoryCountNewest        = valueMap.getLong  ("totalDirectoryCountNewest"       );
                                         long   totalLinkCountNewest             = valueMap.getLong  ("totalLinkCountNewest"            );
                                         long   totalHardlinkCountNewest         = valueMap.getLong  ("totalHardlinkCountNewest"        );
                                         long   totalHardlinkSizeNewest          = valueMap.getLong  ("totalHardlinkSizeNewest",        Long.MAX_VALUE);
                                         long   totalSpecialCountNewest          = valueMap.getLong  ("totalSpecialCountNewest"         );

                                         long   totalSkippedEntryCount           = valueMap.getLong  ("totalSkippedEntryCount"          );

                                         long   totalStorageCount                = valueMap.getLong  ("totalStorageCount"               );
                                         long   totalStorageSize                 = valueMap.getLong  ("totalStorageSize",               Long.MAX_VALUE);
                                         long   totalOKStorageCount              = valueMap.getLong  ("totalOKStorageCount"             );
                                         long   totalUpdateRequestedStorageCount = valueMap.getLong  ("totalUpdateRequestedStorageCount");
                                         long   totalErrorStorageCount           = valueMap.getLong  ("totalErrorStorageCount"          );
                                         long   totalDeletedStorageCount         = valueMap.getLong  ("totalDeletedStorageCount"        );

                                         System.out.println(String.format("Entities: %d", totalNormalEntityCount
                                                                                         +totalFullEntityCount
                                                                                         +totalIncrementalEntityCount
                                                                                         +totalDifferentialEntityCount
                                                                                         +totalContinuousEntityCount                                                                                            ));
                                         System.out.println(String.format("  Normal          : %d",totalNormalEntityCount                                                                                       ));
                                         System.out.println(String.format("  Full            : %d",totalFullEntityCount                                                                                         ));
                                         System.out.println(String.format("  Incremental     : %d",totalIncrementalEntityCount                                                                                  ));
                                         System.out.println(String.format("  Differential    : %d",totalDifferentialEntityCount                                                                                 ));
                                         System.out.println(String.format("  Continuous      : %d",totalContinuousEntityCount                                                                                   ));
                                         System.out.println(String.format("  Locked          : %d",totalLockedEntityCount                                                                                       ));
                                         System.out.println(String.format("  Deleted         : %d",totalDeletedEntityCount                                                                                      ));
                                         System.out.println(String.format("Storages: %d,  %s (%d bytes)",totalStorageCount,Units.formatByteSize(totalStorageSize),totalStorageSize                              ));
                                         System.out.println(String.format("  OK              : %d",totalOKStorageCount                                                                             ));
                                         System.out.println(String.format("  Update requested: %d",totalUpdateRequestedStorageCount                                                                             ));
                                         System.out.println(String.format("  Error           : %d",totalErrorStorageCount                                                                                       ));
                                         System.out.println(String.format("  Deleted         : %d",totalDeletedStorageCount                                                                                     ));
                                         System.out.println(String.format("Entries: %d, %s (%d bytes)",totalEntryCount,Units.formatByteSize(totalEntrySize),totalEntrySize                                      ));
                                         System.out.println(String.format("  Files           : %d, %s (%d bytes)",totalFileCount,Units.formatByteSize(totalFileSize),totalFileSize                              ));
                                         System.out.println(String.format("  Images          : %d, %s (%d bytes)",totalImageCount,Units.formatByteSize(totalImageSize),totalImageSize                           ));
                                         System.out.println(String.format("  Directories     : %d",totalDirectoryCount                                                                                          ));
                                         System.out.println(String.format("  Links           : %d",totalLinkCount                                                                                               ));
                                         System.out.println(String.format("  Hardlinks       : %d, %s (%d bytes)",totalHardlinkCount,Units.formatByteSize(totalHardlinkSize),totalHardlinkSize                  ));
                                         System.out.println(String.format("  Special         : %d",totalSpecialCount                                                                                            ));
                                         System.out.println(String.format("Newest entries: %d, %s (%d bytes)",totalEntryCountNewest,Units.formatByteSize(totalEntrySizeNewest),totalEntrySizeNewest             ));
                                         System.out.println(String.format("  Files           : %d, %s (%d bytes)",totalFileCountNewest,Units.formatByteSize(totalFileSizeNewest),totalFileSizeNewest            ));
                                         System.out.println(String.format("  Images          : %d, %s (%d bytes)",totalImageCountNewest,Units.formatByteSize(totalImageSizeNewest),totalImageSizeNewest         ));
                                         System.out.println(String.format("  Directories     : %d",totalDirectoryCountNewest                                                                                    ));
                                         System.out.println(String.format("  Links           : %d",totalLinkCountNewest                                                                                         ));
                                         System.out.println(String.format("  Hardlinks       : %d, %s (%d bytes)",totalHardlinkCountNewest,Units.formatByteSize(totalHardlinkSizeNewest),totalHardlinkSizeNewest));
                                         System.out.println(String.format("  Special         : %d",totalSpecialCountNewest                                                                                      ));
                                         System.out.println(String.format("Skipped: %d",totalSkippedEntryCount                                                                                                  ));
                                       }
                                     }
                                    );
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot get index info (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.indexDatabaseAddStorageName != null)
        {
          // add index for storage
          try
          {
            BARServer.executeCommand(StringParser.format("INDEX_STORAGE_ADD name=%'S patternType=GLOB ProgressInfo_steps=1000",
                                                         Settings.indexDatabaseAddStorageName
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         long   storageId  = valueMap.getLong  ("storageId", 0L);
                                         String name       = valueMap.getString("name",      "");
                                         long   doneCount  = valueMap.getLong  ("doneCount", 0L);
                                         long   totalCount = valueMap.getLong  ("totalCount",0L);

                                         if      ((storageId != 0) && (!name.isEmpty()))
                                         {
                                           System.out.println(String.format("%s",name));
                                         }
                                       }
                                     }
                                    );
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot add ''{0}'' to index (error: {1})",Settings.indexDatabaseAddStorageName,exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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
          catch (Exception exception)
          {
            printError("cannot refresh index for storage '%s' from index (error: %s)",Settings.indexDatabaseRefreshStorageName,exception.getMessage());
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.indexDatabaseRemoveStorageName != null)
        {
          // remote index for storage
          try
          {
            BARServer.executeCommand(StringParser.format("INDEX_REMOVE name=%'S",
                                                         Settings.indexDatabaseRemoveStorageName
                                                        ),
                                     1  // debug level
                                    );
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot remove index for storage ''{0}'' from index (error: {1})",Settings.indexDatabaseRemoveStorageName,exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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

            System.out.println(String.format("%-8s %-8s %-12s %-14s %-14s %-14s %-19s %s",
                                             BARControl.tr("Entity id"),
                                             BARControl.tr("Job id"),
                                             BARControl.tr("Type"),
                                             BARControl.tr("Size"),
                                             BARControl.tr("Entry count"),
                                             BARControl.tr("Entry size"),
                                             BARControl.tr("Date/Time"),
                                             BARControl.tr("Job")
                                            )
                              );
            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            }
            BARServer.executeCommand(StringParser.format("INDEX_ENTITY_LIST indexStateSet=* indexModeSet=* name=%'S sortMode=JOB_UUID ordering=ASCENDING",
                                                         !Settings.indexDatabaseEntitiesListName.isEmpty() ? Settings.indexDatabaseEntitiesListName : ""
                                                        ),
0,//                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         long         uuid            = valueMap.getLong  ("uuid"            );
                                         String       jobUUID         = valueMap.getString("jobUUID"         );
                                         String       jobName         = valueMap.getString("jobName"         );
                                         long         entityId        = valueMap.getLong  ("entityId"        );
                                         ArchiveTypes archiveType     = valueMap.getEnum  ("archiveType",    ArchiveTypes.class);
                                         long         createdDateTime = valueMap.getLong  ("createdDateTime" );
                                         long         totalSize       = valueMap.getLong  ("totalSize",      0L);
                                         long         totalEntryCount = valueMap.getLong  ("totalEntryCount",0L);
                                         long         totalEntrySize  = valueMap.getLong  ("totalEntrySize", 0L);

                                         System.out.println(String.format("%8d %8d %-12s %-14s %14d %14d %-19s %s",
                                                                          getDatabaseId(entityId),
                                                                          getDatabaseId(uuid),
                                                                          archiveType.toString(),
                                                                          totalSize,
                                                                          totalEntryCount,
                                                                          totalEntrySize,
                                                                          (createdDateTime > 0L) ? DATE_FORMAT.format(new Date(createdDateTime*1000)) : "-",
                                                                          !jobName.isEmpty() ? jobName : jobUUID
                                                                         )
                                                           );
                                         n[0]++;
                                       }
                                     }
                                    );
            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
              System.out.println(BARControl.tr("{0} entities",n[0]));
            }
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot list entities index (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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

            System.out.println(String.format("%-8s %-8s %-14s %-19s %-16s %-6s %s",
                                             BARControl.tr("Storage id"),
                                             BARControl.tr("Entity"),
                                             BARControl.tr("Size"),
                                             BARControl.tr("Date/Time"),
                                             BARControl.tr("State"),
                                             BARControl.tr("Mode"),
                                             BARControl.tr("Name")
                                            )
                              );
            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            }
            BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=* indexStateSet=* indexModeSet=* name=%'S sortMode=NAME ordering=ASCENDING",
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
                                         long        entityId    = valueMap.getLong  ("entityId"                    );
                                         String      storageName = valueMap.getString("name"                        );
                                         long        dateTime    = valueMap.getLong  ("dateTime"                    );
                                         long        size        = valueMap.getLong  ("size"                        );
                                         IndexStates state       = valueMap.getEnum  ("indexState",IndexStates.class);
                                         IndexModes  mode        = valueMap.getEnum  ("indexMode",IndexModes.class  );

                                         System.out.println(String.format("%8d %8d %-19s %-16s %-6s %-6s %s",
                                                                          getDatabaseId(storageId),
                                                                          getDatabaseId(entityId),
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
            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
              System.out.println(BARControl.tr("{0} {0,choice,0#storages|1#storage|1<storages}",n[0]));
            }
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot list storages index (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.indexDatabaseEntriesListName != null)
        {
          // list storage index
          try
          {
            BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST_INFO entryType=* name=%'S newestOnly=%y  selectedOnly=no fragmentsCount=no",
                                                         Settings.indexDatabaseEntriesListName,
                                                         Settings.indexDatabaseEntriesNewestOnly
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                       }
                                     }
                                    );

            final ArrayList<EntryInfo> entryInfoList = new ArrayList<EntryInfo>();
            BARServer.executeCommand(StringParser.format("INDEX_ENTRY_LIST entryType=* name=%'S newestOnly=%y limit=1024",
                                                         Settings.indexDatabaseEntriesListName,
                                                         Settings.indexDatabaseEntriesNewestOnly
                                                        ),
                                     1,  // debug level
                                     new Command.ResultHandler()
                                     {
                                       @Override
                                       public void handle(int i, ValueMap valueMap)
                                         throws BARException
                                       {
                                         String     storageName = valueMap.getString("storageName"               );
                                         long       entryId     = valueMap.getLong  ("entryId",0L                );
                                         EntryTypes entryType   = valueMap.getEnum  ("entryType",EntryTypes.class);

                                         switch (entryType)
                                         {
                                           case FILE:
                                             {
                                               String fileName        = valueMap.getString("name"            );
                                               long   size            = valueMap.getLong  ("size",0L         );
                                               long   dateTime        = valueMap.getLong  ("dateTime",0L     );
                                               long   fragmentCount   = valueMap.getLong  ("fragmentCount",0L);

                                               entryInfoList.add(EntryInfo.newFile(entryId,
                                                                                   storageName,
                                                                                   fileName,
                                                                                   size,
                                                                                   dateTime,
                                                                                   fragmentCount
                                                                                  )
                                                                );
                                             }
                                             break;
                                           case IMAGE:
                                             {
                                               String imageName       = valueMap.getString("name"          );
                                               long   size            = valueMap.getLong  ("size",       0L);
                                               long   blockOffset     = valueMap.getLong  ("blockOffset",0L);
                                               long   blockCount      = valueMap.getLong  ("blockCount", 0L);

                                               entryInfoList.add(EntryInfo.newImage(entryId,
                                                                                    storageName,
                                                                                    imageName,
                                                                                    size,
                                                                                    blockOffset,
                                                                                    blockCount
                                                                                   )
                                                                );
                                             }
                                             break;
                                           case DIRECTORY:
                                             {
                                               String directoryName   = valueMap.getString("name"       );
                                               long   dateTime        = valueMap.getLong  ("dateTime",0L);

                                               entryInfoList.add(EntryInfo.newDirectory(entryId,
                                                                                        storageName,
                                                                                        directoryName,
                                                                                        dateTime
                                                                                       )
                                                                );
                                             }
                                             break;
                                           case LINK:
                                             {
                                               String linkName        = valueMap.getString("name"             );
                                               String destinationName = valueMap.getString("destinationName"  );
// TODO: datetime for link?
                                               long   dateTime        = valueMap.getLong  ("dateTime",      0L);

                                               entryInfoList.add(EntryInfo.newLink(entryId,
                                                                                   storageName,
                                                                                   linkName,
                                                                                   destinationName
                                                                                  )
                                                                );
                                             }
                                             break;
                                           case HARDLINK:
                                             {
                                               String fileName        = valueMap.getString("name"            );
                                               long   size            = valueMap.getLong  ("size",0L         );
                                               long   dateTime        = valueMap.getLong  ("dateTime",0L     );
                                               long   fragmentCount   = valueMap.getLong  ("fragmentCount",0L);

                                               entryInfoList.add(EntryInfo.newHardLink(entryId,
                                                                                       storageName,
                                                                                       fileName,
                                                                                       size,
                                                                                       dateTime,
                                                                                       fragmentCount
                                                                                      )
                                                                );
                                             }
                                             break;
                                           case SPECIAL:
                                             {
                                               String name     = valueMap.getString("name"       );
                                               long   dateTime = valueMap.getLong  ("dateTime",0L);

                                               entryInfoList.add(EntryInfo.newSpecial(entryId,
                                                                                      storageName,
                                                                                      name,
                                                                                      dateTime
                                                                                     )
                                                                );
                                             }
                                             break;
                                         }
                                       }
                                     }
                                    );


            int maxStorageNameWidth = 0;
            for (EntryInfo entryInfo : entryInfoList)
            {
              maxStorageNameWidth = Math.max(entryInfo.storageName.length(),maxStorageNameWidth);
            }

            if (!Settings.noHeaderFooterFlag)
            {
              if (Settings.longFormatFlag)
              {
                System.out.println(String.format("%-16s %-8s %-14s %-19s %-"+Integer.toString(maxStorageNameWidth)+"s %s",
                                                 BARControl.tr("Entry id"),
                                                 BARControl.tr("Type"),
                                                 BARControl.tr("Size"),
                                                 BARControl.tr("Date/Time"),
                                                 BARControl.tr("Storage"),
                                                 BARControl.tr("Name")
                                                )
                                  );
              }
              else
              {
                System.out.println(String.format("%-16s %-8s %-14s %-19s %s",
                                                 BARControl.tr("Entry id"),
                                                 BARControl.tr("Type"),
                                                 BARControl.tr("Size"),
                                                 BARControl.tr("Date/Time"),
                                                 BARControl.tr("Name")
                                                )
                                  );
              }

              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            }

            for (EntryInfo entryInfo : entryInfoList)
            {
              switch (entryInfo.entryType)
              {
               case FILE:
                 if (Settings.longFormatFlag)
                 {
                   System.out.println(String.format("%16d %-8s %14d %-19s %-"+Integer.toString(maxStorageNameWidth)+"s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "FILE",
                                                    entryInfo.file.size,
                                                    DATE_FORMAT.format(new Date(entryInfo.file.dateTime*1000)),
                                                    entryInfo.storageName,
                                                    entryInfo.file.name
                                                   )
                                     );
                 }
                 else
                 {
                   System.out.println(String.format("%16d %-8s %14d %-19s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "FILE",
                                                    entryInfo.file.size,
                                                    DATE_FORMAT.format(new Date(entryInfo.file.dateTime*1000)),
                                                    entryInfo.file.name
                                                   )
                                     );
                 }
                 break;
               case IMAGE:
                 if (Settings.longFormatFlag)
                 {
                   System.out.println(String.format("%8d %-8s %14d %-19s %-"+Integer.toString(maxStorageNameWidth)+"s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "IMAGE",
                                                    entryInfo.image.size,
                                                    "",
                                                    entryInfo.storageName,
                                                    entryInfo.image.name
                                                   )
                                     );
                 }
                 else
                 {
                   System.out.println(String.format("%8d %-8s %14d %-19s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "IMAGE",
                                                    entryInfo.image.size,
                                                    "",
                                                    entryInfo.image.name
                                                   )
                                     );
                 }
                 break;
               case DIRECTORY:
                 if (Settings.longFormatFlag)
                 {
                   System.out.println(String.format("%16d %-8s %14s %-19s %-"+Integer.toString(maxStorageNameWidth)+"s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "DIR",
                                                    "",
                                                    DATE_FORMAT.format(new Date(entryInfo.directory.dateTime*1000)),
                                                    entryInfo.storageName,
                                                    entryInfo.directory.name
                                                   )
                                     );
                 }
                 else
                 {
                   System.out.println(String.format("%16d %-8s %14s %-19s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "DIR",
                                                    "",
                                                    DATE_FORMAT.format(new Date(entryInfo.directory.dateTime*1000)),
                                                    entryInfo.directory.name
                                                   )
                                     );
                 }
                 break;
               case LINK:
                 if (Settings.longFormatFlag)
                 {
                   System.out.println(String.format("%16d %-8s %14s %-19s %-"+Integer.toString(maxStorageNameWidth)+"s %s -> %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "LINK",
                                                    "",
                                                    DATE_FORMAT.format(new Date(entryInfo.link.dateTime*1000)),
                                                    entryInfo.storageName,
                                                    entryInfo.link.name,
                                                    entryInfo.link.destinationName
                                                   )
                                     );
                 }
                 else
                 {
                   System.out.println(String.format("%16d %-8s %14s %-19s %s -> %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "LINK",
                                                    "",
                                                    DATE_FORMAT.format(new Date(entryInfo.link.dateTime*1000)),
                                                    entryInfo.link.name,
                                                    entryInfo.link.destinationName
                                                   )
                                     );
                 }
                 break;
               case HARDLINK:
                 if (Settings.longFormatFlag)
                 {
                   System.out.println(String.format("%16d %-8s %14d %-19s %-"+Integer.toString(maxStorageNameWidth)+"s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "HARDLINK",
                                                    entryInfo.hardLink.size,
                                                    DATE_FORMAT.format(new Date(entryInfo.hardLink.dateTime*1000)),
                                                    entryInfo.storageName,
                                                    entryInfo.hardLink.name
                                                   )
                                     );
                 }
                 else
                 {
                   System.out.println(String.format("%16d %-8s %14d %-19s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "HARDLINK",
                                                    entryInfo.hardLink.size,
                                                    DATE_FORMAT.format(new Date(entryInfo.hardLink.dateTime*1000)),
                                                    entryInfo.hardLink.name
                                                   )
                                     );
                 }
                 break;
               case SPECIAL:
                 if (Settings.longFormatFlag)
                 {
                   System.out.println(String.format("%16d %-8s %14s %-19s %-"+Integer.toString(maxStorageNameWidth)+"s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "SPECIAL",
                                                    "",
                                                    DATE_FORMAT.format(new Date(entryInfo.special.dateTime*1000)),
                                                    entryInfo.storageName,
                                                    entryInfo.special.name
                                                   )
                                     );
                 }
                 else
                 {
                   System.out.println(String.format("%16d %-8s %14s %-19s %s",
                                                    getDatabaseId(entryInfo.entryId),
                                                    "SPECIAL",
                                                    "",
                                                    DATE_FORMAT.format(new Date(entryInfo.special.dateTime*1000)),
                                                    entryInfo.special.name
                                                   )
                                     );
                 }
                 break;
               }
            }

            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
              System.out.println(BARControl.tr("{0} entries (max. 1024 shown)",entryInfoList.size()));
            }
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot list entries index (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.indexDatabaseHistoryList)
        {
          // list history
          try
          {
            final int n[] = new int[]{0};

            System.out.println(String.format("%-32s %-20s %-12s %-19s %-8s %-23s %-23s %-23s %s",
                                             BARControl.tr("Job"),
                                             BARControl.tr("Hostname"),
                                             BARControl.tr("Type"),
                                             BARControl.tr("Date/Time"),
                                             BARControl.tr("Duration"),
// TODO: [bytes]
                                             BARControl.tr("Total           [bytes]"),
                                             BARControl.tr("Skipped         [bytes]"),
                                             BARControl.tr("With errors     [bytes]"),
                                             BARControl.tr("Message")
                                            )
                              );
            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
            }
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
                                         long   totalEntrySize    = valueMap.getLong  ("totalEntrySize",    Long.MAX_VALUE);
                                         long   skippedEntryCount = valueMap.getLong  ("skippedEntryCount"  );
                                         long   skippedEntrySize  = valueMap.getLong  ("skippedEntrySize",  Long.MAX_VALUE);
                                         long   errorEntryCount   = valueMap.getLong  ("errorEntryCount"    );
                                         long   errorEntrySize    = valueMap.getLong  ("errorEntrySize"     );

                                         // filter error message: replace LF
                                         errorMessage = errorMessage.replace("\n"," ");

                                         System.out.println(String.format("%-32s %-20s %-12s %-19s %02d:%02d:%02d %8d %14d %8d %14d %8d %14d %s",
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
            if (!Settings.noHeaderFooterFlag)
            {
              System.out.println(StringUtils.repeat("-",getTerminalWidth()));
              System.out.println(BARControl.tr("{0} {0,choice,0#entries|1#entry|1<entries}",n[0]));
            }
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot list history (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.restoreStorageName != null)
        {
          // set archives to restore
          try
          {
            final ArrayList<Long> storageIds = new ArrayList<Long>();
            BARServer.executeCommand(StringParser.format("INDEX_STORAGE_LIST entityId=%s indexStateSet=%s indexModeSet=%s name=%'S offset=%ld sortMode=NAME ordering=ASCENDING",
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

                                         storageIds.add(storageId);
                                       }
                                     }
                                    );
            if (storageIds.isEmpty())
            {
              throw new BARException(BARException.ARCHIVE_NOT_FOUND,Settings.restoreStorageName);
            }

            BARServer.executeCommand(StringParser.format("INDEX_LIST_CLEAR"),
                                     1  // debug level
                                    );
            int i = 0;
            while (i < storageIds.size())
            {
              int n = storageIds.size()-i; if (n > 1024) n = 1024;
              BARServer.executeCommand(StringParser.format("INDEX_LIST_ADD storageIds=%s",
                                                           StringUtils.join(storageIds,i,n,',')
                                                          ),
                                       1  // debugLevel
                                      );
              i += n;
            }
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot set restore list (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }


          // restore
          try
          {
            BARServer.executeCommand(StringParser.format("RESTORE type=ARCHIVES destination=%'S directoryContent=%y restoreEntryMode=%s",
                                                         Settings.destination,
                                                         true,
                                                         Settings.overwriteEntriesFlag ? "overwrite" : "stop"
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
                                             Actions       action       = valueMap.getEnum  ("action",      Actions.class                         );
                                             PasswordTypes passwordType = valueMap.getEnum  ("passwordType",PasswordTypes.class,PasswordTypes.NONE);
                                             String        passwordText = valueMap.getString("passwordText",""                                    );
                                             String        volume       = valueMap.getString("volume",      ""                                    );
                                             int           error        = valueMap.getInt   ("error",       BARException.NONE);
                                             String        errorMessage = valueMap.getString("errorMessage",""                                    );
                                             String        storage      = valueMap.getString("storage",     ""                                    );
                                             String        entry        = valueMap.getString("entry",       ""                                    );

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
// TODO:
Dprintf.dprintf("still not supported");
//System.exit(ExitCodes.FAIL);
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
                                             RestoreStates state            = valueMap.getEnum  ("state",           RestoreStates.class);
                                             long          doneCount        = valueMap.getLong  ("entryDoneSize",   0L);
                                             long          doneSize         = valueMap.getLong  ("doneSize",        0L);
                                             long          totalCount       = valueMap.getLong  ("totalCount",      0L);
                                             long          totalSize        = valueMap.getLong  ("totalSize",       0L);
                                             String        entryName        = valueMap.getString("entryName",       "");
                                             long          entryDoneSize    = valueMap.getLong  ("entryDoneSize",   0L);
                                             long          entryTotalSize   = valueMap.getLong  ("entryTotalSize",  0L);
                                             String        storageName      = valueMap.getString("storageName",     "");
                                             long          storageDoneSize  = valueMap.getLong  ("storageDoneSize", 0L);
                                             long          storageTotalSize = valueMap.getLong  ("storageTotalSize",0L);

                                             switch (state)
                                             {
                                               case RESTORED:
                                                 System.out.println(String.format(BARControl.tr("Restored {0} entries, {1} bytes",doneCount,doneSize)));
                                                 break;
                                               case FAILED:
                                                 printError(BARControl.tr("cannot restore storage ''{0}''",storageName));
                                                 break;
                                             }
                                           }
                                         }
                                         catch (Exception exception)
                                         {
                                           // ignored
                                           if (Settings.debugLevel > 0)
                                           {
                                             logThrowable(exception);
                                             System.exit(ExitCodes.FAIL);
                                           }
                                         }
                                       }
                                     }
                                    );
          }
          catch (Exception exception)
          {
            printError(BARControl.tr("cannot restore storages (error: {0})",exception.getMessage()));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
          }
        }

        if (Settings.debugQuitServerFlag)
        {
          // quit server
          if (!BARServer.quit())
          {
            printError(BARControl.tr("cannot quit server"));
            BARServer.disconnect();
            System.exit(ExitCodes.FAIL);
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

        // get images
        IMAGE_ERROR         = Widgets.loadImage(display,"error.png");
        IMAGE_LOCK          = Widgets.loadImage(display,"lock.png");
        IMAGE_LOCK_INSECURE = Widgets.loadImage(display,"lockInsecure.png");

        boolean connectedFlag = false;

        // connect to server
        if (!login(loginData,
                   Settings.loginDialogFlag
                  )
           )
        {
          System.exit(ExitCodes.FAIL);
        }

        // store login settings
        Settings.serverNoTLS    = (loginData.tlsMode == BARServer.TLSModes.NONE );
        Settings.serverForceTLS = (loginData.tlsMode == BARServer.TLSModes.FORCE);
        Settings.role           = loginData.role;

        do
        {
          // add watchdog for loaded classes/JARs
          initClassesWatchDog();

          // create main window
          createWindow();
          createTabs();
          createMenu();

          // notify new server
          Widgets.notify(shell,BARControl.USER_EVENT_SELECT_SERVER);

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
      printInternalError(exception.getCause());
      System.exit(ExitCodes.INTERNAL_ERROR);
    }
    catch (CommunicationError error)
    {
      try
      {
        Dialogs.error(new Shell(),error.getMessage());
      }
      catch (Throwable throwable)
      {
        // ignored
      }
      printError(BARControl.tr("communication: {0}",error.getMessage()));
      if (Settings.debugLevel > 0)
      {
        printStackTrace(error);
      }
      System.exit(ExitCodes.FAIL);
    }
    catch (AssertionError error)
    {
      internalError(error);
    }
    catch (InternalError error)
    {
      internalError(error);
    }
    catch (Throwable throwable)
    {
      printError(throwable);
      if (Settings.debugLevel > 0)
      {
        printStackTrace(throwable);
      }
      System.exit(ExitCodes.FAIL);
    }

    System.exit(ExitCodes.OK);
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
