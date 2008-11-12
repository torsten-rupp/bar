/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/StringParser.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: String parser
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.Integer;
import java.lang.Long;
import java.lang.String;
//import java.lang.System;
//import java.util.LinkedList;
//import java.util.ArrayList;

/****************************** Classes ********************************/

class StringParser
{
  public static String QUOTE_CHARS = "'\"";

  private static enum LengthTypes
  {
    INTEGER,
    LONG,
    DOUBLE
  };

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

  private static class FormatToken
  {
    StringBuffer token;
    int          length;
    boolean      alternateFlag;
    boolean      zeroPaddingFlag;
    boolean      leftAdjustedFlag;
    boolean      blankFlag;
    boolean      signFlag;
    int          width;
    int          precision;
    LengthTypes  lengthType;
    char         quoteChar;
    char         conversionChar;
  }

  private static int getNextFormatToken(String format, int formatIndex, FormatToken formatToken)
  {
    formatToken.token            = new StringBuffer();
    formatToken.length           = 0;
    formatToken.alternateFlag    = false;
    formatToken.zeroPaddingFlag  = false;
    formatToken.leftAdjustedFlag = false;
    formatToken.blankFlag        = false;
    formatToken.signFlag         = false;
    formatToken.width            = 0;
    formatToken.precision        = 0;
    formatToken.lengthType       = LengthTypes.INTEGER;
    formatToken.quoteChar        = '\0';
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
    if ((formatIndex+1 < format.length()) && ((format.charAt(formatIndex+1) == 's') || (format.charAt(formatIndex+1) == 'S')))
    {
      formatToken.quoteChar = format.charAt(formatIndex);
      formatIndex++;
    }

    /* length modifier */
    if      ((formatIndex+1 < format.length()) && (format.charAt(formatIndex) == 'h') && (format.charAt(formatIndex+1) == 'h'))
    {
      formatToken.token.append(format.charAt(formatIndex+0));
      formatToken.token.append(format.charAt(formatIndex+1));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex += 2;
    }
    else if ((formatIndex < format.length()) && (format.charAt(formatIndex) == 'h'))
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex++;
    }
    else if ((formatIndex+1 < format.length()) && (format.charAt(formatIndex) == 'l') && (format.charAt(formatIndex+1) == 'l'))
    {
      formatToken.token.append(format.charAt(formatIndex+0));
      formatToken.token.append(format.charAt(formatIndex+1));

      formatToken.lengthType = LengthTypes.LONG;
      formatIndex += 2;
    }
    else if ((formatIndex < format.length()) && (format.charAt(formatIndex) == 'l'))
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.LONG;
      formatIndex++;
    }
    else if ((formatIndex < format.length()) && (format.charAt(formatIndex) == 'j'))
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex++;
    }
    else if ((formatIndex < format.length()) && (format.charAt(formatIndex) == 'z'))
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex++;
    }
    else if ((formatIndex < format.length()) && (format.charAt(formatIndex) == 't'))
    {
      formatToken.token.append(format.charAt(formatIndex));

      formatToken.lengthType = LengthTypes.INTEGER;
      formatIndex++;
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

  public static boolean parse(String string, String format, Object arguments[], String stringQuotes)
  {
    int          index,formatIndex;
    int          argumentIndex;
    FormatToken  formatToken = new FormatToken();
    StringBuffer buffer;
    char         ch;
    int          z;

//System.err.println("StringParser.java"+", "+105+": "+string+" -- "+format);
    index         = 0;
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

//System.err.println("StringParser.java"+", "+121+": "+string.substring(index)+"--"+format.substring(formatIndex));
      if (formatIndex < format.length())
      {
        if (format.charAt(formatIndex) == '%')
        {
          /* get format token */
          formatIndex = getNextFormatToken(format,formatIndex,formatToken);
          if (formatIndex < 0)
          {
            return false;
          }
//System.err.println("StringParser.java"+", "+128+": "+formatIndex+": "+formatToken.conversionChar);

          /* parse string and store values */
          switch (formatToken.conversionChar)
          {
            case 'i':
            case 'd':
            case 'u':
              /* get data */
              buffer = new StringBuffer();
              while (   (index < string.length())
                     && Character.isDigit(string.charAt(index))
                    )
              {
                buffer.append(string.charAt(index));
                index++;
              }
              if (buffer.length() <= 0)
              {
                return false;
              }

              /* convert */
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
                return false;
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
                return false;
              }

              /* convert */
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
                return false;
              }

              /* convert */
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
                return false;
              }

              /* convert */
              arguments[argumentIndex] = Double.parseDouble(buffer.toString());
              argumentIndex++;
              break;
            case 's':
              /* get data */
              buffer = new StringBuffer();
              while (   (index < string.length())
                     && (formatToken.blankFlag || !Character.isSpaceChar((string.charAt(index))))
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
              arguments[argumentIndex] = buffer.toString();
              argumentIndex++;
              break;
            case 'p':
            case 'n':
              break;
            case 'S':
              /* get data */
              buffer = new StringBuffer();
              while (   (index < string.length())
                     && (formatToken.blankFlag || !Character.isSpaceChar(string.charAt(index)))
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
                return false;
              }
              argumentIndex++;
              }
              break;
            case '%':
              if ((index >= string.length()) || (string.charAt(index) != '%'))
              {
                return false;
              }
              index++;
              break;
            default:
              return false;
          }
        }
        else
        {
          if ((index >= string.length()) || (string.charAt(index) != format.charAt(formatIndex)))
          {
            return false;
          }
          index++;
          formatIndex++;
        }
      }
    }

    return true;
  }

  public static boolean parse(String string, String format, Object arguments[])
  {
    return parse(string,format,arguments,null);
  }

  public static String escape(String string)
  {
    StringBuffer buffer = new StringBuffer();

    for (int index = 0; index < string.length(); index++)
    {
      char ch = string.charAt(index);

      if      (ch == '\'')
      {
        buffer.append("\\");
        buffer.append('\'');
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
    if (buffer.length() == 0) buffer.append("''");

    return buffer.toString();
  }

  public static String unescape(String string)
  {
    if (string.startsWith("'") && string.endsWith("'"))
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
