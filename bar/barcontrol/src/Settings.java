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
import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Field;
import java.lang.reflect.Constructor;
import static java.lang.annotation.ElementType.FIELD;
import static java.lang.annotation.ElementType.TYPE;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

import java.text.ParseException;
import java.text.SimpleDateFormat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Date;
import java.util.EnumSet;
import java.util.EnumSet;
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

/** setting comment annotation
 */
@Target({TYPE,FIELD})
@Retention(RetentionPolicy.RUNTIME)
@interface SettingComment
{
  String[] text() default {""};                  // comment before value
}

/** setting value migrate interface
 */
interface SettingMigrate
{
  /** migrate value
   * @param value current value
   * @return new value
   */
  public Object run(Object value);
}

/** setting value annotation
 */
@Target({TYPE,FIELD})
@Retention(RetentionPolicy.RUNTIME)
@interface SettingValue
{
  String  name()         default "";              // name of value
  String  defaultValue() default "";              // default value
  Class   type()         default DEFAULT.class;   // adapter class
  boolean obsolete()     default false;           // true iff obsolete setting

  Class migrate() default DEFAULT.class;

  static final class DEFAULT
  {
  }
}

/** setting value adapter
 */
abstract class SettingValueAdapter<String,Value>
{
  /** convert to value
   * @param string string
   * @return value
   */
  abstract public Value toValue(String string)throws Exception;

  /** convert to string
   * @param value value
   * @return string
   */
  abstract public String toString(Value value) throws Exception;

  /** check if equals
   * @param value0,value1 values to compare
   * @return true if value0==value1
   */
  public boolean equals(Value value0, Value value1)
  {
    return false;
  }

