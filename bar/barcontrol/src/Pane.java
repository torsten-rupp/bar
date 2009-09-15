/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Pane.java,v $
* $Revision: 1.2 $
* $Author: torsten $
* Contents: pane widget
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import org.eclipse.swt.events.DisposeEvent;
import org.eclipse.swt.events.DisposeListener;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.MouseTrackListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.Cursor;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;

/****************************** Classes ********************************/

/** pane widget (vertical, horizontal)
 */
class Pane extends Composite
{
  // --------------------------- constants --------------------------------
  private final int SIZE          = 8;
  private final int SLIDER_WIDTH  = 8;
  private final int SLIDER_HEIGHT = 8;
  private final int SLIDER_OFFSET = 8;
  private final int OFFSET_X      = 0;
  private final int OFFSET_Y      = 0;

  // --------------------------- variables --------------------------------
  private int     style;                 // style flags
  private Pane    prevPane;              // previous pane or null
  private Pane    nextPane;              // next pane or null
  private int     delta;                 // current pane delta
  private boolean dragFlag;              // TRUE iff drag slider in progress
  private int     dragStart;             // drag start value
  private int     dragDelta;             // current drag delta value

  private Color   colorGray;
  private Color   colorWhite;
  private Color   colorNormalShadow;
  private Color   colorHighlightShadow;

  private Cursor  cursor;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create pane
   * @param parent parent widget
   * @param style style flags (support VERTICAL, HORIZONTAL)
   */
  Pane(Composite parent, int style, Pane prevPane)
  {
    super(parent,SWT.NONE);

    // initialize variables
    this.style                = style;
    this.prevPane             = prevPane;
    this.nextPane             = null;
    this.delta                = 0;
    this.dragFlag             = false;

    this.colorWhite           = getDisplay().getSystemColor(SWT.COLOR_WHITE);
    this.colorGray            = getDisplay().getSystemColor(SWT.COLOR_GRAY);
    this.colorNormalShadow    = getDisplay().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW);
    this.colorHighlightShadow = getDisplay().getSystemColor(SWT.COLOR_WIDGET_HIGHLIGHT_SHADOW);

    this.cursor               = new Cursor(getDisplay(),SWT.CURSOR_SIZEWE);
   
    // add paint listener
    addPaintListener(new PaintListener()
    {
      public void paintControl(PaintEvent paintEvent)
      {
        Pane.this.paint(paintEvent);
      }
    });

    // add mouse listeners
    addMouseListener(new MouseListener()
    {
      public void mouseDoubleClick(MouseEvent mouseEvent)
      {
      }
      public void mouseDown(MouseEvent mouseEvent)
      {
        Pane pane = Pane.this;

        // start dragging slider
        pane.dragFlag  = true;
        pane.dragStart = ((pane.style & SWT.VERTICAL) == SWT.VERTICAL)?mouseEvent.x:mouseEvent.y;
        pane.dragDelta = 0;
      }
      public void mouseUp(MouseEvent mouseEvent)
      {
        Pane pane = Pane.this;

        // save dragging delta
        pane.delta += pane.dragDelta;

        // stop dragging slider
        pane.dragFlag  = false;
        pane.dragDelta = 0;
      }
    });
    addMouseTrackListener(new MouseTrackListener()
    {
      public void mouseEnter(MouseEvent mouseEvent)
      {
        Pane pane = Pane.this;

        setCursor(pane.checkInside(mouseEvent.x,mouseEvent.y)?cursor:null);
      }
      public void mouseExit(MouseEvent mouseEvent)
      {
        Pane pane = Pane.this;

        pane.setCursor(null);
      }
      public void mouseHover(MouseEvent mouseEvent)
      {
      }
    });
    addMouseMoveListener(new MouseMoveListener()
    {
      public void mouseMove(MouseEvent mouseEvent)
      {
        Pane      pane = Pane.this;
        Point     size;
        Rectangle bounds;
        int       dragDelta;
        int       n;
        int       min,max;

        // set cursor
        setCursor(pane.checkInside(mouseEvent.x,mouseEvent.y)?cursor:null);

        // drag
        if (pane.dragFlag)
        {
          size   = pane.computeSize(SWT.DEFAULT,SWT.DEFAULT,false);
          bounds = pane.getParent().getBounds();
          if ((pane.style & SWT.VERTICAL) == SWT.VERTICAL)
          {
            dragDelta = mouseEvent.x-pane.dragStart;
            n         = size.x;
            min       = SIZE;
            max       = bounds.width-SIZE;
          }
          else
          {
            dragDelta = mouseEvent.y-pane.dragStart;
            n         = size.y;
            min       = SIZE;
            max       = bounds.height-SIZE;
          }
//Dprintf.dprintf("d=%d %d n=%d",pane.delta,pane.dragDelta,n);

          if (   (dragDelta != pane.dragDelta)
              && ((n+(dragDelta-pane.dragDelta)) > min)
              && ((n+(dragDelta-pane.dragDelta)) < max)
             )
          {
            pane.dragDelta = dragDelta;
            pane.getShell().layout(true,true);
          }
        }
      }
    });

