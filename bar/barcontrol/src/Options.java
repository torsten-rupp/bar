/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: command line options functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.reflect.Field;
import java.text.NumberFormat;
import java.text.ParseException;
import java.util.Locale;
import java.util.EnumSet;

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
    ENUMERATION_SET,
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
      String string      = null;
      int    n           = 0;
//Dprintf.dprintf("op=%s\n",option);
      if      (args[index].startsWith(option.name+"="))
      {
        foundOption = option;
        string      = args[index].substring(option.name.length()+1);
        n           = 1;
      }
      else if (args[index].startsWith(option.shortName+"="))
      {
        foundOption = option;
        string      = args[index].substring(option.shortName.length()+1);
        n           = 1;
      }
      if      (args[index].equals(option.name))
      {
        foundOption = option;
        if ((option.type != Options.Types.BOOLEAN) && (option.type != Options.Types.INCREMENT))
        {
          if      (index+1 < args.length)
          {
            string = args[index+1];
            n      = 2;
          }
          else if (option.defaultValue != null)
          {
            string = option.defaultValue;
            n      = 1;
          }
          else
          {
            printError("Value expected for option '%s'",option.name);
            System.exit(ExitCodes.FAIL);
          }
        }
        else
        {
          string = "1";
          n      = 1;
        }
      }
      else if (args[index].equals(option.shortName))
      {
        foundOption = option;
        if ((option.type != Options.Types.BOOLEAN) && (option.type != Options.Types.INCREMENT))
        {
          if      (index+1 < args.length)
          {
            string = args[index+1];
            n      = 2;
          }
          else if (option.defaultValue != null)
          {
            string = option.defaultValue;
            n      = 1;
          }
          else
          {
            printError("Value expected for option '%s'",option.shortName);
            System.exit(ExitCodes.FAIL);
          }
        }
        else
        {
          string = "1";
          n      = 1;
        }
      }

      if (foundOption != null)
      {
        try
        {
          if (foundOption.fieldName != null)
          {
            // find class
            String fieldNames[]  = foundOption.fieldName.split("\\.");
            Class clazz          = settings;
            Class innerClasses[] = settings.getDeclaredClasses();
            for (String fieldName : fieldNames)
            {
              for (Class innerClass : innerClasses)
              {
                if (fieldName.equals(innerClass.getSimpleName()))
                {
                  clazz        = innerClass;
                  innerClasses = innerClass.getDeclaredClasses();
                  break;
                }
              }
            }

            // get field
            Field field = clazz.getField(fieldNames[fieldNames.length-1]);

            switch (foundOption.type)
            {
              case INTEGER:
                {
                  // get unit
                  int factor = 1;
                  if (foundOption.units != null)
                  {
                    for (int i = 0; i < foundOption.units.length; i += 2)
                    {
                      if (string.endsWith((String)foundOption.units[i+0]))
                      {
                        string = string.substring(0,string.length()-((String)(foundOption.units[i+0])).length());
                        factor = (Integer)foundOption.units[i+1];
                        break;
                      }
                    }
                  }

                  // parse value
                  try
                  {
                    field.setInt(null,Integer.parseInt(string)*factor);
                  }
                  catch (NumberFormatException exception)
                  {
                    printError("Invalid number '%s' for option %s",string,option.name);
                    System.exit(ExitCodes.FAIL);
                  }
                }
                break;
              case STRING:
                field.set(null,string);
                break;
              case FLOAT:
                {
                  // get unit
                  float factor = 1.0F;
                  if (foundOption.units != null)
                  {
                    for (int i = 0; i < foundOption.units.length; i += 2)
                    {
                      if (string.endsWith((String)foundOption.units[i+0]))
                      {
                        string = string.substring(0,string.length()-((String)(foundOption.units[i+0])).length());
                        factor = (Float)foundOption.units[i+1];
                        break;
                      }
                    }
                  }

                  // parse value
                  try
                  {
                    field.setFloat(null,Float.parseFloat(string)*factor);
                  }
                  catch (NumberFormatException exception)
                  {
                    printError("Invalid number '%s' for option %s",string,option.name);
                    System.exit(ExitCodes.FAIL);
                  }
                }
                break;
              case DOUBLE:
                {
                  // get unit
                  double factor = 1.0D;
                  if (foundOption.units != null)
                  {
                    for (int i = 0; i < foundOption.units.length; i += 2)
                    {
                      if (string.endsWith((String)foundOption.units[i+0]))
                      {
                        string = string.substring(0,string.length()-((String)(foundOption.units[i+0])).length());
                        factor = (Double)foundOption.units[i+1];
                        break;
                      }
                    }
                  }

                  // parse value
                  try
                  {
                    field.setDouble(null,NumberFormat.getInstance().parse(string).doubleValue()*factor);
                  }
                  catch (ParseException exception)
                  {
                    printError("Invalid number '%s' for option %s",string,option.name);
                    System.exit(ExitCodes.FAIL);
                  }
                }
                break;
              case BOOLEAN:
                field.setBoolean(null,
                                    string.equals("1")
                                 || string.equalsIgnoreCase("true")
                                 || string.equalsIgnoreCase("yes")
                                 || string.equalsIgnoreCase("on")
                                );
                break;
              case ENUMERATION:
                {
                  boolean foundFlag = false;
                  if      (foundOption.enumeration instanceof OptionEnumeration[])
                  {
                    // option enumeration array
                    OptionEnumeration[] optionEnumerations = (OptionEnumeration[])foundOption.enumeration;
                    for (OptionEnumeration optionEnumeration : optionEnumerations)
                    {
                      if (string.equalsIgnoreCase(optionEnumeration.name))
                      {
                        field.set(null,optionEnumeration.value);
                        foundFlag = true;
                        break;
                      }
                    }
                  }
                  else if (foundOption.enumeration instanceof OptionEnum)
                  {
                    // enum
                    OptionEnum optionEnum = (OptionEnum)foundOption.enumeration;
                    try
                    {
                      Enum value = optionEnum.parse(string);
                      field.set(null,value);
                      foundFlag = true;
                    }
                    catch (Exception exception)
                    {
                      // ignored
                    }
                  }
                  else
                  {
                    printError("INTERNAL ERROR: unsupported enumerationt type '%s' for option %s",foundOption.enumeration.getClass(),option.name);
                    System.exit(ExitCodes.FAIL);
                  }
                  if (!foundFlag)
                  {
                    printError("Unknown value '%s' for option %s",string,option.name);
                    System.exit(ExitCodes.FAIL);
                  }
                }
                break;
              case ENUMERATION_SET:
                EnumSet enumSet = EnumSet.noneOf(foundOption.enumerationSetClass);
                for (String name : string.split("\\s*,\\s*"))
                {
                  boolean foundFlag = false;
                  if      (name.equalsIgnoreCase("ALL"))
                  {
                    enumSet = EnumSet.allOf(foundOption.enumerationSetClass);
                    foundFlag = true;
                  }
                  else if (name.equalsIgnoreCase("NONE"))
                  {
                    enumSet = EnumSet.noneOf(foundOption.enumerationSetClass);
                    foundFlag = true;
                  }
                  else
                  {
                    for (Enum enumeration : (EnumSet<?>)EnumSet.allOf(foundOption.enumerationSetClass))
                    {
                      if (name.equalsIgnoreCase(enumeration.name()))
                      {
                        enumSet.add(enumeration);
                        foundFlag = true;
                        break;
                      }
                    }
                  }
                  if (!foundFlag)
                  {
                    printError("Unknown value '%s' for option %s",name,option.name);
                    System.exit(ExitCodes.FAIL);
                  }
                }
                field.set(null,enumSet);
                break;
              case INCREMENT:
                try
                {
                  field.setInt(null,field.getInt(null)+Integer.parseInt(string));
                }
                catch (NumberFormatException exception)
                {
                  printError("Invalid number '%s'",string);
                  System.exit(ExitCodes.FAIL);
                }
                break;
              case SPECIAL:
                foundOption.special.parse(string,field.get(null));
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
    System.exit(ExitCodes.INTERNAL_ERROR);
  }
}

/* end of file */
