import java.lang.Math;
import java.util.Arrays;

import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Layout;

public class TableLayout extends Layout
{
  // layout constants
  public final static int NONE     = 0;

  public final static int N        = 1 << 0;
  public final static int S        = 1 << 1;
  public final static int W        = 1 << 2;
  public final static int E        = 1 << 3;
  public final static int NS       = N|S;
  public final static int WE       = W|E;
  public final static int NW       = N|W;
  public final static int NE       = N|E;
  public final static int SW       = S|W;
  public final static int SE       = S|E;
  public final static int NSWE     = N|S|W|E;
  public final static int EXPAND_X = 1 << 4;
  public final static int EXPAND_Y = 1 << 5;
  public final static int EXPAND   = EXPAND_X|EXPAND_Y;

  public final static int DEFAULT  = 0;

  // weight of rows for shrinking/expanding
  public double rowWeights[],columnWeights[];

  // margin
  public int      marginTop         = 0;
  public int      marginBottom      = 0;
  public int      marginLeft        = 0;
  public int      marginRight       = 0;

  // spacing
  public int      horizontalSpacing = 2;
  public int      verticalSpacing   = 2;

  // size limits
  public int      minWidth  = SWT.DEFAULT;
  public int      minHeight = SWT.DEFAULT;
  public int      maxWidth  = SWT.DEFAULT;
  public int      maxHeight = SWT.DEFAULT;

  private int     rows,columns;
  private int     rowSizeHints[],columnSizeHints[];
  private boolean rowExpandFlags[],columnExpandFlags[];
  private Point[] sizes;
  private int     totalWidth,totalHeight;

  private final boolean debug = false;
//  private final boolean debug = true;

  /** create table layout
   */
  public TableLayout()
  {
    super();

    this.rowWeights    = null;
    this.columnWeights = null;
    this.marginTop     = 0;
    this.marginBottom  = 0;
    this.marginLeft    = 0;
    this.marginRight   = 0;
  }

  /** create table layout
   * @param margin margin size
   */
  public TableLayout(int margin)
  {
    super();

    this.rowWeights    = null;
    this.columnWeights = null;
    this.marginTop     = margin;
    this.marginBottom  = margin;
    this.marginLeft    = margin;
    this.marginRight   = margin;
  }

  /** create table layout
   * @param rowWeights array with row weights (0.0..1.0)
   * @param columnWeights array with column weights (0.0..1.0)
   */
  public TableLayout(double rowWeights[], double columnWeights[])
  {
    super();

    this.rowWeights    = rowWeights;
    this.columnWeights = columnWeights;
  }

  /** create table layout
   * @param rowWeights array with row weights (0.0..1.0)
   * @param columnWeights array with column weights (0.0..1.0)
   * @param margin margin size
   */
  public TableLayout(double rowWeights[], double columnWeights[], int margin)
  {
    super();

    this.rowWeights    = rowWeights;
    this.columnWeights = columnWeights;
    this.marginTop     = margin;
    this.marginBottom  = margin;
    this.marginLeft    = margin;
    this.marginRight   = margin;
  }

  /** create table layout
   * @param rowWeights array with row weights (0.0..1.0)
   * @param columnWeights array with column weights (0.0..1.0)
   * @param margin margin size
   * @param spacing space between sub-widgets
   */
  public TableLayout(double rowWeights[], double columnWeights[], int margin, int spacing)
  {
    super();

    this.rowWeights        = rowWeights;
    this.columnWeights     = columnWeights;
    this.marginTop         = margin;
    this.marginBottom      = margin;
    this.marginLeft        = margin;
    this.marginRight       = margin;
    this.horizontalSpacing = spacing;
    this.verticalSpacing   = spacing;
  }

  //-----------------------------------------------------------------------

  /** compute size of widget
   * @param composite composite
   * @param widthHint required with (hint)
   * @param heightHint required height (hint)
   * @param flushCache true iff internal cached data should be dicarded
   * @return size of widget
   */
  protected Point computeSize(Composite composite, int widthHint, int heightHint, boolean flushCache)
  {
    Control children[] = composite.getChildren();
    if (flushCache || checkInitializeRequired(children))
    {
      initialize(children);
    }

    int width  = (widthHint  != SWT.DEFAULT)?widthHint :totalWidth;
    int height = (heightHint != SWT.DEFAULT)?heightHint:totalHeight;
    if (minWidth  != SWT.DEFAULT) width  = Math.max(width, minWidth );
    if (minHeight != SWT.DEFAULT) height = Math.max(height,minHeight);
    if (maxWidth  != SWT.DEFAULT) width  = Math.min(width, maxWidth );
    if (maxHeight != SWT.DEFAULT) height = Math.min(height,maxHeight);
if (debug) System.err.println("computeSize: composite="+composite+" width="+width+" height="+height+" #children="+children.length);
if (debug) for (int i=0;i<children.length;i++) System.err.println("  "+children[i]+" "+children[i].computeSize(SWT.DEFAULT,SWT.DEFAULT,true));
    return new Point(marginLeft+width+marginRight,marginTop+height+marginBottom);
  }

