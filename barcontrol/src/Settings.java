/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: load/save program settings
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/

import java.io.File;

import java.text.ParseException;
import java.text.SimpleDateFormat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.LinkedList;
import java.util.regex.Pattern;
import java.util.StringTokenizer;

import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.RGB;

/****************************** Classes ********************************/

/** settings
 */
public class Settings
{
  /** config value adapter String <-> linked string hash set
   */
  class SettingValueAdapterStringSet extends SettingUtils.ValueAdapter<String,LinkedHashSet<String> >
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public LinkedHashSet<String> toValue(String string) throws Exception
    {
      StringTokenizer tokenizer = new StringTokenizer(string,",");
      ArrayList<String> stringList = new ArrayList<String>();
      while (tokenizer.hasMoreTokens())
      {
        stringList.add(tokenizer.nextToken());
      }
      return new LinkedHashSet(stringList);
    }

    /** convert to string
     * @param value value
     * @return string
     */
    public String toString(LinkedHashSet<String> stringSet) throws Exception
    {
      StringBuilder buffer = new StringBuilder();
      for (String string : stringSet)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(string);
      }
      return buffer.toString();
    }
  }

  /** column sizes
   */
  static class ColumnSizes
  {
    public final int[] width;

    /** create column sizes
     * @param width width (int array)
     */
    ColumnSizes(int... width)
    {
      int totalWidth = 0;
      this.width = new int[width.length];
      for (int z = 0; z < width.length; z++)
      {
        this.width[z] = width[z];
        totalWidth += this.width[z];
      }

      // force min. width of at least one element
      if (totalWidth == 0)
      {
        width[0] = 10;
      }
    }

    /** create column sizes
     * @param widthList with list
     */
    ColumnSizes(ArrayList<Integer> widthList)
    {
      int totalWidth = 0;
      this.width = new int[widthList.size()];
      for (int z = 0; z < widthList.size(); z++)
      {
        this.width[z] = widthList.get(z);
        totalWidth += this.width[z];
      }

      // force min. width of at least one element
      if (totalWidth == 0)
      {
        width[0] = 10;
      }
    }

    /** get width
     * @param columNb column index (0..n-1)
     * @return width or 0
     */
    public int get(int columNb)
    {
      return (columNb < width.length) ? width[columNb] : 0;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      StringBuilder buffer = new StringBuilder();
      for (int n : width)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(Integer.toString(n));
      }
      return "ColumnSizes {"+buffer.toString()+"}";
    }
  }

  /** config value adapter String <-> column width array
   */
  class SettingValueAdapterWidthArray extends SettingUtils.ValueAdapter<String,ColumnSizes>
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public ColumnSizes toValue(String string) throws Exception
    {
      StringTokenizer tokenizer = new StringTokenizer(string,",");
      ArrayList<Integer> widthList = new ArrayList<Integer>();
      while (tokenizer.hasMoreTokens())
      {
        widthList.add(Integer.parseInt(tokenizer.nextToken()));
      }
      return new ColumnSizes(widthList);
    }

    /** convert to string
     * @param value value
     * @return string
     */
    public String toString(ColumnSizes columnSizes) throws Exception
    {
      StringBuilder buffer = new StringBuilder();
      for (int width : columnSizes.width)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(Integer.toString(width));
      }
      return buffer.toString();
    }
  }

  /** server
   */
  static class Server implements Cloneable, Comparable
  {
    public String name;
    public int    port;
    public String password;

    /** create server
     * @param name server name
     * @param port server port number
     * @param password server login password
     */
    Server(String name, int port, String password)
    {
      this.name     = name;
      this.port     = port;
      this.password = password;
    }

    /** create server
     * @param name server name
     * @param port server port number
     */
    Server(String name, int port)
    {
      this(name,port,"");
    }

    /** create server
     * @param name server name
     */
    Server(String name)
    {
      this(name,DEFAULT_SERVER_PORT);
    }

    /** create server
     */
    Server()
    {
      this("",0);
    }

    /** clone object
     * @return cloned object
     */
    public Server clone()
    {
      return new Server(name,port,password);
    }

    /** get hashcode
     * @return always 0
     */
    @Override
    public int hashCode()
    {
      return 0;
    }

    /** check if equals
     * @param object server object
     * @return true iff equals
     */
    @Override
    public boolean equals(Object object)
    {
      Server server = (Server)object;
      return this.name.equals(server.name) && (this.port == server.port);
    }

    /** compare index data
     * @param object index data
     * @return -1/0/1 if less/equals/greater
     */
    @Override
    public int compareTo(Object object)
    {
      int result;

      Server server = (Server)object;
      result = this.name.compareTo(server.name);
      if (result == 0)
      {
        if      (this.port < server.port) result = -1;
        else if (this.port > server.port) result =  1;
      }

      return result;
    }

    /** get data string
     * @return string
     */
    public String getData()
    {
      return name+":"+port;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      return name+((port != DEFAULT_SERVER_PORT) ? ":"+port : "");
    }
  }

  /** config value adapter String <-> server
   */
  class SettingValueAdapterServer extends SettingUtils.ValueAdapter<String,Server>
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public Server toValue(String string) throws Exception
    {
      Server server = null;

      Object[] data = new Object[3];
      if      (StringParser.parse(string,"%s:%d:%'s",data,StringUtils.QUOTE_CHARS))
      {
        String name     = (String) data[0];
        int    port     = (Integer)data[1];
        String password = (String) data[2];
        server = new Server(name,port,password);
      }
      else if (StringParser.parse(string,"%s:%d",data,StringUtils.QUOTE_CHARS))
      {
        String name = (String) data[0];
        int    port = (Integer)data[1];
        server = new Server(name,port);
      }
      else if (StringParser.parse(string,"%s",data,StringUtils.QUOTE_CHARS))
      {
        String name = (String)data[0];
        server = new Server(name);
      }
      else
      {
        throw new Exception(String.format("Cannot parse server definition '%s'",string));
      }

      return server;
    }

    /** convert to string
     * @param server server
     * @return string
     */
    public String toString(Server server) throws Exception
    {
      if ((server.password != null) && !server.password.isEmpty())
      {
        return StringParser.format("%s:%d:%'s",server.name,server.port,server.password);
      }
      else
      {
        return StringParser.format("%s:%d",server.name,server.port);
      }
    }

    /** compare servers
     * @param server0,server1 servers to compare
     * @return true if servers equals
     */
    public boolean equals(Server server0, Server server1)
    {
      return    server0.name.equals(server1.name)
             && (server0.port == server1.port);
    }
  }

  /** migrate values
   */
  class SettingMigrateServer implements SettingMigrate
  {
    /** migrate value
     * @param value current value
     * @return new value
     */
    public Object run(Object value)
    {
      LinkedHashSet<Server> servers = (LinkedHashSet<Server>)value;

      for (String serverName : Settings.serverNames)
      {
        Server server = new Server(serverName,Settings.serverPort,Settings.serverPassword);
        servers.remove(server);
        servers.add(server);
      }
      while (servers.size() > MAX_SERVER_HISTORY)
      {
        servers.remove(servers.iterator().next());
      }

      return servers;
    }
  }

  /** window geometry
   */
  static class Geometry
  {
    public int width;
    public int height;
    public int x;
    public int y;

    /** create geometry
     */
    public Geometry()
    {
      this.width  = -1;
      this.height = -1;
      this.x      = -1;
      this.y      = -1;
    }

    /** convert to string
     * @return string
     */
    public String toString()
    {
      return "Geometry {"+x+" "+y+" "+width+" "+height+"}";
    }
  }

  // --------------------------- constants --------------------------------
  static final String DEFAULT_SERVER_NAME                 = "localhost";
  static final int    DEFAULT_SERVER_PORT                 = 38523;
  static final int    DEFAULT_SERVER_TLS_PORT             = 0;
  static final String DEFAULT_BARCONTROL_CONFIG_FILE_NAME = System.getProperty("user.home")+File.separator+".bar"+File.separator+"barcontrol.cfg";

  static final int    MAX_SERVER_HISTORY = 10;

  // --------------------------- variables --------------------------------

  @SettingComment(text={"BARControl configuration",""})

  // program settings
  @SettingValue(type=SettingUtils.ValueAdapterSimpleStringArray.class, name="job-table-column-order")
  public static SettingUtils.SimpleStringArray jobListColumnOrder              = new SettingUtils.SimpleStringArray();
  @SettingValue(type=SettingValueAdapterWidthArray.class, name="job-table-columns")
  public static ColumnSizes                    jobTableColumns                 = new ColumnSizes(110,130,90,90,80,80,100,150,120);
  @SettingValue(type=SettingValueAdapterWidthArray.class, name="mount-table-columns")
  public static ColumnSizes                    mountTableColumns               = new ColumnSizes(600,100);
  @SettingValue(type=SettingValueAdapterWidthArray.class, name="schedule-table-columns")
  public static ColumnSizes                    scheduleTableColumns            = new ColumnSizes(120,250,100,100,90);
  @SettingValue(type=SettingValueAdapterWidthArray.class, name="persistence-tree-columns")
  public static ColumnSizes                    persistenceTreeColumns          = new ColumnSizes(100,90,90,90,140,90,120);

  @SettingComment(text={"","Pause default settings"})
  @SettingValue(name="pause-create")
  public static boolean                        pauseCreateFlag                 = true;
  @SettingValue(name="pause-storage")
  public static boolean                        pauseStorageFlag                = false;
  @SettingValue(name="pause-restore")
  public static boolean                        pauseRestoreFlag                = true;
  @SettingValue(name="pause-index-update")
  public static boolean                        pauseIndexUpdateFlag            = false;
  @SettingValue(name="pause-index-maintenance")
  public static boolean                        pauseIndexMaintenanceFlag       = false;

  // server settings
  @SettingComment(text={"","Server settings"})
  @SettingValue(name="server",type=SettingValueAdapterServer.class,migrate=SettingMigrateServer.class)
  public static LinkedHashSet<Server>          servers                         = new LinkedHashSet<Server>();
  @SettingValue(name="server-ca-file")
  public static String                         serverCAFileName                = null;
  @SettingValue(name="server-keystore-file")
  public static String                         serverKeystoreFileName          = null;
  @SettingValue(name="no-tls")
  public static boolean                        serverNoTLS                     = false;
  @SettingValue(name="force-tls")
  public static boolean                        serverForceTLS                  = false;
  @SettingValue(name="tls-insecure")
  public static boolean                        serverInsecureTLS               = false;
  @SettingValue(name="role")
  public static BARControl.Roles               role                            = BARControl.Roles.BASIC;

  public static Geometry                       geometry                        = new Geometry();

  // file requester shortcuts
  @SettingComment(text={"","Shortcuts"})
  @SettingValue(name="shortcut")
  public static HashSet<String>                shortcuts                       = new HashSet<String>();

  public static String                         configFileName                  = DEFAULT_BARCONTROL_CONFIG_FILE_NAME;

  public static String                         selectedJobName                 = null;
  public static boolean                        loginDialogFlag                 = false;
  public static boolean                        pairMasterFlag                  = false;

  // commands and data
  public static String                         runJobName                      = null;
  public static ArchiveTypes                   archiveType                     = ArchiveTypes.NORMAL;
  public static String                         abortJobName                    = null;
  public static int                            pauseTime                       = 0;
  public static int                            maintenanceTime                 = 0;
  public static boolean                        pingFlag                        = false;
  public static boolean                        suspendFlag                     = false;
  public static boolean                        continueFlag                    = false;
  public static boolean                        listFlag                        = false;
  public static String                         infoJobName                     = null;

  public static boolean                        indexDatabaseInfo               = false;
  public static String                         indexDatabaseAddStorageName     = null;
  public static String                         indexDatabaseRemoveStorageName  = null;
  public static String                         indexDatabaseRefreshStorageName = null;
  public static String                         indexDatabaseEntitiesListName   = null;
  public static String                         indexDatabaseStoragesListName   = null;
  public static String                         indexDatabaseEntriesListName    = null;
  public static boolean                        indexDatabaseEntriesNewestOnly  = false;
  public static boolean                        indexDatabaseHistoryList        = false;

  public static String                         restoreStorageName              = null;
  public static String                         destination                     = "";
  public static boolean                        overwriteEntriesFlag            = false;

  // flags
