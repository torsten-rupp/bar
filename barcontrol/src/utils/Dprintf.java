/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: dprintf functions with level and groups
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.io.PrintStream;

import java.util.HashMap;

import java.lang.reflect.Field;

/****************************** Classes ********************************/

/** debug print functions
 */
public class Dprintf
{
  /** debug group
   */
  static class Group
  {
    protected String  name;
    protected boolean enabled;

    /** create debug group
     * @param naem group name
     * @param enabled TRUE if group is enabled
     */
    Group(String name, boolean enabled)
    {
      this.name    = name;
      this.enabled = enabled;
    }

    /** create debug group
     * @param name group name
     */
    Group(String name)
    {
      this(name,false);
    }

    /** convert to string
     * @return string
     */
    public String toString()
    {
      return "{Group "+name+", "+enabled+"}";
    }
  }

  // --------------------------- constants --------------------------------
  /** any debug group (always enabled)
   */
  public static final Group GROUP_ANY;

  // --------------------------- variables --------------------------------
  private static PrintStream           outputStream = System.err;
  private static int                   debugLevel   = 0;
  private static HashMap<String,Group> debugGroups  = new HashMap<String,Group>();
  private static boolean               timeStampsEnabled = true;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** initialize debug level and groups
   */
  static
  {
    String s;

    // get debug level
    s = System.getProperty("DEBUG_LEVEL");
    if (s != null)
    {
      try
      {
        debugLevel = Math.max(0,Integer.parseInt(s));
      }
      catch (NumberFormatException exception)
      {
        // ignored
        outputStream.println("Warning: cannot parse debug level '"+s+"' (error: "+exception.getMessage()+")");
      }
    }

    // get debug groups
    GROUP_ANY = new Dprintf.Group("ANY",true);
    debugGroups.put("ANY",GROUP_ANY);
    s = System.getProperty("DEBUG_GROUPS");
    if (s != null)
    {
      for (String name : s.split(","))
      {
        debugGroups.put(name,new Group(name,true));
      }
    }
//outputStream.println("Dprintf.java"+", "+85+": "+debugGroups);
  }

  /** set output stream
   * @param outputStream output stream
   */
  public static void setOutputStream(PrintStream outputStream)
  {
    Dprintf.outputStream = outputStream;
  }

  /** add debug group
   * @param name group name
   * @return group
   */
  public static Group addGroup(String name)
  {
    Group group;

    group = debugGroups.get(name);
    if (group == null)
    {
      group = new Group(name);
      debugGroups.put(name,group);
    }

    return group;
  }

  /** output debug data
   * @param level debug level
   * @param group debug group
   * @param format printf-format string
   * @param args optional arguments
   */
  public static void dprintf(int level, Group group, String format, Object... args)
  {
    printOutput(3,level,group,format,args);
  }

  /** output debug data
   * @param level debug level
   * @param format printf-format string
   * @param args optional arguments
   */
  public static void dprintf(int level, String format, Object... args)
  {
    printOutput(3,level,(Group)null,format,args);
  }

  /** output debug data
   * @param level debug level
   * @param object object
   */
  public static void dprintf(int level, Object object)
  {
    printOutput(3,level,(Group)null,"%s",object);
  }

  /** output debug data
   * @param format printf-format string
   * @param args optional arguments
   */
  public static void dprintf(String format, Object... args)
  {
    printOutput(3,0,(Group)null,format,args);
  }

  /** output debug data
   * @param object object
   */
  public static void dprintf(Object object)
  {
    printOutput(3,0,(Group)null,"%s",object);
  }

  /** output debug data
   */
  public static void dprintf()
  {
    printOutput(3,0,GROUP_ANY,"");
  }

  /** dump memory
   */
  public static void dumpMemory(byte[] data)
  {
    StringBuilder buffer = new StringBuilder();
    for (int i = 0; i < data.length; i++)
    {
      if (buffer.length() > 0) buffer.append(' ');
      buffer.append(String.format("%02x",data[i]));
    }
    printOutput(3,0,(Group)null,"%s",buffer.toString());
  }

  /** output debug data and halt
   * @param format printf-format string
   * @param args optional arguments
   */
  public static void halt(String format, Object... args)
  {
    printOutput(3,0,GROUP_ANY,"HALT: "+format,args);
    printStackTrace();
    System.exit(1);
  }

  /** output debug data and halt
   * @param format printf-format string
   * @param args optional arguments
   */
  public static void halt(Object object)
  {
    printOutput(3,0,GROUP_ANY,"HALT: %s",object);
    printStackTrace();
    System.exit(1);
  }

  /** output debug data and halt
   * @param format printf-format string
   * @param args optional arguments
   */
  public static void halt()
  {
    printOutput(3,0,GROUP_ANY,"HALT");
    printStackTrace();
    System.exit(1);
  }

  // -------------------------------------------------------------------

  /** output debug data
   * @param stackLevel stack level to trace back for getting file name, line number
   * @param level debug level
   * @param group debug group or null
   * @param format printf-format string
   * @param args optional arguments
   */
  private static void printOutput(int stackLevel, int level, Group group, String format, Object... args)
  {
    if (debugLevel >= level)
    {
      // get stack trace
      StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();

      // detect group if DEBUG_GROUP is declared and accessable
      if (group == null)
      {
        try
        {
          Class clazz = Class.forName(stackTrace[stackLevel].getClassName());
          Field field = clazz.getDeclaredField("DEBUG_GROUP");
          group = (Group)field.get(stackTrace[stackLevel]);
        }
        catch (ClassNotFoundException exception)
        {
          group = GROUP_ANY;
        }
        catch (NoSuchFieldException exception)
        {
          group = GROUP_ANY;
        }
        catch (IllegalAccessException exception)
        {
          group = GROUP_ANY;
        }
      }

      // output
      if (group.enabled)
      {
        if (timeStampsEnabled)
        {
          outputStream.print(String.format("%8d ",System.currentTimeMillis()));
        }
        outputStream.print(stackTrace[stackLevel].getFileName()+", "+stackTrace[stackLevel].getLineNumber()+", "+stackTrace[stackLevel].getMethodName()+": ");
        outputStream.printf(format,args);
        outputStream.println();
      }
    }
  }

  /** print a stack trace
   * @param stackLevel stack level
   * @param prefix line prefix
   */
  private static void printStackTrace(int stackLevel, String prefix)
  {
    StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();

    for (int z = stackLevel; z < stackTrace.length; z++)
    {
      outputStream.println(prefix+stackTrace[z].getMethodName()+"(), "+stackTrace[z].getFileName()+":"+stackTrace[z].getLineNumber()+": ");
    }
  }

  /** output stack trace
   * @param stackLevel stack level to trace back
   */
  private static void printStackTrace(int stackLevel)
  {
    StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();
    for (int i = stackLevel+1; i < stackTrace.length; i++)
    {
      outputStream.println("  "+stackTrace[i]);
    }
  }

  /** print a stack trace
   * @param prefix line prefix
   */
  public static void printStackTrace(String prefix)
  {
    printStackTrace(3,prefix);
  }

  /** print a stack trace
   */
  public static void printStackTrace()
  {
    printStackTrace(3,"  ");
  }
}

/* end of file */
