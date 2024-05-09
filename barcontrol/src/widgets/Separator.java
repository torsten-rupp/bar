/***********************************************************************\
*
* Contents: separator widget
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

/** separator bar with percentage view
 */
class Separator extends Canvas
{
  // -------------------------- constants -------------------------------

  // -------------------------- variables -------------------------------
  private Point  textSize;
  private String text;

  private Color  colorBlack;
  private Color  colorNormalShadow;
  private Color  colorHighlightShadow;

  // ----------------------- native functions ---------------------------

  // --------------------------- methods --------------------------------

  /** create separator bar
   * @param composite parent composite widget
   * @param text text
   * @param style style flags
   */
  Separator(Composite composite, String text, int style)
  {
    super(composite,style);

    this.colorBlack           = getDisplay().getSystemColor(SWT.COLOR_BLACK);
    this.colorNormalShadow    = getDisplay().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW);
    this.colorHighlightShadow = getDisplay().getSystemColor(SWT.COLOR_WIDGET_HIGHLIGHT_SHADOW);

    addDisposeListener(new DisposeListener()
    {
      public void widgetDisposed(DisposeEvent disposeEvent)
      {
        Separator.this.widgetDisposed(disposeEvent);
      }
    });
    addPaintListener(new PaintListener()
    {
      public void paintControl(PaintEvent paintEvent)
      {
        Separator.this.paint(paintEvent);
      }
    });

    setText(text);
  }

  /** create separator
   * @param composite parent composite widget
   * @param text text
   */
  Separator(Composite composite, String text)
  {
    this(composite,text,SWT.NONE);
  }

  /** create separator
   * @param composite parent composite widget
   */
  Separator(Composite composite)
  {
    this(composite,"");
  }

  /** compute size of pane
   * @param wHint,hHint width/height hint
   * @param changed TRUE iff no cache values should be used
   * @return size
   */
  public Point computeSize(int wHint, int hHint, boolean changed)
  {
    GC  gc;
    int width,height;

    if (text != null)
    {
      width  = 2+textSize.x+2;
      height = 2+textSize.y+2;
    }
    else
    {
      width  = 2+2;
      height = 2+2;
    }
    if (wHint != SWT.DEFAULT) width  = wHint;
    if (hHint != SWT.DEFAULT) height = hHint;

    return new Point(width,height);
  }

  /** set text
   * @param text text
   */
  public void setText(String text)
  {
    GC gc;

    if (!isDisposed())
    {
      this.text = text;

      if (text != null)
      {
        gc = new GC(this);
        textSize = gc.stringExtent(text);
        gc.dispose();
      }
      else
      {
        textSize = null;
      }

      redraw();
    }
  }

  /** free allocated resources
   * @param disposeEvent dispose event
   */
  private void widgetDisposed(DisposeEvent disposeEvent)
  {
  }

  /** paint separator bar
   * @param paintEvent paint event
   */
  private void paint(PaintEvent paintEvent)
  {
    GC        gc;
    Rectangle bounds;
    int       x,y,w,h;

    gc     = paintEvent.gc;
    bounds = getBounds();
    x      = 0;
    y      = 0;
    w      = bounds.width;
    h      = bounds.height;

    if (text != null)
    {
      // draw broken line with shadow
      gc.setForeground(colorNormalShadow);
      gc.drawLine(x                 +0,y+h/2  ,x+(w-textSize.x)/2-4,y+h/2  );
      gc.drawLine(x+(w+textSize.x)/2+4,y+h/2  ,x+w                 ,y+h/2  );
      gc.setForeground(colorHighlightShadow);
      gc.drawLine(x                 +1,y+h/2+1,x+(w-textSize.x)/2-5,y+h/2+1);
      gc.drawLine(x+(w+textSize.x)/2+5,y+h/2+1,x+w               -1,y+h/2+1);

      // draw text
      gc.setForeground(colorBlack);
      gc.drawString(text,(w-textSize.x)/2,(h-textSize.y)/2,true);
    }
    else
    {
      // draw continuous line with shadow
      gc.setForeground(colorNormalShadow);
      gc.drawLine(x                 +0,y+h/2  ,x+w                 ,y+h/2  );
      gc.setForeground(colorHighlightShadow);
      gc.drawLine(x                 +1,y+h/2+1,x+w               -1,y+h/2+1);
    }
  }
}

/* end of file */