  /** migrate values
   */
  public void Xmigrate(Object value)
  {
  }
}

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

  private static long lastModified = 0L;

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
   * @param file settings file to load
   */
  public static void load(File file)
  {
    if (file.exists())
    {
      // get setting classes
      Class[] settingClasses = getSettingClasses();

      BufferedReader input = null;
      try
      {
        // open file
        input = new BufferedReader(new FileReader(file));

        // read file
        int      lineNb = 0;
        String   line;
        Object[] data = new Object[2];
        while ((line = input.readLine()) != null)
        {
          line = line.trim();
          lineNb++;

          // check comment
          if (line.isEmpty() || line.startsWith("#"))
          {
            continue;
          }

          // parse
          if (StringParser.parse(line,"%s = % s",data))
          {
            String name   = (String)data[0];
            String string = (String)data[1];

            for (Class clazz : settingClasses)
            {
              for (Field field : clazz.getDeclaredFields())
              {
                for (Annotation annotation : field.getDeclaredAnnotations())
                {
                  if (annotation instanceof SettingValue)
                  {
                    SettingValue settingValue = (SettingValue)annotation;

                    if (((!settingValue.name().isEmpty()) ? settingValue.name() : field.getName()).equals(name))
                    {
                      try
                      {
                        Class type = field.getType();
                        if      (type.isArray())
                        {
                          // array type
                          type = type.getComponentType();
                          if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            SettingValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (enclosingClass == Settings.class)
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                            }

                            // convert to value
                            Object value = settingValueAdapter.toValue(string);
                            field.set(null,addArrayUniq((Object[])field.get(null),value,settingValueAdapter));
                          }
                          else if (type == int.class)
                          {
                            int value = Integer.parseInt(string);
                            field.set(null,addArrayUniq((int[])field.get(null),value));
                          }
                          else if (type == Integer.class)
                          {
                            int value = Integer.parseInt(string);
                            field.set(null,addArrayUniq((Integer[])field.get(null),value));
                          }
                          else if (type == long.class)
                          {
                            long value = Long.parseLong(string);
                            field.set(null,addArrayUniq((long[])field.get(null),value));
                          }
                          else if (type == Long.class)
                          {
                            long value = Long.parseLong(string);
                            field.set(null,addArrayUniq((Long[])field.get(null),value));
                          }
                          else if (type == boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            field.set(null,addArrayUniq((boolean[])field.get(null),value));
                          }
                          else if (type == Boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            field.set(null,addArrayUniq((Boolean[])field.get(null),value));
                          }
                          else if (type == String.class)
                          {
                            field.set(null,addArrayUniq((String[])field.get(null),StringUtils.unescape(string)));
                          }
                          else if (type.isEnum())
                          {
                            field.set(null,addArrayUniq((Enum[])field.get(null),StringUtils.parseEnum(type,string)));
                          }
                          else if (type == EnumSet.class)
                          {
                            field.set(null,addArrayUniq((EnumSet[])field.get(null),StringUtils.parseEnumSet(type,string)));
                          }
                          else
                          {
Dprintf.dprintf("field.getType()=%s",type);
                          }
                        }
                        else if (Set.class.isAssignableFrom(type))
                        {
                          // Set type
                          if (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            SettingValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (enclosingClass == Settings.class)
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                            }

                            // convert to value
                            Object value = settingValueAdapter.toValue(string);
Dprintf.dprintf("value=%s",value);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == int.class)
                          {
                            int value = Integer.parseInt(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == Integer.class)
                          {
                            int value = Integer.parseInt(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == long.class)
                          {
                            long value = Long.parseLong(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == Long.class)
                          {
                            long value = Long.parseLong(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == Boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            ((Set)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == String.class)
                          {
                            ((Set)field.get(null)).add(StringUtils.unescape(string));
                          }
                          else if (settingValue.type().isEnum())
                          {
                            ((Set)field.get(null)).add(StringUtils.parseEnum(type,string));
                          }
                          else if (settingValue.type() == EnumSet.class)
                          {
                            ((Set)field.get(null)).add(StringUtils.parseEnumSet(type,string));
                          }
                          else
                          {
Dprintf.dprintf("Set without value adapter %s",settingValue.type());
                          }
                        }
                        else if (List.class.isAssignableFrom(type))
                        {
                          // List type
                          if (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            SettingValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (enclosingClass == Settings.class)
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                            }

                            // convert to value
                            Object value = settingValueAdapter.toValue(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == int.class)
                          {
                            int value = Integer.parseInt(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == Integer.class)
                          {
                            int value = Integer.parseInt(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == long.class)
                          {
                            long value = Long.parseLong(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == Long.class)
                          {
                            long value = Long.parseLong(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == Boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            ((List)field.get(null)).add(value);
                          }
                          else if (settingValue.type() == String.class)
                          {
                            ((List)field.get(null)).add(StringUtils.unescape(string));
                          }
                          else if (settingValue.type().isEnum())
                          {
                            ((List)field.get(null)).add(StringUtils.parseEnum(type,string));
                          }
                          else if (settingValue.type() == EnumSet.class)
                          {
                            ((List)field.get(null)).add(StringUtils.parseEnumSet(type,string));
                          }
                          else
                          {
Dprintf.dprintf("List without value adapter %s",settingValue.type());
                          }
                        }
                        else
                        {
                          // primitiv type
                          if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                          {
                            // instantiate config adapter class
                            SettingValueAdapter settingValueAdapter;
                            Class enclosingClass = settingValue.type().getEnclosingClass();
                            if (enclosingClass == Settings.class)
                            {
                              Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                              settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                            }
                            else
                            {
                              settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                            }

                            // convert to value
                            Object value = settingValueAdapter.toValue(string);
                            field.set(null,value);
                          }
                          else if (type == int.class)
                          {
                            int value = Integer.parseInt(string);
                            field.setInt(null,value);
                          }
                          else if (type == Integer.class)
                          {
                            int value = Integer.parseInt(string);
                            field.set(null,new Integer(value));
                          }
                          else if (type == long.class)
                          {
                            long value = Long.parseLong(string);
                            field.setLong(null,value);
                          }
                          else if (type == Long.class)
                          {
                            long value = Long.parseLong(string);
                            field.set(null,new Long(value));
                          }
                          else if (type == boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            field.setBoolean(null,value);
                          }
                          else if (type == Boolean.class)
                          {
                            boolean value = StringUtils.parseBoolean(string);
                            field.set(null,new Boolean(value));
                          }
                          else if (type == String.class)
                          {
                            field.set(null,StringUtils.unescape(string));
                          }
                          else if (type.isEnum())
                          {
                            field.set(null,StringUtils.parseEnum(type,string));
                          }
                          else if (type == EnumSet.class)
                          {
                            Class enumClass = settingValue.type();
                            if (!enumClass.isEnum())
                            {
                              throw new Error(enumClass+" is not an enum class!");
                            }
                            field.set(null,StringUtils.parseEnumSet(enumClass,string));
                          }
                          else
                          {
Dprintf.dprintf("field.getType()=%s",type);
                          }
                        }
                      }
                      catch (NumberFormatException exception)
                      {
                        BARControl.printWarning("Cannot parse number '%s' for configuration value '%s' in line %d",string,name,lineNb);
                      }
                      catch (Exception exception)
                      {
Dprintf.dprintf("exception=%s",exception);
exception.printStackTrace();
                      }
                    }
                  }
                  else
                  {
                  }
                }
              }
            }
          }
          else
          {
            BARControl.printWarning("Unknown configuration value '%s' in line %d",line,lineNb);
          }
        }

        // close file
        input.close(); input = null;
      }
      catch (IOException exception)
      {
        // ignored
      }
      finally
      {
        try
        {
          if (input != null) input.close();
        }
        catch (IOException exception)
        {
          // ignored
        }
      }

      // migrate values
      for (Class clazz : settingClasses)
      {
        for (Field field : clazz.getDeclaredFields())
        {
          for (Annotation annotation : field.getDeclaredAnnotations())
          {
            if (annotation instanceof SettingValue)
            {
              SettingValue settingValue = (SettingValue)annotation;

              try
              {
                Class migrate = settingValue.migrate();
                if (migrate != SettingValue.DEFAULT.class)
                {
                  Constructor constructor = migrate.getDeclaredConstructor(Settings.class);
                  SettingMigrate settingMigrate = (SettingMigrate)constructor.newInstance(new Settings());
//                  settingMigrate.run(settingValue);
                  field.set(null,settingMigrate.run(field.get(null)));
                }
              }
              catch (Exception exception)
              {
Dprintf.dprintf("exception=%s",exception);
exception.printStackTrace();
              }

            }
          }
        }
      }
    }
  }

  /** load program settings
   * @param fileName settings file name
   */
  public static void load(String fileName)
  {
    load(new File(fileName));
  }

  /** load default program settings
   */
  public static void load()
  {
    File file = new File(DEFAULT_BARCONTROL_CONFIG_FILE_NAME);

    // load file
    load(file);

    // save last modified time
    lastModified = file.lastModified();
  }

  /** save program settings
   * @param fileName file nam
   */
  public static void save(File file)
  {
    // create directory
    File directory = file.getParentFile();
    if ((directory != null) && !directory.exists()) directory.mkdirs();

    PrintWriter output = null;
    try
    {
      // get setting classes
      Class[] settingClasses = getSettingClasses();

      // open file
      output = new PrintWriter(new FileWriter(file));

      // write settings
      for (Class clazz : settingClasses)
      {
        for (Field field : clazz.getDeclaredFields())
        {
//Dprintf.dprintf("field=%s",field);
          for (Annotation annotation : field.getDeclaredAnnotations())
          {
            if      (annotation instanceof SettingValue)
            {
              SettingValue settingValue = (SettingValue)annotation;

              if (!settingValue.obsolete())
              {
              // get value and write to file
              String name = (!settingValue.name().isEmpty()) ? settingValue.name() : field.getName();
              try
              {
                Class type = field.getType();
                if      (type.isArray())
                {
                  // array type
                  type = type.getComponentType();
                  if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                  {
                    // instantiate config adapter class
                    SettingValueAdapter settingValueAdapter;
                    Class enclosingClass = settingValue.type().getEnclosingClass();
                    if (enclosingClass == Settings.class)
                    {
                      Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                      settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                    }
                    else
                    {
                      settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                    }

                    // convert to string
                    for (Object object : (Object[])field.get(null))
                    {
                      String value = (String)settingValueAdapter.toString(object);
                      output.printf("%s = %s\n",name,value);
                    }
                  }
                  else if (type == int.class)
                  {
                    for (int value : (int[])field.get(null))
                    {
                      output.printf("%s = %d\n",name,value);
                    }
                  }
                  else if (type == Integer.class)
                  {
                    for (int value : (Integer[])field.get(null))
                    {
                      output.printf("%s = %d\n",name,value);
                    }
                  }
                  else if (type == long.class)
                  {
                    for (long value : (long[])field.get(null))
                    {
                      output.printf("%s = %ld\n",name,value);
                    }
                  }
                  else if (type == Long.class)
                  {
                    for (long value : (Long[])field.get(null))
                    {
                      output.printf("%s = %ld\n",name,value);
                    }
                  }
                  else if (type == boolean.class)
                  {
                    for (boolean value : (boolean[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,value ? "yes" : "no");
                    }
                  }
                  else if (type == Boolean.class)
                  {
                    for (boolean value : (Boolean[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,value ? "yes" : "no");
                    }
                  }
                  else if (type == String.class)
                  {
                    for (String value : (String[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.escape(value));
                    }
                  }
                  else if (type.isEnum())
                  {
                    for (Enum value : (Enum[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,value.toString());
                    }
                  }
                  else if (type == EnumSet.class)
                  {
                    for (EnumSet enumSet : (EnumSet[])field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.join(enumSet,","));
                    }
                  }
                  else
                  {
Dprintf.dprintf("field.getType()=%s",type);
                  }
                }
                else if (Set.class.isAssignableFrom(type))
                {
                  // Set type
                  if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                  {
                    // instantiate config adapter class
                    SettingValueAdapter settingValueAdapter;
                    Class enclosingClass = settingValue.type().getEnclosingClass();
                    if (enclosingClass == Settings.class)
                    {
                      Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                      settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                    }
                    else
                    {
                      settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                    }

                    // convert to string
                    for (Object object : (Set)field.get(null))
                    {
                      String value = (String)settingValueAdapter.toString(object);
                      output.printf("%s = %s\n",name,value);
                    }
                  }
                  else if (settingValue.type() == int.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %d\n",name,(Integer)object);
                    }
                  }
                  else if (settingValue.type() == Integer.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %d\n",name,(Integer)object);
                    }
                  }
                  else if (settingValue.type() == long.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %ld\n",name,(Long)object);
                    }
                  }
                  else if (settingValue.type() == Long.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %ld\n",name,(Long)object);
                    }
                  }
                  else if (settingValue.type() == boolean.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,(Boolean)object ? "yes" : "no");
                    }
                  }
                  else if (settingValue.type() == Boolean.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,(Boolean)object ? "yes" : "no");
                    }
                  }
                  else if (settingValue.type() == String.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.escape((String)object));
                    }
                  }
                  else if (settingValue.type().isEnum())
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,((Enum)object).toString());
                    }
                  }
                  else if (settingValue.type() == EnumSet.class)
                  {
                    for (Object object : (Set)field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.join((EnumSet)object,","));
                    }
                  }
                  else
                  {
Dprintf.dprintf("Set without value adapter %s",settingValue.type());
                  }
                }
                else if (List.class.isAssignableFrom(type))
                {
                  // List type
                  if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                  {
                    // instantiate config adapter class
                    SettingValueAdapter settingValueAdapter;
                    Class enclosingClass = settingValue.type().getEnclosingClass();
                    if (enclosingClass == Settings.class)
                    {
                      Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                      settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                    }
                    else
                    {
                      settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                    }

                    // convert to string
                    for (Object object : (List)field.get(null))
                    {
                      String value = (String)settingValueAdapter.toString(object);
                      output.printf("%s = %s\n",name,value);
                    }
                  }
                  else if (settingValue.type() == int.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %d\n",name,(Integer)object);
                    }
                  }
                  else if (settingValue.type() == Integer.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %d\n",name,(Integer)object);
                    }
                  }
                  else if (settingValue.type() == long.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %ld\n",name,(Long)object);
                    }
                  }
                  else if (settingValue.type() == Long.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %ld\n",name,(Long)object);
                    }
                  }
                  else if (settingValue.type() == boolean.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,(Boolean)object ? "yes" : "no");
                    }
                  }
                  else if (settingValue.type() == Boolean.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,(Boolean)object ? "yes" : "no");
                    }
                  }
                  else if (settingValue.type() == String.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.escape((String)object));
                    }
                  }
                  else if (settingValue.type().isEnum())
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,((Enum)object).toString());
                    }
                  }
                  else if (settingValue.type() == EnumSet.class)
                  {
                    for (Object object : (List)field.get(null))
                    {
                      output.printf("%s = %s\n",name,StringUtils.join((EnumSet)object,","));
                    }
                  }
                  else
                  {
Dprintf.dprintf("List without value adapter %s",settingValue.type());
                  }
                }
                else
                {
                  // primitiv type
                  if      (SettingValueAdapter.class.isAssignableFrom(settingValue.type()))
                  {
                    // instantiate config adapter class
                    SettingValueAdapter settingValueAdapter;
                    Class enclosingClass = settingValue.type().getEnclosingClass();
                    if (enclosingClass == Settings.class)
                    {
                      Constructor constructor = settingValue.type().getDeclaredConstructor(Settings.class);
                      settingValueAdapter = (SettingValueAdapter)constructor.newInstance(new Settings());
                    }
                    else
                    {
                      settingValueAdapter = (SettingValueAdapter)settingValue.type().newInstance();
                    }

                    // convert to string
                    String value = (String)settingValueAdapter.toString(field.get(null));
                    output.printf("%s = %s\n",name,value);
                  }
                  else if (type == int.class)
                  {
                    int value = field.getInt(null);
                    output.printf("%s = %d\n",name,value);
                  }
                  else if (type == Integer.class)
                  {
                    int value = (Integer)field.get(null);
                    output.printf("%s = %d\n",name,value);
                  }
                  else if (type == long.class)
                  {
                    long value = field.getLong(null);
                    output.printf("%s = %ld\n",name,value);
                  }
                  else if (type == Long.class)
                  {
                    long value = (Long)field.get(null);
                    output.printf("%s = %ld\n",name,value);
                  }
                  else if (type == boolean.class)
                  {
                    boolean value = field.getBoolean(null);
                    output.printf("%s = %s\n",name,value ? "yes" : "no");
                  }
                  else if (type == Boolean.class)
                  {
                    boolean value = (Boolean)field.get(null);
                    output.printf("%s = %s\n",name,value ? "yes" : "no");
                  }
                  else if (type == String.class)
                  {
                    String value = (type != null) ? (String)field.get(null) : settingValue.defaultValue();
                    output.printf("%s = %s\n",name,StringUtils.escape(value));
                  }
                  else if (type.isEnum())
                  {
                    Enum value = (Enum)field.get(null);
                    output.printf("%s = %s\n",name,value.toString());
                  }
                  else if (type == EnumSet.class)
                  {
                    EnumSet enumSet = (EnumSet)field.get(null);
                    output.printf("%s = %s\n",name,StringUtils.join(enumSet,","));
                  }
                  else
                  {
Dprintf.dprintf("field.getType()=%s",type);
                  }
                }
              }
              catch (Exception exception)
              {
Dprintf.dprintf("exception=%s",exception);
exception.printStackTrace();
              }
              }
            }
            else if (annotation instanceof SettingComment)
            {
              SettingComment settingComment = (SettingComment)annotation;

              for (String line : settingComment.text())
              {
                if (!line.isEmpty())
                {
                  output.printf("# %s\n",line);
                }
                else
                {
                  output.printf("\n");
                }
              }
            }
          }
        }
      }

      // close file
      output.close();

      // save last modified time
      lastModified = file.lastModified();
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

  /** save program settings
   * @param fileName settings file name
   */
  public static void save(String fileName)
  {
    save(new File(fileName));
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
    return (lastModified != 0L) && (new File(DEFAULT_BARCONTROL_CONFIG_FILE_NAME).lastModified() > lastModified);
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

  /** get all setting classes
   * @return classes array
   */
  protected static Class[] getSettingClasses()
  {
    // get all setting classes
    ArrayList<Class> classList = new ArrayList<Class>();

    classList.add(Settings.class);
    for (Class clazz : Settings.class.getDeclaredClasses())
    {
//Dprintf.dprintf("c=%s",clazz);
      classList.add(clazz);
    }

    return classList.toArray(new Class[classList.size()]);
  }

  /** unique add element to int array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static int[] addArrayUniq(int[] array, int n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to int array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Integer[] addArrayUniq(Integer[] array, int n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to long array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static long[] addArrayUniq(long[] array, long n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to long array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Long[] addArrayUniq(Long[] array, long n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to long array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static boolean[] addArrayUniq(boolean[] array, boolean n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to long array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Boolean[] addArrayUniq(Boolean[] array, boolean n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to string array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static String[] addArrayUniq(String[] array, String string)
  {
    int z = 0;
    while ((z < array.length) && !array[z].equals(string))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = string;
    }

    return array;
  }

  /** unique add element to enum array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static Enum[] addArrayUniq(Enum[] array, Enum n)
  {
    int z = 0;
    while ((z < array.length) && (array[z] != n))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to enum set array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static EnumSet[] addArrayUniq(EnumSet[] array, EnumSet n)
  {
    int z = 0;
    while ((z < array.length) && (array[z].equals(n)))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to object array
   * @param array array
   * @param object element
   * @param settingAdapter setting adapter (use equals() function)
   * @return extended array or array
   */
  private static Object[] addArrayUniq(Object[] array, Object object, SettingValueAdapter settingValueAdapter)
  {
    int z = 0;
    while ((z < array.length) && !settingValueAdapter.equals(array[z],object))
    {
      z++;
    }
    if (z >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = object;
    }

    return array;
  }
}

/* end of file */
