/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Option.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: command line option
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/

/****************************** Classes ********************************/

/** option
 */
public class Option
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  public String              name;
  public String              shortName;
  public Options.Types       type;
  public String              fieldName;
  public OptionEnumeration[] enumeration;
  public OptionSpecial       special;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   */
  public Option(String name, String shortName, Options.Types type, String fieldName)
  {
    this.name        = name;
    this.shortName   = shortName;
    this.type        = type;
    this.fieldName   = fieldName;
  }

  /** create enumeration option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   * @param enumeration enumeration values
   */
  public Option(String name, String shortName, Options.Types type, String fieldName, OptionEnumeration[] enumeration)
  {
    this.name        = name;
    this.shortName   = shortName;
    this.type        = type;
    this.fieldName   = fieldName;
    this.enumeration = enumeration;
  }

  /** create special option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   * @param enumeration enumeration values
   */
  public Option(String name, String shortName, Options.Types type, String fieldName, OptionSpecial special)
  {
    this.name      = name;
    this.shortName = shortName;
    this.type      = type;
    this.fieldName = fieldName;
    this.special   = special;
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    return "{"+name+", "+shortName+", "+type+", "+fieldName+"}";
  }
}

/* end of file */
