/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/TableLayout.java,v $
* $Revision: 1.11 $
* $Author: torsten $
* Contents:
* Systems:
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.lang.Math;
import java.util.Arrays;

import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Layout;

/****************************** Classes ********************************/

public class TableLayout extends Layout
{
  // --------------------------- constants --------------------------------

  // layout constants
  public final static int NONE    = 0;

  public final static int N       = 1 << 0;
  public final static int S       = 1 << 1;
  public final static int W       = 1 << 2;
  public final static int E       = 1 << 3;
  public final static int NS      = N|S;
  public final static int WE      = W|E;
  public final static int NW      = N|W;
  public final static int NE      = N|E;
  public final static int SW      = S|W;
  public final static int SE      = S|E;
  public final static int NSWE    = N|S|W|E;

  public final static int DEFAULT = 0;

  // --------------------------- variables --------------------------------

  // weight of rows for shrinking/expanding
  public double   rowWeight         = 0.0;
  public double   columnWeight      = 0.0;
  public double   rowWeights[]      = null;
  public double   columnWeights[]   = null;

  // margin
  public int      marginTop         = 0;
  public int      marginBottom      = 0;
  public int      marginLeft        = 0;
  public int      marginRight       = 0;

  // spacing
  public int      horizontalSpacing = 2;
  public int      verticalSpacing   = 2;

  // size limits
  public int      minWidth          = SWT.DEFAULT;
  public int      minHeight         = SWT.DEFAULT;
  public int      maxWidth          = SWT.DEFAULT;
  public int      maxHeight         = SWT.DEFAULT;

  private int     rows,columns;
  private int     rowSizeHints[],columnSizeHints[];
  private int     rowSizeMin[],columnSizeMin[];
  private boolean rowExpandFlags[],columnExpandFlags[];
  private Point[] sizes;
  private int     totalWidth,totalHeight;

  /* debug only */
  private final boolean debug = false;
//  private final boolean debug = true;
  private static int debugRecursion = 0;
  private String indent()
  {
    StringBuffer buffer = new StringBuffer(debugRecursion);
    for (int z = 1; z < debugRecursion; z++) buffer.append("    ");
    return buffer.toString();
  }

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------


  /** create table layout
   */
  public TableLayout()
  {
    super();
  }

  /** create table layout
   * @param margin margin size
   */
  public TableLayout(int margin)
  {
    super();

    this.marginTop     = margin;
    this.marginBottom  = margin;
  }

  /** create table layout
   * @param rowWeight default row weight (0.0..1.0)
   * @param columnWeight default column weight (0.0..1.0)
   */
  public TableLayout(double rowWeight, double columnWeight)
  {
    super();

    this.rowWeight    = rowWeight;
    this.columnWeight = columnWeight;
  }

  /** create table layout
   * @param rowWeights array with row weights (0.0..1.0)
   * @param columnWeight default column weight (0.0..1.0)
   */
  public TableLayout(double rowWeights[], double columnWeight)
  {
    super();

    this.rowWeights   = rowWeights;
    this.columnWeight = columnWeight;
  }

