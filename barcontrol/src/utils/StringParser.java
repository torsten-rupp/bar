/***********************************************************************\
*
* Contents: String parser
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.Integer;
import java.lang.Long;
import java.lang.NumberFormatException;
import java.lang.String;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import java.text.NumberFormat;
import java.text.ParseException;

import java.util.HashMap;
import java.util.Locale;

/****************************** Classes ********************************/

/** map parser type
 */
class TypeMap extends HashMap<String,Class>
{
  /** create type map
   * @param data definition (tuples of <String,Class>)
   */
  TypeMap(Object... data)
  {
    assert (data.length%2) == 0;

    for (int i = 0; i < data.length; i+=2)
    {
      put((String)data[i+0],(Class)data[i+1]);
    }
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    StringBuilder buffer = new StringBuilder();

    buffer.append('{');
    for (String name : keySet())
    {
      if (buffer.length() > 1) buffer.append(", ");
      buffer.append(name);
      buffer.append(':');
      buffer.append(get(name));
    }
    buffer.append('}');

    return buffer.toString();
  }
}

/** map parser value
 */
class ValueMap extends HashMap<String,Object>
{
  /** create value map
   * @param name name
   * @param value value
   */
  ValueMap(String name, String value)
  {
    super();
    put(name,value);
  }

  /** create value map
   */
  ValueMap()
  {
    super();
  }

  /** get int value
   * @param name name
   * @param defaultValue default value
   * @return int value
   */
  public int getInt(String name, Integer defaultValue)
    throws IllegalArgumentException
  {
    Object object = get(name);
    if      (object != null)
    {
      if (object instanceof String)
      {
        String string = (String)object;
        try
        {
          if (string.startsWith("0x") || string.startsWith("0X"))
          {
            return Integer.parseInt(string.substring(2),16);
          }
          else
          {
            return Integer.parseInt(string);
          }
        }
        catch (NumberFormatException exception)
        {
          if (defaultValue != null)
          {
            return defaultValue;
          }
          else
          {
            throw new IllegalArgumentException(string,exception);
          }
        }
      }
      else
      {
        return (Integer)object;
      }
    }
    else if (defaultValue != null)
    {
      return defaultValue;
    }
    else
    {
      throw new IllegalArgumentException("unknown entry '"+name+"'");
    }
  }

  /** get int value
   * @param name name
   * @return int value
   */
  public int getInt(String name)
    throws IllegalArgumentException
  {
    return getInt(name,(Integer)null);
  }

  /** get long value
   * @param name name
   * @param defaultValue default value
   * @return int value
   */
  public long getLong(String name, Long defaultValue)
    throws IllegalArgumentException
  {
    Object object = get(name);
    if      (object != null)
    {
      if (object instanceof String)
      {
        String string = (String)object;
        try
        {
          if (string.startsWith("0x") || string.startsWith("0X"))
          {
            return Long.parseLong(string.substring(2),16);
          }
          else
          {
            return Long.parseLong(string);
          }
        }
        catch (NumberFormatException exception)
        {
          if (defaultValue != null)
          {
            return defaultValue;
          }
          else
          {
            throw new IllegalArgumentException(string,exception);
          }
        }
      }
      else
      {
        return (Long)object;
      }
    }
    else if (defaultValue != null)
    {
      return defaultValue;
    }
    else
    {
      throw new IllegalArgumentException("unknown entry '"+name+"'");
    }
  }

  /** get long value
   * @param name name
   * @return long value
   */
  public long getLong(String name)
    throws IllegalArgumentException
  {
    return getLong(name,(Long)null);
  }

  /** get double value
   * @param name name
   * @param defaultValue default value
   * @return double value
   */
  public double getDouble(String name, Double defaultValue)
    throws IllegalArgumentException
  {
    Object object = get(name);
    if      (object != null)
    {
      if (object instanceof String)
      {
        String string = (String)object;
        try
        {
          return NumberFormat.getInstance(Locale.ENGLISH).parse(string).doubleValue();
        }
        catch (ParseException exception)
        {
          if (defaultValue != null)
          {
            return defaultValue;
          }
          else
          {
            throw new IllegalArgumentException(string,exception);
          }
        }
      }
      else
      {
        return (Double)object;
      }
    }
    else if (defaultValue != null)
    {
      return defaultValue;
    }
    else
    {
      throw new IllegalArgumentException("unknown entry '"+name+"'");
    }
  }

  /** get double value
   * @param name name
   * @return double value
   */
  public double getDouble(String name)
    throws IllegalArgumentException
  {
    return getDouble(name,(Double)null);
  }

  /** get boolean value
   * @param name name
   * @param defaultValue default value
   * @return boolean value
   */
  public boolean getBoolean(String name, Boolean defaultValue)
    throws IllegalArgumentException
  {
    Object object = get(name);
    if      (object != null)
    {
      if (object instanceof String)
      {
        String string = (String)object;

        return    string.equalsIgnoreCase("yes")
               || string.equalsIgnoreCase("on")
               || string.equalsIgnoreCase("true")
               || string.equals("1");
      }
      else
      {
        return (Boolean)object;
      }
    }
    else if (defaultValue != null)
    {
      return defaultValue;
    }
    else
    {
      throw new IllegalArgumentException("unknown entry '"+name+"'");
    }
  }