  /** layout widgets in composite
   * @param composite composite widget
   * @param flushCache true iff internal cached data should be dicarded
   */
  protected void layout(Composite composite, boolean flushCache)
  {
    int    width,height;
    int    fixedWidth,fixedHeight;
    double rowWeightNorm,columWeightNorm;
    int    rowSizes[],columnSizes[];
    int    variableSize,suggestedSize,size,addSize;
    int    rowSizesSum[],columnSizesSum[];

    Control children[] = composite.getChildren();
    if (flushCache || checkInitializeRequired(children))
    {
      initialize(children);
    }
if (debug) System.err.println("--------------------------------------------------------");
if (debug) System.err.println("layout "+this+": children="+children.length+" rows="+rows+" columns="+columns);

    // get available space
    Rectangle rectangle = composite.getClientArea();
if (debug) System.err.println("composite="+composite+" rectangle="+rectangle);

    // calculate fixed row, column size
    fixedWidth  = 0;
    fixedHeight = 0;
    for (int i = 0; i < columns; i++)
    {
      fixedWidth += Math.max(columnSizeHints[i],1)+((i < columns-1)?verticalSpacing:0);
    }
    for (int i = 0; i < rows; i++)
    {
//if (debug) if (rowWeights != null) System.err.println("TableLayout.java"+", "+123+": "+rowWeights[i]+" "+rowSizeHints[i]);
      fixedHeight += Math.max(rowSizeHints[i],1)+((i < rows-1)?horizontalSpacing:0);
    }
if (debug) System.err.println("fixedWidth="+fixedWidth+" fixedHeight="+fixedHeight);

    // calculate row/column sizes
    height       = rectangle.height-marginTop-marginBottom;
    rowSizes     = new int[rows];
    variableSize = height-fixedHeight;
if (debug) System.err.println("row variableSize="+variableSize);
if (debug) { System.err.print("row weights "); for (int i = 0; i < rowWeights.length; i++) { System.err.print(" "+rowWeights[i]); }; System.err.println(); }
    for (int i = 0; i < rows; i++)
    {
      if (rowExpandFlags[i])
      {
        // variable row size
        addSize = (int)(variableSize*rowWeights[i]);
      }
      else
      {
        // fixed row size
        addSize = 0;
      }
      rowSizes[i] = rowSizeHints[i]+addSize+horizontalSpacing;
    }
    width        = rectangle.width-marginLeft-marginRight;
    columnSizes  = new int[columns];
    variableSize = width-fixedWidth;
if (debug) System.err.println("column variableSize="+variableSize);
if (debug) { System.err.print("column weights "); for (int i = 0; i < columnWeights.length; i++) { System.err.print(" "+columnWeights[i]); }; System.err.println(); }
    for (int i = 0; i < columns; i++)
    {
      if (columnExpandFlags[i])
      {
        // variable column size
        addSize = (int)(variableSize*columnWeights[i]);
      }
      else
      {
        // fixed column size
        addSize = 0;
      }
      columnSizes[i] = columnSizeHints[i]+addSize+verticalSpacing;
    }
if (debug) { System.err.print("row sizes  : ");for (int i = 0; i < rows;    i++) System.err.print(" "+rowSizes[i]   +(rowExpandFlags[i]   ?"*":""));System.err.println(); }
if (debug) { System.err.print("column size: ");for (int i = 0; i < columns; i++) System.err.print(" "+columnSizes[i]+(columnExpandFlags[i]?"*":""));System.err.println(); }

    rowSizesSum    = new int[rows   ];
    columnSizesSum = new int[columns];
    for (int i = 1; i < rows; i++)
    {
      rowSizesSum[i] = rowSizesSum[i-1]+rowSizes[i-1];
    }
    for (int i = 1; i < columns; i++)
    {
      columnSizesSum[i] = columnSizesSum[i-1]+columnSizes[i-1];
    }

if (debug) System.err.println("layout:");
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();
      if (tableLayoutData == null) throw new Error("no layout data");

      width  = columnSizes[tableLayoutData.column]-2*tableLayoutData.padX-verticalSpacing;
      height = rowSizes   [tableLayoutData.row   ]-2*tableLayoutData.padY-horizontalSpacing;
      for (int z = tableLayoutData.column+1; z < Math.min(tableLayoutData.column+tableLayoutData.columnSpawn,columns); z++) width  += columnSizes[z];
      for (int z = tableLayoutData.row   +1; z < Math.min(tableLayoutData.row   +tableLayoutData.rowSpawn,   rows   ); z++) height += rowSizes   [z];

      int childX = rectangle.x+marginLeft+columnSizesSum[tableLayoutData.column]+tableLayoutData.padX;
      int childY = rectangle.y+marginTop +rowSizesSum   [tableLayoutData.row   ]+tableLayoutData.padY;
      int childWidth  = Math.min(sizes[i].x,width );
      int childHeight = Math.min(sizes[i].y,height);
      if ((tableLayoutData.style & TableLayoutData.WE) == TableLayoutData.WE) childWidth  = width;
      if ((tableLayoutData.style & TableLayoutData.NS) == TableLayoutData.NS) childHeight = height;
      if      ((tableLayoutData.style & TableLayoutData.WE) == TableLayoutData.E) childX += (childWidth < width)?width-childWidth:0;
      else if ((tableLayoutData.style & TableLayoutData.WE) == 0) childX += (childWidth < width)?(width-childWidth)/2:0;
      if      ((tableLayoutData.style & TableLayoutData.NS) == TableLayoutData.S) childY += (childHeight < height)?height-childHeight:0;
      else if ((tableLayoutData.style & TableLayoutData.NS) == 0) childY += (childHeight < height)?(height-childHeight)/2:0;
if (debug) System.err.println(String.format("  %-40s: size=(%4d,%4d) row=%2d column=%2d: %4d,%4d+%4dx%4d (%4d,%4d)-(%4d,%4d)",
                                 children[i],
                                 sizes[i].x,sizes[i].y,
                                 tableLayoutData.row,tableLayoutData.column,
                                 childX,childY,childWidth,childHeight,
                                 childX,childY,childX+childWidth,childY+childHeight
                                )
                  );
//"  "+children[i]+" size=("+sizes[i].x+","+sizes[i].y+") row="+tableLayoutData.row+" column="+tableLayoutData.column+": "+childX+","+childY+"+"+childWidth+"x"+childHeight+" ("+childX+","+childY+")-("+(childX+childWidth)+","+(childY+childHeight)+")");
      if (!tableLayoutData.exclude)  children[i].setBounds(childX,childY,childWidth,childHeight);
    }
  }

  /** initialize
   * @param children children
   */
  private void initialize(Control children[])
  {
if (debug) System.err.println("--------------------------------------------------------");
if (debug) System.err.println("init "+this+": children="+children.length);
    // get sizes
    sizes = new Point[children.length];
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();
      if (tableLayoutData == null) throw new Error("no layout data for "+children[i]);

      sizes[i] = children[i].computeSize(SWT.DEFAULT,SWT.DEFAULT,true);
      sizes[i].x = Math.max(sizes[i].x,tableLayoutData.minWidth );
      sizes[i].y = Math.max(sizes[i].y,tableLayoutData.minHeight);
    }