  /** create table layout
   * @param rowWeight default row weight (0.0..1.0)
   * @param columnWeights array with column weights (0.0..1.0)
   */
  public TableLayout(double rowWeight, double columnWeights[])
  {
    super();

    this.rowWeight     = rowWeight;
    this.columnWeights = columnWeights;
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
   * @param rowWeight default row weight (0.0..1.0)
   * @param columnWeight default column weight (0.0..1.0)
   * @param margin margin size
   */
  public TableLayout(double rowWeight, double columnWeight, int margin)
  {
    super();

    this.rowWeight    = rowWeight;
    this.columnWeight = columnWeight;
    this.marginTop    = margin;
    this.marginBottom = margin;
    this.marginLeft   = margin;
    this.marginRight  = margin;
  }

  /** create table layout
   * @param rowWeights array with row weights (0.0..1.0)
   * @param columnWeight default column weight (0.0..1.0)
   * @param margin margin size
   */
  public TableLayout(double rowWeights[], double columnWeight, int margin)
  {
    super();

    this.rowWeights   = rowWeights;
    this.columnWeight = columnWeight;
    this.marginTop    = margin;
    this.marginBottom = margin;
    this.marginLeft   = margin;
    this.marginRight  = margin;
  }

  /** create table layout
   * @param rowWeight default row weight (0.0..1.0)
   * @param columnWeights array with column weights (0.0..1.0)
   * @param margin margin size
   */
  public TableLayout(double rowWeight, double columnWeights[], int margin)
  {
    super();

    this.rowWeight     = rowWeight;
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
   * @param rowWeight default row weight (0.0..1.0)
   * @param columnWeight default column weight (0.0..1.0)
   * @param margin margin size
   * @param spacing space between sub-widgets
   */
  public TableLayout(double rowWeight, double columnWeight, int margin, int spacing)
  {
    super();

    this.rowWeight         = rowWeight;
    this.columnWeight      = columnWeight;
    this.marginTop         = margin;
    this.marginBottom      = margin;
    this.marginLeft        = margin;
    this.marginRight       = margin;
    this.horizontalSpacing = spacing;
    this.verticalSpacing   = spacing;
  }

  /** create table layout
   * @param rowWeights array with row weights (0.0..1.0)
   * @param columnWeight default column weight (0.0..1.0)
   * @param margin margin size
   * @param spacing space between sub-widgets
   */
  public TableLayout(double rowWeights[], double columnWeight, int margin, int spacing)
  {
    super();

    this.rowWeight         = rowWeight;
    this.columnWeights     = columnWeights;
    this.marginTop         = margin;
    this.marginBottom      = margin;
    this.marginLeft        = margin;
    this.marginRight       = margin;
    this.horizontalSpacing = spacing;
    this.verticalSpacing   = spacing;
  }

  /** create table layout
   * @param rowWeight default row weight (0.0..1.0)
   * @param columnWeights array with column weights (0.0..1.0)
   * @param margin margin size
   * @param spacing space between sub-widgets
   */
  public TableLayout(double rowWeight, double columnWeights[], int margin, int spacing)
  {
    super();

    this.rowWeights        = rowWeights;
    this.columnWeight      = columnWeight;
    this.marginTop         = margin;
    this.marginBottom      = margin;
    this.marginLeft        = margin;
    this.marginRight       = margin;
    this.horizontalSpacing = spacing;
    this.verticalSpacing   = spacing;
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
if (debug) debugRecursion++;
if (debug) System.err.println(indent()+"S++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
if (debug) System.err.println(indent()+"computeSize "+composite);
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
    Point size = new Point(marginLeft+width+marginRight,marginTop+height+marginBottom);
    if (size.x <= 0) size.x = 1;
    if (size.y <= 0) size.y = 1;
if (debug) System.err.println(indent()+"computeSize done: "+composite+" size="+size+" #children="+children.length);
//if (debug) for (int i=0;i<children.length;i++) System.err.println(indent()+"  "+children[i]+" "+children[i].computeSize(SWT.DEFAULT,SWT.DEFAULT,true));
//Dprintf.dprintf("compu com=%s w=%d h=%d\n",composite,marginLeft+width+marginRight,marginTop+height+marginBottom);
if (debug) System.err.println(indent()+"E++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
if (debug) debugRecursion--;

    return size;
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
    int    sizeSum;
    double weightSum;
    int    rowSizesSum[],columnSizesSum[];

    // initialize children
    Control children[] = composite.getChildren();
    if (flushCache || checkInitializeRequired(children))
    {
      initialize(children);
    }
if (debug) debugRecursion++;
if (debug) System.err.println(indent()+"S========================================================");
if (debug) System.err.println(indent()+"layout "+this+": children="+children.length+" rows="+rows+" columns="+columns);

    // get available space
    Rectangle rectangle = composite.getClientArea();
if (debug) System.err.println(indent()+"composite="+composite+" rectangle="+rectangle);

    // calculate fixed row/column sizes
    fixedWidth  = 0;
    fixedHeight = 0;
    for (int i = 0; i < columns; i++)
    {
      if (columnWeights[i] == 0.0) fixedWidth += Math.max(columnSizeHints[i],columnSizeMin[i]);
//      else                         fixedWidth += columnSizeMin[i];
      fixedWidth += ((i < columns-1)?verticalSpacing:0);
    }
    for (int i = 0; i < rows; i++)
    {
      if (rowWeights[i] == 0.0) fixedHeight += Math.max(rowSizeHints[i],rowSizeMin[i]);
//      else                      fixedHeight += rowSizeMin[i];
      fixedHeight += ((i < rows-1)?horizontalSpacing:0);
    }
if (debug) System.err.println(indent()+"fixedWidth="+fixedWidth+" fixedHeight="+fixedHeight);

    // calculate row/column sizes
    height       = rectangle.height-marginTop-marginBottom;
    rowSizes     = new int[rows];
    variableSize = height-fixedHeight;
    sizeSum      = 0;
    weightSum    = 0.0;
if (debug) System.err.println(indent()+"row variableSize="+variableSize);
if (debug) { System.err.print(indent()+"row weights: "); for (int i = 0; i < rowWeights.length; i++) { System.err.print(" "+rowWeights[i]); }; System.err.println(); }
    for (int i = 0; i < rows; i++)
    {
      if (rowWeights[i] > 0.0)
      {
        /* Note: to avoid rounding errors calculate sum of row sizes by sum of weights, then round to integer */
        rowSizes[i] = Math.max((int)Math.round(variableSize*(weightSum+rowWeights[i]))-sizeSum,
                               rowSizeMin[i]
                              );

        sizeSum   += rowSizes[i];
        weightSum += rowWeights[i];
      }
      else
      {
        rowSizes[i] = Math.max(rowSizeHints[i],
                               rowSizeMin[i]
                              );
      }
    }
if (debug) { System.err.print(indent()+"row sizes: ");for (int i = 0; i < rows;    i++) System.err.print(" "+rowSizes[i]   +(rowExpandFlags[i]   ?"*":""));System.err.println(); }
    width        = rectangle.width-marginLeft-marginRight;
    columnSizes  = new int[columns];
    variableSize = width-fixedWidth;
    sizeSum      = 0;
    weightSum    = 0.0;
if (debug) System.err.println(indent()+"column variableSize="+variableSize);
if (debug) { System.err.print(indent()+"column weights: "); for (int i = 0; i < columnWeights.length; i++) { System.err.print(" "+columnWeights[i]); }; System.err.println(); }
    for (int i = 0; i < columns; i++)
    {
      if (columnWeights[i] > 0.0)
      {
        /* Note: to avoid rounding errors calculate sum of column sizes by sum of weights, then round to integer */
        columnSizes[i] = Math.max((int)Math.round(variableSize*(weightSum+columnWeights[i]))-sizeSum,
                                  columnSizeMin[i]
                                 );

        sizeSum   += columnSizes[i];
        weightSum += columnWeights[i];
      }
      else
      {
        columnSizes[i] = Math.max(columnSizeHints[i],
                                  columnSizeMin[i]
                                 );
      }
    }
if (debug) { System.err.print(indent()+"column size: ");for (int i = 0; i < columns; i++) System.err.print(" "+columnSizes[i]+(columnExpandFlags[i]?"*":""));System.err.println(); }

    /* calculate row/column sizes sum */
    rowSizesSum    = new int[rows   ];
    columnSizesSum = new int[columns];
    if (rows > 0)
    {
      rowSizesSum[0] = 0;
      for (int i = 1; i < rows; i++)
      {
        rowSizesSum[i] = rowSizesSum[i-1]+rowSizes[i-1]+horizontalSpacing;
      }
    }
    if (columns > 0)
    {
      columnSizesSum[0] = 0;
      for (int i = 1; i < columns; i++)
      {
        columnSizesSum[i] = columnSizesSum[i-1]+columnSizes[i-1]+verticalSpacing;
      }
    }

if (debug) System.err.println(indent()+"layout:");
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();
      if (tableLayoutData == null) throw new Error("no layout data");

      width  = columnSizes[tableLayoutData.column]-2*tableLayoutData.padX;
      height = rowSizes   [tableLayoutData.row   ]-2*tableLayoutData.padY;
      for (int z = tableLayoutData.column+1; z < Math.min(tableLayoutData.column+tableLayoutData.columnSpawn,columns); z++) width  += columnSizes[z]+verticalSpacing;
      for (int z = tableLayoutData.row   +1; z < Math.min(tableLayoutData.row   +tableLayoutData.rowSpawn,   rows   ); z++) height += rowSizes   [z]+horizontalSpacing;

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
if (debug) System.err.println(indent()+String.format("  %-30s: size=(%4d,%4d) row/col=(%2d,%2d): (%4d,%4d)+(%4dx%4d) = (%4d,%4d)-(%4d,%4d)",
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
if (debug) System.err.println(indent()+"E========================================================");
if (debug) debugRecursion--;
  }

  /** initialize
   * @param children children to initialize
   */
  private void initialize(Control children[])
  {
if (debug) debugRecursion++;
if (debug) System.err.println(indent()+"S--------------------------------------------------------");
if (debug) System.err.println(indent()+"initialize "+this+": children="+children.length);

    // get sizes of children
    sizes = new Point[children.length];
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();
      if (tableLayoutData == null) throw new Error("no layout data for "+children[i]);

      sizes[i] = children[i].computeSize(SWT.DEFAULT,SWT.DEFAULT,true);

      if (tableLayoutData.width  != SWT.DEFAULT) sizes[i].x = tableLayoutData.width;
      if (tableLayoutData.height != SWT.DEFAULT) sizes[i].y = tableLayoutData.height;

      if (tableLayoutData.minWidth  != SWT.DEFAULT) sizes[i].x = Math.max(sizes[i].x,tableLayoutData.minWidth );
      if (tableLayoutData.maxWidth  != SWT.DEFAULT) sizes[i].x = Math.min(sizes[i].x,tableLayoutData.maxWidth );
      if (tableLayoutData.minHeight != SWT.DEFAULT) sizes[i].y = Math.max(sizes[i].y,tableLayoutData.minHeight);
      if (tableLayoutData.maxHeight != SWT.DEFAULT) sizes[i].y = Math.min(sizes[i].y,tableLayoutData.maxHeight);
    }
if (debug) System.err.println(indent()+"sizes:");
if (debug) for (int i = 0; i < sizes.length; i++)
{
  System.err.println(indent()+String.format("  %-20s: (%4d,%4d) %s",
                                            children[i],
                                            sizes[i].x,sizes[i].y,
                                            ((children[i].getLayoutData()!=null)?((TableLayoutData)children[i].getLayoutData()).toString():"")
                                           )
                    );
}

    // get number of rows/columns
    rows    = 0;
    columns = 0;
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();

      assert(tableLayoutData.rowSpawn >= 1);
      assert(tableLayoutData.columnSpawn >= 1);

      rows    = Math.max(rows,   tableLayoutData.row   +tableLayoutData.rowSpawn   );
      columns = Math.max(columns,tableLayoutData.column+tableLayoutData.columnSpawn);
    }
if (debug) System.err.println(indent()+"rows="+rows+" columns="+columns);

    // get row/column expansion flags
    rowExpandFlags    = new boolean[rows   ];
    columnExpandFlags = new boolean[columns];
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();

      assert(tableLayoutData.rowSpawn >= 1);
      assert(tableLayoutData.columnSpawn >= 1);

      for (int z = tableLayoutData.column; z < Math.min(tableLayoutData.column+1/*tableLayoutData.columnSpawn*/,columns); z++)
      {
        columnExpandFlags[z] |= ((tableLayoutData.style & TableLayoutData.E) == TableLayoutData.E);
      }
      for (int z = tableLayoutData.row   ; z < Math.min(tableLayoutData.row   +1/*tableLayoutData.rowSpawn*/,   rows   ); z++)
      {
        rowExpandFlags   [z] |= ((tableLayoutData.style & TableLayoutData.S) == TableLayoutData.S);
      }
    }

    // initialize weights
    rowWeights    = getWeights(rowWeight,   rowWeights,   rows,   rowExpandFlags   );
    columnWeights = getWeights(columnWeight,columnWeights,columns,columnExpandFlags);

    // calculate row/columns hint sizes, max. width/height
    rowSizeHints    = new int[rows   ];
    columnSizeHints = new int[columns];
    rowSizeMin      = new int[rows   ];
    columnSizeMin   = new int[columns];
    for (int i = 0; i < children.length; i++)
    {
      TableLayoutData tableLayoutData = (TableLayoutData)children[i].getLayoutData();

      assert(tableLayoutData.rowSpawn >= 1);
      assert(tableLayoutData.columnSpawn >= 1);

      // get size of children
      int width  = tableLayoutData.padX+sizes[i].x/tableLayoutData.columnSpawn+tableLayoutData.padX;
      int height = tableLayoutData.padY+sizes[i].y/tableLayoutData.rowSpawn   +tableLayoutData.padY;

      // calculate available space
      int    availableWidth  = 0;
      int    availableHeight = 0;
      double widthWeightSum  = 0.0;
      double heightWeightSum = 0.0;
      for (int z = tableLayoutData.column; z < Math.min(tableLayoutData.column+tableLayoutData.columnSpawn,columns); z++)
      {
        availableWidth += columnSizeHints[z];
        widthWeightSum += columnWeights[z];
      }
      for (int z = tableLayoutData.row   ; z < Math.min(tableLayoutData.row   +tableLayoutData.rowSpawn,   rows   ); z++)
      {
        availableHeight += rowSizeHints[z];
        heightWeightSum += rowWeights[z];
      }
//if (debug) { System.err.println(indent()+"avail width/height: "+availableWidth+"/"+availableHeight+", weight sums "+widthWeightSum+"/"+heightWeightSum); }

      // calculate additional required width/height
//      int addWidth  = (width -availableWidth )/tableLayoutData.columnSpawn;
//      int addHeight = (height-availableHeight)/tableLayoutData.rowSpawn;
      int addWidth  = Math.max((int)((double)(width -availableWidth )*widthWeightSum ),0);
      int addHeight = Math.max((int)((double)(height-availableHeight)*heightWeightSum),0);
//      int addWidth  = 0;
//      int addHeight = 0;
//if (debug) { System.err.println(indent()+"add width/height: "+addWidth+"/"+addHeight); }

      // expand row/column size if required (if not expand flag set and expand value>0)
      for (int z = tableLayoutData.row; z < Math.min(tableLayoutData.row+1/* only this row; tableLayoutData.rowSpawn*/,rows); z++)
      {
        rowSizeHints[z] = Math.max(rowSizeHints[z]+addHeight,height);
//        rowSizeHints[z] = Math.max(rowSizeHints[z],tableLayoutData.minHeight);
        rowSizeMin[z] = Math.max(rowSizeMin[z],tableLayoutData.minHeight);
      }
      for (int z = tableLayoutData.column; z < Math.min(tableLayoutData.column+1/* only this column; tableLayoutData.columnSpawn*/,columns); z++)
      {
        columnSizeHints[z] = Math.max(columnSizeHints[z]+addWidth,width);
//        columnSizeHints[z] = Math.max(columnSizeHints[z],tableLayoutData.minWidth);
        columnSizeMin[z] = Math.max(columnSizeMin[z],tableLayoutData.minWidth);
      }
    }
if (debug) { System.err.print(indent()+"row size hints   : ");for (int i = 0; i < rows;    i++) System.err.print(" "+rowSizeHints   [i]+(rowExpandFlags[i]   ?"*":""));System.err.println(); }
if (debug) { System.err.print(indent()+"column size hints: ");for (int i = 0; i < columns; i++) System.err.print(" "+columnSizeHints[i]+(columnExpandFlags[i]?"*":""));System.err.println(); }
if (debug) { System.err.print(indent()+"column size min  : ");for (int i = 0; i < columns; i++) System.err.print(" "+columnSizeMin[i]);System.err.println(); }

    // calculate total width/height
    totalWidth  = marginLeft+marginRight;
    totalHeight = marginTop +marginBottom;
    for (int i = 0; i < rows; i++)
    {
      totalHeight += Math.max(rowSizeHints[i],  rowSizeMin[i]   )+((i < rows   -1)?horizontalSpacing:0);
    }
    for (int i = 0; i < columns; i++)
    {
      totalWidth += Math.max(columnSizeHints[i],columnSizeMin[i])+((i < columns-1)?verticalSpacing  :0);
    }
if (debug) { System.err.println(indent()+"total width/height: "+totalWidth+"/"+totalHeight); }
if (debug) System.err.println(indent()+"E--------------------------------------------------------");
if (debug) debugRecursion--;
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
   * @param weight default weight
   * @param weights array with weights
   * @param count number of rows/columns
   * @param expandFlags array with expand flags for row/column
   * @return new array with weights
   */
  private double[] getWeights(double weight, double weights[], int count, boolean expandFlags[])
  {
    double sum;

    // shrink/extend weights array if needed
    if ((weights == null) || (weights.length != count))
    {
      double newWeights[] = new double[count];
      if (weights != null)
      {
        System.arraycopy(weights,0,newWeights,0,Math.min(weights.length,count));
        if (newWeights.length > weights.length) Arrays.fill(newWeights,weights.length,newWeights.length,weight);
      }
      else
      {
        Arrays.fill(newWeights,weight);
      }
      weights = newWeights;
    }

    // normalize weights
    sum = 0.0;
    for (int i = 0; i < weights.length; i++) { sum += weights[i]; };
    for (int i = 0; i < weights.length; i++) { weights[i] = ((sum > 0.0)?weights[i]/sum:weight); }; 
//System.err.print(indent()+"weights sum="+sum+": "); for (int i = 0; i < weights.length; i++) { System.err.print(" "+weights[i]); }; System.err.println();

    return weights;
  }
}

/* end of file */