//TODO: preference dialog
  @SettingValue
  public static Boolean                        showEntriesExceededInfo         = new Boolean(true);
//TODO: preference dialog
  @SettingValue
  public static Boolean                        showEntriesMarkInfo             = new Boolean(true);
//TODO: preference dialog
  @SettingValue
  public static Boolean                        showNewVersionInformation       = new Boolean(true);
  @SettingValue
  public static Boolean                        showSlaveDisconnected           = new Boolean(true);

  // version, help
  public static boolean                        versionFlag                     = false;
  public static boolean                        helpFlag                        = false;
  public static boolean                        xhelpFlag                       = false;

  // debug
  public static int                            debugLevel                      = 0;
  public static boolean                        debugIgnoreProtocolVersion      = false;
  public static boolean                        debugFakeTLSFlag                = false;
  public static boolean                        debugQuitServerFlag             = false;

  // server name
  public static String                         serverName                      = null;

  // deprecated
  @SettingValue(name="server-name",type=String.class,deprecated=true)
  public static LinkedHashSet<String>          serverNames                     = new LinkedHashSet<String>();
  @SettingValue(name="server-password",deprecated=true)
  public static String                         serverPassword                  = null;
  @SettingValue(name="server-port",deprecated=true)
  public static int                            serverPort                      = DEFAULT_SERVER_PORT;
  @SettingValue(name="server-tls-port",deprecated=true)
  public static int                            serverTLSPort                   = DEFAULT_SERVER_TLS_PORT;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** load program settings
   * @param fileName settings file name
   */
  public static void load(String fileName)
  {
    SettingUtils.load(new File(fileName));
  }

  /** load default program settings
   */
  public static void load()
  {
    load(configFileName);
  }

  /** save program settings
   * @param fileName settings file name
   */
  public static void save(String fileName)
  {
    SettingUtils.save(new File(fileName));
  }

  /** save program settings with default name
   */
  public static void save()
  {
    save(configFileName);
  }

  /** check if program settings file is modified
   * @return true iff modified
   */
  public static boolean isModified()
  {
    return SettingUtils.isModified();
  }

  /** get server
   * @param name server name
   * @param port server port
   * @return server
   */
  public static Server getServer(String name, int port)
  {
    for (Server server : servers)
    {
      if (server.name.equals(name) && (server.port == port))
      {
        return server;
      }
    }
    return null;
  }

  /** get last used server or default server
   * @return server
   */
  public static Server getLastServer()
  {
    if (servers.size() > 0)
    {
      Server serverArray[] = servers.toArray(new Server[servers.size()]);
      return serverArray[serverArray.length-1];
    }
    else
    {
      return new Server(DEFAULT_SERVER_NAME,DEFAULT_SERVER_PORT);
    }
  }

  /** add or update server
   * @param name server name
   * @param port server port
   * @param password server password
   */
  public static void addServer(String name, int port, String password)
  {
    Server server = new Server(name,port,password);
    servers.remove(server);
    servers.add(server);
    while (servers.size() > MAX_SERVER_HISTORY)
    {
      servers.remove(0);
    }
  }

  /** check if basic role enabled
   * @retrue true iff basic role enabled
   */
  public static boolean hasBasicRole()
  {
    return    (role == BARControl.Roles.BASIC )
           || (role == BARControl.Roles.NORMAL)
           || (role == BARControl.Roles.EXPERT);
  }

  /** check if normal role enabled
   * @retrue true iff normal role enabled
   */
  public static boolean hasNormalRole()
  {
    return    (role == BARControl.Roles.NORMAL)
           || (role == BARControl.Roles.EXPERT);
  }

  /** check if expert role enabled
   * @retrue true iff expert role enabled
   */
  public static boolean hasExpertRole()
  {
    return role == BARControl.Roles.EXPERT;
  }

  //-----------------------------------------------------------------------
}

/* end of file */
