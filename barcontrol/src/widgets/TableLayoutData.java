/***********************************************************************\
*
* Contents:
* Systems:
*
\***********************************************************************/

/****************************** Imports ********************************/
import org.eclipse.swt.SWT;

/****************************** Classes ********************************/

public class TableLayoutData
{
  // -------------------------- constants -------------------------------

  public final static int NONE    = TableLayout.NONE;

  public final static int N       = TableLayout.N;
  public final static int S       = TableLayout.S;
  public final static int W       = TableLayout.W;
  public final static int E       = TableLayout.E;
  public final static int NS      = TableLayout.NS;
  public final static int WE      = TableLayout.WE;
  public final static int NW      = TableLayout.NW;
  public final static int NE      = TableLayout.NE;
  public final static int NWE     = TableLayout.NWE;
  public final static int SW      = TableLayout.SW;
  public final static int SE      = TableLayout.SE;
  public final static int SWE     = TableLayout.SWE;
  public final static int NSW     = TableLayout.NSW;
  public final static int NSE     = TableLayout.NSE;
  public final static int NSWE    = TableLayout.NSWE;

  public final static int DEFAULT = TableLayout.DEFAULT;

  // -------------------------- variables -------------------------------

  public int     width     = SWT.DEFAULT;  // requested size
  public int     height    = SWT.DEFAULT;
  public int     minWidth  = SWT.DEFAULT;  // min. size
  public int     minHeight = SWT.DEFAULT;
  public int     maxWidth  = SWT.DEFAULT;  // max. size
  public int     maxHeight = SWT.DEFAULT;
  public boolean isVisible = true;        // true iff widget is visible (drawn)

  protected int row,column;
  protected int style;
  protected int rowSpawn,columnSpawn;
  protected int padX,padY;

  // ----------------------- native functions ---------------------------

  // --------------------------- methods --------------------------------

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param minWidth,minHeight min. width/height
   * @param maxWidth,maxHeight max. width/height
   * @param isVisible true iff visible
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, int minWidth, int minHeight, int maxWidth, int maxHeight, boolean isVisible)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = Math.max(1,rowSpawn);
    this.columnSpawn = Math.max(1,columnSpawn);
    this.padX        = padX;
    this.padY        = padY;
    this.width       = width;
    this.height      = height;
    this.minWidth    = minWidth;
    this.minHeight   = minHeight;
    this.maxWidth    = maxWidth;
    this.maxHeight   = maxHeight;
    this.isVisible   = isVisible;
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param minWidth,minHeight min. width/height
   * @param maxWidth,maxHeight max. width/height
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, int minWidth, int minHeight, int maxWidth, int maxHeight)
  {
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,minWidth,minHeight,maxWidth,maxHeight,true);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param minWidth,minHeight min. width/height
   * @param isVisible true iff visible
   * @param isVisible true iff visible
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, int minWidth, int minHeight, boolean isVisible)
  {
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,minWidth,minHeight,SWT.DEFAULT,SWT.DEFAULT,isVisible);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param minWidth,minHeight min. width/height
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, int minWidth, int minHeight)
  {
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,minWidth,minHeight,true);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param width,height min./max. width/height
   * @param isVisible true iff visible
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height, boolean isVisible)
  {
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,SWT.DEFAULT,SWT.DEFAULT,isVisible);
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
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,width,height,true);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param isVisible true iff visible
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, boolean isVisible)
  {
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,SWT.DEFAULT,SWT.DEFAULT,isVisible);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY)
  {
    this(row,column,style,rowSpawn,columnSpawn,padX,padY,true);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param pad padding X/Z
   * @param isVisible true iff visible
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int pad, boolean isVisible)
  {
    this(row,column,style,rowSpawn,columnSpawn,pad,pad,isVisible);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param pad padding X/Z
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int pad)
  {
    this(row,column,style,rowSpawn,columnSpawn,pad,true);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param isVisible true iff visible
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, boolean isVisible)
  {
    this(row,column,style,rowSpawn,columnSpawn,0,isVisible);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    this(row,column,style,rowSpawn,columnSpawn,true);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param isVisible true iff visible
   */
  TableLayoutData(int row, int column, int style, boolean isVisible)
  {
    this(row,column,style,1,1,isVisible);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   */
  TableLayoutData(int row, int column, int style)
  {
    this(row,column,style,true);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param isVisible true iff visible
   */
  TableLayoutData(int row, int column, boolean isVisible)
  {
    this(row,column,DEFAULT,isVisible);
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   */
  TableLayoutData(int row, int column)
  {
    this(row,column,true);
  }

  /** create table layout data
   * @param isVisible true iff visible
   */
  TableLayoutData(boolean isVisible)
  {
    this(0,0,0,0,0,0,0,0,0,0,0,0,0,isVisible);
  }

  /** create table layout data
   */
  TableLayoutData()
  {
    this(0,0);
  }

  /** convert to string
   * @return string
   */
  public String toString()
  {
    StringBuffer s;

    s = new StringBuffer();
    s.append("TableLayoutData {");
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
    s.append(" size=("+width+","+height+")");
    s.append(" min=("+minWidth+","+minHeight+")");
    s.append(" max=("+maxWidth+","+maxHeight+")");
    s.append("}");

    return s.toString();
  }
}

/* end of file */
