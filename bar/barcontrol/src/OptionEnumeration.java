/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/OptionEnumeration.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: command line option functions for enumerations
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/

/****************************** Classes ********************************/

/** enumeration options
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