    if (prevPane != null)
    {
      prevPane.nextPane = this;
    }
  }

  /** get size of client area
   * @return client area size
   */
  public Rectangle getClientArea()
  {
    Rectangle bounds = super.getClientArea();
    if (nextPane != null)
    {
      if ((style & SWT.VERTICAL) == SWT.VERTICAL)
      {
        bounds.width -= SIZE;
      }
      else
      {
        bounds.height -= SIZE;
      }
    }

    return bounds;
  }

  /** compute size of pane
   * @param wHint,hHint width/height hint
   * @param changed TRUE iff no cache values should be used
   * @return size
   */
  public Point computeSize(int wHint, int hHint, boolean changed)
  {
    Point size = super.computeSize(wHint,hHint,changed);
    if (nextPane != null)
    {
      if ((style & SWT.VERTICAL) == SWT.VERTICAL)
      {
        size.x = size.x+SIZE+delta+dragDelta;
      }
      else
      {
        size.y = size.y+SIZE+delta+dragDelta;
      }
    }

    return size;
  }

  /** get string
   * @return pane object string
   */
  public String toString()
  {
    return "{pane@"+hashCode()+", "+delta+"}";
  }

  /** free allocated resources
   * @param disposeEvent dispose event
   */
  private void widgetDisposed(DisposeEvent disposeEvent)
  {
  }

  /** paint slider
   * @param paintEvent paint event
   */
  private void paint(PaintEvent paintEvent)
  {
    GC        gc;
    Rectangle bounds;
    int       x,y,w,h;

    if (!isDisposed())
    {
      if (nextPane != null)
      {
        gc     = paintEvent.gc;
        bounds = getBounds();
        w      = bounds.width;
        h      = bounds.height;

        if ((style & SWT.VERTICAL) == SWT.VERTICAL)
        {
          x = w-OFFSET_X-SLIDER_WIDTH;
          y = h-OFFSET_Y-SLIDER_OFFSET-SLIDER_HEIGHT;

          // vertical slider
          gc.setForeground(colorHighlightShadow);
          gc.drawLine(x+SLIDER_WIDTH/2-1,0,x+SLIDER_WIDTH/2-1,h-1);
          gc.drawLine(x+SLIDER_WIDTH/2-1,0,x+SLIDER_WIDTH/2  ,0  );
          gc.setForeground(colorNormalShadow);
          gc.drawLine(x+SLIDER_WIDTH/2  ,1,x+SLIDER_WIDTH/2  ,h-1);
        }
        else
        {
          x = w-OFFSET_X-SLIDER_OFFSET-SLIDER_WIDTH;
          y = h-OFFSET_Y-SLIDER_HEIGHT;

          // horizonal slider
          gc.setForeground(colorHighlightShadow);
          gc.drawLine(0,y+SLIDER_HEIGHT/2-1,w-1,y+SLIDER_HEIGHT/2-1);
          gc.drawLine(0,y+SLIDER_HEIGHT/2  ,1  ,y+SLIDER_HEIGHT/2  );
          gc.setForeground(colorNormalShadow);
          gc.drawLine(1,y+SLIDER_HEIGHT/2  ,w-1,y+SLIDER_HEIGHT/2  );
        }

        gc.setForeground(colorWhite);
        gc.drawLine(x,y,  x+SLIDER_WIDTH,y                );
        gc.drawLine(x,y+1,x             ,y+SLIDER_HEIGHT-1);
        gc.setForeground(colorGray);
        gc.fillRectangle(x+1,y+1,SLIDER_WIDTH-2,SLIDER_HEIGHT-2);
        gc.setForeground(colorNormalShadow);
        gc.drawLine(x+SLIDER_WIDTH-1,y+1              ,x+SLIDER_WIDTH-1,y+SLIDER_HEIGHT-1);
        gc.drawLine(x+1             ,y+SLIDER_HEIGHT-1,x+SLIDER_WIDTH-2,y+SLIDER_HEIGHT-1);
      }
    }
  }

  /** check if x,y is inside slider area
   * @param x,y coordinates
   * @return true iff x,y is inside slider area
   */
  private boolean checkInside(int x, int y)
  {
    Rectangle bounds = getBounds();

    if ((style & SWT.VERTICAL) == SWT.VERTICAL)
    {
      return (   (x >= (bounds.width -OFFSET_X-SLIDER_WIDTH))
              && (x <= (bounds.width -OFFSET_X             ))
             );
    }
    else
    {
      return (   (y >= (bounds.height-OFFSET_Y-SLIDER_HEIGHT))
              && (y <= (bounds.height-OFFSET_Y              ))
             );
    }
  }
}

/* end of file */
