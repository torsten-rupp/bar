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
import java.io.FileReader;
import java.io.BufferedReader;
import java.io.IOException;

/****************************** Classes ********************************/

public class Settings
{
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
    INCREMENTAL;

    /** convert to string
     * @return string
     */
    public String toString()
    {
      switch (this)
      {
        case NORMAL:      return "normal";
        case FULL:        return "full";
        case INCREMENTAL: return "incremental";
        default:          return "normal";
      }
    }
  };

  // --------------------------- variables --------------------------------

  // program settings
  public static boolean          pauseCreateFlag                = true;
  public static boolean          pauseStorageFlag               = false;
  public static boolean          pauseRestoreFlag               = true;
  public static boolean          pauseIndexUpdateFlag           = false;

  // server settings
  public static String           serverName                     = DEFAULT_SERVER_NAME;
  public static String           serverPassword                 = null;
  public static int              serverPort                     = DEFAULT_SERVER_PORT;
  public static int              serverTLSPort                  = DEFAULT_SERVER_TLS_PORT;
  public static String           serverKeyFileName              = null;

  public static String           selectedJobName                = null;
  public static boolean          loginDialogFlag                = false;

  // commands and data
  public static String           runJobName                     = null;
  public static ArchiveTypes     archiveType                    = ArchiveTypes.NORMAL;
  public static String           abortJobName                   = null;
  public static String           indexAddStorageName            = null;
  public static String           indexRemoveStorageName         = null;
  public static String           indexStorageListPattern        = null;
  public static String           indexEntriesListPattern        = null;
  public static int              pauseTime                      = 0;
  public static boolean          pingFlag                       = false;
  public static boolean          suspendFlag                    = false;
  public static boolean          continueFlag                   = false;
  public static boolean          listFlag                       = false;

  // debug
  public static boolean          debugFlag                      = false;
  public static boolean          serverDebugFlag                = false;

  // help
  public static boolean          helpFlag                       = false;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** load program settings
   * @param fileName file nam
   * @return Errors.NONE or error code
   */
  public static int load(String fileName)
  {
    /* read $HOME/.bar/.barcontrol.cfg */
    File barControlConfig = new File(fileName);
    if (barControlConfig.exists())
    {
      BufferedReader input = null;
      try
      {
        input = new BufferedReader(new FileReader(barControlConfig));
        String line;
        while ((line = input.readLine()) != null)
        {
          Object[] data = new Object[1];

          // program
          if      (StringParser.parse(line,"pause-create = %y",data,StringParser.QUOTE_CHARS))
          {
            pauseCreateFlag = (Boolean)data[0];
          }
          else if (StringParser.parse(line,"pause-storage = %y",data,StringParser.QUOTE_CHARS))
          {
            pauseStorageFlag = (Boolean)data[0];
          }
          else if (StringParser.parse(line,"pause-restore = %y",data,StringParser.QUOTE_CHARS))
          {
            pauseRestoreFlag = (Boolean)data[0];
          }
          else if (StringParser.parse(line,"pause-index-update = %y",data,StringParser.QUOTE_CHARS))
          {
            pauseIndexUpdateFlag = (Boolean)data[0];
          }

          // server
          else if (StringParser.parse(line,"server = %S",data,StringParser.QUOTE_CHARS))
          {
            serverName = (String)data[0];
          }
          else if (StringParser.parse(line,"server-password = %S",data,StringParser.QUOTE_CHARS))
          {
            serverPassword = (String)data[0];
          }
          else if (StringParser.parse(line,"server-port = %d",data,StringParser.QUOTE_CHARS))
          {
            serverPort = (Integer)data[0];
          }
          else if (StringParser.parse(line,"server-tls-port = %d",data,StringParser.QUOTE_CHARS))
          {
            serverTLSPort = (Integer)data[0];
          }
        }
        input.close();
      }
      catch (IOException exception)
      {
      }
      finally
      {
        try
        {
          if (input != null) input.close();
        }
        catch (IOException exception)
        {
        }
      }
    }

    return Errors.NONE;
  }

  /** load program settings
   * @param fileName file nam
   * @return Errors.NONE or error code
   */
  public static int load()
  {
    return load(DEFAULT_BARCONTROL_CONFIG_FILE_NAME);
  }

  /** save program settings
   * @param fileName file nam
   * @return Errors.NONE or error code
   */
  public static int save(String fileName)
  {
    return Errors.NONE;
  }

  /** save program settings
   * @return Errors.NONE or error code
   */
  public static int save()
  {
    return load(DEFAULT_BARCONTROL_CONFIG_FILE_NAME);
  }
}

/* end of file */
