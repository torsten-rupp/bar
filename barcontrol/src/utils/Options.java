/***********************************************************************\
*
* Contents: command line options functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.reflect.Field;
import java.text.NumberFormat;
import java.text.ParseException;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.Locale;
import java.util.EnumSet;
import java.util.HashSet;

/****************************** Classes ********************************/

/** options
 */
public class Options
{
  // -------------------------- constants -------------------------------
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

  // -------------------------- variables -------------------------------

  // ----------------------- native functions ---------------------------

  // --------------------------- methods --------------------------------

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
      else if (args[index].equals(option.name))
      {
        foundOption = option;
        if ((option.type != Options.Types.BOOLEAN) && (option.type != Options.Types.INCREMENT))
        {
          if      ((index+1 < args.length) && !args[index+1].startsWith("-"))
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
            throw new IllegalArgumentException("Expected value for option '"+option.name+"'");
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
          if      ((index+1 < args.length) && !args[index+1].startsWith("-"))
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
            throw new IllegalArgumentException("Expected value for option '"+option.shortName+"'");
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
            Class type = field.getType();

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
                    int value = Integer.parseInt(string)*factor;

                    if      (type.isArray())
                    {
                      field.set(null,addArrayUniq((int[])field.get(null),value));
                    }
                    else if (type == ArrayList.class)
                    {
                      ArrayList<Integer> arrayList = (ArrayList<Integer>)field.get(null);
                      arrayList.add(value);
                    }
                    else if (type == LinkedHashSet.class)
                    {
                      LinkedHashSet<Integer> hashSet = (LinkedHashSet<Integer>)field.get(null);
                      hashSet.add(value);
                    }
                    else if (type == HashSet.class)
                    {
                      HashSet<Integer> hashSet = (HashSet<Integer>)field.get(null);
                      hashSet.add(value);
                    }
                    else
                    {
                      field.setInt(null,value);
                    }
                  }
                  catch (NumberFormatException exception)
                  {
                    throw new IllegalArgumentException("Invalid number '"+string+"' for option '"+option.name+"'");
                  }
                }
                break;
              case STRING:
                if      (type.isArray())
                {
                  field.set(null,addArrayUniq((String[])field.get(null),string));
                }
                else if (type == ArrayList.class)
                {
                  ArrayList<String> arrayList = (ArrayList<String>)field.get(null);
                  arrayList.add(string);
                }
                else if (type == LinkedHashSet.class)
                {
                  LinkedHashSet<String> hashSet = (LinkedHashSet<String>)field.get(null);
                  hashSet.add(string);
                }
                else if (type == HashSet.class)
                {
                  HashSet<String> hashSet = (HashSet<String>)field.get(null);
                  hashSet.add(string);
                }
                else
                {
                  field.set(null,string);
                }
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
                    float value = NumberFormat.getInstance().parse(string).floatValue()*factor;

