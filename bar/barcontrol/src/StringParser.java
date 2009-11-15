/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/StringParser.java,v $
* $Revision: 1.7 $
* $Author: torsten $
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

/****************************** Classes ********************************/

/** scanf-like string parser
 */
class StringParser
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
    StringBuffer token;
    int          length;
    boolean      alternateFlag;            // #: not used
    boolean      zeroPaddingFlag;          // 0: pad numbers with 0
    boolean      leftAdjustedFlag;         // -: format left adjusted
    boolean      blankFlag;                // ' '/+: pad numbers with spaces/get string with spaces
    boolean      greedyFlag;               // *: get all characters of string if last format token
    int          width;
    int          precision;
    LengthTypes  lengthType;
    char         quoteChar;
    String       enumClassName;
    char         conversionChar;
  }

  /** get next format token
   * Note:
   *   Supported format specifieres
   *     i,d,u           - decimal
   *     o               - octal
   *     x,X             - hexa-decimal
   *     c               - character
   *     e,E,f,F,g,G,a,A - float/double
   *     s               - string
   *     S               - string with quotes
   *     y               - boolean
   *   Supported options:
   *     #,0,-, ,+,*     - flags
   *     1-9             - width
   *     .               - precision
   *     {<name>}s       - enumeration <name>
   *     <x>s, <x>S      - quoting character <x>
   *     h,l,j,z,t       - length modifier
   * @param format format string
   * @param formatIndex index in format string
   * @param formatToken format token
   * @return next format string index
   */
  private static int getNextFormatToken(String format, int formatIndex, FormatToken formatToken)
  {
    formatToken.token            = new StringBuffer();
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

    /* format start character */
    assert format.charAt(formatIndex) == '%';
    formatToken.token.append('%');
    formatIndex++;
    if (formatIndex >= format.length())
    {
      return -1;
    }

    /* flags */
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

    /* width, precision */
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

    /* precision */
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

    /* quoting character */
    if (   (formatIndex+1 < format.length())
        && (format.charAt(formatIndex) != '{')
        && ((format.charAt(formatIndex+1) == 's') || (format.charAt(formatIndex+1) == 'S'))
       )
    {
      formatToken.quoteChar = format.charAt(formatIndex);
      formatIndex++;
    }

    /* length modifier */
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
      /* enum name */
      formatToken.token.append(format.charAt(formatIndex));
      formatIndex++;

      StringBuffer buffer = new StringBuffer();
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

    /* conversion character */
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
   * @return string quote character
   */
  private static char getQuoteChar(String string, int index, FormatToken formatToken, String stringQuotes)
  {
    char stringQuote = '\0';
    if (index < string.length())
    {
      if ((formatToken.quoteChar != '\0') && (formatToken.quoteChar == string.charAt(index)))
      {
        stringQuote = formatToken.quoteChar;
      }
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
    int          formatIndex;
    int          argumentIndex;
    FormatToken  formatToken = new FormatToken();
    StringBuffer buffer;
    char         ch;
    int          z;

    formatIndex   = 0;
    argumentIndex = 0;
    while (formatIndex < format.length())
    {
      // skip spaces in line, format
      while ((index < string.length()) && Character.isSpaceChar(string.charAt(index)))
      {
        index++;
      }
      while ((formatIndex < format.length()) && Character.isSpaceChar(format.charAt(formatIndex)))
      {
        formatIndex++;
      }

      if (formatIndex < format.length())
      {
        if (format.charAt(formatIndex) == '%')
        {
          /* get format token */
          formatIndex = getNextFormatToken(format,formatIndex,formatToken);
          if (formatIndex < 0)
          {
            return -1;
          }

          /* parse string and store values */
          switch (formatToken.conversionChar)
          {
            case 'i':
            case 'd':
            case 'u':
              /* get data */
              buffer = new StringBuffer();
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

              /* convert */
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
                    arguments[argumentIndex] = Double.parseDouble(buffer.toString());
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
              /* get data */
              if (index < string.length())
              {
                ch = string.charAt(index);
                index++;
              }
              else
              {
                return -1;
              }

              /* convert */
              arguments[argumentIndex] = ch;
              argumentIndex++;
              break;
            case 'o':
              /* get data */
              buffer = new StringBuffer();
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

              /* convert */
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
              /* get data */
              buffer = new StringBuffer();
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

              /* convert */
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
              /* get data */
              buffer = new StringBuffer();
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

              /* convert */
              try
              {
                arguments[argumentIndex] = Double.parseDouble(buffer.toString());
                argumentIndex++;
              }
              catch (NumberFormatException exception)
              {
                return -1;
              }
              break;
            case 's':
              /* get data */
              buffer = new StringBuffer();
              while (   (index < string.length())
                     && (formatToken.blankFlag || ((formatIndex >= format.length()) && formatToken.greedyFlag) || !Character.isSpaceChar((string.charAt(index))))
                     && ((formatIndex >= format.length()) || (string.charAt(index) != format.charAt(formatIndex)))
                    )
              {
                if (string.charAt(index) == '\\')
                {
                  index++;
                  if (index < string.length())
                  {
                    if (buffer.length() < formatToken.width-1)
                    {
                      buffer.append(string.charAt(index));
                    }
                    index++;
                  }
                }
                else
                {
                  /* check for string quote */
                  char stringQuote;

                  stringQuote = getQuoteChar(string,index,formatToken,stringQuotes);
                  if (stringQuote != '\0')
                  {
                    do
                    {
                      /* skip quote-char */
                      index++;

                      /* get string */
                      while ((index < string.length()) && (string.charAt(index) != stringQuote))
                      {
                        if (string.charAt(index) == '\\')
                        {
                          index++;
                          if (index < string.length())
                          {
                            if ((formatToken.width == 0) || (buffer.length() < (formatToken.width-1)))
                            {
                              buffer.append(string.charAt(index));
                            }
                            index++;
                          }
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

                      /* skip quote-char */
                      if (index < string.length())
                      {
                        index++;
                      }

                      stringQuote = getQuoteChar(string,index,formatToken,stringQuotes);
                    }
                    while (stringQuote != '\0');
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
              /* get data */
              buffer = new StringBuffer();
              while (   (index < string.length())
                     && (formatToken.blankFlag || ((formatIndex >= format.length()) && formatToken.greedyFlag) || !Character.isSpaceChar(string.charAt(index)))
                     && ((formatIndex >= format.length()) || (string.charAt(index) != format.charAt(formatIndex)))
                    )
              {
                if (string.charAt(index) == '\\')
                {
                  index++;
                  if (index < string.length())
                  {
                    if ((formatToken.width == 0) || (buffer.length() < formatToken.width-1))
                    {
                      buffer.append(string.charAt(index));
                    }
                    index++;
                  }
                }
                else
                {
                  /* check for string quote */
                  char stringQuote;

                  stringQuote = getQuoteChar(string,index,formatToken,stringQuotes);
                  if (stringQuote != '\0')
                  {
                    do
                    {
                      /* skip quote-char */
                      index++;

                      /* get string */
                      while ((index < string.length()) && (string.charAt(index) != stringQuote))
                      {
                        if (string.charAt(index) == '\\')
                        {
                          index++;
                          if (index < string.length())
                          {
                            if ((formatToken.width == 0) || (buffer.length() < formatToken.width-1))
                            {
                              buffer.append(string.charAt(index));
                            }
                            index++;
                          }
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

                      /* skip quote-char */
                      if (index < string.length())
                      {
                        index++;
                      }

                      /* check for string quote */
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
                /* get data */
                buffer = new StringBuffer();
                while (   (index < string.length())
                       && !Character.isSpaceChar(string.charAt(index))
                      )
                {
                  buffer.append(string.charAt(index));
                  index++;
                }

                /* convert */
                boolean foundFlag = false;
                z = 0;
                while (!foundFlag && (z < trueStrings.length))
                {
                  if (trueStrings[z].contentEquals(buffer))
                  {
                    arguments[argumentIndex] = true;
                    foundFlag = true;
                  }
                  z++;
                }
                z = 0;
                while (!foundFlag && (z < falseStrings.length))
                {
                  if (falseStrings[z].contentEquals(buffer))
                  {
                    arguments[argumentIndex] = false;
                    foundFlag = true;
                  }
                  z++;
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
        else
        {
          if ((index >= string.length()) || (string.charAt(index) != format.charAt(formatIndex)))
          {
            return -1;
          }
          index++;
          formatIndex++;
        }
      }
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
//    throws ClassNotFoundException
  {
    return parse(string,index,format,arguments,null);
  }

  /** parse string
   * @param string string to parse
   * @param format format string (like printf)
   * @param arguments parsed values
   * @return true iff string parse, false otherwise
   */
  public static boolean parse(String string, String format, Object arguments[], String stringQuotes)
//    throws ClassNotFoundException
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
//    throws ClassNotFoundException
  {
    return parse(string,format,arguments,null);
  }

  /** escape ' and \ in string, enclose in "
   * @param string string to escape
   * @param enclosingQuotes true to add enclosing quotes "
   * @param quote quote character
   * @return escaped string
   */
  public static String escape(String string, boolean enclosingQuotes, char quote)
  {
    StringBuffer buffer = new StringBuffer();

    if (enclosingQuotes) buffer.append(quote);
    for (int index = 0; index < string.length(); index++)
    {
      char ch = string.charAt(index);

      if      (ch == quote)
      {
        buffer.append("\\"+quote);
      }
      else if (ch == '\\')
      {
        buffer.append("\\\\");
      }
      else
      {
        buffer.append(ch);
      }
    }
    if (enclosingQuotes) buffer.append(quote);

    return buffer.toString();
  }

  /** escape ' and \ in string, enclose in "
   * @param string string to escape
   * @param enclosingQuotes true to add enclosing quotes "
   * @return escaped string
   */
  public static String escape(String string, boolean enclosingQuotes)
  {
    return escape(string,enclosingQuotes,'"');
  }

  /** escape ' and \ in string, enclose in "
   * @param string string to escape
   * @param quote quote character
   * @return escaped string
   */
  public static String escape(String string, char quote)
  {
    return escape(string,true,quote);
  }

  /** escape ' and \ in string, enclose in "
   * @param string string to escape
   * @return escaped string
   */
  public static String escape(String string)
  {
    return escape(string,true);
  }

  /** remove enclosing ' or "
   * @param string string to unescape
   * @return unescaped string
   */
  public static String unescape(String string)
  {
    if      (string.startsWith("\"") && string.endsWith("\""))
    {
      return string.substring(1,string.length()-1);
    }
    else if (string.startsWith("'") && string.endsWith("'"))
    {
      return string.substring(1,string.length()-1);
    }
    else
    {
      return string;
    }
  }
}

/* end of file */