if (debug) System.err.println("sizes:");
if (debug) for (int i = 0; i < sizes.length; i++)
System.err.println(String.format("  (%4d,%4d) %s: %s",
                                 sizes[i].x,sizes[i].y,
                                 ((children[i].getLayoutData()!=null)?((TableLayoutData)children[i].getLayoutData()).toString():""),
                                 children[i]
                                )
                  );

    // get number of rows, columns
    rows    = 0;
    columns = 0;
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();

      // get children row, column spawn
      int rowSpawn    = Math.max(tableLayoutData.rowSpawn,   1);
      int columnSpawn = Math.max(tableLayoutData.columnSpawn,1);

      rows    = Math.max(rows,   tableLayoutData.row   +rowSpawn   );
      columns = Math.max(columns,tableLayoutData.column+columnSpawn);
    }
if (debug) System.err.println("rows="+rows+" columsn="+columns);

    // calculate row/columns hint sizes, max. width/height
    rowSizeHints      = new int[rows   ];
    columnSizeHints   = new int[columns];
    rowExpandFlags    = new boolean[rows   ];
    columnExpandFlags = new boolean[columns];
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();

      // get size of children
      int width  = tableLayoutData.padX+sizes[i].x+tableLayoutData.padX;
      int height = tableLayoutData.padY+sizes[i].y+tableLayoutData.padY;

      // get children row/column spawn
      int rowSpawn    = Math.max(tableLayoutData.rowSpawn,   1);
      int columnSpawn = Math.max(tableLayoutData.columnSpawn,1);

      // get expansion flags
