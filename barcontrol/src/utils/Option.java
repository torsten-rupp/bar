/***********************************************************************\
*
* Contents: command line option
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.util.EnumSet;

/****************************** Classes ********************************/

/** option
 */
public class Option
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  public String        name;
  public String        shortName;
  public Options.Types type;
  public String        fieldName;
  public String        defaultValue;
  public Object[]      units;
  public Object        enumeration;
  public Class         enumerationSetClass;
  public OptionSpecial special;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   * @param units units (array {name,factor})
   */
  public Option(String name, String shortName, Options.Types type, String fieldName, String defaultValue, Object[] units)
  {
    assert (units == null) || (units.length%2 == 0);

    this.name         = name;
    this.shortName    = shortName;
    this.type         = type;
    this.fieldName    = fieldName;
    this.defaultValue = defaultValue;
    this.units        = units;
  }

  /** create option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   */
  public Option(String name, String shortName, Options.Types type, String fieldName, String defaultValue)
  {
    this(name,shortName,type,fieldName,defaultValue,(Object[])null);
  }

  /** create option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   */
  public Option(String name, String shortName, Options.Types type, String fieldName, Object[] units)
  {
    this(name,shortName,type,fieldName,(String)null,units);
  }

  /** create option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   */
  public Option(String name, String shortName, Options.Types type, String fieldName)
  {
    this(name,shortName,type,fieldName,(String)null,(Object[])null);
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

  /** create enumeration option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   * @param enumeration enumeration
   */
  public Option(String name, String shortName, Options.Types type, String fieldName, OptionEnum enumeration)
  {
    this.name        = name;
    this.shortName   = shortName;
    this.type        = type;
    this.fieldName   = fieldName;
    this.enumeration = enumeration;
  }

  /** create enumeration set option
   * @param name name
   * @param shortName short name
   * @param type option type
   * @param fieldName field name to store value
   * @param enumeration enumeration
   */
  public Option(String name, String shortName, Options.Types type, String fieldName, Class enumerationSetClass)
  {
    this.name                = name;
    this.shortName           = shortName;
    this.type                = type;
    this.fieldName           = fieldName;
    this.enumerationSetClass = enumerationSetClass;
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
