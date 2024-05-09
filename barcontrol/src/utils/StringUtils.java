/***********************************************************************\
*
* Contents: String utility functions
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.util.AbstractList;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.Collection;
import java.util.EnumSet;
import java.util.Iterator;

/****************************** Classes ********************************/

/** string utility functions
 */
public class StringUtils
{
  // -------------------------- constants -------------------------------
  /** quoting characters for string values
   */
  public final static String QUOTE_CHARS = "'\"";

  /** default quoting character for string values
   */
  public final static char DEFAULT_QUOTE_CHAR = '"';

  /** default quoting character for string values
   */
  public final static String DEFAULT_QUOTE_STRING = "\"";

  /** white spaces
   */
  public final static String WHITE_SPACES = " \t\f\r\n";

  /** escape character
   */
  public final static Character ESCAPE_CHAR = '\\';

  // -------------------------- variables -------------------------------

  // ----------------------- native functions ---------------------------

  // --------------------------- methods --------------------------------

  /** check if string is null or empty
   * @param string string
   * @return true iff null or empty
   */
  public static boolean isEmpty(String string)
  {
    return (string == null) || string.isEmpty();
  }

  /** find string in string array
   * @param strings string array
   * @param string string to find
   * @return index or -1 if not found
   */
  public static int indexOf(String strings[], String string)
  {
    for (int i = 0; i < strings.length; i++)
    {
      if (strings[i].equals(string)) return i;
    }

    return -1;
  }

  /** find string in string list
   * @param stringList string list
   * @param string string to find
   * @return index or -1 if not found
   */
  public static int indexOf(List<String> stringList, String string)
  {
    for (int i = 0; i < stringList.size(); i++)
    {
      if (stringList.get(i).equals(string)) return i;
    }

    return -1;
  }

  /** check if string contains in string array
   * @param strings string array
   * @param string string to find
   * @return true if string array contains string
   */
  public static boolean contains(String strings[], String string)
  {
    return indexOf(strings,string) >= 0;
  }

  /** check if string contains in string list
   * @param stringList string list
   * @param string string to find
   * @return true if string list contains string
   */
  public static boolean contains(List<String> stringList, String string)
  {
    return indexOf(stringList,string) >= 0;
  }

  /** trim characters from strings at string beginning
   * @param strings strings
   * @param chars characters to trim
   * @return trimmed strings
   */
  public static String[] trimBegin(String strings[], String chars)
  {
    ArrayList<String> stringList = new ArrayList<String>();

    for (String string : strings)
    {
      int i = 0;
      while (   (i < string.length())
             && (chars.indexOf(string.charAt(i)) >= 0)
            )
      {
        i++;
      }
      stringList.add(string.substring(i));
    }

    return stringList.toArray(new String[stringList.size()]);
  }

  /** trim characters from strings at string beginning
   * @param strings strings
   * @param chars characters to trim
   * @return trimmed strings
   */
  public static String[] trimBegin(String strings[], char ch)
  {
    ArrayList<String> stringList = new ArrayList<String>();

    for (String string : strings)
    {
      int i = 0;
      while (   (i < string.length())
             && (string.charAt(i) == ch)
            )
      {
        i++;
      }
      stringList.add(string.substring(i));
    }

    return stringList.toArray(new String[stringList.size()]);
  }

