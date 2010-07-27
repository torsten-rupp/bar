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

/****************************** Classes ********************************/

/** string utility functions
 */
public class StringUtils
{
  // --------------------------- constants --------------------------------
  /** quoting characters for string values
   */
  public static String QUOTE_CHARS = "'\"";

  /** white spaces
   */
  public final static String WHITE_SPACES = " \t\f\r\n";

  // --------------------------- variables --------------------------------

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** escape ' and \ in string, enclose in "
   * @param string string to escape
   * @param enclosingQuotes true to add enclosing quotes "
   * @param quoteChar quote character
   * @return escaped string
   */
  public static String escape(String string, boolean enclosingQuotes, char quoteChar)
  {
    StringBuffer buffer = new StringBuffer();

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

  /** join string array
   * @param objects objects to join (convert to string with toString())
   * @param joinString string used to join two strings
   * @param quoteChar quote char
   * @return string
   */
  public static String join(Object[] objects, String joinString, char quoteChar)
  {
    StringBuffer buffer = new StringBuffer();
    String       string;
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

  /** split string
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param spaceChars characters skipped (spaces)
   * @param quoteChars quote characters
   * @return string array
   */
  public static String[] split(String string, String splitChars, String spaceChars, String quoteChars)
  {
    ArrayList<String> stringList = new ArrayList<String>();

    char[]        chars  = string.toCharArray();
    int           i      = 0;
    int           n      = chars.length;
    StringBuilder buffer = new StringBuilder();
    while (i < n)
    {
      // skip spaces
      while (splitChars.indexOf(chars[i]) >= 0)
      {
        i++;
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
      if (buffer.length() > 0) stringList.add(buffer.toString());
    }

    return stringList.toArray(new String[0]);
  }

  /** split string, discard white spaces between strings
   * @param string string to split
   * @param splitChars characters used for splitting
   * @param quoteChars quote characters
   * @return string array
   */
  public static String[] split(String string, String splitChars, String quoteChars)
  {
    return split(string,splitChars,WHITE_SPACES,quoteChars);
  }

  /** split string (no quotes)
   * @param string string to split
   * @param splitChars characters used for splitting
   * @return string array
   */
  public static String[] split(String string, String splitChars)
  {
    return split(string,splitChars,WHITE_SPACES,null);
  }

  /** create n-concationation of string 
   * @param string string to contact
   * @param number of concatations
   * @return string+...+string (count times)
   */
  public static String multiply(String string, int count)
  {
    StringBuffer buffer = new StringBuffer();
 
    for (int z = 0; z < count; z++)
    {
      buffer.append(string);
    }
 
    return buffer.toString();
  }
}

/* end of file */
