/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/Pane.java,v $
* $Revision: 1.1 $
* $Author: torsten $
* Contents: pane widget
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.MouseListener;
import org.eclipse.swt.events.MouseMoveListener;
import org.eclipse.swt.events.PaintEvent;
import org.eclipse.swt.events.PaintListener;
import org.eclipse.swt.graphics.Color;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Composite;

import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Event;

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
  int     style;                 // style flags
  Pane    prevPane;              // previous pane or null
  Pane    nextPane;              // next pane or null
  int     delta;                 // current pane delta
  boolean dragFlag;              // TRUE iff drag slider in progress
  int     dragStart;             // drag start value
  int     dragDelta;             // current drag delta value

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
    this.style    = style;
    this.prevPane = prevPane;
    this.nextPane = null;
    this.delta    = 0;
    this.dragFlag = false;
   
    // add paint listener
    addPaintListener(new PaintListener()
    {
      public void paintControl(PaintEvent paintEvent)
      {
        Pane.this.paint(paintEvent);
      }
    });

    // add mouse click/drag listeners
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

  /** paint slider
   * @param paintEvent paint event
   */
  private void paint(PaintEvent paintEvent)
  {
    GC        gc;
    Rectangle bounds;
    int       x,y,w,h;

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
        gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_WHITE));
        gc.drawLine(x+SLIDER_WIDTH/2-1,0,x+SLIDER_WIDTH/2-1,h-1);
        gc.drawLine(x+SLIDER_WIDTH/2-1,0,x+SLIDER_WIDTH/2  ,0  );
        gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_DARK_GRAY));
        gc.drawLine(x+SLIDER_WIDTH/2  ,1,x+SLIDER_WIDTH/2  ,h-1);
      }
      else
      {
        x = w-OFFSET_X-SLIDER_OFFSET-SLIDER_WIDTH;
        y = h-OFFSET_Y-SLIDER_HEIGHT;

        // horizonal slider
        gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_WHITE));
        gc.drawLine(0,y+SLIDER_HEIGHT/2-1,w-1,y+SLIDER_HEIGHT/2-1);
        gc.drawLine(0,y+SLIDER_HEIGHT/2  ,1  ,y+SLIDER_HEIGHT/2  );
        gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_DARK_GRAY));
        gc.drawLine(1,y+SLIDER_HEIGHT/2  ,w-1,y+SLIDER_HEIGHT/2  );
      }

      gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_WHITE));
      gc.drawLine(x,y,  x+SLIDER_WIDTH,y                );
      gc.drawLine(x,y+1,x             ,y+SLIDER_HEIGHT-1);
      gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_GRAY));
      gc.fillRectangle(x+1,y+1,SLIDER_WIDTH-2,SLIDER_HEIGHT-2);
      gc.setForeground(getDisplay().getSystemColor(SWT.COLOR_DARK_GRAY));
      gc.drawLine(x+SLIDER_WIDTH-1,y+1              ,x+SLIDER_WIDTH-1,y+SLIDER_HEIGHT-1);
      gc.drawLine(x+1             ,y+SLIDER_HEIGHT-1,x+SLIDER_WIDTH-2,y+SLIDER_HEIGHT-1);
    }
  }
}

/* end of file */
