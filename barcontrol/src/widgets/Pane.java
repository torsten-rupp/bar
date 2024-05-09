/***********************************************************************\
*
* Contents: pane widget
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import java.util.Arrays;
import java.util.HashSet;

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
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Layout;
import org.eclipse.swt.widgets.Listener;

/****************************** Classes ********************************/

/** pane widget (vertical, horizontal)
 */
public class Pane extends Canvas
{
  // -------------------------- constants -------------------------------
  public  final static int SIZE          = 12;   // total width/height of slider

  private final static int SLIDER_SIZE   =  8;   // width/height of slider button
  private final static int SLIDER_OFFSET =  8;   // offset from end of slider line
  private final static int OFFSET_X      =  2;
  private final static int OFFSET_Y      =  2;

  private final Color      COLOR_GRAY;
  private final Color      COLOR_WHITE;
  private final Color      COLOR_NORMAL_SHADOW;
  private final Color      COLOR_HIGHLIGHT_SHADOW;

  private final Cursor     CURSOR;

  // -------------------------- variables -------------------------------
  private Composite         composites[];
  private boolean           initFlag = false;
  private int               offsets[];               // offset of composites
  private double            weights[];               // size weights [0..1]
  private int               count;                   // number of composites
  private int               style;                   // style flags
  private int               dragIndex;               // index of sash if dragging is in progress, otherwise -1
  private int               dragStart;               // drag start value
  private int               dragDelta;               // current drag delta value
  private HashSet<Listener> resizeListenerSet = new HashSet<Listener>();


  /* offsets:

     +----+----+----+
     |    |    |    |
     +----+----+----+
                    \- offset[0]
               \------ offset[1]
          \----------- offset[2]

  */

  // ----------------------- native functions ---------------------------

  // --------------------------- methods --------------------------------