//      rowExpandFlags   [tableLayoutData.row   ] |= ((tableLayoutData.style & TableLayoutData.EXPAND_Y) != 0);
//      columnExpandFlags[tableLayoutData.column] |= ((tableLayoutData.style & TableLayoutData.EXPAND_X) != 0);
      for (int z = tableLayoutData.column; z < Math.min(tableLayoutData.column+columnSpawn,columns); z++) columnExpandFlags[z] |= ((tableLayoutData.style & TableLayoutData.EXPAND_X) != 0);
      for (int z = tableLayoutData.row   ; z < Math.min(tableLayoutData.row   +rowSpawn,   rows   ); z++) rowExpandFlags   [z] |= ((tableLayoutData.style & TableLayoutData.EXPAND_Y) != 0);

      // calculate available space
      int availableWidth  = 0;
      int availableHeight = 0;
      for (int z = tableLayoutData.column; z < Math.min(tableLayoutData.column+columnSpawn,columns); z++) availableWidth  += columnSizeHints[z];
      for (int z = tableLayoutData.row   ; z < Math.min(tableLayoutData.row   +rowSpawn,   rows   ); z++) availableHeight += rowSizeHints   [z];

      // calculate additional required width, height
      int addWidth  = (width -availableWidth )/columnSpawn;
      int addHeight = (height-availableHeight)/rowSpawn;

      // expand row/column size if required (not expand and >0)
      for (int z = tableLayoutData.row; z < Math.min(tableLayoutData.row+rowSpawn,rows); z++)
      {
        if (!rowExpandFlags[tableLayoutData.row] && (addHeight > 0))
        {
          rowSizeHints[z] = Math.max(rowSizeHints[z]+addHeight,height);
        }
        rowSizeHints[z] = Math.max(rowSizeHints[z],tableLayoutData.minHeight);
      }
      for (int z = tableLayoutData.column; z < Math.min(tableLayoutData.column+columnSpawn,columns); z++)
      {
        if (!columnExpandFlags[tableLayoutData.column] && (addWidth > 0))
        {
          columnSizeHints[z] = Math.max(columnSizeHints[z]+addWidth,width);
        }
        columnSizeHints[z] = Math.max(columnSizeHints[z],tableLayoutData.minWidth);
      }
    }
if (debug) { System.err.print("row size hints   : ");for (int i = 0; i < rows;    i++) System.err.print(" "+rowSizeHints   [i]+(rowExpandFlags[i]   ?"*":""));System.err.println(); }
if (debug) { System.err.print("column size hints: ");for (int i = 0; i < columns; i++) System.err.print(" "+columnSizeHints[i]+(columnExpandFlags[i]?"*":""));System.err.println(); }

    // calculate max. width/height
    totalWidth  = 0;
    totalHeight = 0;
    for (int i = 0; i < rows; i++)
    {
      totalHeight += rowSizeHints[i]+((i < rows-1)?verticalSpacing:0);
    }
    for (int i = 0; i < columns; i++)
    {
      totalWidth += columnSizeHints[i]+((i < columns-1)?horizontalSpacing:0);
    }
//System.err.print("TableLayout.java"+", "+133+": sum row Sizes");for (int i = 0; i < rows; i++) System.err.print(" "+rowSizeHintsSum[i]);System.err.println();
//System.err.print("TableLayout.java"+", "+136+": sum column height");for (int i = 0; i < columns; i++) System.err.print(" "+columnSizeHintsSum[i]);System.err.println();

    // initialize weights
    rowWeights    = getWeights(rowWeights,   rows,   rowExpandFlags   );
    columnWeights = getWeights(columnWeights,columns,columnExpandFlags);
  }

  //-----------------------------------------------------------------------

  /** check if initialization is required
   * @param children children
   * @return true iff initialization is required
   */
  private boolean checkInitializeRequired(Control children[])
  {
    return    (rowWeights == null)
           || (rowWeights.length != children.length)
           || (rowWeights == null)
           || (rowWeights.length != children.length)
           || (sizes == null)
           || (sizes.length != children.length);
  }

  /** get initialized weights array
   * @param weights array with weights
   * @param count number of rows/columns
   * @param expandFlags array with expand flags for row/column
   * @return new array with weights
   */
  private double[] getWeights(double weights[], int count, boolean expandFlags[])
  {
    double sum;

    // shrink/extend weights array if needed
    if ((weights == null) || (weights.length != count))
    {
      double newWeights[] = new double[count];
      if (weights != null)
      {
        System.arraycopy(weights,0,newWeights,0,Math.min(weights.length,count));
        if (newWeights.length > weights.length) Arrays.fill(newWeights,weights.length,newWeights.length,1.0);
      }
      else
      {
        Arrays.fill(newWeights,1.0);
      }
      weights = newWeights;
    }

    // normalize weights
    sum = 0.0;
    for (int i = 0; i < weights.length; i++) { sum += expandFlags[i]?weights[i]:0.0; };
    for (int i = 0; i < weights.length; i++) { weights[i] = expandFlags[i]?((sum > 0.0)?weights[i]/sum:1.0):0.0; }; 
//System.err.print("weights "+sum); for (int i = 0; i < weights.length; i++) { System.err.print(" "+weights[i]); }; System.err.println();

    return weights;
  }

}
