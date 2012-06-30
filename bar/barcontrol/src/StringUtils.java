/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/StringUtils.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: String utility functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.Iterator;

/****************************** Classes ********************************/

/** string utility functions
 */
public class StringUtils
{
  // --------------------------- constants --------------------------------
  /** quoting characters for string values
   */
  public final static String QUOTE_CHARS = "'\"";

  /** white spaces
   */
  public final static String WHITE_SPACES = " \t\f\r\n";

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** trim characters from string at string beginning
   * @param string string
   * @param chars characters to trim
   * @return trimmed string
   */
  public static String trimBegin(String string, String chars)
  {
    int i = 0;
    while (   (i < string.length())
           && (chars.indexOf(string.charAt(i)) >= 0)
          )
    {
      i++;
    }

    return string.substring(i);
  }

  /** trim characters from string at string beginning
   * @param string string
   * @return trimmed string
   */
  public static String trimBegin(String string)
  {
    return trimBegin(string,WHITE_SPACES);
  }

  /** trim characters from string at string end
   * @param string string
   * @param chars characters to trim
   * @return trimmed string
   */
  public static String trimEnd(String string, String chars)
  {
    int i = string.length()-1;
    while (   (i > 0)
           && (chars.indexOf(string.charAt(i)) >= 0)
          )
    {
      i--;
    }

    return string.substring(0,i+1);
  }

  /** trim characters from string at string beginning
   * @param string string
   * @return trimmed string
   */
  public static String trimEnd(String string)
  {
    return trimEnd(string,WHITE_SPACES);
  }

  /** trim characters from string
   * @param string string
   * @param chars characters to trim
   */
  public static String trim(String string, String chars)
  {
    int i0 = 0;
    while (   (i0 < string.length())
           && (chars.indexOf(string.charAt(i0)) >= 0)
          )
    {
      i0++;
    }
    int i1 = string.length()-1;
    while (   (i1 >= i0)
           && (chars.indexOf(string.charAt(i1)) >= 0)
          )
    {
      i1--;
    }

    return string.substring(i0,i1-i0+1);
  }

  /** trim characters from string at string beginning
   * @param string string
   * @return trimmed string
   */
  public static String trim(String string)
  {
    return trim(string,WHITE_SPACES);
  }

