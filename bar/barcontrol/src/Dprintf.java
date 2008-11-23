/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Dprintf.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: dprintf functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/

/****************************** Classes ********************************/

public class Dprintf
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  static private int debugLevel = 0;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

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
      }
    }
  }

  /** output debug data
   * @param stackLevel stack level to trace back for getting file name, line number
   * @param level debug level
   * @param format printf-format string
   * @param args optional arguments
   */
  static private void dprintf(int stackLevel, int level, String format, Object... args)
  {
    if (debugLevel >= level)
    {
      StackTraceElement[] stackTrace = Thread.currentThread().getStackTrace();

      System.err.print(stackTrace[stackLevel].getFileName()+", "+stackTrace[stackLevel].getLineNumber()+": ");
      System.err.printf(format,args);
      System.err.println();
    }
  }

  /** output debug data
   * @param level debug level
   * @param format printf-format string
   * @param args optional arguments
   */
  static public void dprintf(int level, String format, Object... args)
  {
    dprintf(3,level,format,args);
  }

  /** output debug data
   * @param level debug level
   * @param object object
   */
  static public void dprintf(int level, Object object)
  {
    dprintf(3,level,"",object);
  }

  /** output debug data
   * @param format printf-format string
   * @param args optional arguments
   */
  static public void dprintf(String format, Object... args)
  {
    dprintf(3,0,format,args);
  }

  /** output debug data
   * @param object object
   */
  static public void dprintf(Object object)
  {
    dprintf(3,0,"",object);
  }

  /** output debug data
   */
  static public void dprintf()
  {
    dprintf(3,0,"");
  }
}

/* end of file */