  /** get boolean value
   * @param name name
   * @return boolean value
   */
  public boolean getBoolean(String name)
    throws IllegalArgumentException
  {
    return getBoolean(name,(Boolean)null);
  }


  /** get character value
   * @param name name
   * @param defaultValue default value
   * @return string value
   */
  public char getChar(String name, Character defaultValue)
    throws IllegalArgumentException
  {
    Object object = get(name);
    if      (object != null)
    {
      if (object instanceof String)
      {
        return ((String)object).charAt(0);
      }
      else
      {
        return (Character)get(name);
      }
    }
    else if (defaultValue != null)
    {
      return defaultValue;
    }
    else
    {
      throw new IllegalArgumentException("unknown entry '"+name+"'");
    }
  }

  /** get character value
   * @param name name
   * @return string value
   */
  public char getChar(String name)
    throws IllegalArgumentException
  {
    return getChar(name,(Character)null);
  }

  /** get string value
   * @param name name
   * @param defaultValue default value
   * @return string value
   */
  public String getString(String name, String defaultValue)
    throws IllegalArgumentException
  {
    Object object = get(name);
    if      (object != null)
    {
      return (String)object;
    }
    else if (defaultValue != null)
    {
      return defaultValue;
    }
    else
    {
      throw new IllegalArgumentException("unknown entry '"+name+"'");
    }
  }

  /** get string value
   * @param name name
   * @param defaultValue default value
   * @return string value
   */
  public String getString(String name)
    throws IllegalArgumentException
  {
    return getString(name,(String)null);
  }

  /** get enum value
   * Note: if T implements EnumParse use method parse() to convert string into enum value
   * @param name name
   * @param defaultValue default value
   * @return enum value
   */
  public <T extends Enum<T>> T getEnum(String name, Class<T> type, T defaultValue)
    throws IllegalArgumentException
  {
    Object object = get(name);
    if      (object != null)
    {
      if (object instanceof String)
      {
        String string = (String)object;

        // use parse method (if available)
        if (EnumParser.class.isAssignableFrom(type))
        {
          try
          {
            Method parseEnum = EnumParser.class.getDeclaredMethod("parse",String.class);
            T enumValue0 = type.getEnumConstants()[0];
            return (T)parseEnum.invoke(enumValue0,string);
          }
          catch (IllegalAccessException exception)
          {
            // ignored
          }
          catch (InvocationTargetException exception)
          {
            // ignored
          }
          catch (NoSuchMethodException exception)
          {
            // ignored
          }
        }

        // compare enum value names
        Enum[] enumConstants = (Enum[])type.getEnumConstants();
        int n;
        try
        {
          n = Integer.parseInt(string);
        }
        catch (NumberFormatException exception)
        {
          n = -1;
        }
        boolean foundFlag = false;
        for (Enum enumConstant : enumConstants)
        {
          if (   string.equalsIgnoreCase(enumConstant.name())
              || (enumConstant.ordinal() == n)
              || string.equalsIgnoreCase(enumConstant.toString())
             )
          {
            return (T)enumConstant;
          }
        }
        if (defaultValue != null)
        {
          return defaultValue;
        }
        else
        {
          throw new IllegalArgumentException("unknown enum value '"+string+"' for "+name);
        }
      }
      else
      {
        return (T)object;
      }
    }
    else if (defaultValue != null)
    {
      return defaultValue;
    }
    else
    {
      throw new IllegalArgumentException("unknown entry '"+name+"'");
    }
  }

  /** get enum value
   * @param name name
   * @return enum value
   */
  public <T extends Enum<T>> T getEnum(String name, Class<T> type)
    throws IllegalArgumentException
  {
    return getEnum(name,type,(T)null);
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    StringBuilder buffer = new StringBuilder();

    buffer.append('{');
    for (String name : keySet())
    {
      if (buffer.length() > 1) buffer.append(", ");
      buffer.append(name);
      buffer.append('=');
      buffer.append(get(name));
    }
    buffer.append('}');

    return buffer.toString();
  }
}

/** enum parser interface
 */
interface EnumParser<T>
{
  /** parse string into enum value
   * @param string string
   * @return enum value
   */
  public T parse(String string);
}

/** scanf-like string parser
 */
public class StringParser
{
  /** quoting characters for string values
   */
  public static String QUOTE_CHARS = "'\"";

  /** specifier length modifiers
   */
  private static enum LengthTypes
  {
    INTEGER,
    LONG,
    DOUBLE
  };

  /** boolean true/false values
   */
  private static String trueStrings[] =
  {
    "1",
    "true",
    "yes",
    "on",
  };
  private static String falseStrings[] =
  {
    "0",
    "false",
    "no",
    "off",
  };

  /** format token
   */
  private static class FormatToken
  {
    StringBuilder token;
    int           length;
    boolean       alternateFlag;            // #: not used
    boolean       zeroPaddingFlag;          // 0: pad numbers with 0
    boolean       leftAdjustedFlag;         // -: format left adjusted
    boolean       blankFlag;                // ' '/+: pad numbers with spaces/get string with spaces
    boolean       greedyFlag;               // *: get all characters of string if last format token
    int           width;
    int           precision;
    LengthTypes   lengthType;
    char          quoteChar;
    String        enumClassName;
    char          conversionChar;
  }

