/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/ProgressBar.java,v $
* $Revision: 1.4 $
* $Author: torsten $
* Contents: progress bar widget
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;

/****************************** Classes ********************************/

/** progress bar with percentage view
 *
 *  +------------------------------------------+
 *  |<- sub value 1 ->| ... |<- sub value n -> |
 *  +------------------------------------------+
 *   <---------------- value ----------------->
 *
 */
public class ProgressBar extends Canvas
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  private double minimum;
  private double maximum;
  private int    subValueCount;
  private double value,subValue;
  private Point  textSize;
  private String text;

  private Color  colorBlack;
  private Color  colorWhite;
  private Color  colorNormalShadow;
  private Color  colorHighlightShadow;
  private Color  colorBar;
  private Color  colorBarSet;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create progress bar
   * @param composite parent composite widget
   * @param style style flags
   */
  ProgressBar(Composite composite, int style)
  {
    super(composite,style);

    this.subValueCount        = 1;
    this.colorBlack           = getDisplay().getSystemColor(SWT.COLOR_BLACK);
    this.colorWhite           = getDisplay().getSystemColor(SWT.COLOR_WHITE);
    this.colorNormalShadow    = getDisplay().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW);
    this.colorHighlightShadow = getDisplay().getSystemColor(SWT.COLOR_WIDGET_HIGHLIGHT_SHADOW);
    this.colorBar             = getDisplay().getSystemColor(SWT.COLOR_WHITE);
    this.colorBarSet          = new Color(null,0xAD,0xD8,0xE6);

    addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposeEvent)
      {
        ProgressBar.this.widgetDisposed(disposeEvent);
      }
    });
    addPaintListener(new PaintListener()
    {
      public void paintControl(PaintEvent paintEvent)
      {
        ProgressBar.this.paint(paintEvent);
      }
    });

    setSelection(0.0);
  }

  /** create progress bar
   * @param composite parent composite widget
   */
  ProgressBar(Composite composite)
  {
    this(composite,SWT.NONE);
  }

  /** compute size of pane
   * @param wHint,hHint width/height hint
   * @param changed TRUE iff no cache values should be used
   * @return size
   */
  public Point computeSize(int wHint, int hHint, boolean changed)
  {
    GC gc;
    int width,height;

    width  = 0;
    height = 0;

    width  = 2+textSize.x+2;
    height = 2+textSize.y+2;
    if (wHint != SWT.DEFAULT) width  = wHint;
    if (hHint != SWT.DEFAULT) height = hHint;

    return new Point(width,height);
  }

  /** get minimal progress value
   * @return minimum value
   */
  public double getMinimum()
  {
    return this.minimum;
  }

  /** set minimal progress value
   * @param n value
   */
  public void setMinimum(double n)
  {
    this.minimum = n;
  }

  /** get maximal progress value
   * @return maxium value
   */
  public double getMaximum()
  {
    return this.maximum;
  }

  /** set maximal progress value
   * @param n value
   */
  public void setMaximum(double n)
  {
    this.maximum = n;
  }

  /** set number of sub-values
   * @param n number of sub-values
   */
  public void setSubValueCount(int n)
  {
    assert n > 0;

    this.subValueCount = n;
  }

  /** get progress value
   * @return value
   */
  public double getSelection()
  {
    return value;
  }

  /** set progress value
   * @param n value [minimum..maximum]
   */
  public void setSelection(double n)
  {
    GC gc;

    if (!isDisposed())
    {
      value    = Math.min(Math.max(((maximum-minimum) > 0.0)
                                     ? (n-minimum)/(maximum-minimum)
                                     : 0.0,
                                   0.0
                                  ),
                          1.0
                         );
      subValue = 0.0;

      gc = new GC(this);
      text = String.format("%.1f%%",(value+subValue)*100.0);
      textSize = gc.stringExtent(text);
      gc.dispose();

      redraw();
    }
  }

  /** get progress sub-value
   * @return value
   */
  public double getSubSelection()
  {
    return subValue;
  }

  /** set progress sub-value
   * @param n value [minimum..maximum]
   */
  public void setSubSelection(double n)
  {
    GC gc;

    assert subValueCount > 0;

    if (!isDisposed())
    {
      subValue = Math.min(Math.max(((maximum-minimum) > 0.0)
                                     ? ((value+n/(double)subValueCount)-minimum)/(maximum-minimum)
                                     : 0.0,
                                   0.0
                                  ),
                          1.0
                         );

      gc = new GC(this);
      text = String.format("%.1f%%",(value+subValue)*100.0);
      textSize = gc.stringExtent(text);
      gc.dispose();

      redraw();
    }
  }

  /** free allocated resources
   * @param disposeEvent dispose event
   */
  private void widgetDisposed(DisposeEvent disposeEvent)
  {
    colorBarSet.dispose();
  }

  /** paint progress bar
   * @param paintEvent paint event
   */
  private void paint(PaintEvent paintEvent)
  {
    GC        gc;
    Rectangle bounds;
    int       x,y,w,h;

    gc = paintEvent.gc;
    bounds = getBounds();
    x = 0;
    y = 0;
    w = bounds.width;
    h = bounds.height;

    // shadow
    gc.setForeground(colorNormalShadow);
    gc.drawRectangle(x+0,y+0,w-2,h-2);

    gc.setForeground(colorHighlightShadow);
    gc.drawLine(x+1  ,y+1  ,x+w-3,y+1  );
    gc.drawLine(x+1  ,y+2  ,x+1  ,y+h-3);
    gc.drawLine(x+0  ,y+h-1,x+w-1,y+h-1);
    gc.drawLine(x+w-1,y+0  ,x+w-1,y+h-2);

    // draw bar
    gc.setBackground(colorBar);
    gc.fillRectangle(x+2,y+2,w-4,h-4);
    gc.setBackground(colorBarSet);
    gc.fillRectangle(x+2,y+2,(int)((double)(w-4)*(value+subValue)),h-4);

    // draw percentage text
    gc.setForeground(colorBlack);
    gc.drawString(text,(w-textSize.x)/2,(h-textSize.y)/2,true);
  }
}

/* end of file */
