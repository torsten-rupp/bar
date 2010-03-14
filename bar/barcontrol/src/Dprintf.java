/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Dprintf.java,v $
* $Revision: 1.3 $
* $Author: torsten $
* Contents: dprintf functions with level and groups
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.util.HashMap;

import java.lang.reflect.Field;

/****************************** Classes ********************************/

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
  private static int                   debugLevel  = 0;
  private static HashMap<String,Group> debugGroups = new HashMap<String,Group>();

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
        System.err.println("Warning: cannot parse debug level '"+s+"' (error: "+exception.getMessage()+")");
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
//System.err.println("Dprintf.java"+", "+85+": "+debugGroups);
  }

  /** output debug data
   * @param stackLevel stack level to trace back for getting file name, line number
   * @param level debug level
   * @param group debug group or null
   * @param format printf-format string
   * @param args optional arguments
   */
  static private void output(int stackLevel, int level, Group group, String format, Object... args)
  {
    if (debugLevel >= level)
    {
      /* get stack trace */
      StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();

      /* detect group if DEBUG_GROUP is declared and accessable */
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

      /* output */
      if (group.enabled)
      {
        System.err.print(stackTrace[stackLevel].getFileName()+", "+stackTrace[stackLevel].getLineNumber()+": ");
        System.err.printf(format,args);
        System.err.println();
      }
    }
  }

  /** add debug group
   * @param name group name
   * @return group
   */
  static public Group addGroup(String name)
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
  static public void dprintf(int level, Group group, String format, Object... args)
  {
    output(3,level,group,format,args);
  }

  /** output debug data
   * @param level debug level
   * @param format printf-format string
   * @param args optional arguments
   */
  static public void dprintf(int level, String format, Object... args)
  {
    output(3,level,null,format,args);
  }

  /** output debug data
   * @param level debug level
   * @param object object
   */
  static public void dprintf(int level, Object object)
  {
    output(3,level,null,"%s",object);
  }

  /** output debug data
   * @param format printf-format string
   * @param args optional arguments
   */
  static public void dprintf(String format, Object... args)
  {
    output(3,0,null,format,args);
  }

  /** output debug data
   * @param object object
   */
  static public void dprintf(Object object)
  {
    output(3,0,null,"%s",object);
  }

  /** output debug data
   */
  static public void dprintf()
  {
    output(3,0,GROUP_ANY,"");
  }

  /** print a stack trace
   * @param stackLevel stack level
   * @param prefix line prefix
   */
  static private void printStackTrace(int stackLevel, String prefix)
  {
    StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();

    for (int z = stackLevel; z < stackTrace.length; z++)
    {
      System.err.println(prefix+stackTrace[z].getMethodName()+"(), "+stackTrace[z].getFileName()+":"+stackTrace[z].getLineNumber()+": ");
    }
  }

  /** print a stack trace
   * @param prefix line prefix
   */
  static public void printStackTrace(String prefix)
  {
    printStackTrace(3,prefix);
  }

  /** print a stack trace
   */
  static public void printStackTrace()
  {
    printStackTrace(3,"  ");
  }
}

/* end of file */