  /** get next format token
   * Note:
   *   Supported format specifieres
   *     i,d,u             - decimal
   *     o                 - octal
   *     x,X               - hexa-decimal
   *     c                 - character
   *     e,E,f,F,g,G,a,A   - float/double
   *     s                 - string
   *     S                 - string with quotes
   *     y                 - boolean
   *   Supported options:
   *     #,0,-, ,+,*       - flags
   *     1-9               - width
   *     .                 - precision
   *     {<name>}s         - enumeration <name>
   *     <x>s, <x>S        - quoting character <x>
   *     <space>s,<space>S - accept spaces, ignore quotes
   *     *s                - accept all to eol
   *     h,l,j,z,t         - length modifier
   * @param format format string
   * @param formatIndex index in format string
   * @param formatToken format token
   * @return next format string index
   */
  private static int getNextFormatToken(String format, int formatIndex, FormatToken formatToken)
  {
    formatToken.token            = new StringBuilder();
    formatToken.length           = 0;
    formatToken.alternateFlag    = false;
    formatToken.zeroPaddingFlag  = false;
    formatToken.leftAdjustedFlag = false;
    formatToken.blankFlag        = false;
    formatToken.greedyFlag       = false;
    formatToken.width            = 0;
    formatToken.precision        = 0;
    formatToken.lengthType       = LengthTypes.INTEGER;
    formatToken.quoteChar        = '\0';
    formatToken.enumClassName    = null;
    formatToken.conversionChar   = '\0';

    // format start character
    assert format.charAt(formatIndex) == '%';
    formatToken.token.append('%');
    formatIndex++;
    if (formatIndex >= format.length())
    {
      return -1;
    }

    // flags
    while (   (formatIndex < format.length())
           && (   (format.charAt(formatIndex) == '#')
               || (format.charAt(formatIndex) == '0')
               || (format.charAt(formatIndex) == '-')
               || (format.charAt(formatIndex) == ' ')
               || (format.charAt(formatIndex) == '+')
               || (format.charAt(formatIndex) == '*')
              )
          )
    {
      formatToken.token.append(format.charAt(formatIndex));
      switch (format.charAt(formatIndex))
      {
        case '#': formatToken.alternateFlag    = true; break;
        case '0': formatToken.zeroPaddingFlag  = true; break;
        case '-': formatToken.leftAdjustedFlag = true; break;
        case ' ': formatToken.blankFlag        = true; break;
        case '+': formatToken.blankFlag        = true; break;
        case '*': formatToken.greedyFlag       = true; break;
        default:
          return -1;
      }
      formatIndex++;
    }
    if (formatIndex >= format.length())
    {
      return -1;
    }

    // width, precision
    while ((formatIndex < format.length()) && (Character.isDigit(format.charAt(formatIndex))))
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.width = formatToken.width*10+(format.charAt(formatIndex)-'0');
      formatIndex++;
    }
    if (formatIndex >= format.length())
    {
      return -1;
    }

    // precision
    if (format.charAt(formatIndex) == '.')
    {
      formatToken.token.append(format.charAt(formatIndex));
      formatIndex++;
      while ((formatIndex < format.length()) && Character.isDigit(format.charAt(formatIndex)))
      {
        formatToken.token.append(format.charAt(formatIndex));

        formatToken.precision = formatToken.precision*10+(format.charAt(formatIndex)-'0');
        formatIndex++;
      }
    }

    // quoting character
    if (   (formatIndex+1 < format.length())
        && (format.charAt(formatIndex) != '{')
        && ((format.charAt(formatIndex+1) == 's') || (format.charAt(formatIndex+1) == 'S'))
       )
    {
      formatToken.quoteChar = format.charAt(formatIndex);
      formatIndex++;
    }

