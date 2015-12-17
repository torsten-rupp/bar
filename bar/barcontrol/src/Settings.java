/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Settings.java,v $
* $Revision: 1.5 $
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
import java.util.List;
import java.util.regex.Pattern;
import java.util.Set;
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
  class SettingValueAdapterStringSet extends SettingValueAdapter<String,LinkedHashSet<String> >
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
  static class SimpleStringArray
  {
    public final String[] stringArray;

    /** create simple string array
     */
    SimpleStringArray()
    {
      this.stringArray = new String[0];
    }

    /** create simple string array
     * @param width width array
     */
    SimpleStringArray(String[] stringArray)
    {
      this.stringArray = stringArray;
    }

    /** create simple string array
     * @param widthList with list
     */
    SimpleStringArray(ArrayList<String> stringList)
    {
      this.stringArray = stringList.toArray(new String[stringList.size()]);
    }

    /** get string
     * @param index index (0..n-1)
     * @return string or null
     */
    public String get(int index)
    {
      return (index < stringArray.length) ? stringArray[index] : null;
    }

    /** get string array
     * @return string array
     */
    public String[] get()
    {
      return stringArray;
    }

    /** get mapped indizes
     * @return indizes
     */
    public int[] getMap(String strings[])
    {
      int indizes[] = new int[strings.length];
      for (int i = 0; i < strings.length; i++)
      {
        indizes[i] = i;
      }
      for (int i = 0; i < stringArray.length; i++)
      {
        int j = StringUtils.indexOf(strings,stringArray[i]);
        if (j >= 0)
        {
          indizes[i] = j;
        }
      }

      return indizes;
    }

    /** convert data to string
     * @return string
     */
    public String toString()
    {
      StringBuilder buffer = new StringBuilder();
      for (String string : stringArray)
      {
        if (buffer.length() > 0) buffer.append(',');
        buffer.append(string);
      }
      return "Strings {"+buffer.toString()+"}";
    }
  }

  /** config value adapter String <-> column width array
   */
  class SettingValueAdapterSimpleStringArray extends SettingValueAdapter<String,SimpleStringArray>
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public SimpleStringArray toValue(String string) throws Exception
    {
      StringTokenizer tokenizer = new StringTokenizer(string,",");
      ArrayList<String> stringList = new ArrayList<String>();
      while (tokenizer.hasMoreTokens())
      {
        stringList.add(tokenizer.nextToken());
      }
      return new SimpleStringArray(stringList);
    }

    /** convert to string
     * @param value value
     * @return string
     */
    public String toString(SimpleStringArray simpleStringArray) throws Exception
    {
      StringBuilder buffer = new StringBuilder();
      for (String string : simpleStringArray.get())
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
     * @param width width array
     */
    ColumnSizes(int[] width)
    {
      this.width = width;
    }

    /** create column sizes
     * @param width width (int list)
     */
    ColumnSizes(Object... width)
    {
      this.width = new int[width.length];
      for (int z = 0; z < width.length; z++)
      {
        this.width[z] = (Integer)width[z];
      }
    }

    /** create column sizes
     * @param widthList with list
     */
    ColumnSizes(ArrayList<Integer> widthList)
    {
      this.width = new int[widthList.size()];
      for (int z = 0; z < widthList.size(); z++)
      {
        this.width[z] = widthList.get(z);
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
  class SettingValueAdapterWidthArray extends SettingValueAdapter<String,ColumnSizes>
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
  static class Server implements Cloneable
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
  class SettingValueAdapterServer extends SettingValueAdapter<String,Server>
  {
    /** convert to value
     * @param string string
     * @return value
     */
    public Server toValue(String string) throws Exception
    {
      Server server = null;

      Object[] data = new Object[2];
      if      (StringParser.parse(string,"%s:%d",data,StringUtils.QUOTE_CHARS))
      {
        String name = (String)data[0];
        int    port = (Integer)data[1];
        server = new Server(name,port);
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
      return server.name+":"+server.port;
    }

    /** compare servers
     * @param server0,server1 servers to compare
     * @return true if servers equals
     */
    public boolean equals(Server server0, Server server1)
    {
      return    server0.name.equals(server1.name)
             && server0.port ==  server1.port;
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
      LinkedList<Server> servers = (LinkedList<Server>)value;

      for (String serverName : Settings.serverNames)
      {
        boolean existsFlag = false;
        for (Server server : servers)
        {
          if (   server.name.equals(serverName)
              && (server.port == Settings.serverPort)
             )
          {
            existsFlag = true;
            break;
          }
        }

        if (!existsFlag)
        {
          servers.add(new Server(serverName,Settings.serverPort,Settings.serverPassword));
        }
      }

      return servers;
    }
  }

  // --------------------------- constants --------------------------------
  static final String DEFAULT_SERVER_NAME                 = "localhost";
  static final int    DEFAULT_SERVER_PORT                 = 38523;
  static final int    DEFAULT_SERVER_TLS_PORT             = 38524;
  static final String DEFAULT_BARCONTROL_CONFIG_FILE_NAME = System.getProperty("user.home")+File.separator+".bar"+File.separator+"barcontrol.cfg";

  /** archive types
   */
  public enum ArchiveTypes
  {
    NORMAL,
    FULL,
    INCREMENTAL,
    DIFFERENTIAL,
    CONTINUOUS;

    /** convert to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case NORMAL:       return "normal";
        case FULL:         return "full";
        case INCREMENTAL:  return "incremental";
        case DIFFERENTIAL: return "differential";
        case CONTINUOUS:   return "continuous";
        default:           return "normal";
      }
    }
  };

  // --------------------------- variables --------------------------------

  @SettingComment(text={"BARControl configuration",""})

  // program settings
  @SettingValue(type=SettingValueAdapterSimpleStringArray.class)
  public static SimpleStringArray     jobListColumnOrder              = new SimpleStringArray();
  @SettingValue(type=SettingValueAdapterWidthArray.class)
  public static ColumnSizes           jobListColumns                  = new ColumnSizes(110,130,90,90,80,80,100,150,120);

  @SettingComment(text={"","Pause default settings"})
  @SettingValue
  public static boolean               pauseCreateFlag                 = true;
  @SettingValue
  public static boolean               pauseStorageFlag                = false;
  @SettingValue
  public static boolean               pauseRestoreFlag                = true;
  @SettingValue
  public static boolean               pauseIndexUpdateFlag            = false;

  // server settings
  @SettingComment(text={"","Server settings"})
  @SettingValue(name="server",type=SettingValueAdapterServer.class,migrate=SettingMigrateServer.class)
  public static LinkedList<Server>    servers                         = new LinkedList<Server>();//[]{new Server(DEFAULT_SERVER_NAME,DEFAULT_SERVER_PORT)};
  @SettingValue(name="serverName",type=String.class,obsolete=true)
  private static LinkedHashSet<String> serverNames                     = new LinkedHashSet<String>();
  @SettingValue(obsolete=true)
  public static String                serverPassword                  = null;
  @SettingValue(obsolete=true)
  public static int                   serverPort                      = DEFAULT_SERVER_PORT;
  @SettingValue(obsolete=true)
  public static int                   serverTLSPort                   = DEFAULT_SERVER_TLS_PORT;
  @SettingValue
  public static String                serverKeyFileName               = null;

  public static String                selectedJobName                 = null;
  public static boolean               loginDialogFlag                 = false;

  // commands and data
  public static String                runJobName                      = null;
  public static ArchiveTypes          archiveType                     = ArchiveTypes.NORMAL;
  public static String                abortJobName                    = null;
  public static String                indexDatabaseAddStorageName     = null;
  public static String                indexDatabaseRemoveStorageName  = null;
  public static String                indexDatabaseRefreshStorageName = null;
  public static String                indexDatabaseStorageListPattern = null;
  public static String                indexDatabaseEntriesListPattern = null;
  public static int                   pauseTime                       = 0;
  public static boolean               pingFlag                        = false;
  public static boolean               suspendFlag                     = false;
  public static boolean               continueFlag                    = false;
  public static boolean               listFlag                        = false;

  // debug
  public static int                   debugLevel                      = 0;
  public static boolean               debugQuitServerFlag             = false;

  // version, help
  public static boolean               versionFlag                     = false;
  public static boolean               helpFlag                        = false;

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
    load(DEFAULT_BARCONTROL_CONFIG_FILE_NAME);
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
    save(DEFAULT_BARCONTROL_CONFIG_FILE_NAME);
  }

  /** check if program settings file is modified
   * @return true iff modified
   */
  public static boolean isModified()
  {
    return SettingUtils.isModified(new File(DEFAULT_BARCONTROL_CONFIG_FILE_NAME));
  }

  /** get last used server or default server
   * @return server
   */
  public static Server getLastServer()
  {
    if (servers.size() > 0)
    {
      return servers.getLast();
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
    for (int i = 0; i < servers.size(); i++)
    {
      Server server = servers.get(i);
      if (   server.name.equals(name)
          && (server.port == port)
         )
      {
        servers.remove(i);
        server.password = password;
        servers.add(server);
        return;
      }
    }

    servers.add(new Server(name,port,password));
  }

  //-----------------------------------------------------------------------
}

/* end of file */
