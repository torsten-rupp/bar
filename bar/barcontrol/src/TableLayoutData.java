import org.eclipse.swt.SWT;

public class TableLayoutData
{
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

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   * @param width,height min./max. width/height
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int width, int height)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = Math.max(1,rowSpawn);
    this.columnSpawn = Math.max(1,columnSpawn);
    this.padX        = padX;
    this.padY        = padY;
    this.minWidth    = width;
    this.minHeight   = height;
    this.maxWidth    = width;
    this.maxHeight   = height;
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param padX,padY padding X/Z
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = Math.max(1,rowSpawn);
    this.columnSpawn = Math.max(1,columnSpawn);
    this.padX        = padX;
    this.padY        = padY;
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   * @param pad padding X/Z
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int pad)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = Math.max(1,rowSpawn);
    this.columnSpawn = Math.max(1,columnSpawn);
    this.padX        = pad;
    this.padY        = pad;
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   * @param rowSpawn,columnSpawn row/column spawn (1..n)
   */
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = Math.max(1,rowSpawn);
    this.columnSpawn = Math.max(1,columnSpawn);
    this.padX        = 0;
    this.padY        = 0;
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   * @param style style flags
   */
  TableLayoutData(int row, int column, int style)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = 1;
    this.columnSpawn = 1;
    this.padX        = 0;
    this.padY        = 0;
  }

  /** create table layout data
   * @param row,column row/column (0..n)
   */
  TableLayoutData(int row, int column)
  {
    this.row         = row;
    this.column      = column;
    this.style       = DEFAULT;
    this.rowSpawn    = 1;
    this.columnSpawn = 1;
    this.padX        = 0;
    this.padY        = 0;
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