  /** escape quote character and \ in string, enclose in quote character
   * @param string string to escape
   * @param enclosingQuotes true to add enclosing quotes "
   * @param quoteChar quote character
   * @return escaped string
   */
  public static String escape(String string, boolean enclosingQuotes, char quoteChar)
  {
    StringBuilder buffer = new StringBuilder();

    if (enclosingQuotes) buffer.append(quoteChar);
    for (int index = 0; index < string.length(); index++)
    {
      char ch = string.charAt(index);

      if      (ch == quoteChar)
      {
        buffer.append("\\"+quoteChar);
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
    if (enclosingQuotes) buffer.append(quoteChar);

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
   * @param quoteChar quote character
   * @return escaped string
   */
  public static String escape(String string, char quoteChar)
  {
    return escape(string,true,quoteChar);
  }

  /** escape ' and \ in string, enclose in "
   * @param string string to escape
   * @return escaped string
   */
  public static String escape(String string)
  {
    return escape(string,true);
  }

  /** remove enclosing ' or ", unescape
   * @param string string to unescape
   * @param enclosingQuotes true to remove enclosing quotes ' or "
   * @param quoteChar quote character
   * @return unescaped string
   */
  public static String unescape(String string, boolean enclosingQuotes, char quoteChar)
  {
    int          startIndex,endIndex;
    StringBuffer buffer = new StringBuffer();

    if  (   enclosingQuotes
         && (   (string.startsWith("\"") && string.endsWith("\""))
             || (string.startsWith("'") && string.endsWith("'"))
            )
        )
    {
      startIndex = 1;
      endIndex   = string.length()-1;
    }
    else
    {
      startIndex = 0;
      endIndex   = string.length();
    }

    int index = startIndex;
    while (index < endIndex)
    {
      char ch = string.charAt(index);

      if      ((ch == '\\') && ((index+1) < endIndex) && string.charAt(index+1) == quoteChar)
      {
        buffer.append(quoteChar);
        index += 2;
      }
      else
      {
        buffer.append(ch);
        index += 1;
      }
    }

    return buffer.toString();
  }

  /** remove enclosing ' or ", unescape
   * @param string string to unescape
   * @param enclosingQuotes true to add enclosing quotes "
   * @return unescaped string
   */
  public static String unescape(String string, boolean enclosingQuotes)
  {
    return unescape(string,enclosingQuotes,'"');
  }

  /** remove enclosing ' or ", unescape
   * @param string string to unescape
   * @param quoteChar quote character
   * @return unescaped string
   */
  public static String unescape(String string, char quoteChar)
  {
    return unescape(string,true,quoteChar);
  }

  /** remove enclosing ' or "
   * @param string string to unescape
   * @return unescaped string
   */
  public static String unescape(String string)
  {
    return unescape(string,true);
  }

  /** map strings in string
   * @param string string
   * @param index start index for mapping
   * @param from from string array
   * @param to to string array
   * @return mapped string
   */
  public static String map(String string, int index, String[] from, String[] to)
  {
    StringBuilder buffer = new StringBuilder();
    int           z;
    boolean       replaceFlag;

    assert from.length == to.length;

    while (index < string.length())
    {
      replaceFlag = false;
      for (z = 0; z < from.length; z++)
      {
        if (string.startsWith(from[z],index))
        {
          buffer.append(to[z]);
          index += from[z].length();
          replaceFlag = true;
          break;
        }
      }
      if (!replaceFlag)
      {
        buffer.append(string.charAt(index));
        index++;
      }
    }

    return buffer.toString();
  }

  /** map strings in string
   * @param string string
   * @param from from string array
   * @param to to string array
   * @return mapped string
   */
  public static String map(String string, String[] from, String[] to)
  {
    return map(string,0,from,to);
  }

  /** join string array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static String join(Object[] objects, String joinString, char quoteChar)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;
    if (objects != null)
    {
      for (Object object : objects)
      {
        if (buffer.length() > 0) buffer.append(joinString);
        string = object.toString();
        buffer.append((quoteChar != '\0') ? escape(string,true,quoteChar) : string);
      }
    }

    return buffer.toString();
  }

  /** join string array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quote true iff escape strings
   * @return string
   */
  public static String join(Object[] objects, String joinString, boolean quote)
  {
    return join(objects,joinString,(quote) ? '"' : '\0');
  }

  /** join string array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(Object[] objects, String joinString)
  {
    return join(objects,joinString,'\0');
  }

  /** join string array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(String[] strings)
  {
    return join(strings," ");
  }

  /** join boolean array
   * @param array array to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(boolean[] array, String joinString)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;
    if (array != null)
    {
      for (boolean n : array)
      {
        if (buffer.length() > 0) buffer.append(joinString);
        buffer.append(Boolean.toString(n));
      }
    }

    return buffer.toString();
  }

  /** join boolean array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(boolean[] array)
  {
    return join(array," ");
  }

  /** join integer array
   * @param array array to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(int[] array, String joinString)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;
    if (array != null)
    {
      for (int n : array)
      {
        if (buffer.length() > 0) buffer.append(joinString);
        buffer.append(Integer.toString(n));
      }
    }

    return buffer.toString();
  }

  /** join integer array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(int[] array)
  {
    return join(array," ");
  }

  /** join long array
   * @param array array to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(long[] array, String joinString)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;
    if (array != null)
    {
      for (long n : array)
      {
        if (buffer.length() > 0) buffer.append(joinString);
        buffer.append(Long.toString(n));
      }
    }

    return buffer.toString();
  }

  /** join long array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(long[] array)
  {
    return join(array," ");
  }

  /** join float array
   * @param array array to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(float[] array, String joinString)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;
    if (array != null)
    {
      for (float n : array)
      {
        if (buffer.length() > 0) buffer.append(joinString);
        buffer.append(Float.toString(n));
      }
    }

    return buffer.toString();
  }

  /** join float array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(float[] array)
  {
    return join(array," ");
  }

  /** join double array
   * @param array array to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(double[] array, String joinString)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;
    if (array != null)
    {
      for (double n : array)
      {
        if (buffer.length() > 0) buffer.append(joinString);
        buffer.append(Double.toString(n));
      }
    }

    return buffer.toString();
  }

  /** join double array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(double[] array)
  {
    return join(array," ");
  }

  /** join double array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(EnumSet enumSet, String joinString, boolean ordinal)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;
    if (enumSet != null)
    {
      Iterator iterator = enumSet.iterator();
      while (iterator.hasNext())
      {
        Enum value = (Enum)iterator.next();
        if (buffer.length() > 0) buffer.append(joinString);
        buffer.append((ordinal) ? Integer.toString(value.ordinal()) : value.toString());
      }
    }

    return buffer.toString();
  }

  /** join double array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(EnumSet enumSet, String joinString)
  {
    return join(enumSet,joinString,false);
  }

  /** join double array with space
   * @param strings strings to join
   * @return string
   */
  public static String join(EnumSet enumSet)
  {
    return join(enumSet,",");
  }

  /** split string
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string array
   */
  public static String[] split(String string, String splitChars, String spaceChars, String quoteChars, boolean emptyFlag)
  {
    ArrayList<String> stringList = new ArrayList<String>();
//Dprintf.dprintf("string=%s splitChars=%s spaceChars=%s quoteChars=%s em=%s",string,splitChars,spaceChars,quoteChars,emptyFlag);

    char[]        chars  = string.toCharArray();
    int           i      = 0;
    int           n      = chars.length;
    StringBuilder buffer = new StringBuilder();
    while (i < n)
    {
      if (spaceChars != null)
      {
        // skip spaces
        while ((i < n) && (spaceChars.indexOf(chars[i]) >= 0))
        {
          i++;
        }
      }

      // get next word, respect quotes
      buffer.setLength(0);
      while ((i < n) && splitChars.indexOf(chars[i]) == -1)
      {
        if      (chars[i] == '\\')
        {
          // escaped character
          if (i+1 < n) buffer.append(chars[i+1]);
          i += 2;
        }
        else if ((quoteChars != null) && (quoteChars.indexOf(chars[i]) >= 0))
        {
          // quote
          char quoteChar = chars[i];
          i += 1;
          while ((i < n) && (chars[i] != quoteChar))
          {
            if      (chars[i] == '\\')
            {
              // escaped character
              if (i+1 < n) buffer.append(chars[i+1]);
              i += 2;
            }
            else
            {
              // character
              buffer.append(chars[i]);
              i += 1;
            }
          }
          i += 1;
        }
        else
        {
          // character
          buffer.append(chars[i]);
          i += 1;
        }
      }
      i += 1;

      // add to list
      if (emptyFlag || (buffer.length() > 0))
      {
        stringList.add(buffer.toString());
      }
    }

    return stringList.toArray(new String[0]);
  }

  /** split string
   * @param string string to split
   * @param splitChar character used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string array
   */
  public static String[] split(String string, char splitChar, String spaceChars, String quoteChars, boolean emptyFlag)
  {
    return split(string,new String(new char[]{splitChar}),spaceChars,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return string array
   */
  public static String[] split(String string, String splitChars, String quoteChars, boolean emptyFlag)
  {
    return split(string,splitChars,WHITE_SPACES,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChar characters used for splitting
   * @param quoteChars quote characters
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return string array
   */
  public static String[] split(String string, char splitChar, String quoteChars, boolean emptyFlag)
  {
    return split(string,splitChar,WHITE_SPACES,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters
   * @return string array
   */
  public static String[] split(String string, String splitChars, String quoteChars)
  {
    return split(string,splitChars,WHITE_SPACES,quoteChars,true);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChar character used for splitting
   * @param quoteChars quote characters
   * @return string array
   */
  public static String[] split(String string, char splitChar, String quoteChars)
  {
    return split(string,splitChar,WHITE_SPACES,quoteChars,true);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return string array
   */
  public static String[] split(String string, String splitChars, boolean emptyFlag)
  {
    return split(string,splitChars,null,null,emptyFlag);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChar character used for splitting
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return string array
   */
  public static String[] split(String string, char splitChar, boolean emptyFlag)
  {
    return split(string,splitChar,null,null,emptyFlag);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChars characters used for splitting
   * @return string array
   */
  public static String[] split(String string, String splitChars)
  {
    return split(string,splitChars,true);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChar character used for splitting
   * @return string array
   */
  public static String[] split(String string, char splitChar)
  {
    return split(string,splitChar,true);
  }

  /** split string (no quotes) at white-spaces
   * @param string string to split
   * @return string array
   */
  public static String[] split(String string)
  {
    return split(string,WHITE_SPACES);
  }

  /** replace string in stringn array
   * @param strings string array
   * @param from,to from/to string
   */
  public static void replace(String[] strings, String from, String to)
  {
    for (int z = 0; z < strings.length; z++)
    {
      strings[z] = strings[z].replace(from,to);
    }
  }

  /** create n-concationation of string
   * @param string string to repeat
   * @param number of concatations
   * @return string+...+string (count times)
   */
  public static String repeat(String string, int count)
  {
    StringBuilder buffer = new StringBuilder();

    for (int z = 0; z < count; z++)
    {
      buffer.append(string);
    }

    return buffer.toString();
  }

  /** create n-concationation of string
   * @param ch charater to repeat
   * @param number of concatations
   * @return string+...+string (count times)
   */
  public static String repeat(char ch, int count)
  {
    StringBuilder buffer = new StringBuilder();

    for (int z = 0; z < count; z++)
    {
      buffer.append(ch);
    }

    return buffer.toString();
  }

  /** convert glob-pattern into regex-pattern
   * @param string glob-pattern
   * @return regex-pattern
   */
  public static String globToRegex(String string)
  {
    StringBuilder buffer = new StringBuilder();

    int z = 0;
    while (z < string.length())
    {
      switch (string.charAt(z))
      {
        case '*':
          buffer.append(".*");
          z++;
          break;
        case '?':
          buffer.append('.');
          z++;
          break;
        case '.':
          buffer.append("\\.");
          z++;
          break;
        case '\\':
          buffer.append("\\\\");
          z++;
          break;
        case '[':
        case ']':
        case '^':
        case '$':
        case '(':
        case ')':
        case '{':
        case '}':
        case '+':
        case '|':
          buffer.append('\\');
          buffer.append(string.charAt(z));
          z++;
          break;
        default:
          buffer.append(string.charAt(z));
          z++;
          break;
      }
    }

    return buffer.toString();
  }

  /** parse string with boolean value
   * @param string string
   * @return boolean value or false
   */
  public static boolean parseBoolean(String string)
  {
    final String TRUE_STRINGS[] =
    {
      "1",
      "true",
      "yes",
      "on",
    };

    for (String trueString : TRUE_STRINGS)
    {
      if (string.equalsIgnoreCase(trueString)) return true;
    }

    return false;
  }

  /** parse string with enum value
   * @param enumClass enum class
   * @param string string
   * @return enum value or null
   */
  public static Enum parseEnum(Class enumClass, String string)
  {
    int n;
    try
    {
      n = Integer.parseInt(string);
    }
    catch (NumberFormatException exception)
    {
      n = -1;
    }
    for (Enum enumConstant : (Enum[])enumClass.getEnumConstants())
    {
      if (   string.equalsIgnoreCase(enumConstant.toString())
          || (enumConstant.ordinal() == n)
         )
      {
        return enumConstant;
      }
    }

    return null;
  }

  /** parse string with enum set value
   * @param enumClass enum class
   * @param string string
   * @return enum set value or null
   */
  public static EnumSet parseEnumSet(Class enumClass, String string)
  {
    EnumSet enumSet = EnumSet.noneOf(enumClass);

    Enum[] enumConstants = (Enum[])enumClass.getEnumConstants();
    for (String value : split(string,",",false))
    {
      int n;
      try
      {
        n = Integer.parseInt(value);
      }
      catch (NumberFormatException exception)
      {
        n = -1;
      }
      for (Enum enumConstant : enumConstants)
      {
        if (   value.equalsIgnoreCase(enumConstant.toString())
            || (enumConstant.ordinal() == n)
           )
        {
          enumSet.add(enumConstant);
        }
      }
    }

    return enumSet;
  }
}

/* end of file */
