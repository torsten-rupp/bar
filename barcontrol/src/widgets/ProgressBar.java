/***********************************************************************\
*
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
  private String text;
  private Point  textSize;

  private String currentText;
  private int    currentBarWidth;

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

    this.minimum              = 0.0;
    this.maximum              = 1.0;
    this.value                = 0.0;
    this.text                 = "";
    this.textSize             = new Point(0,0);

    this.currentText          = "";
    this.currentBarWidth      = 0;

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

  /** set maximal progress value
   * @param minimum minimum value
   * @param maximum maximum value
   */
  public void setRange(double minimum, double maximum)
  {
    this.minimum = minimum;
    this.maximum = maximum;
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
   * @param format format string
   * @param n value [minimum..maximum]
   */
  public void setSelection(String format, double n)
  {
    GC gc;

    if (!isDisposed())
    {
      double newValue = Math.min(Math.max(((maximum-minimum) > 0.0)
                                            ? (n-minimum)/(maximum-minimum)
                                            : 0.0,
                                          0.0
                                         ),
                                 1.0
                                );
      String newText  = String.format(format,newValue*100.0);

      if (isRedrawRequired(newValue,newText))
      {
        value    = newValue;
        subValue = 0.0;
        text     = newText;

        gc = new GC(this);
        textSize = gc.stringExtent(text);
        gc.dispose();

        redraw();
      }
    }
  }

  /** set progress value
   * @param n value [minimum..maximum]
   */
  public void setSelection(double n)
  {
    setSelection("%.1f%%",n);
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
      double newSubValue = Math.min(Math.max(((maximum-minimum) > 0.0)
                                               ? ((value+n/(double)subValueCount)-minimum)/(maximum-minimum)
                                               : 0.0,
                                             0.0
                                            ),
                                    1.0
                                   );
      String newText     = String.format("%.1f%%",(value+newSubValue)*100.0);

      if (isRedrawRequired(value+newSubValue,newText))
      {
        subValue = newSubValue;
        text     = newText;

        gc = new GC(this);
        textSize = gc.stringExtent(text);
        gc.dispose();

        redraw();
      }
    }
  }

  /** convert to string
   * @return string
   */
  @Override
  public String toString()
  {
    return "ProgressBar {"+minimum+", "+maximum+"}";
  }

  /** free allocated resources
   * @param disposeEvent dispose event
   */
  private void widgetDisposed(DisposeEvent disposeEvent)
  {
    colorBarSet.dispose();
  }

  /** check if redraw of progress bar is required (bar value change/text change)
   * @param newValue new value
   * @param newText new text
   */
  private boolean isRedrawRequired(double newValue, String newText)
  {
    Rectangle bounds;
    int       w;
    int       newBarWidth;

    bounds = getBounds();
    w = bounds.width;

    newBarWidth = (int)((double)(w-4)*(value+subValue));

    return    (newBarWidth != currentBarWidth)  // bar width changed
           || !currentText.equals(newText);     // text changed
  }

  /** paint progress bar
   * @param paintEvent paint event
   */
  private void paint(PaintEvent paintEvent)
  {
    GC        gc;
    Rectangle bounds;
    int       x,y,w,h;
    int       barWidth;

    // get bounds
    gc = paintEvent.gc;
    bounds = getBounds();
    x = 0;
    y = 0;
    w = bounds.width;
    h = bounds.height;

    // get bar width
    barWidth = (int)((double)(w-4)*(value+subValue));

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
    gc.fillRectangle(x+2,y+2,barWidth,h-4);

    // draw text
    gc.setForeground(colorBlack);
    gc.drawString(text,(w-textSize.x)/2,(h-textSize.y)/2,true);

    // save current
    currentText     = text;
    currentBarWidth = barWidth;
  }
}

/* end of file */