  /** create pane
   * @param parent parent widget
   * @param style style flags (support VERTICAL, HORIZONTAL)
   */
  Pane(Composite parent, int count, int style)
  {
    super(parent,SWT.NONE);

    // check arguments
    if (count <= 1) throw new IllegalArgumentException();

    // get colors
    this.COLOR_WHITE            = getDisplay().getSystemColor(SWT.COLOR_WHITE);
    this.COLOR_GRAY             = getDisplay().getSystemColor(SWT.COLOR_GRAY);
    this.COLOR_NORMAL_SHADOW    = getDisplay().getSystemColor(SWT.COLOR_WIDGET_NORMAL_SHADOW);
    this.COLOR_HIGHLIGHT_SHADOW = getDisplay().getSystemColor(SWT.COLOR_WIDGET_HIGHLIGHT_SHADOW);

    // get cursor
    if      ((style & SWT.VERTICAL) == SWT.VERTICAL)
    {
      this.CURSOR = new Cursor(getDisplay(),SWT.CURSOR_SIZEWE);
    }
    else if ((style & SWT.HORIZONTAL) == SWT.HORIZONTAL)
    {
      this.CURSOR = new Cursor(getDisplay(),SWT.CURSOR_SIZENS);
    }
    else
    {
      throw new IllegalArgumentException("invalid style");
    }

    // initialize variables
    this.composites = new Composite[count];
    this.offsets    = new int[count];
    this.weights    = new double[count];
    this.count      = count;
    this.style      = style;
    this.dragIndex  = -1;

    // create sashes
    for (int i = 0; i < count; i++)
    {
      composites[i] = new Composite(this,SWT.NONE);
    }
    updateSizes();
    updateWeights();

    // add paint listeners
    addPaintListener(new PaintListener()
    {
      public void paintControl(PaintEvent paintEvent)
      {
        if (!initFlag)
        {
          updateSizes();
          updateWeights();
          initFlag = true;
        }

        Pane.this.paint(paintEvent);
      }
    });

    // add resize listeners
    super.addListener(SWT.Resize, new Listener()
    {
      public void handleEvent(Event event)
      {
        updateSizes();
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

        pane.dragIndex = getSashAt(mouseEvent.x,mouseEvent.y);
        if (pane.dragIndex >= 0)
        {
          // start dragging slider
          pane.dragStart = ((pane.style & SWT.VERTICAL) == SWT.VERTICAL) ? mouseEvent.x : mouseEvent.y;
          pane.dragDelta = 0;
        }
      }
      public void mouseUp(MouseEvent mouseEvent)
      {
        Pane pane = Pane.this;

        if (pane.dragIndex >= 0)
        {
          // notify
          notifyResize(pane.dragIndex,
                       pane.offsets[pane.dragIndex]-((pane.dragIndex > 0) ? offsets[pane.dragIndex-1] : 0)-SIZE
                      );
        }

        // stop dragging slider
        pane.dragIndex = -1;
        pane.dragDelta = 0;

        updateWeights();
      }
    });
    addMouseTrackListener(new MouseTrackListener()
    {
      public void mouseEnter(MouseEvent mouseEvent)
      {
      }
      public void mouseExit(MouseEvent mouseEvent)
      {
        Pane pane = Pane.this;

        setCursor(null);
      }
      public void mouseHover(MouseEvent mouseEvent)
      {
      }
    });
    addMouseMoveListener(new MouseMoveListener()
    {
      public void mouseMove(MouseEvent mouseEvent)
      {
        Composite composite = (Composite)mouseEvent.widget;
        Pane pane = Pane.this;
        Rectangle bounds0,bounds1;
        Rectangle bounds;
        int       dragDelta;

        // set cursor
        setCursor(Pane.this.isInsideSash(mouseEvent.x,mouseEvent.y) ? CURSOR : null);

        // drag
        if (pane.dragIndex >= 0)
        {
          // get available space from parent
          bounds = pane.getBounds();
//Dprintf.dprintf("size=%s bounds=%s",size,bounds);

          // get bounds of sashes
          bounds0 = composites[pane.dragIndex+0].getBounds();
          bounds1 = composites[pane.dragIndex+1].getBounds();

          if      ((pane.style & SWT.VERTICAL) == SWT.VERTICAL)
          {
            dragDelta = mouseEvent.x-pane.dragStart;

            if (dragDelta != 0)
            {
              // limit drag delta
              if ((bounds0.width+dragDelta) < 0) dragDelta = -bounds0.width;
              if ((bounds1.width-dragDelta) < 0) dragDelta =  bounds1.width;
//Dprintf.dprintf("dragDelta=%d",dragDelta);

              // shrink/expand sashes
              bounds0.width  += dragDelta;
              bounds1.x      += dragDelta;
              bounds1.width  -= dragDelta;
              composites[pane.dragIndex+0].setBounds(bounds0);
              composites[pane.dragIndex+1].setBounds(bounds1);
              offsets[pane.dragIndex+0] += dragDelta;

              // redraw sash
              redraw();

              // reset drag start
              pane.dragStart = mouseEvent.x;
            }
          }
          else if ((pane.style & SWT.HORIZONTAL) == SWT.HORIZONTAL)
          {
            dragDelta = mouseEvent.y-pane.dragStart;

            if (dragDelta != 0)
            {
              // limit drag delta
              if ((bounds0.height+dragDelta) < 0) dragDelta = -bounds0.height;
              if ((bounds1.height-dragDelta) < 0) dragDelta =  bounds1.height;

              // shrink/expand sashes
              bounds0.height += dragDelta;
              bounds1.y      += dragDelta;
              bounds1.height -= dragDelta;
              composites[pane.dragIndex+0].setBounds(bounds0);
              composites[pane.dragIndex+1].setBounds(bounds1);
              offsets[pane.dragIndex+0] += dragDelta;

              // redraw sash
              redraw();

              // reset drag start
              pane.dragStart = mouseEvent.y;
            }
          }

          // notify
          notifyResize(pane.dragIndex,
                       pane.offsets[pane.dragIndex]-((pane.dragIndex > 0) ? offsets[pane.dragIndex-1] : 0)-SIZE
                      );
        }
      }
    });
  }

