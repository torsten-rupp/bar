/***********************************************************************\
*
* $Source: /home/torsten/cvs/bar/barcontrol/src/BusyDialog.java,v $
* $Revision: 1199 $
* $Author: trupp $
* Contents: busy dialog
* Systems: all
*
\***********************************************************************/

/****************************** Imports ********************************/
import org.eclipse.swt.events.TraverseEvent;
import org.eclipse.swt.events.TraverseListener;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.SelectionListener;
import org.eclipse.swt.graphics.GC;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.Rectangle;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.List;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Text;

/****************************** Classes ********************************/

/** busy dialog
 */
class BusyDialog
{
  // --------------------------- constants --------------------------------

  // busy dialog flags
  public final static int NONE          = 0;
  public final static int TEXT0         = 1 << 0;
  public final static int TEXT1         = 1 << 1;
  public final static int PROGRESS_BAR0 = 1 << 2;
  public final static int PROGRESS_BAR1 = 1 << 3;
  public final static int LIST          = 1 << 4;

  // --------------------------- variables --------------------------------
  private Display     display;

  private int         animateInterval;
  private long        animateTimestamp;
  private Image       animateImages[] = new Image[2];
  private int         animateImageIndex;
  private boolean     animationQuit;

  private Shell       dialog;
  private Label       widgetImage;
  private Label       widgetMessage;
  private Label       widgetText0,widgetText1;
  private ProgressBar widgetProgressBar0,widgetProgressBar1;
  private List        widgetList;
  private Button      widgetAbortButton;
  private int         maxListLength;

  private boolean     abortedFlag;
  private boolean     resizedFlag;

  // ------------------------ native functions ----------------------------