  /** trim characters from strings at string beginning
   * @param strings strings
   * @param chars characters to trim
   * @return trimmed strings
   */
  public static String[] trimBegin(String strings[])
  {
    return trimBegin(strings,WHITE_SPACES);
  }

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
   * @param ch character to trim
   * @return trimmed string
   */
  public static String trimBegin(String string, char ch)
  {
    int i = 0;
    while (   (i < string.length())
           && (string.charAt(i) == ch)
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

  /** trim characters from strings at string end
   * @param strings strings
   * @param chars characters to trim
   * @return trimmed strings
   */
  public static String[] trimEnd(String strings[], String chars)
  {
    ArrayList<String> stringList = new ArrayList<String>();

    for (String string : strings)
    {
      int i = string.length()-1;
      while (   (i >= 0)
             && (chars.indexOf(string.charAt(i)) >= 0)
            )
      {
        i--;
      }

      stringList.add(string.substring(i));
    }

    return stringList.toArray(new String[stringList.size()]);
  }

  /** trim characters from strings at string end
   * @param strings strings
   * @param chars characters to trim
   * @return trimmed strings
   */
  public static String[] trimEnd(String strings[], char ch)
  {
    ArrayList<String> stringList = new ArrayList<String>();

    for (String string : strings)
    {
      int i = string.length()-1;
      while (   (i > 0)
             && (string.charAt(i) == ch)
            )
      {
        i--;
      }

      stringList.add(string.substring(i));
    }

    return stringList.toArray(new String[stringList.size()]);
  }

  /** trim characters from string at strings end
   * @param strings strings
   * @return trimmed strings
   */
  public static String[] trimEnd(String strings[])
  {
    return trimEnd(strings,WHITE_SPACES);
  }

  /** trim characters from string at string end
   * @param string string
   * @param chars characters to trim
   * @return trimmed string
   */
  public static String trimEnd(String string, String chars)
  {
    int i = string.length()-1;
    while (   (i >= 0)
           && (chars.indexOf(string.charAt(i)) >= 0)
          )
    {
      i--;
    }

    return string.substring(0,i+1);
  }

  /** trim characters from string at string end
   * @param string string
   * @param chars characters to trim
   * @return trimmed string
   */
  public static String trimEnd(String string, char ch)
  {
    int i = string.length()-1;
    while (   (i > 0)
           && (string.charAt(i) == ch)
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

    return string.substring(i0,i1+1);
  }

  /** trim characters from string
   * @param string string
   * @param ch character to trim
   */
  public static String trim(String string, char ch)
  {
    int i0 = 0;
    while (   (i0 < string.length())
           && (string.charAt(i0) == ch)
          )
    {
      i0++;
    }
    int i1 = string.length()-1;
    while (   (i1 >= i0)
           && (string.charAt(i1) == ch)
          )
    {
      i1--;
    }

    return string.substring(i0,i1+1);
  }

  /** trim characters from string at string beginning
   * @param string string
   * @return trimmed string
   */
  public static String trim(String string)
  {
    return trim(string,WHITE_SPACES);
  }

  /** trim characters from string
   * @param string string
   * @param chars characters to trim
   */
  public static String[] trim(String strings[], String chars)
  {
    ArrayList<String> stringList = new ArrayList<String>();

    for (String string : strings)
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

      stringList.add(string.substring(i0,i1+1));
    }

    return stringList.toArray(new String[stringList.size()]);
  }

  /** trim characters from string
   * @param string string
   * @param ch character to trim
   */
  public static String[] trim(String strings[], char ch)
  {
    ArrayList<String> stringList = new ArrayList<String>();

    for (String string : strings)
    {
      int i0 = 0;
      while (   (i0 < string.length())
             && (string.charAt(i0) == ch)
            )
      {
        i0++;
      }
      int i1 = string.length()-1;
      while (   (i1 >= i0)
             && (string.charAt(i1) == ch)
            )
      {
        i1--;
      }

      stringList.add(string.substring(i0,i1+1));
    }

    return stringList.toArray(new String[stringList.size()]);
  }

  /** trim characters from string at string beginning
   * @param string string
   * @return trimmed string
   */
  public static String[] trim(String strings[])
  {
    return trim(strings,WHITE_SPACES);
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
    if (string != null)
    {
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

  /** remove enclosing quote and unescape
   * @param string string to unescape
   * @param enclosingQuotes true to remove enclosing quotes ' or "
   * @param quoteChars quote characters
   * @return unescaped string
   */
  public static String unescape(String string, boolean enclosingQuotes, String quoteChars)
  {
    boolean      quotedFlag;
    char         quoteChar;
    int          startIndex,endIndex;
    StringBuffer buffer = new StringBuffer();

    if (string != null)
    {
      quotedFlag = false;
      quoteChar  = '\0';
      startIndex = 0;
      endIndex   = string.length();

      // check for outer quotes
      if ((enclosingQuotes) && (string.length() >= 2))
      {
        for (int i = 0; i < quoteChars.length(); i++)
        {
          quoteChar = quoteChars.charAt(i);
          if  (   (string.charAt(0) == quoteChar)
               && (string.charAt(string.length()-1) == quoteChar)
              )
          {
            quotedFlag = true;
            startIndex = 1;
            endIndex   = string.length()-1;
            break;
          }
        }
      }

      // unescape
      int index = startIndex;
      while (index < endIndex)
      {
        char ch = string.charAt(index);

        if      ((ch == ESCAPE_CHAR) && ((index+1) < endIndex) && quotedFlag && (string.charAt(index+1) == quoteChar))
        {
          buffer.append(quoteChar);
          index += 2;
        }
        else if ((ch == ESCAPE_CHAR) && ((index+1) < endIndex))
        {
          buffer.append(string.charAt(index+1));
          index += 2;
        }
        else
        {
          buffer.append(ch);
          index += 1;
        }
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
    return unescape(string,enclosingQuotes,"\"'");
  }

  /** remove enclosing ' or ", unescape
   * @param string string to unescape
   * @param quoteChar quote character
   * @return unescaped string
   */
  public static String unescape(String string, char quoteChar)
  {
    return unescape(string,true,Character.toString(quoteChar));
  }

  /** remove enclosing ' or "
   * @param string string to unescape
   * @return unescaped string
   */
  public static String unescape(String string)
  {
    return unescape(string,true);
  }

  /** enclose in quote character if string contain special characters
   * @param string string
   * @param quoteChar quote character
   * @return quoted string
   */
  public static String quote(String string, String specialChars, char quoteChar)
  {
    StringBuilder buffer = new StringBuilder();

    boolean containSpecialCharacters = false;
    if (specialChars != null)
    {
      for (int i = 0; i < specialChars.length(); i++)
      {
        if (string.indexOf(specialChars.charAt(i)) >= 0)
        {
          containSpecialCharacters = true;
          break;
        }
      }
    }

    if ((specialChars == null) || containSpecialCharacters)
    {
      buffer.append(quoteChar);
      if (string != null)
      {
        for (int i = 0; i < string.length(); i++)
        {
          char ch = string.charAt(i);

          if      (ch == quoteChar)
          {
            buffer.append("\\"+quoteChar);
          }
          else
          {
            buffer.append(ch);
          }
        }
      }
      buffer.append(quoteChar);

      return buffer.toString();
    }
    else
    {
      return string;
    }
  }

  /** enclose in quote character
   * @param string string
   * @param quoteChar quote character
   * @return quoted string
   */
  public static String quote(String string, char quoteChar)
  {
    return quote(string,(String)null,quoteChar);
  }

  /** enclose in "
   * @param string string
   * @return quoted string
   */
  public static String quote(String string)
  {
    return quote(string,'"');
  }

  /** remove enclosing quote
   * @param string string
   * @param enclosingQuotes true to remove enclosing quotes ' or "
   * @param quoteChars quote characters
   * @return unescaped string
   */
  public static String unquote(String string, String quoteChars)
  {
    boolean      quotedFlag;
    char         quoteChar;
    int          startIndex,endIndex;
    StringBuffer buffer = new StringBuffer();

    if (string != null)
    {
      quotedFlag = false;
      quoteChar  = '\0';
      startIndex = 0;
      endIndex   = string.length();

      // check for outer quotes
      if (string.length() >= 2)
      {
        for (int i = 0; i < quoteChars.length(); i++)
        {
          quoteChar = quoteChars.charAt(i);
          if  (   (string.charAt(0) == quoteChar)
               && (string.charAt(string.length()-1) == quoteChar)
              )
          {
            quotedFlag = true;
            startIndex = 1;
            endIndex   = string.length()-1;
            break;
          }
        }
      }

      // unescape quote characters
      int index = startIndex;
      while (index < endIndex)
      {
        char ch = string.charAt(index);

        if ((ch == ESCAPE_CHAR) && ((index+1) < endIndex) && quotedFlag && (string.charAt(index+1) == quoteChar))
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
    }

    return buffer.toString();
  }

  /** remove enclosing ' or "
   * @param string string
   * @param quoteChar quote character
   * @return unescaped string
   */
  public static String unquote(String string, char quoteChar)
  {
    return unquote(string,Character.toString(quoteChar));
  }

  /** remove enclosing ' or "
   * @param string string
   * @param enclosingQuotes true to add enclosing quotes "
   * @return unescaped string
   */
  public static String unquote(String string)
  {
    return unquote(string,"\"'");
  }

  /** replace char in string
   * @param string string
   * @param fromChar,toChar from/to character
   * @return string
   */
  public static String replace(String string, char fromChar, char toChar)
  {
    StringBuilder buffer = new StringBuilder();
    char          ch;

    for (int i = 0; i < string.length(); i++)
    {
      ch = string.charAt(i);
      buffer.append((ch == fromChar) ? toChar : ch);
    }

    return buffer.toString();
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

  /** join collection
   * @param collection collection to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static String join(Collection collection, String joinString, char quoteChar)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;
    if (collection != null)
    {
      for (Object object : collection)
      {
        if (buffer.length() > 0) buffer.append(joinString);
        string = object.toString();
        buffer.append((quoteChar != '\0') ? escape(string,true,quoteChar) : string);
      }
    }

    return buffer.toString();
  }

  /** join collection
   * @param collection collection to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quote true iff escape strings
   * @return string
   */
  public static String join(Collection collection, String joinString, boolean quote)
  {
    return join(collection,joinString,(quote) ? '"' : '\0');
  }

  /** join collection
   * @param collection collection to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(Collection collection, String joinString)
  {
    return join(collection,joinString,'\0');
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static <T> String join(List<T> objects, int index, int count, String joinString, char quoteChar)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;

    if (index < 0) throw new IndexOutOfBoundsException();

    if ((objects != null) && (index < objects.size()))
    {
      if (count > objects.size()) throw new IndexOutOfBoundsException();

      if (count < 0)
      {
        count = objects.size()-index;
      }

      for (int i = 0; i < count; i++)
      {
        Object object = objects.get(index+i);

        if (object != null)
        {
          if (buffer.length() > 0) buffer.append(joinString);
          string = object.toString();
          buffer.append((quoteChar != '\0') ? escape(string,true,quoteChar) : string);
        }
      }
    }

    return buffer.toString();
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static <T> String join(List<T> objects, String joinString, char quoteChar)
  {
    return join(objects,0,-1,joinString,quoteChar);
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static <T> String join(List<T> objects, int index, int count, char joinChar, char quoteChar)
  {
    return join(objects,index,count,Character.toString(joinChar),quoteChar);
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static <T> String join(List<T> objects, char joinChar, char quoteChar)
  {
    return join(objects,0,-1,joinChar,quoteChar);
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteFlag true iff escape strings
   * @return string
   */
  public static <T> String join(List<T> objects, int index, int count, String joinString, boolean quoteFlag)
  {
    return join(objects,index,count,joinString,(quoteFlag) ? '"' : '\0');
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteFlag true iff escape strings
   * @return string
   */
  public static <T> String join(List<T> objects, String joinString, boolean quoteFlag)
  {
    return join(objects,0,-1,joinString,(quoteFlag) ? '"' : '\0');
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @param quoteFlag true iff escape strings
   * @return string
   */
  public static <T> String join(List<T> objects, int index, int count, char joinChar, boolean quoteFlag)
  {
    return join(objects,index,count,Character.toString(joinChar),quoteFlag);
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @param quoteFlag true iff escape strings
   * @return string
   */
  public static <T> String join(List<T> objects, char joinChar, boolean quoteFlag)
  {
    return join(objects,0,-1,Character.toString(joinChar),quoteFlag);
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static <T> String join(List<T> objects, int index, int count, String joinString)
  {
    return join(objects,index,count,joinString,'\0');
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static <T> String join(List<T> objects, String joinString)
  {
    return join(objects,0,-1,joinString);
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @return string
   */
  public static <T> String join(List<T> objects, int index, int count, char joinChar)
  {
    return join(objects,index,count,Character.toString(joinChar));
  }

  /** join object list
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @return string
   */
  public static <T> String join(List<T> objects, char joinChar)
  {
    return join(objects,0,-1,joinChar);
  }

  /** join object list with single space
   * @param objects objects to join (convert to string with toString())
   * @return string
   */
  public static <T> String join(List<T> objects)
  {
    return join(objects,0,-1," ");
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static String join(Object[] objects, int index, int count, String joinString, char quoteChar)
  {
    StringBuilder buffer = new StringBuilder();
    String        string;

    if (index < 0) throw new IndexOutOfBoundsException();

    if ((objects != null) && (index < objects.length))
    {
      if (count > objects.length) throw new IndexOutOfBoundsException();

      if (count < 0)
      {
        count = objects.length-index;
      }

      for (int i = 0; i < count; i++)
      {
        Object object = objects[index+i];

        if (object != null)
        {
          if (buffer.length() > 0) buffer.append(joinString);
          string = object.toString();
          buffer.append((quoteChar != '\0') ? escape(string,true,quoteChar) : string);
        }
      }
    }

    return buffer.toString();
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static String join(Object[] objects, String joinString, char quoteChar)
  {
    return join(objects,0,-1,joinString,quoteChar);
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static String join(Object[] objects, int index, int count, char joinChar, char quoteChar)
  {
    return join(objects,index,count,Character.toString(joinChar),quoteChar);
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static String join(Object[] objects, char joinChar, char quoteChar)
  {
    return join(objects,0,-1,joinChar,quoteChar);
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteFlag true iff escape strings
   * @return string
   */
  public static String join(Object[] objects, int index, int count, String joinString, boolean quoteFlag)
  {
    return join(objects,index,count,joinString,(quoteFlag) ? '"' : '\0');
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteFlag true iff escape strings
   * @return string
   */
  public static String join(Object[] objects, String joinString, boolean quoteFlag)
  {
    return join(objects,0,-1,joinString,(quoteFlag) ? '"' : '\0');
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @param quoteFlag true iff escape strings
   * @return string
   */
  public static String join(Object[] objects, int index, int count, char joinChar, boolean quoteFlag)
  {
    return join(objects,index,count,Character.toString(joinChar),quoteFlag);
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @param quoteFlag true iff escape strings
   * @return string
   */
  public static String join(Object[] objects, char joinChar, boolean quoteFlag)
  {
    return join(objects,0,-1,Character.toString(joinChar),quoteFlag);
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(Object[] objects, int index, int count, String joinString)
  {
    return join(objects,index,count,joinString,'\0');
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @return string
   */
  public static String join(Object[] objects, String joinString)
  {
    return join(objects,0,-1,joinString);
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @return string
   */
  public static String join(Object[] objects, int index, int count, char joinChar)
  {
    return join(objects,index,count,Character.toString(joinChar));
  }

  /** join object array
   * @param objects objects to join (convert to string with toString())
   * @param joinChar character used to join two strings
   * @return string
   */
  public static String join(Object[] objects, char joinChar)
  {
    return join(objects,0,-1,joinChar);
  }

  /** join string array with single space
   * @param objects objects to join (convert to string with toString())
   * @return string
   */
  public static String join(Object[] objects)
  {
    return join(objects,0,-1," ");
  }

  /** join string array with single space
   * @param strings strings to join
   * @return string
   */
  public static String join(String[] strings, int index, int count)
  {
    return join(strings," ");
  }

  /** join string array with single space
   * @param strings strings to join
   * @return string
   */
  public static String join(String[] strings)
  {
    return join(strings,0,-1," ");
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

  /** join boolean array with single space
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

  /** join integer array with single space
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

  /** join long array with single space
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

  /** join float array with single space
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

  /** join double array with single space
   * @param strings strings to join
   * @return string
   */
  public static String join(double[] array)
  {
    return join(array," ");
  }

  /** join enum
   * @param enumSet enums to join
   * @param joinString string used to join two enums
   * @param ordinal true for ordinal values (numbers)
   * @return string
   */
  public static String join(EnumSet enumSet, String joinString, boolean ordinal)
  {
    StringBuilder buffer = new StringBuilder();
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

  /** join enum (names)
   * @param enumSet enums to join
   * @param joinString string used to join two enums
   * @return string
   */
  public static String join(EnumSet enumSet, String joinString)
  {
    return join(enumSet,joinString,false);
  }

  /** join enum (names) with single space
   * @param enumSet enums to join
   * @return string
   */
  public static String join(EnumSet enumSet)
  {
    return join(enumSet,",");
  }

  /** split string
   * @param stringList string list
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param escapeChar escape character (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return extended string list
   */
  private static <T extends AbstractList<String> > T split(T stringList, String string, String splitChars, String spaceChars, String quoteChars, Character escapeChar, boolean emptyFlag)
  {
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
        if      ((escapeChar != null) && (chars[i] == escapeChar))
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
            if      ((escapeChar != null) && (chars[i] == escapeChar))
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

    return stringList;
  }

  /** split string
   * @param stringList string list
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, String splitChars, String spaceChars, String quoteChars, boolean emptyFlag)
  {
    return split(stringList,string,splitChars,spaceChars,quoteChars,(Character)null,emptyFlag);
  }

  /** split string
   * @param stringList string list
   * @param string string to split
   * @param splitChar character used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, char splitChar, String spaceChars, String quoteChars, boolean emptyFlag)
  {
    return split(stringList,string,new String(new char[]{splitChar}),spaceChars,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param stringList string list
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, String splitChars, String quoteChars, boolean emptyFlag)
  {
    return split(stringList,string,splitChars,WHITE_SPACES,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param stringList string list
   * @param string string to split
   * @param splitChar characters used for splitting
   * @param quoteChars quote characters
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, char splitChar, String quoteChars, boolean emptyFlag)
  {
    return split(stringList,string,splitChar,WHITE_SPACES,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param stringList string list
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, String splitChars, String quoteChars)
  {
    return split(stringList,string,splitChars,WHITE_SPACES,quoteChars,true);
  }

  /** split string (no quotes)
   * @param stringList string list
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, String splitChars, boolean emptyFlag)
  {
    return split(stringList,string,splitChars,(String)null,(String)null,emptyFlag);
  }

  /** split string (no quotes)
   * @param stringList string list
   * @param string string to split
   * @param splitChar character used for splitting
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, char splitChar, boolean emptyFlag)
  {
    return split(stringList,string,splitChar,(String)null,(String)null,emptyFlag);
  }

  /** split string (no quotes)
   * @param stringList string list
   * @param string string to split
   * @param splitChars characters used for splitting
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, String splitChars)
  {
    return split(stringList,string,splitChars,true);
  }

  /** split string (no quotes)
   * @param stringList string list
   * @param string string to split
   * @param splitChar character used for splitting
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string, char splitChar)
  {
    return split(stringList,string,splitChar,true);
  }

  /** split string (no quotes) at white-spaces
   * @param stringList string list
   * @param string string to split
   * @return extended string list
   */
  public static <T extends AbstractList<String>> AbstractList<String> split(T stringList, String string)
  {
    return split(stringList,string,WHITE_SPACES);
  }

  /** split string
   * @param clazz string list class
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param escapeChar escape character (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, String splitChars, String spaceChars, String quoteChars, Character escapeChar, boolean emptyFlag)
  {
    try
    {
      return split(clazz.getConstructor().newInstance(),string,splitChars,spaceChars,quoteChars,escapeChar,emptyFlag);
    }
    catch (InstantiationException exception)
    {
      return null;
    }
    catch (IllegalAccessException exception)
    {
      return null;
    }
    catch (java.lang.reflect.InvocationTargetException exception)
    {
      return null;
    }
    catch (NoSuchMethodException exception)
    {
      return null;
    }
  }

  /** split string
   * @param clazz string list class
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, String splitChars, String spaceChars, String quoteChars, boolean emptyFlag)
  {
    return split(clazz,string,splitChars,spaceChars,quoteChars,(Character)null,emptyFlag);
  }

  /** split string
   * @param clazz string list class
   * @param string string to split
   * @param splitChar character used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, char splitChar, String spaceChars, String quoteChars, boolean emptyFlag)
  {
    return split(clazz,string,new String(new char[]{splitChar}),spaceChars,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param clazz string list class
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, String splitChars, String quoteChars, Character escapeChar, boolean emptyFlag)
  {
    return split(clazz,string,splitChars,WHITE_SPACES,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param clazz string list class
   * @param string string to split
   * @param splitChar characters used for splitting
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, char splitChar, String quoteChars, Character escapeChar, boolean emptyFlag)
  {
    return split(clazz,string,splitChar,WHITE_SPACES,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param clazz string list class
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters (can be null)
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, String splitChars, String quoteChars, Character escapeChar)
  {
    return split(clazz,string,splitChars,WHITE_SPACES,quoteChars,true);
  }

  /** split string (no quotes)
   * @param clazz string list class
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, String splitChars, boolean emptyFlag)
  {
    return split(clazz,string,splitChars,(String)null,(String)null,emptyFlag);
  }

  /** split string (no quotes)
   * @param clazz string list class
   * @param string string to split
   * @param splitChar character used for splitting
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, char splitChar, boolean emptyFlag)
  {
    return split(clazz,string,splitChar,(String)null,(String)null,emptyFlag);
  }

  /** split string (no quotes)
   * @param clazz string list class
   * @param string string to split
   * @param splitChars characters used for splitting
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, String splitChars)
  {
    return split(clazz,string,splitChars,true);
  }

  /** split string (no quotes)
   * @param clazz string list class
   * @param string string to split
   * @param splitChar character used for splitting
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string, char splitChar)
  {
    return split(clazz,string,splitChar,true);
  }

  /** split string (no quotes) at white-spaces
   * @param clazz string list class
   * @param string string to split
   * @return string list
   */
  public static <T extends AbstractList<String> > T split(Class<T> clazz, String string)
  {
    return split(clazz,string,WHITE_SPACES);
  }

  /** convert to string array
   * @param strings strings
   * @return string array
   */
  public String[] toArray(Object... objects)
  {
    ArrayList<String> stringList = new ArrayList<String>();
    for (Object object : objects)
    {
      if      (object instanceof String[])
      {
        for (String string : (String[])object)
        {
          stringList.add(string);
        }
      }
      else if (object instanceof List)
      {
        for (Object element : (List)object)
        {
          stringList.add(element.toString());
        }
      }
      else
      {
        stringList.add(object.toString());
      }
    }

    return stringList.toArray(new String[stringList.size()]);
  }

  /** split string
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param escapeChar escape character (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string array
   */
  public static String[] splitArray(String string, String splitChars, String spaceChars, String quoteChars, Character escapeChar, boolean emptyFlag)
  {
    ArrayList<String> stringList = new ArrayList<String>();
    StringUtils.split(stringList,string,splitChars,spaceChars,quoteChars,escapeChar,emptyFlag);

    return stringList.toArray(new String[stringList.size()]);
  }

  /** split string
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string array
   */
  public static String[] splitArray(String string, String splitChars, String spaceChars, String quoteChars, boolean emptyFlag)
  {
    return splitArray(string,splitChars,spaceChars,quoteChars,(Character)null,emptyFlag);
  }

  /** split string
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars spaces characters to skip (can be null)
   * @param quoteChars quote characters (can be null)
   * @param emptyFlag true to return empty parts, false to skip empty parts
   * @return string array
   */
  public static String[] splitArray(String string, char splitChar, String spaceChars, String quoteChars, boolean emptyFlag)
  {
    return splitArray(string,new String(new char[]{splitChar}),spaceChars,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return string array
   */
  public static String[] splitArray(String string, String splitChars, String quoteChars, boolean emptyFlag)
  {
    return splitArray(string,splitChars,WHITE_SPACES,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChar characters used for splitting
   * @param quoteChars quote characters
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return string array
   */
  public static String[] splitArray(String string, char splitChar, String quoteChars, boolean emptyFlag)
  {
    return splitArray(string,splitChar,WHITE_SPACES,quoteChars,emptyFlag);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters
   * @return string array
   */
  public static String[] splitArray(String string, String splitChars, String quoteChars)
  {
    return splitArray(string,splitChars,WHITE_SPACES,quoteChars,true);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChar character used for splitting
   * @param quoteChars quote characters
   * @return string array
   */
  public static String[] splitArray(String string, char splitChar, String quoteChars)
  {
    return splitArray(string,splitChar,WHITE_SPACES,quoteChars,true);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return string array
   */
  public static String[] splitArray(String string, String splitChars, boolean emptyFlag)
  {
    return splitArray(string,splitChars,null,null,emptyFlag);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChar character used for splitting
   * @param emptyFlag TRUE to return empty parts, FALSE to skip empty parts
   * @return string array
   */
  public static String[] splitArray(String string, char splitChar, boolean emptyFlag)
  {
    return splitArray(string,splitChar,null,null,emptyFlag);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChars characters used for splitting
   * @return string array
   */
  public static String[] splitArray(String string, String splitChars)
  {
    return splitArray(string,splitChars,true);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChar character used for splitting
   * @return string array
   */
  public static String[] splitArray(String string, char splitChar)
  {
    return splitArray(string,splitChar,true);
  }

  /** split string (no quotes) at white-spaces
   * @param string string to split
   * @return string array
   */
  public static String[] splitArray(String string)
  {
    return splitArray(string,WHITE_SPACES);
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

  /** create n-concatenation of string
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

  /** create n-concatenation of string
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
    if (string != null)
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
    Class<ArrayList<String>> type;
    for (String value : StringUtils.splitArray(string,",",false))
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