  /** get sash composite
   * @param i index [0..count-1]
   * @return composite
   */
  public Composite getComposite(int i)
  {
    return composites[i];
  }

  /** get sizes of sashes
   * @return width array [%]
   */
  public double[] getSizes()
  {
    double sizes[] = new double[count];

    // set sizes of sashes
    Rectangle bounds = getBounds();
    if      ((style & SWT.VERTICAL) == SWT.VERTICAL)
    {
      int width = bounds.width-(count-1)*SIZE;
//Dprintf.dprintf("width=%d",width);

      int w;
      for (int i = 0; i < count; i++)
      {
        w = offsets[i]-((i > 0) ? offsets[i-1] : 0)-SIZE;
        sizes[i] = (double)w/(double)width;
      }
    }
    else if ((style & SWT.HORIZONTAL) == SWT.HORIZONTAL)
    {
      int height = bounds.height-(count-1)*SIZE;

      int h;
      for (int i = 0; i < count; i++)
      {
        h = offsets[i]-((i > 0) ? offsets[i-1] : 0)-SIZE;
        sizes[i] = (double)h/(double)height;
      }
    }

    return sizes;
  }

  /** set sizes of sashes
   * @param widths width array [%]
   */
  public void setSizes(double sizes[])
  {
    // adjust array size
    if (sizes.length != count)
    {
      double newSizes[] = Arrays.copyOf(sizes,count);
      if (newSizes.length > sizes.length)
      {
        for (int i = sizes.length; i < newSizes.length; i++)
        {
          newSizes[i] = 1.0;
        }
      }
      sizes = newSizes;
    }

    // get normalize factor
    double normalizeFactor = 0.0;
    for (int i = 0; i < count; i++)
    {
      normalizeFactor += sizes[i];
    }
    if (normalizeFactor < 0.0) normalizeFactor = 1.0/(double)(count-1);

    // set weights and mark initialized
    for (int i = 0; i < count; i++)
    {
      weights[i] = sizes[i]/normalizeFactor;
    }
    initFlag = true;

    // set sizes of sashes
    Rectangle bounds = getBounds();
    if      ((style & SWT.VERTICAL) == SWT.VERTICAL)
    {
      int width = bounds.width-(count-1)*SIZE;

      int x = 0;
      int w;
      for (int i = 0; i < count; i++)
      {
        w = (int)((double)width*sizes[i]/normalizeFactor);

        bounds = composites[i].getBounds();
        bounds.x     = x;
        bounds.width = w;
        composites[i].setBounds(bounds);
        x += w+SIZE;
        offsets[i] = x;
      }
    }
    else if ((style & SWT.HORIZONTAL) == SWT.HORIZONTAL)
    {
      int height = bounds.height-(count-1)*SIZE;

      int y = 0;
      int h;
      for (int i = 0; i < count; i++)
      {
        h = (int)((double)height*sizes[i]/normalizeFactor);

        bounds = composites[i].getBounds();
        bounds.y      = y;
        bounds.height = h;
        composites[i].setBounds(bounds);
        y += h+SIZE;
        offsets[i] = y;
      }
    }
  }

  /** set layout
   * Note: Pane cannot have a layout. Always throw UnsupportedOperationException.
   * @param layout layout
   */
  public void setLayout(Layout layout)
  {
    throw new UnsupportedOperationException("Pane cannot have a layout");
  }

  /** add listener
   * @param listener listener to add
   */
  public void addListener(int eventType, Listener listener)
  {
    super.addListener(eventType,listener);
    if (eventType == SWT.Resize) resizeListenerSet.add(listener);
  }

  /** remove listener
   * @param listener listener to remove
   */
  public void removeListener(int eventType, Listener listener)
  {
    super.removeListener(eventType,listener);
    if (eventType == SWT.Resize) resizeListenerSet.remove(listener);
  }