  // ---------------------------- methods ---------------------------------

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   * @param message dialog message
   * @param flags busy dialog flags
   * @param maxListLength max. list length
   */
  public BusyDialog(Shell parentShell, String title, int width, int height, String message, int flags, int maxListLength)
  {
    TableLayout     tableLayout;
    TableLayoutData tableLayoutData;
    Composite       composite,subComposite;
    Label           label;
    int             x,y;

    this.maxListLength = maxListLength;

    display = parentShell.getDisplay();

    // load animation images
    animateImages[0] = Widgets.loadImage(display,"busy0.png");
    animateImages[1] = Widgets.loadImage(display,"busy1.png");
    animateTimestamp  = System.currentTimeMillis();
    animateImageIndex = 0;

    // create dialog
    dialog = new Shell(parentShell,SWT.DIALOG_TRIM|SWT.RESIZE|SWT.APPLICATION_MODAL);
    dialog.setText(title);
    tableLayout = new TableLayout(new double[]{1.0,0.0},1.0);
    tableLayout.minWidth  = width;
    tableLayout.minHeight = height;
    dialog.setLayout(tableLayout);
    dialog.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));

    // message
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(1.0,new double[]{0.0,1.0},4));
    composite.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NSWE));
    {
      widgetImage = new Label(composite,SWT.LEFT);
      widgetImage.setImage(animateImages[animateImageIndex]);
      widgetImage.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NW,0,0,4,4));

      subComposite = new Composite(composite,SWT.NONE);
      subComposite.setLayout(new TableLayout(new double[]{0.0,1.0},1.0));
      subComposite.setLayoutData(new TableLayoutData(0,1,TableLayoutData.NSWE));
      {
        int row = 0;

        if (message != null)
        {
          widgetMessage = new Label(subComposite,SWT.LEFT);
          widgetMessage.setText(message);
          widgetMessage.setLayoutData(new TableLayoutData(row,0,TableLayoutData.N|TableLayoutData.WE)); row++;
        }
        else
        {
          widgetMessage = null;
        }

        if ((flags & TEXT0) != 0)
        {
          widgetText0 = new Label(subComposite,SWT.LEFT);
          widgetText0.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
        }
        else
        {
          widgetText0 = null;
        }

        if ((flags & PROGRESS_BAR0) != 0)
        {
          widgetProgressBar0 = new ProgressBar(subComposite,SWT.LEFT);
          widgetProgressBar0.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetProgressBar0.setMinimum(0.0);
          widgetProgressBar0.setMaximum(100.0);
        }
        else
        {
          widgetProgressBar0 = null;
        }

        if ((flags & TEXT1) != 0)
        {
          widgetText1 = new Label(subComposite,SWT.LEFT);
          widgetText1.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
        }
        else
        {
          widgetText1 = null;
        }

        if ((flags & PROGRESS_BAR1) != 0)
        {
          widgetProgressBar1 = new ProgressBar(subComposite,SWT.LEFT);
          widgetProgressBar1.setLayoutData(new TableLayoutData(row,0,TableLayoutData.WE)); row++;
          widgetProgressBar1.setMinimum(0.0);
          widgetProgressBar1.setMaximum(100.0);
        }
        else
        {
          widgetProgressBar1 = null;
        }

        if ((flags & LIST) != 0)
        {
          widgetList = new List(subComposite,SWT.BORDER);
          widgetList.setLayoutData(new TableLayoutData(row,0,TableLayoutData.NSWE)); row++;
        }
        else
        {
          widgetList = null;
        }
      }
    }

    // buttons
    composite = new Composite(dialog,SWT.NONE);
    composite.setLayout(new TableLayout(0.0,1.0));
    composite.setLayoutData(new TableLayoutData(1,0,TableLayoutData.WE,0,0,4,4));
    {
      widgetAbortButton = new Button(composite,SWT.CENTER|SWT.BORDER);
      widgetAbortButton.setText("Abort");
      widgetAbortButton.setLayoutData(new TableLayoutData(0,0,TableLayoutData.NONE,0,0,0,0,60,SWT.DEFAULT));
      widgetAbortButton.addSelectionListener(new SelectionListener()
      {
        public void widgetDefaultSelected(SelectionEvent selectionEvent)
        {
        }
        public void widgetSelected(SelectionEvent selectionEvent)
        {
          Button widget = (Button)selectionEvent.widget;
          abort();
        }
      });
    }

    // resize handler
    dialog.addListener(SWT.Resize,new Listener()
    {
      public void handleEvent(Event event)
      {
        resizedFlag = true;
      }
    });

    // add escape key handler
    dialog.addTraverseListener(new TraverseListener()
    {
      public void keyTraversed(TraverseEvent traverseEvent)
      {
        Shell widget = (Shell)traverseEvent.widget;

        if (traverseEvent.detail == SWT.TRAVERSE_ESCAPE)
        {
          abort();
          traverseEvent.doit = false;
        }
      }
    });

    // close handler to abort
    dialog.addListener(SWT.Close,new Listener()
    {
      public void handleEvent(Event event)
      {
        Shell widget = (Shell)event.widget;

        abort();
        event.doit = false;
      }
    });

    // pack
    dialog.pack();

    // get location for dialog (keep 16 pixel away form right/bottom)
    Point cursorPoint = display.getCursorLocation();
    Rectangle displayBounds = display.getBounds();
    Rectangle bounds = dialog.getBounds();
    x = Math.min(Math.max(cursorPoint.x-bounds.width /2,0),displayBounds.width -bounds.width -16);
    y = Math.min(Math.max(cursorPoint.y-bounds.height/2,0),displayBounds.height-bounds.height-64);
    dialog.setLocation(x,y);

    // run dialog
    resizedFlag = false;
    abortedFlag = false;
    dialog.open();

    // start animation
    animateInterval = 250;
    Thread thread = new Thread()
    {
      public void run()
      {
        while (!dialog.isDisposed())
        {
          animate();
          try { Thread.sleep(animateInterval); } catch (InterruptedException exception) { /* ignore */ }
        }
      }
    };
    thread.start();
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   * @param message dialog message
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, int width, int height, String message, int flags)
  {
    this(parentShell,title,width,height,message,flags,10);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   * @param message dialog message
   */
  public BusyDialog(Shell parentShell, String title, int width, int height, String message)
  {
    this(parentShell,title,width,height,message,NONE);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, int width, int height, int flags)
  {
    this(parentShell,title,width,height,null,flags);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param width,height width/height of dialog
   */
  public BusyDialog(Shell parentShell, String title, int width, int height)
  {
    this(parentShell,title,width,height,NONE);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   * @param flags busy dialog flags
   * @param maxListLength max. list length
   */
  public BusyDialog(Shell parentShell, String title, String message, int flags, int maxListLength)
  {
    this(parentShell,title,300,150,message,flags,maxListLength);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, String message, int flags)
  {
    this(parentShell,title,300,150,message,flags,10);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param message dialog message
   */
  public BusyDialog(Shell parentShell, String title, String message)
  {
    this(parentShell,title,message,NONE);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   * @param flags busy dialog flags
   */
  public BusyDialog(Shell parentShell, String title, int flags)
  {
    this(parentShell,title,null,flags,10);
  }

  /** create busy dialog
   * @param parentShell parent shell
   * @param title dialog title
   */
  public BusyDialog(Shell parentShell, String title)
  {
    this(parentShell,title,NONE);
  }

  /** close busy dialog
   */
  public void close()
  {
    display.syncExec(new Runnable()
    {
      public void run()
      {
        dialog.dispose();
      }
    });
  }

  /** check if dialog is closed
   * @return true iff closed
   */
  public boolean isClosed()
  {
    return dialog.isDisposed();
  }

  /** close busy dialog
   */
  public void abort()
  {
    abortedFlag = true;
    widgetAbortButton.setEnabled(false);
  }

  /** check if "cancel" button clicked
   * @return true iff "cancel" button clicked, false otherwise
   */
  public boolean isAborted()
  {
    return abortedFlag;
  }

  /** set message
   * @param format format string
   * @param args optional arguments
   */
  public void setMessage(final String format, final Object... args)
  {
    if ((widgetMessage != null) && !dialog.isDisposed())
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          String text = String.format(format,args);

          // set message text
          widgetMessage.setText(text);
          display.update();

          // resize dialog (it not manually changed)
          if (!resizedFlag)
          {
            GC gc = new GC(widgetMessage);
            int width = gc.stringExtent(text).x;
            gc.dispose();

            if (widgetMessage.getSize().x < width) dialog.pack();
          }
        }
      });
    }
  }

  /** set minimal progress value
   * @param i index 0|1
   * @param n value
   */
  public void setMinimum(int i, double n)
  {
    ProgressBar widgetProgressBar = null;
    switch (i)
    {
      case 0: widgetProgressBar = widgetProgressBar0; break;
      case 1: widgetProgressBar = widgetProgressBar1; break;
    }
    if (widgetProgressBar != null) widgetProgressBar.setMinimum(n);
  }

  /** set maximal progress value
   * @param i index 0|1
   * @param n value
   */
  public void setMaximum(int i, double n)
  {
    ProgressBar widgetProgressBar = null;
    switch (i)
    {
      case 0: widgetProgressBar = widgetProgressBar0; break;
      case 1: widgetProgressBar = widgetProgressBar1; break;
    }
    if (widgetProgressBar != null) widgetProgressBar.setMaximum(n);
  }

  /** set minimal progress value
   * @param n value
   */
  public void setMinimum(double n)
  {
    setMinimum(0,n);
  }

  /** set maximal progress value
   * @param n value
   */
  public void setMaximum(double n)
  {
    setMaximum(0,n);
  }

  /** update busy dialog text
   * @param i index 0|1
   * @param format format string
   * @param args optional arguments
   * @return true if closed, false otherwise
   */
  public boolean updateText(final int i, final String format, final Object... args)
  {
    if (!dialog.isDisposed())
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          animate();

          if (format != null)
          {
            String text = String.format(format,args);

            // set message text
            Label widgetText = null;
            switch (i)
            {
              case 0: widgetText = widgetText0; break;
              case 1: widgetText = widgetText1; break;
            }
            if (widgetText != null) widgetText.setText(text);

            // resize dialog (it not manually changed)
            if (!resizedFlag)
            {
              GC gc = new GC(widgetText);
              int width = gc.stringExtent(text).x;
              gc.dispose();

              if (widgetText.getSize().x < width) dialog.pack();
            }
          }

          display.update();
        }
      });

      return true;
    }
    else
    {
      return false;
    }
  }

  /** update busy dialog text
   * @param i index 0|1
   * @param n number to show
   * @return true if closed, false otherwise
   */
  public boolean updateText(int i, Long n)
  {
    return updateText(i,Long.toString(n));
  }

  /** update busy dialog text
   * @param i index 0|1
   * @return true if closed, false otherwise
   */
  public boolean update(int i)
  {
    return updateText(i,(String)null);
  }

  /** update busy dialog text
   * @param text text to show (can be null)
   * @return true if closed, false otherwise
   */
  public boolean updateText(String text)
  {
    return updateText(0,text);
  }

  /** update busy dialog progress bar
   * @param i index 0|1
   * @param n progress value
   * @return true if closed, false otherwise
   */
  public boolean updateProgressBar(final int i, final double n)
  {
    if (!dialog.isDisposed())
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          animate();

          // set progress bar value
          ProgressBar widgetProgressBar = null;
          switch (i)
          {
            case 0: widgetProgressBar = widgetProgressBar0; break;
            case 1: widgetProgressBar = widgetProgressBar1; break;
          }
          if (widgetProgressBar != null) widgetProgressBar.setSelection(n);

          display.update();
        }
      });

      return true;
    }
    else
    {
      return false;
    }
  }

  /** update busy dialog progress bar
   * @param n progress value
   * @return true if closed, false otherwise
   */
  public boolean updateProgressBar(double n)
  {
    return updateProgressBar(0,n);
  }

  /** update busy dialog
   * @param n number to show
   * @return true if closed, false otherwise
   */
  public boolean updateText(Long n)
  {
    return updateText(0,Long.toString(n));
  }

  /** update busy dialog
   * @return true if closed, false otherwise
   */
  public boolean update()
  {
    return update(0);
  }

  /** update busy dialog list
   * @param i index 0|1
   * @param format format string
   * @param args optional arguments
   * @return true if closed, false otherwise
   */
  public boolean updateList(final String format, final Object... args)
  {
    if (!dialog.isDisposed())
    {
      display.syncExec(new Runnable()
      {
        public void run()
        {
          animate();

          if (format != null)
          {
            String text = String.format(format,args);

            // add list text
            widgetList.add(text);
            while (widgetList.getItemCount() > maxListLength)
            {
              widgetList.remove(0);
            }

            // resize dialog (it not manually changed)
            if (!resizedFlag)
            {
              GC gc = new GC(widgetList);
              int width = gc.stringExtent(text).x;
              gc.dispose();

              if (widgetList.getSize().x < width) dialog.pack();
            }
          }

          display.update();
        }
      });

      return true;
    }
    else
    {
      return false;
    }
  }

  /** set animate interval
   * @param interval animate time interval [ms]
   */
  public void setAnimateInterval(int interval)
  {
    this.animateInterval = interval;
  }

  /** auto animate dialog
   * @param interval animate time interval [ms]
   */
  public void autoAnimate(int interval)
  {
    setAnimateInterval(interval);

    animationQuit = false;
    Thread thread = new Thread()
    {
      public void run()
      {
        while (!animationQuit)
        {
          // animate
          display.syncExec(new Runnable()
          {
            public void run()
            {
              animate();
            }
          });

          // sleep
          try { Thread.sleep(animateInterval); } catch (InterruptedException exception) { /* ignore */ }
        }
      }
    };
    thread.start();
  }

  /** auto animate dialog
   */
  public void autoAnimate()
  {
    autoAnimate(250);
  }

  //-----------------------------------------------------------------------

  /** animate dialog
   */
  private void animate()
  {
    if (!dialog.isDisposed())
    {
      long timestamp = System.currentTimeMillis();
      if (timestamp > (animateTimestamp+animateInterval))
      {
        animateTimestamp  = timestamp;
        animateImageIndex = (animateImageIndex+1)%2;
        widgetImage.setImage(animateImages[animateImageIndex]);
      }
    }
  }
}

/* end of file */
