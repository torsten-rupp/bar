/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/ProgressBar.java,v $
* $Revision: 1.3 $
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
 */
class ProgressBar extends Canvas
{
  // --------------------------- constants --------------------------------

  // --------------------------- variables --------------------------------
  Color  barColor;
  Color  barSetColor;
  double minimum;
  double maximum;
  double value;
  Point  textSize;
  String text;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create progress bar
   * @param composite parent composite widget
   * @param style style flags
   */
  ProgressBar(Composite composite, int style)
  {
    super(composite,SWT.NONE);

    barColor    = getDisplay().getSystemColor(SWT.COLOR_WHITE);
    barSetColor = new Color(null,0xAD,0xD8,0xE6);

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

    width  = textSize.x;
    height = textSize.y;
    if (wHint != SWT.DEFAULT) width  = wHint;
    if (hHint != SWT.DEFAULT) height = hHint;         

    return new Point(width,height);
  }

  /** set minimal progress value
   * @param n value
   */
  public void setMinimum(double n)
  {
    this.minimum = n;
  }

  /** set maximal progress value
   * @param n value
   */
  public void setMaximum(double n)
  {
    this.maximum = n;
  }

  /** set progress value
   * @param n value
   */
  public void setSelection(double n)
  {
    GC gc;

    value = Math.min(Math.max(((maximum-minimum)>0.0)?(n-minimum)/(maximum-minimum):0.0,minimum),maximum);

    gc = new GC(this);
    text = String.format("%.1f%%",value*100.0);
    textSize = gc.stringExtent(text);
    gc.dispose();

    redraw();
  }

  /** free allocated resources
   * @param disposeEvent dispose event
   */
  private void widgetDisposed(DisposeEvent disposeEvent)
  {
    barSetColor.dispose();
    barColor.dispose();
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
    gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_DARK_GRAY));
    gc.drawLine(x,y,  x+w-1,y    );
    gc.drawLine(x,y+1,x,    y+h-1);

    gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_WHITE));
    gc.drawLine(x+1,  y+h-1,x+w-1,y+h-1);
    gc.drawLine(x+w-1,y+1,  x+w-1,y+h-1);

    // draw bar
    gc.setBackground(barColor);
    gc.fillRectangle(x+1,y+1,w-2,h-2);
    gc.setBackground(barSetColor);
    gc.fillRectangle(x+1,y+1,(int)((double)w*value)-2,h-2);

    // draw percentage text
    gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_BLACK));
    gc.drawString(text,(w-textSize.x)/2,(h-textSize.y)/2,true);
  }
}

/* end of file */
