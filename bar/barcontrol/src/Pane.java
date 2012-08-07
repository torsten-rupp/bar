/***********************************************************************\
*
* $Source: /tmp/cvs/onzen/src/Pane.java,v $
* $Revision: 970 $
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
  private final int SIZE          = 8;   // total width/height of slider
  private final int SLIDER_SIZE   = 8;   // width/height of slider button
  private final int SLIDER_OFFSET = 8;   // offset from end of slider line
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

    if ((style & SWT.VERTICAL) == SWT.VERTICAL)
    {
      this.cursor = new Cursor(getDisplay(),SWT.CURSOR_SIZEWE);
    }
    else
    {
      this.cursor = new Cursor(getDisplay(),SWT.CURSOR_SIZENS);
    }

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
//        Pane pane = Pane.this;
        Pane pane = (Pane)mouseEvent.widget;

        // start dragging slider
        pane.dragFlag  = true;
        pane.dragStart = ((pane.style & SWT.VERTICAL) == SWT.VERTICAL) ? mouseEvent.x : mouseEvent.y;
        pane.dragDelta = 0;
      }
      public void mouseUp(MouseEvent mouseEvent)
      {
//        Pane pane = Pane.this;
        Pane pane = (Pane)mouseEvent.widget;

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
//        Pane pane = Pane.this;
        Pane pane = (Pane)mouseEvent.widget;

        setCursor(pane.checkInside(mouseEvent.x,mouseEvent.y) ? cursor : null);
      }
      public void mouseExit(MouseEvent mouseEvent)
      {
//        Pane pane = Pane.this;
        Pane pane = (Pane)mouseEvent.widget;

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
//        Pane      pane = Pane.this;
        Pane pane = (Pane)mouseEvent.widget;
        Point     size;
        Rectangle bounds;
        int       dragDelta;
        int       n;
        int       min,max;

        // set cursor
        setCursor(pane.checkInside(mouseEvent.x,mouseEvent.y) ? cursor : null);

        // drag
        if (pane.dragFlag)
        {
          // get current size of pane
          size = pane.computeSize(SWT.DEFAULT,SWT.DEFAULT,false);

          // get available space from parent
          bounds = pane.getParent().getBounds();
//Dprintf.dprintf("size=%s bounds=%s",size,bounds);

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
//Dprintf.dprintf("delta=%d dragDelta=%d n=%d min=%d max=%d",pane.delta,pane.dragDelta,n,min,max);

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

  /** compute trim
   * Note: required?
   * @param x,y position
   * @param width,height size
   * @return reduced rectangle
   */
  public Rectangle computeTrim(int x, int y, int width, int height)
  {
//Dprintf.dprintf("computeTrim %d %d %d %d",x,y,width,height);
    Rectangle trim;
    if ((style & SWT.VERTICAL) == SWT.VERTICAL)
    {
      trim = new Rectangle(0,0,width-SLIDER_SIZE-OFFSET_X,height);
    }
    else
    {
      trim = new Rectangle(0,0,width,height-SLIDER_SIZE-OFFSET_Y);
    }
//Dprintf.dprintf("trim=%s",trim);

    return trim;
  }

  /** get size of client area
   * @return client area size
   */
  public Rectangle getClientArea()
  {
//Dprintf.dprintf("getClientArea");
    Rectangle bounds = super.getClientArea();
    if (nextPane != null)
    {
      if ((style & SWT.VERTICAL) == SWT.VERTICAL)
      {
        bounds.width -= (OFFSET_X+SLIDER_SIZE);
      }
      else
      {
        bounds.height -= (OFFSET_Y+SLIDER_SIZE);
      }
    }
//Dprintf.dprintf(this+" getClientArea end "+bounds);

    return bounds;
  }

  /** compute size of pane
   * @param wHint,hHint width/height hint
   * @param changed TRUE iff no cache values should be used
   * @return size
   */
  public Point computeSize(int wHint, int hHint, boolean changed)
  {
//Dprintf.dprintf(this+" computeSize begin");
    Point size = super.computeSize(wHint,hHint,changed);
    if (nextPane != null)
    {
      if ((style & SWT.VERTICAL) == SWT.VERTICAL)
      {
        size.x += OFFSET_X+SLIDER_SIZE+delta+dragDelta;
      }
      else
      {
        size.y += OFFSET_Y+SLIDER_SIZE+delta+dragDelta;
      }
    }
//Dprintf.dprintf(this+" computeSize end "+size+" "+delta+", "+dragDelta);

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
          x = w-OFFSET_X-SLIDER_SIZE;
          y = h-OFFSET_Y-SLIDER_OFFSET-SLIDER_SIZE;

          /* vertical slider
             ##
             #o
             #o
             #o

             # = highlight shadow
             o = shadow
          */
          gc.setForeground(colorHighlightShadow);
          gc.drawLine(x+SLIDER_SIZE/2-1,0,x+SLIDER_SIZE/2-1,h-1);
          gc.drawLine(x+SLIDER_SIZE/2  ,0,x+SLIDER_SIZE/2  ,0  );
          gc.setForeground(colorNormalShadow);
          gc.drawLine(x+SLIDER_SIZE/2  ,1,x+SLIDER_SIZE/2  ,h-1);
        }
        else
        {
          x = w-OFFSET_X-SLIDER_OFFSET-SLIDER_SIZE;
          y = h-OFFSET_Y-SLIDER_SIZE;

          /* horizonal slider
             ####
             #ooo

             # = highlight shadow
             o = shadow
          */
          gc.setForeground(colorHighlightShadow);
          gc.drawLine(0,y+SLIDER_SIZE/2-1,w-1,y+SLIDER_SIZE/2-1);
          gc.drawLine(0,y+SLIDER_SIZE/2  ,1  ,y+SLIDER_SIZE/2  );
          gc.setForeground(colorNormalShadow);
          gc.drawLine(1,y+SLIDER_SIZE/2  ,w-1,y+SLIDER_SIZE/2  );
        }

        /* slider button
           @******
           *     o
           *     o
           *     o
           *oooooo

           * = white
           o = shadow

        */
        gc.setForeground(colorWhite);
        gc.drawLine(x,y,  x+SLIDER_SIZE-1,y              );
        gc.drawLine(x,y+1,x              ,y+SLIDER_SIZE-1);
        gc.setForeground(colorGray);
        gc.fillRectangle(x+1,y+1,SLIDER_SIZE-2,SLIDER_SIZE-2);
        gc.setForeground(colorNormalShadow);
        gc.drawLine(x+SLIDER_SIZE-1,y+1            ,x+SLIDER_SIZE-1,y+SLIDER_SIZE-1);
        gc.drawLine(x+1            ,y+SLIDER_SIZE-1,x+SLIDER_SIZE-2,y+SLIDER_SIZE-1);
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
      return (   (x >= (bounds.width -OFFSET_X-SLIDER_SIZE))
              && (x <= (bounds.width -OFFSET_X            ))
             );
    }
    else
    {
      return (   (y >= (bounds.height-OFFSET_Y-SLIDER_SIZE))
              && (y <= (bounds.height-OFFSET_Y            ))
             );
    }
  }
}

/* end of file */