  /** get string
   * @return pane object string
   */
  public String toString()
  {
    return "Pane@"+hashCode()+" {"+count+", "+(((style & SWT.VERTICAL) == SWT.VERTICAL) ? "vertical" : "horizontal")+"}";
  }

  /** update pane sizes
   * @param
   * @return
  */
  private void updateSizes()
  {
    Pane      pane   = Pane.this;
    Rectangle bounds = pane.getClientArea();

    int dw,dh;
    int x      = 0;
    int y      = 0;
    int width  = 0;
    int height = 0;
    for (int i = 0; i < pane.count; i++)
    {
      if      ((pane.style & SWT.VERTICAL) == SWT.VERTICAL)
      {
        dw = initFlag ? (int)((double)bounds.width*weights[i]) : (bounds.width -(pane.count-1)*SIZE)/pane.count;
        pane.composites[i].setBounds(x,0,dw,bounds.height);
        x += dw + SIZE;
        pane.offsets[i] = x;
      }
      else if ((pane.style & SWT.HORIZONTAL) == SWT.HORIZONTAL)
      {
        dh = initFlag ? (int)((double)bounds.height*weights[i]) : (bounds.height -(pane.count-1)*SIZE)/pane.count;
        pane.composites[i].setBounds(0,y,bounds.width,dh);
        y += dh + SIZE;
        pane.offsets[i] = y;
      }
//Dprintf.dprintf("pane.composites[i]=%s",pane.composites[i].getBounds());
    }
  }

  /** update pane size weights
   */
  private void updateWeights()
  {
    Rectangle bounds = getBounds();
    for (int i = 0; i < count; i++)
    {
      if      ((style & SWT.VERTICAL) == SWT.VERTICAL)
      {
        int x = offsets[i]-((i > 0) ? offsets[i-1] : 0)-bounds.x;
        weights[i] = (bounds.height > 0) ? (double)x/(double)bounds.width : 1.0;
      }
      else if ((style & SWT.HORIZONTAL) == SWT.HORIZONTAL)
      {
        int y = offsets[i]-((i > 0) ? offsets[i-1] : 0)-bounds.y;
        weights[i] = (bounds.width > 0) ? (double)y/(double)bounds.height : 1.0;
      }
    }
  }

