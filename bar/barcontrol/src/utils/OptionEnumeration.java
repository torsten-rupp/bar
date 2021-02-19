/***********************************************************************\
*
* $Revision: 1564 $
* $Date: 2016-12-24 16:12:38 +0100 (Sat, 24 Dec 2016) $
* $Author: torsten $
* Contents: command line option functions for enumerations
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.util.AbstractSet;
import java.util.EnumSet;
import java.util.HashSet;

/****************************** Classes ********************************/

/** enumeration option
 */
abstract class OptionEnum<E extends Enum>
{
  /** parse string into enum value
   * @param string string
   * @param value value
   */
  abstract E parse(String string);
}

/** enumeration option
 */
public class OptionEnumeration
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  public String name;
  public Object value;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create enumeration option
   * @param name name
   * @param value value
   */
  public OptionEnumeration(String name, Object value)
  {
    this.name  = name;
    this.value = value;
  }
}

/* end of file */
