/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TableLayoutData.java,v $
* $Revision: 1.6 $
* $Author: torsten $
* Contents:
* Systems:
*
\***********************************************************************/

/****************************** Imports ********************************/
import org.eclipse.swt.SWT;

/****************************** Classes ********************************/

/* end of file */

public class TableLayoutData
{
  // --------------------------- constants --------------------------------

  public final static int NONE    = TableLayout.NONE;

  public final static int N       = TableLayout.N;
  public final static int S       = TableLayout.S;
  public final static int W       = TableLayout.W;
  public final static int E       = TableLayout.E;
  public final static int NS      = TableLayout.NS;
  public final static int WE      = TableLayout.WE;
  public final static int NW      = TableLayout.NW;
  public final static int NE      = TableLayout.NE;
  public final static int SW      = TableLayout.SW;
  public final static int SE      = TableLayout.SE;
  public final static int NSWE    = TableLayout.NSWE;

  public final static int DEFAULT = TableLayout.DEFAULT;

  // --------------------------- variables --------------------------------

  // true iff widget should be exclued (not drawn)
  public int     minWidth  = SWT.DEFAULT;
  public int     minHeight = SWT.DEFAULT;
  public int     maxWidth  = SWT.DEFAULT;
  public int     maxHeight = SWT.DEFAULT;
  public boolean exclude   = false;

  protected int row,column;
  protected int style;
  protected int rowSpawn,columnSpawn;
  protected int padX,padY;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param minWidth,minHeight min. width/height
   * @param maxWidth,maxHeight max. width/height
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int minWidth, int minHeight, int maxWidth, int maxHeight)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = Math.max(1,rowSpawn);
    this.columnSpawn = Math.max(1,columnSpawn);
    this.padX        = padX;
    this.padY        = padY;
    this.minWidth    = minWidth;
    this.minHeight   = minHeight;
    this.maxWidth    = maxWidth;
    this.maxHeight   = maxHeight;
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param width,height min./max. width/height
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height)
  {
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY)
  {
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,SWT.DEFAULT,SWT.DEFAULT);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param pad padding X/Z
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int pad)
  {
    this(row,column,style,rowSpawn,columnSpawn,pad,pad);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    this(row,column,style,rowSpawn,columnSpawn,0);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   */
  TableLayoutData(int row, int column, int style)
  {
    this(row,column,style,1,1);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   */
  TableLayoutData(int row, int column)
  {
    this(row,column,DEFAULT);
  }

  public String toString()
  {
    StringBuffer s;

    s = new StringBuffer();
    s.append("row="+row);
    s.append(" column="+column);
    s.append(" style=(");
    if ((style & N) == N) s.append("N");
    if ((style & S) == S) s.append("S");
    if ((style & W) == W) s.append("W");
    if ((style & E) == E) s.append("E");
    s.append(")");
    s.append(" spawn=("+rowSpawn+","+columnSpawn+")");
    s.append(" pad=("+padX+","+padY+")");
    s.append(" min=("+minWidth+","+minHeight+")");
    s.append(" max=("+maxWidth+","+maxHeight+")");

    return s.toString();
  }
}

/* end of file */