  /** paint slider
   * @param paintEvent paint event
   */
  private void paint(PaintEvent paintEvent)
  {
    GC        gc;
    Rectangle bounds;
    int       x0,y0;
    int       w,h;
    int       x,y;

    if (!isDisposed())
    {
      gc     = paintEvent.gc;
      bounds = getBounds();
      x0     = bounds.x;
      y0     = bounds.y;
      w      = bounds.width;
      h      = bounds.height;

      for (int i = 0; i < count-1; i++)
      {
        bounds = composites[i].getBounds();

        if      ((style & SWT.VERTICAL) == SWT.VERTICAL)
        {
          x = bounds.x+bounds.width+OFFSET_X;
          y = h-OFFSET_Y-SLIDER_OFFSET-SLIDER_SIZE;

          /* vertical slider
             ##
             #o
             #o
             #o

             # = highlight shadow
             o = shadow
          */
          gc.setForeground(COLOR_HIGHLIGHT_SHADOW);
          gc.drawLine(x+SLIDER_SIZE/2-1,0,x+SLIDER_SIZE/2-1,h-1);
          gc.drawLine(x+SLIDER_SIZE/2  ,0,x+SLIDER_SIZE/2  ,0  );
          gc.setForeground(COLOR_NORMAL_SHADOW);
          gc.drawLine(x+SLIDER_SIZE/2  ,1,x+SLIDER_SIZE/2  ,h-1);
        }
        else if ((style & SWT.HORIZONTAL) == SWT.HORIZONTAL)
        {
          x = w-OFFSET_X-SLIDER_OFFSET-SLIDER_SIZE;
          y = bounds.y+bounds.height+OFFSET_Y;

          /* horizonal slider
             ####
             #ooo

             # = highlight shadow
             o = shadow
          */
          gc.setForeground(COLOR_HIGHLIGHT_SHADOW);
          gc.drawLine(0,y+SLIDER_SIZE/2-1,w-1,y+SLIDER_SIZE/2-1);
          gc.drawLine(0,y+SLIDER_SIZE/2  ,1  ,y+SLIDER_SIZE/2  );
          gc.setForeground(COLOR_NORMAL_SHADOW);
          gc.drawLine(1,y+SLIDER_SIZE/2  ,w-1,y+SLIDER_SIZE/2  );
        }
        else
        {
          throw new IllegalArgumentException("invalid style");
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
        gc.setForeground(COLOR_WHITE);
        gc.drawLine(x,y,  x+SLIDER_SIZE-1,y              );
        gc.drawLine(x,y+1,x              ,y+SLIDER_SIZE-1);
        gc.setForeground(COLOR_GRAY);
        gc.fillRectangle(x+1,y+1,SLIDER_SIZE-2,SLIDER_SIZE-2);
        gc.setForeground(COLOR_NORMAL_SHADOW);
        gc.drawLine(x+SLIDER_SIZE-1,y+1            ,x+SLIDER_SIZE-1,y+SLIDER_SIZE-1);
        gc.drawLine(x+1            ,y+SLIDER_SIZE-1,x+SLIDER_SIZE-2,y+SLIDER_SIZE-1);
      }
    }
  }

  /** get bounds of sash
   * @param i sash index [0..count-2]
   * @return bounds
   */
  private Rectangle getSashBounds(int i)
  {
    Rectangle bounds  = getBounds();
//Dprintf.dprintf("%s = %s",composite.getBounds(),getBounds());
    Rectangle bounds0 = composites[i+0].getBounds();
    Rectangle bounds1 = composites[i+1].getBounds();

    if      ((style & SWT.VERTICAL) == SWT.VERTICAL)
    {
      bounds.x      = offsets[i]-SIZE; //bounds0.x+bounds0.width;
      bounds.y      = bounds0.y;
      bounds.width  = SIZE; //((i < count-1) ? offsets[i+1] : bounds.width)-offsets[i]; // bounds1.x-(bounds0.x+bounds0.width);
    }
    else if ((style & SWT.HORIZONTAL) == SWT.HORIZONTAL)
    {
      bounds.x      = bounds0.x;
      bounds.y      = offsets[i]-SIZE; //bounds0.y+bounds0.height;
      bounds.height = SIZE; // ((i < count-1) ? offsets[i+1] : bounds.height)-offsets[i]; //bounds1.y-(bounds0.y+bounds0.height);
    }

    return bounds;
  }

  /** check if x,y is inside sash area
   * @param i sash index [0..count-2]
   * @param x,y coordinates
   * @return true iff x,y is inside sash area
   */
  private boolean isInsideSash(int i, int x, int y)
  {
    return getSashBounds(i).contains(x,y);
  }

  /** check if x,y is inside slider area
   * @param x,y coordinates
   * @return true iff x,y is inside slider area
   */
  private boolean isInsideSash(int x, int y)
  {
    for (int i = 0; i < count-1; i++)
    {
      if (isInsideSash(i,x,y)) return true;
    }

    return false;
  }

  /** get sash at x,y
   * @param x,y coordinates
   * @return sash index [0..count-2] or -1 if no sash found
   */
  private int getSashAt(int x, int y)
  {
    for (int i = 0; i < count-1; i++)
    {
      if (isInsideSash(i,x,y)) return i;
    }

    return -1;
  }

  /** notify resize listeners
   * @param index [0..count-2]
   * @param size new size
   */
  private void notifyResize(int i, int size)
  {
    Event event = new Event();
    event.type   = SWT.Resize;
    event.widget = this;
    event.detail = size;
    event.doit   = true;
    for (Listener listener : resizeListenerSet)
    {
      listener.handleEvent(event);
      if (!event.doit) break;
    }
  }
}

/* end of file */
