/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Options.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: command line options functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.reflect.Field;
import java.util.Locale;

/****************************** Classes ********************************/

/** options
 */
public class Options
{
  // --------------------------- constants --------------------------------
  public enum Types
  {
    NONE,
    STRING,
    INTEGER,
    FLOAT,
    DOUBLE,
    BOOLEAN,
    ENUMERATION,
    INCREMENT,
    SPECIAL,
  };

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** get default value string from enumeration set
   * @param enumeration enumeration set
   * @param defaultValue default value
   * @return default value string
   */
  public static String defaultValue(OptionEnumeration[] enumeration, Object defaultValue)
  {
    for (OptionEnumeration optionEnumeration : enumeration)
    {
      if (defaultValue.equals(optionEnumeration.value))
      {
        return optionEnumeration.name;
      }
    }

    return "";
  }

  /** parse an option
   * @param args arguments array
   * @param index current index in arguments array
   * @return new index arguments array or -1 on error
   */
  public static int parse(Option[] options, String args[], int index, Class settings)
  {
    for (Option option : options)
    {
      Option foundOption = null;
      String value       = null;
      int    n           = 0;
//Dprintf.dprintf("op=%s\n",option);
      if      (args[index].startsWith(option.name+"="))
      {
        foundOption = option;
        value       = args[index].substring(option.name.length()+1);
        n           = 1;
      }
      else if (args[index].startsWith(option.shortName+"="))
      {
        foundOption = option;
        value       = args[index].substring(option.shortName.length()+1);
        n           = 1;
      }
      if      (args[index].equals(option.name))
      {
        foundOption = option;
        if ((option.type != Options.Types.BOOLEAN) && (option.type != Options.Types.INCREMENT))
        {
          if (index+1 >= args.length)
          {
            printError("Value expected for option '%s'",option.name);
            System.exit(1);
          }
          value = args[index+1];
          n     = 2;
        }
        else
        {
          value = "1";
          n     = 1;
        }
      }
      else if (args[index].equals(option.shortName))
      {
        foundOption = option;
        if ((option.type != Options.Types.BOOLEAN) && (option.type != Options.Types.INCREMENT))
        {
          if (index+1 >= args.length)
          {
            printError("Value expected for option '%s'",option.shortName);
            System.exit(1);
          }
          value = args[index+1];
          n     = 2;
        }
        else
        {
          value = "1";
          n     = 1;
        }
      }

      if (foundOption != null)
      {
        try
        {
          if (foundOption.fieldName != null)
          {
            Field field = settings.getField(foundOption.fieldName);
            switch (foundOption.type)
            {
              case INTEGER:
                try
                {
                  field.setInt(null,Integer.parseInt(value));
                }
                catch (NumberFormatException exception)
                {
                  printError("Invalid number '%s' for option %s",value,option.name);
                  System.exit(1);
                }
                break;
              case STRING:
                field.set(null,value);
                break;
              case FLOAT:
                try
                {
                  field.setFloat(null,Float.parseFloat(value));
                }
                catch (NumberFormatException exception)
                {
                  printError("Invalid number '%s' for option %s",value,option.name);
                  System.exit(1);
                }
                break;
              case DOUBLE:
                try
                {
                  field.setDouble(null,Double.parseDouble(value));
                }
                catch (NumberFormatException exception)
                {
                  printError("Invalid number '%s' for option %s",value,option.name);
                  System.exit(1);
                }
                break;
              case BOOLEAN:
                field.setBoolean(null,
                                    value.equals("1") 
                                 || value.equalsIgnoreCase("true")
                                 || value.equalsIgnoreCase("yes")
                                 || value.equalsIgnoreCase("on")
                                );
                break;
              case ENUMERATION:
                boolean foundFlag = false;
                for (OptionEnumeration optionEnumeration : foundOption.enumeration)
                {
                  if (value.equalsIgnoreCase(optionEnumeration.name))
                  {
                    field.set(null,optionEnumeration.value);
                    foundFlag = true;
                    break;
                  }
                }
                if (!foundFlag)
                {
                  printError("Unknown value '%s' for option %s",value,option.name);
                  System.exit(1);
                }
                break;
              case INCREMENT:
                try
                {
                  field.setInt(null,field.getInt(null)+Integer.parseInt(value));
                }
                catch (NumberFormatException exception)
                {
                  printError("Invalid number '%s'",value);
                  System.exit(1);
                }
                break;
              case SPECIAL:
                foundOption.special.parse(value,field.get(null));
                break;
            }
          }

          return index+n;
        }
        catch (NoSuchFieldException exception)
        {
          internalError("Options field not found '%s'",exception.getMessage());
        }
        catch (IllegalAccessException exception)
        {
          internalError("Invalid access to options field '%s'",exception.getMessage());
        }
      }
    }

    return -1;
  }

  /** print error
   * @param format error text
   * @param args optional arguments
   */
  private static void printError(String format, Object... args)
  {
    System.err.println("ERROR: "+String.format(Locale.US,format,args));
  }

  /** print internal error and halt
   * @param format error text
   * @param args optional arguments
   */
  private static void internalError(String format, Object... args)
  {
    System.err.println("INTERNAL ERROR: "+String.format(Locale.US,format,args));
    System.exit(128);
  }
}

/* end of file */