    // length modifier
    if      (   (formatIndex+1 < format.length())
             && (format.charAt(formatIndex) == 'h')
             && (format.charAt(formatIndex+1) == 'h')
            )
    {
      formatToken.token.append(format.charAt(formatIndex+0));
      formatToken.token.append(format.charAt(formatIndex+1));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex += 2;
    }
    else if (   (formatIndex < format.length())
             && (format.charAt(formatIndex) == 'h')
            )
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex++;
    }
    else if (   (formatIndex+1 < format.length())
             && (format.charAt(formatIndex) == 'l')
             && (format.charAt(formatIndex+1) == 'l')
            )
    {
      formatToken.token.append(format.charAt(formatIndex+0));
      formatToken.token.append(format.charAt(formatIndex+1));

      formatToken.lengthType = LengthTypes.LONG;
      formatIndex += 2;
    }
    else if (   (formatIndex < format.length())
             && (format.charAt(formatIndex) == 'l')
            )
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.LONG;
      formatIndex++;
    }
    else if (   (formatIndex < format.length())
             && (format.charAt(formatIndex) == 'j')
            )
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex++;
    }
    else if (   (formatIndex < format.length())
             && (format.charAt(formatIndex) == 'z')
            )
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex++;
    }
    else if (   (formatIndex < format.length())
             && (format.charAt(formatIndex) == 't')
            )
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex++;
    }

    if (   (formatIndex < format.length())
        && (format.charAt(formatIndex) == '{')
       )
    {
      // enum name
      formatToken.token.append(format.charAt(formatIndex));
      formatIndex++;

      StringBuilder buffer = new StringBuilder();
      while ((formatIndex < format.length()) && (format.charAt(formatIndex) != '}'))
      {
        char ch = format.charAt(formatIndex);
        formatToken.token.append(ch);
        buffer.append((ch != '.') ? ch : '$');
        formatIndex++;
      }
      formatIndex++;

      formatToken.enumClassName = buffer.toString();
    }

    if (formatIndex >= format.length())
    {
      return -1;
    }

    // conversion character
    switch (format.charAt(formatIndex))
    {
      case 'S':
        formatToken.token.append('s');
        formatToken.conversionChar = 'S';
        break;
      default:
        formatToken.token.append(format.charAt(formatIndex));
        formatToken.conversionChar = format.charAt(formatIndex);
        break;
    }
    formatIndex++;

    return formatIndex;
  }

  /** get quote char
   * @param string string
   * @param index index in string
   * @param formatToken format token
   * @param string quote characters
   * @return string quote character if
   *   not blank/greedy flag set and
   *   string position match to format quote character or string quotes
   */
  private static char getQuoteChar(String string, int index, FormatToken formatToken, String stringQuotes)
  {
    char stringQuote = '\0';
    if (!formatToken.blankFlag && !formatToken.greedyFlag && (index < string.length()))
    {
      // check if quote character specified and in string
      if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string.charAt(index)))
      {
        stringQuote = formatToken.quoteChar;
      }
      // check if one of string quote characters is in string
      if ((stringQuote == '\0') && (stringQuotes != null) && (stringQuotes.indexOf(string.charAt(index)) >= 0))
      {
        stringQuote = stringQuotes.charAt(stringQuotes.indexOf(string.charAt(index)));
      }
    }

    return stringQuote;
  }

  /** parse string
   * @param string string to parse
   * @param index start index for parsing
   * @param format format string (like printf)
   * @param arguments parsed values
   * @param stringQuotes string quote characters
   * @return index of first not parsed character or -1 on error
   */
  public static int parse(String string, int index, String format, Object arguments[], String stringQuotes)
  {
    int           formatIndex;
    int           argumentIndex;
    FormatToken   formatToken = new FormatToken();
    StringBuilder buffer;
    char          ch;
    int           i;

    formatIndex   = 0;
    argumentIndex = 0;
    while (formatIndex < format.length())
    {
//Dprintf.dprintf("index=%d %d",index,string.length());
//Dprintf.dprintf("format=%s formatIndex=%d %d",format,formatIndex,format.length());
      if      (format.charAt(formatIndex) == '%')
      {
        // get format token
        formatIndex = getNextFormatToken(format,formatIndex,formatToken);
        if (formatIndex < 0)
        {
          return -1;
        }

        // skip spaces in line
        while ((index < string.length()) && Character.isSpaceChar(string.charAt(index)))
        {
          index++;
        }

        // parse string and store values
        switch (formatToken.conversionChar)
        {
          case 'i':
          case 'd':
          case 'u':
            // get data
            buffer = new StringBuilder();
            if ((index < string.length()) && ((string.charAt(index) == '+') || (string.charAt(index) == '-')))
            {
              buffer.append(string.charAt(index));
              index++;
            }
            while (   (index < string.length())
                   && Character.isDigit(string.charAt(index))
                  )
            {
              buffer.append(string.charAt(index));
              index++;
            }
            if (buffer.length() <= 0)
            {
              return -1;
            }

            // convert
            try
            {
              switch (formatToken.lengthType)
              {
                case INTEGER:
                  arguments[argumentIndex] = Integer.parseInt(buffer.toString(),10);
                  break;
                case LONG:
                  arguments[argumentIndex] = Long.parseLong(buffer.toString(),10);
                  break;
                case DOUBLE:
                  try
                  {
                    arguments[argumentIndex] = NumberFormat.getInstance(Locale.ENGLISH).parse(buffer.toString()).doubleValue();
                  }
                  catch (ParseException exception)
                  {
                    return -1;
                  }
                  break;
              }
              argumentIndex++;
            }
            catch (NumberFormatException exception)
            {
              return -1;
            }
            break;
          case 'c':
            // get data
            if (index < string.length())
            {
              ch = string.charAt(index);
              index++;
            }
            else
            {
              return -1;
            }

            // convert
            arguments[argumentIndex] = ch;
            argumentIndex++;
            break;
          case 'o':
            // get data
            buffer = new StringBuilder();
            while (   (index < string.length())
                   && (string.charAt(index) >= '0')
                   && (string.charAt(index) <= '7')
                  )
            {
              buffer.append(string.charAt(index));
              index++;
            }
            if (buffer.length() <= 0)
            {
              return -1;
            }

            // convert
            try
            {
              switch (formatToken.lengthType)
              {
                case INTEGER:
                  arguments[argumentIndex] = Integer.parseInt(buffer.toString(),8);
                  break;
                case LONG:
                  arguments[argumentIndex] = Long.parseLong(buffer.toString(),8);
                  break;
                case DOUBLE:
                  break;
              }
              argumentIndex++;
            }
            catch (NumberFormatException exception)
            {
              return -1;
            }
            break;
          case 'x':
          case 'X':
            // get data
            buffer = new StringBuilder();
            if (((index+1) < string.length()) && (string.charAt(index+0) == '0') && (string.charAt(index+0) == 'x'))
            {
              index+=2;
            }
            while (   (index < string.length())
                   && Character.isDigit(string.charAt(index))
                  )
            {
              buffer.append(string.charAt(index));
              index++;
            }
            if (buffer.length() <= 0)
            {
              return -1;
            }

            // convert
            try
            {
              switch (formatToken.lengthType)
              {
                case INTEGER:
                  arguments[argumentIndex] = Integer.parseInt(buffer.toString(),16);
                  break;
                case LONG:
                  arguments[argumentIndex] = Long.parseLong(buffer.toString(),16);
                  break;
                case DOUBLE:
                  break;
              }
              argumentIndex++;
            }
            catch (NumberFormatException exception)
            {
              return -1;
            }
            break;
          case 'e':
          case 'E':
          case 'f':
          case 'F':
          case 'g':
          case 'G':
          case 'a':
          case 'A':
            // get data
            buffer = new StringBuilder();
            if ((index < string.length()) && ((string.charAt(index) == '+') || (string.charAt(index) == '-')))
            {
              buffer.append(string.charAt(index));
              index++;
            }
            while (   (index < string.length())
                   && Character.isDigit(string.charAt(index))
                  )
            {
              buffer.append(string.charAt(index));
              index++;
            }
            if ((index < string.length()) && (string.charAt(index) == '.'))
            {
              buffer.append(string.charAt(index));
              index++;
              while (   (index < string.length())
                     && Character.isDigit(string.charAt(index))
                    )
              {
                buffer.append(string.charAt(index));
                index++;
              }
            }
            if (buffer.length() <= 0)
            {
              return -1;
            }

            // convert
            try
            {
              try
              {
                arguments[argumentIndex] = NumberFormat.getInstance(Locale.ENGLISH).parse(buffer.toString()).doubleValue();
              }
              catch (ParseException exception)
              {
                return -1;
              }
              argumentIndex++;
            }
            catch (NumberFormatException exception)
            {
              return -1;
            }
            break;
          case 's':
            // get data
            buffer = new StringBuilder();
            while (   (index < string.length())
                   && (formatToken.blankFlag || ((formatIndex >= format.length()) && formatToken.greedyFlag) || !Character.isSpaceChar((string.charAt(index))))
                   && ((formatIndex >= format.length()) || (string.charAt(index) != format.charAt(formatIndex)))
                  )
            {
              if (   !formatToken.greedyFlag
                  && ((index+1) < string.length())
                  && (string.charAt(index) == '\\')
                  && (stringQuotes != null)
                  && (stringQuotes.indexOf(string.charAt(index+1)) >= 0)
                 )
              {
                // quoted quote
                if ((formatToken.width == 0) || (buffer.length() < formatToken.width-1))
                {
                  buffer.append(string.charAt(index+1));
                }
                index+=2;
              }
              else
              {
                if ((formatToken.width == 0) || (buffer.length() < (formatToken.width-1)))
                {
                  buffer.append(string.charAt(index));
                }
                index++;
              }
            }

            if (formatToken.enumClassName != null)
            {
              // enum class
              try
              {
                // find enum class
                Class enumClass = Class.forName(formatToken.enumClassName);
                if (!enumClass.isEnum())
                {
                  return -1;
                }

                // convert to enum
                arguments[argumentIndex] = Enum.valueOf(enumClass,buffer.toString());
              }
              catch (ClassNotFoundException exception)
              {
//Dprintf.dprintf(""+exception);
                throw new Error("Enumeration class '"+formatToken.enumClassName+"' not found",exception);
              }
              catch (IllegalArgumentException exception)
              {
//Dprintf.dprintf(""+exception);
                return -1;
              }
            }
            else
            {
              // store string
              arguments[argumentIndex] = buffer.toString();
            }
            argumentIndex++;
            break;
          case 'p':
          case 'n':
            break;
          case 'S':
            // get data
            buffer = new StringBuilder();
            while (   (index < string.length())
                   && (formatToken.blankFlag || ((formatIndex >= format.length()) && formatToken.greedyFlag) || !Character.isSpaceChar(string.charAt(index)))
                   && ((formatIndex >= format.length()) || (string.charAt(index) != format.charAt(formatIndex)))
                  )
            {
              if (   ((index+1) < string.length())
                  && (string.charAt(index) == '\\')
                  && (stringQuotes != null)
                  && (stringQuotes.indexOf(string.charAt(index+1)) >= 0)
                 )
              {
                // quoted quote
                if ((formatToken.width == 0) || (buffer.length() < formatToken.width-1))
                {
                  buffer.append(string.charAt(index+1));
                }
                index+=2;
              }
              else
              {
                // check for string quote
                char stringQuote;

                stringQuote = getQuoteChar(string,index,formatToken,stringQuotes);
                if (stringQuote != '\0')
                {
                  do
                  {
                    // skip quote-char
                    index++;

                    // get string
                    while ((index < string.length()) && (string.charAt(index) != stringQuote))
                    {
                      if (   ((index+1) < string.length())
                          && (string.charAt(index) == '\\')
                          && (stringQuotes != null)
                          && (stringQuotes.indexOf(string.charAt(index+1)) >= 0)
                         )
                      {
                        // quoted quote
                        if ((formatToken.width == 0) || (buffer.length() < formatToken.width-1))
                        {
                          buffer.append(string.charAt(index+1));
                        }
                        index+=2;
                      }
                      else
                      {
                        if ((formatToken.width == 0) || (buffer.length() < formatToken.width-1))
                        {
                          buffer.append(string.charAt(index));
                        }
                        index++;
                      }
                    }

                    // skip quote-char
                    if (index < string.length())
                    {
                      index++;
                    }

                    // check for string quote
                    stringQuote = getQuoteChar(string,index,formatToken,stringQuotes);
                  }
                  while (stringQuote != '\0');
                }
                else
                {
                  if ((formatToken.width == 0) || (buffer.length() < formatToken.width-1))
                  {
                    buffer.append(string.charAt(index));
                  }
                  index++;
                }
              }
            }
            arguments[argumentIndex] = buffer.toString();
            argumentIndex++;
            break;
          case 'y':
            {
              // get data
              buffer = new StringBuilder();
              while (   (index < string.length())
                     && !Character.isSpaceChar(string.charAt(index))
                    )
              {
                buffer.append(string.charAt(index));
                index++;
              }

              // convert
              boolean foundFlag = false;
              i = 0;
              while (!foundFlag && (i < trueStrings.length))
              {
                if (trueStrings[i].contentEquals(buffer))
                {
                  arguments[argumentIndex] = true;
                  foundFlag = true;
                }
                i++;
              }
              i = 0;
              while (!foundFlag && (i < falseStrings.length))
              {
                if (falseStrings[i].contentEquals(buffer))
                {
                  arguments[argumentIndex] = false;
                  foundFlag = true;
                }
                i++;
              }

              if (!foundFlag)
              {
                return -1;
              }
              argumentIndex++;
            }
            break;
          case '%':
            if ((index >= string.length()) || (string.charAt(index) != '%'))
            {
              return -1;
            }
            index++;
            break;
          default:
            return -1;
        }
      }
      else if (Character.isSpaceChar(format.charAt(formatIndex)))
      {
        // check if at least one space character
        if ((index >= string.length()) || !Character.isSpaceChar(string.charAt(index)))
        {
          return -1;
        }
        index++;
        formatIndex++;

        // skip further spaces in line, format
        while ((index < string.length()) && Character.isSpaceChar(string.charAt(index)))
        {
          index++;
        }
        while ((formatIndex < format.length()) && Character.isSpaceChar(format.charAt(formatIndex)))
        {
          formatIndex++;
        }
      }
      else
      {
        // skip spaces in line
        while ((index < string.length()) && Character.isSpaceChar(string.charAt(index)))
        {
          index++;
        }

        // compare non-format characters
        if ((index >= string.length()) || (string.charAt(index) != format.charAt(formatIndex)))
        {
          return -1;
        }
        index++;
        formatIndex++;
      }
    }

    // skip spaces at end of line
    while ((index < string.length()) && Character.isSpaceChar(string.charAt(index)))
    {
      index++;
    }

    return index;
  }

  /** parse string
   * @param string string to parse
   * @param format format string (like printf)
   * @param arguments parsed values
   * @return index of first not parsed character
   */
  public static int parse(String string, int index, String format, Object arguments[])
  {
    return parse(string,index,format,arguments,(String)null);
  }

  /** parse string
   * @param string string to parse
   * @param format format string (like printf)
   * @param arguments parsed values
   * @return true iff string parse, false otherwise
   */
  public static boolean parse(String string, String format, Object arguments[], String stringQuotes)
  {
    return parse(string,0,format,arguments,stringQuotes) >= string.length();
  }

  /** parse string
   * @param string string to parse
   * @param format format string (like printf)
   * @param arguments parsed values
   * @return true iff string parse, false otherwise
   */
  public static boolean parse(String string, String format, Object arguments[])
  {
    return parse(string,format,arguments,(String)null);
  }

  /** get quote char
   * @param string string
   * @param index index in string
   * @param string quote characters
   * @return string quote character or '\0'
   */
  private static char getQuoteChar(String string, int index, String stringQuotes)
  {
    char stringQuote = '\0';
    if (index < string.length())
    {
      // check if one of string quote characters is in string
      if ((stringQuote == '\0') && (stringQuotes != null) && (stringQuotes.indexOf(string.charAt(index)) >= 0))
      {
        stringQuote = stringQuotes.charAt(stringQuotes.indexOf(string.charAt(index)));
      }
    }

    return stringQuote;
  }

  /** parse string into hash map
   * @param string string to parse
   * @param index start index for parsing
   * @param typeMap type map
   * @param valueMap parsed values
   * @param unknownValueMap unknown values
   * @param stringQuotes string quote characters
   * @return index of first not parsed character or -1 on error
   */
  public static int parse(String string, int index, TypeMap typeMap, ValueMap valueMap, ValueMap unknownValueMap, String stringQuotes)
    throws NumberFormatException
  {
    final String ESCAPE_CHARACTERS = "0abtnvfre";
    final char[] ESCAPE_MAP        = new char[]{'\0','\007','\b','\t','\n','\013','\014','\r','\033'};

    StringBuilder buffer;
    char          ch;
    int           z;

    while (index < string.length())
    {
      // skip spaces in line, format
      while ((index < string.length()) && Character.isSpaceChar(string.charAt(index)))
      {
        index++;
      }

      if (index < string.length())
      {
        // get name
        buffer = new StringBuilder();
        if (!Character.isLetter(string.charAt(index)) && (string.charAt(index) != '_'))
        {
          return index;
        }
        do
        {
          buffer.append(string.charAt(index));
          index++;
        }
        while ((index < string.length()) && (Character.isLetterOrDigit(string.charAt(index)) || (string.charAt(index) == '_')));
        String name = buffer.toString();
//Dprintf.dprintf("name=%s",name);

        // check if '='
        if ((index >= string.length()) || (string.charAt(index) != '='))
        {
          return index;
        }
        index++;

        // get value
        buffer = new StringBuilder();
        while (   (index < string.length())
               && !Character.isSpaceChar(string.charAt(index))
              )
        {
          if (   ((index+1) < string.length())
              && (string.charAt(index) == '\\')
              && (stringQuotes != null)
              && (stringQuotes.indexOf(string.charAt(index+1)) >= 0)
             )
          {
            // quoted quote
            buffer.append(string.charAt(index+1));
            index+=2;
          }
          else
          {
            // check for string quote
            char stringQuote;

            stringQuote = getQuoteChar(string,index,stringQuotes);
            if (stringQuote != '\0')
            {
              do
              {
                // skip quote-char
                index++;

                // get string
                while ((index < string.length()) && (string.charAt(index) != stringQuote))
                {
                  if (   ((index+1) < string.length())
                      && (string.charAt(index) == '\\')
                     )
                  {
                    index++;

                    if      (   (stringQuotes != null)
                             && (stringQuotes.indexOf(string.charAt(index)) >= 0)
                            )
                    {
                      // quoted quote
                      buffer.append(string.charAt(index));
                    }
                    else if (ESCAPE_CHARACTERS.indexOf(string.charAt(index)) >= 0)
                    {
                      // known escape characater
                      buffer.append(ESCAPE_MAP[ESCAPE_CHARACTERS.indexOf(string.charAt(index))]);
                    }
                    else
                    {
                      // other escaped character
                      buffer.append(string.charAt(index));
                    }
                  }
                  else
                  {
                    buffer.append(string.charAt(index));
                  }
                  index++;
                }

                // skip quote-char
                if (index < string.length())
                {
                  index++;
                }

                // check for string quote
                stringQuote = getQuoteChar(string,index,stringQuotes);
              }
              while (stringQuote != '\0');

            }
            else
            {
              buffer.append(string.charAt(index));
              index++;
            }
          }
        }
        String value = buffer.toString();
//Dprintf.dprintf("name=%s value=#%s#",name,value);

        // store value
        if (typeMap != null)
        {
          Class type = typeMap.get(name);
//Dprintf.dprintf("type=%s",type);
          if      ((type == int.class) || (type == Integer.class))
          {
            valueMap.put(name,Integer.parseInt(value));
          }
          else if ((type == long.class) || (type == Long.class))
          {
            valueMap.put(name,Long.parseLong(value));
          }
          else if ((type == float.class) || (type == Float.class))
          {
            valueMap.put(name,Float.parseFloat(value));
          }
          else if ((type == double.class) || (type == Double.class))
          {
            try
            {
              valueMap.put(name,NumberFormat.getInstance(Locale.ENGLISH).parse(value).doubleValue());
            }
            catch (ParseException exception)
            {
              throw new IllegalArgumentException(value,exception);
            }
          }
          else if ((type == boolean.class) || (type == Boolean.class))
          {
            valueMap.put(name,
                            value.equalsIgnoreCase("yes")
                         || value.equalsIgnoreCase("on")
                         || value.equalsIgnoreCase("true")
                         || value.equals("1")
                        );
          }
          else if (type == Character.class)
          {
            valueMap.put(name,value.charAt(0));
          }
          else if (type == String.class)
          {
            valueMap.put(name,value);
          }
          else if ((type != null) && type.isEnum())
          {
            Enum[] enumConstants = (Enum[])type.getEnumConstants();
            int n;
            try
            {
              n = Integer.parseInt(value);
            }
            catch (NumberFormatException exception)
            {
              n = -1;
            }
            boolean foundFlag = false;
            for (Enum enumConstant : enumConstants)
            {
              if (   value.equalsIgnoreCase(enumConstant.name())
                  || (enumConstant.ordinal() == n)
                  || value.equalsIgnoreCase(enumConstant.toString())
                 )
              {
                valueMap.put(name,enumConstant);
                foundFlag = true;
                break;
              }
            }
            if (!foundFlag)
            {
              throw new IllegalArgumentException("unknown enum value '"+value+"' for "+name);
            }
          }
          else if (unknownValueMap != null)
          {
            unknownValueMap.put(name,value);
          }
          else
          {
            valueMap.put(name,value);
          }
        }
        else
        {
          valueMap.put(name,value);
        }
      }
    }

    return index;
  }

  /** parse string into hash map
   * @param string string to parse
   * @param typeMap type map
   * @param valueMap parsed values
   * @param unknownValueMap unknown values
   * @param stringQuotes string quote characters
   * @return index of first not parsed character or -1 on error
   */
  public static boolean parse(String string, TypeMap typeMap, ValueMap valueMap, ValueMap unknownValueMap, String stringQuotes)
  {
    return parse(string,0,typeMap,valueMap,unknownValueMap,stringQuotes) >= string.length();
  }

  /** parse string into hash map
   * @param string string to parse
   * @param typeMap type map
   * @param valueMap parsed values
   * @param unknownValueMap unknown values
   * @return index of first not parsed character or -1 on error
   */
  public static boolean parse(String string, TypeMap typeMap, ValueMap valueMap, ValueMap unknownValueMap)
  {
    return parse(string,typeMap,valueMap,unknownValueMap,QUOTE_CHARS);
  }

  /** parse string into hash map
   * @param string string to parse
   * @param valueMap parsed values
   * @param unknownValueMap unknown values
   * @return index of first not parsed character or -1 on error
   */
  public static boolean parse(String string, ValueMap valueMap, ValueMap unknownValueMap)
  {
    return parse(string,(TypeMap)null,valueMap,unknownValueMap);
  }

  /** parse string into hash map
   * @param string string to parse
   * @param typeMap type map
   * @param valueMap parsed values
   * @return index of first not parsed character or -1 on error
   */
  public static boolean parse(String string, TypeMap typeMap, ValueMap valueMap)
  {
    return parse(string,typeMap,valueMap,(ValueMap)null);
  }

  /** parse string into hash map
   * @param string string to parse
   * @param valueMap parsed values
   * @return index of first not parsed character or -1 on error
   */
  public static boolean parse(String string, ValueMap valueMap)
  {
    return parse(string,(TypeMap)null,valueMap);
  }

  /** format string and escape %S strings
   * @param format format string
   * @param quoteChar quote character to use
   * @param arguments optional arguments
   * @return formated string
   */
  public static String format(String format, char quoteChar, Object... arguments)
  {
    final String ESCAPE_CHARACTERS = "\0\007\b\t\n\013\014\r\033";
    final char[] ESCAPE_MAP        = new char[]{'0','a','b','t','n','v','f','r','e'};

    StringBuilder buffer;
    int           formatIndex;
    int           argumentIndex;
    FormatToken   formatToken = new FormatToken();
    char          ch;
    int           i;

    buffer        = new StringBuilder();
    formatIndex   = 0;
    argumentIndex = 0;
    while (formatIndex < format.length())
    {
      // copy prefix
      while (   (formatIndex < format.length())
             && (format.charAt(formatIndex) != '%')
            )
      {
        if (   ((formatIndex+1) < format.length())
            && (format.charAt(formatIndex) == '\\')
           )
        {
          formatIndex++;
          buffer.append(format.charAt(formatIndex));
        }
        else
        {
          buffer.append(format.charAt(formatIndex));
        }
        formatIndex++;
      }

      if (formatIndex < format.length())
      {
        if (format.charAt(formatIndex) == '%')
        {
          // get format token
          formatIndex = getNextFormatToken(format,formatIndex,formatToken);
          if (formatIndex < 0)
          {
            return null;
          }

          // format string
          switch (formatToken.conversionChar)
          {
            case 'y':
              buffer.append((Boolean)arguments[argumentIndex] ? "yes" : "no");
              argumentIndex++;
              break;
            case 'S':
              buffer.append(quoteChar);
              String string = String.format("%s",arguments[argumentIndex]);
              for (i = 0; i < string.length(); i++)
              {
                ch = string.charAt(i);
                if      (ch == quoteChar)
                {
                  buffer.append('\\');
                  buffer.append(ch);
                }
                else if (ch == '\\')
                {
                  buffer.append("\\\\");
                }
                else if (ESCAPE_CHARACTERS.indexOf(ch) >= 0)
                {
                  buffer.append('\\');
                  buffer.append(ESCAPE_MAP[ESCAPE_CHARACTERS.indexOf(ch)]);
                }
                else
                {
                  buffer.append(ch);
                }
              }
              buffer.append(quoteChar);
              argumentIndex++;
              break;
            case '%':
              buffer.append('%');
              break;
            default:
              buffer.append(String.format("%"+formatToken.conversionChar,arguments[argumentIndex]));
              argumentIndex++;
              break;
          }
        }
        else
        {
          buffer.append(format.charAt(formatIndex));
          formatIndex++;
        }
      }
    }

    // copy postfix
    while (formatIndex < format.length())
    {
      buffer.append(format.charAt(formatIndex));
      formatIndex++;
    }

    return buffer.toString();
  }

  /** format string and escape %S strings
   * @param format format string
   * @param arguments optional arguments
   * @return formated string
   */
  public static String format(String format, Object... arguments)
  {
    return format(format,'"',arguments);
  }

  /** parse int value
   * @param name name
   * @param radix conversion radix
   * @return int value
   */
  public static int parseInt(String string, int radix)
    throws NumberFormatException
  {
    if (string.startsWith("0x") || string.startsWith("0X"))
    {
      return Integer.parseInt(string.substring(2),16);
    }
    else
    {
      return Integer.parseInt(string,radix);
    }
  }

  /** parse int value
   * @param name name
   * @return int value
   */
  public static int parseInt(String string)
    throws NumberFormatException
  {
    return parseInt(string,10);
  }

  /** parse long value
   * @param name name
   * @param radix conversion radix
   * @return long value
   */
  public static long parseLong(String string, int radix)
    throws NumberFormatException
  {
    if (string.startsWith("0x") || string.startsWith("0X"))
    {
      return Long.parseLong(string.substring(2),16);
    }
    else
    {
      return Long.parseLong(string,radix);
    }
  }

  /** parse long value
   * @param name name
   * @return long value
   */
  public static long parseLong(String string)
    throws NumberFormatException
  {
    return parseLong(string,10);
  }
}

/* end of file */