                    if      (type.isArray())
                    {
                      field.set(null,addArrayUniq((float[])field.get(null),value));
                    }
                    else if (type == ArrayList.class)
                    {
                      ArrayList<Float> arrayList = (ArrayList<Float>)field.get(null);
                      arrayList.add(value);
                    }
                    else if (type == LinkedHashSet.class)
                    {
                      LinkedHashSet<Float> hashSet = (LinkedHashSet<Float>)field.get(null);
                      hashSet.add(value);
                    }
                    else if (type == HashSet.class)
                    {
                      HashSet<Float> hashSet = (HashSet<Float>)field.get(null);
                      hashSet.add(value);
                    }
                    else
                    {
                      field.setFloat(null,value);
                    }
                  }
                  catch (ParseException exception)
                  {
                    throw new IllegalArgumentException("Invalid number '"+string+"' for option '"+option.name+"'");
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
                    double value = NumberFormat.getInstance().parse(string).doubleValue()*factor;

                    if      (type.isArray())
                    {
                      field.set(null,addArrayUniq((double[])field.get(null),value));
                    }
                    else if (type == ArrayList.class)
                    {
                      ArrayList<Double> arrayList = (ArrayList<Double>)field.get(null);
                      arrayList.add(value);
                    }
                    else if (type == LinkedHashSet.class)
                    {
                      LinkedHashSet<Double> hashSet = (LinkedHashSet<Double>)field.get(null);
                      hashSet.add(value);
                    }
                    else if (type == HashSet.class)
                    {
                      HashSet<Double> hashSet = (HashSet<Double>)field.get(null);
                      hashSet.add(value);
                    }
                    else
                    {
                      field.setDouble(null,value);
                    }
                  }
                  catch (ParseException exception)
                  {
                    throw new IllegalArgumentException("Invalid number '"+string+"' for option '"+option.name+"'");
                  }
                }
                break;
              case BOOLEAN:
                {
                  boolean value =    string.equals("1")
                                  || string.equalsIgnoreCase("true")
                                  || string.equalsIgnoreCase("yes")
                                  || string.equalsIgnoreCase("on");
                  if      (type.isArray())
                  {
                    field.set(null,addArrayUniq((boolean[])field.get(null),value));
                  }
                  else if (type == ArrayList.class)
                  {
                    ArrayList<Boolean> arrayList = (ArrayList<Boolean>)field.get(null);
                    arrayList.add(value);
                  }
                  else if (type == LinkedHashSet.class)
                  {
                    LinkedHashSet<Boolean> hashSet = (LinkedHashSet<Boolean>)field.get(null);
                    hashSet.add(value);
                  }
                  else if (type == HashSet.class)
                  {
                    HashSet<Boolean> hashSet = (HashSet<Boolean>)field.get(null);
                    hashSet.add(value);
                  }
                  else
                  {
                    field.setBoolean(null,value);
                  }
                }
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

                      if      (type.isArray())
                      {
                        field.set(null,addArrayUniq((Enum[])field.get(null),value));
                      }
                      else if (type == ArrayList.class)
                      {
                        ArrayList<Enum> arrayList = (ArrayList<Enum>)field.get(null);
                        arrayList.add(value);
                      }
                      else if (type == LinkedHashSet.class)
                      {
                        LinkedHashSet<Enum> hashSet = (LinkedHashSet<Enum>)field.get(null);
                        hashSet.add(value);
                      }
                      else if (type == HashSet.class)
                      {
                        HashSet<Enum> hashSet = (HashSet<Enum>)field.get(null);
                        hashSet.add(value);
                      }
                      else
                      {
                        field.set(null,value);
                      }
                      foundFlag = true;
                    }
                    catch (Exception exception)
                    {
                      // ignored
                    }
                  }
                  else
                  {
                    throw new InternalError("unsupported enumeration type '"+foundOption.enumeration.getClass()+"' for option '"+option.name+"'");
                  }
                  if (!foundFlag)
                  {
                    throw new IllegalArgumentException("Unknown value '"+string+"' for option '"+option.name+"'");
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
                    throw new IllegalArgumentException("Unknown value '"+string+"' for option '"+option.name+"'");
                  }
                }

                if      (type.isArray())
                {
                  field.set(null,addArrayUniq((EnumSet[])field.get(null),enumSet));
                }
                else if (type == ArrayList.class)
                {
                  ArrayList<EnumSet> arrayList = (ArrayList<EnumSet>)field.get(null);
                  arrayList.add(enumSet);
                }
                else if (type == LinkedHashSet.class)
                {
                  LinkedHashSet<EnumSet> hashSet = (LinkedHashSet<EnumSet>)field.get(null);
                  hashSet.add(enumSet);
                }
                else if (type == HashSet.class)
                {
                  HashSet<EnumSet> hashSet = (HashSet<EnumSet>)field.get(null);
                  hashSet.add(enumSet);
                }
                else
                {
                  field.set(null,enumSet);
                }
                break;
              case INCREMENT:
                try
                {
                  field.setInt(null,field.getInt(null)+Integer.parseInt(string));
                }
                catch (NumberFormatException exception)
                {
                  throw new IllegalArgumentException("Invalid number '"+string+"'");
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
          throw new InternalError("Options field not found '"+exception.getMessage()+"'");
        }
        catch (IllegalAccessException exception)
        {
          throw new InternalError("Invalid access to options field '"+exception.getMessage()+"'");
        }
      }
    }

    return -1;
  }

  /** unique add element to int array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static int[] addArrayUniq(int[] array, int n)
  {
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
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
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
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
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
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
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to float array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static float[] addArrayUniq(float[] array, float n)
  {
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to double array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static double[] addArrayUniq(double[] array, double n)
  {
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to double array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Double[] addArrayUniq(Double[] array, double n)
  {
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to boolean array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static boolean[] addArrayUniq(boolean[] array, boolean n)
  {
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** unique add element to boolean array
   * @param array array
   * @param n element
   * @return extended array or array
   */
  private static Boolean[] addArrayUniq(Boolean[] array, boolean n)
  {
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }

  /** add element to string array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static String[] addArray(String[] array, String string)
  {
    array = Arrays.copyOf(array,array.length+1);
    array[array.length-1] = string;

    return array;
  }

  /** unique add element to string array
   * @param array array
   * @param string element
   * @return extended array or array
   */
  private static String[] addArrayUniq(String[] array, String string)
  {
    int i = 0;
    while ((i < array.length) && !array[i].equals(string))
    {
      i++;
    }
    if (i >= array.length)
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
    int i = 0;
    while ((i < array.length) && (array[i] != n))
    {
      i++;
    }
    if (i >= array.length)
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
    int i = 0;
    while ((i < array.length) && (array[i].equals(n)))
    {
        i++;
    }
    if (i >= array.length)
    {
      array = Arrays.copyOf(array,array.length+1);
      array[array.length-1] = n;
    }

    return array;
  }
}

/* end of file */
