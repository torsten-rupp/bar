public class TableLayoutData
{
  public final static int NONE     = TableLayout.NONE;

  public final static int N        = TableLayout.N;
  public final static int S        = TableLayout.S;
  public final static int W        = TableLayout.W;
  public final static int E        = TableLayout.E;
  public final static int NS       = TableLayout.NS;
  public final static int WE       = TableLayout.WE;
  public final static int NW       = TableLayout.NW;
  public final static int NE       = TableLayout.NE;
  public final static int SW       = TableLayout.SW;
  public final static int SE       = TableLayout.SE;
  public final static int NSWE     = TableLayout.NSWE;
  public final static int EXPAND_X = TableLayout.EXPAND_X;
  public final static int EXPAND_Y = TableLayout.EXPAND_Y;
  public final static int EXPAND   = TableLayout.EXPAND;

  public final static int DEFAULT  = TableLayout.DEFAULT;

  // true iff widget should be exclued (not drawn)
  public int     minWidth  = 0;
  public int     minHeight = 0;
  public int     maxWidth  = 0;
  public int     maxHeight = 0;
  public boolean exclude   = false;

  protected int row,column;
  protected int style;
  protected int rowSpawn,columnSpawn;
  protected int padX,padY;

  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY, int minWidth, int minHeight)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = rowSpawn;
    this.columnSpawn = columnSpawn;
    this.padX        = padX;
    this.padY        = padY;
    this.minWidth    = minWidth;
    this.minHeight   = minHeight;
  }
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn, int padX, int padY)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = rowSpawn;
    this.columnSpawn = columnSpawn;
    this.padX        = padX;
    this.padY        = padY;
  }
  TableLayoutData(int row, int column, int style, int rowSpawn, int columnSpawn)
  {
    this.row         = row;
    this.column      = column;
    this.style       = style;
    this.rowSpawn    = rowSpawn;
    this.columnSpawn = columnSpawn;
    this.padX        = 0;
    this.padY        = 0;
  }
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

    s = new StringBuffer("row="+row+" column="+column+" style=");
    if ((style & W) == W) s.append("W");
    if ((style & E) == E) s.append("E");
    if ((style & N) == N) s.append("N");
    if ((style & S) == S) s.append("S");
    if ((style & EXPAND_X) == EXPAND_X) s.append(" expandX ");
    if ((style & EXPAND_Y) == EXPAND_Y) s.append(" expandY");
    s.append(" rowSpawn="+rowSpawn);
    s.append(" columnSpawn="+columnSpawn);
    s.append(" padX="+padX);
    s.append(" padY="+padY);

    return s.toString();
  }
};
