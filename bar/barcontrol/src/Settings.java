/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Settings.java,v $
* $Revision: 1.1 $
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
  static final String DEFAULT_BARCONTROL_CONFIG_FILE_NAME = System.getProperty("user.home")+File.separator+".bar"+File.separator+"barcontrol.cfg";

  // --------------------------- variables --------------------------------

  public static String serverName     = null;
  public static String serverPassword = null;
  public static int    serverPort     = 0;
  public static int    serverTLSPort  = 0;

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

          if      (StringParser.parse(line,"server = %S",data,StringParser.QUOTE_CHARS))
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
//else {  System.err.println("BARControl.java"+", "+6090+": "+line); }
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
